"""Actual owned effects imports, Fn sprite/polygon packets and error lifetime."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args
from jka_cgame_effects_tests import fixture as registration_fixture

ROOT=Path(__file__).resolve().parents[1]


def fixture():
    prefix=registration_fixture().split('int main(){')[0]
    old='''static void AddEffectEntity(const refEntity_t *,qboolean){assert(false);}
static void AddEffectPolys(qhandle_t,int,const polyVert_t *,int){assert(false);}'''
    assert prefix.count(old)==1
    prefix=prefix.replace(old,r'''
static int sprites,polygons;static bool renderDrop;
static std::vector<refEntity_t> entities;
static std::vector<polyVert_t> vertices;
static void AddEffectEntity(const refEntity_t *entity,qboolean integerTime){
 assert(entity&&entity->reType==RT_SPRITE&&!integerTime);if(renderDrop)Com_Error(ERR_DROP,"render failure");
 ++sprites;entities.push_back(*entity);
}
static void AddEffectPolys(qhandle_t shader,int count,const polyVert_t *data,int instances){
 assert(shader>=0&&count==4&&instances>0);if(renderDrop)Com_Error(ERR_DROP,"render failure");
 polygons+=instances;vertices.insert(vertices.end(),data,data+count*instances);
}
''')
    return prefix+r'''
namespace playback=fntech3::jka::effectplay;
using Call=fntech3::jka::module::Call;
static intptr_t InvokePlayback(intptr_t *args){
 intptr_t result=17;const auto call=fntech3::jka::module::Lookup(COM_GAME_JKA_MP,
  fntech3::jka::module::Module::CGAME,fntech3::jka::module::Direction::IMPORT,args[0]);
 assert(CL_JKACgameEffectsCall(args,call,result));return result;
}
static int Active(){int count=0;if(jkaCgameEffects)for(const auto &instance:jkaCgameEffects->instances)count+=instance.active;return count;}
static void Frame(int time){assert(System(Call::CG_FX_ADJUST_TIME,time)==0);}
static void Draw(int portal=0){intptr_t args[]{270,portal};assert(InvokePlayback(args)==0);}
static std::string Primitive(const char *kind,const char *shader){
 return std::string(kind)+"\n{\nshaders\n[\n"+shader+"\n]\norigin 3 0 0\nrgb\n{\nstart .2 .3 .4\nend .9 .8 .7\n}\nalpha\n{\nstart .2 .3\n}\nsize\n{\nstart 5\n}\nsize2\n{\nstart 2\nend 0\n}\nlength\n{\nstart 7\n}\n}\n";
}
int main(){
 vm_t vm{};vm.entryPoint=reinterpret_cast<decltype(vm.entryPoint)>(1);vm.index=VM_CGAME;cgvm=&vm;
 if(sizeof(void*)!=4)return 0;
 Reset();fntech3_jka_mp_refdef_t view{};view.width=320;view.height=240;view.fov_x=90;view.fov_y=75;
 assert(System(Call::CG_FX_INIT_SYSTEM,reinterpret_cast<intptr_t>(&view))==1);
 fxVariables[2].value=16;Frame(500);
 assets["effects/own/visual.efx"]=Primitive("Cylinder","gfx/own/cylinder")+
  Primitive("Particle","gfx/own/a")+Primitive("Particle","gfx/own/b")+Primitive("Particle","gfx/own/c")+
  Primitive("OrientedParticle","gfx/own/oriented")+Primitive("Particle","gfx/own/d");
 const auto id=Register("own/visual");assert(id>0);
 std::array<byte,14> originBytes,forwardBytes;originBytes.fill(0xa5);forwardBytes.fill(0xa5);
 const float origin[]{100,200,300},forward[]{0,0,1};
 std::memcpy(originBytes.data()+1,origin,12);std::memcpy(forwardBytes.data()+1,forward,12);
 const auto originalOrigin=originBytes,originalForward=forwardBytes;
 intptr_t args[]{266,id,reinterpret_cast<intptr_t>(originBytes.data()+1),reinterpret_cast<intptr_t>(forwardBytes.data()+1),-1,-1};
 assert(InvokePlayback(args)==0&&Active()==6&&originBytes==originalOrigin&&forwardBytes==originalForward);
 assert(jkaCgameEffects->effects[id-1]->playable&&jkaCgameEffects->spawnWork.empty()&&jkaCgameEffects->slotWork.empty());
 for(const auto &instance:jkaCgameEffects->instances)if(instance.active){
  assert(instance.startTime==500&&instance.spawn.life==50&&instance.spawn.origin==(playback::Vector{100,200,303}));
  assert(instance.spawn.color[0]>=10&&instance.spawn.color[0]<=15);
  assert(instance.spawn.color[1]>=15&&instance.spawn.color[1]<=22);
  assert(instance.spawn.color[2]>=20&&instance.spawn.color[2]<=30&&instance.spawn.color[3]==255);
 }
 const auto pausedSeed=jkaCgameEffects->random;Frame(0);assert(InvokePlayback(args)==0&&Active()==6&&jkaCgameEffects->random==pausedSeed);Frame(500);
 Draw(2);assert(!sprites&&!polygons&&Active()==6);Draw();assert(sprites==4&&polygons==17&&vertices.size()==68);
 for(const auto &entity:entities){assert(entity.radius==5&&entity.shaderTime.f==0&&entity.origin[2]==303&&entity.shader.rgba[3]==255);}
 for(const auto &vertex:vertices){for(float component:vertex.xyz)assert(playback::Finite(component));}
 assert(vertices[0].xyz[2]==303&&vertices[2].xyz[2]==310); // Actual generated cylinder ends.
 Frame(550);Draw();assert(Active()==6&&sprites==8&&polygons==34);
 Frame(551);Draw();assert(!Active()&&sprites==8&&polygons==34);
 // Near culling retains active ownership and does not fabricate a spawn failure.
 std::memcpy(view.vieworg,origin,12);Frame(600);assert(InvokePlayback(args)==0&&Active()==6);Draw();assert(sprites==8&&polygons==34);
 view.vieworg[0]=view.vieworg[1]=view.vieworg[2]=0;
 const auto oldID=args[1];args[1]=0;args[2]=0;assert(InvokePlayback(args)==0&&Active()==6);
 args[1]=256;assert(InvokePlayback(args)==0&&Active()==6);args[1]=oldID;args[2]=reinterpret_cast<intptr_t>(originBytes.data()+1);
 // The measured wrapper consumes forward even for a missing definition.
 args[1]=0;args[3]=0;Reject([&]{InvokePlayback(args);});args[1]=oldID;args[3]=reinterpret_cast<intptr_t>(forwardBytes.data()+1);
 vm.index=VM_UI;Reject([&]{InvokePlayback(args);});vm.index=VM_CGAME;
 vm.entryPoint=nullptr;Reject([&]{Draw();});vm.entryPoint=reinterpret_cast<decltype(vm.entryPoint)>(1);
 vm.dllExports=reinterpret_cast<decltype(vm.dllExports)>(1);Reject([&]{InvokePlayback(args);});vm.dllExports=nullptr;
 for(int position:{2,3}){const auto address=args[position];args[position]=0;Reject([&]{InvokePlayback(args);});
  args[position]=static_cast<intptr_t>(UINTPTR_MAX-8);Reject([&]{InvokePlayback(args);});args[position]=address;}
 for(std::uint32_t bits:{0x7fc00001u,0x7f800000u,0xff800000u}){
  std::memcpy(forwardBytes.data()+1,&bits,4);Reject([&]{InvokePlayback(args);});assert(Active()==6);
 }forwardBytes=originalForward;
 const float zero[3]{};std::memcpy(forwardBytes.data()+1,zero,12);Reject([&]{InvokePlayback(args);});forwardBytes=originalForward;
 Frame(599);Reject([&]{Draw();});Frame(600);
 const auto renderer=re.AddPolyToScene;re.AddPolyToScene=nullptr;Reject([&]{Draw();});re.AddPolyToScene=renderer;
 jkaCgameEffects->view=0;Reject([&]{Draw();});jkaCgameEffects->view=reinterpret_cast<intptr_t>(&view);
 std::uint32_t nan=0x7fc00001;std::memcpy(&fxVariables[2].value,&nan,4);Reject([&]{Draw();});fxVariables[2].value=16;
 // Malformed/unsupported declarations refuse before publishing any primitive.
 const auto baseline=Active();
 for(const char *field:{"flags relative","count 501","life 0","delay 60001"}){
  assets["effects/own/bad.efx"]=std::string("particle\n{\nshader gfx/own/bad\n")+field+"\n}\n";
  CL_JKACgameEffectsFree();const auto bad=Register("own/bad");args[1]=bad;
  Reject([&]{InvokePlayback(args);});assert(!Active());
 }
 assert(baseline==6);CL_JKACgameEffectsFree();args[1]=Register("own/visual");
 assert(InvokePlayback(args)==0&&Active()==6);
 // A full pool evicts the owned instances nearest their end, as retail's
 // slot search (0x40f2c0) makes room rather than refusing.
 const auto seed=jkaCgameEffects->instances[0];for(auto &instance:jkaCgameEffects->instances)instance=seed;
 for(int n=100;n<106;++n)jkaCgameEffects->instances[n].startTime=599;
 for(int n=200;n<206;++n)jkaCgameEffects->instances[n].spawn.life=5000;
 assert(Active()==1800);assert(InvokePlayback(args)==0&&Active()==1800);
 // The six ending soonest made the room; the long-lived six survive.
 for(const auto &instance:jkaCgameEffects->instances)assert(instance.active&&instance.startTime==600);
 for(int n=200;n<206;++n)assert(jkaCgameEffects->instances[n].spawn.life==5000);
 for(auto &instance:jkaCgameEffects->instances)instance.spawn.life=50;
 Frame(651);Draw();assert(!Active());
 Frame(700);assert(InvokePlayback(args)==0&&Active()==6);
 renderDrop=true;recover=jumpRecovery=true;
 if(!setjmp(recoveryPoint)){Draw();assert(false);}
 recover=jumpRecovery=renderDrop=false;
 assert(!jkaCgameEffects&&files.empty()&&opens==closes);
}
'''


class JKAEffectPlayTests(unittest.TestCase):
    def test_actual_imports_owned_lifetimes_fn_packets_and_failure_cleanup(self):
        compiler=shutil.which('clang++') or shutil.which('g++')
        if not compiler:self.skipTest('C++ compiler unavailable')
        scratch=ROOT/'.tmp/tests/jka-effect-play';scratch.mkdir(parents=True,exist_ok=True)
        with tempfile.TemporaryDirectory(dir=scratch) as folder:
            directory=Path(folder);source=directory/'play.cpp';source.write_text(fixture())
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
