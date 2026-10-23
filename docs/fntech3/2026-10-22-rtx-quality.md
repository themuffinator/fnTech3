# RTX speckles and tracing cost

This record covers the ray-traced view (`rtx_rt_mode 2`), with
`rtx_rt_require 1` throughout runtime testing. It records source, numerical
and bounded live evidence; no compatibility gate closes. Machine-readable
identities, commands and measurements are beside it in
[the JSON record](2026-10-22-rtx-quality.json).

## Corrections

The spatial filter previously compared raw view depths. Perspective makes
even a perfectly flat floor change depth rapidly across the image, so this
test rejected useful neighbours and left isolated bright samples. Both the
spatial filter and current-frame temporal neighbourhood now compare depth
against a local reciprocal-depth plane. The smaller valid one-sided slope
avoids deriving that plane across a foreground/background edge. Existing
normal, ownership and material checks remain in place.

The temporal filter also clipped all old direct and specular illumination
to the current noisy neighbourhood. A rare emitter sample followed by dark
samples therefore lost its energy immediately. Only reactive receivers now
clip history. Moving blockers retain their departure window; moving surfaces,
lamps and flashes retain fast response. A numerical pulse sequence verifies
that quiet rare samples keep their mean at 20, 60, 120 and 240 FPS.

For sparse indirect samples, a blend near one could make the residual against
the new sample nearly zero, even for a firefly. The variance floor now also
uses the incoming sample's difference from the old history. Tests exercise
sampling densities 1, 4 and 16 across those frame rates.

Compute BVH construction now evaluates all three split axes. Choosing only
the widest centroid extent can leave overlapping children around long walls
and foliage. The extra build work buys a better traversal tree; the refit,
stack-depth and allocation policies remain unchanged. A long-triangle fixture
checks a case where the narrower axis separates the geometry correctly.
Light candidates with zero contribution skip proposal-density table reads and
membership searches. The random draws and nonzero reservoir weights are
unchanged; energy and overflow-support tests exercise the actual GLSL sampler.

All implementations are original changes to the existing renderer. No external
implementation text was incorporated.

## Runtime method

The stable Windows x86 development runtime was rebuilt in place. Tests ran
windowed at 800 × 600 on an unswitched private desktop, with mouse and joystick
disabled, isolated writable homes, and installed assets read-only. Every image
came from the engine's registered `screenshotPNG` command. The runs checked
installation inventories and executable/package/renderer hashes before and
after; no input was injected and no desktop capture was used.

The device was an NVIDIA GeForce RTX 4060 Laptop GPU. Its 32-bit Vulkan driver
does not expose the required hardware ray-tracing extensions, so these are
compute ray-tracing observations. Hardware shader variants compile, but this
record provides no hardware-backend runtime evidence.

The `forest` benchmark freezes the game clock, disables frozen accumulation,
warms up for 240 frames, then takes six captures 61 frames apart. Primary and
direct lighting remain full resolution, with the default indirect divisor 2,
one bounce, denoising enabled, and automatic quality adjustment disabled.
FPS limits are disabled. Performance logging samples every 30 frames. The
other renderer task paused its GPU tests and builds for the benchmark window.
Existing unrelated CSM edits were preserved in this shared checkout.
The initial before run preceded the other task's final raster edits; the
repeated before run and final after run both include those edits. Use that
latest pair for the comparison, with the initial run illustrating variation.

The before variant restores only the six affected pre-existing renderer files
from the recorded base commit; the after variant contains this change. The
new plane helper has no effect when the before shaders do not include it.
An attempted separate sky-endpoint/first-blocker sun query was removed after
it increased tracing cost; the final code keeps the original sun query.

## Measurements

The image region is the shaded ground at x=320, y=340, width=460, height=180.
For each frame, the RGB-channel mean is compared with its 5 × 5 median; a
strong bright outlier exceeds that median by 30 display-code values. The
table reports median counts over six captures. Temporal variation is the
mean per-pixel standard deviation across those captures.

| Measurement | Initial before | Repeated before | Final after |
| --- | ---: | ---: | ---: |
| Strong bright outliers | 260 | 250 | 108 |
| Outliers above 10 values | 951 | 906 | 462 |
| Temporal variation, display-code values | 1.560 | 1.549 | 1.162 |
| GPU trace, ms | 27.09 | 26.81 | 26.61 |
| GPU denoise, ms | 1.93 | 1.76 | 2.21 |
| GPU total measured passes, ms | 29.19 | 28.61 | 28.78 |
| CPU frame rolling p50, ms | 35.87 | 33.80 | 34.26 |

The latest pair has **57% fewer strong bright outliers** and **25% less
temporal variation** in this region. Mean displayed intensity changes from
7.15 to 6.99; this is an image-space metric, not proof of lighting accuracy.
The timing columns are medians of the periodic observations after warmup;
the CPU column is the median of rolling 256-frame p50 reports. CPU frame
times correspond to about 29.6 versus 29.2 FPS in the latest pair. There is
**no demonstrated overall FPS gain**. The tracing reductions approximately
offset the more effective filter's extra work, within run variation. World
BVH build time in that pair rises from 8.1 to 13.4 ms at map load.

The Windows x86 RTX DLL built successfully with the preset's strict compiler
checks. Shader generation compiled all 97 variants, including both tracing
backends. The complete RTX Python test selection plus the player-manual tests
passed: **155 tests and 1,061 subtests**. The first test invocation exposed a
source assertion that still expected the old proposal ordering and two fog
fixture failures from a compiler shell without the required x86 target. The
assertion now checks the new ordering, and the complete selection passed in
the configured x86 development shell; no engine architecture restriction was
relaxed.

Every probe exited normally without a renderer failure or raster fallback.
RTCW SP `forest` supplies the paired quality/timing evidence. Quake III
`q3dm1` supplies twelve direct-light captures of a scripted BODY caster moving
and then becoming non-shadowing, exercising the final denoiser before the
last proposal-evaluation optimization. The floor's mean absolute difference
from the clear view falls from 2.84 with the settled caster to 0.69 after
shadowing is disabled; these noisy captures are a bounded response check,
not a display-latency measurement. RTCW MP `mp_beach` supplies final-build
world/direct-light smoke captures after its registered `CloseLimboMenu`
command. The earlier MP capture with the limbo UI open establishes startup
only. Both MP runs use the final shader sources.

## Scope and remaining limits

The changes retain fnTech3's traced direct lighting and shadows, indirect
bounces, material reflectance and specular transport, HDR, liquid handling and
fog. Texture colour is still applied after illumination filtering. No profile
ABI, asset format, network or gameplay contract changes; no retail-exact
presentation path is substituted. The common filter affects every RTX profile,
but runtime coverage here is bounded and does not establish all sixteen games'
renderer gates.

The bright-outlier metric is a local image diagnostic, not a ground-truth
lighting error metric. Foliage silhouettes and true dappled light can also
produce high-frequency features; residual noise and alpha-test edge aliasing
remain. Timing depends on this scene, driver and laptop power state. Engine
PNG writes can affect CPU tail percentiles, so these are not display-latency
measurements or a general FPS guarantee.

An unrelated baseline warning remains: `forest` references the missing
`textures/lightimage/light_m15.blend.tga` light image for
`textures/lights/mlight_10k`. Retail assets were not changed.

Local logs, engine captures and reproduction helpers are retained under
`.tmp/rtx-quality/`; disposable writable homes and compiler temporary files
are removed after validation.
