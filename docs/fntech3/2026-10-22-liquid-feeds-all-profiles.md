# Underwater view and liquid ripples in every game, 2026-10-22

FnQL's [underwater view](../fnql/UNDERWATER_VIEW.md) and the ripples of its
[enhanced liquids](../fnql/LIQUID_RENDERING.md) are drawn by the renderers
but fed by the client: the client samples what the camera is inside and where
moving entities cross a liquid's surface, because only it has the collision
model. Only the Quake III client-game path fed them (`CG_R_RENDERSCENE` and
`CG_S_UPDATEENTITYPOSITION` in `code/client/cl_cgame.cpp`), which Quake III
Arena, Quake Live, Elite Force and both Jedi games use. The other eight
profiles' scene adapters handed their views straight to the renderer, so
`rtcw-sp`, `rtcw-mp`, `wolfet`, `alice`, `fakk2`, `mohaa`, `q3-ihv` and
`q3test` never went underwater and never rippled. This record feeds both
features from every profile, each with its own maps' liquid bits and its own
entity types, which also corrects the Jedi games' liquid bits, and aligns
GLx's enhanced liquids and soft particles with Vulkan and RTX under the
Ritual far-plane fog. A follow-up gives the audio the same per-game liquid
bits, follows Jedi Academy multiplayer's and Holomatch's own entity rules,
and stops RTCW's player shadows from ending the game under water. The
[JSON companion](2026-10-22-liquid-feeds-all-profiles.json)
has the identities, commands and captures. No compatibility gate closes: this
is presentation.

## Observed

### Each profile's liquid bits

The collision model keeps a map's contents bits as its compiler wrote them:
the IBSP 43, Q3Test, RBSP, FAKK and 2015 translators copy them unchanged
(`code/qcommon/bsp_*.h`). The renderers' liquid and underwater records read
Quake III's bits (`MASK_WATER`: lava 8, slime 16, water 32), and the feed
masked every profile's contents with them.

Every liquid brush in the installed maps, read from their brush and shader
lumps:

| Profile | Liquid shaders' contents | Maps read |
| --- | --- | --- |
| `rtcw-sp`, `rtcw-mp` | water 0x20 | all 47 maps with liquids, `pak0` and `mp_pak*` |
| `wolfet` | water 0x20 | `battery`, `oasis`, `radar` |
| `alice` | water 0x20, slime 0x10, lava 0x8 | 26 maps |
| `fakk2` | water 0x20, slime 0x10, lava 0x8 | 21 maps |
| `mohaa` | water 0x20 | 12 maps |
| `q3-ihv` | slime 0x10 | `km_portal`, the only one with a liquid |
| `q3test` | lava 0x8 | `q3test1`, `q3tourney` (1.05 and 1.08) |
| `jka-*` | lava 0x2, water 0x4 | 8 maps, `mp/siege_korriban` the only multiplayer one |
| `jk2-*` | water 0x4 | 6 maps |

They agree with each game's own definitions: RTCW's and Enemy Territory's
`surfaceflags.h`, the F.A.K.K.2 SDK's (Ritual's licence: read, not used),
OpenMoHAA's (reference observation) and OpenJK's, where Raven's lava is 0x2,
water 0x4 and slime 0x20000. With Quake III's mask the Jedi games never found
their water, and read their fog (0x8) as lava: Jedi Outcast's `artus_mine`
slime carries water 0x4 and fog 0x8 together.

### Each profile's moving entities

Quake III's feed tracks `ET_PLAYER` (1) and `ET_MISSILE` (3) and resets a
track when `EF_TELEPORT_BIT` (4) toggles. Entity types are the game's, so
each was read from the game's own module where one could be read:

- **RTCW and Enemy Territory** keep Quake III's three values (`bg_public.h`;
  Enemy Territory's enum is in `q_shared.h`). RTCW single player's AI
  characters are `ET_PLAYER` entities in client slots (`ai_cast.c`).
- **Q3Test 1.05 to 1.08**: each `cgamex86.dll`'s `CG_AddCEntity` switches on
  the type below 11 or 12 (`ET_EVENTS`); case 1 calls the function that
  raises `Bad clientNum on player entity` (1.08 0x3000e080), and each
  `qagamex86.dll`'s `fire_rocket`, `fire_plasma` and `fire_grenade` store
  type 3 (1.08 0x2001234a). `TeleportPlayer` sets `PMF_TIME_KNOCKBACK` and
  toggles eFlags bit 4 (1.05 0x20013bb9, 1.06 and 1.07 0x200109f9, 1.08
  0x200111e5).
- **The IHV Test**: `cgamex86.dll`'s `CG_AddCEntity` (switch at 0x10006976)
  sends type 1 to the function that raises `Bad client index on entity`;
  `qagamex86.dll` stores type 3 in the entity's type byte at offset 2 for
  rockets, plasma and grenades (0x2001138f, 0x20011180, 0x20011290), and
  `TeleportPlayer` toggles bit 4 of the 16-bit flags (0x20010625). Its
  frames never hold the local player's own entity (listener 1 absent from
  2,195 frame entities in a `km_portal` run), and it tells the engine where
  an entity is only through the callback it hands `S_Respatialize`
  (`CG_GetEntitySoundOrigin` 0x10006af0, which bounds the number to 0..1023
  and copies the entity's interpolated origin).
- **Alice and F.A.K.K.2**: Ritual's `CG_AddCEntity` (Alice 0x300228e0,
  F.A.K.K.2 1.02 0x30020cb5) skips types from 19 (`ET_EVENTS`); type 1
  (`ET_PLAYER`) calls `CG_Player` and falls through into type 0
  (`ET_MODELANIM`), which calls `CG_Splash` and `CG_ModelAnim`; type 4
  (`ET_MISSILE`) is the `Bad entity type` error. The F.A.K.K.2 SDK names the
  same values, makes actors `ET_MODELANIM` and projectiles `Animate`s, which
  are `ET_MODELANIM` once animated. The SDK's `CG_SetEntitySoundPosition`
  reports every packet entity's origin to `S_UpdateEntity` each frame, but
  for an entity the player carries it sends no origin and asks for the
  listener.
- **Medal of Honor**: `cgamex86.dll`'s `CG_AddCEntity` (0x30020c26) skips
  types from 20; types 0 (`ET_MODELANIM_SKEL`), 1 (`ET_MODELANIM`),
  2 (`ET_VEHICLE`) and 3 (`ET_PLAYER`) each call `CG_Splash` (0x30035c0a);
  type 6 (`ET_MISSILE`) is the error. OpenMoHAA names the same values and
  bit 4 for teleports, and its `CG_SetEntitySoundPosition` sends carried
  entities with the listener flag as Ritual's does.
- **Elite Force Holomatch**: its `cgame.qvm`'s `CG_AddCEntity` sends type 3
  to `CG_Missile` and type 4 (`ET_ALT_MISSILE`) to the same function with its
  alt-fire argument set (instructions 24425..24444). Its `qagame.qvm` stores
  types 0 to 3 and 5 to 12 as constants and never 4, so whether the retail
  game sends an alt-fire missile at all is not shown. Its `TeleportPlayer`
  toggles bit 4 (instruction 2897).
- **Jedi Academy multiplayer**: `jampgamex86.dll`'s `TeleportPlayer` sets
  `PMF_TIME_KNOCKBACK` and then toggles eFlags bit 8 (0x2006af4e; the same
  pair at 0x200932ef), OpenJK's `EF_TELEPORT_BIT (1<<3)`. `cgamex86.dll`'s
  `CG_AddCEntity` (0x30035540) draws type 13 (`ET_NPC`) as a Ghoul2
  humanoid, a body like a player's.
- **The other Jedi profiles** keep Quake III's rule: Jedi Academy single
  player's `jagamex86.dll` toggles bit 4 after `PMF_TIME_KNOCKBACK`
  (0x200d4f7b), Jedi Outcast single player's `jk2gamex86.dll` toggles bit 4
  (0x2001e6b7, 0x2001e7c3), and Jedi Outcast multiplayer's `jk2mpgame.qvm`
  toggles bit 4 of the player state's flags after setting
  `PMF_TIME_KNOCKBACK` (instruction 150620). The single-player games' NPCs
  are `ET_PLAYER` (OpenJK `NPC_spawn.cpp`).

So Ritual's and 2015's games have no projectile type to follow; their bodies
are the types their own client games splash.

### The audio's liquid bits

The OpenAL path asks the collision model whether a sound or the listener is
in a liquid and which contents block a sound
(`code/client/audio/openal/AudioSystemShared.inl`). It used Quake III's bits
for every profile: lava 8 and slime 16 as occluders beside solid, and lava,
slime and water 8, 16 and 32 as liquid. In the Jedi games those bits are fog
(0x8), player clip (0x10) and monster clip (0x20), so their fog volumes and
clip brushes muffled sound like a liquid and blocked it like a wall, while
their real water (0x4), lava (0x2) and slime (0x20000) did neither.

### RTCW's player shadows under water

With RTCW's default blob shadows (`cg_shadows 1`), standing under water in
single player's `forest` and `dam` could end the game with
`Non-finite RTCW mark corner`, whatever the liquid settings. A trace of the
request showed rotation 11, every corner NaN and a zero projection. RTCW
single player's `CG_PlayerShadow` traces down from the body with
`MASK_PLAYERSOLID`; when that trace starts in solid it reports a fraction
below 1 with a zero plane normal, and each shadow part's `CG_ImpactMark`
builds its corners and projection from that normal (the GPL source,
`cg_players.c`, `cg_marks.c`). Retail's `R_MarkFragments` returns no
fragments for such a request: with a zero direction no curve triangle passes
its facing test, and every face's chop planes are NaN, so
`R_ChopPolyBehindPlane` finds no point in front and leaves no polygon
(`tr_marks.c`). fnTech3's adapter dropped the game instead.

### GLx's fog gate

GLx's enhanced liquid pass (`RB_LiquidEffectActive`) and its ARB depth-fade
program (`RB_DepthFadeActive`) stood aside whenever `RB_RTCWHasFog()`, which
for the Ritual view (Alice, F.A.K.K.2, Medal of Honor) is the far-plane fog
most of their levels have on. Vulkan and RTX test only RTCW's and Enemy
Territory's scene fog (`backEnd.refdef.rtcwFog`), so the same levels kept
both there. Vulkan's depth fade lives in its generic stage shader, which also
applies the stage's fog; GLx's is a separate ARB program with no fog. No
retail Alice, F.A.K.K.2 or Medal of Honor shader asks for depth fade (none of
their paks' shaders has `q3map_depthFade`), so only mods' shaders reach that
program in those games.

## Served

- **One liquid contract per profile** (`CL_LiquidContract`,
  `code/client/cl_cgame.cpp`): the profile's lava, slime and water bits, its
  teleport bit, the entity types that move like players and its projectile
  types. Quake III's rule serves Quake Live, both RTCW modes, Enemy
  Territory, Elite Force single player, the IHV Test, Q3Test, both Jedi
  single-player games and Jedi Outcast multiplayer; Holomatch adds type 4 as
  a projectile; Jedi Academy multiplayer resets on bit 8 and takes type 13 as
  a body; Ritual's bodies are types 1 and 0, Medal of Honor's 3, 2, 1 and 0,
  and neither has a projectile. The liquid bits are the collision contract's
  (`code/qcommon/cm_game_contract.h`): `CM_LiquidMaskForGame` gives Raven's
  0x2, 0x4 and 0x20000 to the four Jedi profiles and Quake III's to the rest,
  and `CM_LiquidContentsForGame` translates a point's bits to Quake III's for
  both renderer records. The boundary traces take the profile's mask.
- **Audio** takes the same helpers: its occlusion traces and buried-source
  test block on solid and the profile's opaque liquids
  (`CM_OpaqueLiquidMaskForGame`: lava and slime), and its in-liquid tests
  read the profile's liquids. Nothing changes for the twelve profiles that
  keep Quake III's bits.
- **RTCW marks** (`code/client/client_rtcw_marks.hpp`): a request with a
  non-finite corner or projection returns no fragments, as retail's clipper
  does, before the renderer is asked. The renderer's own entry point still
  refuses non-finite input, and every fragment it returns is still checked
  before the module sees it.
- **The census** that classifies each snapshot's entities reads the
  profile's own frame: the engine's snapshot for the Quake III-family and
  Ritual profiles, Medal of Honor's own frames (`CL_MOHAALiquidCensus`), the
  Q3Test release's 204-byte entities (`CL_Q3TestLiquidCensus`) and the IHV
  Test's 164-byte records (`CL_Q3IHVLiquidCensus`). The local player is the
  one the player state names, or, in the IHV Test, the listener its client
  game names; while the frame holds no entity of its own the camera stands in
  for it, as in Quake III.
- **The feeds** (`CL_LiquidFeedView`, `CL_LiquidFeedEntity`, declared in
  `client.h`): every other scene adapter calls the first with the view it is
  about to render, after the entity editor has placed its camera -- RTCW's
  and Enemy Territory's client-game scenes (not their menus'), Alice's and
  F.A.K.K.2's, Medal of Honor's, the IHV Test's and Q3Test's -- and the second
  wherever the audio hears an entity's position: RTCW's and Enemy Territory's
  `CG_S_UPDATEENTITYPOSITION`, Q3Test's import 33, Ritual's and Medal of
  Honor's `S_UpdateEntity` except for carried entities heard at the listener,
  and the IHV Test's respatialize callback for the frame's entities.
- **Sky portals.** RTCW's and Enemy Territory's portal skies are scenes of
  their own drawn before the eye's, as Jedi Academy's is: the feeds skip them,
  and all three renderers clear the underwater medium on them, so the layer
  composites once, over the eye's view.
- **GLx** now stands aside only for RTCW's and Enemy Territory's scene fog
  (`RB_RTCWSceneFog`), as Vulkan and RTX do. The liquid pass draws unfogged
  on every GLx tier, as Vulkan's and RTX's do (its underlay is the already
  fogged scene; the stages over it keep their fog); a stage under GL linear
  fog takes a depth-fade variant whose vertex program hands on the fog
  coordinate the fixed-function stage would (the eye's distance, or depth
  without `GL_NV_fog_distance`) and whose fragment program is the depth fade
  compiled with `ARB_fog_linear`. Without that variant such a stage draws
  without its depth fade, as before.
- **Diagnostics.** With `developer 1` the client logs each ripple it emits
  (`liquid ripple: body|projectile at x y z, radius, strength, time`) and each
  change of the camera's medium (`underwater view: medium 0x.. at x y z`).

FnQ3/FnQL features: the underwater view and enhanced liquids with their
interaction ripples, now in all sixteen profiles; the renderers, effect
model and controls are unchanged. `r_underwater 0`, `r_liquid 0` and
`r_liquidRipples 0` keep each game's own presentation, as before.
Retail-exact behaviour is unchanged where it is contract: nothing here reaches
snapshots, prediction, demos or any module, and RTCW's and Enemy Territory's
own water ripples (`RT_SPLASH`) still draw as their games send them.

## Live

A worktree build of this slice (MSVC x86, `win32-dev` options with strict
warnings, build directory `C:\ft3l`) ran each profile on an unswitched
private desktop: windowed 960x720, isolated home, `net_enabled 0`, mouse,
joystick and sound off, no input, GLx, `developer 1`, `r_underwater 1`,
`r_liquid 2`, `r_liquidRipples 2`. Every capture is the engine's
`screenshotPNG`; each underwater pair is one place with the layer at its
defaults, then with its four live strengths at 0 (the layer inert). Every
installation was unchanged. The harness and the BSP readers that chose the
places from each map's liquid and solid brushes are under
`.tmp/liquid-feeds/`.

| Profile | Map, place | Underwater (eye's medium, captures) | Ripples logged | Ring seen in a capture |
| --- | --- | --- | --- | --- |
| `rtcw-mp` | `mp_base` pool | water 0x20: blue medium over the cave | crossing at the surface (z -192, radius 51, strength 0.97), wake | yes, around the player's feet |
| `rtcw-sp` | `forest` lake, `baseout` pool | water 0x20 in both, first and third person | crossing at `baseout`'s surface (radius 56, strength 1.10) and wakes; crossing in `forest` and `dam` | no: `baseout`'s pool is dark and `forest`'s scene fog pauses enhanced liquids (with `r_wolffog 0` they draw, the lake's scrolling texture hides the ring) |
| `wolfet` | `battery` sea by the beach | water 0x20: wave distortion over the game's own murky sea fog | crossing at the sea surface (z 112) and 17 wakes from the limbo camera drifting at the surface | no |
| `alice` | `garden1` pool | water 0x20: teal medium against plain earth | crossing (z -448, radius 40); wakes from creatures moving in the nearby pool; a body's splash in the level's opening cinematic | not clearly |
| `fakk2` | `cemetery` shaft | water 0x20: medium and edge falloff over the shaft's own blue fog volume | crossing (z -392, radius 44) and a wake | yes, concentric rings around Julie's feet |
| `mohaa` | `m4l0` pond | water 0x20: blue-green medium against the brown pond floor | crossing (z -160, radius 56, strength 1.10) and two wakes | not clearly |
| `q3-ihv` | `km_portal` slime channel | slime 0x10: green absorption over the red-lit pit | the camera going in (radius 49) and bobbing out (radius 38) of the slime | no |
| `q3test` | 1.08 `q3tourney` lava trench | lava 0x8: lava haze against the sharp underside | crossing at the lava (z -216, radius 56, strength 1.10) | no |
| `jka-mp` | `mp/siege_korriban` lava | lava 0x8, from Raven's 0x2 (the old mask read nothing) | -- | -- |

The IHV Test and Q3Test have no `setviewpos`; their places came from a
`maps/<map>.ent` override in the isolated home that moves the deathmatch
spawn points. `km_portal`'s slime lies partly under patch floors, which a
brush test cannot see; the channel used was found by trial. The IHV Test's
local player ripples only when the view itself crosses the surface, its own
entity being absent from its frames; a view snapping down at death
(43 units in 15 ms) is rejected as motion, as Quake III's feed rejects it.

**GLx under the far plane.** `m4l0`'s worldspawn sets `farplane 13000`. A
capture of its pond with the liquid pass on and another with
`r_liquidRefraction 0` and `r_liquidReflection 0` differ over the pond
(35,844 pixels by more than 12 levels) about twice as much as two captures
with the pass on (16,306, the pond's own animation); the fogged depth-fade
programs compiled with no ARB error on this GPU. The same pair with the
renderer built from the slice's base commit has not been captured yet, so the
gate change itself is shown by source and unit tests, not by a before/after
capture.

**RTCW shadows under water.** Before the marks fix, `forest` with `noclip`
and `setviewpos 3584 -5120 150 0` and `dam` with `setviewpos 572 -1920 -1350
0` both ended in `Non-finite RTCW mark corner` at the default `cg_shadows 1`,
as did one of two `forest` runs without `noclip` at `setviewpos 3584 -5120
190 0`. With the fix both runs played on under water with no error,
`forest` in first and third person. The audio change is shown by source and
unit tests; no Jedi listening capture was taken.

## Tests

`tests/liquid_interaction_source_tests.py` pins the contract's values per
profile, the census's three record readers, every adapter's feed calls and
the audio's use of the per-game bits; `tests/collision_game_contract_tests.cpp`
runs the three liquid helpers for all sixteen profiles;
`tests/rtcw_marks_tests.py` runs the RTCW marks adapter with a NaN corner and
a NaN projection and asserts no fragments, no renderer call and no write;
`tests/underwater_view_source_tests.py` the translated contents;
`tests/jka_sky_portal_tests.py` the three sky-portal skips;
`tests/rtcw_fog_gpu_tests.py` runs `tr_rtcw_fog_gl.h` and asserts that the
Ritual far plane is no scene fog and reports linear GL fog, measured from the
eye; `tests/rtcw_render_import_tests.py` and `tests/rtcw_spatial_audio_tests.py`
run RTCW's adapters and check that the client game's scene, not the menus',
and every entity position reach the feeds with the copied values. On the
worktree build every target builds without warnings, the C++ tests and the
150 affected Python test files pass (detail in the JSON); the `git archive`
export build and test pass has not been run.

## Limits and open items

- The sheen pass of the enhanced liquid draws unfogged under the Ritual far
  plane in all three renderers; the stages over the underlay keep their fog.
- Ritual's and 2015's projectiles ripple only as bodies, within a body's
  speed, having no type of their own.
- Holomatch's type 4 is followed on its client game's evidence alone; no
  retail game was seen sending it.
- Rings are seen in captures for RTCW multiplayer and F.A.K.K.2 only; in the
  others the emission is logged at the surface and the ring falls behind the
  body, under fog or in a dark pool.
- No capture is compared with a retail engine: none of these engines drew
  either feature.
