"""Portals seen through portals, through the production snapshot walk.

Every retail engine that shares SV_AddEntitiesVisibleFromPoint recurses into a
portal seen from another portal's camera, bounded only by its already-added
test: quake3.exe 1.32 (0x43b972), quakelive_steam.exe (0x4e565d), WolfSP.exe
(0x439489), WolfMP.exe (0x43c749), etded.exe (0x42f8b5), stvoyHM.exe
(0x435cc8), stvoy.exe (0x42876f), jamp.exe (0x4613a7) and jk2mp.exe
(0x44ff65). RTCW's engines also send a viewer watching a scripted camera only
the portals and what their cameras see (0x4391de, 0x43c49e). Quake3e's second
view (SVF_SELF_PORTAL2) is served to Quake III and Quake Live only and adds no
portal's view. See docs/fntech3/2026-10-15-snapshot-portal-recursion.md.

This compiles the production walk with the production SV_JediSnapshotBroadcast
over the shared fake game-data view, given three rooms whose viewpoints see
only their own cluster.
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

# The shared fixture has two viewpoints; a chain needs a third.
ROOMS = (
    ('static byte visibility[2]={1,2};', 'static byte visibility[3]={1,2,4};'),
    ('assert(viewpoints.size()<100);', 'assert(viewpoints.size()<2*MAX_GENTITIES);'),
    ('return point[0]>=100?1:0;', 'return point[0]>=200?2:point[0]>=100?1:0;'),
    ('visibility+(cluster==1?1:0)', 'visibility+cluster'),
    ('visibility[0]=1;visibility[1]=2;', 'visibility[0]=1;visibility[1]=2;visibility[2]=4;'),
)

CHECKS = r'''
static const comGameId_t profiles[]={COM_GAME_QUAKE3,COM_GAME_QUAKELIVE,COM_GAME_RTCW_SP,COM_GAME_RTCW_MP,
 COM_GAME_WOLFET,COM_GAME_STVEF_MP,COM_GAME_STVEF_SP,COM_GAME_JKA_MP,COM_GAME_JK2_MP};
static bool jediGame(comGameId_t id){return id==COM_GAME_JKA_MP||id==COM_GAME_JK2_MP;}
static std::vector<int> walk(){
 SnapshotEntityNumbers out;out.clientNum=0;clientSnapshot_t frame{};frame.ps.clientNum=0;
 // The viewer's own entity is marked sent first.
 sv.svEntities[0].snapshotCounter=++sv.snapshotCounter;viewpoints.clear();
 vec3_t origin{};SV_AddEntitiesVisibleFromPoint(origin,&frame,out,false);
 std::vector<int> numbers(out.indices.begin(),out.indices.begin()+out.count);
 std::sort(numbers.begin(),numbers.end());return numbers;
}
static std::vector<float> seen(){
 std::vector<float> x;for(const auto &point:viewpoints)x.push_back(point[0]);return x;
}
// Room r is x in [100r, 100r+100) and cluster r; cluster 7 is never seen.
static void in(int n,int cluster,int flags=0,float camera=-1){
 entities[n].r.svFlags=flags;if(n<int(academy.size()))academy[n].r.svFlags=flags;
 sv.svEntities[n].clusternums[0]=cluster;
 if(camera>=0)entities[n].s.origin2[0]=camera;
}
static void begin(comGameId_t id,int count=8){
 setup(id,count);academy={};outcastMasks={};unreadable=-1;
 for(int n=0;n<count;++n)sv.svEntities[n].clusternums[0]=7;
}
int main(){
 int checks=0;
 for(auto id:profiles){
  // A portal in the viewer's room shows room 1, where a second portal shows
  // room 2: every engine walks all three.
  begin(id);in(1,0,SVF_PORTAL,100);in(2,1,SVF_PORTAL,200);in(3,2);in(4,1);
  assert(walk()==(std::vector<int>{1,2,3,4}));
  assert(seen()==(std::vector<float>{0,100,200}));++checks;
  // A portal that shows its own room, and two that show each other, end
  // at the already-added test.
  begin(id);in(1,0,SVF_PORTAL,0);in(5,0);
  assert(walk()==(std::vector<int>{1,5}));assert(seen()==(std::vector<float>{0,0}));++checks;
  begin(id);in(1,0,SVF_PORTAL,100);in(2,1,SVF_PORTAL,0);in(4,1);
  assert(walk()==(std::vector<int>{1,2,4}));assert(seen()==(std::vector<float>{0,100,0}));++checks;
  // The viewer watching a scripted camera: RTCW sends the portals, out of
  // view or not, and what their cameras see, but nothing from its own eye
  // (broadcasts arrive through the camera's pass). Every other engine reads
  // nothing of the bit.
  begin(id);in(1,2,SVF_PORTAL,100);in(2,1,SVF_PORTAL,200);in(3,2);in(4,1);in(5,0);in(6,7,SVF_BROADCAST);
  entities[0].s.eFlags=FNTECH3_RTCW_EF_VIEWING_CAMERA;
  const bool rtcw=id==COM_GAME_RTCW_SP||id==COM_GAME_RTCW_MP;
  assert(walk()==(rtcw?std::vector<int>{1,2,3,4,6}:std::vector<int>{5,6}));
  assert(seen()==(rtcw?std::vector<float>{0,100,200}:std::vector<float>{0}));++checks;
  // Quake3e's second view: Quake III and Quake Live add what it sees but no
  // portal's view from it; no other game's engine reads the bit.
  begin(id);in(2,1,SVF_PORTAL,200);in(3,2);in(4,1);in(5,0);
  entities[0].r.svFlags=SVF_SELF_PORTAL2;entities[0].r.s.origin2[0]=100;
  const bool q3e=id==COM_GAME_QUAKE3||id==COM_GAME_QUAKELIVE;
  assert(walk()==(q3e?std::vector<int>{2,4,5}:std::vector<int>{5}));
  assert(seen()==(q3e?std::vector<float>{0,100}:std::vector<float>{0}));++checks;
  // A portal the second view sees still shows its room when the viewer's
  // own eye reaches it; as in Quake3e, each pass from the eye or a portal
  // ends with the second view, which finds nothing new after the first.
  if(q3e){
   begin(id);in(1,0,SVF_PORTAL,100);in(2,1,SVF_PORTAL,200);in(3,2);in(4,1);
   entities[0].r.svFlags=SVF_SELF_PORTAL2;entities[0].r.s.origin2[0]=100;
   assert(walk()==(std::vector<int>{1,2,3,4}));
   assert(seen()==(std::vector<float>{0,100,200,100,100,100}));++checks;
  }
  // Quake III, Quake Live and the Jedi games carry a range in generic1,
  // measured from the pass that sees the portal: room 1's portal is 150
  // from its camera's pass, out of a 100 range.
  if(q3e||jediGame(id)){
   begin(id);in(1,0,SVF_PORTAL,100);in(2,1,SVF_PORTAL,200);in(3,2);in(4,1);
   entities[1].s.origin[0]=40;entities[1].s.generic1=50;
   entities[2].s.origin[0]=250;entities[2].s.generic1=100;
   assert(walk()==(std::vector<int>{1,2,4}));assert(seen()==(std::vector<float>{0,100}));
   entities[2].s.generic1=200;
   assert(walk()==(std::vector<int>{1,2,3,4}));++checks;
  }
  // The depth is at most the number of portal entities: here every one sees
  // the next, 1,023 deep, past the snapshot's entity cap, which still marks
  // each entity added.
  if(!jediGame(id)){
   begin(id,MAX_GENTITIES);
   for(int n=1;n<MAX_GENTITIES;++n)in(n,0,SVF_PORTAL,0);
   const auto chain=walk();
   const int limit=id==COM_GAME_WOLFET?MAX_GENTITIES-1:MAX_SNAPSHOT_ENTITIES;
   assert(int(chain.size())==limit&&chain.front()==1&&chain.back()==limit);
   assert(viewpoints.size()==MAX_GENTITIES);++checks;
  }
 }
 assert(checks==9*5+2+4+7);
}
'''


class SnapshotPortalChainTests(unittest.TestCase):
    def test_portals_through_portals_in_every_profile(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler unavailable')
        game = (ROOT / 'code/server/sv_game.cpp').read_text(encoding='utf-8')
        broadcast = jedi.function(game, 'qboolean SV_JediSnapshotBroadcast( int entityNum, int viewer, qboolean *forced ) {')
        walk = snapshots.WolfETSnapshotTests.snapshot_source(None, visibility=True)
        self.assertTrue(walk.startswith(snapshots.HOST))
        host = snapshots.HOST.replace(jedi.STUB, '')
        for old, new in ROOMS:
            self.assertEqual(host.count(old), 1, old)
            host = host.replace(old, new)
        source = host + jedi.FAKES + broadcast + '\n' + walk[len(snapshots.HOST):] + CHECKS
        scratch = ROOT / '.tmp' / 'snapshot-portal-chain'
        scratch.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=scratch) as directory:
            folder = Path(directory)
            path = folder / 'chain.cpp'
            executable = folder / ('chain.exe' if os.name == 'nt' else 'chain')
            path.write_text(source, encoding='utf-8')
            target = compiler_target_args(compiler)
            if not target:
                target = ['-DFNTECH3_Q3_ONLY'] if os.name == 'nt' else ['-m32']
            built = subprocess.run([compiler, *target, '-std=c++17', '-I', str(ROOT), str(path),
                                    '-o', str(executable)], cwd=ROOT, capture_output=True,
                                   text=True, timeout=120)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            run = subprocess.run([str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=60)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)

    def test_the_walk_keeps_no_record_in_its_recursive_frame(self):
        snapshot = (ROOT / 'code/server/sv_snapshot.cpp').read_text(encoding='utf-8')
        walk = jedi.function(snapshot, 'static void SV_AddEntitiesVisibleFromPoint(')
        self.assertNotIn('sharedEntity_t entity;', walk)
        self.assertNotIn('sharedEntity_t viewer;', walk)
        self.assertNotIn('sharedEntity_t clientEntity;', walk)
        self.assertIn('sharedEntity_t &entity = eNums.entity;', walk)
        # Nor does ET's proxy rule, which a compiler may inline into it.
        proxies = jedi.function(snapshot, 'static bool SV_AddWolfETVisibilityTargets(')
        self.assertNotIn('sharedEntity_t target', proxies)
        self.assertIn('sharedEntity_t &target = numbers.target;', proxies)
        # Each recursion copies its viewpoint out of the shared record first.
        self.assertNotIn('SV_AddEntitiesVisibleFromPoint( entity.', walk)
        self.assertEqual(walk.count('SV_AddEntitiesVisibleFromPoint( view, frame, eNums, '), 4)


if __name__ == '__main__':
    unittest.main()
