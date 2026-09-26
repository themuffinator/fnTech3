#!/usr/bin/env python3
"""Compare the independent fog controller with original/iortcw transitions.

Reference functions and types are compiled only in disposable GPLv3 fixtures.
This checks controller state and far-clip decisions, not GPU rendering.
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
from rtcw_server_world_tests import function

PRELUDE = r'''
#include "code/qcommon/q_shared.h"
#include "code/renderercommon/tr_rtcw_fog.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <initializer_list>
namespace reference {
// Original C stores these boolean fields and the selected enum as integers.
using qboolean=int;
constexpr int GL_LINEAR=0x2601,GL_EXP=0x800,GL_DONT_CARE=0x1100,PRINT_ALL=0;
constexpr int RDF_SNOOPERVIEW=64;
struct { struct { int rdflags; } refdef; } backEnd;
struct { struct { int time; } refdef; struct { float zFar; } viewParms; } tr;
struct Speeds { int integer; } speeds{};
static Speeds *r_speeds=&speeds;
static void Print(int,const char*,...){}
static struct { decltype(&Print) Printf; } ri{Print};
'''

COMPARE = r'''
static fnRTCWFogState_t state;
static int commands,frames,comparisons;
static void equalFloat(float a,float b){
 assert(std::isfinite(a)&&std::isfinite(b));
 assert(std::fabs(a-b)<=0.000002f*std::fmax(1.0f,std::fmax(std::fabs(a),std::fabs(b))));
}
static void compare(){
 assert(state.selected==(int)reference::glfogNum);
 for(int i=0;i<FN_RTCW_FOG_SLOTS;++i){
  const auto &a=state.slots[i];const auto &b=reference::glfogsettings[i];
  assert(a.mode==b.mode&&a.hint==b.hint&&a.startTime==b.startTime&&a.finishTime==b.finishTime);
  assert(a.useEndForClip==b.useEndForClip&&a.registered==b.registered&&a.drawsky==b.drawsky&&a.clearscreen==b.clearscreen);
  for(int c=0;c<4;++c)equalFloat(a.color[c],b.color[c]);
  equalFloat(a.start,b.start);equalFloat(a.end,b.end);equalFloat(a.density,b.density);
#if MODE_SP
  if(a.dirty!=b.dirty)std::fprintf(stderr,"dirty mismatch: commands=%d frames=%d slot=%d actual=%d reference=%d\n",commands,frames,i,a.dirty,b.dirty);
  assert(a.dirty==b.dirty);
#endif
 }++comparisons;
}
static void reset(){
 FnRTCWFogReset(&state);std::memset(reference::glfogsettings,0,sizeof(reference::glfogsettings));
 reference::glfogNum=reference::FOG_NONE;compare();
}
static void command(int now,int slot,int a,int b,float r,float g,float blue,float density){
 assert(FnRTCWFogSet(&state,MODE_SP?qfalse:qtrue,slot,a,b,r,g,blue,density,now));
 reference::tr.refdef.time=now;reference::R_SetFog(slot,a,b,r,g,blue,density);
 ++commands;compare();
}
static void frame(int now){
 float clip=FnRTCWFogAdvance(&state,now,MODE_SP?qfalse:qtrue,
  reference::backEnd.refdef.rdflags?qtrue:qfalse,8192.0f);
 reference::tr.refdef.time=now;reference::tr.viewParms.zFar=8192;
 reference::R_SetFrameFog();equalFloat(clip,reference::tr.viewParms.zFar);
 ++frames;compare();
}
int main(){
 for(int snoop:{0,1})for(int origin:{-1000,0,1000})for(int duration:{-10,0,1,100})
 for(float from:{0.0f,0.1f,1.0f,1.1f})for(float to:{0.0f,0.1f,1.0f,1.1f})
 for(int slot=0;slot<FN_RTCW_FOG_SLOTS;++slot){
  reset();
  reference::backEnd.refdef.rdflags=snoop?reference::RDF_SNOOPERVIEW:0;
  command(origin,FN_RTCW_FOG_MAP,0,500,0.2f,0.4f,0.6f,from);
  command(origin,FN_RTCW_FOG_SWITCH,FN_RTCW_FOG_MAP,0,0,0,0,0);frame(origin+1);
  command(origin+1,slot,5,1200,0.8f,0.1f,0.3f,to);
  command(origin+1,FN_RTCW_FOG_SWITCH,slot,duration,0,0,0,0);
  for(int offset:{1,2,26,51,101,102})frame(origin+offset);
  // Restart a transition while another is active, then unregister its target.
  command(origin+102,FN_RTCW_FOG_SWITCH,FN_RTCW_FOG_MAP,200,0,0,0,0);
  frame(origin+152);
  command(origin+152,FN_RTCW_FOG_SERVER,10,800,0.6f,0.8f,0.2f,to);
  command(origin+152,FN_RTCW_FOG_SWITCH,FN_RTCW_FOG_SERVER,100,0,0,0,0);
  frame(origin+177);frame(origin+252);frame(origin+253);
  command(origin+253,FN_RTCW_FOG_TARGET,0,0,0,0,0,0);frame(origin+254);
 }
 reset();frame(0);frame(1000);
 std::printf("{\"commands\":%d,\"frames\":%d,\"stateComparisons\":%d}\n",commands,frames,comparisons);
}
'''


def typedef(text, kind, name):
    pattern = r'typedef ' + kind + r'\s*\{[^{}]*\}\s*' + name + r'\s*;'
    match = re.search(pattern, text)
    if not match:
        raise ValueError(f'Missing reference type {name}')
    return match.group(0)


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sp-source', type=Path, required=True)
    parser.add_argument('--mp-source', type=Path, required=True)
    parser.add_argument('--iortcw-source', type=Path)
    parser.add_argument('--target')
    parser.add_argument('--sanitize', action='store_true')
    parser.add_argument('--compiler', default=shutil.which('clang++'))
    parser.add_argument('--report', type=Path, required=True)
    args = parser.parse_args()
    report_path = args.report.resolve(); report_path.relative_to(ROOT)
    report_path.parent.mkdir(parents=True, exist_ok=True)
    target = [f'--target={args.target}'] if args.target else ([] if os.name == 'nt' else ['-m32'])
    flags = ['-DFNTECH3_Q3_ONLY'] if os.name == 'nt' and not args.target else []
    if args.sanitize:
        flags += ['-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-g']
    report = dict(utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
        scope='Original/iortcw fog controller and far-clip decisions; no GPU or gameplay parity',
        target=subprocess.check_output([args.compiler, *target, '-dumpmachine'], cwd=ROOT, text=True).strip(),
        sanitized=args.sanitize, implementationSha256=sha(ROOT / 'code/renderercommon/tr_rtcw_fog.h'), references=[])
    references = [('sp', 'original', args.sp_source / 'src'), ('mp', 'original', args.mp_source / 'src')]
    if args.iortcw_source:
        references += [(mode, 'iortcw', args.iortcw_source / mode.upper() / 'code') for mode in ('sp', 'mp')]
    for mode, variant, root in references:
        path = root / 'renderer/tr_main.c'; text = path.read_text()
        types_path = root / ('renderer/tr_types.h' if variant == 'iortcw' else 'cgame/tr_types.h')
        types = types_path.read_text()
        source = text[:text.index('*/')+2] + '\n' + types[:types.index('*/')+2] + '\n' + PRELUDE
        source += typedef(types, 'enum', 'glfogType_t') + '\n' + typedef(types, 'struct', 'glfog_t')
        source += '\nglfog_t glfogsettings[NUM_FOGS]; int glfogNum=FOG_NONE;\n'
        source += function(text, 'void R_SetFog(') + '\n' + function(text, 'void R_SetFrameFog(') + '\n}\n' + COMPARE
        row = dict(mode=mode, variant=variant, source=str(path), sourceSha256=sha(path), typesSha256=sha(types_path))
        report['references'].append(row)
        with tempfile.TemporaryDirectory(prefix='fog-reference-', dir=report_path.parent) as directory:
            folder = Path(directory); cpp = folder / 'probe.cpp'; exe = folder / 'probe.exe'
            cpp.write_text(source, encoding='utf-8')
            command = [args.compiler, *target, *flags, '-std=c++17', '-O2', '-D_CRT_SECURE_NO_WARNINGS',
                f'-DMODE_SP={int(mode == "sp")}', '-I', str(ROOT), str(cpp), '-o', str(exe)]
            built = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=60)
            row.update(compileCommand=command, buildExitCode=built.returncode)
            if built.returncode:
                row['buildOutput'] = built.stdout + built.stderr
            else:
                options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
                run = subprocess.run([str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=30, **options)
                row.update(exitCode=run.returncode, stderr=run.stderr)
                if not run.returncode: row.update(json.loads(run.stdout))
                else: row['stdout'] = run.stdout
            row['passed'] = not built.returncode and row.get('exitCode') == 0
        report_path.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
        print(mode, variant, 'PASS' if row['passed'] else 'FAIL', row.get('stateComparisons', 0), 'comparisons', flush=True)
    return 0 if all(row['passed'] for row in report['references']) else 1


if __name__ == '__main__':
    raise SystemExit(main())
