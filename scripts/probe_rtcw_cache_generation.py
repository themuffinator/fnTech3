#!/usr/bin/env python3
"""Compare RTCW visibility generation with original/iortcw functions.

Reference GPLv3 code is compiled only in a disposable local fixture, with its
notices. No implementation text is incorporated into fnTech3. This compares
generation decisions under controlled collision/PVS results, not BSP parity.
"""
from pathlib import Path
import argparse
import datetime
import hashlib
import json
import os
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tests'))
from rtcw_routecache_build_tests import generation_source
from rtcw_routing_tests import RTCWRoutingTests
from rtcw_server_world_tests import function

REFERENCES = r'''
struct ReferenceWorld {int numareas;aas_area_t *areas;byte **areavisibility;
 byte *decompressedvis;vec3_t *areawaypoints;};
'''
COMPARE = r'''
static uint64_t comparisons;
static void compare(){
 const auto &engine=*aas_currentWorld;
 world={};world.numareas=engine.numareas;world.areas=engine.areas;
 const auto before=allocations;const int traceBefore=traceCalls,pvsBefore=pvsCalls;
 AAS_CreateVisibility();
 assert(traceCalls-traceBefore==expectedTraces&&pvsCalls-pvsBefore==expectedPvs);
 std::vector<byte> encoded(engine.numareas*2);
 for(int area=0;area<engine.numareas;++area){
  const auto &row=engine.rtcwVisibility[area];
  assert(bool(row.data)==bool(world.areavisibility[area]));
  if(row.data){
   assert(fntech3::rtcw::routecache::DecompressVisibility({row.data,size_t(row.bytes)},
    engine.numareas,world.decompressedvis,engine.numareas));
   const int bytes=AAS_CompressVis(world.decompressedvis,engine.numareas,encoded.data());
   assert(bytes==row.bytes&&!std::memcmp(row.data,world.areavisibility[area],bytes));
   assert(!std::memcmp(row.data,encoded.data(),bytes));comparisons+=engine.numareas;
  }
  for(int axis=0;axis<3;++axis){assert(engine.rtcwWaypoints[area][axis]==world.areawaypoints[area][axis]);++comparisons;}
 }
 std::vector<void*> release;
 for(void *p:allocations)if(!before.count(p))release.push_back(p);
 for(void *p:release)FreeMemory(p);world={};assert(allocations==before);
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--references', type=Path, default=Path('E:/_SOURCE/_CODE'))
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--target')
    parser.add_argument('--sanitize', action='store_true')
    args = parser.parse_args()
    output = args.output.resolve()
    output.relative_to(ROOT)
    output.mkdir(parents=True, exist_ok=True)
    paths = [('originalSP', 'RTCW-SP-master/src/botlib/be_aas_route.c', True),
             ('originalMP', 'RTCW-MP-master/src/botlib/be_aas_route.c', False),
             ('iortcwSP', 'iortcw-master/SP/code/botlib/be_aas_route.c', True),
             ('iortcwMP', 'iortcw-master/MP/code/botlib/be_aas_route.c', False)]
    source = generation_source() + '\n#undef aasworld\nstatic int expectedTraces,expectedPvs;\n' + REFERENCES
    records = []
    for name, relative, sp in paths:
        path = (args.references / relative).resolve(strict=True)
        text = path.read_text(encoding='utf-8', errors='replace')
        notice = text[:text.index('*/')+2]
        if 'version 3' not in notice or 'Free Software Foundation' not in notice:
            raise ValueError(f'Unrecognized reference licence: {path}')
        source += f'\n{notice}\nnamespace {name} {{\nstatic ReferenceWorld world,*aasworld=&world;\n'
        for signature in ('int AAS_CompressVis(', 'void AAS_CreateVisibility('):
            source += function(text[text.rindex(signature):], signature) + '\n'
        source += COMPARE + '\n}\n'
        records.append(dict(name=name, singlePlayer=sp, path=str(path), sha256=hashlib.sha256(path.read_bytes()).hexdigest()))
    source += r'''
#define aasworld (*aas_currentWorld)
int main(){
 for(bool sp:{true,false}){profile=&profiles[sp?2:3];
  for(int ordinary:{1,3,17,85,100,125})for(int variation=0;variation<8;++variation){
   prepare(ordinary);
   if(variation&1)blockedArea=2;
   if(variation&2)missedArea=3;
   if(variation&4)escapedArea=4;
   assert(fntech3::rtcw::navigation::BuildRouteVisibility());checkVisibility();
   expectedTraces=traceCalls;expectedPvs=pvsCalls;
   if(sp){originalSP::compare();iortcwSP::compare();}else{originalMP::compare();iortcwMP::compare();}
   shutdown();
  }
 }
 printf("{\"originalSP\":%llu,\"originalMP\":%llu,\"iortcwSP\":%llu,\"iortcwMP\":%llu}\n",
  (unsigned long long)originalSP::comparisons,(unsigned long long)originalMP::comparisons,
  (unsigned long long)iortcwSP::comparisons,(unsigned long long)iortcwMP::comparisons);
}
'''
    compiler = shutil.which('clang++')
    if not compiler:
        parser.error('clang++ is required')
    target = ['--target=' + args.target] if args.target else (['-DFNTECH3_Q3_ONLY'] if os.name == 'nt' else ['-m32'])
    sanitizer = ['-fsanitize=address,undefined', '-fno-omit-frame-pointer'] if args.sanitize else []
    # The containing directory is explicit; no probe source lands in the OS temp folder.
    with tempfile.TemporaryDirectory(prefix='reference-', dir=output) as directory:
        directory = Path(directory)
        cpp, executable = directory/'probe.cpp', directory/('probe.exe' if os.name == 'nt' else 'probe')
        cpp.write_text(source, encoding='utf-8')
        command = [compiler, *target, *sanitizer, '-O2', '-std=c++17', '-D_CRT_SECURE_NO_WARNINGS',
                   '-I', str(ROOT), str(cpp), *RTCWRoutingTests.extra(), '-o', str(executable)]
        built = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=90)
        (output/'build.log').write_text(built.stdout+built.stderr, encoding='utf-8')
        if built.returncode:
            print(built.stdout+built.stderr)
            return 1
        result = subprocess.run([str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=90)
        (output/'run.log').write_text(result.stdout+result.stderr, encoding='utf-8')
        report = dict(utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
                      evidence='Source comparison under controlled collision and PVS results',
                      target=args.target or 'native', sanitize=args.sanitize, command=command,
                      references=records, executableSha256=hashlib.sha256(executable.read_bytes()).hexdigest(),
                      exitCode=result.returncode, passed=result.returncode==0,
                      comparisons=json.loads(result.stdout) if result.returncode==0 else None)
    (output/'result.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    print(json.dumps(report, indent=2))
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
