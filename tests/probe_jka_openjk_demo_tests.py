"""Bounded demo framing and maintained playback evidence; no games launched."""
from pathlib import Path
import contextlib
import io
import json
import os
import stat
import struct
import sys
import tempfile
import types
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
import jka_demo_evidence as container
import probe_jka_openjk_demo as probe
from probe_jka_openjk_peer_tests import Process, fixture, png_header, demo_records


class DemoProcess(Process):
    fault = None
    fault_origin = 'server'
    closed = []

    def __init__(self, command, cwd, stdout):
        self.replay = '+demo' in command
        if not self.replay:
            return super().__init__(command, cwd, stdout)
        self.server = False
        self.terminated = False
        self.origin = 'server' if command[-1] == 'jka-bounded-server.dm_26' else 'client'
        home = Path(command[command.index('fs_homepath') + 1])
        self.events.append((command, dict(TEMP=os.environ.get('TEMP'), TMP=os.environ.get('TMP'))))
        (home / 'os-temp/native.tmp').write_bytes(b'generated module identity')
        stdout.write_text('fixture stdout\n')
        active, eof = probe.markers(self.origin)
        fault = self.fault if self.origin == self.fault_origin else None
        rows = ['MODE: -1, 800 x 600 windowed hz:', active, eof,
            'snapshot:10 delta:-1 ping:0', 'snapshot:11 delta:10 ping:0',
            'VM_CreateLegacy: uix86.dll succeeded [0x40000000]',
            'VM_CreateLegacy: cgamex86.dll succeeded [0x30000000]']
        if fault == 'eof': rows.remove(eof)
        if fault == 'active': rows.remove(active)
        if fault == 'delta': rows[4] = 'snapshot:10 delta:-1 ping:0'
        if fault == 'module': rows.pop()
        if fault == 'windowed': rows[0] = 'MODE: 800 x 600 fullscreen'
        if fault == 'error': rows += ['ERROR: read past end of server message']
        if fault == 'truncated': rows += ['Demo file was truncated.']
        (home / 'base/qconsole.log').write_text(''.join('2026-10-02 19:29:44 ' + row + '\n' for row in rows))
        screenshots = home / 'base/screenshots'
        screenshots.mkdir()
        (screenshots / f'jka-demo-{self.origin}.png').write_bytes(png_header(640 if fault == 'png' else 800))

    def poll(self):
        if self.replay:
            return None if self.fault in ('error', 'truncated', 'timeout') and self.origin == self.fault_origin and not self.terminated else 0
        return super().poll()

    def close(self):
        self.closed.append(self.origin if self.replay else 'capture')
        if self.replay and self.fault == 'stop' and self.origin == self.fault_origin:
            raise OSError('owned fixture close failed')
        return super().close()


class DemoEvidenceTests(unittest.TestCase):
    def setUp(self):
        self.parent = ROOT / '.tmp/runtime/jka-openjk-tests'
        self.parent.mkdir(parents=True, exist_ok=True)
        self.temporary = tempfile.TemporaryDirectory(dir=self.parent)
        self.folder = Path(self.temporary.name).resolve()
        self.args = fixture(self.folder)
        DemoProcess.events = []
        DemoProcess.closed = []

    def tearDown(self):
        for path in (self.folder, *self.folder.rglob('*')):
            self.assertFalse(path.is_symlink() or getattr(path.lstat(), 'st_file_attributes', 0) & 0x400)
        self.temporary.cleanup()

    def run_fixture(self, **faults):
        with contextlib.ExitStack() as stack:
            stack.enter_context(patch.object(probe.peer, 'WindowsPrivateProcess', DemoProcess))
            stack.enter_context(patch.object(probe.peer, 'ready_identity', return_value={'getinfo': {}, 'getstatus': {}}))
            stack.enter_context(contextlib.redirect_stdout(io.StringIO()))
            for name, value in faults.items(): stack.enter_context(patch.object(DemoProcess, name, value))
            passed = probe.run(self.args)
        return passed, json.loads((self.args.output / 'result.json').read_text())

    def test_container_requires_exact_bounded_records_and_strict_footer(self):
        raw = demo_records()
        details = container.validate_container(raw)
        self.assertEqual((details['recordCount'], details['firstSequence'], details['lastSequence']), (3, 10, 12))
        self.assertTrue(details['exactEOF'])
        cases = [b'', raw[:-1], raw[:-8], raw + b'trailing', struct.pack('<ii', -1, -1),
            struct.pack('<ii', 10, -1), struct.pack('<ii', -1, 1) + b'\0',
            struct.pack('<ii', -2, 1) + b'\0', struct.pack('<ii', 1, 0),
            struct.pack('<ii', 1, 49153), struct.pack('<ii', 1, 10) + b'\0',
            raw[:9] + raw[:9] + raw[18:]]
        for bad in cases:
            with self.subTest(raw=bad[:20]), self.assertRaises(ValueError): container.validate_container(bad)
        with patch.object(container, 'MAX_DEMO_BYTES', len(raw) - 1):
            with self.assertRaises(ValueError): container.validate_container(raw)
        with patch.object(container, 'MAX_RECORDS', 2):
            with self.assertRaises(ValueError): container.validate_container(raw)

    def test_retention_checks_path_suffix_regular_file_links_and_size_before_read(self):
        home = self.folder / 'home'
        home.mkdir()
        demo = home / 'generated.dm_26'
        demo.write_bytes(demo_records())
        raw, details = container.generated_demo(demo, home)
        self.assertEqual(raw, demo_records())
        self.assertEqual(len(details['sha256']), 64)
        with self.assertRaises(ValueError): container.generated_demo(demo, self.folder / 'installation')
        with self.assertRaises(ValueError): container.generated_demo(home / '..' / 'home/generated.dm_26', home)
        (home / 'wrong.dll').write_bytes(raw)
        with self.assertRaises(ValueError): container.generated_demo(home / 'wrong.dll', home)
        (home / 'directory.dm_26').mkdir()
        with self.assertRaises(ValueError): container.generated_demo(home / 'directory.dm_26', home)
        with patch.object(container, 'MAX_DEMO_BYTES', len(raw) - 1), patch.object(Path, 'open') as opened:
            with self.assertRaises(ValueError): container.generated_demo(demo, home)
            opened.assert_not_called()
        original = Path.lstat
        def linked(path, *args, **kwargs):
            if path == home:
                return types.SimpleNamespace(st_mode=stat.S_IFDIR, st_file_attributes=0x400)
            return original(path, *args, **kwargs)
        with patch.object(Path, 'lstat', linked):
            with self.assertRaises(ValueError): container.generated_demo(demo, home)

    def test_capture_and_both_playbacks_require_semantics_and_cleanup(self):
        passed, report = self.run_fixture()
        self.assertTrue(passed and report['capturePassed'])
        self.assertEqual([row['origin'] for row in report['playbacks']], ['server', 'client'])
        self.assertEqual(report['playbacks'][0]['writer'], 'fnTech3 native server')
        self.assertEqual(report['playbacks'][1]['writer'], 'OpenJK client control')
        self.assertFalse(report['retailEnginePlayback'] or report['fnTech3ClientPlayback'])
        for row in report['playbacks']:
            self.assertTrue(row['demoCompletedObserved'] and row['activeActionObserved'])
            self.assertTrue(row['deltaSnapshotObserved'] and row['homeRemoved'] and row['demoUnchanged'])
            self.assertTrue(row['installationUnchanged'] and row['binariesUnchanged'])
            self.assertEqual(len(row['screenshots']), 1)
        self.assertFalse(list(self.args.output.rglob('*.dll')) or list(self.args.output.rglob('*.tmp')))
        self.assertEqual(len(list(self.args.output.rglob('*.dm_26'))), 2)
        self.assertEqual(DemoProcess.closed, ['capture', 'capture', 'server', 'client'])

    def test_partial_playback_evidence_or_bad_capture_cannot_pass(self):
        for fault in ('eof', 'active', 'delta', 'windowed', 'module', 'png', 'error', 'truncated'):
            self.args.output = self.folder / fault
            passed, report = self.run_fixture(fault=fault)
            self.assertFalse(passed)
            self.assertTrue(report['capturePassed'])
            self.assertFalse(report['playbacks'][0]['passed'])
            self.assertTrue(report['playbacks'][0]['homeRemoved'])
            self.assertTrue(report['playbacks'][1]['passed'])
            if fault in ('error', 'truncated'):
                self.assertTrue(report['playbacks'][0]['forcedStop'])
                self.assertIn('earlyStopReason', report['playbacks'][0])

    def test_capture_refusal_never_launches_a_reader_and_cleanup_failure_is_honest(self):
        passed, report = self.run_fixture(malformed_demo=True)
        self.assertFalse(passed or report['capturePassed'])
        self.assertEqual(report['playbacks'], [])
        self.assertEqual(len(DemoProcess.closed), 2)
        self.args.output = self.folder / 'stop'
        passed, report = self.run_fixture(fault='stop')
        self.assertFalse(passed)
        self.assertFalse(report['playbacks'][0]['homeRemoved'])
        self.assertIn('close failed', report['playbacks'][0]['stopError'])

    def test_replay_uses_explicit_suffix_stable_reader_windowed_disabled_input_and_no_network(self):
        for origin in ('server', 'client'):
            command = probe.launch_command(self.args.openjk_directory / 'openjk.x86.exe',
                self.args.installation, self.args.output / origin / 'home', origin)
            self.assertEqual(command[-2:], ['+demo', f'jka-bounded-{origin}.dm_26'])
            for key in ('r_fullscreen', 'in_mouse', 'in_keyboard', 'in_joystick', 'net_enabled', 'net_port'):
                self.assertEqual(command[command.index(key) + 1], '0')
            self.assertIn('set nextdemo "echo FNTECH3_JKA_DEMO_EOF_', probe.configuration(origin))
            self.assertIn('screenshot_png', probe.configuration(origin))
            self.assertLess(len(probe.subprocess.list2cmdline(command[1:]).encode()), 1024)
        with self.assertRaises(ValueError): probe.configuration('server;quit')
        wrong = 'CL_NextDemo: echo FNTECH3_JKA_DEMO_EOF_SERVER; quit;\n'
        self.assertFalse(probe.playback_evidence(wrong, 'server')['demoCompletedObserved'])

    def test_unbounded_stage_time_refuses_before_any_runtime_or_file_creation(self):
        self.args.timeout = 61
        with patch.object(probe.peer, 'launch_private') as launch:
            with self.assertRaises(ValueError): probe.run(self.args)
            launch.assert_not_called()
        self.assertFalse(self.args.output.exists())

    def test_replay_reuse_requires_strict_finalized_capture_and_matching_identities(self):
        passed, report = self.run_fixture()
        self.assertTrue(passed)
        self.args.replay_capture = self.args.output / 'capture'
        original = self.args.replay_capture / 'result.json'
        self.args.output = self.folder / 'retry'
        with patch.object(probe.peer, 'run', side_effect=AssertionError('Must not recapture')):
            passed, report = self.run_fixture()
        self.assertTrue(passed and report['captureReused'])
        self.assertEqual(len(DemoProcess.closed), 6)
        raw = original.read_text()
        for key, value in (('passed', False), ('homesRemoved', False), ('recordingRequested', False),
                ('pure', True), ('direction', 'fntech3-to-openjk'), ('binarySha256', []), ('demos', [{}]), ('map', 'different')):
            bad = json.loads(raw)
            bad[key] = value
            original.write_text(json.dumps(bad))
            self.args.output = self.folder / ('bad-' + key)
            with patch.object(probe.peer, 'launch_private') as launch:
                passed, report = self.run_fixture()
                launch.assert_not_called()
            self.assertFalse(passed)
        original.write_text(raw)
        pe = self.args.server
        pe.write_bytes(pe.read_bytes() + b'changed binary')
        self.args.output = self.folder / 'bad-sha'
        passed, report = self.run_fixture()
        self.assertFalse(passed)
        self.assertIn('matching stable identities', report['error'])


if __name__ == '__main__': unittest.main()
