"""Decode every Elite Force MP3 with fnTech3's minimp3 configuration.

The retail files end in Raven ID3v1 tags that record the uncompressed PCM size
("#UNCOMP") and a lip-sync volume ("#MAXVOL"). This probe compares the decoder's
PCM byte count against that size for every MP3 in baseEF/pak*.pk3. It reads a
legitimate installation, writes only below --output and never stores retail
payloads in the repository.
"""
import argparse
import collections
import json
import os
import re
import shutil
import subprocess
import sys
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tests'))
from compiler_support import compiler_target_args  # noqa: E402

TAG = re.compile(rb'#UNCOMP\s+(\d+)')
MAXVOL = re.compile(rb'#MAXVOL\s+(\d+)')


def raven_tag(data):
    tail = data[-128:]
    if len(data) < 128 or tail[:3] != b'TAG':
        return None, None
    uncomp = TAG.search(tail)
    maxvol = MAXVOL.search(tail)
    return (int(uncomp.group(1)) if uncomp else None,
            int(maxvol.group(1)) if maxvol else None)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--installation', required=True)
    parser.add_argument('--output', required=True)
    parser.add_argument('--compiler', default=shutil.which('clang++') or shutil.which('g++'))
    args = parser.parse_args()

    output = Path(args.output)
    if output.exists():
        sys.exit(f'{output} exists; choose a fresh output directory')
    extracted = output / 'mp3'
    extracted.mkdir(parents=True)
    minimp3 = ROOT / 'subprojects' / 'minimp3'
    if not (minimp3 / 'minimp3_ex.h').exists():
        sys.exit('run a Meson configure first so subprojects/minimp3 is fetched')

    probe = output / ('stvef_mp3_probe.exe' if os.name == 'nt' else 'stvef_mp3_probe')
    subprocess.run([args.compiler, *compiler_target_args(args.compiler), '-std=c++17', '-O2',
                    '-I', str(minimp3), str(ROOT / 'tests/fixtures/stvef_mp3_probe.cpp'),
                    '-o', str(probe)], check=True)

    base = Path(args.installation) / 'baseEF'
    tags = {}
    order = []
    for pak in sorted(base.glob('pak*.pk3')):
        with zipfile.ZipFile(pak) as archive:
            for info in archive.infolist():
                if not info.filename.lower().endswith('.mp3'):
                    continue
                data = archive.read(info)
                name = f'{pak.stem}/{info.filename}'
                target = extracted / name
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(data)
                tags[str(target)] = raven_tag(data) + (len(data), info.CRC)
                order.append(str(target))

    result = subprocess.run([str(probe)], input='\n'.join(order) + '\n',
                            capture_output=True, text=True, check=True)
    rows = []
    for line in result.stdout.splitlines():
        path, rate, channels, effect, streamed, status = line.rsplit(' ', 5)
        uncomp, maxvol, size, crc = tags[path]
        rows.append({'file': os.path.relpath(path, extracted).replace('\\', '/'),
                     'bytes': size, 'crc': f'{crc:08x}', 'rate': int(rate),
                     'channels': int(channels), 'effectPcmBytes': int(effect),
                     'streamPcmBytes': int(streamed),
                     'ravenUncompBytes': uncomp, 'ravenMaxVol': maxvol, 'status': status})
    shutil.rmtree(extracted)

    # Retail sizes effects as mono and music as streamed stereo; a tag matches
    # when either unpacking reproduces it. Report which one did.
    delta = collections.Counter()
    matched = collections.Counter()
    for row in rows:
        uncomp = row['ravenUncompBytes']
        if uncomp is None:
            continue
        if row['effectPcmBytes'] == uncomp:
            matched['effect'] += 1
        elif row['streamPcmBytes'] == uncomp:
            matched['stream'] += 1
        else:
            delta[row['effectPcmBytes'] - uncomp] += 1
    summary = {
        'files': len(rows),
        'decodeFailures': sum(row['status'] != 'ok' for row in rows),
        'rates': dict(collections.Counter(row['rate'] for row in rows)),
        'channels': dict(collections.Counter(row['channels'] for row in rows)),
        'withRavenTag': sum(row['ravenUncompBytes'] is not None for row in rows),
        'uncompMatchedAsEffect': matched['effect'],
        'uncompMatchedAsStream': matched['stream'],
        'uncompMismatchEffectMinusTag': {str(k): v for k, v in sorted(delta.items())},
    }
    (output / 'result.json').write_text(json.dumps({'summary': summary, 'files': rows}, indent=1))
    print(json.dumps(summary, indent=1))


if __name__ == '__main__':
    main()
