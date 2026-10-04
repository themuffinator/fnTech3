"""Exercise ET map routing limits, cache misses, eviction and server setup.

Contract facts come from the original ET source, not retail execution. These
fixtures execute fnTech3's production routines over generated navigation.
"""
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import rtcw_route_query_tests as routes
import rtcw_routing_tests as routing
from rtcw_server_world_tests import function

ROOT = Path(__file__).resolve().parents[1]


class WolfETRoutingLimitsTests(unittest.TestCase):
    def compile_and_run(self, source, extra=(), flags=()):
        scratch = ROOT / '.tmp/wolfet-routing-limits-tests'
        scratch.mkdir(parents=True, exist_ok=True)
        with patch.object(tempfile, 'tempdir', str(scratch)):
            routing.RTCWRoutingTests.compile_and_run(self, source, extra, flags)

    def test_map_limits_defaults_overrides_snapshot_and_invalid_numbers(self):
        source = routing.routing_source() + r'''
static float fromBits(uint32_t bits){float out;std::memcpy(&out,&bits,4);return out;}
int main(){
 using namespace fntech3::wolfet::navigation;
 for(float invalid:{-1.0f,fromBits(0x7fc00000),fromBits(0x7f800000),fromBits(0xff800000),2097152.0f})
  assert(CacheBudget(invalid)==16384*1024);
 assert(CacheBudget(0)==0&&CacheBudget(-0.0f)==0&&CacheBudget(.9f)==0);
 assert(CacheBudget(1.99f)==1024&&CacheBudget(2097151)==2147482624);
 for(float invalid:{-1.0f,fromBits(0x7fc00000),fromBits(0x7f800000),2147483648.0f})
  assert(FrameRoutingUpdates(invalid)==1000);
 assert(FrameRoutingUpdates(0)==0&&FrameRoutingUpdates(-0.0f)==0);
 assert(FrameRoutingUpdates(1.99f)==1&&FrameRoutingUpdates(2147483520.0f)==2147483520);
 for(const auto &p:profiles){profile=&p;if(!p.bots.navigationWorlds||Com_GameIsRTCW())continue;
  initialize();AAS_InitRoutingLimits();
  assert(aasworld.max_routingcachesize==(Com_GameIsWolfET()?16384:12288)*1024);
  if(Com_GameIsWolfET())assert(aasworld.wolfetMaxFrameRoutingUpdates==1000);
  LibVarSet("max_routingcache","32.9");LibVarSet("bot_frameroutingupdates","7.9");
  // Settings are sampled at map initialization, not during cache queries.
  if(Com_GameIsWolfET())assert(aasworld.wolfetMaxFrameRoutingUpdates==1000);
  AAS_InitRoutingLimits();assert(aasworld.max_routingcachesize==32*1024);
  if(Com_GameIsWolfET())assert(aasworld.wolfetMaxFrameRoutingUpdates==7);
  shutdown();
 }
}
'''
        self.compile_and_run(source, routing.RTCWRoutingTests.extra(), ['-O2', '-ffast-math'])

    def test_frame_threshold_cache_hits_deferral_and_forced_portal_dependencies(self):
        source = routes.route_source() + routes.GRAPH + r'''
int main(){
 const int flags=TFL_WALK|TFL_AIR;
 for(const auto &p:profiles)if(p.id==COM_GAME_WOLFET)profile=&p;
 for(int limit:{0,1,1000,9999999}){
  initialize();graph(0,2,1);aasworld.wolfetMaxFrameRoutingUpdates=limit;
  aasworld.frameroutingupdates=limit;
  auto *hit=AAS_GetAreaRoutingCache(1,2,flags);assert(hit&&aasworld.frameroutingupdates==limit+2);
  cacheLinks();int bytes=aasworld.routingcachesize;
  assert(AAS_GetAreaRoutingCache(1,2,flags)==hit&&aasworld.routingcachesize==bytes);
  assert(!AAS_GetAreaRoutingCache(1,1,flags)&&aasworld.routingcachesize==bytes);
  assert(AAS_AreaTravelTimeToGoalArea(2,nullptr,1,flags)==0);
  LibVarSet("bot_frameroutingupdates","99999999");
  assert(!AAS_GetAreaRoutingCache(1,1,flags)); // Existing map keeps its snapshot.
  assert(AAS_GetAreaRoutingCache(1,1,flags,true));cacheLinks();shutdown();
 }
 initialize();graph(0,2,1);aasworld.wolfetMaxFrameRoutingUpdates=0;
 aasworld.frameroutingupdates=1;int bytes=aasworld.routingcachesize;
 assert(!AAS_GetAreaRoutingCache(1,2,flags)&&aasworld.routingcachesize==bytes);
 aasworld.frameroutingupdates=0;assert(AAS_GetAreaRoutingCache(1,2,flags));cacheLinks();shutdown();
 initialize();graph();aasworld.wolfetMaxFrameRoutingUpdates=0;aasworld.frameroutingupdates=INT_MAX;
 auto *portal=AAS_GetPortalRoutingCache(2,4,flags);
 assert(portal&&portal->traveltimes[1]&&aasworld.frameroutingupdates==INT_MAX);
 // Portal construction forces its ordinary and portal-area dependencies even
 // after the ordinary-query budget is spent.
 assert(aasworld.clusterareacache[2][0]);cacheLinks();shutdown();
}
'''
        self.compile_and_run(source, routing.RTCWRoutingTests.extra(), ['-O2', '-ffast-math'])

    def test_eviction_uses_table_order_and_frees_both_cache_kinds(self):
        source = routes.route_source() + routes.GRAPH + r'''
int main(){
 const int flags=TFL_WALK|TFL_AIR;
 for(const auto &p:profiles){profile=&p;if(!Com_GameIsWolfET()&&!Com_GameIsRTCW())continue;
  initialize();graph();
  auto *first=AAS_GetAreaRoutingCache(1,1,flags,true);
  auto *second=AAS_GetAreaRoutingCache(1,2,flags,true);
  auto *portalArea=AAS_GetAreaRoutingCache(1,10,flags,true);
  auto *portal=AAS_GetPortalRoutingCache(2,4,flags);
  // Equal times choose the first table entry, even after its most recent use.
  assert(AAS_GetAreaRoutingCache(1,1,flags,true)==first);
  int expected=first->size+portal->size,bytes=aasworld.routingcachesize;
  assert(AAS_FreeOldestCache()&&aasworld.routingcachesize==bytes-expected);
  assert(!aasworld.clusterareacache[1][0]&&aasworld.clusterareacache[1][1]==second);
  assert(aasworld.clusterareacache[1][3]==portalArea&&!aasworld.portalcache[4]);cacheLinks();
  while(AAS_FreeOldestCache())cacheLinks();
  assert(aasworld.routingcachesize>0&&aasworld.clusterareacache[1][3]==portalArea);
  shutdown();
 }
}
'''
        self.compile_and_run(source, routing.RTCWRoutingTests.extra(), ['-O2', '-ffast-math'])

    def test_server_supplies_et_mode_defaults_and_preserves_overrides(self):
        source = r'''
#include <cassert>
#include <map>
#include <string>
#include <cstring>
static bool wolfet;static int gameType,bot_enable=1,starts;
static std::map<std::string,std::string> cvars,vars;
struct cvar_t{const char *string;};
bool Com_GameIsWolfET(){return wolfet;}
bool SV_GameTypeIsSinglePlayer(int type){return type==2;}
int Cvar_VariableIntegerValue(const char *name){assert(!strcmp(name,"g_gametype"));return gameType;}
cvar_t *Cvar_Get(const char *name,const char *value,int flags){
 assert(flags==0);auto inserted=cvars.emplace(name,value);static cvar_t c;
 c.string=inserted.first->second.c_str();return &c;
}
void Com_Printf(const char*,...){}
#define S_COLOR_ERROR ""
static int setup(){++starts;return 73;}
static int varSet(const char *name,const char *value){vars[name]=value;return 0;}
struct exports_t{int (*BotLibSetup)();int (*BotLibVarSet)(const char*,const char*);};
static exports_t exports{setup,varSet};static exports_t *botlib_export=&exports;
'''
        source += function((ROOT / 'code/server/sv_bot.cpp').read_text(), 'int SV_BotLibSetup(')
        source += r'''
int main(){
 for(bool selected:{false,true})for(int type:{2,3,4,5,6}){
  wolfet=selected;gameType=type;cvars.clear();vars.clear();int before=starts;
  assert(SV_BotLibSetup()==73&&starts==before+1);
  if(selected){assert(vars.at("bot_frameroutingupdates")== (type==2?"9999999":"1000"));
   cvars["bot_frameroutingupdates"]="17.9";assert(SV_BotLibSetup()==73);
   assert(vars.at("bot_frameroutingupdates")=="17.9");
  }else assert(cvars.empty()&&vars.empty());
 }
 cvars.clear();vars.clear();wolfet=true;bot_enable=0;int before=starts;
 assert(SV_BotLibSetup()==0&&starts==before&&cvars.empty()&&vars.empty());
 bot_enable=1;botlib_export=nullptr;
 assert(SV_BotLibSetup()==-1&&starts==before&&cvars.empty()&&vars.empty());
}
'''
        self.compile_and_run(source, flags=['-O2', '-ffast-math'])


if __name__ == '__main__':
    unittest.main()
