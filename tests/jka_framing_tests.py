"""Execute production Academy framing controls with bounds and side-effect checks."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args, configured_target_is_windows_x64

ROOT = Path(__file__).resolve().parents[1]
SCRATCH = ROOT / '.tmp/jka-compat'


def is_link(path):
    return path.is_symlink() or bool(getattr(path.lstat(), 'st_file_attributes', 0) & 0x400)


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2]


def block(source, marker):
    start = source.index(marker)
    opening = source.index('{', start)
    depth = 1
    end = opening + 1
    while depth:
        if source[end] == '{':
            depth += 1
        elif source[end] == '}':
            depth -= 1
        end += 1
    return source[start:end]


def enum(source, member):
    position = source.index(member)
    start = source.rfind('typedef enum {', 0, position)
    return source[start:source.index(';', source.index('}', position)) + 1]


PRELUDE = r'''
#include "code/qcommon/netchan_profile.h"
#include <cassert>
#include <cstdarg>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <cctype>
using byte=unsigned char;
using qboolean=int;
constexpr qboolean qtrue=1,qfalse=0;
constexpr int ERR_DROP=1,MAX_QPATH=64,svc_bad=0,svc_gamestate=2;
#define S_COLOR_YELLOW ""
struct Drop {};
static std::string dropReason;
void Com_Error(int,const char *format,...) { dropReason=format;throw Drop{}; }
static int warnings,sets,restarts,cacheResets,lastFeed,mapCalls,lastExport,lastCanonical;
static std::string game,base="base",serverGame;
static std::vector<std::string> events;
void Com_Printf(const char *,...) { ++warnings; }
const char *FS_GetBaseGameDir() { return base.c_str(); }
const char *FS_GetCurrentGameDir() { return serverGame.c_str(); }
const char *Cvar_VariableString(const char *name) { assert(!strcmp(name,"fs_game")); return game.c_str(); }
void Cvar_Set(const char *name,const char *value) { assert(!strcmp(name,"fs_game"));game=value;++sets;events.push_back("set"); }
void CL_JKAStringCacheReset() { ++cacheResets;events.push_back("cache"); }
void FS_Restart(int feed) { ++restarts;lastFeed=feed;events.push_back("restart"); }
int Q_stricmp(const char *a,const char *b) {
 while(*a && *b) { int d=std::tolower(static_cast<unsigned char>(*a))-std::tolower(static_cast<unsigned char>(*b));if(d)return d;++a;++b; }
 return static_cast<unsigned char>(*a)-static_cast<unsigned char>(*b);
}
struct msg_t { std::vector<byte> data;int readcount=0,cursize=0; };
int MSG_ReadByte(msg_t *msg) { const int index=msg->readcount++;return index<msg->cursize ? msg->data[index] : -1; }
int MSG_ReadShort(msg_t *msg) { const int low=MSG_ReadByte(msg),high=MSG_ReadByte(msg);return low<0||high<0?-1:static_cast<short>(low|(high<<8)); }
void MSG_WriteByte(msg_t *msg,int value) { msg->data.push_back(static_cast<byte>(value));msg->cursize=static_cast<int>(msg->data.size()); }
void MSG_WriteShort(msg_t *msg,int value) { MSG_WriteByte(msg,value);MSG_WriteByte(msg,static_cast<unsigned>(value)>>8); }
void MSG_WriteLong(msg_t *msg,int value) { for(int i=0;i<4;++i)MSG_WriteByte(msg,static_cast<unsigned>(value)>>(8*i)); }
struct { int checksumFeed=0x12345678,clientNum=3; } clc;
struct { int checksumFeed=0x11223344; } sv;
struct client_t { struct { netchanWireProfile_t wireProfile; } netchan;int reliableSequence=0x01020304; };
qboolean SV_LegacyFraming(client_t *client) { return client->netchan.wireProfile==NETCHAN_WIRE_STVEF_HM; }
int SV_ClientIndex(client_t *) { return 3; }
static netchanWireProfile_t active=NETCHAN_WIRE_JKA_MP;
netchanWireProfile_t CL_ActiveWireProfile() { return active; }
static int vm;
static int *cgvm=&vm;
void SHOWNET(msg_t *,const char *) {}
'''

CHECKS = r'''
static void Reset(const char *current="") {
 game=current;warnings=sets=restarts=cacheResets=mapCalls=0;
 lastFeed=lastExport=lastCanonical=-1;events.clear();cgvm=&vm;active=NETCHAN_WIRE_JKA_MP;base="base";
}
static msg_t Bytes(const std::string &text,bool terminator=true) {
 msg_t msg;msg.data.assign(text.begin(),text.end());if(terminator)msg.data.push_back(0);msg.cursize=static_cast<int>(msg.data.size());return msg;
}
template<class F> void Reject(F action) { bool dropped=false;try { action(); } catch(const Drop &) { dropped=true; }assert(dropped); }
static void NoChanges(const char *expected) { assert(game==expected && !sets && !restarts && !cacheResets); }
int main() {
 assert(Netchan_ServerEOFOpcode(NETCHAN_WIRE_JKA_MP)==10);
 assert(Netchan_ServerEOFOpcode(NETCHAN_WIRE_JK2_MP)==9);
 assert(Netchan_ServerEOFOpcode(NETCHAN_WIRE_MOHAA)==11);
 for(auto profile:{NETCHAN_WIRE_LEGACY_Q3,NETCHAN_WIRE_IOQ3,NETCHAN_WIRE_QL_RETAIL,NETCHAN_WIRE_RTCW_SP,NETCHAN_WIRE_RTCW_MP,NETCHAN_WIRE_WOLFET})
  assert(Netchan_ServerEOFOpcode(profile)==8);
 assert(CG_RAVEN_MAP_CHANGE>=0x10000 && CG_RAVEN_MAP_CHANGE!=CG_GET_TAG);
 assert(FNTech3_JKACgameExportOrdinal(CG_RAVEN_MAP_CHANGE)==27);
 assert(FNTech3_JKACgameExportArgumentCount(CG_RAVEN_MAP_CHANGE)==0);
 assert(FNTech3_JKACgameExportOrdinal(CG_GET_TAG)==-1 && FNTech3_JKACgameExportOrdinal(27)==-1);

 Reset();auto mod=Bytes("japlus");CL_JKAParseSetGame(&mod);
 assert(game=="japlus" && sets==1 && restarts==1 && cacheResets==1 && lastFeed==clc.checksumFeed);
 assert((events==std::vector<std::string>{"set","cache","restart"}));
 assert(mod.readcount==mod.cursize);
 Reset("japlus");mod=Bytes("JAPLUS");CL_JKAParseSetGame(&mod);NoChanges("japlus");
 Reset();mod=Bytes("base");CL_JKAParseSetGame(&mod);NoChanges("");
 Reset("japlus");mod=Bytes("BASE");CL_JKAParseSetGame(&mod);assert(game.empty() && restarts==1);
 Reset("japlus");mod=Bytes("");CL_JKAParseSetGame(&mod);assert(game.empty() && restarts==1);
 for(const char *bad:{".","..","../other","child/mod","child\\mod","\\absolute"}) {
  Reset("japlus");mod=Bytes(bad);CL_JKAParseSetGame(&mod);NoChanges("japlus");assert(warnings==1);
 }
 for(int length:{64,65,255}) {
  Reset("japlus");mod=Bytes(std::string(length,'x'));Reject([&] { CL_JKAParseSetGame(&mod); });NoChanges("japlus");assert(!warnings);
 }
 for(int length=0;length<64;++length) {
  Reset("japlus");mod=Bytes(std::string(length,'x'),false);Reject([&] { CL_JKAParseSetGame(&mod); });NoChanges("japlus");
 }
 Reset();mod=Bytes(std::string(63,'x'));CL_JKAParseSetGame(&mod);assert(game.size()==63 && restarts==1);
 Reset();mod=Bytes("base");mod.data.push_back(9);mod.cursize++;CL_JKAParseSetGame(&mod);assert(mod.readcount==5 && MSG_ReadByte(&mod)==9);NoChanges("");

 Reset();msg_t controls=Bytes("japlus");controls.data.insert(controls.data.begin(),8);controls.data.push_back(9);controls.data.push_back(10);controls.cursize=static_cast<int>(controls.data.size());
 ParseControls(&controls);assert(game=="japlus" && restarts==1 && mapCalls==1 && lastCanonical==CG_RAVEN_MAP_CHANGE && lastExport==27);
 assert(controls.readcount==controls.cursize);
 Reset();controls={{9,9,10},0,3};ParseControls(&controls);assert(mapCalls==2 && !restarts);
 Reset();cgvm=nullptr;controls={{9,10},0,2};ParseControls(&controls);assert(!mapCalls);
 Reset();controls={{10,8,'x',0},0,4};ParseControls(&controls);assert(controls.readcount==1);NoChanges("");
 Reset();active=NETCHAN_WIRE_LEGACY_Q3;controls={{8,'x',0},0,3};ParseControls(&controls);assert(controls.readcount==1);NoChanges("");
 Reset();controls={{8,'x'},0,2};Reject([&] { ParseControls(&controls); });NoChanges("");

 // MSG_ReadString would alter percent/high bytes. These controls preserve
 // the bounded raw bytes the selected profile actually put on the wire.
 Reset();serverGame=std::string("mod%")+static_cast<char>(0xe1);controls={};
 SV_WriteJKAGameDirectory(&controls,NETCHAN_WIRE_JKA_MP);MSG_WriteByte(&controls,10);
 assert(controls.data[4]=='%' && controls.data[5]==0xe1);ParseControls(&controls);
 assert(game==serverGame && restarts==1 && controls.readcount==controls.cursize);

 Reset();serverGame="japlus";client_t client{{NETCHAN_WIRE_JKA_MP}};msg_t server;
 ServerPrefix(&client,server);assert((server.data==std::vector<byte>{8,'j','a','p','l','u','s',0,2,4,3,2,1}));
 msg_t tail;ServerTail(&client,tail);assert((tail.data==std::vector<byte>{10,3,0,0,0,0x44,0x33,0x22,0x11,0,0}));
 msg_t demo;ServerDemoTail(&client,demo);assert(demo.data==tail.data);
 demo={};ClientDemoTail(demo,false);assert((demo.data==std::vector<byte>{10,3,0,0,0,0x78,0x56,0x34,0x12,0,0,10}));
 msg_t budget;BudgetPrefix(budget,NETCHAN_WIRE_JKA_MP);assert(budget.data[0]==8 && budget.data[8]==2);
 budget={};BudgetTail(budget,NETCHAN_WIRE_JKA_MP);assert((budget.data==std::vector<byte>{10,7,0,0,0,0x44,0x33,0x22,0x11,0,0,10}));
 for(auto profile:{NETCHAN_WIRE_LEGACY_Q3,NETCHAN_WIRE_IOQ3,NETCHAN_WIRE_QL_RETAIL,NETCHAN_WIRE_RTCW_SP,
  NETCHAN_WIRE_RTCW_MP,NETCHAN_WIRE_WOLFET,NETCHAN_WIRE_IOEF,NETCHAN_WIRE_JK2_MP,NETCHAN_WIRE_MOHAA}) {
  client.netchan.wireProfile=profile;server={};ServerPrefix(&client,server);assert(server.data[0]==2 && server.data.size()==5);
  tail={};ServerTail(&client,tail);assert(tail.data[0]==Netchan_ServerEOFOpcode(profile) && tail.data.size()==9);
  demo={};ServerDemoTail(&client,demo);assert(demo.data==tail.data);
  active=profile;demo={};ClientDemoTail(demo,false);assert(demo.data.size()==10 && demo.data.front()==Netchan_ServerEOFOpcode(profile) && demo.data.back()==Netchan_ServerEOFOpcode(profile));
  budget={};BudgetPrefix(budget,profile);assert(budget.data[0]==2);
  budget={};BudgetTail(budget,profile);assert(budget.data.size()==10 && budget.data.front()==Netchan_ServerEOFOpcode(profile) && budget.data.back()==Netchan_ServerEOFOpcode(profile));
  auto noTrailer=Bytes("x");ClientGamestateTrailer(&noTrailer);assert(noTrailer.readcount==0);
 }
 active=NETCHAN_WIRE_JKA_MP;demo={};ClientDemoTail(demo,true);assert((demo.data==std::vector<byte>{0}));
 auto rmg=Bytes("");rmg.data={0,0,10};rmg.cursize=3;ClientGamestateTrailer(&rmg);assert(rmg.readcount==2 && MSG_ReadByte(&rmg)==10);
 for(const std::vector<byte> &truncated:std::vector<std::vector<byte>>{{},{0},{10}}) {
  rmg={truncated,0,static_cast<int>(truncated.size())};Reject([&] { ClientGamestateTrailer(&rmg); });assert(dropReason.find("truncated")!=std::string::npos);
 }
 for(const int length:{1,255,32768,65535}) {
  rmg={};MSG_WriteShort(&rmg,length);MSG_WriteByte(&rmg,10);Reject([&] { ClientGamestateTrailer(&rmg); });
  assert(rmg.readcount==2 && dropReason.find("not served yet")!=std::string::npos && MSG_ReadByte(&rmg)==10);
 }
 for(const std::string &bad:std::vector<std::string>{".","../other",std::string(64,'x')}) {
  serverGame=bad;server={};Reject([&] { SV_WriteJKAGameDirectory(&server,NETCHAN_WIRE_JKA_MP); });assert(server.data.empty());
 }
 serverGame=std::string(63,'x');server={};SV_WriteJKAGameDirectory(&server,NETCHAN_WIRE_JKA_MP);assert(server.data.size()==65 && server.data.front()==8 && server.data.back()==0);
}
'''


class JediAcademyFramingTests(unittest.TestCase):
    def test_production_controls_and_gamestate_framing(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler unavailable')
        client = (ROOT / 'code/client/cl_parse.cpp').read_text(encoding='utf-8')
        client_main = (ROOT / 'code/client/cl_main.cpp').read_text(encoding='utf-8')
        server = (ROOT / 'code/server/sv_client.cpp').read_text(encoding='utf-8')
        filesystem = (ROOT / 'code/qcommon/files.c').read_text(encoding='utf-8')
        canonical = (ROOT / 'code/cgame/cg_public.h').read_text(encoding='utf-8')
        enums = '\n'.join(enum(canonical, member) for member in ('CG_INIT,', 'CG_GET_TAG =', 'CGAME_EVENT_NONE,'))
        body = PRELUDE + enums + '\n#include "code/compat/jka_cgame_exports.h"\n'
        body += function(filesystem, 'qboolean FS_InvalidGameDir(')
        body += '\n' + function(client, 'static void CL_JKAParseSetGame(')
        body += '\n' + function(client, 'static void CL_JKAParseRMGTrailer(')
        gamestate = function(client, 'static void CL_ParseGamestate(')
        trailer = block(gamestate, 'if ( CL_ActiveWireProfile() == NETCHAN_WIRE_JKA_MP )')
        body += '\nvoid ClientGamestateTrailer(msg_t *msg) {\n' + trailer + '\n}\n'
        parser = function(client, 'void CL_ParseServerMessage(')
        control = block(parser, 'if ( CL_ActiveWireProfile() == NETCHAN_WIRE_JKA_MP &&')
        body += r'''
void VM_Call(int *target,int zero,int operation) {
 assert(target==cgvm && zero==0);++mapCalls;lastCanonical=operation;lastExport=FNTech3_JKACgameExportOrdinal(operation);
 assert(FNTech3_JKACgameExportArgumentCount(operation)==0);
}
void ParseControls(msg_t *msg) {
 while(true) {
  const int cmd=MSG_ReadByte(msg);if(cmd==Netchan_ServerEOFOpcode(CL_ActiveWireProfile()))break;
''' + control + '\nthrow Drop{};\n }\n}\n'
        body += function(server, 'static void SV_WriteJKAGameDirectory(')
        budget = function(server, 'int SV_RemainingGameState(')
        sender = function(server, 'void SV_SendClientGameState(')
        for name, source, signature in (
            ('BudgetPrefix', budget, 'msg_t &msg,netchanWireProfile_t wire'),
            ('ServerPrefix', sender, 'client_t *client,msg_t &msg')):
            start = source.index('\tSV_WriteJKAGameDirectory(')
            end = source.index('\n\t// write the configstrings', start)
            body += f'\nvoid {name}({signature}) {{\n{source[start:end]}\n}}\n'
        start = budget.index('\tMSG_WriteByte( &msg, Netchan_ServerEOFOpcode( wire ) );')
        end = budget.index('\n\tlen = PAD(', start)
        body += '\nvoid BudgetTail(msg_t &msg,netchanWireProfile_t wire) {\n' + budget[start:end] + '\n}\n'
        start = sender.index('\tif ( SV_LegacyFraming( client ) ) {', sender.index('// write the baselines'))
        end = sender.index('\n\t// it is important to handle gamestate overflow', start)
        body += '\nvoid ServerTail(client_t *client,msg_t &msg) {\n' + sender[start:end] + '\n}\n' + CHECKS
        demo = function(server, 'static void SV_WriteDemoGamestate(')
        start = demo.index('\tMSG_WriteByte( &msg, Netchan_ServerEOFOpcode(')
        end = demo.index('\n\tif ( msg.overflowed )', start)
        body = body.removesuffix(CHECKS)
        body += '\nvoid ServerDemoTail(client_t *client,msg_t &msg) {\n' + demo[start:end] + '\n}\n'
        demo = function(client_main, 'static void CL_WriteGamestate(')
        start = demo.index('\tif ( stvef ) {', demo.index('// finalize message'))
        end = demo.index('\n\t// write it to the demo file', start)
        body += '\nvoid ClientDemoTail(msg_t &msg,bool stvef) {\n' + demo[start:end] + '\n}\n' + CHECKS
        SCRATCH.mkdir(parents=True, exist_ok=True)
        self.assertFalse(is_link(SCRATCH))
        folder = Path(tempfile.mkdtemp(prefix='framing-', dir=SCRATCH))
        try:
            cpp = folder / 'probe.cpp'
            executable = folder / ('probe.exe' if os.name == 'nt' else 'probe')
            cpp.write_text(body, encoding='utf-8')
            target = compiler_target_args(compiler)
            if not target and os.name == 'nt':
                target = ['-DFNTECH3_Q3_ONLY'] if configured_target_is_windows_x64() else ['-m32']
            compiler_env = os.environ.copy()
            compiler_env.update(TMP=str(folder), TEMP=str(folder), TMPDIR=str(folder))
            built = subprocess.run([compiler, *target, '-std=c++17', '-D_CRT_SECURE_NO_WARNINGS',
                '-I', str(ROOT), str(cpp), '-o', str(executable)], cwd=ROOT,
                env=compiler_env, capture_output=True, text=True, timeout=60)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            result = subprocess.run([str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=15)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        finally:
            self.assertTrue(folder.resolve().is_relative_to(SCRATCH.resolve()))
            self.assertFalse(is_link(folder))
            self.assertFalse(any(is_link(path) for path in folder.rglob('*')))
            shutil.rmtree(folder)


if __name__ == '__main__':
    unittest.main()
