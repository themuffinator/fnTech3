#!/usr/bin/env python3
"""Observe ET area travel costs without starting the original engine.

Only hash-pinned x86 cost, vector/scalar math and travel-type routines execute. Generated
records replace the navigation world and vectors; no entry point, platform
import, renderer or input runs. Results are numeric Fixture evidence, with the
normal nearest-even floating-point mode stated explicitly.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import random
import struct
import sys

ROOT = Path(__file__).resolve().parents[1]
IMAGES = {
    '4d1326c5936b61c5f878fc017e4aa0abddf13e008139e31fce2f13d57b26243e':
        dict(cost=(0x47d470, 0x47d4de), distance=(0x41de70, 0x41deb6),
             length=(0x41df50, 0x41df97), integer=(0x41e3b0, 0x41e3c2),
             sqrt=(0x507420, 0x507472), types=(0x481270, 0x48128f),
             initTypes=(0x47f9a0, 0x47fae4), worldPointer=0x1726368),
    '7eb606913a56d6901e358a598fb0079120b2730e113a10fc963f90b777580cdf':
        dict(cost=(0x447dd0, 0x447e3e), distance=(0x402db0, 0x402df6),
             length=(0x402e90, 0x402ed7), integer=(0x4032f0, 0x403302),
             sqrt=(0x471910, 0x471962), types=(0x44bbd0, 0x44bbef),
             initTypes=(0x44a300, 0x44a444), worldPointer=0xcc7888),
}
ARENA, STACK, RETURN = 0x30000000, 0x50000000, 0x60000000
WORLD, SETTINGS, START, END = ARENA, ARENA + 0x2000, ARENA + 0x4000, ARENA + 0x5000


def inputs():
    vectors = [(0, 0, 0), (1, 0, 0), (3, 4, 0), (3, 4, 12),
               (15, 0, 0), (50, 0, 0), (100, 0, 0), (150, 0, 0), (250, 0, 0),
               (150 - 1 / 65536, 0, 0), (150 + 1 / 65536, 0, 0),
               (50412.30859375, 0, 0), (65536, 0, 0), (131072, 0, 0),
               (-131072, 131072, -131072)]
    rng = random.Random(0x45544141)
    vectors += [tuple(rng.randrange(-65536, 65537) / 8 for _ in range(3)) for _ in range(40)]
    cases = []
    for number, end in enumerate(vectors):
        for presence, flags, contents, slope in ((2, 0, 0, 0), (4, 0, 0, 0),
                                                (2, 4, 1, 1), (4, 4, 1, .5)):
            cases.append(dict(name=f'vector-{number}-presence-{presence}-liquid-{bool(flags)}',
                              start=[0, 0, 0], end=list(end), presence=presence,
                              areaFlags=flags, contents=contents, steepness=slope))
    # Translation and direction do not change a distance cost. Presence bit 2
    # alone selects normal speed; ET ignores liquid and slope for this routine.
    for presence in (0, 2, 4, 6, 8):
        for slope in (0, .5, 1):
            for start, end in (([0, 0, 0], [150, 0, 0]), ([150, 0, 0], [0, 0, 0]),
                               ([-100, 200, -300], [50, 200, -300])):
                cases.append(dict(name=f'presence-{presence}-slope-{slope}-{len(cases)}',
                                  start=start, end=end, presence=presence,
                                  areaFlags=4, contents=7, steepness=slope))
    return cases


class Observer:
    def __init__(self, executable):
        import pefile
        import unicorn
        from unicorn import x86_const
        data = executable.read_bytes()
        self.sha256 = hashlib.sha256(data).hexdigest()
        self.contract = IMAGES.get(self.sha256)
        if self.contract is None:
            raise ValueError('unrecognized original ET executable identity')
        pe = pefile.PE(data=data)
        if pe.FILE_HEADER.Machine != 0x14c or pe.OPTIONAL_HEADER.ImageBase != 0x400000:
            raise ValueError('expected the original Windows x86 image')
        self.cpu = unicorn.Uc(unicorn.UC_ARCH_X86, unicorn.UC_MODE_32)
        self.registers = x86_const
        self.cpu.mem_map(0x400000, pe.OPTIONAL_HEADER.SizeOfImage)
        self.cpu.mem_write(0x400000, pe.get_memory_mapped_image())
        for address in (ARENA, STACK, RETURN):
            self.cpu.mem_map(address, 0x10000)
        self.cpu.mem_write(self.contract['worldPointer'], struct.pack('<I', WORLD))
        ranges = [value for value in self.contract.values() if isinstance(value, tuple)]

        def check(cpu, address, size, _):
            if not any(first <= address <= last for first, last in ranges):
                raise ValueError(f'unexpected original instruction address {address:#x}')
        self.cpu.hook_add(unicorn.UC_HOOK_CODE, check)

    def cost(self, case):
        regs = self.registers
        self.cpu.mem_write(ARENA, bytes(0x10000))
        self.cpu.mem_write(WORLD + 0xdc, struct.pack('<I', SETTINGS))
        self.cpu.mem_write(SETTINGS + 32, struct.pack('<iiiiiiif', case['contents'],
                            case['areaFlags'], case['presence'], 1, 0, 1, 0, case['steepness']))
        self.cpu.mem_write(START, struct.pack('<3f', *case['start']))
        self.cpu.mem_write(END, struct.pack('<3f', *case['end']))
        for register in (regs.UC_X86_REG_EAX, regs.UC_X86_REG_EBX, regs.UC_X86_REG_ECX,
                         regs.UC_X86_REG_EDX, regs.UC_X86_REG_ESI, regs.UC_X86_REG_EDI,
                         regs.UC_X86_REG_EBP):
            self.cpu.reg_write(register, 0)
        self.cpu.reg_write(regs.UC_X86_REG_FPCW, 0x037f)
        self.cpu.reg_write(regs.UC_X86_REG_FPSW, 0)
        self.cpu.reg_write(regs.UC_X86_REG_FPTAG, 0xffff)
        self.cpu.reg_write(regs.UC_X86_REG_MXCSR, 0x1f80)
        stack = STACK + 0xf000
        self.cpu.reg_write(regs.UC_X86_REG_ESP, stack)
        self.cpu.mem_write(stack, struct.pack('<4I', RETURN, 1, START, END))
        self.cpu.emu_start(self.contract['cost'][0], RETURN, timeout=1000000, count=1000)
        if self.cpu.reg_read(regs.UC_X86_REG_EIP) != RETURN:
            raise ValueError('original cost routine did not return within the instruction bound')
        return self.cpu.reg_read(regs.UC_X86_REG_EAX) & 0xffff

    def travel_type(self, travel):
        regs = self.registers
        self.cpu.mem_write(ARENA, bytes(0x10000))
        stack = STACK + 0xf000
        for function, args in ((self.contract['initTypes'][0], []), (self.contract['types'][0], [travel & 0xffffffff])):
            self.cpu.reg_write(regs.UC_X86_REG_ESP, stack)
            self.cpu.mem_write(stack, struct.pack('<' + 'I' * (1 + len(args)), RETURN, *args))
            self.cpu.emu_start(function, RETURN, timeout=1000000, count=1000)
            if self.cpu.reg_read(regs.UC_X86_REG_EIP) != RETURN:
                raise ValueError('original travel-type routine exceeded the instruction bound')
        return self.cpu.reg_read(regs.UC_X86_REG_EAX)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--python-deps', type=Path)
    args = parser.parse_args()
    output = args.output.resolve()
    if not output.is_relative_to(ROOT) or output == ROOT:
        parser.error('output must be a file in the active project')
    if args.python_deps:
        sys.path.insert(0, str(args.python_deps.resolve(strict=True)))
    executable = args.executable.resolve(strict=True)
    observer = Observer(executable)
    record = dict(evidence='Fixture: bounded original x86 math on generated inputs',
                  executable=str(executable), sha256=observer.sha256,
                  functions={key: [hex(value[0]), hex(value[1])] for key, value in observer.contract.items()
                             if isinstance(value, tuple)},
                  floatingPoint=dict(x87ControlWord='0x037f', mxcsr='0x1f80', rounding='nearest-even'),
                  cases=[case | dict(expected=observer.cost(case)) for case in inputs()])
    types = list(range(32)) + [-1, 32, 33, 0x01000002, 0x02000002, 0x03000002,
                              0x04000002, 0x08000002, 0x7fffffff, 0xffffffff]
    record['travelTypes'] = [dict(type=travel, expected=observer.travel_type(travel)) for travel in types]
    record['installationUnchanged'] = observer.sha256 == hashlib.sha256(executable.read_bytes()).hexdigest()
    if not record['installationUnchanged']:
        raise ValueError('original executable changed during observation')
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(record, indent=2) + '\n', encoding='utf-8')
    print(f'{len(record["cases"])} original cost observations saved to {output}')


if __name__ == '__main__':
    main()
