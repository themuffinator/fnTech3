# RTX renderer: Q2RTX comparison and remaining work

Updated 2026-09-25. **fnTech3 has not reached Q2RTX renderer parity.** This is
a feature and implementation inventory for Q3/QL, not a full-compatibility or
visual-quality sign-off. The [compatibility ledger](COMPATIBILITY.md) owns
runtime gate status. A successful RTX raster test does not exercise ray tracing.

## Reference and scope

The comparison uses NVIDIA's official Q2RTX tree at
[`f2526e9a165949f66e91e82f0d63aa7bb2567b4d`](https://github.com/NVIDIA/Q2RTX/tree/f2526e9a165949f66e91e82f0d63aa7bb2567b4d),
resolved from `refs/heads/master` on 2026-09-06. Its
[README](https://github.com/NVIDIA/Q2RTX/blob/f2526e9a165949f66e91e82f0d63aa7bb2567b4d/readme.md)
describes the renderer feature set and marks the project discontinued. Its
[changelog through 1.8.1](https://github.com/NVIDIA/Q2RTX/blob/f2526e9a165949f66e91e82f0d63aa7bb2567b4d/changelog.md)
provides additional implementation history. These are reference observations;
this work does not incorporate Q2RTX implementation text or distribute its media.

The relevant target is lighting, materials, geometry, image reconstruction, and
reliable operation for Q3 assets. Q2-specific weapons, monsters, mission packs,
and gameplay are not features for this engine-only distribution. Legitimate
Q3/QL modules and assets remain user supplied. QL's x86 client-module boundary
also remains a separate requirement from Q2RTX's client architecture.

A separate Windows x86_64 diagnostic configuration, `-Dq3-only=true`, builds
`fntech3-q3.exe` with `fnql_rtx_x86_64.dll` and rejects QL selection before
filesystem startup. It permits Q3 RT investigation on drivers whose required
features differ between Win32 and x86_64. It is not the shared Q3/QL release
contract, and Q3-only evidence must retain that executable distinction.

In the matrix, **implemented** means a production path exists for the stated
scope, **partial** means important restrictions remain, and **open** means the
comparable capability is absent. None of those labels alone establishes Live
evidence or equivalent performance and image quality.

## Architecture

Native mode (`rtx_rt_mode 2`) is a path tracer for the primary view
([backend](../../code/rendererrtx/rt/rt_backend.c)). The static world is one
vertex/index buffer pair partitioned into what the map's lights are blocked
by, the surfaces they pass through (`surfaceparm trans`, and patches without
`-patchshadows`), the surfaces that filter them (`alphashadow`, q3map2's
`lightfilter`), and sky and portal surfaces. Each frame the entities' posed
models, the first-person weapon and the viewer's own body are extracted and
each moving entity matched to its previous frame. Two backends trace the same
scene: the Vulkan ray-tracing pipeline against bottom- and top-level
acceleration structures rebuilt in the frame's command buffer, or a compute
shader against bounding volume hierarchies the renderer builds on the CPU
([BVH](../../code/rendererrtx/rt/rt_bvh.c)), for drivers without the pipeline
(the Win32 x86 driver on the test machine). Both run the same per-pixel code
([path](../../code/rendererrtx/shaders/rt_path.glsl)): primary rays, each
owned hit shaded with the shader's stages
([materials](../../code/rendererrtx/rt/rt_material.c)) and the map's lights
([lights](../../code/rendererrtx/rt/rt_lights.c)), diffuse bounces and a
glossy reflection, and separate diffuse, specular, reflectance, emission and
motion images. A temporal pass and edge-aware a-trous iterations filter the
light, and a composite writes owned pixels over the raster frame before the
raster overlay draws everything the tracer does not own.

## Feature matrix

| Capability | fnTech3 status | Production scope and remaining work |
| --- | --- | --- |
| Scene tracing | Implemented | Vulkan KHR ray-tracing pipeline (world, per-frame dynamic and TLAS builds per command buffer; recursion depth 1, the ray-generation shader shading every hit), or a compute backend traversing CPU-built binned-SAH BVHs of the same instances (`rtx_rt_backend`); capability, descriptor, SBT and failure gates for each. The two backends' frozen frames agree to a mean absolute difference of 0.0043 or less (0-255). |
| Q3 shader materials | Implemented, bounded | Each traced material evaluates its shader's stages as authored, up to eight: blend functions, `rgbGen`/`alphaGen` (waveforms with the renderer's own tables, entity colours, `lightingDiffuse`, `lightingSpecular`), `tcGen` (texture, lightmap, vector, environment), `tcMod` chains, `animMap` and alpha tests. Two evaluations with the lighting term at one and at zero split the drawn colour into reflectance and emission, so unlit stages stay emissive. Primary hits sample with ray-differential gradients, so the anisotropic samplers filter as in raster. Blended, deformed, polygon-offset and screen-mapped shaders stay raster-owned. |
| Direct lighting from the map | Implemented, bounded | By default (`rtx_rt_lighting 0`) as Q2RTX lights Quake II: from the sky and the surfaces that give off light, nothing baked and no light entity. `q3map_surfacelight` polygons are area lights of their authored strength in their light image's colour (`q3map_lightRGB`, `q3map_lightImage`, editor image or shader image; autosprite points). Every other world shader gives off what its stages draw with the lighting term black (a glow stage over a lightmapped base, an additive flame), each stage's image averaged and folded through its blend as the GPU folds it; a shader drawn fullbright is lit by the scene and only its bright texels glow, Q2RTX's synthesized emissive mask (`smoothstep(0.55, 0.85)` of the brightest channel, the same on CPU and GPU), and gives off `rtx_rt_emissive_light` times what it shows. The default shader never glows. The sun is the sky shader's `q3map_sun` (q3map2's `q3map_sunExt` spreads it); the sky lights evenly in its light colour where its shader asks (`q3map_surfacelight`, `q3map_skyLight`, capped at 0.75 lightmap units as the compilers' lightmaps capped it), else with its own images, one cosine-weighted visibility ray a sample. A PVS-culled light grid lists up to 64 emitters per 128-unit cell; resampled importance sampling picks one shadowed emitter sample per pixel. `rtx_rt_lighting 2` traces the compilers' own lights and models instead (light entities with id's q3map, q3map2's or the Wolfenstein falloff and spawnflag rules, backsplash, q3map2's sky-light dome, worldspawn ambient and `_minlight`); `1` lights the world from its lightmaps. Shadow casters: `surfaceparm trans` surfaces cast none; every other world surface is solid, alpha-tested holes included, while models and movers shadow through their own alpha tests; `alphashadow` and `lightfilter` surfaces filter by their light image; patches shadow as with `-patchshadows` (`rtx_rt_patch_shadows`). Dynamic lights are shadowed through alpha tests. Rooms lit only by light entities with no visible fixture (many of RTCW's) are lit by the daylight and emitters that reach them and their bounce; a windowless one stays dim. Quake Live's advertisements, brush models the renderer draws as world surfaces, are traced in place with the world. |
| Dynamic lights | Implemented | The scene's dlights and linear lights, validated and scaled to match the raster light pass (its falloff eased toward smoothstep by `r_dlightFalloff`), sampled with the map's lights and shadowed by all geometry; with `rtx_rt_lighting 1` they are traced over the lightmap. A surface the raster light pass skips (no lighting stage: `nodlight`, sky) takes none, and raster's surface-light proxies are left to the traced surface lights. RTCW's light materials are not sampled: `negdlightshader` and `flamedlightshader` multiply what a world surface or brush model shows by the `$dlight` image projected along world z, once per darkening or brightening stage, as its renderer drew them. |
| Indirect light | Implemented, bounded | Cosine-sampled diffuse paths of up to four bounces per pixel (`rtx_rt_bounces`, one by default), each hit shaded with one light sample drawn among the emitters, the sun and the sky (the first hit takes one of each) and weighted by the reflectances before it; an escaping bounce sees the sky only where the sky is not itself sampled as a light. The compute backend traces the bounce in a checkerboard of 8 x 8 tiles that alternates each frame and counts twice (`rtx_rt_checkerboard`), as Q2RTX checkerboards its indirect light. Surface lights and glowing shaders reach diffuse paths through light sampling alone. |
| Reflections | Partial | One GGX-sampled glossy reflection ray per pixel, shading its hit like a bounce path one bounce shorter. Where it meets a q3map surface light, it and the light sample share that light by multiple importance sampling (balance heuristic): the light sampler's density is taken with the reached point in place of the resampling candidate that could have drawn it, so the shares sum to one although the resampling's normalizer is random (`rtx_rt_mis`). A sky's light is left to the reflection. Materials without material maps take a rough dielectric response (roughness 0.8, normal-incidence reflectance 0.04); a shader with an environment-mapped "shine" stage is glossier (0.3, 0.25) and the traced reflection replaces that stage. Mirrors and portals remain raster. |
| Refraction and caustics | Open | Blended glass and liquids are raster-owned; no transmission, absorption or caustics. |
| PBR material authoring | Partial | A texture pack's companion maps, found by OpenJK rend2's names beside a lit material's texture: `_nh`/`_n` normal maps (rend2's axes; tangent frame from the stage's own coordinates and modifiers), `_specGloss` specular colour and gloss, `_rmo`/`_orm` roughness and metalness (`rtx_rt_material_maps`). Emission remains the shaders' own glow stages; occlusion is unused; no parallax, no replacement-material library or override files like the [Q2RTX material system](https://github.com/NVIDIA/Q2RTX/blob/f2526e9a165949f66e91e82f0d63aa7bb2567b4d/readme.md#material-system). |
| Posed models and player avatar | Implemented, bounded | MD3 (frame interpolation), MDR, IQM (CPU skinning), RTCW MDC/MDS and ET skeletal meshes, and brush models are traced for primary, shadow and secondary rays. The first-person weapon is traced separately and draws over the world as `RF_DEPTHHACK` does; the viewer's own body only shadows and reflects. IHV entities follow the IHV test's visibility flags. |
| Particles, beams, decals and animated effects | Partial | Raster overlay preserves authored primary appearance and draw order. They are not in the acceleration structures, so they neither shadow nor appear in reflections. |
| Environment and sun | Partial | The sky shader's sun, sampled over a 0.5 degree cone or `q3map_sunExt`'s spread, and shadowed; the sky's light, even in its shader's light colour or from its own images, reaches a point through a cosine-weighted visibility ray. Reflections and bounces that escape see the skybox and cloud layer. No procedural atmosphere or time of day. |
| Participating media | Open for RT | Fog volumes are drawn by the raster fog pass over traced colour. RTCW's and Enemy Territory's global fog and Alice's far-plane fog, which raster applies inside each surface's draw, are applied to traced pixels by the same equation at the traced surface's distance (a `nofog` material stays clear). No traced volumetric scattering. |
| Temporal image reconstruction | Partial | Per-pixel motion from the camera and, for moving models, brush models and the weapon, from each vertex's previous position (entities matched frame to frame by model, class, vertex count and nearest origin); a surface new this frame starts without history. History rejection by depth and normal, history length tracking (shorter for moving surfaces), and sample accumulation while view and game time stand still; a model or dynamic light changed while they do restarts it, and one changing every frame is filtered as motion. No history for animated texture changes beyond depth and normal tests. |
| Denoising | Partial | Separate diffuse and specular signals, variance-guided a-trous filtering (after Schied et al. 2017 and Dammertz et al. 2010) with up to six iterations. No gradient-domain history rejection (A-SVGF). |
| Exposure, tone mapping and HDR | Implemented, bounded | Where the sky and emitters light the scene, light is traced in linear units (the shaders' and the sky's colours decoded from gamma 2.2, as Q2RTX decodes its textures) and each view is tone mapped as Q2RTX maps its own: a 128-bin log histogram of the view, weighted toward its middle; a curve fitted to it after Eilertsen, Mantiuk and Unger (equations 14 and 17, the slopes blurred over 12 bins, light below 2^-12 treated as noise) within `rtx_rt_tonemap_range` stops; Reinhard's curve behind an exposure metered on the 70th to 90th percentiles, blended with it (`rtx_rt_tonemap_curve`); a knee toward a white point of 10; then the display's gamma. Unlike Q2RTX, the curve is fitted to the light a surface receives rather than to the surface (the detail layer the paper describes and Q2RTX leaves out), so textures keep their detail, and the exposure adapts over a limited range, the curve darkened with it past that range, so a dark room stays darker than a lit one. Both adapt over time as Q2RTX's do (`rtx_rt_exposure_speed`); `rtx_rt_exposure_bias` offsets them and `rtx_rt_exposure` fixes the exposure, with an ACES curve. A scene-linear frame (`r_hdr` with a tone mapper) is only exposed. The raster sky and overlays keep their authored brightness. The other lighting modes keep the lightmap's units and ceiling. |
| Resolution scaling and performance | Partial | Full-resolution tracing with GPU and CPU timing (`rtx_rt_perf_timing`); the compute backend checkerboards its bounce. Measured on the development machine at 1280 x 720: the x64 hardware backend traces `q3dm1` in about 9 ms, the Win32 compute backend in 45 to 60 ms. The path tracer's pipelines are compiled when the renderer starts, before a level loads: the 32-bit driver compiles the compute backend's trace inside the client's 2 GB address space, and after a large level had loaded (Quake Live's `campgrounds`) it failed about half the time. No upscaling or dynamic resolution. |
| Photo mode and alternative projections | Partial | Still views accumulate to a converged image; no depth of field or alternate projections. |
| Multi-GPU rendering | Open | No comparable distributed path-tracing implementation. |
| Portal, stereo, subview and HUD integration | Partial | Native dispatch is limited to the primary non-portal, centred-stereo world view covering at least half the screen, traced into its own rectangle (ET insets its view by a pixel). Other views, and the portal sky RTCW and Enemy Territory draw as a scene of its own before the main view, are rasterized completely; the main view is then traced from the pass that scene left open. |

## Operator controls

Current registrations and ranges live in
[tr_init.c](../../code/rendererrtx/tr_init.c). These controls are renderer
options, not game-profile ABI switches. Profile selection is immutable and
must be supplied separately at process startup.

| Control | Default | Use and limits |
| --- | --- | --- |
| `cl_renderer` | Project default | Set `rtx` to load this renderer. |
| `rtx_rt_mode` | `0` | `0` exercises RTX raster; `2` path traces the primary view. Latched; use startup settings or `vid_restart`. |
| `rtx_rt_require` | `0` | Set `1` in native validation to reject capability/primary dispatch failures instead of silently using raster. Latched. |
| `rtx_rt_backend` | `0` | `0` the ray-tracing pipeline where the driver has it, else compute; `1` pipeline only; `2` compute only. Latched. |
| `rtx_rt_material_maps` | `1` | Load a texture pack's companion normal and material maps for traced materials. Latched. |
| `r_vertexLight` | `0` | Map-ingestion policy, latched. The installation's own `q3config.cfg` may set it; isolate probe homes. Vertex-lit world surfaces are traced from their authored stages either way. |
| `rtx_rt_lighting` | `0` | `0` lights the scene from the sky and the surfaces that give off light, as Q2RTX does; `1` uses the lightmaps for the world's static light and traces dynamic lights over them, and lights models with the light the raster pass gives them from the light grid; `2` traces the map compilers' own lights and light models. A change rebuilds the lights at once. |
| `rtx_rt_bounces` / `rtx_rt_indirect_strength` | `1` / `1.0` | Diffuse bounces (`0`..`4`) and their scale; `0` keeps the compiler's direct light and ambient only. |
| `rtx_rt_patch_shadows` | `1` | Patches shadow the map's lights, as for maps compiled with `-patchshadows`. |
| `rtx_rt_sun_intensity` / `rtx_rt_sky_intensity` | `1.0` / `1.0` | Scale of the compiler's sun, and of the sky seen by reflections and bounces. |
| `rtx_rt_emissive_scale` / `rtx_rt_dlight_scale` | `1.0` / `1.0` | Scale of glowing stages and of dynamic lights. |
| `rtx_rt_emissive_light` | `2.0` | Light a glowing shader without `q3map_surfacelight` gives off, as a multiple of what it shows. |
| `rtx_rt_exposure` / `rtx_rt_exposure_bias` / `rtx_rt_exposure_speed` | `0` / `0` / `1` | The tone mapper (`0`) or a fixed exposure; stops added to the tone mapper; how fast it adapts (`0` at once). |
| `rtx_rt_tonemap_range` / `rtx_rt_tonemap_curve` | `7` / `0.5` | The display's range in stops (Q2RTX's `tm_dyn_range_stops`), and the fitted curve's share against Reinhard's (one minus Q2RTX's `tm_reinhard`). |
| `rtx_rt_checkerboard` | `0` | The bounce in alternating 8 x 8 tiles: `0` on the compute backend only, `1` always, `2` never. |
| `rtx_rt_specular` | `1` | Specular highlights and reflections. |
| `rtx_rt_mis` | `1` | Reflections share surface lights with light sampling; `0` leaves them to light sampling. Changing this or any other lighting control restarts still-view accumulation. |
| `rtx_rt_denoise` / `rtx_rt_denoise_iterations` / `rtx_rt_accumulate` | `1` / `4` / `1` | Filtering and still-view accumulation. Disable accumulation for sequential diagnostic captures of one view. |
| `rtx_rt_debug_visualizer` | `0` | `1` reflectance, `2` shading normal, `3` denoised light, `4` raw light, `5` specular, `6` emission, `7` history length, `8` view depth, `9` motion. Diagnostics, not shaded-output evidence. |
| `rtx_rt_perf_timing` / `rtx_rt_perf_interval` | `0` / `120` | GPU and CPU timing reports. |
| `rtx_rt_pick` / `rtx_rt_lightinfo` | commands | Name the traced surface at the view centre or along a ray, with its texture directions, light filtering and material maps; list the lights sampled at a point. |

The [runtime smoke script](../../scripts/rtx_runtime_smoke.py) accepts
`--game-profile q3|ql` (default `ql`) independently of
`--profile raster|native|all`. It requires an explicit installed map name,
verifies Q3's base pak or QL's retail markers, creates isolated writable homes,
forces windowed mode, and disables mouse capture, joystick, Steam integration,
Workshop and WebUI. It sets `r_vertexLight 0` before startup to keep the
lightmap comparison explicit. `--plan` emits a reviewable command/manifest without starting
the engine. Example plan:

```text
python scripts/rtx_runtime_smoke.py --exe <fntech3.exe> --retail-root <legitimate-Q3-root> --game-profile q3 --map <installed-map> --profile all --plan
```

Q3 uses `devmap <map>`; QL retains `devmap <map> <gametype>`. Captures must use
the engine's `screenshotPNG` command. No operating-system capture or injected
user input belongs in renderer validation. For comparisons of one frame across
settings, [rtx_probe.py](../../scripts/rtx_probe.py) plays a demo or loads a
map on a private desktop, freezes it, and writes one engine screenshot per
setting with the console's validation and dispatch summary.

## Evidence and next gates

The [2026-09-06 RTX runtime report](2026-09-06-rtx-round.md) records the
earlier hybrid renderer's shared Win32 raster checks and Q3-only x86_64
probes. The 2026-09-24 path tracer record, `2026-09-24-rtx-path-tracer.md`,
records the current renderer: Q3-only x86_64 probes across retail maps with
Vulkan validation on both backends, lifecycle and demo checks, light-filter,
motion and material-map probes, and the shared Win32 client tracing Quake
III, Quake Live, RTCW SP, RTCW MP and Enemy Territory retail maps with the
compute backend in isolated development probes. The
[diagnostic build instructions](../../BUILD.md#windows-q3-only-rtx-diagnostics-x86_64)
describe the explicit `-Dq3-only=true` target; its evidence does not establish
the shared release contract.

Executable regressions cover
[draw-list ownership and fog overlay](../../tests/rtx_raster_execution_tests.py),
[material stage roles and ownership](../../tests/rtx_lightmap_execution_tests.py),
[surface and entity ownership](../../tests/rtx_surface_owner_tests.py),
[the compiler's sun](../../tests/rtx_light_rules_tests.py),
[waveform tables, blending and transport math](../../tests/rtx_transport_tests.py)
and [MDR geometry](../../tests/rtx_mdr_geometry_tests.c). The
[smoke-tool tests](../../tests/rtx_runtime_smoke_tests.py) exercise both profile
plans without launching a game. These establish Unit/Source evidence only.

Next useful gates are the shared Win32 build's hardware backend on a driver
whose x86 path exposes the ray-tracing pipeline, animated emitters, and
measured frame-time budgets for the compute backend. Refraction, traced
volumetrics, a replacement-material library and upscaling are the largest
missing pieces for a Q2RTX-class result; they go beyond what the id Tech 3
games drew.
