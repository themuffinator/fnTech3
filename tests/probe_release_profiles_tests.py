"""Reject unrelated status replies as evidence for a shared release binary."""
import importlib.util
from pathlib import Path
import socket
import threading
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('release_profile_probe', ROOT / 'scripts/probe_release_profiles.py')
probe = importlib.util.module_from_spec(spec)
spec.loader.exec_module(probe)


def response(protocol=68, mapname='q3dm1', challenge='request', extra='', kind='infoResponse'):
    protocol_field = f'\\protocol\\{protocol}' if protocol is not None else ''
    ending = '\n' if kind == 'statusResponse' else ''
    return (b'\xff\xff\xff\xff' + kind.encode('ascii') + b'\n' +
            f'{protocol_field}\\mapname\\{mapname}\\challenge\\{challenge}{extra}{ending}'.encode('latin1'))


class ReleaseProfileProbeEvidenceTests(unittest.TestCase):
    def test_own_protocol_map_and_challenge_are_required(self):
        for profile, mapname in [('q3', 'q3dm1'), ('ql', 'campgrounds'), ('rtcw-mp', 'mp_beach')]:
            protocol = probe.PROTOCOLS[profile]
            with self.subTest(profile=profile):
                expected = {'protocol': str(protocol), 'mapname': mapname, 'challenge': 'request'}
                self.assertEqual(probe.server_info(response(protocol, mapname), 'infoResponse', mapname, 'request', protocol), expected)
                self.assertIsNone(probe.server_info(response(protocol + 1, mapname), 'infoResponse', mapname, 'request', protocol))
                self.assertIsNone(probe.server_info(response(None, mapname), 'infoResponse', mapname, 'request', protocol))
                self.assertIsNone(probe.server_info(response(protocol, 'foreign_map'), 'infoResponse', mapname, 'request', protocol))
                self.assertIsNone(probe.server_info(response(protocol, mapname, 'stale'), 'infoResponse', mapname, 'request', protocol))
                # The status contract only promises serverinfo and challenge;
                # Q3/QL/RTCW need not include protocol in that response.
                self.assertIsNotNone(probe.server_info(response(None, mapname, kind='statusResponse'), 'statusResponse', mapname, 'request'))

    def test_malformed_and_ambiguous_replies_are_rejected(self):
        valid = response()
        for payload in [b'\xff\xff\xff\xffinfoResponse',
                        valid.replace(b'infoResponse', b'statusResponse'),
                        valid.replace(b'\\protocol\\68', b'protocol\\68'),
                        valid.replace(b'\\protocol\\68', b'\\protocol'),
                        valid.replace(b'\\challenge\\request', b''),
                        response(extra='\\protocol\\68'), response(extra='\\mapname\\q3dm1'),
                        response(extra='\\challenge\\request'), response(extra='\\\\empty-key'),
                        response(extra='\\unterminated'), response(extra='\\Protocol\\68'),
                        response(extra='\\key\\va\x00lue')]:
            with self.subTest(payload=payload):
                self.assertIsNone(probe.server_info(payload, 'infoResponse', 'q3dm1', 'request', 68))
        self.assertIsNotNone(probe.server_info(valid + b'\x00', 'infoResponse', 'q3dm1', 'request', 68))
        status = response(protocol=None, kind='statusResponse')
        self.assertIsNotNone(probe.server_info(status + b'0 10 "player"\n\x00', 'statusResponse', 'q3dm1', 'request'))
        self.assertIsNone(probe.server_info(status.rstrip(b'\n'), 'statusResponse', 'q3dm1', 'request'))

    def test_bound_port_query_detects_child_fallback_from_the_requested_port(self):
        self.assertEqual(probe.observed_port('"net_port" is:"27960^7" default:"27960^7"\n'), 27960)
        self.assertEqual(probe.observed_port('"net_port" is:"27960^7"\n"net_port" is:"27961^7"\n'), 27961)
        self.assertIsNone(probe.observed_port('Opening IP socket: 127.0.0.1:27960\n'))
        self.assertIsNone(probe.observed_port('"net_port" is:"27960extra^7"\n'))

    def test_udp_request_nonce_changes_and_observed_info_is_preserved(self):
        requests, failures = [], []
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as server:
            server.bind(('127.0.0.1', 0))
            server.settimeout(3)
            port = server.getsockname()[1]

            def reply():
                try:
                    for _ in range(4):
                        request, address = server.recvfrom(65535)
                        requests.append(request)
                        command = request[4:].split(b' ', 1)[0]
                        challenge = request.split(b' ', 1)[1].strip().decode('ascii')
                        is_info = command == b'getinfo'
                        server.sendto(response(protocol=68 if is_info else None, challenge=challenge,
                                               extra='\\sv_hostname\\observed',
                                               kind='infoResponse' if is_info else 'statusResponse'), address)
                except Exception as error:
                    failures.append(error)

            worker = threading.Thread(target=reply)
            worker.start()
            results = [probe.getstatus(port, 68, 'q3dm1') for _ in range(2)]
            worker.join(4)
            self.assertFalse(worker.is_alive())
        self.assertEqual(failures, [])
        self.assertEqual(len(requests), 4)
        self.assertNotEqual(requests[0], requests[2])
        for index, result in enumerate(results):
            info_request, status_request = requests[index * 2:index * 2 + 2]
            self.assertTrue(info_request.startswith(b'\xff\xff\xff\xffgetinfo fntech3-'), info_request)
            self.assertTrue(status_request.startswith(b'\xff\xff\xff\xffgetstatus fntech3-'), status_request)
            challenge = info_request.split(b' ', 1)[1].strip().decode('ascii')
            self.assertEqual(status_request.split(b' ', 1)[1].strip().decode('ascii'), challenge)
            self.assertEqual(result['getinfo']['protocol'], '68')
            self.assertNotIn('protocol', result['getstatus'])
            for observed in result.values():
                self.assertEqual(observed['sv_hostname'], 'observed')
                self.assertEqual(observed['challenge'], challenge)

    def test_udp_reply_from_a_foreign_source_port_is_ignored(self):
        failures = []
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as server, \
                socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as foreign:
            server.bind(('127.0.0.1', 0))
            foreign.bind(('127.0.0.1', 0))
            server.settimeout(3)

            def spoof():
                try:
                    request, address = server.recvfrom(65535)
                    challenge = request.split(b' ', 1)[1].strip().decode('ascii')
                    # Even a matching challenge and identity cannot establish
                    # evidence when it came from the wrong server socket.
                    foreign.sendto(response(challenge=challenge), address)
                except Exception as error:
                    failures.append(error)

            worker = threading.Thread(target=spoof)
            worker.start()
            result = probe.getstatus(server.getsockname()[1], 68, 'q3dm1')
            worker.join(4)
            self.assertFalse(worker.is_alive())
        self.assertEqual(failures, [])
        self.assertIsNone(result)

    def test_run_preserves_both_observed_reply_dictionaries(self):
        replies = {'getinfo': {'protocol': '68', 'mapname': 'q3dm1', 'challenge': 'nonce'},
                   'getstatus': {'mapname': 'q3dm1', 'challenge': 'nonce', 'sv_hostname': 'observed'}}
        process = mock.Mock()
        process.poll.side_effect = [None, 0, 0, 0]
        with mock.patch.object(probe, 'WindowsPrivateProcess', return_value=process), \
                mock.patch.object(probe.time, 'sleep'):
            result = probe.run(['unused'], ROOT, ROOT / '.tmp/unused.log', 10, lambda: replies)
        self.assertTrue(result['answeredGetstatus'])
        self.assertEqual(result['serverInfo'], replies['getinfo'])
        self.assertEqual(result['statusInfo'], replies['getstatus'])
        self.assertEqual(result['exitCode'], 0)
        self.assertFalse(result['forcedTermination'])
        process.close.assert_called_once()


if __name__ == '__main__':
    unittest.main()
