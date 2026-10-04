"""Bound the public .dm_26 container before retaining a generated Academy demo.

This validates record framing, not compressed message semantics. Successful
playback by the selected maintained client is a separate runtime observation.
"""
from pathlib import Path
import hashlib
import os
import stat
import struct

MAX_DEMO_BYTES = 64 * 1024 * 1024
MAX_MESSAGE_BYTES = 49152
MAX_RECORDS = 10000


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
