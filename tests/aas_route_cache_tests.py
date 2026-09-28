"""Read Quake III route cache dumps only after checking every cache against the map.

The fixture builds the real Quake III routing over the route query fixture's
cluster rings, routes every area to every other under two sets of travel
flags, writes the caches with the real AAS_WriteRouteCache and reads them back
with the real AAS_ReadRouteCache (maps/NAME.rcd, docs/fntech3/2026-09-28-route-cache-checks.md).
Every cache must come back as the routing made it, linked where routing looks
for it and in the least-recently-used list, and every route must be answered
from them without a cache being made. Then each thing the reader checks is
broken alone -- the header, the counts against the length, each cache's
extent, kind, area, cluster, number and size, and each reachability a route
query can take -- and the whole file must be refused with its message, with
the file closed, nothing linked and nothing left allocated, before routing
builds its caches as it goes. The binary also holds each reader of MUTATIONS,
each missing one check or obligation, and each of those must fail.
"""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import time
import unittest

from compiler_support import compiler_target_args
import rtcw_route_query_tests
import rtcw_routing_tests
from rtcw_server_world_tests import function

ROOT = Path(__file__).resolve().parents[1]

# The reader's functions, in the order be_aas_route.cpp defines them.
READER = ('static long long AAS_RoutingCacheBytes(', 'static bool AAS_RouteCacheAreaInCluster(',
          'static bool AAS_RouteCacheCountsFit(', 'static int AAS_RouteCacheRecordSize(',
          'static bool AAS_RouteCacheRecordsFit(', 'static int AAS_ReadRouteCache(')

# Each mutation replaces text in the reader and must make the checks fail.
MUTATIONS = {
    'a file shorter than its header is read': (
        'length < static_cast<int>(sizeof(routecacheheader_t))\n\t\t|| ', ''),
    'a header read short is trusted': (
        'fp ) != static_cast<int>(sizeof(routecacheheader_t))', 'fp ) < 0'),
    'a file that is no dump stays open': (
        'AAS_Error("%s is not a route cache dump\\n", filename);\n\t\tbotimport.FS_FCloseFile(fp);',
        'AAS_Error("%s is not a route cache dump\\n", filename);'),
    'a dump of another version stays open': (
        'should be %d\\n", routecacheheader.version, RCVERSION);\n\t\tbotimport.FS_FCloseFile(fp);',
        'should be %d\\n", routecacheheader.version, RCVERSION);'),
    'a dump of another AAS file stays open': (
        'aasworld.numclusters))\n\t{\n\t\tbotimport.FS_FCloseFile(fp);\n\t\treturn qfalse;',
        'aasworld.numclusters))\n\t{\n\t\treturn qfalse;'),
    'counts that cannot fit leave the file open': (
        'AAS_Error("%s: %s\\n", filename, fault);\n\t\tbotimport.FS_FCloseFile(fp);',
        'AAS_Error("%s: %s\\n", filename, fault);'),
    'a file of any length is read whole': ('if (length > MAX_ROUTECACHE_BYTES)', 'if (false)'),
    'a negative count of portal caches is taken': ('routecacheheader.numportalcache < 0 || ', ''),
    'a negative count of area caches is taken': ('routecacheheader.numareacache < 0\n\t\t|| ', ''),
    'more portal caches than the file holds are read': (
        '|| routecacheheader.numportalcache > length / portalbytes\n\t\t', ''),
    'more area caches than the file holds are read': (
        '\n\t\t|| routecacheheader.numareacache > (length - routecacheheader.numportalcache * portalbytes) '
        '/ AAS_RoutingCacheBytes(0)', ''),
    'a file read short is checked as though whole': (
        'if (numread != length || !AAS_RouteCacheRecordsFit(', 'if (!AAS_RouteCacheRecordsFit('),
    'the file is read without its checks': (
        'if (numread != length || !AAS_RouteCacheRecordsFit(routecacheheader, caches, length, fault, sizeof(fault)))',
        'if (numread != length)'),
    'a refused file keeps its bytes': (
        'AAS_Error("%s: %s\\n", filename, fault);\n\t\tif (caches) FreeMemory(caches);',
        'AAS_Error("%s: %s\\n", filename, fault);'),
    'a read file keeps its bytes': (
        '\tif (caches) FreeMemory(caches);\n\tbotimport.Print(', '\tbotimport.Print('),
    'a cache too short for its members is copied': (
        'if (remaining < static_cast<int>(sizeof(aas_routingcache_t)))', 'if (false)'),
    'a cache of the wrong kind is listed': (
        'if (stored.type != (portal ? CACHETYPE_PORTAL : CACHETYPE_AREA))', 'if (false)'),
    'area 0 and below are taken': ('stored.areanum <= 0 || stored.areanum >= aasworld.numareas',
                                   'stored.areanum >= aasworld.numareas'),
    'areas past the map are taken': ('stored.areanum <= 0 || stored.areanum >= aasworld.numareas',
                                     'stored.areanum <= 0'),
    'cluster 0 and below are taken': ('stored.cluster <= 0 || stored.cluster >= aasworld.numclusters',
                                      'stored.cluster >= aasworld.numclusters'),
    'clusters past the map are taken': ('stored.cluster <= 0 || stored.cluster >= aasworld.numclusters',
                                        'stored.cluster <= 0'),
    "an area outside the cache's cluster is taken": (
        'if (!AAS_RouteCacheAreaInCluster(stored.areanum, stored.cluster))', 'if (false)'),
    "a portal past the map's portals is followed": (
        'if (areacluster <= -aasworld.numportals) return false;', ''),
    'a number past the cluster is linked': (
        'if (number < 0 || number >= aasworld.clusters[stored.cluster].numareas)', 'if (number < 0)'),
    'a negative number is linked': (
        'if (number < 0 || number >= aasworld.clusters[stored.cluster].numareas)',
        'if (number >= aasworld.clusters[stored.cluster].numareas)'),
    'a cache of any size is taken': (
        'if (numtraveltimes < 0 || stored.size != AAS_RoutingCacheBytes(numtraveltimes))',
        'if (numtraveltimes < 0)'),
    'a cluster of negative reachability areas takes a cache smaller than any': (
        'if (numtraveltimes < 0 || stored.size', 'if (stored.size'),
    'a cache past the end of the file is taken': ('if (stored.size > remaining)', 'if (false)'),
    'any reachability is taken': ('if (reachabilities[i] < bound) continue;', 'continue;'),
    "a portal's area gives no bound": ('\t\t\tfewer(-settings->cluster, reachabilities);\n', ''),
    "a cluster's areas give no bound": (
        '\t\t\t\tfewer(start[joined[side]] + number, reachabilities);', '\t\t\t\t;'),
    'a portal bounds only its front cluster': (
        'joined[1] = aasworld.portals[-settings->cluster].backcluster;', 'joined[1] = 0;'),
    'the last of the areas sharing a number bounds it': (
        'if (fewest[slot] < 0 || count < fewest[slot]) fewest[slot] = count;', 'fewest[slot] = count;'),
    'numbers past a cluster are bounded by the next': (
        'else if (i < aasworld.clusters[stored.cluster].numareas) bound', 'else if (true) bound'),
    'bytes after the last cache are ignored': ('if (size && at != length)', 'if (false)'),
    'the scratch of the checks is kept': ('\tFreeMemory(fewest);\n', ''),
    'a read cache is not in the least-recently-used list': ('\t\tAAS_LinkCache(cache);\n', ''),
}

CHECKS = r'''
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <tuple>
#ifdef _WIN32
extern "C" __declspec(dllimport) unsigned int __stdcall SetErrorMode(unsigned int);
#endif
static int (*reader)();
static bool writing;
static std::string writeName;
static int openAny(const char *name,fileHandle_t *handle,fsMode_t mode){
 if(mode!=FS_WRITE)return openFile(name,handle,mode);
 assert(!writing&&!fileOpen);writing=true;writeName=name;files[name].clear();*handle=2;return 0;
}
static int writeAny(const void *data,int length,fileHandle_t handle){
 assert(writing&&handle==2&&length>=0);auto &file=files[writeName];
 file.insert(file.end(),static_cast<const byte*>(data),static_cast<const byte*>(data)+length);return length;
}
static void closeAny(fileHandle_t handle){if(handle==2){assert(writing);writing=false;}else closeFile(handle);}
static void useFiles(){botimport.FS_FOpenFile=openAny;botimport.FS_Write=writeAny;botimport.FS_FCloseFile=closeAny;}

// A cache as the routing holds it, pointers aside.
struct Cache{
 int type,cluster,area,flags,size;float time,start,origin[3];std::vector<unsigned short> times;std::vector<byte> reach;
 bool operator==(const Cache &o)const{return std::tie(type,cluster,area,flags,size,time,start,times,reach)
  ==std::tie(o.type,o.cluster,o.area,o.flags,o.size,o.time,o.start,o.times,o.reach)
  &&!std::memcmp(origin,o.origin,sizeof(origin));}
 bool operator<(const Cache &o)const{return std::tie(type,area,cluster,flags)<std::tie(o.type,o.area,o.cluster,o.flags);}
};
static Cache capture(const byte *memory){
 aas_routingcache_t c;std::memcpy(&c,memory,sizeof(c));
 const int n=(c.size-(int)sizeof(aas_routingcache_t))/3;assert(c.size==(int)sizeof(aas_routingcache_t)+3*n);
 Cache out{c.type,c.cluster,c.areanum,c.travelflags,c.size,c.time,c.starttraveltime,{c.origin[0],c.origin[1],c.origin[2]},{},{}};
 for(int i=0;i<n;++i){unsigned short t;std::memcpy(&t,memory+offsetof(aas_routingcache_t,traveltimes)+2*i,2);
  out.times.push_back(t);out.reach.push_back(memory[sizeof(aas_routingcache_t)+2*n+i]);}
 return out;
}
// Every cache linked, oldest first.
static std::vector<Cache> lru(){
 std::vector<Cache> out;
 for(auto *c=aasworld.oldestcache;c;c=c->time_next){
  const int n=(c->size-(int)sizeof(aas_routingcache_t))/3;
  assert(c->reachabilities==reinterpret_cast<const byte*>(c)+sizeof(aas_routingcache_t)+2*n);
  out.push_back(capture(reinterpret_cast<const byte*>(c)));
 }
 return out;
}
static std::vector<Cache> sorted(std::vector<Cache> v){std::sort(v.begin(),v.end());return v;}
static int field(const std::vector<byte> &f,size_t at){int v;std::memcpy(&v,f.data()+at,4);return v;}
template<class T>static void poke(std::vector<byte> &f,size_t at,T v){std::memcpy(f.data()+at,&v,sizeof(v));}
struct Record{size_t at;int size;bool portal;int index,type,cluster,area;};
static std::vector<Record> records(const std::vector<byte> &f){
 const int portals=field(f,offsetof(routecacheheader_t,numportalcache));
 const int areas=field(f,offsetof(routecacheheader_t,numareacache));
 std::vector<Record> out;size_t at=sizeof(routecacheheader_t);
 for(int i=0;i<portals+areas;++i){
  const int size=field(f,at+offsetof(aas_routingcache_t,size));
  out.push_back({at,size,i<portals,i<portals?i:i-portals,f[at+offsetof(aas_routingcache_t,type)],
   field(f,at+offsetof(aas_routingcache_t,cluster)),field(f,at+offsetof(aas_routingcache_t,areanum))});
  at+=size;
 }
 assert(at==f.size());return out;
}
static std::vector<Cache> inFile(const std::vector<byte> &f){
 std::vector<Cache> out;for(const auto &r:records(f))out.push_back(capture(f.data()+r.at));return out;
}
// The file with one record first among those of its kind.
static std::vector<byte> leading(const std::vector<byte> &f,const Record &lead){
 std::vector<byte> out(f.begin(),f.begin()+sizeof(routecacheheader_t));
 for(bool portal:{true,false})for(bool first:{true,false})for(const auto &r:records(f))
  if(r.portal==portal&&(r.at==lead.at)==first)out.insert(out.end(),f.begin()+r.at,f.begin()+r.at+r.size);
 assert(out.size()==f.size());return out;
}
static std::string kind(const Record &r){return r.portal?"portal":"area";}
static bool inCluster(int area,int cluster){
 const int c=aasworld.areasettings[area].cluster;
 if(c>=0)return c==cluster;
 return aasworld.portals[-c].frontcluster==cluster||aasworld.portals[-c].backcluster==cluster;
}
// The area routing numbers `number` in `cluster`, and 0 where none is.
static int numbered(int cluster,int number){
 for(int a=1;a<aasworld.numareas;++a)
  if(inCluster(a,cluster)&&AAS_ClusterAreaNum(cluster,a)==number)return a;
 return 0;
}
struct Answer{int ok,time,reach;bool operator==(const Answer &o)const{return ok==o.ok&&time==o.time&&reach==o.reach;}};
static std::vector<Answer> answers(int flags){
 std::vector<Answer> out;
 for(int a=1;a<aasworld.numareas;++a)for(int g=1;g<aasworld.numareas;++g){
  int time=-1,reach=-1;aasworld.frameroutingupdates=0;
  const int ok=AAS_AreaRouteToGoalArea(a,aasworld.areas[a].center,g,flags,&time,&reach);
  out.push_back({ok,ok?time:-1,ok?reach:-1});
 }
 return out;
}
static void fresh(){routingTables();aasErrors=0;lastError.clear();}
static bool emptyRouting(){
 if(aasworld.oldestcache||aasworld.newestcache||aasworld.routingcachesize)return false;
 for(int a=0;a<aasworld.numareas;++a)if(aasworld.portalcache[a])return false;
 for(int c=0;c<aasworld.numclusters;++c)for(int j=0;j<aasworld.clusters[c].numareas;++j)
  if(aasworld.clusterareacache[c][j])return false;
 return true;
}
static std::string fault(const std::string &text){return "maps/probe.rcd: "+text+"\n";}
static const char *const notDump="maps/probe.rcd is not a route cache dump\n";
// The file must be refused whole with `message` ("" for a silent refusal),
// closed, with nothing linked and nothing it allocated left allocated.
static void refused(const std::vector<byte> &data,const std::string &message,bool allocates=true){
 fresh();files["maps/probe.rcd"]=data;
 const auto live=allocations;const int count=allocationCount;
 const int loaded=reader();
 const bool reported=message.empty()?aasErrors==0:aasErrors==1&&lastError==message;
 if(loaded||!reported){
  std::fprintf(stderr,"expected refusal: %s\ngot %d, %d errors: %s\n",message.c_str(),loaded,aasErrors,lastError.c_str());
  std::exit(4);
 }
 assert(!fileOpen&&allocations==live&&emptyRouting());
 if(!allocates)assert(allocationCount==count);
}
// The file must load: its caches linked in its order, counted, and closed.
static void loads(const std::vector<byte> &data){
 fresh();files["maps/probe.rcd"]=data;
 const int loaded=reader();
 if(loaded!=1||aasErrors){std::fprintf(stderr,"expected a load, got %d: %s\n",loaded,lastError.c_str());std::exit(5);}
 assert(!fileOpen);cacheLinks();
 assert(lru()==inFile(data));
 assert(aasworld.routingcachesize==(int)(data.size()-sizeof(routecacheheader_t)));
}

static const int flagSets[]={TFL_WALK|TFL_AIR,TFL_DEFAULT};
static std::vector<byte> good;

// Routes everything, writes the caches, and reads them back.
static void roundTrip(unsigned seed,int ordinary,int clusters){
 graph(seed,ordinary,clusters);
 Q_strncpyz(aasworld.mapname,"probe",sizeof(aasworld.mapname));
 std::vector<Answer> before[2];
 for(int f=0;f<2;++f)before[f]=answers(flagSets[f]);
 cacheLinks();
 const auto made=sorted(lru());const int bytes=aasworld.routingcachesize;
 assert(!made.empty());
 files.erase("maps/probe.rcd");AAS_WriteRouteCache();assert(!writing);
 good=files.at("maps/probe.rcd");
 assert(good.size()==sizeof(routecacheheader_t)+bytes&&sorted(inFile(good))==made);
 loads(good);
 assert(sorted(lru())==made);
 // every route comes from the caches read, and none is made
 const auto count=lru().size();
 for(int f=0;f<2;++f){assert(answers(flagSets[f])==before[f]);}
 assert(aasworld.routingcachesize==bytes&&lru().size()==count);cacheLinks();
 // the order of the caches of a kind is free
 auto reversed=good;const auto all=records(good);size_t at=sizeof(routecacheheader_t);
 for(bool portal:{true,false})for(auto r=all.rbegin();r!=all.rend();++r)if(r->portal==portal){
  std::memcpy(reversed.data()+at,good.data()+r->at,r->size);at+=r->size;}
 loads(reversed);
 for(int f=0;f<2;++f)assert(answers(flagSets[f])==before[f]);
 // a refused file leaves routing to build its caches as it goes
 auto stale=good;poke<int>(stale,offsetof(routecacheheader_t,areacrc),field(good,offsetof(routecacheheader_t,areacrc))^1);
 refused(stale,"");
 for(int f=0;f<2;++f)assert(answers(flagSets[f])==before[f]);
 assert(sorted(lru())==made);cacheLinks();
}

static void damaged(){
 const auto all=records(good);
 const size_t H=sizeof(routecacheheader_t),T=sizeof(aas_routingcache_t);
 const int L=(int)(good.size()-H),P=field(good,offsetof(routecacheheader_t,numportalcache));
 const int A=field(good,offsetof(routecacheheader_t,numareacache));
 const int portalBytes=(int)T+3*aasworld.numportals;
 assert(P>0&&A>=20&&(int)all.size()==P+A);
 const Record &portal0=all[0],&area0=all[P],&last=all.back();
 const std::string lastName="area cache "+std::to_string(A-1);
 auto header=[&](size_t offset,int value){auto d=good;poke<int>(d,offset,value);return d;};
 using H_=routecacheheader_t;

 // the header
 refused(header(offsetof(H_,ident),RCID+1),notDump,false);
 refused(header(offsetof(H_,version),RCVERSION+1),"route cache dump has wrong version 3, should be 2\n",false);
 for(size_t offset:{offsetof(H_,numareas),offsetof(H_,numclusters),offsetof(H_,areacrc),offsetof(H_,clustercrc)})
  refused(header(offset,field(good,offset)+1),"",false);
 for(size_t n=0;n<H;++n)refused({good.begin(),good.begin()+n},notDump,false);
 fileSizeOverride=(int)H-1;refused(good,notDump,false);fileSizeOverride=-1;
 shortRead=10;refused(good,notDump,false);shortRead=-1;

 // the counts against the length, before anything is allocated
 fileSizeOverride=INT_MAX;
 refused(good,fault("has "+std::to_string(INT_MAX-(int)H)+" bytes of caches, more than the 268435456 read"),false);
 fileSizeOverride=(int)H+268435456;
 refused(good,fault("could read "+std::to_string(L)+" of its 268435456 bytes of caches"));
 fileSizeOverride=-1;
 const auto cannot=[&](int portals,int areas){
  return fault("cannot hold "+std::to_string(portals)+" portal and "+std::to_string(areas)+" area caches in "+std::to_string(L)+" bytes");};
 refused(header(offsetof(H_,numportalcache),-1),cannot(-1,A),false);
 refused(header(offsetof(H_,numareacache),-1),cannot(P,-1),false);
 refused(header(offsetof(H_,numportalcache),L/portalBytes+1),cannot(L/portalBytes+1,A),false);
 {std::vector<byte> d(good.begin(),good.begin()+H);d.resize(H+portalBytes-5,0);
  poke<int>(d,offsetof(H_,numportalcache),1);poke<int>(d,offsetof(H_,numareacache),0);
  refused(d,fault("cannot hold 1 portal and 0 area caches in "+std::to_string(portalBytes-5)+" bytes"),false);}
 for(int count:{INT_MAX}){refused(header(offsetof(H_,numportalcache),count),cannot(count,A),false);
  refused(header(offsetof(H_,numareacache),count),cannot(P,count),false);}
 const int most=(L-P*portalBytes)/(int)T;
 refused(header(offsetof(H_,numareacache),most+1),cannot(P,most+1),false);
 refused(header(offsetof(H_,numareacache),most),
  fault("area cache "+std::to_string(A)+" starts 0 bytes from the end of the file, too few for a cache"));
 refused(header(offsetof(H_,numportalcache),P+1),fault("portal cache "+std::to_string(P)+" has type 1"));
 refused(header(offsetof(H_,numareacache),A-1),fault("has "+std::to_string(last.size)+" bytes after its last cache"));
 {auto d=good;d.insert(d.end(),3,0);refused(d,fault("has 3 bytes after its last cache"));}
 shortRead=(int)H+5;refused(good,fault("could read "+std::to_string(H+5)+" of its "+std::to_string(L)+" bytes of caches"));
 shortRead=-1;
 {auto d=good;d.pop_back();
  refused(d,fault(lastName+" is "+std::to_string(last.size)+" bytes, more than the "+std::to_string(last.size-1)+" left in the file"));}
 refused({good.begin(),good.begin()+last.at+30},
  fault(lastName+" starts 30 bytes from the end of the file, too few for a cache"));
 for(size_t n=H;n<good.size();++n){
  fresh();files["maps/probe.rcd"]={good.begin(),good.begin()+n};const auto live=allocations;
  assert(!reader()&&aasErrors==1&&!fileOpen&&allocations==live&&emptyRouting());
 }
 {std::vector<byte> none(good.begin(),good.begin()+H);
  poke<int>(none,offsetof(H_,numportalcache),0);poke<int>(none,offsetof(H_,numareacache),0);
  loads(none);assert(emptyRouting());}

 // each cache's kind, area, cluster and size
 const auto at=[&](const Record &r,size_t member){return r.at+member;};
 const auto name=[&](const Record &r){return kind(r)+" cache "+std::to_string(r.index);};
 for(const Record *r:{&portal0,&area0}){
  for(int type:{r->portal?CACHETYPE_AREA:CACHETYPE_PORTAL,7}){auto d=good;d[at(*r,offsetof(aas_routingcache_t,type))]=(byte)type;
   refused(d,fault(name(*r)+" has type "+std::to_string(type)));}
  for(int area:{0,aasworld.numareas,-1,INT_MIN}){auto d=good;poke<int>(d,at(*r,offsetof(aas_routingcache_t,areanum)),area);
   refused(d,fault(name(*r)+" is for area "+std::to_string(area)+", which the map lacks"));}
  for(int cluster:{0,aasworld.numclusters,-1,INT_MAX}){auto d=good;poke<int>(d,at(*r,offsetof(aas_routingcache_t,cluster)),cluster);
   refused(d,fault(name(*r)+" is for cluster "+std::to_string(cluster)+", which the map lacks"));}
  for(int c=1;c<aasworld.numclusters;++c)if(!inCluster(r->area,c)){auto d=good;
   poke<int>(d,at(*r,offsetof(aas_routingcache_t,cluster)),c);
   refused(d,fault(name(*r)+" is for area "+std::to_string(r->area)+", which is not in cluster "+std::to_string(c)));}
  const int n=r->portal?aasworld.numportals:aasworld.clusters[r->cluster].numreachabilityareas;
  for(int size:{r->size+1,r->size-1,r->size+3,(int)T}){auto d=good;poke<int>(d,at(*r,offsetof(aas_routingcache_t,size)),size);
   refused(d,fault(name(*r)+" is "+std::to_string(size)+" bytes, not the size of a cache of "+std::to_string(n)+" travel times"));}
  // a dump written with pointers of the other size
  const size_t other=T==64?88:64,otherTimes=T==64?80:60;
  std::vector<byte> moved(other+3*n,0);std::memcpy(moved.data(),good.data()+r->at,40);
  std::memcpy(moved.data()+otherTimes,good.data()+r->at+offsetof(aas_routingcache_t,traveltimes),2*n);
  std::memcpy(moved.data()+other+2*n,good.data()+r->at+T+2*n,n);poke<int>(moved,offsetof(aas_routingcache_t,size),(int)(other+3*n));
  auto d=good;d.erase(d.begin()+r->at,d.begin()+r->at+r->size);d.insert(d.begin()+r->at,moved.begin(),moved.end());
  refused(d,fault(name(*r)+" is "+std::to_string(other+3*n)+" bytes, not the size of a cache of "+std::to_string(n)+" travel times"));
 }
 // a portal area's cache, moved out of the clusters it joins
 for(const auto &r:all)if(!r.portal&&aasworld.areasettings[r.area].cluster<0){
  for(int c=1;c<aasworld.numclusters;++c)if(!inCluster(r.area,c)){auto d=good;
   poke<int>(d,at(r,offsetof(aas_routingcache_t,cluster)),c);
   refused(d,fault(name(r)+" is for area "+std::to_string(r.area)+", which is not in cluster "+std::to_string(c)));}
  break;
 }

 // an area whose cluster names no portal: first its own caches, and a portal
 // past the table that would take the cache in, were it followed
 {const Record &target=area0;const int area=target.area,saved=aasworld.areasettings[area].cluster;
  auto *portals=botlib::ClearedAlloc<aas_portal_t>(aasworld.numportals+1);
  std::memcpy(portals,aasworld.portals,sizeof(aas_portal_t)*aasworld.numportals);
  FreeMemory(aasworld.portals);aasworld.portals=portals;
  auto &past=portals[aasworld.numportals];past.areanum=area;past.frontcluster=past.backcluster=target.cluster;
  past.clusterareanum[0]=past.clusterareanum[1]=aasworld.areasettings[area].clusterareanum;
  for(int cluster:{-aasworld.numportals,INT_MIN}){aasworld.areasettings[area].cluster=cluster;
   const Record *lead=nullptr;for(const auto &r:all)if(r.area==area){lead=&r;break;}
   refused(leading(good,*lead),fault(kind(*lead)+" cache 0 is for area "+std::to_string(area)
    +", which is not in cluster "+std::to_string(lead->cluster)));}
  aasworld.areasettings[area].cluster=saved;}

 // a portal numbered outside a cluster it joins
 for(const auto &r:all)if(!r.portal&&aasworld.areasettings[r.area].cluster<0){
  auto &portal=aasworld.portals[-aasworld.areasettings[r.area].cluster];
  const int side=portal.frontcluster!=r.cluster,saved=portal.clusterareanum[side];
  for(int number:{aasworld.clusters[r.cluster].numareas,-1,INT_MAX}){portal.clusterareanum[side]=number;
   refused(leading(good,r),fault("area cache 0 is for area "+std::to_string(r.area)+", whose number "+std::to_string(number)
    +" in cluster "+std::to_string(r.cluster)+" is outside its "+std::to_string(aasworld.clusters[r.cluster].numareas)+" areas"));}
  portal.clusterareanum[side]=saved;break;
 }

 // a cluster without reachability areas, whose cache would be smaller than any
 {const Record &r=area0;auto &cluster=aasworld.clusters[r.cluster];const int saved=cluster.numreachabilityareas;
  cluster.numreachabilityareas=-1;auto d=leading(good,r);
  poke<int>(d,offsetof(H_,clustercrc),(int)CRC_ProcessArray(aasworld.clusters,aasworld.numclusters));
  poke<int>(d,H+P*portalBytes+offsetof(aas_routingcache_t,size),(int)T-3);
  refused(d,fault("area cache 0 is "+std::to_string((int)T-3)+" bytes, not the size of a cache of -1 travel times"));
  cluster.numreachabilityareas=saved;}

 // the reachabilities a route query takes
 {bool found=false;
  for(const auto &r:all){if(r.portal)continue;const int n=aasworld.clusters[r.cluster].numreachabilityareas;
   for(int j=0;j<n&&!found;++j){const size_t byteAt=r.at+T+2*n+j;if(!good[byteAt])continue;
    const int area=numbered(r.cluster,j),count=aasworld.areasettings[area].numreachableareas;assert(area&&count>1);
    auto d=good;d[byteAt]=(byte)count;
    refused(d,fault(name(r)+" gives number "+std::to_string(j)+" of cluster "+std::to_string(r.cluster)
     +" reachability "+std::to_string(count)+", which its area lacks"));
    d[byteAt]=(byte)(count-1);loads(d);found=true;}
   if(found)break;}
  assert(found);}
 {int p=1;while(p<aasworld.numportals&&aasworld.areasettings[aasworld.portals[p].areanum].numreachableareas<2)++p;
  assert(p<aasworld.numportals);const int count=aasworld.areasettings[aasworld.portals[p].areanum].numreachableareas;
  const size_t reach=portal0.at+T+2*aasworld.numportals;auto d=good;
  for(int j=0;j<aasworld.numportals;++j)assert(!good[reach+j]);  // Quake III's routing leaves them 0
  d[reach+p]=(byte)count;
  refused(d,fault("portal cache 0 gives portal "+std::to_string(p)+" reachability "+std::to_string(count)+", which its area lacks"));
  d[reach+p]=(byte)(count-1);loads(d);
  d=good;d[reach]=1;refused(d,fault("portal cache 0 gives portal 0 reachability 1, which its area lacks"));}
 // a portal sharing number 0 of its front cluster with an area of fewer reachabilities
 {int p=1;const aas_portal_t *portal=nullptr;
  for(;p<aasworld.numportals;++p){portal=&aasworld.portals[p];if(portal->frontcluster!=portal->backcluster)break;}
  assert(p<aasworld.numportals);
  const int cluster=portal->frontcluster,member=numbered(cluster,0),shared=aasworld.portals[p].clusterareanum[0];
  const int few=aasworld.areasettings[member].numreachableareas,many=aasworld.areasettings[portal->areanum].numreachableareas;
  assert(member>0&&aasworld.areasettings[member].cluster==cluster&&few<many);
  const Record *target=nullptr;for(const auto &r:all)if(!r.portal&&r.cluster==cluster){target=&r;break;}
  aasworld.portals[p].clusterareanum[0]=0;
  const int n=aasworld.clusters[cluster].numreachabilityareas;
  auto d=leading(good,*target);const size_t reach=H+P*portalBytes+T+2*n;
  d[reach+shared]=0;d[reach]=(byte)few;
  refused(d,fault("area cache 0 gives number 0 of cluster "+std::to_string(cluster)+" reachability "+std::to_string(few)+", which its area lacks"));
  d[reach]=(byte)(few-1);
  for(const auto &r:records(d))if(!r.portal&&r.cluster==cluster)d[r.at+T+2*n+shared]=0;
  loads(d);
  aasworld.portals[p].clusterareanum[0]=shared;}
 // a number past the cluster's areas, in a cluster with more reachability areas than areas
 {const Record &r=area0;auto &cluster=aasworld.clusters[r.cluster];const int n=cluster.numreachabilityareas;
  std::vector<byte> grown(T+3*(n+1),0);std::memcpy(grown.data(),good.data()+r.at,T+2*n);
  std::memcpy(grown.data()+T+2*(n+1),good.data()+r.at+T+2*n,n);grown[T+2*(n+1)+n]=1;
  poke<int>(grown,offsetof(aas_routingcache_t,size),(int)grown.size());
  auto d=leading(good,r);const size_t first=H+P*portalBytes;
  d.erase(d.begin()+first,d.begin()+first+r.size);d.insert(d.begin()+first,grown.begin(),grown.end());
  cluster.numreachabilityareas=cluster.numareas+1;assert(n==cluster.numareas);
  poke<int>(d,offsetof(H_,clustercrc),(int)CRC_ProcessArray(aasworld.clusters,aasworld.numclusters));
  refused(d,fault("area cache 0 gives number "+std::to_string(n)+" of cluster "+std::to_string(r.cluster)
   +" reachability 1, which its area lacks"));
  cluster.numreachabilityareas=n;}
}

int main(int argc,char **argv){
#ifdef _MSC_VER
 _set_abort_behavior(0,_WRITE_ABORT_MSG|_CALL_REPORTFAULT);
#endif
#ifdef _WIN32
 SetErrorMode(0x0001|0x0002);
#endif
 const int variant=argc>1?std::atoi(argv[1]):0;
 assert(variant>=0&&variant<(int)(sizeof(readers)/sizeof(readers[0])));
 reader=readers[variant];
 int checked=0;
 for(const auto &p:profiles){profile=&p;if(!p.bots.navigationWorlds||Com_GameIsRTCW())continue;
  initialize();useFiles();roundTrip(7,3,3);damaged();shutdown();
  initialize();useFiles();roundTrip(3,4,1);shutdown();
  initialize();useFiles();roundTrip(11,2,5);shutdown();
  std::printf("%s\n",p.key);++checked;
 }
 assert(checked>0);
 return 0;
}
'''


def fixture_source(readers):
    """The route query fixture with the real writer, and each of `readers` in a namespace of its own."""
    source = rtcw_route_query_tests.route_source() + rtcw_route_query_tests.GRAPH
    replacements = (
        ('static int failAllocation=-1;', 'static int failAllocation=-1,allocationCount;'),
        ('if(failAllocation==0)throw Drop{};', '++allocationCount;if(failAllocation==0)throw Drop{};'),
        ('void QDECL AAS_Error(const char*,...){}',
         'static int aasErrors;static std::string lastError;\n'
         'void QDECL AAS_Error(const char *fmt,...){char text[1024];va_list args;va_start(args,fmt);\n'
         ' vsnprintf(text,sizeof(text),fmt,args);va_end(args);lastError=text;++aasErrors;}'),
        ('void AAS_WriteRouteCache(){++legacyCaches;}', 'void AAS_WriteRouteCache(void);'),
    )
    for old, new in replacements:
        assert source.count(old) == 1, old
        source = source.replace(old, new)
    crc = (ROOT / 'code/botlib/l_crc.cpp').read_text()
    end = crc.index('} //end of the function CRC_ProcessString') + len('} //end of the function CRC_ProcessString')
    source += '\n#include "code/botlib/l_crc.h"\n' + crc[crc.index('static constexpr unsigned short CRC_INIT_VALUE'):end]
    route = (ROOT / 'code/botlib/be_aas_route.cpp').read_text()
    source += '\n' + route[route.index('//the route cache header'):route.index('//void AAS_DecompressVis')]
    source += '\nstatic bool AAS_WriteRTCWRouteCache(){assert(false);return false;}\n'
    source += function(route, 'void AAS_WriteRouteCache(void)') + '\n'
    for index, text in enumerate(readers):
        source += f'\nnamespace reader{index} {{\n{text}\n}}\n'
    source += '\nstatic int (*const readers[])()={' + ','.join(
        f'reader{index}::AAS_ReadRouteCache' for index in range(len(readers))) + '};\n'
    return source + CHECKS


def reader_source():
    """The dump's header and the reader, as be_aas_route.cpp has them."""
    route = (ROOT / 'code/botlib/be_aas_route.cpp').read_text()
    text = route[route.index('//the route cache header'):route.index('//void AAS_DecompressVis')]
    return text + '\n'.join(function(route, signature) for signature in READER)


def remove_folder(folder):
    # A failed variant's process can hold its executable for a moment after it exits.
    for _ in range(50):
        try:
            shutil.rmtree(folder)
            return
        except PermissionError:
            time.sleep(0.1)
    shutil.rmtree(folder)


class RouteCacheReadTests(unittest.TestCase):
    def test_engine_dumps_load_and_every_damage_is_refused_by_its_check(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler required')
        production = reader_source()
        readers = [production]
        for description, (old, new) in MUTATIONS.items():
            self.assertEqual(production.count(old), 1, description)
            readers.append(production.replace(old, new))
        (ROOT / '.tmp').mkdir(parents=True, exist_ok=True)
        folder = Path(tempfile.mkdtemp(prefix='route-cache-', dir=ROOT / '.tmp'))
        self.addCleanup(remove_folder, folder)
        cpp, exe = folder / 'probe.cpp', folder / 'probe.exe'
        cpp.write_text(fixture_source(readers), encoding='utf-8')
        target = compiler_target_args(compiler)
        if os.name == 'nt' and not target:
            target = ['-DFNTECH3_Q3_ONLY']
        built = subprocess.run([compiler, *target, '-O2', '-ffast-math', '-std=c++17', '-D_CRT_SECURE_NO_WARNINGS',
                                '-I', str(ROOT), str(cpp), *rtcw_routing_tests.RTCWRoutingTests.extra(),
                                '-o', str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=600)
        self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
        ran = subprocess.run([str(exe), '0'], cwd=ROOT, capture_output=True, text=True, timeout=300)
        self.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)
        # Every profile whose bots route with Quake III's botlib reads these dumps.
        checked = ran.stdout.split()
        for name in ('quake3', 'quakelive', 'stvef-mp'):
            self.assertIn(name, checked)
        for index, description in enumerate(MUTATIONS, 1):
            with self.subTest(mutation=description):
                try:
                    mutated = subprocess.run([str(exe), str(index)], cwd=ROOT, capture_output=True, text=True,
                                             timeout=120)
                except subprocess.TimeoutExpired:
                    continue  # a reader that never returns fails too
                self.assertNotEqual(mutated.returncode, 0, description)


if __name__ == '__main__':
    unittest.main()
