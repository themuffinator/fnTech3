"""Production Academy UI ownership, scene-time posing and Fn MDR submission."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args
from jka_ui_ghoul2_tests import fixture as ownership_fixture
from rtcw_music_fixture import function

ROOT = Path(__file__).resolve().parents[1]


def fixture():
    ownership = ownership_fixture().split('int main(){')[0]
    poses = (ROOT / 'tests/jka_pose_tests.cpp').read_text()
    host = (ROOT / 'code/client/client_jka_ui_host.hpp').read_text()
    first = host.index('\tcase Call::UI_R_ADDREFENTITYTOSCENE:')
    last = host.index('\tcase Call::UI_KEY_GETBINDINGBUF:', first)
    return ownership + r'''
#include "code/ui/ui_public.h"
#include "code/renderercommon/tr_mdr_format.h"
namespace pose=fntech3::ghoul2::pose;
''' + '\n'.join(function(poses, x) for x in ('static std::size_t BoneRecord(',
        'static void PutFloat(', 'static Bytes PoseSkeleton(')) + r'''
static int registrations,updates,draws,lights;static bool registerFailure,updateFailure;
static std::uint64_t capturedOwner;static Bytes imported;static std::vector<float> matrices;
static refEntity_t captured;static std::array<float,3> capturedMins,capturedMaxs;
static qhandle_t Register(std::uint64_t owner,const void *bytes,int length){
 assert(owner&&length>0);fnMdrView_t view;
 assert(FnMdr_Open(bytes,static_cast<std::size_t>(length),2000,4000,&view));
 assert(view.numFrames==1&&view.numBones==3&&view.numLODs==1&&!view.compressed);
 ++registrations;capturedOwner=owner;
 if(registerFailure)return 0;
 const auto *first=static_cast<const unsigned char *>(bytes);imported.assign(first,first+length);return 37;
}
static qboolean Update(qhandle_t handle,std::uint64_t owner,const float *bones,int count,const vec3_t mins,const vec3_t maxs){
 assert(handle==37&&owner==capturedOwner&&count==3);++updates;
 matrices.assign(bones,bones+12*count);std::copy(mins,mins+3,capturedMins.begin());std::copy(maxs,maxs+3,capturedMaxs.begin());
 return updateFailure?qfalse:qtrue;
}
static void Draw(const refEntity_t *entity,qboolean integerTime){assert(!integerTime);++draws;captured=*entity;}
static void Light(const vec3_t origin,float radius,float red,float green,float blue){
 assert(origin[0]==90&&origin[1]==-60&&origin[2]==120&&radius==250&&red==1&&green==.5f&&blue==.25f);++lights;
}
static struct {decltype(&Register) RegisterRuntimeMDR=Register;decltype(&Update) UpdateRuntimeMDRPose=Update;
 decltype(&Draw) AddRefEntityToScene=Draw;decltype(&Light) AddLightToScene=Light;} re;
#include "code/client/client_jka_ui_render.hpp"
static bool Dispatch(fntech3::jka::module::Call call,intptr_t *args,intptr_t &operation,intptr_t &result){
 using namespace fntech3::jka;using module::Call;result=0;switch(call){
''' + host[first:last] + r'''
 default:assert(false);return false;}}
int main(){
 vm_t vm{};vm.entryPoint=reinterpret_cast<decltype(vm.entryPoint)>(1);uivm=&vm;
 if(sizeof(void*)!=4)return 0;
 std::array<byte,16> lightBytes;lightBytes.fill(0xa5);const float lightOrigin[]{90,-60,120};
 std::memcpy(lightBytes.data()+1,lightOrigin,sizeof(lightOrigin));
 intptr_t lightArgs[]{25,reinterpret_cast<intptr_t>(lightBytes.data()+1),FloatWord(250),FloatWord(1),FloatWord(.5f),FloatWord(.25f)};
 intptr_t lightOperation=-1,lightResult=-1;
 assert(Dispatch(fntech3::jka::module::Call::UI_R_ADDLIGHTTOSCENE,lightArgs,lightOperation,lightResult)&&lights==1);
 assert(lightBytes[0]==0xa5&&lightBytes[13]==0xa5);
 for(int n=2;n<=5;++n){auto prior=lightArgs[n];lightArgs[n]=0x7fc00001;
  Reject([&]{Dispatch(fntech3::jka::module::Call::UI_R_ADDLIGHTTOSCENE,lightArgs,lightOperation,lightResult);});lightArgs[n]=prior;}
 const std::uint32_t infinity=0x7f800000;std::memcpy(lightBytes.data()+1,&infinity,4);
 Reject([&]{Dispatch(fntech3::jka::module::Call::UI_R_ADDLIGHTTOSCENE,lightArgs,lightOperation,lightResult);});
 lightArgs[1]=0;Reject([&]{Dispatch(fntech3::jka::module::Call::UI_R_ADDLIGHTTOSCENE,lightArgs,lightOperation,lightResult);});assert(lights==1);
 auto files=Installed();assets=files.files;assets["models/players/_humanoid/_humanoid.gla"]=PoseSkeleton();
 std::uint32_t identity=0;char filename[]="models/players/test/model.glm";
 intptr_t init[]{208,reinterpret_cast<intptr_t>(&identity),reinterpret_cast<intptr_t>(filename),0,0,0,0,0};
 assert(Invoke(init)==0&&identity);
 char bone[]="model_root";intptr_t animate[]{213,static_cast<intptr_t>(identity),0,reinterpret_cast<intptr_t>(bone),0,3,0x48,FloatWord(1),0,FloatWord(-1),150};
 assert(Invoke(animate)==1);
 fntech3_jka_mp_refEntity_t record{};record.ghoul2=identity;record.frame=77;record.oldframe=88;record.backlerp=.25f;
 for(int axis=0;axis<3;++axis)record.axis[axis][axis]=1;
 record.origin[0]=19;record.shaderTime=2.5f;const auto pristine=record;
 intptr_t args[]{0,reinterpret_cast<intptr_t>(&record)},operation=-1,result=-1;
 assert(Dispatch(fntech3::jka::module::Call::UI_R_ADDREFENTITYTOSCENE,args,operation,result));
 assert(!std::memcmp(&record,&pristine,sizeof(record))&&draws==0&&registrations==0);
 record.origin[0]=999; // Staging owns the record, not the module's address.
 fntech3_jka_mp_refdef_t view{};view.width=640;view.height=480;view.fov_x=90;view.fov_y=75;view.time=0;
 args[1]=reinterpret_cast<intptr_t>(&view);
 assert(!Dispatch(fntech3::jka::module::Call::UI_R_RENDERSCENE,args,operation,result)&&operation==UI_R_RENDERSCENE);
 assert(registrations==1&&draws==1&&captured.hModel==37&&captured.origin[0]==19&&captured.frame==0&&captured.oldframe==0&&captured.backlerp==0&&captured.shaderTime.f==2.5f);
 const auto firstOwner=capturedOwner;
 // Cgame shares this allocator; another caller's owner cannot collide with UI.
 const auto otherOwner=fntech3::jka::mdr::NextOwner();assert(otherOwner>firstOwner);
 intptr_t clock[]{228,50,0};assert(Invoke(clock)==0);clock[2]=1;assert(Invoke(clock)==0);
 CL_JKAUIRenderQueue(pristine);CL_JKAUIRenderFlush(99999);assert(registrations==1&&updates==1&&draws==2);
 pose::Palette expected;const auto &model=CL_JKAUIRenderModel(identity);
 assert(CL_JKAUIGhoul2Store().DeformationPalette(model,50,expected));
 for(std::size_t n=0;n<matrices.size();++n)assert(matrices[n]==expected.deformation[n/12][n%12]);
 for(int axis=0;axis<3;++axis)assert(capturedMins[axis]<=capturedMaxs[axis]);
 CL_JKAUIRenderQueue(pristine);CL_JKAUIRenderClearScene();CL_JKAUIRenderFlush(100);assert(draws==2&&updates==1);
 auto bad=pristine;bad.ghoul2=0x12345678;Reject([&]{CL_JKAUIRenderQueue(bad);});
 for(int kind=0;kind<5;++kind){bad=pristine;if(kind==0)bad.customSkin=1;if(kind==1)bad.uRefEnt.miniCount=1;if(kind==2)bad.modelScale[1]=2;
  if(kind==3){std::uint32_t nan=0x7fc00001;std::memcpy(&bad.angles[0],&nan,4);}if(kind==4)bad.reType=2;
  Reject([&]{CL_JKAUIRenderQueue(bad);});}
 assert(jkaUIPendingModels.empty()&&draws==2);
 updateFailure=true;CL_JKAUIRenderQueue(pristine);Reject([&]{CL_JKAUIRenderFlush(100);});
 assert(jkaUIPendingModels.empty()&&registrations==1&&draws==2);updateFailure=false;
 CL_JKAUIRenderClear();assert(jkaUIRuntimeModels.empty());
 registerFailure=true;CL_JKAUIRenderQueue(pristine);Reject([&]{CL_JKAUIRenderFlush(0);});assert(draws==2);registerFailure=false;
 CL_JKAUIRenderQueue(pristine);CL_JKAUIRenderFlush(0);assert(capturedOwner>firstOwner&&draws==3);
 assert(capturedOwner>otherOwner);
 CL_JKAUIRenderQueue(pristine);intptr_t clean[]{211,reinterpret_cast<intptr_t>(&identity)};assert(Invoke(clean)==0);
 Reject([&]{CL_JKAUIRenderFlush(0);});assert(jkaUIPendingModels.empty()&&draws==3);
 CL_JKAUIRenderClear();CL_JKAUIGhoul2Clear();assert(!jkaUIGhoul2Store&&jkaUIRuntimeModels.empty()&&opens==closes);
}
'''


class JKAUIRenderTests(unittest.TestCase):
    def test_production_owned_model_scene_time_and_submission(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++ compiler unavailable')
        scratch = ROOT / '.tmp/tests/jka-ui-render'
        scratch.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=scratch) as folder:
            directory = Path(folder)
            source = directory / 'host.cpp'
            executable = directory / ('host.exe' if os.name == 'nt' else 'host')
            source.write_text(fixture())
            built = subprocess.run([compiler, *compiler_target_args(compiler), '-std=c++17', '-O2', '-ffast-math',
                '-Werror', '-D_CRT_SECURE_NO_WARNINGS', '-I', str(ROOT), str(source), '-o', str(executable)],
                cwd=ROOT, capture_output=True, text=True, timeout=120)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            run = subprocess.run([str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=30)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)


if __name__ == '__main__':
    unittest.main()
