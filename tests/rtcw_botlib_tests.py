"""Execute the shared bot lifecycle and RTCW native/QVM bootstrap boundary."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args
from rtcw_server_world_tests import function

ROOT = Path(__file__).resolve().parents[1]

PRELUDE = r'''
#include "code/server/server.h"
#include "code/botlib/botlib.h"
#include "code/botlib/be_interface.h"
#include "code/compat/rtcw_module_abi.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <set>
#include <string>
#include <vector>
static const comGameProfile_t *profile;
const comGameProfile_t *Com_GameProfile(){return profile;}
int Com_GameIsRTCW(){return profile->id==COM_GAME_RTCW_SP||profile->id==COM_GAME_RTCW_MP;}
// The visibility fixture replaces this stand-in with the actual route query.
bool AAS_RTCWRouteFirstVisiblePosition(const float*,const float*,int,float*){assert(false);return false;}
bool AAS_RTCWHidePosition(const float*,int,int,const float*,int,int,float*){assert(false);return false;}
int AAS_RTCWAttackPosition(int,int,int,float,int,float*){assert(false);return 0;}
struct Drop{};
void QDECL Com_Error(errorParm_t,const char*,...){throw Drop{};}
// Server AI slots are exercised with real entity adapters in their own fixture.
int SV_BotAllocateClient(){assert(false);return -1;}
void SV_BotFreeClient(int){assert(false);}
static std::set<void*> allocations;
void *GetMemory(unsigned long size){auto *p=std::malloc(size);assert(p);allocations.insert(p);return p;}
void FreeMemory(void *p){assert(p&&allocations.erase(p)==1);std::free(p);}
void *GetClearedHunkMemory(unsigned long size){auto *p=GetMemory(size);std::memset(p,0,size);return p;}
int Q_stricmp(const char *a,const char *b){
 while(*a&&std::tolower((unsigned char)*a)==std::tolower((unsigned char)*b)){++a;++b;}
 return std::tolower((unsigned char)*a)-std::tolower((unsigned char)*b);
}
void Q_strncpyz(char *out,const char *in,int n){assert(n>0);int i=0;while(i+1<n&&in[i]){out[i]=in[i];++i;}out[i]=0;}
#include "code/botlib/l_libvar.cpp"
// ET derives its own waypoints and visibility; these fixtures never build them.
// The callback record is declared so the extracted setter compiles.
struct aas_wolfet_callbacks_t{
 bool (*available)()=nullptr;
 int (*visible)(const float*,int,const float*,int,int)=nullptr;
};
void AAS_WolfETBuildVisibility(){}
void AAS_WolfETFreeVisibility(){}
int AAS_WolfETAreaVisible(int,int){return 0;}
const float *AAS_WolfETWaypoint(int){return nullptr;}
static int Export_BotLibSetup();
static int Export_BotLibShutdown();
static int failure,dropStage,stopStage;
static bool recursiveStop;
static std::vector<int> starts,stops;
static std::array<void*,7> resources{};
static int begin(int stage){
 starts.push_back(stage);assert(!resources[stage]);resources[stage]=GetMemory(16);
 assert(Export_BotLibSetup()==BLERR_LIBRARYNOTSETUP);
 if(dropStage==stage)throw Drop{};
 if(stopStage==stage){assert(Export_BotLibShutdown()==0);return 0;}
 return failure==stage?100+stage:0;
}
static void end(int stage){
	if(!resources[stage])return;
 stops.push_back(stage);assert(resources[stage]);FreeMemory(resources[stage]);resources[stage]=nullptr;
 if(recursiveStop){assert(Export_BotLibShutdown()==0);assert(Export_BotLibSetup()==BLERR_LIBRARYNOTSETUP);}
}
int AAS_Setup(){return begin(1);} void AAS_Shutdown(){end(1);}
int EA_Setup(){return begin(2);} void EA_Shutdown(){end(2);}
int BotSetupWeaponAI(){return begin(3);} void BotShutdownWeaponAI(){end(3);}
int BotSetupGoalAI(){return begin(4);} void BotShutdownGoalAI(){end(4);}
int BotSetupChatAI(){return begin(5);} void BotShutdownChatAI(){end(5);}
int BotSetupMoveAI(){return begin(6);} void BotShutdownMoveAI(){end(6);}
void BotShutdownWeights(){} void BotShutdownCharacters(){}
static int logOpen,logClose,definesCleared,handlesChecked,levelItems,brushTypes,entityUpdates,mapResult;
static std::string loadedMap;static bool nullMap;
void Log_Open(const char*){++logOpen;} void Log_Shutdown(){++logClose;}
void PC_RemoveAllGlobalDefines(){++definesCleared;} void PC_CheckOpenSourceHandles(){++handlesChecked;}
static std::string define;
int PC_AddGlobalDefine(const char *s){define=s;return 73;}
int AAS_LoadMap(const char*name){nullMap=!name;loadedMap=name?name:"";return mapResult;}
void BotInitLevelItems(){++levelItems;} void BotSetBrushModelTypes(){++brushTypes;}
int AAS_UpdateEntity(int,bot_entitystate_t*){++entityUpdates;return 0;}
static float frameTime;static int frameResult;
int AAS_StartFrame(float time){frameTime=time;return frameResult;}
static void QDECL print(int,const char*,...){}
'''

CHECKS = r'''
server_t sv{};serverStatic_t svs{};
void SV_ClientThink(client_t*,usercmd_t*){assert(false);}
botlib_export_t *botlib_export=&be_botlib_export;
int SV_BotLibSetup(){return Export_BotLibSetup();}
int SV_BotLibShutdown(){return Export_BotLibShutdown();}
// Navigation has its own executable fixture; this one isolates bootstrap.
bool AAS_SetCurrentWorld(int){assert(false);return false;}
void AAS_SetBlockingEntity(const float*,const float*,int){assert(false);}
void BotInitAvoidReach(int){assert(false);}
bool EA_ClientInputAvailable(int){assert(false);return false;}
unsigned EA_ExtraInputFlags(int){assert(false);return 0;}
void EA_RestoreInput(int,const bot_input_t*,unsigned){assert(false);}
#include "code/server/server_rtcw_botlib.hpp"
using namespace fntech3::rtcw::module;
template<class F> void rejected(F f){try{f();assert(false);}catch(Drop&) {}}
static void reset(){
 assert(allocations.empty());starts.clear();stops.clear();failure=dropStage=stopStage=0;
 recursiveStop=true;levelItems=brushTypes=entityUpdates=0;mapResult=0;
}
static void checkLifecycle(){
 for(const auto &p:profiles){profile=&p;reset();
  const bool sp=p.id==COM_GAME_RTCW_SP,mp=p.id==COM_GAME_RTCW_MP,et=p.id==COM_GAME_WOLFET;
  std::vector<int> expected=sp?std::vector<int>{1,2,6}:(mp||et)?std::vector<int>{1,2}:std::vector<int>{1,2,3,4,5,6};
  assert(Export_BotLibShutdown()==BLERR_LIBRARYNOTSETUP);
  assert(Export_BotLibUpdateEntity(0,nullptr)==BLERR_LIBRARYNOTSETUP);
  assert(Export_BotLibStartFrame(0)==BLERR_LIBRARYNOTSETUP);
  assert(Export_BotLibSetup()==0&&starts==expected&&botlibsetup&&botlibglobals.botlibsetup);
  assert(botlibglobals.maxclients==(sp||mp?128:64)&&botlibglobals.maxentities==(sp?2048:1024));
  assert(Export_BotLibSetup()==0&&starts==expected);
  assert(Export_BotLibLoadMap("synthetic")==0&&levelItems==(!sp&&!mp&&!et)&&brushTypes==1);
  mapResult=37;assert(Export_BotLibLoadMap("invalid")==37&&brushTypes==1);
  assert(Export_BotLibUpdateEntity(-1,nullptr)==BLERR_INVALIDENTITYNUMBER);
  assert(Export_BotLibUpdateEntity(botlibglobals.maxentities,nullptr)==BLERR_INVALIDENTITYNUMBER);
  assert(!entityUpdates&&Export_BotLibUpdateEntity(botlibglobals.maxentities-1,nullptr)==0&&entityUpdates==1);
  assert(Export_BotLibShutdown()==0&&allocations.empty()&&!botlibsetup&&!botlibglobals.botlibsetup);
  auto sorted=stops;std::sort(sorted.begin(),sorted.end());assert(sorted==expected);
  reset();assert(Export_BotLibSetup()==0);
  for(int stage=3;stage<=6;++stage)if(!resources[stage])resources[stage]=GetMemory(16);
  assert(Export_BotLibShutdown()==0&&allocations.empty());
  for(int stage:expected){
   // Normal error returns and nonlocal engine drops both release every
   // attempted subsystem, including the one which failed partway through.
   for(bool drop:{false,true}){reset();failure=drop?0:stage;dropStage=drop?stage:0;
    if(drop){rejected([]{Export_BotLibSetup();});assert(Export_BotLibShutdown()==0);}
    else assert(Export_BotLibSetup()==100+stage);
    assert(allocations.empty()&&!botlibsetup&&botlibPhase==BotLibPhase::Inactive);
    sorted=stops;std::sort(sorted.begin(),sorted.end());assert(sorted==starts);
    reset();assert(Export_BotLibSetup()==0&&starts==expected);assert(Export_BotLibShutdown()==0);
   }
   reset();stopStage=stage;assert(Export_BotLibSetup()==BLERR_LIBRARYNOTSETUP&&allocations.empty());
  }
  for(const char *name:{"maxclients","maxentities"}){
   for(const char *value:{"0","-1","nonsense","1e3","999999999999999999999999999999999999999999999999"}){
    reset();LibVarSet(name,value);assert(Export_BotLibSetup()==BLERR_LIBRARYNOTSETUP&&starts.empty()&&allocations.empty());
   }
   reset();LibVarSet(name,name[3]=='c'?"129":"2049");
   assert(Export_BotLibSetup()==BLERR_LIBRARYNOTSETUP&&starts.empty()&&allocations.empty());
  }
  reset();LibVarSet("maxclients","32");LibVarSet("maxentities","1024");LibVarSet("bot_developer","1");
  int before=logOpen;assert(Export_BotLibSetup()==0&&botlibglobals.maxclients==32&&logOpen==before+1);
  assert(Export_BotLibShutdown()==0&&allocations.empty());
 }
}
static void checkArguments(){
 be_botlib_export.BotLibVarSet=Export_BotLibVarSet;be_botlib_export.BotLibVarGet=Export_BotLibVarGet;
 be_botlib_export.PC_AddGlobalDefine=PC_AddGlobalDefine;
 be_botlib_export.BotLibLoadMap=Export_BotLibLoadMap;
 be_botlib_export.BotLibStartFrame=Export_BotLibStartFrame;
 for(const auto &p:profiles){profile=&p;
  if(!Com_GameIsRTCW()){intptr_t result=31;assert(!SV_RTCWBotLibCall(nullptr,Call::BOTLIB_SETUP,nullptr,result)&&result==31);continue;}
  for(bool qvm:{false,true}){
   reset();std::array<byte,4096> memory{};vm_t vm{};vm.dataBase=memory.data();vm.exactDataLength=(int)memory.size();vm.dataMask=4095;
   if(!qvm)vm.entryPoint=reinterpret_cast<decltype(vm.entryPoint)>(1);
   auto address=[&](size_t n){return qvm?(intptr_t)n:reinterpret_cast<intptr_t>(memory.data()+n);};
   auto put=[&](size_t n,const char *s){std::strcpy(reinterpret_cast<char*>(memory.data()+n),s);};
   auto invoke=[&](intptr_t ordinal,std::initializer_list<intptr_t> v){
    std::array<intptr_t,5> args{};std::copy(v.begin(),v.end(),args.begin()+1);args[0]=ordinal;intptr_t result=-123;
    assert(SV_RTCWBotLibCall(&vm,Lookup(p.id,Module::GAME,Direction::IMPORT,ordinal),args.data(),result));return result;
   };
   put(1,"MAXCLIENTS");put(33,"32");assert(invoke(202,{address(1),address(33)})==0);
   put(1,"maxclients");memory.fill(0xa5);put(1,"maxclients");
   assert(invoke(203,{address(1),address(69),16})==0);
   assert(memory[68]==0xa5&&!std::strcmp(reinterpret_cast<char*>(memory.data()+69),"32")&&memory[72]==0xa5);
   assert(invoke(203,{address(1),address(69),1})==0&&memory[69]==0&&memory[70]=='2');
   assert(invoke(203,{address(1),0,0})==0);
   // Overlapping input/output is copied before the callee can overwrite it.
   assert(invoke(203,{address(1),address(1),16})==0&&memory[1]=='3'&&memory[2]=='2');
   put(1,"maxclients");auto before=memory;
   for(int n:{-1,MAX_STRING_CHARS+1,INT_MAX})rejected([&]{invoke(203,{address(1),address(69),n});});
   assert(memory==before);
   put(1,"BOT_TEST 7");assert(invoke(204,{address(1)})==73&&define=="BOT_TEST 7");
   rejected([&]{invoke(202,{0,address(33)});});
   std::fill_n(memory.data()+1024,1024,'a');rejected([&]{invoke(202,{address(1),address(1024)});});
   if(qvm){
    rejected([&]{invoke(202,{address(4095),address(33)});});
    rejected([&]{invoke(203,{address(1),address(4094),3});});
    rejected([&]{invoke(202,{4097,address(33)});});
    rejected([&]{invoke(202,{-1,address(33)});});
   }else{rejected([&]{SV_RTCWPointer(&vm,static_cast<intptr_t>(-1),2);});}
   assert(invoke(200,{})==0&&botlibglobals.maxclients==32);
   for(uint32_t bits:{0u,0x80000000u,0x3f800000u,0xbf800000u,0x7f7fffffu}){
    int argument;std::memcpy(&argument,&bits,4);intptr_t args[2]={205,argument},result=-1;
    frameResult=73;assert(SV_RTCWBotLibCall(&vm,Call::BOTLIB_START_FRAME,args,result)&&result==73);
    uint32_t actual;std::memcpy(&actual,&frameTime,4);assert(actual==bits);
   }
   for(uint32_t bits:{0x7fc00000u,0x7f800000u,0xff800000u}){
    int argument;std::memcpy(&argument,&bits,4);rejected([&]{invoke(205,{argument});});
   }
   if(sizeof(intptr_t)>4)rejected([&]{invoke(205,{static_cast<intptr_t>(INT64_C(0x100000000))});});
   put(1,"escape1");assert(invoke(206,{address(1)})==0&&loadedMap=="escape1"&&!nullMap);
   assert(invoke(206,{0})==0&&nullMap);
   mapResult=37;assert(invoke(206,{address(1)})==37);mapResult=0;
   std::fill_n(memory.data()+1024,MAX_QPATH,'a');rejected([&]{invoke(206,{address(1024)});});
   if(qvm){memory[4095]='a';rejected([&]{invoke(206,{address(4095)});});}
   assert(invoke(201,{})==0&&allocations.empty());
   intptr_t result=9;assert(!SV_RTCWBotLibCall(&vm,Call::G_PRINT,nullptr,result)&&result==9);
   botlib_export=nullptr;put(1,"maxclients");rejected([&]{invoke(203,{address(1),address(69),16});});
   rejected([&]{invoke(205,{0});});botlib_export=&be_botlib_export;
  }
 }
}
int main(){botimport.Print=print;checkLifecycle();checkArguments();assert(allocations.empty());}
'''


class RTCWBotLibTests(unittest.TestCase):
    def test_variable_allocation_failures_and_aliased_replacement(self):
        source = PRELUDE[:PRELUDE.index('static int Export_BotLibSetup();')]
        source = source.replace('void *GetMemory(unsigned long size){',
            'static int failAllocation=-1;\nvoid *GetMemory(unsigned long size){'
            'if(failAllocation==0)throw Drop{};if(failAllocation>0)--failAllocation;')
        source += r'''
int main(){
 for(bool set:{false,true})for(int fail=0;fail<=3;++fail){
  failAllocation=fail;bool failed=false;
  try{if(set)LibVarSet("name","value");else LibVar("name","value");}catch(Drop&){failed=true;}
  assert(failed==(fail<3));failAllocation=-1;LibVarDeAllocAll();assert(allocations.empty());
 }
 LibVarSet("name","12.5");auto *v=LibVarGet("name");auto *previous=v->string;
 failAllocation=0;try{LibVarSet("name","99");assert(false);}catch(Drop&){}
 assert(v->string==previous&&v->value==12.5f&&!strcmp(v->string,"12.5"));failAllocation=-1;
 LibVarSet("name",v->string);assert(!strcmp(v->string,"12.5"));
 LibVarSet("name",v->string+1);assert(!strcmp(v->string,"2.5")&&v->value==2.5f);
 LibVarDeAllocAll();assert(allocations.empty());
}
'''
        self.compile_and_run(source)

    def compile_and_run(self, source, extra=(), flags=()):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler required')
        with tempfile.TemporaryDirectory(prefix='rtcw-botlib-') as directory:
            folder = Path(directory); cpp = folder / 'probe.cpp'; exe = folder / 'probe.exe'
            cpp.write_text(source, encoding='utf-8')
            target = compiler_target_args(compiler)
            if os.name == 'nt' and not target:
                target = ['-DFNTECH3_Q3_ONLY']
            built = subprocess.run([compiler, *target, *flags, '-std=c++17', '-D_CRT_SECURE_NO_WARNINGS',
                                    '-I', str(ROOT), str(cpp), *extra, '-o', str(exe)],
                                   cwd=ROOT, capture_output=True, text=True, timeout=90)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            run = subprocess.run([str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=20)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)

    def test_lifecycle_profile_setup_failures_and_native_qvm_arguments(self):
        interface = (ROOT / 'code/botlib/be_interface.cpp').read_text()
        catalog = (ROOT / 'code/qcommon/game_profile.cpp').read_text()
        source = PRELUDE + catalog[catalog.index('const comGameProfile_t profiles[]'):catalog.index('\nstatic_assert(')]
        source += interface[interface.index('botlib_globals_t botlibglobals;'):interface.index('static constexpr float RETAIL_BOT_DEBUG')]
        for signature in ('static qboolean ValidEntityNumber(', 'static qboolean BotLibSetup(',
                          'static int Export_BotLibSetup( void )', 'static int Export_BotLibShutdown(void)',
                          'static int Export_BotLibVarSet(', 'static int Export_BotLibVarGet(',
                          'static int Export_BotLibLoadMap(', 'static int Export_BotLibUpdateEntity(',
                          'static int Export_BotLibStartFrame('):
            source += '\n' + function(interface, signature)
        source += CHECKS
        extra = [str(ROOT / 'code/server/server_game_data.cpp'), '-x', 'c++',
                 str(ROOT / 'code/compat/game_abi_adapter.c')]
        self.compile_and_run(source, extra)

    def test_numeric_settings_and_empty_action_shutdown(self):
        source = PRELUDE[:PRELUDE.index('static int Export_BotLibSetup();')]
        source += '\nbotlib_globals_t botlibglobals;\nstatic bot_input_t *botinputs;\nstatic unsigned actionExtras[MAX_GAME_CLIENTS];\n'
        actions = (ROOT / 'code/botlib/be_ea.cpp').read_text()
        source += function(actions, 'int EA_Setup(void)') + '\n' + function(actions, 'void EA_Shutdown(void)')
        source += r'''
int main(){
 EA_Shutdown();botlibglobals.maxclients=128;EA_Setup();assert(botinputs&&allocations.size()==1);
 EA_Shutdown();EA_Shutdown();assert(allocations.empty());
 const std::pair<const char*,float> values[]={{"0",0},{"1",1},{"1.",1},{".5",.5f},{"18.25",18.25f},
  {"-1",0},{"+2",0},{" 3",0},{"3 ",0},{"1e3",0},{"nan",0},{"inf",0},{"1..2",0},{"1.a",0}};
 for(auto &v:values){auto n=std::strlen(v.first)+1;auto *exact=(char*)std::malloc(n);std::memcpy(exact,v.first,n);
  assert(LibVarStringValue(exact)==v.second);std::free(exact);}
 for(int n:{8,9,38,128,1024}){
  auto fraction="0."+std::string(n,'9');float f=LibVarStringValue(fraction.c_str());assert(f>.99f&&f<=1.000001f);
  auto huge=std::string(n,'9');if(n>38)assert(LibVarStringValue(huge.c_str())==0);
 }
 // Replacing values, case-insensitive lookup and repeated cleanup use the
 // actual library allocation path, including text which is not numeric.
 LibVarSet("SomeVar","12.5");LibVarSet("somevar","maps/escape1_b0.aas");
 assert(LibVarGetValue("SOMEVAR")==0&&!std::strcmp(LibVarGetString("somevar"),"maps/escape1_b0.aas"));
 LibVarDeAllocAll();LibVarDeAllocAll();assert(allocations.empty());
}
'''
        self.compile_and_run(source, flags=['-O2', '-ffast-math'])


if __name__ == '__main__':
    unittest.main()
