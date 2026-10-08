"""Execute the production demo reader against synthetic malformed envelopes."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]


class DemoReaderTests(unittest.TestCase):

    def test_paused_demo_preserves_command_history_and_samples_input(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++ compiler unavailable')
        source = (ROOT / 'code/client/cl_input.cpp').read_text(encoding='utf-8')
        start = source.index('static void CL_CreateNewCommands( void ) {')
        function = source[start:source.index('\n}', start)+2]
        harness = r"""
#include <cassert>
#include <climits>
#include "code/qcommon/netchan_safety.hpp"
constexpr int CA_PRIMED=1, CMD_MASK=63;
struct Cmd { int serverTime=0; int sample=0; };
struct { int state=CA_PRIMED; } cls;
struct { bool demoplaying=false; } clc;
struct { int cmdNumber=0, serverTime=100; Cmd cmds[64]{}; } cl;
struct Cvar { int integer=0; float value=1; } freeze, scale;
Cvar *cl_freezeDemo=&freeze, *com_timescale=&scale;
unsigned frame_msec=0, old_com_frameTime=0, com_frameTime=0;
int samples=0, suspends=0;
void CL_SuspendUsercmdInputSampling() { ++suspends; }
Cmd CL_CreateCmd() { return {cl.serverTime,++samples}; }
""" + function + r"""
int main() {
 // Populate history, then freeze with the snapshot between old and new commands.
 for (int i=0;i<80;++i) {
  cl.serverTime=100+i; com_frameTime+=16; CL_CreateNewCommands();
 }
 const int number=cl.cmdNumber;
 const int oldest=(number-63)&CMD_MASK;
 const Cmd old=cl.cmds[oldest];
 clc.demoplaying=true; freeze.integer=1;
 for (int i=0;i<200;++i) { com_frameTime+=16; CL_CreateNewCommands(); }
 assert(cl.cmdNumber==number && cl.cmds[oldest].serverTime==old.serverTime);
 assert(samples==280 && cl.cmds[number&CMD_MASK].sample==samples);
 assert(frame_msec==16 && old_com_frameTime==com_frameTime);
 // This is the stock Q3 lost-command condition: the retained oldest command
 // must not move beyond the frozen snapshot's acknowledged command time.
 const int snapshotCommandTime=cl.serverTime-10;
 assert(cl.cmds[oldest].serverTime<=snapshotCommandTime);
 freeze.integer=0; scale.value=0;
 CL_CreateNewCommands(); assert(cl.cmdNumber==number && samples==281);
 scale.value=1; CL_CreateNewCommands(); assert(cl.cmdNumber==number+1);
 // Pause controls do not freeze command history during live play.
 clc.demoplaying=false; freeze.integer=1; scale.value=0;
 CL_CreateNewCommands(); assert(cl.cmdNumber==number+2);
 cl_freezeDemo=nullptr; clc.demoplaying=true; scale.value=1;
 cl.cmdNumber=INT_MAX; CL_CreateNewCommands(); assert(cl.cmdNumber==INT_MIN);
 cls.state=0; const int before=samples;
 CL_CreateNewCommands(); assert(samples==before && suspends==1);
}
"""
        with tempfile.TemporaryDirectory() as tmp:
            cpp, exe = Path(tmp)/'paused_cmd.cpp', Path(tmp)/'paused_cmd.exe'
            cpp.write_text(harness, encoding='utf-8')
            subprocess.run([compiler, *compiler_target_args(compiler), '-std=c++17',
                            '-I', str(ROOT), str(cpp), '-o', str(exe)], check=True,
                           capture_output=True, text=True)
            subprocess.run([str(exe)], check=True, capture_output=True, text=True)

    def test_completion_defers_quit_to_chained_playback(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++ compiler unavailable')
        source = (ROOT / 'code/client/cl_main.cpp').read_text(encoding='utf-8')
        start = source.index('static void CL_DemoCompleted( void ) {')
        function = source[start:source.index('\n}', start)+2]
        harness = r'''
#include <cassert>
#include <string>
struct Cvar { int integer; } timedemo{0}, quit{0};
Cvar *com_timedemo=&timedemo, *cl_quitOnDemoCompleted=&quit;
struct { bool demoplaying=true; int timeDemoStart=0, timeDemoFrames=1; } clc;
constexpr bool qtrue=true;
bool chain=false, enableQuit=false;
int disconnects=0;
std::string queued;
int Sys_Milliseconds() { return 10; }
template<class... T> void Com_Printf(const char *, T...) {}
void CL_Disconnect(bool) { ++disconnects; clc.demoplaying=false; }
void CL_NextDemo() {
 if (chain) clc.demoplaying=true;
 if (enableQuit) quit.integer=1;
}
void Cbuf_AddText(const char *s) { queued+=s; }
''' + function + r'''
int main() {
 // The completed demo did not request exit; the next demo does.
 chain=true; enableQuit=true;
 CL_DemoCompleted();
 assert(disconnects==1 && clc.demoplaying && queued.empty());
 // The replacement must reach its own end before the queued quit.
 chain=false; enableQuit=false;
 CL_DemoCompleted();
 assert(disconnects==2 && !clc.demoplaying && queued=="quit\n");
 queued.clear(); quit.integer=0;
 CL_DemoCompleted(); assert(queued.empty());
 cl_quitOnDemoCompleted=nullptr;
 CL_DemoCompleted(); assert(queued.empty());
 cl_quitOnDemoCompleted=&quit; quit.integer=1; chain=true;
 CL_DemoCompleted(); assert(clc.demoplaying && queued.empty());
 chain=false; timedemo.integer=1;
 CL_DemoCompleted(); assert(queued=="quit\n");
}
'''
        with tempfile.TemporaryDirectory() as tmp:
            cpp, exe = Path(tmp)/'completion.cpp', Path(tmp)/'completion.exe'
            cpp.write_text(harness, encoding='utf-8')
            subprocess.run([compiler, *compiler_target_args(compiler), '-std=c++17',
                            str(cpp), '-o', str(exe)], check=True,
                           capture_output=True, text=True)
            subprocess.run([str(exe)], check=True, capture_output=True, text=True)

    def test_envelope_outcomes_do_not_parse_incomplete_messages(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++ compiler unavailable')
        source = (ROOT / 'code/client/cl_main.cpp').read_text(encoding='utf-8')
        start = source.index('void CL_ReadDemoMessage( void )')
        function = source[start:source.index('\n}', start)+2]
        harness = r'''
#include "demo_stream.hpp"
#include "../qcommon/netchan_profile.h"
#include <algorithm>
#include <cassert>
#include <cstring>
#include <string>
#include <vector>
using byte = unsigned char;
constexpr int MAX_GAME_MSGLEN_BUF=32776, FS_INVALID_HANDLE=0;
netchanWireProfile_t wireProfile=NETCHAN_WIRE_LEGACY_Q3;
netchanWireProfile_t CL_ActiveWireProfile(){return wireProfile;}
constexpr int ERR_DROP=1, EM_GAMESTATE=1, EM_SNAPSHOT=2, EM_COMMAND=4;
constexpr bool qfalse=false;
struct { int demofile=1, serverMessageSequence=42, lastPacketTime=43;
 int demoCommandSequence=44, serverCommandSequence=45, eventMask=0;
 bool demorecording=false; } clc;
struct { int realtime=99; } cls;
struct msg_t { byte *data; int maxsize, cursize=0, readcount=0; };
std::vector<byte> input;
std::size_t cursor=0;
int completed=0, parsed=0, dropped=0;
std::string output;
void MSG_Init(msg_t *m, byte *p, int n) { m->data=p; m->maxsize=n; }
std::ptrdiff_t FileRead(int, byte *p, std::size_t n) {
 n=std::min(n,input.size()-cursor);
 if(n) std::memcpy(p,input.data()+cursor,n);
 cursor+=n; return n;
}
void CL_DemoCompleted() { ++completed; }
// Q3Test reads its demos with its own reader (cl_q3test.cpp).
int Com_GameIsQ3Test() { return 0; }
bool CL_Q3TestReadDemoMessage() { return false; }
int Com_GameIsQuake3IHV() { return 0; }
bool CL_Q3IHVReadDemoMessage() { return false; }
void Com_Printf(const char *s) { output+=s; }
void Com_Error(int, const char *, int) { ++dropped; }
void CL_ParseServerMessage(msg_t *m) { ++parsed; assert(m->readcount==0); }
void CL_WriteGamestate(bool) {}
void CL_WriteSnapshot() {}
''' + function + r'''
void reset(std::vector<byte> bytes) {
 input=bytes; cursor=0; completed=parsed=dropped=0; output.clear();
 clc.demofile=1; clc.serverMessageSequence=42; clc.lastPacketTime=43;
 clc.demoCommandSequence=44;
}
void untouched() {
 assert(parsed==0 && clc.serverMessageSequence==42);
 assert(clc.lastPacketTime==43 && clc.demoCommandSequence==44);
}
std::vector<byte> header(int length) {
 auto n=static_cast<std::uint32_t>(length);
 return {7,0,0,0,byte(n),byte(n>>8),byte(n>>16),byte(n>>24)};
}
int main() {
 // A clean boundary EOF stays tolerated, including historical no-trailer files.
 for(auto profile:{NETCHAN_WIRE_LEGACY_Q3,NETCHAN_WIRE_RTCW_MP}) {
 wireProfile=profile;
 const int limit=Netchan_MessageLimit(profile);
 reset({}); CL_ReadDemoMessage();
 assert(completed==1 && dropped==0 && output.empty()); untouched();
 for(int size=1;size<8;++size) {
  reset(std::vector<byte>(size,0)); CL_ReadDemoMessage();
  assert(completed==1 && dropped==0 && output=="Demo file was truncated.\n");
  untouched();
 }
 // Keep the legacy length-only sentinel, independently of its sequence word.
 reset(header(-1)); CL_ReadDemoMessage();
 assert(completed==1 && dropped==0 && output.empty()); untouched();
 for(int length : {-2, -2147483647-1, limit+1, 2147483647}) {
  reset(header(length)); CL_ReadDemoMessage();
  assert(dropped==1 && completed==0 && cursor==8); untouched();
 }
 for(int size=0;size<4;++size) {
  auto bytes=header(4); bytes.resize(8+size,0xab);
  reset(bytes); CL_ReadDemoMessage();
  assert(completed==1 && dropped==0 && output=="Demo file was truncated.\n");
  untouched();
 }
 for(int size : {0, 4, limit}) {
  auto bytes=header(size); bytes.resize(8+size,0xab);
  reset(bytes); CL_ReadDemoMessage();
  assert(parsed==1 && completed==0 && dropped==0 && output.empty());
  assert(clc.serverMessageSequence==7 && clc.lastPacketTime==99);
  assert(clc.demoCommandSequence==45);
 }
 reset({}); clc.demofile=FS_INVALID_HANDLE; CL_ReadDemoMessage();
 assert(completed==1 && cursor==0 && output.empty()); untouched();
 }
}
'''
        with tempfile.TemporaryDirectory() as tmp:
            cpp, exe = Path(tmp)/'probe.cpp', Path(tmp)/'probe.exe'
            cpp.write_text(harness, encoding='utf-8')
            subprocess.run([compiler, *compiler_target_args(compiler), '-std=c++17',
                            '-I', str(ROOT/'code/client'), str(cpp), '-o', str(exe)],
                           check=True, capture_output=True, text=True)
            subprocess.run([str(exe)], check=True, capture_output=True, text=True)


if __name__ == '__main__':
    unittest.main()
