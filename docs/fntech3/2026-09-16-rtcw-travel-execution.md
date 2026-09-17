# RTCW travel execution

The shared movement implementation now applies original RTCW policies to its
22 walking, crouching, jumping, swimming, ladder, teleporter, mover and grapple
travel routines. Quake retains its existing policies. Movement-to-goal remains
unadmitted until its orchestration and external result contract are complete.

RTCW uses original float normalization and snapshots movement gravity, step
and barrier settings at setup. SP deliberately skips obstruction traces; MP
retains them. Walking switches toward the next endpoint within 32 units and
crouches only when the destination supports crouching but not standing.
Barrier jumps move forward during takeoff and aim beyond the landing point.
Ledge approach speeds differ between SP and MP; both crouch during descent.
Water-jump completion and jump-pad approach normalize their movement vectors.

SP ladder movement projects onto the ladder axis, corrects lateral drift,
backs down the ladder and uses a projected approach point. MP uses its fixed
approach point and distinct descent view. Elevator arrival retains the C
integer-truncation behavior of the original height comparison. A waiting
bobbing platform returns RTCW's elevator-wait result type.

Grapples use `hookon`/`hookoff`, without Quake's weapon selection or launch-path
trace. Original RTCW's model-name lookup returns zero; hook-state inspection
therefore uses model-zero entities and reports no laser-hook success. This
observed limitation is retained explicitly. RTCW rocket jumps select weapon
5, do not require Quake's preexisting downward view, and finish with the
original 800-speed request; the action layer still clamps movement to 400.

## Evidence

Policies were independently implemented over inherited GPL-2.0-or-later code
using the original [SP movement source](https://github.com/id-Software/RTCW-SP/blob/master/src/botlib/be_ai_move.c)
and [MP movement source](https://github.com/id-Software/RTCW-MP/blob/master/src/botlib/be_ai_move.c).
Original and [iortcw](https://github.com/iortcw/iortcw) GPLv3 implementation
bodies and notices remain confined to disposable reference programs.

`tests/rtcw_travel_tests.py` exercises real movement, elementary actions,
entity lookup, AAS traces and physics, with controlled BSP/model callbacks.
The cases include profile controls, obstruction/presence policies, settings
lifetime, approach and descent, hook state/reset, movers and weapon choice.
Fourteen focused tests, including movement dependencies, pass on Windows x86
(25.450 seconds), Windows x64 (25.011 seconds), and Linux x86 with address and
undefined-behavior sanitizers and leak detection (162.827 seconds).

`scripts/probe_rtcw_travel.py` compares 126,720 cases per reference across all
22 routines, positions, links, movement flags, obstruction responses and
settings. It checks requested movement, final actions, commands, changed
state, trace counts and defined result fields. Both sides share engine AAS,
prediction and entity services; this isolates travel policy rather than
establishing original collision/prediction parity. The original C `abs`
binding is retained explicitly in the C++ reference fixture.

Original SP and MP agree exactly under strict Windows x86/x64 arithmetic and
sanitized Linux x86 SSE2. The reference result-clear function is wrapped to
seed fields that it otherwise leaves undefined. Only overwritten words are
compared: 397,080/395,820 defined optional words for SP/MP, with
489,960/491,220 untouched words excluded, in addition to the six always
initialized integers per result. Whole-record byte parity is not claimed.
iortcw initializes the entire record; all 887,040 optional words are compared.

Strict iortcw SP/MP comparisons have 8,355/9,543 differing records. Fast-math
Windows x86 has 216/10,380 differing records against original SP/MP and
57,213/57,297 against iortcw. None changes the compared result integers,
action flags/weapons, movement-state flags/jump status, or emitted commands.
Numeric differences remain diagnostics; this observation does not supersede
the installed prediction corpus's previously recorded decision difference.

Default and experimental Win32 client/dedicated builds with GLX, VK and RTX
pass, as does the complete Linux x86 movement translation unit.
The full suite passes 111 Meson tests with two skips, including 1,440 Python
tests with 11 skips in 394.553 seconds.
The default executable SHA-256 is
`8e9004fe1d6471fd811b0df88d0aabbdf549216a6e39a28d088594e5f80c009a`;
the experimental executable SHA-256 is
`dd29d920751d114a788b2d5ec481395d25bcc7615f03cb302cbc83ddb856c137`.
Both include recorded uncommitted work after `a80f240`. Evidence and retained
fixture-investigation failures are under `.tmp/rtcw-manager/travel-*`.

All eight engine screenshots were inspected: readable SP mission briefing
and ready arrow, MP deployment screen/movie, Q3 world/weapon/HUD on three
renderers, and QL join UI/world on three renderers. SP remains paused before
gameplay; QL has not joined the match. Tests used windowed private desktops,
disabled input, isolated writable homes and unchanged installations.
SP initializes 29 soldier clients and emits `rockandroll` while paused; MP
retains pure admission. Temporary homes were removed after the probes.

## Remaining work

The checked movement-to-goal import still needs its frozen result, goal/result
flag mappings, previous-area ladder fallback, route lifetime/stuck detection,
jump-pad recovery and distinct future-view policies. Travel fixtures do not
establish safe public admission of arbitrary module inputs or missing mover
geometry. Active cast AI, installed dynamic collisions, campaign/save-load
transitions, demos and peer interoperability remain unverified. Default RTCW
startup remains gated; no full compatibility claim is made.

The unrelated Q3/QL routing-cache reader bounds audit remains open. No new
unrelated production issue was found in this checkpoint.
