# RTCW fog on transparent materials

The two reported `escape1` screenshots expose torch polygons as pale rectangles.
Their image hashes match the Vulkan and RTX raster captures from the
[interaction/eye checkpoint](2026-09-17-rtcw-player-interactions.md). GL has the
same underlying material-pass error.

## Cause and correction

Scene fog mixed the view's fog RGB into every pass before framebuffer blending.
For an additive torch, black texture regions and stages at the dark end of an
animation waveform then contributed fog-colored light across the whole polygon.
The installed map's flame materials include `SRC_ALPHA, ONE` stages, some with
`nofog` and some without it.

The shared per-pass helper now selects the blend's neutral fog color: black
for additive/screen effects, white for multiplicative filters, and half intensity
for doubled modulation. Existing RGB fog interpolation consequently fades the
effect's contribution away while retaining the already-fogged framebuffer.
Opaque and ordinary alpha-blended surfaces still use the view's fog color.
Alpha, discard, fog visibility/distance, `nofog`, stage overrides, and disabled
Q3/QL scene-fog snapshots retain their existing contracts.

The correction is applied at GL fixed-function/GLSL and both Vulkan uniform
boundaries. It changes per-draw data and requires no shader binary or UBO-layout
change. It is independently authored from the blend equations and the observed
materials; no external implementation text or proprietary assets are included.

## Evidence

Windows x86 development build on a dirty worktree based on
`0b6af9ba7a17d73bf27c5c016c00772a1a93899a`. The client and GL/Vulkan/RTX renderer
targets build using `meson/build/win32-dev` and the existing MSVC environment.
The ordinary Windows x86 Release targets also build in `meson/build/win32`.
An initial Release regeneration encountered the old `pkg/baseq3` path during
a concurrent package relocation; the rerun against the current paths passed.

The new compiled compositing test first failed at the intended pixel-result
assertion in all three backends. It then passed with the correction. It executes
the production fog-transfer functions, including GL fixed-function/GLSL parity,
both actual Vulkan UBO layouts, ten blend pairs, partial/full visibility, source
alpha, black/white/half-intensity neutral texels, and non-neutral texels over
different framebuffer colors. It also checks normal alpha, opaque, sky, UI,
`nofog`, and stage overrides. The complete nine-test RTCW fog selection passes
on Windows x86 and Linux x86.

Three windowed installed-SP probes of `escape1` exit normally without engine
errors. Engine `screenshotPNG` captures at approximately 5 and 35 seconds were
inspected for GL, Vulkan, and RTX raster. The upper and lower torch rectangles
are gone in all three; the later character shots retain visible eyes. RTX uses
explicit raster mode 0. This does not add native ray-tracing runtime evidence.

The probes use the independently linked diagnostic client described in the
interaction checkpoint, disabled mouse/joystick devices, an unswitched private
desktop, isolated writable homes, and only the standing-authorized intro Ready
click. No gameplay input or operating-system capture is used. The legitimate
installation is unchanged and the temporary homes are removed.

Evidence is under `.tmp/rtcw-player-fixes/`: `fog-blend-before.log`,
`fog-tests.log`, `fog-linux.log`, `fog-build.log`, and
`fog-runtime/manifest.json`. Each runtime entry records the full command,
binary hashes, engine observations, installation check, and screenshot hashes.
All six `fog-runtime/eyes-*/eyes-*.png` images were inspected.
The useful evidence and reproduction scripts are retained. Automatic approval
review rejected both recursive and explicitly enumerated nonrecursive deletion
of disposable private probe outputs with `blocked by policy`; consequently
`probe-bin/`, the generated client source/object, and linker response file
remain in the same task directory. Installed content remains unchanged.

Separate observation: RTX's overall fog brightness remains visibly different
from GL/Vulkan. This change addresses transparent-pass compositing; general
fog/color-space parity, unusual blend combinations, original-engine visual
comparisons, and native ray-tracing output remain open.
