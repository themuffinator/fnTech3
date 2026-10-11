"""Exercise Academy's production clock boundaries in separate translation units."""
from pathlib import Path
import os
import re
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]


def read(path):
    return (ROOT / path).read_text(encoding='utf-8')


def function(source, signature):
    start = source.index(signature)
    opening = source.index('{', start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end] + '\n'


def declaration(source, name):
    return re.search(r'^.*&' + re.escape(name) + r'\s*=.*;$', source, re.M).group() + '\n'


COMMON = r'''
#pragma once
#include "code/compat/jka_ghoul2_clock.hpp"
#include "code/compat/jka_module_abi.h"
#include <cassert>
#include <climits>
#include <stdexcept>
using fntech3::jka::module::Call;
enum { ERR_DROP, VM_CGAME };
struct Drop {};
[[noreturn]] inline void Com_Error(int, const char*, ...) { throw Drop{}; }
struct Cleanup { int count=0; void Clear(){++count;} void reset(){++count;} };
void ServerProfile(comGameId_t);
void ServerSet(int);
int ServerTime();
bool* ServerSelector();
void ServerClear();
void ClientSet(int,int);
int ClientTime();
void ClientFrame(int,bool);
bool ClientReady();
bool ClientLocal();
bool* ClientSelector();
void ClientClear();
void UISet(int,int);
int UITime();
bool* UISelector();
void UIClear();
'''


def server_fixture():
    source = read('code/server/server_jka_game.hpp')
    result = r'''
#include "fixture.hpp"
static comGameId_t profile=COM_GAME_JKA_MP;
int Com_GameIsJKAMP(){return profile==COM_GAME_JKA_MP;}
static void* jkaSharedBuffer;
static float jkaServerCullDistance;
static Cleanup jkaNavigation;
static void SV_JKAIcarusClear(){}
static void SV_RavenAssetsClear(){}
'''
    result += declaration(source, 'jkaServerNoRotationPending')
    for name in ('static int SV_JKAGhoul2Time()', 'void SV_JKAGhoul2SetTime( int time )',
                 'static void SV_JKAClearGame( void )'):
        result += function(source, name)
    return result + r'''
void ServerProfile(comGameId_t value){profile=value;}
void ServerSet(int value){SV_JKAGhoul2SetTime(value);}
int ServerTime(){return SV_JKAGhoul2Time();}
bool* ServerSelector(){return &jkaServerNoRotationPending;}
void ServerClear(){SV_JKAClearGame();assert(jkaNavigation.count==1);}
'''


def client_fixture():
    source = read('code/client/client_jka_cgame_ghoul2.hpp')
    result = r'''
#include "fixture.hpp"
namespace fntech3::jka::cgame::scene { static Cleanup state; }
static Cleanup jkaCgameGhoul2Store, skins;
static void CL_JKACgameSkinClear(){skins.Clear();}
static void CL_JKACgameGhoul2Native(){}
static struct { int index=VM_CGAME; } vm;
static auto* cgvm=&vm;
'''
    result += declaration(source, 'jkaCgameBoneClocks')
    result += declaration(source, 'jkaCgameNoRotationPending')
    for name in ('inline void CL_JKACgameGhoul2BeginFrame(',
                 'static void CL_JKACgameGhoul2Clear()'):
        result += function(source, name)
    # Execute the production syscall cases. Only unrelated VM/renderer services
    # are stubbed; clock selection, validation and publication are not copied.
    start = source.index('    case Call::CG_G2_GETTIME:')
    end = source.index('    case Call::CG_G2_GETBOLT_NOREC_NOROT:', start)
    result += 'static bool Dispatch(intptr_t* args,Call call,intptr_t& result){switch(call){\n'
    result += source[start:end] + 'default:return false;}}\n'
    return result + r'''
void ClientSet(int index,int value){
 intptr_t args[]{0,value,index},result=-1;
 assert(Dispatch(args,Call::CG_G2_SETTIME,result)&&result==0);
}
int ClientTime(){intptr_t args[]{0},result=-1;assert(Dispatch(args,Call::CG_G2_GETTIME,result));return int(result);}
void ClientFrame(int value,bool local){CL_JKACgameGhoul2BeginFrame(value,local);}
bool ClientReady(){return jkaCgameBoneClocks.FrameReady();}
bool ClientLocal(){return jkaCgameBoneClocks.LocalServer();}
bool* ClientSelector(){return &jkaCgameNoRotationPending;}
void ClientClear(){
 CL_JKACgameGhoul2Clear();
 assert(fntech3::jka::cgame::scene::state.count==1&&skins.count==1&&jkaCgameGhoul2Store.count==1);
}
'''


def ui_fixture():
    source = read('code/client/client_jka_ui_ghoul2.hpp')
    result = r'''
#include "fixture.hpp"
static Cleanup jkaUIGhoul2Store;
static void CL_JKAUIGhoul2Native(){}
'''
    for name in ('static int CL_JKAUIGhoul2Time()', 'static void CL_JKAUIGhoul2Clear()'):
        result += function(source, name)
    start = source.index('\tcase Call::UI_G2_GETTIME:')
    end = source.index('\tcase Call::UI_G2_INITGHOUL2MODEL:', start)
    result += 'static bool Dispatch(intptr_t* args,Call call,intptr_t& result){switch(call){\n'
    result += source[start:end] + 'default:return false;}}\n'
    return result + r'''
void UISet(int index,int value){
 intptr_t args[]{0,value,index},result=-1;
 assert(Dispatch(args,Call::UI_G2_SETTIME,result)&&result==0);
}
int UITime(){intptr_t args[]{0},result=-1;assert(Dispatch(args,Call::UI_G2_GETTIME,result));return int(result);}
// UI has no served no-rotation import; observe its translation unit's storage
// without inventing a UI selector service.
bool* UISelector(){return &fntech3::jka::clock::noRotationPending;}
void UIClear(){CL_JKAUIGhoul2Clear();assert(jkaUIGhoul2Store.count==1);}
'''


MAIN = r'''
#include "fixture.hpp"
static void Time(int expected){assert(ServerTime()==expected&&ClientTime()==expected&&UITime()==expected);}
int main(){
 Time(0);assert(!ClientReady()&&!ClientLocal());
 assert(ServerSelector()==ClientSelector()&&ClientSelector()==UISelector());
 assert(!*ServerSelector());
 ServerSet(1000);Time(1000);
 UISet(1,1200);Time(1200); // The +200 bound is inclusive.
 ServerSet(999);Time(999); // Updating server time also applies the cutoff.
 ClientSet(0,2000);ClientSet(1,2100);Time(2100);
 for(unsigned id=COM_GAME_AUTO;id<COM_GAME_COUNT;++id){
  if(id==COM_GAME_JKA_MP)continue;
  ServerProfile(static_cast<comGameId_t>(id));ServerSet(0);Time(2100);
 }
 ServerProfile(COM_GAME_JKA_MP);
 ClientFrame(2150,true);Time(2150);assert(ClientReady()&&ClientLocal());
 ClientFrame(2201,true);Time(2000); // Local frames retain server clock 0.
 ClientFrame(700,false);Time(700);assert(ClientReady()&&!ClientLocal());
 UISet(1,0);Time(700); // Zero is the retail fallback sentinel.
 UISet(0,INT32_MAX-50);UISet(1,INT32_MAX-51);Time(INT32_MAX-50);
 ClientSet(0,INT32_MIN+20);Time(INT32_MIN+20);
 ClientSet(1,INT32_MIN+220);Time(INT32_MIN+220);
 ClientSet(1,INT32_MIN+221);Time(INT32_MIN+20);
 for(int index:{INT_MIN,-1,2,INT_MAX}){
  try{UISet(index,17);assert(false);}catch(const Drop&){}
  Time(INT32_MIN+20);
  try{ClientSet(index,17);assert(false);}catch(const Drop&){}
  Time(INT32_MIN+20);
 }
 ClientSet(1,0);Time(INT32_MIN+20); // Invalid writes changed neither slot.
 ServerSet(500);ClientFrame(550,true);Time(550);
 *ServerSelector()=true;assert(*ClientSelector()&&*UISelector());
 ClientClear();Time(550);assert(!ClientReady()&&!ClientLocal());
 assert(*ServerSelector()&&*ClientSelector()&&*UISelector());
 UIClear();ServerClear();Time(550);assert(*ClientSelector());
 *ClientSelector()=false;assert(!*ServerSelector()&&!*UISelector());
 ClientFrame(575,true);Time(575);assert(ClientReady()&&ClientLocal());
}
'''


class JKAGhoul2ClockTests(unittest.TestCase):
    def test_production_boundaries_share_process_globals(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler unavailable')
        scratch = (ROOT / '.tmp/tests/jka-ghoul2-clock').resolve()
        scratch.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=scratch) as folder:
            folder = Path(folder)
            (folder / 'fixture.hpp').write_text(COMMON, encoding='utf-8')
            sources = []
            for name, source in [('server', server_fixture()), ('client', client_fixture()),
                                 ('ui', ui_fixture()), ('main', MAIN)]:
                path = folder / (name + '.cpp')
                path.write_text(source, encoding='utf-8')
                sources.append(str(path))
            binary = folder / ('probe.exe' if os.name == 'nt' else 'probe')
            compiled = subprocess.run(
                [compiler, *compiler_target_args(compiler), '-std=c++17', '-Wall', '-Wextra',
                 '-Werror', '-I', str(ROOT), *sources, '-o', str(binary)],
                cwd=folder, capture_output=True, text=True, timeout=120)
            self.assertEqual(compiled.returncode, 0, compiled.stdout + compiled.stderr)
            options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
            result = subprocess.run([str(binary)], cwd=folder, capture_output=True, text=True,
                                    timeout=30, **options)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == '__main__':
    unittest.main()
