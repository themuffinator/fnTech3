"""Exercise RTCW entity/listener imports with exact native and QVM buffers."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args
from rtcw_music_tests import IMPORTS

ROOT = Path(__file__).resolve().parents[1]

RECORDERS = r'''
static int spatialEntity, spatialWater;
static std::array<float,3> spatialOrigin;
static std::array<float,9> spatialAxis;
static const void *lastOrigin, *lastAxis;
static void S_UpdateEntityPosition(int entity,const float *origin){
 operation=7;spatialEntity=entity;std::memcpy(spatialOrigin.data(),origin,12);lastOrigin=origin;
}
// The liquid feed takes the same entity and the copied origin (cl_cgame.cpp).
static int liquidEntity=-1;
static std::array<float,3> liquidOrigin;
static void CL_LiquidFeedEntity(int entity,const float *origin){
 liquidEntity=entity;std::memcpy(liquidOrigin.data(),origin,12);
}
static void S_Respatialize(int entity,const float *origin,float axis[3][3],int water){
 operation=8;spatialEntity=entity;spatialWater=water;
 std::memcpy(spatialOrigin.data(),origin,12);std::memcpy(spatialAxis.data(),axis,36);
 lastOrigin=origin;lastAxis=axis;axis[0][0]=999; // An old backend may accept mutable axes.
}
'''

CHECKS = r'''
#include "code/client/client_rtcw_audio_compat.hpp"
#include <cmath>
using fntech3::rtcw::module::Call;
template<class F>static void reject(F action){
 const int saved=operation;bool failed=false;
 try{action();}catch(int){failed=true;}
 assert(failed&&operation==saved);
}
int main(){
 const float origin[]={1.5f,-2.5f,3.5f};
 const float axis[]={1,0,0,0,1,0,0,0,1};
 for(bool native:{false,true})for(auto id:{COM_GAME_RTCW_SP,COM_GAME_RTCW_MP}){
  profile=id;vm_t vm{native,{}};
  auto ptr=[&](int offset)->intptr_t{return native?reinterpret_cast<intptr_t>(vm.bytes+offset):offset;};
  std::memcpy(vm.bytes+13,origin,12);std::memcpy(vm.bytes+53,axis,36);
  unsigned char saved[256];std::memcpy(saved,vm.bytes,256);
  for(int entity:{0,127,1023}){
   intptr_t update[]={0,entity,ptr(13)};
   assert(CL_RTCWAudioCall(&vm,Call::CG_S_UPDATEENTITYPOSITION,update));
   assert(operation==7&&spatialEntity==entity&&!memcmp(origin,spatialOrigin.data(),12));
   assert(liquidEntity==entity&&!memcmp(origin,liquidOrigin.data(),12));
   assert(lastOrigin!=vm.bytes+13);
   for(int water:{INT_MIN,0,1,INT_MAX}){
    intptr_t listen[]={0,entity,ptr(13),ptr(53),water};
    assert(CL_RTCWAudioCall(&vm,Call::CG_S_RESPATIALIZE,listen));
    assert(operation==8&&spatialEntity==entity&&spatialWater==water);
    assert(!memcmp(origin,spatialOrigin.data(),12)&&!memcmp(axis,spatialAxis.data(),36));
    assert(lastOrigin!=vm.bytes+13&&lastAxis!=vm.bytes+53&&!memcmp(saved,vm.bytes,256));
   }
  }
  for(int entity:{-1,INT_MIN,1024,INT_MAX}){
   intptr_t words[]={0,entity,ptr(13),ptr(53),0};
   reject([&]{CL_RTCWAudioCall(&vm,Call::CG_S_UPDATEENTITYPOSITION,words);});
   reject([&]{CL_RTCWAudioCall(&vm,Call::CG_S_RESPATIALIZE,words);});
  }
  for(int word:{2,3}){
   intptr_t words[]={0,0,ptr(13),ptr(53),0};words[word]=0;
   reject([&]{CL_RTCWAudioCall(&vm,Call::CG_S_RESPATIALIZE,words);});
   words[word]=static_cast<intptr_t>(UINTPTR_MAX-7);
   reject([&]{CL_RTCWAudioCall(&vm,Call::CG_S_RESPATIALIZE,words);});
   if(!native){words[word]=word==2?245:221;
    reject([&]{CL_RTCWAudioCall(&vm,Call::CG_S_RESPATIALIZE,words);});}
  }
  for(int part=0;part<12;++part)for(uint32_t bits:{UINT32_C(0x7f800000),UINT32_C(0xff800000),UINT32_C(0x7fc00000),UINT32_C(0x7fa00001)}){
   const int offset=part<3?13+part*4:53+(part-3)*4;
   std::memcpy(vm.bytes+offset,&bits,4);
   intptr_t words[]={0,0,ptr(13),ptr(53),0};
   reject([&]{CL_RTCWAudioCall(&vm,Call::CG_S_RESPATIALIZE,words);});
   if(part<3)reject([&]{CL_RTCWAudioCall(&vm,Call::CG_S_UPDATEENTITYPOSITION,words);});
   std::memcpy(vm.bytes,saved,256);
  }
  intptr_t words[]={0,0,ptr(13),ptr(53),0};
  reject([&]{CL_RTCWAudioCall(nullptr,Call::CG_S_RESPATIALIZE,words);});
  reject([&]{CL_RTCWAudioCall(nullptr,Call::CG_S_UPDATEENTITYPOSITION,words);});
  if(sizeof(intptr_t)>4){words[4]=static_cast<intptr_t>(INT64_C(0x100000000));
   reject([&]{CL_RTCWAudioCall(&vm,Call::CG_S_RESPATIALIZE,words);});}
  assert(!memcmp(saved,vm.bytes,256));
 }
 for(auto id:{COM_GAME_QUAKE3,COM_GAME_QUAKELIVE}){
  profile=id;const int saved=operation;
  assert(!CL_RTCWAudioCall(nullptr,Call::CG_S_UPDATEENTITYPOSITION,nullptr));
  assert(!CL_RTCWAudioCall(nullptr,Call::CG_S_RESPATIALIZE,nullptr));
  assert(operation==saved);
 }
}
'''


class RTCWSpatialAudioTests(unittest.TestCase):
    def test_native_and_qvm_positions_and_listener(self):
        prefix = IMPORTS[:IMPORTS.index('#include "code/client/client_rtcw_audio_compat.hpp"')]
        prefix = prefix.replace('static void S_UpdateEntityPosition(int,const float *){assert(false);}', '')
        prefix = prefix.replace('static void CL_LiquidFeedEntity(int,const float *){assert(false);}', '')
        prefix = prefix.replace('static void S_Respatialize(int,const float *,float (*)[3],int){assert(false);}', '')
        compiler = shutil.which('clang++')
        if not compiler:
            self.skipTest('Clang C++ compiler unavailable')
        target = compiler_target_args(compiler)
        if os.name == 'nt' and not target:
            target = ['-DFNTECH3_Q3_ONLY']
        elif os.name != 'nt':
            target = ['-m32']
        (ROOT / '.tmp').mkdir(parents=True, exist_ok=True)
        for optimize in ([], ['-O2', '-ffast-math']):
            with self.subTest(flags=optimize), tempfile.TemporaryDirectory(prefix='rtcw-spatial-audio-', dir=ROOT / '.tmp') as temp:
                folder = Path(temp); source = folder / 'probe.cpp'; executable = folder / 'probe.exe'
                source.write_text(prefix + RECORDERS + CHECKS, encoding='utf-8')
                command = [compiler, *target, *optimize, '-std=c++17', '-I', str(ROOT), str(source), '-o', str(executable)]
                built = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=60)
                self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
                options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
                run = subprocess.run([str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=30, **options)
                self.assertEqual(run.returncode, 0, run.stdout + run.stderr)


if __name__ == '__main__':
    unittest.main()
