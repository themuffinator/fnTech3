#!/usr/bin/env python3
"""Compare protocol-84 bodies with an offline original-engine codec oracle.

Original sources remain outside fnTech3 and never enter its binaries. Packet
fixtures contain only synthetic data. A pass establishes source interoperability,
not a live client/server or gameplay gate.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import tempfile
import probe_rtcw_messages as common

ROOT = Path(__file__).resolve().parents[1]


def cases(meta):
    corpus = [c for c in common.cases(meta, False) if c['kind'] != 0]
    empty = bytes(meta['sizes'][0])
    for time in (0, 1, 255, 256, 65536, 0x7fffffff):
        for offset, size, width in [(8,4,16), (12,4,16), (16,4,16),
                (20,1,8), (21,1,8), (22,1,8), (4,1,8), (5,1,8),
                (6,1,8), (7,1,8), (23,1,3), (24,1,8)]:
            changed = bytearray(empty)
            common.word(changed, 0, time)
            changed[offset:offset+size] = ((1 << width)-1).to_bytes(size,'little')
            corpus.append(dict(name=f'command-{time}-{offset}', kind=0, old=empty,
                new=bytes(changed), flags=1, offset=offset % 8, key=0x87654321))
    return corpus


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--cc', default='clang')
    parser.add_argument('--cxx', default='clang++')
    parser.add_argument('--target', default='i686-pc-windows-msvc' if os.name == 'nt' else None)
    parser.add_argument('--report', type=Path, required=True)
    parser.add_argument('--fixtures', type=Path)
    args = parser.parse_args()
    common.DRIVER = ROOT / 'tests/fixtures/wolfet_message_probe.cpp'
    common.TEMP_ROOT = ROOT / '.tmp/wolfet'
    common.TEMP_ROOT.mkdir(parents=True, exist_ok=True)
    report = {'evidence': 'Source/Unit', 'protocol': 84, 'target': args.target, 'passed': False}
    try:
        with tempfile.TemporaryDirectory(dir=common.TEMP_ROOT) as temporary:
            folder = Path(temporary)
            reference = common.build(args, args.source.resolve(), False, folder)
            engine = common.build(args, None, False, folder)
            meta = common.metadata(reference)
            if meta != common.metadata(engine):
                raise AssertionError('ET wire field metadata differs')
            corpus = cases(meta)
            inputs = folder / 'input.bin'
            common.request_file(inputs, corpus)
            results = []
            for exe in (reference, engine):
                output = folder / (exe.stem + '.bin')
                common.run([str(exe), 'encode', str(inputs), str(output)])
                results.append(common.read_replies(output))
            common.compare(*results, corpus)
            for index, exe in ((0,engine), (1,reference)):
                common.request_file(inputs, corpus, results[index])
                output = folder / 'decoded.bin'
                common.run([str(exe), 'decode', str(inputs), str(output)])
                common.compare(results[index], common.read_replies(output), corpus)
            report.update(passed=True, cases=len(corpus), metadata=meta,
                sourceSha256={name: hashlib.sha256((args.source/'src'/name).read_bytes()).hexdigest()
                    for name in ('qcommon/msg.c','qcommon/huffman.c','game/q_shared.h')})
            if args.fixtures:
                fixtures = [{**case, **reply} for case,reply in zip(corpus,results[0])
                    if case['name'] in ('removed','player-complete','mixed-0','field-70-0','field-76-0','command-65536-23')]
                args.fixtures.resolve().parent.mkdir(parents=True,exist_ok=True)
                args.fixtures.write_text(json.dumps(fixtures,default=lambda b:b.hex(),indent=2)+'\n')
    except (OSError,RuntimeError,AssertionError,ValueError) as error:
        report['error'] = str(error)
        print(error)
    args.report.resolve().parent.mkdir(parents=True,exist_ok=True)
    args.report.write_text(json.dumps(report,indent=2)+'\n')
    print(f"WolfET codec: {'PASS' if report['passed'] else 'FAIL'} ({report.get('cases',0)} source comparisons)")
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
