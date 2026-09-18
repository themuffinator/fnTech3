# Every official Enemy Territory map loads

## Scope and authority

This work follows the [client run record](2026-09-18-wolfet-client-run.md),
which loaded one map, oasis, with the hunk raised by hand. It loads all six
official maps -- battery, fueldump, goldrush, oasis, radar and railgun -- in
the developer client, the way a player would: with ET's own retail
configuration, through the retail cgame, UI and game modules, on all three
renderers, and fixes everything that load found.

Behaviour facts come from the pinned original
[Enemy Territory](https://github.com/id-Software/Enemy-Territory/tree/40342a9e3690cb5b627a433d4d5cbf30e3c57698)
and, where ET kept it, the original RTCW multiplayer source it grew from.
[ETe](https://github.com/etfdevs/ETe) was consulted for structure. All three are
GPL-3.0; no implementation text from any of them is incorporated into fnTech3.
Retail content was only read: every run leaves the installation unchanged, and
captures are kept under `.tmp`, never in the repository.

## Result

**All six official maps load, draw and quit cleanly on every renderer.** Each
map starts from a clean isolated home, with ET's own `etconfig.cfg` from the
installation, runs 240 frames as a spectator, loads its [audio
zones](2026-09-18-wolfet-audio-zones.md), captures a frame with the engine's own
`screenshotPNG` and quits with exit code 0.

| Map | Audio zones | Hunk peak (MB) | Absent retail content |
| --- | --- | --- | --- |
| battery | 255 | 60.4 | 30 models, 14 sounds, 1 image |
| fueldump | 417 | 69.7 | 30 models, 16 sounds, 2 images |
| goldrush | 512 | 60.3 | 30 models, 8 sounds, 1 image |
| oasis | 512 | 65.5 | 30 models, 18 sounds, 1 image |
| radar | 318 | 62.7 | 30 models, 15 sounds, 1 image |
| railgun | 141 | 66.1 | 30 models, 7 sounds |

Absent retail content is exactly that: every file in that column was checked
against the installation's own paks and shader scripts, and none is shipped.
The retail engine meets the same gaps the same way -- ET's modules register
models and sounds the game never shipped, and its scripts name compile-time
images that only its map compiler read. Anything the content does ship but the
engine failed to load counts as an engine issue, and none remains.

What else the logs hold is either a report the original engine makes in the
same words for the same content -- a shader with a lightmap but no lightmap
stage, two modules registering one cvar with different defaults, one image
asked for with and without mipmaps, and ET's own game module reporting a map
script's `setstate` that names no entity -- or a fact about the machine the
probe ran on.

## What the load found

Each of these was found by loading the maps; none was visible to a unit test,
and each is fixed.

1. **A map that opens no campaign could not load in a listen server.** ET
   starts in campaign mode; a map that is not a campaign's first reloads itself
   in Objective mode by re-issuing the very command that loaded it, which ET's
   game reads back while it initialises. Unloading the client and clearing its
   pure lists tokenize strings of their own, so the game found no command and
   stopped with `Usage 'map <mapname>'`. Battery, fueldump and goldrush could
   not load. The command is now kept and restored for the game's
   initialisation, as the original engines leave it.
2. **That reload then collided with Quake 3's game-type rule.** Quake 3 turns
   its single-player game type, 2, into a free-for-all on an ordinary map
   command. ET numbers its game types differently -- 2 is Objective -- so the
   reload became game type 0, which ET's game refused, corrected and restarted
   from, leaving the player on "Connection Interrupted". ET reads its
   single-player and cooperative game types from `gameinfo.dat`, and so does
   the server now: retail lists type 0 alone as single player, keeps the
   single-player map commands disabled, and changes no game type on an ordinary
   map command. The same rule had made a listen server in Objective mode ignore
   every challenge, status and info query as if it were a single-player game.
3. **A map change did not reset ET's match.** ET begins every map from a fresh
   match state, round and time limit, tells its game when it starts inside a
   map load, and moves its match state through ET's own transitions on a
   restart. The server did all of this for RTCW multiplayer only.
4. **Every official map was drawn without its lightmaps.** ET's compiler wrote
   them beside each map, as `maps/<map>/lm_NNNN.tga`, and left the BSP's own
   lightmap lump empty, so every world surface fell back to vertex light. All
   external lightmaps now load, shifted for the map's overbright range as a
   lightmap in the BSP is; one that cannot be loaded leaves its surfaces vertex
   lit, as in ET.
5. **Radar could not load at all.** It carries 111 foliage surfaces -- its
   13,208 rain ripples -- a surface type ET added and no other profile knows.
   Foliage is now loaded, culled and drawn as ET draws it: one small mesh at
   many origins, each instance faded out between its shader's `distancecull`
   distances, every second and fourth instance thinned a little earlier, and
   nothing drawn while `r_drawfoliage` is zero.
6. **Three maps stopped as soon as the cgame lit a lamp.** ET's coronas are the
   other Wolfenstein profile's multiplayer coronas; the renderer refused them
   outside that profile.
7. **Large planar faces were cut short and drawn with the default shader.** ET
   draws a planar surface of any size its compiler wrote; this renderer kept
   Quake 3's 64-point cap, which 37 of fueldump's surfaces and 47 of radar's
   exceed.
8. **Most of ET's shaders failed outright.** ET's dialect is RTCW
   multiplayer's with three additions, and the renderer knew neither half for
   ET, so a shader using any keyword from it -- 692 implicitly mapped shaders,
   101 `nocompress`, 247 `nofog`, the fog, sun and light-grid directives, and a
   stage keyword of ET's own -- was replaced by the default shader. Crosshairs,
   player bodies, crates, map textures, the command map image and skies were all
   drawn as the default checkerboard. ET's dialect is now read in full: RTCW
   multiplayer's keywords, implicit mapping with ET's own stages for a bare
   image, `distancecull`, the `lightmap` stage keyword, and shader names written
   with backslashes, which ET's models use.
9. **ET's skins attached no models.** An ET skin names its hat, rank badge and
   backpack models on lines of their own; the renderer read those as surfaces
   and their models as shaders. ET's cgame also names a skin whose path is too
   long for itself, and reads the truncated name as a file that is not there;
   the renderer had instead drawn it as a shader.
10. **Models were looked up in the wrong order.** ET looks for a compressed
    model first and the plain one second, whichever of the two it was asked
    for.
11. **The console had no art.** The client registered Quake 3's console,
    character set and cursor, none of which ET ships. It now registers ET's
    own; ET has no recording icon and no connect backdrop, so none is
    registered.
12. **The decal imports stopped every reloaded map.** Once the reload worked,
    ET's cgame reached its first snapshot and pressed a decal, an import the
    engine refused. Decals are now ET's own: each projector of a scene still in
    view is pressed onto every map surface a view adds that it reaches, clipped
    to the projector, kept on the surface's brush model -- 1024 for the world,
    128 for any other -- and faded as ET fades them.
13. **Goldrush stopped on a sound.** Its map speakers play louder than the
    usual volume range, which ET's mixer allows and this engine refused.
14. **ET's own configuration asks for too little memory.** Its `etconfig.cfg`
    sets a 56 MB hunk, sized for ET's original engine; this engine keeps more of
    a map there -- 60 to 70 MB across the six maps, where the original recorded
    36 to 47 -- so ET's hunk is now at least 128 MB, applied at once.
15. **Railgun's sky smeared.** Its sky shader has no stages: ET clears every
    world view to the colour of the map's global fog, and the sky shows that.
    Without the clear, falling snow smeared across whatever the last frame left.
16. **Two diagnostics ET itself does not make** were printed: a warning for
    images that declare a top-down row order, which ET, like this engine,
    ignores silently, and a developer report of invalid patch bevels, which ET
    disabled because its maps make so many.

## Evidence

`scripts/probe_wolfet_maps.py` runs `scripts/probe_wolfet_client.py` once per
map on a private desktop, windowed, with no input injected, and sorts every
warning and error in each console log. It checks each missing file against the
installation's paks and shader scripts, fails a map on any engine issue, and
requires its audio zones to load. The runs recorded here passed on all six maps
with the GL, Vulkan and RTX renderers, each with no hunk given, so ET's own
configuration applied.

The captures show each map lit by its lightmaps, its sky or fog, its foliage,
decals and coronas, and ET's own warmup or limbo screen. Radar's command map
now draws its overhead image, which the client run left black.

Executable regressions check foliage's counts, height scale, fade and culling
arithmetic; external lightmap naming and sizes; the decal arithmetic, from
building a projector to clipping, colouring, ageing and slot choice; ET's
`gameinfo.dat` reader; and ET's restart transitions. Source regressions pin
each wiring above in all three renderers, the server and the client, and the
checker's classification.

All 112 compiled and source tests pass on the Windows x86 developer build. Every
Python regression passes; those this change affected were run again against
the change alone, apart from other work in progress.

## Remaining gates

- **No gameplay is claimed.** Every map was loaded as a spectator in warmup or
  limbo; no team was joined in these runs.
- The RTX renderer was run with its ray-traced mode off; foliage and decals
  are drawn by its raster path and are not part of its ray-traced scene.
- Render to texture stays refused, as recorded; retail ET never calls it.
- ET fills its loading bar from a hunk-usage table this engine does not keep.
- ET leaves textures of 128 by 128 or smaller uncompressed; that rule only
  matters with texture compression enabled and is not applied.
- ET adjusts the fog colour of alpha-blended stages its own way; the shared
  rule is kept.
- Decals are pressed in every view that adds a surface, as in ET, portal views
  included.

## Touched outside ET

- The server keeps the command that started a map tokenized for the game's
  initialisation for every profile, so a listen server now matches a dedicated
  one and the original engines.
- The RTCW restart transition gained an optional argument; RTCW passes none.
- The BSP surface-type enumeration and each renderer's surface types and
  interface gained entries at their ends; no existing value moves.
- Shared helpers that answered for the RTCW profiles now also answer for ET:
  coronas, the shader dialect, world directives, fog directives and skins. The
  RTCW paths are unchanged.
- Eight existing regressions were brought up to date. Fixtures that compile
  slices of shared code gained stubs for the external lightmap call, the ET
  server helpers and the fog colour's scale, and now match whole names where
  they rename surface-table entries, since ET's foliage and decal entries begin
  with its model entry's name. Two source checks count the lightmap stage's
  image lookup, which goes through the shared image policy, and ET's two new
  surface types. The restart, server-policy and fog fixtures gained ET cases;
  none asserts anything new about another profile.

## Unrelated observations

The engine's cvar range check never corrects a value that is already set,
because setting a cvar to its own value is a no-op before validation. Quake
3's own 64 MB hunk floor has therefore never applied to a configuration that
asks for less. ET's floor is set outright; the general fix is left to its own
change.
