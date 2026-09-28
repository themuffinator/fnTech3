#!/usr/bin/env python3
"""Write Quake III route caches with the engine's own saveroutingcache, measure them, and read them back.

The botlib keeps each route it works out in a cache, and writes them all to
maps/NAME.rcd in the home when the game sets its saveroutingcache variable
(Quake III's game does so for `bot_saveroutingcache 1`). The next load of the
map reads the file back (AAS_ReadRouteCache,
docs/fntech3/2026-09-28-route-cache-checks.md). For each map below this probe:

  1. writes a cache: the dedicated server loads the map with bots on (by
     devmap: the game registers bot_saveroutingcache as a cheat), adds
     bots, lets them play, then sets bot_saveroutingcache and quits;
  2. measures it offline against the navigation file the installation serves:
     its header, and every cache's kind, area, cluster, size and each
     reachability a route query can take, by the reader's rules written again
     here in Python;
  3. reads it back, intact and damaged, with each of two dedicated servers
     (--before and --after, built alike but for the reader): the map is
     loaded with the file in the probe's home, the bots play, and the server
     writes the cache again before it quits, so every cache read can be
     looked for, unchanged, among those it writes.

A game that never writes a dump (Elite Force's Holomatch game ignores
bot_saveroutingcache) is given two made here from its navigation file's
header instead: one without caches, which must be read, and one with a
single area cache for area 0, which must be refused.

Windows only: private desktops, networking off, console commands only.
Installations are only read.

  --installation KEY=PATH  q3, ql or stvef, as for probe_retail_map_loads.py
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import shutil
import struct
import subprocess
import sys
import time
import zipfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
sys.path.insert(0, str(ROOT / 'tests'))
from probe_retail_map_loads import CONFIGS, DROP, FATAL, FAULT, TITLES, sha256, stop  # noqa: E402

# label: (installation key, profile, game folder, map, map command, bots)
MAPS = {
    'q3:q3dm7': ('q3', 'q3', 'baseq3', 'q3dm7', 'devmap q3dm7', ('sarge', 'xaero', 'anarki', 'grunt')),
    'q3:q3dm1': ('q3', 'q3', 'baseq3', 'q3dm1', 'devmap q3dm1', ('sarge', 'xaero', 'anarki', 'grunt')),
    'q3:q3dm17': ('q3', 'q3', 'baseq3', 'q3dm17', 'devmap q3dm17', ('sarge', 'xaero')),
    'q3:q3ctf2': ('q3', 'q3', 'baseq3', 'q3ctf2', 'devmap q3ctf2', ('sarge', 'xaero', 'anarki', 'grunt')),
    'q3:missionpack/mpteam1': ('q3', 'q3', 'missionpack', 'mpteam1', 'devmap mpteam1', ('xaero',)),
    'ql:falloutbunker': ('ql', 'ql', 'baseq3', 'falloutbunker', 'devmap falloutbunker ffa', ('anarki', 'sarge')),
    'ql:campgrounds': ('ql', 'ql', 'baseq3', 'campgrounds', 'devmap campgrounds ffa', ('anarki', 'sarge')),
    'stvef-mp:hm_borg1': ('stvef', 'stvef-mp', 'baseEF', 'hm_borg1', 'devmap hm_borg1', ('alexandria',)),
}
# Damage done to the cache written for DAMAGED's map; each must be refused whole.
DAMAGED = 'q3:q3dm7'
VARIANTS = ('intact', 'area-outside-cluster', 'reachability-past-area', 'size-plus-one', 'type-swapped',
            'truncated', 'counts-past-file', 'trailing-bytes')
DONE = 'ROUTE_CACHE_PROBE_DONE'
COLOR = re.compile(r'\^[0-9A-Za-z]')
WRITTEN = re.compile(r'^route cache written to (maps/\S+\.rcd)\s*$', re.M)
WRITTEN_BYTES = re.compile(r'^written (\d+) bytes of routing cache\s*$', re.M)
READ = re.compile(r'^route cache read from (maps/\S+\.rcd): (\d+) portal and (\d+) area caches, (\d+) bytes\s*$', re.M)
REFUSED = re.compile(r'^Fatal: ((?:maps/\S+\.rcd|route cache dump).*?)\s*$', re.M)
STATUS = re.compile(r'^\s*\d+\s+-?\d+\s+(?:\d+|PRM|CON|ZMB)\s+(.*?)\s+bot\s+\d+\s+\d+\s*$', re.M)

# The x86 layout of a cache (be_aas_def.h aas_routingcache_s with 4-byte pointers).
HEADER = struct.Struct('<8i')
CACHE_BYTES, TIMES_AT = 64, 60
TYPE_PORTAL, TYPE_AREA = 0, 1
# The members a route cache keeps from the process that wrote it: time and the five pointers.
VOLATILE = (range(4, 8), range(40, 60))

# Quake III's AAS lumps and records.
AREAS, AREASETTINGS, PORTALS, CLUSTERS = 7, 8, 11, 13


def crc16(data):
    """The botlib's CRC_ProcessString: CCITT, from 0xffff, not reflected."""
    table = crc16.table
    crc = 0xffff
    for byte in data:
        crc = ((crc << 8) & 0xffff) ^ table[(crc >> 8) ^ byte]
    return crc


def _crc_table():
    table = []
    for i in range(256):
        value = i << 8
        for _ in range(8):
            value = ((value << 1) ^ (0x1021 if value & 0x8000 else 0)) & 0xffff
        table.append(value)
    return table


crc16.table = _crc_table()


def served(installation, folder, entry):
    """The bytes the filesystem serves for `entry` under `folder`: a loose file, else the last pak holding it."""
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
    raise FileNotFoundError(f'{folder}/{entry}')


def parse_aas(data):
    """The tables a route cache is checked against, from a Quake III AAS file (version 4 or 5)."""
    version = struct.unpack_from('<i', data, 4)[0]
    header = bytearray(data[:124])
    if version != 4:
        for i in range(8, 124):
            header[i] ^= ((i - 8) * 119) & 255

    def lump(index, size):
        offset, length = struct.unpack_from('<ii', header, 12 + 8 * index)
        return data[offset:offset + length], length // size

    areas, numareas = lump(AREAS, 48)
    settings, _ = lump(AREASETTINGS, 28)
    portals, numportals = lump(PORTALS, 20)
    clusters, numclusters = lump(CLUSTERS, 16)
    return dict(
        version=version, numareas=numareas, numportals=numportals, numclusters=numclusters,
        areacrc=crc16(areas[:numareas * 48]), clustercrc=crc16(clusters[:numclusters * 16]),
        # cluster, clusterareanum, numreachableareas
        settings=[struct.unpack_from('<iii', settings, i * 28 + 12) for i in range(numareas)],
        # areanum, frontcluster, backcluster, clusterareanum front, back
        portals=[struct.unpack_from('<5i', portals, i * 20) for i in range(numportals)],
        # numareas, numreachabilityareas
        clusters=[struct.unpack_from('<ii', clusters, i * 16) for i in range(numclusters)])


def cluster_number(aas, cluster, area):
    """AAS_ClusterAreaNum: an area's number in its cluster, a portal's in a cluster it joins."""
    own, number, _ = aas['settings'][area]
    if own > 0:
        return number
    portal = aas['portals'][-own]
    return portal[3] if portal[1] == cluster else portal[4]


def in_cluster(aas, area, cluster):
    own = aas['settings'][area][0]
    if own >= 0:
        return own == cluster
    if -own >= aas['numportals']:
        return False
    portal = aas['portals'][-own]
    return cluster in (portal[1], portal[2])


def records(data):
    """(offset, size) of each cache in a route cache dump, as its sizes chain them."""
    header = HEADER.unpack_from(data)
    at, out = HEADER.size, []
    for _ in range(header[6] + header[7]):
        if at + 12 > len(data):
            break
        size = struct.unpack_from('<i', data, at + 8)[0]
        out.append((at, size))
        if size <= 0:
            break
        at += size
    return out


def check(data, aas):
    """The first rule the reader refuses a dump by, in its words, or None, with what the dump holds."""
    ident, version, numareas, numclusters, areacrc, clustercrc, portals, areas = HEADER.unpack_from(data)
    if ident != 0x4352454D:
        return 'not a route cache dump', {}
    if version != 2:
        return f'version {version}', {}
    if (numareas, numclusters, areacrc, clustercrc) != (aas['numareas'], aas['numclusters'], aas['areacrc'],
                                                        aas['clustercrc']):
        return 'made for another AAS file', {}
    length = len(data) - HEADER.size
    portal_bytes = CACHE_BYTES + 3 * aas['numportals']
    if portals < 0 or areas < 0 or portals > length // portal_bytes or \
            areas > (length - portals * portal_bytes) // CACHE_BYTES:
        return f'cannot hold {portals} portal and {areas} area caches in {length} bytes', {}
    # the fewest reachabilities of the areas a route query reads each number for
    fewest = {}

    def fewer(key, count):
        fewest[key] = min(fewest.get(key, count), count)
    for area in range(1, aas['numareas']):
        cluster, _, count = aas['settings'][area]
        count = max(count, 0)
        if cluster > 0:
            joined = (cluster,)
        elif cluster < 0 and -cluster < aas['numportals']:
            fewer(('portal', -cluster), count)
            joined = aas['portals'][-cluster][1:3]
        else:
            continue
        for c in joined:
            if 0 < c < aas['numclusters']:
                number = cluster_number(aas, c, area)
                if 0 <= number < aas['clusters'][c][0]:
                    fewer((c, number), count)
    stats = dict(portalCaches=portals, areaCaches=areas, cacheBytes=length, portalCacheBytes=portal_bytes,
                 areaCacheBytes=set(), travelFlags=set(), clusters=set(), largestReachability=0,
                 nonzeroPortalReachabilities=0, portalAreaCaches=0, reachabilitiesChecked=0)
    at = HEADER.size
    for i in range(portals + areas):
        portal = i < portals
        name = f"{'portal' if portal else 'area'} cache {i if portal else i - portals}"
        if len(data) - at < CACHE_BYTES:
            return f'{name} starts {len(data) - at} bytes from the end of the file', stats
        kind = data[at]
        size, cluster, area = struct.unpack_from('<iii', data, at + 8)
        flags = struct.unpack_from('<i', data, at + 36)[0]
        if kind != (TYPE_PORTAL if portal else TYPE_AREA):
            return f'{name} has type {kind}', stats
        if not 0 < area < aas['numareas']:
            return f'{name} is for area {area}, which the map lacks', stats
        if not 0 < cluster < aas['numclusters']:
            return f'{name} is for cluster {cluster}, which the map lacks', stats
        if not in_cluster(aas, area, cluster):
            return f'{name} is for area {area}, which is not in cluster {cluster}', stats
        if not portal and not 0 <= cluster_number(aas, cluster, area) < aas['clusters'][cluster][0]:
            return f'{name} is for area {area}, numbered outside cluster {cluster}', stats
        n = aas['numportals'] if portal else aas['clusters'][cluster][1]
        if n < 0 or size != CACHE_BYTES + 3 * n:
            return f'{name} is {size} bytes, not the size of a cache of {n} travel times', stats
        if size > len(data) - at:
            return f'{name} is {size} bytes, more than the {len(data) - at} left in the file', stats
        reach = data[at + CACHE_BYTES + 2 * n:at + CACHE_BYTES + 3 * n]
        for j, value in enumerate(reach):
            stats['reachabilitiesChecked'] += 1
            if not value:
                continue
            if portal:
                stats['nonzeroPortalReachabilities'] += 1
            bound = fewest.get(('portal', j) if portal else (cluster, j), -1) \
                if portal or j < aas['clusters'][cluster][0] else -1
            if value >= bound:
                return f'{name} gives {"portal" if portal else "number"} {j} reachability {value}, which its area lacks', stats
            stats['largestReachability'] = max(stats['largestReachability'], value)
        if not portal:
            stats['areaCacheBytes'].add(size)
            stats['clusters'].add(cluster)
            stats['portalAreaCaches'] += aas['settings'][area][0] < 0
        stats['travelFlags'].add(flags)
        at += size
    if at != len(data):
        return f'has {len(data) - at} bytes after its last cache', stats
    return None, stats


def synthetic(aas, variant):
    """A dump made here, not by the engine, for a game that never writes one."""
    caches = b''
    if variant == 'area-zero':
        n = aas['clusters'][1][1]
        record = bytearray(CACHE_BYTES + 3 * n)
        record[0] = TYPE_AREA
        struct.pack_into('<iii', record, 8, len(record), 1, 0)
        caches = bytes(record)
    header = HEADER.pack(0x4352454D, 2, aas['numareas'], aas['numclusters'], aas['areacrc'], aas['clustercrc'],
                         0, 1 if caches else 0)
    expected = 'area cache 0 is for area 0, which the map lacks' if caches else None
    return header + caches, expected


def masked(record):
    """A cache's bytes without what only its writer's process means: its time and pointers."""
    out = bytearray(record)
    for span in VOLATILE:
        for i in span:
            out[i] = 0
    return bytes(out)


def damage(data, aas, variant):
    """The dump with one thing wrong, and what that is."""
    d = bytearray(data)
    header = HEADER.unpack_from(data)
    all_records = records(data)
    first_area = all_records[header[6]]
    if variant == 'area-outside-cluster':
        at = first_area[0]
        area = struct.unpack_from('<i', d, at + 16)[0]
        other = next(c for c in range(1, aas['numclusters']) if not in_cluster(aas, area, c))
        struct.pack_into('<i', d, at + 12, other)
        return bytes(d), f'area cache 0 is for area {area}, which is not in cluster {other}'
    if variant == 'reachability-past-area':
        for index, (at, size) in enumerate(all_records[header[6]:]):
            cluster = struct.unpack_from('<i', d, at + 12)[0]
            n = aas['clusters'][cluster][1]
            for j in range(n):
                if d[at + CACHE_BYTES + 2 * n + j]:
                    area = next(a for a in range(1, aas['numareas'])
                                if in_cluster(aas, a, cluster) and cluster_number(aas, cluster, a) == j)
                    count = aas['settings'][area][2]
                    d[at + CACHE_BYTES + 2 * n + j] = count
                    return bytes(d), (f'area cache {index} gives number {j} of cluster {cluster} '
                                      f'reachability {count}, which its area lacks')
    if variant == 'size-plus-one':
        at, size = all_records[0]
        struct.pack_into('<i', d, at + 8, size + 1)
        kind = 'portal' if header[6] else 'area'
        n = aas['numportals'] if header[6] else aas['clusters'][struct.unpack_from('<i', d, at + 12)[0]][1]
        return bytes(d), f'{kind} cache 0 is {size + 1} bytes, not the size of a cache of {n} travel times'
    if variant == 'type-swapped':
        at = first_area[0]
        d[at] = TYPE_PORTAL
        return bytes(d), 'area cache 0 has type 0'
    if variant == 'truncated':
        at, size = all_records[-1]
        return bytes(d[:-1]), (f'area cache {header[7] - 1} is {size} bytes, more than the {size - 1} left in the '
                               f'file')
    if variant == 'counts-past-file':
        struct.pack_into('<i', d, 28, header[7] + 1000000)
        return bytes(d), (f'cannot hold {header[6]} portal and {header[7] + 1000000} area caches in '
                          f'{len(data) - HEADER.size} bytes')
    if variant == 'trailing-bytes':
        return bytes(d) + b'\0\0\0', 'has 3 bytes after its last cache'
    raise ValueError(variant)


def play(exe, case, installation, home, output, slug, lines, timeout, place=None):
    """One server run; `place` is the dump put in the home's maps folder first."""
    from windows_private_process import WindowsPrivateProcess

    _, profile, folder, name, _, _ = case
    if home.exists():
        shutil.rmtree(home)
    title = {'q3': 'q3', 'ql': 'ql', 'stvef-mp': 'stvef'}[profile]
    game = home / TITLES[title][1]
    mod = folder if folder.lower() == 'missionpack' else ''
    for where in (game, home / mod) if mod else (game,):
        (where / 'configs').mkdir(parents=True, exist_ok=True)
        for config in CONFIGS:
            for suffix in ('.cfg', '_server.cfg'):
                (where / (config + suffix)).write_bytes(b'')
    written = (home / mod if mod else game) / 'maps' / f'{name}.rcd'
    if place is not None:
        written.parent.mkdir(parents=True, exist_ok=True)
        written.write_bytes(place)
    (game / 'routecache.cfg').write_text(''.join(line + '\n' for line in lines), encoding='utf-8')
    variables = dict(com_gameProfile=profile, fs_basepath=str(installation), fs_homepath=str(home), fs_game=mod,
                     logfile='2', developer='0', sv_pure='0', bot_enable='1', net_enabled='0', dedicated='1',
                     com_steamIntegration='0', fs_skipWorkshop='1', sv_errorExit='0', bot_developer='0')
    command = [str(exe)]
    for key, value in variables.items():
        command += ['+set', key, value]
    command += ['+exec', 'routecache.cfg']
    log = (home / mod if mod else game) / 'qconsole.log'
    read = lambda: log.read_text(encoding='latin-1') if log.exists() else ''
    begin = time.monotonic()
    process = WindowsPrivateProcess(command, exe.parent, output / 'logs' / f'{slug}-stdout.log')
    dropped_at = None
    try:
        while process.poll() is None and time.monotonic() - begin < timeout:
            text = read()
            if FATAL.search(text):
                break
            # an error drop ends the level and the script; the server idles after it
            if dropped_at is None and DROP.search(text):
                dropped_at = time.monotonic()
            if dropped_at is not None and time.monotonic() - dropped_at >= 2:
                break
            time.sleep(0.2)
        forced = process.poll() is None
        code = process.poll()
    finally:
        stop(process)
    text = read()
    (output / 'logs' / f'{slug}-console.log').write_text(text, encoding='latin-1')
    after = written.read_bytes() if written.exists() else None
    shutil.rmtree(home, ignore_errors=True)
    plain = COLOR.sub('', text)
    drops, fatal = DROP.findall(text), FATAL.findall(text)
    if code == 0 and DONE in text and not drops and not fatal:
        outcome = 'loaded'
    elif fatal:
        outcome = 'fatal'
    elif not forced and code != 0:
        outcome = 'crashed'
    elif drops:
        outcome = 'faulted' if FAULT in drops[0] else 'rejected'
    else:
        outcome = 'hung'
    listed = STATUS.findall(plain)
    return dict(outcome=outcome, exitCode=code, forcedTermination=forced,
                elapsedSeconds=round(time.monotonic() - begin, 1), errors=(fatal or drops)[:2],
                read=[list(m.groups()) for m in READ.finditer(plain)], refused=REFUSED.findall(plain),
                written=WRITTEN.findall(plain), writtenBytes=[int(v) for v in WRITTEN_BYTES.findall(plain)],
                routingInitialized=plain.count('AAS initialized'), botsListed=len(listed)), after


def script(case, play_waits, save):
    _, _, _, _, load, bots = case
    lines = [load, 'wait 100']
    lines += [f'addbot {bot} 3' for bot in bots]
    lines += [f'wait {play_waits}']
    if save:
        lines += ['set bot_saveroutingcache 1', 'wait 40']
    return lines + ['status', f'echo {DONE}', 'quit']


def jsonable(value):
    if isinstance(value, set):
        return sorted(value)
    if isinstance(value, dict):
        return {k: jsonable(v) for k, v in value.items()}
    return value


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--before', type=Path, required=True, help='dedicated server with the old reader')
    parser.add_argument('--after', type=Path, required=True, help='dedicated server with the checked reader')
    parser.add_argument('--installation', action='append', default=[], metavar='KEY=PATH')
    parser.add_argument('--only', help='comma-separated labels to run')
    parser.add_argument('--variants', help='comma-separated variants to read back (default: all)')
    parser.add_argument('--play', type=int, default=800, help='frames the bots play before a save')
    parser.add_argument('--timeout', type=float, default=240)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if os.name != 'nt':
        parser.error('This probe requires the Windows private-desktop harness')
    installations = {}
    for item in args.installation:
        key, _, path = item.partition('=')
        installations[key] = Path(path).resolve(strict=True)
    before, after = args.before.resolve(strict=True), args.after.resolve(strict=True)
    output = args.output.resolve()
    (output / 'logs').mkdir(parents=True, exist_ok=False)
    (output / 'caches').mkdir()
    binaries = {'before': sha256(before), 'after': sha256(after)}
    started = time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime())
    labels = [label for label in MAPS if MAPS[label][0] in installations]
    if args.only:
        labels = [label for label in labels if label in args.only.split(',')]
    report = dict(binaries=binaries, startedUtc=started, installations={k: str(v) for k, v in installations.items()},
                  sourceCommit=subprocess.run(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True,
                                              capture_output=True).stdout.strip(), maps={})
    for label in labels:
        case = MAPS[label]
        key, profile, folder, name, _, _ = case
        installation = installations[key]
        slug = re.sub(r'[^A-Za-z0-9_.-]+', '_', label)
        home = output / 'homes' / slug
        entry = dict(profile=profile, map=name, folder=folder)
        # 1. the engine writes the cache
        row, dump = play(after, case, installation, home, output, f'{slug}-write', script(case, args.play, True),
                         args.timeout)
        entry['write'] = row
        print(label, 'write', row['outcome'], row['written'], row['writtenBytes'], flush=True)
        if dump is None:
            aas = parse_aas(served(installation, folder, f'maps/{name}.aas'))
            entry['synthetic'] = {}
            for variant in ('empty', 'area-zero'):
                data, expected = synthetic(aas, variant)
                for side, exe in (('before', before), ('after', after)):
                    row, _ = play(exe, case, installation, home, output, f'{slug}-{variant}-{side}',
                                  script(case, args.play // 2, False), args.timeout, place=data)
                    row['expectedRefusal'] = expected
                    entry['synthetic'][f'{variant}/{side}'] = row
                    print(label, 'synthetic', variant, side, row['outcome'], row['read'], row['refused'][:1],
                          row['elapsedSeconds'], flush=True)
            report['maps'][label] = entry
            (output / 'result.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
            continue
        (output / 'caches' / f'{slug}.rcd').write_bytes(dump)
        # 2. measured offline against the navigation the installation serves
        aas = parse_aas(served(installation, folder, f'maps/{name}.aas'))
        fault, stats = check(dump, aas)
        entry['cache'] = dict(bytes=len(dump), sha256=hashlib.sha256(dump).hexdigest(),
                              fault=fault, header=list(HEADER.unpack_from(dump)), aasVersion=aas['version'],
                              numareas=aas['numareas'], numportals=aas['numportals'],
                              numclusters=aas['numclusters'], **jsonable(stats))
        print(label, 'measured', fault, stats.get('portalCaches'), stats.get('areaCaches'), flush=True)
        # 3. read back, intact and damaged, by both servers
        variants = VARIANTS if label == DAMAGED else ('intact',)
        if args.variants:
            variants = [v for v in variants if v in args.variants.split(',')]
        entry['loads'] = {}
        for variant in variants:
            data, expected = (dump, None) if variant == 'intact' else damage(dump, aas, variant)
            for side, exe in (('before', before), ('after', after)):
                row, again = play(exe, case, installation, home, output, f'{slug}-{variant}-{side}',
                                  script(case, args.play // 2, True), args.timeout, place=data)
                row['expectedRefusal'] = expected
                if again is not None and variant == 'intact' and row['written']:
                    kept = {masked(dump[at:at + size]) for at, size in records(dump)}
                    rewritten = {masked(again[at:at + size]) for at, size in records(again)}
                    row['rewritten'] = dict(caches=len(rewritten), readCachesKept=len(kept & rewritten),
                                            readCaches=len(kept), bytes=len(again))
                entry['loads'][f'{variant}/{side}'] = row
                print(label, variant, side, row['outcome'], row['read'], row['refused'][:1],
                      row.get('rewritten'), row['elapsedSeconds'], flush=True)
        report['maps'][label] = entry
        (output / 'result.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    report['binariesUnchanged'] = binaries == {'before': sha256(before), 'after': sha256(after)}
    report['endedUtc'] = time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime())
    (output / 'result.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print('binaries unchanged' if report['binariesUnchanged'] else 'BINARIES CHANGED')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
