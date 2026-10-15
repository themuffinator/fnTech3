"""Exercise real Academy effect declaration registration and cgame ownership."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args
from rtcw_music_fixture import function

ROOT=Path(__file__).resolve().parents[1]
TASK=ROOT/'.tmp/tests/jka-cgame-effects'

def fixture():
    vm=(ROOT/'code/qcommon/vm.c').read_text()
    host=(ROOT/'code/client/client_jka_cgame_host.hpp').read_text()
    return r'''
#include "code/qcommon/q_shared.h"
#include "code/client/client_jka_command_values.hpp"
#include "code/compat/jka_lightstyle.hpp"
#include "code/qcommon/qcommon.h"
#include "code/cgame/cg_public.h"
#include "code/qcommon/vm_local.h"
#include "code/compat/jka_module_abi.h"
#include "code/compat/jka_client_abi.h"
#include <array>
#include <map>
#include <limits>
#include <vector>
#include <string>
#include <cassert>
#include <csetjmp>
#include <cstring>
static void CL_JKACgameClear();
struct Drop{};static bool recover,jumpRecovery;static std::jmp_buf recoveryPoint;
void QDECL Com_Error(errorParm_t,const char *,...){
 if(recover)CL_JKACgameClear();if(jumpRecovery)std::longjmp(recoveryPoint,1);throw Drop{};
}
static vm_t *cgvm;
static void *jkaCgameSharedBuffer;
intptr_t QDECL VM_Call(vm_t*,int,int,...){assert(false);return 0;}
''' + function(vm,'void *VM_ExplicitArgPtr(')+'\n'+function(vm,'void VM_CheckBounds(')+'\n'+ \
        function(host,'static void *CL_JKACgameSpan(') + r'''
static const std::string mainEffect=
 "particle\n{\nshader gfx/own/main\nmodel models/own/chunk.md3\nsound sound/own/main.mp3\nimpactfx own/child\n}\n";
static const std::string childEffect=
 "fxrunner\n{\nshader gfx/own/child\nplayfx own/main\n}\n";
static std::map<std::string,std::string> assets;
static std::map<int,std::string> files;
static int opens,reads,closes,nextFile=1,overrideLength=-1,registeredMessages,initializedMessages;
static int cvarDrop,cvarCalls;
static bool shortRead,openDrop,readDrop,closeDrop,silent,noHandle;
static int mediaDrop;
static std::vector<std::string> shaders,models,sounds;
void QDECL Com_Printf(const char *format,...){
 if(std::strstr(format,"effect registered"))++registeredMessages;
 if(std::strstr(format,"effects context initialized"))++initializedMessages;
}
int FS_FOpenFileRead(const char *name,fileHandle_t *handle,qboolean unique){
 assert(!unique);++opens;*handle=0;
 std::string key(name);for(char &c:key)if(c>='A'&&c<='Z')c+='a'-'A';
 const auto found=assets.find(key);if(found==assets.end())return -1;
 if(!noHandle){*handle=nextFile++;files[*handle]=key;}
 if(openDrop)Com_Error(ERR_DROP,"open failure");
 return overrideLength<0?static_cast<int>(found->second.size()):overrideLength;
}
int FS_Read(void *output,int length,fileHandle_t file){
 ++reads;assert(files.count(file));const auto &data=assets.at(files.at(file));
 assert(length==static_cast<int>(data.size()));
 if(readDrop)Com_Error(ERR_DROP,"read failure");if(shortRead)--length;
 std::memcpy(output,data.data(),length);return length;
}
void FS_FCloseFile(fileHandle_t file){
 assert(files.erase(file)==1);++closes;if(closeDrop)Com_Error(ERR_DROP,"close failure");
}
static qhandle_t RegisterShader(const char *name){
 shaders.emplace_back(name);if(mediaDrop==1)Com_Error(ERR_DROP,"shader failure");
 return silent?0:static_cast<qhandle_t>(100+shaders.size());
}
static qhandle_t RegisterModel(const char *name){
 models.emplace_back(name);if(mediaDrop==2)Com_Error(ERR_DROP,"model failure");
 return silent?0:static_cast<qhandle_t>(200+models.size());
}
#include "code/renderercommon/tr_types.h"
// Real playback/geometry runs in jka_effect_play_tests. An unexpected render
// call in this registration/lifecycle fixture must fail.
static void AddEffectEntity(const refEntity_t *,qboolean){assert(false);}
static void AddEffectPolys(qhandle_t,int,const polyVert_t *,int){assert(false);}
static void AddEffectLight(const vec3_t,float,float,float,float){assert(false);}
// Decal fragments are cut by Fn's mark clipper; jka_decal_tests supplies one.
static int MarkEffectFragments(int,const vec3_t *,const vec3_t,int,vec3_t,int,markFragment_t *){assert(false);return 0;}
static struct {decltype(&RegisterShader) RegisterShader;decltype(&RegisterModel) RegisterModel;
 decltype(&AddEffectEntity) AddRefEntityToScene;decltype(&AddEffectPolys) AddPolyToScene;
 decltype(&AddEffectLight) AddLightToScene;decltype(&MarkEffectFragments) MarkFragments;}
 re{RegisterShader,RegisterModel,AddEffectEntity,AddEffectPolys,AddEffectLight,MarkEffectFragments};
sfxHandle_t S_RegisterSound(const char *name,qboolean compression){
 assert(!compression);sounds.emplace_back(name);if(mediaDrop==3)Com_Error(ERR_DROP,"sound failure");
 return silent?0:static_cast<sfxHandle_t>(300+sounds.size());
}
static cvar_t fxVariables[5];
cvar_t *Cvar_Get(const char *name,const char *value,int flags){
 int index=-1;++cvarCalls;
 if(!std::strcmp(name,"fx_debug")){index=0;assert(!std::strcmp(value,"0")&&flags==CVAR_TEMP);}
 if(!std::strcmp(name,"fx_countScale")){index=1;assert(!std::strcmp(value,"1")&&flags==CVAR_ARCHIVE);}
 if(!std::strcmp(name,"fx_nearCull")){index=2;assert(!std::strcmp(value,"16")&&flags==CVAR_ARCHIVE);}
 if(!std::strcmp(name,"RMG")){index=3;assert(!std::strcmp(value,"0")&&flags==0);}
 if(!std::strcmp(name,"r_markcount")){index=4;assert(!std::strcmp(value,"100")&&flags==CVAR_ARCHIVE);}
 assert(index>=0);if(cvarDrop==index+1)Com_Error(ERR_DROP,"cvar failure");return &fxVariables[index];
}
// Sound primitives start their sound through the profile's spatial entry.
struct EffectSound{std::array<float,3> origin;int entity,channel,handle;};
static std::vector<EffectSound> effectSounds;
void S_StartJKASound(vec3_t origin,int entity,int channel,sfxHandle_t handle){
 assert(origin);effectSounds.push_back({{origin[0],origin[1],origin[2]},entity,channel,handle});
}
// cl_viewShake's factor; jka_camera_shake_tests sets it.
static float viewShakeScale=1;
float CL_ViewShakeScale(){return viewShakeScale;}
#include "code/client/client_jka_cgame_effects.hpp"
static fntech3::jka::lightstyle::Store jkaCgameLightStyles;
static std::vector<int> jkaFonts;
static int modelResets,stringResets,ambientResets;
static void CL_JKACgameGhoul2Clear(){++modelResets;}
static void CL_JKACgameAmbientClear(){++ambientResets;}
static void CL_JKAStringReset(){++stringResets;}
void S_ClearJKAMusicPreload(){}
void S_JKASetSoundRegistrationQuiet(qboolean){}
''' + function(host,'static void CL_JKACgameClear(') + r'''
static intptr_t Invoke(intptr_t address){
 intptr_t args[]{263,address},result=-99;
 assert(CL_JKACgameEffectsCall(args,fntech3::jka::module::Call::CG_FX_REGISTER_EFFECT,result));return result;
}
static intptr_t Register(const char *name){return Invoke(reinterpret_cast<intptr_t>(name));}
static intptr_t System(fntech3::jka::module::Call call,intptr_t address=0){
 intptr_t args[]{0,address},result=-99;assert(CL_JKACgameEffectsCall(args,call,result));return result;
}
template<class F>static void Reject(F action){bool rejected=false;try{action();}catch(Drop){rejected=true;}assert(rejected);}
static void Reset(){
 shortRead=openDrop=readDrop=closeDrop=silent=noHandle=false;mediaDrop=0;overrideLength=-1;
 assert(files.empty());CL_JKACgameClear();
 assets.clear();assets["effects/own/main.efx"]=mainEffect;assets["effects/own/child.efx"]=childEffect;
 shaders.clear();models.clear();sounds.clear();registeredMessages=initializedMessages=0;cvarDrop=cvarCalls=0;
}
int main(){
 vm_t vm{};vm.entryPoint=reinterpret_cast<decltype(vm.entryPoint)>(1);cgvm=&vm;Reset();
 using Call=fntech3::jka::module::Call;
 assert(System(Call::CG_FX_FREE_SYSTEM)==1&&!jkaCgameEffects);
 assert(System(Call::CG_FX_SET_REFDEF)==0&&jkaCgameEffects&&!jkaCgameEffects->initialized);
 assert(System(Call::CG_FX_INIT_SYSTEM)==1&&jkaCgameEffects->initialized&&jkaCgameEffects->view==0);
 assert(initializedMessages==1&&cvarCalls==4&&jkaCgameEffects->debug==&fxVariables[0]&&jkaCgameEffects->rmg==&fxVariables[3]);
 Reset();
 assert(System(Call::CG_FX_ADJUST_TIME,0)==0&&jkaCgameEffects&&!jkaCgameEffects->initialized&&cvarCalls==0);
 assert(!jkaCgameEffects->time&&!jkaCgameEffects->previousTime&&!jkaCgameEffects->frameTime&&!jkaCgameEffects->realTime);
 assert(System(Call::CG_FX_ADJUST_TIME,1000)==0);
 assert(jkaCgameEffects->time==1000&&!jkaCgameEffects->previousTime&&jkaCgameEffects->frameTime==1000&&jkaCgameEffects->realTime==1.0f);
 assert(System(Call::CG_FX_ADJUST_TIME,1020)==0);
 assert(jkaCgameEffects->time==1020&&jkaCgameEffects->previousTime==1000&&jkaCgameEffects->frameTime==20);
 uint32_t seconds;std::memcpy(&seconds,&jkaCgameEffects->realTime,4);assert(seconds==0x3ca3d70bu);
 for(int paused:{0,-1,INT_MIN}){
  assert(System(Call::CG_FX_ADJUST_TIME,paused)==0);
  assert(jkaCgameEffects->time==1020&&jkaCgameEffects->previousTime==1000&&!jkaCgameEffects->frameTime&&!jkaCgameEffects->realTime);
 }
 assert(System(Call::CG_FX_ADJUST_TIME,500)==0&&jkaCgameEffects->previousTime==1020&&jkaCgameEffects->frameTime==-520&&jkaCgameEffects->realTime<0);
 jkaCgameEffects->time=1;assert(System(Call::CG_FX_ADJUST_TIME,INT_MAX)==0&&jkaCgameEffects->frameTime==INT_MAX-1);
 std::memcpy(&seconds,&jkaCgameEffects->realTime,4);assert(seconds==0x4a03126fu);
 jkaCgameEffects->time=INT_MIN;assert(System(Call::CG_FX_ADJUST_TIME,INT_MAX)==0&&jkaCgameEffects->frameTime==-1);
 std::memcpy(&seconds,&jkaCgameEffects->realTime,4);assert(seconds==0xba83126fu);
 if(sizeof(intptr_t)>4){Reject([&]{System(Call::CG_FX_ADJUST_TIME,INT64_C(0x100000000));});assert(jkaCgameEffects->time==INT_MAX&&jkaCgameEffects->frameTime==-1);}
 assert(System(Call::CG_FX_FREE_SYSTEM)==1&&jkaCgameEffects->time==INT_MAX&&jkaCgameEffects->realTime<0);
 assert(System(Call::CG_FX_INIT_SYSTEM)==1&&!jkaCgameEffects->time&&!jkaCgameEffects->realTime);
 Reset();
 assert(Register("own/main")==1&&jkaCgameEffects->effects.size()==2&&files.empty());
 assert(registeredMessages==2&&shaders.size()==2&&models.size()==1&&sounds.size()==1);
 auto &first=*jkaCgameEffects->effects[0];auto &child=*jkaCgameEffects->effects[1];
 assert(first.ready&&child.ready&&first.bytes.empty()&&child.bytes.empty());
 assert(first.definition.primitives[0].media[0].handle==101&&first.definition.primitives[0].media[3].handle==2);
 assert(child.definition.primitives[0].media[1].handle==1);
 fntech3_jka_mp_refdef_t view{};auto viewAddress=reinterpret_cast<intptr_t>(&view);
 assert(System(Call::CG_FX_INIT_SYSTEM,viewAddress)==1&&jkaCgameEffects->view==viewAddress);
 assert(first.ready&&jkaCgameEffects->effects.size()==2);
 jkaCgameEffects->instances[8].definition=2;jkaCgameEffects->time=50;
 jkaCgameEffects->previousTime=40;jkaCgameEffects->frameTime=10;jkaCgameEffects->realTime=0.01f;jkaCgameEffects->frozen=true;
 assert(System(Call::CG_FX_INIT_SYSTEM,0)==1&&jkaCgameEffects->view==0);
 assert(jkaCgameEffects->instances[8].definition==2&&!jkaCgameEffects->time&&!jkaCgameEffects->previousTime&&!jkaCgameEffects->frameTime&&!jkaCgameEffects->realTime&&!jkaCgameEffects->frozen);
 assert(System(Call::CG_FX_SET_REFDEF,viewAddress)==0&&jkaCgameEffects->view==viewAddress);
 int oldOpens=opens;assert(Register("OWN/MAIN.EFX")==1&&opens==oldOpens);
 // Prefixes are retained in the retail cache key, not silently folded away.
 assert(Register("effects/own/main.efx")==3&&jkaCgameEffects->effects.size()==3);
 const auto count=jkaCgameEffects->effects.size();assert(Register("own/missing")==0);
 assert(jkaCgameEffects->effects.size()==count&&files.empty());
 assets["effects/own/missing.efx"]="particle\n{\n}\n";assert(Register("own/missing")==4);
 jkaCgameSharedBuffer=&vm;jkaFonts.push_back(1);int oldModels=modelResets,oldStrings=stringResets,oldAmbient=ambientResets;
 assert(jkaCgameLightStyles.Set(63,UINT32_C(0x817f1305))==fntech3::jka::lightstyle::Status::Ok);
 fntech3::jka::command::selections.Set(255,257);
 CL_JKACgameClear();
 { usercmd_t cleared{};cleared.raven.generic_cmd=77;
   fntech3::jka::command::selections.Publish(cleared);
   assert(!cleared.raven.forcesel&&!cleared.raven.invensel&&cleared.raven.generic_cmd==77); }
assert(!jkaCgameEffects&&!jkaCgameSharedBuffer&&jkaFonts.empty());
 fntech3::jka::lightstyle::Color resetStyle;assert(jkaCgameLightStyles.Get(63,resetStyle));
 assert((resetStyle==fntech3::jka::lightstyle::Color{{255,255,255,255}}));
 assert(modelResets==oldModels+1&&stringResets==oldStrings+1&&ambientResets==oldAmbient+1);
 Reset();assert(Register("own/main")==1);
 assert(System(Call::CG_FX_INIT_SYSTEM,viewAddress)==1);
 assert(System(Call::CG_FX_FREE_SYSTEM)==1&&jkaCgameEffects->initialized&&jkaCgameEffects->view==viewAddress&&jkaCgameEffects->effects.empty()&&jkaCgameEffects->lookup.empty());
 assert(Register("own/main")==1);
 Reset();silent=true;assert(Register("own/main")==1&&registeredMessages==2);
 for(const auto &effect:jkaCgameEffects->effects)for(const auto &primitive:effect->definition.primitives)
  for(const auto &media:primitive.media)if(media.kind<=fntech3::jka::effects::MediaKind::Sound)assert(media.handle==0);
 Reset();recover=true;
 for(int failure=1;failure<=4;++failure){Reset();cvarDrop=failure;
  Reject([&]{System(Call::CG_FX_INIT_SYSTEM,viewAddress);});assert(!jkaCgameEffects&&files.empty()&&!initializedMessages);}
 Reset();jumpRecovery=true;cvarDrop=2;
 if(!setjmp(recoveryPoint)){System(Call::CG_FX_INIT_SYSTEM,viewAddress);assert(false);}
 assert(!jkaCgameEffects);jumpRecovery=false;
 for(int failure=0;failure<10;++failure){
  Reset();
  if(failure==0)shortRead=true;if(failure==1)openDrop=true;if(failure==2)readDrop=true;
  if(failure==3)closeDrop=true;if(failure>=4&&failure<=6)mediaDrop=failure-3;
  if(failure==7)assets["effects/own/main.efx"]="particle\n{\n";
  if(failure==8)assets["effects/own/main.efx"]="particle\n{\nshader ../outside\n}\n";
  if(failure==9)assets["effects/own/child.efx"]="fxrunner\n{\nplayfx ../outside\n}\n";
  Reject([&]{Register("own/main");});assert(!jkaCgameEffects&&files.empty()&&registeredMessages==0);
 }
 for(int failure=0;failure<3;++failure){
  Reset();if(failure==0)noHandle=true;if(failure==1)overrideLength=0;if(failure==2)overrideLength=65535;
  assert(Register("own/main")==0&&jkaCgameEffects->effects.empty()&&files.empty()&&!registeredMessages);
 }
 // Engine ERR_DROP uses longjmp: verify the owner also survives skipped C++
 // stack unwinding, with no file or partial graph left behind.
 Reset();jumpRecovery=true;readDrop=true;
 if(!setjmp(recoveryPoint)){Register("own/main");assert(false);}
 assert(!jkaCgameEffects&&files.empty());jumpRecovery=false;
 Reset();jumpRecovery=true;mediaDrop=2;
 if(!setjmp(recoveryPoint)){Register("own/main");assert(false);}
 assert(!jkaCgameEffects&&files.empty());jumpRecovery=false;
 Reset();Reject([&]{Register("../outside");});assert(!jkaCgameEffects);
 Reject([&]{Invoke(0);});Reject([&]{Invoke(-1);});
 std::array<char,64> longName;longName.fill('x');Reject([&]{Invoke(reinterpret_cast<intptr_t>(longName.data()));});
 vm.entryPoint=nullptr;std::array<byte,256> qvm{};vm.dataBase=qvm.data();vm.dataMask=255;
 std::memset(qvm.data()+250,'x',6);Reject([&]{Invoke(250);});
 Reject([&]{System(Call::CG_FX_INIT_SYSTEM,200);});
 Reject([&]{System(Call::CG_FX_SET_REFDEF,200);});
 vm.entryPoint=reinterpret_cast<decltype(vm.entryPoint)>(1);Reset();
 Reject([&]{System(Call::CG_FX_INIT_SYSTEM,-1);});
 for(int n=1;n<256;++n){const auto name="own/slot"+std::to_string(n);assets["effects/"+name+".efx"]="particle\n{\n}\n";
  assert(Register(name.c_str())==n);}
 assets["effects/own/overflow.efx"]="particle\n{\n}\n";
 assert(Register("own/overflow")==0&&jkaCgameEffects->effects.size()==255);
 assert(Register("own/slot1")==1);
 intptr_t args[]{266,1,0,0,-1,-1},result=17;
 Reject([&]{CL_JKACgameEffectsCall(args,fntech3::jka::module::Call::CG_FX_PLAY_EFFECT_ID,result);});assert(result==17);
 recover=false;CL_JKACgameClear();assert(!jkaCgameEffects&&files.empty());
}
'''

class JKACgameEffectsTests(unittest.TestCase):
    def test_registration_cache_resources_cycles_and_error_cleanup(self):
        compiler=shutil.which('clang++') or shutil.which('g++')
        if not compiler:self.skipTest('C++ compiler unavailable')
        self.assertTrue(TASK.resolve().is_relative_to(ROOT.resolve()))
        TASK.mkdir(parents=True,exist_ok=True)
        output=Path(tempfile.mkdtemp(dir=TASK,prefix='effects-host-')).absolute()
        try:
            src=output/'effects-host.cpp';exe=output/('effects-host.exe' if os.name=='nt' else 'effects-host')
            src.write_text(fixture())
            target=compiler_target_args(compiler)
            if os.name=='nt' and not target:target=['-DFNTECH3_Q3_ONLY']
            built=subprocess.run([compiler,*target,'-std=c++17','-I',str(ROOT),str(src),'-o',str(exe)],
                cwd=ROOT,text=True,capture_output=True,timeout=90)
            self.assertEqual(built.returncode,0,built.stdout+built.stderr)
            ran=subprocess.run([str(exe)],cwd=ROOT,text=True,capture_output=True,timeout=30)
            self.assertEqual(ran.returncode,0,ran.stdout+ran.stderr)
        finally:
            self.assertEqual(output.resolve(),output)
            self.assertTrue(output.is_relative_to(TASK.absolute()))
            for directory,directories,names in os.walk(output,followlinks=False):
                for path in [Path(directory),*(Path(directory)/name for name in directories+names)]:
                    self.assertFalse(path.is_symlink() or getattr(path.stat(),'st_file_attributes',0)&0x400)
            shutil.rmtree(output)

if __name__=='__main__':unittest.main()
