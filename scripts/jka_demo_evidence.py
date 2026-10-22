"""Bound the public .dm_26 container before retaining a generated Academy demo.

This validates record framing, not compressed message semantics. Successful
playback by the selected maintained client is a separate runtime observation.
snapshot_flags() reads only each snapshot's header (time, delta and flags), so
a report can say which snapshots a client game draws as play rather than as
its information screen (SNAPFLAG_NOT_ACTIVE).
"""
from pathlib import Path
import functools
import hashlib
import json
import os
import re
import stat
import struct
import sys

MAX_DEMO_BYTES = 64 * 1024 * 1024
MAX_MESSAGE_BYTES = 49152
MAX_RECORDS = 10000
MAX_COMMAND_CHARS = 8192
HUFFMAN_SOURCE = Path(__file__).resolve().parents[1] / 'code/qcommon/huffman_static.c'
SVC_SERVER_COMMAND = 5
SVC_SNAPSHOT = 7
SNAPFLAG_NOT_ACTIVE = 2


def validate_container(raw):
    if not 8 <= len(raw) <= MAX_DEMO_BYTES:
        raise ValueError('Academy demo size is outside the bounded capture limit')
    cursor = 0
    sequences = []
    lengths = []
    while cursor + 8 <= len(raw):
        sequence, length = struct.unpack_from('<ii', raw, cursor)
        cursor += 8
        if sequence == -1 or length == -1:
            if (sequence, length) != (-1, -1) or cursor != len(raw):
                raise ValueError('Academy demo needs exact -1/-1 EOF with no trailing bytes')
            if len(sequences) < 3:
                raise ValueError('Academy capture needs gamestate and multiple message records')
            return dict(recordCount=len(sequences), firstSequence=sequences[0],
                lastSequence=sequences[-1], maximumMessageBytes=max(lengths),
                bytes=len(raw), exactEOF=True, selectedProtocol=26)
        if sequence < 0 or sequences and sequence <= sequences[-1]:
            raise ValueError('Academy demo message sequences must strictly increase')
        if not 1 <= length <= MAX_MESSAGE_BYTES or length > len(raw) - cursor:
            raise ValueError('Academy demo has a truncated or oversized message record')
        if len(sequences) >= MAX_RECORDS:
            raise ValueError('Academy demo has too many records for a bounded probe')
        sequences.append(sequence)
        lengths.append(length)
        cursor += length
    raise ValueError('Academy demo has a missing or truncated EOF record')


def generated_demo(path, home):
    """Read only a regular, generated .dm_26 inside the isolated engine home."""
    path, home = Path(path), Path(home)
    if '..' in path.parts or '..' in home.parts:
        raise ValueError('Generated demo paths must not contain traversal')
    path, home = Path(os.path.abspath(path)), Path(os.path.abspath(home))
    current = path
    while True:
        if not current.is_relative_to(home):
            raise ValueError('Generated demo escapes its isolated home')
        meta = current.lstat()
        if stat.S_ISLNK(meta.st_mode) or getattr(meta, 'st_file_attributes', 0) & 0x400:
            raise ValueError('Generated demo traverses a linked path')
        if current == home:
            if not stat.S_ISDIR(meta.st_mode):
                raise ValueError('Generated demo home is not a directory')
            break
        current = current.parent
    meta = path.lstat()
    if not stat.S_ISREG(meta.st_mode) or path.suffix.casefold() != '.dm_26':
        raise ValueError('Retain only a regular generated Academy .dm_26')
    if meta.st_size > MAX_DEMO_BYTES:
        raise ValueError('Generated demo exceeds the bounded capture size')
    with path.open('rb') as source:
        raw = source.read(MAX_DEMO_BYTES + 1)
    details = validate_container(raw)
    details['sha256'] = hashlib.sha256(raw).hexdigest()
    return raw, details


@functools.lru_cache(maxsize=1)
def huffman_decoder_table(source=HUFFMAN_SOURCE):
    """The engine's static message decoder: 11 bits in, symbol and length out."""
    text = Path(source).read_text()
    body = re.search(r'HuffmanDecoderTable\[ 2048 \] =\s*\{(.*?)\};', text, re.S)
    if not body:
        raise ValueError('huffman_static.c has no HuffmanDecoderTable')
    table = tuple(int(word) for word in body.group(1).split(',') if word.strip())
    if len(table) != 2048:
        raise ValueError('HuffmanDecoderTable must hold 2048 entries')
    return table


class _MessageBits:
    """MSG_ReadBits for a Huffman-coded message: n & 7 raw bits, then bytes."""

    def __init__(self, data):
        self.data, self.bit, self.limit = data, 0, len(data) * 8
        self.table = huffman_decoder_table()

    def raw_bit(self):
        if self.bit >= self.limit:
            raise ValueError('Academy demo message ends inside a field')
        value = self.data[self.bit >> 3] >> (self.bit & 7) & 1
        self.bit += 1
        return value

    def symbol(self):
        available = min(11, self.limit - self.bit)
        code = 0
        for index in range(available):
            at = self.bit + index
            code |= (self.data[at >> 3] >> (at & 7) & 1) << index
        entry = self.table[code]
        if not 0 < entry >> 8 <= available:
            raise ValueError('Academy demo message ends inside a field')
        self.bit += entry >> 8
        return entry & 0xff

    def bits(self, count):
        value = 0
        raw = count & 7
        for index in range(raw):
            value |= self.raw_bit() << index
        for index in range(raw, count, 8):
            value |= self.symbol() << index
        return value

    def long(self):
        value = self.bits(32)
        return value - (1 << 32) if value & 0x80000000 else value

    def string(self):
        for _ in range(MAX_COMMAND_CHARS):
            if self.bits(8) == 0:
                return
        raise ValueError('Academy demo server command is unterminated')


def snapshot_flags(raw):
    """Each snapshot record's header, after any reliable commands before it."""
    validate_container(raw)
    rows = []
    cursor = 0
    for record in range(MAX_RECORDS):
        sequence, length = struct.unpack_from('<ii', raw, cursor)
        cursor += 8
        if length == -1:
            return rows
        message = _MessageBits(raw[cursor:cursor + length])
        cursor += length
        message.long()  # reliable acknowledge
        command = message.bits(8)
        while command == SVC_SERVER_COMMAND:
            message.long()
            message.string()
            command = message.bits(8)
        if command == SVC_SNAPSHOT:
            server_time = message.long()
            delta = message.bits(8)
            rows.append(dict(record=record, sequence=sequence, serverTime=server_time,
                deltaNum=delta, snapFlags=message.bits(8)))
    raise ValueError('Academy demo has too many records for a bounded probe')


def snapshot_summary(raw):
    """How much of a demo a client game draws as play: the active span."""
    rows = snapshot_flags(raw)
    active = [row for row in rows if not row['snapFlags'] & SNAPFLAG_NOT_ACTIVE]
    leading = next((index for index, row in enumerate(rows)
        if not row['snapFlags'] & SNAPFLAG_NOT_ACTIVE), len(rows))
    summary = dict(snapshotCount=len(rows), activeSnapshotCount=len(active),
        notActiveSnapshotCount=len(rows) - len(active), leadingNotActiveCount=leading,
        flagCounts={str(flags): sum(row['snapFlags'] == flags for row in rows)
            for flags in sorted({row['snapFlags'] for row in rows})})
    if rows:
        summary.update(firstServerTime=rows[0]['serverTime'], lastServerTime=rows[-1]['serverTime'])
    if active:
        summary.update(firstActiveRecord=active[0]['record'],
            firstActiveServerTime=active[0]['serverTime'],
            activeSpanMilliseconds=rows[-1]['serverTime'] - active[0]['serverTime'])
    return summary


if __name__ == '__main__':
    if len(sys.argv) != 2:
        raise SystemExit('usage: jka_demo_evidence.py DEMO.dm_26')
    print(json.dumps(snapshot_summary(Path(sys.argv[1]).read_bytes()), indent=2))
