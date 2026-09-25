# RTCW and the engine's own scene lights

## Scope

Every profile but RTCW took three kinds of light the engine adds to a scene
itself: the static map lights of a `maps/<map>.lights.json` sidecar
(`r_staticLights`, on by default), the surface-light proxies promoted from
`q3map_surfacelight` world surfaces (`r_surfaceLightProxies`, off by
default) and the `r_dlightTest` debug lights. RTCW SP and MP now take all
three, in GL, Vulkan and RTX raster, under the rules below. The machine
record is [2026-09-24-rtcw-engine-lights.json](2026-09-24-rtcw-engine-lights.json).

## Why RTCW was left out

The three calls in `RE_RenderScene` were wrapped in `if ( !R_UsesRTCWLighting() )`
by the RTCW foundation commit (`a80f240`), which gave no reason, and no record
names one. Nothing in the three features is foreign to RTCW: its maps are Quake
III BSP 47 with baked lightmaps, its shaders carry `q3map_surfacelight`
(522 of 3,776 shaders in the SP and MP paks, among them 56 flame and fire
materials), and ordinary RTCW lights already take the shared per-pixel pass
and shadow atlas ([dynamic-light record](2026-09-17-rtcw-dlight-shadows.md)).
Reading RTCW's light contracts found five places where the engine's lights,
added as they are for Quake III, would have disturbed them. Each now has a
rule; none makes the features inapplicable.

1. **The 32 lights a frame belong to the modules.** RTCW's
   `RE_AddLightToScene` refuses a light once the frame holds `MAX_DLIGHTS`
   (32), counted across every scene the frame renders
   ([SP](https://github.com/id-Software/RTCW-SP/blob/master/src/renderer/tr_scene.c),
   [MP](https://github.com/id-Software/RTCW-MP/blob/master/src/renderer/tr_scene.c)).
   fnTech3 kept that as `r_numdlights >= 32` over the frame's whole light
   array, which holds 64 under per-pixel lighting. The engine adds its lights
   when a scene is rendered, after that scene's module lights, and RTCW's
   cgame renders its sky portal as a scene of its own before the main view
   submits any light (`CG_DrawSkyBoxPortal` runs in `CG_DrawActiveFrame`
   before the entities are added, `cg_view.c`); the UI's player preview adds
   two more in a later scene (`ui_players.c`). Sky-portal lights would have
   taken the main view's slots. A portal or mirror view's copies of the scene's
   lights already did: fnTech3 gives each view its own light set, retail does
   not.
2. **RTCW's light materials multiply what is drawn.** `negdlightshader`
   blends `GL_ZERO, GL_ONE_MINUS_SRC_COLOR` and darkens the frame beneath it;
   `flamedlightshader` blends `GL_DST_COLOR, GL_ONE` three times and
   brightens it (`scripts/gfx.shader` and `scripts/particle.shader` in both
   games' paks). The per-light passes run in the view's light order, so an
   engine light appended after a darkening light would have escaped it. The
   published module source submits the darkening light for SP's zombie
   spirits (`cg_players.c`, overdraw 10, two sites) and never passes the
   flame material's 11.
3. **`r_dynamiclight` means something else in RTCW.** At 0 and 2 it keeps
   only forced module lights (`RTCW_LightParameters`), so the renderer does
   not clear an RTCW scene's lights at 0 as it does every other profile's,
   and the Quake III rule that switched the engine's lights off with it did
   not apply.
4. **`r_dlightMode 0` is RTCW's original projection.** In mode 0 every RTCW
   light is drawn by `RB_RTCWProjectLights`: a planar projection of retail's
   dlight image with a vertical band, which has no spot form and no
   per-pixel falloff.
5. **The SP snooper scope.** Its heat view draws no light on the world but
   still adds every non-material light to the models it lights.

The original RTCW SP and MP sources were read for behaviour only, as reference
observation; no implementation text from them is incorporated. The shader
blends were read from the installation's own scripts.

## What RTCW does now

| Case | Engine lights |
| --- | --- |
| `r_dlightMode 1` or `2`, `r_dynamiclight` other than 0 or 2 | Added, and drawn by the shared per-pixel pass with the shadow atlas, as in every other profile. |
| `r_dlightMode 0` | None. Mode 0 stays the documented original projected-light option. |
| `r_dynamiclight 0` or `2` | None: the engine's lights are not forced module lights. |
| Snooper view | None. |
| Scene without a world | None, as for every profile. |

- **Slots.** RTCW's module lights are counted on their own,
  `r_numModuleDlights` (`r_numRTCWDlights` until Enemy Territory took the
  same rule), and `RE_AddRTCWLightToScene` refuses at 32 of those
  rather than at 32 slots of any kind. A light is counted where retail stores
  it, so one fnTech3 cannot draw still takes its slot: the infinite-origin
  light of retail MP's empty player preview, and one the per-pixel path
  refuses (a radius beyond the world, no gain). The engine's lights, and a portal
  view's copies of them, take only slots beyond those the modules may still
  claim (`RTCW_EngineLightEnd`), and keep a scene within the 32 lights its
  surface masks can name. The engine therefore never holds more than 32 of
  the frame's 64 slots.
- **Order.** A view's per-light pass draws the engine's lights first and the
  module lights after them in submission order (`RTCW_LightDrawIndex`), so a
  darkening or flame material applies over the engine's light as it applies
  over the lightmap. The array keeps the module lights first, so a portal
  copy that runs short drops the engine's.
- `code/renderercommon/tr_rtcw_engine_lights.h` holds the four rules;
  `R_EngineSceneLightEnd` in each renderer's `tr_scene.c` applies them.

### What the module lights keep

Without portal views, the modules' lights are accepted exactly as retail
accepts them, the first 32 a frame in submission order, whatever the engine
adds. A portal or mirror view still copies its parent's lights into slots of
its own, a per-pixel feature retail does not have. Those copies no longer
count against the modules' 32: before this change a mirror's copies could
refuse a later scene's lights while fewer than 32 had been submitted. What
remains is that module lights and their copies together can fill the 64
slots, and slots the engine took earlier in the frame are then not free. A
module light is still accepted whenever the rule before this change would
have accepted it.

### Radius

The engine's lights go through the same dynamic-light entry as every
profile's, so their radius is scaled by `r_dlightScale`. In RTCW SP that is
retail's own cvar, default 1.0, which also scales SP's module lights; a light
file's radius is therefore drawn unscaled in SP, where MP and the other
profiles halve it by default.

## FnQ3/FnQL features (invariant 11)

RTCW now uses the static map light sidecar and its promotion budget
(`r_staticLightMaxLights`, `r_staticLightShadows`), surface-light proxies with
their spot and point projections, `r_dlightTest`, and the per-pixel pass and
shadow atlas they feed. Left out, and why: mode 0, the documented original
projection, which cannot draw them; `r_dynamiclight` 0 and 2 and the snooper
view, whose RTCW meanings exclude an unforced light. Native RTX ray tracing is
not validated for RTCW, as before. No retail-exact option was added or kept
beyond mode 0.

No RTCW map ships a light file yet. RTCW maps keep their compiled `light`
entities in the entity lump -- 571 in `escape1` and 203 in `mp_ice`, with 94
and 112 `lightJunior` grid-only lights -- so one could be generated as
Alice's are; that generator is open work. It would have to leave out lights
the cgame already submits: `escape1`'s 18 `dlight` entities are separate
entities of their own class, and its flame surfaces carry `q3map_surfacelight`,
so surface-light proxies there add to the cgame's animated torch lights just
as they add to the lightmap in every profile.

## Evidence

Base commit `b68782b`, on a shared tree carrying other sessions' uncommitted
work. The [JSON record](2026-09-24-rtcw-engine-lights.json) holds the
installation and pak hashes, every executable and renderer hash, the fixture
lights, each run's command, counters and capture hashes, and every image
comparison.

### Executable tests

- `tests/rtcw_light_tests.py`: 9 tests and 19 subtests pass. Two are new. One
  runs 20,000 random frames of one to four scenes, each with up to 40 module
  lights submitted through the real `RE_AddRTCWLightToScene`, up to 32 engine
  lights and, in a third of the frames, sibling or nested portal views. In
  every frame the engine holds at most 32 slots and no scene more than 32
  lights; without portal views the modules' accepted lights equal retail's
  first 32 exactly and do not change when the engine's are removed; with
  them, no module light is refused that the rule before this change accepted,
  and a portal view's copies of module lights take any free slot. The other
  slices `R_EngineSceneLightEnd` and `RB_LightingPass` out of each of the
  three renderers and runs them: every other profile keeps the whole frame
  array and index order; RTCW gets the bound and the order above.
- Seven deliberate regressions, applied one at a time to a copy of the tree,
  are each caught: counting every slot against the 32, dropping the module
  reserve, unbounded portal copies of engine lights, module lights drawn
  first, the snooper view or mode 0 taking engine lights, and Vulkan ignoring
  the bound.
- `scripts/probe_rtcw_lights.py` passes all 22 comparisons with the original
  SP and MP sources and iortcw. Its light submissions, now 72,000 a mode and
  half of them at an infinite origin, leave the module count equal to the
  original's in every case; a light at an infinite origin is counted but
  never published.
- A clean export of `b68782b` plus this change alone built all 1,351 targets
  with warnings as errors and passed 167 of 169 C++ tests (two skip for want
  of an audio loopback device and a retail factory probe). Its full Python
  suite passed 1,999 tests and failed 12, every one because a fixture
  prelude in `tests/q3_ihv_particles_tests.py`, which four test files share,
  slices `R_InitNextFrame` without declaring the new counter. The prelude now
  declares it: the five files that use it then pass (19 tests), and so do
  156 documentation, packaging and light tests rerun on the final export.
- In the shared tree, 12 RTX ray-tracing and IHV ray tests fail, and fail
  identically with this change's hunks removed from the same tree: they are
  another session's work in progress, not this change's.

### Runtime

`scripts/probe_rtcw_engine_lights.py`. Every run is windowed at 800 by 600 on
an unswitched private desktop with no input, from an isolated home, and every
image is the engine's `screenshotPNG`. The installations were only read and
their file inventories did not change. The fixture lights sit in each
isolated home, and a light file the engine package ships for the same map
would outrank them: every one of the 95 runs below reported the fixture's
three lights, from a package with no RTCW light file, and the probe now
fails a run that loaded any other file.

- **RTCW MP, shared build.** One `mp_ice` demo, replayed with `timedemo 1` 23
  times (GL, Vulkan, RTX; eight settings, `r_dlightTest` not on RTX). With
  the fixture file the renderers promote all 3 lights, 1 of them shadowed, on
  every one of 90 frames, and the frame changes in about 277,500 pixels in
  each renderer; the default 4 of `mp_ice`'s 85 surface-light proxies change
  about 424,000; `r_dlightTest`'s four lights 54,000 to 70,000. With
  `r_dlightMode 0`, or with `r_dynamiclight 0`, a run with the file and the
  proxies on is byte-identical to one without them, in all three renderers.
- **RTCW MP, before and after.** One snapshot of the tree was built twice,
  with and without this change, and both renderer sets ran the same demo
  from one executable. Before, the light file, the proxies and the test
  lights change nothing: every capture is byte-identical to the run without
  them, and the counters never run. After, they are drawn as above, and with
  them off each capture is byte-identical to the one before the change, in
  all three renderers: RTCW's own lights are drawn exactly as they were.
- **Quake III, before and after.** One `q3dm17` demo replayed 23 times on each
  runtime: all 46 captures in every setting are byte-identical before and
  after, while the same settings change Quake III's frames as they always
  have: static lights about 160,000 pixels, and about 196,000 in mode 0,
  where Quake III draws them too.
- **RTCW SP.** SP records a demo only with `g_synchronousClients 1`, so one
  `escape1` client per renderer showed the lights on, off and on again. The
  counters give 52 frames with 3 lights promoted and 22 with none; switching
  them off darkens about 374,000 pixels by 48, 71 and 45 levels a channel,
  while the two captures with them on differ by 6,000 to 17,000 pixels of
  the live scene's own motion.

Two probe details: the counter line printed each frame reports that frame's
last scene, which in Quake III is the HUD head, a scene without a world, so
its lines read `skip disabled` although its frames show the lights; and RTX's
proxy skip counts read about twice GL's and Vulkan's for the same map (170
disabled of 85 built), an accounting difference that predates this change and
was not investigated.

## What this does not claim

- No frame is compared with retail: retail has none of these lights.
- No RTCW light file ships with this change; the runtime file is a probe
  fixture. Generated files for the official maps followed separately
  (`2026-09-24-rtcw-map-lights.md`); with them packaged, RTCW captures like
  these differ unless `r_staticLights` is 0.
- Beyond the start of `escape1`, the SP campaign was not run, nor the
  snooper scope or the zombie spirits' darkening light; their rules are held
  by the executable tests above.
- No RTCW scene with a portal or mirror view was run.
- Native RTX ray tracing is not exercised for RTCW.

## Touched outside RTCW

The three renderers' static-light, proxy and test-light functions take their
bound from `R_EngineSceneLightEnd`, which for every other profile is the
whole frame array they used before; the per-light pass order, the portal
copies and the frame counters are unchanged outside RTCW. `trRefdef_t` gains
`num_moduleDlights`, set for every scene and read only for RTCW. Enemy Territory
has since taken the same bound, counter and portal copies; see the
[ET engine scene lights record](2026-09-24-wolfet-engine-lights.md).
