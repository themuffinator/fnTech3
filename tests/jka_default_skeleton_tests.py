"""Measured engine-default skeleton through real cgame ownership/Fn geometry."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import engine_fixture_args
from jka_cgame_render_tests import fixture as scene_fixture

ROOT=Path(__file__).resolve().parents[1]


def fixture():
    prefix=scene_fixture().split('int main(){')[0]
    assert prefix.count('view.numBones==3')==1 and prefix.count('count==3')==1
    prefix=prefix.replace('view.numBones==3','view.numBones==1').replace('count==3','count==1')
    return prefix+r'''
// x87 fast-math quaternion reconstruction can leave ~1.5e-8 in zero cells.
static bool NearMatrix(const float *actual,const pose::Matrix &expected){
 for(int n=0;n<12;++n)if(std::fabs(actual[n]-expected[n])>1e-7f)return false;
 return true;
}
static Bytes OwnDefaultMesh(){
 auto bytes=Mesh("*default",1);model::Mesh parsed;model::MeshSurface surface;
 assert(model::ParseMesh(bytes.data(),bytes.size(),1,parsed)==model::Error::None);
 assert(model::SurfaceAt(bytes.data(),bytes.size(),parsed,0,0,surface));
 const float positions[3][3]{{2,3,5},{4,-3,9},{2,7,5}};
 for(int v=0;v<3;++v)for(int n=0;n<3;++n)GeometryFloat(bytes,surface.vertexOffset+v*32+12+n*4,positions[v][n]);
 return bytes;
}
int main(){
 // Opt-in is owned by the consumer. Legacy stores keep their star metadata;
 // the enabled store parses generated data without reading a star filename.
 TestFiles files;files.files["models/own/static.glm"]=OwnDefaultMesh();
 ghoul2::Store legacy(files);void *legacyID=nullptr;LoadError error;
 assert(legacy.AddModel(&legacyID,"models/own/static.glm",error)==0);
 const auto *metadata=legacy.Resolve(legacyID)->At(0);assert(!metadata->skeletonBytes&&!metadata->meshBytes&&!metadata->skeleton.numFrames);
 ghoul2::Store enabled(files,true);void *enabledID=nullptr;
 assert(enabled.AddModel(&enabledID,"models/own/static.glm",error)==0);
 const auto *owned=enabled.Resolve(enabledID)->At(0);
 assert(owned->meshBytes&&owned->skeletonBytes&&owned->skeleton.numFrames==1&&owned->skeleton.numBones==1);
 assert(owned->skeletonName=="*default"&&owned->surfaceNames==std::vector<std::string>{"root"});
 assert(owned->boneNames==std::vector<std::string>{"ModView internal default"});
 assert(files.asked==std::set<std::string>{"models/own/static.glm"});
 pose::Palette palette;assert(enabled.DeformationPalette(*owned,0,palette));
 const pose::Matrix basis{0,-1,0,0,1,0,0,0,0,0,1,0};
 assert(palette.deformation.size()==1&&palette.bones.size()==1);
 assert(NearMatrix(palette.deformation[0].data(),basis)&&NearMatrix(palette.bones[0].data(),basis));
 std::vector<float> boneMatrices;assert(enabled.BoneMatrices(*owned,INT_MAX,boneMatrices));
 assert(boneMatrices.size()==12&&NearMatrix(boneMatrices.data(),basis));
 const auto bolt=enabled.AddBolt(enabledID,0,"ModView internal default");assert(bolt>=0);
 float output[12];assert(enabled.BoltMatrix(enabledID,0,bolt,123,output));
 assert(NearMatrix(output,basis));
 void *duplicate=nullptr;assert(enabled.Duplicate(enabledID,&duplicate));
 assert(enabled.Release(enabledID)&&enabled.DeformationPalette(*enabled.Resolve(duplicate)->At(0),100,palette));
 // A one-bone engine skeleton cannot authorize a contradictory GLM header.
 files.files["models/own/bad.glm"]=Mesh("*default",2);void *badID=nullptr;
 assert(enabled.AddModel(&badID,"models/own/bad.glm",error)==-1&&error==LoadError::Bones&&!badID);
 if(sizeof(void*)!=4)return 0;
 vm_t vm{};vm.index=VM_CGAME;vm.entryPoint=reinterpret_cast<decltype(vm.entryPoint)>(1);cgvm=&vm;
 assets=files.files;std::uint32_t identity=0;
 intptr_t init[]{297,reinterpret_cast<intptr_t>(&identity),reinterpret_cast<intptr_t>("models/own/static.glm"),0,0,0,0,0};
 assert(Invoke(init)==0&&identity&&opens==2&&closes==2&&openFiles.empty());
 char glaName[64]{};intptr_t getName[]{306,static_cast<intptr_t>(identity),0,reinterpret_cast<intptr_t>(glaName)};
 assert(Invoke(getName)==0&&!std::strcmp(glaName,"*default"));
 fntech3_jka_mp_refEntity_t record{};record.ghoul2=identity;for(int n=0;n<3;++n)record.axis[n][n]=1;
 fntech3_jka_mp_refdef_t view{};view.width=320;view.height=240;view.fov_x=90;view.fov_y=75;view.time=99999;
 jkaCgameBoneClocks.BeginFrame(100,false);Queue(record);Render(view);
 assert(draws==1&&registrations==1&&renders==1&&submitted.size()==1);
 fnMdrView_t mdr;assert(FnMdr_Open(imported.data(),imported.size(),2000,4000,&mdr)&&mdr.numBones==1);
 float actual[3][4];assert(FnMdr_Bone(&mdr,0,0,actual));
 assert(NearMatrix(&actual[0][0],basis));
 ClearScene();jkaCgameBoneClocks.BeginFrame(200,false);Queue(record);Render(view);
 assert(draws==2&&registrations==1&&updates==1&&matrices.size()==12&&NearMatrix(matrices.data(),basis));
 const float expectedMins[]{-7,2,5},expectedMaxs[]{3,4,9};
 for(int n=0;n<3;++n){assert(std::fabs(capturedMins[n]-expectedMins[n])<2e-6f);
  assert(std::fabs(capturedMaxs[n]-expectedMaxs[n])<2e-6f);}
 assert(hudAdjusts==2&&liquidUpdates==2&&underwaterUpdates==2&&highlightFlushes==2);
 ClearScene();const int before=draws;std::uint32_t badSlot=0xa5a5a5a5;
 init[1]=reinterpret_cast<intptr_t>(&badSlot);init[2]=reinterpret_cast<intptr_t>("models/own/bad.glm");
 Reject([&]{Invoke(init);});assert(badSlot==0xa5a5a5a5&&draws==before);
 // Backend failure unwinds the generated skeleton, retained mesh and queue.
 scene::state.models.clear();Queue(record);registerFailure=jumpRecovery=true;
 if(!setjmp(recovery)){Render(view);assert(false);}
 registerFailure=jumpRecovery=false;
 assert(!jkaCgameGhoul2Store&&scene::state.pending.empty()&&scene::state.models.empty()&&openFiles.empty()&&opens==closes);
}
'''


class JKADefaultSkeletonTests(unittest.TestCase):
    def test_generated_default_real_reader_native_store_fn_geometry_and_cleanup(self):
        compiler=shutil.which('clang++') or shutil.which('g++')
        if not compiler:self.skipTest('C++ compiler unavailable')
        scratch=ROOT/'.tmp/tests/jka-default-skeleton';scratch.mkdir(parents=True,exist_ok=True)
        with tempfile.TemporaryDirectory(dir=scratch) as folder:
            directory=Path(folder);source=directory/'host.cpp';source.write_text(fixture())
            for fast in (False,True):
                with self.subTest(fast_math=fast):
                    executable=directory/(('fast' if fast else 'plain')+('.exe' if os.name=='nt' else ''))
                    built=subprocess.run([compiler,*engine_fixture_args(compiler),'-std=c++17','-O2','-Werror',
                        '-D_CRT_SECURE_NO_WARNINGS',*(['-ffast-math'] if fast else []),'-I',str(ROOT),str(source),'-o',str(executable)],
                        cwd=ROOT,capture_output=True,text=True,timeout=120)
                    self.assertEqual(built.returncode,0,built.stdout+built.stderr)
                    run=subprocess.run([str(executable)],cwd=ROOT,capture_output=True,text=True,timeout=30)
                    self.assertEqual(run.returncode,0,run.stdout+run.stderr)


if __name__=='__main__':unittest.main()
