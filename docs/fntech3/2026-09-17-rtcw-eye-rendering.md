# RTCW eyes, blinking, and Vulkan state recovery

This follow-up to the [mesh blink correction](2026-09-17-rtcw-player-interactions.md)
starts from `0b6af9ba7a17d73bf27c5c016c00772a1a93899a` and an already modified
worktree. The reported renderer was Vulkan. Two additional rendering gaps
were reproduced and corrected; the existing SP/MP blink-selection contracts
were also reviewed and exercised.

## Findings and implementation

**Vulkan lost the head's texture bindings after bloom.** A controlled probe
draws an open and a blinking view of each of four installed SP heads. Before
the correction, the first blinking head loses its face while the eyelid
overlay remains. The same assets draw correctly in GL and RTX raster, and
in Vulkan when all heads share one scene. Vertex/index submission and the
blink flag were correct.

`vk_bloom` uses descriptor layouts incompatible with the ordinary material
layout. Its conditional restoration depended on `last_pipeline`, which the
render-pass helper had already cleared. A subsequent draw reusing the head
material therefore retained stale bindings. Changing to the eyelid material
could make that surface visible again. A diagnostic descriptor rebind alone
restored the missing face; resetting only the pipeline or scissor did not.

The Vulkan bloom exit now invalidates the material bindings, pipeline,
viewport/depth range, and scissor caches unconditionally. The next ordinary
draw restores the supported main-layout descriptor range. This follows
[Vulkan's descriptor-layout compatibility rules](https://docs.vulkan.org/spec/latest/chapters/descriptorsets.html#descriptorsets-compatibility).
Sampler gaps receive a valid fallback, and an absent uniform binding receives
its uniform descriptor and matching dynamic offset. The single-texture path
also initializes its declared depth-fade sampler when needed. Existing valid
samplers remain cached on subsequent uniform-only updates.

The executable regression found a related RTX fallback error: its dynamic
offset was collected before the fallback supplied the correct value. Offset
collection now follows fallback resolution. RTX already invalidated its
bindings after bloom; that policy is preserved.

**RTCW MD3 head normals used the Q3/QL waveform table.** Original SP and MP
decode MD3 and MDC base-frame normals with a 1023-period sine table, without
the Q3/QL table's bias. MDC already used the correct table here, but MD3 did
not. All three raster backends now select the existing RTCW normal table for
RTCW MD3 frames, including both ends of interpolation. The native RTX MD3
decoder uses the same RTCW decoding path. Q3/QL retain their existing raster
and native-ray calculations; waveform animation tables are unchanged.

## Contract audit

- SP draws `h_blink` over the open-eye mesh only while the entity's blink flag
  is set. MP selects a skin's `<surface>_b` alternate, falling back to the
  ordinary surface when the alternate is absent. MD3, MDC, MDS, and native
  ray geometry follow these profile-specific policies.
- Custom-shader precedence, missing/invalid skin handles, and Q3/QL isolation
  remain covered by the mesh regressions. The external flag is marshalled to
  engine-owned blink state. RTCW's blink/wrap-frame bit alias is intentional.
- Blink timing, speaking-dependent scheduling, and closed eyes on dead
  characters belong to the retail cgame. The engine does not add a competing
  timer. The installed SP eyelid materials include both alpha blending and
  alpha testing; no synthetic global polygon offset is introduced.

The original [SP renderer](https://github.com/id-Software/RTCW-SP/tree/master/src/renderer),
[MP renderer](https://github.com/id-Software/RTCW-MP/tree/master/src/renderer),
and [SP character code](https://github.com/id-Software/RTCW-SP/blob/master/src/cgame/cg_players.c)
were inspected for behavioral facts only. Their GPLv3 implementation text was
not incorporated. Corrections and fixtures are independently authored changes
to this project's GPLv2-or-later implementation.

## Validation and retained evidence

Evidence is under `.tmp/rtcw-eyes/`. `evidence.json` pins source, build and
capture identities; each runtime folder retains its command/configuration,
binary hashes, engine log and registered `screenshotPNG` output.

- Windows x86 development (`rtcw-development=true`) and Release (default RTCW
  gate retained) build the client and all three renderer modules. The final
  incremental build logs are `normal-dev-build.log` and
  `normal-release-build.log`.
- `vulkan_bloom_state_tests.py` executes the shipping bloom and descriptor
  functions against a driver-state recorder. It covers unchanged head
  materials, eyelid changes, empty fallback state, dynamic offsets, four/five
  descriptor-set limits, cache reuse, and bloom's early-return guards.
- `rtcw_md3_normal_tests.py` compares every 16-bit packed normal for all four
  profiles in each raster backend, exercises interpolated poses, and checks
  RTCW raster/native-ray agreement with legacy Q3/QL ray behavior preserved.
- Both new tests and the existing mesh-blink/renderer-record tests pass on
  Linux x86 (six tests). The Windows RTCW-focused run passes 162 tests.
  `baseline-state-regression.log` records successful compilation
  followed by the expected behavioral failures in the old Vulkan bloom path
  and all three old RTCW raster normal decoders.
- Installed SP head captures in `verified-sp-{vk,glx,rtx}` exercise infantry,
  Dr. Zee, elite guard and officer heads, with open and closed eyes together.
  `verified-mp-vk` exercises Axis and Allied MD3 heads. The sampled stock MP
  skins have no `_b` alternate and correctly retain their normal texture;
  synthetic mesh tests cover alternate selection.
- `sequence-verified-runtime/eyes-vk` runs the legitimate SP modules on
  `escape1`. A 35-second cinematic capture shows closed eyelids; a second
  capture after reducing `cg_blinktime` shows open eyes on the same guard.
  This tests the retail module's flag submission as well as rendering.

The first full development suite completed 111 Meson targets with two skips;
its Python aggregate loaded the older blink fixture before the normal-decoder
change was finished and failed that fixture. Fresh focused runs pass. The
final aggregate rerun executes 1,514 tests with 62 skips and reports eight
subtest failures plus one error, all in release-packaging fixtures: their
staged ledger referenced a missing `2026-09-17-aspect-correction.md` during
concurrent documentation work. The eye, descriptor and normal regressions
pass in that run. A fresh release-packaging run passes all 48 tests with one
skip after the documentation update. `full-suite.log`, `full-suite-rerun.log`,
`aggregate-details.log` and `packaging-rerun.log` retain these outcomes
separately from the focused runs; this is not a claim of a single all-green
aggregate run.

Every runtime probe uses windowed rendering, an isolated writable home,
disabled input devices and an unswitched private desktop. The gallery uses
no input. The cinematic uses exactly one briefing Ready click under the
standing project authorization. Installed files remain unchanged, and writable
homes are removed. Private engine hooks supply
the gallery/capture schedule; game modules and assets are unmodified.

These are source, unit, build and bounded installed-runtime observations.
They do not close original-engine image parity for every character/skin,
native ray-tracing GPU parity, full campaign, or interoperability gates.

## Other observations

The first build attempt encountered a transient compiler object-file access
failure; the subsequent builds succeeded. Early private probes omitted the
new profile package and reported a missing console texture; the final probes
include the package. These investigation/setup issues are separate from the
eye-rendering corrections.

The installed cinematic still reports reused sprite images with mixed texture
flags. Those existing material-cache warnings are unrelated to the reproduced
head/eyelid failure and remain outside this correction.

Automatic approval review rejected cleanup of the disposable private probe
directories, reporting only "blocked by policy", including a narrower retry
with explicit, verified paths. The private builds and helpers therefore
remain confined to `.tmp/rtcw-eyes/`; no cleanup was attempted elsewhere.
