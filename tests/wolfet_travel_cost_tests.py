"""ET geometry costs measured in hash-pinned original client/server routines."""
from pathlib import Path
import json
import unittest

import rtcw_route_query_tests as routes
import rtcw_routing_tests as routing

ROOT = Path(__file__).resolve().parents[1]
FIXTURE = ROOT / 'tests/fixtures/wolfet-travel-cost.json'


def cpp_vector(vector):
    return '{' + ','.join(repr(float(value)) + 'f' for value in vector) + '}'


class WolfETTravelCostTests(unittest.TestCase):
    compile_and_run = routing.RTCWRoutingTests.compile_and_run

    def test_costs_match_original_client_and_dedicated_nearest_even_math(self):
        fixture = json.loads(FIXTURE.read_text(encoding='utf-8'))
        self.assertEqual(fixture['floatingPoint']['rounding'], 'nearest-even')
        self.assertEqual([value['sha256'] for value in fixture['retailIdentities']], [
            '4d1326c5936b61c5f878fc017e4aa0abddf13e008139e31fce2f13d57b26243e',
            '7eb606913a56d6901e358a598fb0079120b2730e113a10fc963f90b777580cdf'])
        source = routing.routing_source() + r'''
#include <cstdio>
int main(){
 for(const auto &p:profiles)if(p.id==COM_GAME_WOLFET)profile=&p;
 assert(profile&&Com_GameIsWolfET());initialize();
 aasworld.numareas=aasworld.numareasettings=2;
 aasworld.areasettings=botlib::ClearedAlloc<aas_areasettings_t>(2);
 aasworld.rtcwGroundSteepness=botlib::ClearedAlloc<float>(2);
 auto &s=aasworld.areasettings[1];int failures=0;
'''
        for case in fixture['cases']:
            source += f'''{{float start[3]={cpp_vector(case['start'])},end[3]={cpp_vector(case['end'])};
 s.presencetype={case['presence']};s.areaflags={case['areaFlags']};s.contents={case['contents']};
 aasworld.rtcwGroundSteepness[1]={float(case['steepness'])}f;
 const auto actual=AAS_AreaTravelTime(1,start,end);
 if(actual!={case['expected']}){{if(failures<10)fprintf(stderr,"%s: %u expected %u\\n",
 {json.dumps(case['name'])},actual,{case['expected']}u);++failures;}}}}
'''
        source += f'printf("{len(fixture["cases"])} original ET costs: %d mismatches\\n",failures);shutdown();return failures?1:0;}}\n'
        self.compile_and_run(source, routing.RTCWRoutingTests.extra(), ['-O2', '-ffast-math'])

    def test_original_travel_types_reject_quake_team_annotations(self):
        fixture = json.loads(FIXTURE.read_text(encoding='utf-8'))
        source = routing.routing_source() + r'''
int main(){
 for(const auto &p:profiles)if(p.id==COM_GAME_WOLFET)profile=&p;
 initialize();AAS_InitTravelFlagFromType();
'''
        for case in fixture['travelTypes']:
            value = case['type'] if case['type'] <= 0x7fffffff else -1
            source += f'assert(AAS_TravelFlagForType({value})=={case["expected"]});\n'
        source += 'shutdown();}\n'
        self.compile_and_run(source, routing.RTCWRoutingTests.extra(), ['-O2', '-ffast-math'])

    def test_endpoint_large_entry_restrictions_are_profile_owned(self):
        source = routes.route_source() + routes.GRAPH + r'''
int main(){
 for(const auto &p:profiles){profile=&p;
  if(!Com_GameIsRTCW()&&!Com_GameIsWolfET()&&p.id!=COM_GAME_QUAKELIVE&&p.id!=COM_GAME_QUAKE3&&!Com_GameIsSTVEFMP())continue;
  for(int endpoint:{1,2}){initialize();graph(0,2,1);
   aasworld.areasettings[endpoint].contents=1024;AAS_InitAreaContentsTravelFlags();
   const int time=AAS_AreaTravelTimeToGoalArea(1,nullptr,2,TFL_WALK|TFL_AIR);
   if(time<=0)fprintf(stderr,"profile %s, restricted endpoint %d: no route\n",p.displayName,endpoint);
   assert(time>0);cacheLinks();shutdown();
  }
 }
}
'''
        # stdio is part of the normal engine source, but the extracted fixture
        # only needs it here for useful before/after mismatch diagnostics.
        source = '#include <cstdio>\n' + source
        self.compile_and_run(source, routing.RTCWRoutingTests.extra(), ['-O2', '-ffast-math'])

    def test_geometry_rows_origin_costs_and_other_profile_speeds(self):
        source = routes.route_source() + routes.GRAPH + r'''
int main(){
 for(const auto &p:profiles){profile=&p;if(!p.bots.navigationWorlds)continue;
  for(bool liquid:{false,true})for(bool crouch:{false,true}){
   initialize();graph(0,2,1);auto &s=aasworld.areasettings[2];
   s.presencetype=crouch?PRESENCE_CROUCH:PRESENCE_NORMAL;
   s.areaflags=liquid?AREA_LIQUID:0;s.contents=0;
   // Incoming end -> first outgoing start is 150 units in goal area 2.
   VectorClear(aasworld.reachability[0].end);
   VectorClear(aasworld.reachability[1].start);aasworld.reachability[1].start[0]=150;
   routingTables();
   const int expected=Com_GameIsWolfET()?(crouch?195:50):crouch?195:liquid?150:Com_GameIsRTCWSP()?50:49;
   assert(aasworld.areatraveltimes[2][0][0]==expected);
   const int flags=TFL_WALK|TFL_AIR;
   assert(AAS_AreaTravelTimeToGoalArea(1,nullptr,2,flags)==(Com_GameIsRTCW()||Com_GameIsWolfET()||Com_GameIsSTVEFMP()?11+expected:11));
   vec3_t origin{};
   // Source 1's outgoing start is (8,1,0); nearest-even ET charges 3.
   const int sourceCost=Com_GameIsRTCWSP()||Com_GameIsWolfET()?3:2;
   const int noOrigin=AAS_AreaTravelTimeToGoalArea(1,nullptr,2,flags);
   assert(AAS_AreaTravelTimeToGoalArea(1,origin,2,flags)==noOrigin+sourceCost);
   cacheLinks();shutdown();
  }
 }
}
'''
        self.compile_and_run(source, routing.RTCWRoutingTests.extra(), ['-O2', '-ffast-math'])

    def test_invalid_coordinates_area_bounds_and_partial_world(self):
        source = routing.routing_source() + r'''
static float fromBits(uint32_t bits){volatile uint32_t input=bits;bits=input;float out;std::memcpy(&out,&bits,4);return out;}
int main(){
 for(const auto &p:profiles)if(p.id==COM_GAME_WOLFET)profile=&p;
 initialize();float start[3]={},end[3]={150,0,0};uint16_t cost=73;
 using namespace fntech3::wolfet::navigation;
 for(float invalid:{fromBits(0x7fc00000),fromBits(0x7f800000),fromBits(0xff800000),131073.0f,-131073.0f})
  for(int axis=0;axis<3;++axis){end[axis]=invalid;
   assert(!AreaTravelTime(start,end,false,cost)&&cost==73);end[axis]=0;}
 assert(!AreaTravelTime(nullptr,end,false,cost)&&cost==73);
 assert(!AreaTravelTime(start,nullptr,false,cost)&&cost==73);
 assert(AreaTravelTime(start,end,false,cost)&&cost==1);
 assert(AAS_AreaTravelTime(1,start,end)==0);
 aasworld.numareas=aasworld.numareasettings=2;
 aasworld.areasettings=botlib::ClearedAlloc<aas_areasettings_t>(2);
 aasworld.areasettings[1].presencetype=PRESENCE_NORMAL;
 assert(AAS_AreaTravelTime(0,start,end)==0&&AAS_AreaTravelTime(-1,start,end)==0);
 assert(AAS_AreaTravelTime(2,start,end)==0&&AAS_AreaTravelTime(INT_MAX,start,end)==0);
 assert(AAS_AreaTravelTime(1,start,end)==1); // ET does not require a slope sidecar.
 assert(!aasworld.rtcwGroundSteepness);
 assert(AAS_AreaTravelTime(1,nullptr,end)==0&&AAS_AreaTravelTime(1,start,nullptr)==0);
 end[0]=fromBits(0x7fc00000);assert(AAS_AreaTravelTime(1,start,end)==0);
 end[0]=150;--aasworld.numareasettings;assert(AAS_AreaTravelTime(1,start,end)==0);
 shutdown();assert(AAS_AreaTravelTime(1,start,end)==0);
}
'''
        self.compile_and_run(source, routing.RTCWRoutingTests.extra(), ['-O2', '-ffast-math'])


if __name__ == '__main__':
    unittest.main()
