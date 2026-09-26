#!/usr/bin/env python3
"""Write an Elite Force SP capture script with matched screenshot phases.

Use activeAction="exec capture.cfg" when loading a map, fixedtime=25/50/100,
isolated writable homes, windowed mode and disabled input. The tested tutorial
captures have matching displayed snapshot times, while packet sequence numbers
can differ. This does not promise equal dynamic states or rendering.
See docs/fntech3/2026-09-21-stvef-sp-startup.md for the observations and limits.
"""
# Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later.

import argparse
from pathlib import Path


def capture_script(engine, samples=(20, 80, 160)):
    if engine not in ('retail', 'fntech3'):
        raise ValueError('engine must be retail or fntech3')
    samples = tuple(samples)
    if (not samples or any(type(n) is not int or n < 2 or n > 1_000_000 for n in samples)
            or any(a >= b for a, b in zip(samples, samples[1:]))):
        raise ValueError('samples must increase strictly within 2..1000000')
    retail = engine == 'retail'
    result = ['r_textureMode', 'r_simpleMipMaps', 'r_picmip', 'r_gamma', 'r_overBrightBits',
              'r_ext_compress_textures']
    previous = 0
    for sample in samples:
        count = (sample - previous) * (2 if retail else 1)
        if previous == 0:
            # Retail reads the previous render immediately; fnTech3 captures
            # at the end of the current frame. Wait counters also differ.
            count += 2 if retail else -1
        result += [f'wait {count}', 'viewpos', f'screenshot frame_{sample}']
        previous = sample
    if not retail:
        result += ['wait 1', 'renderer_switch']
    result += ['echo SP_FRAME_COMPARE_COMPLETE', 'quit']
    return '\n'.join(result) + '\n'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--engine', choices=('retail', 'fntech3'), required=True)
    parser.add_argument('--samples', nargs='+', type=int, default=(20, 80, 160))
    parser.add_argument('--output', type=Path, required=True, help='new capture.cfg in an isolated test home')
    args = parser.parse_args()
    try:
        script = capture_script(args.engine, args.samples)
        with args.output.resolve().open('x', encoding='ascii', newline='\n') as stream:
            stream.write(script)
    except (OSError, ValueError) as exc:
        parser.error(str(exc))
    print(args.output.resolve())


if __name__ == '__main__':
    main()
