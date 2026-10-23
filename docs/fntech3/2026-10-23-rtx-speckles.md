# RTX forest speckle follow-up — 2026-10-23

This round extends the [quality fixes](2026-10-22-rtx-quality.md) and
[traversal/filter optimizations](2026-10-23-rtx-optimization.md). The baseline
includes both uncommitted rounds; HEAD alone does not identify it. The
[machine-readable record](2026-10-23-rtx-speckles.json) identifies source,
binaries, commands, captures and measurements. No compatibility gate closes.

## Change and diagnosis

Reactive direct light previously stopped spatial filtering after two
iterations, regardless of its uncertainty. Forest's tree models cast shadows
through the dynamic geometry class even while stationary. Engine diagnostic
captures show these shadows marking much of the ground reactive; rare direct
light samples retain conspicuous bright peaks inside that narrow support.

The third existing iteration now filters reactive direct light when its
blurred variance exceeds 5% of squared luminance. Settled shadow edges retain
the previous narrow support, and the widest iterations remain excluded.
Relative variance makes the decision independent of signal exposure.
Geometry stops and luminance weights still bound every accepted sample.
Reflectance textures remain separate from filtered illumination.

This adds no rays, images, allocations or dispatches. Primary/direct resolution,
indirect sampling, bounces, light intensities, the 35 ms reactive temporal
response and the 150 ms departure window are unchanged. Both tracing backends
use this common denoiser; no tracing or profile contract changes are retained.

## Validation method

Windows x86 `win32-dev`, NVIDIA GeForce RTX 4060 Laptop GPU. Every probe requires
`rtx_rt_mode 2` and `rtx_rt_require 1`. This driver exposes compute ray tracing
to the x86 process, not the hardware ray-tracing extensions; this is not
hardware-backend runtime evidence. The renderer is rebuilt in the stable
`meson/build/win32-dev` path. Retail RTCW assets and modules are read from the
user's installation; each probe has an isolated writable home.

The fixed forest view uses 800 x 600, default full-resolution primary/direct
lighting, indirect divisor 2, one bounce, four denoiser iterations, no adaptive
budget, uncapped rendering and frozen accumulation disabled. The simulation
clock is frozen to compare the same geometry. After 240 warmup frames, six
engine PNG captures are spaced 61 frames apart. Periodic timing observations
cover that settled interval. CPU percentiles include screenshot I/O and are
not display-latency measurements.

The ground region is x=320, y=340, width=460, height=180, excluding the HUD,
sky and upper foliage edges. Bright outliers exceed the local 5 x 5 median by
30 display levels; temporal variation is mean per-pixel standard deviation
across the six captures, using mean display RGB. These are local diagnostics,
not ground-truth lighting-error metrics. Smoothing can change local brightness;
neither a lower outlier count nor a static view proves every moving scene.

All games run windowed on an unswitched private desktop, with mouse and
joystick disabled. Captures use the engine's `screenshotPNG` command. No input
is injected, no OS captures are used, and installation inventories and runtime
binary hashes are checked before and after each probe.

## Results

| Measurement | Baseline | Baseline repeat | Final |
| --- | ---: | ---: | ---: |
| GPU passes, median ms | 23.16 | 23.17 | 23.14 |
| Denoiser, median ms | 1.78 | 1.78 | 1.75 |
| CPU frame p50, median ms | 28.85 | 29.54 | 28.34 |
| Strong ground outliers, median pixels | 77.5 | 77.5 | 3.0 |
| Ground temporal standard deviation | 1.0883 | 1.0945 | 0.6458 |
| Ground mean brightness, 0–255 | 6.9884 | 6.9778 | 6.8479 |

The final comparison reduces the strong-outlier count by 96.1% and temporal
variation by 40.7%. Mean brightness in that region changes by about -2.0%.
GPU time is effectively unchanged; the CPU results also show no measured FPS
loss. These scene-specific observations are not a general FPS guarantee.
An earlier version of the support condition independently gave 1 strong
outlier at median, temporal deviation 0.6359 and GPU time 23.14 ms. The final
version also excludes zero-variance black pixels from the extra support.

A paired forest direct-light probe places a deterministic BODY caster over
a sunlit patch, moves it through four positions and disables its shadow.
The settled shadow remains clearly defined. In the fixed x=270, y=345,
90 x 30 region, mean absolute difference from the clear capture is 56.40
before the change and 56.83 after it with the settled caster. On the first
capture after disabling the shadow it falls to 13.06 and 12.93 respectively;
after 30 additional wait frames it is 11.35 and 11.29. Background sample
noise remains, but this check shows no additional departure trail. Captures
do not measure display latency.

The final build also runs forest with the simulation clock advancing at
801 x 603, covering partial denoiser workgroups. Both beauty and direct-light
captures complete normally. All recorded probes exit normally, keep the
installation and runtime binaries unchanged, and report no renderer error.

All 97 shader variants compile, the strict x86 renderer build passes, and
embedded compute/denoiser SPIR-V validates. The complete RTX Python suite
plus player-manual checks passes: **159 tests and 1,061 subtests**. The new
numerical test executes the actual support helper across exposure scales,
settled black/white shadow edges, uncertain dark receivers and long pass
counts, ensuring only the one permitted extra iteration receives support.

## Discarded experiments

Explicitly resetting the sun response output did not improve the result.
A per-ray previous-vertex motion check also left the reactive region and
speckles unchanged, while increasing GPU time from about 23.16 to 24.84 ms.
It was removed. A class-colour diagnostic confirmed the stationary tree
blockers belong to the dynamic model class. Diagnostic shader changes are
not present in the final sources.

## Scope and remaining issues

The change retains fnTech3's traced direct lighting and shadows, indirect
bounces, material/specular transport, fog and liquid presentation. It adds no
retail-exact alternative or profile-specific feature omission. Runtime
evidence here is bounded to forest; the common unit suite does not replace
all sixteen profiles' renderer gates. Foliage silhouette aliasing and residual
sampling noise remain outside the measured ground-speckle improvement.

The unrelated baseline asset warning remains: forest references missing
`textures/lightimage/light_m15.blend.tga` for `textures/lights/mlight_10k`.
No retail assets were changed.

Useful local evidence is retained under `.tmp/rtx-speckles/`: build/test logs,
probe scripts, run metadata, console logs, metrics and engine PNG captures.
Disposable writable homes, copied baselines and compiler temporaries also
remain there: automatic approval review blocked cleanup, including a retry
with explicit absolute paths after containment and link checks. The tool
reported only "blocked by policy". Each run's generated probe configuration
has been preserved beside its captures, independently of its disposable home.
