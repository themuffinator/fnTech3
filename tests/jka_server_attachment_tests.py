"""The Raven servers' own Ghoul2 stores: Jedi Academy's serves the engine
`*default` skeleton, attached models and retail's tag frame through its real
game imports; Jedi Outcast's keeps its earlier store."""
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
// Outcast's server keeps the earlier store: `*default` stays metadata-only,
// links are kept and not posed through, tags keep this engine's earlier frame.
static void CheckOutcast(){
 auto &store=ravenGhoul2Outcast;
 CHECK(!store.ServesAttachments());
 void *weapon=nullptr;LoadError error;
 CHECK(store.AddModel(&weapon,Weapon,error)==0&&store.AddBolt(weapon,0,DefaultBone)==-1);
 float out[12];CHECK(!store.BoltMatrix(weapon,0,0,100,out));
 void *body=nullptr;
 CHECK(store.AddModel(&body,Body,error)==0&&store.AddModel(&body,Body,error)==1);
 CHECK(store.AddBolt(body,0,"root")==0&&store.AddBolt(body,1,"root")==0&&store.SetBoltInfo(body,1,0));
 float own[12],linked[12];CHECK(store.BoltMatrix(body,0,0,100,own)&&store.BoltMatrix(body,1,0,100,linked));
 CHECK(!std::memcmp(own,linked,48));
 pose::Palette palette;CHECK(store.DeformationPalette(*store.Resolve(body)->At(0),100,palette));
 double first[3];Skin(palette.deformation[0],BodyTag[1],first);
 for(int r=0;r<3;++r)CHECK(std::fabs(own[r*4+3]-first[r])<.00002);
 CHECK(store.Clean(&weapon)&&store.Clean(&body));
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


class JKAServerAttachmentTests(unittest.TestCase):
    def test_each_dispatcher_binds_its_titles_store(self):
        self.assertEqual(binding('code/server/server_jka_game.hpp', 'static bool SV_JKAGameCall('),
                         'ravenGhoul2Academy')
        self.assertEqual(binding('code/server/server_jk2_game.hpp', 'static bool SV_JK2GameCall('),
                         'ravenGhoul2Outcast')
        assets = (ROOT / 'code/server/server_raven_assets.hpp').read_text(encoding='utf-8')
        # No shared store is left for a third consumer to pick up by accident.
        self.assertNotRegex(assets, r'Store ravenGhoul2\(')

    def test_academy_serves_attachments_and_outcast_keeps_its_store(self):
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


if __name__ == '__main__':
    unittest.main()
