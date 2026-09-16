#!/usr/bin/env python3
"""Compare MP limbo history against original RTCW and iortcw source.

Reference functions and GPLv3 notices stay in disposable fixtures. The corpus
never invokes the references' negative-index or colour-expansion overflows.
Those malformed cases are covered by the engine's checked-boundary tests.
"""
from pathlib import Path
import argparse
import datetime
import hashlib
import json
import os
import re
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]

PRELUDE = r'''
#include "code/compat/rtcw_limbo_chat.hpp"
#include <cassert>
#include <cctype>
#include <climits>
#include <cstdio>
#include <string>
#undef Q_IsColorString
'''

COMPARE = r'''
static unsigned randomState=0x19810917;
static unsigned randomWord(){randomState^=randomState<<13;randomState^=randomState>>17;randomState^=randomState<<5;return randomState;}
int main(){
 rtcwLimboChat_t history{};int submissions=0,reads=0,colorDifferences=0;
 auto check=[&](const std::string &text,bool expectDifference=false){
  assert(text.size()<=420); // Both reference arrays have room for this corpus.
  fntech3::rtcw::limbo::Append(history,[&](std::size_t i){assert(i<=text.size());return text[i];});
  original::CL_AddToLimboChat(text.c_str());iortcw::CL_AddToLimboChat(text.c_str());
  for(int row=0;row<7;++row){
   assert(!std::strcmp(history.lines[row],original::cl.limboChatMsgs[row]));
   if(!expectDifference)assert(!std::strcmp(history.lines[row],iortcw::cl.limboChatMsgs[row]));
   char actual[142],old[142],io[142];std::memset(actual,0x55,142);std::memset(old,0x55,142);std::memset(io,0x55,142);
   assert(fntech3::rtcw::limbo::Get(history,row,actual+1));
   assert(original::CL_GetLimboString(row,old+1));assert(iortcw::CL_GetLimboString(row,io+1));
   assert(!std::memcmp(actual,old,142));if(!expectDifference)assert(!std::memcmp(actual,io,142));
   ++reads;
  }
  if(expectDifference){assert(std::strcmp(history.lines[0],iortcw::cl.limboChatMsgs[0]));++colorDifferences;}
  for(int row:{7,8,INT_MAX}){
   char actual[140],old[140],io[140];std::memset(actual,0x55,140);std::memset(old,0x55,140);std::memset(io,0x55,140);
   assert(!fntech3::rtcw::limbo::Get(history,row,actual));
   assert(!original::CL_GetLimboString(row,old));assert(!iortcw::CL_GetLimboString(row,io));
   assert(!std::memcmp(actual,old,140)&&!std::memcmp(actual,io,140));++reads;
  }
  ++submissions;
 };
 auto reset=[&]{history={};original::cl={};iortcw::cl={};};
 for(int round=0;round<8;++round){
  reset();
  for(int n=0;n<1024;++n){
   std::string text;const auto length=randomWord()%421;
   while(text.size()<length){
    const auto value=randomWord();
    if(value%5==0 && text.size()+2<=length){text+='^';text+="0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"[value%62];}
    else {char ch=char(32+value%95);text+=ch=='^'?' ':ch;}
   }
   check(text);
  }
 }
 for(int length:{0,1,139,140,141,419,420}){reset();check(std::string(length,'x'));check("");check("^1red^^caret^");}
 // iortcw deliberately narrows colour escapes to alphanumeric bytes. Pin
 // that divergence rather than changing the original RTCW contract to match.
 for(int value=1;value<256;++value){
  reset();std::string text="^";text+=char(value);text+=std::string(140,'x');
  const bool differs=value!='^'&&!std::isalnum(static_cast<unsigned char>(value));
  check(text,differs);
 }
 std::printf("{\"submissionComparisons\":%d,\"rowReadComparisons\":%d,\"iortcwColorDifferences\":%d}\n",submissions,reads,colorDifferences);
}
'''


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2] + '\n'


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def fixture(name, root, original):
    client = root / 'client/cl_main.c'
    types = root / 'client/client.h'
    shared = root / ('game/q_shared.h' if original else 'qcommon/q_shared.c')
    client_text = client.read_text(encoding='utf-8', errors='replace')
    shared_text = shared.read_text(encoding='utf-8', errors='replace')
    source = ''.join(text[:text.index('*/') + 2] + '\n' for text in (client_text, shared_text))
    source += f'namespace {name} {{\n'
    for key in ('LIMBOCHAT_WIDTH', 'LIMBOCHAT_HEIGHT'):
        source += re.search(r'^#define\s+' + key + r'\s+\d+.*$', types.read_text(), re.M)[0] + '\n'
    source += 'static struct {char limboChatMsgs[LIMBOCHAT_HEIGHT][LIMBOCHAT_WIDTH*3+1];int limboChatPos;}cl;\n'
    if original:
        source += re.search(r'^#define Q_IsColorString.*$', shared_text, re.M)[0] + '\n'
    else:
        source += function(shared_text, 'qboolean Q_IsColorString(')
    source += function(client_text, 'void CL_AddToLimboChat(')
    source += function(client_text, 'qboolean CL_GetLimboString(')
    source += '\n}\n#undef Q_IsColorString\n#undef LIMBOCHAT_WIDTH\n#undef LIMBOCHAT_HEIGHT\n'
    return source, [client, types, shared]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mp-source', type=Path, required=True)
    parser.add_argument('--iortcw', type=Path, required=True)
    parser.add_argument('--compiler', default=shutil.which('clang++'))
    parser.add_argument('--target')
    parser.add_argument('--sanitize', action='store_true')
    parser.add_argument('--report', type=Path, required=True)
    args = parser.parse_args()
    report_path = args.report.resolve(); report_path.relative_to(ROOT)
    report_path.parent.mkdir(parents=True, exist_ok=True)
    source = PRELUDE; sources = []
    for name, root, original in (('original', args.mp_source / 'src', True),
                                  ('iortcw', args.iortcw / 'MP/code', False)):
        text, paths = fixture(name, root, original); source += text; sources += paths
    source += COMPARE
    target = [f'--target={args.target}'] if args.target else ([] if os.name == 'nt' else ['-m32'])
    if os.name == 'nt' and not args.target:
        target += ['-DFNTECH3_Q3_ONLY']
    flags = ['-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-g'] if args.sanitize else []
    report = dict(utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
        scope='MP limbo history and UI bytes, with explicit iortcw colour differences; no UI rendering evidence',
        target=subprocess.check_output([args.compiler, *target, '-dumpmachine'], cwd=ROOT, text=True).strip(),
        sanitized=args.sanitize, implementationSha256=sha(ROOT / 'code/compat/rtcw_limbo_chat.hpp'),
        referenceSources=[dict(path=str(path), sha256=sha(path)) for path in sources])
    with tempfile.TemporaryDirectory(prefix='limbo-reference-', dir=report_path.parent) as directory:
        folder = Path(directory); cpp = folder / 'probe.cpp'; exe = folder / 'probe.exe'
        cpp.write_text(source, encoding='utf-8')
        command = [args.compiler, *target, *flags, '-std=c++17', '-O2', '-I', str(ROOT), str(cpp), '-o', str(exe)]
        built = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=60)
        report.update(compileCommand=command, buildExitCode=built.returncode)
        if built.returncode:
            report['buildOutput'] = built.stdout + built.stderr
        else:
            options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
            run = subprocess.run([str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=30, **options)
            report.update(exitCode=run.returncode, stderr=run.stderr)
            if not run.returncode: report.update(json.loads(run.stdout))
            else: report['stdout'] = run.stdout
        report['passed'] = not built.returncode and report.get('exitCode') == 0
    report_path.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print('PASS' if report['passed'] else 'FAIL', report.get('submissionComparisons', 0), 'submissions,',
          report.get('rowReadComparisons', 0), 'row reads,', report.get('iortcwColorDifferences', 0), 'iortcw differences')
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
