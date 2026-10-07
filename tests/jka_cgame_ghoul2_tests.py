"""Execute the cgame model boundary against authored GLM/GLA fixtures."""
from pathlib import Path
import os
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
from compiler_support import compiler_target_args
from rtcw_music_fixture import function

def fixture():
    models = (ROOT/'tests/raven_ghoul2_server_tests.cpp').read_text()
    models = models[:models.index('static void CheckAModelLoads()')]
    models = models.replace('#include "../code/ghoul2/ghoul2_server.hpp"',
                            '#include "code/ghoul2/ghoul2_server.hpp"')
    vm = (ROOT/'code/qcommon/vm.c').read_text()
    host = (ROOT/'code/client/client_jka_cgame_host.hpp').read_text()
    return r'''
#include "code/qcommon/q_shared.h"
#include "code/client/client_jka_command_values.hpp"
#include "code/compat/jka_lightstyle.hpp"
#include "code/qcommon/qcommon.h"
#include "code/qcommon/vm_local.h"
#include "code/compat/jka_module_abi.h"
#include <array>
#include <cassert>
#include <limits>
#include <csetjmp>
#include "code/compat/jka_skin.hpp"
static std::jmp_buf recovery;static bool jumpRecovery;
static void CL_JKACgameClear();
struct Drop {}; void QDECL Com_Error(errorParm_t,const char *,...){
 if(jumpRecovery){CL_JKACgameClear();std::longjmp(recovery,1);}throw Drop{};
}
static vm_t *cgvm;
''' + models + r'''
static std::map<std::string,Bytes> assets;
static std::map<int,std::string> openFiles;
static int opens,opened,closes,reads,nextFile=1;static bool shortRead,readDrop,closeDrop;
static int skinCalls,nextSkin=1;static bool skinDrop,skinZero;
static std::map<std::string,int> rendererSkins;
static qhandle_t RegisterSkin(const char *name){
 ++skinCalls;if(skinDrop)Com_Error(ERR_DROP,"skin backend failure");if(skinZero)return 0;
 std::string key=fntech3::jka::skin::Lower(name);
 if(key.size()>=5&&key.compare(key.size()-5,5,".skin")==0&&!assets.count(name))return 0;
 auto &id=rendererSkins[key];if(!id)id=nextSkin++;return id;
}
struct FixtureRenderer {qhandle_t (*RegisterSkin)(const char *);};
static FixtureRenderer re{RegisterSkin};
int FS_FOpenFileRead(const char *path,fileHandle_t *handle,qboolean unique){
 assert(!unique);++opens;*handle=0;
 auto found=assets.find(path);if(found==assets.end())return -1;
 *handle=nextFile++;openFiles[*handle]=path;++opened;
 if(std::strstr(path,"oversize"))return INT_MAX;
 if(std::strstr(path,"negative"))return -2;
 return static_cast<int>(found->second.size());
}
int FS_Read(void *output,int length,fileHandle_t handle){
 ++reads;assert(openFiles.count(handle)&&length>=0);
 if(readDrop)Com_Error(ERR_DROP,"file read failure");
 const auto &data=assets[openFiles[handle]];assert(static_cast<std::size_t>(length)==data.size());
 if(shortRead)--length;std::memcpy(output,data.data(),length);return length;
}
void FS_FCloseFile(fileHandle_t handle){++closes;assert(openFiles.erase(handle)==1);if(closeDrop)Com_Error(ERR_DROP,"file close failure");}
''' + function(vm,'void *VM_ExplicitArgPtr(')+'\n'+function(vm,'void VM_CheckBounds(')+'\n'+ \
        function(host,'static void *CL_JKACgameSpan(') + r'''
#include "code/client/client_jka_cgame_ghoul2.hpp"
static fntech3::jka::lightstyle::Store jkaCgameLightStyles;
static void *jkaCgameSharedBuffer;static std::vector<int> jkaFonts;
static int stringResets;static void CL_JKAStringReset(){++stringResets;}
static qboolean registrationQuiet;
void S_ClearJKAMusicPreload(){}
void S_JKASetSoundRegistrationQuiet(qboolean quiet){registrationQuiet=quiet;}
static void CL_JKACgameAmbientClear(){}
static void CL_JKACgameEffectsClear(){}
''' + function(host,'static void CL_JKACgameClear(') + r'''
static intptr_t Invoke(intptr_t *args){
 intptr_t result=-1;const auto call=fntech3::jka::module::Lookup(COM_GAME_JKA_MP,
  fntech3::jka::module::Module::CGAME,fntech3::jka::module::Direction::IMPORT,args[0]);
 assert(CL_JKACgameGhoul2Call(args,call,result));return result;
}
template<class F>static void Reject(F action){bool rejected=false;try{action();}catch(Drop){rejected=true;}assert(rejected);}
int main(){
 vm_t vm{};vm.entryPoint=reinterpret_cast<decltype(vm.entryPoint)>(1);cgvm=&vm;
 for(int ordinal:{339,340,341}){intptr_t cache[]{ordinal};assert(!Invoke(cache)&&!jkaCgameGhoul2Store);}
 auto files=Installed();assets=files.files;
 // The older metadata fixture's arbitrary compressed pool is sufficient for
 // format/ownership checks. Give this animation fixture an authored identity
 // quaternion and zero translation so it can exercise real Fn deformation.
 auto &authoredGLA=assets["models/players/_humanoid/_humanoid.gla"];
 fntech3::jka::model::Skeleton authoredSkeleton;
 assert(fntech3::jka::model::ParseSkeleton(authoredGLA.data(),authoredGLA.size(),authoredSkeleton)==fntech3::jka::model::Error::None);
 const std::uint16_t identityPose[]{49149,32766,32766,32766,32768,32768,32768};
 for(std::size_t n=0;n<7;++n){authoredGLA[authoredSkeleton.poseOffset+n*2]=static_cast<byte>(identityPose[n]);
  authoredGLA[authoredSkeleton.poseOffset+n*2+1]=static_cast<byte>(identityPose[n]>>8);}
 const fntech3::jka::model::Reader authoredReader(authoredGLA.data(),authoredGLA.size());
 for(int bone=0;bone<authoredSkeleton.numBones;++bone){
  const auto record=fntech3::jka::model::SkeletonHeaderBytes+static_cast<std::size_t>(authoredReader.Int(fntech3::jka::model::SkeletonHeaderBytes+bone*4));
  for(std::size_t n=0;n<24;++n)PutInt(authoredGLA,record+72+n*4,(n%12)%5?0:0x3f800000);
 }
 std::array<byte,11> slot;slot.fill(0xa5);std::memset(slot.data()+3,0,4);
 std::array<char,MAX_QPATH> filename{};std::memcpy(filename.data(),"models/players/test/model.glm",sizeof("models/players/test/model.glm"));
 intptr_t init[]{297,reinterpret_cast<intptr_t>(slot.data()+3),reinterpret_cast<intptr_t>(filename.data()),0,0,0,0,0};
 intptr_t have[]{293,0},has[]{310,reinterpret_cast<intptr_t>(slot.data()+3),0};
 intptr_t clean[]{301,reinterpret_cast<intptr_t>(slot.data()+3)};
 if(sizeof(void *)!=4){Reject([&]{Invoke(init);});assert(opens==0&&!jkaCgameGhoul2Store);return 0;}
 assert(!Invoke(have)&&!Invoke(has)&&!Invoke(clean)&&!jkaCgameGhoul2Store&&!opens);
 const auto initialSlot=slot;
 for(int argument:{3,5,6,7}){init[argument]=1;Reject([&]{Invoke(init);});assert(slot==initialSlot&&!opens);init[argument]=0;}
 for(int skin:{-1,INT_MIN}){init[4]=skin;Reject([&]{Invoke(init);});assert(slot==initialSlot&&!opens);}init[4]=0;
 init[2]=0;assert(Invoke(init)==-1&&slot==initialSlot&&!opens);init[2]=reinterpret_cast<intptr_t>(filename.data());
 filename[0]=0;assert(Invoke(init)==-1&&slot==initialSlot&&!opens);
 std::memcpy(filename.data(),"../models/players/test/model.glm",sizeof("../models/players/test/model.glm"));assert(Invoke(init)==-1&&!opens&&slot==initialSlot);
 std::memcpy(filename.data(),"models/players/test/model.glm",sizeof("models/players/test/model.glm"));
 vm.dllExports=reinterpret_cast<decltype(vm.dllExports)>(1);Reject([&]{Invoke(init);});vm.dllExports=nullptr;
 vm.entryPoint=nullptr;Reject([&]{Invoke(init);});vm.entryPoint=reinterpret_cast<decltype(vm.entryPoint)>(1);
 assert(Invoke(init)==0);std::uint32_t identity;std::memcpy(&identity,slot.data()+3,4);
 assert(identity&&slot[2]==0xa5&&slot[7]==0xa5&&openFiles.empty()&&opened==closes);
 have[1]=identity;assert(Invoke(have)==1&&Invoke(has)==1);
 assert(CL_JKACgameGhoul2Resolve(identity)->At(0)->customSkin==0);
 // A loaded model owns the exact nonnegative client skin word. Appending a
 // model binds that returned index, preserving earlier models' bindings.
 init[4]=1;assert(Invoke(init)==1);
 assert(CL_JKACgameGhoul2Resolve(identity)->At(0)->customSkin==0);
 assert(CL_JKACgameGhoul2Resolve(identity)->At(1)->customSkin==1);
 init[4]=INT_MAX;assert(Invoke(init)==2);
 assert(CL_JKACgameGhoul2Resolve(identity)->At(2)->customSkin==INT_MAX);
 assert(CL_JKACgameGhoul2Resolve(identity)->At(1)->customSkin==1);
 const auto beforeFailure=slot;const int beforeFailedOpens=opens;
 init[4]=-1;Reject([&]{Invoke(init);});assert(slot==beforeFailure&&opens==beforeFailedOpens);
 assert(CL_JKACgameGhoul2Resolve(identity)->Count()==3);
 init[4]=7;init[2]=reinterpret_cast<intptr_t>("models/missing.glm");
 assert(Invoke(init)==-1&&slot==beforeFailure&&CL_JKACgameGhoul2Resolve(identity)->Count()==3);
 assert(CL_JKACgameGhoul2Resolve(identity)->At(1)->customSkin==1&&CL_JKACgameGhoul2Resolve(identity)->At(2)->customSkin==INT_MAX);
 init[2]=reinterpret_cast<intptr_t>(filename.data());init[4]=0;
 has[2]=-1;assert(!Invoke(has));has[2]=3;assert(!Invoke(has));has[2]=0;
 // Register through the actual renderer service; the same file's ordered
 // visibility declarations belong to this cgame owner and no other Store.
 const char *skinName="models/players/test/model.skin";
 const std::string skinText="tag_head,\n ROOT,*off\n absent,material/ignored\n";
 assets[skinName]=Bytes(skinText.begin(),skinText.end());
 intptr_t registerSkin[]{55,reinterpret_cast<intptr_t>(skinName)};
 const auto skinId=Invoke(registerSkin);assert(skinId==1&&skinCalls==1);
 const int beforeCacheOpens=opens;assert(Invoke(registerSkin)==skinId&&opens==beforeCacheOpens&&skinCalls==2);
 intptr_t setSkin[]{298,static_cast<intptr_t>(identity),0,skinId,skinId};
 assert(Invoke(setSkin)==1&&CL_JKACgameGhoul2Resolve(identity)->At(0)->customSkin==skinId);
 const auto bindingKey=std::make_pair(identity,0);
 assert(jkaCgameSkins->bindings.at(bindingKey).renderSkin==skinId);
 assert(jkaCgameSkins->bindings.at(bindingKey).overrides.at(0)==2);
 setSkin[3]=42;setSkin[4]=0;assert(Invoke(setSkin)==1);
 assert(CL_JKACgameGhoul2Resolve(identity)->At(0)->customSkin==42);
 assert(jkaCgameSkins->bindings.at(bindingKey).overrides.at(0)==2); // renderSkin zero retains overrides
 setSkin[3]=-1;Reject([&]{Invoke(setSkin);});assert(CL_JKACgameGhoul2Resolve(identity)->At(0)->customSkin==42);
 setSkin[3]=7;setSkin[4]=999;Reject([&]{Invoke(setSkin);});assert(CL_JKACgameGhoul2Resolve(identity)->At(0)->customSkin==42);
 setSkin[4]=skinId;setSkin[2]=32;assert(!Invoke(setSkin));setSkin[2]=0;
 setSkin[1]=0;assert(!Invoke(setSkin));setSkin[1]=identity;
 const char *visibleName="models/players/test/visible.skin";
 const std::string visibleText="root, material/root\n";assets[visibleName]=Bytes(visibleText.begin(),visibleText.end());
 registerSkin[1]=reinterpret_cast<intptr_t>(visibleName);const auto visibleId=Invoke(registerSkin);assert(visibleId==2);
 setSkin[4]=visibleId;assert(Invoke(setSkin)==1&&jkaCgameSkins->bindings.at(bindingKey).overrides.empty());
 registerSkin[1]=reinterpret_cast<intptr_t>("models/missing.skin");assert(!Invoke(registerSkin));
 const auto definitions=jkaCgameSkins->definitions.size();skinZero=true;
 registerSkin[1]=reinterpret_cast<intptr_t>("material/plain");assert(!Invoke(registerSkin)&&jkaCgameSkins->definitions.size()==definitions);skinZero=false;
 registerSkin[1]=reinterpret_cast<intptr_t>("material/plain");const auto plainId=Invoke(registerSkin);assert(plainId==3);
 assert(jkaCgameSkins->definitions.at(plainId).surfaces.size()==1);
 registerSkin[1]=reinterpret_cast<intptr_t>("models/players/test/parts|head|torso");Reject([&]{Invoke(registerSkin);});
 registerSkin[1]=reinterpret_cast<intptr_t>("../escape.skin");Reject([&]{Invoke(registerSkin);});
 registerSkin[1]=0;assert(!Invoke(registerSkin));
 // Bone animation owns the actual float words, requested range and timing.
 const auto FloatWord=[](float value){std::uint32_t word;std::memcpy(&word,&value,4);return static_cast<intptr_t>(word);};
 intptr_t play[]{303,static_cast<intptr_t>(identity),0,reinterpret_cast<intptr_t>("MODEL_ROOT"),4,19,0x20,FloatWord(1.5f),1000,FloatWord(4.5f),200};
 assert(Invoke(play)==1);
 auto *animationHandle=reinterpret_cast<void *>(static_cast<std::uintptr_t>(identity));
 const auto *playing=CL_JKACgameGhoul2Store().BoneAnimation(animationHandle,0,"model_root");
 assert(playing&&playing->bone==0&&playing->startFrame==4&&playing->endFrame==19&&playing->flags==0x20);
 assert(playing->speed==1.5f&&playing->startTime==983&&playing->setFrame==4.5f&&playing->blendTime==200);
 assert(CL_JKACgameGhoul2Resolve(identity)->At(0)->animations.size()==1);
 play[7]=FloatWord(1.0f);play[9]=FloatWord(-1.0f);play[8]=1500;assert(Invoke(play)==1);
 playing=CL_JKACgameGhoul2Store().BoneAnimation(animationHandle,0,"model_root");
 assert(playing->startTime==1500&&playing->setFrame==-1.0f&&playing->speed==1.0f);
 const auto beforeBadAnimation=*playing;
 for(std::uint32_t word:{0x7f800000u,0xff800000u,0x7fc00001u}) {
  play[7]=static_cast<intptr_t>(word);assert(!Invoke(play));play[7]=FloatWord(1.0f);
  play[9]=static_cast<intptr_t>(word);assert(!Invoke(play));play[9]=FloatWord(-1.0f);
 }
 assert(!std::memcmp(&beforeBadAnimation,CL_JKACgameGhoul2Store().BoneAnimation(animationHandle,0,"model_root"),sizeof(beforeBadAnimation)));
 play[3]=reinterpret_cast<intptr_t>("absent");assert(!Invoke(play));play[3]=0;assert(!Invoke(play));
 play[3]=-1;Reject([&]{Invoke(play);});
 std::array<char,MAX_QPATH> longBone;longBone.fill('b');play[3]=reinterpret_cast<intptr_t>(longBone.data());Reject([&]{Invoke(play);});
 play[3]=reinterpret_cast<intptr_t>("model_root");play[2]=32;assert(!Invoke(play));play[2]=0;
 play[1]=0;assert(!Invoke(play));play[1]=identity;
 // Retail clears an incoming blend when the previous bone is inactive.
 play[4]=0;play[5]=1;play[6]=0xc8;play[8]=0;play[10]=100;assert(Invoke(play)==1);
 playing=CL_JKACgameGhoul2Store().BoneAnimation(animationHandle,0,"model_root");
 assert(playing->flags==0x48&&playing->blendTime==0);
 fntech3::ghoul2::pose::Palette initialPose;
 assert(CL_JKACgameGhoul2Store().DeformationPalette(*CL_JKACgameGhoul2Resolve(identity)->At(0),25,initialPose));
 // A missing prior clip on another bone is the same inactive case.
 play[3]=reinterpret_cast<intptr_t>("child");assert(Invoke(play)==1);
 const auto *childClip=CL_JKACgameGhoul2Store().BoneAnimation(animationHandle,0,"child");
 assert(childClip&&childClip->flags==0x48&&childClip->blendTime==0);
 // An actual active prior clip retains the frozen transition sample.
 play[3]=reinterpret_cast<intptr_t>("Model_Root");play[8]=50;assert(Invoke(play)==1);
 playing=CL_JKACgameGhoul2Store().BoneAnimation(animationHandle,0,"model_root");
 assert(playing->flags==0xc8&&playing->blendTime==100);
 assert(playing->previousKnown&&playing->blendStart==50);
 assert(playing==CL_JKACgameGhoul2Store().BoneAnimation(animationHandle,0,"MODEL_ROOT"));
 assert(CL_JKACgameGhoul2Resolve(identity)->At(0)->animations.size()==2);
 assert(CL_JKACgameGhoul2Store().DeformationPalette(*CL_JKACgameGhoul2Resolve(identity)->At(0),75,initialPose));
 // A valid loop record reaches the existing Fn Ghoul2 deformation evaluator.
 play[4]=0;play[5]=1;play[6]=0x18;play[8]=0;assert(Invoke(play)==1);
 fntech3::ghoul2::pose::Palette deformation;
 assert(CL_JKACgameGhoul2Store().DeformationPalette(*CL_JKACgameGhoul2Resolve(identity)->At(0),25,deformation));
 assert(deformation.deformation.size()==3);
 play[4]=-3;play[5]=100000;play[9]=FloatWord(-2.0f);play[8]=100;assert(Invoke(play)==1);
 playing=CL_JKACgameGhoul2Store().BoneAnimation(animationHandle,0,"model_root");
 assert(playing->startFrame==0&&playing->endFrame==1&&playing->setFrame==0.0f&&playing->startTime==100);
 play[4]=100000;play[5]=-1;play[9]=FloatWord(100001.0f);assert(Invoke(play)==1);
 assert(CL_JKACgameGhoul2Store().BoneAnimation(animationHandle,0,"model_root")->setFrame==0.0f);
 // ANGLEOVERRIDE owns an unaligned twelve-byte vector and measured scalar
 // fields. Retail's model-list word is forwarded but never consumed.
 std::array<byte,16> angleBytes;angleBytes.fill(0xa5);
 const float bends[]{10,20,30};std::memcpy(angleBytes.data()+1,bends,12);
 intptr_t angle[]{302,static_cast<intptr_t>(identity),0,reinterpret_cast<intptr_t>("MODEL_ROOT"),
  reinterpret_cast<intptr_t>(angleBytes.data()+1),2,1,3,2,0,70,200};
 assert(Invoke(angle)==1&&angleBytes.front()==0xa5&&angleBytes[13]==0xa5);
 const auto angled=[&](){return CL_JKACgameGhoul2Store().BoneAngles(animationHandle,0,"model_root");};
 assert(angled()&&angled()->bone==0&&angled()->angles[0]==10&&angled()->angles[1]==20&&angled()->angles[2]==30);
 assert(angled()->flags==2&&angled()->up==1&&angled()->right==3&&angled()->forward==2&&angled()->blendTime==70&&angled()->startTime==200);
 angle[3]=reinterpret_cast<intptr_t>("Model_Root");assert(Invoke(angle)==1);
 assert(angled()==CL_JKACgameGhoul2Store().BoneAngles(animationHandle,0,"MODEL_ROOT"));
 assert(CL_JKACgameGhoul2Resolve(identity)->At(0)->angles.size()==1);
 const auto beforeBadAngle=*angled();
 for(std::uint32_t word:{0x7f800000u,0xff800000u,0x7fc00001u}){
  std::memcpy(angleBytes.data()+1,&word,4);assert(!Invoke(angle));
  assert(!std::memcmp(&beforeBadAngle,angled(),sizeof(beforeBadAngle)));
 }
 std::memcpy(angleBytes.data()+1,bends,12);
 angle[9]=-1;assert(Invoke(angle)==1);angle[9]=0; // Unused list is never dereferenced.
 angle[4]=0;Reject([&]{Invoke(angle);});angle[4]=-1;Reject([&]{Invoke(angle);});
 angle[4]=reinterpret_cast<intptr_t>(angleBytes.data()+1);
 angle[3]=reinterpret_cast<intptr_t>("absent");assert(!Invoke(angle));angle[3]=0;assert(!Invoke(angle));
 angle[3]=-1;Reject([&]{Invoke(angle);});angle[3]=reinterpret_cast<intptr_t>(longBone.data());Reject([&]{Invoke(angle);});
 angle[3]=reinterpret_cast<intptr_t>("model_root");angle[2]=-1;assert(!Invoke(angle));angle[2]=32;assert(!Invoke(angle));angle[2]=0;
 angle[1]=0;angle[4]=0;assert(!Invoke(angle));angle[1]=identity;angle[4]=reinterpret_cast<intptr_t>(angleBytes.data()+1);
 // A supported angle mode changes the same Fn deformation matrices used by
 // skeletal rendering. Metadata for unsupported modes is not rendered.
 play[4]=0;play[5]=1;play[6]=0x18;play[8]=0;play[9]=FloatWord(-1.0f);play[10]=0;assert(Invoke(play)==1);
 angle[10]=0;assert(Invoke(angle)==1);
 fntech3::ghoul2::pose::Palette bentPalette;
 assert(CL_JKACgameGhoul2Store().DeformationPalette(*CL_JKACgameGhoul2Resolve(identity)->At(0),25,bentPalette));
 assert(bentPalette.deformation.size()==3&&std::memcmp(deformation.deformation[0].data(),
  bentPalette.deformation[0].data(),sizeof(deformation.deformation[0])));
 angle[5]=0x100;assert(Invoke(angle)==1);
 assert(!CL_JKACgameGhoul2Store().DeformationPalette(*CL_JKACgameGhoul2Resolve(identity)->At(0),25,bentPalette));
 angle[5]=2;assert(Invoke(angle)==1);
 // The void setter stores the attachment descriptor and always returns zero.
 // Invalid model indices leave the actual owned model untouched.
 intptr_t info[]{319,static_cast<intptr_t>(identity),0,0x1234};
 assert(!Invoke(info)&&CL_JKACgameGhoul2Resolve(identity)->At(0)->boltInfo==0x1234);
 for(int index:{-1,32}){info[2]=index;info[3]=57;assert(!Invoke(info));
  assert(CL_JKACgameGhoul2Resolve(identity)->At(0)->boltInfo==0x1234);}
 info[2]=0;info[3]=INT_MIN;assert(!Invoke(info));
 assert(CL_JKACgameGhoul2Resolve(identity)->At(0)->boltInfo==INT_MIN);
 info[1]=0;assert(!Invoke(info));info[1]=identity;
 // Bolt names resolve against this model's authored surface and bone tables.
 // Repetition keeps the same index; a missing name publishes no new bolt.
 intptr_t bolt[]{317,static_cast<intptr_t>(identity),0,reinterpret_cast<intptr_t>("ROOT")};
 assert(Invoke(bolt)==0&&Invoke(bolt)==0);
 auto *handle=reinterpret_cast<void *>(static_cast<std::uintptr_t>(identity));
 const auto *surface=CL_JKACgameGhoul2Store().BoltAt(handle,0,0);
 assert(surface&&surface->surface==0&&surface->bone==-1);
 bolt[3]=reinterpret_cast<intptr_t>("rOoT");assert(Invoke(bolt)==0);
 bolt[3]=reinterpret_cast<intptr_t>("MODEL_ROOT");assert(Invoke(bolt)==1&&Invoke(bolt)==1);
 bolt[3]=reinterpret_cast<intptr_t>("Model_Root");assert(Invoke(bolt)==1);
 const auto *bone=CL_JKACgameGhoul2Store().BoltAt(handle,0,1);
 assert(bone&&bone->surface==-1&&bone->bone==0);
 bolt[3]=reinterpret_cast<intptr_t>("absent");assert(Invoke(bolt)==-1);
 assert(CL_JKACgameGhoul2Resolve(identity)->At(0)->bolts.size()==2);
 bolt[3]=0;assert(Invoke(bolt)==-1);bolt[3]=-1;Reject([&]{Invoke(bolt);});
 std::array<char,MAX_QPATH> longBolt;longBolt.fill('x');bolt[3]=reinterpret_cast<intptr_t>(longBolt.data());
 Reject([&]{Invoke(bolt);});bolt[2]=-1;assert(Invoke(bolt)==-1);bolt[2]=0;
 bolt[1]=0;assert(Invoke(bolt)==-1);bolt[1]=identity;
 std::array<char,MAX_QPATH+4> output;output.fill(char(0x5a));
 intptr_t gla[]{306,static_cast<intptr_t>(identity),0,reinterpret_cast<intptr_t>(output.data()+1)};
 assert(!Invoke(gla)&&!std::strcmp(output.data()+1,"models/players/_humanoid/_humanoid"));
 assert(output.front()==char(0x5a)&&output.back()==char(0x5a));
 output.fill(char(0x5a));gla[2]=99;assert(!Invoke(gla));for(char value:output)assert(value==char(0x5a));
 gla[1]=0;gla[3]=0;assert(!Invoke(gla));gla[1]=identity;gla[2]=0;Reject([&]{Invoke(gla);});
 // Duplicate the actual model records and separately owned render-skin
 // visibility map. The output is an unaligned four-byte native identity.
 setSkin[1]=identity;setSkin[2]=0;setSkin[3]=46;setSkin[4]=skinId;assert(Invoke(setSkin)==1);
 std::array<byte,9> duplicateSlot;duplicateSlot.fill(0xa5);std::memset(duplicateSlot.data()+1,0,4);
 intptr_t duplicate[]{309,static_cast<intptr_t>(identity),reinterpret_cast<intptr_t>(duplicateSlot.data()+1)};
 const auto emptyDuplicateSlot=duplicateSlot;const auto beforeDuplicateCount=CL_JKACgameGhoul2Store().InstanceCount();
 for(intptr_t bad:{intptr_t(0),intptr_t(-1)}){
  duplicate[2]=bad;Reject([&]{Invoke(duplicate);});assert(duplicateSlot==emptyDuplicateSlot);
  assert(CL_JKACgameGhoul2Store().InstanceCount()==beforeDuplicateCount);
 }
 duplicate[2]=reinterpret_cast<intptr_t>(duplicateSlot.data()+1);duplicate[1]=0;
 assert(!Invoke(duplicate)&&duplicateSlot==emptyDuplicateSlot&&CL_JKACgameGhoul2Store().InstanceCount()==beforeDuplicateCount);
 duplicate[1]=identity;
 vm.dllExports=reinterpret_cast<decltype(vm.dllExports)>(1);Reject([&]{Invoke(duplicate);});vm.dllExports=nullptr;
 vm.entryPoint=nullptr;Reject([&]{Invoke(duplicate);});vm.entryPoint=reinterpret_cast<decltype(vm.entryPoint)>(1);
 assert(duplicateSlot==emptyDuplicateSlot&&CL_JKACgameGhoul2Store().InstanceCount()==beforeDuplicateCount);
 const int beforeDuplicateOpens=opens;assert(!Invoke(duplicate));
 std::uint32_t duplicateIdentity;std::memcpy(&duplicateIdentity,duplicateSlot.data()+1,4);
 assert(duplicateIdentity&&duplicateIdentity!=identity&&duplicateSlot[0]==0xa5&&duplicateSlot[5]==0xa5);
 assert(opens==beforeDuplicateOpens&&CL_JKACgameGhoul2Store().InstanceCount()==beforeDuplicateCount+1);
 auto *duplicateHandle=reinterpret_cast<void *>(static_cast<std::uintptr_t>(duplicateIdentity));
 auto *duplicateInstance=CL_JKACgameGhoul2Resolve(duplicateIdentity);assert(duplicateInstance->Count()==3);
 const auto *sourceModel=CL_JKACgameGhoul2Resolve(identity)->At(0);
 const auto *duplicateModel=duplicateInstance->At(0);
 assert(duplicateModel->customSkin==46&&duplicateModel->boltInfo==INT_MIN);
 assert(duplicateModel->meshBytes==sourceModel->meshBytes&&duplicateModel->skeletonBytes==sourceModel->skeletonBytes);
 assert(duplicateModel->surfaceNames==sourceModel->surfaceNames&&duplicateModel->surfaceNames.data()!=sourceModel->surfaceNames.data());
 assert(duplicateModel->bolts.size()==2&&duplicateModel->bolts.data()!=sourceModel->bolts.data());
 assert(duplicateModel->bolts[0].surface==0&&duplicateModel->bolts[1].bone==0);
 const auto duplicateBindingKey=std::make_pair(duplicateIdentity,0);
 assert(jkaCgameSkins->bindings.at(duplicateBindingKey).renderSkin==skinId);
 assert(jkaCgameSkins->bindings.at(duplicateBindingKey).overrides.at(0)==2);
 const auto copiedAnimation=*CL_JKACgameGhoul2Store().BoneAnimation(duplicateHandle,0,"model_root");
 const auto copiedAngles=*CL_JKACgameGhoul2Store().BoneAngles(duplicateHandle,0,"model_root");
 fntech3::ghoul2::pose::Palette copyPalette,sourcePalette;
 assert(CL_JKACgameGhoul2Store().DeformationPalette(*duplicateModel,25,copyPalette));
 assert(CL_JKACgameGhoul2Store().DeformationPalette(*sourceModel,25,sourcePalette));
 assert(copyPalette.deformation==sourcePalette.deformation);
 // Mutating either identity leaves the other's model/visibility/pose records
 // independent, while both may read the same immutable loaded GLM/GLA bytes.
 setSkin[3]=91;setSkin[4]=visibleId;assert(Invoke(setSkin)==1);
 assert(duplicateModel->customSkin==46&&jkaCgameSkins->bindings.at(duplicateBindingKey).overrides.at(0)==2);
 assert(jkaCgameSkins->bindings.at(bindingKey).overrides.empty());
 play[8]=400;assert(Invoke(play)==1);
 info[3]=57;assert(!Invoke(info)&&duplicateModel->boltInfo==INT_MIN);
 const float otherBends[]{45,0,0};std::memcpy(angleBytes.data()+1,otherBends,12);assert(Invoke(angle)==1);
 assert(!std::memcmp(&copiedAnimation,CL_JKACgameGhoul2Store().BoneAnimation(duplicateHandle,0,"model_root"),sizeof(copiedAnimation)));
 assert(!std::memcmp(&copiedAngles,CL_JKACgameGhoul2Store().BoneAngles(duplicateHandle,0,"model_root"),sizeof(copiedAngles)));
 intptr_t copyInfo[]{319,static_cast<intptr_t>(duplicateIdentity),0,777};assert(!Invoke(copyInfo));
 assert(CL_JKACgameGhoul2Resolve(identity)->At(0)->boltInfo==57);
 const auto populatedDuplicateSlot=duplicateSlot;
 for(std::uint32_t ignoredSource:{0u,identity,0xffffffffu}){
  duplicate[1]=ignoredSource;assert(!Invoke(duplicate)&&duplicateSlot==populatedDuplicateSlot);
  assert(CL_JKACgameGhoul2Store().InstanceCount()==beforeDuplicateCount+1&&duplicateModel->boltInfo==777);
 }
 duplicate[1]=identity;
 // Specific-model copy can replace or append within the same bounded owner.
 // Its arguments are identities, not native addresses of those identities.
 std::uint32_t modelCopyIdentity=0;
 const auto savedInitSlot=init[1];init[1]=reinterpret_cast<intptr_t>(&modelCopyIdentity);
 assert(Invoke(init)==0&&modelCopyIdentity);init[1]=savedInitSlot;
 intptr_t modelCopy[]{308,static_cast<intptr_t>(identity),0,static_cast<intptr_t>(modelCopyIdentity),0};
 const auto modelCopyHandle=reinterpret_cast<void *>(static_cast<std::uintptr_t>(modelCopyIdentity));
 const auto modelCopyKey=std::make_pair(modelCopyIdentity,0);
 assert(!Invoke(modelCopy));
 auto *copied=CL_JKACgameGhoul2Resolve(modelCopyIdentity)->At(0);
 assert(copied->customSkin==91&&copied->boltInfo==57&&copied->bolts.size()==2);
 assert(jkaCgameSkins->bindings.at(modelCopyKey).renderSkin==visibleId);
 assert(jkaCgameSkins->bindings.at(modelCopyKey).overrides.empty());
 const auto sourcePlaying=*CL_JKACgameGhoul2Store().BoneAnimation(animationHandle,0,"model_root");
 assert(!std::memcmp(&sourcePlaying,CL_JKACgameGhoul2Store().BoneAnimation(modelCopyHandle,0,"model_root"),sizeof(sourcePlaying)));
 // Replacing from a model without an override removes the old override;
 // copying a bound model restores its exact independent visibility table.
 modelCopy[2]=1;assert(!Invoke(modelCopy));
 assert(CL_JKACgameGhoul2Resolve(modelCopyIdentity)->At(0)->customSkin==1);
 assert(!jkaCgameSkins->bindings.count(modelCopyKey));
 modelCopy[1]=duplicateIdentity;modelCopy[2]=0;assert(!Invoke(modelCopy));
 assert(CL_JKACgameGhoul2Resolve(modelCopyIdentity)->At(0)->customSkin==46);
 assert(jkaCgameSkins->bindings.at(modelCopyKey).overrides.at(0)==2);
 // Same-instance append and self-copy preserve their data and copy skin
 // ownership without sharing a mutable map or reading an invalidated vector.
 modelCopy[1]=modelCopyIdentity;modelCopy[4]=1;assert(!Invoke(modelCopy));
 assert(CL_JKACgameGhoul2Resolve(modelCopyIdentity)->Count()==2);
 assert(jkaCgameSkins->bindings.at({modelCopyIdentity,1}).overrides.at(0)==2);
 modelCopy[4]=0;assert(!Invoke(modelCopy));
 intptr_t copiedSkin[]{298,static_cast<intptr_t>(modelCopyIdentity),0,99,visibleId};assert(Invoke(copiedSkin)==1);
 assert(CL_JKACgameGhoul2Resolve(duplicateIdentity)->At(0)->customSkin==46);
 assert(jkaCgameSkins->bindings.at(duplicateBindingKey).overrides.at(0)==2);
 assert(jkaCgameSkins->bindings.at({modelCopyIdentity,1}).overrides.at(0)==2);
 const auto beforeBadCopyCount=CL_JKACgameGhoul2Resolve(modelCopyIdentity)->Count();
 for(int index:{-1,32,INT_MAX}){
  modelCopy[2]=index;assert(!Invoke(modelCopy));modelCopy[2]=0;
  modelCopy[4]=index;assert(!Invoke(modelCopy));modelCopy[4]=0;
  assert(CL_JKACgameGhoul2Resolve(modelCopyIdentity)->Count()==beforeBadCopyCount);
 }
 modelCopy[4]=5;Reject([&]{Invoke(modelCopy);});modelCopy[4]=0;
 assert(CL_JKACgameGhoul2Resolve(modelCopyIdentity)->Count()==beforeBadCopyCount);
 modelCopy[1]=0;assert(!Invoke(modelCopy));modelCopy[1]=modelCopyIdentity;
 modelCopy[3]=0;assert(!Invoke(modelCopy));modelCopy[3]=modelCopyIdentity;
 vm.dllExports=reinterpret_cast<decltype(vm.dllExports)>(1);Reject([&]{Invoke(modelCopy);});vm.dllExports=nullptr;
 for(int index=2;index<32;++index){modelCopy[4]=index;assert(!Invoke(modelCopy));
  assert(CL_JKACgameGhoul2Resolve(modelCopyIdentity)->Count()==index+1);}
 modelCopy[4]=32;assert(!Invoke(modelCopy));assert(CL_JKACgameGhoul2Resolve(modelCopyIdentity)->Count()==32);
 intptr_t cleanCopy[]{301,reinterpret_cast<intptr_t>(&modelCopyIdentity)};
 const auto staleCopyIdentity=modelCopyIdentity;assert(!Invoke(cleanCopy)&&!modelCopyIdentity);
 modelCopy[3]=identity;Reject([&]{Invoke(modelCopy);}); // stale source identity
 modelCopy[1]=identity;modelCopy[3]=staleCopyIdentity;Reject([&]{Invoke(modelCopy);});
 assert(!jkaCgameSkins->bindings.count(modelCopyKey));
 for(int n=3;n<32;++n)assert(Invoke(init)==n);
 for(int n=3;n<32;++n)assert(CL_JKACgameGhoul2Resolve(identity)->At(n)->customSkin==0);
 assert(Invoke(init)==-1);has[2]=31;assert(Invoke(has));has[2]=32;assert(!Invoke(has));has[2]=0;
 const auto ownedSlot=slot;
 for(int ordinal:{339,340,341}){intptr_t cache[]{ordinal};assert(!Invoke(cache)&&slot==ownedSlot);
  assert(CL_JKACgameGhoul2Resolve(identity)->Count()==32);}
 init[2]=reinterpret_cast<intptr_t>("models/missing.glm");assert(Invoke(init)==-1&&slot==ownedSlot&&openFiles.empty());
 init[2]=reinterpret_cast<intptr_t>(filename.data());
 std::array<char,MAX_QPATH> unterminated;unterminated.fill('x');init[2]=reinterpret_cast<intptr_t>(unterminated.data());
 Reject([&]{Invoke(init);});assert(slot==ownedSlot);init[2]=reinterpret_cast<intptr_t>(filename.data());
 clean[1]=0;Reject([&]{Invoke(clean);});clean[1]=reinterpret_cast<intptr_t>(slot.data()+3);
 assert(!Invoke(clean)&&!Invoke(clean));std::uint32_t zero;std::memcpy(&zero,slot.data()+3,4);
 assert(!zero&&slot[2]==0xa5&&slot[7]==0xa5&&!Invoke(has));Reject([&]{Invoke(have);});
 // Cleaning the source cannot destroy the clone's actual file backing or
 // per-model binding. Clone cleanup invalidates only its own identity/slot.
 assert(CL_JKACgameGhoul2Resolve(duplicateIdentity)->HoldsAny());
 assert(jkaCgameSkins->bindings.count(duplicateBindingKey)&&!jkaCgameSkins->bindings.count(bindingKey));
 assert(CL_JKACgameGhoul2Store().DeformationPalette(*duplicateModel,25,sourcePalette));
 assert(sourcePalette.deformation==copyPalette.deformation);
 intptr_t cleanDuplicate[]{301,reinterpret_cast<intptr_t>(duplicateSlot.data()+1)};
 assert(!Invoke(cleanDuplicate));std::memcpy(&zero,duplicateSlot.data()+1,4);
 assert(!zero&&duplicateSlot[0]==0xa5&&duplicateSlot[5]==0xa5);
 have[1]=duplicateIdentity;Reject([&]{Invoke(have);});have[1]=identity;
 duplicate[1]=duplicateIdentity;const auto cleanedDuplicateSlot=duplicateSlot;
 Reject([&]{Invoke(duplicate);});assert(duplicateSlot==cleanedDuplicateSlot);
 assert(jkaCgameSkins->bindings.empty()&&!jkaCgameSkins->definitions.empty());
 std::memcpy(slot.data()+3,&identity,4);const auto staleSlot=slot;Reject([&]{Invoke(init);});Reject([&]{Invoke(clean);});assert(slot==staleSlot);
 std::memset(slot.data()+3,0,4);
 init[4]=17;assert(Invoke(init)==0);std::uint32_t replacement;std::memcpy(&replacement,slot.data()+3,4);assert(replacement!=identity);
 assert(CL_JKACgameGhoul2Resolve(replacement)->At(0)->customSkin==17);init[4]=0;
 jkaCgameSharedBuffer=slot.data();jkaFonts.push_back(3);registrationQuiet=qtrue;const int beforeReset=stringResets;fntech3::jka::command::selections.Set(255,257);
 CL_JKACgameClear();
 { usercmd_t cleared{};cleared.raven.generic_cmd=77;
   fntech3::jka::command::selections.Publish(cleared);
   assert(!cleared.raven.forcesel&&!cleared.raven.invensel&&cleared.raven.generic_cmd==77); }
assert(!registrationQuiet&&!jkaCgameGhoul2Store&&!jkaCgameSharedBuffer&&jkaFonts.empty()&&stringResets==beforeReset+1);have[1]=replacement;Reject([&]{Invoke(have);});
 fntech3::ghoul2::Store foreign(files);void *foreignHandle=nullptr;fntech3::ghoul2::LoadError error;
 assert(foreign.AddModel(&foreignHandle,"models/players/test/model.glm",error)==0);
 auto foreignIdentity=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(foreignHandle));
 modelCopy[1]=foreignIdentity;modelCopy[3]=0;Reject([&]{Invoke(modelCopy);});
 // A valid source cannot write a foreign destination, and no cross-owner
 // model transfer is made from server/UI ownership into this cgame Store.
 std::uint32_t validCopySource=0;const auto copyInitSlot=init[1];init[1]=reinterpret_cast<intptr_t>(&validCopySource);
 assert(Invoke(init)==0);init[1]=copyInitSlot;
 modelCopy[1]=validCopySource;modelCopy[3]=foreignIdentity;Reject([&]{Invoke(modelCopy);});
 intptr_t cleanSource[]{301,reinterpret_cast<intptr_t>(&validCopySource)};assert(!Invoke(cleanSource));
 duplicate[1]=foreignIdentity;const auto beforeForeignDuplicate=duplicateSlot;
 Reject([&]{Invoke(duplicate);});assert(duplicateSlot==beforeForeignDuplicate);
 std::memcpy(duplicateSlot.data()+1,&foreignIdentity,4);const auto foreignDestination=duplicateSlot;
 duplicate[1]=0;Reject([&]{Invoke(duplicate);});assert(duplicateSlot==foreignDestination);
 std::memset(duplicateSlot.data()+1,0,4);
 info[1]=foreignIdentity;Reject([&]{Invoke(info);});
 bolt[1]=foreignIdentity;Reject([&]{Invoke(bolt);});
 play[1]=foreignIdentity;Reject([&]{Invoke(play);});
 angle[1]=foreignIdentity;Reject([&]{Invoke(angle);});
 std::memcpy(slot.data()+3,&foreignIdentity,4);const auto foreignSlot=slot;Reject([&]{Invoke(init);});Reject([&]{Invoke(clean);});Reject([&]{Invoke(has);});assert(slot==foreignSlot);
 std::memset(slot.data()+3,0,4);shortRead=true;assert(Invoke(init)==-1);shortRead=false;assert(openFiles.empty()&&opened==closes);
 assets["models/broken.glm"]={1,2,3};init[2]=reinterpret_cast<intptr_t>("models/broken.glm");assert(Invoke(init)==-1);
 init[2]=reinterpret_cast<intptr_t>(filename.data());
 std::array<std::uint32_t,1024> identities{};
 for(auto &id:identities){init[1]=reinterpret_cast<intptr_t>(&id);assert(Invoke(init)==0&&id);}
 init[1]=reinterpret_cast<intptr_t>(slot.data()+3);assert(Invoke(init)==-1);std::memcpy(&zero,slot.data()+3,4);assert(!zero);
 duplicate[1]=identities[0];const auto fullBudgetSlot=duplicateSlot;
 Reject([&]{Invoke(duplicate);});assert(duplicateSlot==fullBudgetSlot&&CL_JKACgameGhoul2Store().InstanceCount()==1024);
 intptr_t unknown[]{299};intptr_t result=13;assert(!CL_JKACgameGhoul2Call(unknown,
  fntech3::jka::module::Call::CG_G2_COLLISIONDETECT,result)&&result==13);
 assert(openFiles.empty()&&opened==closes);CL_JKACgameGhoul2Clear();assert(!jkaCgameSkins);
 // Every read/backend failure leaves cleanup responsible for the actual open
 // file and pending definition; repeated teardown never closes it twice.
 for(int mode=0;mode<3;++mode){
  rendererSkins.clear();nextSkin=1;
  registerSkin[1]=reinterpret_cast<intptr_t>(skinName);
  shortRead=mode==0;skinDrop=mode==1;
  if(mode==2)assets[skinName]=Bytes{'b','a','d'};
  Reject([&]{Invoke(registerSkin);});CL_JKACgameGhoul2Clear();CL_JKACgameGhoul2Clear();
  assert(!jkaCgameSkins&&openFiles.empty()&&opened==closes);shortRead=false;skinDrop=false;
 }
 // The production error boundary uses longjmp. The pending file, bytes,
 // names and definitions all belong to the owner it destroys before jumping.
 assets[skinName]=Bytes(skinText.begin(),skinText.end());
 for(int mode=0;mode<3;++mode){
  rendererSkins.clear();nextSkin=1;registerSkin[1]=reinterpret_cast<intptr_t>(skinName);
  readDrop=mode==0;closeDrop=mode==1;skinDrop=mode==2;jumpRecovery=true;registrationQuiet=qtrue;
  if(!setjmp(recovery)){Invoke(registerSkin);assert(false);}
  jumpRecovery=readDrop=closeDrop=skinDrop=false;
  assert(!registrationQuiet&&!jkaCgameSkins&&!jkaCgameGhoul2Store&&openFiles.empty()&&opened==closes);
  CL_JKACgameClear();
 }

 vm.entryPoint=nullptr;vm.dataMask=255;vm.dataBase=slot.data();init[1]=254;Reject([&]{Invoke(init);});
}
'''

class JKACgameGhoul2Tests(unittest.TestCase):
    def test_owned_models_slots_and_failures(self):
        compiler=shutil.which('clang++') or shutil.which('g++')
        if not compiler:self.skipTest('C++ compiler unavailable')
        scratch=ROOT/'.tmp/tests/jka-cgame-ghoul2'
        scratch.mkdir(parents=True,exist_ok=True)
        with tempfile.TemporaryDirectory(dir=scratch) as folder:
            source=Path(folder)/'cgame.cpp';source.write_text(fixture())
            executable=Path(folder)/('cgame.exe' if os.name=='nt' else 'cgame')
            build=subprocess.run([compiler,*compiler_target_args(compiler),'-std=c++17','-Wall','-Wextra',
                '-Werror','-I',str(ROOT),str(source),'-o',str(executable)],cwd=ROOT,capture_output=True,text=True,timeout=120)
            self.assertEqual(build.returncode,0,build.stdout+build.stderr)
            run=subprocess.run([str(executable)],cwd=ROOT,capture_output=True,text=True,timeout=30)
            self.assertEqual(run.returncode,0,run.stdout+run.stderr)

if __name__=='__main__':unittest.main()
