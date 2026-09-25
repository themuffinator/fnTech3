# RTX path tracer — 2026-09-24

The RTX renderer's native mode (`rtx_rt_mode 2`) is now a path tracer for the
primary view. It replaces the earlier hybrid tracer recorded in the
[2026-09-06 round](2026-09-06-rtx-round.md). This record covers five rounds
of work on it. The first ran on the Q3-only Windows x86_64 diagnostic build. The
second ([below](#second-round-backends-games-filters-motion-and-materials))
added a compute tracing backend that brings the path tracer to the shared
Win32 client, the Quake Live, RTCW and Enemy Territory light models, the
compilers' light filters, motion-aware denoising and a texture pack's
material maps. The third
([below](#third-round-emitter-mis-q3map2-skies-bounces-and-accumulation))
made glossy reflections share the map's surface lights with light sampling,
added q3map2's sky light and spread sun, made `rtx_rt_bounces` trace the
bounces it names, and fixed a Quake Live map that stopped the tracer. The
fourth ([below](#fourth-round-portal-skies-fog-dynamic-and-stripped-lights))
traced the main view behind RTCW's portal skies, matched dynamic lights and
RTCW's light materials to raster, fogged traced pixels with RTCW's and Enemy
Territory's global fog, left dark the RTCW lights its compiler left dark, and
lit the maps whose light entities q3map2 left out from their lightmaps. The
fifth ([below](#fifth-round-sky-and-emitter-lighting-linear-light-and-q2rtxs-tone-mapper))
lit every map by default as Q2RTX lights Quake II, from the sky and the
surfaces that give off light, traced that light in linear units, and tone
mapped it with Q2RTX's tone mapper.
**Q2RTX parity and full compatibility remain open**; the
[feature comparison](RTX_Q2RTX_PARITY.md) lists what is implemented and what
is not, and the [compatibility ledger](COMPATIBILITY.md) owns the gates.

## What the renderer does

- The world's opaque and alpha-tested surfaces, posed MD3, MDR, IQM, MDC, MDS
  and ET models, brush models and the first-person weapon are traced against
  hardware acceleration structures rebuilt per frame in the frame's own
  command buffer. The raster base pass draws the traced surfaces for depth,
  the composite writes traced pixels over it, and the raster overlay draws
  everything else, including the fog pass of traced surfaces in fog volumes.
- Materials are the shaders' own stages, evaluated per hit with the lighting
  term at one and at zero to separate reflectance from emission.
- Light is the map's: light entities, surface lights, sun and ambient, with the
  compilers' light models, sampled by resampled importance sampling from a
  PVS-culled grid and shadowed; one diffuse bounce and one glossy reflection
  are added; a temporal pass and a-trous iterations denoise the result. (As
  first recorded; since the fifth round the default lights with the sky and
  emitters alone, and the compilers' light models are `rtx_rt_lighting 2`.)

## Defects found and fixed in this round

| Finding | Evidence | Fix |
| --- | --- | --- |
| Traced textures blurred at grazing angles, where raster stays sharp | q3dm17 floor at 1280x720: traced tile detail lost past a few metres; jump pad rings smeared | Primary hits carry ray differentials (Igehy 1999); every stage samples with `textureGrad` through its own `tcGen`/`tcMod` chain, so the anisotropic samplers apply |
| The sun was missing on maps whose sky shader has no `skyparms`, and `q3map_sun` colour was normalised by its largest component, as q3map2 does, instead of to unit length, as id's q3map does | q3dm17 (`textures/skies/blacksky`, `q3map_sun 1 1 1 32 90 90`): floor 18% darker than raster; the sun contributed nothing | The tracer resolves the sun as the map's compiler did (`RT_FindSun`): the first sky surface's shader for q3map, the first shader with a sun for q3map2 (and its `_noshadersun`), each with its own colour normalisation |
| A third-party pak in the test installation (`zz-q3netpack.pk3`) redefines every retail sky shader without `surfaceparm sky`, so traced skies became opaque occluders | q3dm17 and q3ctf1 skies classified as traced; the sun's shadow rays stopped at the sky | Sky is also taken from the map's BSP shader lump, the flags its compiler saw (`rtCompiledSky`) |
| Walls lying on a light-grid cell boundary took the cell behind them, whose PVS-culled list lacks their lights | q3ctf1 red wall (`red_scary4_csansfacefin`, y=-1536): direct light exactly zero; traced mean 9.1 against raster 38.5 | The cell is looked up one unit out along the lit side's normal, where the grid samples its cells (48.9 against 38.5 after the fix, the difference being bounce) |
| q3map and q3map2 cast no shadows from `surfaceparm trans` surfaces without `alphashadow`/`lightfilter`; the tracer did | Source (both compilers' `light_trace.c`) | Such world surfaces go to their own instance, ignored by the map's lights and sun and not by dynamic lights |
| id compiled its maps with `-patchshadows`, which neither compiler defaults to | q3dm7 direct light: patches shadowing 45.7 (floor 129/106/63) against raster 41.7 (123/98/55); not shadowing 59.9 (187/161/83) | Patches shadow by default; `rtx_rt_patch_shadows 0` lets the map's lights through them, rebuilding the world's acceleration structures at the next frame |
| q3map reads `light` and falls back to `_light` only when it is zero | Source | Both keys are kept and combined as q3map does |
| The light grid's cell-count cap multiplied three counts in 32 bits | Source: a far-flung but valid world wraps past the cap | 64-bit product |
| The raster path's scissor was clamped to the window, not the render target, so dlight-shadow atlas tiles past the window width got negative widths | Demo `four` with validation: 9 `vkCmdSetScissor` overflow errors and 9 zero-width `vkCmdClearAttachments` errors, in raster mode too | Clamp to the render pass's target (as renderervk already did); skip empty clears |
| IHV entities' visibility flags were ignored by the tracer | Source | Portal markers are skipped, hidden bodies only shadow, view weapons are traced as weapons |

The round also removed the replaced tracer's code and 68 inert `rtx_rt_*`
controls, the per-scene surface-light proxy selection whose output nothing
read, and two orphaned IHV ray headers with their scene-signature test.

## Build identity

Source: `b68782bfe41eeff02097ec2b2eaeed3eb0a1b4db` plus the uncommitted working
tree, including other sessions' concurrent work. These are dirty-tree
artifacts, not a reproducible build of that commit.

| Artifact | SHA-256 |
| --- | --- |
| `.tmp/meson-q3-rtx-x64/fntech3-q3.exe` (map sweeps, 1080p timing) | `70063dc4b0de4cea48691a73ed399d5bd265f743972a19d9d91880e355e3cabb` |
| `.tmp/meson-q3-rtx-x64/fnql_rtx_x86_64.dll` (map sweeps, 1080p timing) | `746f6ded51c9eab6e49dae1175cfae6b54a105f27017bc808e285c6f5913a86a` |
| `.tmp/meson-q3-rtx-x64/fntech3-q3.exe` (final: lifecycle, demo) | `55a46d9d2950289a8f9225e4145ea292a6361bffa6c46e1f5603f6c216a82bc7` |
| `.tmp/meson-q3-rtx-x64/fnql_rtx_x86_64.dll` (final) | `24ab7efcb769b84402b62eef7ee47443572050f472fea42fbf4dc5e252e1a2c1` |
| `meson/build/win32-dev/fntech3.exe` (Win32 fallback) | `6cce62cddab5069847fcadfe8e0fa8b07c2b27a788faf892995a21f8dcb5c095` |
| `meson/build/win32-dev/fnql_rtx_x86.dll` (Win32 fallback) | `9b59e9409d1e0a6b849c750854714b189a78f8a4d941d7867f41422949304fe8` |

The final x64 renderer differs from the sweep build only by the raster scissor
fix and an array-size cast.

Platform: Windows 11 Home 10.0.26200, NVIDIA GeForce RTX 4060 Laptop GPU,
Vulkan API 1.4.325, driver 591.74. Installation: the Steam Quake III Arena
install, read-only, with an isolated `fs_homepath` per run whose empty
`q3config.cfg` shadows the installation's (which sets `r_vertexLight 2`).

## Runtime probes

All runs used `scripts/rtx_probe.py` on a private desktop, windowed, with no
input; images are engine `screenshotPNG` captures from the render target.
Unless stated, `rtx_rt_mode 2`, `rtx_rt_require 1` and
`rtx_debug_vk_validation 1` (Khronos validation layer) were set at startup,
the map was loaded with `devmap` and advanced 250 frames, the traced frame
captured, then `rtx_rt_mode 0` and `vid_restart` captured the same view
rasterized.

### Eight retail maps

Mean brightness of the 1280x720 frame above the HUD (0-255), traced against
raster in the same session and view. Every run: native dispatch logged, no
validation messages, no fatal line, exit code 0.

| Map | Default settings | ratio | Direct light only (`rtx_rt_bounces 0`) | ratio |
| --- | --- | --- | --- | --- |
| q3dm1 | 51.7 / 48.2 | 1.07 | 46.9 / 48.2 | 0.97 |
| q3dm7 | 42.5 / 30.0 | 1.42 | 45.7 / 41.5 | 1.10 |
| q3dm17 | 25.3 / 21.4 | 1.18 | 24.7 / 21.4 | 1.15 |
| q3tourney2 | 37.5 / 30.0 | 1.25 | 44.6 / 38.7 | 1.15 |
| q3ctf1 | 34.8 / 27.9 | 1.25 | 25.2 / 22.9 | 1.10 |
| q3dm6 | 55.4 / 39.9 | 1.39 | 23.9 / 25.2 | 0.95 |
| q3dm11 | 50.2 / 43.6 | 1.15 | 45.9 / 43.6 | 1.05 |
| q3tourney4 | 49.1 / 46.2 | 1.06 | 39.9 / 46.2 | 0.86 |

Spawn points differ between runs, so compare each row's pair, not rows across
columns. Traced direct light is within 16% of the compiler's lightmaps on all
eight maps; the default adds a bounce that the compilers never baked, which is
most visible in enclosed spaces. The largest remaining direct-light
difference, on q3tourney4, is a sun shaft under a skylight that the tracer
does not attenuate: the overlay's blended surfaces are not in the
acceleration structures, while q3map lets `alphashadow` and `lightfilter`
surfaces filter light.

### Lifecycle

`lifecycle-0924b`, final build: q3dm1 at 960x540, `vid_restart`,
`devmap q3dm17`, `rtx_rt_patch_shadows 0` (world rebuilt at the next frame),
a resize to 1280x720 with `vid_restart`, `rtx_rt_mode 0` with `vid_restart`,
then `rtx_rt_mode 2` with `vid_restart`. Seven captures, mode gate
`requested 2, active 2, require 1`, no validation messages, exit code 0 in
27.9 s.

### Demo playback

`demo-four-0924b`, final build: `demo four` played by `timedemo` for 700
frames (moving camera, players, weapon fire and dynamic lights) traced, then
rasterized. Two validation messages remain, both a warning that a raster
shadow-atlas pipeline's fragment shader writes a colour output its depth-only
pass does not have; the same two appear in a raster-only run of the demo.

### Performance

`perf1080-q3dm1` and `perf1080-q3dm7`: 1920x1080, `com_maxfps 0`,
`rtx_rt_perf_timing 1`, 400 frames. GPU time per traced frame, steady state:

| Map | Acceleration builds | Trace | Denoise | Composite | Total | CPU |
| --- | --- | --- | --- | --- | --- | --- |
| q3dm1 | 0.22 ms | 8.2 ms | 5.4 ms | 0.24 ms | 14.1 ms | 0.4 ms |
| q3dm7 | 0.24-0.31 ms | 8.4-9.1 ms | 6.5-6.9 ms | 0.25 ms | 15.8-16.1 ms | 0.4-0.5 ms |

### Win32

The shared Win32 client and `fnql_rtx_x86.dll` build with strict warnings. On
this machine's Win32 driver, `rtx_rt_mode 2` with `rtx_rt_require 0` reports
the missing `VK_KHR_acceleration_structure`, `VK_KHR_deferred_host_operations`
and `VK_KHR_ray_tracing_pipeline`, falls back to raster, and renders q3dm1
(`x86-fallback-0924`). The second round traces there with the compute
backend.

## Automated evidence

The RTX-related test set (every `tests/*.py` naming the renderer) passes
except four tests outside this work: three collision-loader fixtures that
lag another session's uncommitted `cm_load.c` change, and a screenshot fixture
that compiles as 64-bit where the shared headers require 32-bit. New and
rewritten tests: `rtx_surface_owner_tests.py` and `rtx_light_rules_tests.py`
(executed ownership and sun choice), the IHV beam and particle ownership
checks, and source contracts for the new buffer, light-input, sun, shadow
and texture-filtering rules.

### Open after the first round

Native tracing on the shared Win32 client; QL, RTCW and ET content; light
filters (`alphashadow`, `lightfilter`); motion-aware denoising; authored PBR
inputs; emitter multiple importance sampling. The second round addresses all
but the last.

## Second round: backends, games, filters, motion and materials

### What changed

- **Compute backend.** Where the driver offers no ray-tracing pipeline (this
  machine's 32-bit NVIDIA driver lacks `VK_KHR_acceleration_structure`,
  `VK_KHR_ray_tracing_pipeline` and `VK_KHR_deferred_host_operations`), the
  path tracer traces with compute shaders. They traverse binned-SAH BVHs of
  the same instances that the renderer builds on the CPU
  ([`rt_bvh.c`](../../code/rendererrtx/rt/rt_bvh.c)). The per-pixel code
  ([`rt_path.glsl`](../../code/rendererrtx/shaders/rt_path.glsl)) is shared,
  and `rtx_rt_backend` chooses a backend (0 automatic, 1 pipeline, 2 compute).
- **View rectangle.** Any main view covering at least half the screen is
  traced into its own rectangle; Enemy Territory insets its view by one
  pixel, which kept it rasterized before.
- **Light models.** Each map is lit with its compiler's model. id's q3map
  reads `light` before `_light` and always applies the angle term. q3map2
  reads `_light` first with `scale`, and drops the angle term for linear and
  spawnflag-2 lights. RTCW's and ET's compilers make a light linear and
  angle-free unless its spawnflags say otherwise, and apply `fade`. A
  `lightJunior` lights models only; a no-grid light lights surfaces only.
  Backsplash follows each compiler: id's only for unsubdivided emitters,
  q3map2's for every piece but none on sky, and `q3map_backsplash`.
  A sun ray must reach the sky.
- **Shadows as the compilers cast them.** `trans` surfaces cast no shadow
  from the map's lights. `alphashadow` surfaces, and q3map2's `lightfilter`
  ones, filter the light through the compiler's light image
  (`q3map_lightImage`, `qer_editorimage` or the shader-named image), read
  texel by texel. Every other surface is solid to the map's lights and sun,
  alpha-tested holes included, as in both compilers' traces (id's
  `light_trace.c` `SetFacetFilter`, q3map2's `TraceTriangle`). Dynamic lights
  still pass alpha-tested holes. Near bright lights the composite eases
  specular light with the irradiance toward the lightmaps' ceiling.
- **Motion.** Each moving entity is matched to its previous frame by model,
  class, vertex count and nearest origin. Its surfaces reproject along their
  own motion, and a surface new this frame starts without history.
- **Material maps.** A texture pack's `_nh`/`_n` normal maps, `_specGloss`
  maps and `_rmo`/`_orm` roughness and metalness maps, found under OpenJK
  rend2's names beside a lit material's texture (`rtx_rt_material_maps`). No
  retail Q3, QL, RTCW or ET pak pairs any texture with an image under those
  names (a scan of every image in the installations below).
- **Light grid build.** Each cell now tests only the lights whose reach can
  touch it, listed per light, not every light in the map. An executable test
  (`tests/rtx_light_entity_tests.py`) builds a random 3,000-light grid both
  ways and gets the same grid, byte for byte.

### Defects found and fixed

| Finding | Evidence | Fix |
| --- | --- | --- |
| The shared Win32 client could not trace | x86 gate: the three extensions above missing | Compute backend |
| Enemy Territory's main view was never traced | `oasis`: viewport 1,1 1278x718, eligibility required the whole screen | View rectangle |
| RTCW MP traced at 2.4 times its raster frame | `mp_ice`: `lightJunior` entities lit world surfaces | Grid-only juniors (0.99 afterwards) |
| A backsplash light per emitter facet made 4,000 lights and a 4.2 s grid build | `mp_ice` | Backsplash gathered per subdivision cell (407 point lights) |
| Alpha-tested surfaces let the map's lights through their holes | q3dm6: grate-lamp cages cast grid-shaped light that the lightmaps lack; direct light 0.99-1.92 times raster over the map's ten spawn views | Solid rays for the map's lights and sun: 0.82-1.16 |
| Filter surfaces read the drawn texture, and a leading `$lightmap` stage's alpha | Source: both compilers' `LoadShaderImage` | The compiler's light image, texel by texel |
| Specular from a lamp 8 units away washed its wall out | q3dm6 spawn 7: 3.0% of the frame saturated against the raster frame's 0.17% | Specular eased with the irradiance: 1.3% |
| RTCW MP's light grid took 1.85 s to build | `mp_ice`, 4,529 lights over 168,168 cells | Per-light cell lists: 0.27 s, the same 3,838,521 entries |

### Build identity

Source: `b68782bfe41eeff02097ec2b2eaeed3eb0a1b4db` plus the uncommitted working
tree, including other sessions' concurrent work.

| Artifact | SHA-256 |
| --- | --- |
| `.tmp/meson-q3-rtx-x64/fntech3-q3.exe` | `1f29e4355671eb7bef1615d82de6dd4bb8120aca56d2e1f4aa824aa4b063fe8c` |
| `.tmp/meson-q3-rtx-x64/fnql_rtx_x86_64.dll` | `67e5e28a3c2bee84265bdf534a32f5849a104585553873958f758183b64cf423` |
| `meson/build/win32-dev/fntech3.exe` | `dcc9f117db4ffff49ce2c6bd86d1a3ae8186e6118fc2b1dadcffbbe82f85aeeb` |
| `meson/build/win32-dev/fnql_rtx_x86.dll` | `31aa58d88c5502a6e09f1317036e2bc728a0b688eb4c2e97599a4ebbc491730b` |

Every result below comes from these four files. The Win32 build is the
developer configuration, with RTCW and ET enabled for isolated probes.

### Both backends draw the same image

`backend_sweep` (a scratch harness driving `scripts/rtx_probe.py`): one launch
per map, frozen with `timescale 0`, one capture on the ray-tracing pipeline
and one on the compute backend, each after its own `vid_restart`, with the
Khronos validation layer. 1280x720; per-channel difference over the view
above the HUD (0-255).

| Map | Mean difference | Pixels over 16 | Validation messages | Trace, pipeline | Trace, compute |
| --- | --- | --- | --- | --- | --- |
| q3dm7 | 0.0033 | 6 | 0 | 5.9 ms | 29.2 ms |
| q3dm17 | 0.0024 | 1 | 0 | 3.5 ms | 7.2 ms |
| q3tourney2 | 0.0035 | 0 | 0 | 6.4 ms | 30.8 ms |
| q3ctf1 | 0.0043 | 5 | 0 | 4.5 ms | 33.6 ms |
| q3dm6 | 0.0026 | 1 | 0 | 7.8 ms | 33.8 ms |
| q3dm11 | 0.0032 | 2 | 0 | 9.7 ms | 53.7 ms |

GPU trace time at 1280x720 on the NVIDIA GeForce RTX 4060 Laptop GPU; the
denoiser adds 1.3-2.9 ms. Before solid shadow rays the compute backend took
12.5-63.8 ms on the same maps.

### Eight retail maps, final build

`map_sweep`, as in the first round (traced, then `rtx_rt_mode 0` and
`vid_restart`, the same frozen view); no validation messages, native dispatch
and exit code 0 on every run.

| Map | Default | Direct light only |
| --- | --- | --- |
| q3dm1 | 1.048 | 0.950 |
| q3dm7 | 1.251 | 1.144 |
| q3dm17 | 1.178 | 1.154 |
| q3tourney2 | 1.233 | 1.033 |
| q3ctf1 | 1.247 | 1.022 |
| q3dm6 | 1.204 | 1.069 |
| q3dm11 | 1.122 | 1.028 |
| q3tourney4 | 0.989 | 0.803 |

Traced frame brightness over the raster frame's. Traced direct light is
within 16% of the lightmaps on seven maps. On q3tourney4 the traced back walls
are darker; the same view gave 0.806 before the solid-shadow and specular
changes, so they did not cause it. q3dm17's sunlit floor is brighter than its
lightmap, whose floor seems to lack its shader's sun.

On q3dm6, over its ten deathmatch spawn points (`setviewpos` to each), direct
light alone went from 0.99-1.92 of the raster frame with alpha-tested shadows
to 0.82-1.16 with solid ones. With the defaults the range is 1.02-1.31, the
bounce the compilers never baked. The lightmap luxel behind a grate lamp is
saturated (255 248 247), matching the traced light there.

### Five games on the shared Win32 client

`scripts/probe_rtx_games.py`, `x86-final-2`: each profile loads its map on a
private desktop, freezes, traces with `rtx_rt_require 1`, then restarts to
raster and captures the same frame. All five traced with the compute backend,
with the capability gate at requested 2, active 2, no errors, exit code 0 and
the installation's file inventory unchanged. No 32-bit Vulkan validation layer
is installed on this machine.

| Profile | Map | Light model | Lights | Grid build | Traced / raster | Trace (720p) |
| --- | --- | --- | --- | --- | --- | --- |
| q3 | q3dm17 | q3map | 278 | 40.5 ms | 1.490 | 7.2 ms |
| ql | campgrounds | q3map2 | 1,650 | 36.3 ms | 0.826 | 39.7 ms |
| rtcw-mp | mp_ice | Wolfenstein | 4,529 | 273.3 ms | 1.023 | 37.3 ms |
| rtcw-sp | escape1 | Wolfenstein | 3,500 | 75.6 ms | 1.029 (raster main view; see the fourth round) | 16.3 ms |
| wolfet | oasis | Wolfenstein | 12 | 71.5 ms | 0.906 | 10.8 ms (1278x718) |

Quake Live's frame is mostly its join-match menu. Under it the grate-lamp
light patterns of the previous build are gone. The traced scene is darker
than the lightmap, which q3map2 compiled with radiosity bounces that the
default single traced bounce does not fully replace. Solid shadow rays also
made the compute trace cheaper, since alpha tests are no longer evaluated for
the map's lights: the previous build traced `mp_ice` in 55.9 ms, `escape1` in
28.4 ms and `oasis` in 24.9 ms.

### Light filters

`flt` backend sweep (q3dm1, q3dm10, q3dm19; the build before the light-image
change): 12, 655 and 198 world triangles filter the map's light; the
backends agree to 0.0033, 0.0011 and 0.0078; no validation messages.

### Motion-aware denoising

`motionz-q3dm1`: q3dm1 unfrozen, with the rotating and bobbing items in view,
`rtx_rt_debug_visualizer 9` (motion) and `7` (history), with validation, no
messages. The static world shows no motion (at most 0.5/255 from the neutral
value) and 32 frames of history. The armour's 1,879 moving pixels move up to
1.1 pixels a frame and keep a median 12 frames of history, under the cap of
16 for moving surfaces.

### Material maps

Synthetic packs only (`make_material_pack.py`: no retail content, only
texture names), each staged into the probe's isolated home with
`scripts/rtx_probe.py --pak`:

- **Orientation.** A uniform normal map for the q3dm1 wall at the view
  centre (`blocks17floor2`, whose s grows along +X and t along -Z,
  per `rtx_rt_pick`), read through `rtx_rt_debug_visualizer 2` with
  `r_overBrightBits 0` and `r_bloom 0`. Red 204 (+s) gave the normal
  (0.600, 0.796, -0.004), and green 204 (+t) gave (0.004, 0.796, -0.600),
  where rend2's axes predict (0.6, 0.8, 0) and (0, 0.8, -0.6). A flat map
  left it at (0, 1, 0).
- **Whole map.** Packs for q3dm1's 74 texture names: 61 materials took the
  maps; bump maps shade as lit domes; `_rmo` metal and `_orm` roughness
  change the frame as expected. With `rtx_rt_material_maps 0` no material
  took one.
- **Backends** (`matbkz-bumps`, final build): in one launch the pipeline and
  compute frames differ by 0.0042 (final colour) and 0.0004 (normals), with
  no validation messages.

### Automated evidence

The RTX test set passes except for failures outside this work: the
`REF_API_VERSION` 29 assertions after another session's bump to 30, the
Huffman boundary test, and Alice, collision-loader and 64-bit fixture tests
behind other sessions' uncommitted changes. New or extended here: the
compute backend's bindings and signatures, filter and solid-ray rules, the
light image, material maps (names, flags, GLSL constants, shading) and the
specular easing (`rtx_renderer_contract_source_tests.py`,
`rtx_transport_tests.py`); an executable light-grid equivalence test and the
light entity rules (`rtx_light_entity_tests.py`).

## Third round: emitter MIS, q3map2 skies, bounces and accumulation

### What changed

- **Glossy reflections share surface lights with light sampling.** A GGX
  reflection ray that meets a q3map surface light now shows it, weighted
  against the light sample by the balance heuristic (Veach and Guibas 1995).
  A world triangle names the light it emits as through a table built by
  exact centroid match (`RT_MapPrimitiveLights`); lights a reflection cannot
  meet keep light sampling's whole share. The light sampler's density at a
  point the reflection met is taken with that point in place of the
  resampling candidate that could have drawn it (the light's own place when
  the cell is enumerated, else the first candidate); each candidate draws
  from its own random stream, so the reflection side draws the replaced one
  again. The two shares then sum to one on every path, although the
  resampling's normalizer is random. A sky's emitters are left to the
  reflection, which sees the sky itself. `rtx_rt_mis 0` restores light
  sampling alone.
- **q3map2's skies.** `q3map_sunExt`'s deviance and sample count spread the
  sun as the compiler did (one sample in the sun's direction, the rest
  uniform over a disc of the deviance in azimuth and elevation), and
  `q3map_skyLight value iterations` lights the map with its dome of
  `4 (iterations - 1)^2 + 1` equal suns in the sky's light colour
  (`CreateSunLight`, `CreateSkyLights`). A map from a q3map2 build that wrote
  no `_q3map2_version` is recognised by worldspawn keys only q3map2 reads
  (`_lightmapscale`, `_blocksize`, `_celshader`, `_farplanedist`,
  `_foghull`, `_keepLights`, `_floodlight`, `_ignoreleaks`) or by a sky that
  uses either directive.
- **Diffuse bounces.** `rtx_rt_bounces` now traces the paths it names, up to
  four bounces: each hit adds its light sample, weighted by the reflectances
  before it, and a baked surface ends the path. A reflection's surface takes
  one bounce fewer.
- **Accumulation restarts** when a setting the trace reads changes
  (`RT_TraceSettingsCount`), with the denoiser's history.
- The timing report (`rtx_rt_perf_timing`) states how many still frames have
  been averaged.

### Defects found and fixed

| Finding | Evidence | Fix |
| --- | --- | --- |
| Mirror-like surfaces lost the reflections of the map's surface lights: a light sample in a narrow GGX lobe exceeds the radiance clamp and the temporal filter's outlier clip | q3dm1 with a synthetic gloss pack (roughness 0.05), view 3: the wall beside the door, converged specular 4.0 with light sampling alone, the same over two repeats | Emitter MIS: 21.1, both repeats |
| Glossy surfaces counted the sky twice: the light sample's specular from the sky's emitters and the reflection's skybox | q3dm1, synthetic satin pack (roughness 0.3), outdoor view 1: converged specular 10.26 without MIS and 8.24 with it; a test build that kept the sky's light-sample specular gave 10.26 either way | The sky's emitters are left to the reflection |
| `rtx_rt_bounces 2` drew what `1` drew | Source: the shader only tested for a bounce | Bounce paths: `ct3tourney3` traced over raster 0.360, 0.425, 0.456 and 0.466 at 0, 1, 2 and 4 bounces |
| q3map2's sky light was not lit | Enemy Territory's `oasis`: 0.906 of its raster frame | Dome of suns: 0.987 |
| `q3map_sunExt`'s spread was ignored | Quake Live's `pak00.pk3`: 45 skies spread the sun, 33 over 3 degrees in 16 samples | Spread sun; campgrounds reports 3.0 degrees in 16 samples |
| A q3map2 map without `_q3map2_version` was lit by id's rules | `ct3tourney3`, a community map in the test installation (`_lightmapscale`, `q3map_sunExt`, `q3map_skyLight`): sun 146 136 114 by id's normalisation | q3map2's: 230 214 179, and its sky light |
| A setting changed while the view stood still stayed mixed with the old image | Source: after 400 frames 7% of the average predates the change | Accumulation and history restart |
| On a map with depth-fade shaders, the first traced alpha-tested surface of the raster base pass took the depth-fade snapshot, which leaves the post-bloom render pass open; the trace then refused the view, and `rtx_rt_require 1` ended the game | Quake Live's `asylum` on the Win32 client: `rtx_rt_debug_as_stats 2` showed the view refused in render pass 3 (post-bloom) | The overlay list after the trace takes the snapshot, as it already did for liquids; `asylum` traces |
| A frame the tracer could not draw did not say why | `asylum` above | Each reason is reported once per session (`RT_FrameFailed`), and the remaining silent exits at developer level |

The multi-game probe (`scripts/probe_rtx_games.py`) did not shadow an
installation's own configuration: the Steam Quake III install's
`q3config.cfg` sets `r_vertexLight 2` and `r_picmip 1`, so the second round's
Win32 Quake III ratio (q3dm17, 1.490) compared the traced frame with a
vertex-lit raster frame. The probe now writes an empty configuration under
each profile's own name; the Quake Live, RTCW and Enemy Territory installs
have none that sets renderer variables. Quake Live keeps `timescale` at 0.1 or
more, as retail does, so its live views never stand still and are not
accumulated; its demos now freeze with `cl_freezeDemo` and accumulate.

Where the lightmap brightness still differs, compile options the BSP does
not record may be the cause: `ct3tourney3`'s room stays at 0.47 of its
lightmap's brightness at four bounces, as if its build brightened the
lightmap (`-gamma`, `-compensate`, `-bounce` scaling).

### Build identity

Source: `b68782bfe41eeff02097ec2b2eaeed3eb0a1b4db` plus the uncommitted working
tree, including other sessions' concurrent work.

| Artifact | SHA-256 |
| --- | --- |
| `.tmp/meson-q3-rtx-x64/fntech3-q3.exe` | `ef40d2d751c76d25c0f5972897d80c44ef8daf5aa5cdb87e74b649a4e58ceb26` |
| `.tmp/meson-q3-rtx-x64/fnql_rtx_x86_64.dll` | `4c403c784601464cb841ed83b78c4f1411195113aec5d445ea16f7d37a023530` |
| `meson/build/win32-dev/fntech3.exe` | `dcc9f117db4ffff49ce2c6bd86d1a3ae8186e6118fc2b1dadcffbbe82f85aeeb` |
| `meson/build/win32-dev/fnql_rtx_x86.dll` | `5d1b2494d3916f9244620fc942aec952cb429cc29e4200fdd1ba8722bb019d73` |

The x64 sweeps below ran on the renderer before the still-frame count joined
the timing report, its only difference from the final one.

### The shares sum to one

A Monte Carlo model of the sampler (a scratch harness in numpy: four triangle
lights and two point lights over a receiver, candidates enumerated or drawn
with replacement, shadow rays between lights, 4,000,000 samples per cell)
estimates the triangle lights' specular four ways:

| GGX alpha, candidates | Light sampling alone | Reflection alone | MIS, candidate replaced | MIS, drawn normalizer as is |
| --- | --- | --- | --- | --- |
| 0.12, all 6 | 0.1058 ± 0.0006 | 0.1037 ± 0.0011 | 0.1054 ± 0.0006 | 0.1054 |
| 0.12, 3 | 0.1048 ± 0.0005 | 0.1044 ± 0.0011 | 0.1047 ± 0.0004 | 0.1146 |
| 0.12, 1 | 0.1049 ± 0.0001 | 0.1055 ± 0.0011 | 0.1050 ± 0.0001 | 0.1553 |
| 0.4, 3 | 0.1312 ± 0.0005 | 0.1306 ± 0.0010 | 0.1312 ± 0.0004 | 0.1441 |
| 0.4, 1 | 0.1316 ± 0.0001 | 0.1329 ± 0.0011 | 0.1316 ± 0.0001 | 0.1965 |

The replaced-candidate density agrees with both single estimators; taking the
drawn normalizer as the density biases the result by 9-49% once candidates
are drawn.

On the GPU, a temporarily instrumented shader (not kept) replayed every
candidate at each reflection hit on a traced light and compared the rebuilt
weight sum with the one the light sample recorded, over four q3dm1 views with
a gloss pack: 6,577 hits on the ray-tracing pipeline and 6,441 on the compute
backend, no mismatch.

In the renderer (q3dm1, satin pack, a test build keeping the sky's
light-sample specular, two alternating repeats each): converged specular with
and without MIS agrees to within the repeats' spread where the lobes are wide
(outdoor view: 10.264 and 10.260 without, 10.267 and 10.269 with). Where
lobes are narrow MIS adds the light that light sampling's clamps lost (1.6-1.9%
of a view's specular). Per-frame specular noise is unchanged over whole
views except where surface lights dominate the reflection.

### Both backends draw the same image

`backend_sweep`, final build, with the Khronos validation layer:

| Map | Mean difference | Pixels over 16 | Validation messages | Trace, pipeline | Trace, compute |
| --- | --- | --- | --- | --- | --- |
| q3dm7 | 0.0036 | 16 | 0 | 7.8 ms | 32.5 ms |
| q3dm17 | 0.0016 | 0 | 0 | 3.4 ms | 7.5 ms |
| q3tourney2 | 0.0033 | 0 | 0 | 7.4 ms | 35.3 ms |
| q3ctf1 | 0.0040 | 0 | 0 | 4.7 ms | 43.6 ms |
| q3dm6 | 0.0047 | 0 | 0 | 9.1 ms | 36.1 ms |
| q3dm11 | 0.0031 | 2 | 0 | 10.7 ms | 43.9 ms |

q3dm17 differed by 1.81 in two of five launches, with the same still-frame
counts in both captures (99, 199, 299): its per-frame random seeds follow the
engine's frame counter, which a session occasionally starts a frame apart,
giving another noise realisation of the same image (block means within 2 of
255, equal frame means). With the synthetic material packs, which exercise the
reflection paths, the backends were not compared in one launch: two such
launches ended in the driver fault below.

### Eight retail maps

`map_sweep` (views are the random spawn of each launch, so ratios compare
only within a row); no validation messages, native dispatch and exit code 0:

| Map | Default | Direct light only |
| --- | --- | --- |
| q3dm1 | 1.000 | 0.885 |
| q3dm7 | 1.328 | 0.936 |
| q3dm17 | 1.178 | 1.150 |
| q3tourney2 | 1.226 | 1.030 |
| q3ctf1 | 1.342 | 1.014 |
| q3dm6 | 1.214 | 1.016 |
| q3dm11 | 1.119 | 1.027 |
| q3tourney4 | 0.970 | 0.790 |

### Demos with strict native RT

`x86-mis-demos` (probe_rtx_games, `rtx_rt_require 1`, compute backend): the
Quake Live demo `fntech3-ql-probe.dm_91` and the RTCW MP demo
`retail-mp-probe.dm_60` (the [RTCW demo record](2026-09-18-rtcw-demos.md)'s
retail recording) play and trace, exit code 0, no errors; the frames match
the second round's to within 0.14 of 255. Enemy Territory has no demo
playback in fnTech3 yet (`code/client/cl_main.cpp`), and retail RTCW SP
cannot replay SP demos, which fnTech3 refuses in the same way; neither is a
renderer limit.

### Driver fault at teardown

Five of about ninety probe launches in this round ended with an access
violation inside the NVIDIA driver (`nvoglv64.dll` + `0xf3bdd3`) while the
renderer shut down, at `vid_restart` or at quit, all with the Khronos
validation layer loaded and none with a validation message before it. Three
repeats of a launch that faulted, and one under `cdb`, did not fault. The
shutdown waits for the device to go idle and destroys acceleration
structures before their storage. The cause is open.

## Fourth round: portal skies, fog, dynamic and stripped lights

### What changed

- **The main view behind a portal sky is traced.** RTCW and Enemy Territory
  draw a map's portal sky (`props_skyportal`) as a scene of its own before the
  main view, flagging only that scene `RDF_SKYBOXPORTAL` (their `cg_view.c`
  `CG_DrawSkyBoxPortal`). The trace takes one view a frame and took that one,
  so on those maps the main view stayed raster. The portal sky is now
  rasterized, and the main view is traced from the pass the portal scene left
  open: its depth-fade snapshot ends the main render pass, and the post-bloom
  pass that follows now keeps its depth whenever tracing is available.
  `rtx_rt_debug_as_stats 2` no longer reports the portal sky's refusal, which
  had hidden the main view's.
- **RTCW's light materials multiply instead of lighting.** `negdlightshader`
  (the zombie spirits' white light of radius 300 to 500, `cg_players.c`) was
  traced as a white light; retail and fnTech3's raster darken the surfaces it
  reaches (`GL_ZERO GL_ONE_MINUS_SRC_COLOR`), and `flamedlightshader`
  brightens them twice over (`GL_DST_COLOR GL_ONE`). The tracer now leaves
  both out of light sampling and multiplies what a world surface or brush
  model shows by the `$dlight` image projected along world z, as
  `RTCW_ProjectLightVertex` projects it, once per darkening or brightening
  stage of the material. Models are left alone, as raster leaves them.
- **Dynamic lights reach what the raster light pass reaches.** Their falloff
  eases toward smoothstep by `r_dlightFalloff`, as `light_frag.tmpl` does; a
  shader without a lighting stage (`surfaceparm nodlight`, sky) takes none,
  as `tr_world.c` gives it none; with `rtx_rt_lighting 1` they are traced
  over a lightmapped surface's lightmap, where before such a surface took
  none; raster's surface-light proxies (`r_surfaceLightProxies`) are left to
  the traced surface lights they stand for.
- **Still frames start over when a frozen scene changes.** A model or dynamic
  light changed while the view and clock stand still (a light switched off, a
  weapon hidden) restarts the average, as a setting does; one that keeps
  changing is filtered as motion. The models' part of the scene hash is taken
  only on frames whose clock stood still (0.3 ms for 100,000 vertices).
  `r_dlightFalloff` and `r_dlightScale` restart it as settings.
- **RTCW lights whose target is missing stay dark.** id's q3map and q3map2
  keep a light whose `target` names no entity as a point light (both
  compilers' `CreateEntityLights`). RTCW's own compiler lit nothing with it:
  on the four retail maps where such a light can be told apart from its
  neighbours (`mp_assault`, `mp_depot`, `mp_keep`, `mp_trenchtoast`), the
  luxels beside all 29 measurable ones hold 9 to 40 above the ambient where
  the point light would add 70 to 245 and every other light 0 to 13. The same
  measure finds `q3dm11`'s such lights lit (118 to 255 against 106 to 239),
  as id's source keeps them. The tracer skips them in a Wolfenstein map
  q3map2 did not compile and says how many (86 on `mp_depot`, 15 on
  `mp_trenchtoast`).
- **The global fog reaches traced pixels.** RTCW's and Enemy Territory's
  global fog (`fogvars`, and the cgame's fog commands), like Alice's far
  plane, is part of each raster surface's own draw (`tr_rtcw_fog_vk.h`,
  `gen_frag.tmpl`), which the trace replaces. The trace now takes the same
  draw's colour and parameters (`RT_ViewFog`), finds each owned pixel's
  visibility by the raster's own equation (`rtcw_fog.glsl`, shared) at the
  traced surface's distance from the eye, and the composite blends toward
  the fog colour after the light is resolved, so the fog never enters the
  still-frame average. A `nofog` material stays clear unless a stage turns
  fog back on.
- **A map q3map2 stripped of its lights is lit from its lightmaps.** q3map2
  lights from a map's light entities and leaves them out of the BSP unless
  `-keeplights`: every Quake Live and Enemy Territory retail map arrives with
  none, and its lightmaps alone hold their light. On such a map
  (`rtx_rt_lighting 0`, the default) the world is lit from its lightmaps, as
  `rtx_rt_lighting 1` lights it, with dynamic lights traced over them;
  `rtx_rt_lighting 2` traces what remains, as `0` did before. The load says
  which (`q3map2 kept none of the map's light entities`).
- **Lightmap mode lights models as raster does.** A model's traced light
  in that mode was the lights that remained, missing the stripped ones; it
  now takes the light the raster pass gave its vertices from the light grid
  (`RB_CalcDiffuseColor`: the entity's ambient, and its directed light by
  each normal, clamped at 255), which the host stamps into each model
  vertex's unused fourth position lane and the shaders interpolate. Where
  the per-pixel light pass draws the dynamic lights, they are traced over it;
  in the legacy modes, which fold them into the entity's light, they are
  not added twice.
- **Models and movers keep their alpha tests in the map lights' shadow
  rays.** Those rays took every surface as solid, as the compilers took the
  world. The compilers never saw a model or mover, so the trace now takes
  their alpha-tested holes as holes: the compute backend alpha-tests the
  moving instances on a solid ray, and the pipeline traces them in a second,
  non-opaque ray after the world's opaque one.
- **`_farplanedist` no longer marks a Wolfenstein map as q3map2's.** RTCW's
  `mp_tram` sets it beside Wolfenstein's own `fogclip` and keeps its 828 light
  entities, which q3map2 would have left out; it now lights by id's rules,
  whose backsplash differs from q3map2's (11,222 lights, not 11,188).
- **`rtx_rt_debug_light` shows one light alone.** `rtx_rt_lightinfo` numbers
  the lights; `rtx_rt_debug_light N` lights the view with light N through
  the light sample's own shadow and filter rays, and
  `rtx_rt_debug_light_mode` shows it unshadowed, whether its shadow rays get
  through (their blocker's instance class in colour, in the compute
  backend), or what the light filters pass. A second traversal in the
  compute shader, tried first, left the 32-bit driver unable to create the
  pipeline.
- **The compute backend frees the world BVH's host copy** once uploaded (the
  frames' model trees reuse its arrays at their own size), for the 32-bit
  client's address space; see the address-space row below.

### Defects found and fixed

| Finding | Evidence | Fix |
| --- | --- | --- |
| An RTCW or ET map's portal sky took the frame's one trace, leaving its main view raster | `escape1`: the tracer's debug view never showed and the traced and raster captures were byte-identical; `rtx_rt_debug_as_stats 2` with `developer 1` logged the full-screen main view "left to raster ... traced this frame=1". 18 of the 41 RTCW retail maps probed have a portal sky (`props_skyportal`); none of the six ET maps does | The portal sky is left to raster; the main view is traced from the post-bloom pass it left open (`renderPass=3` in the log once the portal sky stopped taking the trace), whose depth is now stored when tracing is available |
| RTCW's `negdlightshader` light (its zombie spirits) was traced as a white light of radius 300 to 500, and `flamedlightshader` added light instead of multiplying | Source: `RT_UploadLights` sampled every scene light; `tr_rtcw_light_backend.h` draws these with their stages' blends; RTCW SP `cg_players.c` submits the zombie spirits' | They multiply a world surface's or brush model's result by the projected `$dlight` image per stage (`rt_material_light_factor`) |
| With `rtx_rt_lighting 1`, lightmapped world surfaces took no dynamic light | Demo `four` at frame 700 (x64, ray-tracing pipeline): the scene's lights add 2.164 of 255 in that mode and 2.165 with full tracing; before, the lightmap mode left the world out | The dynamic lights are sampled alone over the lightmap |
| Dynamic lights reached surfaces the raster light pass skips | Source: `tr_world.c` leaves a shader without a lighting stage (`nodlight`, sky) out of the pass | `RT_MF_NODLIGHT` receivers skip `RT_LIGHT_DYNAMIC` lights |
| The traced falloff was linear, raster's eased toward smoothstep | Source: `light_frag.tmpl` with `r_dlightFalloff 1`, the default | The same mix, from the cvar |
| Raster's surface-light proxies would have been traced over the surface lights they stand for | Source (`r_surfaceLightProxies 1`) | Skipped |
| A light switched off, or a model hidden, while the view and clock stood still stayed in the still-frame average | `crypt1`: switching `r_dynamiclight` off during accumulation kept the count rising (279, 379 still frames); now the average restarts and the frame reads 20.53 of 255, as `rtx_rt_dlight_scale 0`'s does | A hash of the frame's models and dynamic lights |
| `mp_depot`'s Cargo Bay traced 2.6 times as bright as raster (44.19 against 16.92 of 255); with no bounce and no specular, the tracer's direct light read 39.05 against its own lightmap-lit frame's 11.07 | The bay's side walls hold 9 to 25 in the lightmap where the untargeted `light` 300 beside each lamp would add 104 to 152 (`t867` to `t871` name no entity); the survey above | Skipped. The bay then read 13.65 against raster's 15.54 (0.88), and its direct light 11.44 against 11.05 lightmap-lit (1.035) |
| RTCW's and ET's global fog was missing from traced frames | `mp_trenchtoast`: raster's windows show the valley fogged white, the traced frame's show its hills clear; the tracer's own lightmap-lit frame read 25.06 against raster's 43.66 while its direct light (24.04) matched that lightmap, so the lighting was not the difference | The trace fogs each owned pixel as above; the next launch read 60.52 against 60.43 |
| Quake Live's and ET's traced worlds lacked the light of their light entities, which q3map2 left out of their BSPs | `goldrush`: the lamps' pools of light by the door and along the facade were missing, 29.65 against raster's 47.19; no Quake Live or ET retail map keeps a light entity; `battery`'s traced terrain also shaded in facets its lightmap does not have | The world is lit from its lightmaps, and models from the light grid: `goldrush` 0.938 of raster, the six maps 0.93-0.99; a frozen Quake Live demo frame 0.963 (0.817 before) |
| A screenshot the device had no memory for ended the game (`VK_ERROR_OUT_OF_DEVICE_MEMORY` from `vk_read_pixels`'s allocation, fatal) | The final sweep's `escape1`, traced for 198 still frames, died at its capture while another program held the GPU; RTCW's `dam` did the same in the third round's sweep | `vk_read_pixels` reports the failure; the screenshot or video frame is skipped with a warning, and `escape1` reran at 1.093 |
| The 32-bit client's address space ran out on Enemy Territory's `railgun` after two renderer restarts with tracing: the third load of the client game module failed | Raster: five loads, clean exit. Traced: virtual size peaked at 1,653 MB against raster's 1,424 MB (private 964 against 771 MB) in a 2 GB process; no growth across restarts | The compute backend frees the world tree's host copy after upload; the client is not large-address-aware (open below) |

### Corrections to the earlier rounds

- **Portal skies.** The second round's `escape1` row (1.029 of raster) and
  the third round's coverage figures for the 18 RTCW maps with a portal sky
  (`escape1`, `escape2`, `village1`, `village2`, `forest`, `dam`, `factory`,
  `baseout`, `sfm`, `castle`, `end`, `boss1`, `trainyard`, `mp_castle`,
  `mp_chateau`, `mp_dam`, `mp_keep`, `mp_village`) compared the raster main
  view with itself: only their portal sky was traced. `mp_ice` and ET's
  `oasis` have no portal sky and stand.
- **Enemy Territory's limbo panel.** `battery`, `fueldump`, `goldrush` and
  `railgun` read 1.0 because both captures showed the limbo panel: the
  probe's `openlimbomenu` toggles it, and it opened the panel on maps where it
  was closed. After a renderer restart the panel covers the view only on maps
  whose campaign briefing, when closed, becomes the panel (ET's
  `CG_EventHandling`): `oasis` and `radar` here. The probe now toggles it on
  those alone.
- **RTCW's torch lights.** The excess on `crypt1`, `boss1` and `church`
  (2.0-2.5 of raster) came from the probe: the raster capture followed a
  renderer restart, which reloads RTCW's client game with the clock stopped,
  and its light styles restart dark, so the raster frame had none of the
  `dlight` entities' light that the traced frame, captured before the
  restart, had. Both captures now follow a restart; the lights are compared
  separately below.
- **Quake Live's darker trace.** The second round put Quake Live's darker
  traced scene (0.826 on `campgrounds`) down to the radiosity bounces
  q3map2 compiled that one traced bounce does not replace. It was the light
  entities q3map2 left out of the BSP: the lightmaps hold their light, the
  trace had none of it.
- **Quake Live's maps.** Their live captures are its join-match menu over a
  spectator camera that moves between the two captures (Quake Live keeps
  `timescale` at 0.1 or more), so they show only that each map traces without
  error; the frozen demo below is Quake Live's comparison.

### Dynamic lights against raster

`crypt1`, shared x86 client, compute backend, the same frozen moment in
separate launches (no restart before either capture, so RTCW's torch
`dlight` entities are lit in both): raster with and without the module's
lights (`r_dynamiclight 1` and `0`) 42.01 and 19.55 of 255, a difference of
22.46; the tracer over the lightmaps (`rtx_rt_lighting 1`, no bounce, no
specular) with them and with `rtx_rt_dlight_scale 0` 37.26 and 16.18, a
difference of 21.08, 0.946 of raster's over the pixels the lights reach; full
tracing 39.81 and 20.53. The traced lights cast shadows raster's do not.

### Known differences

- **A closed door in a lit room.** `escape1`'s first cell reads darker
  traced than raster around the body by the door. `rtx_rt_debug_light 628`
  (the torch's spotlight, `light` 575) shows why: unshadowed it lights the
  floor and body as the lightmap does, its light filters (the cobweb, whose
  light image passes 99.6% of its texels) pass it, but its shadow rays stop
  at the cell's door, a `script_mover` (`cine_door`) standing closed between
  the torch and the cell. The door's brushes compiled around an origin brush
  sit at the world's origin in the BSP, so the compiler's light rays never
  met them where the door stands; the lightmap lights the cell as if the
  doorway were open. The trace shadows with the door where it is.
- **RTCW's vertex-lit terrain.** Terrain with `q3map_novertexshadows` and
  `q3map_forcesunlight` (`mp_dam`, `mp_beach`) was lit by the compiler at its
  vertices alone. The tracer lights it per point: on `mp_dam` the frame lit
  by the compile's vertex light reads 0.97 of raster, the traced direct
  light 1.40 times that vertex light in one view and 1.07 in another, and
  the bounce adds 9%. A least-squares fit of `mp_dam`'s terrain vertex
  colours gives the sun 0.76 to 0.82 of id's model per channel, the sky's
  light and the ambient as the model has them; the per-point sun reaches
  surfaces between vertices that the compiler sampled in shadow. The final
  sweep's `mp_dam` reads 1.50 of raster, and `mp_beach`, at night, 1.45 live
  and 1.54 in its demo.

### Content coverage

`sweep9` (`content_sweep.py` over `scripts/probe_rtx_games.py`): every retail
map probed in the earlier rounds, one launch each, with the compute backend
and `rtx_rt_require 1`; each capture follows its own renderer restart. Views
are each map's spawn, random in multiplayer, so ratios compare only within a
row. Five of Enemy Territory's six maps ran in the large-address-aware copy
of the client (LAA; see Remaining gates). The installations' inventories
were unchanged. The sweep ran on renderer `30ba97ba...`; the final build
below differs from it only in how a failed screenshot readback ends, and
reran `escape1`.

| Game | Traced | Traced / raster | Trace (720p) |
| --- | --- | --- | --- |
| Quake III | 10 of 10 | 0.87-1.27 | 16-74 ms |
| Quake Live | 12 of 12 | 0.95-1.07 (the join-match menu over a moving camera) | 2-4 ms |
| RTCW SP | 25 of 26 | 0.81-1.33 | 27-65 ms |
| RTCW MP | 15 of 15 | 0.97-1.50 | 36-70 ms |
| Enemy Territory | 6 of 6 | 0.93-0.99 | 4-7 ms |

- Quake III: `q3dm1` 0.979, `q3dm3` 0.866, `q3dm6` 1.183, `q3dm7` 1.242,
  `q3dm11` 1.067, `q3dm13` 1.235, `q3dm17` 1.116, `q3tourney2` 1.274,
  `q3tourney4` 0.935, `q3ctf1` 1.224.
- Quake Live: `campgrounds` 0.991, `bloodrun` 0.982, `aerowalk` 0.952,
  `almostlost` 0.994, `asylum` 1.067, `hektik` 0.991, `hellsgate` 0.996,
  `longestyard` 0.995, `furiousheights` 0.990, `cure` 0.976, `deepinside`
  0.994, `arkinholm` 0.993.
- RTCW SP: `escape1` 1.093 (final build), `escape2` 1.170, `tram` 0.989,
  `village1` 0.993, `village2` 1.332, `crypt1` 1.157, `crypt2` 1.091, `church`
  1.210, `forest` 0.986, `dam` 1.065, `factory` 1.064, `norway` stops in its
  game script ("can't find entity with targetname ice2_spot2"), in raster too,
  `rocket` 0.807, `xlabs` 1.100, `assault` 1.032, `baseout` 1.152, `sfm`
  0.942, `swf` 0.930, `chateau` 1.119, `castle` 1.060, `dig` 1.070, `dark`
  1.328, `end` 1.006, `boss1` 1.077, `boss2` 1.049, `trainyard` 1.048. In the
  sweep, `escape1` ran out of device memory allocating the screenshot readback
  while another program held the GPU, which ended the game; the final build
  skips such a screenshot with a warning.
- RTCW MP: `mp_assault` 1.015, `mp_base` 1.235, `mp_beach` 1.449, `mp_castle`
  1.027, `mp_chateau` 1.056, `mp_dam` 1.496, `mp_depot` 1.144,
  `mp_destruction` 0.969, `mp_ice` 1.188, `mp_keep` 1.095, `mp_rocket` 1.016,
  `mp_sub` 1.107, `mp_tram` 1.080, `mp_trenchtoast` 1.007, `mp_village` 1.187.
- Enemy Territory: `battery` 0.988 (LAA), `fueldump` 0.932 (LAA), `goldrush`
  0.938 (LAA), `oasis` 0.963 (LAA), `radar` 0.987, `railgun` 0.976 (LAA).

### Demos and validation

On the final build: Quake Live's demo (`fntech3-ql-probe.dm_91`, frozen after
30 frames, `--demo-advance 30`) traces at 0.963 of its raster frame (0.817
before its world was lit from the lightmaps), and RTCW MP's retail demo
(`retail-mp-probe.dm_60`, `mp_beach` at night, also frozen after 30 frames;
after 120 the demo had ended and both captures showed the main menu) at
1.542, its vertex-lit terrain as above. The x64 build with the Khronos
validation layer traced `q3dm1`, `q3dm7`, `q3dm17`, `q3tourney2`, `q3ctf1` and
`q3dm6` with the ray-tracing pipeline (0.98, 1.33, 1.18, 1.29, 1.19 and 1.23
of raster), and `q3dm17` with the compute backend, with no validation
message, native dispatch and exit code 0 each time; the two backends' frozen
frames differ by a mean of 0.0018 (`q3dm17`) and 0.0027 (`q3dm6`) of 255.

### Build identity

Source: `b68782bfe41eeff02097ec2b2eaeed3eb0a1b4db` plus the uncommitted working
tree, including other sessions' concurrent work. NVIDIA GeForce RTX 4060
Laptop GPU, driver 591.74 (Vulkan 1.4.325); the 32-bit driver offers no
ray-tracing pipeline, so the Win32 client traces with the compute backend.

| Artifact | SHA-256 |
| --- | --- |
| `.tmp/meson-q3-rtx-x64/fntech3-q3.exe` | `716c1eadfe337c93d7200a9232223e97747ef83a5184d3566a5ea617dea9e9b1` |
| `.tmp/meson-q3-rtx-x64/fnql_rtx_x86_64.dll` | `f50e9e35e76af5e8788a878c35d695cd3234587d116ef78fb084382875e2ad8f` |
| `meson/build/win32-dev/fntech3.exe` | `dcc9f117db4ffff49ce2c6bd86d1a3ae8186e6118fc2b1dadcffbbe82f85aeeb` |
| `meson/build/win32-dev/fnql_rtx_x86.dll` | `4e7b2dd38f51cd9ce8f5638adf6b62f2c76e43f365920db5aac3fe71c417b7a1` |
| `.tmp/runtime/rtx-laa/fntech3.exe` | `001cf01386ca2d714285f3f7f8011b349cf90ff39ebcb6dff4d318c11f9e8de9` |
| `.tmp/runtime/rtx-laa/fnql_rtx_x86.dll` | `4e7b2dd38f51cd9ce8f5638adf6b62f2c76e43f365920db5aac3fe71c417b7a1` |

The `.tmp/runtime/rtx-laa/` client is `meson/build/win32-dev/fntech3.exe`
(`dcc9f117...`) marked large-address-aware with `editbin
/LARGEADDRESSAWARE`, beside a copy of the same renderer. The demos,
`escape1`'s rerun and the x64 validation ran on these files; the coverage
sweep on renderer `30ba97ba...`, and the probes before it, quoted in the
tables above, on the builds between.

## Fifth round: sky and emitter lighting, linear light and Q2RTX's tone mapper

### What changed

- **The default lighting is Q2RTX's.** `rtx_rt_lighting 0` lights a map from
  the sky, its `q3map_sun` and the surfaces that give off light, as Q2RTX
  lights Quake II, with nothing baked and no light entity. A
  `q3map_surfacelight` shader is an area light of its authored strength; any
  other world shader gives off what its stages draw with the lighting term at
  zero (a glow stage over a lightmapped base, an additive flame), each
  stage's image averaged and folded through its blend as the GPU folds it; a
  shader drawn fullbright is lit by the scene, and only its bright texels
  glow (Q2RTX's synthesized emissive mask, `smoothstep(0.55, 0.85)` of the
  brightest channel on both sides), with `rtx_rt_emissive_light` (2) times
  what it shows. The sky lights evenly in its shader's light colour where it
  asks for light, capped at 0.75 lightmap units, else with its own images.
  The compilers' light models remain as `rtx_rt_lighting 2` and the
  lightmaps as `1`.
- **Light is traced in linear units.** With the default lighting, the
  material's reflectance and emission and the sky's images are decoded from
  the display's gamma (2.2) before they reflect or give off light
  (`RT_FRAME_LINEAR_LIGHT`; a scene-linear frame's textures are decoded
  already), an emitter's light is decoded the same way on the CPU, and the
  composite encodes once. The raster renderer shows light the way Quake III
  shows lightmaps, as a display value: light and display contrast are the
  same, so a room lit by bounced light alone is crushed toward black and a
  6:1 sun-to-shade ratio became about 50:1 once the tone mapper decoded the
  frame.
- **Q2RTX's tone mapper.** `rt_tonemap.comp` histograms each view's light
  (128 bins over 2^-24 to 2^8, weighted toward the view's middle as Q2RTX
  weighs it) and fits a curve to it after Eilertsen, Mantiuk and Unger
  ("Real-time noise-aware tone mapping", 2015: the slopes of their equation
  14 with the threshold of equation 17 iterated, blurred over 12 bins,
  light below 2^-12 treated as noise), within `rtx_rt_tonemap_range` (7)
  stops; the composite blends it (`rtx_rt_tonemap_curve`, 0.5) with
  Reinhard's curve behind an exposure metered on the 70th to 90th
  percentiles, eases channels past 0.6 toward a white point of 10, and
  encodes. Written for this renderer from Q2RTX's design and defaults
  (reference observation of Q2RTX's `tone_mapping_*.comp`, GPLv2+; no text
  incorporated). Two departures: the curve is fitted to the light a pixel
  receives (its diffuse light on a mid-grey surface, plus its specular light
  and emission), so textures keep their detail however hard the light is
  compressed - the paper's detail layer, which Q2RTX leaves out and notes
  that tone mapping the lighting would stand in for; and the exposure adapts
  only down to a metered luminance of 0.001, the curve darkened with it past
  that, so a windowless cell stays darker than a lit hall. Both adapt over
  real time as Q2RTX's do (`rtx_rt_exposure_speed`). A 1 KB copy of the
  tone mapper's state reaches the host each frame; `rtx_rt_perf_timing`
  prints the metered luminance and five points of the curve.
- **The compute backend checkerboards its bounce.** The diffuse bounce is
  traced for half the pixels a frame, in 8 x 8 tiles (one workgroup each)
  that alternate each frame and count twice (`rtx_rt_checkerboard`), as
  Q2RTX checkerboards its indirect light.
- **Quake Live's advertisements are traced.** The renderer draws a map's
  advertisement brush models (`*4` to `*9` on `campgrounds`) as world
  surfaces (`R_UpdateAdvertisements`); the tracer's world took brush model
  0 alone, so a traced view showed the wall behind each billboard. Their
  surfaces are now part of the traced world, and the `$lightmap` sheen their
  shader adds with `tcGen environment` is left to traced reflections, as any
  other environment-mapped stage is.
- **The pipelines are compiled at renderer start.** On the 32-bit client
  the driver compiles the compute backend's trace inside the client's 2 GB
  address space. Compiled at the first traced frame, after Quake Live's
  `campgrounds` had loaded, it failed in two runs of three ("NVVM compilation
  failed: 3", then "failed to create the compute path tracing pipeline");
  during that phase the largest free block was 29 to 52 MB. Compiled when
  the renderer starts, before the level loads, it failed in none of three.
- **A screenshot that was not written no longer says "Wrote".** The writers
  return whether the frame was read back.
- **Engine: an unloaded native module's address range is held.** See the
  Enemy Territory entries below and in the [ledger](COMPATIBILITY.md).

### Defects found and fixed

| Finding | Evidence | Fix |
| --- | --- | --- |
| Traced frames showed little ray-traced light: rooms black, windows and sunlit floors blown out | `mp_ice` metered at the 0.0002 floor with the room at the bottom of the curve; `q3dm1`'s sunlit floor read (255, 255, 255) where raster's reads (176, 90, 59), more than 6.3 stops above the metered band | Linear light, and the tone mapper fitted to the light rather than the surface (above). `mp_ice`'s room now shows the sun patch through its door and the bounce it lights; `q3dm1`'s floor keeps its texture |
| The tone buffer, copied for the host, lacked `TRANSFER_SRC` usage | x64 with the Khronos layer: 20 messages, "`vkCmdCopyBuffer(): srcBuffer ... requires VK_BUFFER_USAGE_TRANSFER_SRC_BIT`" | Added; the next run had none |
| Quake Live's billboards traced as the wall behind them | `rtx_rt_pick` along the view hit `pewter_shiney` at y = -53 behind the advertisement patch at y = -57 (brush model `*4`, surface 3889); `rtx_rt_debug_visualizer 1` showed a flat dark rectangle | Advertisements join the traced world |
| An environment-mapped `$lightmap` stage counted as a second full lighting term | The ad shader's third stage (`rgbGen wave sin .15 0 0 0`, `blendfunc add`) was taken as light 1.0 | The environment role is tested before the lightmap role |
| "Wrote screenshots/rt.png" printed for a frame not read back | `rtcw-mp`, fourth round: `vk_read_pixels` out of memory, then "Wrote" | The writers return `qboolean` |
| The 32-bit compute pipeline failed to compile after a large level loaded | Above | Compiled at renderer start |
| Enemy Territory's terrain traced as flat grey facets, shaded as if it stood on edge | `goldrush` (`setviewpos -3586 -171 -50 0`): `rtx_rt_pick` read `lmterrain_0to1`'s normal as (0.00, -0.07, -0.00) where its BSP holds (0, -0.062, 0.998); the BSP's 160,156 vertex normals are all unit length. `tr_bsp.c` smooths a face whose points share no plane into four floats a point (`GenerateNormals`); the tracer read them three apart | Read four apart; the ground traces as continuous cobblestone |

### Evidence

Probes ran windowed on a private desktop, in isolated homes under
`.tmp/rtx-look/` and `.tmp/rtx-games/`, with console commands alone.

- **Nine fixed views**, traced and raster (`final-*`, 1280 x 720, 270 frames
  after a traced `vid_restart` under `timescale 0`), with the tone mapper's
  metered luminance: `q3dm1` 0.147, a torch-lit `q3dm1` corridor 0.035,
  `q3dm7` 0.078, `q3dm17` 0.177, `goldrush` 0.062 and its arcade 0.021,
  `campgrounds` 0.026, `mp_ice`'s hut 0.0025, `escape1`'s cell at the 0.001
  limit. The torches light their walls, the hut its floor through its door,
  and the billboard shows its poster lit by the stage lights.
- **Five games on the shared Win32 client** (`probe_rtx_games.py`,
  `rtx_rt_require 1`, `.tmp/rtx-games/x86-tone-profiles/`): Quake III,
  Quake Live, RTCW SP, RTCW MP and Enemy Territory all passed with the
  compute backend and exit code 0; traced over raster 2.17, 1.59, 3.30, 2.13
  and 1.31 (the default lighting is not the lightmaps', so these ratios no
  longer measure agreement); trace 15.7, 76.2, 48.6, 64.3 and 37.4 ms.
- **x64 with the Khronos validation layer**, ray-tracing pipeline: `q3dm1`
  and `q3dm17` with no validation message and exit code 0 (trace 9.7 and
  5.9 ms, denoise 2.5 and 1.4 ms). The first run found the tone buffer's
  missing `TRANSFER_SRC` usage above.
- **Pipeline compile.** `campgrounds` with `rtx_rt_debug_visualizer 1`, three
  runs each: compiled at the first traced frame, two failed at pipeline
  creation; compiled at renderer start, none did. An address-space sampler
  on the 32-bit client saw private commit rise from 523 MB (raster) to 958
  MB while tracing, and the largest free block fall from 228 MB to 29 MB.
  Of the ~400 MB of driver images loaded, Intel's (`igc32.dll`,
  `igd10um32xe.dll` and others, about 150 MB) and NVIDIA's D3D driver
  (`nvwgf2um.dll`, 52 MB) stay loaded even with `VK_LOADER_DRIVERS_SELECT`
  naming NVIDIA's Vulkan driver alone: this laptop presents NVIDIA's frames
  through its Intel display adapter.
- **Tests**: `tests/rtx_emitter_lighting_tests.py` (11 tests: the stage fold,
  bright texels, the sky, the tone mapper's passes and buffer layout, a
  Python model of the curve on a room-and-window histogram, linear light,
  advertisements, face normals), `tests/rtx_renderer_contract_source_tests.py`,
  `tests/rtx_vulkan_safety_source_tests.py` and
  `tests/engine_input_capture_tests.py` (the screenshot report); every test
  file naming the RTX renderer passed in the x86 developer shell (766
  tests), and the full suite's failures (8 of 2,243, 2 on rerun) were in
  other sessions' concurrent work.

### Build identity

Source: `d1d98ba8fa23363355472eab5cb2b2860a857f40` plus the uncommitted
working tree, including other sessions' concurrent work. NVIDIA GeForce RTX
4060 Laptop GPU, driver 591.74.

| Artifact | SHA-256 |
| --- | --- |
| `.tmp/meson-q3-rtx-x64/fntech3-q3.exe` | `26891e812421dcd10f0015c09690ed5a45e74b851dda5fd9ad6ec7f624854618` |
| `.tmp/meson-q3-rtx-x64/fnql_rtx_x86_64.dll` | `fa53529d609ffe93fee7deb4ffc2789cb75230a3332ca7a68db2445967fcb989` |
| `meson/build/win32-dev/fntech3.exe` | `e655c33b5176180f12d4ada705a488d917cf0507fdf0be9f0f0d41590779affb` |
| `meson/build/win32-dev/fnql_rtx_x86.dll` | `e9d8701ab56698707221269c0693d874f0734818b30bc0db5d2ae0f54ce37766` |

The x64 files ran the final validation probes; the Win32 renderer the
`goldrush` terrain rerun. The nine views and the five games ran on the
renderer before the terrain normal fix, which changes only the smoothed
faces' normals.

## Remaining gates

- **The shared Win32 client's ray-tracing pipeline.** This machine's 32-bit
  NVIDIA driver offers no ray-tracing pipeline, so the Win32 client traces
  with the compute backend; the pipeline backend is the same code, run by the
  x64 build. A 32-bit driver that offers the pipeline would close this.
- **An x86 Vulkan validation-layer run.** The Vulkan SDK installed here
  (1.4.313) offers no 32-bit component, so a 32-bit Khronos layer would have
  to be downloaded or built from its source. The compute backend's API use
  is validated in the x64 build (`rtx_rt_backend 2`).
- **The 32-bit client's address space.** The shared Win32 client is not
  large-address-aware. Enemy Territory's client game module now reloads
  (the engine holds an unloaded module's range), and the path tracer's
  pipelines are compiled before a level loads, but a traced Quake Live or
  Enemy Territory level still leaves the 2 GB process with little free
  space (29 MB largest block on `campgrounds`), and this laptop's display
  drivers take about 200 MB of it. Marking the shipped client
  large-address-aware is an engine-wide decision, outside the renderer.
- **The driver fault at renderer teardown** described above.
- Enemy Territory demos now play (`2026-09-25-wolfet-demo-playback.md`);
  RTCW SP demos cannot be replayed by retail either. Neither is a renderer
  gate.
- Beyond what the id Tech 3 games drew, and so outside this renderer's
  contract: refraction, traced volumetrics, a replacement-material library,
  upscaling, and the compute backend's speed on the largest maps.
