"""Execute map settling frames against retail SP clock observations."""
from pathlib import Path
import json
import os
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]

HARNESS = r'''
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include "code/qcommon/game_profile.h"
#undef assert
#define assert(test) do { if (!(test)) { std::fprintf(stderr,"line %d: %s\n",__LINE__,#test); std::exit(1); } } while(0)
static struct { int time; } sv, svs;
static comGameId_t profile;
int Com_GameIsSTVEFSP() { return profile == COM_GAME_STVEF_SP; }
static void *gvm = reinterpret_cast<void *>(1);
enum { GAME_RUN_FRAME=8 };
struct Event { int kind, argument, clock; };
static std::vector<Event> events;
static void Cbuf_Wait() { events.push_back({0,sv.time,sv.time}); }
static int VM_Call(void *vm, int count, int ordinal, int time) {
    assert(vm==gvm && count==1 && ordinal==GAME_RUN_FRAME);
    events.push_back({1,time,sv.time}); return 0;
}
static void SV_BotFrame(int time) { events.push_back({2,time,sv.time}); }
static std::vector<int> SV_Indices(int count) {
    std::vector<int> result;
    for(int i=0;i<count;++i) result.push_back(i);
    return result;
}
HELPER
static void FirstThree() { FIRST }
static void LastFrame() { LAST }
int main() {
    int start,afterThree,final,times[4],clocks[4],rows=0;
    while(std::scanf("%d%d%d",&start,&afterThree,&final)==3) {
        for(int i=0;i<4;++i) assert(std::scanf("%d%d",times+i,clocks+i)==2);
        for(int p=COM_GAME_QUAKE3;p<COM_GAME_COUNT;++p) {
            profile=static_cast<comGameId_t>(p); sv.time=start; svs.time=17; events.clear();
            FirstThree(); assert(sv.time==afterThree && events.size()==9 && svs.time==17);
            LastFrame(); assert(sv.time==final && events.size()==12 && svs.time==117);
            for(int i=0;i<4;++i) {
                const int expectedTime=Com_GameIsSTVEFSP()?times[i]:start+(i+1)*100;
                const int expectedClock=Com_GameIsSTVEFSP()?clocks[i]:expectedTime;
                const Event wait=events[i*3],game=events[i*3+1],bot=events[i*3+2];
                assert(wait.kind==0 && wait.clock==start+i*100);
                if(game.kind!=1 || game.argument!=expectedTime || game.clock!=expectedClock ||
                   bot.kind!=2 || bot.argument!=expectedTime || bot.clock!=expectedClock) {
                    std::fprintf(stderr,"profile %d start %d frame %d: argument %d/%d engine clock %d/%d\n",
                        p,start,i,game.argument,expectedTime,game.clock,expectedClock);
                    return 1;
                }
            }
        }
        ++rows;
    }
    assert(rows==7);
    std::puts("49 startup sequences / 196 frame boundaries; all seven profiles passed");
}
'''


class STVEFSPStartupTests(unittest.TestCase):
    def test_native_startup_frame_sequence(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler unavailable')
        source = (ROOT / 'code/server/sv_init.cpp').read_text()
        spawn = source[source.index('void SV_SpawnServer('):]
        first = spawn[spawn.index('// run a few frames'):spawn.index('// create a baseline')]
        last = spawn[spawn.index('// run another frame'):spawn.index('// Publish the selected profile')]
        helper = ''
        if 'static void SV_RunStartupFrame(' in source:
            begin = source.index('static void SV_RunStartupFrame(')
            helper = source[begin:source.index('\n}\n', begin)+3]
        fixture = json.loads((ROOT/'tests/fixtures/stvef_sp_startup_retail.json').read_text())
        inputs = []
        for row in fixture['cases']:
            values = [row['start'],row['afterThree'],row['final']]
            values += [value for frame in row['frames'] for value in (frame['argument'],frame['engineTime'])]
            inputs.append(' '.join(map(str,values)))
        sanitizer = [] if os.name=='nt' else ['-fsanitize=address,undefined','-fno-sanitize-recover=all']
        with tempfile.TemporaryDirectory(prefix='stvef-startup-') as folder:
            cpp = Path(folder)/'probe.cpp'
            exe = Path(folder)/('probe.exe' if os.name=='nt' else 'probe')
            cpp.write_text(HARNESS.replace('HELPER',helper).replace('FIRST',first).replace('LAST',last))
            built = subprocess.run([compiler,*compiler_target_args(compiler),'-std=c++17','-O1',
                '-D_CRT_SECURE_NO_WARNINGS',*sanitizer,'-I',str(ROOT),str(cpp),'-o',str(exe)],
                cwd=ROOT,capture_output=True,text=True,timeout=120)
            self.assertEqual(built.returncode,0,built.stdout+built.stderr)
            options = {'creationflags':subprocess.CREATE_NO_WINDOW} if os.name=='nt' else {}
            ran = subprocess.run([str(exe)],input='\n'.join(inputs)+'\n',cwd=ROOT,
                capture_output=True,text=True,timeout=30,**options)
            self.assertEqual(ran.returncode,0,ran.stdout+ran.stderr)


if __name__=='__main__':
    unittest.main()
