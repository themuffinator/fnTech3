#!/usr/bin/env python3
"""Load every installed SP RCD through the real engine cache conversion.

Also exercises the MP12 selector with synthetic version headers, and both
navigation worlds. This verifies loading/ownership, not live AI routing.
Proprietary AAS/RCD input remains in memory.
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
from rtcw_routecache_load_tests import loader_source, COMPARE

CHECKS = r'''
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif
static std::vector<byte> readInput(){
 byte size[4];assert(fread(size,1,4,stdin)==4);unsigned length=rc::U32(size);
 assert(length>=36&&length<=rc::MaxFileBytes);std::vector<byte> out(length);assert(fread(out.data(),1,length,stdin)==length);return out;
}
int main(){
#ifdef _WIN32
 _setmode(_fileno(stdin),_O_BINARY);
#endif
 const auto aas=readInput(),rcd=readInput();byte header[124];assert(aas.size()>=sizeof(header));
 std::memcpy(header,aas.data(),sizeof(header));for(size_t i=8;i<124;++i)header[i]^=static_cast<byte>((i-8)*119);
 for(const auto &p:profiles){profile=&p;if(!Com_GameIsRTCW())continue;initialize();
  LibVarSet("sv_mapChecksum",std::to_string(rc::U32(header+8)).c_str());
  files["maps/probe.aas"]=aas;files["maps/probe.rcd"]=rcd;if(!Com_GameIsRTCWSP())files["maps/probe.rcd"][4]=12;
  for(int world=0;world<2;++world){AAS_SetCurrentWorld(world);
   assert(AAS_LoadAASFile("maps/probe.aas")==0);Q_strncpyz(aasworld.mapname,"probe",sizeof(aasworld.mapname));
   assert(AAS_ReadRTCWRouteCache());compare(files.at("maps/probe.rcd"));
  }shutdown();
 }
 printf("{\"worldLoads\":4,\"cacheRecords\":%llu,\"travelEntries\":%llu,\"visibilityRows\":%llu,\"waypoints\":%llu}",
  (unsigned long long)recordComparisons,(unsigned long long)entryComparisons,
  (unsigned long long)visibilityComparisons,(unsigned long long)waypointComparisons);
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
    paths = ['code/botlib/rtcw_routecache_loader.hpp', 'code/compat/rtcw_routecache_format.hpp',
             'code/botlib/rtcw_aas_loader.hpp', 'code/botlib/be_aas_route.cpp', 'code/botlib/be_aas_def.h',
             'tests/rtcw_routecache_load_tests.py', 'tests/rtcw_aas_load_tests.py']
    report = dict(utc=datetime.datetime.now(datetime.timezone.utc).isoformat(), sanitized=args.sanitize,
        scope='Native cache/visibility ownership and conversion; installed SP15 and synthetic MP12 header, both worlds; not live AI',
        target=subprocess.check_output([args.compiler, *target, '-dumpmachine'], cwd=ROOT, text=True).strip(),
        installation=str(args.installation.resolve(strict=True)),
        implementationSha256={p: hashlib.sha256((ROOT / p).read_bytes()).hexdigest() for p in paths}, assets=[])
    with tempfile.TemporaryDirectory(prefix='rcd-load-', dir=report_path.parent) as directory:
        folder = Path(directory); source = folder / 'probe.cpp'; exe = folder / 'probe.exe'
        source.write_text(loader_source() + COMPARE + CHECKS, encoding='utf-8')
        command = [args.compiler, *target, *flags, '-std=c++17', '-O2', '-ffast-math', '-D_CRT_SECURE_NO_WARNINGS',
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
          sum(x.get('travelEntries', 0) for x in report['assets']), 'converted travel entries')
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
