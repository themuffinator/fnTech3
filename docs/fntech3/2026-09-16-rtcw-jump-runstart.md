# RTCW jump run-up

The shared jump run-up helper now uses RTCW's normalization and traversable
slime policy. Lava and damaging falls still restore the proposed starting
point; Quake retains its slime avoidance and normalization.

RTCW travel uses a private boolean entry point. It validates coordinates and
required callbacks, owns input vectors, and leaves the output untouched when
prediction fails. Jump travel then reports failure before issuing movement
or jump actions. The existing void entry remains available to Quake and
debug drawing; RTCW debug callers receive a defined starting-point fallback.
No external botlib POD or module ordinal changes.

## Evidence

The policy extends inherited GPL-2.0-or-later code independently, using facts
from the original [SP movement source](https://github.com/id-Software/RTCW-SP/blob/master/src/botlib/be_aas_move.c)
and [MP movement source](https://github.com/id-Software/RTCW-MP/blob/master/src/botlib/be_aas_move.c).
Original and [iortcw](https://github.com/iortcw/iortcw) GPLv3 bodies and notices
remain confined to disposable comparison programs.

`tests/rtcw_jump_runstart_tests.py` checks normalization, liquid/fall events,
prediction failure without actions or state mutation, overlapping vectors,
partial initialization, real AAS prediction and horizontal jump velocity.
The original horizontal-velocity calculation agrees over defined inputs;
the inherited zero-denominator guard is retained for its undefined 0/0 case.

`scripts/probe_rtcw_jump_runstart.py` compares 9,600 run-up cases across flat
and ledge geometry, positions, directions and liquids, plus 840 horizontal
velocity cases per reference. Run-up comparisons share the checked engine
prediction and collision implementation. A failed shared prediction is
excluded because the original helper ignores its failure return; nonfinite
reference velocity outputs are also excluded explicitly. These are helper
comparisons rather than original physics/collision or live AI evidence.

Thirteen focused tests pass on Windows x86 (26.511 seconds), Windows x64
(25.910 seconds), and Linux x86 with address/undefined-behavior sanitizers and
leak detection (164.403 seconds). Full Linux x86 movement translation units
also pass their checks.

All four references agree exactly in strict Windows x86/x64 and sanitized
Linux x86 SSE2 comparisons. Each excludes 960 failed predictions and four
nonfinite reference velocities, leaving 9,476 compared cases. The defined
run-up corpus includes 2,160 stops each in water, slime and lava, plus 840 gap
stops. Fast-math Windows x86 excludes six nonfinite velocities; it produces
1,640 run-up position differences per original SP/MP reference and none
against iortcw. Compared stop events and horizontal-velocity decisions remain
unchanged. This does not supersede the previously recorded installed
prediction decision difference.

Both Win32 client/dedicated builds with GLX, VK and RTX pass. The default
executable SHA-256 is
`23c65b6420823886626a8da795c953b1845f59e7732b2c948ea6443b75790c55`;
the experimental executable SHA-256 is
`832050dedb2d55769d989215eb3688c7394ff5cbe59660eca72828957fad888d`.
These contain recorded uncommitted work after `a80f240`.

All eight engine screenshots were inspected: readable SP mission briefing
and ready arrow, MP deployment screen/movie, Q3 world/weapon/HUD on three
renderers, and QL join UI/world on three renderers. SP is still paused before
gameplay and QL has not joined the match. Probes use private windowed
desktops, disabled input and isolated writable homes; installations remain
unchanged. This is startup evidence rather than live actor movement.

The complete suite passes: 111 Meson tests with two skips, including 1,449
Python tests in 397.311 seconds with eleven skips. The runtime logs also
confirm 29 SP soldier clients and the `rockandroll` readiness command while
the mission remains paused, plus MP pure mode. All probe homes were removed.
Evidence and source identities are recorded under
`.tmp/rtcw-manager/jump-runstart-*`, including
`jump-runstart-source-identities.json`.

## Remaining work

Active cast movement, installed dynamic collisions, missing-cache generation,
campaign/save-load transitions, entity-stream audio, demos and peer
interoperability remain open. Default RTCW startup remains gated; no full
compatibility claim is made.

The unrelated Q3/QL routing-cache reader bounds audit remains open. No new
unrelated production issue was found in this checkpoint.
