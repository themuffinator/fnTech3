#!/usr/bin/env python3
"""Compare installed skin parsing and model lookup with the four RTCW sources.

Reference functions and skin records are compiled unchanged in disposable
fixtures with their notices. The installed corpus fits the original model
and surface capacities. This measures parsing and lookup semantics, not retail
rendering or malformed-input safety of the references. Assets remain in memory.
"""
from __future__ import annotations

import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tests'))
from skin_model_tests import fixture, function


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def make_fixture(mode, image, local, qfiles):
    end = local.index('} skinSurface_t;')
    start = local.rindex('typedef struct', 0, end)
    records = local[start:local.index('} skin_t;', end) + len('} skin_t;')]
    capacity = re.search(r'#define\s+MD3_MAX_SURFACES\s+\d+', qfiles)[0]
    source = fixture('renderer') + '\nnamespace reference {\n' + capacity + '\n' + records + r'''
static struct { int numSkins; skin_t *skins[MAX_SKINS]; } tr;
static auto ri = ::ri;
static void R_SyncRenderThread() {}
static void R_IssuePendingRenderCommands() {}
static void Com_Printf(const char *, ...) {}
'''
    for signature in ('static char *CommaParse(', 'qhandle_t RE_RegisterSkin(', 'qboolean RE_GetSkinModel('):
        source += function(image, signature)
    source += '\n}\n#define GAME COM_GAME_RTCW_' + mode.upper() + r'''
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif
int main() {
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
#endif
    unsigned index = 0, lengths[2];
    while (std::fread(lengths, 4, 2, stdin) == 2) {
        if (!lengths[0]) break;
        assert(lengths[0] < MAX_QPATH && lengths[1] < 1024 * 1024);
        std::string name(lengths[0], '\0'), data(lengths[1], '\0');
        assert(std::fread(&name[0], 1, name.size(), stdin) == name.size());
        assert(std::fread(data.data(), 1, data.size(), stdin) == data.size());
        Reset(GAME); reference::tr = {}; reference::tr.numSkins = 1;
        files[name] = data;
        const int actual = RE_RegisterSkin(name.c_str());
        // Do not exercise the original loader beyond its backing arrays.
        assert(tr.numSkins == 2 && tr.skins[1]->numModels <= 5 && tr.skins[1]->numSurfaces <= 32);
        const int expected = reference::RE_RegisterSkin(name.c_str());
        assert(actual == expected && tr.numSkins == 2 && reference::tr.numSkins == 2);
        const auto *a = tr.skins[1]; const auto *b = reference::tr.skins[1];
        assert(a->numModels == b->numModels && a->numSurfaces == b->numSurfaces);
        assert(!std::memcmp(a->scale, b->scale, sizeof(a->scale)));
        unsigned queries = 0;
        if (actual) {
            for (int s = 0; s < a->numSurfaces; ++s) {
                const auto *expectedSurface = REFERENCE_SURFACE;
                assert(!std::strcmp(a->surfaces[s].name, expectedSurface->name));
                assert(a->surfaces[s].shader->name == expectedSurface->shader->name);
            }
            for (int m = 0; m < a->numModels; ++m) {
                assert(!std::strcmp(a->models[m].type, b->models[m]->type));
                assert(!std::strcmp(a->models[m].model, b->models[m]->model));
            }
            std::vector<std::string> keys{"playerscale", "PLAYERSCALE", "missing-key"};
            for (int m = 0; m < a->numModels; ++m) {
                std::string key = a->models[m].type; keys.push_back(key);
                for (char &c : key) c = (char)std::toupper((unsigned char)c);
                keys.push_back(key);
            }
            for (const auto &key : keys) {
                char out[MAX_QPATH+2], expectedOut[MAX_QPATH+2];
                std::memset(out, 0x55, sizeof(out)); std::memset(expectedOut, 0x55, sizeof(expectedOut));
                const auto found = RE_GetSkinModel(actual, key.c_str(), out+1);
                assert(found == reference::RE_GetSkinModel(expected, key.c_str(), expectedOut+1));
                if (found) assert(!std::strcmp(out+1, expectedOut+1));
                else assert(!std::memcmp(out, expectedOut, sizeof(out)));
                assert(out[0] == 0x55 && out[MAX_QPATH+1] == 0x55);
                assert(expectedOut[0] == 0x55 && expectedOut[MAX_QPATH+1] == 0x55);
                ++queries;
            }
        }
        assert(buffers.empty());
        std::printf("{\"index\":%u,\"accepted\":%s,\"surfaces\":%d,\"models\":%d,\"queries\":%u,\"exceedsStockModelCapacity\":%s}\n",
            index++, actual ? "true" : "false", a->numSurfaces, a->numModels, queries, a->numModels > 5 ? "true" : "false");
    }
    Reset(GAME);
}
'''
    return source.replace('REFERENCE_SURFACE', 'b->surfaces[s]' if '[MD3_MAX_SURFACES]' in local else '&b->surfaces[s]')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('installation', 'sp-source', 'mp-source', 'iortcw-source', 'report'):
        parser.add_argument('--' + name, type=Path, required=name != 'iortcw-source')
    parser.add_argument('--compiler', default=shutil.which('clang++'))
    parser.add_argument('--target')
    parser.add_argument('--sanitize', action='store_true')
    args = parser.parse_args()
    report_path = args.report.resolve(); report_path.relative_to(ROOT)
    report_path.parent.mkdir(parents=True, exist_ok=True)
    target = ['--target=' + args.target] if args.target else (['-m32'] if os.name != 'nt' else [])
    triple = subprocess.run([args.compiler, *target, '-dumpmachine'], cwd=ROOT,
                            capture_output=True, text=True, check=True).stdout.strip()
    own_paths = ['code/renderercommon/tr_skin.h', 'tests/skin_model_tests.py']
    own_paths += [f'code/{renderer}/{name}' for renderer in ('renderer', 'renderervk', 'rendererrtx') for name in ('tr_image.c', 'tr_local.h')]
    report = {'kind': __doc__, 'dateUtc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
              'target': triple, 'compiler': args.compiler, 'sanitized': args.sanitize,
              'implementationSha256': {name: digest(ROOT / name) for name in own_paths},
              'installation': str(args.installation.resolve(strict=True)), 'assets': [], 'results': []}
    payload = bytearray()
    for archive in sorted((args.installation / 'main').glob('*.pk3')):
        with zipfile.ZipFile(archive) as pak:
            for entry in pak.infolist():
                if not entry.filename.lower().endswith('.skin'):
                    continue
                data, name = pak.read(entry), entry.filename.encode('utf-8')
                report['assets'].append({'archive': archive.name, 'name': entry.filename, 'bytes': len(data),
                                         'sha256': hashlib.sha256(data).hexdigest()})
                payload += struct.pack('<II', len(name), len(data)) + name + data
    if not report['assets']:
        parser.error('No skins in the supplied installation/main/*.pk3')
    payload += bytes(8)
    references = [('sp', 'original', args.sp_source / 'src'), ('mp', 'original', args.mp_source / 'src')]
    if args.iortcw_source:
        references += [(mode, 'iortcw', args.iortcw_source / mode.upper() / 'code') for mode in ('sp', 'mp')]
    for mode, variant, reference in references:
        paths = {name: reference / 'renderer' / name for name in ('tr_image.c', 'tr_local.h')}
        paths['qfiles.h'] = reference / 'qcommon/qfiles.h'
        texts = {name: path.read_text(encoding='utf-8') for name, path in paths.items()}
        notices = '\n'.join(text[:text.index('*/') + 2] for text in texts.values())
        source = notices + make_fixture(mode, texts['tr_image.c'], texts['tr_local.h'], texts['qfiles.h'])
        with tempfile.TemporaryDirectory(prefix='skin-reference-', dir=report_path.parent) as temp:
            folder = Path(temp); fixture_path = folder / 'probe.cpp'
            fixture_path.write_text(source, encoding='utf-8')
            executable = folder / ('probe.exe' if os.name == 'nt' else 'probe')
            command = [args.compiler, *target, '-std=c++17', '-O1',
                       '-I', str(ROOT), str(fixture_path), '-o', str(executable)]
            if os.name == 'nt' and triple.startswith('x86_64'):
                command.append('-DFNTECH3_Q3_ONLY')
            if args.sanitize:
                command += ['-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-g']
            build = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=90)
            result = {'mode': mode, 'reference': variant, 'source': str(reference),
                      'sourceSha256': {name: digest(path) for name, path in paths.items()},
                      'stockModelCapacity': 5,
                      'buildExitCode': build.returncode, 'buildOutput': build.stdout + build.stderr}
            if build.returncode == 0:
                environment = os.environ.copy()
                if args.sanitize:
                    environment.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1', UBSAN_OPTIONS='halt_on_error=1')
                options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
                run = subprocess.run([str(executable)], input=payload, cwd=ROOT, capture_output=True,
                                     timeout=120, env=environment, **options)
                result.update(exitCode=run.returncode, diagnostics=run.stderr.decode('utf-8', errors='replace'),
                              skins=[json.loads(line) for line in run.stdout.splitlines()])
                result['passed'] = run.returncode == 0 and len(result['skins']) == len(report['assets'])
            else:
                result['passed'] = False
            report['results'].append(result)
            report_path.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
            print(f'{variant} {mode}: {"PASS" if result["passed"] else "FAIL"}; {len(result.get("skins", []))} skins', flush=True)
    return 0 if all(result['passed'] for result in report['results']) else 1


if __name__ == '__main__':
    raise SystemExit(main())
