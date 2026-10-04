"""Exercise maintained Academy peer safety and evidence without game launches."""
from pathlib import Path
import argparse
import binascii
import contextlib
import io
import json
import math
import os
import struct
import sys
import tempfile
import types
import unittest
from unittest.mock import patch
import zipfile
import zlib

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
import probe_jka_openjk_peer as probe


def pe(path):
    image = bytearray(128)
    image[:2] = b'MZ'
    struct.pack_into('<I', image, 60, 80)
    image[80:84] = b'PE\0\0'
    struct.pack_into('<H', image, 84, 0x14c)
    path.write_bytes(image)


def fixture(folder):
    installation = folder / 'installation'
    (installation / 'base').mkdir(parents=True)
    (installation / 'jamp.exe').write_bytes(b'generated retail identity')
    with zipfile.ZipFile(installation / 'base/assets0.pk3', 'w') as pak:
        for name in ('uix86.dll', 'cgamex86.dll', 'jampgamex86.dll'):
            pak.writestr(name, b'generated module identity')
    fntech = folder / 'fntech'
    fntech.mkdir()
    pe(fntech / 'fntech3.exe')
    pe(fntech / 'fntech3.ded.exe')
    (fntech / 'fnql_glx_x86.dll').write_bytes(b'generated renderer')
    openjk = folder / 'openjk'
    openjk.mkdir()
    pe(openjk / 'openjk.x86.exe')
    pe(openjk / 'openjkded.x86.exe')
    (openjk / 'rd-vanilla_x86.dll').write_bytes(b'generated renderer')
    (openjk / 'SDL2.dll').write_bytes(b'generated dependency')
    return argparse.Namespace(installation=installation, client=fntech / 'fntech3.exe',
        server=fntech / 'fntech3.ded.exe', openjk_directory=openjk,
        output=folder / 'evidence', direction='fntech3-to-openjk',
        server_only=False, renderer='glx', map='mp/ffa1', pure=False, frames=3, timeout=5)


def png_header(width=800, height=600):
    def chunk(kind, body):
        return struct.pack('>I', len(body)) + kind + body + struct.pack('>I', binascii.crc32(kind + body) & 0xffffffff)
    header = struct.pack('>IIBBBBB', width, height, 8, 2, 0, 0, 0)
    pixels = zlib.compress(bytes((width * 3 + 1) * height), level=1)
    return b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', header) + chunk(b'IDAT', pixels) + chunk(b'IEND', b'')


def demo_records():
    return b''.join(struct.pack('<ii', sequence, 1) + b'\x00' for sequence in (10, 11, 12)) + struct.pack('<ii', -1, -1)


class Process:
    EXIT_GRACE = 5
    name = 'fixture-unswitched-desktop'
    events = []
    frontier = False
    stale = False
    running = False
    bad_capture = False
    active = True
    begin = True
    disconnect = True
    drop = True
    unpure = False
    server_error = False
    malformed_demo = False

    def __init__(self, command, cwd, stdout):
        self.server = 'dedicated' in command
        self.terminated = False
        home = Path(command[command.index('fs_homepath') + 1])
        self.events.append((command, dict(TEMP=os.environ.get('TEMP'), TMP=os.environ.get('TMP'))))
        native = home / 'os-temp/native.tmp'
        native.write_bytes(b'generated cache fixture')
        stdout.write_text('fixture stdout\n')
        if self.server:
            text = f'Going from CS_PRIMED to CS_ACTIVE for {probe.CLIENT_NAME}\n' if self.active else ''
            if self.drop:
                text += (f'Going to CS_ZOMBIE for {probe.CLIENT_NAME}\n' if Path(command[0]).name == 'openjkded.x86.exe'
                         else f'Going from CS_ACTIVE to CS_ZOMBIE for {probe.CLIENT_NAME}\n')
            if self.unpure:
                text += 'broadcast: print "Unpure client detected. Invalid pack files referenced!"\n'
            if self.server_error:
                text += 'ERROR: Unsupported server teardown import\n'
            game = '0:00 ClientBegin: 0\n' if self.begin else ''
            if self.disconnect:
                game += '0:10 ClientDisconnect: 0\n'
            (home / 'base/games.log').write_text(game)
            if 'set sv_autoRecordDemos "1"' in (home / 'base' / probe.CONFIG).read_text():
                text += 'Server demo recording to base/demos/server/generated\nStopped server demo recording: base/demos/server/generated.dm_26\n'
                demos = home / 'base/demos/server'
                demos.mkdir(parents=True)
                (demos / 'generated.dm_26').write_bytes(demo_records())
        else:
            text = '\n'.join(('MODE: -1, 800 x 600 windowed hz:', probe.MARKER,
                '   snapshot:10  delta:-1  ping:5',
                '   snapshot:10  delta:-1  ping:5' if self.stale else '   snapshot:11  delta:10  ping:6')) + '\n'
            if self.frontier:
                text += 'ERROR: Unsupported jka-mp cgame import 119\n'
            if 'record jka-maintained-client;' in (home / 'base' / probe.CONFIG).read_text():
                text += 'recording to demos/jka-maintained-client.dm_26.\nStopped demo.\n'
                demos = home / 'base/demos'
                demos.mkdir(parents=True)
                raw = demo_records()
                (demos / 'jka-maintained-client.dm_26').write_bytes(raw[:-1] if self.malformed_demo else raw)
            if Path(command[0]).name == 'openjk.x86.exe':
                text = ''.join('2026-10-02 19:29:44 ' + line + '\n' for line in text.splitlines())
            screenshots = home / 'base/screenshots'
            screenshots.mkdir()
            (screenshots / 'jka-maintained-peer.png').write_bytes(png_header(640 if self.bad_capture else 800))
        (home / 'base/qconsole.log').write_text(text)

    def poll(self):
        return None if self.server or (self.running and not self.terminated) else 0

    def close(self):
        self.terminated = True


class FakeSocket:
    def __init__(self, replies): self.replies = iter(replies)
    def __enter__(self): return self
    def __exit__(self, *args): pass
    def settimeout(self, value): pass
    def sendto(self, payload, address): pass
    def recvfrom(self, size): return next(self.replies)


class MaintainedPeerTests(unittest.TestCase):
    def setUp(self):
        self.parent = ROOT / '.tmp/runtime/jka-openjk-tests'
        self.parent.mkdir(parents=True, exist_ok=True)
        self.temporary = tempfile.TemporaryDirectory(dir=self.parent)
        self.folder = Path(self.temporary.name).resolve()
        self.args = fixture(self.folder)
        Process.events = []

    def tearDown(self):
        self.assertTrue(self.folder.is_relative_to(self.parent.resolve()))
        for path in (self.folder, *self.folder.rglob('*')):
            self.assertFalse(path.is_symlink() or getattr(path.lstat(), 'st_file_attributes', 0) & 0x400)
        self.temporary.cleanup()

    def run_fixture(self, readiness_error=None, **options):
        with contextlib.ExitStack() as stack:
            stack.enter_context(patch.object(probe, 'WindowsPrivateProcess', Process))
            stack.enter_context(patch.object(probe, 'ready_identity', side_effect=readiness_error,
                return_value={'getinfo': {}, 'getstatus': {}}))
            stack.enter_context(contextlib.redirect_stdout(io.StringIO()))
            for name, value in options.items():
                stack.enter_context(patch.object(Process, name, value))
            passed = probe.run(self.args)
        return passed, json.loads((self.args.output / 'result.json').read_text())

    def test_server_crash_before_readiness_is_recorded_before_cleanup_without_launching_peer(self):
        passed, report = self.run_fixture(readiness_error=RuntimeError('No ready server'),
            poll=lambda process: 0xc0000005 if process.server else 0)
        self.assertFalse(passed)
        self.assertEqual(report['serverExitBeforeCleanup'], 0xc0000005)
        self.assertFalse(report['serverForced'])
        self.assertNotIn('clientDesktop', report)
        self.assertEqual(len(Process.events), 1)
        self.assertIn('No ready server', report['error'])
        self.assertTrue(report['homesRemoved'])

    def test_both_directions_use_selected_stable_binaries_windowed_loopback_and_config(self):
        for direction, server_name, client_name in (
                ('fntech3-to-openjk', 'openjkded.x86.exe', 'fntech3.exe'),
                ('openjk-to-fntech3', 'fntech3.ded.exe', 'openjk.x86.exe')):
            self.args.direction = direction
            install, output, server, client, renderer, se, ce = probe.validate(self.args)
            self.assertEqual(server.name, server_name)
            self.assertEqual(client.name, client_name)
            for exe, engine, hostname in ((server, se, 'fixture-host'), (client, ce, None)):
                command = probe.launch_command(exe, engine, install, output / ('s' if hostname else 'c'), self.args, 27961, hostname)
                for name, value in {'r_fullscreen': '0', 'in_mouse': '0', 'in_keyboard': '0',
                        'in_joystick': '0', 'net_ip': '127.0.0.1'}.items():
                    self.assertEqual(command[command.index(name) + 1], value)
                if engine == 'fntech3':
                    self.assertEqual(command[command.index('com_gameProfile') + 1], 'jka-mp')
                else:
                    self.assertEqual(command[command.index('com_unpackLibraries') + 1], '1')
                self.assertIn('+exec', command)
                self.assertEqual(command[-2:], ['+map', 'mp/ffa1'] if hostname else ['+connect', '127.0.0.1:27961'])
                self.assertLess(len(probe.subprocess.list2cmdline(command[1:])), 1024)
            config = probe.configuration(ce, 3)
            self.assertIn('set cl_shownet "3"', config)
            self.assertIn('screenshotPNG' if ce == 'fntech3' else 'screenshot_png', config)

    def test_ready_only_never_resolves_or_launches_a_shared_fntech_binary(self):
        self.args.server_only = True
        self.args.client = self.folder / 'missing.exe'
        self.args.server = self.folder / 'missing.ded.exe'
        passed, report = self.run_fixture()
        self.assertTrue(passed)
        self.assertIsNone(report['clientCommand'])
        self.assertEqual(len(Process.events), 1)
        self.assertEqual(Path(Process.events[0][0][0]).name, 'openjkded.x86.exe')
        self.assertTrue(report['homesRemoved'])

    def test_recording_is_opt_in_and_only_the_known_reverse_pair_is_allowed(self):
        self.assertNotIn('record jka-maintained-client;', probe.configuration('openjk', 3))
        self.assertIn('set sv_autoRecordDemos "0"', probe.configuration('fntech3', 3, 'host'))
        self.args.record_demos = True
        with self.assertRaises(ValueError): probe.validate(self.args)
        self.args.direction = 'openjk-to-fntech3'
        passed, report = self.run_fixture()
        self.assertTrue(passed)
        self.assertEqual({row['origin'] for row in report['demos']}, {'server', 'client'})
        self.assertTrue(all(row['exactEOF'] and row['recordCount'] == 3 for row in report['demos']))
        self.assertTrue(report['homesRemoved'])
        self.assertFalse(list(self.args.output.rglob('*.tmp')))

    def test_malformed_generated_demo_refuses_retention_and_success(self):
        self.args.record_demos = True
        self.args.direction = 'openjk-to-fntech3'
        passed, report = self.run_fixture(malformed_demo=True)
        self.assertFalse(passed)
        self.assertIn('EOF', report['demoError'])
        self.assertEqual(report['demos'], [])
        self.assertFalse((self.args.output / 'demos').exists())
        self.assertTrue(report['homesRemoved'])

    def test_valid_admission_requires_delta_snapshot_begin_capture_and_preserves_assets(self):
        before = probe.inventory(self.args.installation)
        for direction in ('fntech3-to-openjk', 'openjk-to-fntech3'):
            self.args.direction = direction
            self.args.output = self.folder / direction
            passed, report = self.run_fixture()
            self.assertTrue(passed)
            self.assertTrue(report['serverBegin'] and report['serverActive'] and report['deltaSnapshotObserved'])
            self.assertTrue(report['serverDisconnect'] and report['serverDropCompleted'] and report['disconnectDrainComplete'])
            self.assertEqual(report['validSnapshotCount'], 2)
            self.assertEqual(len(report['screenshots']), 1)
            self.assertIsNone(report['visualPassed'])
            self.assertTrue(report['homesRemoved'] and report['binariesUnchanged'] and report['installationUnchanged'])
            self.assertFalse(list(self.args.output.rglob('*.dll')))
        self.assertEqual(before, probe.inventory(self.args.installation))

    def test_partial_or_stale_evidence_cannot_pass(self):
        for name in ('frontier', 'stale', 'bad_capture', 'active', 'begin', 'disconnect', 'drop'):
            self.args.output = self.folder / name
            passed, report = self.run_fixture(**{name: False if name in ('active', 'begin', 'disconnect', 'drop') else True})
            self.assertFalse(passed)
            self.assertTrue(report['homesRemoved'])

    def test_explicit_frontier_stops_owned_modal_child_without_waiting_for_timeout(self):
        passed, report = self.run_fixture(frontier=True, running=True)
        self.assertFalse(passed)
        self.assertTrue(report['clientForced'] and report['homesRemoved'])
        self.assertIn('Unsupported jka-mp cgame import', report['earlyStopReason'])

    def test_disconnect_drain_requires_return_from_module_and_is_bounded(self):
        host = types.SimpleNamespace(poll=lambda: None)
        begin = 'ClientDisconnect: 0\n'
        complete = f'Going from CS_ACTIVE to CS_ZOMBIE for {probe.CLIENT_NAME}\n'
        with (patch.object(probe, 'read', side_effect=[begin, '', complete, '']),
              patch.object(probe.time, 'sleep') as sleep):
            self.assertTrue(probe.drain_disconnect(host, self.folder, self.folder / 'stdout'))
            sleep.assert_called_once_with(.05)
        with (patch.object(probe, 'read', return_value=''),
              patch.object(probe.time, 'monotonic', side_effect=[0,0,3]),
              patch.object(probe.time, 'sleep') as sleep):
            self.assertFalse(probe.drain_disconnect(host, self.folder, self.folder / 'stdout'))
            sleep.assert_called_once_with(.05)
        with patch.object(probe, 'read', return_value='ERROR: cleanup import refused\n'):
            self.assertFalse(probe.drain_disconnect(host, self.folder, self.folder / 'stdout'))

    def test_server_pure_rejection_or_named_error_stops_owned_client_early(self):
        for fault in ('unpure', 'server_error'):
            self.args.output = self.folder / fault
            with patch.object(probe.time, 'sleep') as sleep:
                passed, report = self.run_fixture(running=True, **{fault: True})
            self.assertFalse(passed)
            self.assertTrue(report['clientForced'] and report['homesRemoved'])
            self.assertEqual(report['earlyStopSource'], 'server')
            self.assertIn('Unpure client detected' if fault == 'unpure' else 'Unsupported server', report['earlyStopReason'])
            sleep.assert_not_called()

    def test_child_temp_is_project_contained_and_parent_environment_is_restored(self):
        original = {key: os.environ.get(key) for key in ('TEMP', 'TMP')}
        self.run_fixture()
        self.assertEqual(original, {key: os.environ.get(key) for key in ('TEMP', 'TMP')})
        for command, environment in Process.events:
            home = Path(command[command.index('fs_homepath') + 1])
            self.assertEqual(environment['TEMP'], str(home / 'os-temp'))
            self.assertEqual(environment['TMP'], str(home / 'os-temp'))

    def test_rejects_bad_time_map_architecture_overlap_and_reused_evidence(self):
        for timeout in (0, 601, math.inf, math.nan):
            self.args.timeout = timeout
            with self.assertRaises(ValueError): probe.validate(self.args)
        self.args.timeout = 5
        for name in ('../map', 'mp/map;quit', 'mp/map\nquit'):
            self.args.map = name
            with self.assertRaises(ValueError): probe.validate(self.args)
        self.args.map = 'mp/ffa1'
        self.args.output = self.args.openjk_directory / 'evidence'
        with self.assertRaises(ValueError): probe.validate(self.args)
        self.args.output = self.folder / 'evidence'
        self.args.output.mkdir()
        with self.assertRaises(ValueError): probe.validate(self.args)
        self.args.output.rmdir()
        raw = bytearray(self.args.client.read_bytes())
        struct.pack_into('<H', raw, 84, 0x8664)
        self.args.client.write_bytes(raw)
        with self.assertRaises(ValueError): probe.validate(self.args)
        self.args.server_only = True
        self.args.direction = 'openjk-to-fntech3'
        with self.assertRaises(ValueError): probe.validate(self.args)

    def test_constructor_failure_preserves_home_and_writes_failure(self):
        with patch.object(probe, 'WindowsPrivateProcess', side_effect=OSError('partial constructor fixture')), contextlib.redirect_stdout(io.StringIO()):
            self.assertFalse(probe.run(self.args))
        report = json.loads((self.args.output / 'result.json').read_text())
        self.assertFalse(report['homesRemoved'])
        self.assertIn('partial constructor fixture', report['error'])

    def test_cleanup_failure_preserves_report_and_refuses_success(self):
        with patch.object(probe, 'remove_home', side_effect=RuntimeError('fixture linked directory')):
            passed, report = self.run_fixture()
        self.assertFalse(passed)
        self.assertIn('linked directory', report['cleanupError'])

    def test_engine_png_needs_complete_bounded_crc_checked_pixel_data(self):
        valid = png_header()
        self.assertTrue(probe.valid_capture(valid))
        for image in (valid[:33], valid[:-1], valid + b'trailing', png_header(640), b'generated capture'):
            self.assertFalse(probe.valid_capture(image))
        bad = bytearray(valid)
        bad[50] ^= 0x40
        self.assertFalse(probe.valid_capture(bad))

    def test_ready_query_requires_own_host_nonce_map_protocol_address_and_empty_server(self):
        nonce, hostname = 'fixture-nonce', 'fixture-host'
        def response(kind, protocol='26', host=hostname, challenge=nonce, players=''):
            key = 'hostname' if kind == 'infoResponse' else 'sv_hostname'
            body = f'\\challenge\\{challenge}\\mapname\\mp/ffa1\\{key}\\{host}\\protocol\\{protocol}'
            return b'\xff\xff\xff\xff' + kind.encode() + b'\n' + body.encode() + (b'\n' + players.encode() if kind == 'statusResponse' else b'')
        replies = [
            (response('infoResponse', host='other-server'), ('127.0.0.1', 27961)),
            (response('statusResponse', players='0 5 "another client"\n'), ('127.0.0.1', 27961)),
            (response('infoResponse', protocol='25'), ('127.0.0.1', 27961)),
            (response('statusResponse', challenge='stale'), ('127.0.0.1', 27961)),
            (response('infoResponse'), ('127.0.0.1', 27962)),
            (response('statusResponse'), ('127.0.0.1', 27961)),
            (response('infoResponse'), ('127.0.0.1', 27961)),
            (response('statusResponse'), ('127.0.0.1', 27961))]
        with (patch.object(probe.socket, 'socket', return_value=FakeSocket(replies)),
                patch.object(probe.uuid, 'uuid4', return_value=types.SimpleNamespace(hex=nonce))):
            result = probe.ready_identity(types.SimpleNamespace(poll=lambda: None), 27961, 'mp/ffa1', hostname)
        self.assertEqual(set(result), {'getinfo', 'getstatus'})
        self.assertEqual(result['getinfo']['fields']['protocol'], '26')
        self.assertIn('packetHex', result['getstatus'])


class JKABlankNameDropEvidenceTests(unittest.TestCase):
    def test_empty_name_transition_requires_ordered_matching_disconnect(self):
        name = probe.CLIENT_NAME
        command = f'clientCommand: {name} : 3 : disconnect\n'
        callback = 'ClientDisconnect: 0\n'
        transition = 'Going to CS_ZOMBIE for \n'
        complete = command + callback + transition
        self.assertTrue(probe.drop_completed(complete))
        timestamped = ''.join('2026-10-03 17:29:17 ' + line + '\n' for line in complete.splitlines())
        self.assertTrue(probe.drop_completed(timestamped))
        self.assertTrue(probe.drop_completed(command + 'ClientDisconnect: 0 [owned] (generated)\n' + transition))
        self.assertTrue(probe.drop_completed('Going to CS_ZOMBIE for ' + name + '\n'))
        for partial in (transition, callback + transition, command + transition,
                        command + callback, callback + command + transition,
                        command + transition + callback,
                        complete.replace(name, 'different-client'),
                        complete.replace('ClientDisconnect: 0', 'ClientDisconnect: 1'),
                        complete.replace('CS_ZOMBIE for ', 'CS_ZOMBIE for different-client'),
                        complete.replace(' : disconnect', ' : userinfo'),
                        complete.replace(' : 3 : ', ' : invalid : ')):
            with self.subTest(trace=partial):
                self.assertFalse(probe.drop_completed(partial))


if __name__ == '__main__': unittest.main()
