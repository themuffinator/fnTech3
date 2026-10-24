# Static spot lights light only inside their cones

A map's light sidecar (`maps/<map>.lights.json`) gives each spot an origin,
a direction, a radius and cone angles. Until this change every renderer drew
a sidecar spot as a *segment light*: `R_AddStaticMapLightsToScene` passed
`RE_AddLinearLightToScene` the segment from the spot's origin along its
direction for its radius, and the per-pixel light passes lit a capsule round
that segment on every side. The cone angles reached only the spot shadows
(`r_spotShadows`, off by default). A narrow spot therefore lit surfaces far
outside its cone, and lit them from the side: each fragment took its light
direction from the segment's nearest point, so a spot pointing down at a
floor nearer than its radius lit the walls along its axis and hardly the
floor it faced.

The F.A.K.K.2 playability round of 2026-10-24 traced a black-fog passage on
`cemetery`, about 2.6 times as bright as retail's, to `cemetery-light-0117`,
a 15-degree spot whose capsule passed 1,073 units from the passage. This change
fixes the model for every profile with spot sidecars: Return to Castle
Wolfenstein (both modes), American McGee's Alice, Heavy Metal: F.A.K.K.2 and
Medal of Honor: Allied Assault. The [machine-readable
record](2026-10-24-static-spot-cones.json) holds the runtimes' hashes, every
run's command and every measurement.

Reproduce the checks and the shaders (the last needs the Vulkan SDK's
`glslangValidator`):

```powershell
python -m pytest -q tests/static_spot_cone_tests.py tests/dlight_face_bounds_tests.py
python code/rendererrtx/shaders/build_shaders.py --check
```

The runtime comparisons need legitimate installations and two runtimes
built from this commit and its parent; `.tmp/spot-cone/tools` holds the
harnesses (`fakk2_spot.py`, `rtcw_batch.sh`, `view_probe.py`) and the
measurements (`fakk2_measure.py`, `views_measure.py`, `collect.py`).

## The spot model

`code/renderercommon/tr_spot_cone.h` defines one term, evaluated the same way
by every per-pixel pass:

    cone = smoothstep( clamp( cos(theta) * scale + bias, 0, 1 ) )

where `theta` is a fragment's angle off the spot's axis seen from its origin,
and `scale` and `bias` come from the cosines of the inner and outer
half-angles (`R_SpotConeScaleBias`). The term is 1 inside `innerAngle`, eases
to 0 at `outerAngle` and is 0 beyond it; a fragment outside the outer angle
is discarded. The sidecar format's `innerAngle` defaults to 0, and no shipped
sidecar sets one, so every shipped spot fades from its axis to its outer
edge. The parser's clamps (outer 1 to 179 degrees, inner 0 to outer) are
repeated in `R_SpotConeCosines`; an inner angle equal to the outer one gives a
hard edge.

A spot keeps the segment's *distance* falloff: a fragment is lit by
`1 - d^2 / r^2` (eased by `r_dlightFalloff`) of its distance `d` to the
segment, `r` being the radius times `r_dlightScale`, so along its axis it
reaches the segment's length plus that radius. That is the reach the sidecar
generators fitted along each spot's axis
(`scripts/generate_stock_rtcw_lights.py` `drawn_falloff`), and it is
unchanged. What changes is the light's *direction*: diffuse and specular now
take the direction to the spot's origin, so a surface the spot faces is lit
by its cosine to the light, and the cone limits where it reaches.

Point lights, client-game lights, segment lights a client game adds through
`RE_AddLinearLightToScene` and the surface-light proxies' spot projection
have no cone and are drawn as before: their ARB programs and the point-light
shaders are unchanged, and the segment-light shaders take the new path only
for a spot (below).

## Where it is evaluated

- **GLx, ARB programs** (`code/renderer/tr_arb.c`). Four new fragment
  programs, `DLIGHT_SPOT_FRAGMENT` and its fog and two-sided variants, are the
  linear programs plus the cone term and the origin's light direction;
  `ARB_SetupLightParams` selects them for a spot and passes the cone's scale
  (divided by the segment's length) and bias in `program.local[11]`. Every
  other program's text is unchanged. The ARB path never shadows a segment
  light, so no shadow variant is needed.
- **GLx, GLSL program** (`GLX_LightingSetupProgram`,
  `code/rendererglx/glx_module.cpp`). The linear program's spot block runs
  when `u_DlightFactors.w` is 1, its cone's scale and bias in `.y` and `.z`;
  those three were always 0 and stay 0 for every other light. The VBO world
  path (`tr_vbo.c` `RB_StageIteratorVBODlight`) binds the same program.
- **Vulkan and RTX raster** (`VK_LightingPass`, `shaders/light_frag.tmpl`).
  The linear shader variants read a `spotCone` vector that shares the
  uniform slot of `csmModelX`, which only the CSM shaders read;
  `VK_SetLightParams` writes it for every lit batch, zero for every other
  light. The point-light SPIR-V is byte-identical to the base commit's; the
  two linear variants were recompiled with `glslangValidator` 15.2.0, which
  reproduces every committed light array byte for byte.
- **RTX path tracer** (`rt_backend.c` `RT_SpotDlight`, `rt_lighting.glsl`
  `rt_light_eval`). A spot used to be traced as a point light at its
  segment's midpoint, lit all round and shadow-traced from there, which for
  `cemetery-light-0117` is about 2,370 units down its axis from its lamp. It
  is now traced from its origin, with raster's distance to the segment and
  the cone. Every other
  dynamic light's record keeps `dir.w` 0, and its falloff is multiplied by
  exactly 1. `rt_trace_comp_spv` and `rt_main_rgen_spv` grew by 1,336 bytes
  each.
- **Front end** (`R_LightCullBounds`, all three renderers). A spot no longer
  reaches a surface, model or BSP node whose bounding sphere lies wholly
  outside its cone (`R_SpotConeCullBounds`), so such surfaces get no light
  pass at all. The test measures a signed distance to the cone that never
  exceeds the true one, so it only rejects what the cone term would leave
  black.

## Runtime evidence

Two client runtimes, `.tmp/runtime/spot-cone-before` (the base commit,
`1b11e9e8`) and `.tmp/runtime/spot-cone-after` (the base commit plus this
change), were built from one `git archive` export with `win32-dev`'s options;
only the three renderer libraries differ between them (their hashes are in
the JSON record). Every run was windowed at 800 by 600 on a private desktop
with an isolated home and no input, every image is the engine's own
`screenshotPNG`, and every installation's inventory was unchanged. Unless
said otherwise each run shoots its view with `r_staticLights 1` and then `0`,
so "the lights' share" below is the difference between those two frames of
one run. The tools are in `.tmp/spot-cone/tools`.

**F.A.K.K.2** (`fakk2_spot.py`, `fakk2_measure.py`). Each client joined the
after runtime's dedicated server (its executable is identical to before's)
on loopback. Retail `fakk2.exe` 1.02's frames of the same views are reused
from the playability round and from the light-law harness.

- `cemetery`, Julie placed in the black-fog passage at (3264, 128, 300) as the
  playability round placed her. View means below the HUD, and in brackets
  each frame's mean difference from retail's:

  | Frame | GL | Vulkan |
  | --- | --- | --- |
  | Retail | 2.90, 2.08, 1.44 | |
  | Before, lights on | 7.57, 4.37, 2.97 (2.94) | 7.59, 4.40, 3.03 (2.96) |
  | Before, lights off | 4.03, 2.37, 1.69 (0.82) | 4.02, 2.40, 1.67 (0.84) |
  | After, lights on | 4.06, 2.37, 1.69 (0.80) | 4.01, 2.40, 1.66 (0.84) |
  | After, lights off | 4.06, 2.37, 1.70 (0.81) | 4.01, 2.40, 1.67 (0.85) |

  With the change the static lights no longer reach the passage: on and off
  differ in 2,451 pixels (GL) where they differed in 148,522, and the view
  sits as close to retail's as it does with the lights off. GL's ARB
  programs (`r_glxDlightProgram 0`) give the same: 7.63, 4.38, 2.98 before,
  4.10, 2.38, 1.70 after, beside 4.09, 2.38, 1.70 off. The RTX path tracer
  had lit the passage less (on 1.97, 1.23, 0.68 against off 1.64, 1.14, 0.59,
  a share of 0.44); after, on and off are 1.66, 1.16, 0.60 and 1.69, 1.18,
  0.60 (0.08).
- `towncenter_evil`'s first view, by the light-law harness's regions,
  fnTech3 over retail (GL; Vulkan's, in the JSON record, differ from these by
  up to 0.15, most in the narrow arch region, which varies between frames
  with the lights off too):

  | Region | Before | After | Lights off |
  | --- | --- | --- | --- |
  | upper wall | 1.28 | 1.00 | 1.00 |
  | back wall | 1.55 | 1.00 | 1.00 |
  | arch | 1.27 | 0.90 | 0.97 |
  | door | 1.27 | 1.01 | 1.01 |
  | console | 1.59 | 0.99 | 0.99 |
  | floor left | 1.05 | 1.12 | 1.01 |
  | floor right | 1.09 | 1.23 | 1.00 |

  The room's lamps are spots aimed at the floor. Before, they lit the walls
  round them and left the floor; after, the walls are retail's and the floor
  under the lamps is lit. The lights' share fell from 367,895 changed pixels
  to 89,433 (GL; Vulkan 374,089 to 90,320). The ARB programs give the same
  lit frame (walls 1.00, floor 1.12 and 1.22); in that run the frame with
  the lights off came out about a tenth darker everywhere, a change of the
  scene between the two shots, not of the light pass. The path tracer draws
  this room about twice as bright as retail with or without the static
  lights; their share there fell from 4.79 to 2.82 (mean absolute
  difference).
- `over`'s first view is unchanged: its sidecar has 3 spots among 128 lights,
  and its tunnel walls stay brightened (right wall 1.41 of retail before and
  after, 1.12 with the lights off) by point lights.

**Return to Castle Wolfenstein** (`scripts/probe_rtcw_map_lights.py`).

- MP, `mp_castle`: one demo recorded on the before runtime and replayed with
  `timedemo 1` on both. With the lights off, every frame of both runtimes is
  identical byte for byte in GL, Vulkan and RTX. With them on (8 promoted a
  frame), their share fell from 74,735 and 78,038 changed pixels (frames 40
  and 80; mean 2.35 and 2.46) to 57,322 and 58,895 (0.79 and 0.80) in GL, and
  from about 76,300 and 79,800 to 57,600 and 59,300 in Vulkan and RTX.
- SP, `escape1`, live, in the corridor of three wall lamps
  (`escape1-light-0828`, `-0830`, `-0836`, 35.7-degree spots), facing along
  their beams and facing the wall they hang on. Lights-off frames are
  identical across the two runtimes in all three renderers, and so are the
  frames along the beams (no pixel changed by 6 or more). Facing the wall,
  12,508 pixels change in GL (6,758 in Vulkan and RTX), mean 0.10 to 0.15:
  the lamps no longer light the wall behind them. Point lights light most of
  that view.

**Medal of Honor** (`view_probe.py`). `dm/mohdm7` with `thereisnomonkey 1`,
the player teleported beside `mohdm7-light-0114`, a 60-degree lamp over a
doorway. Each GL and Vulkan run repeats itself exactly (its two lit frames
are identical), and their lights-off frames are identical across runtimes.
The lights' share fell from 102,913 changed pixels (mean 8.73) to 49,970
(3.01) in GL and from 100,689 to 51,829 in Vulkan. Over the 48,695 pixels
round and above the lamp that the old capsule lit, GL averaged 137.6 before
and 99.5 after, which is the lights-off level; over the 49,970 below the lamp
the cone now lights, 154.7 before and 138.5 after against 109.7 off. The path
tracer never lit the wall round the lamp (its stand-in point sat 94 units
below it); the cone below goes from 125.8 to 117.2 against 88.7 off (the
tracer's two lit frames of one run differ by 2.6 on average).

**Alice** (`view_probe.py`). Alice's views move: two lit frames of one run
differ in about 120,000 pixels. On `utemple`'s first view, whose eye is
1,707 units from `utemple-light-0006`'s segment, inside its old reach of
1,875 and 67 degrees off its 7.75-degree cone, over the pixels that
stay still in both runs, 1,113 (GL) and 1,391 (Vulkan) were lit by the
static lights before and are not after; they average 44.7 before, 39.6 after
and 39.3 with the lights off. On a view of `garden2` 418 units behind
`garden2-light-0145` nothing measurable changed: those lamps light nothing in
that view either way.

**Profiles without spot sidecars and lights without cones.** Quake III ships
no sidecar, so its frames must not change at all.

- `scripts/probe_shadow_finish.py --profile q3`: a `q3dm17` demo replayed on
  both runtimes, frames 40 and 80, `cg_shadows` 0 and 2: identical in GL,
  Vulkan and RTX.
- The retail `four.dm_68` (`pak8.pk3`) replayed with `timedemo 1`, its
  weapons' dynamic lights and `r_surfaceLightProxies 1` (whose surface-light
  proxies include segment lights; up to 4 were promoted): three frames,
  identical in all three renderers.
- The `q3dm17` demo with four engine test point lights (`r_dlightTest 4 320
  192 24 0`): its first frame identical in all three renderers.
- The path tracer is not repeatable to the bit: with the test lights, two
  after runs differ in 44,640 pixels (mean 0.039) and before and after in
  49,263 (mean 0.045).

## FnQ3/FnQL features (invariant 11)

The static map lights are the engine's own feature, drawn by the FnQ3/FnQL
per-pixel light pass with its smoothstep falloff and the shadow atlas. This
change adapts each profile's spot data -- its direction and cone, which
retail's compilers baked into the lightmaps -- into that pass instead of
discarding the cone. Nothing is reproduced from a retail renderer: retail
has no such lights. `r_staticLights 0` still returns every profile to its
baked lighting alone.

## Not covered

- Spot shadows (`r_spotShadows 1`) were not run; their projection already
  used the cone, and their slope bias now takes the origin's direction.
- The sidecars' spots have no `innerAngle`, so their light fades from the
  axis to the outer edge. Retail's compilers lit a spot fully to its cone's
  edge and softened only its last 32 units. A generator that wants that look
  can write `innerAngle`; none does yet.
- Surface-light proxies keep their segment form along a surface's normal
  (`r_surfaceLightProxies`, off by default); they are not sidecar spots.
- The path tracer's frames were compared by region and mean, not byte for
  byte, as it does not repeat itself exactly.

## Tests

`tests/static_spot_cone_tests.py`:

- compiles the header and checks the term (1 on and near the axis, 0 at and
  past the outer angle, monotone between, half lit half way in cosine, hard
  edges, cones past 90 degrees, the clamps and non-finite inputs) and that
  the cull is conservative: for 20,000 random cones from 1 to 179 degrees,
  every sampled point of every sphere it rejects lies outside the cone;
- checks `cemetery-light-0117`, read from the shipped sidecar: the front end
  culls the fog passage, no point of it on a 9 x 9 x 9 grid is lit, and the
  point 1,000 units down its axis is fully lit;
- compiles `ARB_BuildDlightFP` and builds every dynamic-light program: each
  spot program, with its cone lines, its origin direction and its one
  multiply taken out, equals its linear twin byte for byte, and no other
  program contains any of them;
- recompiles the four Vulkan light-pass variants from `light_frag.tmpl` and
  compares them with the embedded SPIR-V (skipped without
  `glslangValidator`; `rendererrtx`'s are covered by `build_shaders.py
  --check`);
- pins the GLSL, Vulkan, RTX raster and path-tracer spot paths and their
  guards, that only `R_AddStaticMapLightsToScene` marks a light as a spot,
  that both scene functions and RTCW's and ET's module-light helpers start a
  light with no cone, and that surface-light proxies stay segment lights.

`tests/dlight_face_bounds_tests.py`'s hand-written light record gained the
two fields `R_LightCullBounds` now reads.
