# Elite Force: separating electricity jitter from renderer differences

The largest residual in the [matched-frame captures](2026-09-21-stvef-effect-captures.md)
is mainly caused by different randomized electricity ribbons. Giving both
engines the same constant random input reduces the selected burst's RGB
mean absolute difference to 0.65–0.67 out of 255 on all three renderers.
This is diagnostic evidence from instrumented copies, separate from the
earlier production captures. No production effect code or binary changes.

## Draw and native observations

An instrumented GLx renderer traces the `demo0001` burst at time 2218000.
It has six electricity ribbons using `gfx/misc/dnBolt`, each with 64
vertices and two additive material stages. Their positions change between
frames while the view time, material time and blend states stay fixed.
The other sixteen traced material-stage draws, including ordinary lines,
particles and burn marks, retain identical geometry, texture coordinates,
colors and view matrices across the three frames at that time.

The existing native geometry probe was extended to count its owned
replacement for retail `rand`. All 56 native geometry records remain
identical to the previous fixture. Each electricity entity consumes 45
random values: three for each of fourteen ordinary segments, another
three for the penultimate segment, and none for the final endpoint. The
count remains 45 for zero deviation and zero-length fixtures. The seven
other primitive types consume none in these observations. The installed
`stvoyHM.exe` routine at `0x475d90` and its six call sites support these
counts independently of the engine implementation.

The [native fixture](../../tests/fixtures/stvef_surface_retail.json) now
includes these observed counts, and the existing compiled test asserts them
for all three renderers at both batch offsets. It continues to compare
geometry, colors, UVs, indices, capacity flushes and malformed-number
handling. fnTech3's existing implementation already has the correct counts.
This adds a check on random-stream consumption, not a new random generator
or a claim that whole-process random streams are identical.

## Controlled runtime comparison

Four independent test runs use identical constant input, 16384 out of the
retail 15-bit range, only for electricity jitter. A private copy of the
retail executable replaces the six calls inside that tessellator with the
constant result. Private fnTech3 renderer builds override the corresponding
macro only while compiling the shared Elite Force surface header. Other
geometry and material code is unchanged. The original installation and
production binaries retain their original hashes.

Both successive captures at each of samples 80, 280 and 460 use the checked-in
capture helper's command sequence. Every run exits normally and produces
six engine screenshots. All sixteen control images at samples 80 and 280
are pixel-identical to their respective uninstrumented captures. The two
burst images in each engine change when its random jitter is replaced.

| Renderer | Original burst A / B RGB MAE | Matching constant jitter A / B RGB MAE |
| --- | --- | --- |
| GLx | 1.3436 / 2.2037 | 0.6671 / 0.6668 |
| Vulkan | 1.3247 / 2.1962 | 0.6501 / 0.6499 |
| RTX | 1.3247 / 2.1962 | 0.6501 / 0.6499 |

Each value compares all 640x480 RGB channels with its corresponding retail
image, without masks, alignment or selecting a best frame. The controlled
images were inspected for world, weapon, beam, burst and HUD placement.
They are close, not pixel-identical: individual channel differences still
reach 240. The result identifies the random ribbons as the dominant excess
difference in this scene; it does not establish full renderer parity or
justify changing production randomness just to match one image.

The native count probe runs mapped mathematical routines with owned inputs,
without game startup or device access. Runtime tests use isolated writable
homes and independent retail copies under `E:/Games/fnTech3-tests`, a private
desktop that is never activated, windowed mode and disabled mouse, joystick
and sound. All captures use the engine's registered screenshot command.
No OS capture, input injection, installed-file changes or proprietary files
in Git are involved. RTX uses the Windows driver's SDR raster fallback.

## Verification and remaining gates

The expanded surface fixture passes on Windows x86 and Ubuntu Linux i686,
covering 336 geometry/count comparisons per platform plus the existing
bounds modes. Linux enables address, undefined-behavior and float-cast-
overflow sanitizers. The Windows check took 3.886 seconds and Linux took
15.494 seconds. An initial Windows invocation outside the configured x86
environment was rejected by the existing pointer-width guard; rerunning
through the x86 build wrapper passed. No complete production suite was
rerun for these test/evidence changes; the previous build and suite results
remain in the [force-feedback record](2026-09-21-stvef-force-feedback.md).

The [manifest](evidence/2026-09-21-stvef-electricity.json) keeps instrumented
and production identities distinct, and records the native observations,
commands, source files, logs, screenshots, controls and image metrics.
Local probes and their build outputs are retained under `.tmp/stvef-gaps`.
Previously cleanup-rejected files remain untouched.

Broader SP image comparisons, campaign play, physical input, listening and
force-feedback devices, and native Windows ray tracing remain open. Both
Elite Force profiles stay development-gated. No unrelated defect was found
in this investigation.
