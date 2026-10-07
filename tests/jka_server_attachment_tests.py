"""The Raven servers' own Ghoul2 stores: Jedi Academy's serves the engine
`*default` skeleton, attached models and retail's tag frame through its real
game imports; Jedi Outcast's serves the same posing through one build per time,
answering its bolt queries 586 and 587 from what that build kept, as
jk2mp.exe 1.02a does."""
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
    start = host.index('\tcase Call::G_G2_SETBOLTINFO:')
    end = host.index('\n\tcase Call::G_G2_ADDBOLT:', start)
    academy = binding('code/server/server_jka_game.hpp', 'static bool SV_JKAGameCall(')
    vm = (ROOT / 'code/qcommon/vm.c').read_text(encoding='utf-8')
    tests = (ROOT / 'tests/jka_pose_tests.cpp').read_text(encoding='utf-8').replace(
        'int main(int argc,char **argv)', 'int PoseUnitFixtureMain(int argc,char **argv)')
    return tests + r'''
#include "code/compat/jka_module_abi.h"
#include <cstdlib>
#include <initializer_list>
#include <stdexcept>
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
static bool DispatchJKA(const intptr_t *args,intptr_t &result){
 using namespace fntech3::jka::module;
 fntech3::ghoul2::Store &ravenGhoul2 = ''' + academy + r''';
 const Call call=Lookup(COM_GAME_JKA_MP,Module::GAME,Direction::IMPORT,args[0]);
 const auto pointer=[args](int index){return VM_ExplicitArgPtr(gvm,args[index]);};
 switch(call){
''' + host[start:end] + r'''
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
 // Animation blends stay a client-game capability: a measured forward
 // transition, which the client game's store serves, refuses here.
 CHECK(store.SetBoneAnim(body,0,"child",0,2,0x10,1.0f,60,-1.0f,0)&&Bolt(body,0,0,out));
 CHECK(store.SetBoneAnim(body,0,"child",1,2,0x10|0x80,1.0f,90,-1.0f,50));
 CHECK(!Bolt(body,0,0,out)&&Untouched(out));
 for(void *&weapon:weapons)CHECK(store.Clean(&weapon)&&!weapon);
 CHECK(store.Clean(&body)&&!body);
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
 // Bends PREMULT and POSTMULT at any blend time; REPLACE and clips refuse.
 CHECK(store.SetBoneAngles(body,0,"model_root",bend,2,1,6,5,100,300)&&Kept(body,0,0,300,false,out));
 CHECK(!std::memcmp(out,bentTag,48));
 CHECK(store.SetBoneAngles(body,0,"model_root",bend,4,1,6,5,0,350)&&!Kept(body,0,0,350,false,out)&&Untouched(out));
 CHECK(store.SetBoneAngles(body,0,"model_root",bend,2,1,6,5,0,400)&&Kept(body,0,0,400,false,out));
 CHECK(store.SetBoneAnim(body,0,"child",0,2,0x10,1.0f,400,-1.0f,0)&&!Kept(body,0,0,450,false,out)&&Untouched(out));
 // Academy's store answers on demand and keeps no build.
 CHECK(academy.KeptBolt(twin,0,0,100,false,out)==fntech3::ghoul2::Posing::Unserved&&Untouched(out));
 for(void **handle:{&body,&weapon})CHECK(store.Clean(handle));
 for(void **handle:{&twin,&twinWeapon})CHECK(academy.Clean(handle));
}
int main(){
 installed[Body]=AuthoredTagMesh("models/players/test/skeleton",3,BodyTag);
 installed["models/players/test/skeleton.gla"]=PoseSkeleton();
 installed[Weapon]=AuthoredTagMesh("*default",1,WeaponTag);
 CheckAcademy();CheckOutcast();
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
    reading, driven as the retail QVM drives them and as a native module would."""
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
''' + numbers + '\n' + function(host, 'static void *SV_JK2Ghoul2Handle(') + r'''
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
 BendAt=0x440,MatrixAt=0x500 };
static void Put(intptr_t at,const void *bytes,std::size_t size){std::memcpy(memory+at,bytes,size);}
static intptr_t Word(intptr_t at){std::uint32_t word;std::memcpy(&word,memory+at,4);return intptr_t(word);}
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
 // still converts it, it names nothing: pinned on purpose until 592, 593,
 // 596 and 599 are served (docs/fntech3/COMPATIBILITY.md).
 CHECK(!ravenGhoul2Outcast.Resolve(VM_ArgPtr(body))&&ravenGhoul2Outcast.Resolve(SV_JK2Ghoul2Handle(body)));
 CHECK(Call({584,body})==0);
 // 589 finds the body's tag and root bone and, on the engine skeleton, the
 // weapon's tag and bone; 590 links the weapon to the body's tag; 595 puts it
 // into the body's second place, as the retail module does its lightsaber.
 CHECK(Call({589,body,0,TagName})==0&&Call({589,body,0,RootName})==1);
 CHECK(Call({590,weapon,0,0})==1);
 CHECK(Call({589,weapon,0,TagName})==0&&Call({589,weapon,0,DefaultName})==1);
 CHECK(Call({595,weapon,0,body,1})==1);
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
// A native module passes real pointers, and the same answers come back.
static void CheckNative(){
 gvm=&native;
 void *body=nullptr,*weapon=nullptr;
 const auto at=[](const void *pointer){return reinterpret_cast<intptr_t>(pointer);};
 CHECK(Call({588,at(&body),at(Body),0,0,-20,0,0})==0&&Call({588,at(&weapon),at(Weapon),0,0,-20,0,0})==0);
 CHECK(Call({584,at(body)})==1);
 CHECK(Call({589,at(body),0,at("root")})==0&&Call({589,at(weapon),0,at(DefaultBone)})==0);
 CHECK(Call({590,at(weapon),0,0})==1&&Call({595,at(weapon),0,at(body),1})==1);
 float tagAt[12],rootAt[12];
 CHECK(Query(586,at(body),0,0,100,tagAt)&&Query(586,at(body),1,0,100,rootAt)&&!std::memcmp(tagAt,rootAt,48));
 CHECK(Call({591,at(body),0,at("model_root"),at(Bend),2,1,6,5,0,0,100})==1);
 float bent[12];CHECK(Query(587,at(body),0,0,100,bent)&&!std::memcmp(bent,tagAt,48));
 CHECK(Query(586,at(body),0,0,100,bent)&&std::memcmp(bent,tagAt,48));
 CHECK(ravenGhoul2Outcast.Clean(&body)&&ravenGhoul2Outcast.Clean(&weapon));
}
int main(){
 installed[Body]=AuthoredTagMesh("models/players/test/skeleton",3,BodyTag);
 installed["models/players/test/skeleton.gla"]=PoseSkeleton();
 installed[Weapon]=AuthoredTagMesh("*default",1,WeaponTag);
 CheckQVM();CheckNative();
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

    def test_outcast_serves_its_bolt_imports_from_kept_builds(self):
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


if __name__ == '__main__':
    unittest.main()
