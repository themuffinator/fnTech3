#!/usr/bin/env python3
"""Read every installed map's entity text as the entity editor and the
server's `.ent` override check read it.

Each map the installations carry (scripts/probe_retail_map_loads.py finds
them) is presented as IBSP 46 through the engine's own translators
(scripts/audit_bsp_indexes.py's shim), and its entity lump is handed to
code/qcommon/entity_text.hpp, compiled into a small harness:

  - parse: whether a Quake III-line game module would read the text, its
    entity count, and whether any token is past the 1,023 bytes a module
    keeps;
  - canonical: whether writing the entities back (Serialize) gives the lump's
    own bytes, as q3map writes them;
  - stable: whether the written text reads back to the same entities, so an
    override saved from the editor holds exactly what was read;
  - worldspawn: whether the first entity is the worldspawn, as the override
    check requires.

Nothing is run and nothing is written to an installation. The report is JSON,
per map and per title.

  --installation KEY=PATH  as for probe_retail_map_loads.py
"""
from collections import Counter, defaultdict
from pathlib import Path
import argparse
import json
import os
import shutil
import struct
import subprocess
import sys
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
sys.path.insert(0, str(ROOT / 'tests'))
from compiler_support import compiler_target_args  # noqa: E402
from probe_retail_map_loads import TITLES, discover  # noqa: E402
from audit_bsp_indexes import build_shim, translate  # noqa: E402

HARNESS = r'''
#include "code/qcommon/entity_text.hpp"
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
namespace et = fntech3::entities;
int main( int argc, char **argv ) {
	for ( int i = 1; i < argc; i++ ) {
		std::ifstream file( argv[i], std::ios::binary );
		std::string text( ( std::istreambuf_iterator<char>( file ) ), std::istreambuf_iterator<char>() );
		const std::size_t nul = text.find( '\0' );
		const std::string lump = nul == std::string::npos ? text : text.substr( 0, nul );
		const et::ParseResult parsed = et::Parse( lump );
		const std::string written = et::Serialize( parsed.entities );
		const et::ParseResult again = et::Parse( written );
		const bool world = !parsed.entities.empty() && et::EqualsNoCase( parsed.entities[0].Classname(), "worldspawn" );
		std::printf( "%d\t%d\t%zu\t%d\t%d\t%d\t%d\t%d\t%s\n", i, parsed.ok ? 1 : 0, parsed.entities.size(),
			parsed.truncated ? 1 : 0, written == lump ? 1 : 0, again.ok && again.entities == parsed.entities ? 1 : 0,
			world ? 1 : 0, parsed.errorLine, parsed.error.c_str() );
	}
	return 0;
}
'''


def build_harness(directory):
    compiler = shutil.which('clang++') or shutil.which('g++')
    if not compiler:
        raise SystemExit('A C++17 compiler is required')
    source, exe = directory / 'entities.cpp', directory / 'entities.exe'
    source.write_text(HARNESS, encoding='utf-8')
    target = compiler_target_args(compiler)
    subprocess.run([compiler, *target, '-std=c++17', '-O2', '-I', str(ROOT), str(source), '-o', str(exe)],
                   check=True, cwd=ROOT)
    return exe


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--installation', action='append', default=[], metavar='KEY=PATH')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    installations = {}
    for item in args.installation:
        key, _, path = item.partition('=')
        if key not in TITLES or not path:
            parser.error(f'unknown installation {item!r}')
        installations[key] = Path(path).resolve(strict=True)
    (ROOT / '.tmp').mkdir(parents=True, exist_ok=True)
    maps, totals = {}, defaultdict(Counter)
    with tempfile.TemporaryDirectory(dir=ROOT / '.tmp') as scratch:
        scratch = Path(scratch)
        shim, harness = build_shim(scratch), build_harness(scratch)
        lumps, labels = [], []
        for title, path in sorted(installations.items()):
            roots = sorted(p for p in path.iterdir() if p.is_dir()) if title == 'q3test' else [path]
            for root in roots:
                for (folder, name), source in sorted(discover(title, root).items()):
                    where, _, entry = source.partition(':')
                    if where == 'loose':
                        data = (root / folder / entry).read_bytes()
                    else:
                        with zipfile.ZipFile(root / folder / where) as archive:
                            data = archive.read(entry)
                    label = f'{title}:{folder}/{name}' if title != 'q3test' else f'q3test-{root.name}:{name}'
                    translated, error = translate(shim, scratch, data, False, title == 'fakk2')
                    if translated is None:
                        maps[label] = dict(source=source, translationRefused=error)
                        totals[title]['translationRefused'] += 1
                        continue
                    offset, length = struct.unpack_from('<ii', translated, 8)
                    lump = scratch / f'lump{len(lumps)}.txt'
                    lump.write_bytes(translated[offset:offset + length])
                    lumps.append(lump)
                    labels.append((title, label, source, length))
        rows = []
        for start in range(0, len(lumps), 200):
            batch = lumps[start:start + 200]
            run = subprocess.run([str(harness), *map(str, batch)], capture_output=True, text=True, check=True)
            rows += run.stdout.splitlines()
    for (title, label, source, length), row in zip(labels, rows):
        _, ok, count, truncated, canonical, stable, world, line, error = (row.split('\t') + [''])[:9]
        entry = dict(source=source, bytes=length, entities=int(count), parsed=ok == '1', truncated=truncated == '1',
                     canonical=canonical == '1', stable=stable == '1', worldspawnFirst=world == '1')
        if ok != '1':
            entry.update(errorLine=int(line), error=error)
        maps[label] = entry
        counter = totals[title]
        counter['maps'] += 1
        counter['entities'] += int(count)
        for name in ('parsed', 'canonical', 'stable', 'worldspawnFirst', 'truncated'):
            counter[name] += entry[name]
    report = dict(titles={k: dict(v) for k, v in sorted(totals.items())}, maps=maps)
    args.output.write_text(json.dumps(report, indent=1) + '\n', encoding='utf-8')
    for title, counter in sorted(totals.items()):
        print(title, dict(counter), flush=True)
    unread = [label for label, entry in maps.items() if not entry.get('parsed', False)]
    unstable = [label for label, entry in maps.items() if entry.get('parsed') and not entry.get('stable')]
    print('maps the parser refuses:', len(unread), unread[:10])
    print('maps whose written text reads back differently:', len(unstable), unstable[:10])
    return 1 if unstable else 0


if __name__ == '__main__':
    raise SystemExit(main())
