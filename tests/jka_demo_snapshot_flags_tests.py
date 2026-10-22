"""Academy demo snapshot headers: the decoder and the retail-matched flag contract.

A server-side demo starts at the gamestate, so it keeps every snapshot sent
while the client was still loading. Those carry SNAPFLAG_NOT_ACTIVE, as retail
jamp.exe 1.01 writes them (SV_WriteSnapshotToClient 0x00460c93), and playback
skips them one per frame, as retail does (CL_FirstSnapshot 0x00414a20,
CL_SetCGameTime 0x00414a90). Record: docs/fntech3/2026-10-14-jka-demo-snapshot-flags.md.
"""
from pathlib import Path
import re
import struct
import sys
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
import jka_demo_evidence as evidence

CAPTURE = ROOT / '.tmp/jka-compat/maintained-demo-01/capture/demos'


def encoder_table():
    text = (ROOT / 'code/qcommon/huffman_static.c').read_text()
    body = re.search(r'HuffmanEncoderTable\[ 256 \] =\s*\{(.*?)\};', text, re.S).group(1)
    table = [int(word) for word in body.split(',') if word.strip()]
    assert len(table) == 256
    return table


class MessageWriter:
    """MSG_WriteBits for a Huffman-coded message, from the engine's encoder table."""

    table = encoder_table()

    def __init__(self):
        self.bits_out = []

    def bits(self, value, count):
        value &= (1 << count) - 1
        raw = count & 7
        for index in range(raw):
            self.bits_out.append(value >> index & 1)
        for index in range(raw, count, 8):
            entry = self.table[value >> index & 0xff]
            code, length = entry >> 4 & 0x7ff, entry & 15
            self.bits_out.extend(code >> bit & 1 for bit in range(length))
        return self

    def long(self, value):
        return self.bits(value, 32)

    def string(self, text):
        for byte in text.encode('latin1') + b'\0':
            self.bits(byte, 8)
        return self

    def data(self):
        out = bytearray((len(self.bits_out) + 7) // 8)
        for index, bit in enumerate(self.bits_out):
            out[index >> 3] |= bit << (index & 7)
        return bytes(out)


def snapshot(server_time, delta, flags, commands=()):
    message = MessageWriter().long(len(commands))
    for number, text in commands:
        message.bits(evidence.SVC_SERVER_COMMAND, 8).long(number).string(text)
    # Header, then an empty area mask; the decoder stops after the flags.
    return message.bits(evidence.SVC_SNAPSHOT, 8).long(server_time).bits(delta, 8).bits(flags, 8).bits(0, 8).data()


def demo(messages):
    raw = b''.join(struct.pack('<ii', sequence, len(data)) + data
        for sequence, data in enumerate(messages, 1))
    return raw + struct.pack('<ii', -1, -1)


def gamestate():
    return MessageWriter().long(0).bits(2, 8).long(0).bits(8, 8).data()


class SnapshotFlagTests(unittest.TestCase):
    def test_every_byte_symbol_round_trips_through_the_engine_tables(self):
        writer = MessageWriter()
        for value in range(256):
            writer.bits(value, 8)
        writer.bits(5, 3).long(-123456789)
        reader = evidence._MessageBits(writer.data())
        self.assertEqual([reader.bits(8) for _ in range(256)], list(range(256)))
        self.assertEqual((reader.bits(3), reader.long()), (5, -123456789))

    def test_loading_snapshots_lead_and_only_active_ones_count_as_play(self):
        raw = demo([gamestate(),
            snapshot(5350, 0, 6), snapshot(5450, 0, 6),
            snapshot(26200, 1, 4, [(1, 'cs 822 "sound/weapons/force/speedloop.wav"'), (7, 'spc')]),
            snapshot(26250, 1, 4), snapshot(26300, 0, 5)])
        rows = evidence.snapshot_flags(raw)
        self.assertEqual([(row['record'], row['serverTime'], row['deltaNum'], row['snapFlags']) for row in rows],
            [(1, 5350, 0, 6), (2, 5450, 0, 6), (3, 26200, 1, 4), (4, 26250, 1, 4), (5, 26300, 0, 5)])
        self.assertEqual(evidence.snapshot_summary(raw), dict(snapshotCount=5, activeSnapshotCount=3,
            notActiveSnapshotCount=2, leadingNotActiveCount=2, flagCounts={'4': 2, '5': 1, '6': 2},
            firstServerTime=5350, lastServerTime=26300, firstActiveRecord=3,
            firstActiveServerTime=26200, activeSpanMilliseconds=100))

    def test_a_demo_that_never_goes_active_has_no_active_span(self):
        summary = evidence.snapshot_summary(demo([gamestate(), snapshot(100, 0, 2), snapshot(150, 0, 6)]))
        self.assertEqual((summary['activeSnapshotCount'], summary['leadingNotActiveCount']), (0, 2))
        self.assertNotIn('activeSpanMilliseconds', summary)

    def test_truncated_or_unbounded_messages_are_refused(self):
        whole = snapshot(26200, 0, 4, [(1, 'spc')])
        for cut in (1, 3, 6):
            with self.subTest(cut=cut), self.assertRaises(ValueError):
                evidence.snapshot_flags(demo([gamestate(), whole[:cut]]))
        with patch.object(evidence, 'MAX_COMMAND_CHARS', 3), self.assertRaises(ValueError):
            evidence.snapshot_flags(demo([gamestate(), whole]))
        with self.assertRaises(ValueError):
            evidence.snapshot_flags(demo([gamestate()])[:-1])

    def test_server_marks_a_loading_client_snapshot_not_active_as_retail_does(self):
        source = (ROOT / 'code/server/sv_snapshot.cpp').read_text()
        writer = source[source.index('static void SV_WriteSnapshotToClient'):]
        writer = writer[:writer.index('\n}\n')]
        self.assertRegex(writer, r'if \( client->state != CS_ACTIVE \) \{\s*snapFlags \|= SNAPFLAG_NOT_ACTIVE;')

    def test_client_ignores_not_active_snapshots_until_play_begins_as_retail_does(self):
        source = (ROOT / 'code/client/cl_cgame.cpp').read_text()
        first = source[source.index('static void CL_FirstSnapshot( void ) {'):]
        self.assertRegex(first[:200], r'\{\s*// ignore snapshots that don\'t have entities\s*'
            r'if \( cl\.snap\.snapFlags & SNAPFLAG_NOT_ACTIVE \) \{\s*return;')

    @unittest.skipUnless((CAPTURE / 'server.dm_26').is_file(), 'local 2026-10-02 capture absent')
    def test_maintained_capture_server_demo_has_a_1600_ms_active_span(self):
        server = evidence.snapshot_summary((CAPTURE / 'server.dm_26').read_bytes())
        self.assertEqual((server['leadingNotActiveCount'], server['activeSnapshotCount'],
            server['firstActiveServerTime'], server['activeSpanMilliseconds']), (406, 33, 26200, 1600))
        client = evidence.snapshot_summary((CAPTURE / 'client.dm_26').read_bytes())
        self.assertEqual((client['notActiveSnapshotCount'], client['firstActiveServerTime']), (0, 26300))


if __name__ == '__main__':
    unittest.main()
