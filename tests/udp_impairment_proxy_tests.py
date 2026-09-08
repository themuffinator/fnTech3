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
