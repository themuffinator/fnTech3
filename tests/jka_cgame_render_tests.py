"""Production Academy cgame scene staging, skins, poses and Fn renderer hooks."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args
from jka_cgame_ghoul2_tests import fixture as ghoul2_fixture
from rtcw_music_fixture import function

ROOT=Path(__file__).resolve().parents[1]


def fixture():
    ownership=ghoul2_fixture().split('int main(){')[0]
    old='''struct FixtureRenderer {qhandle_t (*RegisterSkin)(const char *);};
static FixtureRenderer re{RegisterSkin};'''
    assert ownership.count(old)==1
    ownership=ownership.replace(old,r'''
#include "code/renderercommon/tr_types.h"
#include "code/renderercommon/tr_mdr_format.h"
static qhandle_t Register(std::uint64_t,const void *,int);
static qboolean Update(qhandle_t,std::uint64_t,const float *,int,const vec3_t,const vec3_t);
static int SceneTag(orientation_t *,qhandle_t,int,int,float,const char *){assert(false);return 0;}
static void SceneLight(const vec3_t,float,float,float,float){assert(false);}
static void ClearBackend();
static void RenderBackend(const refdef_t *);
struct FixtureRenderer {
 qhandle_t (*RegisterSkin)(const char *);
 decltype(&Register) RegisterRuntimeMDR;
 decltype(&Update) UpdateRuntimeMDRPose;
 decltype(&ClearBackend) ClearScene;
 decltype(&RenderBackend) RenderScene;
 decltype(&SceneTag) LerpTag;
 decltype(&SceneLight) AddLightToScene;
 decltype(&SceneLight) AddAdditiveLightToScene;
};
static FixtureRenderer re{RegisterSkin,Register,Update,ClearBackend,RenderBackend,SceneTag,SceneLight,SceneLight};
''')
    host=(ROOT/'code/client/client_jka_cgame_host.hpp').read_text()
    start=host.index('    case Call::CG_R_CLEARSCENE:')
    end=host.index('    case Call::CG_LANGUAGE_ISASIAN:',start)
    shared=(ROOT/'code/client/cl_cgame.cpp').read_text()
    canonical_start=shared.index('\tcase CG_R_RENDERSCENE: {')
    canonical_end=shared.index('\tcase CG_R_SETCOLOR:',canonical_start)
    poses=(ROOT/'tests/jka_pose_tests.cpp').read_text()
    geometry=(ROOT/'tests/jka_geometry_tests.cpp').read_text()
    return ownership+r'''
#include "code/cgame/cg_public.h"
#include "code/compat/jka_render_adapter.hpp"
namespace pose=fntech3::ghoul2::pose;
'''+'\n'.join(function(poses,name) for name in ('static std::size_t BoneRecord(',
        'static void PutFloat(', 'static Bytes PoseSkeleton('))+ '\n'+ '\n'.join(
            function(geometry,name) for name in ('static void GeometryFloat(', 'static Bytes GeometryFixture('))+r'''
namespace scene=fntech3::jka::cgame::scene;
static int registrations,updates,draws,renders,clearCalls;
static bool registerFailure,updateFailure,backendPending;
static std::map<qhandle_t,std::uint64_t> owners;
static std::vector<refEntity_t> submitted;
static Bytes imported;static std::vector<float> matrices;
static std::array<float,3> capturedMins,capturedMaxs;
static qhandle_t Register(std::uint64_t owner,const void *bytes,int length){
 assert(owner&&length>0);fnMdrView_t view;
 assert(FnMdr_Open(bytes,static_cast<std::size_t>(length),2000,4000,&view));
 assert(view.numFrames==1&&view.numBones==3&&view.numLODs==1&&!view.compressed);
 for(const auto &entry:owners)assert(entry.second!=owner);
 ++registrations;if(registerFailure)return 0;
 const auto *first=static_cast<const byte *>(bytes);imported.assign(first,first+length);
 const int handle=100+registrations;owners.emplace(handle,owner);return handle;
}
static qboolean Update(qhandle_t handle,std::uint64_t owner,const float *bones,int count,const vec3_t mins,const vec3_t maxs){
 assert(owners.at(handle)==owner&&count==3);++updates;
 matrices.assign(bones,bones+12*count);std::copy(mins,mins+3,capturedMins.begin());std::copy(maxs,maxs+3,capturedMaxs.begin());
 return updateFailure||backendPending?qfalse:qtrue;
}
static void ClearBackend(){++clearCalls;submitted.clear();backendPending=false;}
static void RenderBackend(const refdef_t *view){assert(view&&view->time==99999);++renders;backendPending=false;}
static void CL_AddEnemyHighlightRefEntity(const refEntity_t *entity,qboolean integerTime){
 assert(!integerTime);++draws;submitted.push_back(*entity);backendPending=true;
}
#include "code/client/client_jka_cgame_render.hpp"
static bool Dispatch(intptr_t *args,intptr_t &operation,intptr_t &result){
 using namespace fntech3::jka;using module::Call;result=0;
 const auto call=module::Lookup(COM_GAME_JKA_MP,module::Module::CGAME,module::Direction::IMPORT,args[0]);
 switch(call){
'''+host[start:end]+r'''
 default:assert(false);return false;}}
static bool hiddenHud;static int cl_enemyHighlightNumPending;
static cvar_t captureActive,hideWeapon;
static cvar_t *cl_captureActive=&captureActive,*r_levelshotHideViewWeapon=&hideWeapon;
static int hudAdjusts,liquidUpdates,underwaterUpdates,highlightFlushes;
static bool isAcademy=true;
int Com_GameIsJKAMP(){return isAcademy;}
static qboolean CL_CaptureHidesHud(){return hiddenHud?qtrue:qfalse;}
static refdef_t CL_ModuleRefdef(vm_t *vm,intptr_t address){
 assert(vm==cgvm);refdef_t view{};
 assert(fntech3::jka::render::ReadView(CL_JKACgameSpan(address,sizeof(fntech3_jka_mp_refdef_t)),view)==fntech3::jka::render::Status::Ok);return view;
}
static void CL_HudAdjustRefdef(refdef_t *){++hudAdjusts;}
static void CL_HudFlushQueue(){}
static void CL_UpdateLocalViewLiquidInteraction(const refdef_t *){++liquidUpdates;}
static void CL_UpdateUnderwaterView(const refdef_t *){++underwaterUpdates;}
static void CL_FlushEnemyHighlightRefEntities(const refdef_t *){++highlightFlushes;}
static intptr_t CanonicalRender(intptr_t *args,intptr_t operation){switch(operation){
'''+shared[canonical_start:canonical_end]+r'''
 default:assert(false);return -1;}}
static void Queue(const fntech3_jka_mp_refEntity_t &record){
 std::array<byte,sizeof(record)+2> unaligned;unaligned.fill(0xa5);std::memcpy(unaligned.data()+1,&record,sizeof(record));
 const auto before=unaligned;intptr_t args[]{202,reinterpret_cast<intptr_t>(unaligned.data()+1)},operation=-1,result=-1;
 assert(Dispatch(args,operation,result)&&result==0&&unaligned==before);
}
static void ClearScene(){intptr_t args[]{200},operation=-1,result=-1;
 assert(!Dispatch(args,operation,result)&&result==0&&operation==CG_R_CLEARSCENE);re.ClearScene();}
static void Render(fntech3_jka_mp_refdef_t &view){
 intptr_t args[]{209,reinterpret_cast<intptr_t>(&view)},operation=-1,result=-1;
 assert(!Dispatch(args,operation,result)&&result==0&&operation==CG_R_RENDERSCENE);
 assert(CanonicalRender(args,operation)==0);
}
int main(){
 vm_t vm{};vm.index=VM_CGAME;vm.entryPoint=reinterpret_cast<decltype(vm.entryPoint)>(1);cgvm=&vm;
 if(sizeof(void*)!=4)return 0;
 for(intptr_t address:{intptr_t(0),static_cast<intptr_t>(UINTPTR_MAX-100)}){
  intptr_t args[]{202,address},operation=-1,result=-1;Reject([&]{Dispatch(args,operation,result);});
  assert(scene::state.pending.empty());
 }
 auto files=Installed();assets=files.files;
 assets["models/players/_humanoid/_humanoid.gla"]=PoseSkeleton();
 std::uint32_t identity=0;char filename[]="models/players/test/model.glm";
 intptr_t init[]{297,reinterpret_cast<intptr_t>(&identity),reinterpret_cast<intptr_t>(filename),0,0,0,0,0};
 assert(Invoke(init)==0&&identity);
 const std::string skinText="root,material/root\n";
 assets["models/players/test/visible.skin"]=Bytes(skinText.begin(),skinText.end());
 intptr_t skinArgs[]{55,reinterpret_cast<intptr_t>("models/players/test/visible.skin")};
 const auto skin=Invoke(skinArgs);assert(skin>0);
 intptr_t setSkin[]{298,static_cast<intptr_t>(identity),0,skin,skin};assert(Invoke(setSkin)==1);
 const auto bindingKey=std::make_pair(identity,0);
 assert(jkaCgameSkins->bindings.at(bindingKey).overrides.empty());
 intptr_t animate[]{303,static_cast<intptr_t>(identity),0,reinterpret_cast<intptr_t>("model_root"),0,3,0xc8,0,0,0,100};
 float speed=1,seed=-1;std::uint32_t word;std::memcpy(&word,&speed,4);animate[7]=word;std::memcpy(&word,&seed,4);animate[9]=word;
 assert(Invoke(animate)==1&&CL_JKACgameGhoul2Resolve(identity)->At(0)->animations[0].flags==0x48);
 fntech3_jka_mp_refEntity_t record{};record.ghoul2=identity;record.renderfx=0x82;
 for(int axis=0;axis<3;++axis)record.axis[axis][axis]=1;
 record.origin[0]=19;record.frame=77;record.oldframe=88;record.backlerp=.25f;record.shaderTime=2.5f;
 fntech3_jka_mp_refdef_t view{};view.width=640;view.height=480;view.fov_x=90;view.fov_y=75;view.time=99999;view.rdflags=0x10;
 const auto otherOwner=fntech3::jka::mdr::NextOwner();
 Queue(record);assert(draws==0&&registrations==0&&scene::state.pending.size()==1);
 record.origin[0]=999; // Only the copied record reaches the renderer.
 jkaCgameBoneClocks.BeginFrame(75,false);Render(view);
 assert(draws==1&&registrations==1&&renders==1&&submitted.size()==1&&scene::state.pending.empty());
 const auto captured=submitted[0];const auto firstOwner=owners.at(captured.hModel);
 assert(firstOwner>otherOwner&&captured.origin[0]==19&&captured.customSkin==skin);
 assert(captured.frame==0&&captured.oldframe==0&&captured.backlerp==0&&captured.shaderTime.f==2.5f);
 assert(captured.renderfx==(RF_THIRD_PERSON|RF_LIGHTING_ORIGIN));
 record.origin[0]=19;ClearScene();jkaCgameBoneClocks.BeginFrame(100,false);Queue(record);Queue(record);Render(view);
 assert(registrations==1&&updates==2&&draws==3&&renders==2);
 pose::Palette expected;const auto &model=*CL_JKACgameGhoul2Resolve(identity)->At(0);
 assert(CL_JKACgameGhoul2Store().DeformationPalette(model,100,expected));
 for(std::size_t n=0;n<matrices.size();++n)assert(matrices[n]==expected.deformation[n/12][n%12]);
 for(int axis=0;axis<3;++axis)assert(capturedMins[axis]<=capturedMaxs[axis]);
 assert(hudAdjusts==2&&liquidUpdates==2&&underwaterUpdates==2&&highlightFlushes==2);
 ClearScene();Queue(record);ClearScene();Render(view);assert(draws==3&&updates==2);
 hiddenHud=true;view.rdflags=1;Queue(record);Render(view);assert(draws==3&&updates==2&&renders==3&&scene::state.pending.empty());
 hiddenHud=false;view.rdflags=0x10;
 // Other profiles do not flush Academy records through this shared scene hook.
 isAcademy=false;Queue(record);Render(view);assert(draws==3&&scene::state.pending.size()==1);
 isAcademy=true;ClearScene();
 for(int kind=0;kind<8;++kind){auto bad=record;
  if(kind==0)bad.ghoul2=0x12345678;if(kind==1)bad.customSkin=1;if(kind==2)bad.skinNum=1;
  if(kind==3)bad.uRefEnt.miniCount=1;if(kind==4)bad.modelScale[1]=2;if(kind==5)bad.reType=2;
  if(kind==6){std::uint32_t nan=0x7fc00001;std::memcpy(&bad.angles[0],&nan,4);}
  if(kind==7)bad.renderfx|=0x1000;
  Reject([&]{Queue(bad);});assert(scene::state.pending.empty()&&draws==3);
 }
 // Academy's tint and forced alpha (0x800, 0x400) are served as their own
 // entity fields (jka_entity_override_tests.py), so the instance queues.
 {auto faded=record;faded.renderfx|=0xc00;Queue(faded);
  assert(scene::state.pending.size()==1&&scene::state.pending[0].renderfx==(0x82|0xc00)&&draws==3);ClearScene();}
 vm.index=VM_UI;Reject([&]{Queue(record);});vm.index=VM_CGAME;
 vm.entryPoint=nullptr;Reject([&]{Queue(record);});vm.entryPoint=reinterpret_cast<decltype(vm.entryPoint)>(1);
 vm.dllExports=reinterpret_cast<decltype(vm.dllExports)>(1);Reject([&]{Queue(record);});vm.dllExports=nullptr;
 Queue(record);jkaCgameBoneClocks.Reset();Reject([&]{Render(view);});ClearScene();
 Queue(record);jkaCgameBoneClocks.BeginFrame(100,true);Reject([&]{Render(view);});ClearScene();
 jkaCgameBoneClocks.BeginFrame(100,false);
 // Visibility changes are consumed; all-off means no generated surface/draw.
 const std::string offText="root,*off\n";assets["models/players/test/off.skin"]=Bytes(offText.begin(),offText.end());
 skinArgs[1]=reinterpret_cast<intptr_t>("models/players/test/off.skin");const auto offSkin=Invoke(skinArgs);
 setSkin[4]=offSkin;assert(Invoke(setSkin)==1);Queue(record);Render(view);assert(draws==3&&registrations==1);
 setSkin[4]=skin;assert(Invoke(setSkin)==1);Queue(record);Render(view);assert(draws==4&&registrations==2);
 assert(owners.at(submitted.back().hModel)>firstOwner);ClearScene();
 // Unknown topology flags and stale skin declarations refuse before drawing.
 scene::state.models.at({identity,0}).geometry.surfaces[0].metadata.flags=0x80000000;
 Queue(record);Reject([&]{Render(view);});ClearScene();scene::state.models.at({identity,0}).geometry.surfaces[0].metadata.flags=0;
 jkaCgameSkins->bindings[bindingKey].overrides[32]=0;Queue(record);Reject([&]{Render(view);});ClearScene();
 jkaCgameSkins->bindings[bindingKey].overrides.erase(32);
 const auto renderSkin=jkaCgameSkins->bindings[bindingKey].renderSkin;jkaCgameSkins->bindings[bindingKey].renderSkin=999;
 Queue(record);Reject([&]{Render(view);});ClearScene();jkaCgameSkins->bindings[bindingKey].renderSkin=renderSkin;
 // A measured active blend reaches the real Fn skeletal update/draw path.
 animate[8]=150;assert(Invoke(animate)==1);jkaCgameBoneClocks.BeginFrame(175,false);
 Queue(record);Render(view);ClearScene();assert(draws==5);
 assert(pose::Finite(matrices.at(7))&&std::fabs(matrices.at(7)-1.625f)<.00005f);
 // Unknown animation controls still refuse before any renderer publication.
 animate[6]=0x108;assert(Invoke(animate)==1);Queue(record);Reject([&]{Render(view);});ClearScene();assert(draws==5);
 jkaCgameBoneClocks.BeginFrame(100,false);animate[6]=0x48;assert(Invoke(animate)==1);
 updateFailure=true;Queue(record);Reject([&]{Render(view);});assert(draws==5);updateFailure=false;ClearScene();
 scene::state.models.clear();registerFailure=true;Queue(record);Reject([&]{Render(view);});assert(draws==5);registerFailure=false;ClearScene();
 Queue(record);Render(view);assert(draws==6&&registrations==4);ClearScene();
 const auto nonce=fntech3::jka::mdr::ownerCounter;fntech3::jka::mdr::ownerCounter=UINT64_MAX;
 scene::state.models.clear();Queue(record);Reject([&]{Render(view);});assert(draws==6);ClearScene();fntech3::jka::mdr::ownerCounter=nonce;
 // Native error unwinding releases pending/working/cache state in real clear.
 Queue(record);registerFailure=true;jumpRecovery=true;
 if(!setjmp(recovery)){Render(view);assert(false);}
 jumpRecovery=false;registerFailure=false;
 assert(!jkaCgameGhoul2Store&&!jkaCgameSkins&&scene::state.models.empty()&&scene::state.pending.empty());
 assert(scene::state.bytes.empty()&&scene::state.palettes.empty()&&!jkaCgameBoneClocks.FrameReady());
 // A real two-surface hierarchy exercises default flags, overrides and
 // descendant suppression through the same owned registration/flush path.
 auto weighted=GeometryFixture();model::Reader reader(weighted.data(),weighted.size());
 const auto rootFlags=164+static_cast<std::size_t>(reader.Int(164))+64;
 const auto childFlags=164+static_cast<std::size_t>(reader.Int(168))+64;
 PutInt(weighted,rootFlags,0);PutInt(weighted,childFlags,0);
 assets["models/players/weighted/model.glm"]=weighted;
 assets["models/players/weighted/pose.gla"]=PoseSkeleton();
 identity=0;init[2]=reinterpret_cast<intptr_t>("models/players/weighted/model.glm");
 assert(Invoke(init)==0&&identity);record.ghoul2=identity;
 const std::string allText="root,material/root\nchild,material/child\n";
 assets["models/players/weighted/all.skin"]=Bytes(allText.begin(),allText.end());
 skinArgs[1]=reinterpret_cast<intptr_t>("models/players/weighted/all.skin");const auto allSkin=Invoke(skinArgs);
 setSkin[1]=identity;setSkin[3]=setSkin[4]=allSkin;assert(Invoke(setSkin)==1);
 jkaCgameBoneClocks.BeginFrame(100,false);Queue(record);Render(view);ClearScene();
 auto &runtime=scene::state.models.at({identity,0});assert(runtime.visible==std::vector<int>({0,1}));
 auto &visibility=jkaCgameSkins->bindings.at({identity,0}).overrides;
 for(std::uint32_t flags:{1u,2u,0x102u}){
  visibility[0]=flags;Queue(record);Render(view);ClearScene();
  assert(runtime.visible==(flags==0x102?std::vector<int>{}:std::vector<int>{1}));
 }
 visibility[0]=0;visibility[1]=1;Queue(record);Render(view);ClearScene();assert(runtime.visible==std::vector<int>{0});
 visibility.clear();
 auto *owned=const_cast<fntech3::ghoul2::Model *>(CL_JKACgameGhoul2Resolve(identity)->At(0));
 const auto skeleton=owned->skeletonBytes;owned->skeletonBytes=nullptr;Reject([&]{Queue(record);});owned->skeletonBytes=skeleton;
 // A model linked to its own bolt is never reached by retail's sort: no draw.
 owned->boltInfo=1;{const int before=draws;Queue(record);Render(view);ClearScene();assert(draws==before);}
 owned->boltInfo=fntech3::ghoul2::link::None;
 const auto customSkin=owned->customSkin;owned->customSkin=999;Reject([&]{Queue(record);});owned->customSkin=customSkin;
 // Cleanup after queueing invalidates the copied identity before any draw.
 Queue(record);const int beforeStale=draws;
 CL_JKACgameGhoul2Store().Clear();Reject([&]{Render(view);});assert(draws==beforeStale);ClearScene();
 identity=0;assert(Invoke(init)==0&&identity);record.ghoul2=identity;
 // Two unlinked models are both drawn from the root basis, one Fn entity each.
 assert(Invoke(init)==1&&CL_JKACgameGhoul2Resolve(identity)->Count()==2);
 {const int before=draws;Queue(record);Render(view);assert(draws==before+2);
  assert(submitted.size()==2&&submitted[0].hModel!=submitted[1].hModel);ClearScene();}
 CL_JKACgameClear();
 assert(opens==closes);
}
'''


class JKACgameRenderTests(unittest.TestCase):
    def test_owned_scene_skin_pose_and_real_canonical_hooks(self):
        compiler=shutil.which('clang++') or shutil.which('g++')
        if not compiler:self.skipTest('C++ compiler unavailable')
        scratch=ROOT/'.tmp/tests/jka-cgame-render';scratch.mkdir(parents=True,exist_ok=True)
        with tempfile.TemporaryDirectory(dir=scratch) as folder:
            directory=Path(folder);source=directory/'host.cpp';source.write_text(fixture())
            for fast in (False,True):
                with self.subTest(fast_math=fast):
                    executable=directory/(('fast' if fast else 'plain')+('.exe' if os.name=='nt' else ''))
                    built=subprocess.run([compiler,*compiler_target_args(compiler),'-std=c++17','-O2','-Werror',
                        '-D_CRT_SECURE_NO_WARNINGS',*(['-ffast-math'] if fast else []),'-I',str(ROOT),str(source),'-o',str(executable)],
                        cwd=ROOT,capture_output=True,text=True,timeout=120)
                    self.assertEqual(built.returncode,0,built.stdout+built.stderr)
                    run=subprocess.run([str(executable)],cwd=ROOT,capture_output=True,text=True,timeout=30)
                    self.assertEqual(run.returncode,0,run.stdout+run.stderr)


if __name__=='__main__':unittest.main()
