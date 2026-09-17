# RTCW movement prediction

This checkpoint implements SP/MP movement-prediction import 319 and the
original release behavior of ShowRoute import 320 in the shared executable.
It follows the tactical-navigation checkpoint after
`a80f24099284604326ec41dc41b4f73d7442ecd0`. Installed-map movement, dynamic
entity collisions and live campaign behavior remain open.

## Contracts and implementation

The original [SP movement routines](https://github.com/id-Software/RTCW-SP/blob/master/src/botlib/be_aas_move.c),
[MP movement routines](https://github.com/id-Software/RTCW-MP/blob/master/src/botlib/be_aas_move.c),
their sampling and game headers, and [iortcw](https://github.com/iortcw/iortcw)
provide contract observations. Production extends the inherited GPL-2.0-or-later
engine and uses independently authored profile policies and adapters. GPLv3
reference functions run only in disposable comparison programs with their
original notices; their implementation text is not incorporated into the engine.

The frozen RTCW result is 80 bytes, with a 36-byte trace and floating-point
`endcontents`. Quake's internal result is 84 bytes, includes `endarea`, and
uses integer contents. The adapter marshals each field explicitly and converts
contents numerically. Native and QVM arguments use checked complete spans,
owned input vectors and staged output, including unaligned and overlapping
buffers. Both original release ShowRoute bodies are empty under their normal
build configuration; import 320 returns without accessing arguments or AAS
state. Debug-only route visualization remains unimplemented.

The boundary audit also corrects cast/tactical entity guards. Original SP and
MP both define `GENTITYNUM_BITS` as 10, allowing indices 0 through 1,023.
SP's allocation of 2,048 internal botlib slots is not a larger game-module
contract. Callback, hide/attack and prediction adapters now enforce the game
limit; prediction additionally permits the original no-entity sentinel -1.
The ABI probe checks this constant against all four reference trees.

RTCW selects fixed original physics independently of Quake's physics cvars.
This includes walk speed 300, step height 18, water-jump height 17 and distinct
SP/MP barrier heights. The inherited algorithm applies original RTCW dry/water
friction selection, vector normalization, SP start-solid ground behavior,
slime gap handling and final-area jump-pad/teleporter ordering. Quake profiles
retain their settings, trace-event ordering and normalization. Trace traversal
also checks for an empty stack before decrementing, avoiding an invalid
one-before-array pointer.

Prediction requires initialized AAS arrays and valid plane accesses, checks
finite coordinates and arithmetic, and limits work to 4,096 frames. This is an
engine safety limit, not an external ABI constant. Negative frame counts and
nonpositive frame durations retain original zero-frame/default-time behavior.
Unknown stop bits are ignored as in the original routines. A failed prediction
publishes its cleared result; hide/attack imports retain their separate rule
of leaving failed output unchanged. Q3/QL reject RTCW imports before argument
access.

## Source comparisons and limits

`scripts/probe_rtcw_prediction.py` exercises the actual engine prediction and
AAS world tracing against original SP/MP and iortcw. Each reference receives
8,192 predictions over two worlds and four synthetic geometries: floor,
crouch-only passage, step and slope. The corpus varies liquids, commands,
velocities, frame counts, durations and 15 stop masks, comparing return values
and the complete external record. Dynamic entity collision deliberately
returns no hit in this fixture; it is not evidence for that path.

Original traces read an uninitialized direction when a trace starts in solid.
The probe records those raw observations but excludes any prediction using
that path from agreement. Strict runs exclude 925 cases, leaving 7,267 defined
comparisons per reference. Windows fast-math excludes 926 because its arithmetic
changes one such path. The inherited engine initializes that direction; unsafe
original behavior is not reproduced.

| Arithmetic and target | Original SP and MP defined-record differences | Decision differences |
| --- | ---: | ---: |
| Strict, Windows x86 and x64 | 0 | 0 |
| Strict SSE2, Linux x86 | 0 | 0 |
| Strict default x87, Linux x86 | 790 | 0 |
| Fast-math, Windows x86 and x64 | 898 | 0 |
| Fast-math default x87, Linux x86 | 797 | 0 |

The default x87 strict agreement gate therefore remains **failed**, despite
matching decisions in this corpus. Maximum observed position, velocity and
trace-fraction differences are respectively 0.00164794922, 0.0000762939453
and 0.000000282656401. Windows fast-math maxima are 0.00610351562,
0.000122070312 and 0.000000357627869. The SSE2 comparison isolates extended
precision; it does not change production compiler settings. These observations
do not prove numeric agreement for retail executables or untested inputs.

iortcw changed friction selection and vector normalization. Its diagnostic
comparisons report 102 decision differences per reference, in addition to
numeric differences. Original SP/MP behavior is the selected contract; this
checkpoint makes no iortcw prediction-parity claim. Complete reference source
hashes, fixture hashes and compiler commands are retained in each report.

## Validation

The two new executable tests cover actual prediction/world tracing and the
module adapter. Cases include profile physics, event order, both worlds,
crouching/jumping, work limits, partial initialization, invalid planes,
nonfinite/oversized arithmetic, native/QVM exact-end bounds, aliasing, invalid
backend results and cleared failure output. Cast/tactical tests additionally
reject entity 1,024 and 2,047 in SP while accepting 1,023.

Sixteen focused tests pass on Windows x86 in 26.830 seconds, Windows x64 in
25.344 seconds and Linux x86 with AddressSanitizer, UndefinedBehaviorSanitizer
and leak detection in 191.905 seconds. Complete movement, sampling, routing
and server-bot translation units pass Linux x86 syntax checks. Frozen ABI
probes pass 2,169 SP and 2,159 MP assertions against each original/iortcw
reference on Windows x86 and x64. The x64 fixtures do not establish retail
native-module support on that architecture.

Both Win32 release configurations build the client, dedicated server and all
three renderers. The full Meson gate passes 111 targets with two skips; its
Python gate passes 1,424 tests with 11 skips in 374.023 seconds.
The first suite run encountered Windows linker error LNK1104 when an existing
material test reused a just-executed output filename for its next renderer.
An isolated rerun passed; the fixture now uses a distinct filename per renderer
to avoid that reuse. The failed log and rerun are retained separately.

Windowed SP/MP map probes pass through the same experimental executable with
isolated writable homes and unchanged installed content. Personally inspected
engine captures show SP's mission briefing and ready arrow, and MP's deployment
UI/movie. SP reaches `rockandroll` with 29 soldier clients while paused; MP
retains pure mode. No input is supplied and these probes do not establish live
prediction calls or campaign play. All six Q3/QL renderer probes also pass with
inspected engine captures and verified module/renderer selection; QL remains
at its join UI. Temporary home paths are removed.

Evidence is under `.tmp/rtcw-manager/prediction-*`, with final identities in
`prediction-source-identities.json`. Final comparison reports use the
`prediction-compared-` prefix; map evidence is `prediction-checked-map` and
Q3/QL evidence is `prediction-live`. Earlier build/comparison failures and
the pre-correction `prediction-map` probe remain distinct historical evidence.
Default client SHA-256:
`6362f5ebf61d6910b905c6fb1b0efffa3bd9c7adb31b519561875a1cc1d7541b`;
RTCW development client:
`7e6c076e965eb37782db1e6d22f1293650b9f63cc8e0f8b930a3a00cb1038405`.
Proprietary modules and assets are not tracked.

## Remaining scope

Installed-map movement and tactical comparisons, dynamic entity collision,
cold-cache retail observations, missing-cache generation/writing, installed
MP-cache evidence, live campaign AI, save/load, transitions, demos and
bidirectional interoperability remain open. Default RTCW startup stays gated.
The existing unrelated Q3/QL routing-cache reader bounds audit remains open;
the unrelated material-test output reuse found during validation is corrected.
