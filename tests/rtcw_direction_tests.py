"""Exercise directional RTCW movement, elementary actions and syscall arguments."""
from pathlib import Path
import unittest
from rtcw_prediction_tests import prediction_source, SCENE
from rtcw_server_world_tests import function
import rtcw_botlib_tests as bootstrap

ROOT=Path(__file__).resolve().parents[1]


def direction_source(scripted_prediction=False):
    source=prediction_source().replace('bool EA_ClientInputAvailable(int){assert(false);return false;}','')
    source += r'''
#include "code/botlib/be_ai_goal.h"
#include "code/botlib/be_ai_move.h"
#include "code/botlib/be_ea.h"
botlib_globals_t botlibglobals;
static bot_input_t *botinputs;
static unsigned actionExtras[MAX_GAME_CLIENTS];
static constexpr int MAX_USERMOVE=400,PREDICTIONTIME_JUMP=3;
static int brushTypes;
void BotSetBrushModelTypes(){++brushTypes;}
'''
    ea=(ROOT/'code/botlib/be_ea.cpp').read_text()
    for signature in ['bool EA_ClientInputAvailable(', 'void EA_Crouch(', 'void EA_Jump(', 'void EA_Move(',
                      'void EA_ResetInput(', 'void EA_GetInput(', 'int EA_Setup(', 'void EA_Shutdown(']:
        source += '\n'+function(ea,signature)
    move=(ROOT/'code/botlib/be_ai_move.cpp').read_text()
    source += '\n'+move[move.index('struct bot_movestate_t'):move.index('//used to avoid reachability')]
    start=move.index('static libvar_t *sv_maxstep')
    source += '\n'+move[start:move.index('//========================================================================',start)]
    math=(ROOT/'code/qcommon/q_math.c').read_text()
    source += '\n'+function(math,'vec_t VectorNormalize2(')
    if scripted_prediction:
        source=source.replace('int AAS_PredictClientMovement(', 'int Real_AAS_PredictClientMovement(',1)
        source += r'''
static aas_clientmove_t predictedMove;
static int predictionCalls,predictionResult=1;
int AAS_PredictClientMovement(aas_clientmove_t *out,int entity,vec3_t origin,int presence,int ground,
 vec3_t velocity,vec3_t command,int commandFrames,int maximumFrames,float duration,int events,int area,int visualize){
 ++predictionCalls;assert(entity==0&&origin[2]==1.5f&&ground==1&&duration==.1f&&!area&&!visualize);
 assert(presence==PRESENCE_NORMAL||presence==PRESENCE_CROUCH);
 assert(commandFrames==1||commandFrames==2);assert(maximumFrames==2||maximumFrames==30);
 assert(events&SE_ENTERLAVA);*out=predictedMove;return predictionResult;
}
'''
    for signature in ['int BotAllocMoveState(', 'void BotFreeMoveState(', 'static bot_movestate_t *BotMoveStateFromHandle(',
                      'void BotInitMoveState(', 'static float BotGapDistance(', 'static int BotCheckBarrierJump(',
                      'static int BotSwimInDirection(', 'static int BotWalkInDirection(', 'int BotMoveInDirection(',
                      'int BotSetupMoveAI(', 'void BotShutdownMoveAI(']:
        source += '\n'+function(move,signature)
    return source


GEOMETRY=SCENE+r'''
static void noEntityHit(bsp_trace_t *out,vec3_t,vec3_t,vec3_t,vec3_t end,int,int){
 *out={};out->fraction=1;VectorCopy(end,out->endpos);
}
static int prepare(){
 setup();AAS_InitAASLinkedEntities();botimport.EntityTrace=noEntityHit;
 botlibglobals.maxclients=profile->maxClients;assert(EA_Setup()==0);
 assert(BotSetupMoveAI()==0);const int state=BotAllocMoveState();assert(state==1);
 bot_initmove_t input{};input.origin[2]=1;input.thinktime=.1f;input.presencetype=PRESENCE_NORMAL;
 BotInitMoveState(state,&input);return state;
}
static void finish(){BotShutdownMoveAI();EA_Shutdown();stop();}
static void gapGeometry(){
 FreeMemory(aasworld.nodes);FreeMemory(aasworld.planes);FreeMemory(aasworld.areasettings);
 aasworld.numareas=aasworld.numareasettings=3;aasworld.areasettings=botlib::ClearedAlloc<aas_areasettings_t>(3);
 for(int area:{1,2})aasworld.areasettings[area].presencetype=PRESENCE_NORMAL|PRESENCE_CROUCH;
 aasworld.numnodes=4;aasworld.nodes=botlib::ClearedAlloc<aas_node_t>(4);
 aasworld.numplanes=6;aasworld.planes=botlib::ClearedAlloc<aas_plane_t>(6);
 aasworld.planes[0].normal[0]=1;aasworld.planes[0].dist=20;
 aasworld.planes[1].normal[0]=-1;aasworld.planes[1].dist=-20;
 for(int p:{2,4}){aasworld.planes[p].normal[2]=1;aasworld.planes[p+1].normal[2]=-1;}
 aasworld.planes[4].dist=-64;aasworld.planes[5].dist=64;
 aasworld.nodes[1].planenum=0;aasworld.nodes[1].children[0]=3;aasworld.nodes[1].children[1]=2;
 aasworld.nodes[2].planenum=2;aasworld.nodes[2].children[0]=-1;aasworld.nodes[2].children[1]=0;
 aasworld.nodes[3].planenum=4;aasworld.nodes[3].children[0]=-2;aasworld.nodes[3].children[1]=0;
 AAS_InitAASLinkedEntities();
}
'''


class RTCWDirectionTests(unittest.TestCase):
    compile_and_run=bootstrap.RTCWBotLibTests.compile_and_run

    def run_source(self,checks,scripted=False):
        self.compile_and_run(direction_source(scripted)+GEOMETRY+checks,
            [str(ROOT/'code/server/server_game_data.cpp'),'-x','c++',str(ROOT/'code/compat/game_abi_adapter.c')],
            ['-O2','-ffast-math'])

    def test_real_prediction_ground_flag_liquids_snapshot_settings_and_shutdown(self):
        self.run_source(r'''
int main(){
 for(const auto &p:profiles){profile=&p;const bool rtcw=Com_GameIsRTCW();int state=prepare();auto *ms=botmovestates[state];
  vec3_t direction{1,0,0};assert(BotMoveInDirection(state,direction,200,MOVE_WALK)==1);
  assert(botinputs[0].speed==(rtcw?0:200));assert(bool(ms->moveflags&MFL_ONGROUND)==!rtcw);
  EA_ResetInput(0);ms->moveflags=MFL_ONGROUND;
  assert(BotMoveInDirection(state,direction,200,MOVE_WALK)==1&&botinputs[0].speed==200);
  assert(fabs(botinputs[0].dir[0]-1)<(rtcw?.000001f:.002f)
    &&!botinputs[0].dir[1]&&!botinputs[0].dir[2]);
  contents=CONTENTS_WATER;direction[0]=3;direction[1]=4;direction[2]=12;EA_ResetInput(0);
  assert(BotMoveInDirection(state,direction,600,MOVE_CROUCH)==1&&botinputs[0].speed==400);
  assert(fabs(botinputs[0].dir[0]-3.0f/13)<(rtcw?.000001f:.002f)&&direction[0]==3&&direction[2]==12);
  contents=0;direction[0]=1;direction[1]=direction[2]=0;
  assert(BotMoveMaxStep()==18&&BotMoveMaxBarrier()==32);
  LibVarSet("sv_step","19");LibVarSet("sv_maxbarrier","48");
  assert(BotMoveMaxStep()==(rtcw?18:19)&&BotMoveMaxBarrier()==(rtcw?32:48));
  BotSetupMoveAI();assert(BotMoveMaxStep()==19&&BotMoveMaxBarrier()==48);
  LibVarSet("sv_step","18");LibVarSet("sv_maxbarrier","32");BotSetupMoveAI();
  gapGeometry();
  contents=0;assert(BotGapDistance(ms->origin,direction,0)>0);
  contents=CONTENTS_WATER;assert(BotGapDistance(ms->origin,direction,0)==0);
  contents=CONTENTS_SLIME;assert((BotGapDistance(ms->origin,direction,0)==0)==rtcw);
  contents=CONTENTS_LAVA;assert(BotGapDistance(ms->origin,direction,0)>0);contents=0;
  if(rtcw){
   ms->moveflags=MFL_BARRIERJUMP;ms->velocity[2]=49;EA_ResetInput(0);direction[0]=3;
   assert(BotMoveInDirection(state,direction,250,0)==1&&botinputs[0].speed==250&&botinputs[0].dir[0]==3);
   ms->velocity[2]=50;EA_ResetInput(0);assert(BotMoveInDirection(state,direction,250,0)==1&&!botinputs[0].speed);
   ms->velocity[2]=0;ms->moveflags=0;direction[0]=1;
   const auto previous=*ms;const auto action=botinputs[0];
   for(int bad:{INT_MIN,-1,0,profile->maxClients+1,INT_MAX})assert(!BotMoveInDirection(bad,direction,100,0));
   for(unsigned bits:{0x48000001u,0x7f7fffffu,0x7f800000u,0x7fc00001u}){
    float invalid;memcpy(&invalid,&bits,4);direction[1]=invalid;assert(!BotMoveInDirection(state,direction,100,0));direction[1]=0;
    assert(!BotMoveInDirection(state,direction,invalid,0));ms->thinktime=invalid;
    assert(!BotMoveInDirection(state,direction,100,0));*ms=previous;
    ms->origin[0]=invalid;assert(!BotMoveInDirection(state,direction,100,0));*ms=previous;
    rtcwMoveBarrier=invalid;assert(!BotMoveInDirection(state,direction,100,0));rtcwMoveBarrier=32;
   }
   ms->thinktime=131072;assert(!BotMoveInDirection(state,direction,2,0));*ms=previous;
   for(int client:{-1,profile->maxClients,INT_MAX}){ms->client=client;assert(!BotMoveInDirection(state,direction,100,0));}*ms=previous;
   ms->entitynum=1024;assert(!BotMoveInDirection(state,direction,100,0));*ms=previous;
   auto *links=aasworld.arealinkedentities;aasworld.arealinkedentities=nullptr;
   assert(!BotMoveInDirection(state,direction,100,0));aasworld.arealinkedentities=links;
   aasworld.initialized=qfalse;assert(!BotMoveInDirection(state,direction,100,0));aasworld.initialized=qtrue;
   assert(!memcmp(ms,&previous,sizeof(*ms))&&!memcmp(&botinputs[0],&action,sizeof(action)));
   EA_Shutdown();assert(!BotMoveInDirection(state,direction,100,0));
  }
  finish();
 }
}
''')

    def test_prediction_stop_events_failed_prediction_and_partial_actions(self):
        self.run_source(r'''
int main(){
 for(const auto &p:profiles){profile=&p;const bool rtcw=Com_GameIsRTCW();int state=prepare();auto *ms=botmovestates[state];
  vec3_t direction{1,0,0};
  for(int event:{0,SE_ENTERSLIME,SE_ENTERLAVA,SE_HITGROUNDDAMAGE,SE_HITGROUND}){
   ms->moveflags=MFL_ONGROUND;EA_ResetInput(0);predictedMove={};predictedMove.endpos[0]=100;
   predictedMove.endpos[2]=1;predictedMove.velocity[0]=100;predictedMove.stopevent=event;predictionResult=1;
   const bool accepted=event==0||event==SE_HITGROUND||(rtcw&&event==SE_ENTERSLIME);
   assert(BotMoveInDirection(state,direction,200,MOVE_CROUCH)==accepted);
   assert(botinputs[0].speed==(accepted?200:0));assert(bool(botinputs[0].actionflags&ACTION_CROUCH)==accepted);
  }
  predictedMove.stopevent=0;predictionResult=0;ms->moveflags=MFL_ONGROUND;EA_ResetInput(0);
  assert(BotMoveInDirection(state,direction,200,0)==!rtcw&&botinputs[0].speed==(rtcw?0:200));
  predictionResult=1;predictedMove.frames=30;ms->moveflags=MFL_ONGROUND;EA_ResetInput(0);
  assert(!BotMoveInDirection(state,direction,200,MOVE_JUMP)&&!botinputs[0].actionflags&&!botinputs[0].speed);
  assert(predictionCalls>0);finish();
 }
}
''',True)

    def test_import_550_native_qvm_vectors_scalars_and_result_validation(self):
        self.run_source(r'''
static int calls,returned,seenState,seenType;static float seenSpeed;static vec3_t seenDirection;
static int backend(int state,vec3_t direction,float speed,int type){
 ++calls;seenState=state;seenType=type;seenSpeed=speed;VectorCopy(direction,seenDirection);direction[0]=97;return returned;
}
int main(){
 botlib_export_t api{};api.ai.BotMoveInDirection=backend;
 for(const auto &p:profiles){profile=&p;intptr_t output=71;
  if(!Com_GameIsRTCW()){assert(!SV_RTCWBotLibCall(nullptr,Call::BOTLIB_AI_MOVE_IN_DIRECTION,nullptr,output)&&output==71);continue;}
  for(bool native:{false,true}){
   std::array<byte,128> memory;memory.fill(0xa5);vm_t vm{};vm.dataBase=memory.data();vm.exactDataLength=128;vm.dataMask=127;
   if(native)vm.entryPoint=reinterpret_cast<decltype(vm.entryPoint)>(1);
   auto address=[&](int n){return native?reinterpret_cast<intptr_t>(memory.data()+n):intptr_t(n);};
   auto bits=[](float f){int word;memcpy(&word,&f,4);return word;};
   vec3_t direction{1,2,3};auto write=[&](int at=1){memcpy(memory.data()+at,direction,12);};write();
   auto invoke=[&](intptr_t state,intptr_t ptr,intptr_t speed,intptr_t type){intptr_t args[]{550,state,ptr,speed,type},out=19;
    assert(SV_RTCWBotLibCall(&vm,Lookup(p.id,Module::GAME,Direction::IMPORT,550),args,out));return out;};
   auto fail=[&](intptr_t state,intptr_t ptr,intptr_t speed,intptr_t type){auto before=memory;int count=calls;
    rejected([&]{invoke(state,ptr,speed,type);});assert(memory==before&&count==calls);};
   botlib_export=nullptr;fail(1,address(1),0,0);botlib_export=&api;
   for(int state:{INT_MIN,-1,0,1,128,INT_MAX})for(int type:{INT_MIN,0,1,2,4,63,INT_MAX})
   for(float speed:{-131072.0f,-400.0f,0.0f,200.0f,600.0f,131072.0f}){
    returned=calls%2;auto before=memory;assert(invoke(state,address(1),bits(speed),type)==returned&&memory==before);
    assert(seenState==state&&seenType==type&&seenSpeed==speed&&!memcmp(seenDirection,direction,12));
   }
   fail(1,0,0,0);for(unsigned invalid:{0x48000001u,0x7f7fffffu,0x7f800000u,0xff800000u,0x7fc00001u}){
    fail(1,address(1),int(invalid),0);write();memcpy(memory.data()+1,&invalid,4);fail(1,address(1),0,0);write();
   }
   if(!native)for(int bad:{-1,117,127,129,INT_MAX})fail(1,bad,0,0);
   if(sizeof(intptr_t)>4){const auto wide=static_cast<intptr_t>(INT64_C(0x100000000));
    fail(wide,address(1),0,0);fail(1,address(1),wide,0);fail(1,address(1),0,wide);}
   returned=1;write(116);auto before=memory;assert(invoke(1,address(116),0,0)==1&&memory==before);
   for(int invalid:{-1,2,INT_MAX}){returned=invalid;int count=calls;before=memory;
    rejected([&]{invoke(1,address(1),0,0);});assert(memory==before&&calls==count+1);}
  }
 }
}
''')


if __name__=='__main__':
    unittest.main()
