#!/usr/bin/env python3
"""Load damaged copies of an Elite Force single-player savegame and classify each.

First a fnTech3 client writes a well-formed save of `voy2` with the retail game
module (`devmap voy2`, its opening waited out at `fixedtime 50`, `save`). A
save is a run of chunks, each an id, a length and a checksum, the data and a
magic word (code/compat/stvef_sp_savegame.hpp). From it this writes fault
variants and loads each with `load` in a fresh process and isolated home:

  empty                 the file holds nothing
  short-header          the file ends inside the first chunk's frame
  truncated-N           the file ends N eighths of the way through (1..7)
  first-id              the first chunk's id is changed
  first-length-huge     the first chunk claims 2147483647 bytes
  first-length-short    the first chunk claims one byte less than it holds
  first-checksum        the first chunk's checksum is changed
  first-magic           the first chunk's magic word is changed
  trailing-garbage      64 seeded bytes follow the last chunk
  fuzz-SEED-NNN         (--fuzz) 1-16 seeded byte changes anywhere in the file

Each load ends in one of:

  loaded    the level spawned (`CL_FirstSnapshot`) with no error, and the
            process was still running three seconds later
  rejected  an engine error was printed and the process was still running two
            seconds later
  crashed   the process died on its own with a failure status
  faulted   rejected, but the error is the engine's report of a hardware
            exception it caught
  hung      none of these within the time limit

`crashed`, `faulted` and `hung` fail; a damaged save should be rejected, and
`trailing-garbage` may load, since retail reads a save chunk by chunk and never
looks past the last. Windows only: windowed on unswitched private desktops,
sound, mouse and joystick off, console commands only. Damaged copies are
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

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tests'))
from windows_private_process import WindowsPrivateProcess  # noqa: E402

GAME = 'baseEF'
ERROR = re.compile(r'^ERROR: .*$', re.M)
FAULT = 'Unhandled exception caught'
SPAWNED = re.compile(r'^CL_FirstSnapshot', re.M)
FRAME = 12  # id, length, checksum
OUTCOMES = ('loaded', 'rejected', 'crashed', 'faulted', 'hung')


def chunks(data):
    """The save's chunk frames, as (offset, id, length)."""
    found, offset = [], 0
    while offset + FRAME + 4 <= len(data):
        ident, length = struct.unpack_from('<II', data, offset)
        if offset + FRAME + length + 4 > len(data):
            raise ValueError('source save is not a run of whole chunks')
        found.append((offset, ident, length))
        offset += FRAME + length + 4
    if offset != len(data) or not found:
        raise ValueError('source save is not a run of whole chunks')
    return found


def variants(data, fuzz, seed):
    found = chunks(data)
    offset, ident, length = found[0]
    out = {'empty': (b'', 'rejected'), 'short-header': (data[:6], 'rejected')}
    for n in range(1, 8):
        out[f'truncated-{n}'] = (data[:len(data) * n // 8], 'rejected')

    def patch(at, value):
        buffer = bytearray(data)
        buffer[at:at + 4] = struct.pack('<I', value & 0xffffffff)
        return bytes(buffer)

    out['first-id'] = (patch(offset, ident ^ 0x01010101), 'rejected')
    out['first-length-huge'] = (patch(offset + 4, 0x7fffffff), 'rejected')
    out['first-length-short'] = (patch(offset + 4, max(0, length - 1)), 'rejected')
    checksum = struct.unpack_from('<I', data, offset + 8)[0]
    out['first-checksum'] = (patch(offset + 8, checksum ^ 0x5a5a5a5a), 'rejected')
    magic = struct.unpack_from('<I', data, offset + FRAME + length)[0]
    out['first-magic'] = (patch(offset + FRAME + length, magic ^ 0x5a5a5a5a), 'rejected')
    rng = random.Random(seed)
    out['trailing-garbage'] = (data + bytes(rng.randrange(256) for _ in range(64)), 'loaded-or-rejected')
    for n in range(fuzz):
        buffer = bytearray(data)
        for _ in range(rng.randint(1, 16)):
            buffer[rng.randrange(len(buffer))] = rng.randrange(256)
        out[f'fuzz-{seed}-{n:03d}'] = (bytes(buffer), 'rejected')
    return out


def settings(installation, home):
    return dict(com_gameProfile='stvef-sp', fs_basepath=str(installation), fs_homepath=str(home), fs_game='',
                r_fullscreen='0', r_mode='3', in_mouse='0', in_joystick='0', in_nograb='1', s_initsound='0',
                com_steamIntegration='0', fs_skipWorkshop='1', cl_webuiEnable='0', cl_renderer='glx',
                logfile='2', net_enabled='0', com_hunkMegs='256', com_introplayed='1', com_maxfps='60',
                com_maxfpsUnfocused='60')


def launch(client, installation, home, script, output, name, until, limit):
    game = home / GAME
    game.mkdir(parents=True, exist_ok=True)
    (game / 'efconfig.cfg').write_text('// empty\n', encoding='latin1')
    (game / 'probe.cfg').write_text(script, encoding='latin1')
    command = [str(client)]
    for key, value in settings(installation, home).items():
        command += ['+set', key, value]
    command += ['+exec', 'probe.cfg']
    log = game / 'qconsole.log'
    read = lambda: log.read_text(encoding='latin1', errors='replace') if log.exists() else ''
    process = WindowsPrivateProcess(command, client.parent, output / f'{name}-stdout.log')
    begin = time.monotonic()
    marks = {}
    try:
        while process.poll() is None and time.monotonic() - begin < limit:
            text = read()
            now = time.monotonic()
            for key, pattern in until.items():
                if key not in marks and pattern.search(text):
                    marks[key] = now
            if any(now - at >= wait for key, at in marks.items() for wait in [{'error': 2, 'spawned': 3}[key]]):
                break
            time.sleep(0.1)
        alive = process.poll() is None
        code = process.poll()
    finally:
        try:
            process.close()
        except subprocess.TimeoutExpired:
            process.wait(60)
            process.close()
    text = read()
    (output / f'{name}-console.log').write_text(text, encoding='utf-8')
    return command, text, marks, alive, code, round(time.monotonic() - begin, 1)


def make_base(client, installation, output):
    home = output / 'base-home'
    script = ('fixedtime 50\ndevmap voy2\nwait 600\nfixedtime 0\nwait 60\nsave faultbase\nwait 120\n'
              'echo SAVE_DONE\nwait 10\nquit\n')
    marks = dict(error=ERROR)
    command, text, _, _, code, seconds = launch(client, installation, home, script, output, 'base', marks, 240)
    save = home / GAME / 'saves' / 'faultbase.sav'
    if not save.exists():
        raise SystemExit(f'no base save was written (exit {code}); see {output / "base-console.log"}')
    return save.read_bytes(), dict(command=command, exitCode=code, seconds=seconds,
                                   errors=ERROR.findall(text)[:4])


def play(client, installation, output, name, payload):
    home = output / f'{name}-home'
    saves = home / GAME / 'saves'
    saves.mkdir(parents=True)
    path = saves / 'fault.sav'
    path.write_bytes(payload)
    command, text, marks, alive, code, seconds = launch(
        client, installation, home, 'load fault\n', output, name, dict(error=ERROR, spawned=SPAWNED), 90)
    path.unlink(missing_ok=True)
    errors = ERROR.findall(text)
    faulted = FAULT in text
    if code is not None and code != 0 and not alive:
        outcome = 'crashed'
    elif errors and alive:
        outcome = 'faulted' if faulted else 'rejected'
    elif 'spawned' in marks and not errors and alive:
        outcome = 'loaded'
    else:
        outcome = 'hung'
    return dict(command=command, exitCode=code, seconds=seconds, outcome=outcome, errors=errors[:4],
                inputBytes=len(payload), inputSha256=hashlib.sha256(payload).hexdigest())


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--client', type=Path, required=True)
    parser.add_argument('--installation', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--fuzz', type=int, default=0)
    parser.add_argument('--seed', type=int, default=20261024)
    args = parser.parse_args()
    if os.name != 'nt':
        parser.error('This probe requires the Windows private-desktop harness')
    client, installation = args.client.resolve(strict=True), args.installation.resolve(strict=True)
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    base, made = make_base(client, installation, output)
    report = dict(client=str(client), clientSha256=hashlib.sha256(client.read_bytes()).hexdigest(),
                  installation=str(installation), base=dict(made, bytes=len(base),
                                                           sha256=hashlib.sha256(base).hexdigest(),
                                                           chunks=len(chunks(base))),
                  sourceCommit=subprocess.run(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True,
                                              capture_output=True).stdout.strip(), seed=args.seed, variants={})
    for name, (payload, expected) in variants(base, args.fuzz, args.seed).items():
        row = play(client, installation, output, name, payload)
        row['expected'] = expected
        row['passed'] = row['outcome'] in ('loaded', 'rejected') and (
            expected == 'loaded-or-rejected' or row['outcome'] == expected)
        report['variants'][name] = row
        print(json.dumps({'variant': name, 'outcome': row['outcome'], 'expected': expected,
                          'errors': row['errors'][:1]}), flush=True)
    counts = {o: sum(1 for r in report['variants'].values() if r['outcome'] == o) for o in OUTCOMES}
    report['counts'] = counts
    report['failed'] = sorted(n for n, r in report['variants'].items() if not r['passed'])
    (output / 'manifest.json').write_text(json.dumps(report, indent=1), encoding='utf-8')
    print(json.dumps(dict(counts=counts, failed=report['failed'])))
    return 1 if report['failed'] else 0


if __name__ == '__main__':
    sys.exit(main())
