"""Exercise impairment policy over real loopback UDP sockets."""
import socket
import struct
import unittest
from udp_impairment_proxy import UdpImpairmentProxy

class UdpImpairmentProxyTests(unittest.TestCase):

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
