"""Exercise ET world-only traces and preserve every other profile's behavior."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]
HOST = r'''
#include "code/qcommon/q_shared.h"
#include "code/qcommon/game_profile.h"
#include <cassert>
#include <initializer_list>
const vec3_t vec3_origin{};
static comGameId_t profile;
int Com_GameIsWolfET(){return profile==COM_GAME_WOLFET;}
static bool SV_AsBool(qboolean b){return b!=qfalse;}
struct moveclip_t {
 trace_t trace{};int contentmask,passEntityNum;const float *start,*mins,*maxs;
 vec3_t end,boxmins,boxmaxs;bool capsule;
};
static float fraction=1;
static int worldCalls,entityCalls;
static qboolean lastCapsule;
static void CM_BoxTrace(trace_t *out,const float *,const float *,const float *mins,
 const float *maxs,int model,int mask,qboolean capsule){
 assert(mins&&maxs&&model==0&&mask==0x120);++worldCalls;
 out->fraction=fraction;lastCapsule=capsule;
}
static void SV_ClipMoveToEntities(moveclip_t &clip){
 ++entityCalls;clip.trace.entityNum=7;assert(clip.contentmask==0x120);
}
'''
CHECKS = r'''
int main(){
 for(auto id:{COM_GAME_QUAKE3,COM_GAME_QUAKELIVE,COM_GAME_RTCW_SP,COM_GAME_RTCW_MP,COM_GAME_WOLFET})
 for(int pass:{-2,ENTITYNUM_NONE,1})for(float f:{0.f,.5f,1.f})for(auto capsule:{qfalse,qtrue}){
  profile=id;worldCalls=entityCalls=0;fraction=f;trace_t output{};vec3_t start{},end{10,-10,5};
  SV_Trace(&output,start,nullptr,nullptr,end,pass,0x120,capsule);
  const bool worldOnly=f==0||(id==COM_GAME_WOLFET&&pass==-2);
  assert(worldCalls==1&&lastCapsule==capsule);
  assert(entityCalls==(worldOnly?0:1));
  assert(output.entityNum==(worldOnly?(f==1?ENTITYNUM_NONE:ENTITYNUM_WORLD):7));
  assert(output.fraction==f);
 }
}
'''


class WolfETTraceTests(unittest.TestCase):
    def test_world_only_sentinel_is_scoped_to_et(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler unavailable')
        world = (ROOT/'code/server/sv_world.cpp').read_text(encoding='utf-8')
        begin = world.index('void SV_Trace(')
        body = world[begin:world.index('\n}',begin)+2]
        scratch = ROOT/'.tmp/wolfet'
        scratch.mkdir(parents=True,exist_ok=True)
        with tempfile.TemporaryDirectory(dir=scratch) as directory:
            folder = Path(directory)
            source = folder/'trace.cpp'
            executable = folder/('trace.exe' if os.name == 'nt' else 'trace')
            source.write_text(HOST+body+CHECKS,encoding='utf-8')
            target = compiler_target_args(compiler)
            if not target:
                target = ['-DFNTECH3_Q3_ONLY'] if os.name == 'nt' else ['-m32']
            built = subprocess.run([compiler,*target,'-std=c++17',
                '-I',str(ROOT),str(source),'-o',str(executable)],
                cwd=ROOT,capture_output=True,text=True,timeout=90)
            self.assertEqual(built.returncode,0,built.stdout+built.stderr)
            run = subprocess.run([str(executable)],cwd=ROOT,capture_output=True,text=True,timeout=20)
            self.assertEqual(run.returncode,0,run.stdout+run.stderr)


if __name__ == '__main__':
    unittest.main()
