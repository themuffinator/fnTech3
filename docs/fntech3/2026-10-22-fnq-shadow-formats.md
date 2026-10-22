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
- A `git archive` of the change's commit (`7a831c15`) built every target
  with strict warnings; its native tests passed (274, 2 skipped) and 607 of
  its 610 test files passed, the other three failing only when Windows held
  a just-run probe executable during temporary-folder cleanup and passing
  when run again.

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
- **Probe stalls.** Three runs stopped before any capture and were run again
  under a new name: two Alice clients (one with each build) and an Enemy
  Territory one, each set to zombie by the server right after its connect
  response, before the first frame of the level, with the same engine
  executable on both sides. The cause is found and fixed under Follow-ups.

The captures, masks and console logs are in `.tmp/fnq-shadows/runs/`
(`*-final-before`, `*-final-after`); the JSON lists their hashes.

## Follow-ups

This slice's first commit found four problems outside its scope. Three are
fixed here; the fourth, sun shadows that shaded baked light twice, was left
for a decision and is fixed under
[Sun shadows on baked light](#sun-shadows-on-baked-light).

### RTX's path tracer had no sun on Alice and F.A.K.K.2 maps

`RT_FindSun` reads only a sky shader's `q3map_sun`, and Medal of Honor alone
had its own reader (`RT_FindMOHAASun`), so every Alice and F.A.K.K.2 map
traced without a sun. `RT_FindRitualSun` (`code/rendererrtx/rt/rt_lights.c`)
now gives the tracer the entity sun under the two Ritual profiles, read with
the same `RitualModelLight_Keys`, refused below the horizon as the shadows
refuse it, and falls back to `RT_FindSun` for a level that names none. The
sun is given in lightmap bytes, `suncolor` as is:

- **Observed.** Opposite vertical walls of every Alice and F.A.K.K.2 map with
  a sun, measured as the Medal of Honor record measured them
  (`.tmp/fnq-shadows/tools/ritual_walls.py`): the sunward wall is brighter by
  1.0 to 1.3 times `suncolor` x cos on `fakkhouse` (1.05), `gruff` (1.05),
  `end` (1.01) and `homes1` (1.26), and by less (0.1 to 0.8) on the cliff,
  swamp and town maps and on Alice's four, whose lit walls saturate at
  `suncolor` 223.
- **[I]** The compiler stored the sun as `suncolor` times the cosine, as
  2015's did; the low pairs are sunward walls that stand in shade.
- **Live.** On `fakkhouse` under `cl_renderer rtx` (path traced, the view of
  the table above), the before runtime logged that the sky shader
  `textures/eden/sky/morningsky` has no `q3map_sun`; the after one logs "entity
  sun 56.0 56.0 50.0 toward 0.390 0.162 0.906". The sunlit path's mean sRGB
  rose from 36.9 25.8 13.7 to 119.8 88.7 52.0, where GLx's baked lightmaps
  give 100.7 73.1 43.1, and the tree and the buildings cast traced shadows
  (Julie, a TIKI model, is drawn the normal way and casts none in the
  trace).

### Medal of Honor's sun keys were matched without regard to case

`code/renderercommon/tr_mohaa_world.c` (the light grid's sun and ambient) and
`code/qcommon/cm_mohaa_lights.c` (the client game's foot-shadow lights) read
`suncolor`, `sunlight`, `sundirection` and `ambientlight` with `Q_stricmp`,
where 2015's renderer, as OpenMoHAA's `R_Sphere_InitLights` reads it, and
fnTech3's flare and shadow reader match them exactly. Both now match exactly.
No installed map writes them in another case, so no installed map changes.

### A local client was dropped after a long map load

The two stalled probe runs above were not stalls in the map load. In both
logs the server answered the client's connect and set the client to zombie
in the same frame, before the client's first packet; the client then sat
until its own 200-second timeout ("Server connection timed out.") and
restarted its renderer at the menu. The rule that drops a just-connected
client after four seconds of silence (`SV_CheckTimeouts`) measures
`svs.time`, which fnTech3 advances by real time; [I] the frame after a long
map load adds the whole load at once, so a client whose connect was stamped
just before read as four seconds silent. Alice's `grounds1` rebuilds its
path nodes in every fresh home, a long load. The rule now skips the local
(loopback) client, which answers from the same process; remote clients and
every other timeout keep it. With the fix Alice's local start connected in
6 runs of 6 (it had stalled in 2 of 7) and Enemy Territory's in 3 of 3.
`tests/rtcw_cast_lifecycle_tests.py` checks that a local client past the
four seconds is kept.

A `git archive` of these follow-ups (`150d6230`) built every target with
strict warnings; its native tests (274, 2 skipped) and all 610 test files
passed.

## Sun shadows on baked light

The follow-ups left one finding for a decision. The sun shadows darkened
whatever they covered by the sun colour scaled to unit length, times
`r_csmShadowStrength`, however much of a receiver's light the sun gave, on
every profile. On F.A.K.K.2's `shield`, whose start is a lamp-lit room under
a roof and whose sun is a faint "11 12 18", turning them on darkened 31 % of
the frame (mean luma 50.4 to 42.1) and tinted the room, which the sun does
not light. A world surface in the world's own shadow was shaded twice, once
in its lightmap and again by the cascades. The decision: world surfaces
take only the models' sun shadows, where the world does not already shade
them, scaled by how much of each surface's light is the sun's; models keep
every shadow; all three renderers, every game.

### What changed

- **Two rows of cascades.** The atlas is twice as tall
  (`CSM_ATLAS_LAYERS`, `R_CSMShadowAtlasLayout`; four 1024-texel cascades
  make a 4096 x 2048 depth texture, and the rows must fit the texture limit
  too). Each cascade draws the world entity's surfaces into the first row
  (`CSM_LAYER_WORLD`) and every other entity's, brush models among them,
  into the second (`RB_BuildCSMShadowView`'s `viewportY = layer *
  cascadeSize`, `RB_RenderCSMShadowCascade`). The compilers bake the world's
  own brushes, patches and static models into its lightmaps; a door or a
  lift moves, so it is a model here.
- **What a receiver takes** (`code/renderercommon/tr_csm_receivers.h`,
  shared by the three renderers): a model (any entity but a brush model), or
  a surface its shader lights from the light grid (`rgbGen
  lightingDiffuse`), takes either row's shadow by the sun colour as before
  (`CSM_RECEIVER_ALL`). A world or brush-model surface with a lightmap
  stage (`CSM_RECEIVER_LIGHTMAP`) or baked vertex colours (`rgbGen vertex`,
  `exactVertex`, Medal of Honor's map-colour lighting;
  `CSM_RECEIVER_VERTEX`) takes the models' row only where the world's row
  leaves it in the sun, per filter tap, and darkens by
  `r_csmShadowStrength` times the sun's share of its light, per channel:
  S x max(0, N . toSun) / L, held to 0..1. L is its lightmap texel (through
  the stage's own texture-coordinate path) or vertex colour; N its
  interpolated normal; toSun the sun's direction in its entity's frame. A
  surface lit by neither (full bright, a constant or entity colour) is not
  lit by the sun and takes nothing. The receiver pass batches by kind and
  lightmap page.
- **The sun in lightmap units** (`R_CSMSunLightmapColor`,
  `csmPlan_t.sunLightmapColor`): the level's sun light, shifted as
  `R_ColorShiftLightingBytes` shifts the lightmaps and over 255, with the
  colour at unit length for the profiles whose compiler stored it so (see
  below).
- **The passes.** GLx's ARB programs carry the normal, lightmap coordinates
  and vertex colour, sample both rows (eight taps, each held inside its
  row) and the lightmap on unit 0 (`ARB_CSMShadowPass`, local parameters 7
  to 9). Vulkan and RTX's raster path give the CSM pipeline the colour,
  second texture coordinate and normal inputs and the lightmap in descriptor
  set 2; `csm_shadow.vert`/`.frag` are rebuilt into both renderers' SPIR-V
  (RTX's reflection JSON was regenerated with it, which also brought its
  stale `liquid.frag` size up to date). The rows are mirrored between GL's
  bottom-left and Vulkan's top-left image origins as the producers' viewports
  are.
- **Diagnostics.** `r_csmDebug 1` adds `baked:N`, the baked receivers of
  the frame, after `recv world:W ent:E`. A tessellation restart keeps the
  normals a baked receiver asked for (GLx's `needsNormal`).

Invariant 11: this adapts each level's own baked light and sun into
fnTech3's cascaded shadows. No retail engine of these games drew sun shadow
maps, so there is no retail presentation to keep beside them.

### The sun's units

The share needs the sun in the units the compiler stored in the
lightmaps. The Medal of Honor and Ritual records measured theirs:
`suncolor` x cos, as given. For a sky shader's `q3map_sun` (or Raven's
`sun`), the step a shadow edge makes across one sun-facing lightmapped
surface measures it: each planar surface's unsaturated luxels split in two
by luma, both halves holding 15 % of them, and the step's colour within
0.995 (cosine) of the sun's hue (`.tmp/fnq-shadows/tools/ibsp_steps.py`,
read-only). Against the colour scaled to a brightest channel of 1, as
fnTech3's sky-sun parser keeps it, and against the colour at unit length,
over the maps whose sun has a colour (a white sun cannot tell the two
apart):

| Game | Coloured-sun maps | Surfaces | Step / S, brightest channel 1 | Step / S, unit length | Stored |
| --- | --- | --- | --- | --- | --- |
| Quake III Arena | 27 | 1,440 | 0.71 | 1.04 | unit length |
| Q3Test 1.08 | 2 | 95 | 0.71 | 1.07 | unit length |
| Elite Force | 3 | 229 | 0.65 | 0.95 | unit length |
| Jedi Academy | 7 | 695 | 0.63 | 0.98 | unit length |
| Jedi Outcast | 1 | 30 | 0.80 | 1.19 | unit length [I] |
| Return to Castle Wolfenstein | 35 | 3,608 | 0.88 | 1.19 | brightest channel 1 |
| Quake Live | 77 | 3,678 | 0.83 | 1.19 | brightest channel 1 |

- **[I]** id's q3map and Raven's scaled the sun colour to unit length,
  as the Jedi engines' own renderers do with the same line (the
  [Raven sun record](2026-10-15-raven-sun-keyword.md)); Quake Live's
  q3map2 and Wolfenstein's compiler kept a brightest channel of 1. Outcast's
  one coloured map does not decide between the two; it takes Raven's rule.
  Enemy Territory's maps keep their lightmaps in external images and were
  not measured; they were compiled with q3map2, whose rule they take. The
  IHV test's data has no sun.
- Raven's BSPs and Quake Live's record no lightmap rectangle on their
  surfaces, so their luxels were found from the vertices' lightmap
  coordinates.
- The steps read a little under the stored sun where a shadow's soft edge
  blurs the two halves, so the nearer rule is taken, not an exact factor.

### Evidence

- **Unit and source.** `tests/csm_receiver_tests.py` compiles the shared
  receiver rules against each renderer's own enums, for GLx's stage colour
  and the Vulkan renderers' bundle colour (models, lightmap stages before
  or after the diffuse one, white and non-lightmap bundles, vertex and
  map-colour lighting, the light grid, unlit colours, stage order, batch
  matching); compiles each renderer's atlas layout and sun units (rows,
  texture limits, shifts, unit length per profile, a colourless sun); and
  checks the casters' rows, the receiver passes, GLx's programs and
  parameters, the Vulkan inputs, descriptors and shaders, and that the
  embedded Vulkan SPIR-V is what `glslangValidator` builds from the GLSL.
  `tests/shadow_projection_source_tests.py` follows the two-row sampling
  and `tests/csm_shadow_cache_source_tests.py` the debug line.
- **Export.** A `git archive` of the change (`dfa14b7e`) built all 1,546
  targets with strict warnings. Its native tests failed one script,
  `csm_shadow_cache_source_tests.py`, which pytest does not collect and
  which still ended its check at the old debug line; `47e7f617` follows
  the line, and with it the native tests (274, 2 skipped) passed, as did
  all 611 test files, one pytest each (602; the nine script-style files
  collect nothing and pass when run directly).
- **Live.** The *before* runtime is the build of `150d6230` above
  (`.tmp/fnq-shadows/runtime/csm-before`); the *after* runtime is the same
  export with this change, whose engine executable and package are the same
  files and whose three renderer libraries differ (`runtime/csm-after`).
  Each of the 72 runs is a windowed 800x600 client on a private desktop, as
  in the table above, with `r_csmShadows 1`, placed by console steps only
  (`setviewpos` at the spawn point the map's own light grid shows most
  sunlit, facing the sun, `.tmp/fnq-shadows/tools/sun_spots.py`; `team`
  joins; `cg_thirdPerson 1`). "Sun shadow" is measured as above. Every run
  ended without an error, and the installations and binaries were unchanged.

| Game, map | GLx | Vulkan | RTX, `rtx_rt_mode 0` | Receivers after, GLx: world, model, baked |
| --- | --- | --- | --- | --- |
| Quake III `q3dm6` | 38.3% to 3.2% | 38.8% to 3.5% | 38.3% to 3.5% | 1,116, 52, 1,116 |
| Quake Live `hiddenfortress` (`team f`) | 17.2% to 2.4% | 17.3% to 2.6% | 17.6% to 2.4% | 1,149, 44, 1,149 |
| Elite Force Holomatch `ctf_and1` | 27.9% to 2.2% | 28.7% to 1.2% | 28.5% to 1.3% | 990, 48, 990 |
| Jedi Academy `mp/ffa2` (`team free`, `ui_openmenu none`) | 21.7% to 1.7% | 21.7% to 1.7% | 21.5% to 1.7% | 173, 80, 173 |
| Jedi Outcast `duel_jedi`, the spectator's view behind the player menu | 4.7% to 0.0% | 4.7% to 0.0% | 4.7% to 0.0% | 509, 16, 509 |
| Q3Test 1.08 `q3test2`, a random spawn each run | 0.6% to 0.6% | 0.6% to 0.8% | 0.7% to 0.7% | not printed |
| Enemy Territory `oasis` | 16.4% to 2.1% | 16.4% to 1.1% | 16.4% to 1.1% | 4,965, 104, 4,973 |
| Return to Castle Wolfenstein `forest` | 69.0% to 74.7% | 68.2% to 73.5% | 0.7% to 0.7% | 104, 576, 104 |
| F.A.K.K.2 `fakkhouse`, camera raised | 3.1% to 0.9% | 2.9% to 0.9% | 3.0% to 0.9% | 622, 112, 658 |
| F.A.K.K.2 `shield`, the lamp-lit start | 31.2% to 1.1% | 31.7% to 1.2% | 31.7% to 1.2% | 155, 60, 171 |
| Alice `grounds1` | 6.1% to 4.5% | 6.1% to 4.3% | 6.1% to 4.3% | 865, 328, 1,101 |
| Medal of Honor `m5l1a`, a soldier on the street | 11.3% to 0.9% | 9.9% to 1.0% | 9.9% to 0.9% | 1,609, 288, 1,753 |

- **The world's own shadows.** Before, every view's floors and walls
  darkened where the level's own walls and roofs stood between them and
  the sun, though their lightmaps held those shadows already; after, they
  do not. `shield`'s mean luma with the shadows on went from 42.1 to 50.0
  (50.4 without them); `q3dm6`'s, under a roof, from 28.4 to 40.5 (44.3).
- **The models' shadows.** After, what darkens is the models and the
  ground beside them: the player's or soldier's shadow on the sand, the
  street, the path and Alice's floor, at the depth of the sun's part of
  that ground's light. Models still take every shadow: a player under
  `q3dm6`'s roof is darkened by it.
- **Return to Castle Wolfenstein's forest got darker,** as it should: its
  trees are runtime game models (`misc_gamemodel`, 149 of them), so their
  shadows were never baked. Across 108 trees the terrain's vertex light
  is the same down-sun of the trunk as up-sun (median luma 92.4 and 93.2
  at 64 units, level at every distance to 384;
  `.tmp/fnq-shadows/tools/gamemodel_shadow_bake.py`), and the sun is nearly
  all of that ground's light, so a tree's shadow now takes nearly all of it
  where the fixed factor took about 60 %.
- **Counters.** `baked` counts brush-model surfaces too, so it can exceed
  `world`. Q3Test draws its status-bar head, a scene without the world,
  after the view, so `r_csmDebug` reports that scene ("skip no-world")
  though the view's shadows draw; the run's on/off difference shows them.
- **Views.** Q3Test's game has no `setviewpos` (it was sent as chat), so
  its two runs spawned in different places and compare only within each
  run. Jedi Outcast's multiplayer game has neither `setviewpos` nor a
  console way past its player menu, and its single player does not reach
  play in fnTech3 yet ("client import 3 is not implemented"), so its view
  is the spectator's, behind the menu. The IHV test has no sun.

### Limits

- **Models under a roof.** A model takes the roof's sun shadow although
  the light grid it is lit from already holds less sun there, as the
  decision asked; `q3dm6`'s player shows it.
- **What a compiler baked but the cascades call a model.** A brush model
  or game model whose shadow a compiler did bake would shade a baked
  surface twice. Wolfenstein's forest trees are not baked; the record has
  no evidence for other games' movers either way.
- **What the world row holds but a compiler did not bake** (id's q3map
  cast no shadows from patches unless told to): such a surface keeps the
  sun in its lightmap but takes no model shadow where the world row
  covers it.
- **Saturated lightmaps** cap L, so the share reads high there and is held
  to 1.

### Unrelated observations

- **RTX's raster path drew no sun shadows on Wolfenstein's `forest`**
  before or after this change: 4 world receivers and no model receivers a
  frame, where GLx and Vulkan have 104 and 576; Enemy Territory's `oasis`
  draws them under RTX.
- **RTX's path tracer scaled Wolfenstein's sky suns to unit length**
  (`RT_FindSun`), which keeps a brightest channel of 1 only for maps it
  knows q3map2 compiled. Fixed under
  [Sky suns in the path tracer](#sky-suns-in-the-path-tracer).

## Sky suns in the path tracer

`RT_FindSun` took a sky sun's colour at unit length unless it knew the map
for q3map2's, by its worldspawn keys or q3map2-only shader directives. So
Return to Castle Wolfenstein's suns, which its own compiler stored with a
brightest channel of 1 (see [The sun's units](#the-suns-units)), were
traced too dim by the colour's brightest channel over its length: 29 % for
`forest`'s "1 0.8 0.6". The raster's sun shadows had the opposite gap: they
took the profile alone, so a Quake III map that q3map2 compiled took unit
length.

Both now ask one rule (`code/renderercommon/tr_sky_sun_units.h`): unit
length for id's q3map and Raven's; a brightest channel of 1 for a map
q3map2 compiled and for the Quake Live, Wolfenstein and Enemy Territory
profiles. A map is q3map2's when its worldspawn carries a key only q3map2
writes or reads -- the keys RTX's light parser reads, checked equal by a
test (`R_SkySunEntitiesQ3map2`). The raster reads them once a level's
entities load, into `tr.worldSun.unitColour`; RTX keeps its own parse,
which also knows q3map2's shader directives, and passes its result to the
rule (`R_SkySunUnitLengthColour`). Entity suns stay as given. The sky suns
of Alice, F.A.K.K.2 and Medal of Honor, which no installed map has, now
take id's rule, as their compilers' ancestor; the raster took a brightest
channel for them before.

- **Observed, installed maps.** Through the production scan
  (`tests/sky_sun_units_tests.py`): 141 of Quake Live's 149 maps carry
  q3map2's keys, all 6 of Enemy Territory's, none of Wolfenstein's 50 (its
  one `_farplanedist` is Wolfenstein's own key), and 7 of the 45 in the
  Quake III installation, all in third-party paks.
- **Live, RTX path traced** (its default `rtx_rt_mode`), the views of the
  table above, before with the libraries of `dfa14b7e` and after with this
  change (`.tmp/fnq-shadows/runtime/sunits-after`): `forest`'s load log
  went from "q3map sun 91.9 73.5 55.2" to "130.0 104.0 78.0" (x 1.41), and
  its sunlit patches (pixels above 40 luma) from mean sRGB 85.4 83.0 74.3 to
  94.2 90.7 78.1, the exposure adapting. `oasis` ("q3map2 sun 135.0 126.0
  108.0"), `hiddenfortress` ("q3map2 sun 69.3 81.9 90.0") and `q3dm6` ("q3map
  sun 67.1 80.5 107.3") logged the same sun before and after, and their
  frames' means agree within 0.5. Every run ended without an error; the
  installations and binaries were unchanged.
- **Tests.** `tests/sky_sun_units_tests.py` compiles the rule and the scan
  with the production string compares (every profile; q3map2's keys in any
  case; id's keys and lookalikes; `_farplanedist` by profile; keys outside
  the worldspawn, values, cut-off, malformed and over-long text), checks the
  scan's keys against RTX's parser and where the rule is used, and sweeps
  the installed maps. `rtx_light_rules_tests.py` traces Wolfenstein's,
  Quake Live's and Enemy Territory's unmarked suns at a brightest channel of
  1 and Quake III's, Q3Test's, Elite Force's and the Jedi games' at unit
  length; `entity_sun_shadow_tests.py` checks the units a level's load sets
  and that an entity sun keeps its own; `csm_receiver_tests.py`,
  `raven_bsp_shader_flags_tests.py` and
  `rtx_renderer_contract_source_tests.py` follow.

### Final export validation

The validation started for `c845803b2049cdd5c8304c1ace809550bc79f359`
finished before the session stopped. Its logs were checked on 2026-10-22:

- **Source identity.** The existing archive export at
  `.tmp/fnq-shadows/export3` was updated to that commit. All 4,605 tracked
  files match: 1,114 byte for byte and 3,491 after CRLF-to-LF normalization;
  none are missing or otherwise changed. This was an incremental rebuild
  of the earlier clean export, not another clean build.
- **Build.** Windows x86, MSVC, `debugoptimized`, `strict-warnings=true`,
  client and dedicated server enabled, with GLx, Vulkan and RTX as loadable
  renderers. The all-target build completed its 82 pending actions with
  exit 0 and no compiler warnings (`units-build.log`).
- **Native suite.** 274 passed, none failed, two skipped
  (`units-native.log`). OpenAL loopback was unavailable; the retail factory
  probe did not find Quake Live at its configured location. These skips do
  not establish either runtime gate.
- **Python suite.** All 612 test files were attempted, four processes at a
  time: 602 passed, nine contain standalone checks rather than pytest tests,
  and `release_packaging_tests.py` hit the 1,500-second per-file timeout.
  Its isolated rerun passed: 48 tests and 22 subtests, with one Windows
  symlink-privilege skip, in 932.59 seconds. All nine standalone scripts
  also passed when run directly against the verified export. Thus 603 files
  passed under pytest and nine passed as scripts; no failure remains after
  the rerun. The original timeout remains recorded in the pool summary.

Logs are under `.tmp/fnq-shadows/`: `units-build.log`, `units-native.log`,
`pytest-units/summary.json`, `release-packaging-rerun.log`, and
`completion/validation.json` with log hashes and the direct-script results.
The JSON companion carries the same final results under
`skySunUnits.exportValidation`.

The Games guide and both README templates now describe the expanded model
shadows; the Display guide also names the separate RTX raster limitation
on RTCW `forest`. No engine code or compatibility-gate status changed while
closing this validation record.
The generated-document and player-manual checks passed after these updates:
20 tests and 840 subtests (`completion/docs-tests.log`). The record's JSON
also parses and its file totals agree.
