#!/usr/bin/env python3
"""Compare RTCW tactical policies against disposable original/iortcw fixtures.

GPLv3 reference bodies stay in temporary comparison programs. The production
implementation is independently authored. Each side owns its queues, caches,
visibility decoder and query scratch. Geometry is shared read-only. Synthetic
game callbacks produce deterministic answers and record their complete inputs;
these comparisons do not establish live campaign behavior. --normalization
original isolates the tactical algorithms using original-game normalization;
iortcw otherwise uses its different reciprocal-square-root implementation.
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
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tests'))
import probe_rtcw_visibility as visibility
from rtcw_tactical_tests import tactical_source
from rtcw_server_world_tests import function

CALLBACKS = r'''
static int side,salt;
static std::vector<std::array<uint32_t,12>> calls[2];
static int see(const float *from,int source,const float *to,int destination,int update){
 std::array<uint32_t,12> call{};call[0]=11;call[1]=source;call[2]=destination;call[3]=update;
 std::memcpy(call.data()+4,from,12);std::memcpy(call.data()+7,to,12);calls[side].push_back(call);
 return (int(to[0])+int(to[1])+salt)%7<3;
}
static int attack(int source,int enemy,const float *at,int duck,int world){
 std::array<uint32_t,12> call{};call[0]=12;call[1]=source;call[2]=enemy;call[3]=duck;call[4]=world;
 std::memcpy(call.data()+5,at,12);calls[side].push_back(call);return (int(at[0])+salt)%5<3;
}
'''

REFERENCE = r'''
namespace reference {
static aas_t *defaultaasworld=&world;
struct CastImports {
 qboolean (*AICast_VisibleFromPos)(const float*,int,const float*,int,int);
 qboolean (*AICast_CheckAttackAtPos)(int,int,const float*,int,int);
 decltype(::botimport.Print) Print;
};
static CastImports botimport{
 [](const float *a,int b,const float *c,int d,int e){return ::see(a,b,c,d,e)?qtrue:qfalse;},
 [](int a,int b,const float *c,int d,int e){return ::attack(a,b,c,d,e)?qtrue:qfalse;},::print};
static float AAS_Time(){return world.time;}
static int AAS_AreaSwim(int area){return world.areasettings[area].areaflags&AREA_LIQUID;}
static int AAS_AreaGrounded(int area){return world.areasettings[area].areaflags&AREA_GROUNDED;}
'''

COMPARE = r'''
#define aasworld (*aas_currentWorld)
static uint64_t hideQueries,attackQueries,callbackComparisons,sequence,coldQueries,coldDifferences,coldRouteQueries,coldRouteDifferences;
static int firstColdRoute[7];
static void attachTactical(){
 reference::attach(aasworld,false);reference::attachVisibility();
 reference::world.hidetraveltimes=nullptr;reference::world.visCache=nullptr;
 reference::world.areawaypoints=aasworld.rtcwWaypoints;
}
static void detachTactical(){
 if(reference::world.hidetraveltimes)FreeMemory(reference::world.hidetraveltimes);
 if(reference::world.visCache)FreeMemory(reference::world.visCache);
 reference::world.visCache=nullptr;reference::detachVisibility();reference::detach();
}
static void bind(int entity,int area){
 auto &ent=AAS_EntityWorld().entities[entity];ent.i.valid=qtrue;ent.i.solid=SOLID_BBOX;
 // iortcw MP disables occupancy penalties that original MP still applies.
 // Its algorithm corpus uses an intangible range anchor to isolate that
 // documented policy difference; original MP also checks occupied candidates.
 if(IO_MP&&entity==2)ent.i.solid=SOLID_NOT;
 VectorCopy(aasworld.areas[area].center,ent.i.origin);
 auto *link=AAS_AllocAASLink();assert(link);*link={};link->entnum=entity;link->areanum=area;
 link->next_ent=aasworld.arealinkedentities[area];aasworld.arealinkedentities[area]=link;
 ent.areas=link;ent.areaWorld=aas_currentWorld;
}
static void entities(int source,int range,int enemy){
 AAS_FreeAASLinkedEntities();AAS_FreeAASLinkHeap();AAS_InitAASLinkHeap();AAS_InitAASLinkedEntities();
 bind(1,source);bind(2,range);bind(3,enemy);
 reference::world.linkheap=aasworld.linkheap;reference::world.linkheapsize=aasworld.linkheapsize;
 reference::world.arealinkedentities=aasworld.arealinkedentities;
 reference::world.entities=AAS_EntityWorld().entities;
 reference::world.maxentities=AAS_EntityWorld().maxentities;
 reference::world.maxclients=AAS_EntityWorld().maxclients;
}
static void prime(int source,int flags){
 for(int to=1;to<aasworld.numareas;++to){
  aasworld.frameroutingupdates=reference::world.frameroutingupdates=0;
  assert(AAS_AreaTravelTimeToGoalArea(source,aasworld.areas[source].center,to,flags)
   ==reference::AAS_AreaTravelTimeToGoalArea(source,aasworld.areas[source].center,to,flags));
 }
}
static void inspectColdRoutes(int source,int range,int enemy,int flags){
 for(int to=1;to<aasworld.numareas;++to){
  aasworld.frameroutingupdates=reference::world.frameroutingupdates=0;
  int ours=AAS_AreaTravelTimeToGoalArea(source,aasworld.areas[source].center,to,flags);
  int theirs=reference::AAS_AreaTravelTimeToGoalArea(source,aasworld.areas[source].center,to,flags);
  ++coldRouteQueries;
  if(ours!=theirs){if(!coldRouteDifferences){int values[]={source,range,enemy,to,flags,ours,theirs};
    std::memcpy(firstColdRoute,values,sizeof(values));}++coldRouteDifferences;}
 }
 // Restore independent empty caches after diagnostics. The next policy
 // comparison must not inherit damaged reference cache/update state.
 detachTactical();AAS_FreeAllClusterAreaCache();AAS_FreeAllPortalCache();
 AAS_InitClusterAreaCache();AAS_InitPortalCache();attachTactical();
}
static void compare(bool hide,int source,int range,int enemy,int flags,bool warm){
 entities(source,range,enemy);if(warm)prime(source,flags);
 int queued=0;for(int a=0;a<aasworld.numareas;++a)queued+=reference::world.areaupdate[a].inlist!=0;
 const float now=float(++sequence);aasworld.time=reference::world.time=now;
 aasworld.frameroutingupdates=reference::world.frameroutingupdates=0;
 calls[0].clear();calls[1].clear();salt=int(sequence%11);
 int ours,theirs;vec3_t ourPoint{71,72,73},theirPoint{71,72,73};
 side=0;
 if(hide)ours=fntech3::rtcw::tactical::Hide(1,aasworld.areas[source].center,source,
  3,aasworld.areas[enemy].center,enemy,flags);
 else ours=AAS_RTCWAttackPosition(1,2,3,256,flags,ourPoint);
 side=1;
 if(hide)theirs=reference::AAS_NearestHideArea(1,aasworld.areas[source].center,source,
  3,aasworld.areas[enemy].center,enemy,flags);
 else theirs=reference::AAS_FindAttackSpotWithinRange(1,2,3,256,flags,theirPoint);
 const bool same=ours==theirs&&calls[0]==calls[1]&&!std::memcmp(ourPoint,theirPoint,12);
 if(!warm&&!hide){++coldQueries;if(!same)++coldDifferences;return;}
 if(!same){
  std::fprintf(stderr,"%s query %llu %d/%d/%d flags=%x warm=%d: %d != %d callbacks=%zu/%zu\n",
   hide?"hide":"attack",(unsigned long long)sequence,source,range,enemy,flags,warm,ours,theirs,calls[0].size(),calls[1].size());
  std::fprintf(stderr,"reference queued before call: %d\n",queued);
  for(const auto &call:calls[0])std::fprintf(stderr,"ours callback %u destination-x-bits=%08x\n",call[0],call[call[0]==11?7:5]);
  for(const auto &call:calls[1])std::fprintf(stderr,"reference callback %u destination-x-bits=%08x\n",call[0],call[call[0]==11?7:5]);
  std::abort();
 }
 callbackComparisons+=calls[0].size();if(hide)++hideQueries;else ++attackQueries;
}
int main(){
 profile=&profiles[MODE_SP?2:3];
 for(unsigned seed=0;seed<16;++seed){initialize();
  for(int w=0;w<2;++w){AAS_SetCurrentWorld(w);graph(seed);slabs();visibility(seed);
   aasworld.rtcwWaypoints=botlib::ClearedAlloc<vec3_t>(aasworld.numareas);
   for(int a=0;a<aasworld.numareas;++a)VectorCopy(aasworld.areas[a].center,aasworld.rtcwWaypoints[a]);
   AAS_SetRTCWCastCallbacks({[](){return true;},see,attack});attachTactical();
   unsigned random=seed+3;auto next=[&](){random=random*1664525u+1013904223u;return random;};
   for(int i=0;i<128;++i){int source=1+next()%(aasworld.numareas-1);
    int range=1+next()%(aasworld.numareas-1),enemy=1+next()%(aasworld.numareas-1);
    static const int masks[]={-1,0,TFL_WALK|TFL_AIR,20451262,20713406};int flags=masks[i%5];
    compare(true,source,range,enemy,flags,false);
#if COLD_QUERIES
    compare(false,source,range,enemy,flags,false);
    inspectColdRoutes(source,range,enemy,flags);
#endif
    compare(false,source,range,enemy,flags,true);
   }
   detachTactical();
  }
  shutdown();
 }
 std::printf("{\"hideQueries\":%llu,\"attackQueries\":%llu,\"callbackComparisons\":%llu,\"coldQueries\":%llu,\"coldDifferences\":%llu,\"coldRouteQueries\":%llu,\"coldRouteDifferences\":%llu,\"firstColdRouteDifference\":[%d,%d,%d,%d,%d,%d,%d]}\n",
  (unsigned long long)hideQueries,(unsigned long long)attackQueries,(unsigned long long)callbackComparisons,
  (unsigned long long)coldQueries,(unsigned long long)coldDifferences,(unsigned long long)coldRouteQueries,(unsigned long long)coldRouteDifferences,
  firstColdRoute[0],firstColdRoute[1],firstColdRoute[2],firstColdRoute[3],firstColdRoute[4],firstColdRoute[5],firstColdRoute[6]);
}
'''


def fixture(base, variant, normalization_path=None):
    source, provenance = visibility.fixture(base, variant)
    source = source.removesuffix(visibility.COMPARE)
    source = source.replace(visibility.visibility_source(), tactical_source(), 1)
    source = source.replace('struct VisibilityWorld:aas_t {',
                            'struct VisibilityWorld:aas_t {\n byte *visCache=nullptr;vec3_t *areawaypoints=nullptr;', 1)
    source += CALLBACKS + REFERENCE
    math_path = base / ('game/q_math.c' if variant.startswith('original') else 'qcommon/q_math.c')
    reach_path, entity_path, route_path = (base / 'botlib' / name for name in ('be_aas_reach.c', 'be_aas_entity.c', 'be_aas_route.c'))
    for path, names in ((normalization_path or math_path, ('VectorNormalize',)),
                        (math_path, ('ProjectPointOntoVector',)),
                        (reach_path, ('AAS_BestReachableLinkArea',)),
                        (entity_path, ('AAS_EntityOrigin', 'AAS_BestReachableEntityArea', 'AAS_IsEntityInArea')),
                        (route_path, ('AAS_NearestHideArea', 'AAS_FindAttackSpotWithinRange'))):
        body = path.read_text()
        source += body.split('*/', 1)[0] + '*/\n'
        for name in names:
            signature = re.search(r'^(?:int|void|qboolean|vec_t)\s+' + name + r'\([^\n]*\)\s*\{', body, re.M)[0]
            source += function(body, signature) + '\n'
        provenance.append(dict(path=str(path), sha256=visibility.routes.sha(path)))
    return source + '\n}\n' + COMPARE, list({row['path']: row for row in provenance}.values())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('sp-source', 'mp-source', 'iortcw-source', 'report'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--target')
    parser.add_argument('--sanitize', action='store_true')
    parser.add_argument('--normalization', choices=('source', 'original'), default='source')
    parser.add_argument('--cold-diagnostics', action='store_true', help='Count cold-cache attack differences separately from the policy gate')
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
    sources = ['code/botlib/rtcw_tactical.hpp', 'code/botlib/be_aas_def.h', 'code/botlib/be_aas_route.cpp',
               'code/botlib/rtcw_navigation.h', 'code/compat/rtcw_routing.hpp', 'code/server/server_rtcw_aas_query.hpp',
               'tests/rtcw_tactical_tests.py', 'scripts/probe_rtcw_tactical.py']
    report = dict(utc=datetime.datetime.now(datetime.timezone.utc).isoformat(), target=triple,
                  sanitized=args.sanitize, normalization=args.normalization, coldDiagnostics=args.cold_diagnostics, scope=__doc__,
                  implementationSha256={p: visibility.routes.sha(ROOT / p) for p in sources}, results=[])
    env = os.environ.copy()
    if args.sanitize:
        env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1', UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    for variant, base in [('originalSP', args.sp_source / 'src'), ('originalMP', args.mp_source / 'src'),
                          ('iortcwSP', args.iortcw_source / 'SP/code'), ('iortcwMP', args.iortcw_source / 'MP/code')]:
        if triple.startswith('x86_64') and variant.startswith('original'):
            report['results'].append(dict(reference=variant, skipped='Original decompressor requires 32-bit pointers'))
            continue
        normalization = (args.sp_source if variant.endswith('SP') else args.mp_source) / 'src/game/q_math.c'
        source, provenance = fixture(base, variant, normalization if args.normalization == 'original' else None)
        row = dict(reference=variant, sources=provenance,
                   rangeAnchorSolid=variant!='iortcwMP'); report['results'].append(row)
        with tempfile.TemporaryDirectory(prefix='tactical-reference-', dir=output.parent) as folder:
            cpp = Path(folder) / 'probe.cpp'; exe = Path(folder) / 'probe.exe'; cpp.write_text(source, encoding='utf-8')
            command = [compiler, *target, *flags, '-std=c++17', '-O2', '-ffast-math', '-D_CRT_SECURE_NO_WARNINGS',
                       '-I', str(ROOT), f'-DMODE_SP={int(variant.endswith("SP"))}', f'-DIO_MP={int(variant=="iortcwMP")}',
                       f'-DCOLD_QUERIES={int(args.cold_diagnostics)}', str(cpp),
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
            row['passed'] = not built.returncode and row.get('exitCode') == 0
        output.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
        print(variant, 'PASS' if row['passed'] else 'FAIL', row.get('synthetic'), flush=True)
    return 0 if all(row.get('passed', bool(row.get('skipped'))) for row in report['results']) else 1


if __name__ == '__main__':
    raise SystemExit(main())
