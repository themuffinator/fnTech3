#!/usr/bin/env python3
"""Compare fnTech3's Quake III and Quake Live key numbers with the retail engines.

quake3.exe 1.32 and quakelive_steam.exe each carry the {name, keynum} table
that their Key_StringToKeynum and Key_KeynumToString search. This probe finds
each table from its MWHEELDOWN row, walks it from its first row to its NULL
terminator, and records the code that loads its address. It then compiles
code/compat/q3_key_adapter.h, code/client/keycodes.h and the client's
CL_Q3KeyString (over the engine's own name table from code/qcommon/keys.c) in
a disposable fixture and checks that:

- every number quake3.exe names reaches the engine key of that name, and that
  key goes back to the same number; numbers below 128 reach themselves, the
  rest of Quake III's range (to K_AUX16, 232) reaches an engine key, and
  nothing above it or below zero does;
- every number quakelive_steam.exe names is this engine's own number for that
  key, so Quake Live's modules take this engine's numbers unchanged;
- the keys only Quake Live names (mouse buttons 6-9), and every engine key
  above K_AUX16, are never delivered to a Quake III module.

It also compares the name a Quake III module is given for every number from -3
to 300 with the name quake3.exe gives (its table, looked up as the GPL
release's Key_KeynumToString does) and lists the differences; those are the
engine's naming choices and do not fail the probe. It reads legitimate
installations, writes only --output and stores no retail bytes.
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

FIRST_KEY, LAST_KEY = -3, 300
Q3_LAST_KEY = 232  # K_AUX16

FIXTURE = r'''
#include "code/compat/q3_key_adapter.h"
#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <cstring>
typedef int qboolean;
enum { qfalse, qtrue };
enum errorParm_t { ERR_FATAL, ERR_DROP };
static void Com_Error( errorParm_t, const char * ) {}
static void Q_strncpyz( char *dest, const char *src, int size ) {
	int i = 0;
	for ( ; i + 1 < size && src[i]; ++i ) dest[i] = src[i];
	dest[i] = '\0';
}
static int Com_sprintf( char *dest, int size, const char *format, ... ) {
	va_list list;
	va_start( list, format );
	const int length = std::vsnprintf( dest, size, format, list );
	va_end( list );
	return length;
}
static const char *Key_GetBinding( int ) { return nullptr; }
KEYNAMES
NAMES
struct engineKey_t { const char *name; int keynum; };
static const engineKey_t engineKeys[] = { ENGINE_KEYS };
int main() {
	char name[64];
	for ( int key = FIRST_KEY; key <= LAST_KEY; ++key ) {
		CL_Q3KeyString( key, name, sizeof( name ), qfalse );
		std::printf( "q3 %d %d ", key, FnTech3_Q3KeyToInternal( key ) );
		for ( const unsigned char *c = reinterpret_cast<const unsigned char *>( name ); *c; ++c ) std::printf( "%02x", *c );
		std::printf( "\n" );
	}
	for ( int key = 0; key < MAX_KEYS; ++key ) {
		std::printf( "event %d %d %d\n", key, FnTech3_Q3KeyEventFromInternal( key ), FnTech3_Q3KeyFromInternal( key ) );
	}
	for ( const engineKey_t &entry : engineKeys ) std::printf( "engine %s %d\n", entry.name, entry.keynum );
	std::printf( "last %d %d\n", K_AUX16, FNTECH3_Q3_K_AUX16 );
}
'''


class Image:
    """Reads a 32-bit PE image by virtual address."""

    def __init__(self, data):
        self.data = data
        pe = struct.unpack_from('<I', data, 0x3c)[0]
        if data[pe:pe + 4] != b'PE\0\0':
            raise SystemExit('not a PE image')
        count = struct.unpack_from('<H', data, pe + 6)[0]
        optional = struct.unpack_from('<H', data, pe + 20)[0]
        if struct.unpack_from('<H', data, pe + 24)[0] != 0x10b:
            raise SystemExit('not a 32-bit PE image')
        self.base = struct.unpack_from('<I', data, pe + 24 + 28)[0]
        self.sections = []
        at = pe + 24 + optional
        for _ in range(count):
            size, address, raw_size, raw = struct.unpack_from('<IIII', data, at + 8)
            flags = struct.unpack_from('<I', data, at + 36)[0]
            self.sections.append(dict(address=address, size=max(size, raw_size), raw=raw, raw_size=raw_size,
                                      code=bool(flags & 0x20000000)))
            at += 40

    def offset(self, va):
        rva = va - self.base
        for section in self.sections:
            if section['address'] <= rva < section['address'] + section['raw_size']:
                return section['raw'] + rva - section['address']
        return None

    def va(self, offset):
        for section in self.sections:
            if section['raw'] <= offset < section['raw'] + section['raw_size']:
                return self.base + section['address'] + offset - section['raw']
        return None

    def string(self, va):
        at = self.offset(va)
        if at is None:
            return None
        end = self.data.find(b'\0', at, at + 32)
        text = self.data[at:end] if end >= 0 else b''
        return text.decode('ascii') if text and all(32 < c < 127 for c in text) else None

    def row(self, va):
        at = self.offset(va)
        if at is None or at + 8 > len(self.data):
            return None
        pointer, keynum = struct.unpack_from('<Ii', self.data, at)
        name = self.string(pointer) if pointer else None
        return (name, keynum) if name and 0 <= keynum <= 255 else None

    def references(self, va):
        needle = struct.pack('<I', va)
        found = []
        for section in self.sections:
            if not section['code']:
                continue
            body = self.data[section['raw']:section['raw'] + section['raw_size']]
            at = body.find(needle)
            while at >= 0:
                found.append(self.base + section['address'] + at)
                at = body.find(needle, at + 1)
        return found


def key_table(path):
    """The {name, keynum} table holding MWHEELDOWN, walked to its terminator."""
    data = path.read_bytes()
    image = Image(data)
    tables = []
    at = data.find(b'\0MWHEELDOWN\0')
    while at >= 0:
        needle = struct.pack('<I', image.va(at + 1))
        entry = data.find(needle)
        while entry >= 0:
            row_va = image.va(entry)
            if row_va is not None and image.row(row_va):
                start = row_va
                while image.row(start - 8):
                    start -= 8
                rows, end = [], start
                while image.row(end):
                    rows.append(image.row(end))
                    end += 8
                terminator = struct.unpack_from('<Ii', data, image.offset(end))
                if terminator == (0, 0) and len({name for name, _ in rows}) == len(rows):
                    tables.append((start, rows))
            entry = data.find(needle, entry + 1)
        at = data.find(b'\0MWHEELDOWN\0', at + 1)
    if len(tables) != 1:
        raise SystemExit(f'{path}: expected one key name table, found {len(tables)}')
    start, rows = tables[0]
    references = image.references(start)
    if not references:
        raise SystemExit(f'{path}: no code loads the key name table at {start:#x}')
    return dict(file=path.name, sha256=hashlib.sha256(data).hexdigest(), table=f'{start:#x}', rows=len(rows),
                firstRow=list(rows[0]), codeReferences=[f'{va:#x}' for va in references]), rows


def retail_name(key, rows):
    """quake3.exe's name for a key number: the GPL release's Key_KeynumToString over its table."""
    if key == -1:
        return '<KEY NOT FOUND>'
    if key < 0 or key > 255:
        return '<OUT OF RANGE>'
    if 32 < key < 127 and key not in (ord('"'), ord(';')):
        return chr(key)
    for name, keynum in rows:
        if keynum == key:
            return name
    return '0x%02x' % key


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}\n', start) + 3]


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--quake3', type=Path, required=True, help='retail quake3.exe 1.32')
    parser.add_argument('--quakelive', type=Path, required=True, help='retail quakelive_steam.exe')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    compiler = shutil.which('clang++') or shutil.which('g++')
    if not compiler:
        raise SystemExit('a C++17 compiler is required')
    target = compiler_target_args(compiler)
    if os.name == 'nt' and not target:
        target = ['-DFNTECH3_Q3_ONLY']

    quake3, q3_rows = key_table(args.quake3)
    quakelive, ql_rows = key_table(args.quakelive)
    implementation = {}
    for relative in ('code/compat/q3_key_adapter.h', 'code/compat/rtcw_key_adapter.h', 'code/compat/rtcw_keycodes.def',
                     'code/client/keycodes.h', 'code/qcommon/keys.c', 'code/client/cl_keys.cpp'):
        # Hashed with LF line ends, as the repository stores them.
        implementation[relative] = hashlib.sha256(
            (ROOT / relative).read_bytes().replace(b'\r\n', b'\n')).hexdigest()
    report = dict(implementation=implementation, retail=dict(quake3=quake3, quakelive=quakelive))

    keys = (ROOT / 'code/qcommon/keys.c').read_text(encoding='utf-8')
    table = keys[keys.index('typedef struct {\n\tconst char *name;\n\tint keynum;\n} keyname_t;'):]
    table = table[:table.index('\n};\n') + 4] + function(keys, 'const char *Key_KeynumToString( int keynum ) {')
    names = function((ROOT / 'code/client/cl_keys.cpp').read_text(encoding='utf-8'), 'void CL_Q3KeyString(')
    constants = sorted({name for name, _ in q3_rows} | {name for name, _ in ql_rows})
    source = (FIXTURE.replace('KEYNAMES', table).replace('NAMES', names)
              .replace('ENGINE_KEYS', ', '.join('{ "%s", K_%s }' % (name, name) for name in constants))
              .replace('FIRST_KEY', str(FIRST_KEY)).replace('LAST_KEY', str(LAST_KEY)))
    (ROOT / '.tmp').mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='q3-key-numbers-', dir=ROOT / '.tmp') as folder:
        work = Path(folder)
        (work / 'keys.cpp').write_text(source, encoding='utf-8')
        binary = work / ('keys.exe' if os.name == 'nt' else 'keys')
        built = subprocess.run([compiler, *target, '-std=c++17', '-Wall', '-Wextra', '-I', str(ROOT),
                                str(work / 'keys.cpp'), '-o', str(binary)], capture_output=True, text=True, timeout=120)
        if built.returncode:
            raise SystemExit(built.stdout + built.stderr)
        ran = subprocess.run([str(binary)], capture_output=True, text=True, timeout=120, check=True)

    engine, to_internal, events, ours = {}, {}, {}, {}
    last = None
    for line in ran.stdout.splitlines():
        fields = line.split(' ')
        if fields[0] == 'engine':
            engine[fields[1]] = int(fields[2])
        elif fields[0] == 'q3':
            to_internal[int(fields[1])] = int(fields[2])
            ours[int(fields[1])] = bytes.fromhex(fields[3] if len(fields) > 3 else '').decode('latin-1')
        elif fields[0] == 'event':
            events[int(fields[1])] = (int(fields[2]), int(fields[3]))
        elif fields[0] == 'last':
            last = (int(fields[1]), int(fields[2]))

    # Quake III: numbers reach the keys quake3.exe names, and come back.
    q3_names = {}
    for name, keynum in q3_rows:
        q3_names.setdefault(keynum, set()).add(name)
    adapter = dict(checked=0, mismatches=[])
    for key in range(FIRST_KEY, LAST_KEY + 1):
        internal = to_internal[key]
        if key in q3_names:
            good = all(internal == engine[name] for name in q3_names[key]) and events[internal] == (key, key)
        elif 0 <= key < 128:
            good = internal == key
        elif 128 <= key <= Q3_LAST_KEY:
            good = internal >= 0 and events[internal] == (key, key)
        else:
            good = internal < 0
        adapter['checked'] += 1
        if not good:
            adapter['mismatches'].append(dict(key=key, internal=internal, retail=sorted(q3_names.get(key, []))))
    # Engine keys Quake III has no number for.
    reached = {internal for key, internal in to_internal.items() if internal >= 0}
    dropped = sorted(internal for internal in events if internal not in reached)
    adapter['engineKeysNotDelivered'] = dict(count=len(dropped), first=dropped[:8],
                                             quakeLiveOnly=sorted(name for name, _ in ql_rows
                                                                  if name not in {n for n, _ in q3_rows}))
    adapter['notDeliveredLeaks'] = [internal for internal in dropped if events[internal] != (-1, -1)]

    # Quake Live: every number it names is this engine's own.
    quake_live = dict(checked=len(ql_rows), mismatches=[dict(name=name, retail=keynum, engine=engine[name])
                                                        for name, keynum in ql_rows if engine[name] != keynum])

    differences = [dict(key=key, retail=retail_name(key, q3_rows), fnTech3=ours[key])
                   for key in range(FIRST_KEY, LAST_KEY + 1) if retail_name(key, q3_rows) != ours[key]]
    report['quake3Numbers'] = adapter
    report['quakeLiveNumbers'] = quake_live
    report['quake3Names'] = dict(compared=LAST_KEY - FIRST_KEY + 1, differences=differences)
    report['lastKey'] = dict(engineAux16=last[0], quake3Aux16=last[1]) if last else None
    report['target'] = target or ['default']
    report['passed'] = (not adapter['mismatches'] and not adapter['notDeliveredLeaks']
                        and adapter['checked'] == LAST_KEY - FIRST_KEY + 1
                        and not quake_live['mismatches'] and last == (236, Q3_LAST_KEY))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({k: v for k, v in report.items() if k != 'implementation'}, indent=2)[:3000])
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
