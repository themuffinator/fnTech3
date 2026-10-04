#!/usr/bin/env python3
"""Measure protocol-15 bodies by emulating the legitimate retail engine.

Requires pefile and Unicorn. Reads a user-supplied jk2mp.exe 1.02a, never
launches it or writes its installation. The retail body instructions execute
in an isolated x86 emulator; MSG_WriteBits and MSG_ReadBits are bounded raw
bit sinks/sources, so the output pins bodies independently of Huffman framing.
No proprietary binary, machine code or assets are written to the output.

Run from the project directory, for example:
 python tests/jk2_retail_wire_oracle.py --engine "E:/Games/Jedi Outcast/jk2mp.exe"
 --output .tmp/jk2-wire/oracle.json
"""
from pathlib import Path
import argparse
import hashlib
import json
import struct
import pefile
import unicorn
from unicorn import x86_const as x86

EXPECTED_SHA256 = '2f08dcf9a089cb66062766f4922fda63a9e553de893c174b497dfefeb7a1577a'


class Oracle:
    BASE, ARENA, STACK, STOP = 0x400000, 0x10000000, 0x101ff000, 0x100ff000
    MESSAGE, FROM, TO = 0x10000000, 0x10001000, 0x10003000

    def __init__(self, engine):
        data = engine.read_bytes()
        self.sha256 = hashlib.sha256(data).hexdigest()
        if self.sha256 != EXPECTED_SHA256:
            raise ValueError('This oracle requires the pinned legitimate 1.02a engine')
        pe = pefile.PE(data=data)
        self.uc = unicorn.Uc(unicorn.UC_ARCH_X86, unicorn.UC_MODE_32)
        self.uc.mem_map(self.BASE, (pe.OPTIONAL_HEADER.SizeOfImage + 4095) & ~4095)
        self.uc.mem_write(self.BASE, pe.get_memory_mapped_image())
        self.uc.mem_map(self.ARENA, 0x200000)
        self.bits, self.cursor = [], 0
        self.uc.hook_add(unicorn.UC_HOOK_CODE, self.hook)
        self.uc.mem_write(0x8ca3d0, struct.pack('<I', self.MESSAGE + 0x100))

    def args(self, count):
        sp = self.uc.reg_read(x86.UC_X86_REG_ESP)
        return struct.unpack('<' + 'I' * count, self.uc.mem_read(sp + 4, 4 * count))

    def ret(self, result=0):
        sp = self.uc.reg_read(x86.UC_X86_REG_ESP)
        target, = struct.unpack('<I', self.uc.mem_read(sp, 4))
        self.uc.reg_write(x86.UC_X86_REG_EAX, result & 0xffffffff)
        self.uc.reg_write(x86.UC_X86_REG_ESP, sp + 4)
        self.uc.reg_write(x86.UC_X86_REG_EIP, target)

    def hook(self, uc, address, size, _):
        if address == self.STOP:
            uc.emu_stop()
        elif address == 0x431010:  # retail MSG_WriteBits
            _, value, width = self.args(3)
            if width & 0x80000000:
                width = (-width) & 0xffffffff
            if not 1 <= width <= 32:
                raise ValueError('Retail body asked for an invalid bit width')
            self.bits.extend((value >> i) & 1 for i in range(width))
            self.ret()
        elif address == 0x4311b0:  # retail MSG_ReadBits
            _, width = self.args(2)
            signed = width & 0x80000000
            if signed:
                width = (-width) & 0xffffffff
            if not 1 <= width <= 32 or self.cursor + width > len(self.bits):
                raise ValueError('Retail body read beyond the supplied fixture')
            value = sum(self.bits[self.cursor + i] << i for i in range(width))
            self.cursor += width
            if signed and width < 32 and value & (1 << (width - 1)):
                value |= (-1 << width) & 0xffffffff
            self.ret(value)
        elif address == 0x42a5f0:  # retail memset
            destination, value, count = self.args(3)
            self.uc.mem_write(destination, bytes([value & 255]) * count)
            self.ret(destination)
        elif address in (0x427ed0, 0x427cf0):
            raise ValueError('Retail body reported an error or requested diagnostic output')

    def call(self, address, args):
        for reg in (x86.UC_X86_REG_EAX, x86.UC_X86_REG_EBX, x86.UC_X86_REG_ECX,
                    x86.UC_X86_REG_EDX, x86.UC_X86_REG_ESI, x86.UC_X86_REG_EDI,
                    x86.UC_X86_REG_EBP):
            self.uc.reg_write(reg, 0)
        self.uc.reg_write(x86.UC_X86_REG_ESP, self.STACK)
        self.uc.mem_write(self.STACK, struct.pack('<' + 'I' * (len(args) + 1), self.STOP, *args))
        self.uc.emu_start(address, self.STOP, count=1000000)
        if self.uc.reg_read(x86.UC_X86_REG_EIP) != self.STOP:
            raise ValueError('Retail body did not return before the instruction limit')

    def measure(self, kind, baseline, sent, key=0, force=True):
        self.bits, self.cursor = [], 0
        self.uc.mem_write(self.FROM, baseline)
        self.uc.mem_write(self.TO, sent)
        if kind == 'entity':
            self.call(0x431aa0, [self.MESSAGE, self.FROM, self.TO, int(force)])
            number, = struct.unpack_from('<I', sent)
            self.cursor = 10
            self.call(0x431cd0, [self.MESSAGE, self.FROM, self.TO, number])
        elif kind == 'player':
            self.call(0x431f60, [self.MESSAGE, self.FROM, self.TO])
            self.call(0x432340, [self.MESSAGE, self.FROM, self.TO])
        elif kind == 'command':
            self.call(0x431750, [self.MESSAGE, key, self.FROM, self.TO])
            self.call(0x431900, [self.MESSAGE, key, self.FROM, self.TO])
        else:
            raise ValueError(kind)
        decoded = bytes(self.uc.mem_read(self.TO, len(sent)))
        if self.cursor != len(self.bits):
            raise ValueError('Retail reader did not consume its writer output')
        packed = bytearray((len(self.bits) + 7) // 8)
        for i, bit in enumerate(self.bits):
            packed[i >> 3] |= bit << (i & 7)
        return {'kind': kind, 'baseline': baseline.hex(), 'sent': sent.hex(),
                'key': key, 'bits': len(self.bits), 'wire': packed.hex(), 'decoded': decoded.hex()}


def record(size, words=(), bytes_=()):
    data = bytearray(size)
    for offset, value in words:
        struct.pack_into('<I', data, offset, value & 0xffffffff)
    for offset, value in bytes_:
        data[offset] = value & 255
    return bytes(data)


def vectors(oracle):
    result = []
    cases = [
        ('entity-zero', 'entity', record(296), record(296, [(0, 7)]), 0),
        ('entity-integral-floats', 'entity', record(296),
         record(296, [(0, 42), (16, 1000), (24, 0xc5800000), (28, 0x457ff000),
                      (212, -3), (236, 19), (244, 1)]), 0),
        ('entity-binary32', 'entity', record(296),
         record(296, [(0, 1022), (24, 0x3fa00000), (164, 0x7fc12345), (196, 0xaabbccdd)]), 0),
        ('player-arrays', 'player', record(1368),
         record(1368, [(0, 12345), (44, -17), (168, -12), (216, -32768),
                      (280 + 15*4, 32767), (408 + 3*4, -1), (344 + 7*4, 0x87654321)]), 0),
        ('player-force', 'player', record(1368),
         record(1368, [(20, 0x3fa00000), (832, 0xabcd1234), (988, 3), (1080, 0x45800000),
                      (1356, 0xc5800000), (1364, 0x7f800000)]), 0),
        ('command-raven', 'command', record(28, [(0, 1000)]),
         record(28, [(0, 1010), (4, 40000), (8, 12), (16, 0x1234)],
                [(20, 4), (21, 7), (22, 9), (23, 11), (24, -128), (25, 127), (26, -1)]), 0x12355678),
        ('command-no-change', 'command', record(28, [(0, 1000)]), record(28, [(0, 1000)]), 0x12355678),
        ('command-long-time', 'command', record(28), record(28, [(0, 100000), (4, 32768)]), 0x10000),
        ('command-backwards-time', 'command', record(28, [(0, 1000)]), record(28, [(0, 999)]), 0),
    ]
    for name, kind, baseline, sent, key in cases:
        item = oracle.measure(kind, baseline, sent, key)
        item['name'] = name
        result.append(item)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--engine', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    root = Path(__file__).resolve().parents[1]
    if not output.is_relative_to(root):
        raise ValueError('Output must be inside this project')
    oracle = Oracle(args.engine.resolve())
    result = {'engineSha256': oracle.sha256, 'method': 'retail x86 instructions; bounded raw bit IO hooks',
              'vectors': vectors(oracle)}
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(result, indent=2) + '\n')
    print(f'{len(result["vectors"])} retail writer/reader observations saved to {output}')


if __name__ == '__main__':
    main()
