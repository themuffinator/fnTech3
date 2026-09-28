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
        with tempfile.TemporaryDirectory() as folder:
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


if __name__ == '__main__':
    unittest.main()
