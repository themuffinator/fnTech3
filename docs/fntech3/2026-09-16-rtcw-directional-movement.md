# RTCW directional AI movement

The shared executable now admits game import 550,
`BOTLIB_AI_MOVE_IN_DIRECTION`, through a checked native/QVM adapter. It owns
the direction vector, checks scalar encodings and coordinate bounds, and
requires a boolean result. Invalid movement handles retain the state pool's
nonfatal behavior. The incoming speed is not clamped to 400: prediction uses
the requested speed, while the existing elementary-action service clamps the
eventual command as the original engine does.

The inherited movement implementation now observes these RTCW policies:

- Walking trusts the game-supplied grounded flag. Quake still probes the floor.
- Slime does not veto predicted movement and, like water, prevents a gap from
  being classified as a dangerous dry fall. Lava and damaging landings still
  veto movement.
- Movement AI captures `sv_step` and `sv_maxbarrier` at setup, with defaults
  18 and 32. Quake retains live library-variable lookup. These settings are
  separate from the AAS prediction physics settings.
- Direction normalization follows original RTCW float arithmetic. Existing
  swim, barrier-jump, crouch, landing-gap and airborne action ordering remains.

RTCW rejects incomplete AAS worlds, missing callbacks/action storage,
out-of-range clients/entities, nonfinite values and excessive coordinate or
time-speed arithmetic before generating actions. A failed prediction produces
no subsequent movement actions. The originals ignore the prediction's return
value; refusing to act on a failed prediction is an explicit engine safety
policy. Movement-state flags already changed during the attempted movement
are not rolled back. Original zero-valued settings before setup are retained.

## Source and fixture evidence

These policies were observed in the published [SP movement code](https://github.com/id-Software/RTCW-SP/blob/master/src/botlib/be_ai_move.c),
[MP movement code](https://github.com/id-Software/RTCW-MP/blob/master/src/botlib/be_ai_move.c)
and [iortcw](https://github.com/iortcw/iortcw). Production changes independently
extend inherited GPL-2.0-or-later code. Reference GPLv3 function bodies and
notices are confined to disposable comparison programs, not shipped game code.

`tests/rtcw_direction_tests.py` exercises the real state pool, movement
functions, AAS geometry/prediction and elementary actions. Controlled
prediction results separately exercise stop events, failed prediction and
the jump frame limit. Boundary tests cover unaligned/native/QVM vectors,
exact end spans, null and invalid spans, widened scalars, nonfinite values,
mutating backends, invalid returns, partial initialization and teardown.
Q3/QL checks retain their distinct grounding, slime and setting behavior.

`scripts/probe_rtcw_direction.py` compares 51,200 queries against each original
SP/MP and iortcw implementation. The corpus includes dry/liquid movement,
grounded/airborne flags, five direction vectors, signed and over-command-limit
speeds, crouch/jump combinations, two setting snapshots, and synthetic floor,
step and gap geometry. All combined result, movement-flag, action and prediction
argument records match under strict Windows x86/x64 arithmetic and Linux x86
SSE2 with address/undefined-behavior sanitizers and leak detection. Each run
contains 48 barrier-jump actions and no failed predictions.

Both implementations use fnTech3's same AAS traces, prediction and elementary
actions. This isolates directional policy, not original collision or prediction
parity. The fast-math Windows x86 diagnostic has 25,744 differing combined
records per reference, with zero changed success decisions in this corpus.
This is not a general numeric-parity guarantee; the separate installed-map
prediction checkpoint records a decision difference under fast math.

Nine focused tests pass on Windows x86 and sanitized Linux x86. Windows x64
passes the six dependent tests and all three directional tests after correcting
an overly strict unit-vector assertion for fast-math rounding. Strict source
comparisons still require exact records. Initial test/probe diagnostics are
retained, including the initial probe's incorrect expected query count.
Complete Linux x86 movement/server-dispatch translation units compile, and
default/experimental Win32 client and dedicated-server builds pass.
The full suite passes 111 Meson checks and 1,431 Python tests, with two and
11 expected skips respectively.

The default executable SHA-256 is
`15ac621085cd8d080aa55a25c4cc7a93a1d6e332fbb085435c41a7854050fe16`;
the experimental executable SHA-256 is
`198ce18a21b7fc44afdbd6e2ab388ef1a74cbcabd78609760d1b33daf7d8c5be`.
Both contain recorded uncommitted work after `a80f240`.

Eight engine screenshots were personally inspected: readable SP mission
briefing/ready arrow, MP deployment UI/movie, and Q3/QL scenes across GLX,
VK and RTX. Runs used windowed private desktops, disabled user input, isolated
writable homes and read-only installed assets. QL remains at its join screen;
SP remains paused before campaign play. These captures establish startup
non-regression, not active actor movement. Useful logs, source identities,
reference reports and captures remain under `.tmp/rtcw-manager/direction-*`.

## Remaining gates

Movement-to-goal and view/visible-position imports still need explicit RTCW
contracts. Their goal/result flag meanings conflict with Quake despite similar
record sizes, and SP/MP use different route selection and future-view policies.
Active campaign AI, moving-entity physics, cold navigation-cache generation,
save/load, transitions, demos and peer interoperability remain unverified.
Stock MP bot behavior is not established by these fixtures. The default RTCW
startup gate remains closed; full compatibility is not claimed.

The unrelated Q3/QL routing-cache reader bounds audit remains open. No new
unrelated production issue was found in this slice.
