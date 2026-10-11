"""Execute real cgame bolt records, frame clocks and owned poses."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args
from jka_cgame_ghoul2_tests import fixture as ghoul2_fixture
from rtcw_server_world_tests import function

ROOT = Path(__file__).resolve().parents[1]


def fixture():
    source = ghoul2_fixture().replace('int main(){', 'int LegacyCgameHostMain(){', 1)
    # A renamed main no longer receives C++'s implicit return-zero rule.
    at = source.rfind('}')
    assert source[at:].strip() == '}'
    source = source[:at] + '\nreturn 0;\n' + source[at:]
    poses = (ROOT / 'tests/jka_pose_tests.cpp').read_text()
    source += '\nnamespace pose = fntech3::ghoul2::pose;\n'
    source += poses[poses.index('static bool NearPose('):poses.index('static void CheckHierarchyAndInheritance()')]
    source += r'''
#include "code/cgame/cg_public.h"
#include "code/renderercommon/tr_types.h"
static comGameId_t frameProfile;
int Com_GameIsJKAMP(){return frameProfile==COM_GAME_JKA_MP;}
int Com_GameIsWolfET(){return frameProfile==COM_GAME_WOLFET;}
static struct { int serverTime; } cl;
static struct { qboolean demoplaying; } clc;
static cvar_t ownServer;
cvar_t *com_sv_running=&ownServer;
enum { HUD_PANEL_NONE=0 };
static int panelCalls,beginCalls,endCalls,frameCalls;
static int CL_WolfETHudPanel(){return 37;}
static void CL_HudSetGamePanel(int panel){++panelCalls;assert(panel==(Com_GameIsWolfET()?37:0));}
static void CL_HudBeginFrame(){++beginCalls;}
static void CL_HudEndFrame(){++endCalls;}
intptr_t QDECL VM_Call(vm_t *vm,int count,int ordinal,...){
 assert(vm==cgvm&&count==3&&ordinal==CG_DRAW_ACTIVE_FRAME);
 va_list arguments;va_start(arguments,ordinal);
 assert(va_arg(arguments,int)==cl.serverTime);assert(va_arg(arguments,int)==STEREO_CENTER);
 assert(va_arg(arguments,int)==clc.demoplaying);va_end(arguments);++frameCalls;
 if(Com_GameIsJKAMP())assert(jkaCgameBoneClocks.FrameReady()&&jkaCgameBoneClocks.Time()==cl.serverTime);
 return 0;
}
'''
    source += function((ROOT / 'code/client/cl_cgame.cpp').read_text(), 'void CL_CGameRendering( stereoFrame_t stereo ) {')
    source += r'''
static void CheckClocks(){
 using fntech3::jka::cgame::clock::State;
 State observed;
 assert(observed.Time()==0&&!observed.FrameReady()&&!observed.LocalServer());
 assert(observed.Set(0,1000)&&observed.Set(1,1200)&&observed.Time()==1200);
 assert(observed.Set(1,1201)&&observed.Time()==1000);
 assert(!observed.Set(-1,77)&&!observed.Set(2,77)&&observed.Time()==1000);
 assert(observed.Set(1,0)&&observed.Time()==1000);
 observed.BeginFrame(777,false);assert(observed.FrameReady()&&!observed.LocalServer()&&observed.Time()==777);
 observed.BeginFrame(888,true);assert(observed.LocalServer());
 observed.Reset();assert(!observed.FrameReady()&&!observed.LocalServer()&&observed.Time()==0);
 assert(observed.Set(0,INT32_MAX-50)&&observed.Set(1,INT32_MAX-51)&&observed.Time()==INT32_MAX-50);
 observed.BeginFrame(INT32_MIN+10,false);assert(observed.Time()==INT32_MIN+10);
}
int main(){
 assert(LegacyCgameHostMain()==0);CheckClocks();CL_JKACgameClear();
 vm_t vm{};vm.index=VM_CGAME;vm.entryPoint=reinterpret_cast<decltype(vm.entryPoint)>(1);cgvm=&vm;
 assets["models/players/test/model.glm"]=Mesh("models/players/test/skeleton",3);
 assets["models/players/test/skeleton.gla"]=PoseSkeleton();
 void *identity=nullptr;fntech3::ghoul2::LoadError error;
 assert(CL_JKACgameGhoul2Store().AddModel(&identity,"models/players/test/model.glm",error)==0);
 const auto id=reinterpret_cast<intptr_t>(identity);
 const int bolt=CL_JKACgameGhoul2Store().AddBolt(identity,0,"child");assert(bolt==0);
 assert(CL_JKACgameGhoul2Store().SetBoneAnim(identity,0,"model_root",0,3,0x10,1,0,-1,0));
 std::array<byte,128> storage;storage.fill(0xa5);
 const float angles[]{0,0,0},position[]{7,-11,13},scale[]{2,.5f,0};
 std::memcpy(storage.data()+65,angles,12);std::memcpy(storage.data()+81,position,12);std::memcpy(storage.data()+97,scale,12);
 auto at=[&](int offset){return reinterpret_cast<intptr_t>(storage.data()+offset);};
 intptr_t args[]{294,id,0,bolt,at(3),at(65),at(81),INT_MIN,INTPTR_MAX,at(97)};
 const auto original=storage;Reject([&]{Invoke(args);});assert(storage==original);
 for(unsigned profile=COM_GAME_QUAKE3;profile<COM_GAME_COUNT;++profile){
  frameProfile=static_cast<comGameId_t>(profile);jkaCgameBoneClocks.Reset();
  cl.serverTime=75;clc.demoplaying=qtrue;ownServer.integer=0;CL_CGameRendering(STEREO_CENTER);
  assert(jkaCgameBoneClocks.FrameReady()==(frameProfile==COM_GAME_JKA_MP));
 }
 assert(frameCalls==16&&panelCalls==16&&beginCalls==16&&endCalls==16);
 frameProfile=COM_GAME_JKA_MP;CL_CGameRendering(STEREO_CENTER);assert(Invoke(args)==1);
 float matrix[12];std::memcpy(matrix,storage.data()+3,48);
 assert(NearPose(matrix[0],1)&&NearPose(matrix[7],-5)&&NearPose(matrix[11],10));
 // Six native words: opaque owner, name, ignored time, float output,
 // ignored model list and model index. Output intentionally unaligned.
 std::array<byte,11> frameStorage;frameStorage.fill(0xa5);
 auto readFrame=[&]{float value;std::memcpy(&value,frameStorage.data()+3,4);return value;};
 intptr_t frameArgs[]{305,id,reinterpret_cast<intptr_t>("model_root"),INT_MIN,
   reinterpret_cast<intptr_t>(frameStorage.data()+3),INTPTR_MAX,0};
 assert(Invoke(frameArgs)==1&&readFrame()==1.5f&&frameStorage[2]==0xa5&&frameStorage[7]==0xa5);
 const auto framePublished=frameStorage;
 for(int ignored:{INT_MIN,0,75,INT_MAX}){frameArgs[3]=ignored;assert(Invoke(frameArgs)==1&&frameStorage==framePublished);}
 for(intptr_t ignored:{intptr_t(0),intptr_t(-1),INTPTR_MAX}){frameArgs[5]=ignored;assert(Invoke(frameArgs)==1&&frameStorage==framePublished);}
 frameArgs[2]=reinterpret_cast<intptr_t>("unknown");frameStorage.fill(0xa5);const auto frameUntouched=frameStorage;
 assert(!Invoke(frameArgs)&&frameStorage==frameUntouched);
 frameArgs[2]=reinterpret_cast<intptr_t>("child");assert(!Invoke(frameArgs)&&readFrame()==0);
 assert(frameStorage[2]==0xa5&&frameStorage[7]==0xa5);
 assert(CL_JKACgameGhoul2Store().SetBoneAnim(identity,0,"child",0,3,0,1,0,-1,0));
 frameStorage.fill(0xa5);assert(!Invoke(frameArgs)&&readFrame()==0);
 frameArgs[2]=reinterpret_cast<intptr_t>("model_root");
 for(int missing:{-1,32,INT_MAX}){frameArgs[6]=missing;frameStorage=frameUntouched;
   assert(!Invoke(frameArgs)&&frameStorage==frameUntouched);}
 frameArgs[6]=0;
 auto unchangedFrame=[&](auto attempt){frameStorage=frameUntouched;Reject(attempt);assert(frameStorage==frameUntouched);};
 const auto frameOutput=frameArgs[4];
 for(intptr_t bad:{intptr_t(0),intptr_t(-1)}){frameArgs[4]=bad;unchangedFrame([&]{Invoke(frameArgs);});}
 frameArgs[4]=frameOutput;
 std::array<char,MAX_QPATH> unterminatedBone;unterminatedBone.fill('x');
 frameArgs[2]=reinterpret_cast<intptr_t>(unterminatedBone.data());unchangedFrame([&]{Invoke(frameArgs);});
 frameArgs[2]=reinterpret_cast<intptr_t>("model_root");
 vm.index=VM_UI;unchangedFrame([&]{Invoke(frameArgs);});vm.index=VM_CGAME;
 vm.entryPoint=nullptr;unchangedFrame([&]{Invoke(frameArgs);});vm.entryPoint=reinterpret_cast<decltype(vm.entryPoint)>(1);
 vm.dllExports=reinterpret_cast<decltype(vm.dllExports)>(1);unchangedFrame([&]{Invoke(frameArgs);});vm.dllExports=nullptr;
 frameArgs[1]=0;unchangedFrame([&]{Invoke(frameArgs);});frameArgs[1]=id;
 jkaCgameBoneClocks.Reset();unchangedFrame([&]{Invoke(frameArgs);});
 jkaCgameBoneClocks.Set(0,50);jkaCgameBoneClocks.BeginFrame(75,true);
 assert(Invoke(frameArgs)==1&&readFrame()==1.5f);
 jkaCgameBoneClocks.BeginFrame(75,false);
 auto setPlaying=[&](int first,int last,int flags,float speed,int now){
   assert(CL_JKACgameGhoul2Store().SetBoneAnim(identity,0,"model_root",first,last,flags,speed,now,-1,0));};
 for(int flags:{8,16,24,0x48,0x58,0x88,0x98,0xc8,0xd8}){
   setPlaying(0,3,flags,1,0);assert(Invoke(frameArgs)==1&&readFrame()==1.5f);
   jkaCgameBoneClocks.BeginFrame(175,false);
   if(flags&0x50){assert(Invoke(frameArgs)==1);assert(readFrame()==(flags&16?.5f:2.f));}
   else unchangedFrame([&]{Invoke(frameArgs);});
   jkaCgameBoneClocks.BeginFrame(75,false);
 }
 assert(CL_JKACgameGhoul2Store().SetBoneAnim(identity,0,"model_root",0,3,0xc8,.05f,0,-1,150));
 assert(Invoke(frameArgs)==1&&NearPose(readFrame(),.075f));
 setPlaying(0,3,8,0,0);assert(Invoke(frameArgs)==1&&readFrame()==0);
 for(auto parameters:{std::array<int,3>{2,1,8},std::array<int,3>{-1,3,8},std::array<int,3>{0,4,8},std::array<int,3>{0,3,0x108}}){
   setPlaying(parameters[0],parameters[1],parameters[2],1,0);unchangedFrame([&]{Invoke(frameArgs);});}
 setPlaying(0,3,8,-1,0);unchangedFrame([&]{Invoke(frameArgs);});
 setPlaying(0,3,16,(std::numeric_limits<float>::max)(),0);unchangedFrame([&]{Invoke(frameArgs);});
 setPlaying(0,3,8,1,INT_MIN);unchangedFrame([&]{Invoke(frameArgs);});
 setPlaying(0,3,16,1,0);jkaCgameBoneClocks.BeginFrame(75,false);
 assert(Invoke(frameArgs)==1&&readFrame()==1.5f);
 // The source mutation above restores the root animation before existing
 // pose/clear checks; the child has only an inactive metadata override.
 // Native296 shares the bounded owner/clock/vector core but preserves
 // its pre-correction rotation. A false query leaves that selector set.
 args[0]=296;assert(Invoke(args)==1&&!jkaCgameNoRotationPending);
 std::memcpy(matrix,storage.data()+3,48);
 assert(NearPose(matrix[0],0)&&NearPose(matrix[1],-1)&&NearPose(matrix[4],1));
 assert(NearPose(matrix[3],7)&&NearPose(matrix[7],-5)&&NearPose(matrix[11],10));
 assert(storage[2]==0xa5&&storage[51]==0xa5);
 const auto noRotation=storage;
 for(int time:{INT_MIN,0,75,INT_MAX}){args[7]=time;assert(Invoke(args)==1&&storage==noRotation);}
 args[3]=-1;assert(Invoke(args)==0&&jkaCgameNoRotationPending);
 std::memcpy(matrix,storage.data()+3,48);
 assert(NearPose(matrix[1],-1)&&NearPose(matrix[4],1)&&NearPose(matrix[7],-11));
 args[0]=294;args[3]=bolt;assert(Invoke(args)==1&&storage==noRotation&&!jkaCgameNoRotationPending);
 assert(Invoke(args)==1);std::memcpy(matrix,storage.data()+3,48);assert(NearPose(matrix[0],1));
 // The no-rotation variant retains all malformed-vector/span/context guards.
 args[0]=296;
 for(int start:{65,81,97})for(std::uint32_t bits:{0x7f800000u,0xff800000u,0x7fc00001u}){
  const auto clean=storage;std::memcpy(storage.data()+start,&bits,4);const auto invalid=storage;
  Reject([&]{Invoke(args);});assert(storage==invalid);storage=clean;
 }
 for(int field:{4,5,6,9}){const auto old=args[field];args[field]=0;const auto clean=storage;
  Reject([&]{Invoke(args);});assert(storage==clean);args[field]=old;}
 vm.index=VM_UI;const auto foreign=storage;Reject([&]{Invoke(args);});assert(storage==foreign);vm.index=VM_CGAME;
 assert(Invoke(args)==1&&!jkaCgameNoRotationPending);args[0]=294;assert(Invoke(args)==1);
 assert(storage[2]==0xa5&&storage[51]==0xa5);const auto posed=storage;
 for(int time:{INT_MIN,0,75,1000,INT_MAX}){args[7]=time;assert(Invoke(args)==1&&storage==posed);}
 for(int missing:{-1,32,INT_MAX}){
  args[2]=missing;assert(Invoke(args)==0);std::memcpy(matrix,storage.data()+3,48);
  assert(NearPose(matrix[1],-1)&&NearPose(matrix[4],1)&&NearPose(matrix[10],1));
  assert(NearPose(matrix[3],7)&&NearPose(matrix[7],-11)&&NearPose(matrix[11],13));
  assert(storage[2]==0xa5&&storage[51]==0xa5);
 }
 args[2]=0;args[3]=-1;assert(Invoke(args)==0);
 const float yaw=90;std::memcpy(storage.data()+69,&yaw,4);assert(Invoke(args)==0);
 std::memcpy(matrix,storage.data()+3,48);assert(NearPose(matrix[0],-1)&&NearPose(matrix[5],-1)&&NearPose(matrix[10],1));
 std::memcpy(storage.data()+65,angles,12);args[3]=bolt;
 for(int start:{65,81,97})for(int field=0;field<3;++field)for(std::uint32_t bad:{0x7f800000u,0xff800000u,0x7fc00001u}){
  const auto clean=storage;std::memcpy(storage.data()+start+field*4,&bad,4);const auto invalid=storage;
  Reject([&]{Invoke(args);});assert(storage==invalid);storage=clean;
 }
 for(int word:{4,5,6,9}){const auto old=args[word];args[word]=0;const auto saved=storage;
  Reject([&]{Invoke(args);});assert(storage==saved);args[word]=old;}
 const auto saved=storage;args[4]=static_cast<intptr_t>(-1);Reject([&]{Invoke(args);});assert(storage==saved);args[4]=at(3);
 vm.index=VM_UI;Reject([&]{Invoke(args);});assert(storage==saved);vm.index=VM_CGAME;
 vm.entryPoint=nullptr;Reject([&]{Invoke(args);});assert(storage==saved);vm.entryPoint=reinterpret_cast<decltype(vm.entryPoint)>(1);
 vm.dllExports=reinterpret_cast<decltype(vm.dllExports)>(1);Reject([&]{Invoke(args);});assert(storage==saved);vm.dllExports=nullptr;
 assert(Invoke(args)==1);const auto remotePose=storage;
 ownServer.integer=1;CL_CGameRendering(STEREO_CENTER);assert(Invoke(args)==1&&storage==remotePose);ownServer.integer=0;
 intptr_t getter[]{325};intptr_t setBase[]{326,1000,0},setClient[]{326,1200,1};
 assert(!Invoke(setBase)&&!Invoke(setClient)&&Invoke(getter)==1200);
 setClient[1]=1201;assert(!Invoke(setClient)&&Invoke(getter)==1000);
 for(int index:{-1,2,INT_MAX}){const auto previous=jkaCgameBoneClocks.Time();setClient[2]=index;
  Reject([&]{Invoke(setClient);});assert(jkaCgameBoneClocks.Time()==previous);}
 for(int ordinal:{295}){intptr_t unseen=-1;assert(!CL_JKACgameGhoul2Call(args,
  fntech3::jka::module::Lookup(COM_GAME_JKA_MP,fntech3::jka::module::Module::CGAME,
    fntech3::jka::module::Direction::IMPORT,ordinal),unseen));}
 const auto guarded=storage;float bad[]{0,std::numeric_limits<float>::infinity(),0};
 assert(!pose::FailureBolt(bad,position,reinterpret_cast<float*>(storage.data()+3))&&storage==guarded);
 assert(!pose::FailureBolt(angles,nullptr,reinterpret_cast<float*>(storage.data()+3))&&storage==guarded);
 const int retainedTime=jkaCgameBoneClocks.Time();
 jkaCgameNoRotationPending=true;CL_JKACgameClear();assert(!jkaCgameBoneClocks.FrameReady()&&jkaCgameBoneClocks.Time()==retainedTime);assert(jkaCgameNoRotationPending);
 args[1]=id;Reject([&]{Invoke(args);});assert(storage==guarded);
 unchangedFrame([&]{Invoke(frameArgs);});
 assert(openFiles.empty());std::cout<<"Owned JKA cgame bolt/frame/failure guards passed\n";
}
'''
    return source


class JKACgameBoltTests(unittest.TestCase):
    def test_actual_frame_clock_owned_pose_fallback_and_native_records(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler unavailable')
        parent = ROOT / '.tmp/tests/jka-cgame-bolt'
        at = parent
        while at != ROOT:
            if at.exists():
                assert not at.is_symlink() and not getattr(at.lstat(), 'st_file_attributes', 0) & 0x400
            at = at.parent
        parent.mkdir(parents=True, exist_ok=True)
        assert parent.resolve().is_relative_to(ROOT.resolve())
        with tempfile.TemporaryDirectory(dir=parent) as temporary:
            folder = Path(temporary)
            source = folder / 'bolt.cpp'
            source.write_text(fixture())
            for extra in ([], ['-O2', '-ffast-math']):
                # One executable per build: Windows can hold the last one run.
                executable = folder / ('bolt-fast.exe' if extra else 'bolt.exe')
                built = subprocess.run([compiler, *compiler_target_args(compiler), '-std=c++17', *extra,
                    '-I', str(ROOT), str(source), '-o', str(executable)], cwd=ROOT,
                    capture_output=True, text=True, timeout=120)
                self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
                options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
                ran = subprocess.run([str(executable)], cwd=folder, capture_output=True,
                    text=True, timeout=30, **options)
                self.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)
            assert folder.resolve().is_relative_to(parent.resolve())
            assert not any(item.is_symlink() or getattr(item.lstat(), 'st_file_attributes', 0) & 0x400
                           for item in folder.rglob('*'))


if __name__ == '__main__':
    unittest.main()
