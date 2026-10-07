"""The Raven servers' own Ghoul2 stores: Jedi Academy's serves the engine
`*default` skeleton, attached models and retail's tag frame through its real
game imports; Jedi Outcast's serves the same posing through one build per time,
answering its bolt queries 586 and 587 from what that build kept, as
jk2mp.exe 1.02a does, and serves the client setup and clips its retail module
asks for next (584, 592, 593, 596, 599) with that engine's clip timing."""
from pathlib import Path
import os
import re
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args
from rtcw_server_world_tests import function

ROOT = Path(__file__).resolve().parents[1]
BINDING = re.compile(r'\n\tfntech3::ghoul2::Store &ravenGhoul2 = (ravenGhoul2\w+);\n')


def binding(path, signature):
    """The store a dispatcher binds `ravenGhoul2` to, read from its body."""
    body = function((ROOT / path).read_text(encoding='utf-8'), signature)
    found = BINDING.findall(body)
    assert len(found) == 1, (path, found)
    return found[0]


def fixture():
    host = (ROOT / 'code/server/server_jka_game.hpp').read_text(encoding='utf-8')
    start = host.index('\tcase Call::G_G2_PLAYANIM:')
    end = host.index('\n\tcase Call::G_G2_ADDBOLT:', start)
    assert '\tcase Call::G_G2_SETBOLTINFO:' in host[start:end]
    # The server's pose clock, its no-rotation selector and the clock's setter.
    clock = host[host.index('static int jkaServerGhoul2Clock'):host.index('static void SV_JKAClearGame(')]
    academy = binding('code/server/server_jka_game.hpp', 'static bool SV_JKAGameCall(')
    # Import 597, and the angle setter whose bits its flags word carries.
    bone_anim = host[host.index('\tcase Call::G_G2_GETBONEANIM:'):host.index('\n\tcase Call::G_G2_GETGLANAME:')]
    bend = host[host.index('\tcase Call::G_G2_ANGLEOVERRIDE:'):host.index('\tcase Call::G_G2_PLAYANIM:')]
    vm = (ROOT / 'code/qcommon/vm.c').read_text(encoding='utf-8')
    tests = (ROOT / 'tests/jka_pose_tests.cpp').read_text(encoding='utf-8').replace(
        'int main(int argc,char **argv)', 'int PoseUnitFixtureMain(int argc,char **argv)')
    return tests + r'''
#include "code/compat/jka_module_abi.h"
#include <cstdlib>
#include <initializer_list>
#include <stdexcept>
#include <tuple>
using vec3_t=float[3];
struct vm_t { void *entryPoint; void *dllExports; unsigned char *dataBase; unsigned dataMask; };
constexpr int ERR_DROP=1;
static void Com_Error(int,const char*,...){throw std::runtime_error("bounds");}
#define VM_CHECKBOUNDS VM_CheckBounds
''' + function(vm, 'void *VM_ExplicitArgPtr(') + '\n' + function(vm, 'void VM_CheckBounds(') + r'''
// The engine's filesystem, served from memory, under the production stores.
static std::map<std::string,Bytes> installed;
static int FS_ReadFile(const char *path,void **buffer){
 const auto found=installed.find(path?path:"");
 if(found==installed.end()){if(buffer)*buffer=nullptr;return -1;}
 if(buffer){void *copy=std::malloc(found->second.size());std::memcpy(copy,found->second.data(),found->second.size());*buffer=copy;}
 return int(found->second.size());
}
static void FS_FreeFile(void *buffer){std::free(buffer);}
#include "code/server/server_raven_assets.hpp"
static vm_t native{reinterpret_cast<void*>(1),nullptr,nullptr,0};
static vm_t *gvm=&native;
static bool academyProfile=true;
int Com_GameIsJKAMP( void ){return academyProfile;}
''' + clock + r'''
static bool DispatchJKA(const intptr_t *args,intptr_t &result){
 using namespace fntech3::jka::module;
 fntech3::ghoul2::Store &ravenGhoul2 = ''' + academy + r''';
 const Call call=Lookup(COM_GAME_JKA_MP,Module::GAME,Direction::IMPORT,args[0]);
 const auto pointer=[args](int index){return VM_ExplicitArgPtr(gvm,args[index]);};
 const auto text=[&pointer](int index){return static_cast<const char *>(pointer(index));};
 const auto VMF=[args](int index){float value;const auto word=static_cast<std::uint32_t>(args[index]);std::memcpy(&value,&word,4);return value;};
 switch(call){
''' + host[start:end] + bone_anim + '\n' + bend + r'''
 default:return false;
 }
}
static intptr_t Import(std::initializer_list<intptr_t> list){
 intptr_t args[16]{};int at=0;for(intptr_t value:list)args[at++]=value;
 intptr_t result=-7;CHECK(DispatchJKA(args,result));return result;
}
static const float Angles[3]{15,25,35},Position[3]{7,-11,13},Scale[3]{1,1,1};
// One G_G2_GETBOLT (587) through the production case; false leaves `out` as
// the -777 the module wrote first.
static bool Bolt(void *owner,int model,int bolt,float out[12]){
 for(int n=0;n<12;++n)out[n]=-777.0f;
 return Import({587,reinterpret_cast<intptr_t>(owner),model,bolt,reinterpret_cast<intptr_t>(out),
  reinterpret_cast<intptr_t>(Angles),reinterpret_cast<intptr_t>(Position),100,0,reinterpret_cast<intptr_t>(Scale)})==1;
}
static bool Untouched(const float out[12]){for(int n=0;n<12;++n)if(out[n]!=-777.0f)return false;return true;}
static Bytes AuthoredTagMesh(const char *skeleton,int bones,const float (&points)[3][3]){
 auto bytes=Mesh(skeleton,bones);model::Mesh parsed;model::MeshSurface surface;
 CHECK(model::ParseMesh(bytes.data(),bytes.size(),bones,parsed)==model::Error::None);
 CHECK(model::SurfaceAt(bytes.data(),bytes.size(),parsed,0,0,surface));
 for(int v=0;v<3;++v)for(int n=0;n<3;++n)PutFloat(bytes,surface.vertexOffset+v*32+12+n*4,points[v][n]);
 // Its first triangle starts at vertex 1: retail reads the first three vertices.
 const int corners[3]{1,2,0};for(int c=0;c<3;++c)PutInt(bytes,surface.triangleOffset+c*4,corners[c]);
 return bytes;
}
static const float BodyTag[3][3]{{1,2,3},{4,2,3.5f},{1,-1,5}};
static const float WeaponTag[3][3]{{6,0,0},{6,3,0},{6,0,-2}};
static const char Body[]="models/players/test/model.glm",Weapon[]="models/weapons2/test/w.glm";
static const char DefaultBone[]="ModView internal default";
static void Skin(const pose::Matrix &m,const float in[3],double out[3]){
 for(int r=0;r<3;++r)out[r]=double(m[r*4])*in[0]+double(m[r*4+1])*in[1]+double(m[r*4+2])*in[2]+m[r*4+3];
}
// Retail's frame (jamp.exe 0x499476..0x499848) re-derived in double: columns
// |p0-p2|, the orthogonalized |p1-p0| and -|(p1-p0)x(p0-p2)|; origin p2.
static std::array<double,12> RetailTag(const double p[3][3]){
 double s0[3],s2[3],a[3],b[3],c[3];
 for(int n=0;n<3;++n){s0[n]=p[1][n]-p[0][n];s2[n]=p[0][n]-p[2][n];}
 auto unit=[](double *v){const double l=std::sqrt(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);for(int n=0;n<3;++n)v[n]/=l;};
 for(int n=0;n<3;++n){a[n]=s0[n];b[n]=s2[n];}unit(a);unit(b);
 const double d=a[0]*b[0]+a[1]*b[1]+a[2]*b[2];for(int n=0;n<3;++n)a[n]-=d*b[n];unit(a);
 c[0]=s0[1]*s2[2]-s0[2]*s2[1];c[1]=s0[2]*s2[0]-s0[0]*s2[2];c[2]=s0[0]*s2[1]-s0[1]*s2[0];unit(c);
 std::array<double,12> out{};
 for(int r=0;r<3;++r){out[r*4]=b[r];out[r*4+1]=a[r];out[r*4+2]=-c[r];out[r*4+3]=p[2][r];}
 return out;
}
static void *NewWeapon(fntech3::ghoul2::Store &store,int link,bool viaImport){
 void *weapon=nullptr;LoadError error;
 CHECK(store.AddModel(&weapon,Weapon,error)==0&&error==LoadError::None);
 if(viaImport)CHECK(Import({594,reinterpret_cast<intptr_t>(weapon),0,link})==1);
 else CHECK(store.SetBoltInfo(weapon,0,link));
 return weapon;
}
// One bolt query through the production case, with any of 587 to 589, time
// word and placement; false leaves `out` as written first unless a fallback is.
static const float Zero[3]{0,0,0};
static bool Query(int op,void *owner,int model,int bolt,int word,float out[12],
 const float *angles=Angles,const float *position=Position){
 for(int n=0;n<12;++n)out[n]=-777.0f;
 return Import({op,reinterpret_cast<intptr_t>(owner),model,bolt,reinterpret_cast<intptr_t>(out),
  reinterpret_cast<intptr_t>(angles),reinterpret_cast<intptr_t>(position),word,0,reinterpret_cast<intptr_t>(Scale)})==1;
}
static intptr_t Word(float value){std::uint32_t word;std::memcpy(&word,&value,4);return intptr_t(word);}
// Import 596, the clip setter.
static bool Play(void *owner,const char *bone,int first,int last,int flags,float speed,int now,float seed,int blend){
 return Import({596,reinterpret_cast<intptr_t>(owner),0,reinterpret_cast<intptr_t>(bone),first,last,flags,
  Word(speed),now,Word(seed),blend})==1;
}
static bool Same(const float a[12],const float b[12]){return !std::memcmp(a,b,48);}
// Retail jamp.exe 1.01's failed query, measured through its game imports with
// the angles and position used here: the entity's placement times the fixed
// MP axes (0x4dfc1d), scale not applied. Its x87 arithmetic and this engine's
// float part in the last bit or two; unplaced, they are the same bits.
static const std::uint32_t RetailFallback[12]{0xbe58b975,0xbf601bec,0x3ede7d9f,0x40e00000,0x3f4e1dd3,0xbed101f0,
 0xbedc47f8,0xc1300000,0x3f0dd510,0x3e8483ee,0x3f4a8eb6,0x41500000};
static const std::uint32_t RetailUnplacedFallback[12]{0,0xbf800000,0,0,0x3f800000,0,0,0,0,0,0x3f800000,0};
static bool IsFallback(const float out[12],const std::uint32_t (&retail)[12],float tolerance){
 for(int n=0;n<12;++n){float want;std::memcpy(&want,&retail[n],4);if(!(std::fabs(out[n]-want)<=tolerance))return false;}
 return true;
}
// The body as the client game's store would hold it (client_jka_cgame_ghoul2.hpp
// builds that store `*default`, blends and attachments on): a clip at 60, a
// transition at 90 lasting 50 -- or, `blend` false, the same clip change
// without one -- on the animated bone, and a weapon on that bone.
static void *Twin(fntech3::ghoul2::Store &reference,bool blend,void *&weapon){
 void *twin=nullptr;LoadError error;
 CHECK(reference.AddModel(&twin,Body,error)==0&&reference.AddBolt(twin,0,"root")==0&&reference.AddBolt(twin,0,"child")==1);
 CHECK(reference.SetBoneAnim(twin,0,"child",0,2,0x10,1.0f,60,-1.0f,0));
 CHECK(reference.SetBoneAnim(twin,0,"child",1,2,blend?0x10|0x80:0x10,1.0f,90,-1.0f,blend?50:0));
 weapon=NewWeapon(reference,1,false);
 CHECK(reference.AddBolt(weapon,0,"root")==0&&reference.CopyModel(weapon,0,twin,1));
 return twin;
}
// What the reference answers at one clock, placed as 587 places it.
static bool Reference(fntech3::ghoul2::Store &reference,void *owner,int model,int bolt,int clock,float out[12]){
 float raw[12];
 return reference.BoltMatrix(owner,model,bolt,clock,raw)&&pose::PlaceBolt(raw,Angles,Position,Scale,false,out);
}
// Academy's server poses at its own clock, through active transitions, and
// writes retail's fallback for retail's own failures (server_jka_game.hpp).
static void CheckAcademyClock(fntech3::ghoul2::Store &store,void *body){
 // The clock is the server's own, and only Academy's server sets it.
 SV_JKAGhoul2SetTime(100);CHECK(jkaServerGhoul2Clock==100);
 academyProfile=false;SV_JKAGhoul2SetTime(5000);academyProfile=true;CHECK(jkaServerGhoul2Clock==100);
 fntech3::ghoul2::Store reference(ravenGhoul2Files,true,true,true);
 void *blendWeapon=nullptr,*instantWeapon=nullptr;
 void *blended=Twin(reference,true,blendWeapon),*instant=Twin(reference,false,instantWeapon);
 // The same clip and transition through import 596, at the caller's times,
 // and the body's second weapon back on the animated bone (bolt 1).
 CHECK(Play(body,"child",0,2,0x10,1.0f,60,-1.0f,0)&&Play(body,"child",1,2,0x10|0x80,1.0f,90,-1.0f,50));
 CHECK(Import({594,reinterpret_cast<intptr_t>(body),2,1})==1);
 float out[12],word[12],want[12],cut[12],bone[12];
 for(int clock:{80,100,120,150}){
  SV_JKAGhoul2SetTime(clock);
  // The animated bone and the weapon it holds: the client game's answers at
  // the server's clock, whatever time word the module passes.
  for(auto [model,bolt,twinModel]:{std::tuple{0,1,0},std::tuple{2,0,1}}){
   CHECK(Query(587,body,model,bolt,clock,out)&&Reference(reference,blended,twinModel,bolt,clock,want)&&Same(out,want));
   for(int other:{0,clock+1234,99999})CHECK(Query(587,body,model,bolt,other,word)&&Same(word,out));
   CHECK(Reference(reference,instant,twinModel,bolt,clock,cut));
   // Inside the transition (90..140) the pose is a blend; before it starts
   // -- a clip set at 90 asked about at 80, as a server frame asks one
   // frame behind -- and after it ends it is the new clip alone.
   CHECK((clock>=90&&clock<140)==!Same(out,cut));
   if(clock==100&&!model)std::memcpy(bone,out,48);
  }
 }
 // The answer follows the clock, not the word: at 150 the 100 answer is gone.
 CHECK(Query(587,body,0,1,100,out)&&!Same(out,bone));
 SV_JKAGhoul2SetTime(100);CHECK(Query(587,body,0,1,150,out)&&Same(out,bone));
 // Retail's own failures write its fallback and answer 0: a model or bolt
 // the instance lacks, through any of the three imports.
 for(int op:{587,588,589}){
  for(auto [model,bolt]:{std::pair{9,0},std::pair{0,99},std::pair{0,-1},std::pair{1,5}}){
   float fallback[12];CHECK(pose::FailureBolt(Angles,Position,fallback));
   CHECK(!Query(op,body,model,bolt,100,out)&&IsFallback(out,RetailFallback,1e-6f)&&Same(out,fallback));
   CHECK(!Query(op,body,model,bolt,100,out,Zero,Zero)&&IsFallback(out,RetailUnplacedFallback,0));
  }
 }
 jkaServerNoRotationPending=false;
 // One this store never handed out has no retail answer: nothing written.
 CHECK(!Query(587,&out,0,0,100,out)&&Untouched(out));
 // A failed 589 leaves the no-rotation selector set: the next successful
 // query, 587 or 588, answers as 589 does, and the one after it rotates.
 float unrotated[12],rotated[12];
 CHECK(Query(589,body,0,1,100,unrotated)&&Query(587,body,0,1,100,rotated)&&!Same(unrotated,rotated));
 CHECK(!jkaServerNoRotationPending&&Same(rotated,bone));
 for(int op:{587,588}){
  CHECK(!Query(589,body,9,0,100,out)&&jkaServerNoRotationPending);
  CHECK(Query(op,body,0,1,100,out)&&Same(out,unrotated)&&!jkaServerNoRotationPending);
  CHECK(Query(op,body,0,1,100,out)&&Same(out,rotated));
 }
 // Import 596 bounds the clip as 0x4df326..0x4df36f does.
 CHECK(Play(body,"child",-5,0,0x10,1.0f,200,-0.5f,0));
 const auto *clip=store.BoneAnimation(body,0,"child");
 CHECK(clip&&clip->startFrame==0&&clip->endFrame==1&&clip->setFrame==0.0f&&clip->startTime==200);
 CHECK(Play(body,"child",100000,100000,0x10,1.0f,200,-1.0f,0));
 clip=store.BoneAnimation(body,0,"child");CHECK(clip&&clip->startFrame==0&&clip->endFrame==1&&clip->setFrame==-1.0f);
 for(void **handle:{&blended,&instant,&blendWeapon,&instantWeapon})CHECK(reference.Clean(handle));
}
// Academy's server: the engine `*default` skeleton, links and retail's frame.
static void CheckAcademy(){
 auto &store=ravenGhoul2Academy;
 CHECK(store.ServesAttachments());
 void *body=nullptr;LoadError error;
 CHECK(store.AddModel(&body,Body,error)==0&&error==LoadError::None);
 CHECK(store.AddBolt(body,0,"root")==0&&store.AddBolt(body,0,"child")==1);
 // 1: the body's tag; 2: the body's bone; 3: weapon 1's tag (a chain).
 void *weapons[3]{};
 for(int slot=1;slot<=3;++slot){
  weapons[slot-1]=NewWeapon(store,slot==1?0:slot==2?1:(1<<10),true);
  // A weapon on the engine skeleton has its tag and its one bone to bolt on.
  CHECK(store.AddBolt(weapons[slot-1],0,"root")==0&&store.AddBolt(weapons[slot-1],0,DefaultBone)==1);
  CHECK(store.CopyModel(weapons[slot-1],0,body,slot));
  CHECK(store.Resolve(body)->At(slot)->boltInfo==(slot==1?0:slot==2?1:(1<<10)));
 }
 float bodyTag[12],bodyBone[12],weaponTag[12],root[12],boneRoot[12],chainRoot[12],chainTag[12];
 CHECK(Bolt(body,0,0,bodyTag)&&Bolt(body,0,1,bodyBone)&&Bolt(body,1,0,weaponTag));
 CHECK(Bolt(body,1,1,root)&&Bolt(body,2,1,boneRoot)&&Bolt(body,3,1,chainRoot)&&Bolt(body,3,0,chainTag));
 // An attached root is its parent's bolt, placed alike: retail's m1_root ==
 // body_r_hand, m2_root == body_rhand_bone, m3_root == m1_flash, bit for bit.
 CHECK(!std::memcmp(root,bodyTag,48)&&!std::memcmp(boneRoot,bodyBone,48)&&!std::memcmp(chainRoot,weaponTag,48));
 CHECK(std::memcmp(weaponTag,root,48)&&std::memcmp(chainTag,chainRoot,48));
 // The surface bolt in model space is retail's frame of the skinned first
 // three vertices, not the first triangle's.
 pose::Palette parent;CHECK(store.DeformationPalette(*store.Resolve(body)->At(0),100,parent));
 double points[3][3];for(int v=0;v<3;++v)Skin(parent.deformation[0],BodyTag[v],points[v]);
 const auto frame=RetailTag(points);float raw[12];CHECK(store.BoltMatrix(body,0,0,100,raw));
 for(int n=0;n<12;++n)CHECK(std::fabs(raw[n]-frame[n])<.00002);
 // The weapon's own tag is its authored tag carried by the body's tag.
 pose::Matrix carriedBy;std::memcpy(carriedBy.data(),raw,48);double carried[3][3];
 for(int v=0;v<3;++v)Skin(carriedBy,WeaponTag[v],carried[v]);
 const auto weaponFrame=RetailTag(carried);float weaponRaw[12];CHECK(store.BoltMatrix(body,1,0,100,weaponRaw));
 for(int n=0;n<12;++n)CHECK(std::fabs(weaponRaw[n]-weaponFrame[n])<.00005);
 // The weapon's own instance links its only model to itself, so retail's
 // sort never poses it: refused, nothing written.
 float out[12];CHECK(!Bolt(weapons[0],0,0,out)&&Untouched(out));
 // A link to a bolt its parent lacks is refused rather than read past.
 CHECK(Import({594,reinterpret_cast<intptr_t>(body),2,9})==1);
 CHECK(!Bolt(body,2,1,out)&&Untouched(out));
 CHECK(Bolt(body,1,1,out)&&!std::memcmp(out,bodyTag,48));
 CheckAcademyClock(store,body);
 for(void *&weapon:weapons)CHECK(store.Clean(&weapon)&&!weapon);
 CHECK(store.Clean(&body)&&!body);
}
// Import 597 (G_G2_GETBONEANIM) through the production case, as retail
// jamp.exe 1.01's server answers it (0x45cc9c, 0x4df400, 0x4e3750, 0x4e3640;
// docs/fntech3/2026-10-07-jka-server-bone-anim.md). Every output starts as
// its own sentinel, and the model-list word points at a canary never read.
struct BoneAnswer{intptr_t result;std::uint32_t words[5];};
static const std::uint32_t BoneSentinels[5]{0x11111111u,0x22222222u,0x33333333u,0x44444444u,0x55555555u};
static void BoneArgs(intptr_t (&args)[16],void *owner,const char *bone,int model,int word,BoneAnswer &answer,int *list){
 std::memcpy(answer.words,BoneSentinels,sizeof(BoneSentinels));
 const intptr_t values[]{597,reinterpret_cast<intptr_t>(owner),reinterpret_cast<intptr_t>(bone),word,
  reinterpret_cast<intptr_t>(&answer.words[0]),reinterpret_cast<intptr_t>(&answer.words[1]),
  reinterpret_cast<intptr_t>(&answer.words[2]),reinterpret_cast<intptr_t>(&answer.words[3]),
  reinterpret_cast<intptr_t>(&answer.words[4]),reinterpret_cast<intptr_t>(list),model};
 for(int n=0;n<16;++n)args[n]=n<11?values[n]:0;
}
static BoneAnswer BoneAnim(void *owner,const char *bone,int model=0,int word=4321){
 BoneAnswer answer{};int list[2]{0x66666666,0x66666666};intptr_t args[16];
 BoneArgs(args,owner,bone,model,word,answer,list);
 CHECK(DispatchJKA(args,answer.result));
 CHECK(list[0]==0x66666666&&list[1]==0x66666666);
 return answer;
}
// A refusal by name (Com_Error) that writes nothing.
static bool BoneAnimRefused(void *owner,const char *bone,int model=0){
 BoneAnswer answer{};int list[2]{0x66666666,0x66666666};intptr_t args[16];
 BoneArgs(args,owner,bone,model,0,answer,list);
 try{DispatchJKA(args,answer.result);}catch(const std::runtime_error &){
  return !std::memcmp(answer.words,BoneSentinels,sizeof(BoneSentinels));
 }
 return false;
}
static bool Untouched(const BoneAnswer &answer){
 return answer.result==0&&!std::memcmp(answer.words,BoneSentinels,sizeof(BoneSentinels));
}
static std::uint32_t Bits(float value){std::uint32_t word;std::memcpy(&word,&value,4);return word;}
static bool Answers(const BoneAnswer &answer,intptr_t result,float frame,int start,int end,std::uint32_t flags,float speed){
 const std::uint32_t want[5]{Bits(frame),std::uint32_t(start),std::uint32_t(end),flags,Bits(speed)};
 return answer.result==result&&!std::memcmp(answer.words,want,sizeof(want));
}
// Import 595, the angle setter, with a blend time of 0.
static bool Bend(void *owner,const char *bone,int flags){
 static const float turn[3]{10,20,30};
 return Import({595,reinterpret_cast<intptr_t>(owner),0,reinterpret_cast<intptr_t>(bone),
  reinterpret_cast<intptr_t>(turn),flags,1,3,2,0,0,0})==1;
}
static void CheckAcademyBoneAnim(){
 auto &store=ravenGhoul2Academy;
 std::vector<void*> made;
 const auto fresh=[&]{void *owner=nullptr;LoadError error;
  CHECK(store.AddModel(&owner,Body,error)==0&&store.AddBolt(owner,0,"root")==0);made.push_back(owner);return owner;};
 void *body=fresh(),*weapon=NewWeapon(store,0,true);
 CHECK(store.CopyModel(weapon,0,body,1));
 SV_JKAGhoul2SetTime(0);
 // A name the skeleton lacks answers 0 and writes nothing (0x4e377e).
 for(const char *bone:{"no_such_bone",""})CHECK(Untouched(BoneAnim(body,bone)));
 // A bone it has, with no clip running, answers 0 and writes frame 0, frames
 // 0..1, no flags and no speed (0x4e3709). Names match without case.
 for(const char *bone:{"child","model_root","MODEL_ROOT"})CHECK(Answers(BoneAnim(body,bone),0,0.0f,0,1,0,0.0f));
 // A loop from time 0, asked at the server's clock whatever the time word:
 // the frame, the clip's range, its word with the setter's 0x8000, its speed.
 CHECK(Play(body,"model_root",0,3,0x10,1.0f,0,-1.0f,0));
 SV_JKAGhoul2SetTime(75);
 for(int word:{0,75,99999,-5})CHECK(Answers(BoneAnim(body,"model_root",0,word),1,1.5f,0,3,0x8010,1.0f));
 CHECK(Answers(BoneAnim(body,"Model_Root"),1,1.5f,0,3,0x8010,1.0f));
 SV_JKAGhoul2SetTime(175);CHECK(Answers(BoneAnim(body,"model_root"),1,0.5f,0,3,0x8010,1.0f));
 // A freeze holds its last frame.
 CHECK(Play(body,"child",0,3,0x48,1.0f,0,-1.0f,0));
 SV_JKAGhoul2SetTime(500);CHECK(Answers(BoneAnim(body,"child"),1,2.0f,0,3,0x8048,1.0f));
 // A blend from a running clip keeps 0x80 in the word, after the blend too.
 CHECK(Play(body,"model_root",1,3,0x90,1.0f,200,-1.0f,50));
 SV_JKAGhoul2SetTime(250);CHECK(Answers(BoneAnim(body,"model_root"),1,2.0f,1,3,0x8090,1.0f));
 SV_JKAGhoul2SetTime(1000);const BoneAnswer blended=BoneAnim(body,"model_root");
 CHECK(Answers(blended,1,1.0f,1,3,0x8090,1.0f));
 // One with no clip to leave drops it (0x4e34c7).
 void *second=fresh();
 CHECK(Play(second,"model_root",0,3,0xc8,1.0f,1000,-1.0f,50));
 SV_JKAGhoul2SetTime(1025);CHECK(Answers(BoneAnim(second,"model_root"),1,0.5f,0,3,0x8048,1.0f));
 // A bend alone runs no clip: the word's angle bit is not written. Played,
 // the word carries the bend; bent again, its low three bits are replaced.
 CHECK(Bend(second,"child",2)&&Answers(BoneAnim(second,"child"),0,0.0f,0,1,0,0.0f));
 CHECK(Play(second,"child",0,3,0x48,1.0f,1000,-1.0f,0));
 CHECK(Answers(BoneAnim(second,"child"),1,0.5f,0,3,0x804a,1.0f));
 CHECK(Bend(second,"child",4)&&Answers(BoneAnim(second,"child"),1,0.5f,0,3,0x804c,1.0f));
 // The retail game hands an answer straight back to the clip setter (its IK
 // release): the mark is no part of the clip, which still poses, and a blend
 // asked of a running clip is kept, of none dropped.
 void *third=fresh();
 int start,end,flags;float speed;
 std::memcpy(&start,&blended.words[1],4);std::memcpy(&end,&blended.words[2],4);
 std::memcpy(&flags,&blended.words[3],4);std::memcpy(&speed,&blended.words[4],4);
 CHECK(Play(third,"model_root",start,end,flags,speed,1000,float(start),300));
 CHECK(store.BoneAnimation(third,0,"model_root")->flags==0x10);
 CHECK(Answers(BoneAnim(third,"model_root"),1,1.5f,1,3,0x8010,1.0f));
 CHECK(Play(second,"child",start,end,flags,speed,1025,float(start),300));
 CHECK(store.BoneAnimation(second,0,"child")->flags==0x90);
 SV_JKAGhoul2SetTime(1050);CHECK(Answers(BoneAnim(second,"child"),1,1.5f,1,3,0x8094,1.0f));
 float out[12];CHECK(Bolt(third,0,0,out)&&Bolt(second,0,0,out));
 // A seed shifts the clip's start; a range outside the setter's bounds is
 // bounded (0x4df326).
 void *fourth=fresh();
 CHECK(Play(fourth,"model_root",0,3,0x48,0.5f,100,1.5f,0));
 SV_JKAGhoul2SetTime(100);CHECK(Answers(BoneAnim(fourth,"model_root"),1,1.5f,0,3,0x8048,0.5f));
 CHECK(Play(fourth,"child",-5,200000,0x48,1.0f,0,-1.0f,0));
 CHECK(Answers(BoneAnim(fourth,"child"),1,0.0f,0,1,0x8048,1.0f));
 // A record carrying 0x2000 takes neither setter's flags: the clip set on it
 // runs, as far as import 597 can tell, nowhere.
 void *fifth=fresh();
 CHECK(Bend(fifth,"child",0x2000)&&Play(fifth,"child",0,3,0x48,1.0f,0,-1.0f,0));
 CHECK(store.BoneFlagsWord(fifth,0,"child")==0x2000&&Answers(BoneAnim(fifth,"child"),0,0.0f,0,1,0,0.0f));
 // A word with bit 31 is cleared before the next clip's flags, mark and all
 // (0x4e3566..0x4e3572, read rather than measured).
 CHECK(Play(fifth,"model_root",0,3,static_cast<int>(0x80000048u),1.0f,0,-1.0f,0));
 CHECK(store.BoneFlagsWord(fifth,0,"model_root")==0x80008048u);
 CHECK(Play(fifth,"model_root",0,3,0x48,1.0f,0,-1.0f,0)&&store.BoneFlagsWord(fifth,0,"model_root")==0x48);
 CHECK(Answers(BoneAnim(fifth,"model_root"),1,2.0f,0,3,0x48,1.0f));
 // The weapon's engine skeleton, by model index; a model the instance lacks
 // answers 0 and writes nothing, as does a bone that model lacks.
 CHECK(Answers(BoneAnim(body,DefaultBone,1),0,0.0f,0,1,0,0.0f));
 for(int model:{-1,2,9})CHECK(Untouched(BoneAnim(body,"model_root",model)));
 CHECK(Untouched(BoneAnim(body,"model_root",1)));
 CHECK(Import({596,reinterpret_cast<intptr_t>(body),1,reinterpret_cast<intptr_t>(DefaultBone),0,1,0x48,
  Word(1.0f),1000,Word(-1.0f),0})==1);
 CHECK(Answers(BoneAnim(body,DefaultBone,1),1,0.0f,0,1,0x8048,1.0f));
 // A copy keeps its records' words.
 void *copy=nullptr;CHECK(store.Duplicate(body,&copy));made.push_back(copy);
 CHECK(Answers(BoneAnim(copy,"model_root"),1,1.0f,1,3,0x8090,1.0f));
 // Refused by name, nothing written: a clip outside the served forward
 // timing -- a plain clip past its end, which retail's timing answers by
 // clearing the word's animation bits, a reverse clip, a flag bit outside
 // the clip's own -- animation bits given through the angle setter, an
 // instance this engine never handed out, and an output retail would write
 // through null. A null output beside a name the skeleton lacks is never
 // written, and answers 0.
 void *sixth=fresh();
 CHECK(Play(sixth,"model_root",0,3,0x08,1.0f,1000,-1.0f,0));
 SV_JKAGhoul2SetTime(1050);CHECK(Answers(BoneAnim(sixth,"model_root"),1,1.0f,0,3,0x8008,1.0f));
 SV_JKAGhoul2SetTime(1150);CHECK(BoneAnimRefused(sixth,"model_root"));
 CHECK(Play(sixth,"child",0,3,0x48,-1.0f,1000,-1.0f,0)&&BoneAnimRefused(sixth,"child"));
 void *seventh=fresh();
 CHECK(Play(seventh,"model_root",0,3,0x1048,1.0f,1000,-1.0f,0)&&BoneAnimRefused(seventh,"model_root"));
 CHECK(store.BoneFlagsWord(seventh,0,"model_root")==0x9048);
 CHECK(Bend(seventh,"child",0x48)&&BoneAnimRefused(seventh,"child"));
 // So is a running clip whose word the angle setter changed: its loop bit,
 // or a 0x2000 after which retail keeps the old clip whatever is set.
 CHECK(Bend(fourth,"model_root",0x10)&&BoneAnimRefused(fourth,"model_root"));
 CHECK(Bend(fourth,"child",0x2000)&&BoneAnimRefused(fourth,"child"));
 CHECK(BoneAnimRefused(&out,"model_root"));
 for(const char *bone:{"model_root","no_such_bone"}){
  intptr_t args[16];BoneAnswer answer{};int list[2]{};BoneArgs(args,body,bone,0,0,answer,list);args[6]=0;
  bool refused=false;try{DispatchJKA(args,answer.result);}catch(const std::runtime_error &){refused=true;}
  CHECK(refused==(bone[0]=='m'));
  if(!refused)CHECK(answer.result==0&&!std::memcmp(answer.words,BoneSentinels,sizeof(BoneSentinels)));
 }
 CHECK(store.Clean(&weapon));
 for(void *&owner:made)CHECK(store.Clean(&owner)&&!owner);
}
// Outcast's server keeps builds as jk2mp.exe 1.02a does (0x4790f0, 0x4a7620):
// one build per time, judged by model 0 alone; answers from what the build
// kept in each bolt; 587 never building; and the engine `*default` skeleton,
// links and retail's tag frame, which it shares with Academy.
static bool Kept(void *owner,int model,int bolt,int now,bool norec,float out[12]){
 for(int n=0;n<12;++n)out[n]=-777.0f;
 return ravenGhoul2Outcast.KeptBolt(owner,model,bolt,now,norec,out)==fntech3::ghoul2::Posing::Served;
}
// A body with its tag and root bone as bolts 0 and 1, and a weapon on the
// engine skeleton linked to the body's tag and copied into its second place.
static void Rig(fntech3::ghoul2::Store &store,void *&body,void *&weapon){
 LoadError error;
 CHECK(store.AddModel(&body,Body,error)==0&&store.AddBolt(body,0,"root")==0&&store.AddBolt(body,0,"model_root")==1);
 CHECK(store.AddModel(&weapon,Weapon,error)==0&&store.SetBoltInfo(weapon,0,0));
 CHECK(store.AddBolt(weapon,0,"root")==0&&store.AddBolt(weapon,0,DefaultBone)==1);
 CHECK(store.CopyModel(weapon,0,body,1));
}
// Outcast's clip timing, read from jk2mp.exe 1.02a: the transform's frames
// (0x476380), the setter's clamps and transition sample (0x4a7440, 0x4ab070,
// 0x4ab420), and what its store poses from them.
static void CheckOutcastClips(){
 namespace outcast=fntech3::ghoul2::outcast;
 using Clip=fntech3::ghoul2::BoneAnimation;
 // A clip's age: milliseconds times the float .02, kept as a float, where
 // Academy divides by fifty; the int subtraction wraps.
 CHECK(outcast::Age(5,0)==0.099999994039535522f&&outcast::Age(5,0)!=5/50.0f&&outcast::Age(50,0)==1.0f);
 CHECK(outcast::Age(0,50)==0.0f&&outcast::Age(INT_MIN+20,INT_MAX-29)==1.0f);
 // _ftol: truncation, the low word of 64 bits, 0 for what it cannot store.
 CHECK(outcast::Truncate(2.75)==2&&outcast::Truncate(-2.75)==-2&&outcast::Truncate(4294967298.0)==2);
 const std::uint64_t notANumber=0x7ff8000000000000ull;double nan;std::memcpy(&nan,&notANumber,8);
 CHECK(!outcast::Truncate(nan)&&!outcast::Truncate(1e30));
 const auto control=[](int flags,int start,int end,float speed,int now,pose::Animation &out){
  Clip clip;clip.bone=1;clip.flags=flags;clip.startFrame=start;clip.endFrame=end;clip.speed=speed;
  bool has=false;return outcast::Control(clip,now,out,has)&&has;};
 const auto frames=[&](int flags,int start,int end,float speed,int now,int current,int next,float lerp){
  pose::Animation out;
  return control(flags,start,end,speed,now,out)&&!out.inheritFrame&&!out.blending&&out.bone==1
   &&out.frame.current==current&&out.frame.next==next&&out.frame.lerp==lerp;};
 // Forwards; past the end looping (towards the start, then wrapped by
 // fmod), frozen one frame inside, and neither: the parent's frames.
 CHECK(frames(0x10,0,4,1,75,1,2,.5f)&&frames(0x10,0,4,1,175,3,0,.5f)&&frames(0x10,0,4,1,225,0,1,.5f));
 CHECK(frames(0x48,0,4,1,1000,3,3,0)&&frames(0x10,2,2,1,500,1,1,0));
 pose::Animation ended;CHECK(control(0x08,0,4,1,1000,ended)&&ended.inheritFrame&&!ended.blending);
 // Evaluate keeps such a bone on its parent's frames, not on frame 0.
 {
  const Bytes file=PoseSkeleton();model::Skeleton skeleton;
  CHECK(model::ParseSkeleton(file.data(),file.size(),skeleton)==model::Error::None);
  const pose::Animation root{0,{1,2,.5f}},zero{1,{0,0,0}};
  pose::Palette inherited,parentOnly,frameZero;
  CHECK(pose::Evaluate(file.data(),file.size(),skeleton,{root,ended},{},inherited));
  CHECK(pose::Evaluate(file.data(),file.size(),skeleton,{root},{},parentOnly));
  CHECK(pose::Evaluate(file.data(),file.size(),skeleton,{root,zero},{},frameZero));
  CHECK(inherited.deformation==parentOnly.deformation&&inherited.deformation!=frameZero.deformation);
 }
 // Backwards, from 3 down to 0: ceilings, then past the end looped back.
 CHECK(frames(0x10,3,0,-1,25,3,2,.5f)&&frames(0x10,3,0,-1,100,1,1,0));
 CHECK(frames(0x10,3,0,-1,125,0,3,.5f)&&frames(0x10,3,0,-1,175,3,2,.5f));
 // A reverse clip whose start lies below its end, as the retail module runs
 // one, is past its end at once: frozen one frame outside it.
 CHECK(frames(0x48,1,3,-1,40,4,4,0));
 // Refused: a frame below 0 (retail abandons the bone and those below it),
 // angle bits given to a clip, and a speed that is not finite.
 pose::Animation refused;
 CHECK(!control(0x10,0,2,-1,50,refused)&&!control(0x11,0,2,1,50,refused));
 const std::uint32_t infiniteBits=0x7f800000u;float infinite;std::memcpy(&infinite,&infiniteBits,4);
 CHECK(!control(0x10,0,2,infinite,50,refused));
 // No clip and no transition: no control at all.
 {Clip none;none.bone=1;bool has=true;CHECK(outcast::Control(none,0,refused,has)&&!has);}

 auto &store=ravenGhoul2Outcast;
 void *body=nullptr;LoadError error;CHECK(store.AddModel(&body,Body,error)==0);
 const auto clip=[&](const char *bone){return store.BoneAnimation(body,0,bone);};
 // A transition with no clip running drops its bit and its sample.
 CHECK(store.SetBoneAnim(body,0,"child",0,2,0x90,1,0,-1,100));
 CHECK(clip("child")->flags==0x10&&!clip("child")->blendTime&&!clip("child")->blendStart);
 // At 75 the loop has reached 1.5: the sample keeps it, and the frame after
 // it, which the loop wraps to its start.
 CHECK(store.SetBoneAnim(body,0,"child",1,3,0xc8,1,75,-1,100));
 const Clip *child=clip("child");
 CHECK(child->flags==0xc8&&child->blendFrame==1.5f&&child->blendLerpFrame==0&&child->blendStart==75);
 CHECK(child->blendTime==100&&child->startTime==75&&child->startFrame==1&&child->endFrame==3);
 // Set again at the same time: the sample stays and the duration is new.
 CHECK(store.SetBoneAnim(body,0,"child",1,3,0xc8,1,75,-1,150)&&child->blendFrame==1.5f&&child->blendTime==150);
 // Halfway through, the transition blends the sample -- frame 1 weighted
 // .5, the frame after it .5 -- into the new clip's frame 2.
 pose::Animation half;
 {bool has=false;CHECK(outcast::Control(*child,150,half,has)&&has&&half.blending&&!half.inheritFrame);}
 CHECK(half.previous.current==0&&half.previous.next==1&&half.previous.lerp==.5f&&half.transition==.5f);
 CHECK(half.frame.current==2&&half.frame.next==2&&half.frame.lerp==0);
 const fntech3::ghoul2::Model &model=*store.Resolve(body)->At(0);
 pose::Palette palette,expect;CHECK(store.DeformationPalette(model,150,palette));
 CHECK(pose::Evaluate(model.skeletonBytes->data(),model.skeletonBytes->size(),model.skeleton,{half},{},expect));
 CHECK(palette.deformation==expect.deformation&&palette.bones==expect.bones);
 // After it, none; without the bit, the sample and its start are zeroed.
 {bool has=false;CHECK(outcast::Control(*child,225,half,has)&&has&&!half.blending);}
 CHECK(store.SetBoneAnim(body,0,"child",0,2,0x48,1,300,-1,0)&&!child->blendStart&&!child->blendTime&&child->blendFrame==0);
 // A reverse clip is sampled at its floor, twice.
 CHECK(store.SetBoneAnim(body,0,"model_root",2,0,0x10,-1,0,-1,0)&&clip("model_root")->endFrame==1);
 CHECK(store.SetBoneAnim(body,0,"model_root",0,2,0x90,1,25,-1,100));
 CHECK(clip("model_root")->blendFrame==2.0f&&clip("model_root")->blendLerpFrame==2);
 // The core's clamps, and a set frame moving the start back at fifty
 // milliseconds a frame over the speed, truncated.
 CHECK(store.SetBoneAnim(body,0,"model_root",-5,100000,0x10,2,1000,1.5f,0));
 CHECK(clip("model_root")->startFrame==0&&clip("model_root")->endFrame==1&&clip("model_root")->startTime==962);
 CHECK(store.SetBoneAnim(body,0,"model_root",1,2,0x10,1,1000,-3,0)&&clip("model_root")->setFrame==0&&clip("model_root")->startTime==1050);
 CHECK(store.SetBoneAnim(body,0,"model_root",1,2,0x10,0,1000,1,0)&&!clip("model_root")->startTime);
 // The model's build time goes to 0 before the bone is looked up.
 float out[12];CHECK(store.AddBolt(body,0,"root")==0&&Kept(body,0,0,600,false,out));
 CHECK(!store.SetBoneAnim(body,0,"no such bone",0,2,0x10,1,600,-1,0)&&!store.Resolve(body)->At(0)->builtAt);
 CHECK(store.Clean(&body));
}
static void CheckOutcast(){
 auto &store=ravenGhoul2Outcast,&academy=ravenGhoul2Academy;
 CHECK(store.ServesAttachments()&&store.KeepsBuilds()&&!academy.KeepsBuilds());
 void *body=nullptr,*weapon=nullptr,*twin=nullptr,*twinWeapon=nullptr;
 Rig(store,body,weapon);Rig(academy,twin,twinWeapon);
 const auto builtAt=[&](int model){return store.Resolve(body)->At(model)->builtAt;};
 // Academy answers on demand; for one pose Outcast's kept answers are the same bits.
 const auto same=[&](int model,int bolt,int now,const float kept[12]){
  float expect[12];return academy.BoltMatrix(twin,model,bolt,now,expect)&&!std::memcmp(expect,kept,48);};
 float out[12],tag[12],bone[12],root[12],weaponTag[12];
 // Nothing is kept before a build, and 587 never builds: refused, nothing
 // written. (The copy that grew the body set model 0's build time to 0.)
 CHECK(!Kept(body,0,0,100,true,out)&&Untouched(out)&&builtAt(0)==0);
 // 586 builds at 100; the weapon's root is the body's tag bit for bit.
 CHECK(Kept(body,0,0,100,false,tag)&&Kept(body,0,1,100,false,bone));
 CHECK(Kept(body,1,1,100,false,root)&&Kept(body,1,0,100,false,weaponTag));
 CHECK(!std::memcmp(root,tag,48)&&builtAt(0)==100&&builtAt(1)==100);
 CHECK(same(0,0,100,tag)&&same(0,1,100,bone)&&same(1,1,100,root)&&same(1,0,100,weaponTag));
 // Bending model 0 sets its build time to 0, so a query at the same time builds again.
 const float bend[3]{10,20,30};
 CHECK(store.SetBoneAngles(body,0,"model_root",bend,2,1,6,5,0,100)&&builtAt(0)==0);
 CHECK(academy.SetBoneAngles(twin,0,"model_root",bend,2,1,6,5,0,100));
 float bentTag[12],bentWeaponTag[12];
 CHECK(Kept(body,0,0,100,false,bentTag)&&std::memcmp(bentTag,tag,48)&&same(0,0,100,bentTag));
 CHECK(Kept(body,1,0,100,false,bentWeaponTag)&&std::memcmp(bentWeaponTag,weaponTag,48)&&same(1,0,100,bentWeaponTag));
 // Bending the weapon sets only its own: model 0 still says 100, so a query
 // at 100 answers the last build. 587 answers it at any time; 586 at 150 builds.
 CHECK(store.SetBoneAngles(body,1,DefaultBone,bend,2,1,6,5,0,100)&&builtAt(0)==100&&builtAt(1)==0);
 CHECK(academy.SetBoneAngles(twin,1,DefaultBone,bend,2,1,6,5,0,100));
 CHECK(Kept(body,1,0,100,false,out)&&!std::memcmp(out,bentWeaponTag,48));
 CHECK(Kept(body,1,0,150,true,out)&&!std::memcmp(out,bentWeaponTag,48)&&builtAt(0)==100);
 float moved[12];CHECK(Kept(body,1,0,150,false,moved)&&std::memcmp(moved,bentWeaponTag,48)&&same(1,0,150,moved));
 // A 587 refused for its bolt index leaves its flag set for the next query
 // (0x4a76c1 clears it only past the index checks).
 CHECK(!Kept(body,0,9,200,true,out)&&Untouched(out));
 CHECK(Kept(body,0,0,200,false,out)&&builtAt(0)==150);
 CHECK(Kept(body,0,0,200,false,out)&&builtAt(0)==200);
 // A bolt added after the last build is kept by none until a build at a new time.
 CHECK(store.AddBolt(body,0,"child")==2);
 CHECK(!Kept(body,0,2,200,false,out)&&Untouched(out)&&Kept(body,0,2,250,false,out));
 // A copy that grows the instance sets model 0's build time to 0; one that
 // replaces a model does not, and the record it copies kept nothing.
 CHECK(store.CopyModel(weapon,0,body,2)&&builtAt(0)==0&&Kept(body,2,0,250,false,out));
 CHECK(store.CopyModel(weapon,0,body,1)&&builtAt(0)==250);
 CHECK(!Kept(body,1,0,250,false,out)&&Untouched(out)&&Kept(body,1,0,300,false,out));
 // The weapon's own instance links its only model to itself: never built.
 CHECK(!Kept(weapon,0,0,300,false,out)&&Untouched(out));
 // Bends PREMULT and POSTMULT at any blend time; REPLACE refuses.
 CHECK(store.SetBoneAngles(body,0,"model_root",bend,2,1,6,5,100,300)&&Kept(body,0,0,300,false,out));
 CHECK(!std::memcmp(out,bentTag,48));
 CHECK(store.SetBoneAngles(body,0,"model_root",bend,4,1,6,5,0,350)&&!Kept(body,0,0,350,false,out)&&Untouched(out));
 CHECK(store.SetBoneAngles(body,0,"model_root",bend,2,1,6,5,0,400)&&Kept(body,0,0,400,false,out));
 // A clip on model 0 sets its build time to 0 at once (0x4a74f6). At 475 the
 // loop has reached 1.5 under either timing, lerping towards its start, so
 // Outcast's kept answers are Academy's bits.
 CHECK(store.SetBoneAnim(body,0,"child",0,2,0x10,1.0f,400,-1.0f,0)&&builtAt(0)==0);
 CHECK(academy.SetBoneAnim(twin,0,"child",0,2,0x10,1.0f,400,-1.0f,0)&&academy.AddBolt(twin,0,"child")==2);
 for(int bolt=0;bolt<3;++bolt)CHECK(Kept(body,0,bolt,475,false,out)&&same(0,bolt,475,out));
 float still[12];CHECK(Kept(body,0,2,400,false,still)&&std::memcmp(still,out,48));
 for(float &value:out)value=-777.0f;
 // Academy's store answers on demand and keeps no build.
 CHECK(academy.KeptBolt(twin,0,0,100,false,out)==fntech3::ghoul2::Posing::Unserved&&Untouched(out));
 for(void **handle:{&body,&weapon})CHECK(store.Clean(handle));
 for(void **handle:{&twin,&twinWeapon})CHECK(academy.Clean(handle));
 CheckOutcastClips();
}
int main(){
 installed[Body]=AuthoredTagMesh("models/players/test/skeleton",3,BodyTag);
 installed["models/players/test/skeleton.gla"]=PoseSkeleton();
 installed[Weapon]=AuthoredTagMesh("*default",1,WeaponTag);
 CheckAcademy();CheckAcademyBoneAnim();CheckOutcast();
 // A map change clears both titles' stores.
 void *a=nullptr,*b=nullptr;LoadError error;
 CHECK(ravenGhoul2Academy.AddModel(&a,Body,error)==0&&ravenGhoul2Outcast.AddModel(&b,Body,error)==0);
 SV_RavenAssetsClear();
 CHECK(!ravenGhoul2Academy.InstanceCount()&&!ravenGhoul2Outcast.InstanceCount());
 std::cout<<"Raven server attachment stores passed\n";
 return 0;
}
'''


def outcast_fixture():
    """Jedi Outcast's production Ghoul2 cases of SV_JK2GameCall, with its handle
    and field reading, driven as the retail QVM drives them -- its bolt queries,
    its client setup and its per-frame clips -- and as a native module would."""
    host = (ROOT / 'code/server/server_jk2_game.hpp').read_text(encoding='utf-8')
    numbers = '\n'.join(line for line in host.splitlines()
                        if line.startswith('inline constexpr int FNTECH3_JK2_G2_'))
    start = host.index('\tif ( trap == FNTECH3_JK2_G2_HAVE_MODELS ) {')
    end = host.index("\n\t/* The call that publishes the module's arrays.", start)
    outcast = binding('code/server/server_jk2_game.hpp', 'static bool SV_JK2GameCall(')
    vm = (ROOT / 'code/qcommon/vm.c').read_text(encoding='utf-8')
    tests = (ROOT / 'tests/jka_pose_tests.cpp').read_text(encoding='utf-8').replace(
        'int main(int argc,char **argv)', 'int PoseUnitFixtureMain(int argc,char **argv)')
    return tests + r'''
#include <cstdlib>
#include <initializer_list>
#include <stdexcept>
using vec3_t=float[3];
struct vm_t { void *entryPoint; void *dllExports; unsigned char *dataBase; unsigned dataMask; };
constexpr int ERR_DROP=1;
static void Com_Error(int,const char*,...){throw std::runtime_error("bounds");}
static void Com_Printf(const char*,...){}
#define VM_CHECKBOUNDS VM_CheckBounds
''' + function(vm, 'void *VM_ExplicitArgPtr(') + '\n' + function(vm, 'void VM_CheckBounds(') + r'''
static std::map<std::string,Bytes> installed;
static int FS_ReadFile(const char *path,void **buffer){
 const auto found=installed.find(path?path:"");
 if(found==installed.end()){if(buffer)*buffer=nullptr;return -1;}
 if(buffer){void *copy=std::malloc(found->second.size());std::memcpy(copy,found->second.data(),found->second.size());*buffer=copy;}
 return int(found->second.size());
}
static void FS_FreeFile(void *buffer){std::free(buffer);}
#include "code/server/server_raven_assets.hpp"
static vm_t *gvm=nullptr;
static void *VM_ArgPtr(intptr_t value){return VM_ExplicitArgPtr(gvm,value);}
// A float argument's word, as qcommon.h's VMF reads it.
static float _vmf(intptr_t x){const std::int32_t word=std::int32_t(x);float value;std::memcpy(&value,&word,4);return value;}
#define VMF(x) _vmf(args[x])
''' + numbers + '\n' + '\n'.join(function(host, signature) for signature in (
        'static bool SV_JK2ModuleIsBytecode(', 'static void *SV_JK2Ghoul2Handle(',
        'static std::size_t SV_JK2Ghoul2FieldBytes(')) + r'''
static bool DispatchJK2(const intptr_t *args,intptr_t &result){
 fntech3::ghoul2::Store &ravenGhoul2 = ''' + outcast + r''';
 const int trap=static_cast<int>(args[0]);
''' + host[start:end] + r'''
 return false;
}
static intptr_t Call(std::initializer_list<intptr_t> list){
 intptr_t args[16]{};int at=0;for(intptr_t value:list)args[at++]=value;
 intptr_t result=-7;CHECK(DispatchJK2(args,result));return result;
}
// The retail module's memory: a QVM's data segment, addressed by offset.
alignas(16) static unsigned char memory[0x10000];
static vm_t qvm{nullptr,nullptr,memory,0xffff};
static int nativeEntry;
static vm_t native{&nativeEntry,nullptr,nullptr,0};
static const char Body[]="models/players/test/model.glm",Weapon[]="models/weapons2/test/w.glm";
static const char DefaultBone[]="ModView internal default";
static const float Angles[3]{15,25,35},Position[3]{7,-11,13},Zero[3]{},Bend[3]{10,20,30};
enum : intptr_t { BodySlot=0x100,WeaponSlot=0x108,BodyName=0x200,WeaponName=0x280,TagName=0x300,
 RootName=0x310,ChildName=0x320,DefaultName=0x330,AnglesAt=0x400,PositionAt=0x410,ZeroAt=0x420,
 BendAt=0x440,MatrixAt=0x500,
 // The retail module's own fields, a word each with a guard word after it,
 // and a 64-byte buffer for 593.
 KyleField=0x600,SaberField=0x608,ClientField=0x610,OtherField=0x618,GLABuffer=0x700 };
static void Put(intptr_t at,const void *bytes,std::size_t size){std::memcpy(memory+at,bytes,size);}
static intptr_t Word(intptr_t at){std::uint32_t word;std::memcpy(&word,memory+at,4);return intptr_t(word);}
static intptr_t FloatWord(float value){std::uint32_t word;std::memcpy(&word,&value,4);return intptr_t(std::int32_t(word));}
static bool Untouched(const float out[12]){for(int n=0;n<12;++n)if(out[n]!=-777.0f)return false;return true;}
// A mesh whose tag has three usable first vertices; its first triangle starts
// at vertex 1, since retail's frame reads the first three vertices.
static Bytes AuthoredTagMesh(const char *skeleton,int bones,const float (&points)[3][3]){
 auto bytes=Mesh(skeleton,bones);model::Mesh parsed;model::MeshSurface surface;
 CHECK(model::ParseMesh(bytes.data(),bytes.size(),bones,parsed)==model::Error::None);
 CHECK(model::SurfaceAt(bytes.data(),bytes.size(),parsed,0,0,surface));
 for(int v=0;v<3;++v)for(int n=0;n<3;++n)PutFloat(bytes,surface.vertexOffset+v*32+12+n*4,points[v][n]);
 const int corners[3]{1,2,0};for(int c=0;c<3;++c)PutInt(bytes,surface.triangleOffset+c*4,corners[c]);
 return bytes;
}
static const float BodyTag[3][3]{{1,2,3},{4,2,3.5f},{1,-1,5}};
static const float WeaponTag[3][3]{{6,0,0},{6,3,0},{6,0,-2}};
// One 586 or 587 through the production case. In the QVM the matrix, angles,
// position and scale are offsets into its memory; natively they are pointers.
static bool Query(int import,intptr_t owner,int model,int bolt,int now,float out[12]){
 const float unwritten=-777.0f;
 if(gvm==&qvm){
  for(int n=0;n<12;++n)Put(MatrixAt+4*n,&unwritten,4);
  const bool answered=Call({import,owner,model,bolt,MatrixAt,AnglesAt,PositionAt,now,0,ZeroAt})==1;
  std::memcpy(out,memory+MatrixAt,48);return answered;
 }
 for(int n=0;n<12;++n)out[n]=unwritten;
 return Call({import,owner,model,bolt,reinterpret_cast<intptr_t>(out),reinterpret_cast<intptr_t>(Angles),
  reinterpret_cast<intptr_t>(Position),now,0,reinterpret_cast<intptr_t>(Zero)})==1;
}
// jk2mp.exe 0x4a76d0..0x4a7827 re-derived in double: world = AnglesToAxis
// columns and the position, times the kept matrix unnormalized; the rotation
// alone turned by yaw 270, (x, y, z) -> (-y, x, z); the translation kept.
static std::array<double,12> Placed(const pose::Matrix &kept){
 const double r=3.14159265358979323846/180,p=Angles[0]*r,y=Angles[1]*r,l=Angles[2]*r;
 const double sp=std::sin(p),cp=std::cos(p),sy=std::sin(y),cy=std::cos(y),sr=std::sin(l),cr=std::cos(l);
 const double w[12]{cp*cy,sr*sp*cy-cr*sy,cr*sp*cy+sr*sy,Position[0],cp*sy,sr*sp*sy+cr*cy,cr*sp*sy-sr*cy,Position[1],
  -sp,sr*cp,cr*cp,Position[2]};
 std::array<double,12> out{};
 for(int row=0;row<3;++row){
  double m[4];
  for(int c=0;c<4;++c)m[c]=w[row*4]*kept[c]+w[row*4+1]*kept[4+c]+w[row*4+2]*kept[8+c]+(c==3?w[row*4+3]:0);
  out[row*4]=-m[1];out[row*4+1]=m[0];out[row*4+2]=m[2];out[row*4+3]=m[3];
 }
 return out;
}
static void CheckQVM(){
 gvm=&qvm;
 const char tag[]="root",root[]="model_root",child[]="child";
 Put(BodyName,Body,sizeof(Body));Put(WeaponName,Weapon,sizeof(Weapon));Put(TagName,tag,sizeof(tag));
 Put(RootName,root,sizeof(root));Put(ChildName,child,sizeof(child));Put(DefaultName,DefaultBone,sizeof(DefaultBone));
 Put(AnglesAt,Angles,12);Put(PositionAt,Position,12);Put(ZeroAt,Zero,12);Put(BendAt,Bend,12);
 // 588, as the retail module calls it: the slot and the name are offsets.
 CHECK(Call({588,BodySlot,BodyName,0,0,-20,0,0})==0&&Call({588,WeaponSlot,WeaponName,0,0,-20,0,0})==0);
 const intptr_t body=Word(BodySlot),weapon=Word(WeaponSlot);
 CHECK(body&&weapon&&body!=weapon);
 // The word the module keeps is the handle. Converted as an address, as 584
 // once converted it, it names nothing, and the retail module's client setup
 // stopped there; read as retail's handler reads it, 584 finds the models.
 CHECK(!ravenGhoul2Outcast.Resolve(VM_ArgPtr(body))&&ravenGhoul2Outcast.Resolve(SV_JK2Ghoul2Handle(body)));
 CHECK(Call({584,body})==1&&Call({584,0})==0);
 // 589 finds the body's tag and root bone and, on the engine skeleton, the
 // weapon's tag and bone; 590 links the weapon to the body's tag; 595 puts it
 // into the body's second place, as the retail module does its lightsaber.
 // Both handlers return 0 whatever their cores did.
 CHECK(Call({589,body,0,TagName})==0&&Call({589,body,0,RootName})==1);
 CHECK(Call({590,weapon,0,0})==0&&ravenGhoul2Outcast.Resolve(SV_JK2Ghoul2Handle(weapon))->At(0)->boltInfo==0);
 CHECK(Call({589,weapon,0,TagName})==0&&Call({589,weapon,0,DefaultName})==1);
 CHECK(Call({595,weapon,0,body,1})==0&&ravenGhoul2Outcast.Resolve(SV_JK2Ghoul2Handle(body))->Count()==2);
 // 586 at 100: the weapon's root is placed exactly where the body's tag is.
 float tagAt[12],boneAt[12],rootAt[12];
 CHECK(Query(586,body,0,0,100,tagAt)&&Query(586,body,0,1,100,boneAt)&&Query(586,body,1,1,100,rootAt));
 CHECK(!std::memcmp(tagAt,rootAt,48));
 // Outcast's placement of the kept matrices. The skeleton's bind scale of .5
 // stays in the bone's rows: nothing is normalized, as Academy's engine does.
 for(int bolt=0;bolt<2;++bolt){
  const fntech3::ghoul2::Bolt *kept=ravenGhoul2Outcast.BoltAt(SV_JK2Ghoul2Handle(body),0,bolt);
  CHECK(kept&&kept->kept);
  const auto expect=Placed(kept->matrix);const float *got=bolt?boneAt:tagAt;
  for(int n=0;n<12;++n)CHECK(std::fabs(got[n]-expect[n])<.0002);
 }
 for(int row=0;row<3;++row){
  const double length=std::sqrt(double(boneAt[row*4])*boneAt[row*4]+double(boneAt[row*4+1])*boneAt[row*4+1]
   +double(boneAt[row*4+2])*boneAt[row*4+2]);
  CHECK(std::fabs(length-.5)<.0001);
 }
 // 591 bends model 0 and its build time with it: seen at the same time.
 CHECK(Call({591,body,0,RootName,BendAt,2,1,6,5,0,0,100})==1);
 float bent[12],out[12];CHECK(Query(586,body,0,0,100,bent)&&std::memcmp(bent,tagAt,48));
 // Cleared again at 150: 587 answers the last build, 586 builds.
 CHECK(Call({591,body,0,RootName,ZeroAt,0,1,6,5,0,0,150})==1);
 CHECK(Query(587,body,0,0,150,out)&&!std::memcmp(out,bent,48));
 CHECK(Query(586,body,0,0,150,out)&&!std::memcmp(out,tagAt,48));
 // Refusals write nothing: a bolt out of range, one no build has reached,
 // and the weapon's own instance, whose only model links to itself.
 CHECK(!Query(586,body,0,7,150,out)&&Untouched(out));
 CHECK(Call({589,body,0,ChildName})==2&&!Query(586,body,0,2,150,out)&&Untouched(out));
 CHECK(Query(586,body,0,2,175,out)&&!Query(586,weapon,0,0,175,out)&&Untouched(out));
 for(intptr_t word:{body,weapon}){void *handle=SV_JK2Ghoul2Handle(word);CHECK(ravenGhoul2Outcast.Clean(&handle));}
}
// The retail module's client setup (QVM procedure 108882) and its per-frame
// clips (procedure 235883), through the production cases. Its fields are
// 32-bit words: on any host, the guard word after each must survive.
static void CheckQVMClientSetup(){
 gvm=&qvm;
 auto &store=ravenGhoul2Outcast;
 const std::uint32_t guard=0xa5a5a5a5u,empty=0;
 const intptr_t fields[]{KyleField,SaberField,ClientField,OtherField};
 for(intptr_t field:fields){Put(field,&empty,4);Put(field+4,&guard,4);}
 const auto guarded=[&]{for(intptr_t field:fields)if(Word(field+4)!=intptr_t(guard))return false;return true;};
 const auto instance=[&](intptr_t word){return store.Resolve(SV_JK2Ghoul2Handle(word));};
 // worldspawn precaches the Kyle and the saber with 588.
 CHECK(Call({588,KyleField,BodyName,0,0,-20,0,0})==0&&Call({588,SaberField,WeaponName,0,0,-20,0,0})==0&&guarded());
 const intptr_t kyle=Word(KyleField),saber=Word(SaberField);
 CHECK(kyle&&saber&&kyle!=saber);
 // The setup: 599 leaves the client's empty field alone, 584 finds the
 // Kyle's models, and 596 copies the Kyle, records whole, into the field.
 CHECK(Call({599,ClientField})==0&&!Word(ClientField)&&Call({584,kyle})==1);
 CHECK(Call({596,kyle,ClientField})==0&&guarded());
 const intptr_t client=Word(ClientField);
 CHECK(client&&client!=kyle&&client!=saber&&instance(client)->Count()==1&&instance(client)->At(0)->name==Body);
 // A field already holding an instance is left as it is.
 CHECK(Call({596,saber,ClientField})==0&&Word(ClientField)==client&&instance(client)->Count()==1);
 // 593 copies the skeleton name the mesh header carries, and nothing for a
 // model past the count or no instance.
 const char *name=reinterpret_cast<const char *>(memory+GLABuffer);
 CHECK(Call({593,client,0,GLABuffer})==0&&!std::strcmp(name,"models/players/test/skeleton"));
 const char unchanged[]="unchanged";Put(GLABuffer,unchanged,sizeof(unchanged));
 CHECK(Call({593,client,1,GLABuffer})==0&&Call({593,0,0,GLABuffer})==0&&!std::strcmp(name,unchanged));
 // The hand bolt, model_root looping from frame 0 with no transition (blend
 // time -1), a bend, and the saber linked to the hand and copied in.
 CHECK(Call({589,client,0,TagName})==0&&Call({589,client,0,RootName})==1);
 CHECK(Call({592,client,0,RootName,0,2,16,FloatWord(1),100,FloatWord(-1),-1})==1);
 CHECK(Call({592,client,0,ChildName,0,2,16,FloatWord(1),100,FloatWord(-1),-1})==1);
 CHECK(Call({592,client,0,ChildName+1,0,2,16,FloatWord(1),100,FloatWord(-1),-1})==0);
 CHECK(Call({591,client,0,RootName,BendAt,2,1,6,5,0,0,100})==1);
 CHECK(Call({590,saber,0,0})==0&&Call({589,saber,0,TagName})==0&&Call({595,saber,0,client,1})==0);
 // The same calls made on the store directly keep the same bolts: 592's ten
 // arguments reach it in retail's order.
 void *twin=nullptr;LoadError error;
 CHECK(store.AddModel(&twin,Body,error)==0&&store.AddBolt(twin,0,"root")==0&&store.AddBolt(twin,0,"model_root")==1);
 CHECK(store.SetBoneAnim(twin,0,"model_root",0,2,16,1.0f,100,-1.0f,-1)&&store.SetBoneAnim(twin,0,"child",0,2,16,1.0f,100,-1.0f,-1));
 CHECK(store.SetBoneAngles(twin,0,"model_root",Bend,2,1,6,5,0,100));
 float out[12],direct[12],first[12];
 const auto agree=[&](int bolt,int now){
  return Query(586,client,0,bolt,now,out)&&store.KeptBolt(twin,0,bolt,now,false,direct)==fntech3::ghoul2::Posing::Served
   &&!std::memcmp(store.BoltAt(SV_JK2Ghoul2Handle(client),0,bolt)->matrix.data(),store.BoltAt(twin,0,bolt)->matrix.data(),48);};
 CHECK(agree(1,100)&&agree(0,100));std::memcpy(first,out,48);
 CHECK(agree(1,175)&&agree(0,175)&&std::memcmp(first,out,48));
 // Per frame, as procedure 235883 sets them: model_root one frame frozen
 // with a 150 ms transition from where its loop had got to; child running
 // backwards, frozen, its set frame its start. 586, 587 and the saber's
 // bolt answer through them, refusing nothing.
 CHECK(Call({592,client,0,RootName,1,2,200,FloatWord(1),300,FloatWord(-1),150})==1);
 CHECK(Call({592,client,0,ChildName,0,1,200,FloatWord(-1),300,FloatWord(0),150})==1);
 CHECK(store.SetBoneAnim(twin,0,"model_root",1,2,200,1.0f,300,-1.0f,150)&&store.SetBoneAnim(twin,0,"child",0,1,200,-1.0f,300,0.0f,150));
 const fntech3::ghoul2::BoneAnimation *root=store.BoneAnimation(SV_JK2Ghoul2Handle(client),0,"model_root");
 CHECK(root&&root->flags==200&&root->blendStart==300&&root->blendTime==150&&root->blendFrame==0&&root->blendLerpFrame==1);
 for(int now:{300,375,450,500}){
  CHECK(agree(0,now)&&agree(1,now)&&Query(587,client,0,1,now,out)&&Query(586,client,1,0,now,out));
 }
 // 599: the client's field was listed by 596, so its instance goes; a
 // field holding the Kyle's word unlisted is only emptied; the Kyle's own,
 // listed by 588, frees it.
 CHECK(Call({599,ClientField})==0&&!Word(ClientField)&&!instance(client)&&guarded());
 const std::uint32_t kyleWord=std::uint32_t(kyle);Put(OtherField,&kyleWord,4);
 CHECK(Call({599,OtherField})==0&&!Word(OtherField)&&instance(kyle));
 CHECK(Call({599,KyleField})==0&&!Word(KyleField)&&!instance(kyle));
 CHECK(Call({599,SaberField})==0&&!instance(saber)&&guarded()&&store.Clean(&twin));
}
// A native module passes real pointers, and the same answers come back.
static void CheckNative(){
 gvm=&native;
 void *body=nullptr,*weapon=nullptr;
 const auto at=[](const void *pointer){return reinterpret_cast<intptr_t>(pointer);};
 CHECK(Call({588,at(&body),at(Body),0,0,-20,0,0})==0&&Call({588,at(&weapon),at(Weapon),0,0,-20,0,0})==0);
 CHECK(Call({584,at(body)})==1);
 CHECK(Call({589,at(body),0,at("root")})==0&&Call({589,at(weapon),0,at(DefaultBone)})==0);
 CHECK(Call({590,at(weapon),0,0})==0&&Call({595,at(weapon),0,at(body),1})==0);
 float tagAt[12],rootAt[12];
 CHECK(Query(586,at(body),0,0,100,tagAt)&&Query(586,at(body),1,0,100,rootAt)&&!std::memcmp(tagAt,rootAt,48));
 CHECK(Call({591,at(body),0,at("model_root"),at(Bend),2,1,6,5,0,0,100})==1);
 float bent[12];CHECK(Query(587,at(body),0,0,100,bent)&&!std::memcmp(bent,tagAt,48));
 CHECK(Query(586,at(body),0,0,100,bent)&&std::memcmp(bent,tagAt,48));
 // A pointer-sized field: duplicated into, clipped, named, cleaned.
 void *copy=nullptr;CHECK(Call({596,at(body),at(&copy)})==0&&copy&&copy!=body);
 CHECK(Call({592,at(copy),0,at("model_root"),0,2,0x10,FloatWord(1),100,FloatWord(-1),0})==1);
 char skeleton[64]{};CHECK(Call({593,at(copy),0,at(skeleton)})==0&&!std::strcmp(skeleton,"models/players/test/skeleton"));
 CHECK(Query(586,at(copy),0,0,175,bent)&&Call({599,at(&copy)})==0&&!copy);
 CHECK(ravenGhoul2Outcast.Clean(&body)&&ravenGhoul2Outcast.Clean(&weapon));
}
int main(){
 installed[Body]=AuthoredTagMesh("models/players/test/skeleton",3,BodyTag);
 installed["models/players/test/skeleton.gla"]=PoseSkeleton();
 installed[Weapon]=AuthoredTagMesh("*default",1,WeaponTag);
 CheckQVM();CheckQVMClientSetup();CheckNative();
 // A map change empties Outcast's store; nothing it held answers afterwards.
 void *left=nullptr;LoadError error;
 CHECK(ravenGhoul2Outcast.AddModel(&left,Body,error)==0&&ravenGhoul2Outcast.AddBolt(left,0,"root")==0);
 float out[12];CHECK(ravenGhoul2Outcast.KeptBolt(left,0,0,100,false,out)==fntech3::ghoul2::Posing::Served);
 SV_RavenAssetsClear();
 CHECK(!ravenGhoul2Outcast.InstanceCount()&&ravenGhoul2Outcast.KeptBolt(left,0,0,200,false,out)!=fntech3::ghoul2::Posing::Served);
 std::cout<<"Jedi Outcast server bolt imports passed\n";
 return 0;
}
'''


class JKAServerAttachmentTests(unittest.TestCase):
    def test_each_dispatcher_binds_its_titles_store(self):
        self.assertEqual(binding('code/server/server_jka_game.hpp', 'static bool SV_JKAGameCall('),
                         'ravenGhoul2Academy')
        self.assertEqual(binding('code/server/server_jk2_game.hpp', 'static bool SV_JK2GameCall('),
                         'ravenGhoul2Outcast')
        assets = (ROOT / 'code/server/server_raven_assets.hpp').read_text(encoding='utf-8')
        # No shared store is left for a third consumer to pick up by accident.
        self.assertNotRegex(assets, r'Store ravenGhoul2\(')

    def test_academy_serves_attachments_and_outcast_keeps_builds(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler unavailable')
        scratch = ROOT / '.tmp/tests/jka-server-attachment'
        scratch.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=scratch) as folder:
            directory = Path(folder)
            source = directory / 'host.cpp'
            source.write_text(fixture(), encoding='utf-8')
            for fast in (False, True):
                with self.subTest(fast_math=fast):
                    executable = directory / (('fast' if fast else 'plain') + ('.exe' if os.name == 'nt' else ''))
                    built = subprocess.run([compiler, *compiler_target_args(compiler), '-std=c++17', '-O2',
                                            '-Wall', '-Wextra', '-Werror', '-D_CRT_SECURE_NO_WARNINGS',
                                            *(['-ffast-math'] if fast else []), '-I', str(ROOT),
                                            '-I', str(ROOT / 'tests'), str(source), '-o', str(executable)],
                                           cwd=ROOT, capture_output=True, text=True, timeout=180)
                    self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
                    run = subprocess.run([str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=30)
                    self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
                    self.assertIn('Raven server attachment stores passed', run.stdout)

    def test_outcast_serves_its_bolt_imports_setup_and_clips(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler unavailable')
        scratch = ROOT / '.tmp/tests/jka-server-attachment'
        scratch.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=scratch) as folder:
            directory = Path(folder)
            source = directory / 'outcast.cpp'
            source.write_text(outcast_fixture(), encoding='utf-8')
            for fast in (False, True):
                with self.subTest(fast_math=fast):
                    executable = directory / (('fast' if fast else 'plain') + ('.exe' if os.name == 'nt' else ''))
                    built = subprocess.run([compiler, *compiler_target_args(compiler), '-std=c++17', '-O2',
                                            '-Wall', '-Wextra', '-Werror', '-D_CRT_SECURE_NO_WARNINGS',
                                            *(['-ffast-math'] if fast else []), '-I', str(ROOT),
                                            '-I', str(ROOT / 'tests'), str(source), '-o', str(executable)],
                                           cwd=ROOT, capture_output=True, text=True, timeout=180)
                    self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
                    run = subprocess.run([str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=30)
                    self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
                    self.assertIn('Jedi Outcast server bolt imports passed', run.stdout)


# Retail jamp.exe 1.01's dedicated server under fixedtime 75 and sv_fps 20,
# read from its Ghoul2 clock (0x10bcb74) by a synthetic game module in every
# game frame after the level's spawn left the clock at 400: (level time the
# frame was given, the clock its bolt queries pose at). One server frame runs
# one game frame, the next two. Then a map_restart's four frames, which set no
# clock, and four more server frames. docs/fntech3/2026-10-07-jka-server-pose-clock.json
RETAIL_FRAME_CLOCKS = ((450, 400), (500, 450), (550, 450), (600, 550), (650, 600), (700, 600), (750, 700),
                       (800, 750), (850, 750), (900, 850), (950, 900), (1000, 900), (1050, 1000), (1100, 1050),
                       (1150, 1050), (1200, 1150), (1250, 1200), (1300, 1200), (1350, 1300), (1400, 1350),
                       (1450, 1350), (1500, 1450),
                       (1500, 1500), (1600, 1500), (1700, 1500), (1800, 1500),
                       (1950, 1500), (2000, 1500), (2050, 2000), (2100, 2050), (2150, 2050), (2200, 2150))
CLOCK_CALL = 'SV_JKAGhoul2SetTime( sv.time );'


def read(relative):
    return (ROOT / relative).read_text(encoding='utf-8').replace('\r\n', '\n')


def frame_loop():
    """SV_Frame's production run of game frames and the clock set after it."""
    frame = function(read('code/server/sv_main.cpp'), 'void SV_Frame( int msec, int realMsec ) {')
    start = frame.index('\t// run the game simulation in chunks\n')
    end = frame.index(CLOCK_CALL, start) + len(CLOCK_CALL)
    return frame[start:end]


FRAME_HARNESS = r'''
#include <cstdio>
enum { GAME_RUN_FRAME = 8 };
static struct { int time, timeResidual; } sv;
static int *gvm, poseClock = -1;
static void VM_Call(int *, int count, int call, int time) {
 if (count == 1 && call == GAME_RUN_FRAME) std::printf("%d %d\n", time, poseClock);
}
static void SV_JKAGhoul2SetTime(int time) { poseClock = time; }
static void Frame(int msec) {
 const int frameMsec = 50;
 sv.timeResidual += msec;
LOOP
}
int main() {
 sv.time = poseClock = 400;
 for (int n = 0; n < 15; ++n) Frame(75);
 // A map_restart's frames, 100 ms apart; neither engine's restart sets the
 // clock (test_the_spawn_sets_the_clock_where_retail_does).
 for (int n = 0; n < 4; ++n) { VM_Call(gvm, 1, GAME_RUN_FRAME, sv.time); sv.time += 100; }
 for (int n = 0; n < 4; ++n) Frame(75);
 return 0;
}
'''


class JKAServerPoseClockTests(unittest.TestCase):
    """The clock Academy's server poses bolts at is set where retail's is."""

    def test_server_frames_pose_at_the_clock_retail_measured(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler unavailable')
        scratch = ROOT / '.tmp/tests/jka-server-attachment'
        scratch.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=scratch) as folder:
            source = Path(folder) / 'frames.cpp'
            executable = Path(folder) / ('frames.exe' if os.name == 'nt' else 'frames')
            source.write_text(FRAME_HARNESS.replace('LOOP', frame_loop()), encoding='utf-8')
            built = subprocess.run([compiler, *compiler_target_args(compiler), '-std=c++17', '-Wall', '-Wextra',
                                    '-Werror', str(source), '-o', str(executable)],
                                   cwd=ROOT, capture_output=True, text=True, timeout=120)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            run = subprocess.run([str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=30)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
            frames = tuple(tuple(int(word) for word in line.split()) for line in run.stdout.splitlines())
            self.assertEqual(frames, RETAIL_FRAME_CLOCKS)

    def test_the_spawn_sets_the_clock_where_retail_does(self):
        spawn = function(read('code/server/sv_init.cpp'), 'void SV_SpawnServer( const char *mapname, qboolean killBots ) {')
        self.assertEqual(spawn.count(CLOCK_CALL), 3)
        # Before the game starts (jamp.exe 0x45e9e8), after the configstrings.
        first = spawn.index(CLOCK_CALL)
        self.assertLess(spawn.index('sv.configstrings[index] = CopyString("");'), first)
        self.assertLess(first, spawn.index('SV_InitGameProgs();'))
        # After each of the three settling frames' advance: the clock is the
        # level time at every frame and after the last (0x45eb0e, 0x45eb6a).
        loop = spawn[spawn.index('for ( int ignored : SV_Indices( 3 ) ) {'):]
        loop = loop[:loop.index('\n\t\t}\n')]
        self.assertLess(loop.index('sv.time += 100;'), loop.index(CLOCK_CALL))
        # After the last frame's advance (0x45ecf0).
        last = spawn[spawn.index('// 0x44db99..0x44dbc8).'):]
        last = last[:last.index('\n\t}\n')]
        self.assertLess(last.index('svs.time += 100;'), last.index(CLOCK_CALL))
        # jamp.exe's map_restart sets no clock.
        self.assertNotIn('SV_JKAGhoul2SetTime', function(read('code/server/sv_ccmds.cpp'),
                                                         'static void SV_MapRestart_f( void ) {'))
        # One definition, which sets nothing for another profile.
        setter = function(read('code/server/server_jka_game.hpp'), 'void SV_JKAGhoul2SetTime( int time )')
        self.assertIn('if ( Com_GameIsJKAMP() ) jkaServerGhoul2Clock = time;', setter)
        self.assertIn('void SV_JKAGhoul2SetTime( int time );', read('code/server/server.h'))


if __name__ == '__main__':
    unittest.main()
