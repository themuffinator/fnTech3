#!/usr/bin/env python3
"""Load every official Enemy Territory map in the developer client and list every issue.

Runs scripts/probe_wolfet_client.py once per map, each in its own isolated home
on a private desktop, then reads each map's console log and sorts every warning
and error it contains. The installation is only read.

Every line naming a file the engine could not load is checked against the
installation's own paks and shader scripts. A file retail content does not
ship is reported as absent content: the retail engine meets that gap the same
way, so it does not fail the map. A file the content does ship but the engine
failed to load is an engine issue and fails the map. A map passes only when it
loads, draws, captures and quits cleanly, has no engine issue left, and loads
its audio zones.
"""
from pathlib import Path
import argparse
import json
import re
import subprocess
import sys
import zipfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(Path(__file__).resolve().parent))
from stock_wolfet_maps import STOCK_WOLFET_MAPS  # noqa: E402

COLOR = re.compile(r'\^[0-9A-Za-z]')

# Lines that name a file the engine could not load: (kind, pattern). The first
# capture group is the file.
MISSING = [
    ('image', re.compile(r'Shader image unavailable for: (\S+)')),
    ('image', re.compile(r"R_FindImageFile could not find '([^']+)'")),
    ('image', re.compile(r"q3map_lightImage '([^']+)' could not be loaded")),
    ('shader', re.compile(r'shader (\S+) in skin \S+ not found')),
    ('model', re.compile(r"R_Register\w+: couldn't load (\S+)")),
    ('sound', re.compile(r"couldn't load sound: (\S+)")),
    ('sound', re.compile(r'Failed to load sound (\S+?)!?$')),
]
# Developer reports the original ET engine makes in the same words for the
# same content: the fault is the content's, and ET draws it the same way.
RETAIL_IDENTICAL = [
    re.compile(r"WARNING: shader '[^']+' has lightmap but no lightmap stage!"),
    re.compile(r'Warning: cvar "[^"]+" given initial values'),
    # ET warns the same way when one image is asked for with and without mipmaps.
    re.compile(r'WARNING: reused image \S+ with mixed flags'),
    # ET's own game module reporting a map script's setstate that names no entity.
    re.compile(r'Warning: setstate called and no entities found'),
]
# Facts about the machine or the probe itself, not about a map.
ENVIRONMENT = [
    re.compile(r'NET_IP6Socket: bind: WSAEADDRNOTAVAIL'),
    re.compile(r'has no OpenAL Soft extensions'),
    re.compile(r'audio is currently muted by the window focus/minimize settings'),
]
# Reports rather than problems, though they contain a trigger word.
BENIGN = [
    re.compile(r'^Hunk_Clear', re.I),
    re.compile(r'Audio zones:', re.I),
    re.compile(r'^\s*[0-9]+ (voice sources|stream buffers)', re.I),
    re.compile(r'^Parsing menu file:', re.I),
]
# Everything else that looks like a problem is an engine issue.
ENGINE = [
    ('engine-error', re.compile(r'^\s*ERROR:|Sys_Error|recursive error|fatal crashed|Unhandled exception', re.I)),
    ('unknown-shader-keyword', re.compile(r'unknown (general )?shader parameter|unknown shader stage parameter', re.I)),
    ('shader-error', re.compile(r'shader .*(error|invalid|failed)|WARNING: .*shader', re.I)),
    ('module-refusal', re.compile(r'unsupported .*(cgame|ui|game) (import|export)|refus', re.I)),
    ('other-warning', re.compile(r'WARNING|\bERROR\b|\bwarning\b', re.I)),
]


class Content:
    """The files and shaders the installation's own content provides."""

    def __init__(self, installation):
        game = installation / 'etmain'
        self.files = set()
        self.shaders = set()
        for pak in sorted(game.glob('*.pk3')):
            with zipfile.ZipFile(pak) as archive:
                for member in archive.namelist():
                    name = member.lower()
                    self.files.add(name)
                    if name.startswith('scripts/') and name.endswith('.shader'):
                        self.shaders.update(self._shader_names(archive.read(member).decode('latin-1')))
        for path in game.rglob('*'):
            if path.is_file():
                self.files.add(path.relative_to(game).as_posix().lower())

    @staticmethod
    def _shader_names(text):
        text = re.sub(r'/\*.*?\*/', ' ', text, flags=re.S)
        text = re.sub(r'//[^\n]*', ' ', text)
        names, depth = [], 0
        for token in text.replace('{', ' { ').replace('}', ' } ').split():
            if token == '{':
                depth += 1
            elif token == '}':
                depth = max(depth - 1, 0)
            elif depth == 0:
                names.append(token.lower().replace('\\', '/'))
        return names

    @staticmethod
    def _stem(name):
        name = name.lower().replace('\\', '/')
        leaf = name.rsplit('/', 1)[-1]
        return name.rsplit('.', 1)[0] if '.' in leaf else name

    def ships(self, kind, name):
        stem = self._stem(name)
        if kind in ('image', 'shader'):
            return stem in self.shaders or any(f'{stem}.{ext}' in self.files for ext in ('tga', 'jpg', 'png'))
        if kind == 'model':
            return any(f'{stem}.{ext}' in self.files for ext in ('md3', 'mdc', 'mds', 'mdm', 'mdx'))
        if kind == 'sound':
            return f'{stem}.wav' in self.files
        return name.lower() in self.files


def classify(text, content):
    result = dict(engine={}, absent={}, retailIdentical=[], environment=[])
    for line in text.splitlines():
        stripped = COLOR.sub('', line).strip()
        if not stripped or any(pattern.search(stripped) for pattern in BENIGN):
            continue
        missing = next(((kind, match.group(1)) for kind, pattern in MISSING
                        for match in [pattern.search(stripped)] if match), None)
        if missing:
            kind, name = missing
            if content.ships(kind, name):
                result['engine'].setdefault(f'missing-{kind}-shipped', []).append(stripped)
            else:
                result['absent'].setdefault(kind, set()).add(name.lower().replace('\\', '/'))
            continue
        if any(pattern.search(stripped) for pattern in RETAIL_IDENTICAL):
            result['retailIdentical'].append(stripped)
            continue
        if any(pattern.search(stripped) for pattern in ENVIRONMENT):
            result['environment'].append(stripped)
            continue
        for name, pattern in ENGINE:
            if pattern.search(stripped):
                result['engine'].setdefault(name, []).append(stripped)
                break
    result['absent'] = {kind: sorted(names) for kind, names in result['absent'].items()}
    result['retailIdentical'] = sorted(set(result['retailIdentical']))
    result['environment'] = sorted(set(result['environment']))
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--installation', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--renderer', choices=('glx', 'vk', 'rtx'), default='glx')
    parser.add_argument('--frames', type=int, default=240)
    parser.add_argument('--timeout', type=int, default=180)
    parser.add_argument('--hunk', type=int, default=0)
    parser.add_argument('--audio', choices=('off', 'openal-muted'), default='openal-muted')
    parser.add_argument('--maps', nargs='*', default=list(STOCK_WOLFET_MAPS))
    parser.add_argument('--extra', default='', help='console commands run before the capture')
    args = parser.parse_args()
    output = args.output.resolve()
    output.relative_to(ROOT / '.tmp')
    output.mkdir(parents=True, exist_ok=False)
    content = Content(args.installation)
    summary = dict(renderer=args.renderer, audio=args.audio, hunk=args.hunk, maps=[])
    for name in args.maps:
        if name not in STOCK_WOLFET_MAPS:
            raise SystemExit(f'not an official Enemy Territory map: {name}')
        folder = output / name
        command = [sys.executable, str(ROOT / 'scripts/probe_wolfet_client.py'),
                   '--executable', str(args.executable), '--installation', str(args.installation),
                   '--output', str(folder), '--scene', 'map', '--map', name,
                   '--renderer', args.renderer, '--frames', str(args.frames),
                   '--timeout', str(args.timeout), '--audio', args.audio]
        if args.hunk:
            command += ['--hunk', str(args.hunk)]
        extra = '; '.join(part for part in ('s_info' if args.audio != 'off' else '', args.extra) if part)
        if extra:
            command += ['--extra', extra]
        run = subprocess.run(command, capture_output=True, text=True, timeout=args.timeout + 120)
        manifest = json.loads((folder / 'manifest.json').read_text(encoding='utf-8')) \
            if (folder / 'manifest.json').is_file() else {}
        log = folder / 'console.log'
        text = log.read_text(encoding='utf-8', errors='replace') if log.is_file() else ''
        issues = classify(text, content)
        zones = re.findall(r'Audio zones: (\d+) loaded from (\S+)', text)
        row = dict(
            map=name,
            clientPassed=bool(manifest.get('passed')),
            exitCode=manifest.get('exitCode'),
            error=manifest.get('error'),
            screenshots=len(manifest.get('screenshots', [])),
            installationUnchanged=manifest.get('installationUnchanged'),
            audioZones=[dict(count=int(count), path=path) for count, path in zones],
            engineIssueCounts={key: len(value) for key, value in issues['engine'].items()},
            engineIssues={key: sorted(set(value))[:40] for key, value in issues['engine'].items()},
            absentContent=issues['absent'],
            retailIdentical=issues['retailIdentical'],
            environment=issues['environment'],
        )
        row['passed'] = (row['clientPassed'] and row['installationUnchanged'] is True and
                         not row['engineIssueCounts'] and
                         (args.audio == 'off' or any(zone['count'] > 0 for zone in row['audioZones'])))
        summary['maps'].append(row)
        counts = ', '.join(f'{key}={value}' for key, value in row['engineIssueCounts'].items()) or 'no engine issues'
        absent = ', '.join(f'{kind}={len(names)}' for kind, names in row['absentContent'].items()) or 'none'
        zone = f"zones={row['audioZones'][-1]['count']}" if row['audioZones'] else 'zones=none'
        print(f"{name:10s} {'PASS' if row['passed'] else 'FAIL'}  client={'ok' if row['clientPassed'] else 'failed'} "
              f"{zone}  {counts}; absent content: {absent}", flush=True)
        if not row['clientPassed'] and run.stdout:
            print('  ' + run.stdout.strip().replace('\n', '\n  '))
    summary['passed'] = all(row['passed'] for row in summary['maps'])
    (output / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n', encoding='utf-8')
    return 0 if summary['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
