"""The level clock across a map change, per profile, through production code.

Retail jamp.exe 1.01 and jk2mp.exe 1.02 run their game on the server's own
clock, svs.time: GAME_INIT takes it (0x45dd50, 0x44cf54), every settling and
game frame advances it (0x45eb5d, 0x460856; 0x44da8e, 0x44f37f), and only
SV_Shutdown clears it, so a `map` carries on from the last level's time
whoever is connected. So do quake3.exe 1.32 (0x438514), WolfSP.exe
(0x435fd5), WolfMP.exe (0x4389d7), etded.exe 2.60b (0x42bcb8) and stvoyHM.exe
1.20 (0x433268). FRAME_KEEP_LEVEL_TIME says so for those seven profiles.
quakelive_steam.exe clears its level clock with every level, whoever is
connected, and gives each connected client the old time as an offset
(0x4e35f6, 0x4e363a): FRAME_RESET_LEVEL_TIME. Raven's single-player engines
clear theirs with every spawn and start the level at 1000, whoever is
connected: stvoy.exe 1.20 (0x425ac9), jasp.exe 1.01 (0x439d35) and jk2sp.exe
1.02a (0x437250), FRAME_LEVEL_TIME_1000. Every other profile keeps Quake3e's
rule here: an empty server's map change, or sv_levelTimeReset, starts the
clock again. This compiles the production rule table and SV_SpawnServer's
level-time block with SV_ClearServer and its start at 1000. See
docs/fntech3/2026-10-15-level-clock.md and
docs/fntech3/2026-10-16-jedi-sp-level-clock.md.
"""
# Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later.
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]

HOST = r'''
#include "code/server/server.h"
#include "code/qcommon/profile_frame_time.h"
#include <cassert>
#include <cstdio>
#include <vector>
server_t sv{};
serverStatic_t svs{};
static comGameProfile_t current{};
const comGameProfile_t *Com_GameProfile( void ){return &current;}
static cvar_t resetCvar{};
cvar_t *sv_levelTimeReset=&resetCvar;
int Com_GameConfigstringLimit( void ){return 4;}
void Z_Free( void * ){}
int Com_GameIsRitual( void ){return current.id==COM_GAME_ALICE||current.id==COM_GAME_FAKK2;}
'''

CHECKS = r'''
static std::vector<client_t> slots(2);
static void Spawn(){
SPAWN_BLOCK
START_BLOCK
}
static bool Kept(comGameId_t id){
 return id==COM_GAME_QUAKE3||id==COM_GAME_RTCW_SP||id==COM_GAME_RTCW_MP||id==COM_GAME_WOLFET
  ||id==COM_GAME_STVEF_MP||id==COM_GAME_JKA_MP||id==COM_GAME_JK2_MP;
}
static bool Reset(comGameId_t id){return id==COM_GAME_QUAKELIVE;}
static bool Start1000(comGameId_t id){
 return id==COM_GAME_STVEF_SP||id==COM_GAME_JKA_SP||id==COM_GAME_JK2_SP;
}
int main(){
 // Every profile's other clock rules stay as they were.
 const unsigned base=FRAME_RETAIL|FRAME_FIXED_TICK|FRAME_X87_PRODUCT;
 const unsigned before[COM_GAME_COUNT]={
  0,                                                        // auto
  base|FRAME_CAMERA_SCALE,                                  // q3
  FRAME_RETAIL|FRAME_X87_PRODUCT|FRAME_CAMERA_SCALE,        // ql
  base,base,                                                // rtcw-sp, rtcw-mp
  FRAME_RETAIL|FRAME_FIXED_TICK,                            // wolfet
  base|FRAME_ALWAYS_FLOOR|FRAME_CINEMATIC_CAP,              // stvef-sp
  base|FRAME_ALWAYS_FLOOR,                                  // stvef-mp
  base|FRAME_ALWAYS_FLOOR|FRAME_CAP_200|FRAME_FIXEDTIME_FLOAT, // q3-ihv
  base|FRAME_ALWAYS_FLOOR|FRAME_CINEMATIC_CAP|FRAME_FRACTION,  // jka-sp
  base|FRAME_CAMERA_SCALE|FRAME_FLOAT_INPUT,                // jka-mp
  base|FRAME_ALWAYS_FLOOR|FRAME_CINEMATIC_CAP|FRAME_FRACTION,  // jk2-sp
  base|FRAME_CAMERA_SCALE,                                  // jk2-mp
  base|FRAME_ALWAYS_FLOOR,                                  // alice
  0,0,                                                      // fakk2, mohaa
  base|FRAME_ALWAYS_FLOOR|FRAME_CAP_200                     // q3test
 };
 for(unsigned n=0;n<COM_GAME_COUNT;++n){
  const comGameId_t id=static_cast<comGameId_t>(n);
  const unsigned rules=Com_ProfileFrameRules(id);
  assert((rules&~unsigned(FRAME_KEEP_LEVEL_TIME|FRAME_RESET_LEVEL_TIME|FRAME_LEVEL_TIME_1000))==before[n]);
  assert(((rules&FRAME_KEEP_LEVEL_TIME)!=0)==Kept(id));
  assert(((rules&FRAME_RESET_LEVEL_TIME)!=0)==Reset(id));
  assert(((rules&FRAME_LEVEL_TIME_1000)!=0)==Start1000(id));
  // No profile both keeps the clock and restarts it.
  assert(!(Kept(id)&&(Reset(id)||Start1000(id)))&&!(Reset(id)&&Start1000(id)));
 }
 svs.clients=slots.data();
 for(unsigned n=1;n<COM_GAME_COUNT;++n){
  current.id=static_cast<comGameId_t>(n);
  const bool ritual=Com_GameIsRitual()!=0;
  for(int reset:{0,1})for(bool connected:{false,true})for(int pending:{0,7000}){
   std::memset(&sv,0,sizeof(sv));sv.maxclients=2;sv.time=5400;sv.restartTime=pending;
   resetCvar.integer=reset;
   for(client_t &client:slots){std::memset(&client,0,sizeof(client));client.oldServerTime=-1;}
   slots[0].state=connected?CS_ACTIVE:CS_FREE;slots[1].state=CS_FREE;
   Spawn();
   int time=5400,old=0;
   if(!Kept(current.id)){
    // Quake Live always starts the level again, as sv_levelTimeReset does.
    const bool resets=reset||Reset(current.id);
    if(resets)time=0;
    else if(!connected&&!pending)time=0;
    if(connected&&(resets||ritual))old=resets?5400:time;
   }
   // Raven's single-player levels start at 1000 whatever the rest decided;
   // only sv_levelTimeReset's offset for a connected client is kept.
   if(Start1000(current.id))time=1000;
   if(sv.time!=time||slots[0].oldServerTime!=old||slots[1].oldServerTime!=0||sv.maxclients!=2){
    std::printf("profile %u reset %d connected %d pending %d: time %d (want %d) old %d (want %d)\n",
     n,reset,int(connected),pending,sv.time,time,slots[0].oldServerTime,old);
    return 1;
   }
   // The clear still wipes the rest of the level, a pending restart included.
   assert(sv.restartTime==0&&sv.state==SS_DEAD);
  }
 }
 // A shutdown still clears the clock: SV_Shutdown zeroes it after the clear.
 return 0;
}
'''


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2]


class JediLevelTimeTests(unittest.TestCase):
    def setUp(self):
        self.init = (ROOT / 'code/server/sv_init.cpp').read_text(encoding='utf-8')

    def spawn_block(self):
        spawn = self.init[self.init.index('void SV_SpawnServer( const char *mapname, qboolean killBots ) {'):]
        start = spawn.index('\t// try to reset level time if server is empty')
        end = spawn.index('\tsv.maxclients = preservedMaxClients;\n', start)
        return spawn[start:end + len('\tsv.maxclients = preservedMaxClients;\n')]

    def start_block(self):
        spawn = self.init[self.init.index('void SV_SpawnServer( const char *mapname, qboolean killBots ) {'):]
        start = spawn.index('\tif ( SV_LevelTimeStartsAt1000() ) {\n')
        end = spawn.index('\t}\n', start) + len('\t}\n')
        return spawn[start:end]

    def test_map_change_keeps_the_jedi_level_clock(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler unavailable')
        helpers = (function(self.init, 'static bool SV_KeepsLevelTime( void ) {') + '\n'
                   + function(self.init, 'static bool SV_LevelTimeResets( void ) {') + '\n'
                   + function(self.init, 'static bool SV_LevelTimeStartsAt1000( void ) {') + '\n'
                   + function(self.init, 'static void SV_ClearServer( void ) {') + '\n')
        source = HOST + helpers + CHECKS.replace('SPAWN_BLOCK', self.spawn_block()).replace(
            'START_BLOCK', self.start_block())
        scratch = ROOT / '.tmp' / 'jedi-level-time'
        scratch.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=scratch) as directory:
            folder = Path(directory)
            path = folder / 'level_time.cpp'
            executable = folder / ('level_time.exe' if os.name == 'nt' else 'level_time')
            path.write_text(source, encoding='utf-8')
            target = compiler_target_args(compiler)
            if not target:
                target = ['-DFNTECH3_Q3_ONLY'] if os.name == 'nt' else ['-m32']
            built = subprocess.run([compiler, *target, '-std=c++17', '-I', str(ROOT), str(path),
                                    '-o', str(executable)], cwd=ROOT, capture_output=True,
                                   text=True, timeout=120)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            run = subprocess.run([str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=30)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)

    def test_spawn_and_shutdown_order(self):
        # The level-time decisions precede the clear that keeps or zeroes the
        # clock, and a shutdown zeroes it after its own clear.
        block = self.spawn_block()
        self.assertLess(block.index('SV_KeepsLevelTime()'), block.index('SV_ClearServer();'))
        self.assertLess(block.index('SV_LevelTimeResets() || Com_GameIsRitual()'), block.index('SV_ClearServer();'))
        shutdown = self.init[self.init.index('void SV_Shutdown( const char *finalmsg )'):]
        self.assertLess(shutdown.index('SV_ClearServer();'), shutdown.index('std::memset( &svs, 0, sizeof( svs ) );'))
        self.assertLess(shutdown.index('std::memset( &svs, 0, sizeof( svs ) );'), shutdown.index('sv.time = 0;'))
        # Raven's single-player levels start at 1000 after the clear and
        # before the game's GAME_INIT and settling frames take the clock.
        spawn = self.init[self.init.index('void SV_SpawnServer( const char *mapname, qboolean killBots ) {'):]
        spawn = spawn[:spawn.index('\n}\n')]
        first = spawn.index(self.start_block())
        self.assertLess(spawn.index('\tSV_ClearServer();\n'), first)
        self.assertLess(first, spawn.index('\tSV_InitGameProgs();\n'))
        self.assertEqual(spawn.count('SV_LevelTimeStartsAt1000()'), 1)
        self.assertNotIn('Com_GameIsSTVEFSP() ) {\n\t\tsv.time = 1000;', spawn)
        frame = function((ROOT / 'code/server/sv_main.cpp').read_text(encoding='utf-8'),
                         'void SV_Frame( int msec, int realMsec ) {')
        self.assertIn('( rules & FRAME_KEEP_LEVEL_TIME )', frame)
        self.assertLess(frame.index('const unsigned int rules = Com_ProfileFrameRules('),
                        frame.index('( rules & FRAME_KEEP_LEVEL_TIME )'))


if __name__ == '__main__':
    unittest.main()
