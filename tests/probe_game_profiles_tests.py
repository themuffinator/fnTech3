"""Validate installation evidence matching without launching a game."""
import importlib.util
from pathlib import Path
import stat
import tempfile
from types import SimpleNamespace
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("profile_probe", ROOT / "scripts/probe_game_profiles.py")
probe = importlib.util.module_from_spec(spec)
spec.loader.exec_module(probe)


class ProfileProbeEvidenceTests(unittest.TestCase):
    def exercise_reply(self, extra='', port=27960, observed=27960, stale=False,
                       protocol=68, mapname='q3dm6', address=None):
        """Run the real probe lifecycle against a synthetic process and UDP reply."""
        with tempfile.TemporaryDirectory(prefix='profile-probe-tests-', dir=ROOT / '.tmp') as folder:
            project = Path(folder)
            install, output = project / 'install', project / 'output'
            (install / 'baseq3').mkdir(parents=True)
            output.mkdir()
            marker = install / 'baseq3/pak0.pk3'
            marker.write_bytes(b'synthetic fixture')
            requests, commands = [], []

            class Process:
                returncode = None

                def __init__(self, command, **kwargs):
                    commands.append(command)
                    home = Path(command[command.index('fs_homepath') + 1]) / 'baseq3'
                    home.mkdir()
                    port_line = '' if observed is None else f'"net_port" is:"{observed}^7"\n'
                    (home / 'qconsole.log').write_text(
                        f'{marker} (1 files)\nqagame loaded in 1 msec\n{port_line}', encoding='utf-8')

                def poll(self):
                    return self.returncode

                def wait(self, **kwargs):
                    self.returncode = 0

            process = None

            def start(command, **kwargs):
                nonlocal process
                process = Process(command, **kwargs)
                return process

            class Socket:
                def __enter__(self):
                    return self

                def __exit__(self, *args):
                    pass

                def bind(self, address):
                    pass

                def getsockname(self):
                    return ('127.0.0.1', port)

                def settimeout(self, seconds):
                    pass

                def sendto(self, request, address):
                    requests.append(request)

                def recvfrom(self, size):
                    process.returncode = 0
                    challenge = 'stale' if stale else requests[-1].split(b' ', 1)[1].strip().decode('ascii')
                    reply = (b'\xff\xff\xff\xffinfoResponse\n' +
                             f'\\protocol\\{protocol}\\mapname\\{mapname}\\challenge\\{challenge}{extra}'.encode('latin1'))
                    return reply, address or ('127.0.0.1', port)

            with mock.patch.object(probe, 'ROOT', project), \
                    mock.patch.object(probe.subprocess, 'Popen', side_effect=start), \
                    mock.patch.object(probe.socket, 'socket', side_effect=lambda *args: Socket()):
                result = probe.probe(project / 'server.exe', install, 'q3', 'q3', output, 5)
            return result, requests, commands

    def test_incomplete_and_ambiguous_udp_replies_cannot_pass(self):
        for extra in ('\\unterminated', '\\protocol\\68', '\\Protocol\\68',
                      '\\mapname\\q3dm6', '\\challenge\\repeat', '\\\\empty-key', '\\key\\va\x00lue'):
            with self.subTest(extra=extra):
                result, _, _ = self.exercise_reply(extra=extra)
                self.assertFalse(result['passed'])
        for kwargs in ({'stale': True}, {'protocol': 91}, {'mapname': 'foreign'},
                       {'address': ('127.0.0.1', 27961)}):
            with self.subTest(kwargs=kwargs):
                self.assertFalse(self.exercise_reply(**kwargs)[0]['passed'])

    def test_child_must_confirm_the_requested_bound_port(self):
        for observed in (None, 27961):
            with self.subTest(observed=observed):
                self.assertFalse(self.exercise_reply(observed=observed)[0]['passed'])
        result, _, commands = self.exercise_reply()
        self.assertTrue(result['passed'])
        self.assertIn('+net_port', commands[0])

    def test_each_run_uses_a_fresh_request_challenge(self):
        first, requests, _ = self.exercise_reply()
        second, other_requests, _ = self.exercise_reply()
        self.assertTrue(first['passed'] and second['passed'])
        self.assertNotEqual(requests[0], other_requests[0])
        self.assertTrue(requests[0].startswith(b'\xff\xff\xff\xffgetinfo fntech3-'))

    def test_requires_the_exact_mounted_base_pak(self):
        marker = ROOT / "synthetic-install/baseq3/pak0.pk3"
        self.assertTrue(probe.installation_observed(f"Current search path:\n{marker} (3539 files)\n", marker))
        self.assertFalse(probe.installation_observed(f"{marker}.backup (3539 files)\n", marker))
        self.assertFalse(probe.installation_observed(f"Missing file: {marker}\n", marker))
        self.assertFalse(probe.installation_observed(f"{marker.parent / 'pak00.pk3'} (3539 files)\n", marker))
        self.assertFalse(probe.installation_observed("", marker))

    def test_temporary_homes_stay_inside_the_project(self):
        with tempfile.TemporaryDirectory(prefix='profile-probe-tests-', dir=ROOT / '.tmp') as folder:
            project = Path(folder)
            with mock.patch.object(probe, 'ROOT', project):
                scratch = probe.temporary_home_root()
                self.assertEqual(scratch, project.resolve() / '.tmp')
                with tempfile.TemporaryDirectory(prefix='home-', dir=scratch) as home:
                    self.assertEqual(Path(home).resolve().parent, scratch)

    def test_temporary_home_root_rejects_symlink_and_junction_metadata(self):
        with tempfile.TemporaryDirectory(prefix='profile-probe-tests-', dir=ROOT / '.tmp') as folder:
            project = Path(folder)
            (project / '.tmp').mkdir()
            for metadata in [SimpleNamespace(st_mode=stat.S_IFLNK, st_file_attributes=0),
                             SimpleNamespace(st_mode=stat.S_IFDIR, st_file_attributes=probe.REPARSE_POINT)]:
                with self.subTest(metadata=metadata), mock.patch.object(probe, 'ROOT', project), \
                        mock.patch.object(Path, 'lstat', return_value=metadata):
                    with self.assertRaisesRegex(ValueError, 'real directory inside the project'):
                        probe.temporary_home_root()


if __name__ == "__main__":
    unittest.main()
