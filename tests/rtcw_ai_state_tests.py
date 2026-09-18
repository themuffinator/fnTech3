"""Exercise real goal/movement pools and RTCW native/QVM lifecycle adapters."""
from pathlib import Path
import unittest
import rtcw_botlib_tests as bootstrap
from rtcw_server_world_tests import function

ROOT = Path(__file__).resolve().parents[1]


def state_source():
    source = bootstrap.PRELUDE[:bootstrap.PRELUDE.index('static int Export_BotLibSetup();')]
    source = source.replace('void *GetMemory(unsigned long size){',
        'static bool failAllocation;\nvoid *GetMemory(unsigned long size){if(failAllocation)throw Drop{};')
    catalog = (ROOT / 'code/qcommon/game_profile.cpp').read_text()
    source += catalog[catalog.index('const comGameProfile_t profiles[]'):catalog.index('\nstatic_assert(')]
    source += r'''
#include "code/botlib/be_ai_weight.h"
#include "code/botlib/be_ai_goal.h"
#include "code/botlib/be_ai_move.h"
#include "code/botlib/l_memory.h"
botlib_import_t botimport;
botlib_export_t *botlib_export;
void *GetClearedMemory(unsigned long size){return GetClearedHunkMemory(size);}
static void QDECL print(int,const char*,...){}
void FreeWeightConfig(weightconfig_t *config){FreeMemory(config);}
static void *itemconfig,*levelitemheap,*freelevelitems,*levelitems;
static int numlevelitems;
static void BotFreeInfoEntities(){}
'''
    goal = (ROOT / 'code/botlib/be_ai_goal.cpp').read_text()
    source += goal[goal.index('struct bot_goalstate_t'):goal.index('//item configuration')]
    for signature in ('bot_goalstate_t *BotGoalStateFromHandle(', 'void BotResetAvoidGoals(',
                      'void BotResetGoalState(', 'void BotFreeItemWeights(', 'int BotAllocGoalState(',
                      'void BotFreeGoalState(', 'void BotShutdownGoalAI('):
        source += '\n' + function(goal, signature)
    move = (ROOT / 'code/botlib/be_ai_move.cpp').read_text()
    source += move[move.index('struct bot_movestate_t'):move.index('//used to avoid reachability')]
    source += move[move.index('static bot_movestate_t *botmovestates'):move.index('//========================================================================', move.index('static bot_movestate_t *botmovestates'))]
    for signature in ('int BotAllocMoveState(', 'void BotFreeMoveState(', 'static bot_movestate_t *BotMoveStateFromHandle(',
                      'void BotInitMoveState(', 'static void BotClearAvoidReach(', 'void BotInitAvoidReach(',
                      'void BotResetAvoidReach(', 'void BotResetLastAvoidReach(',
                      'void BotResetMoveState(', 'void BotShutdownMoveAI('):
        source += '\n' + function(move, signature)
    source += '\n#include "code/server/server_rtcw_ai_state.hpp"\n'
    return source


CHECKS = r'''
using namespace fntech3::rtcw::module;
template<class F> void rejected(F f){try{f();assert(false);}catch(Drop&) {}}
template<class T> bool cleared(const T &v){
 const auto *bytes=reinterpret_cast<const byte*>(&v);for(size_t i=0;i<sizeof(v);++i)if(bytes[i])return false;return true;
}
static void checkPools(){
 for(const auto &p:profiles){profile=&p;const int count=p.maxClients;
  assert(count==(p.id==COM_GAME_RTCW_SP?128:p.id==COM_GAME_STVEF_SP?1:64));
  for(int invalid:{-1,0,count+1,INT_MAX}){
   assert(!BotGoalStateFromHandle(invalid)&&!BotMoveStateFromHandle(invalid));
   BotFreeGoalState(invalid);BotFreeMoveState(invalid);BotResetGoalState(invalid);BotResetMoveState(invalid);
  }
  for(int h=1;h<=count;++h){
   assert(BotAllocGoalState(h-1)==h&&BotAllocMoveState()==h);
   auto *gs=BotGoalStateFromHandle(h);auto *ms=BotMoveStateFromHandle(h);assert(gs&&ms);
   assert(gs->client==h-1&&cleared(*ms));gs->client=0;assert(cleared(*gs));gs->client=h-1;
   gs->lastreachabilityarea=31;gs->goalstacktop=5;gs->goalstack[3].number=4;
   gs->avoidgoals[0]=9;gs->avoidgoaltimes[0]=17;
   gs->itemweightconfig=botlib::ClearedAlloc<weightconfig_t>();gs->itemweightindex=botlib::ClearedAlloc<int>(4);
   auto *config=gs->itemweightconfig;auto *index=gs->itemweightindex;
   BotResetGoalState(h);assert(!gs->goalstacktop&&cleared(gs->goalstack)&&cleared(gs->avoidgoals)&&cleared(gs->avoidgoaltimes));
   assert(gs->client==h-1&&gs->lastreachabilityarea==31&&gs->itemweightconfig==config&&gs->itemweightindex==index);
   ms->avoidreach[0]=7;ms->avoidreachtimes[0]=4;ms->avoidreachtries[0]=3;ms->origin[0]=17;
   ms->lastareanum=19;ms->lastgoalareanum=23;ms->lastreachnum=29;
   BotResetLastAvoidReach(h);assert(ms->avoidreach[0]==7&&!ms->avoidreachtimes[0]&&ms->avoidreachtries[0]==2);
   BotInitAvoidReach(h);assert(cleared(ms->avoidreach)&&cleared(ms->avoidreachtimes)&&cleared(ms->avoidreachtries));
   assert(ms->lastareanum==19&&ms->lastgoalareanum==23&&ms->lastreachnum==29);
   ms->avoidreach[0]=7;ms->avoidreachtimes[0]=4;ms->avoidreachtries[0]=3;
   BotResetAvoidReach(h);assert(cleared(ms->avoidreach)&&cleared(ms->avoidreachtimes)&&cleared(ms->avoidreachtries)&&ms->origin[0]==17);
   assert(ms->lastareanum==(Com_GameIsRTCW()?0:19)&&ms->lastgoalareanum==(Com_GameIsRTCW()?0:23)&&ms->lastreachnum==(Com_GameIsRTCW()?0:29));
   BotResetMoveState(h);assert(cleared(*ms));
  }
  const auto full=allocations.size();assert(BotAllocGoalState(0)==0&&BotAllocMoveState()==0&&allocations.size()==full);
  BotFreeItemWeights(count);BotFreeItemWeights(count);assert(!botgoalstates[count]->itemweightconfig&&!botgoalstates[count]->itemweightindex);
  BotFreeGoalState(count);BotFreeMoveState(count);BotFreeGoalState(count);BotFreeMoveState(count);
  const auto remaining=allocations.size();failAllocation=true;
  rejected([&]{BotAllocGoalState(0);});rejected([&]{BotAllocMoveState();});
  assert(!botgoalstates[count]&&!botmovestates[count]&&allocations.size()==remaining);failAllocation=false;
  assert(BotAllocGoalState(count-1)==count&&BotAllocMoveState()==count);
  BotShutdownGoalAI();BotShutdownMoveAI();BotShutdownGoalAI();BotShutdownMoveAI();assert(allocations.empty());
  for(int h=0;h<=MAX_GAME_CLIENTS;++h)assert(!botgoalstates[h]&&!botmovestates[h]);
  assert(BotAllocGoalState(0)==1&&BotAllocMoveState()==1);BotShutdownGoalAI();BotShutdownMoveAI();assert(allocations.empty());
 }
}
static void checkInitFlags(){
 for(const auto &p:profiles){profile=&p;const bool rtcw=Com_GameIsRTCW();
  const int h=BotAllocMoveState();auto *ms=botmovestates[h];
  for(unsigned flags=0;flags<2048;++flags){
   ms->moveflags=static_cast<int>(flags^0x500007ffu);const auto original=ms->moveflags;
   ms->avoidreach[0]=49;ms->lastareanum=67;
   fntech3_rtcw_bot_initmove_t input{{1,2,3},{4,5,6},{7,8,9},1023,p.maxClients-1,.125f,2,{10,11,12},int(flags)};
   bot_initmove_t state{};
   if(rtcw)assert(fntech3::rtcw::bot::MoveState(input,p.maxClients,state));
   else {state.origin[0]=1;state.or_moveflags=int(flags);}
   BotInitMoveState(h,&state);
   const int mask=MFL_ONGROUND|MFL_TELEPORTED|MFL_WATERJUMP|MFL_WALK|(rtcw?0:MFL_GRAPPLEPULL);
   int expected=original&~mask;
   if(rtcw){if(flags&2)expected|=MFL_ONGROUND;if(flags&16)expected|=MFL_WATERJUMP;
    if(flags&32)expected|=MFL_TELEPORTED;if(flags&256)expected|=MFL_WALK;
    assert(ms->client==p.maxClients-1&&ms->entitynum==1023&&ms->thinktime==.125f&&ms->presencetype==2);
    assert(!memcmp(ms->velocity,input.velocity,12)&&!memcmp(ms->viewoffset,input.viewoffset,12)&&!memcmp(ms->viewangles,input.viewangles,12));
   }else expected|=flags&mask;
   assert(ms->moveflags==expected&&ms->origin[0]==1&&ms->avoidreach[0]==49&&ms->lastareanum==67);
  }
  BotShutdownMoveAI();assert(allocations.empty());
 }
}
static void checkImports(){
 botlib_export_t api{};auto &ai=api.ai;
 ai.BotAllocGoalState=BotAllocGoalState;ai.BotFreeGoalState=BotFreeGoalState;ai.BotResetGoalState=BotResetGoalState;
 ai.BotResetAvoidGoals=BotResetAvoidGoals;ai.BotAllocMoveState=BotAllocMoveState;ai.BotFreeMoveState=BotFreeMoveState;
 ai.BotResetMoveState=BotResetMoveState;ai.BotInitMoveState=BotInitMoveState;
 ai.BotResetAvoidReach=BotResetAvoidReach;ai.BotResetLastAvoidReach=BotResetLastAvoidReach;
 for(const auto &p:profiles){profile=&p;intptr_t result=17;
  if(!Com_GameIsRTCW()){assert(!SV_RTCWAIStateCall(nullptr,Call::BOTLIB_AI_ALLOC_MOVE_STATE,nullptr,result));continue;}
  for(bool native:{false,true}){
   std::array<byte,256> memory{};vm_t vm{};vm.dataBase=memory.data();vm.exactDataLength=256;vm.dataMask=255;
   if(native)vm.entryPoint=reinterpret_cast<decltype(vm.entryPoint)>(1);
   auto address=[&](size_t i){return native?reinterpret_cast<intptr_t>(memory.data()+i):(intptr_t)i;};
   auto invoke=[&](int ordinal,std::initializer_list<intptr_t> values){
    std::vector<intptr_t> words{ordinal};words.insert(words.end(),values.begin(),values.end());intptr_t output=99;
    assert(SV_RTCWAIStateCall(&vm,Lookup(p.id,Module::GAME,Direction::IMPORT,ordinal),words.data(),output));return output;
   };
   botlib_export=nullptr;assert(!SV_RTCWAIStateCall(nullptr,Call::G_PRINT,nullptr,result));
   rejected([&]{invoke(546,{0});});rejected([&]{invoke(555,{});});assert(allocations.empty());botlib_export=&api;
   const int goal=int(invoke(546,{p.maxClients-1})),move=int(invoke(555,{}));assert(goal==1&&move==1);
   for(int client:{-1,p.maxClients,INT_MAX})rejected([&]{invoke(546,{client});});
   auto *gs=botgoalstates[goal];gs->goalstacktop=3;gs->avoidgoals[0]=9;
   assert(invoke(526,{goal})==0&&!gs->avoidgoals[0]&&gs->goalstacktop==3);
   assert(invoke(525,{goal})==0&&!gs->goalstacktop&&gs->client==p.maxClients-1);
   auto *ms=botmovestates[move];
   fntech3_rtcw_bot_initmove_t input{{1,2,3},{4,5,6},{7,8,9},1023,p.maxClients-1,.125f,2,{10,11,12},256};
   auto write=[&]{memcpy(memory.data()+1,&input,sizeof(input));};write();auto before=memory;
   assert(invoke(557,{move,address(1)})==0&&memory==before&&ms->moveflags==MFL_WALK&&ms->client==p.maxClients-1);
   auto previous=*ms;
   for(size_t offset:{0u,4u,8u,12u,16u,20u,24u,28u,32u,44u,52u,56u,60u}){
    for(unsigned bits:{0x7f800000u,0xff800000u,0x7fc00001u}){
     write();memcpy(memory.data()+1+offset,&bits,4);rejected([&]{invoke(557,{move,address(1)});});
     assert(!memcmp(ms,&previous,sizeof(*ms)));}}
   write();rejected([&]{invoke(557,{move,0});});
   if(!native)for(int bad:{-1,189,255,257,INT_MAX})rejected([&]{invoke(557,{move,bad});});
   for(int invalid:{-1,128,INT_MAX}){input.client=invalid;write();rejected([&]{invoke(557,{move,address(1)});});}
   input.client=p.maxClients-1;
   for(int invalid:{-1,1024,INT_MAX}){input.entitynum=invalid;write();rejected([&]{invoke(557,{move,address(1)});});}
   input.entitynum=1023;write();memcpy(memory.data()+188,&input,sizeof(input));assert(invoke(557,{move,address(188)})==0);
   for(int ordinal:{551,558}){ms->avoidreach[0]=7;ms->avoidreachtimes[0]=2;ms->avoidreachtries[0]=4;
    ms->lastareanum=19;ms->lastgoalareanum=23;ms->lastreachnum=29;
    assert(invoke(ordinal,{move})==0&&cleared(ms->avoidreach)&&cleared(ms->avoidreachtimes)&&cleared(ms->avoidreachtries));
    assert(ms->lastareanum==(ordinal==551?0:19)&&ms->lastgoalareanum==(ordinal==551?0:23)&&ms->lastreachnum==(ordinal==551?0:29));}
   ms->avoidreachtimes[0]=9;ms->avoidreachtries[0]=2;assert(invoke(552,{move})==0&&!ms->avoidreachtimes[0]&&ms->avoidreachtries[0]==1);
   if(sizeof(intptr_t)>4){const auto tooWide=static_cast<intptr_t>(INT64_C(0x100000000));
    for(int ordinal:{546,547,525,526,556,548,551,552,558})rejected([&]{invoke(ordinal,{tooWide});});
    rejected([&]{invoke(557,{tooWide,address(1)});});}
   assert(invoke(548,{move})==0&&cleared(*ms));
   for(int invalid:{-1,0,p.maxClients+1,INT_MAX})
    for(int ordinal:{547,525,526,556,548,551,552,558})assert(invoke(ordinal,{invalid})==0);
   assert(invoke(547,{goal})==0&&invoke(556,{move})==0&&allocations.empty());
   BotShutdownGoalAI();BotShutdownMoveAI();botlib_export=nullptr;
  }
 }
}
int main(){botimport.Print=print;checkPools();checkInitFlags();checkImports();}
'''


class RTCWAIStateTests(unittest.TestCase):
    compile_and_run = bootstrap.RTCWBotLibTests.compile_and_run

    def test_profile_capacity_reset_ownership_and_checked_state_imports(self):
        self.compile_and_run(state_source() + CHECKS,
            [str(ROOT / 'code/server/server_game_data.cpp'), '-x', 'c++',
             str(ROOT / 'code/compat/game_abi_adapter.c')], ['-O2', '-ffast-math'])


if __name__ == '__main__':
    unittest.main()
