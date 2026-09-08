"""Exercise the production botlib trace callback's entity sentinel conversion."""
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path
from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]


class BotTraceBoundaryTests(unittest.TestCase):
    def test_no_entity_sentinel_and_trace_result(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++ compiler required')
        source = (ROOT / 'code/server/sv_bot.cpp').read_text()
        start = source.index('static void BotImport_Trace(')
        function = source[start:source.index('\n}', start)+2]
        harness = r'''
#include <cassert>
using vec3_t=float[3];
constexpr int ENTITYNUM_NONE=1023, qfalse=0;
struct Plane {float normal[3];float dist;int signbits,type;};
struct trace_t {int allsolid,startsolid;float fraction,endpos[3];Plane plane;int surfaceFlags,entityNum;};
struct bsp_trace_t {int allsolid,startsolid;float fraction,endpos[3];Plane plane;struct{int value,flags;}surface;int ent,exp_dist,sidenum,contents;};
int passed,mask;
void VectorCopy(const float *s,float*d){for(int i=0;i<3;i++)d[i]=s[i];}
void SV_Trace(trace_t *t,float*,float*,float*,float*,int entity,int contents,int capsule){
 passed=entity;mask=contents;assert(capsule==0);
 *t={1,0,.25f,{1,2,3},{{0,0,1},4,5,6},7,8};
}
''' + function + r'''
int main(){
 float v[3]{};bsp_trace_t result{};
 for(int input=-2;input<=1023;input++){
  BotImport_Trace(&result,v,v,v,v,input,17);
  assert(passed==(input==-1?1023:input) && mask==17);
  assert(result.allsolid==1 && result.startsolid==0 && result.fraction==.25f);
  assert(result.endpos[0]==1 && result.endpos[1]==2 && result.endpos[2]==3);
  assert(result.plane.normal[2]==1 && result.plane.dist==4 && result.plane.signbits==5 && result.plane.type==6);
  assert(result.surface.flags==7 && result.surface.value==0 && result.ent==8);
  assert(result.exp_dist==0 && result.sidenum==0 && result.contents==0);
 }
}
'''
        with tempfile.TemporaryDirectory(prefix='fntech3-bot-trace-') as folder:
            cpp=Path(folder)/'probe.cpp';exe=Path(folder)/'probe.exe'
            cpp.write_text(harness)
            run=subprocess.run([compiler,*compiler_target_args(compiler),'-std=c++17',str(cpp),'-o',str(exe)],capture_output=True,text=True,timeout=60)
            self.assertEqual(run.returncode,0,run.stdout+run.stderr)
            run=subprocess.run([str(exe)],capture_output=True,text=True,timeout=15)
            self.assertEqual(run.returncode,0,run.stdout+run.stderr)


if __name__=='__main__':
    unittest.main()
