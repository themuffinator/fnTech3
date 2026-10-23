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

- renderer API version 35, with the export table zero-initialized before
  optional entries are assigned; engine frame-event callbacks are optional
  and do not change any retail module ABI;
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

1. The raster base pass writes only depth for trace-owned surfaces. Opaque
   coverage uses a position-only pipeline; alpha-tested coverage retains its
   authored stage evaluation, including dependencies on earlier depth writes.
   Sky and portal surfaces keep their raster colour. Shadow maps are scheduled
   for actual raster receivers; those maps still include all needed casters.
2. The path tracer updates the frame's dynamic acceleration structures (or,
   on the compute backend, bounding volume hierarchies), traces one primary
   ray per pixel against the world, the entities' posed models and the
   first-person weapon, shades the hits it owns with their shader materials
   and the map's lights, denoises them with each moving entity's own motion
   and writes them over the raster colour.
3. The complementary raster overlay draws everything the tracer does not own:
   blended, additive and decal surfaces, deformed geometry, particles,
   beams, sprites, marks, flares, unsupported liquids and advertisements, plus
   the fog pass of each traced surface inside a fog volume. If tracing fails
   permissively, the renderer clears the partial base and draws the complete
   raster view with its required shadow maps, colour, lighting and fog instead.
4. World-space post effects, optional global fog, bloom, tone mapping, and the
   final SDR transform run in their defined order. Later HUD-only scenes remain
   raster-owned.

The base and overlay classifiers are complements (`RB_DrawSurfIncluded`,
`vk_rt_surface_owner`): every surface is drawn exactly once. Ownership comes
from the shader's stages alone (`rt/rt_material.c`): the tracer owns a surface
whose first stage writes depth without blending, or whose stages alpha-test;
a blended or authored-special surface never becomes an opaque traced material
because its texture name resembles one. When fallback is permitted, a trace,
copy, or capability failure selects a complete raster redraw.
When strict RT is requested, the same failure stops the run.

## Conservative Defaults

RTX and presentation features added by the upstream work are opt-in in FnQL.
This protects retail appearance, driver coverage, and deterministic comparison
against the existing renderers.

| Cvar | Default | Contract |
| --- | ---: | --- |
| `rtx_rt_mode` | `2` | Path trace the main view; `0` keeps the module's raster path. Latched: `vid_restart`. |
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

1. Source-contract tests: API 35 exports, BSP 47/advertisements, conservative
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

## Path Tracer Reference

This section holds the technical detail behind the player-facing
[RTX Renderer Guide](../RTX.md): what each tracing backend needs, what the
path tracer owns and how it lights a map, and its developer diagnostics.

### Tracing Backend Requirements

The path tracer has two tracing backends that draw the same image:

- **Ray-tracing pipeline** (hardware): a Vulkan 1.2 GPU and driver with
  `VK_KHR_acceleration_structure`, `VK_KHR_ray_tracing_pipeline`,
  `VK_KHR_ray_query`, `VK_KHR_deferred_host_operations`, buffer device address
  and descriptor indexing. The renderer also checks its descriptor limits
  (2,048 sampled textures in a ray-tracing stage) and shader-binding-table
  alignments.
- **Compute** (software): any Vulkan GPU with descriptor indexing,
  `VK_KHR_spirv_1_4` and `VK_KHR_shader_float_controls`. It traverses bounding
  volume hierarchies the renderer builds itself, and is slower.

Both need the renderer's storage-image and sampled-texture limits and its
output image formats. `rtx_rt_backend 0` picks the ray-tracing pipeline when
the driver offers it and compute otherwise; the log says which one traces.

Driver capabilities can differ between x86 and x64. On the development
machine's NVIDIA driver, the 32-bit (x86) Vulkan driver offers no ray-tracing
pipeline, so the shared Win32 client traces with compute; the 64-bit
Q3-only diagnostic build ([BUILD.md](../../BUILD.md)) traces with either. The
[Q2RTX feature comparison](../fntech3/RTX_Q2RTX_PARITY.md) records the verified
scope and remaining gaps, and the [compatibility ledger](../fntech3/COMPATIBILITY.md)
the gate status.

RTCW and Enemy Territory remain behind their development build options. The
path tracer has drawn their retail maps in isolated development probes
(`docs/fntech3/2026-09-24-rtx-path-tracer.md`); those probes do not make
either game's renderer gate complete.

The `vkinfo` command reports the selected device and capability result. Use
`rtx_caps_report 1` for a compact RTX capability summary or `rtx_caps_report 2`
for the detailed report.

### Path Tracer Scope And Light Model

Each main view is drawn in three steps. The raster renderer first draws the
surfaces the tracer owns, for their depth. The tracer then shoots a ray per
pixel, shades what it hits and writes those pixels over the raster frame.
Finally the raster renderer draws everything the tracer does not own on top,
in Quake III's usual order.

The tracer owns:

- the world's opaque and alpha-tested surfaces, including curved patches;
- supported liquid surfaces, including small wave-deformed water, slime and lava;
- models (MD3, MDR, IQM, and RTCW's and ET's MDC, MDS and skeletal meshes) and
  brush models such as doors and platforms;
- the first-person weapon. Your own third-person body is traced but never
  seen directly: it casts shadows and shows in reflections.

The raster pass keeps everything else: blended, additive and decal surfaces,
particles, beams, rails, sprites, flares, marks, unsupported liquids and deformed geometry,
the sky box and portal surfaces, the HUD and menus. Portal and mirror views,
stereo eyes, cube captures, views covering less than half the screen and the
portal sky that RTCW and Enemy Territory draw behind a map's main view are
rasterized completely. A main view smaller than the screen (Enemy Territory
insets its view by a pixel) is traced into its own rectangle.

**Liquids.** `rtx_rt_liquids 1` (default, latched) gives contents-marked
liquids to the tracer. Clear, blended water uses dielectric Fresnel, Snell
refraction, total internal reflection and distance-dependent absorption;
opaque first stages remain opaque. FnQL's shared ambient waves and bounded
interaction rings perturb optical normals in world space. Small authored
vertex waves are replaced by that field on the BSP boundary, not frozen into
static geometry. Camera-visible rays carry texture differentials through
interfaces; diffuse paths cross clear water with absorption but without
specular bending, so caustics are not claimed. Both backends share the path,
and secondary interfaces have a bounded iteration count separate from the
diffuse bounce budget. Material identity, waves, malformed/fallback cases
and q3dm12 captures are recorded in the
[liquid evidence](../fntech3/2026-10-11-rtx-liquids.md).

**Materials.** A traced surface uses its shader's stages as authored: the
blend functions, `rgbGen` and `alphaGen` (including waveforms, entity colours,
`lightingDiffuse` and `lightingSpecular`), `tcGen`, `tcMod` animation,
`animMap` and alpha tests are evaluated for every hit. The tracer splits each
material into the part its lighting term scales and the part it does not, so
glowing stages stay glowing. Textures are filtered with the same anisotropic
samplers as raster. An additive environment-mapped "shine" stage is replaced
by traced reflections.

**Material maps.** No retail game ships normal, roughness or metalness maps,
but a texture pack can add them beside a lit material's texture, under the
names OpenJK's rend2 uses:

| Image | Channels |
| --- | --- |
| `<texture>_nh`, else `<texture>_n` | normal map: red leans the normal toward growing s, green toward growing t (rend2's convention); the `_nh` alpha (height) is unused |
| `<texture>_specGloss` | specular colour, gloss in alpha (roughness is 1 - gloss) |
| `<texture>_rmo`, else `<texture>_orm` | roughness, metalness, occlusion; or occlusion, roughness, metalness |

`<texture>` is the image of the material's first stage that is not a lightmap
or environment map, without its extension. Its `tcMod` and `tcGen` apply to
the maps too. Roughness is perceptual: the GGX width is its square. A metal
reflects in its texture's colour and loses its diffuse light; occlusion is
not used, since the tracer finds occlusion itself. No retail Quake III,
Quake Live, RTCW or Enemy Territory pak pairs a texture with an image under
these names. Only the path tracer uses the maps; set
`rtx_rt_material_maps 0` and `vid_restart` to ignore them.

**Lighting.** By default the tracer lights every map with the lights the map
itself defines, in place of its lightmaps and with no setting of its own: the
light entities it keeps, the surfaces that give off light, and its sky and
sun, with traced shadows and bounce light. Nothing baked takes part: no
lightmap, vertex light or light grid is read, not even to set an exposure.
Quake III's and RTCW's maps keep the light entities their designers placed,
the lights their compilers baked into the lightmaps, so their rooms are lit
where the designer lit them; q3map2 left none in Quake Live's and Enemy
Territory's maps, which their glowing fixtures, sky and sun light instead.
The compilers' ambient and `_minlight` floors and q3map's backsplash stood in
for bounced light, which is traced instead. The light is shown as it was
traced, in the units the map compiler gave its lights: the light a surface
receives multiplies its colour as the engine multiplies a surface by its
light, with the engine's overbright and no exposure, so a map is as bright
or as dark as its own light sources make it.

- **Surfaces that give off light.** A shader with `q3map_surfacelight` is a
  light of the strength its author gave it, in the colour of its light image,
  as the map compilers read it. Every other shader gives off what it draws
  where its lighting term is black: a glow stage added over a lightmapped
  base, an additive flame, a lit window. A shader drawn fullbright (no
  lightmap, no vertex light) is lit by the scene like any other surface, and
  only its bright texels glow, as Q2RTX synthesizes emissive masks from a
  texture's bright pixels; `rtx_rt_emissive_light` sets how much light those
  shaders give off, as a multiple of what they show. Each emitting triangle
  of the world is sampled as an area light, so a lamp lights a room with
  soft, correctly shaped shadows.
- **The sky.** The sky lights the scene as its compiler lit it: id's q3map
  from the sky's own triangles, each a light of the sky shader's
  `q3map_surfacelight`, and q3map2 also from a dome of suns of its
  `q3map_skyLight`. The sky shader's `q3map_sun` is the sun, softened over
  q3map2's `q3map_sunExt` deviance. A sky that asks for no light gives none.
- **The camera.** `rtx_rt_camera 1` sees the light through a camera instead,
  as Q2RTX sees its own. Light is then traced in linear units: the colours
  the shaders and the sky show are decoded from the display's gamma (2.2), so
  a surface reflects and gives off light in proportion to what it shows, and
  the view is encoded for the display once, after the camera. In physical
  proportion a room lit through its window is far darker than the sunlit snow
  outside it, so each view is tone mapped as Q2RTX maps its own: a histogram of the view's light gives
  a curve, after Eilertsen, Mantiuk and Unger's noise-aware tone mapping, that
  keeps the contrast of each range of brightness the view holds and
  compresses the ranges it does not, within `rtx_rt_tonemap_range` stops; it
  is blended (`rtx_rt_tonemap_curve`) with Reinhard's curve behind an
  exposure metered on the view's 70th to 90th brightness percentiles. The
  curve is fitted to the light a surface receives, not to the surface, so a
  texture keeps its detail however hard the light is compressed. Exposure and
  curve adapt over time as an eye does (`rtx_rt_exposure_speed`), over a
  limited range: a view darker than a dim room stays darker, as its designer
  left it. `rtx_rt_exposure_bias` brightens or darkens it; `rtx_rt_exposure`
  fixes the exposure instead, with an ACES filmic curve.

A light entity keeps its compiler's light model (point, linear and spot,
with id's q3map, q3map2's or the Wolfenstein compilers' falloff, angle and
spawnflag rules). `rtx_rt_lighting 3` leaves the light entities out and
lights a map from its sky and emitters alone, as Q2RTX lights Quake II, with
an even sky in its light colour and the linear camera: a room lit by light
entities with no visible fixture is then lit only by what reaches it from
outside. Two modes remain only to compare against the baked result:
`rtx_rt_lighting 1` lights the world from its lightmaps and traces only
dynamic lights over them, with models lit from the light grid as the raster
pass lights them; `rtx_rt_lighting 2` traces the map compilers' own lights as
they baked them, with surface lights' backsplash and the worldspawn ambient.
A change of mode or camera rebuilds the lights at once.

Direct light is shadowed. World surfaces cast solid shadows, alpha-tested
holes included (a grate or a lamp's cage); `surfaceparm trans` surfaces cast
none from the map's own lights unless they filter it. Models, doors and other
movers shadow every light through their own alpha-tested holes. Filtering
surfaces pass light as the compilers read their light image
(`q3map_lightImage`, else `qer_editorimage`, else the image named like the
shader): `alphashadow` surfaces where it is transparent, and q3map2's
`lightfilter` surfaces tinted with its colour. Curved patches cast shadows, as
they did for id's maps, which were compiled with `-patchshadows` (set
`rtx_rt_patch_shadows 0` for a map compiled without it). Dynamic lights
(rockets, muzzle flashes, powerups, RTCW's torches) are shadowed by
everything, through alpha-tested holes. They fall off as the raster light
pass draws them, eased toward a smoothstep by `r_dlightFalloff`, and reach
only the surfaces that pass lights: a shader with `surfaceparm nodlight`, or no
stage the pass can light, takes none. RTCW's two light materials are not
lights: `negdlightshader` (its zombie spirits) darkens the world around it and
`flamedlightshader` brightens it, each multiplying what a world surface or
brush model shows by the `$dlight` blob projected down the world's vertical
axis, as the original renderer drew them. Indirect light (one diffuse bounce
by default, up to four with `rtx_rt_bounces`) and glossy reflections are
traced on top.

**Reflections.** A glossy surface (an environment-mapped shader, or a
material map's low roughness) traces a reflection ray. Where that ray meets a
surface light, it and the light sample share the light by multiple importance
sampling (Veach's balance heuristic), so a mirror shows a lamp's shape and a
rough surface a soft highlight, without the noise of either alone. A sky's
light is left to the reflection, which sees the sky itself. Set `rtx_rt_mis 0`
to leave surface lights to the light sample alone.

**Fog.** Fog volumes are drawn by the raster fog pass over the traced colour,
exactly as over raster surfaces. RTCW's and Enemy Territory's global fog, and
Alice's far-plane fog, are part of each raster surface's own draw instead; the
tracer applies the same fog to each pixel it owns, by the same equation at the
traced surface's distance from the eye, after its light is resolved. A
`nofog` material stays clear, as in raster.

**Noise and moving shadows.** Direct, indirect and specular illumination
have separate histories. Moving dynamic geometry, including an offscreen
first-person BODY caster, holds a reactive state for 150 ms. Direct light
then uses a 35 ms response, independently of the slower indirect response.
History weights use elapsed time, so lower FPS does not stretch a fixed
sample count into a long shadow trail. Current compatible neighbours bound
reactive history; quiet stochastic lighting retains its history rather than
being clipped to black between rare samples. Moving lamps and flashes also
mark receivers reactive, and moving surfaces keep the fast response. Old dark
history never caps newly revealed bright light. Indirect variance includes
both the post-blend residual and the incoming sample's difference from old
history, so a sparse sample cannot erase its own uncertainty at low FPS.
Stationary view and game
clock can still accumulate, but changing geometry, lights or relevant
settings invalidates that frozen estimate.

Spatial depth stops measure departure from a local reciprocal-depth plane.
The smaller valid one-sided slope avoids using a foreground/background edge
as the receiver's slope. Grazing floors can then share light samples without
relaxing the normal, material or depth-edge checks. The guide is also used for
the current temporal neighbourhood and sparse indirect reconstruction.

Reactive direct light normally uses the first two spatial iterations. The
third existing iteration also filters it when blurred variance exceeds 5%
of squared luminance. This removes isolated samples around model shadows
without giving settled shadow edges or the widest iterations more support.
The decision uses relative variance, so changing exposure does not change
the policy. Temporal response, geometry stops, ray counts and dispatch counts
are unchanged. The [forest speckle follow-up](../fntech3/2026-10-23-rtx-speckles.md)
records quality, timing and moving-caster checks.

Motion matching uses stable internal entity IDs where the profile supplies
them, plus model/part topology and material signatures. Ambiguous duplicate
parts without IDs, changed topology/materials, teleportation and missing
matches reject history. External module records remain unchanged. A reused
ID without a supplied spawn generation cannot universally identify a respawn.

`rtx_rt_indirect_divisor` controls bounce sampling independently: `0` chooses
2 on compute and 1 on hardware, and explicit values are 1 through 4. One
rotating subpixel phase per divisor-sized block is traced; missing samples
are marked invalid, never counted as black observations. Depth and normals
guide history and a bounded reconstruction at disocclusions. History resets
seed all pixels. `rtx_rt_checkerboard 0` additionally checkerboards 8 x 8
tiles only on compute at divisor 1; `1` forces it, `2` disables it. Its phase
changes after a complete divisor cycle, so combinations leave no permanently
unsampled pixels. Primary visibility, direct shadows and current entity poses
remain full rate. `rtx_rt_frame_budget_ms` optionally adjusts the divisor from
measured GPU pass time with hysteresis and a 45-frame cooldown. Its default,
0, disables adaptation; the direct/primary cost can exceed any requested target.

The 25 intermediate images total 192 bytes per pixel (180 MiB at 720p).
Depth/oct-normal and motion are packed into RG32UI images; moment mean and
standard deviation use RGBA16F. Independent bounce density saves ray work,
not these full-resolution allocations. Spatial filtering computes the shared
variance neighbourhood once for all illumination signals.
An 8 x 8 workgroup now caches the three variance images with a one-pixel
halo in shared memory. This reduces an interior group's variance reads from
1,728 to 300 per spatial pass. Partial edge groups participate in the barrier
before returning; Gaussian weights and image-edge normalization are unchanged.

**Light selection.** Each grid cell retains at most 64 important static
lights, sorted by ID for binary-search membership. Overflow counts and omitted
local importance are reported. A global CDF mixes power weighting with a 10%
uniform floor. The local/global proposal mixture retains nonzero support for
every static light and includes dynamic lights, with its full PDF in RIS and
MIS replacement. Cell overflow no longer removes lights from the estimator.
Candidates with zero target skip proposal-density evaluation; their random
draws, zero weights and the distribution of contributing samples are unchanged.
`rtx_rt_light_reuse 1` replays four candidate random streams across nearby
compatible primary receivers and up to four frames, replacing one stream each
frame. The remaining candidates, current target/PDF, reservoir selection and
visibility are recomputed. This is common-random-number proposal reuse, not
ReSTIR or cached visibility. Receiver position, normal, material and proposal
table keys bound sharing; secondary paths stay independent. Reuse remains
opt-in (default 0): energy tests pass, but the bounded runtime comparison did
not establish a noise improvement and showed higher temporal variation in
the compute capture region.

**Geometry and uploads.** Exact model-space pose keys cache MD3, MDR, IQM and
brush geometry in a 16-entry LRU with a 1 MiB per-entry retention limit. Current
transforms, entity lighting and motion are applied on every append. Frame,
old frame and interpolation are never quantized. Procedural MDS/ET poses and
IHV camera-dependent interpolation bypass this cache. IQM influence scratch
storage is reused. Compute BVHs refit exact connectivity, rebuilding after
120 refits or 1.5x traversal-cost growth after at least eight refits. Hardware
BLASes use UPDATE for stable topology and rebuild after 60 updates. Model/map
invalidation clears retained geometry. `RF_NOSHADOW` suppresses shadow rays
without removing primary or reflected geometry; BODY remains hidden only
from primary rays. Frequent GPU-read buffers prefer coherent host-visible
device-local memory, with portable allocation fallback.

Compute BVH builds evaluate the binned surface-area cost on all three axes,
avoiding overlapping children when the widest centroid axis is a poor split.
This spends more time building the tree to reduce traversal work in subsequent
frames. The [RTX quality record](../fntech3/2026-10-22-rtx-quality.md) records the
bounded `forest` timing and image comparison with ray tracing required.
Compute traversal also keeps the previously tested entry distance beside each
deferred node index. A nearer triangle can discard that node without fetching
and testing its bounds again. Masked materials, first-blocker shadows and
nearest-hit queries retain their existing rules. The
[optimization follow-up](../fntech3/2026-10-23-rtx-optimization.md) records the
paired timing, image and partial-workgroup checks.

**Frame pacing.** `rtx_rt_low_latency 1` waits for at most one GPU frame in
flight before OS input polling and acquires presentation there too. `2` allows
two GPU frames, `0` restores late waits. The engine keeps simulation, network
and prediction ordering; no per-frame queue-idle wait is added. Other renderer
modules leave the optional frame-event callback unset. Engine and renderer
binaries must have matching internal API versions.

The [entity-shadow and performance record](../fntech3/2026-10-11-rtx-entity-shadows.md)
separates executed tests and Q3 runtime observations from remaining profile,
driver and end-to-end latency coverage.

### Developer Diagnostics

`rtx_rt_debug_visualizer` shows the tracer's intermediate
images: `1` reflectance, `2` shading normal, `3` denoised light, `4` raw
light, `5` specular, `6` emission, `7` temporal history, `8` view depth and
`9` motion; `10` shows the light alone, on plain grey surfaces seen through
the same camera, which makes shadows, light pools and bounce light easy to
see. `11` and `12` show direct and indirect illumination through a fixed
Reinhard compression; `13` shows temporal reactivity.
`rtx_rt_perf_timing 1` prints GPU and CPU timings every
`rtx_rt_perf_interval` frames, including rolling 256-sample p50/p95/p99,
active backend and indirect divisor, image storage, geometry cache/refit
counters, upload memory heaps and input-to-submit/present-call intervals.
`2` also prints every frame's monotonic input, simulation, fence/acquire,
submit and present CPU events. These are API timestamps, not display latency.
`rtx_rt_device_local 0` with `vid_restart` selects portable host-visible
allocation for a heap comparison; default 1 prefers host-visible VRAM.
The cheat `rtx_rt_test_caster "x y z halfWidth [noShadow]"` submits a 56-unit
BODY box for scripted engine captures; an empty string disables it.
`rtx_rt_test_fail_trace 1` exercises full raster recovery after the depth base,
or a strict-RT error. Keep both at their defaults outside testing.
With a map loaded, `rtx_rt_pick` names the
traced surface at the centre of the view (or along `rtx_rt_pick x y z dx dy
dz`) with its material flags, the directions its texture's s and t grow in
(where a normal map leans), whether the map's lights pass it and the material
maps it took, and `rtx_rt_lightinfo [x y z]` lists the lights the tracer
samples at a point, numbered. `rtx_rt_debug_light <number>` then lights what
the view sees with that light alone (best with `rtx_rt_debug_visualizer 4`),
and `rtx_rt_debug_light_mode` shows it unshadowed (`1`), where its shadow rays
get through (`2`: white; where they do not, the blocker's kind in colour:
red the world, blue a model or mover, magenta the viewer's body), or what the
compilers' light filters pass of it (`3`); `-1` returns to all lights. The
developer log (`developer 1`) names every material that takes a material map.
`rtx_debug_vk_validation 1` enables the Vulkan validation layers at the next
`vid_restart`. Keep these off in normal play.

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
- Global fog and the submerged-view compositor remain separate controls.
  Supported liquid interfaces are traced by default in native RT; screen-space
  liquid controls remain available in raster mode and explicit liquid fallback.
- A complete maintained GPU/driver promotion corpus has not been established.
  Keep native RT mode explicit and capability-gated until that evidence exists.
