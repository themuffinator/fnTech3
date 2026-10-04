#!/usr/bin/env python3
"""Observe ET's area-cache update on generated records, without starting ET.

Requires pefile and Unicorn 2. The hash-pinned, user-supplied ET executable stays
read-only. Only its area-update and contents-query functions run in an x86
emulator; generated memory replaces the navigation world, caches and stack.
No PE entry point, operating-system import, renderer or input code runs.
This is numeric fixture evidence, not a live navigation or gameplay probe.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys

ROOT = Path(__file__).resolve().parents[1]
IMAGES = {
    '4d1326c5936b61c5f878fc017e4aa0abddf13e008139e31fce2f13d57b26243e':
        dict(name='et.exe', worldPointer=0x01726368, update=0x00481290,
             updateEnd=0x004815c9, contents=0x0047cf00, contentsEnd=0x0047cf56),
    '7eb606913a56d6901e358a598fb0079120b2730e113a10fc963f90b777580cdf':
        dict(name='etded.exe', worldPointer=0x00cc7888, update=0x0044bbf0,
             updateEnd=0x0044bf29, contents=0x00447860, contentsEnd=0x004478b6),
}
ARENA, STACK, RETURN = 0x30000000, 0x50000000, 0x60000000
WORLD, SETTINGS, CLUSTERS = ARENA, ARENA + 0x2000, ARENA + 0x3000
REACHES, REVERSED, LINKS = ARENA + 0x4000, ARENA + 0x5000, ARENA + 0x6000
UPDATES, ROWS, ROW_POINTERS = ARENA + 0x7000, ARENA + 0x8000, ARENA + 0x9000
COSTS, CACHE, CHOICES = ARENA + 0xa000, ARENA + 0xb000, ARENA + 0xc000


def inputs():
    """Two areas; the return link has an excluded travel type, so no cycles.

    Area 1 enters goal 2 with a walking link. Goal 2 has an outgoing jump
    link to area 1, giving it a first cost row while keeping reverse updates
    acyclic. All numbers are generated; no installed navigation is used.
    """
    cases = []
    base = dict(areaFlags=0, travelFlags=0x80002, contents=0, inside=17,
                link=10, missingSourceRow=False, sourceCluster=1,
                sourceIndex=0, goalIndex=1, branch=False, intermediateFlags=0,
                missingIntermediateRow=False)

    def add(name, **changes):
        cases.append(dict(name=name, **(base | changes)))

    add('ordinary')
    add('disabled', areaFlags=8)
    add('avoid', areaFlags=16)
    add('avoid-axis-unselected', areaFlags=2048)
    add('avoid-axis', areaFlags=2048, travelFlags=0x4080002)
    add('avoid-allies-unselected', areaFlags=4096)
    add('avoid-allies', areaFlags=4096, travelFlags=0x8080002)
    add('both-avoid-flags-axis', areaFlags=6144, travelFlags=0x4080002)
    add('both-avoid-flags-allies', areaFlags=6144, travelFlags=0x8080002)
    add('both-teams', areaFlags=6144, travelFlags=0xc080002)
    add('general-avoid-precedes-team', areaFlags=6160, travelFlags=0xc080002)
    add('disabled-precedes-avoid', areaFlags=24)
    for bit in (32, 64, 128, 256, 1024):
        add(f'other-area-bit-{bit}', areaFlags=bit)
    add('water-rejected', contents=1)
    add('water-allowed', contents=1, travelFlags=0x180002)
    add('liquid-do-not-enter-ignored', contents=1 | 256 | 1024, travelFlags=0x180002)
    add('do-not-enter-rejected', contents=256)
    add('do-not-enter-allowed', contents=256, travelFlags=0x880002)
    add('large-rejected', contents=1024)
    add('large-allowed', contents=1024, travelFlags=0x2080002)
    add('quake-team-contents-ignored', contents=2048 | 4096)
    add('missing-source-row', missingSourceRow=True)
    add('other-cluster', sourceCluster=2)
    add('source-past-reachability-areas', sourceIndex=2)
    add('goal-past-reachability-areas', goalIndex=2)
    add('travel-type-rejected', travelFlags=0x80000)
    add('sum-wrap', inside=65535)
    add('avoid-wrap', areaFlags=16, inside=64990)
    add('team-wrap', areaFlags=2048, travelFlags=0x4080002, inside=65450)
    add('zero-cache-cost', inside=65525)
    add('branch-first-link', branch=True)
    add('branch-avoid-intermediate', branch=True, intermediateFlags=16)
    add('branch-axis-unselected', branch=True, intermediateFlags=2048)
    add('branch-axis-selected', branch=True, intermediateFlags=2048, travelFlags=0x4080002)
    add('branch-allies-selected', branch=True, intermediateFlags=4096, travelFlags=0x8080002)
    add('branch-general-avoid', branch=True, intermediateFlags=6160, travelFlags=0xc080002)
    add('branch-disabled-intermediate', branch=True, intermediateFlags=8)
    add('branch-missing-intermediate-row', branch=True, missingIntermediateRow=True)
    return cases


class Observer:
    def __init__(self, executable):
        import pefile
        import unicorn
        from unicorn import x86_const
        data = executable.read_bytes()
        self.contract = IMAGES.get(hashlib.sha256(data).hexdigest())
        if not self.contract:
            raise ValueError('executable does not match a recorded retail ET identity')
        pe = pefile.PE(data=data)
        if pe.FILE_HEADER.Machine != 0x14c or pe.OPTIONAL_HEADER.ImageBase != 0x400000:
            raise ValueError('expected the original Windows x86 ET image')
        self.cpu = unicorn.Uc(unicorn.UC_ARCH_X86, unicorn.UC_MODE_32)
        self.registers = x86_const
        self.cpu.mem_map(pe.OPTIONAL_HEADER.ImageBase, pe.OPTIONAL_HEADER.SizeOfImage)
        self.cpu.mem_write(pe.OPTIONAL_HEADER.ImageBase, pe.get_memory_mapped_image())
        self.cpu.mem_map(ARENA, 0x10000)
        self.cpu.mem_map(STACK, 0x10000)
        self.cpu.mem_map(RETURN, 0x1000)
        self.cpu.mem_write(self.contract['worldPointer'], struct.pack('<I', WORLD))
        contract = self.contract
        def check_call(cpu, address, size, _):
            # There are no platform calls in these two observed functions.
            if not (contract['update'] <= address <= contract['updateEnd']
                    or contract['contents'] <= address <= contract['contentsEnd']):
                raise ValueError(f'unexpected executable address {address:#x}')
        self.cpu.hook_add(unicorn.UC_HOOK_CODE, check_call)

    def write(self, address, fmt, *values):
        self.cpu.mem_write(address, struct.pack('<' + fmt, *values))

    def value(self, address, fmt='I'):
        return struct.unpack('<' + fmt, self.cpu.mem_read(address, struct.calcsize(fmt)))[0]

    def call(self, function, argument):
        regs = self.registers
        for reg in (regs.UC_X86_REG_EAX, regs.UC_X86_REG_EBX, regs.UC_X86_REG_ECX,
                    regs.UC_X86_REG_EDX, regs.UC_X86_REG_ESI, regs.UC_X86_REG_EDI,
                    regs.UC_X86_REG_EBP):
            self.cpu.reg_write(reg, 0)
        stack = STACK + 0xf000
        self.cpu.reg_write(regs.UC_X86_REG_ESP, stack)
        self.write(stack, 'II', RETURN, argument)
        self.cpu.emu_start(function, RETURN, timeout=1000000, count=20000)
        if self.cpu.reg_read(regs.UC_X86_REG_EIP) != RETURN:
            raise ValueError('routing function did not return within the instruction bound')
        return self.cpu.reg_read(regs.UC_X86_REG_EAX)

    def setup(self):
        self.cpu.mem_write(ARENA, bytes(0x10000))
        for offset, pointer in ((0xdc, SETTINGS), (0xe4, REACHES), (0x104, CLUSTERS),
                                (0x11b0, UPDATES), (0x11bc, REVERSED), (0x11c0, ROWS)):
            self.write(WORLD + offset, 'I', pointer)

    def contents(self, contents):
        self.setup()
        self.write(SETTINGS + 32, 'I', contents)
        return self.call(self.contract['contents'], 1)

    def update(self, case):
        self.setup()
        count = 4 if case['branch'] else 2
        links = [(1, 2, 2, case['link']), (2, 1, 5, case['link'])]
        if case['branch']:
            links = [(1, 2, 2, 5), (1, 3, 2, 10), (2, 1, 5, 10), (3, 2, 2, 10), (4, 2, 2, 15)]
        self.write(CLUSTERS + 16, 'iiii', count, count, 0, 0)
        cursor = 0
        for area in range(1, count + 1):
            outgoing = [link for link in links if link[0] == area]
            flags = case['areaFlags'] if area == 2 else case['intermediateFlags'] if area == 3 else 0
            contents = case['contents'] if area == 2 else 0
            cluster = case['sourceCluster'] if area == 1 else 1
            cluster_index = case['sourceIndex'] if area == 1 else case['goalIndex'] if area == 2 else area - 1
            self.write(SETTINGS + area * 32, 'iiiiiii', contents, flags, 1, cluster, cluster_index, len(outgoing), cursor)
            cursor += len(outgoing)
        for index, (_, destination, travel, time) in enumerate(links):
            address = REACHES + 44 * index
            self.write(address, 'i', destination)
            self.write(address + 36, 'iH', travel, time)
        cursor = LINKS
        for area in range(1, count + 1):
            incoming = [(i, link[0]) for i, link in reversed(list(enumerate(links))) if link[1] == area]
            outgoing = [link for link in links if link[0] == area]
            self.write(REVERSED + area * 8, 'iI', len(incoming), cursor if incoming else 0)
            for i, (link, source) in enumerate(incoming):
                self.write(cursor, 'iiI', link, source, cursor + 12 if i + 1 < len(incoming) else 0)
                cursor += 12
            self.write(ROWS + area * 4, 'I', ROW_POINTERS + area * 0x20)
            for row in range(len(outgoing)):
                costs = [case['inside'] if area == 2 else 1] * len(incoming)
                if case['branch'] and area == 2:
                    costs = [50, 1, 99]
                elif case['branch'] and area == 3:
                    costs = [7]
                address = COSTS + area * 0x80 + row * 0x10
                self.write(ROW_POINTERS + area * 0x20 + row * 4, 'I', address)
                if costs:
                    self.write(address, 'H' * len(costs), *costs)
        if case['missingSourceRow']:
            self.write(ROWS + 4, 'I', 0)
        if case['missingIntermediateRow']:
            self.write(ROWS + 12, 'I', 0)
        self.write(WORLD + 0x1130 + 2 * 4, 'I', 2)
        self.write(WORLD + 0x1130 + 5 * 4, 'I', 16)
        self.write(CACHE + 8, 'ii', 1, 2)
        self.write(CACHE + 28, 'f', 1.0)
        self.write(CACHE + 32, 'I', case['travelFlags'])
        self.write(CACHE + 44, 'I', CHOICES)
        self.call(self.contract['update'], CACHE)
        return dict(times=list(struct.unpack('<' + 'H' * count, self.cpu.mem_read(CACHE + 48, count * 2))),
                    choices=list(self.cpu.mem_read(CHOICES, count)),
                    updates=self.value(WORLD + 0x11b8))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--python-deps', type=Path)
    args = parser.parse_args()
    output = args.output.resolve()
    if not output.is_relative_to(ROOT) or output == ROOT:
        parser.error('output must be a file inside the active project')
    if args.python_deps:
        sys.path.insert(0, str(args.python_deps.resolve(strict=True)))
    executable = args.executable.resolve(strict=True)
    before = hashlib.sha256(executable.read_bytes()).hexdigest()
    observer = Observer(executable)
    bits = (1, 2, 4, 256, 1024, 2048, 4096)
    contents = [sum(bit for i, bit in enumerate(bits) if mask & (1 << i)) for mask in range(128)]
    cases = [case | dict(expected=observer.update(case)) for case in inputs()]
    record = dict(evidence='Fixture: emulated original x86 functions on generated records',
                  executable=str(executable), sha256=before,
                  functions={key: hex(observer.contract[key]) for key in ('update', 'contents')},
                  contentsBits=list(bits), contentsFlags=[observer.contents(value) for value in contents], cases=cases,
                  installationUnchanged=before == hashlib.sha256(executable.read_bytes()).hexdigest())
    if not record['installationUnchanged']:
        raise ValueError('retail executable changed during observation')
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(record, indent=2) + '\n', encoding='utf-8')
    print(f'{len(cases)} area-cache and {len(contents)} contents observations: {output}')


if __name__ == '__main__':
    main()
