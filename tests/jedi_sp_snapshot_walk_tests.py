"""Raven's single-player snapshot rules through the production snapshot builder.

Retail jasp.exe (Jedi Academy, SV_AddEntitiesVisibleFromPoint 0x43c710) and
jk2sp.exe (Jedi Outcast, 0x43a170) skip an entity not in use (0x43c7a3,
0x43a1f1) and repair the number of what is left, linked or not; test no
client flag but SVF_NOCLIENT; always send the player's entity 0 (0x43c80a,
0x43a25b), which their builders never mark sent first; walk a portal's view
with their portal argument set; and stop adding at 1,024 entities, or one
fewer than their 512- and 256-state rings on an odd snapshot (0x43c5dd,
0x43a3a5). Academy alone skips a permanent entity (eFlags 0x80000,
0x43c7a9), sends one seen from the sky portal (isPortalEnt, 0x43c80e), and
in the eye's pass under Force Sight (0x43c762) adds, without walking it,
anything the power shows from its client game's camera (0x43c600), whose
position is also its builder's viewpoint (0x43c9d3). See
docs/fntech3/2026-10-16-jedi-sp-snapshot-walk.md.

This compiles the production builder, walk, common snapshot, viewpoint and
SV_JKASPForceSightSees over the shared fake game-data view, with Quake III
and Jedi Academy multiplayer as controls.
"""
# Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later.
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args
import jedi_snapshot_walk_tests as jedi
import wolfet_snapshot_tests as snapshots

ROOT = Path(__file__).resolve().parents[1]

STUBS = (
    'qboolean SV_ReadGameEntityInuse(int,qboolean*){assert(!"The Jedi SP games use their own snapshot fixture");return qfalse;}\n',
    'qboolean SV_JKASPCameraView(vec3_t,vec3_t){assert(!"The Jedi SP games use their own snapshot fixture");return qfalse;}\n',
    'qboolean SV_JKASPForceSightSees(const vec3_t,int,int){assert(!"The Jedi SP games use their own snapshot fixture");return qfalse;}\n',
)

FAKES = r'''
#include <cstdio>
#include <cstdlib>
#undef assert
#define assert(x) do { if(!(x)){std::fprintf(stderr,"line %d profile %d: %s\n",__LINE__,int(profile),#x);std::exit(1);} } while(0)
static std::array<bool,MAX_GENTITIES> inuse{};
static int inuseReads=0;
qboolean SV_ReadGameEntityInuse(int n,qboolean *out){
 assert(profile==COM_GAME_JKA_SP||profile==COM_GAME_JK2_SP);++inuseReads;
 if(n<0||n>=sv.num_entities)return qfalse;*out=inuse[n]?qtrue:qfalse;return qtrue;
}
// The client game's camera, as CG_CAMERA_POS and CG_CAMERA_ANG answer it.
static bool cameraOn=false;
static vec3_t cameraOrigin{},cameraAngles{};
static int positionCalls=0,angleCalls=0;
qboolean SV_JKASPCameraView(vec3_t origin,vec3_t angles){
 assert(profile==COM_GAME_JKA_SP);++positionCalls;if(angles)++angleCalls;
 if(!cameraOn)return qfalse;
 VectorCopy(cameraOrigin,origin);if(angles)VectorCopy(cameraAngles,angles);return qtrue;
}
'''

EXTRA = r'''
static client_t client{};
static playerState_t player{};
static std::array<entityState_t,MAX_GENTITIES> backing{};
qboolean SV_ReadGameClient(int n,playerState_t *out){assert(n==0);*out=player;return qtrue;}
qboolean SV_ReadGameEntity(const void *pointer,sharedEntity_t *out){
 *out=*static_cast<const sharedEntity_t*>(pointer);return qtrue;
}
static void SV_AddAudioReachableEntities(const vec3_t,int,clientSnapshot_t*,SnapshotEntityNumbers&){}
int Com_GameIsRitual(){return 0;}
static void SV_BuildAliceClientSnapshot(client_t*,clientSnapshot_t*){assert(false);}
'''

CHECKS = r'''
static void Begin(comGameId_t id,int count=12){
 setup(id,count);academy={};outcastMasks={};unreadable=-1;
 std::memset(&client,0,sizeof(client));std::memset(&player,0,sizeof(player));
 std::memset(backing.data(),0,sizeof(backing));inuse.fill(true);inuseReads=0;
 cameraOn=false;VectorClear(cameraOrigin);VectorClear(cameraAngles);positionCalls=angleCalls=0;
 svs.clients=&client;sv.maxclients=1;svs.currFrame=nullptr;
 svs.snapshotEntities=backing.data();svs.numSnapshotEntities=backing.size();
 svs.freeStorageEntities=backing.size();client.state=CS_ACTIVE;client.gentity=&entities[0];
 for(int n=0;n<count;++n)sv.svEntities[n].clusternums[0]=7;
}
static std::vector<int> Build(){
 SV_BuildClientSnapshot(&client);const auto &frame=client.frames[0];
 assert(frame.num_entities>=0&&frame.num_entities<=MAX_SERVER_SNAPSHOT_ENTITIES);
 std::vector<int> out;for(int i=0;i<frame.num_entities;++i)out.push_back(frame.ents[i]->number);
 return out;
}
// Cluster 0 is the eye's, cluster 1 the next room's, cluster 7 nobody's.
static void place(int n,int cluster,int flags=0){entities[n].r.svFlags=flags;sv.svEntities[n].clusternums[0]=cluster;}
static void at(int n,float x,float y=0){entities[n].r.currentOrigin[0]=x;entities[n].r.currentOrigin[1]=y;}
static void sight(int level){
 player.raven.fd_forcePowersActive=int(fntech3::jedi::sp::snapshot::kAcademyForceSight);
 player.raven.fd_forcePowerLevel_FP_SEE=level;cameraOn=true;
}
int main(){
 int checks=0;
 const comGameId_t profiles[]={COM_GAME_JKA_SP,COM_GAME_JK2_SP,COM_GAME_QUAKE3,COM_GAME_JKA_MP};
 for(auto id:profiles){
  const bool sp=id==COM_GAME_JKA_SP||id==COM_GAME_JK2_SP;
  // Entity tests, in the common snapshot and the walk.
  Begin(id);
  place(0,1);                         // the player, out of view
  place(1,0);                         // ordinary, in view
  place(2,0);inuse[2]=false;          // not in use, in view
  place(3,0);entities[3].s.eFlags=0x80000; // Academy SP's permanent bit
  place(4,1);entities[4].s.raven.isPortalEnt=1; // seen from the sky portal
  place(5,0,SVF_CLIENTMASK);          // Raven SP: SVF_INACTIVE
  place(6,0,SVF_NOTSINGLECLIENT);     // Raven SP: SVF_NPC_PRECACHE
  place(7,0,SVF_SINGLECLIENT);entities[7].r.singleClient=5; // SVF_TRIMODEL
  place(8,0);entities[8].r.linked=qfalse;entities[8].s.number=98;
  place(9,0);inuse[9]=false;entities[9].s.number=99;
  place(10,7,SVF_BROADCAST);
  place(11,0,SVF_NOCLIENT);
  const std::vector<int> expected=id==COM_GAME_JKA_SP?std::vector<int>{0,1,4,5,6,7,10}:
   id==COM_GAME_JK2_SP?std::vector<int>{0,1,3,5,6,7,10}:
   id==COM_GAME_QUAKE3?std::vector<int>{1,2,3,9,10}:std::vector<int>{1,2,3,5,9,10};
  assert(Build()==expected);
  assert(inuseReads==(sp?12:0));
  // The SP engines repair the number of an unlinked entity in use, and not
  // of one out of use; the others only that of a linked one.
  assert(entities[8].s.number==(sp?8:98)&&entities[9].s.number==(sp?99:9));++checks;
  // The player is still left out unlinked or out of use.
  if(sp){
   Begin(id);entities[0].r.linked=qfalse;assert(Build().empty());
   Begin(id);inuse[0]=false;assert(Build().empty());++checks;
  }
  // Portals seen through portals: the SP passes are flagged, and still
  // walk the portals they see.
  Begin(id);place(1,0,SVF_PORTAL);entities[1].s.origin2[0]=100;
  place(2,1,SVF_PORTAL);place(3,1);
  assert(Build()==(sp?std::vector<int>{0,1,2,3}:std::vector<int>{1,2,3}));
  // The builder's own leaf lookup comes first.
  assert(viewpoints.size()==4&&viewpoints[2][0]==100&&viewpoints[3][0]==0);++checks;
  // Force Sight is read in Academy's single player only.
  Begin(id);for(int n=0;n<12;++n)at(n,5000);at(1,100);sight(2);
  assert(Build()==(id==COM_GAME_JKA_SP?std::vector<int>{0,1}:sp?std::vector<int>{0}:std::vector<int>{}));
  assert(angleCalls==(id==COM_GAME_JKA_SP?11:0));++checks;
  // The add caps: 1,024, and on an odd snapshot one fewer than the ring.
  // (Academy multiplayer's fixture holds 16 records.)
  if(id==COM_GAME_JKA_MP)continue;
  Begin(id,MAX_GENTITIES);
  for(int n=0;n<MAX_GENTITIES;++n)place(n,7,SVF_BROADCAST);
  const int odd=id==COM_GAME_JKA_SP?511:id==COM_GAME_JK2_SP?255:256;
  const int even=sp?MAX_GENTITIES:256;
  for(int round=0;round<3;++round){
   const auto all=Build();assert(int(all.size())==(round==1?even:odd));
   for(int i=0;i<int(all.size());++i)assert(all[i]==i+(sp?0:1));
  }
  ++checks;
 }
 // Force Sight from the camera, at level 2 (2,048 units) then 1 (1,024).
 Begin(COM_GAME_JKA_SP);
 sight(2);
 at(1,100);                          // within 128: seen
 at(2,1000);                         // straight ahead, in range: seen
 at(3,1000,1000);                    // 45 degrees off at 1,414: not
 at(4,3000);                         // past the range: not
 at(5,-50);entities[5].s.eFlags=0x800000; // force-visible, behind: not
 at(6,-50);                          // behind but within 128: seen
 at(7,5000);at(8,5000);at(9,5000);at(10,5000);at(11,5000);
 assert(Build()==(std::vector<int>{0,1,2,6}));
 // Each entity past the player's add asks the camera once; the builder
 // asked its position first.
 assert(angleCalls==11&&positionCalls==12);++checks;
 Begin(COM_GAME_JKA_SP);for(int n=0;n<12;++n)at(n,5000);
 sight(1);at(7,1030);at(8,1030);entities[8].s.eFlags=0x800000;
 assert(Build()==(std::vector<int>{0,8}));++checks;
 // The camera turned: what is straight ahead of it is seen.
 Begin(COM_GAME_JKA_SP);for(int n=0;n<12;++n)at(n,5000);
 sight(2);cameraAngles[YAW]=90;at(1,0,1000);at(2,1000);
 assert(Build()==(std::vector<int>{0,1}));++checks;
 // Without the power, or with no camera, nothing is added by sight.
 Begin(COM_GAME_JKA_SP);for(int n=0;n<12;++n)at(n,100);
 sight(2);player.raven.fd_forcePowersActive=~int(fntech3::jedi::sp::snapshot::kAcademyForceSight);
 assert(Build()==(std::vector<int>{0}));assert(angleCalls==0);
 Begin(COM_GAME_JKA_SP);for(int n=0;n<12;++n)at(n,100);
 sight(2);cameraOn=false;
 assert(Build()==(std::vector<int>{0}));assert(angleCalls==11);++checks;
 // A portal the power shows is added without its view walked; one it does
 // not show is walked, and its pass asks the camera nothing.
 for(bool shown:{true,false}){
  Begin(COM_GAME_JKA_SP);for(int n=0;n<12;++n)at(n,5000);
  sight(2);place(9,0,SVF_PORTAL);entities[9].s.origin2[0]=100;at(9,shown?1000:-5000);
  place(10,1);
  assert(Build()==(shown?std::vector<int>{0,9}:std::vector<int>{0,9,10}));
  // Unshown, the portal's pass adds entity 10 before the eye's pass asks
  // about it.
  assert(angleCalls==(shown?11:10));++checks;
 }
 // Academy's builder sees from the camera when it reports one.
 Begin(COM_GAME_JKA_SP);place(1,1);place(2,0);
 cameraOn=true;cameraOrigin[0]=100;
 assert(Build()==(std::vector<int>{0,1}));assert(viewpoints.front()[0]==100);
 assert(positionCalls==1&&angleCalls==0);
 Begin(COM_GAME_JKA_SP);place(1,1);place(2,0);
 assert(Build()==(std::vector<int>{0,2}));assert(viewpoints.front()[0]==0);++checks;
 assert(checks==4*3+3+2+7);
 std::printf("%d Jedi single-player snapshot cases\n",checks);
}
'''


def function(text, signature):
    begin = text.index(signature)
    return text[begin:text.index('\n}', begin) + 2]


class JediSPSnapshotWalkTests(unittest.TestCase):
    def source(self):
        snapshot = (ROOT / 'code/server/sv_snapshot.cpp').read_text(encoding='utf-8')
        game = (ROOT / 'code/server/sv_game.cpp').read_text(encoding='utf-8')
        math = (ROOT / 'code/qcommon/q_math.c').read_text(encoding='utf-8')
        walk = snapshots.WolfETSnapshotTests.snapshot_source(None, visibility=True)
        self.assertTrue(walk.startswith(snapshots.HOST))
        host = snapshots.HOST.replace(jedi.STUB, '')
        for stub in STUBS:
            self.assertEqual(host.count(stub), 1, stub)
            host = host.replace(stub, '')
        broadcast = function(game, 'qboolean SV_JediSnapshotBroadcast( int entityNum, int viewer, qboolean *forced ) {')
        seen = function(game, 'qboolean SV_JKASPForceSightSees( const vec3_t origin, int eFlags, int level ) {')
        body = walk[len(snapshots.HOST):] + EXTRA
        for signature in ('static void SV_SortEntityNumbers(', 'static void SV_SnapshotViewOrigin(',
                          'static void SV_BuildClientSnapshot('):
            body += '\n' + function(snapshot, signature)
        return (host + jedi.FAKES + FAKES + broadcast + '\n' + seen + '\n' +
                function(math, 'void AngleVectors(') + '\n' + body + CHECKS)

    def test_single_player_rules_through_the_builder(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler unavailable')
        scratch = ROOT / '.tmp' / 'jedi-sp-snapshot-walk'
        scratch.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=scratch) as directory:
            folder = Path(directory)
            path = folder / 'walk.cpp'
            executable = folder / ('walk.exe' if os.name == 'nt' else 'walk')
            path.write_text(self.source(), encoding='utf-8')
            target = compiler_target_args(compiler)
            if not target:
                target = ['-DFNTECH3_Q3_ONLY'] if os.name == 'nt' else ['-m32']
            built = subprocess.run([compiler, *target, '-std=c++17', '-I', str(ROOT), str(path),
                                    '-o', str(executable)], cwd=ROOT, capture_output=True,
                                   text=True, timeout=120)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            run = subprocess.run([str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=60)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)

    def test_the_rules_stay_with_the_jedi_single_player_profiles(self):
        snapshot = (ROOT / 'code/server/sv_snapshot.cpp').read_text(encoding='utf-8')
        walk = function(snapshot, 'static void SV_AddEntitiesVisibleFromPoint(')
        self.assertEqual(walk.count('const bool jediSP = Com_GameIsJKASP() || Com_GameIsJK2SP();'), 1)
        self.assertIn('const bool forceSight = Com_GameIsJKASP() && !portal &&', walk)
        self.assertIn('SV_AddEntitiesVisibleFromPoint( view, frame, eNums, portal || jediSP );', walk)
        flags = function(snapshot, 'static bool SV_EntityPassesSnapshotClientFlags(')
        self.assertLess(flags.index('if ( Com_GameIsJKASP() || Com_GameIsJK2SP() ) return true;'),
                        flags.index('SVF_SINGLECLIENT'))
        game = (ROOT / 'code/server/server_jka_sp_game.hpp').read_text(encoding='utf-8')
        self.assertIn('jkaSpCgameMain = reinterpret_cast<vmMainFunc_t>(Sys_LoadFunction(vm->dllHandle, "vmMain"));',
                      game)
        self.assertIn('jkaSpCgameMain = nullptr;', function(game, 'void SV_JKASPClearGame() {'))


if __name__ == '__main__':
    unittest.main()
