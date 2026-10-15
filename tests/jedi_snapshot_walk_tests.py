"""Raven's snapshot rules through the production snapshot walk.

Retail jamp.exe 1.01 (SV_AddEntitiesVisibleFromPoint, 0x460f70..0x46105c) and
jk2mp.exe 1.02 (0x44fcf0) read no client-mask bit, never leave out a client an
entity's broadcastClients mask does not name, and send an entity whose mask
names the viewer past every visibility test. Academy also never sends a
permanent entity (EF_PERMANENT, 0x460f90) and always sends a sky-portal one.
This compiles the production walk with the production SV_JediSnapshotBroadcast
and its decision header over a fake game-data view, and checks that Quake III
keeps its own client mask on the same bit.
"""
# Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later.
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args
import wolfet_snapshot_tests as snapshots

ROOT = Path(__file__).resolve().parents[1]

STUB = ('qboolean SV_JediSnapshotBroadcast(int,int,qboolean*)'
        '{assert(!"The Jedi games use their own snapshot fixture");return qfalse;}\n')

FAKES = r'''
#include "code/compat/jka_game_abi.h"
#include "code/server/server_jka_snapshot.hpp"
#include <algorithm>
#include <cstdint>
// Academy's records are read in place, Outcast's through the checked view.
static std::array<fntech3_jka_sharedEntity_t,16> academy{};
static std::array<std::array<std::uint32_t,2>,16> outcastMasks{};
sharedEntity_t *SV_GentityNum(int n){
 assert(n>=0&&n<16);return reinterpret_cast<sharedEntity_t*>(&academy[n]);
}
namespace fntech3::server { struct GameEntityRef { int index=-1; }; }
static int unreadable=-1;
static struct {
 bool EntityRefAt(int n,fntech3::server::GameEntityRef &r)const{
  if(n<0||n>=sv.num_entities)return false;r.index=n;return true;
 }
 bool GetSvFlags(const fntech3::server::GameEntityRef &r,std::int32_t &v)const{
  v=entities[r.index].r.svFlags;return true;
 }
 bool GetJK2BroadcastClients(const fntech3::server::GameEntityRef &r,std::uint32_t (&v)[2])const{
  if(profile!=COM_GAME_JK2_MP||r.index==unreadable)return false;
  v[0]=outcastMasks[r.index][0];v[1]=outcastMasks[r.index][1];return true;
 }
} serverGameData;
'''

CHECKS = r'''
static std::vector<int> visible(int client=0){
 SnapshotEntityNumbers out;out.clientNum=client;clientSnapshot_t frame{};frame.ps.clientNum=client;
 // The viewer's own entity is marked sent first, as both engines do
 // (jamp.exe 0x4614cd): its state is the player state.
 sv.svEntities[client].snapshotCounter=sv.snapshotCounter;
 vec3_t origin{};SV_AddEntitiesVisibleFromPoint(origin,&frame,out,false);
 std::vector<int> numbers(out.indices.begin(),out.indices.begin()+out.count);
 std::sort(numbers.begin(),numbers.end());return numbers;
}
static void place(int n,int flags,bool shown,std::uint32_t mask,int eFlags=0,int portal=0){
 entities[n].r.svFlags=flags;entities[n].s.eFlags=eFlags;academy[n].r.svFlags=flags;
 academy[n].s.eFlags=eFlags;academy[n].s.isPortalEnt=portal;
 academy[n].r.broadcastClients[0]=mask;outcastMasks[n][0]=mask;
 if(shown)sv.svEntities[n].clusternums[0]=0;
}
static void world(comGameId_t id){
 // Entity 12 is a second viewer's own, out of view.
 setup(id,13);academy={};outcastMasks={};unreadable=-1;
 place(1,0x2,true,0);           // flagged, names nobody, in view
 place(2,0x2,false,0);          // flagged, names nobody, out of view
 place(3,0x2,false,1);          // flagged, names the viewer, out of view
 place(4,0,false,1);            // names the viewer, out of view
 place(5,0,false,1u<<12);       // names client 12 only, out of view
 place(6,0,true,0,0x80);        // permanent, in view
 place(7,SVF_BROADCAST,false,0,0x80); // permanent and broadcast
 place(8,0,false,0,0,1);        // seen from the sky portal, out of view
 place(9,0,true,0);             // ordinary, in view
}
int main(){
 world(COM_GAME_JKA_MP);
 assert(visible()==(std::vector<int>{1,3,4,8,9}));
 // Only the viewer's own bit counts: client 12 is sent entity 5 instead.
 ++sv.snapshotCounter;assert(visible(12)==(std::vector<int>{1,5,8,9}));
 // The second word is not a low viewer's.
 world(COM_GAME_JKA_MP);academy[2].r.broadcastClients[1]=0xffffffffu;
 assert(visible()==(std::vector<int>{1,3,4,8,9}));
 // Outcast: the same adds, no permanent skip and no portal-scene add.
 world(COM_GAME_JK2_MP);
 assert(visible()==(std::vector<int>{1,3,4,6,7,9}));
 ++sv.snapshotCounter;assert(visible(12)==(std::vector<int>{1,5,6,7,9}));
 // Quake III keeps its client mask on bit 0x2, singleClient as the mask, and
 // reads no broadcast mask.
 world(COM_GAME_QUAKE3);
 assert(visible()==(std::vector<int>{6,7,9}));
 world(COM_GAME_QUAKE3);entities[1].r.singleClient=1;
 assert(visible()==(std::vector<int>{1,6,7,9}));
 // An Outcast record the view cannot read is left out, not guessed at.
 world(COM_GAME_JK2_MP);unreadable=9;
 assert(visible()==(std::vector<int>{1,3,4,6,7}));
}
'''


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2]


class JediSnapshotWalkTests(unittest.TestCase):
    def test_raven_rules_through_the_walk(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler unavailable')
        game = (ROOT / 'code/server/sv_game.cpp').read_text(encoding='utf-8')
        jedi = function(game, 'qboolean SV_JediSnapshotBroadcast( int entityNum, int viewer, qboolean *forced ) {')
        self.assertIn(STUB, snapshots.HOST)
        walk = snapshots.WolfETSnapshotTests.snapshot_source(None, visibility=True)
        self.assertTrue(walk.startswith(snapshots.HOST))
        host = snapshots.HOST.replace(STUB, '')
        source = host + FAKES + jedi + '\n' + walk[len(snapshots.HOST):] + CHECKS
        scratch = ROOT / '.tmp' / 'jedi-snapshot-walk'
        scratch.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=scratch) as directory:
            folder = Path(directory)
            path = folder / 'walk.cpp'
            executable = folder / ('walk.exe' if os.name == 'nt' else 'walk')
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

    def test_the_hook_serves_both_jedi_profiles_only(self):
        snapshot = (ROOT / 'code/server/sv_snapshot.cpp').read_text(encoding='utf-8')
        self.assertEqual(snapshot.count('( Com_GameIsJKAMP() || Com_GameIsJK2MP() ) && !SV_JediSnapshotBroadcast('), 2)
        self.assertNotIn('SV_JKASnapshotBroadcast', snapshot)
        flags = function(snapshot, 'static bool SV_EntityPassesSnapshotClientFlags(')
        self.assertIn('!Com_GameIsJKAMP() && !Com_GameIsJK2MP() && ( entity.r.svFlags & SVF_CLIENTMASK )', flags)


if __name__ == '__main__':
    unittest.main()
