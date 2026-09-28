#!/usr/bin/env python3
"""Count the calls each installed retail game module makes to one engine import.

A game module reaches the engine through its profile's own numbering, and
each kind of module calls in its own way, which this reads from the code
itself rather than from any symbol:

- a QVM (Quake III, Team Arena, Elite Force Holomatch, Jedi Outcast) calls
  import N as `CONST -(N+1)` followed by `CALL`; its whole code segment is
  decoded instruction by instruction from the start;
- a native module of Quake III's older kind (RTCW single player and
  multiplayer, Enemy Territory, Jedi Academy) stores the pointer `dllEntry`
  is given and calls through it with N pushed last; each call through that
  pointer is found and N read from the push before it, and every `push N` in
  the code is counted besides;
- Quake Live's native module stores the table `dllEntry` is given as its
  second argument and calls slot N of it; every load of that pointer is
  followed through the slots it reads until the register is overwritten, and
  every read of slot N through any register is traced back to what loaded it.

x86 code is disassembled with Capstone from start to end of each executable
section. Call sites whose number cannot be read are reported as unresolved.

The numbers are the engine's own: `code/game/g_public.h` for Quake III and
Quake Live, and the module call tables in `code/compat` for RTCW, Enemy
Territory, Elite Force and Jedi Academy. A title whose table lacks the import
is still scanned, for the highest number it calls. Jedi Outcast has no table;
its number is taken as Quake III's unless `--no-jk2-as-q3`.

Only the retail paks of each game directory are read (`pak0.pk3` to
`pak8.pk3` and so on, as MODULES names them), with loose modules where the
retail game ships them loose. Nothing is run and nothing is written to an
installation. The report is JSON.

  --installation KEY=PATH  q3, ql, rtcw, wolfet, stvef, jka or jk2, as for
                           probe_retail_map_loads.py; a key may repeat
  --import NAME            default BOTLIB_AAS_POINT_REACHABILITY_AREA_INDEX
  --output FILE            the JSON report (standard output otherwise); a
                           line per module goes to standard error
"""
from pathlib import Path
import argparse
import hashlib
import json
import re
import struct
import sys
import zipfile

ROOT = Path(__file__).resolve().parents[1]

# qfiles.h opcode_t: the operand width of each opcode that has one.
OP_CALL, OP_CONST = 5, 8
QVM_OPERAND = {3: 4, 4: 4, 8: 4, 9: 4, 33: 1, 34: 4, **{op: 4 for op in range(11, 27)}}
QVM_MAGICS = (0x12721444, 0x12721445)
REGISTERS = ('eax', 'ebx', 'ecx', 'edx', 'esi', 'edi', 'ebp')
CALLER_SAVED = ('eax', 'ecx', 'edx')
NUMBER = r'(0x[0-9a-f]+|\d+)'

# title: [(profile, game directory, module file, retail paks or None, shipped
# loose, kind)]. The directory '.' is the installation itself; kind picks the
# call convention. A loose copy where retail ships none is someone else's build.
MODULES = {
    'q3': [('q3', 'baseq3', 'vm/qagame.qvm', r'pak[0-8]\.pk3', False, 'qvm'),
           ('q3', 'missionpack', 'vm/qagame.qvm', r'pak[0-3]\.pk3', False, 'qvm')],
    'ql': [('ql', 'baseq3', 'qagamex86.dll', None, True, 'table')],
    'rtcw': [('rtcw-sp', '.', 'qagamex86.dll', None, True, 'syscall'),
             ('rtcw-mp', 'main', 'qagame_mp_x86.dll', r'mp_bin\.pk3', True, 'syscall')],
    'wolfet': [('wolfet', 'etmain', 'qagame_mp_x86.dll', r'mp_bin\.pk3', True, 'syscall')],
    'stvef': [('stvef-mp', 'baseef', 'vm/qagame.qvm', r'pak[0-3]\.pk3', False, 'qvm')],
    'jka': [('jka-mp', 'base', 'jampgamex86.dll', r'assets[0-3]\.pk3', False, 'syscall')],
    'jk2': [('jk2-mp', 'base', 'vm/jk2mpgame.qvm', r'assets[0-9]\.pk3', False, 'qvm')],
}


def enum_values(text, closing):
    """Evaluate the plain `NAME` and `NAME = N` rows of the C enum that ends at `closing`."""
    end = text.index(closing)
    start = text.rindex('typedef enum {', 0, end) + len('typedef enum {')
    body = re.sub(r'/\*.*?\*/|//[^\n]*', '', text[start:end], flags=re.S)
    values, current = {}, -1
    for row in (part.strip() for part in body.split(',')):
        if not row:
            continue
        name, _, value = (item.strip() for item in row.partition('='))
        if value:
            current = int(value, 0) if re.fullmatch(r'-?' + NUMBER, value) else values.get(value)
        elif current is not None:
            current += 1
        values[name] = current
    return values


def ordinals(name):
    """{profile: ordinal or None} for the import, from the engine's own tables."""
    public = (ROOT / 'code/game/g_public.h').read_text(encoding='utf-8')
    quake3 = enum_values(public, '} gameImport_t;')
    live = enum_values(public, '} gameNativeImport_t;')
    live_name = 'G_QL_IMPORT_' + (name[2:] if name.startswith('G_') else name)
    found = {'q3': quake3.get(name), 'ql': live.get(live_name)}
    for table, profiles in (('rtcw', ('rtcw-sp', 'rtcw-mp')), ('stvef', ('stvef-sp', 'stvef-mp')),
                            ('wolfet', ('wolfet',)), ('jka', ('jka-mp',))):
        text = (ROOT / f'code/compat/{table}_module_calls.def').read_text(encoding='utf-8')
        row = re.search(r'MODULE_CALL\(GAME, IMPORT, %s, ([-\d, ]+)\)' % re.escape(name), text)
        numbers = [int(n) for n in row.group(1).split(',')] if row else [-1] * len(profiles)
        for profile, number in zip(profiles, numbers[-len(profiles):]):
            found[profile] = number if number >= 0 else None
    return found


def qvm_calls(data, number):
    """Every syscall a QVM makes: {number: [instruction index]}."""
    magic, count, code_offset, code_length = struct.unpack_from('<4i', data)
    if magic not in QVM_MAGICS:
        raise ValueError('not a QVM')
    code = data[code_offset:code_offset + code_length]
    calls, constants, pc, previous = {}, 0, 0, None
    for index in range(count):
        op = code[pc]
        width = QVM_OPERAND.get(op, 0)
        value = int.from_bytes(code[pc + 1:pc + 1 + width], 'little', signed=True) if width else None
        pc += 1 + width
        if op == OP_CALL and previous and previous[0] == OP_CONST and previous[1] < 0:
            calls.setdefault(-1 - previous[1], []).append(index)
        if op == OP_CONST and number is not None and value == -1 - number:
            constants += 1
        previous = (op, value)
    if pc > len(code):
        raise ValueError('instructions run past the code segment')
    detail = dict(instructions=count, codeBytes=len(code), decodedBytes=pc, constantsOfImport=constants)
    return calls, detail, []


def disassembler():
    """Capstone for 32-bit x86, imported only when a native module is read."""
    import capstone
    return capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)


def sweep(pe):
    """Every instruction of every executable section, decoded from start to end."""
    base = pe.OPTIONAL_HEADER.ImageBase
    md = disassembler()
    out = []
    for section in pe.sections:
        if not section.Characteristics & 0x20000000:  # IMAGE_SCN_MEM_EXECUTE
            continue
        data = section.get_data()[:section.Misc_VirtualSize]
        va = base + section.VirtualAddress
        offset = 0
        while offset < len(data):
            for ins in md.disasm(data[offset:], va + offset):
                out.append(ins)
                offset = ins.address - va + ins.size
            offset += 1 if offset < len(data) else 0
    return out


def stored_argument(pe, argument):
    """The global `dllEntry` stores its argument at [esp+argument] in."""
    base = pe.OPTIONAL_HEADER.ImageBase
    entry = next(base + e.address for e in pe.DIRECTORY_ENTRY_EXPORT.symbols if e.name == b'dllEntry')
    md = disassembler()
    held = set()
    for ins in md.disasm(pe.get_data(entry - base, 96), entry):
        loaded = re.fullmatch(r'(\w+), dword ptr \[esp \+ %s\]' % NUMBER, ins.op_str)
        if ins.mnemonic == 'mov' and loaded and int(loaded.group(2), 0) == argument:
            held.add(loaded.group(1))
        stored = re.fullmatch(r'dword ptr \[(0x[0-9a-f]+)\], (\w+)', ins.op_str)
        if ins.mnemonic == 'mov' and stored and stored.group(2) in held:
            return int(stored.group(1), 16)
        if ins.mnemonic == 'ret':
            break
    raise ValueError(f'dllEntry stores no argument at esp+{argument}')


def writes(ins, register):
    """Whether the instruction leaves a new value in the register."""
    if ins.mnemonic in ('cmp', 'test', 'push') or ins.mnemonic.startswith('j'):
        return False
    if ins.mnemonic == 'call':
        return register in CALLER_SAVED
    return ins.op_str.split(',')[0].strip() == register


def syscall_calls(pe, number):
    """RTCW, ET and Jedi Academy: push N ... call [syscall] (or call reg after mov reg, [syscall])."""
    insns = sweep(pe)
    pointer = '[%s]' % hex(stored_argument(pe, 4))
    calls, unresolved = {}, []
    for index, ins in enumerate(insns):
        if ins.mnemonic != 'call':
            continue
        through = ins.op_str == 'dword ptr ' + pointer
        if not through and ins.op_str in REGISTERS:
            for back in reversed(insns[max(0, index - 12):index]):
                if writes(back, ins.op_str):
                    through = back.mnemonic == 'mov' and back.op_str == f'{ins.op_str}, dword ptr {pointer}'
                    break
        if not through:
            continue
        value = None
        for back in reversed(insns[max(0, index - 24):index]):
            if back.mnemonic == 'push':
                value = int(back.op_str, 0) if re.fullmatch(NUMBER, back.op_str) else None
                break
            if back.mnemonic in ('call', 'ret', 'jmp'):
                break
        if value is None:
            unresolved.append(hex(ins.address))
        else:
            calls.setdefault(value, []).append(hex(ins.address))
    pushes = sum(1 for ins in insns if ins.mnemonic == 'push' and number is not None
                 and re.fullmatch(NUMBER, ins.op_str) and int(ins.op_str, 0) == number)
    return calls, dict(instructions=len(insns), pointer=pointer, pushesOfImport=pushes), unresolved


def slot_uses(insns, start, register, depth=0):
    """(slot, address) for each table slot read through `register` from insns[start] until it changes."""
    uses = []
    for ins in insns[start:start + 400]:
        for found in re.finditer(r'\[%s(?: \+ %s)?\]' % (register, NUMBER), ins.op_str):
            uses.append((int(found.group(1) or '0', 0) // 4, hex(ins.address)))
        if ins.mnemonic == 'add' and re.fullmatch(r'%s, %s' % (register, NUMBER), ins.op_str):
            uses.append((int(ins.op_str.split(', ')[1], 0) // 4, hex(ins.address)))  # a slot's address taken
            return uses
        if ins.mnemonic == 'ret' or writes(ins, register):
            return uses
        if ins.mnemonic == 'jmp':
            if depth < 4 and re.fullmatch(r'0x[0-9a-f]+', ins.op_str):  # a shared tail
                target = int(ins.op_str, 16)
                tail = next((n for n, x in enumerate(insns) if x.address == target), None)
                if tail is not None:
                    return uses + slot_uses(insns, tail, register, depth + 1)
            return uses
    return uses


def table_calls(pe, number):
    """Quake Live: dllEntry(exports, imports, version) keeps `imports`; slot N is import N."""
    insns = sweep(pe)
    pointer = '[%s]' % hex(stored_argument(pe, 8))
    calls, unresolved, loads = {}, [], 0
    for index, ins in enumerate(insns):
        if pointer not in ins.op_str or (ins.mnemonic == 'mov' and ins.op_str.startswith('dword ptr ' + pointer)):
            continue  # dllEntry's own store is the one write
        loaded = re.fullmatch(r'(\w+), dword ptr ' + re.escape(pointer), ins.op_str)
        loads += bool(ins.mnemonic == 'mov' and loaded)
        uses = slot_uses(insns, index + 1, loaded.group(1)) if ins.mnemonic == 'mov' and loaded else []
        if not uses:
            unresolved.append(hex(ins.address))
        for slot, address in uses:
            calls.setdefault(slot, []).append(address)
    # Independently: every read of the slot's offset through any register, and what last wrote that register.
    reads = dict(fromTable=0, other=0)
    if number is not None:
        offset = re.compile(r'\[(%s) \+ %s\]' % ('|'.join(REGISTERS), hex(4 * number)))
        for index, ins in enumerate(insns):
            found = offset.search(ins.op_str)
            if not found:
                continue
            source = next((b for b in reversed(insns[max(0, index - 64):index]) if writes(b, found.group(1))), None)
            table = source is not None and source.mnemonic == 'mov' and source.op_str.endswith(pointer)
            reads['fromTable' if table else 'other'] += 1
    detail = dict(instructions=len(insns), pointer=pointer, pointerLoads=loads, slotOffsetReads=reads)
    return calls, detail, unresolved


def modules(title, installation):
    """(profile, where, bytes, kind) for every retail copy of the title's game modules."""
    for profile, folder_name, member, paks, shipped_loose, kind in MODULES[title]:
        folders = [installation] if folder_name == '.' else [
            p for p in installation.iterdir() if p.is_dir() and p.name.lower() == folder_name]
        for folder in folders:
            loose = folder / member
            if shipped_loose and loose.is_file():
                yield profile, loose.relative_to(installation).as_posix(), loose.read_bytes(), kind
            if not paks:
                continue
            for pak in sorted((p for p in folder.iterdir() if re.fullmatch(paks, p.name.lower())),
                              key=lambda p: p.name.lower()):
                with zipfile.ZipFile(pak) as archive:
                    for entry in archive.namelist():
                        if entry.lower() == member:
                            yield profile, f'{pak.relative_to(installation).as_posix()}:{entry}', archive.read(entry), kind


def scan(kind, data, number):
    if kind == 'qvm':
        return qvm_calls(data, number)
    import pefile
    pe = pefile.PE(data=data)
    detail = dict(timeDateStamp=hex(pe.FILE_HEADER.TimeDateStamp), imageBase=hex(pe.OPTIONAL_HEADER.ImageBase))
    calls, extra, unresolved = syscall_calls(pe, number) if kind == 'syscall' else table_calls(pe, number)
    detail.update(extra)
    return calls, detail, unresolved


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--installation', action='append', default=[], metavar='KEY=PATH')
    parser.add_argument('--import', dest='name', default='BOTLIB_AAS_POINT_REACHABILITY_AREA_INDEX')
    parser.add_argument('--no-jk2-as-q3', dest='jk2_as_q3', action='store_false')
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    installations = []
    for item in args.installation:
        key, _, path = item.partition('=')
        if key not in MODULES or not path:
            parser.error(f'unknown installation {item!r}')
        installations.append((key, Path(path).resolve(strict=True)))
    numbers = ordinals(args.name)
    if args.jk2_as_q3:
        numbers['jk2-mp'] = numbers['q3']
    rows = []
    for title, installation in installations:
        for profile, where, data, kind in modules(title, installation):
            number = numbers.get(profile)
            calls, detail, unresolved = scan(kind, data, number)
            rows.append(dict(
                profile=profile, installation=str(installation), module=where, kind=kind, bytes=len(data),
                sha256=hashlib.sha256(data).hexdigest(), ordinal=number,
                callSites=sum(len(v) for v in calls.values()), distinctImports=len(calls),
                highestImport=max(calls) if calls else None, unresolved=unresolved,
                callsToImport=len(calls.get(number, [])) if number is not None else None,
                sites=calls.get(number, [])[:32] if number is not None else [], **detail))
    report = {'import': args.name, 'ordinals': numbers,
              'jk2Ordinal': "Quake III's" if args.jk2_as_q3 else None, 'modules': rows}
    text = json.dumps(report, indent=1) + '\n'
    if args.output:
        args.output.write_text(text, encoding='utf-8')
    else:
        sys.stdout.write(text)
    for row in rows:
        print(f"{row['profile']:9} {row['module']:48} calls={row['callSites']:5} unresolved={len(row['unresolved'])} "
              f"highest={row['highestImport']} ordinal={row['ordinal']} -> {row['callsToImport']}", file=sys.stderr)


if __name__ == '__main__':
    main()
