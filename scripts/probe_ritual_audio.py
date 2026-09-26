#!/usr/bin/env python3
"""Check a Ritual-engine client's audio output from the engine's own capture.

Nobody listens to these probes: a client on a private desktop mixes into a
silent buffer. What it mixes is still recorded by the engine's `video`
command, which writes the legacy mixer's output beside the frames while a
demo plays. This probe

1. records a demo on a level with sound on (`--steps` run while recording),
2. replays it twice into AVIs, with the soundtrack at its default level and
   at zero, printing every sound the playback starts (`s_show_sounds`),
3. reads each AVI's PCM track and reports its level, peak and clipping,
   and how much of it the soundtrack accounts for,
4. looks for every sound effect the playback started in the recorded mix:
   the effect's own WAV, read from the installation's paks, is
   cross-correlated with the track (normalised), the log's server times are
   aligned to track times by the offset the strongest matches agree on, and
   each logged start counts as found when the effect's correlation within
   60 ms of it beats the 99th percentile of the same effect at 400 random
   places away from its starts.

A match needs the effect at its own pitch and speed, at its logged time.
Effects too distant or too quiet to stand out under the level's loops are
reported, not failed. MP3 dialogue and music are not matched sample by
sample; the soundtrack is measured by the level difference instead.

Windows only; each run goes through probe_ritual_client.py (windowed,
private desktop, isolated home, installation checked unchanged).
"""
from pathlib import Path
import argparse
import io
import json
import re
import shutil
import struct
import subprocess
import sys
import wave
import zipfile
from collections import Counter

import numpy as np
from scipy import signal

ROOT = Path(__file__).resolve().parents[1]
CLIENT = ROOT / 'scripts' / 'probe_ritual_client.py'
GAME_DIRS = {'fakk2': 'fakk', 'alice': 'base'}
DEMO = 'ritualaudio'


def run_client(args, output, steps, extra, overlay=None):
    command = [sys.executable, str(CLIENT), '--executable', str(args.executable),
               '--installation', str(args.installation), '--output', str(output),
               '--profile', args.profile, '--steps', steps, '--no-quit', '--sound',
               '--timeout', str(args.timeout), '--extra', 's_backend=legacy']
    for pair in extra:
        command += ['--extra', pair]
    if overlay:
        command += ['--overlay', str(overlay)]
    subprocess.run(command, check=True, cwd=ROOT, capture_output=True, text=True)
    return json.loads((output / 'manifest.json').read_text())


def avi_audio(path):
    """The PCM of an AVI's audio stream, as floats per channel."""
    data = path.read_bytes()
    fmt = None
    chunks = []

    def walk(offset, end):
        nonlocal fmt
        stream_type = None
        while offset + 8 <= end:
            ident, size = struct.unpack_from('<4sI', data, offset)
            body = offset + 8
            if ident in (b'RIFF', b'LIST'):
                walk(body + 4, body + size)
            elif ident == b'strh':
                stream_type = data[body:body + 4]
            elif ident == b'strf' and stream_type == b'auds':
                fmt = struct.unpack_from('<HHIIHH', data, body)
            elif ident[2:] == b'wb':
                chunks.append(data[body:body + size])
            offset = body + size + (size & 1)

    walk(0, len(data))
    if fmt is None:
        raise ValueError(f'{path} has no audio stream')
    tag, channels, rate, _, _, bits = fmt
    if tag != 1 or bits != 16:
        raise ValueError(f'{path}: unexpected audio format {fmt}')
    pcm = np.frombuffer(b''.join(chunks), dtype='<i2').astype(np.float64) / 32768.0
    return rate, pcm.reshape(-1, channels)


def playback_starts(log):
    """(server time, sample) for each sound the demo playback started."""
    lines = log.splitlines()
    first = [i for i, line in enumerate(lines) if 'CL_FirstSnapshot' in line]
    starts = []
    for line in lines[first[-1] if first else 0:]:
        match = re.match(r'^(?:\^5)?(\d+) - (sound/\S+) \(vol ', line)
        if match:
            starts.append((int(match.group(1)), match.group(2)))
    return starts


class Effects:
    """The installation's sound files, read-only from its paks."""

    def __init__(self, game_dir):
        self.archives = []
        for pak in sorted(game_dir.rglob('*.pk3')):
            archive = zipfile.ZipFile(pak)
            self.archives.append((archive, {name.lower(): name for name in archive.namelist()}))

    def load(self, name, rate):
        for archive, names in reversed(self.archives):
            entry = names.get(name.lower())
            if not entry:
                continue
            try:
                with wave.open(io.BytesIO(archive.read(entry))) as wav:
                    width, channels, source_rate = wav.getsampwidth(), wav.getnchannels(), wav.getframerate()
                    frames = wav.readframes(wav.getnframes())
            except (wave.Error, EOFError) as error:
                return None, f'not PCM WAV ({error})'
            if width == 1:
                samples = (np.frombuffer(frames, dtype=np.uint8).astype(np.float64) - 128.0) / 128.0
            elif width == 2:
                samples = np.frombuffer(frames, dtype='<i2').astype(np.float64) / 32768.0
            else:
                return None, f'{width * 8}-bit'
            samples = samples.reshape(-1, channels).mean(axis=1)
            if source_rate != rate:
                common = int(np.gcd(source_rate, rate))
                samples = signal.resample_poly(samples, rate // common, source_rate // common)
            return samples, f'{source_rate} Hz {width * 8}-bit'
        return None, 'not found'


def ncc(track, effect):
    """Normalised cross-correlation of the effect at every offset of the track."""
    n = len(effect)
    effect = effect - effect.mean()
    norm = np.sqrt(np.sum(effect ** 2))
    if norm == 0 or len(track) < n:
        return None
    corr = signal.correlate(track, effect, mode='valid', method='fft')
    energy = np.concatenate(([0.0], np.cumsum(track ** 2)))
    local = np.sqrt(np.maximum(energy[n:] - energy[:-n], 1e-12))
    return corr / (norm * local)


def level(pcm, rate):
    mono = pcm.mean(axis=1)
    block = rate // 10
    blocks = len(mono) // block
    rms = np.sqrt(np.mean(mono[:blocks * block].reshape(blocks, block) ** 2, axis=1))
    db = 20 * np.log10(rms + 1e-12)
    return {'seconds': round(len(pcm) / rate, 2), 'rate': rate, 'channels': pcm.shape[1],
            'peak': round(float(np.max(np.abs(pcm))), 4),
            'clippedSamples': int(np.sum(np.abs(pcm) >= 32767 / 32768.0)),
            'rmsDbfs100ms': {'min': round(float(db.min()), 1), 'median': round(float(np.median(db)), 1),
                             'max': round(float(db.max()), 1)}}


def match_effects(pcm, rate, starts, effects):
    tracks = {'mono': pcm.mean(axis=1), 'left': pcm[:, 0], 'right': pcm[:, 1]}
    by_name = {}
    for time, name in starts:
        by_name.setdefault(name, []).append(time)
    scores, sources = {}, {}
    for name in sorted(by_name):
        effect, facts = effects.load(name, rate)
        sources[name] = facts
        if effect is None or len(effect) < rate // 20:
            continue
        effect = effect[:rate]
        scores[name] = {label: ncc(track, effect) for label, track in tracks.items()}

    votes = Counter()
    for name, by_channel in scores.items():
        best = max(by_channel.values(), key=lambda s: float(np.max(s)))
        if float(np.max(best)) < 0.45:
            continue
        peak_ms = int(np.argmax(best)) * 1000.0 / rate
        for start in set(by_name[name]):
            votes[round((start - peak_ms) / 20.0) * 20] += 1
    if not votes:
        return {'offset': None, 'found': '0 of 0', 'effects': {}, 'notMatched': sources}
    base, agreeing = votes.most_common(1)[0]

    rng = np.random.default_rng(1)
    window = int(0.06 * rate)
    report, found_total, tested = {}, 0, 0
    for name, by_channel in scores.items():
        length = len(next(iter(by_channel.values())))
        times = sorted(by_name[name])
        positions = [int((time - base) * rate / 1000.0) for time in times]
        busy = np.zeros(length, dtype=bool)
        for p in positions:
            busy[max(0, p - rate // 4):max(0, min(length, p + rate // 4))] = True
        free = np.flatnonzero(~busy)
        if len(free) < 50:
            continue
        best = None
        for channel, score in by_channel.items():
            null = [float(np.max(score[max(0, p - window):p + window + 1]))
                    for p in rng.choice(free, size=min(400, len(free)), replace=False)]
            threshold = float(np.percentile(null, 99))
            values = [round(float(np.max(score[max(0, p - window):p + window + 1])), 3)
                      if 0 <= p < length else None for p in positions]
            hits = sum(1 for v in values if v is not None and v > threshold)
            if best is None or hits > best[0]:
                best = (hits, channel, threshold, values)
        hits, channel, threshold, values = best
        inside = sum(1 for v in values if v is not None)
        found_total += hits
        tested += inside
        report[name] = {'source': sources[name], 'starts': len(times), 'inTrack': inside, 'found': hits,
                        'channel': channel, 'null99': round(threshold, 3), 'ncc': values}
    return {'offset': {'serverTimeAtTrackStart': base, 'agreeingEffects': agreeing},
            'found': f'{found_total} of {tested}', 'effects': report,
            'notMatched': {name: facts for name, facts in sources.items() if name not in scores}}


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--installation', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--profile', choices=sorted(GAME_DIRS), default='fakk2')
    parser.add_argument('--map', required=True)
    parser.add_argument('--load-wait', type=int, default=8000, help='milliseconds before recording starts')
    parser.add_argument('--steps', default='wait 15000\n', help='console steps run while recording')
    parser.add_argument('--steps-file', type=Path)
    parser.add_argument('--timeout', type=int, default=300)
    args = parser.parse_args()
    args.executable = args.executable.resolve()
    args.installation = args.installation.resolve()
    output = args.output.resolve()
    game = GAME_DIRS[args.profile]
    steps = args.steps_file.read_text() if args.steps_file else args.steps.replace('\\n', '\n')
    if not steps.endswith('\n'):
        steps += '\n'

    record_steps = (f'devmap {args.map}\nwait {args.load_wait}\nset g_syncronousClients 1\n'
                    f'record {DEMO}\n{steps}stoprecord\nwait 500\nquit\n')
    manifests = {'record': run_client(args, output / 'record', record_steps, [])}
    overlay = output / 'overlay' / game / 'demos'
    overlay.mkdir(parents=True, exist_ok=True)
    demo = next((output / 'record' / 'home' / game / 'demos').glob(f'{DEMO}.*'))
    shutil.copy2(demo, overlay / demo.name)

    replay_steps = (f'wait 3000\nset s_show_sounds 1\nset cl_quitOnDemoCompleted 1\n'
                    f'demo {DEMO}\nwait 1\nvideo {DEMO}\n')
    effects = Effects(args.installation / game)
    summary = {'profile': args.profile, 'map': args.map, 'steps': steps, 'runs': {}}
    tracks = {}
    for label, music in (('music', None), ('noMusic', '0')):
        run = output / label
        extra = [f's_musicVolume={music}'] if music is not None else []
        manifests[label] = run_client(args, run, replay_steps, extra, overlay=output / 'overlay')
        avi = run / 'home' / game / 'videos' / f'{DEMO}.avi'
        rate, pcm = avi_audio(avi)
        tracks[label] = (rate, pcm)
        starts = playback_starts((run / 'home' / game / 'qconsole.log').read_text(errors='replace'))
        summary['runs'][label] = {'level': level(pcm, rate), 'playbackStarts': len(starts),
                                  **match_effects(pcm, rate, starts, effects)}
    for label, manifest in manifests.items():
        summary['runs'].setdefault(label, {})
        summary['runs'][label]['manifest'] = {key: manifest[key] for key in
                                              ('exitCode', 'forcedStop', 'seconds', 'exeSha256',
                                               'installationUnchanged')}
    music, quiet = (tracks[label][1] for label in ('music', 'noMusic'))
    frames = min(len(music), len(quiet))
    summary['soundtrackShareDb'] = round(float(
        10 * np.log10(np.mean(music[:frames] ** 2) / max(np.mean(quiet[:frames] ** 2), 1e-20))), 2)
    (output / 'summary.json').write_text(json.dumps(summary, indent=2))
    for label in ('music', 'noMusic'):
        run = summary['runs'][label]
        print(f"{label}: {run['level']} found {run['found']} offset {run['offset']}")
        for name, entry in run['effects'].items():
            print(f"  {name:50s} {entry['found']:2d}/{entry['inTrack']:2d} {entry['channel']:5s} "
                  f"null99 {entry['null99']:.3f} best {max((v for v in entry['ncc'] if v is not None), default=0):.3f}")
    print('soundtrack share (dB, music vs none):', summary['soundtrackShareDb'])
    print('installation unchanged:', all(m['installationUnchanged'] for m in manifests.values()))


if __name__ == '__main__':
    main()
