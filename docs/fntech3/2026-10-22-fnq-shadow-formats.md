# FnQ shadows for every model format, and the entity suns, 2026-10-22

A survey of FnQ feature coverage on 2026-10-22 found fnTech3's own shadows
missing from half the games. The point and spot shadows of dynamic lights
drew Quake III's, Return to Castle Wolfenstein's and the Raven formats'
models (MD3, MDC, MDS, MDR, IQM and brush models), but Enemy Territory's
skeletal meshes and the TIKI models of Alice, F.A.K.K.2 and Medal of Honor
never cast. The cascaded sun shadows (`r_csmShadows`) took only MD3, MDR,
IQM and brush models: no Wolfenstein character and no TIKI model cast or
received the sun. And those three TIKI games had no sun for the cascades at
all, which take theirs from a sky shader's `q3map_sun`; their levels name
the sun in their entities. This slice extends both shadow paths to every
model format in all three renderers (GLx, Vulkan, and RTX's raster path)
and gives Alice, F.A.K.K.2 and Medal of Honor a sun from their own data.
The [JSON companion](2026-10-22-fnq-shadow-formats.json) records the map
census, binaries, commands, captures and outcomes. No compatibility gate
closes.

Implementation is independently authored. The sun's keys and their meaning
come from the earlier records that read them from the retail executables'
model light ([Ritual model lights](2026-09-25-ritual-model-lights.md)) and
from 2015's game library, maps and lightmaps
([Medal of Honor's worldspawn sun](2026-10-16-mohaa-worldspawn-sun.md)).
Ritual's F.A.K.K.2 SDK FAQ, which describes `suncolor` and `sundirection`
as worldspawn keys, was read as reference observation only (non-free; no
text incorporated).

## The suns

### Observed

Every installed map's entity text and every shader script in its paks was
read (`.tmp/fnq-shadows/tools/sun_survey.py`; counts in the JSON):

| Game | Maps | `sundirection` | `suncolor` or `sunlight` | Shader `q3map_sun` a map uses |
| --- | --- | --- | --- | --- |
| Alice | 36 | 4 | 4 | none |
| F.A.K.K.2 | 30 | 20 | 20 | none |
| Medal of Honor 1.00 | 54 | 46 | 46 (`sunlight` on four) | none |

- **Alice.** `grounds1` ("224 192 192" toward -45 -26.9 0), `rchess1`,
  `wchess1` and `wchess2` (-60 200 0). Alice's `sky.shader` keeps
  `q3map_sun` in four shaders: `tempsky1` and `dtestsky` (sky parameters,
  no map names them), `chesssky` (no map names it either) and `tearssky`,
  which `potears1` and `potears2` name but which is no sky -- it has neither
  `surfaceparm sky` nor `skyParms` -- so fnTech3's parser keeps no sun from
  it. Three more are commented out. No installed Alice map has a sky sun.
  `sunflare`, which Alice's renderer also reads, is set on none of its maps.
- **F.A.K.K.2.** The twenty suns are colours of 11 to 56 toward pitches of
  -45 to -75, except `otto`'s "65.0 -22.5 0.0", 65 degrees *below* the
  horizon. Fifteen maps place a `sunflare`; twelve of them also name a sun,
  and `intro`, `oracle` and `oracleway` place the flare alone. No F.A.K.K.2
  shader has a `q3map_sun`.
- **Medal of Honor.** As the worldspawn-sun record found: every direction is
  above the horizon, every map with one has a colour, and no shader has a
  sun.
- **Their engines' reading.** `alice.exe` (0x004ac3b0) and `fakk2.exe`
  (0x0048e8d0) take `suncolor` and `sundirection` from anywhere in the
  entity text, the last one winning, and light a model from the sun when a
  trace toward `AngleVectors( sundirection )` ends on sky: the vector points
  toward the sun. Medal of Honor's direction does too (its lightmaps'
  brighter walls face it).

### What fnTech3 takes

- **Alice and F.A.K.K.2**: the keys as their model light reads them, through
  the same production reader (`RitualModelLight_Keys`).
- **Medal of Honor**: the worldspawn keys the renderer's sun flare already
  gathers (`R_MOHAASunKey`), read as RTX's path tracer reads them
  (`RT_ParseMOHAASun`): the colour 70 in each channel until one parses as
  three numbers, and no sun without a direction that does
  (`R_MOHAAWorldSun`). The flare's own reading is unchanged.
- **[I] Below the horizon is no sun to shadow.** A shadow map lit from
  beneath would shade every floor, so `otto`'s sun is refused, as are a
  colour without a direction and a black or non-finite sun. 19 F.A.K.K.2
  maps, 4 Alice maps and 46 Medal of Honor maps get a sun.
- **[I] A flare is no light.** `sunflare` is a point the lens flare is drawn
  at, not a light the compiler or the model light used, so the three
  F.A.K.K.2 maps that place only a flare get no sun.
- **Only the shadows' sun is set** (`tr.worldSun` and the sun parameters the
  cascade planner reads). `tr.sunDirection` stays the sky shader's: it
  places the sky's sun image and lights a model where no light grid reaches,
  such as F.A.K.K.2's HUD weapons, and the retail engines set it from
  `q3map_sun` alone. A level's entity sun replaces a sky shader's sun; no
  installed map of these three games has both.

`code/renderercommon/tr_entity_sun.h` does this after `R_LoadEntities` in
each renderer's `RE_LoadWorldMap` (`R_LoadEntitySun`); every other profile
keeps its sky shader's sun alone.

## The model formats

`code/renderercommon/tr_dlight_shadow_casters.h` and each renderer's
`tr_backend.c`, `tr_alice.c`, `tr_mohaa.c`, `tr_main.c` and `tr_local.h`, and
`code/renderercommon/tr_wolfet_add_surfaces.h`:

- **Dynamic-light casters.** The shared caster collection now reaches
  Enemy Territory's meshes (`MOD_WOLFET_MESH`, `SF_WOLFET`) and both TIKI
  kinds (`MOD_ALICE_TIKI`/`SF_ALICE_TIKI`, `MOD_MOHAA_TIKI`/`SF_MOHAA_TIKI`).
  `R_AddWolfETSurfaces`, `R_AddAliceTikiSurfaces` and
  `R_AddMOHAATikiSurfaces` take a caster context, as the MD3, MDR, IQM and
  brush adders do: with one, the entity's bounds are tested against the
  light rather than the view, an entity the view did not pose is posed, no
  lighting, fog or dynamic-light list is built, and each surface the view
  would draw goes to that light's shadow with the shader it is drawn with
  (a crossfade's second skin and a custom extra pass draw the same shape, so
  a surface casts once). The back ends' caster lists admit the same formats.
  Enemy Territory's and Alice's surfaces already drew a shadow face at full
  detail; a Medal of Honor model keeps the detail the main view measured, or
  whole when it measured none.
- **Entities the view would not draw cast nothing.** The collection skips a
  Ritual or Medal of Honor sky entity outside the sky portal and a
  portal-only entity outside a portal, as `R_AddEntitySurfaces` does.
- **Sun casters and receivers.** The cascades' entity and surface lists now
  admit MDS and MDC, Enemy Territory's meshes and both TIKI kinds, so those
  models cast into the cascades and take their shadows. A third-person
  player of these formats casts as an MD3 one does (`DSF_SHADOW_CASTER_ONLY`).
- **The cascades' cache.** Its signature reads an entity's frame numbers,
  origin and axes, which do not pose an MDS, an Enemy Territory mesh or a
  TIKI model, so a frame that shows one draws the atlas again
  (`RB_CSMShadowEntityPoseUnhashed`) rather than reusing a stale pose.

Invariant 11: this extends fnTech3's own shadow features -- the cascaded sun
shadows, the dynamic lights' point and spot shadow atlas and their filters
-- to each game's own models and its own sun. The games' original shadows
(`cg_shadows`) are unchanged and remain the retail-exact option. Not done
here: RTX's path tracer takes Alice's and F.A.K.K.2's sun from no entity
keys yet (it reads Medal of Honor's), and see-through and deformed surfaces
still cast no dynamic-light shadow, as for every format.

## Evidence

### Unit and source

- `tests/entity_sun_shadow_tests.py`: every shadow list in the three
  renderers admits the same eleven surface types and nine model types; each
  adder's caster path and the collection's sky-entity filter; the cache rule;
  the back ends' allow-lists compiled from each renderer's `tr_backend.c`;
  the shared entity-sun reader compiled with the production Ritual key
  reader (retail keys, a later entity's keys, colour or direction alone, a
  black, negative, non-finite or below-horizon sun, Medal of Honor's reader,
  other profiles, `tr.sunDirection` untouched); its call after
  `R_LoadEntities` in every renderer; and, with the installations present,
  every installed Alice, F.A.K.K.2 and Medal of Honor map through the
  production readers (4, 19 and 46 suns, each direction matching
  `AngleVectors` of its key).
- `tests/mohaa_sun_flare_tests.py` now also checks the shadows' reading of
  Medal of Honor's keys; `rtcw_light_tests.py`, `wolfet_draw_tests.py`,
  `q3_ihv_entity_renderer_tests.py`, `mohaa_permanent_decal_tests.py` and
  `bsp_profile_loader_tests.py` follow the new signatures and formats.

### Live

A `git archive` of `acad7f4c` was built for Windows x86 (debugoptimized,
strict warnings) as the *before* runtime; the same export with this change
gave the *after* runtime, whose engine executable and package are the same
files and whose three renderer libraries differ
(`.tmp/fnq-shadows/runtime/before` and `final`). Each run is a windowed
800x600 client on a private desktop with `net_enabled 0`, no sound, no
input, an empty home config and `r_csmShadows 1`, hosting the map with
`devmap` and placed by console steps (`.tmp/fnq-shadows/tools/batch.py`).
The engine's `screenshotPNG` took, a second apart, every shadow on, the sun
shadows' strength at 0, the dynamic lights' shadow strength at 0, and every
shadow on again; then `r_csmDebug` and `r_dlightShadowDebug` printed their
counters for a second. "Sun shadow" is the share of the frame more than 12
luma darker with the sun's shadows than without, among pixels the repeat
leaves within 4. The installations and binaries were unchanged after every
run.

| Game, map, view | Renderer | Sun, before | Sun, after | Sun-shadow receivers, before / after | Sun shadow, before / after |
| --- | --- | --- | --- | --- | --- |
| F.A.K.K.2 `fakkhouse`, Julie on the path, camera raised (`cg_cameraheight 60`, `cg_cameradist 200`) | GLx | none ("skip no-sky-sun") | entities, toward 0.39 0.16 0.91 | -- / 624 world, 112 model | 0.5% / 3.1% |
| the same | Vulkan | none | the same | -- / 624, 112 | 0.4% / 3.0% |
| the same | RTX, `rtx_rt_mode 0` | none | the same | -- / 624, 120 | 0.4% / 3.0% |
| Alice `grounds1`, after the opening (`god`, `notarget`) | GLx | none | entities, toward 0.63 -0.32 0.71 | -- / 875, 328 | 0.7% / 6.1% |
| Medal of Honor `m5l1a`, a spawned soldier on the street (`god`, `notarget`, `spawn`, `tele`, `face`) | GLx | none | worldspawn, toward 0.46 0.26 0.85 | -- / 1,609, 316 | 0.1% / 11.4% |
| the same | Vulkan | none | the same | -- / 1,609, 316 | 0.1% / 10.0% |
| the same | RTX, `rtx_rt_mode 0` | none | the same | -- / 1,609, 328 | 0.1% / 9.9% |
| Enemy Territory `oasis`, third person (`team b 2 8 7`, `cg_thirdPerson 1`) | GLx | the sky's | the sky's | 5,060, 80 / 5,056, 104 | 15.9% / 16.5% |
| Return to Castle Wolfenstein `forest`, third person (`give all`, `weapon 3`, `cg_thirdPerson 1`) | GLx | the sky's | the sky's | 104, 492 / 104, 576 | 69.5% / 72.2% |

- **The sun.** On every F.A.K.K.2, Alice and Medal of Honor view the before
  runtime planned no cascades and the after one planned four from the level's
  own keys; the small "before" shares are the frames' own motion. Julie,
  Alice and the soldier each cast a shadow on the ground beside their feet
  and take shadows on their shaded sides; the buildings' shadows cross the
  path and street. In Enemy Territory and Wolfenstein, whose sky already
  gave a sun, the soldier's mesh and BJ's MDS body join the casters and
  receivers (24 and 84 more receiving surfaces): the soldier casts onto the
  sand and BJ takes the trees' shadow.
- **The cache.** The cascades' atlas was redrawn each frame wherever a
  skeletal model was in view (one uncacheable signature a frame); `forest`'s
  frames had been cacheable before, though each missed the cache.
- **Dynamic lights.** On `grounds1` the opening's fires plan three shadowed
  lights. Before, their shadow maps drew 13 world surfaces from one light;
  after, 232 surfaces from two, the TIKI models' among them. No other view
  had a shadowed dynamic light in reach without input, so Enemy Territory's
  and Medal of Honor's dynamic-light casters have source and unit evidence
  only.
- **Probe stalls.** Two runs stopped before any capture and were run again
  under a new name: an Alice client (once with each build) that never loaded
  its client game after the map load restarted its renderer, and an Enemy
  Territory client that the server set to zombie right after its connect
  response. Both stalls happened before the first frame of the level, with
  the same engine executable on both sides.

The captures, masks and console logs are in `.tmp/fnq-shadows/runs/`
(`*-final-before`, `*-final-after`); the JSON lists their hashes.

## Unrelated observations

- The sun shadows darken what they cover by a fixed share -- the sun colour
  scaled to unit length, times `r_csmShadowStrength` -- however bright the
  sun is, on every profile. [I] On a dim sun such as F.A.K.K.2's `shield`
  ("11 12 18") that is deeper than the light the sun adds to its lightmaps.
- RTX's path tracer takes no sun from Alice's and F.A.K.K.2's entity keys
  (`RT_FindSun` reads sky shaders only; Medal of Honor has `RT_FindMOHAASun`),
  so those maps trace without a sun.
- `code/renderercommon/tr_mohaa_world.c` and `code/qcommon/cm_mohaa_lights.c`
  match Medal of Honor's sun keys without regard to case, where the renderer's
  flare and shadow reader and OpenMoHAA match them exactly; every installed
  map writes them in lower case.
- Alice's listen-server start sometimes stops after the map load's renderer
  restart (two of seven runs here), and an Enemy Territory local connect once
  went straight to zombie; see the probe stalls above.
