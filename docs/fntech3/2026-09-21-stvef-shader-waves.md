# Elite Force shader clocks and noise initialization

Elite Force's `random` wave now selects its value from the view's integer
millisecond clock in all three renderers. It previously used elapsed process
time. Both Elite Force profiles also initialize the renderer's noise and
permutation tables with the deterministic sequence observed in retail,
instead of inheriting the host C library's current random stream.

## Native observations

The user's retail 1.20 executables agree on the following contracts:

| Contract | Single player | Holomatch |
| --- | --- | --- |
| Wave evaluator | `0x4642e0` | `0x47b5c0` |
| Random-wave branch | `0x46431a` | `0x47b5fa` |
| Integer view time read by that branch | `0xaaf924` | `0xac3acc` |
| Noise-table initializer | `0x458ad0` | `0x46bf40` |

The random wave truncates view time plus phase, uses the low byte to choose
a permuted noise value in the range zero to two, and compares it with the
wave's frequency. It returns base plus amplitude when that value is at most
the frequency, otherwise base. fnTech3 adds phase in double precision so
integer millisecond values beyond float's exact range retain their low bits.
Ordinary table waves continue to use material time.

Both native initializers seed at 1001 and use a 15-bit random stream with
32-bit state updated by `state * 214013 + 2531011`. The native scale words are
`0x38000100` for values and `0x3bff01fe` for permutations. Using their exact
values with double intermediates reproduces every float bit pattern and
permutation entry produced by both retail initializers. fnTech3 keeps this
state local to initialization; it does not reseed or consume the engine's
process-wide random stream. This establishes table compatibility, not identity
of every random call elsewhere in either engine. The other five profiles
retain their existing initialization path.

The local probe maps each legitimate executable and calls only these native
mathematical routines with owned storage. The SP probe supplies a private
CRT thread-storage record; its native seed, random and table-initialization
instructions are unchanged. The installation is never patched or launched
for these calls. The checked-in fixture contains numeric observations, not
executable or game-asset bytes. Implementation is independent from the
observed contract; no external implementation text was incorporated.

## Validation

`tests/stvef_wave_tests.py` compiles the shipping functions. Its two checks
pass on Windows x86 and Linux i686; Linux also enables undefined-behavior and
float-cast-overflow sanitizers where applicable. They cover:

- 512 independently chosen native wave vectors in both profiles and each of
  the three renderers, with three unrelated process clocks. Negative times,
  fractional phases, low-byte boundaries and clocks beyond 2^24 are included.
  The renderer must not read the process clock for these waves.
- All 256 native noise values and permutation entries, repeated initialization,
  preservation of the five other profiles, and no use of the process PRNG in
  the Elite Force path.

Both checks fail against their respective old implementations and pass after
the fixes. All three renderer builds complete on Windows and Linux. The
complete Windows suite passes 120 Meson targets with two skips. Its Python
aggregate passes 1,846 tests with 72 conditional skips in 697.146 seconds,
using the project's short `.tmp/w` directory for `TEMP` and `TMP`. These
Linux checks do not replace the previously reported incomplete Linux suite.

The production Windows client completes nine frozen Holomatch demo runs
(three installed demos on each verified renderer), three further runs with
captures at three later snapshot states, and `_brig` in SP on each renderer.
Retail supplies three initial-state controls and one new later-state control.
All exit normally and write engine screenshots. Three Linux i686 software
renderer runs also complete the 107-frame recorded demo and write captures.
Renderer identities are checked from initialization logs, including RTX's
capability report. Windows RTX uses SDR raster output in this comparison.

## What the images establish

At the initial held states of `demo0000` and `demo0001`, repeated captures
are identical within each engine. Full-frame RGB mean absolute differences
from retail are respectively 0.2484–0.2496 and 0.6079–0.6103 channel values
out of 255 across the three fnTech3 renderers. World geometry, weapons and
HUD placement agree in these inspected views. These states are unchanged
by the shader correction and are regression controls.

For later `demo0000` states, all four engines display matching snapshot
readouts: `(743850, 29611, 1070)`, `(753850, 30014, 1082)` and
`(762850, 30298, 1094)` for time, snapshot and command. A snapshot readout
does not establish that every cgame effect or rendering clock is identical.
Repeated captures at the shield states still differ in retail itself: RGB
mean absolute differences are 30.64 and 12.54. fnTech3 also changes between
these captures; its corresponding differences are about 32.34 and 8.91.
The initial `extreme` state also changes in both engines. These are retained
as comparison limits, not declared pixel-parity passes or attributed entirely
to the two corrected shader contracts.

The [earlier effects record](2026-09-20-stvef-effects.md) described fnTech3's
then-current wall-clock behavior. It was not retail's random-wave clock.
The native vectors here correct that implementation and the deterministic
table that the original [cgame/UI](2026-09-18-stvef-holomatch-cgame-ui.md) and
[SP cgame](2026-09-19-stvef-sp-cgame.md) records had not compared directly.

## Reproduction and remaining gates

The [manifest](evidence/2026-09-21-stvef-shader-waves.json) records source,
fixture, retail and built-binary identities, commands, logs, image hashes,
all image metrics and the separate before/after observations. Local probes
and analysis remain under `.tmp/stvef-gaps`; independent runtime homes are
under `E:/Games/fnTech3-tests`. Windows probes use a private desktop that is
never activated, and Linux uses Xvfb. All runs are windowed with mouse and
joystick disabled. Images come only from the engine's `screenshot` command.
No proprietary files or screenshots are committed.

Broader effect image parity, campaign play, physical input, listening and
device acceptance remain open. Neither Elite Force profile leaves its
development gate. No new unrelated issue was found in this slice; the
[previous runtime record](2026-09-21-stvef-runtime-coverage.md) retains the
installation, platform and cleanup limitations already reported.
