"""Exercise RTCW travel actions with real movement/AAS code and controlled BSP callbacks."""
from pathlib import Path
import re
import unittest
from rtcw_direction_tests import direction_source, GEOMETRY
from rtcw_server_world_tests import function
import rtcw_botlib_tests as bootstrap

ROOT=Path(__file__).resolve().parents[1]
TRAVEL_NAMES=['BotTravel_Walk','BotTravel_Crouch','BotTravel_BarrierJump','BotFinishTravel_BarrierJump',
    'BotTravel_Swim','BotTravel_WaterJump','BotFinishTravel_WaterJump','BotTravel_WalkOffLedge',
    'BotFinishTravel_WalkOffLedge','BotTravel_Jump','BotFinishTravel_Jump','BotTravel_Ladder',
    'BotTravel_Teleport','BotTravel_Elevator','BotFinishTravel_Elevator','BotTravel_FuncBobbing',
    'BotFinishTravel_FuncBobbing','BotTravel_Grapple','BotTravel_RocketJump','BotFinishTravel_WeaponJump',
    'BotTravel_JumpPad','BotFinishTravel_JumpPad']


def active_function(text,name):
    # Older jump/grapple implementations remain inside block comments.
    text=re.sub(r'/\*.*?\*/|//[^\n]*','',text,flags=re.S)
    match=re.search(r'^(?:(?:static|ID_INLINE)\s+)*(?:int|float|void|bot_moveresult_t)\s+'+name+r'\s*\(',text,re.M)
    return function(text,match[0])


def travel_source():
    source=direction_source().replace('void EA_Move(', 'void Real_EA_Move(',1)
    source+=r'''
#include "code/botlib/l_utils.h"
#include "code/botlib/be_aas_funcs.h"
const vec3_t vec3_origin{};
static constexpr int WEAPONINDEX_ROCKET_LAUNCHER=5;
struct MoveAction {float direction[3],speed;};
static std::vector<MoveAction> moveActions;
static std::vector<std::string> commands;
static int bspCalls,obstruction=ENTITYNUM_WORLD;
static float traceBottom;
static bool traceSolid;
static vec3_t moverMins{-32,-32,-8},moverMaxs{32,32,8};
void EA_Move(int client,vec3_t dir,float speed){
 moveActions.push_back({{dir[0],dir[1],dir[2]},speed});Real_EA_Move(client,dir,speed);
}
static void command(int client,const char *text){assert(client==0);commands.emplace_back(text);}
static void bspTrace(bsp_trace_t *out,vec3_t start,vec3_t lo,vec3_t hi,vec3_t end,int,int){
 ++bspCalls;traceBottom=lo?lo[2]:0;*out={};out->fraction=1;out->ent=obstruction;
 out->startsolid=traceSolid?qtrue:qfalse;VectorCopy(end,out->endpos);
}
'''
    for filename,signatures in {
        'code/qcommon/q_math.c':['void vectoangles(', 'void AngleVectors('],
        'code/botlib/be_ea.cpp':['void EA_Walk(', 'void EA_MoveForward(', 'void EA_MoveBack(', 'void EA_MoveUp(',
            'void EA_MoveRight(', 'void EA_MoveLeft(', 'void EA_DelayedJump(', 'void EA_Command(',
            'void EA_Attack(', 'void EA_SelectWeapon(', 'void EA_View('],
        'code/botlib/be_aas_bspq3.cpp':['bsp_trace_t AAS_Trace(', 'void AAS_BSPModelMinsMaxsOrigin('],
        'code/botlib/be_aas_sample.cpp':['int AAS_AreaPresenceType('],
        'code/botlib/be_aas_route.cpp':['void AAS_ReachabilityFromNum('],
        'code/botlib/be_aas_entity.cpp':['void AAS_EntityInfo(', 'int AAS_EntityModelindex(', 'int AAS_EntityType(',
            'int AAS_EntityModelNum(', 'int AAS_OriginOfMoverWithModelNum(', 'int AAS_NextEntity('],
        'code/botlib/be_aas_main.cpp':['float AAS_Time('],
        'code/botlib/be_aas_move.cpp':['static bool AAS_JumpReachRunStartChecked(', 'void AAS_JumpReachRunStart(',
            'bool AAS_RTCWJumpReachRunStart(', 'int AAS_HorizontalVelocityForJump('],
    }.items():
        text=(ROOT/filename).read_text()
        for signature in signatures:
            assert signature in text,(filename,signature)
            source+='\n'+function(text,signature)
    move=(ROOT/'code/botlib/be_ai_move.cpp').read_text()
    for name in ['AngleDiff','BotOnMover','MoverDown','MoverBottomCenter','BotCheckBlocked','BotAirControl',
                 'BotMoveProject','BotTravelRTCWLadder','BotFuncBobStartEnd','GrappleState','BotGrappleOff',
                 'BotResetGrapple',*TRAVEL_NAMES]:
        source+='\n'+active_function(move,name)
    return source


SCENE=GEOMETRY+r'''
static bot_movestate_t *travelSetup(){
 int state=prepare();botimport.Trace=bspTrace;botimport.BotClientCommand=command;
 botimport.BSPModelMinsMaxsOrigin=[](int model,vec3_t angles,vec3_t lo,vec3_t hi,vec3_t origin){
  assert(model==3);if(lo)VectorCopy(moverMins,lo);if(hi)VectorCopy(moverMaxs,hi);if(origin)VectorClear(origin);
 };
 auto *ms=botmovestates[state];ms->areanum=ms->reachareanum=1;ms->lastreachnum=1;
 aasworld.areasettings[1].numreachableareas=1;
 aasworld.reachabilitysize=2;aasworld.reachability=botlib::ClearedAlloc<aas_reachability_t>(2);
 aasworld.time=5;contents=bspCalls=0;traceSolid=false;obstruction=ENTITYNUM_WORLD;
 auto &mover=AAS_EntityWorld().entities[1].i;mover.valid=qtrue;mover.type=ET_MOVER;mover.modelindex=3;
 moveActions.clear();commands.clear();return ms;
}
static void travelFinish(){FreeMemory(aasworld.reachability);aasworld.reachability=nullptr;finish();}
static void resetActions(){moveActions.clear();commands.clear();botinputs[0]={};bspCalls=0;}
static aas_reachability_t link(float sx,float sy,float sz,float ex,float ey,float ez){
 aas_reachability_t r{};VectorSet(r.start,sx,sy,sz);VectorSet(r.end,ex,ey,ez);r.areanum=1;r.facenum=3;return r;
}
static bool near(float a,float b){return std::fabs(a-b)<.002f;}
'''


class RTCWTravelTests(unittest.TestCase):
    compile_and_run=bootstrap.RTCWBotLibTests.compile_and_run

    def run_source(self,checks):
        self.compile_and_run(travel_source()+SCENE+checks,
            [str(ROOT/'code/server/server_game_data.cpp'),'-x','c++',str(ROOT/'code/compat/game_abi_adapter.c')],
            ['-O2','-ffast-math'])

    def test_ground_obstructions_walk_crouch_barriers_and_profile_controls(self):
        self.run_source(r'''
int main(){for(const auto &p:profiles){profile=&p;auto *ms=travelSetup();bool rtcw=Com_GameIsRTCW(),sp=Com_GameIsRTCWSP();
 auto r=link(20,0,1,0,15,1);obstruction=7;auto out=BotTravel_Walk(ms,&r);
 assert(out.blocked==!sp&&out.blockentity==(sp?0:7));assert(bspCalls==!sp);
 assert(near(out.movedir[rtcw?1:0],1));
 aasworld.areasettings[1].presencetype=0;resetActions();out=BotTravel_Walk(ms,&r);
 assert(!(botinputs[0].actionflags&ACTION_CROUCH));
 r.start[0]=r.end[0]=5;r.end[1]=0;resetActions();out=BotTravel_Walk(ms,&r);
 assert(bool(botinputs[0].actionflags&ACTION_CROUCH)==!rtcw);
 aasworld.areasettings[1].presencetype=PRESENCE_CROUCH;resetActions();out=BotTravel_Walk(ms,&r);
 assert(botinputs[0].actionflags&ACTION_CROUCH);
 LibVarSet("sv_step","27");resetActions();BotCheckBlocked(ms,out.movedir,0,&out);
 assert(sp||traceBottom==(rtcw?-6:3));
 r=link(5,0,1,5,40,49);resetActions();out=BotTravel_BarrierJump(ms,&r);
 assert(botinputs[0].actionflags&ACTION_JUMP);assert(moveActions.size()==size_t(rtcw));
 assert(near(out.movedir[rtcw?1:0],1));if(rtcw)assert(moveActions.back().speed==360);
 ms->velocity[2]=249;resetActions();out=BotFinishTravel_BarrierJump(ms,&r);
 assert(moveActions.size()==1&&near(moveActions.back().speed,rtcw?360:400));
 assert(near(VectorLength(out.movedir),rtcw?1:std::sqrt(1625.f)));
 ms->velocity[2]=250;resetActions();BotFinishTravel_BarrierJump(ms,&r);assert(moveActions.empty());travelFinish();
}}
''')

    def test_ledges_water_air_control_and_jump_pads(self):
        self.run_source(r'''
int main(){for(const auto &p:profiles){profile=&p;auto *ms=travelSetup();bool rtcw=Com_GameIsRTCW(),sp=Com_GameIsRTCWSP();
 ms->origin[0]=10;auto r=link(0,0,100,10,0,0);auto out=BotTravel_WalkOffLedge(ms,&r);
 assert(bool(botinputs[0].actionflags&ACTION_CROUCH)==rtcw);
 assert(near(moveActions.back().speed,sp?200:100));
 ms->origin[0]=-32;r=link(0,0,100,100,0,0);resetActions();BotTravel_WalkOffLedge(ms,&r);
 assert(near(moveActions.back().speed,sp?250:rtcw?100:200));
 vec3_t origin{0,0,100},velocity{},goal{20,0,0},dir;float speed;
 LibVarSet("sv_gravity","0");assert(bool(BotAirControl(origin,velocity,goal,dir,&speed))==rtcw);
 ms->origin[0]=0;ms->velocity[2]=10000;r=link(3,4,1,10,0,10000);resetActions();out=BotFinishTravel_WalkOffLedge(ms,&r);
 assert(bool(botinputs[0].actionflags&ACTION_CROUCH)==rtcw&&near(VectorLength(out.movedir),rtcw?1:10));
 resetActions();out=BotTravel_JumpPad(ms,&r);assert(near(VectorLength(out.movedir),rtcw?1:5));
 ms->jumpreach=1;resetActions();out=BotFinishTravel_WeaponJump(ms,&r);
 assert(near(moveActions.back().speed,rtcw?800:400));assert(botinputs[0].speed==400);
 contents=CONTENTS_SLIME;resetActions();srand(71);out=BotFinishTravel_WaterJump(ms,&r);
 assert(moveActions.size()==1&&(out.flags&MOVERESULT_MOVEMENTVIEW));
 assert(rtcw?near(VectorLength(out.movedir),1):VectorLength(out.movedir)>10000);
 ms->moveflags=MFL_WATERJUMP;resetActions();BotFinishTravel_WaterJump(ms,&r);assert(moveActions.empty());travelFinish();
}}
''')

    def test_ladder_approach_descent_and_single_player_lateral_correction(self):
        self.run_source(r'''
int main(){for(const auto &p:profiles){profile=&p;auto *ms=travelSetup();bool rtcw=Com_GameIsRTCW(),sp=Com_GameIsRTCWSP();
 auto r=link(0,0,80,32,0,0);ms->origin[1]=10;ms->moveflags=MFL_AGAINSTLADDER;
 auto out=BotTravel_Ladder(ms,&r);assert(out.flags&MOVERESULT_MOVEMENTVIEW);
 assert(bool(botinputs[0].actionflags&ACTION_MOVEBACK)==sp);
 assert(bool(botinputs[0].actionflags&ACTION_MOVEFORWARD)==!sp);
 assert(bool(botinputs[0].actionflags&(ACTION_MOVELEFT|ACTION_MOVERIGHT))==sp);
 if(rtcw)assert(sp?out.ideal_viewangles[0]==0:out.ideal_viewangles[0]<0);
 ms->moveflags=MFL_ONGROUND;VectorSet(ms->origin,-80,10,1);resetActions();out=BotTravel_Ladder(ms,&r);
 if(rtcw){assert(!out.flags&&near(VectorLength(out.movedir),1));assert(near(moveActions.back().speed,sp?160:400));}
 else assert(out.flags&MOVERESULT_MOVEMENTVIEW);
 VectorSet(ms->origin,1,1,1);resetActions();out=BotTravel_Ladder(ms,&r);
 assert(bool(out.flags&MOVERESULT_MOVEMENTVIEW)==(sp||!rtcw));travelFinish();
}}
''')

    def test_movers_grapple_lifecycle_fixed_commands_and_rocket_selection(self):
        self.run_source(r'''
int main(){for(const auto &p:profiles){profile=&p;auto *ms=travelSetup();bool rtcw=Com_GameIsRTCW();
 auto r=link(0,0,1,0,0,100);ms->moveflags=MFL_GRAPPLEPULL;auto &hook=AAS_EntityWorld().entities[2].i;
 hook.valid=qtrue;hook.modelindex=0;VectorCopy(r.end,hook.origin);VectorCopy(hook.origin,hook.lastvisorigin);
 assert(GrappleState(ms,&r)==2);hook.origin[0]=50;assert(GrappleState(ms,&r)==(rtcw?1:2));
 LibVarSet("laserhook","1");assert(GrappleState(ms,&r)==(rtcw?0:2));LibVarSet("laserhook","0");
 ms->moveflags=MFL_GRAPPLERESET|MFL_ACTIVEGRAPPLE;resetActions();auto out=BotTravel_Grapple(ms,&r);
 assert(!(ms->moveflags&MFL_ACTIVEGRAPPLE)&&commands.size()==size_t(rtcw));
 if(rtcw)assert(commands[0]=="hookoff"&&!out.flags);
 ms->moveflags=0;ms->viewangles[0]=-90;resetActions();out=BotTravel_Grapple(ms,&r);
 assert(ms->moveflags&MFL_ACTIVEGRAPPLE);assert(commands.size()==size_t(rtcw));
 assert(rtcw?commands[0]=="hookon":bool(out.flags&MOVERESULT_MOVEMENTWEAPON));
 ms->grapplevisible_time=2;aasworld.reachability[1].traveltype=TRAVEL_GRAPPLEHOOK|TRAVELFLAG_NOTTEAM1;
 resetActions();BotResetGrapple(ms);assert(commands.size()==size_t(rtcw));
 if(rtcw)assert(!ms->grapplevisible_time&&!(ms->moveflags&MFL_ACTIVEGRAPPLE));
 r=link(0,0,1,40,0,100);LibVarSet("weapindex_rocketlauncher","12");ms->viewangles[0]=0;
 resetActions();out=BotTravel_RocketJump(ms,&r);assert(out.weapon==(rtcw?5:12));
 assert(bool(botinputs[0].actionflags&ACTION_ATTACK)==rtcw);
 assert((out.flags&(MOVERESULT_MOVEMENTVIEWSET|MOVERESULT_MOVEMENTWEAPON))==24);
 r=link(-200,0,1,200,0,100);r.edgenum=100;AAS_EntityWorld().entities[1].i.origin[2]=50;
 resetActions();out=BotTravel_FuncBobbing(ms,&r);assert(out.flags&MOVERESULT_WAITING);
 assert(out.type==(rtcw?RESULTTYPE_ELEVATORUP:RESULTTYPE_WAITFORFUNCBOBBING));travelFinish();
}}
''')


if __name__=='__main__':unittest.main()
