# MOHAA terrain and static scenery in RTX, 2026-10-11

RTX path tracing omitted MOHAA's generated terrain and static map scenery.
The omission is reproduced on `m1l1` at the user's view position
`(-3610 -309 486)`, yaw 73, with `rtx_rt_mode 2`. RTX now includes those
surfaces in its static scene. This corrects a shared map-loading path,
without a map-specific exception.

The [JSON companion](2026-10-11-mohaa-rtx-world.json) identifies the tested
build, fixtures, commands and local captures. Evidence is Source, Unit and
Live subset. MOHAA remains experimental and its renderer gate stays partial.

## Cause and correction

`R_MOHAABuildWorld` appends terrain, posed static TIKI scenery and permanent
decals after the original BSP surfaces, including the inline brush models.
Raster rendering reaches the appended surfaces through the leaf lists.
`RT_ExtractWorld` instead visited only model zero's original contiguous
range and Quake Live's separate advertisements. The surface owner treated
opaque generated triangles as traced, so the raster overlay did not repair
the omission.

The world builder now reports the first and actual final count of appended
surfaces. RTX retains that range and extracts it through its existing
material and geometry handling. Counting the final written surfaces avoids
including unused allocation slots when a static surface cannot be built.
Model zero's range remains unchanged: expanding it would bake intervening
movable doors into the static scene. Invalid generated ranges fail before
surface access; an extraction failure releases partial geometry.

Terrain still uses the variance masks shared with collision. On `m1l1`,
1,128 of the 2,304 possible triangles remain in its 18 patches; the other
triangles are intentionally removed by the map. The fix does not restore
those. Permanent decals retain their material's raster ownership. Static
scenery works even on maps without terrain. Other profiles have an empty
appended range, and Quake Live's advertisements keep their separate path.

The new records are engine-internal; no retail module or wire layout changes.
No external implementation text was incorporated.

## Presentation

Terrain and baked static scenery use fnTech3's existing RTX materials,
lighting, shadow and reflection paths, whether its tracing backend is
compute or hardware. Their map geometry, texture coordinates, lightmap
coordinates and vertex colors flow through the existing triangle extractor.
Permanent decals, sprites, effects and animated TIKI entities retain their
existing raster handling. This change does not add traced animated TIKI
models. GL and Vulkan retain their leaf-based world rendering. No feature
is disabled for compatibility and no retail-exact presentation mode is added.

## Validation

- Rebuilt GL, Vulkan and RTX with the existing Windows x86 `win32-dev`
  preset. Tests use the stable build paths, legitimate 1.00 US retail
  libraries and assets, and isolated writable homes under
  `.tmp/mohaa-terrain/`. The source tree contains pre-existing unrelated
  changes; this is not a clean release build.
- All 72 `mohaa*tests.py` and 120 `rtx*tests.py` checks passed on Windows
  with the x86 compiler environment. The terrain fixture was then extended
  to check a nonzero original BSP surface range and reset of a reused
  result record; all six terrain tests passed again.
- The new extraction fixture executes the production world and surface
  extraction functions, covering terrain, static-only ranges, an intervening
  inline door, material-owned decals and light filters, Quake Live ads,
  empty/malformed input and failed geometry appends. Using the pre-fix
  extraction makes its generated-geometry assertion fail.
- The extraction, terrain and permanent-decal fixtures also passed on
  Linux x86 under WSL: 17 tests. The builder fixtures compile the production
  builder and verify the generated range independently of RTX's traversal.
- A read-only inventory of all 54 retail BSPs finds 36 with terrain and
  46 with static models, including 10 with static models but no terrain.
  This establishes the affected data path's breadth, not a visual test of
  every map.

Engine screenshots at the reported `m1l1` view show the repaired ground in
the normals and reflectance views. The engine's integer `viewpos` output
is `(-3610 -309 486) : 72` after the script requests yaw 73. Before the fix,
`rtx_rt_pick -3610 -309 600 0 0 -1` finds no traced surface. Afterward it
hits `textures/algiers/grndset_2af` at Z 401.2, with normal
`(0.02, -0.18, 0.98)`. World geometry increases from 234,031 to 243,073
traced triangles. Additional mode-2 runs render `m1l3a`'s 265 terrain
patches and `m1l3c`'s static scenery without terrain.

GL, Vulkan and RTX raster (`rtx_rt_mode 0`) also completed the same `m1l1`
view probe. Their engine captures retain the expected ground and scenery.

All runtime tests are windowed on a private desktop, with mouse and
joystick input disabled, no injected input, and the engine's registered
`screenshot` command. The original test mission scripts only position the
player and finish loading; these are scene probes, not campaign-play tests.
Retail installations are used only as asset/module search paths.

## Limits and additional findings

The active tracing backend here is compute: the 32-bit NVIDIA driver does
not expose hardware ray-tracing support to this executable. Both backends
use the corrected extraction, but hardware tracing has no new live evidence.

At the default `r_subdivisions 1`, the baseline `m1l1` run fails to allocate
a BVH for one 318,847-triangle partition of the world and falls back to
raster. Lower texture resolution alone does not resolve it. The successful
before/after `m1l1` comparison uses `r_subdivisions 4` in both isolated
homes; this reduces Bezier curve tessellation, not MOHAA terrain detail.
`m1l3c` uses the same setting, while `m1l3a` succeeds at the default 1.
The allocation limit remains a separate issue; no user configuration or
engine default was changed to conceal it. (2026-10-28: it no longer applies;
`m1l1` builds one 533,088-triangle world tree at `r_subdivisions 1` and
traces its whole view, [light entities record](2026-10-28-mohaa-rtx-light-entities.md).)

An existing startup `Unknown command "fov"` diagnostic also remains.
The probes mute audio and do not establish audio fidelity, retail-engine
parity, full campaign coverage or full compatibility.

Automatic approval review rejected cleanup of disposable probe homes and
caches with "blocked by policy". They remain under `.tmp/mohaa-terrain/`,
alongside the useful reports, engine logs and captures; none is staged.
