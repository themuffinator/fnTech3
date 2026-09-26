# RTX Renderer Guide

RTX is fnTech3's Vulkan ray-tracing renderer. It is one of the three renderer
modules shipped with fnTech3 and does not change networking, demos, game logic, or
the retail module ABI.

The module has two ways to draw a frame. By default (`rtx_rt_mode 0`) it is a
complete Vulkan raster renderer. With `rtx_rt_mode 2` it path traces the main
view: the world and models are traced, lit by the sky and by the surfaces
that give off light as Q2RTX lights Quake II, and drawn with their shader
materials. Loading the module never turns ray tracing on by
itself, and hardware that can trace neither way keeps the complete raster
frame unless you ask for strict ray tracing.

## Requirements

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
Q3-only diagnostic build ([BUILD.md](../BUILD.md)) traces with either. The
[Q2RTX feature comparison](fntech3/RTX_Q2RTX_PARITY.md) records the verified
scope and remaining gaps, and the [compatibility ledger](fntech3/COMPATIBILITY.md)
the gate status.

RTCW and Enemy Territory remain behind their development build options. The
path tracer has drawn their retail maps in isolated development probes
(`docs/fntech3/2026-09-24-rtx-path-tracer.md`); those probes do not make
either game's renderer gate complete.

The `vkinfo` command reports the selected device and capability result. Use
`rtx_caps_report 1` for a compact RTX capability summary or `rtx_caps_report 2`
for the detailed report.

## Selecting RTX

Use a build that includes the `fnql_rtx_<arch>` renderer module, then enter:

```cfg
seta cl_renderer "rtx"
seta rtx_rt_mode "2"
seta rtx_rt_require "0"
vid_restart
```

`rtx_rt_mode` controls the requested path:

- `0` loads the RTX module but uses its complete raster renderer. This is the
  safe default.
- `1` requests ray-query capabilities while retaining raster output. This is a
  development mode rather than normal play.
- `2` path traces the main view.

Keep `rtx_rt_require 0` for normal use. If native RT cannot initialize or
complete a frame, the renderer reports the reason and keeps the raster result.
Setting it to `1` is a strict validation mode: unsupported capabilities or a
failed RT frame stop the renderer instead of silently falling back.

`rtx_rt_backend` chooses how the path tracer traces: `0` (the default) the
ray-tracing pipeline where the driver has one and compute shaders otherwise,
`1` only the ray-tracing pipeline, `2` only compute. It is latched; apply it
with `vid_restart`.

The RTX renderer defaults to the linear-light pipeline (`r_hdr 1`); use the
SDR path (`r_hdr 0`) when comparing its images with a retail engine's.

## What the Path Tracer Draws

Each main view is drawn in three steps. The raster renderer first draws the
surfaces the tracer owns, for their depth. The tracer then shoots a ray per
pixel, shades what it hits and writes those pixels over the raster frame.
Finally the raster renderer draws everything the tracer does not own on top,
in Quake III's usual order.

The tracer owns:

- the world's opaque and alpha-tested surfaces, including curved patches;
- models (MD3, MDR, IQM, and RTCW's and ET's MDC, MDS and skeletal meshes) and
  brush models such as doors and platforms;
- the first-person weapon. Your own third-person body is traced but never
  seen directly: it casts shadows and shows in reflections.

The raster pass keeps everything else: blended, additive and decal surfaces,
particles, beams, rails, sprites, flares, marks, liquids, deformed geometry,
the sky box and portal surfaces, the HUD and menus. Portal and mirror views,
stereo eyes, cube captures, views covering less than half the screen and the
portal sky that RTCW and Enemy Territory draw behind a map's main view are
rasterized completely. A main view smaller than the screen (Enemy Territory
insets its view by a pixel) is traced into its own rectangle.

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

**Lighting.** By default the tracer lights every map the way Q2RTX lights
Quake II: from the sky and from the surfaces that give off light, with traced
shadows and bounce light, and nothing baked. No lightmap and no light entity
takes part. Light is traced in linear units, as Q2RTX traces it: the colours
the shaders and the sky show are decoded from the display's gamma (2.2), so a
surface reflects and gives off light in proportion to what it shows, and the
view is encoded for the display once, after the camera.

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
- **The sky.** Where the sky shader asks for light (`q3map_surfacelight`, or
  q3map2's `q3map_skyLight`), the sky lights the scene evenly in that light's
  colour, as the compilers did, at no more than a lightmap could hold; a sky
  that asks for nothing lights with its own images. The sky shader's
  `q3map_sun` is the sun, softened over q3map2's `q3map_sunExt` deviance.
- **The camera.** Light is traced in physical proportion, so a room lit
  through its window is far darker than the sunlit snow outside it. Each view
  is tone mapped as Q2RTX maps its own: a histogram of the view's light gives
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

A map whose light came from light entities with no visible fixture (many of
RTCW's rooms) is lit only by what reaches it from the sky and the emitters
around it: a room with windows by the daylight they let in and its bounce,
a windowless cell dimly. `rtx_rt_lighting 1` lights the world from its
lightmaps instead and traces only dynamic lights over them, with models lit
from the light grid as the raster pass lights them; `rtx_rt_lighting 2`
traces the map compilers' own lights with their light models: the light
entities (point, linear and spot, with id's q3map, q3map2's or the Wolfenstein
compilers' falloff, angle and spawnflag rules), surface lights with their
backsplash, the sun, q3map2's sky-light dome and the worldspawn ambient. A
change of mode rebuilds the lights at once.

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

**Noise.** Each pixel takes one sample of each light source (the emitters,
the sun and the sky) and one bounce ray a frame, and one light sample at the
bounce. On the compute backend the bounce is traced in a checkerboard of 8 x 8
tiles that alternates each frame, as Q2RTX traces its indirect light
(`rtx_rt_checkerboard`). A temporal filter and an edge-aware spatial filter remove the noise;
when the view and the game clock stand still (a paused demo, a photo) samples
keep accumulating into a clean image. Changing a lighting setting, or a model
or dynamic light changing while the clock stands still (a light switched off,
a weapon hidden), starts the accumulation over; one that keeps changing (a
flickering powerup glow) is filtered as a moving scene is. Moving models,
players and brush models carry their own motion into the temporal filter, so
their history follows them instead of smearing; a surface newly in view starts
afresh.

## Settings

| Cvar | Default | Effect |
| --- | --- | --- |
| `rtx_rt_mode` | `0` | `2` path traces the main view. Latched: `vid_restart`. |
| `rtx_rt_require` | `0` | `1` fails instead of falling back to raster. Latched. |
| `rtx_rt_backend` | `0` | `0` the ray-tracing pipeline where available, else compute; `1` pipeline only; `2` compute only. Latched. |
| `rtx_rt_material_maps` | `1` | Use a texture pack's normal and material maps. Latched. |
| `rtx_rt_lighting` | `0` | `0` lights the scene from the sky and the surfaces that give off light, as Q2RTX does; `1` uses the lightmaps for static light and traces dynamic lights over them; `2` traces the map compilers' own lights and light models. |
| `rtx_rt_emissive_light` | `2.0` | Light a glowing shader without `q3map_surfacelight` gives off, as a multiple of what it shows; `0` lights nothing with them. |
| `rtx_rt_exposure` | `0` | `0` exposes and tone maps each view as Q2RTX does; any other value is a fixed exposure, with an ACES filmic curve. |
| `rtx_rt_exposure_bias` | `0` | Stops added to the automatic exposure and tone curve. |
| `rtx_rt_exposure_speed` | `1` | How fast exposure and curve adapt: toward a darker view at this rate a second, a brighter one at twice it, as Q2RTX's; `0` at once. |
| `rtx_rt_tonemap_range` | `7` | The display's range, in stops, that the tone curve fits a view's light into (Q2RTX's `tm_dyn_range_stops`). |
| `rtx_rt_tonemap_curve` | `0.5` | The fitted curve's share; the rest is Reinhard's curve behind the metered exposure (Q2RTX's `tm_reinhard` is one minus this). |
| `rtx_rt_checkerboard` | `0` | The diffuse bounce, traced as Q2RTX traces its indirect light: half the screen a frame in alternating 8 x 8 tiles. `0` on the compute backend only, `1` always, `2` never. |
| `rtx_rt_bounces` | `1` | Diffuse bounces per path, `0` to `4`; a reflected surface takes one fewer. Maps whose q3map2 build baked radiosity (`-bounce`) come closer with `2` or more. |
| `rtx_rt_indirect_strength` | `1.0` | Scale of bounce light; `0` keeps direct light only. |
| `rtx_rt_specular` | `1` | Specular highlights and traced reflections. |
| `rtx_rt_mis` | `1` | Glossy reflections share the map's surface lights with light sampling, so mirrors show the lights; `0` leaves them to light sampling. |
| `rtx_rt_sun_intensity` | `1.0` | Scale of `q3map_sun`. |
| `rtx_rt_patch_shadows` | `1` | Curved patches shadow the map's lights, as with `-patchshadows`; `0` for maps compiled without it. |
| `rtx_rt_sky_intensity` | `1.0` | Scale of the sky's light and of the sky reflections and bounces see. |
| `rtx_rt_emissive_scale` | `1.0` | Scale of glowing material stages. |
| `rtx_rt_dlight_scale` | `1.0` | Scale of dynamic lights; `1` matches the raster light pass. |
| `rtx_rt_denoise` | `1` | The temporal and spatial filters. |
| `rtx_rt_denoise_iterations` | `4` | Spatial filter passes, `0` to `6`; each doubles the filter's reach. |
| `rtx_rt_accumulate` | `1` | Keep averaging samples while the view, the game time and the scene's models and dynamic lights stand still. |
| `rtx_rt_iqm_skinning` | `1` | IQM models: `0` bind pose, `1` interpolated skinning, `2` current frame only. |

## Troubleshooting

If RTX cannot initialize:

1. Run `vkinfo` and `rtx_caps_report 2` and note the first failed capability.
2. Update the Vulkan GPU driver and make sure the `fnql_rtx_<arch>` module is
   beside the executable.
3. Return to Vulkan raster with `cl_renderer vk` followed by `vid_restart`, or
   to GLx with `cl_renderer glx` followed by `vid_restart`.
4. Leave `rtx_rt_require 0` unless you are deliberately testing strict native
   RT activation.

If the tracer runs but a view stays raster, the console says why once per
session ("RTX RT: ... the frame is not traced"), and `rtx_rt_debug_as_stats 2`
with `developer 1` names the state that left a view to raster (its render
pass, viewport and flags).

The 32-bit Windows client has 2 GB of address space, and the path tracer adds
about 200 MB to a large map's share of it. The engine keeps a native game
module's address range reserved while a renderer restart allocates, so Enemy
Territory's 57 MB client module loads back where it was even on its largest
maps.

For developers, `rtx_rt_debug_visualizer` shows the tracer's intermediate
images: `1` reflectance, `2` shading normal, `3` denoised light, `4` raw
light, `5` specular, `6` emission, `7` temporal history, `8` view depth and
`9` motion. `rtx_rt_perf_timing 1` prints GPU and CPU timings every
`rtx_rt_perf_interval` frames, with the number of still frames averaged so
far. With a map loaded, `rtx_rt_pick` names the
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

## Related Documentation

- [Display Guide](DISPLAY.md) for shared renderer and presentation settings.
- [RTX Renderer Contract](fnql/RTX_RENDERER.md) for architecture, known limits,
  build details, and maintainer validation.
- [Build Guide](../BUILD.md) for modular and static renderer builds.
