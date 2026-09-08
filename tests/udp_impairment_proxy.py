"""Loopback-only UDP relay for controlled id Tech netchannel impairment.

Payloads are forwarded unchanged. Connectionless messages are preserved;
sequenced datagrams can be dropped, delayed or duplicated deterministically.
"""
import hashlib, heapq, select, socket, struct, threading, time

class UdpImpairmentProxy:

    def __init__(self, server_port, drop_every=17, delay_every=19, duplicate_every=23, delay_seconds=0.075, drop_first_fragment=False):
        if not 0 < server_port < 65536 or min(drop_every, delay_every, duplicate_every) < 0 or (not 0 <= delay_seconds <= 1):
            raise ValueError('invalid loopback impairment settings')
        self.server = ('127.0.0.1', server_port)
        self.settings = dict(drop_every=drop_every, delay_every=delay_every, duplicate_every=duplicate_every, delay_seconds=delay_seconds, drop_first_fragment=drop_first_fragment)
        self.front = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.back = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        try:
            self.front.bind(('127.0.0.1', 0))
            self.back.bind(('127.0.0.1', 0))
        except BaseException:
            self.front.close()
            self.back.close()
            raise
        self.port = self.front.getsockname()[1]
        self.client = None
        self.stop = threading.Event()
        self.pending = []
        self.serial = 0
        self.error = None
        self.stats = {d: dict(received=0, connectionless=0, sequenced=0, fragments=0, dropped=0, delayed=0, duplicated=0, forwarded=0, overtakes=0, reorderedForwards=0, maxDatagramBytes=0) for d in ('to_server', 'to_client')}
        self.last_sequence = {d: None for d in self.stats}
        self.fragment_drops = {d: [] for d in self.stats}
        self.hashes = {d: hashlib.sha256() for d in self.stats}
        self.thread = threading.Thread(target=self._run, name='fntech3-loopback-relay', daemon=True)
        self.thread.start()

    def _send(self, direction, data):
        if direction == 'to_server':
            self.back.sendto(data, self.server)
        elif self.client:
            self.front.sendto(data, self.client)
        else:
            return
        self.stats[direction]['forwarded'] += 1
        if len(data) >= 4 and data[:4] != b'\xff' * 4:
            raw = struct.unpack_from('<I', data)[0]
            offset = 0
            fragment_at = 6 if direction == 'to_server' else 4
            if raw & 2147483648 and len(data) >= fragment_at + 2:
                offset = struct.unpack_from('<H', data, fragment_at)[0]
            sequence = (raw & 2147483647, offset)
            previous = self.last_sequence[direction]
            if previous is not None and sequence < previous:
                self.stats[direction]['reorderedForwards'] += 1
            self.last_sequence[direction] = sequence

    def _schedule(self, direction, data, delay):
        self.serial += 1
        heapq.heappush(self.pending, (time.monotonic() + delay, self.serial, direction, data))

    def _receive(self, direction, data):
        stat = self.stats[direction]
        stat['received'] += 1
        stat['maxDatagramBytes'] = max(stat['maxDatagramBytes'], len(data))
        self.hashes[direction].update(struct.pack('<I', len(data)) + data)
        if len(data) < 4 or data[:4] == b'\xff' * 4:
            stat['connectionless'] += 1
            self._send(direction, data)
            return
        stat['sequenced'] += 1
        n = stat['sequenced']
        sequence = struct.unpack_from('<I', data)[0]
        if sequence & 2147483648:
            stat['fragments'] += 1
            if self.settings['drop_first_fragment'] and not self.fragment_drops[direction]:
                fragment_at = 6 if direction == 'to_server' else 4
                self.fragment_drops[direction].append({
                    'sequence': sequence & 0x7fffffff,
                    'fragmentStart': struct.unpack_from('<H', data, fragment_at)[0] if len(data) >= fragment_at + 2 else None,
                    'datagramBytes': len(data),
                })
                stat['dropped'] += 1
                return
        drop = self.settings['drop_every']
        delay = self.settings['delay_every']
        duplicate = self.settings['duplicate_every']
        if drop and n % drop == 0 or len(self.pending) >= 128:
            stat['dropped'] += 1
            return
        if delay and n % delay == 0:
            stat['delayed'] += 1
            self._schedule(direction, data, self.settings['delay_seconds'])
        else:
            if any((item[2] == direction for item in self.pending)):
                stat['overtakes'] += 1
            self._send(direction, data)
        if duplicate and n % duplicate == 0:
            stat['duplicated'] += 1
            self._schedule(direction, data, 0.01)

    def _run(self):
        try:
            while not self.stop.is_set():
                ready, _, _ = select.select([self.front, self.back], [], [], 0.005)
                for source in ready:
                    try:
                        data, address = source.recvfrom(65535)
                    except ConnectionResetError:
                        continue
                    if source is self.front:
                        if self.client is None:
                            self.client = address
                        if address == self.client:
                            self._receive('to_server', data)
                    elif address == self.server:
                        self._receive('to_client', data)
                while self.pending and self.pending[0][0] <= time.monotonic():
                    _, _, direction, data = heapq.heappop(self.pending)
                    self._send(direction, data)
        except Exception as exc:
            self.error = repr(exc)

    def close(self):
        self.stop.set()
        self.thread.join(2)
        if self.thread.is_alive():
            raise RuntimeError('loopback relay did not stop')
        self.front.close()
        self.back.close()
        return {'settings': self.settings, 'stats': self.stats, 'targetedFragmentDrops': self.fragment_drops, 'receivedStreamSha256': {d: h.hexdigest() for d, h in self.hashes.items()}, 'pendingAtStop': len(self.pending), 'error': self.error}
