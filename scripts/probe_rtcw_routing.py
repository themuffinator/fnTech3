#!/usr/bin/env python3
"""Compare RTCW routing initialization with original/iortcw source functions.

GPLv3 reference functions compile only in disposable fixtures, with notices.
The production fixture uses fast-math; reference functions use precise math.
Installed navigation remains in memory. MP uses a synthetic RCD12 header;
this is initialization evidence, not retail MP AI or gameplay evidence.
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
from rtcw_routing_tests import routing_source
from rtcw_server_world_tests import function

REFERENCE = r'''
struct aas_areasettings_t : ::aas_areasettings_t {float groundsteepness;};
struct World {
 int numareas,reachabilitysize,numportals,travelflagfortype[MAX_TRAVELTYPES];
 aas_areasettings_t *areasettings;
 ::aas_reachability_t *reachability;
 ::aas_reversedreachability_t *reversedreachability;
 unsigned short ***areatraveltimes;
 ::aas_portal_t *portals;
 int *portalmaxtraveltimes;
 float time;int numframes,frameroutingupdates;
};
static World worlds[2],*aasworld=&worlds[0];
static std::vector<int> frameCalls;
static int Sys_MilliSeconds(){return 0;}
static void *AAS_RoutingGetMemory(int bytes){assert(bytes>=0);void *p=std::calloc(1,bytes);assert(p);return p;}
static void AAS_RoutingFreeMemory(void *p){std::free(p);}
// The original C expression calls double sqrt, even in this C++ harness.
static double sqrt(double x){return std::sqrt(x);}
static void AAS_SetCurrentWorld(int i){assert(i>=0&&i<2);aasworld=&worlds[i];frameCalls.push_back(i*3);}
static void AAS_InvalidateEntities(){frameCalls.push_back(int(aasworld-worlds)*3+1);}
static void AAS_ContinueInit(float){frameCalls.push_back(int(aasworld-worlds)*3+2);}
'''

COMPARE = r'''
static uint64_t scalarChecks,flagChecks,reverseChecks,costChecks,portalChecks,frameChecks;
static void scalar(){
 aas_areasettings_t settings{};worlds[0]={};aasworld=&worlds[0];aasworld->areasettings=&settings;
 AAS_InitTravelFlagFromType();
 for(int type=-1;type<65536;++type){assert(AAS_TravelFlagForType(type)==fntech3::rtcw::routing::TravelFlag(type));++flagChecks;}
 for(int contents=0;contents<16384;++contents){settings.contents=contents;
  assert(AAS_AreaContentsTravelFlag(0)==fntech3::rtcw::routing::ContentsFlags(contents));++flagChecks;}
 uint32_t seed=1;auto nextRandom=[&]{seed=seed*1664525u+1013904223u;return seed;};
 for(int i=0;i<100000;++i){
  settings.presencetype=(i&1)?PRESENCE_NORMAL:0;settings.areaflags=(i&2)?AREA_LIQUID:0;
  settings.groundsteepness=float(nextRandom()%65537)/65536.0f;vec3_t a,b;
  for(int axis=0;axis<3;++axis){a[axis]=(int(nextRandom()%65537)-32768)*.03125f;b[axis]=(int(nextRandom()%65537)-32768)*.03125f;}
  if(i<60000){a[0]=a[1]=a[2]=b[1]=b[2]=0;settings.groundsteepness=0;
   float boundary=float((i/3+1)/((i&1)?((i&2)?1.0:.33):1.3));
   b[0]=(i%3==0)?std::nextafter(boundary,0.0f):(i%3==1)?boundary:std::nextafter(boundary,131072.0f);}
  uint16_t ours=0;assert(fntech3::rtcw::routing::AreaTravelTime(a,b,settings.groundsteepness,
   !(settings.presencetype&PRESENCE_NORMAL),(settings.areaflags&AREA_LIQUID)!=0,singlePlayer,ours));
  auto theirs=AAS_AreaTravelTime(0,a,b);
  if(ours!=theirs){fprintf(stderr,"scalar %d ours %u reference %u\n",i,ours,theirs);std::abort();}++scalarChecks;
 }
 worlds[0]={};worlds[1]={};worlds[0].numframes=7;worlds[1].numframes=11;
 for(int i=0;i<8;++i){frameCalls.clear();worlds[0].frameroutingupdates=33;worlds[1].frameroutingupdates=44;
  float time=i*.125f;assert(AAS_StartFrame(time)==0&&aasworld==&worlds[1]);
  assert((frameCalls==std::vector<int>{0,1,2,3,4,5}));
  assert(worlds[0].time==time&&worlds[1].time==time&&!worlds[0].frameroutingupdates&&!worlds[1].frameroutingupdates);
  assert(worlds[0].numframes==7&&worlds[1].numframes==12+i);++frameChecks;
 }
}
static void compare(const ::aas_t &engine){
 worlds[0]={};aasworld=&worlds[0];aasworld->numareas=engine.numareas;aasworld->reachabilitysize=engine.reachabilitysize;
 aasworld->numportals=engine.numportals;aasworld->reachability=engine.reachability;aasworld->portals=engine.portals;
 aasworld->areatraveltimes=engine.areatraveltimes;
 std::vector<aas_areasettings_t> settings(engine.numareas);aasworld->areasettings=settings.data();
 for(int area=0;area<engine.numareas;++area){static_cast<::aas_areasettings_t&>(settings[area])=engine.areasettings[area];
  settings[area].groundsteepness=engine.rtcwGroundSteepness[area];
  assert(AAS_AreaContentsTravelFlag(area)==engine.areacontentstravelflags[area]);++flagChecks;}
 AAS_CreateReversedReachability();
 for(int area=0;area<engine.numareas;++area){const auto &ours=engine.reversedreachability[area];
  const auto &theirs=aasworld->reversedreachability[area];assert(ours.numlinks==theirs.numlinks);
  auto *a=ours.first,*b=theirs.first;
  for(;a&&b;a=a->next,b=b->next){assert(a->linknum==b->linknum&&a->areanum==b->areanum);++reverseChecks;}
  assert(!a&&!b);const auto &s=settings[area];
  for(int out=0;out<s.numreachableareas;++out){const auto &reach=engine.reachability[s.firstreachablearea+out];int in=0;
   for(auto *link=theirs.first;link;link=link->next,++in){vec3_t start,end;
    VectorCopy(engine.reachability[link->linknum].end,start);VectorCopy(reach.start,end);
    const auto expected=AAS_AreaTravelTime(area,start,end),actual=engine.areatraveltimes[area][out][in];
    if(actual!=expected){fprintf(stderr,"matrix area %d out %d in %d ours %u reference %u\n",area,out,in,actual,expected);std::abort();}
    ++costChecks;
   }
  }
 }
 for(int portal=0;portal<engine.numportals;++portal){assert(AAS_PortalMaxTravelTime(portal)==engine.portalmaxtraveltimes[portal]);++portalChecks;}
 AAS_RoutingFreeMemory(aasworld->reversedreachability);worlds[0]={};
}
'''

CHECKS = r'''
#define aasworld (*aas_currentWorld)
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif
namespace rc=fntech3::rtcw::routecache;
struct Reference {const char *name;bool singlePlayer;void(*scalar)();void(*compare)(const aas_t&);uint64_t *counts[6];};
#define REF(name) {#name,name::singlePlayer,name::scalar,name::compare,{&name::scalarChecks,&name::flagChecks,&name::reverseChecks,&name::costChecks,&name::portalChecks,&name::frameChecks}}
static const Reference references[]={REF(originalSP),REF(originalMP),REF(iortcwSP),REF(iortcwMP)};
#undef REF
static std::vector<byte> readInput(){
 byte size[4];assert(fread(size,1,4,stdin)==4);unsigned length=rc::U32(size);
 assert(length>=36&&length<=rc::MaxFileBytes);std::vector<byte> out(length);assert(fread(out.data(),1,length,stdin)==length);return out;
}
int main(){
#ifdef _WIN32
 _setmode(_fileno(stdin),_O_BINARY);
#endif
 for(const auto &ref:references)ref.scalar();
 const auto aas=readInput(),rcd=readInput();byte header[124];assert(aas.size()>=sizeof(header));
 std::memcpy(header,aas.data(),sizeof(header));for(size_t i=8;i<124;++i)header[i]^=static_cast<byte>((i-8)*119);
 for(const auto &p:profiles){profile=&p;if(!Com_GameIsRTCW())continue;initialize();botimport.Print=checkedPrint;
  LibVarSet("sv_mapChecksum",std::to_string(rc::U32(header+8)).c_str());
  files["maps/probe.aas"]=aas;files["maps/probe.rcd"]=rcd;if(!Com_GameIsRTCWSP())files["maps/probe.rcd"][4]=12;
  for(int world=0;world<2;++world){AAS_SetCurrentWorld(world);
   assert(AAS_LoadAASFile("maps/probe.aas")==0);Q_strncpyz(aasworld.mapname,"probe",sizeof(aasworld.mapname));}
  assert(AAS_StartFrame(1.25f)==0&&aas_currentWorld==&aasWorlds[1]);
  assert(aasWorlds[0].numframes==0&&aasWorlds[1].numframes==1);
  for(int world=0;world<2;++world){AAS_SetCurrentWorld(world);
   assert(aasworld.initialized&&aasworld.rtcwRouteCacheLoaded&&aasworld.time==1.25f&&aasworld.max_routingcachesize==4096*1024);
   aasworld.areaupdate[aasworld.numareas-1].areanum=77; // validate global-area scratch capacity
   aasworld.portalupdate[aasworld.numportals].areanum=77;
   for(const auto &ref:references)if(ref.singlePlayer==(Com_GameIsRTCWSP()!=0))ref.compare(aasworld);
  }shutdown();
 }
 printf("{\"worldLoads\":4,\"references\":[");bool first=true;
 for(const auto &ref:references){if(!first)printf(",");first=false;
  printf("{\"name\":\"%s\",\"scalarChecks\":%llu,\"flagChecks\":%llu,\"reverseChecks\":%llu,\"costChecks\":%llu,\"portalChecks\":%llu,\"frameChecks\":%llu}",
   ref.name,(unsigned long long)*ref.counts[0],(unsigned long long)*ref.counts[1],(unsigned long long)*ref.counts[2],
   (unsigned long long)*ref.counts[3],(unsigned long long)*ref.counts[4],(unsigned long long)*ref.counts[5]);
 }printf("]}");
}
'''


def fixture(references):
    source = routing_source() + '\n#undef aasworld\n#pragma float_control(precise, on, push)\n'
    evidence = {}
    for name, root in references:
        route_path = root / 'botlib/be_aas_route.c'
        reach_path = root / 'botlib/be_aas_reach.c'
        main_path = root / 'botlib/be_aas_main.c'
        math_path = root / ('qcommon/q_shared.h' if name.startswith('iortcw') else 'game/q_math.c')
        flags_path = root / ('game/be_aas.h' if name == 'originalMP' else 'botlib/be_aas.h')
        paths = [route_path, reach_path, main_path, math_path, flags_path, root / 'botlib/aasfile.h']
        texts = {p: p.read_text() for p in paths}
        evidence[name] = {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}
        source += '\n' + texts[route_path].split('*/', 1)[0] + '*/\n'
        # Use each reference's own flags and constants, not engine aliases.
        defines = re.findall(r'^#define\s+((?:TFL_\w+|TRAVEL_\w+|AREACONTENTS_\w+|PRESENCE_\w+|AREA_LIQUID|MAX_TRAVELTYPES|MAX_AAS_WORLDS|DISTANCEFACTOR_\w+|GROUNDSTEEPNESS_TIMESCALE))\s+([^\r\n]+)',
                             '\n'.join(texts[p] for p in paths), re.M)
        for key, value in defines:
            if key == 'TFL_DEFAULT':
                continue  # multiline macro is unused
            source += f'#undef {key}\n#define {key} {value}\n'
        source += '\n#define MAX_AAS_WORLDS 2\nnamespace ' + name + ' {\n'
        source += f'constexpr bool singlePlayer={str(name.endswith("SP")).lower()};\n' + REFERENCE
        signature = 'static ID_INLINE vec_t VectorLength(' if name.startswith('iortcw') else 'vec_t VectorLength('
        source += function(texts[math_path], signature) + '\n'
        for signature in ('int AAS_AreaCrouch(', 'int AAS_AreaSwim('):
            source += function(texts[reach_path], signature) + '\n'
        for signature in ('void AAS_InitTravelFlagFromType(', 'int AAS_TravelFlagForType(',
                          'void AAS_CreateReversedReachability(', 'float AAS_AreaGroundSteepnessScale(',
                          'unsigned short int AAS_AreaTravelTime(', 'int AAS_PortalMaxTravelTime(',
                          'int AAS_AreaContentsTravelFlag('):
            source += function(texts[route_path], signature) + '\n'
        source += function(texts[main_path], 'int AAS_StartFrame(') + '\n' + COMPARE + '\n}\n'
    return source + '\n#pragma float_control(pop)\n' + CHECKS, evidence


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--installation', type=Path, required=True)
    parser.add_argument('--sp-source', type=Path, required=True)
    parser.add_argument('--mp-source', type=Path, required=True)
    parser.add_argument('--iortcw-source', type=Path, required=True)
    parser.add_argument('--report', type=Path, required=True)
    parser.add_argument('--compiler', default=shutil.which('clang++'))
    parser.add_argument('--target')
    parser.add_argument('--sanitize', action='store_true')
    parser.add_argument('--float32-reference', action='store_true',
                        help='Use x86 SSE2 evaluation for reference binary32 parity instead of extended x87 intermediates')
    args = parser.parse_args()
    report_path = args.report.resolve(); report_path.relative_to(ROOT)
    report_path.parent.mkdir(parents=True, exist_ok=True)
    target = ['--target=' + args.target] if args.target else (['-m32'] if os.name != 'nt' else ['-DFNTECH3_Q3_ONLY'])
    flags = ['-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-g'] if args.sanitize else []
    if args.float32_reference:
        flags += ['-msse2', '-mfpmath=sse']
    source, references = fixture([('originalSP', args.sp_source / 'src'), ('originalMP', args.mp_source / 'src'),
        ('iortcwSP', args.iortcw_source / 'SP/code'), ('iortcwMP', args.iortcw_source / 'MP/code')])
    paths = ['code/compat/rtcw_routing.hpp', 'code/botlib/rtcw_routing_init.hpp', 'code/botlib/be_aas_main.cpp',
             'code/botlib/be_aas_route.cpp', 'code/botlib/l_libvar.cpp', 'tests/rtcw_routing_tests.py',
             'scripts/probe_rtcw_routing.py']
    report = dict(utc=datetime.datetime.now(datetime.timezone.utc).isoformat(), sanitized=args.sanitize,
        float32Reference=args.float32_reference,
        scope='Frame initialization, routing tables and costs; SP15 assets and synthetic MP12 header, both worlds; not live AI',
        target=subprocess.check_output([args.compiler, *target, '-dumpmachine'], cwd=ROOT, text=True).strip(),
        installation=str(args.installation.resolve(strict=True)), references=references,
        implementationSha256={p: hashlib.sha256((ROOT / p).read_bytes()).hexdigest() for p in paths}, assets=[])
    with tempfile.TemporaryDirectory(prefix='routing-reference-', dir=report_path.parent) as directory:
        folder = Path(directory); cpp = folder / 'probe.cpp'; exe = folder / 'probe.exe'
        cpp.write_text(source, encoding='utf-8')
        command = [args.compiler, *target, *flags, '-std=c++17', '-O2', '-ffast-math', '-D_CRT_SECURE_NO_WARNINGS',
                   '-I', str(ROOT), str(cpp), str(ROOT / 'code/server/server_game_data.cpp'),
                   '-x', 'c++', str(ROOT / 'code/compat/game_abi_adapter.c'), '-o', str(exe)]
        built = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=90)
        report.update(compileCommand=command, buildExitCode=built.returncode)
        if built.returncode:
            report['buildOutput'] = built.stdout + built.stderr
        else:
            for archive in sorted((args.installation / 'Main').glob('*.pk3')):
                with zipfile.ZipFile(archive) as pak:
                    for entry in pak.infolist():
                        if not entry.filename.lower().endswith('.rcd'):
                            continue
                        data = pak.read(entry); aas = pak.read(entry.filename[:-4] + '.aas')
                        run = subprocess.run([str(exe)], cwd=ROOT,
                            input=struct.pack('<I', len(aas)) + aas + struct.pack('<I', len(data)) + data,
                            capture_output=True, timeout=90)
                        row = dict(archive=archive.name, name=entry.filename, bytes=len(data),
                            sha256=hashlib.sha256(data).hexdigest(), aasSha256=hashlib.sha256(aas).hexdigest(),
                            exitCode=run.returncode, stderr=run.stderr.decode(errors='replace'))
                        try:
                            row.update(json.loads(run.stdout))
                        except (ValueError, UnicodeDecodeError):
                            row['stdout'] = run.stdout.decode(errors='replace')
                        report['assets'].append(row)
                        if run.returncode:
                            print('FAIL', archive.name, entry.filename, row['stderr'][:400]); break
                if report['assets'] and report['assets'][-1]['exitCode']:
                    break
        report['passed'] = not built.returncode and bool(report['assets']) and all(x['exitCode'] == 0 for x in report['assets'])
    report_path.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print('PASS' if report['passed'] else 'FAIL', len(report['assets']), 'assets,',
          sum(r.get('costChecks', 0) for a in report['assets'] for r in a.get('references', [])), 'cost comparisons')
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
