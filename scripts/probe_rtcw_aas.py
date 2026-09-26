#!/usr/bin/env python3
"""Probe AAS8 layouts, installed navigation files and point queries in memory.

Original GPLv3 functions/headers are compiled only in disposable reference
fixtures with notices intact. No implementation or proprietary asset is copied
into production. Reports contain metadata and comparison counts only.
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
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]

PRELUDE = r'''
#include <cstdint>
#include "code/compat/rtcw_aas_format.hpp"
#include <cassert>
#include <cstdio>
#if defined(_MSC_VER)
#define int8_t fn_probe_int8_t
#endif
#if defined(_WIN64) && !defined(__WIN64__)
#define __WIN64__ 1
#endif
#include "q_shared.h"
#undef int8_t
#include "aasfile.h"
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif
#define FNTECH3_RTCW_AAS_RECORD(type,size,alignment) static_assert(sizeof(type)==size && alignof(type)==alignment);
#define FNTECH3_RTCW_AAS_FIELD(type,member,offset,size) static_assert(offsetof(type,member)==offset && sizeof(((type*)0)->member)==size);
#include "code/compat/rtcw_aas_layout.def"
#undef FNTECH3_RTCW_AAS_RECORD
#undef FNTECH3_RTCW_AAS_FIELD
static_assert(AASVERSION==8 && AAS_LUMPS==14 && AREACONTENTS_MOVER==2048 && AREACONTENTS_DONOTENTER_LARGE==1024);
struct World { int loaded=1,numnodes=0,numplanes=0; aas_node_t *nodes=nullptr; aas_plane_t *planes=nullptr; };
static void print(int,const char*,...) {assert(false);}
static struct {void (*Print)(int,const char*,...);} botimport{print};
#define PRT_ERROR 3
#define PRT_MESSAGE 1
'''

CHECKS = r'''
int main(){
 using namespace fntech3::rtcw::aas;
#ifdef _WIN32
 _setmode(_fileno(stdin),_O_BINARY);
#endif
 unsigned char length[4];assert(fread(length,1,4,stdin)==4);unsigned bytes=U32(length);assert(bytes<64*1024*1024&&bytes>=124);
 std::vector<unsigned char> raw(bytes+1);auto *data=raw.data()+1;assert(fread(data,1,bytes,stdin)==bytes);
 aas_header_t header;std::memcpy(&header,data,sizeof(header));
 auto *h=reinterpret_cast<unsigned char*>(&header);
 for(size_t i=8;i<sizeof(header);++i)h[i]^=static_cast<unsigned char>((i-8)*119);
 View view;Error error=Parse(data,bytes,static_cast<uint32_t>(header.bspchecksum),view);
 if(error!=Error::None){printf("{\"error\":\"%s\"}\n",ErrorName(error));return 2;}
 const size_t sizes[]={sizeof(aas_bbox_t),sizeof(aas_vertex_t),sizeof(aas_plane_t),sizeof(aas_edge_t),
  sizeof(aas_edgeindex_t),sizeof(aas_face_t),sizeof(aas_faceindex_t),sizeof(aas_area_t),sizeof(aas_areasettings_t),
  sizeof(aas_reachability_t),sizeof(aas_node_t),sizeof(aas_portal_t),sizeof(aas_portalindex_t),sizeof(aas_cluster_t)};
 uint64_t records=0,queries=0;
 for(unsigned i=0;i<LumpCount;++i){assert(sizes[i]==Strides[i]&&view.Count(static_cast<Lump>(i))==header.lumps[i].filelen/sizes[i]);
  records+=view.Count(static_cast<Lump>(i));}
 std::vector<aas_node_t> decodedNodes(view.Count(Nodes)),originalNodes(view.Count(Nodes));
 std::vector<aas_plane_t> decodedPlanes(view.Count(Planes)),originalPlanes(view.Count(Planes));
 std::memcpy(originalNodes.data(),data+header.lumps[AASLUMP_NODES].fileofs,originalNodes.size()*sizeof(aas_node_t));
 std::memcpy(originalPlanes.data(),data+header.lumps[AASLUMP_PLANES].fileofs,originalPlanes.size()*sizeof(aas_plane_t));
 for(size_t i=0;i<decodedNodes.size();++i){auto *p=view.Record(Nodes,i);auto &n=decodedNodes[i];
  n.planenum=I32(p);n.children[0]=I32(p+4);n.children[1]=I32(p+8);}
 for(size_t i=0;i<decodedPlanes.size();++i){auto *p=view.Record(Planes,i);auto &plane=decodedPlanes[i];
  for(int j=0;j<3;++j)plane.normal[j]=F32(p+j*4);plane.dist=F32(p+12);plane.type=I32(p+16);}
 assert(!std::memcmp(originalNodes.data(),decodedNodes.data(),decodedNodes.size()*sizeof(aas_node_t)));
 assert(!std::memcmp(originalPlanes.data(),decodedPlanes.data(),decodedPlanes.size()*sizeof(aas_plane_t)));
 original::world.nodes=originalNodes.data();original::world.numnodes=(int)originalNodes.size();
 original::world.planes=originalPlanes.data();original::world.numplanes=(int)originalPlanes.size();
 engine::aasworld.nodes=decodedNodes.data();engine::aasworld.numnodes=(int)decodedNodes.size();
 engine::aasworld.planes=decodedPlanes.data();engine::aasworld.numplanes=(int)decodedPlanes.size();
 auto compare=[&](vec3_t point){assert(original::AAS_PointAreaNum(point)==engine::AAS_PointAreaNum(point));++queries;};
 for(size_t i=1;i<view.Count(Areas);++i){
  auto *p=view.Record(Areas,i);vec3_t point;
  for(int sample=0;sample<3;++sample){for(int j=0;j<3;++j)point[j]=F32(p+36+j*4)+(sample-1)*.03125f;compare(point);}
 }
 uint32_t random=0x12345678;
 for(int i=0;i<1024;++i){vec3_t point;for(int j=0;j<3;++j){random=random*1664525u+1013904223u;point[j]=(float)(random&32767u)-16384;}compare(point);}
 printf("{\"records\":%llu,\"areas\":%zu,\"nodes\":%zu,\"pointQueries\":%llu}\n",
  (unsigned long long)records,view.Count(Areas),view.Count(Nodes),(unsigned long long)queries);
}
'''


def function(text, signature):
    start = text.index(signature)
    return text[start:text.index('\n}', start) + 2] + '\n'


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--installation', type=Path, required=True)
    parser.add_argument('--sp-source', type=Path, required=True)
    parser.add_argument('--mp-source', type=Path, required=True)
    parser.add_argument('--iortcw-source', type=Path)
    parser.add_argument('--compiler', default=shutil.which('clang++'))
    parser.add_argument('--target')
    parser.add_argument('--sanitize', action='store_true')
    parser.add_argument('--variant', choices=('all', 'original', 'iortcw'), default='all')
    parser.add_argument('--report', type=Path, required=True)
    args = parser.parse_args()
    report_path = args.report.resolve(); report_path.relative_to(ROOT)
    report_path.parent.mkdir(parents=True, exist_ok=True)
    target = ['--target=' + args.target] if args.target else (['-m32'] if os.name != 'nt' else [])
    instrumentation = ['-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-g'] if args.sanitize else []
    report = dict(kind='CPU AAS8 layout/asset/point-query probe; not live AI or routing parity',
                  utc=datetime.datetime.now(datetime.timezone.utc).isoformat(), sanitized=args.sanitize,
                  target=subprocess.check_output([args.compiler, *target, '-dumpmachine'], cwd=ROOT, text=True).strip(),
                  installation=str(args.installation.resolve(strict=True)),
                  implementationSha256={str(p): sha(ROOT / p) for p in ('code/compat/rtcw_aas_format.hpp', 'code/botlib/be_aas_sample.cpp')},
                  assets=[], references=[])
    inputs = []
    for archive in sorted((args.installation / 'main').glob('*.pk3')):
        with zipfile.ZipFile(archive) as pak:
            for entry in pak.infolist():
                if entry.filename.lower().endswith('.aas'):
                    inputs.append((archive, entry.filename))
                    data = pak.read(entry)
                    report['assets'].append(dict(archive=archive.name, name=entry.filename,
                                                  bytes=len(data), sha256=hashlib.sha256(data).hexdigest()))
    if not inputs:
        parser.error('No AAS assets in installation/main/*.pk3')
    references = [('sp', 'original', args.sp_source / 'src'), ('mp', 'original', args.mp_source / 'src')]
    if args.iortcw_source:
        references += [(mode, 'iortcw', args.iortcw_source / mode.upper() / 'code') for mode in ('sp', 'mp')]
    engine = (ROOT / 'code/botlib/be_aas_sample.cpp').read_text(encoding='utf-8')
    for mode, variant, root in references:
        if args.variant not in ('all', variant):
            continue
        common = root / ('qcommon' if variant == 'iortcw' else 'game')
        files = [common / 'q_shared.h', root / 'botlib/aasfile.h', root / 'botlib/be_aas_sample.c']
        texts = [p.read_text(encoding='utf-8') for p in files]
        notices = '\n'.join(t[:t.index('*/') + 2] for t in [*texts, engine])
        source = notices + PRELUDE + '\nnamespace original { World world; World *aasworld=&world;\n'
        source += function(texts[-1], 'int AAS_PointAreaNum(') + '}\nnamespace engine { World aasworld;\n'
        source += function(engine, 'int AAS_PointAreaNum(') + '}\n' + CHECKS
        row = dict(mode=mode, variant=variant, files={str(p): sha(p) for p in files}, results=[])
        report['references'].append(row)
        with tempfile.TemporaryDirectory(prefix='aas8-probe-', dir=report_path.parent) as directory:
            folder = Path(directory); cpp = folder / 'probe.cpp'; exe = folder / 'probe.exe'
            cpp.write_text(source, encoding='utf-8')
            command = [args.compiler, *target, *instrumentation, '-std=c++17', '-D_CRT_SECURE_NO_WARNINGS',
                       '-I', str(ROOT), '-I', str(common), '-I', str(root / 'botlib'), str(cpp), '-o', str(exe)]
            if variant == 'iortcw' and os.name != 'nt':
                command.append('-DARCH_STRING="' + report['target'].split('-')[0] + '"')
            built = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=90)
            row['compileCommand'] = command
            row['buildExitCode'] = built.returncode
            if built.returncode:
                row['buildOutput'] = built.stdout + built.stderr
            else:
                for index, (archive, name) in enumerate(inputs):
                    with zipfile.ZipFile(archive) as pak:
                        data = pak.read(name)
                    run = subprocess.run([str(exe)], cwd=ROOT, input=struct.pack('<I', len(data)) + data,
                                         capture_output=True, timeout=30)
                    result = dict(asset=index, exitCode=run.returncode, stderr=run.stderr.decode(errors='replace'))
                    try:
                        result.update(json.loads(run.stdout))
                    except (ValueError, UnicodeDecodeError):
                        result['stdout'] = run.stdout.decode(errors='replace')
                    row['results'].append(result)
            row['passed'] = not built.returncode and all(r['exitCode'] == 0 for r in row['results'])
        report_path.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
        print(mode, variant, 'PASS' if row['passed'] else 'FAIL', len(row['results']), 'assets', flush=True)
    return 0 if all(r['passed'] for r in report['references']) else 1


if __name__ == '__main__':
    raise SystemExit(main())
