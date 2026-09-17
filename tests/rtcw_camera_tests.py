"""Execute camera paths, malformed assets and the native/QVM camera boundary.

All inputs are synthetic. These fixtures never read or generate device input.
Installed-file/reference comparisons are a separate, opt-in source probe.
"""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args
from mesh_tag_tests import function

ROOT = Path(__file__).resolve().parents[1]

CORE = r'''
#include "code/compat/rtcw_camera.hpp"
#include <cassert>
#include <cstdio>
#include <string>
using namespace fntech3::rtcw::camera;
static void Near(float actual,float expected,float tolerance=.001f){assert(std::fabs(actual-expected)<=tolerance);}
static void Load(Manager &m,const std::string &text,int index=0){Diagnostic d;assert(m.Load(index,text.data(),text.size(),d));assert(!d.message);}
static void Start(Manager &m,int time=10000,int index=0){Diagnostic d;assert(m.Start(index,time,d));assert(!d.message);}
static Frame Sample(Manager &m,int time,int index=0){Diagnostic d;Frame f;assert(m.Sample(index,time,f,d)==Result::Frame);assert(!d.message);return f;}
static const char fixed[]=R"(cameraPathDef {
 time 4 camera_fixed { time 4000 name fixed baseVelocity 0 pos (10 20 30) }
 target_fixed { name look pos (20 20 30) }
 event { type 4 param "LOOK" time 0 }
 fov { fov 75 startFOV 1 endFOV 179 time 44 }
})";
'''

PATHS = r'''
int main(){
 Manager m;Diagnostic d;Frame f;
 assert(!Manager::ValidSlot(-1)&&!Manager::ValidSlot(64));
 assert(!m.Load(-1,nullptr,0,d)&&!d.message);
 assert(!m.Start(64,0,d)&&!d.message);
 assert(m.Sample(64,0,f,d)==Result::Ended&&!d.message);
 assert(!m.Start(0,0,d)&&d.message);
 for(int slot=0;slot<64;++slot){Load(m,fixed,slot);Start(m,10000,slot);}
 for(int slot=0;slot<64;++slot){f=Sample(m,10000,slot);assert(f.origin==Vector({10,20,30}));Near(f.yaw,0);Near(f.pitch,0);Near(f.fov,75);}
 // Strict greater-than, after integer seconds truncation, retains the last
 // second. Expiry does not modify the caller's result fields.
 f=Sample(m,14999);Frame saved=f;assert(m.Sample(0,15000,f,d)==Result::Ended);assert(f.origin==saved.origin&&f.fov==saved.fov);
 Load(m,R"(cameraPathDef { time 4
 camera_interpolated { time 2000 baseVelocity 999 startPos (0 0 0) endPos (100 0 0) }
 target_fixed { name look pos (100 100 0) } event {type 4 time 0 param "look"} })");
 Start(m);Near(Sample(m,11000).origin[0],50);Near(Sample(m,12000).origin[0],100);
 Start(m,20000);Near(Sample(m,21000).origin[0],25); // Restart uses the previous timeline duration.
 Near(Sample(m,20000).origin[0],0); // A backwards query integrates a negative interval safely.
 Load(m,R"(cameraPathDef { time 4
 camera_interpolated { time 0 baseVelocity 40 velocity 0 4000 999 startPos (0 0 0) endPos (100 0 0) }
 target_interpolated { time 0 name moving baseVelocity 100 velocity 0 1000 0 velocity 1000 1000 20 startPos (0 100 0) endPos (100 100 0) }
 event {type 4 time 0 param "moving"} })");
 Start(m);f=Sample(m,11000);Near(f.origin[0],40);Near(f.yaw,111.8015f,.002f);
 assert(m.Inspect(0)->position.velocities.empty());assert(m.Inspect(0)->targets[0].velocities.size()==2);
 Load(m,R"(cameraPathDef {time 3 camera_fixed {name
 baseVelocity 0 pos (1 2 3)} target_fixed {name look pos (1 2 13)} event {type 4 time 0 param "look"}})");
 Start(m);f=Sample(m,10000);assert(f.origin==Vector({1,2,3}));Near(f.pitch,90.00008f);assert(m.Inspect(0)->position.name=="baseVelocity");
 Load(m,R"(cameraPathDef {time 5 camera_fixed {pos (0 0 0)}
 event {type 11 param "2.5" time 500} event {type 12 param "1.25" time 0}
 event {type 6 param "105,2" time 1000} fov {fov 75}})");
 Start(m);f=Sample(m,11000);assert(f.fadeCount==2&&f.fades[0].alpha==255&&f.fades[1].alpha==0);
 Near(f.fades[0].seconds,2.5f);Near(f.fov,75);f=Sample(m,12000);Near(f.fov,90);assert(!f.fadeCount);
 Near(Sample(m,13000).fov,105);Start(m,20000);Near(Sample(m,20000).fov,105);
 Load(m,R"(cameraPathDef {time 3 camera_fixed {} event {type 6 time 0 param "45"} fov {fov 80}})");
 Start(m);Near(Sample(m,11000).fov,80); // Zero-duration FOV event retains its stored value.
 Load(m,R"(cameraPathDef {time 3 camera_fixed {} event {type 9 time 1000}})");Start(m);
 assert(m.Sample(0,11000,f,d)==Result::Ended);assert(m.Sample(0,11001,f,d)==Result::Frame); // STOP is a one-shot event.
 Load(m,R"(cameraPathDef {time 3 camera_fixed {} event {type 10 time 0 param "other"}})");Start(m);
 assert(m.Sample(0,10000,f,d)==Result::Invalid&&d.message);
 Load(m,R"(cameraPathDef {time 5 camera_spline {time 5000 target {granularity .25
 (0 0 0) (10 0 0) (20 0 0) (30 0 0)}}})");Start(m);
 const auto &path=m.Inspect(0)->position;assert(path.samples.size()==5);Near(path.distance,10);
 Near(Sample(m,10000).origin[0],10);Near(Sample(m,12500).origin[0],15);
 Near(Sample(m,14999).origin[0],17.5f);Near(Sample(m,15500).origin[0],20); // Final segment snap.
 Load(m,R"(cameraPathDef {time 6 camera_interpolated {time 6000 startPos (0 0 0) endPos (600 0 0)}
 event {type 1 time 2000 param "1"}})");Start(m);
 assert(m.Inspect(0)->position.velocities.size()==202);Near(m.Inspect(0)->totalSeconds,7);
 Near(Sample(m,11000).origin[0],100);Near(Sample(m,11500).origin[0],125.5f);Near(Sample(m,12500).origin[0],125.5f);
 Load(m,R"(cameraPathDef {time 5 camera_interpolated {time 5000 startPos (0 0 0) endPos (500 0 0)}
 event {type 13 time 0}})");Start(m);assert(m.Inspect(0)->position.velocities.size()==201);
 Near(Sample(m,10500).origin[0],24.5f);
 m.Clear(0);assert(!m.Inspect(0)->loaded&&m.Inspect(1)->loaded);m.Clear();for(int i=0;i<64;++i)assert(!m.Inspect(i)->loaded);
}
'''

MODES = r'''
int main(){
 const std::string data=R"(cameraPathDef {time 4 camera_interpolated {
 time 2000 baseVelocity 4 velocity 0 1000 10 startPos (0 0 0) endPos (100 0 0)}
 event {type 13 time 0}})";
 Manager sp,mp;Diagnostic d;
 assert(sp.Load(0,data.data(),data.size(),d,Mode::SinglePlayer));
 assert(mp.Load(0,data.data(),data.size(),d,Mode::MultiPlayer));
 Start(sp);Start(mp);assert(sp.Inspect(0)->position.velocities.size()==201);
 assert(mp.Inspect(0)->position.velocities.size()==1);
 Near(sp.Inspect(0)->position.speed,50);Near(mp.Inspect(0)->position.speed,25);
 Near(Sample(sp,10500).origin[0],12.25f);Near(Sample(mp,10500).origin[0],5);
 Start(sp,20000);Start(mp,20000);Near(sp.Inspect(0)->position.speed,25);Near(mp.Inspect(0)->position.speed,25);
 const std::string wait=R"(cameraPathDef {time 6 camera_interpolated {
 time 2000 baseVelocity 4 startPos (0 0 0) endPos (600 0 0)} event {type 1 time 2000 param "1"}})";
 assert(mp.Load(0,wait.data(),wait.size(),d,Mode::MultiPlayer));Start(mp);
 Near(mp.Inspect(0)->position.speed,100);Near(mp.Inspect(0)->position.velocities.front().speed,4);
 assert(mp.Inspect(0)->position.velocities.size()==202);Start(mp,20000);
 assert(mp.Inspect(0)->position.velocities.size()==404);Near(mp.Inspect(0)->position.velocities.front().speed,4);
}
'''

MALFORMED = r'''
int main(){
 Manager m;Diagnostic d;Frame frame;
 auto reject=[&](const std::string &text){Load(m,fixed);assert(!m.Load(0,text.data(),text.size(),d));assert(d.message&&!m.Inspect(0)->loaded);};
 for(const char *text:{"","cameraPathDef {", "cameraPathDef {} trailing", "/*", "cameraPathDef {camera_fixed {name \"x}}",
 "cameraPathDef {time 1e999}","cameraPathDef {time -1}","cameraPathDef {time 86401}","cameraPathDef {time nan}",
 "cameraPathDef {camera_fixed {time 2147483648}}","cameraPathDef {camera_fixed {pos (1 2)}}",
 "cameraPathDef {camera_fixed {pos (1e38 0 0)} target_fixed {pos (-1e38 0 0)} event {type 4 time 0}}",
 "cameraPathDef {fov {fov 0}}","cameraPathDef {event {type 14}}","cameraPathDef {event {type 6 param \"90,nan\"}}",
 "cameraPathDef {event {type 11 param \"-1\"}}","cameraPathDef {camera_spline {target {granularity 0 (0 0 0)(1 0 0)(2 0 0)(3 0 0)}}}",
 "cameraPathDef {camera_spline {target {granularity -1 (0 0 0)(1 0 0)(2 0 0)(3 0 0)}}}",
 "cameraPathDef {camera_spline {target {granularity .000001 (0 0 0)(1 0 0)(2 0 0)(3 0 0)}}}",
 "cameraPathDef {camera_spline {target {(0 0 0)(1 0 0)(2 0 0)}}}","cameraPathDef {camera_fixed {} camera_fixed {}}"}){
  if(std::strstr(text,"1e38")){Load(m,text);Start(m);assert(m.Sample(0,10000,frame,d)==Result::Invalid);}
  else reject(text);
 }
 reject(std::string("cameraPathDef {}\0x",18));
 reject("cameraPathDef {camera_fixed {name \""+std::string(1024,'a')+"\"}}");
 assert(!m.Load(0,"x",MaxFileBytes+1,d));
 std::string many="cameraPathDef {";for(std::size_t i=0;i<=MaxTargets;++i)many+="target_fixed {} ";reject(many+"}");
 many="cameraPathDef {";for(std::size_t i=0;i<=MaxEvents;++i)many+="event {} ";reject(many+"}");
 many="cameraPathDef {camera_fixed {";for(std::size_t i=0;i<=MaxVelocities;++i)many+="velocity 0 1 2 ";reject(many+"}}");
 Load(m,"cameraPathDef {camera_fixed {} event {type 1 time 0 param \"1\"}}");assert(!m.Start(0,0,d)&&d.message);
 reject("cameraPathDef {camera_interpolated {time 1 startPos (0 0 0) endPos (1e38 0 0)}}");
 // Deterministic mutation coverage checks that parsing and optional queries
 // terminate without out-of-bounds reads, infinities or stale loaded slots.
 std::uint32_t random=0x41b2d397u;
 const std::string seed=fixed;
 for(int i=0;i<3000;++i){std::string text=seed;random=random*1664525u+1013904223u;
  const auto at=random%text.size();if(i%3==0)text.resize(at);else if(i%3==1)text[at]=static_cast<char>(random>>24);else text.insert(at,1,static_cast<char>(random>>24));
  if(m.Load(0,text.data(),text.size(),d)&&m.Start(0,10000,d)){
   if(m.Sample(0,10123,frame,d)==Result::Frame){for(float v:frame.origin)assert(std::isfinite(v));assert(std::isfinite(frame.pitch)&&std::isfinite(frame.yaw)&&std::isfinite(frame.fov));}
  }
 }
}
'''

BOUNDARY = r'''
#define _CRT_SECURE_NO_WARNINGS 1
#include "code/qcommon/q_shared.h"
#include "code/qcommon/qcommon.h"
#include "code/compat/rtcw_module_abi.h"
#include <cassert>
#include <climits>
#include <string>
#include <vector>
using fntech3::rtcw::module::Call;
static comGameProfile_t profile;
int Com_GameIsRTCW(){return profile.id==COM_GAME_RTCW_SP||profile.id==COM_GAME_RTCW_MP;}
int Com_GameIsRTCWSP(){return profile.id==COM_GAME_RTCW_SP;}
const comGameProfile_t *Com_GameProfile(){return &profile;}
struct vm_s {bool native;unsigned char bytes[1024];};
static vm_t *cgvm;
static struct {qboolean rtcwCameraMode;} cl;
static int reads,closes,allocations,frees,opens;
static int advertised=-2,shortRead=0;
static std::string fileText,pathObserved;
static std::vector<std::string> commands;
void QDECL Com_Error(errorParm_t,const char *,...){throw 7;}
void QDECL Com_Printf(const char *,...){}
int QDECL Com_sprintf(char *out,int size,const char *format,...){va_list ap;va_start(ap,format);const int n=vsnprintf(out,size,format,ap);va_end(ap);return n;}
void *VM_ExplicitArgPtr(const vm_t *vm,intptr_t address){return vm->native?reinterpret_cast<void *>(address):const_cast<byte *>(vm->bytes)+address;}
static void Bounds(vm_t *vm,intptr_t address,std::size_t size){
 if(!vm||(!vm->native&&(address<0||static_cast<std::uintptr_t>(address)>sizeof(vm->bytes)||size>sizeof(vm->bytes)-address)))throw 7;
}
#undef VM_CHECKBOUNDS
#define VM_CHECKBOUNDS Bounds
int FS_FOpenFileRead(const char *name,fileHandle_t *file,qboolean unique){assert(unique);++opens;pathObserved=name;*file=pathObserved=="missing.camera"?0:11;return advertised==-2?static_cast<int>(fileText.size()):advertised;}
int FS_Read(void *data,int bytes,fileHandle_t file){assert(file==11&&bytes==static_cast<int>(fileText.size()));++reads;memcpy(data,fileText.data(),bytes-shortRead);return bytes-shortRead;}
void FS_FCloseFile(fileHandle_t file){assert(file==11);++closes;}
void *Hunk_AllocateTempMemory(int bytes){++allocations;return malloc(bytes);}
void Hunk_FreeTempMemory(void *p){++frees;free(p);}
void Cbuf_ExecuteText(cbufExec_t mode,const char *text){assert(mode==EXEC_NOW);commands.emplace_back(text);}
static bool CL_RTCWRenderCall(vm_t *,Call,intptr_t *,intptr_t &){return false;}
static bool CL_RTCWAudioCall(vm_t *,Call,intptr_t *){return false;}
static bool CL_RTCWCinematicCall(vm_t *,Call,intptr_t *,intptr_t &){return false;}
static intptr_t CL_CGameTrapForProfile(intptr_t value){return value;}
#include "code/client/client_rtcw_camera.hpp"
template<class F>static void Reject(F action){bool failed=false;try{action();}catch(int){failed=true;}assert(failed);assert(allocations==frees);}
'''

BOUNDARY_CHECKS = r'''
int main(){
 using namespace fntech3::rtcw::module;
 for(auto id:{COM_GAME_RTCW_SP,COM_GAME_RTCW_MP})for(bool native:{false,true}){
  profile.id=id;vm_t vm{native,{}};cgvm=&vm;CL_ClearRTCWCameras();
  auto ptr=[&](int offset)->intptr_t{return native?reinterpret_cast<intptr_t>(vm.bytes+offset):offset;};
  const auto load=Ordinal(id,Call::CG_LOADCAMERA),start=Ordinal(id,Call::CG_STARTCAMERA),get=Ordinal(id,Call::CG_GETCAMERAINFO);
  assert(load==(id==COM_GAME_RTCW_SP?98:97)&&start==load+1&&get==(id==COM_GAME_RTCW_SP?101:99));
  fileText="cameraPathDef {time 3 camera_fixed {pos (10 20 30)} target_fixed {pos (20 20 30)} event {type 4 time 0} event {type 11 time 0 param \"2\"}}";
  std::strcpy(reinterpret_cast<char *>(vm.bytes+13),"cameras/test.camera");
  intptr_t loading[]={load,0,ptr(13)};assert(CL_CgameSystemCalls(loading)==1&&pathObserved=="cameras/test.camera");
  assert((CL_RTCWCameras().Inspect(0)->mode==fntech3::rtcw::camera::Mode::SinglePlayer)==(id==COM_GAME_RTCW_SP));
  intptr_t starting[]={start,0,10000};assert(CL_CgameSystemCalls(starting)==0&&bool(cl.rtcwCameraMode)==(id==COM_GAME_RTCW_SP));
  std::memset(vm.bytes+501,0x5a,48);std::array<byte,48> untouched{};std::memcpy(untouched.data(),vm.bytes+501,48);
  intptr_t query[]={get,0,10000,ptr(501),ptr(517),ptr(537)};commands.clear();assert(CL_CgameSystemCalls(query)==1);
  float xyz[3],angles[2],fov;memcpy(xyz,vm.bytes+501,12);memcpy(angles,vm.bytes+517,8);memcpy(&fov,vm.bytes+537,4);
  assert(xyz[0]==10&&xyz[1]==20&&xyz[2]==30&&angles[0]==0&&angles[1]==0&&fov==90);
  for(int i:{12,13,14,15,24,25,26,27,28,29,30,31,32,33,34,35,40,41,42,43,44,45,46,47})assert(vm.bytes[501+i]==0x5a);
  assert(commands==std::vector<std::string>{"fade 0 0 0 255 2.000000\n"});
  assert(CL_CgameSystemCalls(query)==1&&commands.size()==1);
  if(id==COM_GAME_RTCW_SP){intptr_t stop[]={100,0};assert(CL_CgameSystemCalls(stop)==0&&!cl.rtcwCameraMode);assert(CL_CgameSystemCalls(query)==1);}
  query[2]=14000;memcpy(untouched.data(),vm.bytes+501,48);assert(CL_CgameSystemCalls(query)==0);assert(!memcmp(untouched.data(),vm.bytes+501,48));query[2]=10000;
  for(int slot:{-1,64,INT_MIN,INT_MAX})for(auto operation:{load,start,get}){intptr_t only[]={operation,slot};assert(CL_CgameSystemCalls(only)==0);}
  auto good=query[4];query[4]=query[3]+1;Reject([&]{CL_CgameSystemCalls(query);});query[4]=good;
  good=query[3];query[3]=0;Reject([&]{CL_CgameSystemCalls(query);});query[3]=good;
  if(!native){query[5]=1022;Reject([&]{CL_CgameSystemCalls(query);});query[5]=ptr(537);loading[2]=1023;vm.bytes[1023]='x';Reject([&]{CL_CgameSystemCalls(loading);});loading[2]=ptr(13);}
  if(sizeof(intptr_t)>4){starting[2]=static_cast<intptr_t>(INT_MAX)+std::int64_t{1};Reject([&]{CL_CgameSystemCalls(starting);});starting[2]=10000;}
  for(const char *name:{"../bad.camera","/bad.camera","E:/bad.camera","","missing.camera"}){
   std::strcpy(reinterpret_cast<char *>(vm.bytes+13),name);const int before=opens;
   assert(CL_CgameSystemCalls(loading)==0&&!CL_RTCWCameras().Inspect(0)->loaded);
   assert(opens==before+(std::string(name)=="missing.camera"));
  }
  std::strcpy(reinterpret_cast<char *>(vm.bytes+13),"cameras/test.camera");
  for(int count:{-1,0,1024*1024+1}){advertised=count;const int before=reads;assert(CL_CgameSystemCalls(loading)==0&&reads==before);}
  advertised=-2;shortRead=1;assert(CL_CgameSystemCalls(loading)==0);shortRead=0;
  fileText="cameraPathDef {";assert(CL_CgameSystemCalls(loading)==0&&!CL_RTCWCameras().Inspect(0)->loaded);
  Reject([&]{CL_CgameSystemCalls(starting);});assert(allocations==frees);
  fileText="cameraPathDef {camera_fixed {}}";assert(CL_CgameSystemCalls(loading)==1);assert(CL_CgameSystemCalls(starting)==0);
  CL_ClearRTCWCameras();assert(!cl.rtcwCameraMode&&!CL_RTCWCameras().Inspect(0)->loaded);
 }
 for(auto id:{COM_GAME_QUAKE3,COM_GAME_QUAKELIVE}){profile.id=id;intptr_t result=999;assert(!CL_RTCWCameraCall(nullptr,Call::CG_LOADCAMERA,nullptr,result)&&result==999);}
}
'''

KEYS = r'''
#include "code/qcommon/q_shared.h"
#include "code/client/keys.h"
#include "code/client/client_rtcw_camera_input.hpp"
#include <cassert>
int main(){
 for(bool sp:{false,true})for(bool active:{false,true})for(bool down:{false,true})for(int catcher=0;catcher<64;++catcher)for(int key=1;key<256;++key){
  const auto action=CL_RTCWCameraKeyAction(sp,active,catcher,key,down);
  auto expected=rtcwCameraKeyAction::Pass;
  if(sp&&active){
   if(!(catcher&(KEYCATCH_CONSOLE|KEYCATCH_UI))&&down)expected=(key==K_ESCAPE||key==K_ENTER||key==K_SPACE)?rtcwCameraKeyAction::Interrupt:rtcwCameraKeyAction::Consume;
   else if((catcher&KEYCATCH_CONSOLE)&&key==K_ESCAPE)expected=rtcwCameraKeyAction::Consume;
  }
  assert(action==expected);
 }
}
'''


class RTCWCameraTests(unittest.TestCase):
    def compile_run(self, source):
        compiler = shutil.which('clang++')
        if not compiler:
            self.skipTest('Clang unavailable')
        target = compiler_target_args(compiler)
        if os.name == 'nt' and not target:
            target = ['-DFNTECH3_Q3_ONLY']
        elif os.name != 'nt':
            target = ['-m32']
        with tempfile.TemporaryDirectory(prefix='rtcw-camera-', dir=ROOT / '.tmp') as temp:
            folder = Path(temp)
            cpp, exe = folder / 'probe.cpp', folder / 'probe.exe'
            cpp.write_text(source, encoding='utf-8')
            built = subprocess.run([compiler, *target, '-std=c++17', '-O2', '-I', str(ROOT), str(cpp), '-o', str(exe)],
                                   cwd=ROOT, capture_output=True, text=True, timeout=60)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
            run = subprocess.run([str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=30, **options)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)

    def test_paths_events_timing_and_slot_lifetime(self):
        self.compile_run(CORE + PATHS)

    def test_original_sp_mp_timing_policies(self):
        self.compile_run(CORE + MODES)

    def test_malformed_camera_files_and_bounded_mutations(self):
        self.compile_run(CORE + MALFORMED)

    def test_native_qvm_camera_dispatch_and_owned_outputs(self):
        source = (ROOT / 'code/client/cl_cgame.cpp').read_text()
        start = source.index('static intptr_t CL_CgameSystemCalls(')
        dispatch = source[start:source.index('\t\tswitch ( call )', start)] + '\t}\nreturn -999;\n}\n'
        self.compile_run(BOUNDARY + dispatch + BOUNDARY_CHECKS)
        shutdown = function(source, 'void CL_ShutdownCGame(')
        self.assertLess(shutdown.index('CL_ClearRTCWCameras();'), shutdown.index('if ( !cgvm )'))

    def test_cutscene_input_policy_without_devices(self):
        self.compile_run(KEYS.replace('#include <cassert>', '#include <cassert>\n#include <initializer_list>'))
        source = (ROOT / 'code/client/cl_keys.cpp').read_text()
        for name in ('static void CL_KeyDownEvent(', 'static void CL_KeyUpEvent('):
            self.assertIn('CL_RTCWCameraKeyAction(', function(source, name))


if __name__ == '__main__':
    unittest.main()
