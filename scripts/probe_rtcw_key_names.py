#!/usr/bin/env python3
"""Compare fnTech3's RTCW module key names with the original engines and retail.

1. The original SP and MP `Key_KeynumToString` and their five name tables are
   read from local reference trees and compiled unchanged as C, only inside a
   disposable fixture, next to fnTech3's `fntech3::rtcw::keys::NameForModule`.
   For every key number from -3 to 300 and every cl_language from -2 to 6, in
   both modes, the translated result (the one Key_KeynumToStringBuf gives the UI
   and cgame) must be byte-identical.
2. Every name in code/compat/rtcw_keynames.def must be present as a
   NUL-terminated string in the matching retail executable: the SP columns in
   WolfSP.exe and the MP columns in WolfMP.exe.

No reference text is incorporated into fnTech3.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tests'))
from compiler_support import compiler_target_args

REFERENCE = r'''
#include <stddef.h>
#define qboolean int
#define qtrue 1
#define qfalse 0
typedef enum { LANGUAGE_FRENCH = 0, LANGUAGE_GERMAN, LANGUAGE_ITALIAN, LANGUAGE_SPANISH, MAX_LANGUAGES } languages_t;
typedef struct { int integer; } cvar_t;
typedef struct { char *name; int keynum; } keyname_t;
#define keynames PREFIX_keynames
#define keynames_d PREFIX_keynames_d
#define keynames_f PREFIX_keynames_f
#define keynames_s PREFIX_keynames_s
#define keynames_i PREFIX_keynames_i
#define Key_KeynumToString PREFIX_Key_KeynumToString
#define cl_language PREFIX_cl_language
static cvar_t language_storage;
cvar_t *cl_language = &language_storage;
#include "keycodes.h"
TABLES
FUNCTION
const char *PREFIX_name( int key, int language ) {
	language_storage.integer = language;
	return Key_KeynumToString( key, qtrue );
}
'''

OURS = r'''
#include "code/compat/rtcw_key_names.hpp"
#include <cstdio>
#include <cstring>
extern "C" const char *sp_name( int key, int language );
extern "C" const char *mp_name( int key, int language );
int main() {
	unsigned long long compared = 0;
	char tiny[5];
	for ( int mode = 0; mode < 2; ++mode ) {
		for ( int language = -2; language <= 6; ++language ) {
			for ( int key = -3; key <= 300; ++key ) {
				const char *theirs = mode ? mp_name( key, language ) : sp_name( key, language );
				const char *ours = fntech3::rtcw::keys::NameForModule( key, language, mode != 0, tiny );
				if ( std::strcmp( theirs, ours ) ) {
					std::printf( "{\"mismatch\":{\"mode\":%d,\"language\":%d,\"key\":%d}}\n", mode, language, key );
					return 1;
				}
				++compared;
			}
		}
	}
	std::printf( "{\"compared\":%llu}\n", compared );
}
'''


def reference_unit(source, prefix):
    text = source.read_text(encoding='latin-1')
    # Some tables close with a comment ("};  //end french").
    tables = ''.join(re.search(r'keyname_t %s\[\]\s*=.*?\n\};[^\n]*\n' % name, text, re.S).group(0)
                     for name in ('keynames', 'keynames_d', 'keynames_f', 'keynames_s', 'keynames_i'))
    start = text.index('char *Key_KeynumToString( int keynum, qboolean bTranslate ) {')
    function = text[start:text.index('\n}\n', start) + 3]
    return REFERENCE.replace('TABLES', tables).replace('FUNCTION', function).replace('PREFIX', prefix)


def retail_strings(installation):
    names = re.findall(r'FNTECH3_RTCW_KEYNAME\((\w+), (.*)\)', (ROOT / 'code/compat/rtcw_keynames.def').read_text())
    executables = {mode: (installation / f'Wolf{mode}.exe').read_bytes() for mode in ('SP', 'MP')}
    missing, checked = [], 0
    for key, row in names:
        literals = re.findall(r'((?:"(?:[^"\\]|\\.)*"\s*)+)', row)
        values = []
        for literal in literals:
            parts = re.findall(r'"((?:[^"\\]|\\.)*)"', literal)
            joined = ''.join(parts)
            values.append(bytes(joined.encode('ascii').decode('unicode_escape'), 'latin-1'))
        english, french, german_sp, german_mp, italian, spanish = values
        for mode, column in (('SP', [english, french, german_sp, italian, spanish]),
                             ('MP', [english, french, german_mp, italian, spanish])):
            for value in column:
                checked += 1
                if value + b'\0' not in executables[mode]:
                    missing.append(dict(mode=mode, key=key, name=value.decode('latin-1')))
    return dict(rows=len(names), checked=checked, missing=missing,
                executables={mode: hashlib.sha256(data).hexdigest() for mode, data in executables.items()})


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--sp-reference', type=Path, required=True, help='RTCW-SP src tree')
    parser.add_argument('--mp-reference', type=Path, required=True, help='RTCW-MP src tree')
    parser.add_argument('--installation', type=Path, required=True, help='retail RTCW root with WolfSP.exe/WolfMP.exe')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    compiler_c = shutil.which('clang') or shutil.which('gcc')
    compiler = shutil.which('clang++') or shutil.which('g++')
    if not compiler or not compiler_c:
        raise SystemExit('C and C++ compilers are required')
    target = compiler_target_args(compiler)
    if os.name == 'nt' and not target:
        target = ['-DFNTECH3_Q3_ONLY']
    report = dict(implementation=dict(header='code/compat/rtcw_key_names.hpp', table='code/compat/rtcw_keynames.def',
                                      sha256=hashlib.sha256((ROOT / 'code/compat/rtcw_keynames.def').read_bytes()).hexdigest()))
    (ROOT / '.tmp').mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='rtcw-key-names-', dir=ROOT / '.tmp') as folder:
        work = Path(folder)
        objects = []
        for prefix, tree in (('sp', args.sp_reference), ('mp', args.mp_reference)):
            source = work / f'{prefix}.c'
            source.write_text(reference_unit(tree / 'client' / 'cl_keys.c', prefix), encoding='latin-1')
            obj = work / f'{prefix}.o'
            built = subprocess.run([compiler_c, *[t for t in target if t != '-DFNTECH3_Q3_ONLY'], '-w', '-c',
                                    '-I', str(tree / 'ui'), str(source), '-o', str(obj)],
                                   capture_output=True, text=True, timeout=120)
            if built.returncode:
                raise SystemExit(built.stdout + built.stderr)
            objects.append(obj)
            report[f'{prefix}Reference'] = dict(path=str(tree / 'client' / 'cl_keys.c'), sha256=hashlib.sha256(
                (tree / 'client' / 'cl_keys.c').read_bytes()).hexdigest())
        main_source = work / 'compare.cpp'
        main_source.write_text(OURS, encoding='utf-8')
        binary = work / ('compare.exe' if os.name == 'nt' else 'compare')
        built = subprocess.run([compiler, *target, '-std=c++17', '-w', '-I', str(ROOT), str(main_source),
                                *map(str, objects), '-o', str(binary)], capture_output=True, text=True, timeout=120)
        if built.returncode:
            raise SystemExit(built.stdout + built.stderr)
        ran = subprocess.run([str(binary)], capture_output=True, text=True, timeout=120)
        report['comparison'] = json.loads(ran.stdout.strip().splitlines()[-1]) if ran.stdout.strip() else {}
        report['comparison']['exitCode'] = ran.returncode
        report['comparison']['target'] = target or ['default']
    report['retail'] = retail_strings(args.installation.resolve(strict=True))
    report['passed'] = bool(report['comparison'].get('exitCode') == 0 and 'compared' in report['comparison']
                            and not report['retail']['missing'])
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({k: v for k, v in report.items() if k != 'implementation'}, indent=2)[:1500])
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
