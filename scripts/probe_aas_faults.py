#!/usr/bin/env python3
"""Load stock maps over damaged copies of their bot navigation, and record what the server does.

Each variant is built from a stock navigation file's own bytes and placed in
the probe's home, where the server finds it before the installation's paks;
the map is then loaded with bots on exactly as scripts/probe_retail_bot_loads.py
loads maps (one bot joins where the game has `addbot`, and `status` lists the
clients). Every index a variant damages stays in a file whose header still
matches the map, so only the loader's checks stand between it and the botlib.

  intact               the stock file, copied: the home's copy is the one loaded
  root-cycle           node 1's back child names node 1: a point query behind
                       the root plane loops at the root for ever
  child-outside        node 1's back child names a node 1,000 past the table
  plane-outside        node 1's plane is 16,777,216, far past the planes
  lump-negative        the node lump's length is -12
  face-index-zero      the first face index names face 0 (files that have faces)
  portal-zero-cluster  portal 0, the dummy an area outside every cluster is
                       routed through, names a cluster past the table

RTCW's reader already refused the first four before this change; the last two
are what it let through. RTCW's two hulls are damaged alike.

  --installation KEY=PATH  q3, ql, rtcw or stvef, as for probe_retail_bot_loads.py
"""
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import argparse
import json
import os
import struct
import subprocess
import sys
import threading
import time
import zipfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
sys.path.insert(0, str(ROOT / 'tests'))
from probe_retail_bot_loads import cases, play, record  # noqa: E402
from probe_retail_map_loads import sha256  # noqa: E402

MAPS = ('q3:q3dm1', 'ql:campgrounds', 'stvef-mp:hm_borg1', 'rtcw-sp:escape1')
VARIANTS = ('intact', 'root-cycle', 'child-outside', 'plane-outside', 'lump-negative', 'face-index-zero',
            'portal-zero-cluster')
PLANES, FACEINDEX, NODES, PORTALS, CLUSTERS = 2, 6, 10, 11, 13


def header(data):
    """The version and the header as the loaders decode it."""
    version = struct.unpack_from('<i', data, 4)[0]
    decoded = bytearray(data[:124])
    if version != 4:
        for i in range(8, 124):
            decoded[i] ^= ((i - 8) * 119) & 255
    return version, decoded


def lump(decoded, index):
    return struct.unpack_from('<ii', decoded, 12 + 8 * index)


def damaged(data, variant):
    """`data` with one number changed, or None when the file has nothing of that kind."""
    version, decoded = header(data)
    body = bytearray(data)
    nodes, node_bytes = lump(decoded, NODES)
    if variant == 'root-cycle':
        struct.pack_into('<i', body, nodes + 12 + 8, 1)
    elif variant == 'child-outside':
        struct.pack_into('<i', body, nodes + 12 + 8, node_bytes // 12 + 1000)
    elif variant == 'plane-outside':
        struct.pack_into('<i', body, nodes + 12, 1 << 24)
    elif variant == 'lump-negative':
        struct.pack_into('<i', decoded, 12 + 8 * NODES + 4, -12)
        if version != 4:
            for i in range(8, 124):
                decoded[i] ^= ((i - 8) * 119) & 255
        body[:124] = decoded
    elif variant == 'face-index-zero':
        faces, face_bytes = lump(decoded, FACEINDEX)
        if face_bytes < 4:
            return None
        struct.pack_into('<i', body, faces, 0)
    elif variant == 'portal-zero-cluster':
        portals, portal_bytes = lump(decoded, PORTALS)
        if portal_bytes < 20:
            return None
        struct.pack_into('<i', body, portals + 4, lump(decoded, CLUSTERS)[1] // 16 + 5)
    return bytes(body)


def served(installation, folder, entry):
    """The bytes the installation's filesystem serves for `entry`: loose files, then the last pak."""
    root = installation / folder
    loose = root / entry
    if loose.is_file():
        return loose.read_bytes()
    for pak in sorted((p for p in root.iterdir() if p.suffix.lower() == '.pk3'), key=lambda p: p.name.lower(),
                      reverse=True):
        with zipfile.ZipFile(pak) as archive:
            names = {name.lower(): name for name in archive.namelist()}
            if entry in names:
                return archive.read(names[entry])
    raise FileNotFoundError(entry)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--client', type=Path, required=True)
    parser.add_argument('--server', type=Path, required=True)
    parser.add_argument('--installation', action='append', default=[], metavar='KEY=PATH')
    parser.add_argument('--jobs', type=int, default=1)
    parser.add_argument('--timeout', type=float, default=90)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--compare', nargs=2, metavar=('BEFORE', 'AFTER'))
    args = parser.parse_args()
    if args.compare:
        before = json.loads(Path(args.compare[0]).read_text(encoding='utf-8'))['runs']
        after = json.loads(Path(args.compare[1]).read_text(encoding='utf-8'))['runs']
        for label in sorted(set(before) | set(after)):
            b, a = before.get(label), after.get(label)
            print(label, 'unchanged' if (b and record(b)) == (a and record(a)) else 'CHANGED')
            for name, row in (('  before', b), ('  after ', a)):
                if row:
                    print(name, row['outcome'], row['navigation'], row['botlibErrors'][:1],
                          'routing' if row['routingInitialized'] else '-', 'bot' if row['botListed'] else '-')
        return 0
    if os.name != 'nt' or not args.output:
        parser.error('Windows and --output are required')
    installations = {}
    for item in args.installation:
        key, _, path = item.partition('=')
        installations[key] = Path(path).resolve(strict=True)
    client, server = args.client.resolve(strict=True), args.server.resolve(strict=True)
    output = args.output.resolve()
    (output / 'logs').mkdir(parents=True, exist_ok=False)
    todo = []
    for case in cases(installations):
        if case['label'] not in MAPS:
            continue
        stock = {entry: served(case['basepath'], case['folder'], entry) for entry in case['navigation']}
        for variant in VARIANTS:
            files = {entry: stock[entry] if variant == 'intact' else damaged(stock[entry], variant) for entry in stock}
            if any(data is None for data in files.values()):
                continue
            todo.append((dict(case, label=f"{case['label']}/{variant}"), files))
    binaries = {p.name: sha256(p) for p in (client, server, client.parent / 'fnql_glx_x86.dll') if p.exists()}
    started = time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime())
    runs, lock = {}, threading.Lock()

    def one(item):
        case, files = item
        row = play(case, client, server, output, args.timeout, extra=files)
        row['variant'] = case['label'].rsplit('/', 1)[1]
        with lock:
            runs[case['label']] = row
            print(case['label'], row['outcome'], row['navigation'], row['botlibErrors'][:1],
                  'routing' if row['routingInitialized'] else 'NO ROUTING', 'bot' if row['botListed'] else '-',
                  row['elapsedSeconds'], flush=True)

    with ThreadPoolExecutor(max_workers=max(1, args.jobs)) as pool:
        list(pool.map(one, todo))
    after = {p.name: sha256(p) for p in (client, server, client.parent / 'fnql_glx_x86.dll') if p.exists()}
    report = dict(binaries=binaries, binariesUnchanged=binaries == after, startedUtc=started,
                  endedUtc=time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime()),
                  sourceCommit=subprocess.run(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True,
                                              capture_output=True).stdout.strip(),
                  installations={k: str(v) for k, v in installations.items()}, runs=dict(sorted(runs.items())))
    (output / 'result.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print('binaries unchanged' if report['binariesUnchanged'] else 'BINARIES CHANGED')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
