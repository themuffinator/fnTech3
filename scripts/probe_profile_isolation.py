#!/usr/bin/env python3
"""Run every profile against one shared writable home and check isolation.

The same fnTech3 client runs q3, ql, rtcw-sp and rtcw-mp in turn with one
fs_homepath, each against its own installation:

  round 1  q3, ql, rtcw-sp, rtcw-mp: each archives a marker cvar named after
           itself, a player name and a cl_maxpackets value of its own, then quits
  round 2  the reverse order, so each profile starts after all the others have
           written to the home: each prints every marker, its name,
           cl_maxpackets and protocol; q3 then archives one more marker
  round 3  ql again: it prints that later q3 marker

Q3 and QL share baseq3. QL keeps qzconfig.cfg/repconfig.cfg at the home root and
deliberately imports baseq3/q3config.cfg once, as a one-way migration, when it
starts without a qzconfig.cfg (Com_ExecuteCfg). Because ql follows q3 in
round 1, the probe expects ql to hold q3's first marker and not the later one.

A profile passes when it reads back only its own marker (and, for ql, the one
migrated marker) and its own values, reports its own protocol, persists markers
only in its own configuration files, loads only modules of its own API (Q3 its
QVMs, RTCW MP only `_mp_` modules, QL and RTCW SP none of those) and extracts
native modules into cache directories no other profile uses. Runs are windowed
on unswitched private desktops with sound and mouse/joystick input disabled;
commands come from configs only, and the installations' file lists must be
unchanged.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tests'))
from windows_private_process import WindowsPrivateProcess

PROFILES = {
    # game dir, configuration files (relative to the home), protocol, cl_maxpackets marker
    'q3': ('baseq3', ['baseq3/q3config.cfg'], '68', '41'),
    'ql': ('baseq3', ['qzconfig.cfg', 'repconfig.cfg'], '91', '42'),
    'rtcw-sp': ('main', ['main/wolfconfig.cfg'], '49', '43'),
    'rtcw-mp': ('main', ['main/wolfconfig_mp.cfg'], '60', '44'),
}
MIGRATED = {'ql': {'q3'}}  # markers a profile is expected to import once
LATER = 'fntech3_iso_q3_later'
CACHED = re.compile(r"^VM_LoadDll: loaded native module cache '([^']+)'", re.M)
NATIVE = re.compile(r"^VM_LoadDLL '([^']+)' ok", re.M)
QVM = re.compile(r'^Loading vm file ([^\r\n]+?)\.{0,3}\r?$', re.M)


def marker(profile):
    return 'fntech3_iso_' + profile.replace('-', '_')


def value_of(text, name):
    found = re.findall(rf'^"{re.escape(name)}" is:"([^"]*)"', text, re.M)
    return re.sub(r'\^7$', '', found[-1]) if found else None


def modules(text):
    """Successfully loaded modules: native DLL names, their cache directories, and QVM paths."""
    cached = [Path(path) for path in CACHED.findall(text)]
    return dict(native=sorted(set(NATIVE.findall(text))), qvm=sorted(set(QVM.findall(text))),
                cacheDirectories=sorted({path.parent.name for path in cached}))


def foreign(profile, loaded):
    """Module names that belong to another profile's API."""
    names = loaded['native']
    if profile == 'q3':
        return names  # Q3 runs its QVMs; no native module belongs to it here
    if profile == 'rtcw-mp':
        return [n for n in names if '_mp_' not in n]
    return [n for n in names if '_mp_' in n]


def run(client, profile, installation, home, output, label, lines):
    game = home / PROFILES[profile][0]
    game.mkdir(parents=True, exist_ok=True)
    config = f'probe-{label}.cfg'
    (game / config).write_text('\n'.join(lines + ['quit']) + '\n', encoding='utf-8')
    command = [str(client)]
    for key, value in dict(com_gameProfile=profile, fs_basepath=str(installation), fs_homepath=str(home),
                           fs_game='', r_fullscreen='0', r_mode='3', in_mouse='0', in_joystick='0', in_nograb='1',
                           s_initsound='0', com_steamIntegration='0', fs_skipWorkshop='1', cl_webuiEnable='0',
                           cl_renderer='glx', logfile='2', net_port='0', com_introplayed='1').items():
        command += ['+set', key, value]
    command += ['+exec', config]
    log = game / 'qconsole.log'
    begin = time.monotonic()
    process = WindowsPrivateProcess(command, client.parent, output / f'{label}-stdout.log')
    try:
        while process.poll() is None and time.monotonic() - begin < 90:
            time.sleep(0.2)
        forced = process.poll() is None
        code = process.poll()
    finally:
        process.close()
    text = log.read_text(encoding='utf-8', errors='replace') if log.exists() else ''
    (output / f'{label}-console.log').write_text(text, encoding='utf-8')
    (game / config).unlink()
    return text, dict(command=' '.join(command[1:]), exitCode=code, forcedTermination=forced,
                      elapsedSeconds=round(time.monotonic() - begin, 1))


def listing(root):
    return sorted(str(p.relative_to(root)) for p in root.rglob('*') if p.is_file())


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--client', type=Path, required=True)
    parser.add_argument('--q3', type=Path, required=True, help='Quake III Arena installation')
    parser.add_argument('--ql', type=Path, required=True, help='Quake Live installation')
    parser.add_argument('--rtcw', type=Path, required=True, help='Return to Castle Wolfenstein installation')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if os.name != 'nt':
        parser.error('This probe requires the Windows private-desktop harness')
    client = args.client.resolve(strict=True)
    installations = {'q3': args.q3.resolve(strict=True), 'ql': args.ql.resolve(strict=True),
                     'rtcw-sp': args.rtcw.resolve(strict=True), 'rtcw-mp': args.rtcw.resolve(strict=True)}
    roots = {'q3': installations['q3'], 'ql': installations['ql'], 'rtcw': installations['rtcw-sp']}
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    home = output / 'shared-home'
    home.mkdir()
    before = {name: listing(root) for name, root in roots.items()}
    report = dict(clientSha256=hashlib.sha256(client.read_bytes()).hexdigest(), home=str(home), rounds={},
                  sourceCommit=subprocess.run(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True,
                                              capture_output=True).stdout.strip())
    order = list(PROFILES)
    failures = []
    for profile in order:
        text, row = run(client, profile, installations[profile], home, output, f'round1-{profile}',
                        [f'seta {marker(profile)} {profile}', f'seta name iso-{profile}',
                         f'seta cl_maxpackets {PROFILES[profile][3]}', 'echo ISO_DONE'])
        row['marker'] = 'ISO_DONE' in text
        report['rounds'].setdefault('first', {})[profile] = row
        if row['exitCode'] != 0 or not row['marker']:
            failures.append(f'round 1 {profile}')
    queries = [marker(p) for p in order] + ['name', 'cl_maxpackets', 'protocol']
    for profile in reversed(order):
        _, _, protocol, packets = PROFILES[profile]
        after = [f'seta {LATER} 1'] if profile == 'q3' else []
        text, row = run(client, profile, installations[profile], home, output, f'round2-{profile}',
                        queries + after + ['echo ISO_DONE'])
        seen = {name: value_of(text, name) for name in queries}
        loaded = modules(text)
        expected = {marker(p): (p if p == profile or p in MIGRATED.get(profile, ()) else None) for p in order}
        expected.update(name=f'iso-{profile}', cl_maxpackets=packets, protocol=protocol)
        row.update(marker='ISO_DONE' in text, seen=seen, modules=loaded, foreignModules=foreign(profile, loaded),
                   mismatches={k: dict(expected=v, seen=seen[k]) for k, v in expected.items() if seen[k] != v})
        report['rounds'].setdefault('second', {})[profile] = row
        if row['exitCode'] != 0 or not row['marker'] or row['mismatches'] or row['foreignModules'] \
                or not (loaded['native'] or loaded['qvm']):
            failures.append(f'round 2 {profile}')
    # The migration is one-way and happens once: q3's later marker must not reach ql.
    text, row = run(client, 'ql', installations['ql'], home, output, 'round3-ql',
                    [LATER, marker('q3'), 'echo ISO_DONE'])
    row.update(marker='ISO_DONE' in text, seen={LATER: value_of(text, LATER), marker('q3'): value_of(text, marker('q3'))})
    report['rounds']['third'] = {'ql': row}
    if row['exitCode'] != 0 or not row['marker'] or row['seen'] != {LATER: None, marker('q3'): 'q3'}:
        failures.append('round 3 ql')
    # Each profile's extracted native modules live in cache directories no other profile used.
    caches = {p: set(r['modules']['cacheDirectories']) for p, r in report['rounds']['second'].items()}
    for first in caches:
        for second in caches:
            if first < second and caches[first] & caches[second]:
                failures.append(f'{first} and {second} share native cache {sorted(caches[first] & caches[second])}')
    ownership = {}
    for profile, (_, files, _, _) in PROFILES.items():
        allowed = {profile} | MIGRATED.get(profile, set())
        found = set()
        for relative in files:
            path = home / relative
            text = path.read_text(encoding='utf-8', errors='replace') if path.exists() else None
            markers = sorted(p for p in PROFILES if text and marker(p) in text)
            ownership[relative] = dict(profile=profile, exists=text is not None, markers=markers,
                                       laterMarker=bool(text and LATER in text))
            found.update(markers)
            if set(markers) - allowed or (profile != 'q3' and text and LATER in text):
                failures.append(f'{relative} holds {markers}')
        if profile not in found:
            failures.append(f'{profile} marker not persisted in its own configuration')
    report['configurationFiles'] = ownership
    report['homeFiles'] = [f for f in listing(home) if not f.endswith('.log')]
    report['installationsUnchanged'] = before == {name: listing(root) for name, root in roots.items()}
    if not report['installationsUnchanged']:
        failures.append('an installation file list changed')
    report['failures'] = failures
    report['passed'] = not failures
    (output / 'result.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    for profile, row in report['rounds']['second'].items():
        print(profile, row['exitCode'], row['seen'], row['mismatches'] or '', row['modules'])
    print('round 3 ql', report['rounds']['third']['ql']['seen'])
    print(json.dumps(ownership, indent=1))
    print('passed' if report['passed'] else f'failed: {failures}')
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
