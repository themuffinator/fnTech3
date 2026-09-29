#!/usr/bin/env python3
"""Measure how Elite Force Holomatch's routes change when an area cache starts from the goal's own row.

Quake III 1.32's routing starts every area cache in its goal area with no
travel time inside it (AAS_UpdateAreaRoutingCache). Elite Force Holomatch's
engine starts from the goal area's first row of travel times instead,
aasworld.areatraveltimes[goal][0]: the time within the goal from each link's
end to the start of the goal's first reachability (stvoyHM.exe 0x004992d0,
docs/fntech3/2026-09-29-stvef-route-seed.md), as RTCW's does.

For every navigation file the Elite Force installation serves (the last pak
of the game folder holding a name wins), this compiles the engine's own
loader and routing from source (the fixtures of tests/rtcw_route_query_tests.py),
loads the file under the stvef-mp profile and routes every area to each of up
to GOALS goal areas, spread evenly over the areas with reachabilities, with
the default travel flags twice: from zeros, as Quake III 1.32 does, and from
the goal's row, as Holomatch's engine does. It counts the routes whose travel
time or first link differs, and the routes either start finds and the other
does not.

Run in the Visual Studio x86 developer shell, as the fixtures are. Paks are
only read; each file is copied into a temporary folder.

  --installation PATH   the Elite Force installation folder
"""
from pathlib import Path
import argparse
import hashlib
import json
import shutil
import struct
import subprocess
import sys
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tests'))
import rtcw_route_query_tests as routes  # noqa: E402
import rtcw_routing_tests as routing  # noqa: E402
from compiler_support import compiler_target_args  # noqa: E402

GOALS = 128
# The production switch between the two starts, and the one the measurement flips.
SEED = 'const bool goalrow = rtcw || holomatch;'
MEASURED = 'const bool goalrow = rtcw || (holomatch && measureGoalRow);'

HARNESS = r'''
#include <cstdio>
#include <cstring>
#include <stdlib.h>
static constexpr int GOAL_SAMPLES=%d;
static std::vector<byte> readAll(const char *path){
 std::vector<byte> data;FILE *f=fopen(path,"rb");assert(f);int c;while((c=fgetc(f))!=EOF)data.push_back((byte)c);fclose(f);return data;
}
struct Route{int routed,time,reach;};
static std::vector<Route> routesTo(int goal){
 std::vector<Route> out(aasworld.numareas);
 for(int a=1;a<aasworld.numareas;++a){int time=0,reach=0;aasworld.frameroutingupdates=0;
  const int routed=AAS_AreaRouteToGoalArea(a,nullptr,goal,TFL_DEFAULT,&time,&reach);out[a]={routed,routed?time:0,routed?reach:0};}
 return out;
}
static void freshCaches(){AAS_FreeAllClusterAreaCache();AAS_FreeAllPortalCache();AAS_InitClusterAreaCache();AAS_InitPortalCache();}
int main(int argc,char **argv){
#ifdef _MSC_VER
 _set_abort_behavior(0,_WRITE_ABORT_MSG|_CALL_REPORTFAULT);
#endif
 assert(argc==3);profile=nullptr;
 for(const auto &p:profiles)if(p.id==COM_GAME_STVEF_MP)profile=&p;
 assert(profile&&profile->bots.navigationWorlds&&!Com_GameIsRTCW());
 initialize();LibVarSet("sv_mapChecksum",argv[2]);
 files["maps/probe.aas"]=readAll(argv[1]);
 if(AAS_LoadAASFile("maps/probe.aas")){printf("{\"loaded\":false}\n");return 0;}
 routingTables();
 std::vector<int> candidates;
 for(int a=1;a<aasworld.numareas;++a)if(aasworld.areasettings[a].numreachableareas>0)candidates.push_back(a);
 const size_t count=std::min<size_t>(GOAL_SAMPLES,candidates.size());
 long long compared=0,routedZero=0,routedRow=0,timeChanged=0,reachChanged=0,lost=0,gained=0,goalsChanged=0;
 long long insideTotal=0;int insideMax=0,rowsNonZero=0;
 for(size_t k=0;k<count;++k){const int goal=candidates[k*candidates.size()/count];
  const auto &row=aasworld.areatraveltimes[goal][0];const int links=aasworld.reversedreachability[goal].numlinks;
  bool nonzero=false;for(int i=0;i<links;++i){insideTotal+=row[i];insideMax=std::max<int>(insideMax,row[i]);nonzero|=row[i]!=0;}
  rowsNonZero+=nonzero;
  measureGoalRow=false;freshCaches();const auto zero=routesTo(goal);
  measureGoalRow=true;freshCaches();const auto own=routesTo(goal);
  bool changed=false;
  for(int a=1;a<aasworld.numareas;++a){++compared;routedZero+=zero[a].routed!=0;routedRow+=own[a].routed!=0;
   if(zero[a].routed&&!own[a].routed)++lost;if(!zero[a].routed&&own[a].routed)++gained;
   if(zero[a].routed&&own[a].routed){const bool t=zero[a].time!=own[a].time,r=zero[a].reach!=own[a].reach;
    timeChanged+=t;reachChanged+=r;changed|=t||r;}}
  goalsChanged+=changed;
 }
 printf("{\"loaded\":true,\"areas\":%d,\"goalsSampled\":%d,\"goalsWithReachabilities\":%d,\"routesCompared\":%lld,"
  "\"routedFromZeros\":%lld,\"routedFromRow\":%lld,\"travelTimeChanged\":%lld,\"firstLinkChanged\":%lld,"
  "\"routesLost\":%lld,\"routesGained\":%lld,\"goalsWithAChange\":%lld,\"goalRowsNotAllZero\":%d,"
  "\"goalRowEntriesSum\":%lld,\"goalRowEntryMax\":%d}\n",aasworld.numareas,(int)count,(int)candidates.size(),compared,
  routedZero,routedRow,timeChanged,reachChanged,lost,gained,goalsChanged,rowsNonZero,insideTotal,insideMax);
 shutdown();
}
'''


def header(data):
    """(version, checksum) of an AAS file; version 5 obfuscates its header past the version."""
    raw = bytearray(data[:124])
    version = struct.unpack_from('<i', raw, 4)[0]
    if version == 5:
        for i in range(8, 124):
            raw[i] ^= ((i - 8) * 119) & 255
    return version, struct.unpack_from('<i', raw, 8)[0]


def served_files(installation):
    """Every navigation file the game folder serves, the last pak holding a name winning."""
    served = {}
    for folder in sorted(p for p in installation.iterdir() if p.is_dir() and p.name.lower() == 'baseef'):
        for pak in sorted((p for p in folder.iterdir() if p.suffix.lower() == '.pk3'), key=lambda p: p.name.lower()):
            with zipfile.ZipFile(pak) as archive:
                for name in archive.namelist():
                    if name.lower().startswith('maps/') and name.lower().endswith('.aas'):
                        served[name.lower()] = (pak, name)
    for key, (pak, name) in sorted(served.items()):
        with zipfile.ZipFile(pak) as archive:
            yield f'{pak.parent.name}/{pak.name}:{name}', archive.read(name)


def build(directory):
    compiler = shutil.which('clang++')
    if not compiler:
        raise SystemExit('clang++ is required')
    source = routes.route_source() + routes.GRAPH
    if source.count(SEED) != 1:
        raise SystemExit('the routing no longer chooses its start as this measurement expects')
    source = source.replace(SEED, MEASURED)
    source = source.replace('static void AAS_UpdateAreaRoutingCache(',
                            'static bool measureGoalRow=true;\nstatic void AAS_UpdateAreaRoutingCache(', 1)
    source += HARNESS.replace('%d', str(GOALS), 1)
    cpp, exe = directory / 'measure.cpp', directory / 'measure.exe'
    cpp.write_text(source, encoding='utf-8')
    command = [compiler, *compiler_target_args(compiler), '-O2', '-std=c++17', '-D_CRT_SECURE_NO_WARNINGS',
               '-I', str(ROOT), str(cpp), *routing.RTCWRoutingTests.extra(), '-o', str(exe)]
    built = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=300)
    if built.returncode:
        raise SystemExit(built.stdout + built.stderr)
    return exe, command


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--installation', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    installation = args.installation.resolve(strict=True)
    report = dict(scope=__doc__.split('\n\n')[0], goalsPerFile=GOALS, installation=str(installation),
                  sourceCommit=subprocess.run(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True,
                                              capture_output=True).stdout.strip(), files={})
    (ROOT / '.tmp').mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=ROOT / '.tmp') as folder:
        folder = Path(folder)
        exe, command = build(folder)
        for label, data in served_files(installation):
            path = folder / 'probe.aas'
            path.write_bytes(data)
            version, checksum = header(data)
            run = subprocess.run([str(exe), str(path), str(checksum)], capture_output=True, text=True, timeout=1800)
            if run.returncode:
                raise SystemExit(f'{label}: exit {run.returncode}\n{run.stdout}{run.stderr}')
            row = json.loads(run.stdout)
            row.update(version=version, sha256=hashlib.sha256(data).hexdigest())
            report['files'][label] = row
            print(label, json.dumps(row), flush=True)
        report['compileCommand'] = command
    totals = {}
    for row in report['files'].values():
        for key, value in row.items():
            if isinstance(value, int) and not isinstance(value, bool) and key not in ('version', 'goalRowEntryMax'):
                totals[key] = totals.get(key, 0) + value
    report['totals'] = totals
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(totals))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
