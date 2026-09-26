#!/usr/bin/env python3
"""Count the fog volumes in an installed Enemy Territory, and how many have no brush.

A map compiles its world-covering fog volume with a brush number of -1. Reading
that as a brush index refuses the map, so how many installed maps carry one
decides whether the fog loader can go on treating it as an error. The
installation is only read from; nothing is extracted or written.
"""
from pathlib import Path
import argparse
import json
import struct
import zipfile

LUMP_FOGS = 12               # the Quake BSP fog lump
FOG_BYTES = 64 + 4 + 4       # shader name, brush number, visible side


def volumes(data):
    """Every fog volume in one BSP, as (shader, brushNumber, visibleSide)."""
    if len(data) < 8 + (LUMP_FOGS + 1) * 8 or data[:4] != b'IBSP':
        return None
    offset, length = struct.unpack_from('<ii', data, 8 + LUMP_FOGS * 8)
    if length < 0 or offset < 0 or offset + length > len(data) or length % FOG_BYTES:
        return None
    found = []
    for i in range(length // FOG_BYTES):
        base = offset + i * FOG_BYTES
        shader = data[base:base + 64].split(b'\0')[0].decode('latin-1')
        brush, side = struct.unpack_from('<ii', data, base + 64)
        found.append((shader, brush, side))
    return found


def probe(installation, output):
    before = {str(p.relative_to(installation)): (p.stat().st_size, p.stat().st_mtime_ns)
              for p in installation.rglob('*') if p.is_file()}
    maps, total, brushless, unreadable = [], 0, 0, 0
    for pak in sorted(installation.rglob('*.pk3')):
        try:
            archive = zipfile.ZipFile(pak)
        except (OSError, zipfile.BadZipFile):
            continue
        with archive:
            for entry in sorted(archive.namelist()):
                if not entry.lower().endswith('.bsp'):
                    continue
                found = volumes(archive.read(entry))
                if found is None:
                    unreadable += 1
                    continue
                total += len(found)
                without = [(shader, side) for shader, brush, side in found if brush == -1]
                brushless += len(without)
                maps.append({
                    'pak': pak.name, 'map': entry, 'fogs': len(found),
                    'brushless': [{'shader': shader, 'visibleSide': side}
                                  for shader, side in without],
                })
    after = {str(p.relative_to(installation)): (p.stat().st_size, p.stat().st_mtime_ns)
             for p in installation.rglob('*') if p.is_file()}
    summary = {
        'profile': 'wolfet',
        'evidence': 'Assets: installed BSP fog volumes',
        'installation': str(installation),
        'installationUnchanged': before == after,
        'maps': len(maps),
        'unreadable': unreadable,
        'fogVolumes': total,
        'brushlessVolumes': brushless,
        'mapsWithBrushlessVolume': sum(1 for m in maps if m['brushless']),
        'results': maps,
    }
    # A brushless volume is refused by a loader that reads -1 as a brush index,
    # so finding any at all is the finding.
    summary['passed'] = bool(maps) and summary['installationUnchanged'] and not unreadable
    output.mkdir(parents=True, exist_ok=True)
    (output/'result.json').write_text(json.dumps(summary, indent=2)+'\n', encoding='utf-8')
    print(json.dumps({k: v for k, v in summary.items() if k != 'results'}, indent=2))
    for record in maps:
        print('  %-40s fogs=%-3d brushless=%s' % (
            record['map'], record['fogs'],
            ', '.join(b['shader'] for b in record['brushless']) or 'none'))
    return summary['passed']


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--installation', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    arguments = parser.parse_args()
    return 0 if probe(arguments.installation, arguments.output) else 1


if __name__ == '__main__':
    raise SystemExit(main())
