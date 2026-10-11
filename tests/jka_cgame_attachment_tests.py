"""Academy multi-model Ghoul2 instances: links, retail tag frames, the Fn scene."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args
from jka_cgame_render_tests import fixture as scene_fixture

ROOT=Path(__file__).resolve().parents[1]


def fixture():
    prefix=scene_fixture().split('int main(){')[0]
    # A weapon on the generated *default skeleton registers one bone.
    assert prefix.count('view.numBones==3')==1 and prefix.count('count==3')==1
    prefix=prefix.replace('view.numBones==3','(view.numBones==3||view.numBones==1)')
    prefix=prefix.replace('count==3','(count==3||count==1)')
    return prefix+r'''
using ghoul2::Posing;
namespace link=ghoul2::link;
// A one-surface mesh whose first three vertices are an authored tag.
static Bytes TagMesh(const char *skeleton,int bones,const float (&points)[3][3]){
 auto bytes=Mesh(skeleton,bones);model::Mesh parsed;model::MeshSurface surface;
 assert(model::ParseMesh(bytes.data(),bytes.size(),bones,parsed)==model::Error::None);
 assert(model::SurfaceAt(bytes.data(),bytes.size(),parsed,0,0,surface));
 for(int v=0;v<3;++v)for(int n=0;n<3;++n)GeometryFloat(bytes,surface.vertexOffset+v*32+12+n*4,points[v][n]);
 // Its first triangle starts elsewhere: retail reads the vertices, not it.
 const int corners[3]{1,2,0};for(int c=0;c<3;++c)PutInt(bytes,surface.triangleOffset+c*4,corners[c]);
 return bytes;
}
static const float BodyTag[3][3]{{1,2,3},{4,2,3.5f},{1,-1,5}};
static const float WeaponTag[3][3]{{6,0,0},{6,3,0},{6,0,-2}};
// The retail recipe re-derived in double: columns |p0-p2|, the orthogonalized
// |p1-p0| and -|(p1-p0)x(p0-p2)|; origin p2.
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
static void Skin(const pose::Matrix &m,const float in[3],double out[3]){
 for(int r=0;r<3;++r)out[r]=double(m[r*4])*in[0]+double(m[r*4+1])*in[1]+double(m[r*4+2])*in[2]+m[r*4+3];
}
static bool Near(const float *a,const float *b,float tolerance=.00002f){
 for(int n=0;n<12;++n)if(!(std::fabs(a[n]-b[n])<=tolerance))return false;return true;
}
static void CheckLinksAndOrder(){
 // Only the masked bits are read: 10 bits of bolt, 10 of model, whatever
 // the word's sign or entity bits.
 assert(link::ParentModel(link::None)==0x3ff&&link::ParentBolt(link::None)==0x3ff);
 assert(link::ParentModel((3<<10)|7)==3&&link::ParentBolt((3<<10)|7)==7);
 assert(link::ParentModel((5<<20)|(2<<10)|1)==2&&link::ParentBolt(INT_MIN)==0&&link::ParentModel(INT_MIN)==0);
 TestFiles files;files.files["models/w.glm"]=TagMesh("*default",1,WeaponTag);
 ghoul2::Store store(files,true,true,true);void *slot=nullptr;LoadError error;
 for(int n=0;n<4;++n)assert(store.AddModel(&slot,"models/w.glm",error)==n);
 const auto &instance=*store.Resolve(slot);std::vector<int> order;
 for(int n=0;n<4;++n)assert(instance.At(n)->boltInfo==link::None);
 ghoul2::PoseOrder(instance,order);assert(order==std::vector<int>({0,1,2,3}));
 // Unlinked models by index, then each round's children by index.
 assert(store.SetBoltInfo(slot,0,(2<<10))&&store.SetBoltInfo(slot,3,0));
 ghoul2::PoseOrder(instance,order);assert(order==std::vector<int>({1,2,0,3}));
 // A self-link, a two-cycle and a missing parent are never posed.
 assert(store.SetBoltInfo(slot,3,(3<<10)));ghoul2::PoseOrder(instance,order);assert(order==std::vector<int>({1,2,0}));
 assert(store.SetBoltInfo(slot,1,(0<<10))&&store.SetBoltInfo(slot,0,(1<<10)));
 ghoul2::PoseOrder(instance,order);assert(order==std::vector<int>({2}));
 assert(store.SetBoltInfo(slot,0,link::None)&&store.SetBoltInfo(slot,1,(9<<10)));
 ghoul2::PoseOrder(instance,order);assert(order==std::vector<int>({0,2}));
 // Copies and duplicates carry the link, as retail's record assignment does.
 void *other=nullptr;assert(store.AddModel(&other,"models/w.glm",error)==0&&store.SetBoltInfo(other,0,(1<<10)|4));
 assert(store.CopyModel(other,0,slot,2)&&instance.At(2)->boltInfo==((1<<10)|4));
 void *copy=nullptr;assert(store.Duplicate(slot,&copy)&&store.Resolve(copy)->At(2)->boltInfo==((1<<10)|4));
}
static void CheckTagFrame(){
 // Hand-solved frames: the second also needs the orthogonalizing step.
 const float square[3][3]{{0,0,0},{2,0,0},{0,0,-1}},skew[3][3]{{0,0,0},{2,0,1},{0,0,-2}};
 const float expected[2][12]{{0,1,0,0, 0,0,1,0, 1,0,0,-1},{0,1,0,0, 0,0,1,0, 1,0,0,-2}};
 float out[12];
 assert(ghoul2::TagFrame(square,out)&&Near(out,expected[0],0));
 assert(ghoul2::TagFrame(skew,out)&&Near(out,expected[1],.000001f));
 // A collapsed tag has no frame and leaves the output alone.
 const float collapsed[3][3]{{1,1,1},{1,1,1},{2,2,2}};float kept[12];for(float &v:kept)v=-7;std::memcpy(out,kept,48);
 assert(!ghoul2::TagFrame(collapsed,out)&&!std::memcmp(out,kept,48));
}
static void CheckPosing(){
 TestFiles files;
 files.files["models/body.glm"]=TagMesh("models/players/test/skeleton",3,BodyTag);
 files.files["models/players/test/skeleton.gla"]=PoseSkeleton();
 files.files["models/w.glm"]=TagMesh("*default",1,WeaponTag);
 ghoul2::Store store(files,true,true,true),earlier(files,true,true,false);
 for(auto *owner:{&store,&earlier}){
  void *body=nullptr;LoadError error;
  assert(owner->AddModel(&body,"models/body.glm",error)==0);
  assert(owner->AddBolt(body,0,"root")==0&&owner->AddBolt(body,0,"child")==1);
  for(int slot=1;slot<=3;++slot){void *weapon=nullptr;
   assert(owner->AddModel(&weapon,"models/w.glm",error)==0);
   assert(owner->AddBolt(weapon,0,"root")==0&&owner->AddBolt(weapon,0,"ModView internal default")==1);
   // 1: the body's tag; 2: the body's bone; 3: weapon 1's tag (a chain).
   assert(owner->SetBoltInfo(weapon,0,slot==1?0:slot==2?1:(1<<10)));
   assert(owner->CopyModel(weapon,0,body,slot));}
  const auto &instance=*owner->Resolve(body);
  float bodyTag[12],bodyBone[12],weaponTag[12],root[12],chainRoot[12],chainTag[12];
  assert(owner->BoltMatrix(body,0,0,0,bodyTag)&&owner->BoltMatrix(body,0,1,0,bodyBone));
  assert(owner->BoltMatrix(body,1,0,0,weaponTag)&&owner->BoltMatrix(body,1,1,0,root));
  assert(owner->BoltMatrix(body,3,1,0,chainRoot)&&owner->BoltMatrix(body,3,0,0,chainTag));
  pose::Palette parent;assert(owner->DeformationPalette(*instance.At(0),0,parent));
  if(owner==&earlier){
   // Earlier stores keep their frame (origin at the first triangle's first
   // corner, here vertex 1) and ignore links: the weapon answers from its
   // own root basis.
   double p1[3];Skin(parent.deformation[0],BodyTag[1],p1);
   for(int r=0;r<3;++r)assert(std::fabs(bodyTag[r*4+3]-p1[r])<.00002);
   // Quaternion reconstruction on x87 with fast math leaves ~1.5e-8 in
   // nominally zero cells; compare the frame at float precision.
   const pose::Matrix basis=pose::RootBasis;assert(Near(root,basis.data(),1e-7f));
   std::vector<int> order;std::vector<pose::Palette> palettes;int failed=0;
   assert(owner->InstancePalettes(instance,0,order,palettes,failed)==Posing::Unserved);
   assert(owner->LinkedPalette(instance,1,0,parent)==Posing::Unserved);
   continue;
  }
  // Retail's tag frame from the body's skinned first three vertices.
  double points[3][3];for(int v=0;v<3;++v)Skin(parent.deformation[0],BodyTag[v],points[v]);
  const auto frame=RetailTag(points);
  for(int n=0;n<12;++n)assert(std::fabs(bodyTag[n]-frame[n])<.00002);
  // An attached root is its parent's bolt (retail m1_root == body_r_hand,
  // m2_root == body_rhand_bone, m3_root == m1_flash; see record).
  assert(Near(root,bodyTag));
  float boneRoot[12];assert(owner->BoltMatrix(body,2,1,0,boneRoot)&&Near(boneRoot,bodyBone));
  assert(Near(chainRoot,weaponTag));
  // The weapon's own tag is its authored tag carried by that root.
  double carried[3][3];pose::Matrix bolt;std::memcpy(bolt.data(),bodyTag,48);
  for(int v=0;v<3;++v)Skin(bolt,WeaponTag[v],carried[v]);
  const auto weaponFrame=RetailTag(carried);for(int n=0;n<12;++n)assert(std::fabs(weaponTag[n]-weaponFrame[n])<.00005);
  pose::Palette chain;assert(owner->LinkedPalette(instance,3,0,chain)==Posing::Served);
  assert(chain.deformation.size()==1&&Near(chain.deformation[0].data(),weaponTag));
  // Every model in retail's order, parents before children.
  std::vector<int> order;std::vector<pose::Palette> palettes;int failed=-2;
  assert(owner->InstancePalettes(instance,0,order,palettes,failed)==Posing::Served&&failed==-1);
  assert(order==std::vector<int>({0,1,2,3})&&palettes.size()==4);
  assert(Near(palettes[1].deformation[0].data(),bodyTag)&&Near(palettes[3].deformation[0].data(),weaponTag));
  // The parent's animation carries the attachment with it.
  assert(owner->SetBoneAnim(body,0,"model_root",1,2,0x8,1,0,-1,0));
  float moved[12];assert(owner->BoltMatrix(body,1,1,0,moved)&&!Near(moved,root));
  float movedTag[12];assert(owner->BoltMatrix(body,0,0,0,movedTag)&&Near(moved,movedTag));
  // A bend on the attached root post-multiplies the bolt (mode 2).
  const float bend[3]{-40,20,30};assert(owner->SetBoneAngles(body,1,"ModView internal default",bend,2,1,3,2,0,0));
  pose::Matrix post,unit{1,0,0,0,0,1,0,0,0,0,1,0};assert(pose::AngleMatrix(unit,unit,bend,2,1,3,2,post));
  pose::Matrix tag;std::memcpy(tag.data(),movedTag,48);const auto bent=pose::Compose(tag,post);
  float bentRoot[12];assert(owner->BoltMatrix(body,1,1,0,bentRoot)&&Near(bentRoot,bent.data()));
  // A missing parent bolt, an unreachable model and an unserved parent pose
  // are each named; nothing is published for them.
  assert(owner->SetBoltInfo(body,2,9));float untouched[12];for(float &v:untouched)v=-3;std::memcpy(moved,untouched,48);
  assert(owner->LinkedBolt(body,2,1,0,moved)==Posing::MissingBolt&&!std::memcmp(moved,untouched,48));
  assert(owner->InstancePalettes(instance,0,order,palettes,failed)==Posing::MissingBolt&&failed==2);
  assert(order==std::vector<int>({0,1,2,3}));
  assert(owner->SetBoltInfo(body,2,(7<<10)));
  assert(owner->LinkedBolt(body,2,1,0,moved)==Posing::Unposed&&!std::memcmp(moved,untouched,48));
  assert(owner->InstancePalettes(instance,0,order,palettes,failed)==Posing::Served&&order==std::vector<int>({0,1,3}));
  assert(owner->SetBoneAnim(body,0,"model_root",1,2,0x108,1,0,-1,0));
  assert(owner->LinkedBolt(body,1,1,0,moved)==Posing::Unserved&&!std::memcmp(moved,untouched,48));
  assert(owner->InstancePalettes(instance,0,order,palettes,failed)==Posing::Unserved&&failed==0);
 }
}
static void CheckScene(){
 vm_t vm{};vm.index=VM_CGAME;vm.entryPoint=reinterpret_cast<decltype(vm.entryPoint)>(1);cgvm=&vm;
 assets["models/players/test/body.glm"]=TagMesh("models/players/test/skeleton",3,BodyTag);
 assets["models/players/test/skeleton.gla"]=PoseSkeleton();
 assets["models/weapons2/test/weapon_w.glm"]=TagMesh("*default",1,WeaponTag);
 std::uint32_t body=0,weapon=0;
 intptr_t init[]{297,reinterpret_cast<intptr_t>(&body),reinterpret_cast<intptr_t>("models/players/test/body.glm"),0,0,0,0,0};
 assert(Invoke(init)==0&&body);
 intptr_t hand[]{317,static_cast<intptr_t>(body),0,reinterpret_cast<intptr_t>("root")};assert(Invoke(hand)==0);
 init[1]=reinterpret_cast<intptr_t>(&weapon);init[2]=reinterpret_cast<intptr_t>("models/weapons2/test/weapon_w.glm");
 assert(Invoke(init)==0&&weapon);
 // cg_weapons' order: link to model 0's bolt 0, add the flash bolt, copy
 // the weapon into the body's slot 1.
 intptr_t linkWeapon[]{319,static_cast<intptr_t>(weapon),0,0};assert(Invoke(linkWeapon)==0);
 intptr_t flash[]{317,static_cast<intptr_t>(weapon),0,reinterpret_cast<intptr_t>("root")};assert(Invoke(flash)==0);
 intptr_t copy[]{308,static_cast<intptr_t>(weapon),0,static_cast<intptr_t>(body),1};assert(Invoke(copy)==0);
 assert(CL_JKACgameGhoul2Resolve(body)->Count()==2&&CL_JKACgameGhoul2Resolve(body)->At(1)->boltInfo==0);
 fntech3_jka_mp_refEntity_t record{};record.ghoul2=body;record.renderfx=0x82;
 for(int axis=0;axis<3;++axis)record.axis[axis][axis]=1;record.origin[0]=19;
 fntech3_jka_mp_refdef_t view{};view.width=640;view.height=480;view.fov_x=90;view.fov_y=75;view.time=99999;view.rdflags=0x10;
 jkaCgameBoneClocks.BeginFrame(100,false);
 const int drawsBefore=draws,registrationsBefore=registrations;
 Queue(record);Render(view);
 assert(draws==drawsBefore+2&&registrations==registrationsBefore+2&&submitted.size()==2);
 assert(submitted[0].hModel!=submitted[1].hModel&&submitted[0].origin[0]==19&&submitted[1].origin[0]==19);
 assert(scene::state.models.count({body,0})&&scene::state.models.count({body,1}));
 // The weapon registered last: its one bone is the body's tag.
 float tag[12];assert(CL_JKACgameGhoul2Store().BoltMatrix(reinterpret_cast<void *>(std::uintptr_t(body)),0,0,100,tag));
 fnMdrView_t mdr;assert(FnMdr_Open(imported.data(),imported.size(),2000,4000,&mdr)&&mdr.numBones==1);
 float bone[3][4];assert(FnMdr_Bone(&mdr,0,0,bone));assert(Near(&bone[0][0],tag));
 ClearScene();Queue(record);Render(view);assert(draws==drawsBefore+4&&registrations==registrationsBefore+2);
 assert(matrices.size()==12&&Near(matrices.data(),tag));ClearScene();
 // The cgame's bolt query on model 1 answers from the attached frame.
 std::array<byte,64> storage;storage.fill(0xa5);const float zero[3]{},unitScale[3]{1,1,1};
 intptr_t query[]{294,static_cast<intptr_t>(body),1,0,reinterpret_cast<intptr_t>(storage.data()+1),
  reinterpret_cast<intptr_t>(zero),reinterpret_cast<intptr_t>(zero),0,0,reinterpret_cast<intptr_t>(unitScale)};
 assert(Invoke(query)==1);
 float raw[12],world[12],published[12];
 assert(CL_JKACgameGhoul2Store().BoltMatrix(reinterpret_cast<void *>(std::uintptr_t(body)),1,0,100,raw));
 assert(pose::PlaceBolt(raw,zero,zero,unitScale,false,world));std::memcpy(published,storage.data()+1,48);
 assert(Near(published,world));
 // A link to a bolt the body lacks refuses by name; a link the sort never
 // reaches draws nothing for that model and refuses its bolt queries.
 linkWeapon[1]=body;linkWeapon[2]=1;linkWeapon[3]=5;assert(Invoke(linkWeapon)==0);
 const int drawsMissing=draws;Queue(record);Reject([&]{Render(view);});assert(draws==drawsMissing);ClearScene();
 Reject([&]{Invoke(query);});
 linkWeapon[3]=(3<<10);assert(Invoke(linkWeapon)==0);
 Queue(record);Render(view);assert(draws==drawsMissing+1&&submitted.size()==1);ClearScene();
 Reject([&]{Invoke(query);});
 // Replacing slot 1 with a second, unlinked body draws two bodies; the
 // slot's owned registration follows its new mesh.
 const auto weaponMesh=scene::state.models.at({body,1}).meshBytes;
 intptr_t replace[]{308,static_cast<intptr_t>(body),0,static_cast<intptr_t>(body),1};assert(Invoke(replace)==0);
 assert(CL_JKACgameGhoul2Resolve(body)->At(1)->boltInfo==link::None);
 Queue(record);Render(view);assert(draws==drawsMissing+3);ClearScene();
 assert(scene::state.models.at({body,1}).meshBytes!=weaponMesh);
 intptr_t cleanWeapon[]{301,reinterpret_cast<intptr_t>(&weapon)},cleanBody[]{301,reinterpret_cast<intptr_t>(&body)};
 assert(!Invoke(cleanWeapon)&&!Invoke(cleanBody));CL_JKACgameClear();
 assert(opens==closes);
}
int main(){
 CheckLinksAndOrder();CheckTagFrame();CheckPosing();
 if(sizeof(void*)!=4)return 0;
 CheckScene();
}
'''


class JKACgameAttachmentTests(unittest.TestCase):
    def test_links_retail_tags_attached_poses_and_fn_scene(self):
        compiler=shutil.which('clang++') or shutil.which('g++')
        if not compiler:self.skipTest('C++ compiler unavailable')
        scratch=ROOT/'.tmp/tests/jka-cgame-attachment';scratch.mkdir(parents=True,exist_ok=True)
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
