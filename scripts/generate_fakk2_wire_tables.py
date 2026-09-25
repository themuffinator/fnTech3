#!/usr/bin/env python3
"""Generate Heavy Metal: F.A.K.K.2's delta tables from retail fakk2.exe 1.02.

The disc's fakk2.exe is LaserLock-encrypted; the official 1.02 patch
(fakk2_102.exe) installs a plain one, built 2000-08-22 with the same game and
client-game libraries as the disc's. Its data holds the three tables the
wire needs, each walked here rather than transcribed:

  entityStateFields  0x004ea228  73 x { const char *name, int offset, int bits }
  playerStateFields  0x004ea598  49 x the same
  delta dictionary   0x004e6220  a count (31), then 16-byte entries from
                                 0x004e6228: a use counter and a 10-byte
                                 changed-field vector (MSG_ReadDeltaEntity
                                 0x00439e70 indexes the vectors at 0x004e622c;
                                 its writer's lookup 0x00439b20 compares ten
                                 bytes an entry)

Writes code/compat/fakk2_wire_fields.def and code/compat/fakk2_delta_patterns.def,
or with --check compares them with what the executable holds. Only the exact
1.02 build is accepted.

  python scripts/generate_fakk2_wire_tables.py --exe <path to fakk2.exe 1.02> [--check]
"""
from pathlib import Path
import argparse
import hashlib
import struct
import sys

import pefile

ROOT = Path(__file__).resolve().parents[1]
FIELDS_DEF = ROOT / 'code' / 'compat' / 'fakk2_wire_fields.def'
PATTERNS_DEF = ROOT / 'code' / 'compat' / 'fakk2_delta_patterns.def'
SHA256 = '681bc51c3343d01d772c5ae4aae7523ae4547350b243c875156e138043335785'
ENTITY_TABLE, ENTITY_COUNT = 0x004ea228, 73
PLAYER_TABLE, PLAYER_COUNT = 0x004ea598, 49
DICTIONARY = 0x004e6220
VECTOR_BYTES = 10


class Image:
    def __init__(self, path):
        pe = pefile.PE(str(path), fast_load=True)
        self.base = pe.OPTIONAL_HEADER.ImageBase
        self.data = pe.get_memory_mapped_image()

    def u32(self, va):
        return struct.unpack_from('<I', self.data, va - self.base)[0]

    def i32(self, va):
        return struct.unpack_from('<i', self.data, va - self.base)[0]

    def cstr(self, va):
        offset = va - self.base
        end = self.data.index(b'\0', offset)
        text = self.data[offset:end]
        if not text or len(text) > 64 or not all(32 <= c < 127 for c in text):
            raise ValueError(f'no field name at {va:#x}')
        return text.decode()


def names_a_field(image, va):
    try:
        image.cstr(image.u32(va))
        return True
    except (ValueError, IndexError, struct.error):
        return False


def fields(image, table, count):
    rows = []
    for i in range(count):
        va = table + 12 * i
        rows.append((image.cstr(image.u32(va)), image.i32(va + 4), image.i32(va + 8)))
    # The table ends where the engine's loop does: neither neighbour is a field.
    if names_a_field(image, table - 12) or names_a_field(image, table + 12 * count):
        raise ValueError(f'the table at {table:#x} does not hold exactly {count} fields')
    return rows


def dictionary(image):
    count = image.i32(DICTIONARY)
    if count != 31:
        raise ValueError(f'dictionary count {count}, expected 31')
    vectors = []
    for i in range(count):
        entry = DICTIONARY + 8 + 16 * i
        use_counter = image.i32(entry)
        vector = image.data[entry + 4 - image.base:entry + 4 + VECTOR_BYTES - image.base]
        if use_counter != 0:
            raise ValueError(f'entry {i} ships with use counter {use_counter}')
        vectors.append(bytes(vector))
    return vectors


def fields_text(entity, player):
    lines = [
        '/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later.',
        " * Heavy Metal: F.A.K.K.2's delta field tables, read out of retail fakk2.exe",
        ' * 1.02 (the official patch\'s plain executable, SHA-256',
        f' * {SHA256[:32]}',
        f' * {SHA256[32:]}) by',
        ' * scripts/generate_fakk2_wire_tables.py: entityStateFields at 0x004ea228',
        ' * and playerStateFields at 0x004ea598, each a run of { const char *name,',
        ' * int offset, int bits } in the order MSG_ReadDeltaEntity (0x00439e70) and',
        ' * MSG_ReadDeltaPlayerstate (0x0043a680) walk them. Offsets are into',
        " * F.A.K.K.2's own records (code/compat/fakk2_game_abi.h), not Alice's.",
        ' *',
        ' * Zero bits is a float, a positive count that many unsigned bits and a',
        ' * negative count that many signed bits, as in Alice\'s tables.',
        ' *',
        ' *   FNTECH3_FAKK2_ENTITY_FIELD( name, offset, bits )',
        ' *   FNTECH3_FAKK2_PLAYER_FIELD( name, offset, bits )',
        ' *',
        ' * See docs/fntech3/2026-09-24-fakk2-contracts.md.',
        ' * Intentionally no include guard: consumed as a table.',
        ' */',
        '',
        '#ifdef FNTECH3_FAKK2_ENTITY_FIELD',
    ]
    lines += [f'FNTECH3_FAKK2_ENTITY_FIELD( "{n}", {o}, {b} )' for n, o, b in entity]
    lines += ['#endif', '', '#ifdef FNTECH3_FAKK2_PLAYER_FIELD']
    lines += [f'FNTECH3_FAKK2_PLAYER_FIELD( "{n}", {o}, {b} )' for n, o, b in player]
    lines += ['#endif', '']
    return '\n'.join(lines)


def patterns_text(vectors, entity):
    lines = [
        '/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later.',
        " * Heavy Metal: F.A.K.K.2's entity delta dictionary, read out of retail",
        ' * fakk2.exe 1.02 by scripts/generate_fakk2_wire_tables.py: the count (31) at',
        ' * 0x004e6220, then sixteen-byte entries from 0x004e6228, each a use counter',
        ' * that ships at zero and a ten-byte changed-field vector over the',
        ' * seventy-three entity fields of fakk2_wire_fields.def.',
        ' *',
        ' * The scheme is the one Alice\'s dictionary follows; the vectors are',
        " * F.A.K.K.2's own, because its field table is. The comment under each row",
        ' * names the fields its bits select.',
        ' *',
        ' *   FNTECH3_FAKK2_DELTA_PATTERN( b0, b1, ..., b9 )',
        ' *',
        ' * See docs/fntech3/2026-09-24-fakk2-contracts.md.',
        ' * Intentionally no include guard: consumed as a table.',
        ' */',
        '',
    ]
    for index, vector in enumerate(vectors):
        row = ', '.join(f'0x{b:02x}' for b in vector)
        lines.append(f'/* {index:2d} */ FNTECH3_FAKK2_DELTA_PATTERN( {row} )')
        names = [entity[i][0] for i in range(len(entity)) if (vector[i >> 3] >> (i & 7)) & 1]
        lines.append(f'/*      {", ".join(names)} */')
    lines.append('')
    return '\n'.join(lines)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--exe', type=Path, required=True)
    parser.add_argument('--check', action='store_true', help='compare instead of writing')
    args = parser.parse_args()
    digest = hashlib.sha256(args.exe.read_bytes()).hexdigest()
    if digest != SHA256:
        sys.exit(f'{args.exe} is not fakk2.exe 1.02 (SHA-256 {digest})')
    image = Image(args.exe)
    entity = fields(image, ENTITY_TABLE, ENTITY_COUNT)
    player = fields(image, PLAYER_TABLE, PLAYER_COUNT)
    vectors = dictionary(image)
    for vector in vectors:
        if any((vector[i >> 3] >> (i & 7)) & 1 for i in range(ENTITY_COUNT, VECTOR_BYTES * 8)):
            sys.exit('a dictionary vector selects a field past the table')
    outputs = {FIELDS_DEF: fields_text(entity, player), PATTERNS_DEF: patterns_text(vectors, entity)}
    stale = []
    for path, text in outputs.items():
        if args.check:
            current = path.read_text(encoding='utf-8') if path.is_file() else ''
            if current.replace('\r\n', '\n') != text:
                stale.append(path)
        else:
            path.write_text(text, encoding='utf-8', newline='\n')
            print('wrote', path.relative_to(ROOT))
    if stale:
        sys.exit('stale: ' + ', '.join(str(p.relative_to(ROOT)) for p in stale))


if __name__ == '__main__':
    main()
