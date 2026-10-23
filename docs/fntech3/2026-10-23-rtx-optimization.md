# RTX traversal and filter optimization

This follow-up measures two performance changes on top of the uncommitted
[October 22 quality fixes](2026-10-22-rtx-quality.md). The checkout's committed
base is `d01a4b72e803726ecb228c1b0f7fb5959eba1200`; that commit alone is not the
benchmark baseline. The adjacent [JSON record](2026-10-23-rtx-optimization.json)
identifies the working sources, binaries, installations, commands and results.

## Changes

Compute ray traversal retains each deferred child's already-tested box entry
distance alongside its node index. Only the closest hit distance changes while
that child waits. Comparing those distances can reject a deferred child without
loading its node or repeating a slab test. The stack grows from 256 to 512
bytes per invocation; its depth limit and the CPU builder's depth bound remain
unchanged. Primary, reflected, solid, masked and first-blocker shadow queries
retain their hit and material rules. The all-hit transparency traversal is
unchanged.

The spatial denoiser caches the three variance channels in an 8 x 8 workgroup
with a one-pixel halo. For an interior group, 300 image reads replace 1,728 in
each pass. All 64 invocations reach the shared-memory barrier, including lanes
outside partial edge groups and pixels owned by raster rendering. The 3 x 3
Gaussian, boundary normalization, filter radius, ray counts, bounce density,
history, geometry rejection and material boundaries are unchanged.

An additional experiment let neighboring variance widen the luminance filter.
It lowered peak speckle brightness but spread noisy energy into larger blobs
and increased temporal variation. It was discarded. The final changes preserve
the previous round's filter behavior; residual speckles and foliage edge
aliasing remain.

These are original changes to fnTech3's existing code. No external
implementation text was incorporated.

## Method and scope

The stable Windows x86 development runtime was rebuilt in place between
variants. The other fnTech3 renderer chat had finished its GPU/build work.
Tests used an NVIDIA GeForce RTX 4060 Laptop GPU, windowed rendering, an
unswitched private desktop, disabled mouse/joystick input, isolated writable
homes and read-only installed assets. All captures came from the engine's
registered `screenshotPNG` command. Installation inventories and runtime hashes
were checked before and after each run.

Every probe requires `rtx_rt_mode 2` with `rtx_rt_require 1`. This x86 driver's
missing hardware RT extensions select the compute ray-tracing backend. Both
backend shader variants compile; hardware RT execution remains untested.

Fixed-view benchmarks warm up for 240 frames, freeze simulation with
`timescale 0`, disable frozen accumulation and FPS caps, and capture six frames
61 frames apart at 800 x 600. Default full-resolution primary/direct lighting,
one bounce, indirect divisor 2 and denoising remain enabled. Periodic GPU
timings after warmup are compared; CPU values are rolling frame p50 reports.
Engine PNG writes affect CPU tails, and laptop power and scene costs limit
generalization to gameplay. No clock or power settings were changed.

The shared filter serves all RTX profiles, while only the compute backend
uses the traversal change. fnTech3's direct and indirect lighting, shadows,
material transport, HDR, fog and liquid paths remain enabled. No module ABI,
asset format, gameplay or networking contract changed, and no retail-exact
presentation path replaces those features. These checks close no compatibility
gate and do not establish coverage for all sixteen profiles.

## Validation

All 97 shader variants compiled, and the final Windows x86 RTX DLL built with
the preset's strict compiler checks. The embedded compute trace and spatial
denoiser binaries passed `spirv-val` for SPIR-V 1.4. The RTX Python selection
and player-manual checks passed: **158 tests and 1,061 subtests**.

The new traversal fixture executes the actual GLSL against an independent
double-precision plane/triangle reference over 32,000 queries. It covers
nearest hits, first blockers, masks, alpha rejection, non-shadowing materials,
solid world versus dynamic geometry, both face directions, parallel ray
components, segment bounds and nonzero instance buffer offsets.

The variance fixture executes the shader's cache and convolution against a
direct image convolution for both ping-pong images at 1 x 1, 7 x 9, 9 x 7,
17 x 13 and 32 x 24. Poisoned shared memory catches missing halo loads, bounded
image access catches invalid coordinates, and a source check ensures every
invocation reaches the barrier before an early return. Runtime shader execution
provides the GPU synchronization check that the CPU fixture cannot.

All eleven runtime probes exited normally, with tracing active, no forced
termination, no renderer errors and unchanged installations and runtime files.
Quake III's 801 x 603 capture uses an 800 x 602 traced viewport; the engine
rounds the viewport to even dimensions. It exercises a partial bottom group
and a scripted BODY caster moving and becoming non-shadowing. A separate
807 x 607 capture traces 806 x 606, exercising partial groups on both axes.
Both beauty and direct-light captures show intact image edges. These are
bounded rendering checks, not interactive gameplay or input-latency tests.
In the first shadow probe, the floor region x=50, y=340, width=500, height=160
has a mean absolute RGB difference from the clear view of 2.29 with the settled
caster, falling to 0.64 after its shadow is disabled.

## Measurements

The following pair uses the repeated baseline and final build, with thirteen
settled timing observations per run. GPU total is the instrumented build,
trace, denoise and composite passes, rather than the entire presented frame.

| Scene | GPU total before / after, ms | Reduction | Denoise before / after, ms | CPU frame p50 before / after, ms |
| --- | ---: | ---: | ---: | ---: |
| RTCW SP `forest` | 23.98 / 23.16 | 3.4% | 2.00 / 1.78 | 29.63 / 28.86 |
| Q3 `q3dm1` | 11.67 / 11.38 | 2.5% | 1.81 / 1.63 | 14.13 / 12.99 |
| RTCW MP `mp_beach` | 13.56 / 13.29 | 2.0% | 1.53 / 1.38 | 17.27 / 16.92 |

An earlier forest pair measured 23.86 / 23.16 ms GPU total and 1.99 / 1.78 ms
denoising, supporting the repeatability of the GPU reduction. Forest's final
pair corresponds to about 33.7 / 34.7 FPS when taking the reciprocal of the
rolling median CPU frame time. CPU p95 increased from 33.37 to 38.91 ms in that
pair, so this is not a claim of uniformly improved frame pacing. No quality
setting was lowered to obtain these timings.

The October 22 shaded-ground region (x=320, y=340, width=460, height=180) keeps
similar displayed brightness and noise. Its median count of bright outliers
above a 5 x 5 local median by 30 display values is 77 / 67, and mean per-pixel
temporal standard deviation is 1.086 / 1.071 display values. The earlier pair
varied in the opposite direction for temporal deviation (1.116 / 1.122).
The final filter has the same arithmetic and sampling policy; these small
image differences do not establish a further quality gain. Frame duration
affects temporal weights, and real foliage and dappled light also contribute
to this image-space diagnostic.

## Remaining issues

The unrelated baseline asset warning remains: RTCW `forest` references missing
`textures/lightimage/light_m15.blend.tga` for `textures/lights/mlight_10k`.
Retail assets are unchanged.

Local logs, engine captures and reproduction helpers are retained under
`.tmp/rtx-round2/`. Disposable homes, source variants and compiler temporaries
are removed after validation.
