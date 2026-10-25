#!/usr/bin/env python3
"""Load damaged copies of an installed map and classify how the engine handles each.

The map is read from the installation's own paks in memory. Each variant is
written to an isolated home as maps/bspfault.bsp and loaded with `map` in a fresh
fnTech3 client, which loads its collision model, spawns the level and renders it
for 120 frames before quitting:

  lumpN-offset-past-end   lump N starts beyond the end of the file
  lumpN-offset-negative   lump N starts at -4
  lumpN-length-odd        lump N is one byte longer than a whole number of records
  lumpN-length-huge       lump N claims 2147483647 bytes
  header-version          the BSP version is one lower
  header-ident            the identifier is not IBSP
  truncated-half          the file ends halfway
  truncated-last-lump     the file ends inside the lump stored last
  tree-cycle              the last node's front child is node 0, above it: every
                          index stays inside its table
  tree-self               the last node's front child is that node itself
  tree-shared-path        down the deepest path from node 0, each node's other
                          child is the one on the path, so each subtree below is
                          shared and a walk of the whole tree doubles at every node
  fuzz-SEED-NNN-lumpN     (--fuzz) 1-64 seeded random bytes inside one lump, taking
                          the lumps in turn; never the entity string, which
                          native game modules parse

Each run ends in one of: `loaded` (the level rendered and the client quit
normally), `rejected` (an engine error was printed and the process was still
running two seconds later), `faulted` (the same, but the error is the engine's
own report of a hardware exception it caught, such as an access violation),
`crashed` (the process died with a failure status) or `hung` (none of these
within the time limit). `faulted`, `crashed` and `hung` fail.
Quake Live's retail `sv_errorExit` makes a listen server's recoverable error
fatal, and a fatal error's dialog cannot be dismissed on a private desktop, so
every run sets it to 0 and sees the loader's own verdict.
Windows only: windowed on unswitched private desktops with sound and
mouse/joystick input disabled; console commands only. Damaged copies are
deleted after each run and only their hashes kept.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import random
import re
import struct
import subprocess
import sys
import time
import zipfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tests'))
from windows_private_process import WindowsPrivateProcess

GAMES = {'q3': 'baseq3', 'ql': 'baseq3', 'rtcw-sp': 'main', 'rtcw-mp': 'main', 'stvef-sp': 'baseEF',
         'stvef-mp': 'baseEF'}
# Record sizes of the Q3/RTCW lumps; QL's BSP 47 adds an 18th lump of 128-byte advertisements.
RECORDS = [1, 72, 16, 36, 48, 4, 4, 40, 12, 8, 44, 4, 72, 104, 128 * 128 * 3, 8, 1]
LOADED = 'BSP_FAULT_LOADED'
ERROR = re.compile(r'^ERROR: .*$', re.M)
# win_main.cpp's exception filter reports a caught hardware exception this way.
FAULT = 'Unhandled exception caught'
OUTCOMES = ('loaded', 'rejected', 'faulted', 'crashed', 'hung')


def map_from_paks(installation, game, name):
    member = f'maps/{name}.bsp'
    for pak in sorted((installation / game).glob('*.pk3'), reverse=True):
        with zipfile.ZipFile(pak) as archive:
            for entry in archive.namelist():
                if entry.lower() == member.lower():
                    return archive.read(entry), f'{pak.name}:{entry}'
    raise SystemExit(f'{member} not found in {installation / game}')


def lumps(data, profile):
    ident, version = struct.unpack_from('<4si', data, 0)
    if ident != b'IBSP':
        raise SystemExit('not an IBSP map')
    count = 18 if profile == 'ql' else 17
    return version, [struct.unpack_from('<ii', data, 8 + 8 * i) for i in range(count)]


def with_lump(data, index, offset, length):
    out = bytearray(data)
    struct.pack_into('<ii', out, 8 + 8 * index, offset, length)
    return bytes(out)


def with_children(data, table, changes):
    """The map with the children of some nodes replaced: {node: (front, back)}."""
    out = bytearray(data)
    for node, children in changes.items():
        struct.pack_into('<2i', out, table[3][0] + RECORDS[3] * node + 4, *children)
    return bytes(out)


def deepest_path(data, table):
    """Each node's children, and the nodes down the longest path from node 0."""
    offset, length = table[3]
    children = [struct.unpack_from('<2i', data, offset + RECORDS[3] * node + 4) for node in range(length // RECORDS[3])]
    height, stack = {}, [(0, False)]
    while stack:
        node, below = stack.pop()
        if below:
            height[node] = 1 + max((height[child] for child in children[node] if child >= 0), default=0)
        else:
            stack.append((node, True))
            stack.extend((child, False) for child in children[node] if child >= 0)
    path, node = [], 0
    while node >= 0:
        path.append(node)
        node = max((child for child in children[node] if child >= 0), key=height.get, default=-1)
    return children, path


def variants(data, profile, fuzz, seed):
    version, table = lumps(data, profile)
    records = RECORDS + [128]
    out = {}
    for index, (offset, length) in enumerate(table):
        out[f'lump{index}-offset-past-end'] = with_lump(data, index, len(data) + 16, length)
        out[f'lump{index}-offset-negative'] = with_lump(data, index, -4, length)
        if records[index] > 1 and length > 0:
            out[f'lump{index}-length-odd'] = with_lump(data, index, offset, length + 1)
        out[f'lump{index}-length-huge'] = with_lump(data, index, offset, 0x7fffffff)
    out['header-version'] = data[:4] + struct.pack('<i', version - 1) + data[8:]
    out['header-ident'] = b'XBSP' + data[4:]
    out['truncated-half'] = data[:len(data) // 2]
    last_offset, last_length = max(table, key=lambda lump: lump[0])
    out['truncated-last-lump'] = data[:last_offset + max(1, last_length // 2)]
    # Node trees whose every index stays inside its table: no descent of a
    # cycle ends, and a share at every node doubles a walk of the whole tree.
    children, path = deepest_path(data, table)
    last = len(children) - 1
    out['tree-cycle'] = with_children(data, table, {last: (0, children[last][1])})
    out['tree-self'] = with_children(data, table, {last: (last, children[last][1])})
    out['tree-shared-path'] = with_children(data, table, {node: (below, below) for node, below in zip(path, path[1:])})
    # Each fuzz variant damages one lump, in turn, so lumps the collision loader
    # checks first cannot hide the ones only the renderer reads.
    rng = random.Random(seed)
    spans = [(index, offset, length) for index, (offset, length) in enumerate(table) if index != 0 and length > 0]
    for n in range(fuzz):
        buffer = bytearray(data)
        index, offset, length = spans[n % len(spans)]
        for _ in range(rng.randint(1, 64)):
            buffer[offset + rng.randrange(length)] = rng.randrange(256)
        out[f'fuzz-{seed}-{n:03d}-lump{index}'] = bytes(buffer)
    return version, table, out


def stop(process):
    try:
        process.close()
    except subprocess.TimeoutExpired:
        process.wait(60)
        process.close()


def play(client, installation, profile, name, payload, output):
    home = output / f'{name}-home'
    game = home / GAMES[profile]
    (game / 'maps').mkdir(parents=True)
    bsp = game / 'maps' / 'bspfault.bsp'
    bsp.write_bytes(payload)
    load = 'map bspfault ffa' if profile == 'ql' else 'map bspfault'
    (game / 'probe.cfg').write_text('set sv_pure 0\nset bot_enable 0\nset cl_allowDownload 0\nset com_introplayed 1\n'
                                    f'{load}\nwait 120\necho {LOADED}\nquit\n', encoding='utf-8')
    command = [str(client)]
    for key, value in dict(com_gameProfile=profile, fs_basepath=str(installation), fs_homepath=str(home), fs_game='',
                           r_fullscreen='0', r_mode='3', in_mouse='0', in_joystick='0', in_nograb='1',
                           s_initsound='0', com_steamIntegration='0', fs_skipWorkshop='1', cl_webuiEnable='0',
                           cl_renderer='glx', logfile='2', net_port='0', sv_errorExit='0', sv_master1='',
                           sv_master2='', sv_master3='').items():
        command += ['+set', key, value]
    command += ['+exec', 'probe.cfg']
    log = game / 'qconsole.log'
    read = lambda: log.read_text(encoding='utf-8', errors='replace') if log.exists() else ''
    begin = time.monotonic()
    process = WindowsPrivateProcess(command, client.parent, output / f'{name}-stdout.log')
    rejected_at, alive_after = None, False
    try:
        while process.poll() is None and time.monotonic() - begin < 90:
            if rejected_at is None and ERROR.search(read()):
                rejected_at = time.monotonic()
            if rejected_at is not None and time.monotonic() - rejected_at >= 2:
                alive_after = process.poll() is None
                break
            time.sleep(0.1)
        forced = process.poll() is None
        code = process.poll()
    finally:
        stop(process)
    text = read()
    (output / f'{name}-console.log').write_text(text, encoding='utf-8')
    bsp.unlink()
    errors = ERROR.findall(text)
    if not forced and code != 0:
        outcome = 'crashed'
    elif code == 0 and LOADED in text and not errors:
        outcome = 'loaded'
    elif errors and alive_after:
        outcome = 'faulted' if FAULT in errors[0] else 'rejected'
    else:
        outcome = 'hung'
    return dict(outcome=outcome, exitCode=code, forcedTermination=forced,
                elapsedSeconds=round(time.monotonic() - begin, 2), errors=errors[:3],
                inputBytes=len(payload), inputSha256=hashlib.sha256(payload).hexdigest())


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--client', type=Path, required=True)
    parser.add_argument('--installation', type=Path, required=True)
    parser.add_argument('--profile', choices=tuple(GAMES), required=True)
    parser.add_argument('--map', required=True, help='map name inside the paks, e.g. q3dm17')
    parser.add_argument('--fuzz', type=int, default=0)
    parser.add_argument('--seed', type=int, default=20260918)
    parser.add_argument('--only', help='comma-separated variant names to run')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if os.name != 'nt':
        parser.error('This probe requires the Windows private-desktop harness')
    client, installation = args.client.resolve(strict=True), args.installation.resolve(strict=True)
    data, source = map_from_paks(installation, GAMES[args.profile], args.map)
    version, table, cases = variants(data, args.profile, args.fuzz, args.seed)
    if args.only:
        cases = {name: cases[name] for name in args.only.split(',')}
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    rows = {}
    for name, payload in cases.items():
        rows[name] = play(client, installation, args.profile, name, payload, output)
        print(name, rows[name]['outcome'], rows[name]['errors'][:1], flush=True)
    report = dict(profile=args.profile, map=source, mapBytes=len(data), mapSha256=hashlib.sha256(data).hexdigest(),
                  version=version, lumps=table, fuzz=dict(count=args.fuzz, seed=args.seed),
                  clientSha256=hashlib.sha256(client.read_bytes()).hexdigest(),
                  outcomes={o: sum(r['outcome'] == o for r in rows.values()) for o in OUTCOMES},
                  variants=rows, sourceCommit=subprocess.run(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True,
                                                             capture_output=True).stdout.strip())
    report['passed'] = not any(report['outcomes'][o] for o in ('faulted', 'crashed', 'hung'))
    (output / 'result.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(report['outcomes']))
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
