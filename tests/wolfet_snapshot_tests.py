"""Exercise ET's larger server snapshot window without expanding other profiles."""
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
#include <array>
#include <cassert>
#include <iterator>
static comGameId_t profile;
int Com_GameIsWolfET(){return profile==COM_GAME_WOLFET;}
server_t sv{};
using SnapshotEntityIndex = int;
'''
CHECKS = r'''
int main(){
 static_assert(MAX_SERVER_SNAPSHOT_ENTITIES >= MAX_SNAPSHOT_ENTITIES);
 static_assert(MAX_SERVER_SNAPSHOT_ENTITIES == 512);
 for(auto id:{COM_GAME_QUAKE3,COM_GAME_QUAKELIVE,COM_GAME_RTCW_SP,COM_GAME_RTCW_MP,COM_GAME_WOLFET}){
  profile=id;sv.snapshotCounter=19;
  struct { SnapshotEntityNumbers value; unsigned sentinel=0x5a5aa5a5; } guarded;
  const int expected=id==COM_GAME_WOLFET?512:256;
  svEntity_t entity{};
  for(int index=0;index<600;++index){
   entity.snapshotCounter=0;
   guarded.value.Add(entity,index);
   assert(guarded.value.count==(index<expected?index+1:expected));
   assert(entity.snapshotCounter==19);
  }
  assert(guarded.sentinel==0x5a5aa5a5);
  clientSnapshot_t frame{};
  std::array<entityState_t,600> states{};
  assert(guarded.value.count<=std::size(frame.ents));
  for(int index=0;index<guarded.value.count;++index){
   assert(guarded.value.indices[index]==index);
   frame.ents[index]=&states[guarded.value.indices[index]];
  }
  assert(frame.ents[expected-1]==&states[expected-1]);
 }
}
'''


class WolfETSnapshotTests(unittest.TestCase):
    def test_et_window_and_existing_profile_caps(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler unavailable')
        snapshot = (ROOT/'code/server/sv_snapshot.cpp').read_text(encoding='utf-8')
        begin = snapshot.index('struct SnapshotEntityNumbers {')
        body = snapshot[begin:snapshot.index('\n};',begin)+3]
        scratch = ROOT/'.tmp/wolfet'
        scratch.mkdir(parents=True,exist_ok=True)
        with tempfile.TemporaryDirectory(dir=scratch) as directory:
            folder = Path(directory)
            source = folder/'snapshot.cpp'
            executable = folder/('snapshot.exe' if os.name == 'nt' else 'snapshot')
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
