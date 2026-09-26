#!/usr/bin/env python3
"""Compare visibility, fuzzy areas and SP route visibility against RTCW sources.

GPLv3 reference functions compile unchanged only in disposable fixtures. Original
visibility decompression uses 32-bit pointer arithmetic, so those references run
only on x86; iortcw also runs on x64. Route caches and decoded visibility rows are
independent. Previously verified geometry/cost tables are shared. Route comparisons
require endpoints belonging to routing clusters with outgoing links; fuzzy lookup
also covers fallback areas. Original SP performs extra cache work for some
non-routing endpoints, where the engine's existing safety guards stop earlier.
Installed data stays in memory; MP checks on SP inputs use a synthetic RCD12 header.
"""
from __future__ import annotations
import argparse
import datetime
import fnmatch
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
import probe_rtcw_route_queries as routes
from rtcw_visibility_tests import visibility_source, GEOMETRY
from rtcw_server_world_tests import function

VISIBILITY_WORLD = r'''
struct VisibilityWorld:aas_t {
 byte **areavisibility=nullptr,*decompressedvis=nullptr;
 int decompressedvisarea=-1;
 VisibilityWorld &operator=(const aas_t &input){static_cast<aas_t&>(*this)=input;return *this;}
};
static VisibilityWorld world,*aasworld=&world;
'''

OWNERSHIP = r'''
static std::vector<byte*> rows;
static std::vector<byte> decoded;
static void attachVisibility(){
 rows.resize(world.numareas);decoded.assign(world.numareas,0);
 for(int a=0;a<world.numareas;++a)rows[a]=world.rtcwVisibility[a].data;
 world.areavisibility=rows.data();world.decompressedvis=decoded.data();world.decompressedvisarea=-1;
}
static void detachVisibility(){
 rows.clear();decoded.clear();world.areavisibility=nullptr;world.decompressedvis=nullptr;world.decompressedvisarea=-1;
}
'''

COMPARE = r'''
#define aasworld (*aas_currentWorld)
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif
static uint64_t visibilityQueries,decodedBytes,fuzzyQueries,routeQueries,routeSuccess,nonRoutingEndpoints;
static void compareDecoded(){
 assert(aasworld.rtcwVisibilityArea==reference::world.decompressedvisarea);
 if(aasworld.rtcwVisibilityArea>=0){
  assert(!std::memcmp(aasworld.rtcwDecodedVisibility,reference::world.decompressedvis,aasworld.numareas));
  decodedBytes+=aasworld.numareas;}
}
static void compareVisible(int from,int to){
 int ours=AAS_AreaVisible(from,to),theirs=reference::AAS_AreaVisible(from,to);
 assert(ours==theirs);compareDecoded();++visibilityQueries;
}
static void compareFuzzy(vec3_t point){
 int ours=BotFuzzyPointReachabilityArea(point),theirs=reference::BotFuzzyPointReachabilityArea(point);
 if(ours!=theirs){std::fprintf(stderr,"fuzzy %g %g %g: %d != %d\n",point[0],point[1],point[2],ours,theirs);std::abort();}
 ++fuzzyQueries;
}
static void compareRoute(vec3_t from,vec3_t to,int flags,int budget){
#if MODE_SP
 int a=BotFuzzyPointReachabilityArea(from),b=BotFuzzyPointReachabilityArea(to);
 assert(a==reference::BotFuzzyPointReachabilityArea(from)&&b==reference::BotFuzzyPointReachabilityArea(to));
 if(!a||!b||!AAS_AreaReachability(a)||!AAS_AreaReachability(b)
  ||!aasworld.areasettings[a].cluster||!aasworld.areasettings[b].cluster){++nonRoutingEndpoints;return;}
 aasworld.frameroutingupdates=reference::world.frameroutingupdates=budget;
 vec3_t ours{71,72,73},theirs{71,72,73};
 bool found=AAS_RTCWRouteFirstVisiblePosition(from,to,flags,ours);
 bool expected=reference::AAS_GetRouteFirstVisPos(from,to,flags,theirs)!=0;
 if(found!=expected||std::memcmp(ours,theirs,12)||aasworld.frameroutingupdates!=reference::world.frameroutingupdates){
  std::fprintf(stderr,"route %g -> %g flags %x budget %d: %d (%g,%g,%g) != %d (%g,%g,%g)\n",
   from[0],to[0],flags,budget,found,ours[0],ours[1],ours[2],expected,theirs[0],theirs[1],theirs[2]);
  std::fprintf(stderr,"updates %d != %d; areas %d/%d, reachable %d/%d, clusters %d/%d\n",
   aasworld.frameroutingupdates,reference::world.frameroutingupdates,a,b,
   AAS_AreaReachability(a),AAS_AreaReachability(b),aasworld.areasettings[a].cluster,aasworld.areasettings[b].cluster);
  std::abort();}
 compareDecoded();++routeQueries;routeSuccess+=found;
#endif
}
static void queries(unsigned seed,bool installed){
 unsigned state=seed+17;auto next=[&](){state=state*1664525u+1013904223u;return state;};
 const int n=aasworld.numareas;
 for(int a=0;a<n;++a){
  if(installed){compareVisible(a,next()%n);compareVisible(a,next()%n);}
  else for(int b=0;b<n;++b){compareVisible(a,b);compareVisible(a,b);}
 }
 for(int i=0;i<(installed?128:2048);++i){
  const int area=1+next()%(n-1);vec3_t point;VectorCopy(aasworld.areas[area].center,point);
  for(int axis=0;axis<3;++axis)point[axis]+=float(int(next()%1025)-512)*.5f;
  compareFuzzy(point);
 }
 static const int flags[]={-1,0,TFL_WALK|TFL_AIR,TFL_WALK|TFL_AIR|TFL_DONOTENTER};
 for(int i=0;i<(installed?128:1024);++i){int from=1+next()%(n-1),to=1+next()%(n-1);
  vec3_t source,dest;VectorCopy(aasworld.areas[from].center,source);VectorCopy(aasworld.areas[to].center,dest);
  source[1]+=.125f;dest[2]-=.125f;compareRoute(source,dest,flags[i%4],i%7==0?101:0);
 }
}
static void attach(bool caches){
 reference::attach(aasworld,caches);reference::attachVisibility();
 aasworld.rtcwVisibilityArea=-1;std::memset(aasworld.rtcwDecodedVisibility,0,aasworld.numareas);
}
static void detach(){reference::detachVisibility();reference::detach();}
static void synthetic(){
 for(unsigned seed=0;seed<16;++seed){initialize();
  for(int w=0;w<2;++w){AAS_SetCurrentWorld(w);graph(seed?seed+w*73:0);slabs();visibility(seed);
   attach(false);queries(seed,false);
   // Leave the cost geometry intact; these modifications exercise only fuzzy lookup.
   for(int a=1;a<aasworld.numareas;++a)aasworld.areasettings[a].numreachableareas=(a+seed)%4==0?1:0;
   for(float spacing:{1.0f,16.0f,128.0f,256.0f}){
    for(int a=1;a<aasworld.numnodes;++a){aasworld.planes[a-1].dist=spacing*(float(a)-3);
     aasworld.nodes[a].children[1]=(a+seed)%3?-a:0;}
    for(int x=-600;x<=600;x+=7){vec3_t point{float(x),float(x%13),float(x%17)};compareFuzzy(point);}
   }
   detach();
  }shutdown();
 }
}
static std::vector<byte> readInput(){
 byte header[4];assert(fread(header,1,4,stdin)==4);unsigned bytes=fntech3::rtcw::routecache::U32(header);
 assert(bytes>=36&&bytes<=fntech3::rtcw::routecache::MaxFileBytes);std::vector<byte> data(bytes);
 assert(fread(data.data(),1,bytes,stdin)==bytes);return data;
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
 for(int w=0;w<2;++w){AAS_SetCurrentWorld(w);assert(aasworld.initialized);attach(true);queries(w,true);detach();}
 shutdown();
}
int main(int argc,char**){profile=&profiles[MODE_SP?2:3];if(argc>1)installed();else synthetic();
 std::printf("{\"visibilityQueries\":%llu,\"decodedBytes\":%llu,\"fuzzyQueries\":%llu,\"routeQueries\":%llu,\"routeSuccess\":%llu,\"nonRoutingEndpoints\":%llu}\n",
  (unsigned long long)visibilityQueries,(unsigned long long)decodedBytes,(unsigned long long)fuzzyQueries,
  (unsigned long long)routeQueries,(unsigned long long)routeSuccess,(unsigned long long)nonRoutingEndpoints);
}
'''


def fixture(base, variant):
    source, provenance = routes.fixture(base, variant)
    source = source.removesuffix(routes.COMPARE)
    source = source.replace(routes.route_source() + routes.GRAPH, visibility_source() + routes.GRAPH + GEOMETRY, 1)
    source = source.replace('static aas_t world,*aasworld=&world;', VISIBILITY_WORLD, 1)
    source = source.replace('world={};', 'static_cast<aas_t&>(world)={};', 1)
    paths = [base / 'botlib' / name for name in ('be_aas_sample.c', 'be_aas_reach.c', 'be_ai_move.c', 'be_aas_route.c')]
    source += '\nnamespace reference {\n'
    sample = paths[0].read_text()
    stack = re.search(r'typedef struct aas_tracestack_s.*?} aas_tracestack_t;', sample, re.S)
    assert stack
    source += sample.split('*/', 1)[0] + '*/\n' + stack[0] + '\n'
    # ADL can otherwise find the engine's reachability overload through its POD.
    source += '#define AAS_ReachabilityFromNum Reference_AAS_ReachabilityFromNum\n'
    for path, names in zip(paths, [
        ('AAS_PointAreaNum', 'AAS_TraceAreas'), ('AAS_AreaReachability',),
        ('BotFuzzyPointReachabilityArea',),
        ('AAS_DecompressVis', 'AAS_AreaVisible', 'AAS_ReachabilityFromNum') + (('AAS_GetRouteFirstVisPos',) if variant.endswith('SP') else ()),
    ]):
        body = path.read_text()
        source += body.split('*/', 1)[0] + '*/\n'
        for name in names:
            signature = re.search(r'^(?:int|void|qboolean)\s+' + name + r'\([^\n]*\)\s*\{', body, re.M)[0]
            source += function(body, signature) + '\n'
        provenance.append(dict(path=str(path), sha256=routes.sha(path)))
    source += '#undef AAS_ReachabilityFromNum\n' + OWNERSHIP + '\n}\n'
    return source + COMPARE, list({row['path']: row for row in provenance}.values())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('sp-source', 'mp-source', 'iortcw-source', 'report'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--installation', type=Path)
    parser.add_argument('--asset', default='*', help='Optional installed member glob for investigation')
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
    x64 = triple.startswith('x86_64')
    flags = ['-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-g'] if args.sanitize else []
    if os.name == 'nt' and x64:
        flags += ['-DFNTECH3_Q3_ONLY']
    sources = ['code/botlib/be_aas_route.cpp', 'code/botlib/be_ai_move.cpp', 'code/botlib/rtcw_navigation.h',
               'code/botlib/rtcw_routecache_loader.hpp', 'code/compat/rtcw_routecache_format.hpp',
               'code/server/server_rtcw_aas_query.hpp', 'tests/rtcw_visibility_tests.py',
               'scripts/probe_rtcw_visibility.py', 'scripts/probe_rtcw_route_queries.py']
    report = dict(utc=datetime.datetime.now(datetime.timezone.utc).isoformat(), target=triple,
                  sanitized=args.sanitize, scope=__doc__, assetFilter=args.asset,
                  implementationSha256={p: routes.sha(ROOT / p) for p in sources}, results=[])
    env = os.environ.copy()
    if args.sanitize:
        env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1', UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    references = [('originalSP', args.sp_source / 'src'), ('originalMP', args.mp_source / 'src'),
                  ('iortcwSP', args.iortcw_source / 'SP/code'), ('iortcwMP', args.iortcw_source / 'MP/code')]
    for variant, base in references:
        if x64 and variant.startswith('original'):
            report['results'].append(dict(reference=variant, skipped='Original decompressor requires 32-bit pointers'))
            continue
        source, provenance = fixture(base, variant)
        row = dict(reference=variant, sources=provenance, assets=[]); report['results'].append(row)
        with tempfile.TemporaryDirectory(prefix='visibility-reference-', dir=output.parent) as folder:
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
                                    if not entry.filename.lower().endswith('.rcd') or not fnmatch.fnmatchcase(entry.filename, args.asset):
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
            row['passed'] = not built.returncode and row.get('exitCode') == 0 and all(a['exitCode'] == 0 for a in row['assets'])
        output.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
        print(variant, 'PASS' if row['passed'] else 'FAIL', row.get('synthetic'), flush=True)
    return 0 if all(r.get('passed', bool(r.get('skipped'))) for r in report['results']) else 1


if __name__ == '__main__':
    raise SystemExit(main())
