# Elite Force Holomatch effect geometry

**Live-evidence correction (2026-09-21):** These probes used `r_renderer`,
which does not select fnTech3's backend. All fnTech3 live runs in this record
used GLx; Vulkan/RTX labels in the historical manifest identify the requested
test cases only. Source, native-vector and compiled-fixture evidence is
unchanged. See the [renderer audit](2026-09-21-stvef-renderer-verification.md).

All eight Holomatch effect primitives now have executable geometry comparisons
against retail in each renderer. The checks also exposed two undefined
float-to-integer conversions: cylinder detail was converted before clamping,
and electricity length was truncated without checking its range. Both are
bounded before conversion without changing the measured ordinary geometry.

## Native observations

The local probe maps the user's `stvoyHM.exe` 1.20 and calls its
`RB_SurfaceEntity` at `0x476c30` with independently chosen synthetic records.
The probe reads the tessellated vertices and indices directly. A known random
sequence replaces the retail PRNG only in the mapped probe image. The
installation is unchanged; no game window, input or screen capture is used.

The checked-in `tests/fixtures/stvef_surface_retail.json` contains only those
numeric observations and provenance. It covers oriented sprites, sprites with
per-corner colours, view-facing and oriented lines, tapered lines, Beziers,
cylinders and electricity. Seven variants exercise ordinary and mirror views,
rotation, wrapping, tapering, zero-length lines, and distance/bias boundaries
for cylinder detail.

Across 56 cases and 1,651 vertices, the initial native comparison finds:

- Identical vertex and index counts, index order, texture coordinates and
  colour bytes.
- Maximum positional difference of 0.0000058 in the MSVC probe and 0.0000019
  in the Linux/Clang probe. Regression fixtures allow an absolute 0.00002 for
  the compiler-dependent floating-point calculation.

Retail leaves some primitive alpha bytes untouched. The synthetic native
buffer starts with alpha 255 for comparison; separate tests retain the
engine's deliberate use of the entity's alpha for ribbons and lines.
These primitives belong to the Holomatch record union. SP's ordinary
sprite/beam record remains a separate contract.

## Bounds and executable checks

Cylinder detail is clamped in floating point to 7 through 32 before integer
conversion. Non-finite or excessive bias cannot reach an undefined cast.
An electricity length must be finite, non-negative and below 2^31 before
retail's whole-unit truncation; rejected lengths do not touch the batch.

Both original failures reproduce under UndefinedBehaviorSanitizer: a NaN
cylinder bias and an electricity length of 4,294,967,296. The corrected code
passes these cases, positive and negative infinities, large finite values,
and a large accepted length. This is a bound on those conversions, not a
general claim that every possible malformed effect field is sanitized.

`stvef_surface_tests.py` compiles the shipping shared effect implementation,
engine math functions and each renderer's actual quad builder. It compares
all 56 native cases both in an empty batch and after existing vertices.
It also exercises capacity-triggered flushes, index containment, finite
geometry for degenerate fixtures, preserved vertex alpha and unknown types.
The affected renderer tests pass on Windows; the new fixture also passes
under Ubuntu/Clang with address, undefined-behavior and float-cast-overflow
sanitizers. The build succeeds for all three Windows x86 renderers.
The complete Windows x86 suite passes 120 targets with two skips; its Python
target passes 1,842 tests with 72 conditional skips.

## Windowed checks and limits

Three runs of the rebuilt GLx renderer each play the opening portion of
the installed `demo0000.efdemo`, with three engine `screenshot` captures.
Every run reaches its completion marker and exits normally. Tests use
isolated homes under `E:\Games\fnTech3-tests`, windowed mode and disabled
mouse/joystick input; no operating-system capture is used.

The early and later images are byte-identical across the three GLx runs.
The shield image differs between those GLx runs, with mean
absolute differences of 2.06, 4.16 and 5.04 in its RGB channels. The effect
waveform uses each process's wall clock; these captures do not synchronize
that clock and do not establish full image parity.

The [evidence manifest](evidence/2026-09-20-stvef-effects.json) records build,
source, retail and fixture identities, commands, sanitizer observations and
capture hashes. Local probes, logs and images remain under
`.tmp/stvef-gaps/surface-runtime` and its parent in the isolated worktree.

Full renderer/shadow comparisons and audible movie parity remain separate
gates. This slice strengthens effect geometry and malformed-number handling;
it does not remove the profile's development gate.
