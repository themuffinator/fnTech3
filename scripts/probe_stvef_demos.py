"""Decode the retail Elite Force Holomatch demos with fnTech3's message layer.

The retail paks ship protocol-24 .efdemo recordings made by stvoyHM.exe. This
probe extracts the demos a retail 1.20 search path would open (a later pak
replaces an earlier one of the same name), decodes every record through the
engine's MSG layer and re-encodes each player and entity delta to compare its
bits with the recording. It reads a legitimate installation, writes only below
--output and never stores retail payloads in the repository.
"""
import argparse
import json
import shutil
import subprocess
import sys
import zipfile
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--installation', required=True)
    parser.add_argument('--output', required=True)
    parser.add_argument('--probe', required=True,
                        help='built fntech3_stvef_demo_probe executable')
    args = parser.parse_args()

    output = Path(args.output)
    if output.exists():
        sys.exit(f'{output} exists; choose a fresh output directory')
    extracted = output / 'demos'
    extracted.mkdir(parents=True)

    effective = {}
    for pak in sorted((Path(args.installation) / 'baseEF').glob('pak*.pk3')):
        with zipfile.ZipFile(pak) as archive:
            for info in archive.infolist():
                if info.filename.lower().endswith('.efdemo'):
                    effective[info.filename.lower()] = (pak, info)
    paths = []
    sources = {}
    for name, (pak, info) in sorted(effective.items()):
        with zipfile.ZipFile(pak) as archive:
            data = archive.read(info)
        target = extracted / pak.stem / info.filename
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
        paths.append(str(target))
        sources[str(target)] = {'pak': pak.name, 'name': info.filename,
                                'bytes': len(data), 'crc': f'{info.CRC:08x}'}

    result = subprocess.run([args.probe], input='\n'.join(paths) + '\n',
                            capture_output=True, text=True, check=True)
    demos = []
    for line in result.stdout.splitlines():
        row = json.loads(line)
        row.update(sources[row.pop('file')])
        demos.append(row)
    shutil.rmtree(extracted)

    summary = {
        'demos': len(demos),
        'records': sum(row['records'] for row in demos),
        'decodeErrors': [f"{row['pak']}:{row['name']}: {row['error']}"
                         for row in demos if row['error']],
        'reencodeMismatches': {
            f"{row['pak']}:{row['name']}": row['reencodeMismatches'] for row in demos},
    }
    (output / 'result.json').write_text(json.dumps({'summary': summary, 'demos': demos}, indent=1))
    print(json.dumps(summary, indent=1))


if __name__ == '__main__':
    main()
