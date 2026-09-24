"""Execute the RTCW entity boundary and shared records with real AAS link heaps."""
from pathlib import Path
import unittest
import rtcw_botlib_tests as bootstrap
import rtcw_navigation_tests as navigation
from rtcw_server_world_tests import function

ROOT = Path(__file__).resolve().parents[1]


def entity_source():
    source = navigation.navigation_source().replace(
        'void AAS_InvalidateEntities(){}', 'void AAS_InvalidateEntities();')
    sample = (ROOT / 'code/botlib/be_aas_sample.cpp').read_text()
    for signature in ('void AAS_PresenceTypeBoundingBox(', 'aas_link_t *AAS_LinkEntityClientBBox('):
        source += '\n' + function(sample, signature)
    source += '\n' + function((ROOT / 'code/botlib/be_aas_main.cpp').read_text(), 'float AAS_Time(')
    bsp = (ROOT / 'code/botlib/be_aas_bspq3.cpp').read_text()
    for signature in ('void AAS_BSPModelMinsMaxsOrigin(', 'void AAS_UnlinkFromBSPLeaves(',
                      'bsp_link_t *AAS_BSPLinkEntity('):
        source += '\n' + function(bsp, signature)
    entity = (ROOT / 'code/botlib/be_aas_entity.cpp').read_text()
    source += '\n' + function(entity, 'static void AAS_UnlinkEntityAreas(')
    source += '\n' + entity[entity.index('int AAS_UpdateEntity('):]
    source += navigation.CHECKS[:navigation.CHECKS.index('static bool enabled(')]
    return source


CHECKS = r'''
static int modelCalls, backendCalls;
static bool badModel;
static void modelBounds(int model,vec3_t angles,vec3_t lo,vec3_t hi,vec3_t origin){
 ++modelCalls;assert(model==7&&!origin);
 for(int n=0;n<3;++n){lo[n]=-float(model+n);hi[n]=float(model+n);}
 if(badModel){const unsigned nan=0x7fc00001u;std::memcpy(lo,&nan,4);}
}
static int update(int entity,bot_entitystate_t *state){++backendCalls;return AAS_UpdateEntity(entity,state);}
static bot_entitystate_t box(){
 bot_entitystate_t s{};s.type=1;s.solid=SOLID_BBOX;s.modelindex=7;
 s.origin[0]=100;s.origin[1]=5;s.origin[2]=3;
 for(int i=0;i<3;++i){s.mins[i]=-1-i;s.maxs[i]=1+i;s.angles[i]=20+i;s.old_origin[i]=2+i;}
 s.flags=0x5412;s.groundent=31;s.modelindex2=99;s.frame=71;s.event=113;
 s.eventParm=1;s.powerups=17;s.weapon=3;s.legsAnim=6;s.torsoAnim=5;return s;
}
static void checkRecordsAndWorldOwnership(){
 for(const auto &p:profiles){profile=&p;if(!p.bots.navigationWorlds)continue;const int worlds=p.bots.navigationWorlds;
  const bool rtcw=Com_GameIsRTCW();botimport.BSPModelMinsMaxsOrigin=modelBounds;
  assert(AAS_Setup()==0);auto *records=aasworld.entities;
  auto s=box();assert(AAS_UpdateEntity(1,&s)==BLERR_NOAASFILE);
  for(int w=0;w<worlds;++w)loadWorld(w,float(w*50));
  aasWorlds[0].initialized=1;aasWorlds[0].numframes=1;
  if(rtcw)assert(!aasworld.entities&&!aasworld.maxentities&&!aasworld.initialized);
  assert(&AAS_EntityWorld()==&aasWorlds[0]);
  auto *selected=aas_currentWorld;aasworld.time=10.25f;
  assert(AAS_UpdateEntity(-1,&s)==BLERR_INVALIDENTITYNUMBER);
  assert(AAS_UpdateEntity(1024,&s)==BLERR_INVALIDENTITYNUMBER);
  assert(AAS_UpdateEntity(1,&s)==0&&aas_currentWorld==selected);
  auto &ent=records[1];aas_entityinfo_t info{};AAS_EntityInfo(1,&info);
  assert(info.valid&&info.number==1&&info.ltime==10.25f&&info.update_time==10.25f);
  assert(info.type==s.type&&info.flags==s.flags&&info.modelindex==7&&info.modelindex2==99);
  assert(info.event==(rtcw?0:s.event)&&info.eventParm==s.eventParm);
  assert(info.weapon==s.weapon&&info.powerups==s.powerups&&info.frame==s.frame);
  assert(info.legsAnim==s.legsAnim&&info.torsoAnim==s.torsoAnim&&info.groundent==s.groundent);
  for(int i=0;i<3;++i){assert(info.origin[i]==s.origin[i]&&info.old_origin[i]==s.old_origin[i]);
   assert(info.angles[i]==(rtcw?0:s.angles[i])&&info.lastvisorigin[i]==0);
   assert(info.mins[i]==s.mins[i]&&info.maxs[i]==s.maxs[i]);}
  assert(ent.areas&&ent.areaWorld==selected&&ent.areas->entnum==1&&ent.areas->areanum==1);
  assert(selected->numaaslinks==selected->linkheapsize-1&&!ent.leaves);
  assert(AAS_EntityType(1)==s.type&&AAS_EntityModelNum(1)==7&&AAS_EntityModelindex(1)==7);
  assert(AAS_NextEntity(-1)==1&&AAS_NextEntity(1)==0&&AAS_NextEntity(INT_MAX)==0);
  for(int bad:{-1,1024,INT_MAX}){info.valid=17;AAS_EntityInfo(bad,&info);assert(!info.valid);}
  aasWorlds[0].numframes=2;
  // An unchanged record retains its links even after selection changes.
  AAS_SetCurrentWorld(0);aasworld.time=11;auto *originalLinks=ent.areas;
  assert(AAS_UpdateEntity(1,&s)==0&&ent.areas==originalLinks&&ent.areaWorld==selected);
  assert(ent.i.update_time==.75f&&ent.i.lastvisorigin[0]==100);
  // Movement detaches links from their old heap, then links in the selected graph.
  s.origin[0]=-100;assert(AAS_UpdateEntity(1,&s)==0);
  assert(ent.areaWorld==&aasWorlds[0]&&ent.areas->areanum==2&&aas_currentWorld==&aasWorlds[0]);
  if(rtcw)assert(selected->numaaslinks==selected->linkheapsize&&!selected->arealinkedentities[1]);
  assert(aasWorlds[0].numaaslinks==aasWorlds[0].linkheapsize-1);
  // Stored entity state belongs to world zero; time still belongs to selection.
  if(rtcw){AAS_SetCurrentWorld(1);aasworld.time=19;aasWorlds[0].numframes=0;
   s.origin[0]=100;s.type=4;assert(AAS_UpdateEntity(1,&s)==0&&ent.i.ltime==19);
   assert(aasWorlds[0].numaaslinks==aasWorlds[0].linkheapsize);
   vec3_t origin{};assert(AAS_OriginOfMoverWithModelNum(7,origin)&&origin[0]==100);
  }
  // BSP angles change bounds; a later BBOX update retains those angles in RTCW.
  s.solid=SOLID_BSP;modelCalls=0;assert(AAS_UpdateEntity(1,&s)==0&&modelCalls==1);
  assert(ent.i.angles[0]==20&&ent.i.mins[0]==-7&&ent.i.maxs[2]==9);
  s.solid=SOLID_BBOX;s.angles[0]=80;assert(AAS_UpdateEntity(1,&s)==0);
  assert(ent.i.angles[0]==(rtcw?20:80));
  if(rtcw){auto before=ent;badModel=true;s.solid=SOLID_BSP;
   rejected([&]{AAS_UpdateEntity(1,&s);});badModel=false;
   assert(!std::memcmp(&before.i,&ent.i,sizeof(ent.i))&&ent.areas==before.areas&&ent.areaWorld==before.areaWorld);
   s.solid=SOLID_BBOX;s.origin[0]=s.maxs[0]=std::numeric_limits<float>::max();
   rejected([&]{AAS_UpdateEntity(1,&s);});assert(ent.areas==before.areas&&ent.i.origin[0]==before.i.origin[0]);
  }
  // The world model gets a record but no area links.
  s=box();assert(AAS_UpdateEntity(ENTITYNUM_WORLD,&s)==0);
  assert(!records[ENTITYNUM_WORLD].areas&&!records[ENTITYNUM_WORLD].areaWorld);
  assert(AAS_NextEntity(1)==ENTITYNUM_WORLD);
  AAS_SetCurrentWorld(worlds-1);selected=aas_currentWorld;
  AAS_InvalidateEntities();assert(!ent.i.valid&&!records[ENTITYNUM_WORLD].i.valid);
  AAS_UnlinkInvalidEntities();assert(!ent.areas&&!ent.areaWorld&&aas_currentWorld==selected);
  for(int w=0;w<worlds;++w)assert(aasWorlds[w].numaaslinks==aasWorlds[w].linkheapsize);
  s=box();s.origin[0]+=1;assert(AAS_UpdateEntity(1,&s)==0&&ent.areas);
  AAS_SetCurrentWorld(0);assert(AAS_UpdateEntity(1,nullptr)==0&&!ent.areas&&!ent.areaWorld);
  // Map replacement forgets owners before their heaps are released.
  s.origin[0]+=1;assert(AAS_UpdateEntity(1,&s)==0&&ent.areas);
  AAS_SetCurrentWorld(worlds-1);AAS_ResetEntityLinks();assert(!ent.areas&&!ent.areaWorld);
  AAS_Shutdown();AAS_Shutdown();LibVarDeAllocAll();assert(allocations.empty());
  // Empty/failed setup must remain safe to query and shut down.
  AAS_EntityInfo(1,&info);assert(!info.valid&&AAS_NextEntity(INT_MAX)==0);
  assert(AAS_Setup()==0);failAllocation=0;
  rejected([&]{AAS_Setup();});failAllocation=-1;
  assert(!AAS_EntityWorld().entities);AAS_Shutdown();LibVarDeAllocAll();assert(allocations.empty());
 }
}
static void checkImports(){
 botlib_export_t backend{};backend.BotLibUpdateEntity=update;botlib_export=&backend;
 for(const auto &p:profiles){profile=&p;
  if(!Com_GameIsRTCW()){intptr_t args[3]={207,1,0},result=9;
   assert(!SV_RTCWBotLibCall(nullptr,Call::BOTLIB_UPDATENTITY,args,result)&&result==9);continue;}
  for(bool qvm:{false,true}){
   assert(AAS_Setup()==0);loadWorld(0,0);loadWorld(1,50);aasWorlds[0].initialized=1;
   std::array<byte,256> memory{};vm_t vm{};vm.dataBase=memory.data();vm.exactDataLength=memory.size();vm.dataMask=255;
   if(!qvm)vm.entryPoint=reinterpret_cast<decltype(vm.entryPoint)>(1);
   auto address=[&](size_t offset){return qvm?(intptr_t)offset:reinterpret_cast<intptr_t>(memory.data()+offset);};
   fntech3_rtcw_bot_entitystate_t external{};auto internal=box();
   static_assert(sizeof(external)==sizeof(internal));std::memcpy(&external,&internal,sizeof(external));
   auto write=[&]{std::memcpy(memory.data()+1,&external,sizeof(external));};write();
   auto invoke=[&](intptr_t entity,intptr_t pointer){intptr_t args[]={207,entity,pointer},result=99;
    assert(SV_RTCWBotLibCall(&vm,Lookup(p.id,Module::GAME,Direction::IMPORT,207),args,result));return result;};
   auto before=memory;assert(invoke(1,address(1))==0&&memory==before);
   assert(AAS_EntityWorld().entities[1].i.origin[0]==100);
   // Every vector component is checked, even fields ignored by this solid type.
   const int oldCalls=backendCalls;
   for(size_t offset=8;offset<68;offset+=4){for(unsigned bits:{0x7f800000u,0xff800000u,0x7fc00001u}){
    write();std::memcpy(memory.data()+1+offset,&bits,4);rejected([&]{invoke(1,address(1));});}}
   write();for(int bad:{-1,1024,INT_MAX})rejected([&]{invoke(bad,address(1));});
   rejected([&]{invoke(1,0);});
   external.mins[0]=external.maxs[0]+1;write();rejected([&]{invoke(1,address(1));});
   external.mins[0]=-1;external.origin[0]=external.maxs[0]=std::numeric_limits<float>::max();
   write();rejected([&]{invoke(1,address(1));});
   if(qvm){for(int offset:{-1,145,255,257,INT_MAX})rejected([&]{invoke(1,offset);});}
   else rejected([&]{invoke(1,static_cast<intptr_t>(UINTPTR_MAX-20));});
   if(sizeof(intptr_t)>4)rejected([&]{invoke(static_cast<intptr_t>(INT64_C(0x100000000)),address(1));});
   assert(backendCalls==oldCalls);
   std::memcpy(&external,&internal,sizeof(external));write();botlib_export=nullptr;
   rejected([&]{invoke(1,address(1));});botlib_export=&backend;
   // The last complete QVM record is accepted; no pointer masking is permitted.
   std::memcpy(memory.data()+144,&external,sizeof(external));assert(invoke(1023,address(144))==0);
   AAS_Shutdown();LibVarDeAllocAll();assert(allocations.empty());
  }
 }
 botlib_export=nullptr;
}
int main(){botimport.Print=print;checkRecordsAndWorldOwnership();checkImports();}
'''


class RTCWBotEntityTests(unittest.TestCase):
    compile_and_run = bootstrap.RTCWBotLibTests.compile_and_run

    def test_records_world_ownership_lifecycle_and_checked_module_boundary(self):
        self.compile_and_run(entity_source() + CHECKS,
            [str(ROOT / 'code/server/server_game_data.cpp'), '-x', 'c++',
             str(ROOT / 'code/compat/game_abi_adapter.c')], ['-O2', '-ffast-math'])


if __name__ == '__main__':
    unittest.main()
