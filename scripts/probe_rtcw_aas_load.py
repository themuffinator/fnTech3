#!/usr/bin/env python3
"""Load installed AAS8 records through the real botlib loader in memory.

Checks every converted record against the pinned little-endian disk layout,
including the separately owned ground steepness. This is loader evidence, not
AI routing or gameplay evidence. Proprietary inputs are never written out.
"""
from __future__ import annotations
import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tests'))
from rtcw_aas_load_tests import loader_source

CHECKS = r'''
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif
using namespace fntech3::rtcw::aas;
static uint64_t records;
template<class T> void check(const View &view,Lump kind,const T *values,int count,size_t bytes=sizeof(T)){
 assert(count==(int)view.Count(kind)&&bytes<=Strides[kind]);
 for(int i=0;i<count;++i){assert(!std::memcmp(&values[i],view.Record(kind,i),bytes));++records;}
}
int main(){
#ifdef _WIN32
 _setmode(_fileno(stdin),_O_BINARY);
#endif
 unsigned char size[4];assert(fread(size,1,4,stdin)==4);const unsigned length=U32(size);
 assert(length>=124&&length<64*1024*1024);
 std::vector<byte> input(length);assert(fread(input.data(),1,length,stdin)==length);
 unsigned char header[124];std::memcpy(header,input.data(),124);
 for(unsigned i=8;i<124;++i)header[i]^=static_cast<byte>((i-8)*119);
 const uint32_t checksum=U32(header+8);View view;
 assert(Parse(input.data(),input.size(),checksum,view)==Error::None);
 for(const auto &p:profiles){profile=&p;if(!Com_GameIsRTCW())continue;initialize();
  LibVarSet("sv_mapChecksum",std::to_string(checksum).c_str());files["maps/probe.aas"]=input;
  for(int world=0;world<2;++world){assert(AAS_SetCurrentWorld(world));
   assert(AAS_LoadAASFile("maps/probe.aas")==0&&aasworld.loaded&&!aasworld.initialized);
   check(view,Boxes,aasworld.bboxes,aasworld.numbboxes);
   check(view,Vertices,aasworld.vertexes,aasworld.numvertexes);
   check(view,Planes,aasworld.planes,aasworld.numplanes);
   check(view,Edges,aasworld.edges,aasworld.numedges);
   check(view,EdgeIndices,aasworld.edgeindex,aasworld.edgeindexsize);
   check(view,Faces,aasworld.faces,aasworld.numfaces);
   check(view,FaceIndices,aasworld.faceindex,aasworld.faceindexsize);
   check(view,Areas,aasworld.areas,aasworld.numareas);
   static_assert(sizeof(aas_areasettings_t)==28);
   check(view,Settings,aasworld.areasettings,aasworld.numareasettings);
   for(int i=0;i<aasworld.numareasettings;++i)
    assert(!std::memcmp(&aasworld.rtcwGroundSteepness[i],view.Record(Settings,i)+28,4));
   // Two padding bytes at the end of a reachability are not semantic fields.
   check(view,Reachability,aasworld.reachability,aasworld.reachabilitysize,42);
   check(view,Nodes,aasworld.nodes,aasworld.numnodes);
   check(view,Portals,aasworld.portals,aasworld.numportals);
   check(view,PortalIndices,aasworld.portalindex,aasworld.portalindexsize);
   check(view,Clusters,aasworld.clusters,aasworld.numclusters);
  }
  shutdown();
 }
 printf("{\"worldLoads\":4,\"records\":%llu,\"areasPerWorld\":%zu}\n",
  (unsigned long long)records,view.Count(Areas));
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--installation', type=Path, required=True)
    parser.add_argument('--report', type=Path, required=True)
    parser.add_argument('--compiler', default=shutil.which('clang++'))
    parser.add_argument('--target')
    parser.add_argument('--sanitize', action='store_true')
    args = parser.parse_args()
    report_path = args.report.resolve(); report_path.relative_to(ROOT)
    report_path.parent.mkdir(parents=True, exist_ok=True)
    target = ['--target=' + args.target] if args.target else (['-m32'] if os.name != 'nt' else ['-DFNTECH3_Q3_ONLY'])
    flags = ['-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-g'] if args.sanitize else []
    paths = ['code/botlib/rtcw_aas_loader.hpp', 'code/botlib/be_aas_file.cpp',
             'code/botlib/be_aas_main.cpp', 'code/compat/rtcw_aas_format.hpp',
             'tests/rtcw_aas_load_tests.py', 'tests/rtcw_navigation_tests.py']
    report = dict(kind='AAS8 live-loader CPU record comparison; not AI or gameplay parity',
        utc=datetime.datetime.now(datetime.timezone.utc).isoformat(), sanitized=args.sanitize,
        target=subprocess.check_output([args.compiler, *target, '-dumpmachine'], cwd=ROOT, text=True).strip(),
        installation=str(args.installation.resolve(strict=True)),
        implementationSha256={p: hashlib.sha256((ROOT / p).read_bytes()).hexdigest() for p in paths}, assets=[])
    with tempfile.TemporaryDirectory(prefix='aas8-load-', dir=report_path.parent) as directory:
        folder = Path(directory); source = folder / 'probe.cpp'; exe = folder / 'probe.exe'
        source.write_text(loader_source() + CHECKS, encoding='utf-8')
        command = [args.compiler, *target, *flags, '-std=c++17', '-D_CRT_SECURE_NO_WARNINGS',
                   '-I', str(ROOT), str(source), str(ROOT / 'code/server/server_game_data.cpp'),
                   '-x', 'c++', str(ROOT / 'code/compat/game_abi_adapter.c'), '-o', str(exe)]
        built = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=90)
        report.update(compileCommand=command, buildExitCode=built.returncode)
        if built.returncode:
            report['buildOutput'] = built.stdout + built.stderr
        else:
            for archive in sorted((args.installation / 'Main').glob('*.pk3')):
                with zipfile.ZipFile(archive) as pak:
                    for entry in pak.infolist():
                        if not entry.filename.lower().endswith('.aas'):
                            continue
                        data = pak.read(entry)
                        result = subprocess.run([str(exe)], cwd=ROOT, input=struct.pack('<I', len(data)) + data,
                                                capture_output=True, timeout=40)
                        row = dict(archive=archive.name, name=entry.filename, bytes=len(data),
                            sha256=hashlib.sha256(data).hexdigest(), exitCode=result.returncode,
                            stderr=result.stderr.decode(errors='replace'))
                        try:
                            row.update(json.loads(result.stdout))
                        except (ValueError, UnicodeDecodeError):
                            row['stdout'] = result.stdout.decode(errors='replace')
                        report['assets'].append(row)
        report['passed'] = built.returncode == 0 and bool(report['assets']) and all(
            item['exitCode'] == 0 for item in report['assets'])
    report_path.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print('PASS' if report['passed'] else 'FAIL', len(report['assets']), 'assets,',
          sum(row.get('records', 0) for row in report['assets']), 'record comparisons')
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
