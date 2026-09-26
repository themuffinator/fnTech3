#!/usr/bin/env python3
"""Prepare a Holomatch capture.cfg for matching retail/fnTech3 render frames.

This only writes a command script; it does not launch a game or control input.
Start an installed demo with cl_freezeDemo=1, timedemo=0, and activeAction set
to "exec capture.cfg". Use isolated homes, windowed mode and disabled input.
See docs/fntech3/2026-09-21-stvef-effect-captures.md for the tested settings.
"""
# Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later.

import argparse
from pathlib import Path


def capture_script(engine, samples=(80, 280, 460)):
    """Return two successive captured frames at each increasing demo sample.

    Retail 1.20 executes wait twice per rendered frame and reads pixels when
    screenshot runs (stvoyHM.exe 0x453430 -> 0x4530f0). fnTech3 schedules its
    capture for the end of the frame. Retail therefore renders the held frame
    before each command; fnTech3 renders it afterwards. Both advance exactly
    two held frames per sample, preserving subsequent cgame effect/RNG calls.
    """
    if engine not in ("retail", "fntech3"):
        raise ValueError("engine must be retail or fntech3")
    samples = tuple(samples)
    if (not samples or any(type(n) is not int or n <= 0 or n > 1_000_000 for n in samples)
            or any(a >= b for a, b in zip(samples, samples[1:]))):
        raise ValueError("samples must increase strictly within 1..1000000")

    retail = engine == "retail"
    factor = 2 if retail else 1
    result = [f"wait {120 * factor}"]
    previous = 0
    for sample in samples:
        result.extend(("set cl_freezeDemo 0", "set timedemo 1",
                       f"wait {(sample - previous) * factor}",
                       "set timedemo 0", "set cl_freezeDemo 1"))
        if retail:
            result.append("wait 2")
        result.extend((f"screenshot frozen_{sample}_a", f"wait {factor}",
                       f"screenshot frozen_{sample}_b"))
        if not retail:
            result.append("wait 1")
        previous = sample
    if not retail:
        result.append("renderer_switch")  # Logs the effective backend.
    result.extend(("echo FROZEN_DEMO_COMPLETE", "quit"))
    return "\n".join(result) + "\n"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--engine", choices=("retail", "fntech3"), required=True)
    parser.add_argument("--samples", type=int, nargs="+", default=(80, 280, 460))
    parser.add_argument("--output", type=Path, required=True,
                        help="new capture.cfg inside an isolated test home")
    args = parser.parse_args()
    try:
        script = capture_script(args.engine, args.samples)
    except ValueError as exc:
        parser.error(str(exc))
    output = args.output.resolve()
    try:
        with output.open("x", encoding="ascii", newline="\n") as stream:
            stream.write(script)
    except OSError as exc:
        parser.error(f"cannot write a new capture script: {exc}")
    print(output)


if __name__ == "__main__":
    main()
