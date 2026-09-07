# RTX Renderer Guide

RTX is fnTech3's Vulkan ray-tracing renderer. It is one of the three renderer
modules shipped with fnTech3 and does not change networking, demos, game logic, or
the retail module ABI.

fnTech3 starts RTX conservatively: loading the module does not automatically turn
on native ray tracing. Unsupported hardware retains a complete Vulkan raster
frame unless you explicitly request strict ray tracing.

## Requirements

Native ray tracing requires a Vulkan GPU and driver with buffer device address,
deferred host operations, acceleration structures, ray queries, and ray-tracing
pipelines. fnTech3 also validates the required feature bits, limits, formats,
descriptor capacity, and entry points before enabling the path.

Driver capabilities can differ between x86 and x64. The shared Q3/QL client
remains x86; an explicit Q3-only Windows x64 diagnostic build is documented in
[BUILD.md](../BUILD.md). It cannot run QL. The latest
[runtime evidence](fntech3/2026-09-06-rtx-round.md) and
[Q2RTX feature comparison](fntech3/RTX_Q2RTX_PARITY.md) record the verified scope
and remaining gaps.

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
- `2` requests the native ray-tracing pipeline for eligible primary-view world
  geometry.

Keep `rtx_rt_require 0` for normal use. If native RT cannot initialize or
complete a frame, FnQL reports the reason and preserves the raster result.
Setting it to `1` is a strict validation mode: unsupported capabilities or a
failed RT frame stop the renderer instead of silently falling back.

## Recommended Starting Point

FnQL keeps experimental and compatibility-sensitive presentation features
disabled by default. Start with:

```cfg
seta rtx_rt_mode "2"
seta rtx_rt_require "0"
seta rtx_rt_async_overlap "0"
seta rtx_rt_dynamic_blas "0"
seta rtx_rt_material_heuristics "0"
seta r_hdr "0"
seta r_globalFog "0"
seta r_liquid "0"
seta r_surfaceLightProxies "0"
```

Change optional HDR, fog, liquids, and other presentation settings separately
after confirming that the native renderer is stable on your GPU and driver.
Native RT does not silently enable them.

## What RTX Renders

For an eligible main view, native primary rays replace a conservative opaque
world subset, including static base-texture/lightmap pairs with independent
lightmap coordinates. The raster renderer remains responsible for advertisements,
entities, weapons, sky, portals, complex or animated shader stages, fogged and
translucent surfaces, liquids, particles, marks, flares, and UI. Secondary
views such as mirrors, portals, stereo eyes, partial viewports, and cubemap
faces deliberately use complete raster rendering.

This division preserves authored Quake Live content rather than approximating
unsupported shader behavior as ordinary opaque ray-traced materials. Dynamic
entities remain visible through the raster overlay when experimental dynamic
acceleration structures are disabled.

Native lighting now supports bounded scene-radiance transport. Set
`rtx_rt_indirect_bounce` from `0` to `4` to choose the maximum diffuse/combined
secondary path length. `rtx_rt_reflection_strength` controls GGX scene reflections;
enabled reflections can trace one continuation even when diffuse bounces are
zero. Misses use the analytic environment. Authored lightmaps remain a baked
lighting contribution, so this is an artistic hybrid with possible overlapping
light energy, not an unbiased reference path tracer.

Native shading also consumes baked vertex irradiance for the engine's collapsed
vertex-lightmap stages and implicit `LIGHTMAP_BY_VERTEX` world materials. This
keeps `r_vertexLight 1/2` usable without treating arbitrary authored vertex-color
shaders as ordinary lightmaps.

With `rtx_rt_dynamic_blas 1`, eligible MD3, IQM, MDR, and brush models participate
in secondary rays. MDR uses interpolated skinned triangles. Hidden local player
models can cast shadows; first-person/depth-hacked and no-shadow models are
excluded from this experimental caster path. Complex authored shaders retain
raster ownership. The path remains default-off pending broader runtime tests.

The [Q2RTX feature comparison](fntech3/RTX_Q2RTX_PARITY.md) records functionality
and outstanding work. Full Q2RTX parity is not established. In particular,
area-light sampling, PBR texture maps, physically lit fog, authored environment
transport, and an A-SVGF-class denoiser remain open.

For profile-aware probes use `scripts/rtx_runtime_smoke.py --game-profile q3`
with a legitimate Q3 installation or `--game-profile ql` with retail QL. Both
use isolated writable homes, disable input capture, and request engine screenshots.

## Troubleshooting

If RTX cannot initialize:

1. Run `vkinfo` and `rtx_caps_report 2` and note the first failed capability.
2. Update the Vulkan GPU driver and make sure the `fnql_rtx_<arch>` module is
   beside the FnQL executable.
3. Return to Vulkan raster with `cl_renderer vk` followed by `vid_restart`, or
   to GLx with `cl_renderer glx` followed by `vid_restart`.
4. Leave `rtx_rt_require 0` unless you are deliberately testing strict native
   RT activation.

Useful developer diagnostics include `rtx_debug_vk_validation 1`,
`rtx_debug_framegraph 1`, `rtx_rt_perf_timing 1`, and
`rtx_rt_debug_visualizer 1`. Keep them disabled during normal play.

## Related Documentation

- [Display Guide](DISPLAY.md) for shared renderer and presentation settings.
- [RTX Renderer Contract](fnql/RTX_RENDERER.md) for architecture, known limits,
  build details, and maintainer validation.
- [Build Guide](../BUILD.md) for modular and static renderer builds.
