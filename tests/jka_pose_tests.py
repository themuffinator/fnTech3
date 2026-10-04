"""Compile the independently authored shared pose kernel and physical guards."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args
from rtcw_server_world_tests import function

ROOT=Path(__file__).resolve().parents[1]

def dispatch_fixture():
    host=(ROOT/'code/server/server_jka_game.hpp').read_text()
    start=host.index('\tcase Call::G_G2_GETBOLT:');end=host.index('\n\tcase Call::G_G2_ADDBOLT:',start)
    vm=(ROOT/'code/qcommon/vm.c').read_text()
    tests=(ROOT/'tests/jka_pose_tests.cpp').read_text().replace('int main(int argc,char **argv)', 'int PoseUnitFixtureMain(int argc,char **argv)')
    return tests+r'''
#include "code/compat/jka_module_abi.h"
#include <stdexcept>
using vec3_t=float[3];
struct vm_t { void *entryPoint; void *dllExports; unsigned char *dataBase; unsigned dataMask; };
constexpr int ERR_DROP=1;
static void Com_Error(int,const char*,...){throw std::runtime_error("bounds");}
#define VM_CHECKBOUNDS VM_CheckBounds
''' + function(vm,'void *VM_ExplicitArgPtr(')+'\n'+function(vm,'void VM_CheckBounds(')+r'''
static TestFiles storedFiles;static ghoul2::Store ravenGhoul2(storedFiles);static vm_t *gvm;
static bool Dispatch(comGameId_t profile,const intptr_t *args,intptr_t &result){
 using namespace fntech3::jka::module;
 const Call call=Lookup(profile,Module::GAME,Direction::IMPORT,args[0]);
 const auto pointer=[args](int index){return VM_ExplicitArgPtr(gvm,args[index]);};
 switch(call){
''' + host[start:end] + r'''
 default:return false;
 }
}
int main(){
 storedFiles.files["models/players/test/model.glm"]=Mesh("models/players/test/skeleton",3);
 storedFiles.files["models/players/test/skeleton.gla"]=PoseSkeleton();
 void *instance=nullptr;LoadError error;CHECK(ravenGhoul2.AddModel(&instance,"models/players/test/model.glm",error)==0);
 const int bolt=ravenGhoul2.AddBolt(instance,0,"child");CHECK(bolt==0);
 const float angles[]{0,0,0},position[]{7,-11,13},scale[]{2,.5f,0};
 for(int mode=0;mode<2;++mode){
  std::array<unsigned char,512> memory;memory.fill(0xa5);
  vm_t vm{};if(mode)vm.dllExports=reinterpret_cast<void*>(1);else vm.entryPoint=reinterpret_cast<void*>(1);gvm=&vm;
  auto address=[&](int at){return reinterpret_cast<intptr_t>(memory.data()+at);};
  std::memcpy(memory.data()+129,angles,12);std::memcpy(memory.data()+145,position,12);std::memcpy(memory.data()+161,scale,12);
  intptr_t args[]{587,reinterpret_cast<intptr_t>(instance),0,bolt,address(65),address(129),address(145),0,0,address(161)};
  intptr_t result=-1;CHECK(Dispatch(COM_GAME_JKA_MP,args,result)&&result==1);
  float out[12];std::memcpy(out,memory.data()+65,48);CHECK(NearPose(out[0],1)&&NearPose(out[7],-6.5f));
  CHECK(memory[64]==0xa5&&memory[113]==0xa5);
  const auto original=memory;
  args[0]=588;CHECK(Dispatch(COM_GAME_JKA_MP,args,result)&&result==1&&memory==original);
  args[0]=589;CHECK(Dispatch(COM_GAME_JKA_MP,args,result)&&result==1);std::memcpy(out,memory.data()+65,48);
  CHECK(NearPose(out[1],-1)&&NearPose(out[7],-6.5f));CHECK(memory[64]==0xa5&&memory[113]==0xa5);
  const auto saved=memory;args[3]=-1;CHECK(Dispatch(COM_GAME_JKA_MP,args,result)&&result==0&&memory==saved);args[3]=bolt;
  args[4]=0;CHECK(Dispatch(COM_GAME_JKA_MP,args,result)&&result==0&&memory==saved);args[4]=address(65);
  args[5]=0;CHECK(Dispatch(COM_GAME_JKA_MP,args,result)&&result==0&&memory==saved);args[5]=address(129);
  const std::uint32_t nan=0x7fc00001u;std::memcpy(memory.data()+161,&nan,4);const auto invalid=memory;
  CHECK(Dispatch(COM_GAME_JKA_MP,args,result)&&result==0&&memory==invalid);
  CHECK(!Dispatch(COM_GAME_QUAKE3,args,result)&&!Dispatch(COM_GAME_JKA_SP,args,result)&&!Dispatch(COM_GAME_JK2_MP,args,result));
  vm.entryPoint=nullptr;vm.dllExports=nullptr;vm.dataBase=memory.data();vm.dataMask=511;
  args[4]=500;args[5]=129;args[6]=145;args[9]=161;bool refused=false;
  try{Dispatch(COM_GAME_JKA_MP,args,result);}catch(const std::runtime_error&){refused=true;}CHECK(refused&&memory==invalid);
 }
 CHECK(ravenGhoul2.Clean(&instance)&&!instance);
 std::cout<<"JKA production bolt import guards passed\n";
}
'''

class JKAPoseTests(unittest.TestCase):
    def test_active_animation_blends_owned_state_inheritance_and_numeric_guards(self):
        compiler=shutil.which('clang++') or shutil.which('g++')
        if not compiler:self.skipTest('C++17 compiler unavailable')
        scratch=ROOT/'.tmp/jka-compat/pose-tests';scratch.mkdir(parents=True,exist_ok=True)
        with tempfile.TemporaryDirectory(dir=scratch) as temporary:
            folder=Path(temporary)
            for fast in (False,True):
                with self.subTest(fast_math=fast):
                    exe=folder/('blend-fast.exe' if fast else 'blend-default.exe')
                    built=subprocess.run([compiler,*compiler_target_args(compiler),'-std=c++17','-O2',
                        '-Wall','-Wextra','-Werror',*(['-ffast-math'] if fast else []),'-I',str(ROOT),
                        str(ROOT/'tests/jka_animation_blend_tests.cpp'),'-o',str(exe)],
                        cwd=ROOT,capture_output=True,text=True,timeout=120)
                    self.assertEqual(built.returncode,0,built.stdout+built.stderr)
                    run=subprocess.run([str(exe)],cwd=ROOT,capture_output=True,text=True,timeout=30)
                    self.assertEqual(run.returncode,0,run.stdout+run.stderr)
                    self.assertIn('active animation blend state and numeric guards passed',run.stdout)

    def test_shared_hierarchy_timing_angles_placement_and_malformed_records(self):
        compiler=shutil.which('clang++') or shutil.which('g++')
        if not compiler:self.skipTest('C++17 compiler unavailable')
        scratch=ROOT/'.tmp/jka-compat/pose-tests';scratch.mkdir(parents=True,exist_ok=True)
        with tempfile.TemporaryDirectory(dir=scratch) as temporary:
            folder=Path(temporary)
            for fast in (False,True):
                with self.subTest(fast_math=fast):
                    exe=folder/('fast.exe' if fast else 'default.exe')
                    flags=['-ffast-math'] if fast else []
                    built=subprocess.run([compiler,*compiler_target_args(compiler),'-std=c++17','-O2',
                        '-Wall','-Wextra','-Werror',*flags,'-I',str(ROOT),
                        str(ROOT/'tests/jka_pose_tests.cpp'),'-o',str(exe)],
                        cwd=ROOT,capture_output=True,text=True,timeout=120)
                    self.assertEqual(built.returncode,0,built.stdout+built.stderr)
                    run=subprocess.run([str(exe)],cwd=ROOT,capture_output=True,text=True,timeout=30)
                    self.assertEqual(run.returncode,0,run.stdout+run.stderr)
                    self.assertIn('shared pose kernel tests passed',run.stdout)

    def test_actual_three_bolt_imports_bounds_and_output_transactions(self):
        compiler=shutil.which('clang++') or shutil.which('g++')
        if not compiler:self.skipTest('C++17 compiler unavailable')
        scratch=ROOT/'.tmp/jka-compat/pose-tests';scratch.mkdir(parents=True,exist_ok=True)
        with tempfile.TemporaryDirectory(dir=scratch) as temporary:
            folder=Path(temporary);source=folder/'dispatch.cpp';source.write_text(dispatch_fixture())
            exe=folder/'dispatch.exe'
            built=subprocess.run([compiler,*compiler_target_args(compiler),'-std=c++17','-O2','-ffast-math',
                '-Wall','-Wextra','-Werror','-I',str(ROOT),'-I',str(ROOT/'tests'),str(source),'-o',str(exe)],
                cwd=ROOT,capture_output=True,text=True,timeout=120)
            self.assertEqual(built.returncode,0,built.stdout+built.stderr)
            run=subprocess.run([str(exe)],cwd=ROOT,capture_output=True,text=True,timeout=30)
            self.assertEqual(run.returncode,0,run.stdout+run.stderr)
            self.assertIn('production bolt import guards passed',run.stdout)

if __name__=='__main__':unittest.main()
