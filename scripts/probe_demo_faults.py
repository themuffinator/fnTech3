#!/usr/bin/env python3
"""Play damaged copies of a valid demo and classify how the client handles each.

From one well-formed demo (an envelope of [sequence][length][bytes] records ended
by -1/-1) this writes fault variants and plays each in a fresh fnTech3 process
and isolated home under the given profile:

  truncated-header   the file ends four bytes into a record header
  truncated-body     the file ends halfway through the last record's bytes
  no-terminator      the final -1/-1 is missing
  length-negative    a record claims length -2
  length-over-limit  a record claims one byte more than the profile's message limit
  length-huge        a record claims 2147483647 bytes
  flipped-body       bits are flipped in the middle of every later record
  fuzz-SEED-NNN      (--fuzz) seeded random byte changes inside record bodies

Demos play under `timedemo 1`, which advances demo time 50 ms per rendered
frame, so long demos reach their damaged end within the time limit. Once the
demo has loaded, a console loop prints a tick every 100 frames for as long as
the engine's main loop runs; an engine error clears it. Each run ends in one of:

  completed  the demo completion path printed its timedemo line with no error,
             and the process was still running a second later
  rejected   an engine error was printed and the process was still running two
             seconds later
  stalled    no completion or error, but the tick loop kept running to the time
             limit: playback is waiting for demo time to reach a snapshot's
             time, as the original engines' demo loop does
  crashed    the process died on its own with a failure status
  hung       none of these, and the tick loop stopped

The probe stops every process that is still running. The tick loop keeps the
command buffer busy, so commands queued by the completion path (nextdemo, the
automatic quit) never run; completion is taken from the timedemo line that
CL_DemoCompleted prints itself. The expected outcome per variant is recorded
next to the observed one. With --retail (q3 only) the same variants play in the
installation's retail quake3.exe instead; its outcomes are recorded for
comparison, not judged. Windows only: windowed on unswitched private desktops,
sound and mouse/joystick input disabled, console commands only.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import struct
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tests'))
from windows_private_process import WindowsPrivateProcess

GAMES = {'q3': 'baseq3', 'ql': 'baseq3', 'rtcw-sp': 'main', 'rtcw-mp': 'main', 'wolfet': 'etmain'}
LIMITS = {'q3': 16384, 'ql': 16384, 'rtcw-sp': 32768, 'rtcw-mp': 32768, 'wolfet': 32768}
TICK = 'FAULT_DEMO_TICK'
STATS = re.compile(r'^\d+ frames, [\d.]+ seconds: [\d.]+ fps', re.M)
BODY_DAMAGE = 'completed-or-rejected-or-stalled'
OUTCOMES = ('completed', 'rejected', 'stalled', 'crashed', 'hung')
ERROR = re.compile(r'^(?:ERROR: .*|.*CL_ReadDemoMessage: [^\n]*|.*Illegible server message[^\n]*|'
                   r'.*read past end of server message[^\n]*)$', re.M)


def records(data):
    cursor, found = 0, []
    while cursor + 8 <= len(data):
        sequence, size = struct.unpack_from('<ii', data, cursor)
        if (sequence, size) == (-1, -1):
            return found, cursor
        if size < 0 or cursor + 8 + size > len(data):
            raise ValueError('source demo envelope is not well formed')
        found.append((cursor, size))
        cursor += 8 + size
    raise ValueError('source demo has no terminator')


def fuzzed(data, count, seed):
    """Seeded corruptions inside record bodies only, so the envelope stays valid
    and every change reaches the message decoder rather than the length checks."""
    import random
    found, _ = records(data)
    rng = random.Random(seed)
    out = {}
    for n in range(count):
        buffer = bytearray(data)
        for _ in range(rng.randint(1, 24)):
            offset, size = found[rng.randrange(1, len(found))]
            if size <= 4:
                continue
            position = offset + 8 + 4 + rng.randrange(size - 4)
            buffer[position] = rng.randrange(256)
        out[f'fuzz-{seed}-{n:03d}'] = (bytes(buffer), BODY_DAMAGE)
    return out


def variants(data, limit):
    found, end = records(data)
    if len(found) < 4:
        raise ValueError('source demo needs at least four records')
    last_offset, last_size = found[-1]
    middle_offset, _ = found[len(found) // 2]
    fake = lambda length: data[:end] + struct.pack('<ii', 999999, length)
    flipped = bytearray(data)
    for offset, size in found[len(found) // 2:]:
        if size > 16:
            for k in range(8, min(size, 64), 7):
                flipped[offset + 8 + k] ^= 0x5A
    return {
        'truncated-header': (data[:middle_offset + 4], 'completed'),
        'truncated-body': (data[:last_offset + 8 + last_size // 2], 'completed'),
        'no-terminator': (data[:end], 'completed'),
        'length-negative': (fake(-2), 'rejected'),
        'length-over-limit': (fake(limit + 1), 'rejected'),
        'length-huge': (fake(2147483647), 'rejected'),
        'flipped-body': (bytes(flipped), BODY_DAMAGE),
    }


def stop(process):
    """Close the process; teardown after TerminateProcess can outlast the harness's
    five seconds on a loaded machine."""
    try:
        process.close()
    except subprocess.TimeoutExpired:
        process.wait(60)
        process.close()


def play(client, installation, profile, name, payload, output, retail=False):
    home = output / f'{name}-home'
    game = home / GAMES[profile]
    (game / 'demos').mkdir(parents=True)
    extension = {'q3': 'dm_68', 'ql': 'dm_91', 'rtcw-sp': 'dm_49', 'rtcw-mp': 'dm_60', 'wolfet': 'dm_84'}[profile]
    (game / 'demos' / f'fault.{extension}').write_bytes(payload)
    (game / 'probe.cfg').write_text('set cl_allowDownload 0\nset cl_motd 0\nset com_introplayed 1\nset timedemo 1\n'
                                    f'set fault_tick "echo {TICK}; wait 100; vstr fault_tick"\n'
                                    'demo fault\nvstr fault_tick\n', encoding='utf-8')
    settings = dict(fs_basepath=str(installation), fs_homepath=str(home), fs_game='', r_fullscreen='0', r_mode='3',
                    in_mouse='0', in_joystick='0', in_nograb='1', s_initsound='0', logfile='2')
    if not retail:
        settings.update(com_gameProfile=profile, com_steamIntegration='0', fs_skipWorkshop='1', cl_webuiEnable='0',
                        cl_renderer='glx', net_ip='127.0.0.1', net_port='0', net_ip6='::1', net_port6='0')
    command = [str(client)]
    for key, value in settings.items():
        command += ['+set', key, value]
    command += ['+exec', 'probe.cfg']
    log = game / 'qconsole.log'
    read = lambda: log.read_text(encoding='utf-8', errors='replace') if log.exists() else ''
    begin = time.monotonic()
    process = WindowsPrivateProcess(command, client.parent, output / f'{name}-stdout.log')
    rejected_at = completed_at = None
    alive_after = False
    ticks = []  # (seconds since start, ticks printed so far)
    try:
        while process.poll() is None and time.monotonic() - begin < 60:
            text = read()
            now = time.monotonic()
            ticks.append((now - begin, text.count(TICK)))
            if rejected_at is None and ERROR.search(text):
                rejected_at = now
            if completed_at is None and STATS.search(text):
                completed_at = now
            settled = [at for at, wait in ((rejected_at, 2), (completed_at, 1)) if at is not None and now - at >= wait]
            if settled:
                alive_after = process.poll() is None
                break
            time.sleep(0.1)
        forced = process.poll() is None
        code = process.poll()
    finally:
        stop(process)
    text = read()
    (output / f'{name}-console.log').write_text(text, encoding='utf-8')
    errors = ERROR.findall(text)
    # Responsive to the end: the tick loop kept printing during the final five seconds.
    final = text.count(TICK)
    earlier = max((count for at, count in ticks if at <= ticks[-1][0] - 5), default=final) if ticks else final
    if not forced and code != 0:
        outcome = 'crashed'
    elif completed_at is not None and not errors and alive_after:
        outcome = 'completed'
    elif errors and alive_after:
        outcome = 'rejected'
    elif forced and not errors and final > earlier:
        outcome = 'stalled'
    else:
        outcome = 'hung'
    return dict(command=command, exitCode=code, forcedTermination=forced, elapsedSeconds=round(time.monotonic() - begin, 3),
                outcome=outcome, errors=errors[:5], truncatedNotices=text.count('Demo file was truncated'),
                timedemoLine=(STATS.findall(text) or [None])[0], ticks=final, inputBytes=len(payload),
                inputSha256=hashlib.sha256(payload).hexdigest())


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--client', type=Path, help='fnTech3 client executable (not needed with --retail)')
    parser.add_argument('--installation', type=Path, required=True)
    parser.add_argument('--profile', choices=tuple(GAMES), required=True)
    parser.add_argument('--demo', type=Path, required=True, help='a valid demo for the profile')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--fuzz', type=int, default=0, help='also play this many seeded body corruptions')
    parser.add_argument('--seed', type=int, default=20260918)
    parser.add_argument('--fuzz-only', action='store_true', help='skip the fixed fault variants')
    parser.add_argument('--retail', action='store_true',
                        help='play the same variants in the installation\'s retail quake3.exe instead (q3 only); '
                             'outcomes are recorded for comparison, not judged')
    args = parser.parse_args()
    if os.name != 'nt':
        parser.error('This probe requires the Windows private-desktop harness')
    if args.retail and args.profile != 'q3':
        parser.error('--retail compares against retail quake3.exe, so it needs --profile q3')
    if not args.retail and not args.client:
        parser.error('--client is required unless --retail is given')
    installation = args.installation.resolve(strict=True)
    inventory = lambda: {str(p.relative_to(installation)): (p.stat().st_size, p.stat().st_mtime_ns)
                         for p in installation.rglob('*') if p.is_file()}
    before = inventory()
    client = (installation / 'quake3.exe' if args.retail else args.client).resolve(strict=True)
    source = args.demo.resolve(strict=True).read_bytes()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    rows = {}
    cases = {} if args.fuzz_only else variants(source, LIMITS[args.profile])
    cases.update(fuzzed(source, args.fuzz, args.seed))
    for name, (payload, expected) in cases.items():
        row = play(client, installation, args.profile, name, payload, output, args.retail)
        if not args.retail:
            row['expected'] = expected
            row['passed'] = row['outcome'] in expected.split('-or-')
        rows[name] = row
        print(name, row['outcome'], '' if args.retail else f'expected {expected}', row['errors'][:1], flush=True)
    report = dict(profile=args.profile, engine='retail quake3.exe' if args.retail else 'fnTech3',
                  installation=str(installation), installationUnchanged=before == inventory(),
                  sourceDemo=str(args.demo), sourceSha256=hashlib.sha256(source).hexdigest(),
                  sourceRecords=len(records(source)[0]), clientSha256=hashlib.sha256(client.read_bytes()).hexdigest(),
                  messageLimit=LIMITS[args.profile], fuzz=dict(count=args.fuzz, seed=args.seed),
                  outcomes={o: sum(r['outcome'] == o for r in rows.values()) for o in OUTCOMES},
                  variants=rows, passed=None if args.retail else all(r['passed'] for r in rows.values()) and before == inventory(),
                  sourceCommit=subprocess.run(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True,
                                              capture_output=True).stdout.strip())
    (output / 'result.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    return 0 if report['passed'] is not False else 1


if __name__ == '__main__':
    raise SystemExit(main())
