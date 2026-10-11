"""Probe manifests must not save the per-run remote-console password."""
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]


class GameSpyProbeTests(unittest.TestCase):
    def test_session_launch_keeps_password_but_manifest_redacts_it(self):
        path = ROOT / 'scripts/probe_mohaa_gamespy.py'
        spec = importlib.util.spec_from_file_location('mohaa_gamespy_probe', path)
        probe = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(probe)
        (ROOT / '.tmp').mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(dir=ROOT / '.tmp', prefix='gamespy-manifest-') as tmp:
            folder = Path(tmp)
            executable = folder / 'fixture.exe'
            executable.write_bytes(b'not an executable; launch is mocked')
            session = probe.Session(folder / 'session')
            settings = {'rconPassword': 'first-secret', 'sv_hostname': 'rconPassword'}
            tail = ['+set', 'RCONPASSWORD', 'second-secret', '+map', 'dm/mohdm1']
            with patch.object(probe, 'WindowsPrivateProcess') as launch:
                process, log = session.start('server', executable, settings, tail, folder)
            launched = launch.call_args.args[0]
            self.assertIn('first-secret', launched)
            self.assertIn('second-secret', launched)
            saved = session.result['processes']['server']['command']
            self.assertEqual(saved.count('<redacted>'), 2)
            self.assertNotIn('first-secret', json.dumps(session.result))
            self.assertNotIn('second-secret', json.dumps(session.result))
            self.assertEqual(saved[saved.index('sv_hostname') + 1], 'rconPassword')
            self.assertEqual(saved[-2:], ['+map', 'dm/mohdm1'])
            self.assertEqual(settings['rconPassword'], 'first-secret')
            self.assertEqual(tail[2], 'second-secret')
            self.assertIs(process, launch.return_value)
            self.assertEqual(log, folder / 'session/server-home/main/qconsole.log')


if __name__ == '__main__':
    unittest.main()
