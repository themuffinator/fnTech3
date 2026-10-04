"""Real UI Ghoul2 ownership/metadata transfers, with authored model fixtures."""
from pathlib import Path
import os, shutil, subprocess, tempfile, unittest
from compiler_support import compiler_target_args
from rtcw_music_fixture import function
ROOT=Path(__file__).resolve().parents[1]


def fixture():
    models=(ROOT/'tests/raven_ghoul2_server_tests.cpp').read_text()
    models=models[:models.index('static void CheckAModelLoads()')]
    models=models.replace('#include "../code/ghoul2/ghoul2_server.hpp"','#include "code/ghoul2/ghoul2_server.hpp"')
    host=(ROOT/'code/client/client_jka_ui_host.hpp').read_text()
    vm=(ROOT/'code/qcommon/vm.c').read_text()
    return r'''
#include "code/qcommon/q_shared.h"
#include "code/qcommon/qcommon.h"
#include "code/qcommon/vm_local.h"
#include "code/compat/jka_module_abi.h"
#include "code/compat/jka_render_adapter.hpp"
#include <array>
#include <cassert>
#include <limits>
struct Drop{};void QDECL Com_Error(errorParm_t,const char *,...){throw Drop{};}
static vm_t *uivm;
''' + models + r'''
static std::map<std::string,Bytes> assets;
static std::map<int,std::string> openFiles;
static int opens,closes,reads,nextFile=1;static bool shortRead;
int FS_FOpenFileRead(const char *path,fileHandle_t *handle,qboolean unique){
 assert(!unique);++opens;*handle=0;
 auto found=assets.find(path);if(found==assets.end())return -1;
 *handle=nextFile++;openFiles[*handle]=path;
 if(std::strstr(path,"oversize"))return INT_MAX;
 if(std::strstr(path,"negative"))return -2;
 return static_cast<int>(found->second.size());
}
int FS_Read(void *output,int length,fileHandle_t handle){
 ++reads;assert(openFiles.count(handle)&&length>=0);
 const auto &data=assets[openFiles[handle]];assert(static_cast<std::size_t>(length)==data.size());
 if(shortRead)--length;std::memcpy(output,data.data(),length);return length;
}
void FS_FCloseFile(fileHandle_t handle){++closes;assert(openFiles.erase(handle)==1);}
''' + function(vm,'void *VM_ExplicitArgPtr(')+'\n'+function(vm,'void VM_CheckBounds(')+'\n'+function(host,'static void *CL_JKAUINativeSpan(')+r'''
#include "code/client/client_jka_ui_ghoul2.hpp"
static intptr_t Invoke(intptr_t *args){
 intptr_t result=-1;const auto call=fntech3::jka::module::Lookup(COM_GAME_JKA_MP,
  fntech3::jka::module::Module::UI,fntech3::jka::module::Direction::IMPORT,args[0]);
 assert(CL_JKAUIGhoul2Call(args,call,result));return result;
}
template<class F>static void Reject(F action){bool rejected=false;try{action();}catch(Drop){rejected=true;}assert(rejected);}
static intptr_t FloatWord(float value){std::uint32_t bits;std::memcpy(&bits,&value,4);return static_cast<intptr_t>(bits);}
int main(){
 vm_t vm{};vm.entryPoint=reinterpret_cast<decltype(vm.entryPoint)>(1);uivm=&vm;
 auto files=Installed();assets=files.files;
 std::array<byte,11> slot;slot.fill(0xa5);std::memset(slot.data()+3,0,4);
 std::array<char,MAX_QPATH> filename{};std::strcpy(filename.data(),"models/players/test/model.glm");
 intptr_t init[]{208,reinterpret_cast<intptr_t>(slot.data()+3),reinterpret_cast<intptr_t>(filename.data()),0,0,0,0,0};
 if(sizeof(void *)!=4){Reject([&]{Invoke(init);});assert(opens==0&&!jkaUIGhoul2Store);return 0;}
 assert(Invoke(init)==0);std::uint32_t identity=0;std::memcpy(&identity,slot.data()+3,4);
 assert(identity&&slot[2]==0xa5&&slot[7]==0xa5&&openFiles.empty()&&opens==closes);
 intptr_t getTime[]{227,987654};intptr_t setTime[]{228,2000,0};assert(Invoke(getTime)==0&&Invoke(setTime)==0&&Invoke(getTime)==2000);
 setTime[1]=2199;setTime[2]=1;assert(Invoke(setTime)==0&&Invoke(getTime)==2199);
 setTime[1]=2201;assert(Invoke(setTime)==0&&Invoke(getTime)==2000);
 setTime[2]=2;Reject([&]{Invoke(setTime);});assert(Invoke(getTime)==2000);
 setTime[2]=0;setTime[1]=INT_MAX-199;assert(Invoke(setTime)==0);setTime[2]=1;setTime[1]=INT_MIN;
 assert(Invoke(setTime)==0&&Invoke(getTime)==INT_MIN); // x86 cutoff wraps to INT_MIN.
 setTime[2]=0;setTime[1]=0;Invoke(setTime);setTime[2]=1;Invoke(setTime);assert(Invoke(getTime)==0);
 intptr_t have[]{204,static_cast<intptr_t>(identity)};intptr_t has[]{220,reinterpret_cast<intptr_t>(slot.data()+3),0};assert(Invoke(have)==1&&Invoke(has)==1);
 // PLAYANIM transfers ten retail words into owned timing metadata. Its float
 // seed retains fractions and the -1 sentinel under the fast-math test build.
 std::array<char,MAX_QPATH> bone{};std::strcpy(bone.data(),"model_root");
 intptr_t animate[]{213,static_cast<intptr_t>(identity),0,reinterpret_cast<intptr_t>(bone.data()),0,1,8,FloatWord(2),200,FloatWord(.5f),0};
 assert(Invoke(animate)==1);
 const auto playing=[&](){return CL_JKAUIGhoul2Store().BoneAnimation(reinterpret_cast<void*>(static_cast<std::uintptr_t>(identity)),0,"model_root");};
 assert(playing()&&playing()->startFrame==0&&playing()->endFrame==1&&playing()->flags==8&&playing()->speed==2&&playing()->setFrame==.5f&&playing()->startTime==187);
 animate[9]=FloatWord(-1);assert(Invoke(animate)==1&&playing()->startTime==200);
 const auto savedAnimation=*playing();
 for(int argument:{7,9}){const auto prior=animate[argument];animate[argument]=0x7fc00001u;assert(Invoke(animate)==0);animate[argument]=prior;assert(playing()->startTime==savedAnimation.startTime&&playing()->setFrame==savedAnimation.setFrame);}
 animate[2]=-1;assert(Invoke(animate)==0);animate[2]=0;
 std::strcpy(bone.data(),"missing");assert(Invoke(animate)==0);
 bone.fill('x');Reject([&]{Invoke(animate);});assert(playing()->startTime==savedAnimation.startTime);
 std::strcpy(bone.data(),"model_root");const auto boneAddress=animate[3];animate[3]=0;Reject([&]{Invoke(animate);});animate[3]=boneAddress;
 animate[1]=0;animate[3]=0;assert(Invoke(animate)==0);animate[1]=identity;animate[3]=boneAddress;
 assert(playing()->startTime==savedAnimation.startTime&&opens==closes);
 // ANGLEOVERRIDE copies an unaligned frozen vector into owned metadata.
 std::array<byte,16> angleBytes;angleBytes.fill(0xa5);const float bent[]{10,20,30};std::memcpy(angleBytes.data()+1,bent,12);
 intptr_t angle[]{212,static_cast<intptr_t>(identity),0,reinterpret_cast<intptr_t>(bone.data()),reinterpret_cast<intptr_t>(angleBytes.data()+1),2,1,3,2,0,0,200};
 assert(Invoke(angle)==1&&angleBytes[0]==0xa5&&angleBytes[13]==0xa5);
 const auto angled=[&](){return CL_JKAUIGhoul2Store().BoneAngles(reinterpret_cast<void*>(static_cast<std::uintptr_t>(identity)),0,"model_root");};
 assert(angled()&&angled()->angles[0]==10&&angled()->angles[1]==20&&angled()->angles[2]==30&&angled()->flags==2&&angled()->up==1&&angled()->right==3&&angled()->forward==2&&angled()->startTime==200);
 const std::uint32_t nan=0x7fc00001;std::memcpy(angleBytes.data()+1,&nan,4);assert(Invoke(angle)==0&&angled()->angles[0]==10);std::memcpy(angleBytes.data()+1,bent,12);
 angle[9]=1;Reject([&]{Invoke(angle);});angle[9]=0;
 const auto angleAddress=angle[4];angle[4]=0;Reject([&]{Invoke(angle);});angle[4]=angleAddress;
 angle[1]=0;angle[3]=0;angle[4]=0;assert(Invoke(angle)==0);angle[1]=identity;angle[3]=reinterpret_cast<intptr_t>(bone.data());angle[4]=angleAddress;
 has[2]=1;assert(Invoke(has)==0);has[2]=-1;assert(Invoke(has)==0);
 std::array<byte,72> output;output.fill(0xa5);
 intptr_t gla[]{216,static_cast<intptr_t>(identity),0,reinterpret_cast<intptr_t>(output.data()+3)};assert(Invoke(gla)==0);
 assert(!std::strcmp(reinterpret_cast<char *>(output.data()+3),"models/players/_humanoid/_humanoid"));
 assert(output[2]==0xa5&&output[67]==0xa5);
 intptr_t surface[]{233,static_cast<intptr_t>(identity),0,0,reinterpret_cast<intptr_t>(output.data()+3)};assert(Invoke(surface)==0);
 assert(!std::strcmp(reinterpret_cast<char *>(output.data()+3),"root"));
 surface[2]=-1;assert(Invoke(surface)==0&&output[3]==0);surface[2]=0;
 const auto savedSlot=slot;const int oldOpens=opens;
 for(int argument=3;argument<8;++argument){init[argument]=1;Reject([&]{Invoke(init);});init[argument]=0;}
 assert(slot==savedSlot&&opens==oldOpens);
 filename.fill('x');Reject([&]{Invoke(init);});assert(slot==savedSlot);
 std::strcpy(filename.data(),"models/players/test/missing.glm");assert(Invoke(init)==-1&&slot==savedSlot);
 assets[filename.data()]={1,2,3};assert(Invoke(init)==-1&&slot==savedSlot);
 std::strcpy(filename.data(),"models/players/test/model.glm");shortRead=true;assert(Invoke(init)==-1&&slot==savedSlot);shortRead=false;
 std::strcpy(filename.data(),"../outside.glm");assert(Invoke(init)==-1&&slot==savedSlot);
 for(const char *bad:{"/outside.glm","C:/outside.glm","models/../outside.glm","models//outside.glm"}){
  std::strcpy(filename.data(),bad);assert(Invoke(init)==-1&&slot==savedSlot);
 }
 const int oldReads=reads;for(const char *bad:{"models/oversize.glm","models/negative.glm"}){
  assets[bad]={1};std::strcpy(filename.data(),bad);assert(Invoke(init)==-1&&slot==savedSlot);
 }assert(reads==oldReads);
 assert(opens==closes+1&&openFiles.empty()); // The missing path open had no handle.
 // Inline default-skeleton meshes return their real surface names too.
 assets["models/players/test/inline.glm"]=Mesh("*default",3);
 std::strcpy(filename.data(),"models/players/test/inline.glm");assert(Invoke(init)==1);
 surface[3]=1;assert(Invoke(surface)==0&&!std::strcmp(reinterpret_cast<char *>(output.data()+3),"root"));
 gla[2]=1;assert(Invoke(gla)==0&&!std::strcmp(reinterpret_cast<char *>(output.data()+3),"*default"));
 for(int index=2;index<ghoul2::MaxModelsPerInstance;++index)assert(Invoke(init)==index);
 const auto fullSlot=slot;assert(Invoke(init)==-1&&slot==fullSlot);
 // Scene handoff continues to refuse a real owned nonnull identity.
 fntech3_jka_mp_refEntity_t record{};record.ghoul2=identity;refEntity_t canonical{};
 assert(fntech3::jka::render::Entity(record,canonical)==fntech3::jka::render::Status::Ghoul2);
 intptr_t clean[]{211,reinterpret_cast<intptr_t>(slot.data()+3)};assert(Invoke(clean)==0);
 assert(jkaUIGhoul2Store->InstanceCount()==0&&slot[3]==0&&slot[4]==0&&slot[5]==0&&slot[6]==0&&slot[2]==0xa5&&slot[7]==0xa5);
 assert(Invoke(clean)==0);Reject([&]{Invoke(have);});
 std::memcpy(slot.data()+3,&identity,4);const auto stale=slot;Reject([&]{Invoke(init);});Reject([&]{Invoke(clean);});assert(slot==stale);
 std::memset(slot.data()+3,0,4);std::strcpy(filename.data(),"models/players/test/model.glm");assert(Invoke(init)==0);
 std::uint32_t replacement=0;std::memcpy(&replacement,slot.data()+3,4);assert(replacement!=identity);
 CL_JKAUIGhoul2Clear();assert(!jkaUIGhoul2Store);have[1]=replacement;Reject([&]{Invoke(have);});
 ghoul2::Store server(files);void *foreign=nullptr;ghoul2::LoadError error;
 assert(server.AddModel(&foreign,"models/players/test/model.glm",error)==0);
 const auto foreignIdentity=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(foreign));
 std::memcpy(slot.data()+3,&foreignIdentity,4);Reject([&]{Invoke(init);});Reject([&]{Invoke(clean);});
 // Null query semantics and bad pointers are checked without following handles.
 have[1]=0;assert(Invoke(have)==0);has[1]=0;Reject([&]{Invoke(has);});
 std::memset(slot.data()+3,0,4);has[1]=reinterpret_cast<intptr_t>(slot.data()+3);assert(Invoke(has)==0);
 gla[1]=0;output.fill(0xa5);assert(Invoke(gla)==0&&output[3]==0xa5);
 const auto untouched=output;gla[1]=foreignIdentity;Reject([&]{Invoke(gla);});assert(output==untouched);gla[1]=0;
 surface[1]=0;output.fill(0xa5);const auto prior=output;assert(Invoke(surface)==0&&output==prior);
 init[1]=0;Reject([&]{Invoke(init);});init[1]=-1;Reject([&]{Invoke(init);});init[1]=reinterpret_cast<intptr_t>(slot.data()+3);
 init[2]=-1;Reject([&]{Invoke(init);});init[2]=reinterpret_cast<intptr_t>(filename.data());
 vm.entryPoint=nullptr;Reject([&]{Invoke(init);});Reject([&]{Invoke(have);});
 vm.entryPoint=reinterpret_cast<decltype(vm.entryPoint)>(1);vm.dllExports=reinterpret_cast<void *>(1);Reject([&]{Invoke(clean);});
 vm.dllExports=nullptr;CL_JKAUIGhoul2Clear();assert(openFiles.empty());
}
'''


class JKAUIGhoul2Tests(unittest.TestCase):
    def test_actual_ownership_metadata_bounds_refusals_and_cleanup(self):
        compiler=shutil.which('clang++') or shutil.which('g++')
        if not compiler:self.skipTest('C++ compiler unavailable')
        scratch=ROOT/'.tmp/tests/jka-ui-ghoul2';scratch.mkdir(parents=True,exist_ok=True)
        with tempfile.TemporaryDirectory(dir=scratch) as folder:
            directory=Path(folder);source=directory/'host.cpp';executable=directory/('host.exe' if os.name=='nt' else 'host')
            source.write_text(fixture())
            built=subprocess.run([compiler,*compiler_target_args(compiler),'-std=c++17','-O2','-ffast-math','-Werror','-D_CRT_SECURE_NO_WARNINGS','-I',str(ROOT),str(source),'-o',str(executable)],cwd=ROOT,capture_output=True,text=True,timeout=120)
            self.assertEqual(built.returncode,0,built.stdout+built.stderr)
            run=subprocess.run([str(executable)],cwd=ROOT,capture_output=True,text=True,timeout=30)
            self.assertEqual(run.returncode,0,run.stdout+run.stderr)

if __name__=='__main__':unittest.main()
