#!/usr/bin/env python3
"""Compare fnTech3's Elite Force module key names with the retail engines.

stvoy.exe and stvoyHM.exe 1.20 hand their modules translated key names: the
UI's key-name import calls Key_KeynumToString with translation on (stvoy.exe
0x406bd0, stvoyHM.exe 0x40b830). That function returns a printable character
as itself, then looks the key number up in one of three {name, keynum} tables
(English, or German when g_language is "deutsch", or French when it is
"francais", compared without regard to ASCII case), then falls back to "0x"
and two lowercase hex digits; -1 is "<KEY NOT FOUND>" and a number outside
0-255 "<OUT OF RANGE>".

This probe reads the three tables from each executable, after checking that
the function loads their addresses, and compares that behaviour, for every key
number from -3 to 300 and a set of g_language values, with
fntech3::stvef::keys::NameForModule compiled in a disposable fixture. It also
checks the key number adapter (code/compat/stvef_key_adapter.h): every number
retail names in English must reach the engine's K_ constant of that name
(code/client/keycodes.h), numbers below 128 must reach themselves, the rest of
Elite Force's range (to K_AUX16, 232) must reach an engine key, and nothing
above it may. It reads a legitimate installation, writes only --output and
stores no retail bytes.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import shutil
import struct
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tests'))
from compiler_support import compiler_target_args

# Retail 1.20: the translated Key_KeynumToString and its English, German and
# French tables.
EXECUTABLES = {
    'SP': dict(file='stvoy.exe', function=0x406bd0, tables=(0x488ac8, 0x488e28, 0x489180)),
    'MP': dict(file='stvoyHM.exe', function=0x40b830, tables=(0x4c0ab0, 0x4c0e10, 0x4c1168)),
}
LANGUAGES = ['', 'deutsch', 'DEUTSCH', 'Deutsch', 'francais', 'FRANCAIS', 'english', 'american',
             'italiano', 'espanol', 'deutsch ', 'de', 'francaise']
FIRST_KEY, LAST_KEY = -3, 300

OURS = r'''
#include "code/compat/stvef_key_adapter.h"
#include "code/compat/stvef_key_names.hpp"
#include <cstdio>
static const char *languages[] = { LANGUAGES };
struct keyname_t { const char *name; int keynum; };
static const keyname_t engineKeys[] = { ENGINE_KEYS };
int main() {
	char tiny[5];
	for ( int key = FIRST_KEY; key <= LAST_KEY; ++key ) {
		std::printf( "adapter %d %d\n", key, FnTech3_STVEFKeyToInternal( key ) );
	}
	for ( const keyname_t &entry : engineKeys ) {
		std::printf( "engine %s %d\n", entry.name, entry.keynum );
	}
	for ( int mode = 0; mode < 2; ++mode ) {
		for ( unsigned language = 0; language < sizeof( languages ) / sizeof( languages[0] ); ++language ) {
			for ( int key = FIRST_KEY; key <= LAST_KEY; ++key ) {
				const char *name = fntech3::stvef::keys::NameForModule( key, languages[language], mode != 0, tiny );
				std::printf( "%d %u %d ", mode, language, key );
				for ( const unsigned char *c = reinterpret_cast<const unsigned char *>( name ); *c; ++c ) std::printf( "%02x", *c );
				std::printf( "\n" );
			}
		}
	}
}
'''


class Image:
    """Reads bytes at virtual addresses of a 32-bit PE image."""

    def __init__(self, data):
        self.data = data
        pe = struct.unpack_from('<I', data, 0x3c)[0]
        if data[pe:pe + 4] != b'PE\0\0':
            raise SystemExit('not a PE image')
        sections = struct.unpack_from('<H', data, pe + 6)[0]
        optional = struct.unpack_from('<H', data, pe + 20)[0]
        self.base = struct.unpack_from('<I', data, pe + 24 + 28)[0]
        self.sections = []
        at = pe + 24 + optional
        for _ in range(sections):
            size, address, raw_size, raw = struct.unpack_from('<IIII', data, at + 8)
            self.sections.append((address, max(size, raw_size), raw, raw_size))
            at += 40

    def read(self, va, count):
        rva = va - self.base
        for address, size, raw, raw_size in self.sections:
            if address <= rva < address + size:
                offset = rva - address
                if offset + count > raw_size:
                    raise SystemExit(f'{va:#x} is outside the file data')
                return self.data[raw + offset:raw + offset + count]
        raise SystemExit(f'{va:#x} is not mapped')

    def string(self, va):
        out = b''
        while True:
            byte = self.read(va + len(out), 1)
            if byte == b'\0':
                return out
            out += byte

    def table(self, va):
        rows = []
        while True:
            name, key = struct.unpack('<Ii', self.read(va + 8 * len(rows), 8))
            if not name:
                return rows
            rows.append((self.string(name), key))


def same_language(value, language):
    upper = lambda text: bytes(c - 32 if 97 <= c <= 122 else c for c in text.encode('latin-1'))
    return upper(value) == upper(language)


def retail_name(key, language, tables):
    if key == -1:
        return b'<KEY NOT FOUND>'
    if key < 0 or key > 255:
        return b'<OUT OF RANGE>'
    if 0x20 < key < 0x7f and key != 0x22:
        return bytes([key])
    english, german, french = tables
    table = german if same_language(language, 'deutsch') else french if same_language(language, 'francais') else english
    for name, keynum in table:
        if keynum == key:
            return name
    return ('0x%x%x' % (key >> 4, key & 15)).encode('ascii')


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--installation', type=Path, required=True, help='retail Elite Force root with stvoy.exe and stvoyHM.exe')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    compiler = shutil.which('clang++') or shutil.which('g++')
    if not compiler:
        raise SystemExit('a C++17 compiler is required')
    target = compiler_target_args(compiler)
    if os.name == 'nt' and not target:
        target = ['-DFNTECH3_Q3_ONLY']

    report = dict(implementation=dict(header='code/compat/stvef_key_names.hpp', table='code/compat/rtcw_keynames.def',
                                      sha256=hashlib.sha256((ROOT / 'code/compat/rtcw_keynames.def').read_bytes()).hexdigest()),
                  retail={})
    expected = {}
    for mode, spec in EXECUTABLES.items():
        data = (args.installation / spec['file']).read_bytes()
        image = Image(data)
        # The function must load each table's address as an immediate.
        code = image.read(spec['function'], 0x100)
        for table in spec['tables']:
            if struct.pack('<I', table) not in code:
                raise SystemExit(f'{spec["file"]}: {table:#x} is not referenced by {spec["function"]:#x}')
        tables = [image.table(va) for va in spec['tables']]
        report['retail'][mode] = dict(file=spec['file'], sha256=hashlib.sha256(data).hexdigest(),
                                      rows=[len(rows) for rows in tables])
        expected[mode] = tables

    # Retail's English names, and the key numbers it gives them.
    english = {}
    for mode in EXECUTABLES:
        for name, keynum in expected[mode][0]:
            english.setdefault(keynum, set()).add(name.decode('latin-1'))
    constants = sorted({name for names in english.values() for name in names})
    source = OURS.replace('ENGINE_KEYS', ', '.join('{ "%s", K_%s }' % (name, name) for name in constants))
    source = source.replace('LANGUAGES', ', '.join(json.dumps(language) for language in LANGUAGES))
    source = source.replace('FIRST_KEY', str(FIRST_KEY)).replace('LAST_KEY', str(LAST_KEY))
    (ROOT / '.tmp').mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='stvef-key-names-', dir=ROOT / '.tmp') as folder:
        work = Path(folder)
        (work / 'names.cpp').write_text(source, encoding='utf-8')
        binary = work / ('names.exe' if os.name == 'nt' else 'names')
        built = subprocess.run([compiler, *target, '-std=c++17', '-Wall', '-Wextra', '-I', str(ROOT),
                                str(work / 'names.cpp'), '-o', str(binary)], capture_output=True, text=True, timeout=120)
        if built.returncode:
            raise SystemExit(built.stdout + built.stderr)
        ran = subprocess.run([str(binary)], capture_output=True, text=True, timeout=120, check=True)

    compared, mismatches = 0, []
    adapter = dict(checked=0, mismatches=[])
    lines = ran.stdout.splitlines()
    engine = {line.split(' ')[1]: int(line.split(' ')[2]) for line in lines if line.startswith('engine ')}
    for line in lines:
        if line.startswith('engine '):
            continue
        if line.startswith('adapter '):
            key, internal = map(int, line.split(' ')[1:3])
            adapter['checked'] += 1
            if key in english:
                good = all(internal == engine[name] for name in english[key])
            elif 0 <= key < 128:
                good = internal == key
            elif 128 <= key <= 232:
                good = internal >= 0
            else:
                good = internal < 0
            if not good and len(adapter['mismatches']) < 20:
                adapter['mismatches'].append(dict(key=key, internal=internal, retail=sorted(english.get(key, [])),
                                                  engine={name: engine[name] for name in english.get(key, [])}))
            continue
        mode, language, key, name = (line.split(' ') + [''])[:4]
        mode = 'MP' if mode == '1' else 'SP'
        language = LANGUAGES[int(language)]
        theirs = retail_name(int(key), language, expected[mode])
        ours = bytes.fromhex(name)
        compared += 1
        if ours != theirs and len(mismatches) < 20:
            mismatches.append(dict(mode=mode, language=language, key=int(key),
                                   retail=theirs.decode('latin-1'), ours=ours.decode('latin-1')))
    report['comparison'] = dict(compared=compared, mismatches=mismatches, languages=LANGUAGES,
                                keys=[FIRST_KEY, LAST_KEY], target=target or ['default'])
    report['adapter'] = adapter
    report['passed'] = (compared == 2 * len(LANGUAGES) * (LAST_KEY - FIRST_KEY + 1) and not mismatches
                        and adapter['checked'] == LAST_KEY - FIRST_KEY + 1 and not adapter['mismatches'])
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({k: v for k, v in report.items() if k != 'implementation'}, indent=2)[:1500])
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
