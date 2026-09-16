#!/usr/bin/env python3
"""Compare installed RTCW cache records and visibility with reference readers.

Original GPLv3 functions compile unchanged only in disposable fixtures. Their
32-bit pointer-dependent implementations run only on x86; iortcw's portable
readers also run on x64. Installed AAS/RCD data stays in memory.
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
from rtcw_server_world_tests import function

PRELUDE = r'''
#include "code/compat/rtcw_routecache_format.hpp"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif
using byte=unsigned char;using vec3_t=float[3];using fileHandle_t=int;
static int LittleLong(int v){return v;}static short LittleShort(short v){return v;}static float LittleFloat(float v){return v;}
#define LL(x) (x)=LittleLong(x)
static const byte *fileData;static size_t fileBytes,fileAt;
static int readFile(void *out,int bytes,fileHandle_t){
 assert(bytes>=0&&fileAt<=fileBytes&&size_t(bytes)<=fileBytes-fileAt);
 std::memcpy(out,fileData+fileAt,bytes);fileAt+=bytes;return bytes;
}
static struct {int(*FS_Read)(void*,int,fileHandle_t);} botimport={readFile};
static void *AAS_RoutingGetMemory(int bytes){assert(bytes>=0);void *p=std::calloc(1,bytes);assert(p);return p;}
static void AAS_RoutingFreeMemory(void *p){std::free(p);}
static void AAS_Error(const char *,...){assert(false);}
struct Decoded {
 float time,origin[3],start;int cluster,area,flags;
 std::vector<unsigned short> times;std::vector<byte> reaches;
};
'''

NORMALIZE = r'''
static Decoded decode(size_t count){
 auto *cache=AAS_ReadCache(1);Decoded out{};
 out.time=cache->time;out.cluster=cache->cluster;out.area=cache->areanum;
 std::memcpy(out.origin,cache->origin,sizeof(out.origin));out.start=cache->starttraveltime;out.flags=cache->travelflags;
 const auto *times=reinterpret_cast<const unsigned short *>(reinterpret_cast<const byte *>(cache)+offsetof(aas_routingcache_t,traveltimes));
 out.times.assign(times,times+count);out.reaches.assign(cache->reachabilities,cache->reachabilities+count);
 AAS_RoutingFreeMemory(cache);return out;
}
'''

CHECKS = r'''
namespace rc=fntech3::rtcw::routecache;namespace aa=fntech3::rtcw::aas;
struct Reference {const char *name;Decoded(*decode)(size_t);void(*visibility)(byte*,int,byte*);};
#define REF(name) {#name,name::decode,name::AAS_DecompressVis}
static const Reference references[]={
#if UINTPTR_MAX == 0xffffffffu
 REF(originalSP),REF(originalMP),
#endif
 REF(iortcwSP),REF(iortcwMP)};
#undef REF
static std::vector<byte> readInput(){
 byte size[4];assert(fread(size,1,4,stdin)==4);unsigned length=aa::U32(size);
 assert(length>=36&&length<=rc::MaxFileBytes);std::vector<byte> out(length);assert(fread(out.data(),1,length,stdin)==length);return out;
}
static void equal(float a,float b){assert(!std::memcmp(&a,&b,4));}
int main(){
#ifdef _WIN32
 _setmode(_fileno(stdin),_O_BINARY);
#endif
 const auto aas=readInput(),bytes=readInput();byte header[124];assert(aas.size()>=sizeof(header));
 std::memcpy(header,aas.data(),sizeof(header));for(size_t i=8;i<124;++i)header[i]^=static_cast<byte>((i-8)*119);
 aa::View map;assert(aa::Parse(aas.data(),aas.size(),aa::U32(header+8),map)==aa::Error::None);
 rc::View view;const auto error=rc::Parse(bytes.data(),bytes.size(),rc::Version::SP,rc::MapSignature(map),view);
 if(error!=rc::Error::None){fprintf(stderr,"RCD parse: %s\n",rc::ErrorName(error));return 2;}
 const auto topology=rc::ValidateTopology(view,rc::AAS8Topology(map));
 if(topology!=rc::Error::None){fprintf(stderr,"RCD topology: %s\n",rc::ErrorName(topology));return 3;}
 // Version 12 uses the same record layout. A synthetic MP header exercises
 // version selection; this does not claim that installed SP caches are MP data.
 auto mp=bytes;mp[4]=12;rc::View mpView;
 assert(rc::Parse(mp.data(),mp.size(),rc::Version::MP,rc::MapSignature(map),mpView)==rc::Error::None);
 assert(rc::Parse(bytes.data(),bytes.size(),rc::Version::MP,rc::MapSignature(map),mpView)==rc::Error::Version);
 fileData=bytes.data();fileBytes=bytes.size();printf("{\"references\":[");bool first=true;
 for(const auto &reference:references){
  fileAt=rc::HeaderBytes;uint64_t records=0,times=0,rows=0,visibilityBytes=0;
  for(const auto cache:view.Caches()){
   const size_t at=fileAt;const auto decoded=reference.decode(cache.Count());assert(fileAt==at+cache.Bytes());
   equal(cache.Time(),decoded.time);equal(cache.StartTravelTime(),decoded.start);
   assert(cache.Cluster()==decoded.cluster&&cache.Area()==decoded.area&&cache.TravelFlags()==decoded.flags);
   for(unsigned i=0;i<3;++i)equal(cache.Origin(i),decoded.origin[i]);
   for(size_t i=0;i<cache.Count();++i){assert(cache.TravelTime(i)==decoded.times[i]&&cache.Reachability(i)==decoded.reaches[i]);++times;}
   ++records;
  }
  std::vector<byte> ours(view.AreaCount()+2,33),theirs(view.AreaCount()+2,33);
  for(const auto row:view.Visibility()){
   if(!row.bytes)continue;
   assert(rc::DecompressVisibility(row,view.AreaCount(),ours.data()+1,view.AreaCount()));
   reference.visibility(const_cast<byte *>(row.data),int(view.AreaCount()),theirs.data()+1);
   assert(ours==theirs);++rows;visibilityBytes+=view.AreaCount();
  }
  if(!first)printf(",");first=false;
  printf("{\"name\":\"%s\",\"cacheRecords\":%llu,\"travelEntries\":%llu,\"visibilityRows\":%llu,\"visibilityBytes\":%llu}",
   reference.name,(unsigned long long)records,(unsigned long long)times,(unsigned long long)rows,(unsigned long long)visibilityBytes);
 }
 const size_t start=bytes.size()-view.AreaCount()*12;
 for(size_t a=0;a<view.AreaCount();++a)for(unsigned axis=0;axis<3;++axis)
  equal(view.Waypoint(a,axis),aa::F32(bytes.data()+start+a*12+axis*4));
 printf("],\"waypoints\":%zu}",view.AreaCount());
}
'''


def reference_source(name, root):
    route_path = root / 'botlib/be_aas_route.c'
    def_path = root / 'botlib/be_aas_def.h'
    route = route_path.read_text(encoding='utf-8', errors='replace')
    defs = def_path.read_text(encoding='utf-8', errors='replace')
    source = '#if UINTPTR_MAX == 0xffffffffu\n' if name.startswith('original') else ''
    source += route[:route.index('*/') + 2] + f'\nnamespace {name} {{\n'
    start = defs.index('typedef struct aas_routingcache_s')
    source += defs[start:defs.index('} aas_routingcache_t;', start) + len('} aas_routingcache_t;')] + '\n'
    if name.startswith('iortcw'):
        end = defs.index('} aas_routingcache_32_t;') + len('} aas_routingcache_32_t;')
        start = defs.rindex('typedef struct', 0, end)
        source += defs[start:end] + '\nstatic_assert(sizeof(aas_routingcache_32_t)==52&&offsetof(aas_routingcache_32_t,traveltimes)==48);\n'
    source += '#if UINTPTR_MAX == 0xffffffffu\nstatic_assert(sizeof(aas_routingcache_t)==52&&offsetof(aas_routingcache_t,traveltimes)==48);\n#endif\n'
    source += function(route, 'aas_routingcache_t *AAS_ReadCache( fileHandle_t fp ) {') + '\n'
    source += function(route, 'void AAS_DecompressVis( byte *in, int numareas, byte *decompressed ) {') + '\n'
    source += NORMALIZE + '\n}\n'
    if name.startswith('original'):
        source += '#endif\n'
    return source, [route_path, def_path]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--installation', type=Path, required=True)
    parser.add_argument('--sp-source', type=Path, required=True)
    parser.add_argument('--mp-source', type=Path, required=True)
    parser.add_argument('--iortcw', type=Path, required=True)
    parser.add_argument('--report', type=Path, required=True)
    parser.add_argument('--compiler', default=shutil.which('clang++'))
    parser.add_argument('--target')
    parser.add_argument('--sanitize', action='store_true')
    args = parser.parse_args()
    report_path = args.report.resolve(); report_path.relative_to(ROOT)
    report_path.parent.mkdir(parents=True, exist_ok=True)
    source = PRELUDE; paths = []
    for name, root in [('originalSP', args.sp_source / 'src'), ('originalMP', args.mp_source / 'src'),
                       ('iortcwSP', args.iortcw / 'SP/code'), ('iortcwMP', args.iortcw / 'MP/code')]:
        code, files = reference_source(name, root); source += code; paths.extend(files)
    source += CHECKS
    target = ['--target=' + args.target] if args.target else (['-m32'] if os.name != 'nt' else [])
    flags = ['-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-g'] if args.sanitize else []
    report = dict(utc=datetime.datetime.now(datetime.timezone.utc).isoformat(), sanitized=args.sanitize,
        scope='Installed SP RCD15 cache/visibility decoding; synthetic MP12 header; not live routing or AI parity',
        target=subprocess.check_output([args.compiler, *target, '-dumpmachine'], cwd=ROOT, text=True).strip(),
        installation=str(args.installation.resolve(strict=True)),
        implementationSha256={p: hashlib.sha256((ROOT / p).read_bytes()).hexdigest() for p in
            ['code/compat/rtcw_routecache_format.hpp', 'code/compat/rtcw_aas_format.hpp']},
        referenceSources=[dict(path=str(p), sha256=hashlib.sha256(p.read_bytes()).hexdigest()) for p in paths], assets=[])
    with tempfile.TemporaryDirectory(prefix='rcd-reference-', dir=report_path.parent) as directory:
        folder = Path(directory); cpp = folder / 'probe.cpp'; exe = folder / 'probe.exe'
        cpp.write_text(source, encoding='utf-8')
        command = [args.compiler, *target, *flags, '-std=c++17', '-O2', '-D_CRT_SECURE_NO_WARNINGS',
                   '-I', str(ROOT), str(cpp), '-o', str(exe)]
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
                            print('FAIL', archive.name, entry.filename, row['stderr'][:400])
                            break
                if report['assets'] and report['assets'][-1]['exitCode']:
                    break
        report['passed'] = not built.returncode and bool(report['assets']) and all(x['exitCode'] == 0 for x in report['assets'])
    report_path.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print('PASS' if report['passed'] else 'FAIL', len(report['assets']), 'assets,',
          sum(ref['travelEntries'] for x in report['assets'] for ref in x.get('references', [])), 'travel-entry comparisons')
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
