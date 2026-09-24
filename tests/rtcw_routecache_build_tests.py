"""Execute cold RTCW cache generation, checked disk round-trips and failures."""
from pathlib import Path
import unittest
import rtcw_route_query_tests as routes
import rtcw_routing_tests
from rtcw_server_world_tests import function

ROOT = Path(__file__).resolve().parents[1]


def generation_source():
    source = routes.route_source().replace('static bool AAS_BuildRTCWRouteCache(){return false;}', '')
    reach = (ROOT / 'code/botlib/be_aas_reach.cpp').read_text()
    source += '\n' + function(reach, 'int AAS_AreaReachability(')
    sample = (ROOT / 'code/botlib/be_aas_sample.cpp').read_text()
    source += '\n' + function(sample, 'void AAS_PresenceTypeBoundingBox(')
    bsp = (ROOT / 'code/botlib/be_aas_bspq3.cpp').read_text()
    for signature in ('bsp_trace_t AAS_Trace(', 'qboolean AAS_inPVS('):
        source += '\n' + function(bsp, signature)
    # Geometry is controlled here; installed geometry is exercised by the
    # runtime probe. Keep trace callbacks independently observable.
    source += r'''
static int blockedArea, missedArea, escapedArea, invalidTrace, traceCalls, pvsCalls;
static bool traceDrop;
static int pointArea(vec3_t point){return int(point[0]/8);}
int AAS_PointAreaNum(vec3_t point){return pointArea(point);}
static void trace(bsp_trace_t *out,vec3_t start,vec3_t mins,vec3_t maxs,vec3_t end,int pass,int mask){
 ++traceCalls;assert(pass==-1);if(traceDrop)throw Drop{};
 *out={};VectorCopy(end,out->endpos);out->fraction=1;
 const int from=pointArea(start),to=pointArea(end);
 if(mins){
  assert(maxs&&mins[0]==-15&&mins[2]==-24&&maxs[2]==32);
  assert(mask==(CONTENTS_SOLID|(Com_GameIsRTCWSP()?CONTENTS_PLAYERCLIP|CONTENTS_MONSTERCLIP:0)));
  assert(end[2]==start[2]-256);out->endpos[2]=start[2]-16;out->fraction=.0625f;
  if(from==blockedArea)out->startsolid=qtrue;
  if(from==missedArea)out->fraction=1;
  if(from==escapedArea)out->endpos[0]+=8;
 }else{assert(!maxs&&mask==CONTENTS_SOLID);out->fraction=(from+to)%5==0?.5f:1.0f;}
 if(invalidTrace==1){uint32_t bits=0x7fc00000;std::memcpy(&out->fraction,&bits,4);}
 if(invalidTrace==2)out->endpos[1]=131073;
}
static int pvs(vec3_t from,vec3_t to){++pvsCalls;return (pointArea(from)+pointArea(to))%3!=0;}
static int shortWrite=-1,writeCalls;
static bool writeDenied;
static int writeFile(const void *data,int size,fileHandle_t handle){
 assert(fileOpen&&handle==1);++writeCalls;const int n=shortWrite<0?size:std::min(shortWrite,size);
 const auto *p=static_cast<const byte*>(data);files[currentFile].insert(files[currentFile].end(),p,p+n);return n;
}
'''
    original = function(source, 'static int openFile(')
    source = source.replace(original, original.replace('static int openFile(', 'static int readOpenFile('))
    source = source.replace('botimport.FS_FOpenFile=openFile;', 'botimport.FS_FOpenFile=readOpenFile;')
    source += r'''
static int openFile(const char *name,fileHandle_t *handle,fsMode_t mode){
 if(mode==FS_READ)return readOpenFile(name,handle,mode);
 assert(mode==FS_WRITE&&!fileOpen);*handle=0;if(writeDenied)return 0;
 opened.push_back(name);currentFile=name;files[name].clear();fileOffset=0;fileOpen=true;*handle=1;return 0;
}
#include "code/botlib/rtcw_routecache_writer.hpp"
#include "code/botlib/rtcw_routecache_build.hpp"
'''
    source += routes.GRAPH
    source += r'''
static void prepare(int ordinary=3){
 initialize();graph(0,ordinary,3);Q_strncpyz(aasworld.mapname,"probe",sizeof(aasworld.mapname));
 aasworld.initialized=qfalse;botimport.Print=checkedPrint;botimport.Trace=trace;botimport.inPVS=pvs;
 botimport.FS_FOpenFile=openFile;botimport.FS_Write=writeFile;
 blockedArea=missedArea=escapedArea=invalidTrace=traceCalls=pvsCalls=writeCalls=0;
 traceDrop=writeDenied=false;shortWrite=-1;
}
static void checkVisibility(){
 namespace rc=fntech3::rtcw::routecache;
 const int count=aasworld.numareas;std::vector<byte> decoded(count);
 for(int from=0;from<count;++from){
  const bool valid=from>0&&from!=blockedArea&&from!=missedArea&&from!=escapedArea;
  const auto &row=aasworld.rtcwVisibility[from];assert(bool(row.data)==valid);
  assert(rc::DecompressVisibility({row.data,size_t(row.bytes)},count,decoded.data(),decoded.size()));
  for(int to=0;to<count;++to){
   const bool target=to>0&&to!=blockedArea&&to!=missedArea&&to!=escapedArea;
   const bool expected=valid&&target&&(from==to||((from+to)%5!=0
    &&(!Com_GameIsRTCWSP()||(to>from&&(from+to)%3!=0))));
   assert(decoded[to]==expected);
  }
  if(valid){assert(aasworld.rtcwWaypoints[from][0]==from*8&&aasworld.rtcwWaypoints[from][2]==-16);}
  else for(float v:aasworld.rtcwWaypoints[from])assert(v==0);
 }
 assert(!aasworld.rtcwVisibilityScratch&&!aasworld.rtcwRoutingBuilding);
}
static void checkFile(const std::vector<byte> &bytes){
 namespace rc=fntech3::rtcw::routecache;
 rc::Signature signature{uint32_t(aasworld.numareas),uint32_t(aasworld.numclusters),{}};
 for(unsigned i=0;i<3;++i)signature.crc[i]=aasworld.rtcwNavigationCrc[i];
 rc::View view;assert(rc::Parse(bytes.data(),bytes.size(),Com_GameIsRTCWSP()?rc::Version::SP:rc::Version::MP,
  signature,view)==rc::Error::None);
 assert(rc::ValidateTopology(view,fntech3::rtcw::navigation::RouteTopology(aasworld))==rc::Error::None);
 assert(rc::U32(bytes.data()+28)>0&&rc::U32(bytes.data()+32)>0);
 size_t at=rc::HeaderBytes;
 for(const auto record:view.Caches()){
  for(int i=36;i<48;++i)assert(bytes[at+i]==0); // serialized process pointers
  for(size_t i=48+record.Count()*2;i<52+record.Count()*2;++i)assert(bytes[at+i]==0);
  at+=record.Bytes();
 }
}
'''
    return source


class RTCWRouteCacheBuildTests(unittest.TestCase):
    compile_and_run = rtcw_routing_tests.RTCWRoutingTests.compile_and_run

    def run_source(self, checks):
        self.compile_and_run(generation_source() + checks, rtcw_routing_tests.RTCWRoutingTests.extra(),
                             ['-O2', '-ffast-math'])

    def test_missing_bad_cache_generation_roundtrip_and_two_worlds(self):
        self.run_source(r'''
int main(){
 for(const auto &p:profiles){profile=&p;if(!Com_GameIsRTCW())continue;
  for(int corrupt:{0,1,2}){
   prepare(7);blockedArea=2;missedArea=3;escapedArea=4;
   if(corrupt)files["maps/probe.rcd"]=std::vector<byte>(corrupt==1?36:200,0x5a);
   assert(AAS_InitRTCWRouting()&&!aasworld.initialized&&aasworld.rtcwRouteCacheLoaded);
   checkVisibility();assert(aasworld.rtcwVisibilityOwnedRows&&writeCalls==1);
   assert(traceCalls>0&&(Com_GameIsRTCWSP()?pvsCalls>0:pvsCalls==0));
   const auto written=files.at("maps/probe.rcd");checkFile(written);
   const int traces=traceCalls;assert(AAS_InitRTCWRouting());
   assert(traceCalls==traces&&!aasworld.rtcwVisibilityOwnedRows&&aasworld.rtcwVisibilityData);
   checkVisibility();checkFile(written);
   // Generated and loaded cache rows have different owners; replacement and
   // repeated teardown must release either representation exactly once.
   assert(AAS_WriteRTCWRouteCache());checkFile(files.at("maps/probe.rcd"));
   assert(AAS_ReadRTCWRouteCache());checkVisibility();
   auto *primary=aasworld.rtcwVisibilityData;AAS_SetCurrentWorld(1);graph(0,7,3);
   Q_strncpyz(aasworld.mapname,"second",sizeof(aasworld.mapname));aasworld.initialized=qfalse;
   assert(AAS_InitRTCWRouting());checkVisibility();assert(aasworld.rtcwVisibilityOwnedRows);
   assert(aasWorlds[0].rtcwVisibilityData==primary&&!fileOpen);
   AAS_FreeRoutingCaches();AAS_FreeRoutingCaches();shutdown();
  }
 }
}
''')

    def test_partial_allocations_trace_failures_and_invalid_topology(self):
        self.run_source(r'''
int main(){
 for(const auto &p:profiles){profile=&p;if(!Com_GameIsRTCW())continue;
  bool completed=false;
  for(int fail=0;fail<300;++fail){prepare();failAllocation=fail;
   try{assert(AAS_InitRTCWRouting());completed=true;checkFile(files.at("maps/probe.rcd"));}catch(Drop&){}
   assert(!aasworld.initialized);failAllocation=-1;shutdown();if(completed)break;
  }assert(completed);
  for(int bad=0;bad<5;++bad){prepare();AAS_FreeRoutingCaches();
   if(bad<2)invalidTrace=bad+1;
   if(bad==2)aasworld.clusters[1].numareas=INT_MAX;
   if(bad==3)aasworld.areasettings[1].cluster=INT_MAX;
   if(bad==4)aasworld.portals[1].areanum=1;
   assert(!AAS_InitRTCWRouting()&&!aasworld.initialized&&writeCalls==0);shutdown();
  }
  prepare();traceDrop=true;try{AAS_InitRTCWRouting();assert(false);}catch(Drop&){}
  assert(!aasworld.initialized);shutdown();
 }
}
''')

    def test_write_failure_is_nonfatal_and_damaged_file_regenerates(self):
        self.run_source(r'''
int main(){
 for(const auto &p:profiles){profile=&p;if(!Com_GameIsRTCW())continue;
  for(int failure:{-2,0,35,100}){prepare();
   writeDenied=failure==-2;shortWrite=failure;
   assert(AAS_InitRTCWRouting()&&aasworld.rtcwRouteCacheLoaded&&!fileOpen&&!aasworld.rtcwLoadBytes);
   checkVisibility();const int traces=traceCalls;
   writeDenied=false;shortWrite=-1;assert(AAS_InitRTCWRouting());assert(traceCalls>traces);
   checkFile(files.at("maps/probe.rcd"));checkVisibility();shutdown();
  }
  prepare();assert(AAS_InitRTCWRouting());const auto good=files.at("maps/probe.rcd");
  // Validate before opening/truncating an existing file.
  uint32_t nan=0x7fc00000;std::memcpy(aasworld.rtcwWaypoints[1],&nan,4);
  assert(!AAS_WriteRTCWRouteCache()&&files.at("maps/probe.rcd")==good&&!fileOpen&&!aasworld.rtcwLoadBytes);
  aasworld.rtcwWaypoints[1][0]=8;
  for(const char *name:{"../outside","/absolute","C:outside","a\\b"}){
   Q_strncpyz(aasworld.mapname,name,sizeof(aasworld.mapname));const auto before=opened.size();
   assert(!AAS_WriteRTCWRouteCache()&&opened.size()==before);
  }shutdown();
 }
}
''')


if __name__ == '__main__':
    unittest.main()
