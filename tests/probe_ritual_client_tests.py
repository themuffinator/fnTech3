"""scripts/probe_ritual_client.py writes its manifest however its client ends.

A stand-in for the private-desktop process plays each ending without a game:
a client that quits on its own, one the probe stops at --timeout, one that
began to exit on its own at that moment, one whose teardown outlasts the
probe's wait, and one that cannot be stopped at all.
"""
from pathlib import Path
import contextlib
import importlib.util
import io
import json
import os
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('probe_ritual_client', ROOT / 'scripts' / 'probe_ritual_client.py')
probe = importlib.util.module_from_spec(spec)
spec.loader.exec_module(probe)


def client(ending):
    class Client:
        def __init__(self, command, cwd, output):
            self.code = 0 if ending == 'quits' else None

        def poll(self):
            return self.code

        def terminate(self):
            if ending == 'stuck':
                raise PermissionError(13, 'Access is denied')
            if ending != 'slow':
                self.code = 1 if ending == 'stopped' else 0
            return ending != 'exiting'

        def wait(self, timeout):
            if self.code is None:
                raise subprocess.TimeoutExpired('client', timeout)
            return self.code

        def close(self):
            if self.code is None:
                raise PermissionError(13, 'Access is denied')
    return Client


class RitualClientProbeStopTests(unittest.TestCase):
    def run_probe(self, ending):
        with tempfile.TemporaryDirectory(prefix='ritual-probe-tests-', dir=ROOT / '.tmp') as folder:
            folder = Path(folder)
            executable = folder / 'runtime' / 'fntech3.exe'
            executable.parent.mkdir()
            executable.write_bytes(b'engine')
            installation = folder / 'installation'
            (installation / 'base').mkdir(parents=True)
            (installation / 'base' / 'pak0.pk3').write_bytes(b'pak')
            output = folder / 'output'
            argv = ['probe_ritual_client.py', '--executable', str(executable), '--installation', str(installation),
                    '--output', str(output), '--profile', 'alice', '--timeout', '0']
            with mock.patch.object(probe, 'WindowsPrivateProcess', client(ending)), \
                    mock.patch.object(sys, 'argv', argv), \
                    contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
                status = probe.main()
            return status, json.loads((output / 'manifest.json').read_text())

    def test_every_ending_writes_the_manifest(self):
        for ending, code, forced, error, status in (('quits', 0, False, None, 0),
                                                    ('stopped', 1, True, None, 0),
                                                    ('exiting', 0, False, None, 0),
                                                    ('slow', None, True, 'TimeoutExpired', 1),
                                                    ('stuck', None, False, 'PermissionError', 1)):
            with self.subTest(ending=ending):
                result, manifest = self.run_probe(ending)
                self.assertEqual(result, status)
                self.assertEqual((manifest['exitCode'], manifest['forcedStop']), (code, forced))
                if error:
                    self.assertTrue(manifest['stopError'].startswith(error + ': '), manifest['stopError'])
                else:
                    self.assertIsNone(manifest['stopError'])
                self.assertTrue(manifest['installationUnchanged'])


class RitualClientOutputTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix='ritual-output-tests-', dir=ROOT / '.tmp')
        self.addCleanup(self.temporary.cleanup)
        self.folder = Path(self.temporary.name)
        self.installation = self.folder / 'installation'
        self.installation.mkdir()
        self.executable = self.folder / 'runtime' / 'fntech3.exe'
        self.executable.parent.mkdir()
        self.executable.write_bytes(b'engine')
        self.output = self.folder / 'output'

    def test_existing_ordinary_output_is_retained_until_validation_completes(self):
        data = self.output / 'home' / 'base' / 'config.cfg'
        data.parent.mkdir(parents=True)
        data.write_bytes(b'configuration')
        self.assertEqual(probe.validate_output(self.output, self.installation, self.executable), self.output.resolve())
        self.assertEqual(data.read_bytes(), b'configuration')

    def test_rejects_project_root_scratch_root_and_installation_runtime_overlap(self):
        for target in (ROOT, ROOT / '.tmp', self.installation, self.installation / 'probe',
                       self.executable.parent, self.folder):
            with self.subTest(target=target), self.assertRaises(ValueError):
                probe.validate_output(target, self.installation, self.executable)

    def test_rejects_linked_output_before_mutating_protected_files(self):
        data = self.installation / 'protected.cfg'
        data.write_bytes(b'protected')
        self.output.mkdir()
        link = self.output / 'manifest.json'
        try:
            os.link(data, link)
        except OSError as error:
            self.skipTest(f'Hard links unavailable: {error}')
        with self.assertRaisesRegex(ValueError, 'linked data'):
            probe.validate_output(self.output, self.installation, self.executable)
        self.assertEqual(data.read_bytes(), b'protected')

    def test_rejects_junction_metadata_in_output_ancestors(self):
        self.output.mkdir()
        original = Path.lstat

        def linked_metadata(path, *args, **kwargs):
            metadata = original(path, *args, **kwargs)
            if path == self.output:
                return mock.Mock(st_mode=metadata.st_mode, st_file_attributes=probe.REPARSE_POINT)
            return metadata

        with mock.patch.object(Path, 'lstat', linked_metadata), self.assertRaisesRegex(ValueError, 'ancestor'):
            probe.validate_output(self.output, self.installation, self.executable)

    def test_rejects_dangling_junction_before_resolving_or_writing_output(self):
        # On Windows a dangling junction can report exists() == False and
        # is_symlink() == False while lstat still identifies its reparse point.
        self.output.mkdir()
        original = Path.lstat

        def dangling_metadata(path, *args, **kwargs):
            metadata = original(path, *args, **kwargs)
            if path == self.output:
                return mock.Mock(st_mode=metadata.st_mode, st_file_attributes=probe.REPARSE_POINT)
            return metadata

        with mock.patch.object(Path, 'exists', return_value=False), \
                mock.patch.object(Path, 'is_symlink', return_value=False), \
                mock.patch.object(Path, 'lstat', dangling_metadata), \
                self.assertRaisesRegex(ValueError, 'ancestor'):
            probe.validate_output(self.output, self.installation, self.executable)

    def test_extra_ownership_is_checked_before_mutation_and_preserves_feature_values(self):
        authored = self.installation / 'base' / 'pak0.pk3'
        authored.parent.mkdir()
        authored.write_bytes(b'authored fixture')
        existing = self.output / 'home' / 'base' / 'config.cfg'
        existing.parent.mkdir(parents=True)
        existing.write_bytes(b'previous configuration')
        (self.output / 'manifest.json').write_bytes(b'previous manifest')

        def snapshot():
            return {str(path.relative_to(self.folder)): path.read_bytes() if path.is_file() else None
                    for path in self.folder.rglob('*')}

        before = snapshot()
        argv = ['probe_ritual_client.py', '--executable', str(self.executable),
                '--installation', str(self.installation), '--output', str(self.output),
                '--profile', 'alice', '--timeout', '0']
        owned = (
            f'FS_HOMEPATH={self.installation}', f'fs_basepath={self.executable.parent}',
            'fs_basegame=..', 'fs_game=alternate', 'fs_cdpath=other', 'fs_filedir=other',
            'mapdir=other', 'fs_steampath=other', 'fs_apppath=other',
            'Com_GameProfile=fakk2', 'cl_renderer=vk', 'r_fullscreen=1', 'r_mode=3',
            'r_customWidth=1024', 'r_customHeight=768', 'r_windowedMode=3',
            'r_windowedWidth=1024', 'r_windowedHeight=768',
            'in_mouse=1', 'in_joystick=1', 'in_nograb=0', 'in_synthetic=1',
        )
        malformed = ('s_volume', '=1', '"fs_homepath"=other', 'fs_homepath =other',
                     ' fs_homepath=other', 'fs_homepath\t=other',
                     'fs_homepath\n=other', 'fs_homepath\x00=other')
        for pair in (*owned, *malformed):
            with self.subTest(pair=pair), \
                    mock.patch.object(sys, 'argv', argv + ['--extra', 's_volume=0.25', '--extra', pair]), \
                    mock.patch.object(probe, 'WindowsPrivateProcess') as launch, \
                    mock.patch.object(probe, 'validate_output', wraps=probe.validate_output) as validate, \
                    contextlib.redirect_stderr(io.StringIO()):
                with self.assertRaises(SystemExit) as error:
                    probe.main()
                self.assertEqual(error.exception.code, 2)
                launch.assert_not_called()
                validate.assert_not_called()
                self.assertEqual(snapshot(), before)

        allowed = ('s_volume=0.25', 'm_pitch=0.022', 's_alDopplerFactor=0.5',
                   's_backend=openal', 's_musicVolume=0.5', 'r_dynamiclight=1',
                   'rtx_rt_mode=0', 'developer=2', 'probe_note=left=right', 'probe_empty=')
        extra_argv = [token for pair in allowed for token in ('--extra', pair)]
        with mock.patch.object(sys, 'argv', argv + extra_argv), \
                mock.patch.object(probe, 'WindowsPrivateProcess', side_effect=client('quits')) as launch, \
                contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
            self.assertEqual(probe.main(), 0)
        command = launch.call_args.args[0]
        assignments = [tuple(command[index + 1:index + 3]) for index, token in enumerate(command)
                       if token == '+set']
        for pair in allowed:
            name, _, value = pair.partition('=')
            self.assertIn((name, value), assignments)
        self.assertEqual(authored.read_bytes(), b'authored fixture')
        self.assertEqual(self.executable.read_bytes(), b'engine')


if __name__ == '__main__':
    unittest.main()
