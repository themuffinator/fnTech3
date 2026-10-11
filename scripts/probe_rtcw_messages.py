#!/usr/bin/env python3
"""Compare synthetic RTCW wire bodies with user-supplied SP and MP engines.

Builds only offline codec probes. The original sources remain read-only and
are neither copied into fnTech3 nor linked into its binaries. This establishes
source codec evidence, not retail network or gameplay interoperability.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import random
import struct
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
TEMP_ROOT = ROOT / ".tmp/rtcw-manager"
DRIVER = ROOT / "tests/fixtures/rtcw_message_probe.cpp"
REQUEST = struct.Struct("<6I")
REPLY = struct.Struct("<4I")


def run(command: list[str], **kwargs) -> subprocess.CompletedProcess:
    TEMP_ROOT.mkdir(parents=True, exist_ok=True)
    environment = dict(os.environ, TEMP=str(TEMP_ROOT), TMP=str(TEMP_ROOT), TMPDIR=str(TEMP_ROOT))
    result = subprocess.run(command, cwd=ROOT, env=environment, capture_output=True, **kwargs)
    if result.returncode:
        raise RuntimeError(f"command failed ({result.returncode}): {command}\n"
                           f"{result.stdout!r}\n{result.stderr!r}")
    return result


def build(args, source: Path | None, sp: bool, scratch: Path) -> Path:
    label = ("reference" if source else "fntech3") + ("-sp" if sp else "-mp")
    target = ["--target=" + args.target] if args.target else []
    if not args.target and sys.platform.startswith("linux"):
        target = ["-m32"]
    includes = ["-I" + str(source / "src/game"), "-I" + str(source / "src/qcommon")] if source else ["-I" + str(ROOT / "code/qcommon")]
    flags = target + includes + ["-D_CRT_SECURE_NO_WARNINGS", "-fwrapv"]
    sources = [source / "src/qcommon/msg.c", source / "src/qcommon/huffman.c"] if source else [ROOT / "code/qcommon/msg.cpp", ROOT / "code/qcommon/huffman_static.c"]
    if not source:
        flags += ["-DDEDICATED"]
        if os.name == "nt" and (not args.target or args.target.startswith("x86_64")):
            flags += ["-DFNTECH3_Q3_ONLY"]
    objects = []
    for index, unit in enumerate(sources):
        obj = scratch / f"{label}-{index}.obj"
        cxx = unit.suffix == ".cpp"
        run([args.cxx if cxx else args.cc, "-std=c++17" if cxx else "-std=c11", *flags,
             "-c", str(unit), "-o", str(obj)])
        objects.append(str(obj))
    executable = scratch / (label + (".exe" if os.name == "nt" else ""))
    defines = [f"-DPROBE_SP={int(sp)}"] + (["-DPROBE_REFERENCE"] if source else [])
    run([args.cxx, "-std=c++17", *flags, *defines, str(DRIVER), *objects, "-o", str(executable)])
    return executable


def metadata(executable: Path) -> dict:
    text = run([str(executable), "metadata"], text=True).stdout
    result = {"entity": [], "player": [], "mask": []}
    for line in text.splitlines():
        name, *values = line.split()
        numbers = tuple(map(int, values))
        if name == "sizes":
            result[name] = numbers
        else:
            result[name].append(numbers[0] if name == "mask" else numbers)
    return result


def word(data: bytearray, offset: int, value: int) -> None:
    struct.pack_into("<I", data, offset, value & 0xffffffff)


def scalar(rng: random.Random, width: int) -> int:
    if width == 0:
        real = rng.choice([-4096.0, 4095.0, 4096.0, -4097.0, -0.0, 0.0,
                           1.25, -2.5, rng.uniform(-8192, 8192)])
        return struct.unpack("<I", struct.pack("<f", real))[0]
    if width == 32:
        return rng.getrandbits(32)
    if width < 0:
        return rng.randint(-(1 << (-width - 1)), (1 << (-width - 1)) - 1) & 0xffffffff
    return rng.getrandbits(width)


def cases(meta: dict, sp: bool) -> list[dict]:
    result = []
    rng = random.Random(9010)

    def add(name, kind, old, new, flags=1, offset=0):
        result.append(dict(name=name, kind=kind, old=bytes(old), new=bytes(new),
                           flags=flags, offset=offset, key=0x87654321))

    for kind, size in enumerate(meta["sizes"]):
        empty = bytearray(size)
        if kind == 1:
            word(empty, 0, 73)
        add("unchanged", kind, empty, empty)
        if kind == 1:
            add("suppressed", kind, empty, empty, flags=0)
            add("removed", kind, empty, empty, flags=2)
        if kind == 0:
            fields = [(8, 16), (12, 16), (16, 16)]
            fields += [(i, 8) for i in [20, 21, 22, 4, 5, 6, 7, 23]]
            fields += [(24, 16)] if sp else [(24, 8), (25, 8)]
            for index, (offset, width) in enumerate(fields):
                changed = bytearray(empty)
                word(changed, 0, 123456)
                length = 4 if offset in (8, 12, 16) else 2 if sp and offset == 24 else 1
                changed[offset:offset + length] = ((1 << width) - 1).to_bytes(length, "little")
                add(f"command-field-{index}", kind, empty, changed)
            for elapsed in (0, 1, 255, 256, 65536):
                changed = bytearray(empty)
                word(changed, 0, elapsed)
                add(f"command-time-{elapsed}", kind, empty, changed)
            changed = bytearray(empty)
            changed[4:8] = bytes([255, 128, 63, 9])
            changed[20:26] = bytes([128, 129, 127, 128, 254, 223])
            for i, value in enumerate([0xabcd, 0x1234, 0xffff]):
                word(changed, 8 + 4 * i, value)
            word(changed, 0, 123456)
            add("command-complete", kind, empty, changed)
            continue
        fields = meta["entity" if kind == 1 else "player"]
        for index, (offset, width) in enumerate(fields):
            values = [scalar(rng, width) for _ in range(4)] + [0]
            previous = bytearray(empty)
            for variant, value in enumerate(values):
                changed = bytearray(previous)
                word(changed, offset, value)
                add(f"field-{index}-{variant}", kind, previous, changed, offset=(index + variant) % 8)
                previous = changed
        for iteration in range(80):
            previous = bytearray(empty)
            changed = bytearray(empty)
            for offset, width in fields:
                word(previous, offset, scalar(rng, width))
            changed[:] = previous
            for offset, width in fields:
                if rng.randrange(3):
                    word(changed, offset, scalar(rng, width))
            if kind == 2:
                for array_offset, count, width in [(208, 16, -16), (272, 16, -16),
                        (336, 16, 32), (400, 64, -16), (656, 64, -16), (912, 16, -16)]:
                    for slot in range(count):
                        if rng.randrange(5) == 0:
                            word(changed, array_offset + 4 * slot, scalar(rng, width))
            add(f"mixed-{iteration}", kind, previous, changed, offset=iteration % 8)
        if kind == 1 and sp:
            for index, mask in enumerate(meta["mask"]):
                changed = bytearray(empty)
                for i, (offset, width) in enumerate(fields):
                    if mask & (1 << i):
                        word(changed, offset, 0x3fa00000 if width == 0 else 1)
                add(f"dictionary-{index}", kind, empty, changed)
                if index == 31:
                    add("dictionary-31-compressed", kind, empty, changed, flags=5)
        if kind == 2:
            for start, count in [(208, 16), (272, 16), (336, 16), (400, 64), (656, 64), (912, 16)]:
                for slot in range(count):
                    changed = bytearray(empty)
                    word(changed, start + slot * 4, -1 if slot % 2 else 123)
                    add(f"array-{start}-{slot}", kind, empty, changed, offset=slot % 8)
            changed = bytearray(empty)
            for offset, width in fields:
                word(changed, offset, 0x3fa00000 if width == 0 else 1)
            for offset in [208, 272, 336, 400, 652, 656, 908, 912, 972]:
                word(changed, offset, 321)
            add("player-complete", kind, empty, changed)
    return result


def request_file(path: Path, corpus: list[dict], replies=None) -> None:
    with path.open("wb") as output:
        for index, case in enumerate(corpus):
            reply = replies[index] if replies else None
            packet = reply["packet"] if reply else b""
            output.write(REQUEST.pack(case["kind"], case["key"], case["flags"],
                                      reply["bits"] if reply else 0, len(packet), case["offset"]))
            output.write(case["old"] + case["new"] + packet)


def read_replies(path: Path) -> list[dict]:
    data = path.read_bytes()
    replies = []
    cursor = 0
    while cursor < len(data):
        bits, packet_bytes, read_bits, size = REPLY.unpack_from(data, cursor)
        cursor += REPLY.size
        packet = data[cursor:cursor + packet_bytes]
        cursor += packet_bytes
        decoded = data[cursor:cursor + size]
        cursor += size
        if len(decoded) != size:
            raise ValueError("incomplete probe output")
        replies.append(dict(bits=bits, packet=packet, read_bits=read_bits, decoded=decoded))
    return replies


def compare(expected: list[dict], actual: list[dict], corpus: list[dict]) -> None:
    if len(expected) != len(actual) or len(actual) != len(corpus):
        raise AssertionError("probe reply count differs")
    for i, (left, right) in enumerate(zip(expected, actual)):
        if left != right:
            detail = {key: [a.hex() if isinstance(a, bytes) else a,
                            b.hex() if isinstance(b, bytes) else b]
                      for key in left if (a := left[key]) != (b := right[key])}
            raise AssertionError(f"case {i} kind {corpus[i]['kind']} {corpus[i]['name']}: {detail}")


def cpp_fixtures(fixtures: list[dict]) -> str:
    def patches(data: bytes) -> str:
        words = [(i, struct.unpack_from("<I", data, i)[0]) for i in range(0, len(data), 4)]
        return "{ " + ", ".join(f"{{{i}, 0x{value:08x}u}}" for i, value in words if value) + " }"

    lines = ["// Synthetic packets produced by the original RTCW SP/MP codec probes.",
             "// Regenerate with scripts/probe_rtcw_messages.py --fixtures <this file>.",
             "// Source hashes and method: docs/fntech3/2026-09-14-rtcw-contracts.md.",
             "static const RTCWFixture rtcwFixtures[]{"]
    for case in fixtures:
        lines.append(f'\t{{ "{case["mode"]}-{case["name"]}", COM_GAME_RTCW_{case["mode"].upper()}, '
                     f'{case["kind"]}, 0x{case["key"]:08x}u, {case["flags"]}, {case["offset"]}, {case["bits"]},')
        lines += ["\t\t" + patches(case[field]) + "," for field in ["old", "new", "decoded"]]
        lines.append("\t\t{ " + ", ".join(f"0x{value:02x}" for value in case["packet"]) + " } },")
    return "\n".join(lines + ["};", ""])


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sp-source", type=Path, required=True)
    parser.add_argument("--mp-source", type=Path, required=True)
    parser.add_argument("--cc", default="clang")
    parser.add_argument("--cxx", default="clang++")
    parser.add_argument("--target", default="i686-pc-windows-msvc" if os.name == "nt" else None)
    parser.add_argument("--report", type=Path, required=True)
    parser.add_argument("--fixtures", type=Path, help="Optional reference-generated packet .json or C++ .inc")
    args = parser.parse_args()
    if sys.byteorder != "little":
        parser.error("The reference probe requires a little-endian host")
    report = {"target": args.target, "compiler": run([args.cc, "--version"], text=True).stdout,
              "modes": {}, "status": "failed"}
    fixtures = []
    try:
        with tempfile.TemporaryDirectory(prefix="messages-", dir=TEMP_ROOT) as temporary:
            scratch = Path(temporary)
            for sp, source in [(True, args.sp_source), (False, args.mp_source)]:
                source = source.resolve(strict=True)
                reference = build(args, source, sp, scratch)
                engine = build(args, None, sp, scratch)
                meta = metadata(reference)
                if meta != metadata(engine):
                    raise AssertionError(f"{'SP' if sp else 'MP'} field/dictionary metadata differs")
                corpus = cases(meta, sp)
                input_path = scratch / "encode.bin"
                request_file(input_path, corpus)
                results = []
                for exe in [reference, engine]:
                    output = scratch / (exe.stem + ".bin")
                    run([str(exe), "encode", str(input_path), str(output)])
                    results.append(read_replies(output))
                compare(*results, corpus)
                for writer, reader in [(0, engine), (1, reference)]:
                    request_file(input_path, corpus, results[writer])
                    output = scratch / "cross-decoded.bin"
                    run([str(reader), "decode", str(input_path), str(output)])
                    compare(results[writer], read_replies(output), corpus)
                mode = "sp" if sp else "mp"
                crafted = sum(bool(case["flags"] & 4) for case in corpus)
                report["modes"][mode] = dict(cases=len(corpus), native_encoder_cases=len(corpus) - crafted,
                    decoder_only_fixtures=crafted, status="passed", field_contract=meta,
                    source=str(source), source_sha256={str(p): hashlib.sha256((source / p).read_bytes()).hexdigest()
                        for p in [Path("src/qcommon/msg.c"), Path("src/qcommon/huffman.c"), Path("src/game/q_shared.h")]})
                for case, reply in zip(corpus, results[0]):
                    if case["name"] in {"command-complete", "player-complete", "dictionary-0", "dictionary-31", "dictionary-31-compressed"} or (case["kind"] == 1 and case["name"] in {"unchanged", "removed", "field-70-0"}):
                        fixtures.append({**case, **reply, "mode": mode})
                print(f"RTCW {mode}: {len(corpus) - crafted} matched native encodings, "
                      f"{crafted} decoder-only fixtures; bidirectional source decodes passed", flush=True)
            report["status"] = "passed"
    except (OSError, RuntimeError, AssertionError, ValueError) as error:
        report["error"] = str(error)
        print(str(error))
    args.report.resolve().parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    if report["status"] == "passed" and args.fixtures:
        args.fixtures.resolve().parent.mkdir(parents=True, exist_ok=True)
        content = cpp_fixtures(fixtures) if args.fixtures.suffix == ".inc" else json.dumps(fixtures, default=lambda value: value.hex(), indent=2) + "\n"
        args.fixtures.write_text(content, encoding="utf-8")
    return 0 if report["status"] == "passed" else 1


if __name__ == "__main__":
    raise SystemExit(main())
