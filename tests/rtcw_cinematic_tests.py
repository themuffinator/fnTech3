"""Execute SP/MP UI and cgame cinematic dispatch with native and QVM memory."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]

PRELUDE = r'''
#include "code/qcommon/q_shared.h"
#include "code/compat/rtcw_module_abi.h"
#include <cassert>
#include <climits>
#include <limits>
#include <string>
#include <vector>
using namespace fntech3::rtcw::module;
static comGameProfile_t profile;
const comGameProfile_t *Com_GameProfile(){return &profile;}
int Com_GameIsRTCW(){return profile.id==COM_GAME_RTCW_SP||profile.id==COM_GAME_RTCW_MP;}
int Com_GameIsRTCWSP(){return profile.id==COM_GAME_RTCW_SP;}
void QDECL Com_Error(errorParm_t,const char *,...){throw 1;}
struct vm_t {bool native;unsigned char bytes[512];};
static vm_t *uivm,*cgvm;
static void Bounds(vm_t *vm,intptr_t address,std::size_t size){
 if(!vm||(!vm->native&&(address<0||static_cast<uintptr_t>(address)>sizeof(vm->bytes)||size>sizeof(vm->bytes)-address)))throw 2;
}
#define VM_CHECKBOUNDS Bounds
static void *VM_ExplicitArgPtr(vm_t *vm,intptr_t address){return vm->native?reinterpret_cast<void *>(address):vm->bytes+address;}
static bool CL_RTCWRenderCall(vm_t *,Call,intptr_t *,intptr_t &){return false;}
static bool CL_RTCWAudioCall(vm_t *,Call,intptr_t *){return false;}
static bool CL_RTCWCameraCall(vm_t *,Call,intptr_t *,intptr_t &){return false;}
static intptr_t CL_CGameTrapForProfile(intptr_t value){return value;}
static int backendCalls,operation;
static std::vector<int> values;
static std::string filename;
static const char *moduleName;
static int CIN_PlayCinematic(const char *name,int x,int y,int w,int h,int bits){
 assert(name!=moduleName);filename=name;values={x,y,w,h,bits};++backendCalls;operation=1;
 return filename=="video/missing.roq"?-1:13;
}
static e_status CIN_StopCinematic(int handle){++backendCalls;operation=2;values={handle};return FMV_EOF;}
static e_status CIN_RunCinematic(int handle){++backendCalls;operation=3;values={handle};return handle==13?FMV_PLAY:FMV_EOF;}
static void CIN_DrawCinematic(int handle){++backendCalls;operation=4;values={handle};}
static void CIN_SetExtents(int handle,int x,int y,int w,int h){++backendCalls;operation=5;values={handle,x,y,w,h};}
#include "code/client/client_rtcw_cinematic_compat.hpp"
template<class F>static void Reject(F action){int before=backendCalls;bool failed=false;try{action();}catch(int){failed=true;}assert(failed&&before==backendCalls);}
'''

CHECKS = r'''
int main(){
 for(auto id:{COM_GAME_RTCW_SP,COM_GAME_RTCW_MP})for(bool native:{false,true}){
  profile.id=id;vm_t vm{native,{}};uivm=cgvm=&vm;
  auto ptr=[&](int offset)->intptr_t{return native?reinterpret_cast<intptr_t>(vm.bytes+offset):offset;};
  auto name=[&](const std::string &text){assert(text.size()<400);memcpy(vm.bytes+13,text.c_str(),text.size()+1);moduleName=reinterpret_cast<char *>(vm.bytes+13);return ptr(13);};
  for(bool ui:{false,true}){
   auto dispatch=ui?CL_UISystemCalls:CL_CgameSystemCalls;
   const auto play=ui?Call::UI_CIN_PLAYCINEMATIC:Call::CG_CIN_PLAYCINEMATIC;
   const auto stop=ui?Call::UI_CIN_STOPCINEMATIC:Call::CG_CIN_STOPCINEMATIC;
   const auto run=ui?Call::UI_CIN_RUNCINEMATIC:Call::CG_CIN_RUNCINEMATIC;
   const auto draw=ui?Call::UI_CIN_DRAWCINEMATIC:Call::CG_CIN_DRAWCINEMATIC;
   const auto extents=ui?Call::UI_CIN_SETEXTENTS:Call::CG_CIN_SETEXTENTS;
   for(int flags=0;flags<64;++flags){
    intptr_t words[]={Ordinal(id,play),name("briefing.roq"),-10,0,640,480,flags};
    assert(dispatch(words)==13&&operation==1&&filename=="video/briefing.roq");
    assert((values==std::vector<int>{-10,0,640,480,flags}));
   }
   for(const std::string &text:{std::string("video/test.roq"),std::string("video\\test.roq"),std::string(300,'x'),std::string("video/")+std::string(280,'x')}){
    intptr_t words[]={Ordinal(id,play),name(text),0,0,0,0,0};assert(dispatch(words)==13);
    assert(filename==(text.find_first_of("/\\")==std::string::npos?"video/"+text.substr(0,249):text.substr(0,255)));
   }
   intptr_t missing[]={Ordinal(id,play),name("missing.roq"),0,0,640,480,0};assert(dispatch(missing)==-1);
   for(int handle:{-1,0,13,16,INT_MAX,INT_MIN}){
    intptr_t stopped[]={Ordinal(id,stop),handle};assert(dispatch(stopped)==FMV_EOF&&operation==2&&values[0]==handle);
    intptr_t running[]={Ordinal(id,run),handle};assert(dispatch(running)==(handle==13?FMV_PLAY:FMV_EOF)&&operation==3&&values[0]==handle);
    intptr_t drawing[]={Ordinal(id,draw),handle};assert(dispatch(drawing)==0&&operation==4&&values[0]==handle);
    intptr_t moved[]={Ordinal(id,extents),handle,-10,INT_MIN,INT_MAX,480};assert(dispatch(moved)==0&&operation==5);
    assert((values==std::vector<int>{handle,-10,INT_MIN,INT_MAX,480}));
   }
   for(intptr_t bad:{intptr_t(0),static_cast<intptr_t>(UINTPTR_MAX)}){
    intptr_t words[]={Ordinal(id,play),bad,0,0,640,480,0};Reject([&]{dispatch(words);});
   }
   if(!native){
    vm.bytes[511]='x';intptr_t words[]={Ordinal(id,play),511,0,0,640,480,0};Reject([&]{dispatch(words);});
    vm.bytes[511]=0;assert(dispatch(words)==13&&filename=="video/");
   }
   if(sizeof(intptr_t)>4){
    const intptr_t wide=static_cast<intptr_t>(std::uint64_t(1)<<40);
    intptr_t words[]={Ordinal(id,play),name("valid.roq"),0,0,640,480,0};
    for(int i=2;i<7;++i){auto old=words[i];words[i]=wide;Reject([&]{dispatch(words);});words[i]=old;}
    intptr_t moved[]={Ordinal(id,extents),13,0,0,640,480};
    for(int i=1;i<6;++i){auto old=moved[i];moved[i]=-wide;Reject([&]{dispatch(moved);});moved[i]=old;}
    for(auto call:{stop,run,draw}){intptr_t scalar[]={Ordinal(id,call),wide};Reject([&]{dispatch(scalar);});}
   }
   intptr_t letterbox[]={Ordinal(id,play),name("valid.roq"),0,60,640,360,32};
   assert(dispatch(letterbox)==13);
   assert((values==std::vector<int>{0,60,640,360,32}));
  }
  intptr_t words[]={0,name("valid.roq"),0,0,640,480,0},result=999;
  Reject([&]{CL_RTCWCinematicCall(nullptr,Call::UI_CIN_PLAYCINEMATIC,words,result);});assert(result==999);
  assert(!CL_RTCWCinematicCall(nullptr,Call::Invalid,nullptr,result)&&result==999);
 }
 for(auto id:{COM_GAME_QUAKE3,COM_GAME_QUAKELIVE}){
  profile.id=id;intptr_t result=999;int before=backendCalls;
  for(auto call:{Call::UI_CIN_PLAYCINEMATIC,Call::CG_CIN_RUNCINEMATIC,Call::UI_CIN_DRAWCINEMATIC})
   assert(!CL_RTCWCinematicCall(nullptr,call,nullptr,result)&&result==999&&backendCalls==before);
 }
}
'''


class RTCWCinematicTests(unittest.TestCase):
    def test_native_qvm_dispatch_and_checked_cinematic_arguments(self):
        compiler = shutil.which('clang++')
        if not compiler:
            self.skipTest('Clang unavailable')
        bodies = []
        for filename, signature in [('cl_cgame.cpp', 'CL_CgameSystemCalls'), ('cl_ui.cpp', 'CL_UISystemCalls')]:
            source = (ROOT / 'code/client' / filename).read_text()
            start = source.index(f'static intptr_t {signature}(')
            prefix = source[start:source.index('\t\tswitch ( call )', start)]
            bodies.append(prefix + '\t}\nreturn -999;\n}\n')
        target = compiler_target_args(compiler)
        if os.name == 'nt' and not target:
            target = ['-DFNTECH3_Q3_ONLY']
        elif os.name != 'nt':
            target = ['-m32']
        with tempfile.TemporaryDirectory(prefix='rtcw-cinematic-', dir=ROOT / '.tmp') as temp:
            folder = Path(temp); cpp = folder / 'probe.cpp'; exe = folder / 'probe.exe'
            cpp.write_text(PRELUDE + '\n'.join(bodies) + CHECKS, encoding='utf-8')
            built = subprocess.run([compiler, *target, '-std=c++17', '-O2', '-I', str(ROOT), str(cpp), '-o', str(exe)],
                                   cwd=ROOT, capture_output=True, text=True, timeout=60)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
            run = subprocess.run([str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=30, **options)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)


if __name__ == '__main__':
    unittest.main()
