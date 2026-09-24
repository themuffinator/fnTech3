"""Exercise RTCW movement views, route traversal and owned native/QVM goals."""
from pathlib import Path
import unittest
import rtcw_goal_route_tests as goals
import rtcw_route_query_tests as routes
import rtcw_routing_tests as routing
from rtcw_server_world_tests import function

ROOT=Path(__file__).resolve().parents[1]


def view_source():
    source=goals.goal_route_source().replace('int AAS_RTCWMovementReachability(', 'int Real_RTCWMovementReachability(',1)
    source+=r'''
#include "code/botlib/be_ai_move.h"
#include "code/botlib/be_aas_move.h"
#include "code/botlib/rtcw_movement.hpp"
#define AVOIDREACH
static constexpr int AVOIDREACH_TRIES=4;
static int forceNext=-1,selectorCalls,traceCalls,visibleTrace,expectedPass;
static vec3_t expectedEye;
template<class F>static void rejected(F &&call){try{call();}catch(Drop&){return;}assert(false);}
int AAS_RTCWMovementReachability(const float *origin,int area,int lastGoal,int previous,int goal,int flags,int moveFlags){
 ++selectorCalls;return forceNext>=0?forceNext:Real_RTCWMovementReachability(origin,area,lastGoal,previous,goal,flags,moveFlags);
}
// Quake control cases have no avoid spots. RTCW does not use this service.
static int BotAvoidSpots(vec3_t,aas_reachability_t*,bot_avoidspot_t*,int count){assert(!count);return 0;}
static void sightTrace(bsp_trace_t *out,vec3_t start,vec3_t mins,vec3_t maxs,vec3_t end,int pass,int mask){
 ++traceCalls;assert(pass==expectedPass&&mask==(CONTENTS_SOLID|CONTENTS_PLAYERCLIP));
 assert(!memcmp(start,expectedEye,12)&&!mins&&!maxs);
 *out={};out->fraction=traceCalls==visibleTrace?1:.5f;VectorCopy(end,out->endpos);
}
'''
    move=(ROOT/'code/botlib/be_ai_move.cpp').read_text()
    source+='\n'+move[move.index('struct bot_movestate_t'):move.index('//used to avoid reachability')]
    start=move.index('static libvar_t *sv_maxstep')
    source+='\n'+move[start:move.index('//========================================================================',start)]
    for filename,signatures in {
        'code/qcommon/q_math.c':['vec_t VectorNormalize('],
        'code/botlib/be_aas_route.cpp':['void AAS_ReachabilityFromNum(', 'int AAS_NextAreaReachability('],
        'code/botlib/be_aas_bspq3.cpp':['bsp_trace_t AAS_Trace('],
        'code/botlib/be_ai_move.cpp':['int BotAllocMoveState(', 'void BotFreeMoveState(',
            'static bot_movestate_t *BotMoveStateFromHandle(', 'void BotInitMoveState(',
            'static int BotValidTravel(', 'int BotGetReachabilityToGoal(', 'static int BotAddToTarget(',
            'int BotMovementViewTarget(', 'static int BotVisible(', 'int BotPredictVisiblePosition(',
            'void BotShutdownMoveAI('],
    }.items():
        text=(ROOT/filename).read_text()
        for signature in signatures:source+='\n'+function(text,signature)
    return source


SCENE=routes.GRAPH+r'''
static int prepare(){
 initialize();graph(0,3,1);botimport.Trace=sightTrace;
 forceNext=-1;selectorCalls=traceCalls=visibleTrace=0;expectedPass=1000;
 int state=BotAllocMoveState();assert(state==1);bot_initmove_t init{};init.origin[0]=24;
 init.thinktime=.1f;BotInitMoveState(state,&init);
 auto *ms=botmovestates[state];ms->lastreachnum=5;ms->lastareanum=3;ms->lastgoalareanum=2;
 VectorClear(aasworld.reachability[5].start);aasworld.reachability[5].start[0]=28;
 VectorClear(aasworld.reachability[5].end);aasworld.reachability[5].end[0]=40;
 return state;
}
static void finish(){BotShutdownMoveAI();shutdown();}
'''


class RTCWMovementViewTests(unittest.TestCase):
    compile_and_run=routing.RTCWRoutingTests.compile_and_run

    def run_source(self,checks):
        self.compile_and_run(view_source()+SCENE+checks,routing.RTCWRoutingTests.extra(),['-O2','-ffast-math'])

    def test_view_distances_travel_policies_partial_failure_and_cycle_bound(self):
        self.run_source(r'''
int main(){
 const int flags=TFL_WALK|TFL_AIR;
 for(const auto &p:profiles){profile=&p;if(!p.bots.navigationWorlds)continue;const bool rtcw=Com_GameIsRTCW();int state=prepare();
  bot_goal_t goal{};goal.areanum=2;goal.origin[0]=50;
  auto view=[&](float look,int travel,int expected,int success=1){vec3_t out{-11,-13,-17};
   aasworld.reachability[5].traveltype=travel;aasworld.frameroutingupdates=0;
   assert(BotMovementViewTarget(state,&goal,flags,look,out)==success);
   assert(fabs(out[0]-expected)<.01f&&!out[1]&&!out[2]);};
  view(2,TRAVEL_WALK,26);view(10,TRAVEL_WALK,34);view(100,TRAVEL_WALK,50);
  view(100,TRAVEL_TELEPORT,28);
  view(100,TRAVEL_ROCKETJUMP,rtcw?50:28);view(100,TRAVEL_BFGJUMP,rtcw?50:28);
  view(100,TRAVEL_TELEPORT|TRAVELFLAG_NOTTEAM1,rtcw?50:28);
  for(int travel:{TRAVEL_JUMPPAD,TRAVEL_ELEVATOR,TRAVEL_FUNCBOB})view(10,travel,46);
  vec3_t out{-11,-13,-17};const auto before=std::array<float,3>{-11,-13,-17};
  for(float look:{0.0f,-1.0f})assert(!BotMovementViewTarget(state,&goal,flags,look,out)&&!memcmp(out,before.data(),12));
  if(rtcw){
   goal.areanum=0;view(100,TRAVEL_WALK,40,0);goal.areanum=2;
   for(int bad:{INT_MIN,-1,0,129,INT_MAX})assert(!BotMovementViewTarget(bad,&goal,flags,100,out));
   assert(!BotMovementViewTarget(state,nullptr,flags,100,out));
   botmovestates[state]->lastreachnum=aasworld.reachabilitysize;
   assert(!BotMovementViewTarget(state,&goal,flags,100,out));botmovestates[state]->lastreachnum=5;
   goal.areanum=1;forceNext=5;selectorCalls=0;
   VectorClear(botmovestates[state]->origin);VectorClear(aasworld.reachability[5].start);VectorClear(aasworld.reachability[5].end);
   assert(!BotMovementViewTarget(state,&goal,flags,100,out)&&selectorCalls==4096);
   assert(!out[0]&&!out[1]&&!out[2]);forceNext=-1;
   aasworld.initialized=0;assert(!BotMovementViewTarget(state,&goal,flags,100,out));aasworld.initialized=1;
  }
  finish();
 }
}
''')

    def test_visible_start_end_occluded_goal_same_area_and_twenty_hop_limit(self):
        self.run_source(r'''
int main(){
 const int flags=TFL_WALK|TFL_AIR;
 for(const auto &p:profiles){profile=&p;if(!Com_GameIsRTCW())continue;prepare();
  bot_goal_t goal{};goal.areanum=2;goal.entitynum=expectedPass;goal.origin[0]=50;VectorCopy(goal.origin,expectedEye);
  vec3_t origin{24,0,0},out{-11,-13,-17};
  for(int visible:{0,1,2}){traceCalls=0;visibleTrace=visible;aasworld.frameroutingupdates=0;
   assert(BotPredictVisiblePosition(origin,3,&goal,flags,out));
   assert(out[0]==(visible==1?28:40)&&!out[1]&&!out[2]&&traceCalls==(visible==1?1:2));
  }
  out[0]=-11;out[1]=-13;out[2]=-17;const auto before=std::array<float,3>{-11,-13,-17};
  traceCalls=0;assert(!BotPredictVisiblePosition(origin,2,&goal,flags,out)&&!traceCalls&&!memcmp(out,before.data(),12));
  goal.areanum=1;forceNext=5;visibleTrace=0;selectorCalls=traceCalls=0;
  assert(!BotPredictVisiblePosition(origin,3,&goal,flags,out)&&selectorCalls==20&&traceCalls==40);
  assert(!memcmp(out,before.data(),12));forceNext=-1;
  goal.entitynum=1024;assert(!BotPredictVisiblePosition(origin,3,&goal,flags,out));goal.entitynum=expectedPass;
  botimport.Trace=nullptr;assert(!BotPredictVisiblePosition(origin,3,&goal,flags,out));
  finish();
 }
}
''')

    def test_native_qvm_goal_projection_aliases_partial_outputs_and_rejected_results(self):
        self.run_source(r'''
static int backendCalls,behavior,returned,seenFlags;
static bool expectGoal;
static int output(bot_goal_t *goal,vec3_t target){
 ++backendCalls;assert(bool(goal)==expectGoal);
 if(goal){assert(goal->origin[0]==3&&goal->origin[1]==4&&goal->origin[2]==5&&goal->areanum==2);
  assert(!goal->flags&&!goal->number&&!goal->iteminfo&&!goal->mins[0]&&!goal->maxs[2]);goal->origin[0]=79;}
 if(behavior){target[0]=7;target[1]=8;target[2]=9;}
 if(behavior==2){unsigned nan=0x7fc00001u;memcpy(target,&nan,4);}
 if(behavior==3)target[0]=131073;
 return returned;
}
static int viewBackend(int state,bot_goal_t *goal,int flags,float look,vec3_t target){
 assert(state==-1&&look==100);seenFlags=flags;if(goal)assert(!goal->entitynum);return output(goal,target);
}
static int visibleBackend(vec3_t origin,int area,bot_goal_t *goal,int flags,vec3_t target){
 assert(origin[0]==1&&origin[1]==2&&origin[2]==3&&area==3);origin[0]=88;seenFlags=flags;
 if(goal)assert(goal->entitynum==1000);return output(goal,target);
}
int main(){
 using namespace fntech3::rtcw::module;
 botlib_export_t api{};api.ai.BotMovementViewTarget=viewBackend;api.ai.BotPredictVisiblePosition=visibleBackend;
 for(const auto &p:profiles){profile=&p;
  if(!Com_GameIsRTCW()){intptr_t result=77;
   for(Call call:{Call::BOTLIB_AI_MOVEMENT_VIEW_TARGET,Call::BOTLIB_AI_PREDICT_VISIBLE_POSITION})
    assert(!SV_RTCWBotLibCall(nullptr,call,nullptr,result)&&result==77);continue;
  }
  for(bool native:{false,true})for(bool visible:{false,true}){
   std::array<byte,256> memory;vm_t vm{};vm.dataBase=memory.data();vm.exactDataLength=256;vm.dataMask=255;
   if(native)vm.entryPoint=reinterpret_cast<decltype(vm.entryPoint)>(1);
   auto address=[&](int n){return native?reinterpret_cast<intptr_t>(memory.data()+n):intptr_t(n);};
   auto word=[](float f){int bits;memcpy(&bits,&f,4);return bits;};
   intptr_t args[6]{};int outputAt=97;
   auto reset=[&](int goalAt=17,int targetAt=97){memory.fill(0xa5);outputAt=targetAt;
    fntech3_rtcw_bot_goal_t goal;memset(&goal,0xff,sizeof(goal));
    goal.origin[0]=3;goal.origin[1]=4;goal.origin[2]=5;goal.areanum=2;goal.flags=4;goal.entitynum=visible?1000:INT_MIN;
    vec3_t origin{1,2,3};memcpy(memory.data()+1,origin,12);memcpy(memory.data()+goalAt,&goal,56);
    args[0]=visible?573:554;args[1]=visible?address(1):-1;args[2]=visible?3:address(goalAt);
    args[3]=visible?address(goalAt):INT_MIN;args[4]=visible?INT_MIN:word(100);args[5]=address(targetAt);
    botlib_export=&api;behavior=0;returned=0;expectGoal=true;
   };
   auto invoke=[&](){intptr_t result=77;assert(SV_RTCWBotLibCall(&vm,Lookup(p.id,Module::GAME,Direction::IMPORT,int(args[0])),args,result));return result;};
   auto fail=[&](){auto before=memory;int count=backendCalls;rejected([&]{invoke();});assert(memory==before&&backendCalls==count);};
   reset();auto before=memory;assert(invoke()==0&&memory==before&&seenFlags==INT_MIN);
   // Arbitrary untouched output bytes, including NaNs, remain valid on failure.
   unsigned nan=0x7fc00001u;memcpy(memory.data()+outputAt,&nan,4);before=memory;assert(!invoke()&&memory==before);
   for(int success:{0,1})for(auto locations:std::array<std::array<int,2>,4>{{{17,97},{17,17},{17,1},{200,244}}}){
    reset(locations[0],locations[1]);returned=success;behavior=1;before=memory;
    assert(invoke()==success);vec3_t expected{7,8,9};memcpy(before.data()+outputAt,expected,12);assert(memory==before);
   }
   reset();args[visible?3:2]=0;expectGoal=false;before=memory;assert(!invoke()&&memory==before);
   reset();botlib_export=nullptr;fail();botlib_export=&api;
   for(int index:{visible?3:2,5}){reset();args[index]=-1;fail();
    if(!native){reset();args[index]=index==5?245:201;fail();}}
   reset();args[5]=0;fail();
   for(unsigned invalid:{0x48000001u,0x7f800000u,0x7fc00001u}){
    reset();memcpy(memory.data()+17,&invalid,4);fail();
    reset();if(visible)memcpy(memory.data()+1,&invalid,4);else args[4]=int(invalid);fail();
   }
   for(int invalid:{-1,65535,INT_MAX}){reset();memcpy(memory.data()+29,&invalid,4);fail();
    if(visible){reset();args[2]=invalid;fail();}}
   if(visible){reset();int invalid=1024;memcpy(memory.data()+57,&invalid,4);fail();}
   if(sizeof(intptr_t)>4){const intptr_t wide=static_cast<intptr_t>(INT64_C(0x100000000));
    for(int index:std::array<int,2>{visible?2:1,visible?4:3}){reset();args[index]=wide;fail();}
    if(!visible){reset();args[4]=wide;fail();}}
   for(int bad:{-1,2,INT_MAX}){reset();returned=bad;behavior=1;before=memory;int count=backendCalls;
    rejected([&]{invoke();});assert(memory==before&&backendCalls==count+1);}
   for(int corrupt:{2,3}){reset();behavior=corrupt;before=memory;int count=backendCalls;
    rejected([&]{invoke();});assert(memory==before&&backendCalls==count+1);}
  }
 }
}
''')


if __name__=='__main__':unittest.main()
