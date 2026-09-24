"""Jump run-up policy and failed-prediction action suppression."""
from pathlib import Path
import unittest
import rtcw_travel_tests as travel
from rtcw_server_world_tests import function

ROOT=Path(__file__).resolve().parents[1]


def jump_source(scripted=False):
    source=travel.travel_source()
    if scripted:
        actual=function(source,'int AAS_PredictClientMovement(')
        source=source.replace(actual,r'''
static int predictionCalls,predictionResult=1;
static aas_clientmove_t predicted;
static vec3_t seenCommand,seenOrigin;
int AAS_PredictClientMovement(aas_clientmove_t *out,int entity,const vec3_t origin,int presence,int ground,
 const vec3_t velocity,const vec3_t command,int commandFrames,int frames,float duration,int events,int area,int visualize){
 ++predictionCalls;assert(entity==-1&&presence==PRESENCE_NORMAL&&ground==1);
 assert(!velocity[0]&&!velocity[1]&&!velocity[2]&&commandFrames==1&&frames==2&&duration==.1f&&!area&&!visualize);
 assert(events==(SE_ENTERWATER|SE_ENTERSLIME|SE_ENTERLAVA|SE_HITGROUNDDAMAGE|SE_GAP));
 VectorCopy(origin,seenOrigin);VectorCopy(command,seenCommand);*out=predicted;return predictionResult;
}
''')
    return source


class RTCWJumpRunstartTests(unittest.TestCase):
    compile_and_run=travel.RTCWTravelTests.compile_and_run

    def run_source(self,checks,scripted=False):
        self.compile_and_run(jump_source(scripted)+travel.SCENE+checks,
            [str(ROOT/'code/server/server_game_data.cpp'),'-x','c++',str(ROOT/'code/compat/game_abi_adapter.c')],
            ['-O2','-ffast-math'])

    def test_normalization_slime_water_lava_fall_damage_and_profile_controls(self):
        self.run_source(r'''
int main(){for(const auto &p:profiles){profile=&p;if(!p.bots.navigationWorlds)continue;travelSetup();const bool rtcw=Com_GameIsRTCW();
 auto reach=link(3,4,7,6,8,20);vec3_t out{};predicted={};VectorSet(predicted.endpos,70,80,90);
 for(int event:{0,SE_ENTERWATER,SE_ENTERSLIME,SE_ENTERLAVA,SE_HITGROUNDDAMAGE,SE_GAP,SE_ENTERSLIME|SE_ENTERLAVA}){
  predicted.stopevent=event;predictionCalls=0;AAS_JumpReachRunStart(&reach,out);
  assert(predictionCalls==1&&seenOrigin[0]==3&&seenOrigin[1]==4&&seenOrigin[2]==8);
  assert(fabs(seenCommand[0]+240)<(rtcw?.001:.5)&&fabs(seenCommand[1]+320)<(rtcw?.001:.6));
  const bool fallback=event&(SE_ENTERLAVA|SE_HITGROUNDDAMAGE|(rtcw?0:SE_ENTERSLIME));
  assert(out[0]==(fallback?3:70)&&out[1]==(fallback?4:80)&&out[2]==(fallback?8:90));
 }
 travelFinish();
}}
''',True)

    def test_failure_preserves_private_output_and_stops_jump_actions(self):
        self.run_source(r'''
int main(){for(const auto &p:profiles){profile=&p;if(!p.bots.navigationWorlds)continue;auto *ms=travelSetup();if(!Com_GameIsRTCW()){travelFinish();continue;}
 auto reach=link(3,4,7,6,8,20);predicted={};VectorSet(predicted.endpos,70,80,90);predictionResult=0;
 vec3_t out{11,13,17},before{11,13,17};predictionCalls=0;
 assert(!AAS_RTCWJumpReachRunStart(reach.start,reach.end,out)&&!memcmp(out,before,12)&&predictionCalls==1);
 const auto initial=*ms;resetActions();auto result=BotTravel_Jump(ms,&reach);
 assert(result.failure&&moveActions.empty()&&commands.empty()&&!bspCalls&&!memcmp(ms,&initial,sizeof(initial)));
 assert(!botinputs[0].actionflags&&!botinputs[0].speed);
 AAS_JumpReachRunStart(&reach,out);assert(!memcmp(out,reach.start,12));
 predictionResult=1;predictionCalls=0;
 assert(!AAS_RTCWJumpReachRunStart(nullptr,reach.end,out));assert(!AAS_RTCWJumpReachRunStart(reach.start,nullptr,out));
 assert(!AAS_RTCWJumpReachRunStart(reach.start,reach.end,nullptr));assert(!predictionCalls);
 for(unsigned bits:{0x7fc00001u,0x7f800000u,0x48000001u}){auto bad=reach;memcpy(bad.start+1,&bits,4);
  VectorCopy(before,out);assert(!AAS_RTCWJumpReachRunStart(bad.start,bad.end,out)&&!memcmp(out,before,12)&&!predictionCalls);}
 auto imports=botimport;botimport.PointContents=nullptr;
 assert(!AAS_RTCWJumpReachRunStart(reach.start,reach.end,out)&&!predictionCalls);botimport=imports;
 botimport.EntityTrace=nullptr;assert(!AAS_RTCWJumpReachRunStart(reach.start,reach.end,out)&&!predictionCalls);botimport=imports;
 // Inputs are owned before publishing an overlapping run-up point.
 assert(AAS_RTCWJumpReachRunStart(reach.start,reach.end,reach.start));assert(reach.start[0]==70&&reach.start[2]==90);
 travelFinish();
}}
''',True)

    def test_real_prediction_partial_shutdown_and_horizontal_velocity(self):
        self.run_source(r'''
int main(){for(const auto &p:profiles){profile=&p;if(!p.bots.navigationWorlds)continue;travelSetup();auto reach=link(3,4,1,40,80,1);vec3_t out{7,8,9};
 if(Com_GameIsRTCW()){
  assert(AAS_RTCWJumpReachRunStart(reach.start,reach.end,out));
  assert(fntech3::rtcw::movement::Position(out)&&out[0]<reach.start[0]&&out[1]<reach.start[1]);
  auto saved=aasworld;aasworld.initialized=0;VectorSet(out,7,8,9);
  assert(!AAS_RTCWJumpReachRunStart(reach.start,reach.end,out)&&out[0]==7&&out[1]==8&&out[2]==9);aasworld=saved;
 }else assert(!AAS_RTCWJumpReachRunStart(reach.start,reach.end,out));
 vec3_t from{},to{100,0,-100};float speed;
 assert(AAS_HorizontalVelocityForJump(0,from,to,&speed)&&near(speed,200));
 to[2]=0;assert(AAS_HorizontalVelocityForJump(270,from,to,&speed)&&near(speed,100/.675f));
 to[2]=100;assert(!AAS_HorizontalVelocityForJump(270,from,to,&speed)&&speed==aassettings.phys_maxvelocity);
 // The inherited zero-denominator guard handles the original undefined 0/0.
 VectorClear(to);assert(!AAS_HorizontalVelocityForJump(0,from,to,&speed)&&speed==aassettings.phys_maxvelocity);
 travelFinish();
}}
''')


if __name__=='__main__':unittest.main()
