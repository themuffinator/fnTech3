#!/usr/bin/env python3
"""Measure which routes in installed bot navigation depend on the links past an area's 128th.

Quake III's routing starts each area cache in its goal area with a zero
travel time for every link into it (AAS_UpdateAreaRoutingCache). Retail
quake3.exe 1.32 and quakelive_steam.exe keep those zeros in a stack array of
128, so for an area entered by more reversed links they read the rest of the
stack instead (docs/fntech3/2026-09-28-aas-goal-links.md).

For every navigation file in the given installations' paks, this counts each
area's reversed links as AAS_CreateReversedReachability makes them: one for
each of the first 128 reachabilities of every other area. For each file with
an area entered by more than 128, it compiles the engine's own loader and
routing from source (the fixtures of tests/rtcw_route_query_tests.py), loads
the file under its profile, and routes every area to each such goal twice
with the default travel flags: with the goal's row all zeros, as the engine
does now, and with every entry past the 128th costing 16,384 (163.84 s), as
any word but zero on the stack would make it cost something. A route whose
travel time or first link differs between the two ends through one of those
links into the goal; its travel time was undefined before.

Run in the Visual Studio x86 developer shell, as the fixtures are. Paks are
only read; the file under test is copied into a temporary folder.

  --installation KEY=PATH  q3, ql or stvef: the Quake III, Quake Live or
                           Elite Force installation folder
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
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

# Game folders read per installation, and the alias of the profile that loads their maps.
FOLDERS = {'q3': (('baseq3', 'missionpack'), 'q3'), 'ql': (('baseq3',), 'ql'), 'stvef': (('baseef',), 'stvef-hm')}
LIMIT = 128
PAST = 16384

HARNESS = r'''
#include <cstdio>
#include <cstring>
#include <stdlib.h>
static constexpr int GOAL_LINK_LIMIT=%d,GOAL_PAST_COST=%d;
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
 assert(argc==4);profile=nullptr;
 for(const auto &p:profiles)if(p.aliases[0]&&!strcmp(p.aliases[0],argv[2]))profile=&p;
 assert(profile&&profile->bots.navigationWorlds&&!Com_GameIsRTCW());
 initialize();LibVarSet("sv_mapChecksum",argv[3]);
 files["maps/probe.aas"]=readAll(argv[1]);
 if(AAS_LoadAASFile("maps/probe.aas")){printf("{\"loaded\":false}\n");return 0;}
 routingTables();
 printf("{\"loaded\":true,\"areas\":%d,\"goals\":[",aasworld.numareas);bool first=true;
 for(int goal=1;goal<aasworld.numareas;++goal){const int links=aasworld.reversedreachability[goal].numlinks;
  if(links<=GOAL_LINK_LIMIT)continue;
  freshCaches();const auto zero=routesTo(goal);
  for(int i=GOAL_LINK_LIMIT;i<links;++i)aasworld.startareatraveltimes[i]=GOAL_PAST_COST;
  freshCaches();const auto past=routesTo(goal);
  for(int i=GOAL_LINK_LIMIT;i<links;++i)aasworld.startareatraveltimes[i]=0;
  int routed=0,time=0,reach=0,lost=0,sameCluster=0;const int cluster=aasworld.areasettings[goal].cluster;
  for(int a=1;a<aasworld.numareas;++a){routed+=zero[a].routed!=0;if(zero[a].routed&&!past[a].routed)++lost;
   const bool changed=zero[a].time!=past[a].time||zero[a].reach!=past[a].reach;
   time+=zero[a].time!=past[a].time;reach+=zero[a].reach!=past[a].reach;
   if(changed&&aasworld.areasettings[a].cluster==cluster)++sameCluster;}
  printf("%s{\"area\":%d,\"links\":%d,\"linksPastLimit\":%d,\"cluster\":%d,\"reachabilities\":%d,\"contents\":%d,"
   "\"routedAreas\":%d,\"travelTimeChanged\":%d,\"firstLinkChanged\":%d,\"routesLost\":%d,\"changedInGoalCluster\":%d}",
   first?"":",",goal,links,links-GOAL_LINK_LIMIT,cluster,aasworld.areasettings[goal].numreachableareas,
   aasworld.areasettings[goal].contents,routed,time,reach,lost,sameCluster);first=false;
 }
 printf("]}\n");shutdown();
}
'''


def header(data):
    """(version, checksum) of an AAS file; version 5 obfuscates its header past the version."""
    raw = bytearray(data[:124])
    version = struct.unpack_from('<i', raw, 4)[0]
    if version == 5:
        for i in range(8, 124):
            raw[i] ^= ((i - 8) * 119) & 255
    lumps = [struct.unpack_from('<ii', raw, 12 + 8 * i) for i in range(14)]
    return version, struct.unpack_from('<i', raw, 8)[0], lumps


def most_links(data):
    """Reversed links into each area, from the first 128 reachabilities of every area but area 0."""
    _, _, lumps = header(data)
    settings, reach = lumps[8], lumps[9]
    entry = lambda lump, index, size, fmt: struct.unpack_from(fmt, data, lump[0] + index * size)
    count = settings[1] // 28
    links = [0] * count
    for area in range(1, count):
        numreachable, first = entry(settings, area, 28, '<7i')[5:7]
        for n in range(min(numreachable, LIMIT)):
            links[entry(reach, first + n, 44, '<i')[0]] += 1
    return links


def navigation_files(key, installation):
    wanted, _ = FOLDERS[key]
    for folder in sorted(p for p in installation.iterdir() if p.is_dir() and p.name.lower() in wanted):
        for pak in sorted((p for p in folder.iterdir() if p.suffix.lower() == '.pk3'), key=lambda p: p.name.lower()):
            with zipfile.ZipFile(pak) as archive:
                for name in sorted(archive.namelist()):
                    if name.lower().startswith('maps/') and name.lower().endswith('.aas'):
                        yield f'{key}:{folder.name}/{pak.name}:{name}', archive.read(name)


def build(directory):
    compiler = shutil.which('clang++')
    if not compiler:
        raise SystemExit('clang++ is required')
    source = routes.route_source() + routes.GRAPH + HARNESS.replace('%d,GOAL_PAST_COST=%d', f'{LIMIT},GOAL_PAST_COST={PAST}')
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
    parser.add_argument('--installation', action='append', default=[], metavar='KEY=PATH')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    installations = {}
    for item in args.installation:
        key, _, path = item.partition('=')
        if key not in FOLDERS or not path:
            parser.error(f'unknown installation {item!r}')
        installations[key] = Path(path).resolve(strict=True)
    report = dict(scope=__doc__.split('\n\n')[0], limit=LIMIT, pastCost=PAST, installations={k: str(v) for k, v in
                  installations.items()}, sourceCommit=subprocess.run(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True,
                  capture_output=True).stdout.strip(), files={})
    with tempfile.TemporaryDirectory(dir=ROOT / '.tmp' if (ROOT / '.tmp').is_dir() else None) as folder:
        folder = Path(folder)
        exe = command = None
        scanned = 0
        for key, installation in sorted(installations.items()):
            profile = FOLDERS[key][1]
            for label, data in navigation_files(key, installation):
                scanned += 1
                links = most_links(data)
                if max(links) <= LIMIT:
                    continue
                if exe is None:
                    exe, command = build(folder)
                path = folder / 'probe.aas'
                path.write_bytes(data)
                version, checksum, _ = header(data)
                run = subprocess.run([str(exe), str(path), profile, str(checksum)], capture_output=True,
                                     text=True, timeout=600)
                if run.returncode:
                    raise SystemExit(f'{label}: exit {run.returncode}\n{run.stdout}{run.stderr}')
                row = json.loads(run.stdout)
                row.update(profile=profile, version=version, sha256=hashlib.sha256(data).hexdigest(),
                           mostLinks=max(links), areasOverLimit=sum(n > LIMIT for n in links))
                report['files'][label] = row
                print(label, json.dumps(row))
        report.update(filesScanned=scanned, compileCommand=command)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
