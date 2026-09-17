"""RTCW movement orchestration, routing look-ahead, ladders and module boundaries."""
from pathlib import Path
import unittest
import rtcw_travel_tests as travel
import rtcw_goal_route_tests as goals
import rtcw_route_query_tests as routes
import rtcw_routing_tests as routing
import rtcw_aas_query_tests as queries
from rtcw_server_world_tests import function

ROOT=Path(__file__).resolve().parents[1]


def goal_source():
    source=travel.travel_source()+r'''
static constexpr int MODELTYPE_FUNC_PLAT=1,MODELTYPE_FUNC_BOB=2,MODELTYPE_FUNC_DOOR=3,MODELTYPE_FUNC_STATIC=4;
static bool routingReady=true,futureEnabled=true;
static int selectedReach=2,selectionCalls,futureCalls,observedRouteFlags,observedMoveFlags;
static vec3_t futurePosition{128,64,32};
bool AAS_RTCWMovementRoutingReady(){return routingReady&&aasworld.loaded&&aasworld.initialized;}
int AAS_RTCWMovementReachability(const float*,int,int,int,int,int routeFlags,int moveFlags){
 ++selectionCalls;observedRouteFlags=routeFlags;observedMoveFlags=moveFlags;return selectedReach;
}
bool AAS_RTCWMovementFuturePosition(int,const float*,int,int,float *out){
 ++futureCalls;if(!futureEnabled)return false;VectorCopy(futurePosition,out);return true;
}
static int BotGetReachabilityToGoal(vec3_t,int,int,int,int*,float*,int*,bot_goal_t*,int flags,bot_avoidspot_t*,int,int*){
 return AAS_RTCWMovementReachability(nullptr,0,0,0,0,flags,flags);
}
int AAS_TravelFlagForType(int type){return type==TRAVEL_WALK?TFL_WALK:type==TRAVEL_JUMPPAD?TFL_JUMPPAD:
 type==TRAVEL_ELEVATOR?TFL_ELEVATOR:type==TRAVEL_FUNCBOB?TFL_FUNCBOB:type==TRAVEL_GRAPPLEHOOK?TFL_GRAPPLEHOOK:0;}
'''
    for filename,signatures in {
        'code/botlib/be_aas_sample.cpp':['qboolean AAS_PointInsideFace('],
        'code/botlib/be_aas_reach.cpp':['int AAS_AreaJumpPad('],
        'code/botlib/be_aas_route.cpp':['int AAS_NextAreaReachability(', 'int AAS_NextModelReachability('],
        'code/botlib/be_aas_move.cpp':['static int AAS_AgainstLadderWithFallback(', 'int AAS_AgainstLadder(',
            'bool AAS_RTCWAgainstLadder('],
        'code/botlib/be_ai_move.cpp':['int BotFuzzyPointReachabilityArea(', 'static int BotOnTopOfEntity(',
            'static int BotMoveTravelType(', 'static int BotReachabilityTime(', 'static bot_moveresult_t BotMoveInGoalArea(',
            'static bool BotRTCWMoveToGoalReady(', 'static bool BotRTCWTravelReady('],
    }.items():
        text=(ROOT/filename).read_text()
        for signature in signatures:source+='\n'+function(text,signature)
    move=(ROOT/'code/botlib/be_ai_move.cpp').read_text()
    source+='\n'+travel.active_function(move,'BotTravel_BFGJump')
    source+='\n'+function(move,'void BotMoveToGoal(')
    return source


SCENE=travel.SCENE+r'''
static bot_movestate_t *goalSetup(){
 auto *ms=travelSetup();aasworld.areas=botlib::ClearedAlloc<aas_area_t>(3);
 FreeMemory(aasworld.reachability);aasworld.reachabilitysize=8;
 aasworld.reachability=botlib::ClearedAlloc<aas_reachability_t>(8);
 for(int i=1;i<8;++i){auto &r=aasworld.reachability[i];r=link(40,0,1,80,0,1);r.areanum=2;r.traveltype=TRAVEL_WALK;}
 aasworld.areasettings[1].firstreachablearea=1;aasworld.areasettings[1].numreachableareas=3;
 aasworld.areasettings[2].firstreachablearea=4;aasworld.areasettings[2].numreachableareas=3;
 ms->moveflags=MFL_ONGROUND;ms->lastareanum=1;ms->lastgoalareanum=2;ms->reachability_time=10;ms->lasttime=4;
 ms->client=0;ms->entitynum=1000;selectedReach=2;selectionCalls=futureCalls=0;futureEnabled=routingReady=true;
 return ms;
}
static bot_goal_t destination(int area=2){bot_goal_t goal{};VectorSet(goal.origin,128,0,1);goal.areanum=area;return goal;}
static void goalFinish(){FreeMemory(aasworld.areas);aasworld.areas=nullptr;travelFinish();}
'''


class RTCWMoveGoalTests(unittest.TestCase):
    compile_and_run=travel.RTCWTravelTests.compile_and_run

    def run_source(self,checks):
        self.compile_and_run(goal_source()+SCENE+checks,
            [str(ROOT/'code/server/server_game_data.cpp'),'-x','c++',str(ROOT/'code/compat/game_abi_adapter.c')],
            ['-O2','-ffast-math'])

    def test_reuse_stuck_detection_goal_speed_future_view_and_quake_controls(self):
        self.run_source(r'''
int main(){for(const auto &p:profiles){profile=&p;auto *ms=goalSetup();bool rtcw=Com_GameIsRTCW();
 auto goal=destination();bot_moveresult_t result{};BotMoveToGoal(&result,1,&goal,TFL_WALK);
 assert(!result.failure&&!selectionCalls&&ms->lastreachnum==1);
 assert(result.traveltype==(rtcw?0:TRAVEL_WALK));assert(bool(result.flags&MOVERESULT_FUTUREVIEW)==rtcw);
 assert(futureCalls==int(rtcw)&&ms->lasttime==(rtcw?5:4));
 // Only RTCW invalidates a recently reused route when the actor has not moved.
 ms->lasttime=4.9f;selectionCalls=0;resetActions();BotMoveToGoal(&result,1,&goal,TFL_WALK);
 assert(selectionCalls==int(rtcw)&&ms->lastreachnum==(rtcw?2:1));
 assert(ms->reachability_time==10);assert(!ms->avoidreach[0]);
 // A future timestamp is not a stuck interval; neither is a half-second-old one.
 for(float previous:{5.1f,4.5f}){ms->lasttime=previous;selectionCalls=0;
  BotMoveToGoal(&result,1,&goal,TFL_WALK);assert(!selectionCalls);}
 goal=destination(1);VectorSet(goal.origin,5,0,1);goal.flags=GFL_NOSLOWAPPROACH;
 resetActions();BotMoveToGoal(&result,1,&goal,TFL_WALK);
 assert(!result.failure&&near(botinputs[0].speed,rtcw?400:20));
 assert(result.traveltype==TRAVEL_WALK&&!ms->lastreachnum&&!ms->lastareanum&&ms->lastgoalareanum==1);
 goal.flags=4;resetActions();BotMoveToGoal(&result,1,&goal,TFL_WALK);assert(near(botinputs[0].speed,20));
 goal.flags=0;goal.origin[0]=1;resetActions();BotMoveToGoal(&result,1,&goal,TFL_WALK);assert(!botinputs[0].speed);
 // RTCW intentionally has no solid-area failure when both area numbers are zero.
 aasworld.nodes[1].children[0]=0;goal.areanum=0;goal.origin[0]=50;
 BotMoveToGoal(&result,1,&goal,TFL_WALK);assert(bool(result.failure)==!rtcw);
 if(!rtcw)assert(result.type==RESULTTYPE_INSOLIDAREA);
 goalFinish();
}}
''')

    def test_movers_obstacles_jump_pad_recovery_and_raw_travel_types(self):
        self.run_source(r'''
int main(){for(const auto &p:profiles){profile=&p;bool rtcw=Com_GameIsRTCW();
 for(int modelType:{MODELTYPE_FUNC_PLAT,MODELTYPE_FUNC_BOB}){
  auto *ms=goalSetup();auto goal=destination();bot_moveresult_t result{};
  modeltypes[3]=modelType;obstruction=1;auto &r=aasworld.reachability[1];
  r.traveltype=modelType==MODELTYPE_FUNC_PLAT?TRAVEL_ELEVATOR:TRAVEL_FUNCBOB;
  ms->reachability_time=9;ms->lasttime=4.9f;VectorCopy(ms->origin,ms->lastorigin);
  BotMoveToGoal(&result,1,&goal,TFL_ELEVATOR|TFL_FUNCBOB);
  // Both original modes extend only func_bobbing; Quake also extends elevators.
  assert(near(ms->reachability_time,(rtcw&&modelType==MODELTYPE_FUNC_PLAT)?9:10));
  assert(!selectionCalls);modeltypes[3]=0;goalFinish();
 }
 auto *ms=goalSetup();auto goal=destination();bot_moveresult_t result{};obstruction=1;
 BotMoveToGoal(&result,1,&goal,TFL_WALK);assert(bool(result.flags&MOVERESULT_ONTOPOFOBSTACLE)==!rtcw);
 obstruction=ENTITYNUM_WORLD;ms->origin[2]=100;ms->moveflags=0;ms->lastreachnum=0;
 selectedReach=0;selectionCalls=futureCalls=0;aasworld.areasettings[1].contents=AREACONTENTS_JUMPPAD;
 aasworld.reachability[1].traveltype=aasworld.reachability[2].traveltype=TRAVEL_JUMPPAD;
 BotMoveToGoal(&result,1,&goal,TFL_WALK|TFL_JUMPPAD);
 assert(selectionCalls&&ms->lastreachnum==(rtcw?1:2)&&ms->lastareanum==1&&!futureCalls);
 assert(observedRouteFlags==(rtcw?(TFL_WALK|TFL_JUMPPAD):TFL_JUMPPAD)&&observedMoveFlags==TFL_JUMPPAD);
 auto &r=aasworld.reachability[1];r.traveltype=TRAVEL_WALK|TRAVELFLAG_NOTTEAM1;
 assert(BotMoveTravelType(&r)==(rtcw?r.traveltype:TRAVEL_WALK));
 r.traveltype=TRAVEL_BFGJUMP;assert(BotReachabilityTime(&r)==(rtcw?8:6));
 if(rtcw){for(int count:{0,-1,INT_MAX}){aasworld.areasettings[1].numreachableareas=count;
   assert(!AAS_NextAreaReachability(1,0));}}
 goalFinish();
}}
''')

    def test_partial_initialization_bad_state_and_untouched_optional_results(self):
        self.run_source(r'''
int main(){for(const auto &p:profiles){profile=&p;if(!Com_GameIsRTCW())continue;auto *ms=goalSetup();
 auto goal=destination();bot_moveresult_t result;memset(&result,0xff,sizeof(result));auto before=result;
 for(int handle:{INT_MIN,-1,0,p.maxClients+1,INT_MAX}){result=before;BotMoveToGoal(&result,handle,&goal,TFL_WALK);
  assert(!result.failure&&!result.type&&!result.blocked&&!result.blockentity&&!result.traveltype&&!result.flags);
  assert(!memcmp(&result.weapon,&before.weapon,28));}
 BotMoveToGoal(&result,1,nullptr,TFL_WALK);assert(result.failure&&!memcmp(&result.weapon,&before.weapon,28));
 const auto state=*ms;
 auto fail=[&](){result=before;if(botinputs)resetActions();else{moveActions.clear();commands.clear();bspCalls=0;}
  int calls=selectionCalls;BotMoveToGoal(&result,1,&goal,TFL_WALK);
  assert(result.failure&&!result.blocked&&!result.flags&&!memcmp(&result.weapon,&before.weapon,28));
  assert(moveActions.empty()&&commands.empty()&&!bspCalls&&selectionCalls==calls);};
 for(int client:{-1,p.maxClients,INT_MAX}){ms->client=client;fail();}*ms=state;
 ms->entitynum=1024;fail();*ms=state;
 for(int reach:{-1,aasworld.reachabilitysize,INT_MAX}){ms->lastreachnum=reach;fail();}*ms=state;
 for(unsigned bits:{0x7f800000u,0x7fc00001u,0x7f7fffffu}){
  memcpy(&ms->lasttime,&bits,4);fail();*ms=state;
  memcpy(&ms->velocity[0],&bits,4);fail();*ms=state;}
 ms->thinktime=131072;ms->velocity[0]=1;fail();*ms=state;
 auto saved=aasworld;aasworld.initialized=0;fail();aasworld=saved;
 aasworld.nodes=nullptr;fail();aasworld=saved;routingReady=false;fail();routingReady=true;
 auto imports=botimport;botimport.Trace=nullptr;fail();botimport=imports;
 botimport.BotClientCommand=nullptr;fail();botimport=imports;
 // Week-long server clocks remain valid, unlike absent or invalid mover data.
 aasworld.time=604800;ms->lasttime=604799;ms->reachability_time=604810;
 assert(BotRTCWMoveToGoalReady(ms,&goal));
 auto r=aasworld.reachability[1];r.traveltype=TRAVEL_FUNCBOB;r.facenum=4;assert(!BotRTCWTravelReady(r));
 r.facenum=3;assert(BotRTCWTravelReady(r));moverMins[0]=100;assert(!BotRTCWTravelReady(r));moverMins[0]=-32;
 EA_Shutdown();fail();goalFinish();
}}
''')

    def test_future_route_horizon_equal_cost_cycles_and_real_caches(self):
        future=function((ROOT/'code/botlib/be_aas_route.cpp').read_text(),'bool AAS_RTCWMovementFuturePosition(')
        source=goals.goal_route_source(True)+'\n'+future+routes.GRAPH+r'''
int main(){for(const auto &p:profiles){profile=&p;initialize();graph(0,4,1);
 vec3_t goal{777,88,99},out{7,8,9};const int flags=TFL_WALK|TFL_AIR;
 for(int i=1;i<=4;++i){auto &r=aasworld.reachability[i];r.areanum=i;VectorSet(r.end,10*i,0,0);}
 auto reset=[&](){for(int i=1;i<=4;++i){costs[i]=300;nextLinks[i]=i+1;}
  calls=0;VectorSet(out,7,8,9);};
 auto query=[&](int reach=1){return AAS_RTCWMovementFuturePosition(reach,goal,4,flags,out);};
 if(!Com_GameIsRTCW()){assert(!query()&&!calls);shutdown();continue;}
 bool sp=Com_GameIsRTCWSP();reset();assert(query());assert(out[0]==(sp?777:20));
 reset();costs[2]=costs[3]=180;assert(query()&&out[0]==(sp?777:20));
 reset();costs[2]=179;assert(query()&&out[0]==(sp?30:20));
 reset();costs[2]=301;assert(query()==!sp&&out[0]==(sp?7:20));
 reset();nextLinks[3]=1;assert(query()==!sp&&calls==(sp?4097:1));
 reset();assert(query(4)&&calls==0&&!memcmp(goal,out,12));
 for(int invalid:{-1,0,aasworld.reachabilitysize,INT_MAX}){reset();assert(!query(invalid)&&out[0]==7&&!calls);}
 reset();nextLinks[1]=-1;assert(!query()&&out[0]==7);
 reset();nextLinks[1]=0;aasworld.reachability[0].areanum=4;
 VectorSet(aasworld.reachability[0].end,40,0,0);assert(query()&&out[0]==(sp?777:40));
 reset();aasworld.reachability[1].areanum=-1;assert(!query()&&out[0]==7&&!calls);
 shutdown();
}}
'''
        self.compile_and_run(source,routing.RTCWRoutingTests.extra(),['-O2','-ffast-math'])
        source=goals.goal_route_source()+'\n'+future+routes.GRAPH+r'''
int main(){for(const auto &p:profiles){profile=&p;if(!Com_GameIsRTCW())continue;initialize();
 for(int world:{0,1}){AAS_SetCurrentWorld(world);graph(0,3,1);vec3_t goal{177,23,4},out{};
  auto &r=aasworld.reachability[4];assert(r.areanum==1);int time,next;const int flags=TFL_WALK|TFL_AIR;
  assert(AAS_AreaRouteToGoalArea(r.areanum,r.end,2,flags,&time,&next));
  assert(AAS_RTCWMovementFuturePosition(4,goal,2,flags,out));
  assert(!memcmp(out,Com_GameIsRTCWSP()?goal:aasworld.reachability[next].end,12));cacheLinks();
 }shutdown();
}}
'''
        self.compile_and_run(source,routing.RTCWRoutingTests.extra(),['-O2','-ffast-math'])

    def test_native_qvm_result_flags_owned_aliases_and_rejected_outputs(self):
        checks=r'''
static int backendCalls,behavior;static bool expectGoal;
static void moveBackend(bot_moveresult_t *out,int state,bot_goal_t *goal,int flags){
 ++backendCalls;assert(state==-1&&flags==INT_MIN&&bool(goal)==expectGoal);
 if(goal){assert(goal->origin[0]==3&&goal->origin[1]==4&&goal->origin[2]==5&&goal->areanum==2);
  assert(goal->flags==GFL_NOSLOWAPPROACH&&!goal->entitynum&&!goal->number&&!goal->iteminfo
   &&!goal->mins[0]&&!goal->maxs[2]);goal->origin[0]=79;}
 out->failure=out->blocked=out->blockentity=out->traveltype=out->flags=out->type=0;
 if(behavior==1){out->failure=out->blocked=1;out->blockentity=1000;out->traveltype=17;
  out->type=RESULTTYPE_INVISIBLEGRAPPLE;out->flags=255|MOVERESULT_FUTUREVIEW;out->weapon=5;
  VectorSet(out->movedir,262144,-262144,1);VectorSet(out->ideal_viewangles,0,90,0);}
 if(behavior==2)out->flags=MOVERESULT_BLOCKEDBYAVOIDSPOT;
 if(behavior==3)out->type=RESULTTYPE_WAITFORFUNCBOBBING;
 if(behavior==4)out->blocked=2;
 if(behavior==5)out->blockentity=1024;
 if(behavior==6){unsigned nan=0x7fc00001;memcpy(out->movedir,&nan,4);}
 if(behavior==7){unsigned nan=0x7fc00001;memcpy(out->ideal_viewangles,&nan,4);}
}
int main(){using namespace fntech3::rtcw::module;
 botlib_export_t api{};api.ai.BotMoveToGoal=moveBackend;
 for(const auto &p:profiles){profile=&p;
  if(!Com_GameIsRTCW()){intptr_t result=77;
   assert(!SV_RTCWBotLibCall(nullptr,Call::BOTLIB_AI_MOVE_TO_GOAL,nullptr,result)&&result==77);continue;}
  for(bool native:{false,true}){
   std::array<byte,256> memory;vm_t vm{};vm.dataBase=memory.data();vm.exactDataLength=256;vm.dataMask=255;
   if(native)vm.entryPoint=reinterpret_cast<decltype(vm.entryPoint)>(1);
   auto address=[&](int n){return native?reinterpret_cast<intptr_t>(memory.data()+n):intptr_t(n);};
   intptr_t args[5]{};int outputAt;
   auto reset=[&](int goalAt=17,int targetAt=97){memory.fill(0xa5);outputAt=targetAt;
    fntech3_rtcw_bot_goal_t goal;memset(&goal,0xff,sizeof(goal));
    VectorSet(goal.origin,3,4,5);goal.areanum=2;goal.flags=-1;memcpy(memory.data()+goalAt,&goal,56);
    args[0]=549;args[1]=address(targetAt);args[2]=-1;args[3]=address(goalAt);args[4]=INT_MIN;
    botlib_export=&api;api.ai.BotMoveToGoal=moveBackend;behavior=0;expectGoal=true;
   };
   auto invoke=[&](){intptr_t result=77;
    assert(SV_RTCWBotLibCall(&vm,Lookup(p.id,Module::GAME,Direction::IMPORT,549),args,result));assert(result==0);};
   auto fail=[&](){auto before=memory;int count=backendCalls;rejected([&]{invoke();});assert(memory==before&&backendCalls==count);};
   for(auto locations:std::array<std::array<int,2>,4>{{{17,97},{17,17},{17,1},{200,204}}}){
    for(int write:{0,1}){reset(locations[0],locations[1]);behavior=write;auto expected=memory;
     fntech3_rtcw_bot_moveresult_t output;memcpy(&output,expected.data()+outputAt,52);
     if(write){output.failure=output.blocked=1;output.blockentity=1000;output.traveltype=17;output.flags=511;output.type=2;
      output.weapon=5;VectorSet(output.movedir,262144,-262144,1);VectorSet(output.ideal_viewangles,0,90,0);}
     else output.failure=output.type=output.blocked=output.blockentity=output.traveltype=output.flags=0;
     memcpy(expected.data()+outputAt,&output,52);invoke();assert(memory==expected);
    }
   }
   reset();args[3]=0;expectGoal=false;unsigned nan=0x7fc00001;
   memcpy(memory.data()+outputAt+28,&nan,4);memcpy(memory.data()+outputAt+40,&nan,4);
   auto expected=memory;memset(expected.data()+outputAt,0,24);invoke();assert(memory==expected);
   reset();botlib_export=nullptr;fail();reset();api.ai.BotMoveToGoal=nullptr;fail();
   for(int index:{1,3}){reset();args[index]=-1;fail();
    if(!native){reset();args[index]=index==1?205:201;fail();}}
   reset();args[1]=0;fail();
   for(unsigned invalid:{0x48000001u,0x7f800000u,0x7fc00001u}){reset();memcpy(memory.data()+17,&invalid,4);fail();}
   for(int invalid:{-1,65535,INT_MAX}){reset();memcpy(memory.data()+29,&invalid,4);fail();}
   if(sizeof(intptr_t)>4)for(int index:{2,4}){reset();args[index]=static_cast<intptr_t>(INT64_C(0x100000000));fail();}
   for(int bad:{2,3,4,5,6,7}){reset();behavior=bad;auto before=memory;int count=backendCalls;
    rejected([&]{invoke();});assert(memory==before&&backendCalls==count+1);}
  }
 }
}
'''
        self.compile_and_run(queries.query_source()+checks,
            [str(ROOT/'code/server/server_game_data.cpp'),'-x','c++',str(ROOT/'code/compat/game_abi_adapter.c')],
            ['-O2','-ffast-math'])

    def test_ladder_fallback_geometry_bounds_and_partial_shutdown(self):
        self.run_source(r'''
int main(){for(const auto &p:profiles){profile=&p;auto *ms=goalSetup();bool rtcw=Com_GameIsRTCW();
 aasworld.faceindexsize=1;aasworld.faceindex=botlib::ClearedAlloc<int>(1);aasworld.faceindex[0]=1;
 aasworld.numfaces=2;aasworld.faces=botlib::ClearedAlloc<aas_face_t>(2);
 aasworld.edgeindexsize=4;aasworld.edgeindex=botlib::ClearedAlloc<int>(4);
 aasworld.numedges=4;aasworld.edges=botlib::ClearedAlloc<aas_edge_t>(4);
 aasworld.numvertexes=4;aasworld.vertexes=botlib::ClearedAlloc<vec3_t>(4);
 const float vertices[4][3]={{-2,-2,0},{-2,2,0},{2,2,0},{2,-2,0}};
 memcpy(aasworld.vertexes,vertices,sizeof(vertices));
 for(int i=0;i<4;++i){aasworld.edgeindex[i]=i;aasworld.edges[i].v[0]=i;aasworld.edges[i].v[1]=(i+1)%4;}
 auto &area=aasworld.areas[1];area.numfaces=1;
 auto &face=aasworld.faces[1];face.faceflags=FACE_LADDER;face.numedges=4;
 aasworld.areasettings[1].areaflags=AREA_LADDER;vec3_t point{0,0,1};
 assert(AAS_AgainstLadder(point));assert(AAS_RTCWAgainstLadder(point,0)==rtcw);
 point[0]=3;assert(!AAS_AgainstLadder(point));point[0]=0;
 point[2]=3;assert(!AAS_AgainstLadder(point));point[2]=2.99f;assert(AAS_AgainstLadder(point));point[2]=1;
 aasworld.nodes[1].children[0]=0;assert(!AAS_AgainstLadder(point));
 assert(AAS_RTCWAgainstLadder(point,1)==rtcw);
 for(int previous:{-1,0,3,INT_MAX})assert(!AAS_RTCWAgainstLadder(point,previous));
 if(rtcw){
  auto saved=aasworld;aasworld.planes=nullptr;assert(!AAS_RTCWAgainstLadder(point,1));aasworld=saved;
  aasworld.initialized=0;assert(!AAS_RTCWAgainstLadder(point,1));aasworld=saved;
  for(int bad:{INT_MIN,2,INT_MAX}){aasworld.faceindex[0]=bad;assert(!AAS_RTCWAgainstLadder(point,1));}
  aasworld.faceindex[0]=1;face.firstedge=1;assert(!AAS_RTCWAgainstLadder(point,1));face.firstedge=0;
  aasworld.edgeindex[0]=INT_MIN;assert(!AAS_RTCWAgainstLadder(point,1));aasworld.edgeindex[0]=0;
  aasworld.edges[0].v[0]=4;assert(!AAS_RTCWAgainstLadder(point,1));aasworld.edges[0].v[0]=0;
  face.planenum=2;assert(!AAS_RTCWAgainstLadder(point,1));face.planenum=0;
  assert(AAS_RTCWAgainstLadder(point,1));
 }
 for(void *storage:{(void*)aasworld.faceindex,(void*)aasworld.faces,(void*)aasworld.edgeindex,
      (void*)aasworld.edges,(void*)aasworld.vertexes})FreeMemory(storage);
 aasworld.faceindex=nullptr;aasworld.faces=nullptr;aasworld.edgeindex=nullptr;aasworld.edges=nullptr;aasworld.vertexes=nullptr;
 goalFinish();
}}
''')


if __name__=='__main__':unittest.main()
