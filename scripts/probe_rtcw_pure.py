#!/usr/bin/env python3
"""Compare bounded RTCW MP pak proofs with unchanged original/iortcw functions.

GPLv3 reference functions live only in disposable fixtures with their notices.
The fixture uses synthetic pak identities and one Windows x86 module path;
it does not claim peer interoperability or iortcw's multi-architecture policy.
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
from pure_profile_contract_tests import PRELUDE, CHECKS, function


REFERENCE = r'''
namespace reference {
static struct { int integer; } pureCvar{1};
static auto *sv_pure = &pureCvar;
struct client_t { int gotCP, pureAuthentic, lastSnapshotTime, nextSnapshotTime, state; const char *name; };
static int fs_fakeChkSum = 0;
static int sv_numPureVMPaths = 1;
static const char *sv_pureVMPaths[] = {"%s_mp_x86.dll"};
#define BIG_INFO_STRING 8192
#define SYS_DLLNAME_CGAME "cgame_mp_x86.dll"
#define SYS_DLLNAME_UI "ui_mp_x86.dll"
#define SYS_DLLNAME_CGAME_SHIFT 0
#define SYS_DLLNAME_UI_SHIFT 0
static const char *FS_ShiftStr(const char *name,int shift){assert(shift==0);return name;}
static int FS_FileIsInPAK(const char *name,int *sum){return ::FS_FileIsInPAK(name,sum,nullptr);}
static const char *FS_LoadedPakPureChecksums(){return "11 22 99 -2147483648";}
static void Q_strcat(char *to,int capacity,const char *from){
 const auto a=std::strlen(to),b=std::strlen(from);assert(a+b<static_cast<size_t>(capacity));std::memcpy(to+a,from,b+1);
}
static void Cmd_TokenizeString(const char *text){
 std::istringstream input(text);std::string word;arguments.clear();while(input>>word)arguments.push_back(word);
}
static void SV_SendClientSnapshot(client_t *){++snapshots;}
static void SV_DropClient(client_t *,const char *){++drops;}
'''

COMPARE = r'''
static int cases, proofCases, strictDifferences;
static void Compare(const std::string &command,bool difference=false){
 const auto own=Verify(command);const int ownDrops=drops,ownSnapshots=snapshots;
 reference::Cmd_TokenizeString(command.c_str());drops=snapshots=0;
 reference::client_t ref{0,0,0,0,CS_ACTIVE,"reference"};
 reference::SV_VerifyPaks_f(&ref);++cases;
 if(difference){assert(!own.pureAuthentic&&ref.pureAuthentic&&ownDrops==1&&!drops);++strictDifferences;}
 else assert(own.gotCP==ref.gotCP&&own.pureAuthentic==ref.pureAuthentic&&ownDrops==drops&&ownSnapshots==snapshots);
}
static std::string Join(const std::vector<std::string> &words){
 std::string text;for(const auto &word:words)text+=word+" ";return text;
}
int main(){
 q3=false;mp=true;
 for(int shared=0;shared<2;++shared)for(int mask=0;mask<8;++mask)
 for(int feed:{0,7,-1,INT32_MIN,INT32_MAX}){
  packs[0].referenced=FS_CGAME_REF|(shared?FS_UI_REF:0)|((mask&1)?FS_GENERAL_REF:0);
  packs[1].referenced=(shared?0:FS_UI_REF)|((mask&2)?FS_GENERAL_REF:0);
  packs[2].referenced=(mask&4)?FS_GENERAL_REF:0;uiChecksum=shared?11:22;
  fs_checksumFeed=sv.checksumFeed=feed;
  const std::string proof=FS_ReferencedPakPureChecksums(1023);
  assert(proof==reference::FS_ReferencedPakPureChecksums());++proofCases;
  for(int current:{17,20})for(int incoming:{16,17,19,21}){
   sv.serverId=current;
   Compare("cp "+std::to_string(incoming)+" "+proof);
   missingUi=true;Compare("cp "+std::to_string(incoming)+" "+proof);missingUi=false;
  }
  sv.serverId=17;
  const std::string valid="cp 17 "+proof;
  reference::Cmd_TokenizeString(valid.c_str());const auto original=arguments;
  for(int fault=0;fault<7;++fault){
   auto broken=original;
   if(fault==0)broken[2]="12";
   if(fault==1)broken[3]="23";
   if(fault==2){int mixed;assert(SV_ParsePureChecksum(broken.back().c_str(),mixed));broken.back()=std::to_string(mixed^1);}
   if(fault==3)broken.insert(broken.end()-1,"55");
   if(fault==4){broken.insert(broken.end()-1,"11");broken.insert(broken.end()-1,"11");}
   if(fault==5)broken[4]="%";
   if(fault==6)broken.pop_back();
   Compare(Join(broken));
  }
 }
 // The engine deliberately rejects partial integer/delimiter tokens which
 // the original atoi/first-byte parser accepts. No unsafe oversized fixture.
 uiChecksum=22;sv.serverId=sv.checksumFeedServerId=17;sv.checksumFeed=fs_checksumFeed=7;
 packs[0].referenced=FS_CGAME_REF|FS_GENERAL_REF;
 packs[1].referenced=FS_UI_REF|FS_GENERAL_REF;packs[2].referenced=FS_GENERAL_REF;
 Compare("cp 17 11junk 22 @ 11 22 99 122",true);
 Compare("cp 17 11 22 @extra 11 22 99 122",true);
 Compare("cp 17junk 11 22 @ 11 22 99 122",true);
 Compare("cp 17 11 22 @ 11tail 22 99 122",true);
 std::printf("{\"proofCases\":%d,\"verificationCases\":%d,\"intentionalStrictParserDifferences\":%d}\n",proofCases,cases,strictDifferences);
}
'''


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mp-source', type=Path, required=True)
    parser.add_argument('--iortcw-source', type=Path)
    parser.add_argument('--target')
    parser.add_argument('--sanitize', action='store_true')
    parser.add_argument('--compiler', default=shutil.which('clang++'))
    parser.add_argument('--report', type=Path, required=True)
    args = parser.parse_args()
    report_path = args.report.resolve(); report_path.relative_to(ROOT)
    report_path.parent.mkdir(parents=True, exist_ok=True)
    targets = ['--target=' + args.target] if args.target else ([] if os.name == 'nt' else ['-m32'])
    flags = ['-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-g'] if args.sanitize else []
    files = (ROOT / 'code/qcommon/files.c').read_text()
    client = (ROOT / 'code/client/cl_main.cpp').read_text()
    server = (ROOT / 'code/server/sv_client.cpp').read_text()
    public = (ROOT / 'code/qcommon/qcommon.h').read_text()
    constants = '\n'.join(re.findall(r'^#define\s+FS_(?:GENERAL|UI|CGAME)_REF\s+\S+', public, re.M))
    own = '\n'.join(function(source, signature) for source, signature in (
        (files, 'static char *FS_AddPureChecksumToken'),
        (files, 'const char *FS_ReferencedPakPureChecksums'),
        (client, 'static void CL_SendPureChecksums'),
        (server, 'static bool SV_HasDuplicateClientChecksums'),
        (server, 'static bool SV_ClientChecksumsArePure'),
        (server, 'static int SV_MixedClientChecksumFeed'),
        (server, 'static bool SV_ParsePureChecksum'),
        (server, 'static void SV_VerifyPaks_f')))
    # Reference traversal predates const-qualified searchpath pointers.
    prelude = PRELUDE.replace('const searchpath_t *next;', 'searchpath_t *next;')
    prelude = prelude.replace('static const searchpath_t *fs_searchpaths', 'static searchpath_t *fs_searchpaths')
    report = dict(utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
        scope='Synthetic RTCW MP proof/decision comparison; one Windows x86 module path; no peer interoperability',
        target=subprocess.check_output([args.compiler, *targets, '-dumpmachine'], cwd=ROOT, text=True).strip(),
        sanitized=args.sanitize, implementationSha256={path: sha(ROOT / path) for path in
            ('code/qcommon/files.c', 'code/client/cl_main.cpp', 'code/server/sv_client.cpp', 'code/qcommon/game_profile.cpp')},
        references=[])
    references = [('original', args.mp_source / 'src')]
    if args.iortcw_source:
        references.append(('iortcw', args.iortcw_source / 'MP/code'))
    for variant, root in references:
        paths = [root / 'qcommon/files.c', root / 'server/sv_client.c']
        texts = [path.read_text() for path in paths]
        notices = '\n'.join(text[:text.index('*/') + 2] for text in texts)
        reference = function(texts[0], 'const char *FS_ReferencedPakPureChecksums(')
        reference += function(texts[1], 'static void SV_VerifyPaks_f(')
        source = notices + constants + prelude + own + CHECKS[:CHECKS.index('int main()')]
        source += REFERENCE + reference + '\n}\n' + COMPARE
        row = dict(variant=variant, source=str(root), sourceSha256={str(path.relative_to(root)): sha(path) for path in paths})
        report['references'].append(row)
        with tempfile.TemporaryDirectory(prefix='pure-reference-', dir=report_path.parent) as directory:
            folder = Path(directory); cpp = folder / 'probe.cpp'; exe = folder / 'probe.exe'
            cpp.write_text(source, encoding='utf-8')
            command = [args.compiler, *targets, *flags, '-std=c++17', '-O2', '-D_CRT_SECURE_NO_WARNINGS',
                       '-I', str(ROOT), str(cpp), '-o', str(exe)]
            built = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=60)
            row.update(compileCommand=command, buildExitCode=built.returncode)
            if built.returncode:
                row['buildOutput'] = built.stdout + built.stderr
            else:
                run = subprocess.run([str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=30)
                row.update(exitCode=run.returncode, stderr=run.stderr)
                if not run.returncode:
                    row.update(json.loads(run.stdout))
                else:
                    row['stdout'] = run.stdout
            row['passed'] = not built.returncode and row.get('exitCode') == 0
        report_path.write_text(json.dumps(report, indent=2) + '\n')
        print(variant, 'PASS' if row['passed'] else 'FAIL', row.get('verificationCases', 0), 'cases', flush=True)
    return 0 if all(row['passed'] for row in report['references']) else 1


if __name__ == '__main__':
    raise SystemExit(main())
