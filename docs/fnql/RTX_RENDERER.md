# RTX Renderer Contract

This inherited document describes the shared renderer. Current fnTech3 changes
and the comparison against Q2RTX are tracked in the
[RTX feature ledger](../fntech3/RTX_Q2RTX_PARITY.md).

## Status

FnQL carries the renderer introduced by
[FnQ3 commit `a78f78b`](https://github.com/themuffinator/FnQ3/commit/a78f78b7d8076311e96001ee47a2b064247b2c16)
as the ray-traced member of FnQL's **supported three-renderer set**, named
`rtx`. Default builds include `glx`, `vk`, and `rtx`; fresh configurations
select GLx while keeping RTX available for an explicit renderer switch.

The port has two useful operating states:

- `rtx_rt_mode 0` loads the RTX module but uses its complete raster path. This
  is the hardware-independent compatibility and module-lifecycle gate.
- `rtx_rt_mode 2` requests the path tracer, which traces through the Vulkan
  ray-tracing pipeline or, where the device lacks it, a compute backend that
  traverses the renderer's own bounding volume hierarchies (`rtx_rt_backend`
  forces either). With `rtx_rt_require 0`, a device that can trace neither way
  retains the complete raster frame. With `rtx_rt_require 1`, capability or
  primary-dispatch failure is fatal; a raster-only frame is never reported as
  successful required-RT output.

The renderer module is supported, while native ray-tracing mode remains an
explicit capability-gated path. Release-quality native-RT claims still
require reviewed runtime evidence on the relevant operating-system, GPU, Vulkan
loader, and driver matrix using a legitimate retail Quake Live Steam install.

## FnQL Compatibility Boundary

The original renderer was built in FnQ3's Quake III-oriented tree. The FnQL
port rebases it on the engine contracts used by retail Quake Live rather than
freezing the original renderer API and asset assumptions.

The required boundary is:

- renderer API version 13, with the export table zero-initialized before
  optional entries are assigned;
- Quake Live BSP version 47 alongside the retained legacy BSP readers, with
  every lump range validated before use;
- Quake Live advertisement cells, including loading-view refresh, cell display
  state, labels, and per-frame world submission;
- the host WebUI upload/draw export and runtime-sized RGBA shader registration;
- the retained FontStash host-text exports for drawing, measurement, metrics,
  and atlas diagnostics;
- `RDF_NOFIRSTPERSON` behavior for retail capture and view composition;
- the QL profile's own `r_fovCorrection` rule. Retail Quake Live game modules
  keep a 4:3 vertical FOV and widen the horizontal one only for their four
  `r_aspectRatio` shapes (4:3, 5:4, 16:10, 16:9). On any other shape the
  renderer widens that horizontal FOV by the view's shape over the preset's.
  On a preset's shape it changes nothing, so the modules' own correction is
  never applied twice. The 4:3 rule the other games take is never applied to
  QL.

The renderer remains an engine module. It does not include, reconstruct, or
replace retail `cgame`, UI, or `qagame` code.

## Frame Ownership

Native RT is deliberately restricted to an eligible primary view: a
center-eye, non-portal world view inside the screen and covering at least half
of it, traced into its own rectangle (Enemy Territory insets its main view by
a pixel). Portals, mirrors, stereo eyes, cube captures, smaller viewports,
HUD-only views, and other secondary work retain complete raster behavior.

An eligible primary frame is composed in this order:

1. The raster base pass draws the surfaces the path tracer owns, for their
   depth, and the sky and portal surfaces, whose raster colour is kept. With
   the overlay it is also a complete raster frame if tracing cannot run.
2. The path tracer rebuilds the frame's dynamic acceleration structures (or,
   on the compute backend, bounding volume hierarchies), traces one primary
   ray per pixel against the world, the entities' posed models and the
   first-person weapon, shades the hits it owns with their shader materials
   and the map's lights, denoises them with each moving entity's own motion
   and writes them over the raster colour.
3. The complementary raster overlay draws everything the tracer does not own:
   blended, additive and decal surfaces, deformed geometry, particles,
   beams, sprites, marks, flares, liquids and advertisements, plus the fog
   pass of each traced surface inside a fog volume. A failed trace leaves the
   base pass's complete raster draw, fog included, standing.
4. World-space post effects, optional global fog, bloom, tone mapping, and the
   final SDR transform run in their defined order. Later HUD-only scenes remain
   raster-owned.

The base and overlay classifiers are complements (`RB_DrawSurfIncluded`,
`vk_rt_surface_owner`): every surface is drawn exactly once. Ownership comes
from the shader's stages alone (`rt/rt_material.c`): the tracer owns a surface
whose first stage writes depth without blending, or whose stages alpha-test;
a blended or authored-special surface never becomes an opaque traced material
because its texture name resembles one. When fallback is permitted, a trace,
copy, or capability failure preserves the already complete raster result.
When strict RT is requested, the same failure stops the run.

## Conservative Defaults

RTX and presentation features added by the upstream work are opt-in in FnQL.
This protects retail appearance, driver coverage, and deterministic comparison
against the existing renderers.

| Cvar | Default | Contract |
| --- | ---: | --- |
| `rtx_rt_mode` | `0` | Load the module in raster fallback; native RT requires an explicit request and `vid_restart`. |
| `rtx_rt_require` | `0` | Permit honest fallback unless the operator explicitly asks to fail closed. |
| `rtx_rt_backend` | `0` | Trace with the ray-tracing pipeline where the device offers it, else with compute; forcing either is explicit. |
| `rtx_rt_material_maps` | `1` | Traced materials take a texture pack's companion normal and material maps when one supplies them; retail paks carry none, so retail appearance is unchanged. |
| `rtx_rt_async_overlap` | `0` | Keep experimental queue overlap disabled. |
| `r_globalFog` | `0` | Do not load or apply optional map fog sidecars by default. |
| `r_fogMode` | `0` | Preserve the legacy BSP fog lookup unless analytic fog is selected explicitly. |
| `r_liquid` | `0` | Preserve authored liquid stages without the enhanced screen-space layer by default. |
| `r_underwater` | `0` | Keep the classic dry presentation while submerged unless the submerged-view compositor is requested. |
| `r_hdr` | `1` | Use high-precision scene storage that stays display-referred, as in GLx and Vulkan. Scene-linear colour (sRGB texture decode and linear blending) needs `r_srgbTextures 1` and a non-legacy tone mapper, colour grading or an active HDR presentation. |
| `r_surfaceLightProxies` | `0` | Do not infer raster analytic emitters from compiled surface-light metadata by default. The path tracer lights surface lights from the map itself and does not use these proxies. |

`rtx_rt_mode 1` is a capability-development mode that requests ray-query
support while retaining raster output. It is not a hidden substitute for mode
2 and is not part of the ordinary compatibility smoke test.

## Capability and Safety Rules

Native activation is a negotiated device contract, not an extension-name
guess. The renderer must verify every required feature, entry point, limit,
format, descriptor layout, acceleration-structure property, shader-binding
table alignment, and output transfer path before it publishes an active mode.
Core Vulkan feature promotion and extension aliases must both be handled where
the supported loader permits them.

The renderer also treats retail content as untrusted input:

- BSP and advertisement lump offsets and sizes are validated before pointer
  arithmetic;
- geometry capacities use logical buffer sizes, not padded Vulkan allocation
  sizes, and byte/count arithmetic is checked before allocation or upload;
- indexed geometry is accepted or rejected as complete triangles so a bad
  index cannot reconnect unrelated vertices;
- optional fog and light sidecars have explicit byte, item, numeric, and
  nesting limits; non-finite values and trailing junk are rejected;
- WebUI dimensions are bounded before calculating RGBA upload bytes;
- material ownership remains conservative. Broad texture-name guesses and
  unrelated surface flags are not compatibility evidence;
- traced materials evaluate the shader's own stages (blend functions, colour,
  alpha and texture-coordinate generators and modifiers, animated maps and
  alpha tests) with bounded stage, modifier, material and texture indices;
  blended, deformed and screen-mapped shaders retain raster ownership;
- map lights, their grid and the sun are validated as finite and in range
  before they reach a GPU buffer; a malformed entity or surface light is
  dropped rather than clamped into the scene;
- the compute backend's hierarchies are bounded (at most 2^22 triangles,
  splits by surface-area heuristic to depth 40 and middle splits past it,
  leaves of four triangles or, where splitting stops paying, up to 64), so
  its shaders' 64-entry traversal stack cannot overflow;
- companion material maps are looked up only under names no retail pak uses
  for a pair of textures, through the shader image policy, and a map the
  shader draws as a stage of its own is never taken as a companion;
- trace/copy failures leave render-pass state recoverable for fallback, or
  terminate a strict-RT run explicitly.

Debug and experimental paths must not weaken those checks. A diagnostic cvar
may expose more information, but it may not convert malformed content into a
trusted allocation or silently relax a strict capability request.

## Building

Meson is the canonical build graph. The default modular build includes RTX:

```text
meson setup meson/build-rtx -Drenderers=glx,vk,rtx
meson compile -C meson/build-rtx fnql_rtx_x86_64
```

The exact architecture suffix follows the configured target. A static renderer
build is useful as a separate linkage gate:

```text
meson setup meson/build-rtx-static -Drenderer-dlopen=false -Drenderer-default=rtx
meson compile -C meson/build-rtx-static
```

On Windows, retail module compatibility still requires the Win32/x86 FnQL
client. An x64 renderer build is valuable for engine and driver validation but
does not make x86 retail modules loadable in a 64-bit process.

Shader source and its generated C payload are one reviewed unit. After changing
an RTX shader, regenerate through the repository's shader tooling and run the
freshness/source tests; do not hand-edit a generated byte array.

## Runtime Smoke Probe

`scripts/rtx_runtime_smoke.py` launches a bounded windowed session against a
legitimate Quake Live Steam installation. The caller supplies the FnQL client
and a retail map name; the script intentionally does not guess a map or stage
sample assets.

The probe:

- verifies Steam app ID `282440` and retail Quake Live install markers;
- passes the retail root through `fs_basepath` and `fs_steampath` without
  writing beneath it;
- creates a unique per-profile `fs_homepath` below `.tmp`;
- forces `r_fullscreen 0` and a bounded custom window;
- disables downloads, sound, optional presentation effects and async overlap
  to isolate renderer activation;
- never connects to a server and never synthesizes a Steam ticket or successful
  authorization decision;
- requests renderer diagnostics, captures a PNG screenshot, and collects all
  discovered console logs;
- validates the complete bounded PNG structure, chunk CRCs, zlib stream,
  scanline filters, requested dimensions, luminance range/variance, and black
  or white clipping before accepting the capture as runtime evidence;
- rejects fatal/Vulkan/validation diagnostics and writes a JSON manifest with
  executable and screenshot sizes/SHA-256 identities plus platform metadata.

Inspect a launch without creating files or starting the engine:

```text
python scripts/rtx_runtime_smoke.py \
  --exe <path-to-fnql-client> \
  --retail-root <path-to-steam-quake-live> \
  --map <retail-map-name> \
  --profile all \
  --plan
```

Run the hardware-independent module/fallback gate first:

```text
python scripts/rtx_runtime_smoke.py \
  --exe <path-to-fnql-client> \
  --retail-root <path-to-steam-quake-live> \
  --map <retail-map-name> \
  --profile raster
```

Then run strict native RT only on an intended RT-capable test machine:

```text
python scripts/rtx_runtime_smoke.py \
  --exe <path-to-fnql-client> \
  --retail-root <path-to-steam-quake-live> \
  --map <retail-map-name> \
  --profile native
```

`--profile native` sets `rtx_rt_mode 2` and `rtx_rt_require 1`. Unsupported
hardware, a capability downgrade, a failed primary dispatch, missing mode
evidence, validation diagnostics, a missing, malformed, mis-sized, uniform, or
heavily clipped capture, timeout, or nonzero process exit all fail the profile.
This strict result must not be replaced by a raster success when reporting
native-RT coverage.

Set `FNQL_RETAIL_QL_PATH` instead of passing `--retail-root` when useful. The
default artifact root is `.tmp/rtx-runtime-smoke`; each invocation adds a UTC
run ID and never deletes or reuses an earlier run.

## Validation Matrix

Before treating an RTX slice as complete, keep these gates distinct:

1. Source-contract tests: API 13 exports, BSP 47/advertisements, conservative
   defaults, hybrid ownership, absence of duplicate FOV correction, and the
   isolated smoke-command contract.
2. Shader tests: every source variant compiles, generated payloads are fresh,
   descriptor bindings match host declarations, and all index/alpha paths are
   bounded.
3. Modular and static builds with strict warnings on each maintained compiler.
4. Raster smoke on non-RT and RT systems. This proves module loading and the
   complete fallback without claiming native coverage.
5. Strict native smoke on reviewed RT-capable hardware and drivers, retaining
   the manifest, console logs, screenshot, exact binary revision, loader, GPU,
   and driver versions.
6. Retail scene review covering BSP 47 advertisements, WebUI/host text, map
   transitions, portals, first-person suppression, entities, translucent and
   alpha-tested content, fog, liquids, resize, `vid_restart`, screenshots, and
   clean shutdown.
7. Regression runs for the GLx and VK selections. RTX must not change their
   output names, source lists, cvar defaults, or runtime behavior.

A plan-only manifest, a successful compile, or a raster-profile capture is not
native-RT runtime evidence.

## Known Limits

- Six-face `screenshot ... cubemap` export is not yet wired through the RTX
  readback path. The RTX renderer rejects that subcommand without writing files
  instead of emitting a misleading single-face image; ordinary screenshots,
  clipboard BMP capture, AVI frames, and levelshots remain available.
- Entity models are traced when their format is one the tracer poses (MD3,
  MDR, IQM, MDC, MDS, ET meshes and brush models). Other entity types and
  formats (sprites, beams, rails, IHV beams, Alice's TIKI models) keep raster
  ownership and are neither shadowed nor reflected.
- Portals, mirrors, stereo/cube views, viewports under half the screen, and
  later secondary scenes are deliberately raster-owned until they have
  independent history and composition contracts.
- Raster-overlay pixels are composited after native tracing and are not
  retroactively relit by the RT pass.
- The path tracer lights a map from its compiled light entities, surface
  lights, sun and ambient with the compilers' light models, and adds bounce
  light the compilers never baked. Traced scenes are therefore brighter in
  enclosed spaces than their lightmaps; `rtx_rt_lighting 1` keeps the
  lightmaps. `alphashadow` and q3map2's `lightfilter` surfaces filter the
  map's light as their compiler did. The deviance of q3map2's
  `q3map_sunExt`, and its `_deviance`, `_samples`, `_filterradius` and `_sun`
  light keys, are not reproduced.
- Participating media are not traced: fog volumes and global fog remain raster
  passes over the traced colour.
- Optional global fog, enhanced liquid presentation, and scene-linear HDR are
  independent opt-ins. Enabling native RT does not silently enable them.
- A complete maintained GPU/driver promotion corpus has not been established.
  Keep native RT mode explicit and capability-gated until that evidence exists.
