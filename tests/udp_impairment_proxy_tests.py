"""Exercise impairment policy over real loopback UDP sockets."""
import socket
import struct
import unittest
from udp_impairment_proxy import UdpImpairmentProxy

class UdpImpairmentProxyTests(unittest.TestCase):

    def test_first_fragment_drop_is_independent_per_direction(self):
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as server, socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as client:
            server.bind(('127.0.0.1', 0))
            server.settimeout(1)
            client.bind(('127.0.0.1', 0))
            client.settimeout(1)
            relay = UdpImpairmentProxy(server.getsockname()[1], drop_every=0, delay_every=0, duplicate_every=0, drop_first_fragment=True)
            try:
                oob = b'\xff' * 4 + b'connect'
                client.sendto(oob, ('127.0.0.1', relay.port))
                self.assertEqual((received := server.recvfrom(100))[0], oob)
                peer = received[1]
                for direction, sender, destination, receiver in (
                    ('to_server', client, ('127.0.0.1', relay.port), server),
                    ('to_client', server, peer, client),
                ):
                    qport = struct.pack('<H', 1234) if direction == 'to_server' else b''
                    fragment = struct.pack('<I', 0x80000001) + qport + struct.pack('<HH', 0, 4) + b'test'
                    sender.sendto(fragment, destination)
                    # Only the first fragment is dropped; identical retransmission survives.
                    sender.sendto(fragment, destination)
                    self.assertEqual(receiver.recvfrom(100)[0], fragment)
            finally:
                record = relay.close()
            self.assertIsNone(record['error'])
            for direction in ('to_server', 'to_client'):
                self.assertEqual(record['stats'][direction]['fragments'], 2)
                self.assertEqual(record['stats'][direction]['dropped'], 1)
                drops = record['targetedFragmentDrops'][direction]
                self.assertEqual(len(drops), 1)
                self.assertEqual(drops[0]['sequence'], 1)
                self.assertEqual(drops[0]['fragmentStart'], 0)

    def test_second_fragment_drop_preserves_first_and_retransmission(self):
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as server, socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as client:
            server.bind(('127.0.0.1', 0)); server.settimeout(1); client.bind(('127.0.0.1', 0)); client.settimeout(1)
            relay = UdpImpairmentProxy(server.getsockname()[1], drop_every=0, delay_every=0, duplicate_every=0, drop_fragment_number=2)
            try:
                client.sendto(b'\xff' * 4 + b'connect', ('127.0.0.1', relay.port))
                _, peer = server.recvfrom(100)
                for direction, sender, destination, receiver in (
                    ('to_server', client, ('127.0.0.1', relay.port), server),
                    ('to_client', server, peer, client),
                ):
                    qport = struct.pack('<H', 1234) if direction == 'to_server' else b''
                    first = struct.pack('<I', 0x80000001) + qport + struct.pack('<HH', 0, 4) + b'abcd'
                    second = struct.pack('<I', 0x80000001) + qport + struct.pack('<HH', 4, 4) + b'efgh'
                    sender.sendto(first, destination)
                    self.assertEqual(receiver.recvfrom(100)[0], first)
                    sender.sendto(second, destination)
                    sender.sendto(second, destination)
                    self.assertEqual(receiver.recvfrom(100)[0], second)
            finally:
                record = relay.close()
            self.assertIsNone(record['error'])
            for direction in ('to_server', 'to_client'):
                self.assertEqual(record['stats'][direction]['fragments'], 3)
                self.assertEqual(record['stats'][direction]['dropped'], 1)
                self.assertEqual(record['targetedFragmentDrops'][direction][0]['fragmentStart'], 4)
                self.assertEqual(record['targetedFragmentDrops'][direction][0]['fragmentOrdinal'], 2)

    def test_checksummed_second_fragment_metadata(self):
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as server, socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as client:
            server.bind(('127.0.0.1', 0)); server.settimeout(1); client.bind(('127.0.0.1', 0)); client.settimeout(1)
            relay = UdpImpairmentProxy(server.getsockname()[1], drop_every=0, delay_every=0, duplicate_every=0, drop_fragment_number=2, sequence_checksum=True)
            try:
                client.sendto(b'\xff' * 4 + b'connect', ('127.0.0.1', relay.port))
                _, peer = server.recvfrom(100)
                for direction, sender, destination, receiver in (
                    ('to_server', client, ('127.0.0.1', relay.port), server),
                    ('to_client', server, peer, client),
                ):
                    qport = (struct.pack('<H', 1234) if direction == 'to_server' else b'') + struct.pack('<I', 0x12345678)
                    first = struct.pack('<I', 0x80000001) + qport + struct.pack('<HH', 0, 4) + b'abcd'
                    second = struct.pack('<I', 0x80000001) + qport + struct.pack('<HH', 4, 4) + b'efgh'
                    sender.sendto(first, destination)
                    self.assertEqual(receiver.recvfrom(100)[0], first)
                    sender.sendto(second, destination)
                    sender.sendto(second, destination)
                    self.assertEqual(receiver.recvfrom(100)[0], second)
            finally:
                record = relay.close()
            self.assertIsNone(record['error'])
            for direction in ('to_server', 'to_client'):
                self.assertEqual(record['stats'][direction]['fragments'], 3)
                self.assertEqual(record['stats'][direction]['dropped'], 1)
                self.assertEqual(record['targetedFragmentDrops'][direction][0]['fragmentStart'], 4)
                self.assertEqual(record['targetedFragmentDrops'][direction][0]['fragmentOrdinal'], 2)

    def test_terminal_fragment_selection_handles_empty_tail_and_bad_lengths(self):
        for checksum in (False, True):
            for tail in (b'', b'end'):
                with self.subTest(checksum=checksum, tail=tail), socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as server, socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as client:
                    server.bind(('127.0.0.1', 0)); server.settimeout(1); client.bind(('127.0.0.1', 0)); client.settimeout(1)
                    relay = UdpImpairmentProxy(server.getsockname()[1], drop_every=0, delay_every=0, duplicate_every=0, sequence_checksum=checksum, drop_terminal_fragment=True)
                    try:
                        client.sendto(b'\xff' * 4 + b'connect', ('127.0.0.1', relay.port))
                        _, peer = server.recvfrom(2000)
                        for direction, sender, destination, receiver in (
                            ('to_server', client, ('127.0.0.1', relay.port), server),
                            ('to_client', server, peer, client),
                        ):
                            prefix = struct.pack('<I', 0x80000001)
                            if direction == 'to_server': prefix += struct.pack('<H', 1234)
                            if checksum: prefix += struct.pack('<I', 0x12345678)
                            full = prefix + struct.pack('<HH', 0, 1300) + b'x' * 1300
                            bad = prefix + struct.pack('<HH', 1300, 2) + b'x'
                            short_header = prefix + b'\0'
                            for packet in (full, bad, short_header):
                                sender.sendto(packet, destination)
                                self.assertEqual(receiver.recvfrom(2000)[0], packet)
                            terminal = prefix + struct.pack('<HH', 1300, len(tail)) + tail
                            sender.sendto(terminal, destination)
                            sender.sendto(terminal, destination)
                            self.assertEqual(receiver.recvfrom(2000)[0], terminal)
                    finally:
                        record = relay.close()
                    self.assertIsNone(record['error'])
                    for direction in ('to_server', 'to_client'):
                        self.assertEqual(record['stats'][direction]['dropped'], 1)
                        drop = record['targetedFragmentDrops'][direction][0]
                        self.assertEqual(drop['fragmentOrdinal'], 4)
                        self.assertEqual(drop['fragmentLength'], len(tail))
                        self.assertEqual(drop['fragmentStart'], 1300)

    def test_loss_delay_duplication_and_connectionless_preservation(self):
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as server, socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as client:
            server.bind(('127.0.0.1', 0))
            server.settimeout(0.5)
            client.bind(('127.0.0.1', 0))
            client.settimeout(0.5)
            relay = UdpImpairmentProxy(server.getsockname()[1], drop_every=3, delay_every=2, duplicate_every=4, delay_seconds=0.08)
            try:
                oob = b'\xff' * 4 + b'challenge'
                client.sendto(oob, ('127.0.0.1', relay.port))
                packet, peer = server.recvfrom(100)
                self.assertEqual(packet, oob)
                server.sendto(oob, peer)
                self.assertEqual(client.recvfrom(100)[0], oob)
                for n in range(1, 7):
                    client.sendto(struct.pack('<I', n) + b'fixture', ('127.0.0.1', relay.port))
                got = []
                for _ in range(5):
                    got.append(struct.unpack_from('<I', server.recvfrom(100)[0])[0])
                self.assertCountEqual(got, [1, 2, 4, 4, 5])
                self.assertLess(got.index(5), got.index(2))
            finally:
                record = relay.close()
            self.assertIsNone(record['error'])
            s = record['stats']['to_server']
            self.assertEqual(s['dropped'], 2)
            self.assertEqual(s['delayed'], 2)
            self.assertEqual(s['duplicated'], 1)
            self.assertGreater(s['overtakes'], 0)
            self.assertGreater(s['reorderedForwards'], 0)
if __name__ == '__main__':
    unittest.main()
