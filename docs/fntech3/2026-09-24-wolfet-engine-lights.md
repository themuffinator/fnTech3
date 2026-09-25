# Enemy Territory's module lights beside the engine's own

## Scope

Enemy Territory scenes take the engine's own lights as every Quake III
profile does: the static map lights of a `maps/<map>.lights.json` sidecar,
surface-light proxies and `r_dlightTest`. Until now they took them on Quake
III's terms, and those terms broke two of ET's own light contracts. This
change gives ET the rules RTCW got in the
[RTCW engine scene lights record](2026-09-24-rtcw-engine-lights.md), through
the same code, and fixes a second defect found on the way: fnTech3 dropped
ET's forced lights at `r_dynamiclight 0`. The machine record is
[2026-09-24-wolfet-engine-lights.json](2026-09-24-wolfet-engine-lights.json).

The pinned original Enemy Territory source
([`40342a9`](https://github.com/id-Software/Enemy-Territory/tree/40342a9e3690cb5b627a433d4d5cbf30e3c57698))
was read for behaviour only, as reference observation. It is GPL-3.0-or-later
with additional terms, and no text from it is incorporated.

## What the original does

- **32 module lights a frame.** `RE_AddLightToScene` (`renderer/tr_scene.c`)
  refuses a light once the frame holds `MAX_DLIGHTS` (32), and the count runs
  across every scene the frame renders; it is reset once a frame. Its own
  refusals, in order, are a frame already holding 32, a light with no radius
  or no brightness, two old cards, and an unforced light while
  `r_dynamiclight` is 0. Every light past them is stored, and takes one of the
  32, whatever its flags say.
- **A sky portal is a scene of its own.** `CG_DrawActiveFrame`
  (`cgame/cg_view.c`) clears the scene, renders the sky portal
  (`CG_DrawSkyBoxPortal`, flagged `RDF_SKYBOXPORTAL`) and only then adds the
  entities, particles and local entities whose lights the main view carries.
  Multiview (`cgame/cg_multiview.c`) renders a further world scene for each
  view it shows. Scenes without a world (the limbo head, `CG_Draw3DModel`)
  follow the main view.
- **Forced lights survive `r_dynamiclight 0`.** `REF_FORCE_DLIGHT` bypasses
  the setting at submission, and `RE_RenderScene` clears a scene's lights only
  on `GLHW_PERMEDIA2`, not at `r_dynamiclight 0` (its own comment says why: to
  force lights such as lightning). The cgame's only forced light is a thrown
  charge's glow (`CG_DynamiteTrail`, `cgame/cg_weapons.c`); an unarmed
  charge's fades out over 15.5 s.
- **No light material is drawn.** A light's shader is stored, but the
  renderer only uses it to keep such a light off entities
  (`renderer/tr_light.c`), and every light adds to what is drawn. The cgame
  and UI pass no shader at all.

## What fnTech3 did, and why it mattered

1. `RE_AddWolfETLightToScene` refused at `r_numdlights >= 32`, counting every
   slot of the frame's light array: 64 under per-pixel lighting, holding the
   engine's lights and every portal view's copies as well as the modules'.
   Engine lights added to a sky portal or multiview scene, or copied into a
   portal view, could therefore refuse the module lights of a later scene
   that the original accepts. No stock ET map has a sky portal, but many
   community maps do.
2. The same count did not include the directed and junior lights fnTech3
   skips, although the original stores them. radar's nine lightning
   `dlight` entities are directed. A frame holding them accepted more module
   lights than the original.
3. `RE_RenderScene` cleared a scene's lights at `r_dynamiclight 0` for every
   profile but RTCW. ET's forced lights were accepted at submission and then
   thrown away, so the charge's glow vanished, which the
   [light record](2026-09-18-wolfet-light.md) said did not happen. The
   engine's own lights were promoted into those scenes too, only to be
   cleared.

## What ET does now

| Case | Module lights | Engine lights |
| --- | --- | --- |
| Any frame | The first 32 the original would store, counted on their own (`r_numModuleDlights`), whatever the engine adds | Only slots the modules can no longer claim, and a scene keeps within the 32 lights its surface masks can name |
| `r_dynamiclight 0` | Forced lights drawn, unforced ones refused, as in the original | None: they are not forced module lights |
| `r_dlightMode 0`, `1`, `2` | As above | Added in every mode: ET has no projected-light mode of its own, so mode 0 is Quake III's, which draws them |
| Portal or mirror view | Copied into free slots, as for any profile | Copied only within the engine's bound |

- The counter RTCW introduced, `r_numRTCWDlights`, is now
  `r_numModuleDlights` and serves both. `R_ReservesModuleLights()` names the
  three profiles whose modules own their 32. `R_EngineSceneLightEnd`, the
  portal copy (`tr_portal_plan.h`) and the RTX mirror copy apply the same
  `RTCW_EngineLightEnd` and `RTCW_PortalLightCopies` rules to them.
  `WolfET_SceneTakesEngineLights` is ET's predicate
  (`code/renderercommon/tr_rtcw_engine_lights.h`).
- `RE_AddWolfETLightToScene` now runs the original's refusals first, in its
  order, and counts every light past them. Lights fnTech3 cannot draw --
  directed and junior lights, and non-finite values the original's
  comparisons let through -- are counted and not stored. The slots they would
  have taken are never claimed by a module, so the engine may use them.
- **Order.** ET's lights keep the order they were added in. The original
  draws no light material and every light adds, so there is no darkening
  light for an engine light to escape, and nothing to reorder. RTCW's draw
  order (`RTCW_LightDrawIndex`) stays RTCW's alone.
- **`r_dynamiclight 0`.** RTCW's and ET's scenes keep their lights at 0.
  Only forced module lights can be there, since the rest are refused at
  submission and the engine adds none.

### What the module lights keep

Without portal or mirror views, ET's modules get exactly the original's first
32 lights a frame, whatever the engine adds. A portal or mirror view still
copies its parent's lights into slots of its own, a per-pixel feature the
original does not have, and module lights with their copies can fill the 64
slots. A module light is then refused that the original accepts; no light
that the rule before this change drew is refused unless the original refuses
it too.

## FnQ3/FnQL features (invariant 11)

ET uses the static map light sidecar and its promotion budget
(`r_staticLightMaxLights`, `r_staticLightShadows`), surface-light proxies with
their spot and point projections, `r_dlightTest`, and the per-pixel pass and
shadow atlas they feed, in every shared `r_dlightMode`, as it did before this
change. Left out, and why: engine lights at `r_dynamiclight 0`, where ET draws
only forced module lights. No retail-exact option was added or kept.

No ET map ships a light file. The six stock maps keep no `light` entity in
their entity lumps, where RTCW's keep hundreds, so a generator like RTCW's has
nothing to fit and would need another source.

## Evidence

Base commit `b68782b`. The shared tree carried other sessions' uncommitted
work, among it the RTCW engine scene lights this change generalizes, staged
but not committed. The before and after renderers were built from one export
of the staged index (tree `b0bd26f`), without and with this change's hunks,
and run from one executable. The RTCW session then re-staged its light
submission to count lights retail stores but fnTech3 does not draw, as ET's
now does; that touches only RTCW's submission, and the runs below predate it. The [JSON record](2026-09-24-wolfet-engine-lights.json)
holds the installation and pak hashes, every executable and renderer hash,
each run's command, counters and capture hashes, and every comparison.

### Executable tests

- `tests/wolfet_light_tests.py`: 3 tests and 5 subtests. The first runs the
  real `RE_AddWolfETLightToScene`, with and without per-pixel lighting: every
  profile but ET refused, the brightness scaling the colour, forced lights
  at `r_dynamiclight 0`, each of the original's refusals taking no slot and
  no count, the 32 counted apart from the frame's slots, directed, junior and
  non-finite lights counted but not stored, and each unsupported kind said
  once. The second runs 20,000 random frames of one to four scenes. Each has
  up to 40 submissions, some forced, directed, junior or without radius, up
  to 32 engine lights and, in a third of the frames, sibling or nested portal
  views. A quarter of the frames run at `r_dynamiclight 0`. In every frame
  without portal views the lights counted are exactly the original's first
  32, with or without the engine's; with them, a light the original accepts
  is refused only when module copies have filled the array, and no light the
  rule before this change drew is refused unless the original refuses it
  (5,259 such lights were past the original's 32). The third slices
  `R_ReservesModuleLights`, `R_EngineSceneLightEnd`, the `r_dynamiclight`
  clear and the portal copy out of all three renderers and runs them for six
  profiles.
- Nine deliberate regressions, applied one at a time, are each caught:
  counting every slot, directed lights taking no slot, ET reserving nothing,
  engine lights at `r_dynamiclight 0`, forced lights cleared at 0, unbounded
  portal copies, an unbounded RTX mirror, Vulkan ignoring the bound, and no
  32-light bound on a scene.
- `tests/rtcw_light_tests.py` now expects ET's bound where it expected the
  whole frame, and uses the shared counter. `scripts/probe_rtcw_lights.py`
  still passes all 22 comparisons with the original RTCW sources and iortcw.
- All 47 test files that read the changed sources pass in the export:
  308 tests and 22,051 subtests. Applied to the shared tree, 49 such files
  give 314 passes and one failure, `tests/alice_savegame_source_tests.py`,
  which reads another session's unfinished `SV_SpawnServer` in
  `code/server/sv_init.cpp`, a file this change does not touch.

### Runtime

`scripts/probe_wolfet_engine_lights.py`. fnTech3 does not replay ET demos
yet, so every ET run is a live listen server on `oasis`: windowed at 800 by
600 on a private desktop, from an isolated home, with `net_enabled 0`. An
engineer joins the Allies and spawns on sand. Every image is the
engine's `screenshotPNG`, and the installation's file inventory did not
change.

- **Engine lights, before and after, in GL, Vulkan and RTX.** Three fixture
  lights in front of the engineer change about 209,000 to 214,000 pixels when
  switched on, and switching them off and on again leaves 1,500 to 2,300,
  the live scene's own motion. At `r_dynamiclight 0` the frame is the same
  with them on or off, before and after. Before, the counters show all three
  promoted there and then cleared. After, they show them skipped as
  disabled. With 32 `r_dlightTest` lights filling the scene (GL and Vulkan),
  the before build still promoted the fixture past the scene's first 32,
  which are all a world surface takes: switching it off left 1,300 to 2,100
  pixels, the scene's motion. The after build skips it on budget, and the
  frames show the same difference.
- **A forced module light, after.** With the user's permission, one run threw
  one unarmed charge at the engineer's feet in GL. At `r_dynamiclight 0` its
  glow changes 290,107 pixels against the same view 20 s later, once the glow
  has faded; the charge itself is gone by then too, and outside the box it
  lay in 261,456 pixels are brightened by about 13, 19 and 11 levels. At
  `r_dynamiclight 0` and at 1 the frames with the glow differ in only 1,099
  pixels, mean 0.16 a channel, and the frames without it in 147. No throw
  was run on the before build. The first approved firing run, on battery,
  threw nothing: the server came up with the match running and the engineer
  never left limbo.
- **Quake III, Quake Live and RTCW MP, before and after.** The RTCW record's
  `mp_ice` and `q3dm17` demos, replayed with `timedemo 1` in eight settings
  in three renderers (23 replays each), give 46 of 46 byte-identical
  captures in each profile. A Quake Live `campgrounds` demo gives 12 of 12
  in three renderers, with `cg_shadows` 2 and 0 (`scripts/probe_shadow_finish.py`).

## What this does not claim

- No ET frame with a sky portal, a multiview or a portal view was run. No
  stock map has one, entity overrides (`sv_altEntDir`) are Quake Live's
  alone, and a demo carrying one cannot be replayed. The frame simulation
  holds those rules.
- The charge's glow was not captured in Vulkan or RTX, nor on the build
  before the change.
- No frame is compared with the original's: it has none of the engine's
  lights.

## Touched outside ET

RTCW's counter is renamed, and its rules are unchanged. `R_ReservesModuleLights()`
is true for RTCW exactly where `R_UsesRTCWLighting()` is, so its engine-light
bound, portal copies and `r_dynamiclight` handling are what they were. Every
other profile still keeps the whole frame array. `tests/q3_ihv_particles_tests.py`
and `scripts/probe_rtcw_lights.py` use the new counter name.

## Unrelated observations

- `demo <name>` without an extension, in any profile but Quake III, prints
  `couldn't open` followed by an uninitialised buffer when no demo is found:
  `CL_WalkDemoExt` returns without naming a path (`code/client/cl_main.cpp`).
- On `battery` the Allied engineer drowned about 20 s after spawning on the
  beach while looking down. Not investigated.
- On `battery`, but not `oasis`, the listen server restarted the level after
  `Gametype changed, clearing session data.` and came up with the match
  running rather than in warmup. Not investigated.
