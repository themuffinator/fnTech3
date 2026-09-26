#!/usr/bin/env python3
"""Compare RTCW MP dictionaries and translated bytes with both source releases.

Original functions are compiled unchanged as C in disposable licensed fixtures.
Installed text stays in memory; reports retain only identities and counts. Each
file is tested alone and in forward/reverse overlays, followed by synthetic
duplicate, offset, empty-target, case, formatting and newline cases. This is
source-contract evidence, not proof of localized retail UI rendering.
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
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]


def function(source, signature):
    start = re.search(signature, source).start()
    return source[start:source.index('\n}', start) + 2] + '\n'


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def reference_fixture(client, shared):
    notices = '\n'.join(text[:text.index('*/') + 2] for text in (client, shared))
    source = notices + r'''
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef enum { qfalse, qtrue } qboolean;
typedef int fileHandle_t;
#define FS_READ 0
#define ERR_FATAL 0
#define S_COLOR_YELLOW "^3"
#define FILE_HASH_SIZE 1024
#define MAX_VA_STRING 32000
#define MAX_TRANS_STRING 4096
#define MAX_TOKEN_CHARS 1024
#define MAX_LANGUAGES 4
enum { LANGUAGE_FRENCH, LANGUAGE_GERMAN, LANGUAGE_ITALIAN, LANGUAGE_SPANISH };
static struct { int integer; } language, debug;
#define cl_language (&language)
#define cl_debugTranslation (&debug)
static struct { qboolean corruptedTranslationFile; char translationVersion[1024]; } cl;
static char com_token[MAX_TOKEN_CHARS];
static int com_lines, com_tokenline;
static const char *input;
static int inputBytes, opened;
static void Com_Printf(const char *format, ...) { (void)format; }
static void Com_DPrintf(const char *format, ...) { (void)format; }
static void Com_Error(int error, const char *format, ...) { (void)error; (void)format; abort(); }
static void COM_BackupParseSession(char **data) { (void)data; }
static void CL_SaveTransTable(void) { abort(); }
static char *va(const char *format, ...) {
    static char buffer[4096]; va_list args; va_start(args,format);
    vsnprintf(buffer,sizeof(buffer),format,args); va_end(args); return buffer;
}
static int FS_FOpenFileByMode(const char *name, fileHandle_t *file, int mode) {
    (void)name; assert(!opened && mode==FS_READ); opened=1; *file=1; return inputBytes;
}
static int FS_Read(void *output, int bytes, fileHandle_t file) {
    assert(opened && file==1 && bytes==inputBytes); memcpy(output,input,bytes); return bytes;
}
static void FS_FCloseFile(fileHandle_t file) { assert(opened && file==1); opened=0; }
'''
    start = client.index('typedef struct trans_s')
    source += client[start:client.index('} trans_t;', start) + len('} trans_t;')]
    source += '\nstatic trans_t *transTable[FILE_HASH_SIZE];\n'
    for signature in (r'void Q_strncpyz\(', r'int Q_stricmpn\s*\(', r'int Q_stricmp\(',
                      r'static char \*SkipWhitespace\(', r'char \*COM_ParseExt\(', r'char \*COM_Parse\('):
        source += function(shared, signature)
    for signature in (r'static trans_t\* AllocTrans\(', r'static long generateHashValue\(',
                      r'static trans_t\* LookupTrans\(', r'qboolean CL_CheckTranslationString\(',
                      r'void CL_LoadTransTable\(', r'void CL_TranslateString\('):
        source += function(client, signature)
    return source + r'''
void ReferenceClear(void) {
    int i; for(i=0;i<FILE_HASH_SIZE;++i) {
        trans_t *item=transTable[i]; while(item) { trans_t *next=item->next; free(item); item=next; }
        transTable[i]=NULL;
    }
}
int ReferenceLoad(const char *data, int bytes) {
    input=data; inputBytes=bytes; debug.integer=0; CL_LoadTransTable("in-memory.cfg");
    assert(!opened); input=NULL; return !cl.corruptedTranslationFile;
}
static const trans_t *ReferenceEntry(int index) {
    int i; for(i=0;i<FILE_HASH_SIZE;++i) {
        const trans_t *item; for(item=transTable[i];item;item=item->next)
            if(item->fromFile && index--==0) return item;
    }
    return NULL;
}
const char *ReferenceKey(int index) { const trans_t *item=ReferenceEntry(index); return item ? item->original : NULL; }
const char *ReferenceLanguage(int index,int lang) { return ReferenceEntry(index)->translated[lang]; }
float ReferenceOffset(int index,int axis) { const trans_t *item=ReferenceEntry(index); return axis ? item->y_offset : item->x_offset; }
void ReferenceTranslate(const char *text,int lang,int debugMode,char *output) {
    language.integer=lang; debug.integer=debugMode; CL_TranslateString(text,output);
}
'''


DRIVER = r'''
#include "code/compat/rtcw_translation.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif
extern "C" {
void ReferenceClear(); int ReferenceLoad(const char *,int);
const char *ReferenceKey(int); const char *ReferenceLanguage(int,int);
float ReferenceOffset(int,int); void ReferenceTranslate(const char *,int,int,char *);
}
using namespace fntech3::rtcw::translation;
static unsigned long long queries, entries;
static void CheckQuery(const Catalog &catalog,const std::string &input) {
    for(int language=0;language<=5;++language) for(int debug : {0,1,3}) {
        std::array<char,OutputBytes+2> expected; expected.fill(char(0x55));
        ReferenceTranslate(input.c_str(),language,debug,expected.data()+1);
        const auto actual=catalog.Translate(input,language,debug);
        assert(expected.front()==char(0x55) && expected.back()==char(0x55));
        assert(actual==expected.data()+1); ++queries;
    }
}
static void Compare(const Catalog &catalog) {
    // Reference translation can insert missing strings. Snapshot only loaded
    // keys before querying; do not enumerate a changing hash table by index.
    std::vector<std::string> keys;
    for(int index=0;const char *key=ReferenceKey(index);++index) {
        const auto *item=catalog.Find(key); assert(item);
        for(int language=0;language<4;++language) assert(item->languages[language]==ReferenceLanguage(index,language));
        assert(item->x==ReferenceOffset(index,0) && item->y==ReferenceOffset(index,1));
        keys.emplace_back(key); ++entries;
    }
    assert(keys.size()==catalog.Size());
    for(const auto &key:keys) {
        CheckQuery(catalog,key);
        std::string upper=key;
        for(char &c:upper) if(c>='a' && c<='z') c-=32;
        CheckQuery(catalog,upper); CheckQuery(catalog,key+"\n");
        std::string inner=key; inner.insert(inner.size()/2,1,'\n'); CheckQuery(catalog,inner);
    }
    for(const char *missing : {"", "fntech3 missing string", "Mi\nss\n", "\n\n"}) CheckQuery(catalog,missing);
}
int main() {
#ifdef _WIN32
    _setmode(_fileno(stdin),_O_BINARY);
#endif
    Catalog catalog; unsigned lengths[2], index=0;
    while(std::fread(lengths,4,2,stdin)==2) {
        if(!lengths[0]) break;
        assert(lengths[0]<=FileBytes && lengths[1]<=1);
        if(lengths[1]) { catalog.Clear(); ReferenceClear(); }
        std::string text(lengths[0],'\0'); assert(std::fread(text.data(),1,text.size(),stdin)==text.size());
        auto loaded=catalog.Load(text);
        if(!loaded.accepted) { std::fprintf(stderr,"Rejected input %u at byte %zu\n",index,loaded.offset); return 2; }
        assert(ReferenceLoad(text.data(),int(text.size())));
        const auto oldQueries=queries, oldEntries=entries; Compare(catalog);
        std::printf("{\"index\":%u,\"records\":%zu,\"dictionarySize\":%zu,\"entriesCompared\":%llu,\"queries\":%llu}\n",
            index++,loaded.records,catalog.Size(),entries-oldEntries,queries-oldQueries);
    }
    ReferenceClear();
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('installation', 'mp-source', 'iortcw-source', 'report'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--compiler', default=shutil.which('clang++'))
    parser.add_argument('--target')
    parser.add_argument('--sanitize', action='store_true')
    args = parser.parse_args()
    report_path = args.report.resolve(); report_path.relative_to(ROOT / '.tmp')
    report_path.parent.mkdir(parents=True, exist_ok=True)
    target = ['--target=' + args.target] if args.target else ([] if os.name == 'nt' else ['-m32'])
    flags = target + ['-O1']
    if args.sanitize:
        flags += ['-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-g']
    triple = subprocess.run([args.compiler, *target, '-dumpmachine'], cwd=ROOT,
                            capture_output=True, text=True, check=True).stdout.strip()
    report = {'kind': __doc__, 'dateUtc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
              'target': triple, 'compiler': args.compiler, 'sanitized': args.sanitize,
              'implementationSha256': {name: digest(ROOT / name) for name in
                                      ('code/compat/rtcw_translation.hpp', 'code/client/cl_main.cpp',
                                       'scripts/probe_rtcw_translation.py')},
              'installation': str(args.installation.resolve(strict=True)), 'assets': [], 'inputs': [], 'results': []}
    corpus = []
    for archive in sorted((args.installation / 'main').glob('*.pk3')):
        with zipfile.ZipFile(archive) as pak:
            for item in pak.infolist():
                name = item.filename.lower()
                if name != 'scripts/translation.cfg' and not (name.startswith('translations/') and name.endswith('.cfg')):
                    continue
                data = pak.read(item)
                if not data or len(data) > 4*1024*1024 or b'\0' in data:
                    parser.error('Corpus is outside the reference probe bounds')
                # Never send oversized quoted tokens or format scratch buffers
                # to the original parser. Malformed input is tested only on ours.
                for token in re.findall(rb'"([^\"]*)"', data):
                    if len(token) >= 1024 or token.count(b'%') > 40:
                        parser.error('Corpus exceeds the original token/format capacity')
                report['assets'].append({'archive': archive.name, 'name': item.filename, 'bytes': len(data),
                                         'sha256': hashlib.sha256(data).hexdigest()})
                corpus.append(data)
    if not corpus:
        parser.error('No installed MP translation files')
    payload = bytearray()

    def add(data, reset, label):
        report['inputs'].append({'kind': label, 'reset': reset, 'bytes': len(data)})
        payload.extend(struct.pack('<II', len(data), int(reset)) + data)

    for index, data in enumerate(corpus):
        add(data, True, f'individual:{index}')
    for label, data_set in [('forward', corpus), ('reverse', list(reversed(corpus)))]:
        for index, data in enumerate(data_set):
            add(data, index == 0, f'{label}:{index}')
    add(b'''/* fixture */ #version "test"
{ english "Yes" french "oui" german "ja" italian "si" spanish "si" offset -1.25 2.5 }
{ english "empty" french "" german "" italian "" spanish "" }
{ english "accent" french "caf\xe9" german "caf\xe9" italian "caf\xe9" spanish "caf\xe9" }
{ english "literal" french "line\\nnext" german "line\\nnext" italian "line\\nnext" spanish "line\\nnext" }
{ english "newline" french "oui\n" german "ja\n" italian "si\n" spanish "si\n" }
{ english "%s %d %%x" french "%s %d %%x" german "%s %d %%x" italian "%s %d %%x" spanish "%s %d %%x" }
''', True, 'synthetic')
    add(b'{ english "YES" french "dernier" german "letzte" italian "ultimo" spanish "ultimo" }\n',
        False, 'duplicate-retains-offset')
    payload.extend(bytes(8))
    references = [('original', args.mp_source / 'src/client/cl_main.c', args.mp_source / 'src/game/q_shared.c'),
                  ('iortcw', args.iortcw_source / 'MP/code/client/cl_main.c', args.iortcw_source / 'MP/code/qcommon/q_shared.c')]
    for name, client, shared in references:
        with tempfile.TemporaryDirectory(prefix='translation-reference-', dir=report_path.parent) as temporary:
            folder = Path(temporary)
            fixture = folder / 'reference.c'; fixture.write_text(reference_fixture(client.read_text(), shared.read_text()))
            driver = folder / 'driver.cpp'; driver.write_text(DRIVER)
            obj = folder / 'reference.o'; executable = folder / ('probe.exe' if os.name == 'nt' else 'probe')
            result = {'reference': name, 'sourceSha256': {str(path): digest(path) for path in (client, shared)}}
            commands = [[args.compiler, *flags, '-x', 'c', '-std=c99', '-c', str(fixture), '-o', str(obj)],
                        [args.compiler, *flags, '-std=c++17', '-I', str(ROOT), str(driver), str(obj), '-o', str(executable)]]
            result['builds'] = []
            for command in commands:
                build = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=90)
                result['builds'].append({'command': command, 'exitCode': build.returncode, 'output': build.stdout + build.stderr})
                if build.returncode:
                    break
            result['passed'] = False
            if not build.returncode:
                environment = os.environ.copy()
                if args.sanitize:
                    environment.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1', UBSAN_OPTIONS='halt_on_error=1')
                options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
                run = subprocess.run([str(executable)], input=payload, cwd=ROOT, capture_output=True,
                                     timeout=90, env=environment, **options)
                result.update(exitCode=run.returncode, diagnostics=run.stderr.decode('utf-8', errors='replace'),
                              comparisons=[json.loads(line) for line in run.stdout.splitlines()])
                result['passed'] = run.returncode == 0 and len(result['comparisons']) == len(report['inputs'])
            report['results'].append(result)
            report_path.write_text(json.dumps(report, indent=2) + '\n')
            print(name, 'PASS' if result['passed'] else 'FAIL', flush=True)
    return 0 if all(result['passed'] for result in report['results']) else 1


if __name__ == '__main__':
    raise SystemExit(main())
