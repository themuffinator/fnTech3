"""Exercise SP's visible player entity through the real snapshot builder."""
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

EXTRA = r'''
static client_t client{};
static playerState_t player{};
static bool validPlayer=true;
static vec3_t viewOrigin{};
static std::array<entityState_t,MAX_GENTITIES> backing{};
qboolean SV_ReadGameClient(int n,playerState_t *out){
 assert(n==0); if(!validPlayer)return qfalse;*out=player;return qtrue;
}
static void SV_SnapshotViewOrigin(const client_t*,const playerState_t&,vec3_t origin){
 VectorCopy(viewOrigin,origin);
}
static void SV_AddAudioReachableEntities(const vec3_t,int,clientSnapshot_t*,SnapshotEntityNumbers&){}
static void Begin(comGameId_t id,int viewer=0){
 setup(id,4);std::memset(&client,0,sizeof(client));std::memset(&player,0,sizeof(player));
 std::memset(backing.data(),0,sizeof(backing));VectorClear(viewOrigin);validPlayer=true;
 svs.clients=&client;sv.maxclients=1;svs.currFrame=nullptr;
 svs.snapshotEntities=backing.data();svs.numSnapshotEntities=backing.size();
 svs.freeStorageEntities=backing.size();client.state=CS_ACTIVE;
 client.gentity=&entities[viewer];player.clientNum=viewer;
 player.weapon=6;player.eventSequence=14;player.events[0]=18;player.events[1]=35;
 player.eventParms[0]=7;player.eventParms[1]=10;
 for(int i=0;i<4;++i)sv.svEntities[i].clusternums[0]=0;
}
'''

CHECKS = r'''
static std::vector<int> Build(){
 SV_BuildClientSnapshot(&client);const auto &frame=client.frames[0];
 assert(frame.num_entities>=0&&frame.num_entities<=MAX_SERVER_SNAPSHOT_ENTITIES);
 std::vector<int> out;
 for(int i=0;i<frame.num_entities;++i)out.push_back(frame.ents[i]->number);
 return out;
}
int main(){
 int checks=0;
 for(auto id:{COM_GAME_QUAKE3,COM_GAME_QUAKELIVE,COM_GAME_RTCW_SP,COM_GAME_RTCW_MP,
              COM_GAME_WOLFET,COM_GAME_STVEF_SP,COM_GAME_STVEF_MP}){
  const bool sp=id==COM_GAME_STVEF_SP;
  for(int viewer:{0,2}){
   Begin(id,viewer);auto expected=std::vector<int>{0,1,2,3};
   if(!sp)expected.erase(expected.begin()+viewer);
   assert(Build()==expected);assert(std::memcmp(&client.frames[0].ps,&player,sizeof(player))==0);++checks;
   // Repeated snapshots and an overlapping portal retain each entity once.
   assert(Build()==expected);++checks;
   Begin(id,viewer);entities[3].r.svFlags=SVF_PORTAL;entities[3].s.origin2[0]=100;
   sv.svEntities[1].clusternums[0]=1;
   assert(Build()==expected);++checks;
   // A camera in a disjoint PVS does not force the player's entity through.
   Begin(id,viewer);viewOrigin[0]=100;sv.svEntities[3].clusternums[0]=1;
   assert(Build()==std::vector<int>{3});++checks;
   Begin(id,viewer);viewOrigin[0]=100;entities[viewer].r.svFlags=SVF_BROADCAST;
   assert(Build()==(sp?std::vector<int>{viewer}:std::vector<int>{}));++checks;
   // Linked-state and NOCLIENT filtering still apply to SP's own entity.
   for(int mode:{0,1}){
    Begin(id,viewer);if(mode==0)entities[viewer].r.linked=qfalse;else entities[viewer].r.svFlags=SVF_NOCLIENT;
    auto hidden=std::vector<int>{0,1,2,3};hidden.erase(hidden.begin()+viewer);
    assert(Build()==hidden);++checks;
   }
   Begin(id,viewer);areasConnected=false;assert(Build().empty());++checks;
   Begin(id,viewer);client.gentity=nullptr;assert(Build().empty());++checks;
   Begin(id,viewer);client.state=CS_ZOMBIE;assert(Build().empty());++checks;
  }
  for(int invalid:{-1,MAX_GENTITIES,INT_MAX}){
   Begin(id);player.clientNum=invalid;bool dropped=false;
   try{Build();}catch(Drop&){dropped=true;}assert(dropped&&client.frames[0].num_entities==0);++checks;
  }
  Begin(id);validPlayer=false;bool dropped=false;
  try{Build();}catch(Drop&){dropped=true;}assert(dropped&&client.frames[0].num_entities==0);++checks;
 }
 std::printf("%d snapshot membership/lifecycle cases across seven profiles\n",checks);
}
'''


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2]


class STVEFSPSnapshotTests(unittest.TestCase):
    def test_visible_player_and_other_profile_filters(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler unavailable')
        source = (ROOT / 'code/server/sv_snapshot.cpp').read_text()
        start = source.index('struct SnapshotEntityNumbers {')
        body = source[start:source.index('\n};', start) + 3]
        for name in ('static bool SV_AddWolfETVisibilityTargets(',
                     'static bool SV_EntityPassesSnapshotClientFlags(',
                     'static void SV_AddEntitiesVisibleFromPoint(',
                     'static void SV_BuildCommonSnapshot(',
                     'static void SV_SortEntityNumbers('):
            body += '\n' + function(source, name)
        host = snapshots.HOST.replace('#include <cassert>', r'''#include <cassert>
#include <cstdio>
#include <cstdlib>
#undef assert
#define assert(x) do { if(!(x)){std::fprintf(stderr,"line %d profile %d: %s\n",__LINE__,int(profile),#x);std::exit(1);} } while(0)''')
        text = host + body + EXTRA + function(source, 'static void SV_BuildClientSnapshot(') + CHECKS
        target = compiler_target_args(compiler)
        if not target:
            target = ['-DFNTECH3_Q3_ONLY'] if os.name == 'nt' else ['-m32']
        sanitizer = [] if os.name == 'nt' else ['-fsanitize=address,undefined', '-fno-sanitize-recover=all']
        with tempfile.TemporaryDirectory(prefix='stvef-snapshot-') as folder:
            cpp = Path(folder) / 'snapshot.cpp'
            exe = Path(folder) / ('snapshot.exe' if os.name == 'nt' else 'snapshot')
            cpp.write_text(text)
            built = subprocess.run([compiler, *target, '-std=c++17', '-O1', *sanitizer,
                '-I', str(ROOT), str(cpp), '-o', str(exe)], cwd=ROOT,
                capture_output=True, text=True, timeout=120)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
            ran = subprocess.run([str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=30, **options)
            self.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)


if __name__ == '__main__':
    unittest.main()
