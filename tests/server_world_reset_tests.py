"""Execute world-link reset without erasing retained snapshot baselines."""
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path
from compiler_support import compiler_target_args

ROOT=Path(__file__).resolve().parents[1]


class ServerWorldResetTests(unittest.TestCase):
    def test_reset_clears_both_link_directions_and_preserves_baselines(self):
        compiler=shutil.which('clang++') or shutil.which('g++')
        if not compiler:self.skipTest('C++ compiler required')
        source=(ROOT/'code/server/sv_world.cpp').read_text()
        start=source.index('void SV_ClearWorld( void )')
        function=source[start:source.index('\n}',start)+2]
        harness=r'''
#include <array>
#include <cassert>
using clipHandle_t=int;
using vec3_t=float[3];
struct worldSector_t {void *entities;int oldMarker;};
struct svEntity_t {worldSector_t *worldSector;svEntity_t *nextEntityInWorldSector;int baseline;int wolfetOriginCluster;};
std::array<worldSector_t,4> sv_worldSectors;
int sv_numworldSectors=4;
struct {std::array<svEntity_t,4> svEntities;} sv;
int CM_InlineModel(int n){assert(n==0);return 19;}
void CM_ModelBounds(int model,float *mins,float *maxs){assert(model==19);for(int i=0;i<3;i++){mins[i]=-100;maxs[i]=100;}}
void SV_CreateworldSector(int depth,float *mins,float *maxs){assert(depth==0 && mins[0]==-100 && maxs[2]==100);++sv_numworldSectors;}
''' + function + r'''
int main(){
 for(int i=0;i<4;i++){
  sv_worldSectors[i]={&sv.svEntities[i],37};
  sv.svEntities[i]={&sv_worldSectors[i],&sv.svEntities[(i+1)%4],100+i,47};
 }
 for(int repeat=0;repeat<2;repeat++){
  SV_ClearWorld();
  assert(sv_numworldSectors==1);
  for(int i=0;i<4;i++){
   assert(!sv_worldSectors[i].entities && sv_worldSectors[i].oldMarker==0);
   assert(!sv.svEntities[i].worldSector && !sv.svEntities[i].nextEntityInWorldSector);
   assert(sv.svEntities[i].baseline==100+i);
   assert(sv.svEntities[i].wolfetOriginCluster==-1);
  }
 }
}
'''
        with tempfile.TemporaryDirectory(prefix='fntech3-world-reset-') as folder:
            cpp=Path(folder)/'probe.cpp';exe=Path(folder)/'probe.exe';cpp.write_text(harness)
            run=subprocess.run([compiler,*compiler_target_args(compiler),'-std=c++17',str(cpp),'-o',str(exe)],capture_output=True,text=True,timeout=60)
            self.assertEqual(run.returncode,0,run.stdout+run.stderr)
            run=subprocess.run([str(exe)],capture_output=True,text=True,timeout=15)
            self.assertEqual(run.returncode,0,run.stdout+run.stderr)


if __name__=='__main__':unittest.main()
