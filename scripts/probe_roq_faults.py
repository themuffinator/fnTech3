#!/usr/bin/env python3
"""Play seeded corruptions of an installed RoQ movie in one running client.

The movie is read in memory from the installation's own paks. Each variant
changes 1-32 random bytes, alternately only inside chunk payloads (so framing
stays valid and the codebook, quad and audio decoders see the damage) or
anywhere, headers included. The variants are written to the isolated home's
video/ directory and played in sequence with `cinematic`, every second one
looping, each started while the previous one may still be playing. Then the
unmodified movie plays from the installation and the engine's own screenshot
records it. Each variant is classified from its console segment:

  played     the movie started and was not rejected while it ran
  rejected   the decoder rejected it with an "Invalid RoQ movie" reason
  refused    CIN_PlayCinematic refused its file header

The run passes when the client reaches the final marker and exits normally.
A crash or hang is attributed to the last variant that started. Corrupted
copies are deleted afterwards; only their hashes are kept. Windows only:
windowed on an unswitched private desktop, sound and mouse/joystick input
disabled, console commands only.
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

GAMES = {'q3': 'baseq3', 'ql': 'baseq3', 'rtcw-sp': 'main', 'rtcw-mp': 'main'}
DONE = 'ROQ_FUZZ_DONE'


def movie_from_paks(installation, game, name):
    for pak in sorted((installation / game).glob('*.pk3'), reverse=True):
        with zipfile.ZipFile(pak) as archive:
            for member in archive.namelist():
                if member.lower() == name.lower():
                    return archive.read(member), f'{pak.name}:{member}'
    raise SystemExit(f'{name} not found in {installation / game}')


def payload_ranges(data):
    """RoQ: an 8-byte file header, then chunks of [u16 id][u32 size][u16 flags] + size bytes."""
    ranges, cursor = [], 8
    while cursor + 8 <= len(data):
        _, size, _ = struct.unpack_from('<HIH', data, cursor)
        start, end = cursor + 8, min(cursor + 8 + size, len(data))
        if end > start:
            ranges.append((start, end))
        cursor += 8 + size
    return ranges


def variants(data, count, seed):
    rng = random.Random(seed)
    ranges = payload_ranges(data)
    for n in range(count):
        buffer = bytearray(data)
        anywhere = n % 2 == 1
        for _ in range(rng.randint(1, 32)):
            if anywhere or not ranges:
                position = rng.randrange(len(buffer))
            else:
                start, end = ranges[rng.randrange(len(ranges))]
                position = rng.randrange(start, end)
            buffer[position] = rng.randrange(256)
        yield f'fuzz-{n:03d}', bytes(buffer), 'anywhere' if anywhere else 'payload', n % 4 >= 2


def classify(segment, name):
    rejected = re.search(rf'Invalid RoQ movie video/{re.escape(name)}\.roq: ([^\r\n]+)', segment)
    if rejected:
        return 'rejected', rejected.group(1)
    if re.search(r'invalid RoQ ID or frame size|truncated RoQ file header', segment):
        return 'refused', None
    if f'playing {name}.roq' in segment:
        return 'played', None
    return 'unknown', None


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--client', type=Path, required=True)
    parser.add_argument('--installation', type=Path, required=True)
    parser.add_argument('--profile', choices=tuple(GAMES), required=True)
    parser.add_argument('--movie', required=True, help='path inside the paks, e.g. video/tier1.RoQ')
    parser.add_argument('--count', type=int, default=60)
    parser.add_argument('--seed', type=int, default=20260918)
    parser.add_argument('--frames', type=int, default=20, help='frames each variant plays before the next starts')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if os.name != 'nt':
        parser.error('This probe requires the Windows private-desktop harness')
    client, installation = args.client.resolve(strict=True), args.installation.resolve(strict=True)
    data, source = movie_from_paks(installation, GAMES[args.profile], args.movie)
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    home = output / 'home'
    game = home / GAMES[args.profile]
    (game / 'video').mkdir(parents=True)
    rows, lines = {}, ['set com_introplayed 1', 'set cl_allowDownload 0', 'set cl_motd 0', 'set developer 1',
                       'set con_notifytime 0']
    for name, payload, mode, looping in variants(data, args.count, args.seed):
        (game / 'video' / f'{name}.roq').write_bytes(payload)
        rows[name] = dict(corruption=mode, looping=looping, inputSha256=hashlib.sha256(payload).hexdigest())
        lines += [f'echo ROQ_FUZZ_BEGIN {name}', f'cinematic {name}.roq{" 2" if looping else ""}', f'wait {args.frames}']
    final = Path(args.movie).name
    lines += ['echo ROQ_FUZZ_BEGIN unmodified', f'cinematic {final}', 'wait 90', 'screenshotPNG roq-fuzz-final',
              'wait 5', f'echo {DONE}', 'quit']
    (game / 'probe.cfg').write_text('\n'.join(lines) + '\n', encoding='utf-8')
    command = [str(client)]
    for key, value in dict(com_gameProfile=args.profile, fs_basepath=str(installation), fs_homepath=str(home),
                           fs_game='', r_fullscreen='0', r_mode='3', in_mouse='0', in_joystick='0', in_nograb='1',
                           s_initsound='0', com_steamIntegration='0', fs_skipWorkshop='1', cl_webuiEnable='0',
                           cl_renderer='glx', logfile='2', net_port='0').items():
        command += ['+set', key, value]
    command += ['+exec', 'probe.cfg']
    begin = time.monotonic()
    process = WindowsPrivateProcess(command, client.parent, output / 'stdout.log')
    try:
        while process.poll() is None and time.monotonic() - begin < 120 + args.count * 5:
            time.sleep(0.2)
        forced = process.poll() is None
        code = process.poll()
    finally:
        process.close()
    for name in rows:
        (game / 'video' / f'{name}.roq').unlink()
    log = game / 'qconsole.log'
    text = log.read_text(encoding='utf-8', errors='replace') if log.exists() else ''
    (output / 'console.log').write_text(text, encoding='utf-8')
    segments = re.split(r'^ROQ_FUZZ_BEGIN (\S+)\r?$', text, flags=re.M)
    started = segments[1::2]
    for name, segment in zip(segments[1::2], segments[2::2]):
        if name in rows:
            rows[name]['outcome'], rows[name]['reason'] = classify(segment, name)
    shots = sorted(home.rglob('roq-fuzz-final*.png'))
    for shot in shots:
        (output / shot.name).write_bytes(shot.read_bytes())
    completed = code == 0 and DONE in text
    report = dict(profile=args.profile, movie=source, movieBytes=len(data), movieSha256=hashlib.sha256(data).hexdigest(),
                  count=args.count, seed=args.seed, framesPerVariant=args.frames, command=' '.join(command[1:]),
                  exitCode=code, forcedTermination=forced, elapsedSeconds=round(time.monotonic() - begin, 1),
                  completed=completed, lastStarted=started[-1] if started else None,
                  outcomes={o: sum(r.get('outcome') == o for r in rows.values())
                            for o in ('played', 'rejected', 'refused', 'unknown')},
                  reasons={}, screenshots=[dict(name=s.name, sha256=hashlib.sha256(s.read_bytes()).hexdigest())
                                           for s in shots],
                  clientSha256=hashlib.sha256(client.read_bytes()).hexdigest(), variants=rows,
                  sourceCommit=subprocess.run(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True,
                                              capture_output=True).stdout.strip())
    for row in rows.values():
        if row.get('reason'):
            report['reasons'][row['reason']] = report['reasons'].get(row['reason'], 0) + 1
    report['passed'] = completed and not report['outcomes']['unknown'] and len(shots) == 1
    (output / 'result.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({k: report[k] for k in ('profile', 'movie', 'exitCode', 'completed', 'lastStarted',
                                             'outcomes', 'reasons', 'passed')}, indent=1))
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
