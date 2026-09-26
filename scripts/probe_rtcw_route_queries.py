#!/usr/bin/env python3
"""Compare RTCW route updates and queries with unchanged SP/MP source functions.

GPLv3 reference functions and notices enter disposable fixtures only. The
already verified native cost tables are shared; caches and update queues are
independent. Optional installed AAS/RCD inputs stay in memory. MP checks over
SP navigation use a synthetic RCD12 header, not retail MP or gameplay evidence.
"""
from __future__ import annotations
import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tests'))
from rtcw_route_query_tests import route_source, GRAPH
from rtcw_server_world_tests import function

REFERENCE = r'''
static aas_t world,*aasworld=&world;
static int routingcachesize,max_routingcachesize,bot_developer;
static void *AAS_RoutingGetMemory(int size){return GetClearedMemory(size);}
static void AAS_RoutingFreeMemory(void *p){FreeMemory(p);}
// Cost arithmetic and the matrices have a separate original/iortcw comparison
// corpus. This probe isolates updates, cache ownership and route decisions.
static unsigned short AAS_AreaTravelTime(int area,vec3_t a,vec3_t b){
 auto *saved=aas_currentWorld;aas_currentWorld=aasworld;
 auto result=::AAS_AreaTravelTime(area,a,b);aas_currentWorld=saved;return result;
}
'''

OWNERSHIP = r'''
static void attach(const aas_t &engine,bool caches){
 world=engine;world.oldestcache=world.newestcache=nullptr;
 AAS_InitTravelFlagFromType();
 for(int i=0;i<MAX_TRAVELTYPES;++i)assert(world.travelflagfortype[i]==engine.travelflagfortype[i]);
 world.clusterareacache=nullptr;world.portalcache=nullptr;
 world.areaupdate=botlib::ClearedAlloc<aas_routingupdate_t>(world.numareas);
 world.portalupdate=botlib::ClearedAlloc<aas_routingupdate_t>(world.numportals+1);
 routingcachesize=0;max_routingcachesize=engine.max_routingcachesize;
 auto *saved=aas_currentWorld;aas_currentWorld=&world;
 ::AAS_InitClusterAreaCache();::AAS_InitPortalCache();aas_currentWorld=saved;
 auto copy=[&](aas_routingcache_t *input,int entries){
  aas_routingcache_t *head=nullptr,*previous=nullptr;
  for(;input;input=input->next){auto *cache=AAS_AllocRoutingCache(entries);auto *reaches=cache->reachabilities;
   std::memcpy(cache,input,cache->size);cache->reachabilities=reaches;
   cache->next=nullptr;cache->prev=previous;cache->time_next=cache->time_prev=nullptr;
   if(previous)previous->next=cache;else head=cache;previous=cache;}
  return head;
 };
 if(caches){
  for(int c=0;c<world.numclusters;++c)for(int a=0;a<world.clusters[c].numareas;++a)
   world.clusterareacache[c][a]=copy(engine.clusterareacache[c][a],world.clusters[c].numreachabilityareas);
  for(int a=0;a<world.numareas;++a)world.portalcache[a]=copy(engine.portalcache[a],world.numportals);
 }
}
static void detach(){
 auto clear=[](aas_routingcache_t *cache){while(cache){auto *next=cache->next;AAS_FreeRoutingCache(cache);cache=next;}};
 for(int c=0;c<world.numclusters;++c)for(int a=0;a<world.clusters[c].numareas;++a)clear(world.clusterareacache[c][a]);
 for(int a=0;a<world.numareas;++a)clear(world.portalcache[a]);
 FreeMemory(world.clusterareacache);FreeMemory(world.portalcache);
 FreeMemory(world.areaupdate);FreeMemory(world.portalupdate);assert(!routingcachesize);world={};
}
'''

COMPARE = r'''
#define aasworld (*aas_currentWorld)
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif
static uint64_t comparisons,entries,cacheComparisons,sequence;
static void compareCaches(){
 assert(aasworld.routingcachesize==reference::routingcachesize);cacheLinks();
 auto compare=[&](aas_routingcache_t *ours,aas_routingcache_t *theirs,int count){
  for(;ours&&theirs;ours=ours->next,theirs=theirs->next){
   assert(ours->size==theirs->size&&ours->time==theirs->time&&ours->cluster==theirs->cluster);
   assert(ours->areanum==theirs->areanum&&ours->travelflags==theirs->travelflags);
   assert(ours->starttraveltime==theirs->starttraveltime&&!std::memcmp(ours->origin,theirs->origin,12));
   for(int i=0;i<count;++i){assert(ours->traveltimes[i]==theirs->traveltimes[i]);
    assert(ours->reachabilities[i]==theirs->reachabilities[i]);++entries;}
   ++cacheComparisons;
  }assert(!ours&&!theirs);
 };
 for(int c=0;c<aasworld.numclusters;++c)for(int a=0;a<aasworld.clusters[c].numareas;++a)
  compare(aasworld.clusterareacache[c][a],reference::world.clusterareacache[c][a],aasworld.clusters[c].numreachabilityareas);
 for(int a=0;a<aasworld.numareas;++a)compare(aasworld.portalcache[a],reference::world.portalcache[a],aasworld.numportals);
}
static void compare(int a,vec3_t origin,int goal,int flags,int budget){
 ++sequence;aasworld.frameroutingupdates=reference::world.frameroutingupdates=budget;
 aasworld.time=reference::world.time=float(sequence/32)*.125f;
 int ourTime=73,theirTime=73,ourReach=-99,theirReach=-99;
 int ours=AAS_AreaRouteToGoalArea(a,origin,goal,flags,&ourTime,&ourReach);
 int theirs=reference::AAS_AreaRouteToGoalArea(a,origin,goal,flags,&theirTime,&theirReach);
 if(ours!=theirs||ourTime!=theirTime||ourReach!=theirReach||aasworld.frameroutingupdates!=reference::world.frameroutingupdates){
  std::fprintf(stderr,"query %llu a %d goal %d flags %x origin %d budget %d: ours %d/%d/%d/%d reference %d/%d/%d/%d\n",
   (unsigned long long)sequence,a,goal,flags,origin!=nullptr,budget,ours,ourTime,ourReach,aasworld.frameroutingupdates,
   theirs,theirTime,theirReach,reference::world.frameroutingupdates);std::abort();
 }
 ++comparisons;
}
static void queries(int count,bool retail){
 unsigned state=17;auto nextValue=[&](){state=state*1664525u+1013904223u;return state;};
 std::vector<int> areas;
 for(int a=1;a<aasworld.numareas;++a){const auto &s=aasworld.areasettings[a];
  if(s.cluster&&s.numreachableareas>0)areas.push_back(a);}
 assert(!areas.empty());
 static const int flags[]={TFL_WALK|TFL_AIR,-1,0,20451262,TFL_WALK|TFL_AIR|TFL_DONOTENTER,
  TFL_WALK|TFL_AIR|TFL_SWIM|TFL_WATER,TFL_WALK|TFL_AIR|0x02000000,INT_MIN};
 for(int i=0;i<count;++i){int a=areas[nextValue()%areas.size()],goal=areas[nextValue()%areas.size()];
  vec3_t origin;VectorCopy(aasworld.areas[a].center,origin);origin[0]+=.5f;
  if(i%53==0)a=goal=0;else if(i%59==0)a=INT_MAX;else if(i%61==0)goal=-1;
  int budget=i%7==0?101:i%7==1?100:0;
  compare(a,(i&1)?origin:nullptr,goal,flags[(i/2)%8],budget);
  if(!retail||i%32==0)compareCaches();
  if(!retail&&i%37==0){int ours=AAS_FreeOldestCache(),theirs=reference::AAS_FreeOldestCache();assert(ours==theirs);compareCaches();}
  if(!retail&&i%131==0){aasworld.max_routingcachesize=reference::max_routingcachesize=(i%262)?0:1024;}
 }
 compareCaches();
}
static void synthetic(){
 for(unsigned seed=0;seed<16;++seed){initialize();
  for(int w=0;w<2;++w){AAS_SetCurrentWorld(w);graph(seed?seed+w*73:0);
   if(seed%4==1){aasworld.areasettings[10].areaflags|=AREA_DISABLED;
    aasworld.areasettings[12].areaflags|=AREA_DISABLED;}
   if(seed%4==2){aasworld.clusters[1].numreachabilityareas=2;}
   if(seed%4==3){aasworld.areasettings[1].contents|=1024;aasworld.areasettings[4].contents|=256;
    AAS_InitAreaContentsTravelFlags();}
   reference::attach(aasworld,false);queries(2048,false);reference::detach();
  }shutdown();
 }
 initialize();graph(0,131,1,true);reference::attach(aasworld,false);queries(512,false);reference::detach();shutdown();
 initialize();graph(0,2,1);aasworld.reachability[0].traveltime=65530;
 reference::attach(aasworld,false);queries(128,false);reference::detach();shutdown();
}
static std::vector<byte> readInput(){
 byte header[4];assert(fread(header,1,4,stdin)==4);unsigned size=fntech3::rtcw::routecache::U32(header);
 assert(size>=36&&size<=fntech3::rtcw::routecache::MaxFileBytes);std::vector<byte> bytes(size);
 assert(fread(bytes.data(),1,size,stdin)==size);return bytes;
}
static void installed(){
#ifdef _WIN32
 _setmode(_fileno(stdin),_O_BINARY);
#endif
 const auto aas=readInput(),rcd=readInput();byte header[124];assert(aas.size()>=sizeof(header));
 std::memcpy(header,aas.data(),124);for(unsigned i=8;i<124;++i)header[i]^=byte((i-8)*119);
 initialize();botimport.Print=checkedPrint;
 LibVarSet("sv_mapChecksum",std::to_string(fntech3::rtcw::routecache::U32(header+8)).c_str());
 files["maps/probe.aas"]=aas;files["maps/probe.rcd"]=rcd;if(!Com_GameIsRTCWSP())files["maps/probe.rcd"][4]=12;
 for(int w=0;w<2;++w){AAS_SetCurrentWorld(w);assert(AAS_LoadAASFile("maps/probe.aas")==0);
  Q_strncpyz(aasworld.mapname,"probe",sizeof(aasworld.mapname));}
 assert(AAS_StartFrame(1.25f)==0);
 for(int w=0;w<2;++w){AAS_SetCurrentWorld(w);assert(aasworld.initialized);
  reference::attach(aasworld,true);compareCaches();queries(128,true);reference::detach();}
 shutdown();
}
int main(int argc,char**){profile=&profiles[MODE_SP?2:3];
 if(argc>1)installed();else synthetic();
 std::printf("{\"comparisons\":%llu,\"cacheComparisons\":%llu,\"entries\":%llu}\n",
  (unsigned long long)comparisons,(unsigned long long)cacheComparisons,(unsigned long long)entries);
}
'''


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def fixture(base, variant):
    route_path = base / 'botlib/be_aas_route.c'
    reach_path = base / 'botlib/be_aas_reach.c'
    flags_path = base / ('game/be_aas.h' if variant == 'originalMP' else 'botlib/be_aas.h')
    paths = [route_path, reach_path, flags_path, base / 'botlib/aasfile.h']
    texts = {p: p.read_text() for p in paths}
    source = texts[route_path].split('*/', 1)[0] + '*/\n#include <cstdio>\n' + route_source() + GRAPH
    source += '\n#undef aasworld\n'
    names = set()
    for name, value in re.findall(r'^#define\s+((?:TFL_\w+|TRAVEL_\w+|AREACONTENTS_\w+|MAX_FRAMEROUTINGUPDATES))\s+([^\r\n]+)',
                                  '\n'.join(texts.values()), re.M):
        if name == 'TFL_DEFAULT' or name in names:
            continue
        names.add(name)
        source += f'#pragma push_macro("{name}")\n#undef {name}\n#define {name} {value}\n'
    # Distinct linker names avoid C++ ADL finding engine functions for native
    # cache pointers. The extracted reference function text remains unchanged.
    renamed = ('AAS_FreeRoutingCache', 'AAS_UpdateAreaRoutingCache', 'AAS_UpdatePortalRoutingCache')
    for name in renamed:
        source += f'#define {name} Reference_{name}\n'
    source += '\nnamespace reference {\n' + REFERENCE
    for name in ('AAS_AreaDoNotEnter', 'AAS_AreaDoNotEnterLarge'):
        source += '\n' + function(texts[reach_path], f'int {name}(')
    for name in ('AAS_ClusterAreaNum', 'AAS_InitTravelFlagFromType', 'AAS_RoutingTime', 'AAS_FreeRoutingCache', 'AAS_FreeOldestCache',
                 'AAS_AllocRoutingCache', 'AAS_AreaContentsTravelFlag', 'AAS_UpdateAreaRoutingCache',
                 'AAS_GetAreaRoutingCache', 'AAS_UpdatePortalRoutingCache', 'AAS_GetPortalRoutingCache',
                 'AAS_AreaRouteToGoalArea', 'AAS_AreaTravelTimeToGoalArea'):
        signature = re.search(r'^(?:__inline |static ID_INLINE )?(?:int|float|void|aas_routingcache_t\s*\*)\s*'
                              + name + r'\([^\n]*', texts[route_path], re.M)[0]
        source += '\n' + function(texts[route_path], signature)
    source += OWNERSHIP + '\n}\n'
    for name in renamed:
        source += f'#undef {name}\n'
    for name in names:
        source += f'#pragma pop_macro("{name}")\n'
    return source + COMPARE, [dict(path=str(p), sha256=sha(p)) for p in paths]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('sp-source', 'mp-source', 'iortcw-source', 'report'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--installation', type=Path)
    parser.add_argument('--target')
    parser.add_argument('--sanitize', action='store_true')
    parser.add_argument('--compiler', default='clang++')
    args = parser.parse_args()
    compiler = shutil.which(args.compiler)
    if not compiler:
        parser.error('C++ compiler unavailable')
    output = args.report.resolve(); output.relative_to(ROOT / '.tmp')
    output.parent.mkdir(parents=True, exist_ok=True)
    target = [f'--target={args.target}'] if args.target else ([] if os.name == 'nt' else ['-m32'])
    triple = subprocess.check_output([compiler, *target, '-dumpmachine'], cwd=ROOT, text=True).strip()
    flags = ['-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-g'] if args.sanitize else []
    if os.name == 'nt' and triple.startswith('x86_64'):
        flags += ['-DFNTECH3_Q3_ONLY']
    sources = ['code/botlib/be_aas_route.cpp', 'code/botlib/rtcw_routing_init.hpp',
               'code/compat/rtcw_routing.hpp', 'code/server/server_rtcw_aas_query.hpp',
               'tests/rtcw_route_query_tests.py', 'scripts/probe_rtcw_route_queries.py']
    report = dict(utc=datetime.datetime.now(datetime.timezone.utc).isoformat(), target=triple,
                  sanitized=args.sanitize, scope=__doc__,
                  implementationSha256={p: sha(ROOT / p) for p in sources}, results=[])
    references = [('originalSP', args.sp_source / 'src'), ('originalMP', args.mp_source / 'src'),
                  ('iortcwSP', args.iortcw_source / 'SP/code'), ('iortcwMP', args.iortcw_source / 'MP/code')]
    env = os.environ.copy()
    if args.sanitize:
        env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1', UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    for variant, base in references:
        source, provenance = fixture(base, variant)
        row = dict(reference=variant, sources=provenance, assets=[]); report['results'].append(row)
        with tempfile.TemporaryDirectory(prefix='route-query-reference-', dir=output.parent) as folder:
            cpp = Path(folder) / 'probe.cpp'; exe = Path(folder) / 'probe.exe'; cpp.write_text(source, encoding='utf-8')
            command = [compiler, *target, *flags, '-std=c++17', '-O2', '-ffast-math', '-D_CRT_SECURE_NO_WARNINGS',
                       '-I', str(ROOT), f'-DMODE_SP={int(variant.endswith("SP"))}', str(cpp),
                       str(ROOT / 'code/server/server_game_data.cpp'), '-x', 'c++',
                       str(ROOT / 'code/compat/game_abi_adapter.c'), '-o', str(exe)]
            built = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=90)
            row.update(compileCommand=command, buildExitCode=built.returncode)
            if built.returncode:
                row['buildOutput'] = built.stdout + built.stderr
            else:
                run = subprocess.run([str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=120, env=env)
                row.update(exitCode=run.returncode, stderr=run.stderr)
                if run.returncode:
                    row['stdout'] = run.stdout
                else:
                    row['synthetic'] = json.loads(run.stdout)
                    if args.installation:
                        for archive in sorted((args.installation / 'Main').glob('*.pk3')):
                            with zipfile.ZipFile(archive) as pak:
                                for entry in pak.infolist():
                                    if not entry.filename.lower().endswith('.rcd'):
                                        continue
                                    rcd = pak.read(entry); aas = pak.read(entry.filename[:-4] + '.aas')
                                    asset = dict(archive=str(archive), member=entry.filename,
                                                 aasSha256=hashlib.sha256(aas).hexdigest(), rcdSha256=hashlib.sha256(rcd).hexdigest())
                                    row['assets'].append(asset)
                                    data = struct.pack('<I', len(aas)) + aas + struct.pack('<I', len(rcd)) + rcd
                                    run = subprocess.run([str(exe), '--asset'], cwd=ROOT, input=data,
                                                         capture_output=True, timeout=120, env=env)
                                    asset.update(exitCode=run.returncode, stderr=run.stderr.decode(errors='replace'))
                                    if not run.returncode:
                                        asset.update(json.loads(run.stdout))
                                    output.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
                                    print(variant, entry.filename, 'PASS' if not run.returncode else 'FAIL', flush=True)
            row['passed'] = not built.returncode and row.get('exitCode') == 0 and all(a['exitCode'] == 0 for a in row['assets'])
        output.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
        print(variant, 'PASS' if row['passed'] else 'FAIL', row.get('synthetic'), flush=True)
    return 0 if all(r['passed'] for r in report['results']) else 1


if __name__ == '__main__':
    raise SystemExit(main())
