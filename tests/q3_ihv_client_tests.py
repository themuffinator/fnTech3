"""The IHV Test's developer client: its client-game table, records, demo pacing and commands."""
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args
from rtcw_server_world_tests import function

ROOT = Path(__file__).resolve().parents[1]
CGAME = ROOT / 'code/client/cl_q3ihv_cgame.cpp'
SESSION = ROOT / 'code/client/cl_q3ihv.cpp'

# quake3.exe 0x405e30..0x4060a1 builds the 62-word table the client game's
# Init copies; the target each word names, by what it does.
ORIGINAL_TABLE = {
    1: 'IhvPrint', 2: 'IhvError', 3: 'IhvMilliseconds', 4: 'IhvCvarGet', 5: 'IhvCvarSet',
    6: 'IhvArgc', 7: 'IhvArgv', 8: 'IhvArgs', 9: 'IhvReadFile', 10: 'IhvFreeFile',
    11: 'IhvWriteFile', 12: 'IhvAddCommandText', 13: 'IhvDebugGraph', 14: 'IhvClientCommand',
    15: 'IhvTempBox', 16: 'IhvBoxBrushes', 17: 'IhvLerpTag', 18: 'IhvLoadMap', 19: 'IhvMapLoaded',
    20: 'IhvInlineModel', 21: 'IhvCollisionModel', 22: 'IhvNumInlineModels', 23: 'IhvPointContents',
    24: 'IhvTransformedContents', 25: 'IhvBoxTrace', 26: 'IhvTransformedTrace',
    27: 'IhvStartSound', 28: 'IhvLocalSound', 29: 'IhvClearLoops', 30: 'IhvAddLoop',
    31: 'IhvRespatialize', 32: 'IhvSoundBegin', 33: 'IhvRegisterSound', 34: 'IhvRegisterSound',
    35: 'IhvSoundEnd', 36: 'IhvMusic', 39: 'IhvLoadWorld', 40: 'IhvRegisterModel',
    41: 'IhvRegisterSkin', 42: 'IhvRegisterShader', 43: 'IhvRenderScene', 44: 'IhvSetColor',
    45: 'IhvStretchPic', 46: 'IhvBeginFrame', 47: 'IhvEndFrame', 48: 'IhvUpdateScreen',
    49: 'IhvBigStringWidth', 50: 'IhvSmallStringWidth', 51: 'IhvAdjustFrom640', 52: 'IhvFillRect',
    53: 'IhvDrawPic', 54: 'IhvBigString', 55: 'IhvBigStringColor', 56: 'IhvGameState',
    58: 'IhvEntity', 59: 'IhvCurrentCommand', 60: 'IhvCommand',
}


class Q3IhvClientTests(unittest.TestCase):
    def execute(self, source):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++ compiler unavailable')
        scratch = ROOT / '.tmp'
        scratch.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='ihv-client-', dir=scratch) as folder:
            folder = Path(folder)
            src, exe = folder / 'test.cpp', folder / 'test.exe'
            src.write_text(source, encoding='utf-8')
            result = subprocess.run([compiler, *compiler_target_args(compiler), '-std=c++17', '-I', str(ROOT),
                str(src), '-o', str(exe)], cwd=ROOT, text=True, capture_output=True, timeout=60)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            result = subprocess.run([str(exe)], cwd=ROOT, text=True, capture_output=True, timeout=20)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_import_table_is_the_originals(self):
        body = function(CGAME.read_text(encoding='utf-8'), 'void IhvFillImports(')
        slots = {int(n): name for n, name in re.findall(r't\[(\d+)\] = Slot\( (\w+) \)', body)}
        self.assertEqual(slots, ORIGINAL_TABLE)
        # Words 37 and 38 are the video size itself, 61 the weapon word; 0 and 57 stay null.
        self.assertIn('t[37] = cls.glconfig.vidWidth; t[38] = cls.glconfig.vidHeight;', body)
        self.assertIn('t[61] = reinterpret_cast<std::intptr_t>( &ihvcg.weapon );', body)
        self.assertNotRegex(body, r't\[(0|57)\] =')

    def test_collision_model_record(self):
        self.execute(r'''
#include "code/compat/q3_ihv_client_abi.h"
#include <cstddef>
// cgamex86.dll 0x1000e73f reads word 6 as the headnode; quake3.exe 0x40aafd walks from it.
static_assert(sizeof(fntech3_q3_ihv_collision_model_t) == 112, "size");
static_assert(offsetof(fntech3_q3_ihv_collision_model_t, mins) == 0, "mins");
static_assert(offsetof(fntech3_q3_ihv_collision_model_t, maxs) == 12, "maxs");
static_assert(offsetof(fntech3_q3_ihv_collision_model_t, headnode) == 24, "headnode");
static_assert(offsetof(fntech3_q3_ihv_collision_model_t, name) == 28, "name");
static_assert(offsetof(fntech3_q3_ihv_collision_model_t, isMD3) == 92, "isMD3");
static_assert(offsetof(fntech3_q3_ihv_collision_model_t, frames) == 104, "frames");
static_assert(offsetof(fntech3_q3_ihv_collision_model_t, tags) == 108, "tags");
int main(){return 0;}
''')

    def test_resource_tokens_keep_their_kind(self):
        source = CGAME.read_text(encoding='utf-8')
        start = source.index('enum class IhvKind')
        end = source.index('struct IhvCGame {')
        self.execute(r'''
#include <cstdint>
#include <cstdio>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"%d: %s\n",__LINE__,#x);return 1;}}while(0)
''' + source[start:end] + r'''
int main(){
 int h=-7;
 const auto shader0=IhvToken(IhvKind::Shader,0);
 CHECK(shader0!=0&&IhvHandle(IhvKind::Shader,shader0,h)&&h==0);
 CHECK(!IhvHandle(IhvKind::Model,shader0,h));
 CHECK(!IhvHandle(IhvKind::Shader,0,h));
 CHECK(IhvToken(IhvKind::Model,-1)==0&&IhvToken(IhvKind::Model,0x0fffffff)==0);
 const auto skin=IhvToken(IhvKind::Skin,1234);
 CHECK(IhvHandle(IhvKind::Skin,skin,h)&&h==1234&&!IhvHandle(IhvKind::Sound,skin,h));
}
''')

    def test_demo_pacing(self):
        body = function(SESSION.read_text(encoding='utf-8'), 'void CL_Q3IHVSetCGameTime(')
        self.execute(r'''
#include <cstdio>
#include <vector>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"%d: %s\n",__LINE__,#x);return 1;}}while(0)
enum { CA_DISCONNECTED=1, CA_PRIMED=7, CA_ACTIVE=8 };
struct { int state=CA_DISCONNECTED; int realtime=500; } cls;
struct { bool demoplaying=false; int timeDemoStart=0; int timeDemoFrames=0; } clc;
struct { int integer=0; } timedemoCvar; auto *com_timedemo=&timedemoCvar;
static std::vector<bool> answers; static int frames, reads, endAfter=-1, lastTime=-1; static bool lastDemo;
int Sys_Milliseconds(){return 1234;}
bool CL_Q3IHVCGameFrame(int t,bool demo){lastTime=t;lastDemo=demo;bool r=frames<(int)answers.size()?answers[frames]:true;++frames;return r;}
void CL_ReadDemoMessage(){++reads;if(cls.state==CA_PRIMED)cls.state=CA_ACTIVE;if(endAfter>=0&&reads>=endAfter)cls.state=CA_DISCONNECTED;}
''' + body + r'''
int main(){
 // a primed demo reads to its first frame, then asks the module
 cls.state=CA_PRIMED;clc.demoplaying=true;answers={false,false,true};
 CL_Q3IHVSetCGameTime();
 CHECK(cls.state==CA_ACTIVE&&reads==3&&frames==3&&lastTime==500&&lastDemo);
 // a timed demo counts the frames that drew
 com_timedemo->integer=1;frames=reads=0;answers={true};
 CL_Q3IHVSetCGameTime();
 CHECK(reads==0&&frames==1&&clc.timeDemoFrames==1&&clc.timeDemoStart==1234);
 // the demo ending stops the loop without asking again
 frames=reads=0;answers={false,false,false,false};endAfter=2;
 CL_Q3IHVSetCGameTime();
 CHECK(reads==2&&frames==2&&cls.state==CA_DISCONNECTED&&clc.timeDemoFrames==1);
 // live play asks once and reads nothing
 cls.state=CA_ACTIVE;clc.demoplaying=false;frames=reads=0;answers={false};endAfter=-1;
 CL_Q3IHVSetCGameTime();
 CHECK(frames==1&&reads==0&&!lastDemo);
 // nothing before the level is active
 cls.state=CA_PRIMED;frames=0;CL_Q3IHVSetCGameTime();CHECK(frames==0);
}
''')

    def test_server_commands(self):
        body = function(SESSION.read_text(encoding='utf-8'), 'void CL_Q3IHVServerCommand(')
        self.execute(r'''
#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"%d: %s\n",__LINE__,#x);return 1;}}while(0)
enum { ERR_SERVERDISCONNECT=5 };
struct Cvar { int integer=0; } devCvar, svCvar; Cvar *com_developer=&devCvar, *com_sv_running=&svCvar;
static std::string tokens, added; static int exports, closes; struct Drop { int code; };
void Com_Printf(const char*,...){}
[[noreturn]] void Com_Error(int code,const char*,...){throw Drop{code};}
void Cmd_TokenizeString(const char *t){tokens=t;}
const char *Cmd_Argv(int){static std::string first;first=tokens.substr(0,tokens.find(' '));return first.c_str();}
void Con_Close(){++closes;}
void Cbuf_AddText(const char *t){added+=t;}
void CL_Q3IHVCGameServerCommand(){++exports;}
''' + body + r'''
int main(){
 CL_Q3IHVServerCommand("cp \"hello\"");CHECK(exports==1&&tokens=="cp \"hello\"");
 try{CL_Q3IHVServerCommand("disconnect");CHECK(false);}catch(const Drop &d){CHECK(d.code==ERR_SERVERDISCONNECT);}
 CHECK(exports==1);
 // a level shot needs the local server; without it the module is not told
 CL_Q3IHVServerCommand("clientLevelShot");CHECK(exports==1&&added.empty()&&closes==0);
 svCvar.integer=1;CL_Q3IHVServerCommand("clientLevelShot");
 CHECK(exports==2&&closes==1&&added=="wait ; wait ; wait ; wait ; screenshot levelshot\n");
}
''')

    def test_native_command(self):
        body = function(SESSION.read_text(encoding='utf-8'), 'fntech3_q3_ihv_usercmd_t CL_Q3IHVNativeCmd(')
        self.execute(r'''
#include "code/qcommon/q3_ihv_wire.hpp"
#include "code/compat/q3_ihv_client_abi.h"
#include <cstdio>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"%d: %s\n",__LINE__,#x);return 1;}}while(0)
namespace ihvwire = fntech3::q3ihv::wire;
enum { CMD_MASK = 63, BUTTON_ANY = 2048 };
struct usercmd_t { int serverTime; int angles[3]; int buttons; unsigned char weapon; signed char forwardmove, rightmove, upmove; };
struct { usercmd_t cmds[64]; } cl;
''' + body + r'''
int main(){
 cl.cmds[4]={1000,{0,0,0},0,0,0,0,0};
 cl.cmds[5]={1016,{0x4000,-1,0x12345},1|16|BUTTON_ANY|0x100,9,127,-128,-3};
 auto c=CL_Q3IHVNativeCmd(5,7);
 CHECK(c.bytes[0]==16&&c.bytes[1]==(1|16|0x80)&&c.bytes[2]==7&&c.bytes[3]==0);
 CHECK(c.bytes[4]==0x00&&c.bytes[5]==0x40&&c.bytes[6]==0xff&&c.bytes[7]==0xff&&c.bytes[8]==0x45&&c.bytes[9]==0x23);
 CHECK(c.bytes[10]==127&&c.bytes[11]==0x80&&c.bytes[12]==0xfd&&c.bytes[13]==0);
 // CL_FinishMove 0x402ea4: past 250 ms is 100; a step back is nothing
 cl.cmds[6]=cl.cmds[5];cl.cmds[6].serverTime=1016+251;CHECK(CL_Q3IHVNativeCmd(6,0).bytes[0]==100);
 cl.cmds[7]=cl.cmds[5];cl.cmds[7].serverTime=900;CHECK(CL_Q3IHVNativeCmd(7,0).bytes[0]==0);
}
''')

    def test_frame_ping(self):
        body = function(SESSION.read_text(encoding='utf-8'), 'std::uint32_t CL_Q3IHVPing(')
        self.execute(r'''
#include "code/qcommon/q3_ihv_message.hpp"
#include <array>
#include <cstdio>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"%d: %s\n",__LINE__,#x);return 1;}}while(0)
namespace ihvwire = fntech3::q3ihv::wire;
struct { bool demoplaying=false; } clc;
struct { int realtime=5000; } cls;
struct IhvOutPacket { int realtime = 0; int cmdNumber = 0; };
struct Chan { int out=40; int OutgoingSequence() const { return out; } };
struct { Chan chan; std::array<IhvOutPacket,32> out{}; } ihvLive;
''' + body + r'''
int main(){
 for(int s=8;s<40;++s) ihvLive.out[s&31]={s*100,s};
 ihvwire::Snapshot frame; frame.commandSequence=37;
 CHECK(CL_Q3IHVPing(frame)==5000-3700);
 frame.commandSequence=7; CHECK(CL_Q3IHVPing(frame)==999);
 clc.demoplaying=true; frame.commandSequence=37; CHECK(CL_Q3IHVPing(frame)==0);
}
''')

    def test_recorded_gamestate(self):
        # CL_Record_f 0x404650..0x404707: the client's configstrings and the
        # baselines that carry a model, a narrower test than the server's.
        body = function(SESSION.read_text(encoding='utf-8'), 'bool CL_Q3IHVDemoGamestate(')
        self.execute(r'''
#include "code/qcommon/q3_ihv_message.hpp"
#include <cstdio>
#include <memory>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"%d: %s\n",__LINE__,#x);return 1;}}while(0)
namespace ihvwire = fntech3::q3ihv::wire;
''' + body + r'''
int main(){
 auto game=std::make_unique<ihvwire::GameState>();
 game->configstrings[0]="\\protocol\\33\\mapname\\tim_dm1";
 game->configstrings[5]="models/x.md3";
 auto set=[&](unsigned n,unsigned field,unsigned value){
  auto &b=game->baselines[n];ihvwire::Store(b.data(),2,n);b[field]=static_cast<std::uint8_t>(value);};
 set(3,0x95,7);   // a model: kept
 set(4,0x94,9);   // a looping sound alone: the server sends it, a recording drops it
 set(6,4,1);      // effects alone: dropped too
 auto out=std::make_unique<ihvwire::MessageWriter>();
 CHECK(CL_Q3IHVDemoGamestate(*game,*out));
 ihvwire::MessageDecoder decoder;ihvwire::DecodedMessage decoded;
 CHECK(decoder.Decode(out->Data(),out->Bytes(),decoded)==ihvwire::MessageResult::Ok&&decoded.gamestate);
 CHECK(decoded.gamestate->configstrings[5]=="models/x.md3");
 CHECK(decoded.gamestate->baselines[3][0x95]==7);
 CHECK(!ihvwire::Load(decoded.gamestate->baselines[4].data(),2)&&!ihvwire::Load(decoded.gamestate->baselines[6].data(),2));
 // A gamestate without its protocol is refused, as the shared writer refuses it.
 game->configstrings[0]="\\mapname\\tim_dm1";out=std::make_unique<ihvwire::MessageWriter>();
 CHECK(!CL_Q3IHVDemoGamestate(*game,*out));
}
''')


if __name__ == '__main__':
    unittest.main()
