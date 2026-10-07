# In-game entity editor and `.ent` entity overrides, 2026-10-05

fnTech3 now loads a map's entity override, `maps/<map>.ent`, in place of the
entities its BSP holds, in every profile, and has an in-game editor, drawn
with [Dear ImGui](https://github.com/ocornut/imgui), that reads a level's
entities, edits one or many of them visually and writes that override. The
[JSON companion](2026-10-05-entity-editor.json) records the source and build
identities, every probe's command line, console proof, override and capture
hashes, the map audit and the tests. The player guide is the
[Entity Editor Guide](../ENTITY_EDITOR.md). No compatibility gate changes:
this is an engine feature, and with no `.ent` file present every profile
hands its game module the BSP's entity text exactly as before.

## Entity overrides

`SelectServerEntityString` (`code/server/sv_game.cpp`) chooses a level's
entity text each time a game module initialises, a restart included:

1. Quake Live's retail `sv_altEntDir/<map>.ent`, when the cvar is set and the
   file exists, handed over unchecked as retail does
   ([QL server cvars](../fnql/SERVER_CVAR_COMPATIBILITY.md)).
2. `maps/<map>.ent`, while `sv_entityOverrides` is 1 (the default, every
   profile). The path is the map's own beside its BSP, its directories kept
   (`maps/dm/mohdm1.ent`), each part checked as an `sv_altEntDir` path is; an
   Alice or F.A.K.K.2 spawn target after `$` is dropped. The file is read
   through the filesystem, so it may be loose or in a pak. It is used only if
   it has no NUL byte, parses as a Quake III-line game module reads entity
   text, and begins with the `worldspawn`; otherwise the server prints the
   reason and the line, and the BSP's text runs. Quake III's
   `advertisement`-to-`func_static` rewrite applies to it as to the BSP's.
3. The BSP's entity lump.

Every consumer reads the chosen text (`SV_EntityText`): the token trap that
serves the Quake III line's game modules, the `Init` text of the
function-table games, Alice's and F.A.K.K.2's `SpawnEntities` copy, Medal of
Honor's `SpawnEntities`, Q3Test's `SpawnEntities`, and the botlib's
`BSPEntityData`, so bots seek the items the level spawned. On a listen
server the client game's entity tokens follow the same override
(`CL_GetEntityToken`), for the cgames that spawn entities of their own (Enemy
Territory, RTCW, Jedi Academy); a client of another server keeps the
renderer's BSP text. Render-time worldspawn reads tied to the BSP's compiled
data (`gridsize`, sun and fog keys) still read the BSP.

`sv_saveentfile` (DarkPlaces' name) writes the running level's text to its
`maps/<map>.ent` -- a starting file on a dedicated server -- and says so when
the file cannot be written.

## Reading and writing entity text

`code/qcommon/entity_text.hpp` holds the reader and writer the server's check
and the editor share. Its tokenizer is `COM_ParseExt( &p, qtrue )`: bytes up
to `' '` as whitespace compared as a plain `char` (so a byte from 0x80 up is
whitespace where `char` is signed, as in `SkipWhitespace`), both comment
forms outside quotes, quoted tokens without escapes, 1,023 kept bytes a
token, and the first NUL as the end. Its parser follows `G_ParseSpawnVars`,
judging a token by its first byte. `tests/entity_override_tests.py` compiles
the engine's own `SkipWhitespace` and `COM_ParseExt`, sliced from
`code/qcommon/q_shared.c`, beside it and compares both over 6,000 seeded
random texts of braces, quotes, comments, high bytes, line breaks, NULs and
overlong tokens: every token is the same.

The writer emits what q3map emits: braces on their own lines and each pair
quoted on its own line. `scripts/audit_entity_text.py` read every map the
installations here carry -- 607 maps of twelve titles, 359,920 entities --
through the engine's BSP translators: all parse, all begin with the
`worldspawn`, none has a token past 1,023 bytes, and for every one the
writer reproduces the retail lump byte for byte. An override saved without
changes is therefore the BSP's own text; q3dm17's is SHA-256
`3063891b17639c53...` both ways.

## The editor

`entityeditor` (`code/client/cl_entity_editor.cpp`) opens over the running
map on a local server, during a demo, or on a server with `sv_cheats 1`.

- **Interface**: Dear ImGui 1.92.9b (MIT), fetched by the pinned wrap
  `subprojects/imgui.wrap` (tag `v1.92.9b`, commit `f1cc2ae1`) with a Meson
  build of its core and `imgui_stdlib` in `subprojects/packagefiles/imgui`;
  the Makefile and CMake build the same sources from `subprojects/imgui`
  after `meson subprojects download imgui`, and the release workflow fetches
  them. The client defines `FNTECH3_ENTITY_EDITOR_UI` with it; without it the
  overrides remain and `entityeditor` says the build has no editor. A menu bar
  (File, Edit, Select, View, Help, Save, Apply), the entity list, the keys
  panel, a status line, the new-entity prompt and the key list are ImGui
  windows; the overlay is drawn into ImGui's background draw list.
- **Drawing**: an engine backend under ImGui's `RendererHasTextures`
  contract. Each texture ImGui creates or changes -- in practice only its
  font atlas -- is uploaded whole with `RE_RegisterShaderFromRGBA` under a
  name of its own (`*fntech3/entityeditor/imgui<n>`), since a repeated name
  keeps the old image yet spends an image slot. The glyphs the interface
  needs (ASCII, the ellipsis and the replacement character, at both text
  sizes) are rasterised before the first frame, so the atlas changes only
  when the panel scale does or text needs a glyph outside them; in both
  dense probes, which report it, one upload served the run. At most 32
  uploads are made under one renderer, and a renderer restart marks every
  texture destroyed, so ImGui asks for it again. `entity_editor_ui.hpp`'s
  `AppendDrawList` turns the draw lists into the renderers' 2D commands:
  axis-aligned textured rectangles become stretched pictures, other
  triangles `DrawQuad`s (two triangles sharing a diagonal merged into one),
  each clipped in software to its clip rectangle, so GLx, Vulkan and RTX draw
  it alike. The text is ImGui's own default font, ProggyForever (MIT), at
  14 pixels times the panel scale (12 for the overlay's labels).
- **Input**: an engine-owned catcher bit, `KEYCATCH_ENTITYEDITOR` (0x40),
  which `Key_SetCatcher` keeps for every other caller, so no module or map
  load clears it; the editor drops it itself while the client is not active.
  Key, character and pointer events reach the editor after the console's own
  keys and feed ImGui's input (keys with their modifiers, characters, pointer
  and wheel); the editor's own shortcuts wait while a text box or a prompt
  has the keys. A release always clears what the editor holds, so a key or
  button held as the console opens neither flies the camera nor drags on
  after it closes. The SDL, Win32 and X11 layers present the OS arrow through
  the shared pointer policy's menu lane while the editor shows a pointer,
  relative motion while its camera turns, and text input and key repeat
  while it holds the keys.
- **View**: every profile's scene adapter (generic Quake III line, RTCW, ET,
  Medal of Honor, Alice and F.A.K.K.2, Q3Test) hands its world view to
  `CL_EntityEditorAdjustView` before the renderer draws it; while the editor
  is open its free camera replaces the view's origin and axis, every area is
  drawn, the view weapon is suppressed, RTCW's and ET's sky portals turn with
  it and Ritual's sky portal follows its axis. Every view it draws fills the
  screen -- Enemy Territory's limbo camera, a smaller viewsize, otherwise
  left the editor a small picture over black -- at the editor's own field of
  view, which the renderer draws as given (see
  [Box alignment](#box-alignment-2026-10-06)).
- **Selection**: any number of entities, one of them primary (the keys panel
  shows it and the axis handles stand on it): click, Ctrl+click to toggle,
  Shift+click to add (in the list, a range), a box dragged over empty view,
  and Ctrl+A for every listed entity. A press on one of several selected
  entities keeps the others for a drag; released without one, it selects
  that entity alone. Dragging, the axis handles, nudging, dropping, moving to
  the pointer, turning, duplicating, copying, cutting, pasting and deleting
  act on the whole selection, as does a key set or removed on all of them;
  the console's `select`, `set`, `unset`, `move`, `nudge`, `turn` and `print`
  do too. Undo and redo restore the selection with the entities.
- **Command budget**: every line is a 2D quad command (up to 136 bytes),
  every fill or glyph a stretched picture (48), in the renderer's 512 KiB
  `MAX_RENDER_COMMANDS` buffer, which drops whatever no longer fits. The
  overlay draws the 48 nearest entities as boxes, the next 384 as dots, at
  most 32 labels of 40 characters and 48 links, the selected entity's
  first; the editor's frame is held to 320 KiB, the overlay converted first
  and the panels last, so any excess is the overlay's, never the panels';
  the hovered and selected entities are drawn over the rest.
  `entityeditor stats` reports the last frame and the textures uploaded.
- **Document**: `code/client/entity_editor_model.hpp` (identities that
  survive edits, the selection, undo and redo up to 256 steps, a whole drag as
  one step, saved state, target links, filters, per-title spawn limits, and
  entity text pasted from the clipboard, without a `worldspawn` or brush
  entity) and `code/client/entity_editor_ui.hpp` (the draw-list conversion)
  know nothing of the engine. Their unit test, `tests/entity_editor_tests.cpp`,
  also drives a headless ImGui context through the conversion, and passes
  under GCC 13 and Clang 18 on i686 Linux with `-Werror -Wshadow`.
- **Apply**: in the Quake III line, `map_restart 0` when the entities to spawn
  are alike -- the same count and, entity by entity, the same classname,
  `spawnflags` and game-type filters (`ee::SpawnsAlike`) -- and otherwise the
  map loaded again with the level's own start command
  (`SV_ReloadMapCommand`: `spdevmap` or `spmap` for a single-player game,
  `devmap` with `sv_cheats`, else `map`); see the RTCW SP finding below.
  Alice, F.A.K.K.2 and Medal of Honor restart with `restart`, and Elite
  Force's single player and the Q3Test releases before 1.06 load the map
  again, having no `map_restart`. The editor then confirms from the server
  that the saved file is the override in use. On another server, with
  `rconPassword` set, Apply saves and sends `rcon devmap <map>` (`restart`
  for the Ritual titles and Medal of Honor) -- always a full load, since the
  editor there cannot compare against the running level's text -- and reads
  the server's reply, which rcon sends to the asking client rather than to
  the server's log, for its `Loaded entity override` or `Ignored` line
  (`CL_EntityEditorServerPrint`, called from the client's connectionless
  `print` handler); after 20 seconds without either it says the server read
  its own home. The map name goes as one argument: anything but letters,
  digits, `_`, `-`, `/` and `.` refuses it.
- **Menus and HUD**: while the editor is open the game's menus wait,
  undrawn -- `UI_REFRESH`, the menu blur and the browser overlay
  (`cl_scrn.cpp`), Alice's and F.A.K.K.2's menus and cursor
  (`cl_alice_screen.cpp`), and Medal of Honor's HUD and menus once its pause
  and timers are serviced (`cl_mohaa_ui.cpp`). The HUD, 3D HUD models and
  notify lines are hidden through the existing capture switches
  (`CL_CaptureHidesHud`, `CL_HudAdjustPic`); Q3Test's client game, which
  draws through its own adapter, hides its 2D draws and its status bar's
  worldless head scene there (`cl_q3test_cgame.cpp`). A single-player game
  pauses (`cl_entityEditorPause`).

Under invariant 11 this is a new interface path: engine-owned, the same in
every profile, and drawn through the renderers' existing 2D path; it changes
no renderer, audio or input system. It uses ImGui's own font rather than
each profile's console character set, which offers no glyph atlas ImGui can
lay text out with.

## RTCW SP and `map_restart`

The first RTCW SP run stopped after the editor's Apply. Repeating variants
on `forest` (`.tmp/entity-editor/hunt*-rtcw-sp`, a run counted as hung when
the client never finished and the probe stopped it):

| Variant | Hung |
|---|---|
| full flow: open, select, make an entity, key, nudge, undo, redo, save, Apply | 3 of 6; 2 of 4 again |
| the same without the captures before Apply | 5 of 6 |
| without the list selection either | 6 of 6 |
| only make an entity, save, Apply | 6 of 6 |
| make an entity 30 frames after opening, Apply 10 frames later | 0 of 6 |
| value only: nudge an existing entity, save, Apply | 0 of 6 |
| make an entity, save, then load the map again | 0 of 6 |
| Apply with no edit, the editor open (paused) 120 or 300 frames | 0 of 6, 0 of 6 |
| open (paused) 300 frames, close, `map_restart 0` | 0 of 6 |
| no editor: `map_restart 0` at once or after 150 frames | 0 of 6, 0 of 6 |

Observed in `cdb` stacks of the hung clients: the main thread spun inside the
retail `cgamex86.dll` at `vmMain+0xdb72`, a loop that adds an animation's
`frameLerp` to a lerp frame's `frameTime` until it passes `cg.time`;
`frameLerp` read 0 from an animation record zeroed but for its name,
`idle`, with `frameTime` just below `cg.time`. Inferred: `map_restart`
keeps the client game's state for each entity number, and when an entity is
added, removed or reordered a number's lerp frame can refer to an animation
the restarted level never filled. Which entity, and why some runs escape, was
not established. A full reload re-creates the client game, so Apply loads the
map again whenever the entities to spawn change, in every Quake III-line
profile, since their client games keep the same per-number state. On that
rule RTCW SP passed 8 of 8 runs, each with a structural Apply that reloaded
and a value-only Apply with `map_restart 0`, both loading `maps/forest.ent`,
and 6 of 6 more on the final build, beside the matrix's own run.

Q3Test releases before 1.06 have no `map_restart`: the string is absent from
the 1.02, 1.03 and 1.05 `quake3.exe` and `qagamex86.dll` and present in 1.06,
1.07 and 1.08. No retail Q3Test client game handles the command, so 1.06 to
1.08 print `Unknown client game command: map_restart`, as they always did.

## Runtime evidence

`scripts/probe_entity_editor.py` runs the client windowed on an unswitched
private desktop with an isolated home and no input sent: it opens the editor,
selects an entity through the list filter, makes one, keys, moves, undoes and
redoes it, saves and applies, then nudges an existing entity alone and
applies again, capturing each step with `screenshotPNG`; the installation is
inventoried before and after. Each run starts its map with `devmap`, which
a structural Apply repeats (the single-player forms, `spdevmap` and `spmap`,
are pinned by source tests only). On the `meson/build/win32-dev` build
named in the companion, every run passed:

| Run | Map | Entities | Override | First Apply, second Apply |
|---|---|---|---|---|
| q3, GLx, Vulkan, RTX | q3dm17 | 153 -> 154 | `maps/q3dm17.ent` | `devmap q3dm17`, `map_restart 0` |
| ql | campgrounds | 233 -> 234 | `maps/campgrounds.ent` | `devmap campgrounds`, `map_restart 0` |
| wolfet | oasis | 330 -> 331 | `maps/oasis.ent` | `devmap oasis`, `map_restart 0` |
| rtcw-mp | mp_beach | 871 -> 872 | `maps/mp_beach.ent` | `devmap mp_beach`, `map_restart 0` |
| rtcw-sp | forest | 877 -> 878 | `maps/forest.ent` | `devmap forest`, `map_restart 0` |
| stvef-mp | hm_voy1 | 1,041 -> 1,042 | `maps/hm_voy1.ent` | `devmap hm_voy1`, `map_restart 0` |
| stvef-sp | _holodeck_firingrange | 75 -> 76 | `maps/_holodeck_firingrange.ent` | `devmap _holodeck_firingrange`, both |
| q3test 1.08 | q3test1 | 479 -> 480 | `maps/q3test1.ent` | `devmap q3test1`, `map_restart 0` |
| alice | garden1 | 705 -> 706 | `maps/garden1.ent` | `restart`, both |
| fakk2 | homes1 | 1,042 -> 1,043 | `maps/homes1.ent` | `restart`, both |
| mohaa | dm/mohdm1 | 251 -> 252 | `maps/dm/mohdm1.ent` | `restart`, both |

In each, the server printed `Loaded entity override maps/<map>.ent` on both
restarts and the editor confirmed it. Quake III's run made a `weapon_bfg`,
which q3dm17 does not hold, closed the editor and stood the player beside it
with `setviewpos`: the game spawned it from the override and the player
picked it up. A malformed override was set aside (`Ignored maps/q3dm17.ent,
line 1: the entity begun here has no closing brace. Using the BSP's
entities.`), `sv_saveentfile` wrote the running text over it, and the next
restart loaded that.

The client game's own entity tokens follow a local override too. Enemy
Territory's client game reads the worldspawn's `atmosphere` key from them
when a map loads, and `oasis` has none. Through the editor's console
commands the snow `railgun` carries (`T=SNOW,B=5 10,C=0.5,G=0.3 2,BV=20
30,GV=25 40,W=3 5,D=2400`) was set on `oasis`'s worldspawn, saved, and the
map loaded again; a control run saved and reloaded without it. Both servers
loaded their override (46,020 and 45,946 bytes), and from the same camera
the snow run's view shows falling snow: 742 near-white pixels against the
control's 195. The client game could only have read the key from the
override.

The panels were driven by input once, with the user's permission for that
one run (2026-10-06): an Elite Force Holomatch client on `hm_voy1`,
windowed on a private desktop, given key, character and relative-motion
events by the engine's own `in_synthesize`, which queues them into that
client's event queue only. The editor's pointer was homed to the corner
and moved by exact amounts. A click on list row 1 selected #1; Ctrl+click
on row 3 added #3; Shift+click on row 5 selected #3 to #5; Escape cleared
the selection; a box dragged over the view from empty sky selected 391
entities; a click in the filter box and typed text narrowed the list to
`info_player_intermission`, whose row a click selected; a click in the
new-key box, typed text, Tab (which moved the focus to the value box, as
the guide says), typed text and Enter added `fntech3_input`; Ctrl+Z and
Ctrl+Y undid and redid it (1, 0, then 1 matching entity); Ctrl+S saved;
F5 restarted the level with `map_restart 0`, the server loading the
override; and Escape, with nothing selected, closed the editor. Each step
was checked in the log and in engine captures; the installation was
unchanged.

RTCW multiplayer's client game reads the worldspawn's warning-sound keys
from its entity tokens and registers those sounds (the game module does not
read them). With the legacy mixer and `developer 1`, an override setting
`twoMinuteSound_axis` to `sound/fntech3/override_marker.wav`, a file that
does not exist, made the client report `could not find
sound/fntech3/override_marker.wav - using default` after the reload; the
control run reported nothing.

Apply over rcon was run with a Quake III client connected to a fnTech3
dedicated server whose rcon password it held, the server started with
`devmap q3dm17` in its own home. The client saved; the runner copied the
saved file into the server's home, as a shared home or a copy would; Apply
saved again and sent `rcon devmap q3dm17`, and the server's reply carried
its `Loaded entity override maps/q3dm17.ent`, which the editor reported
(`the server loaded maps/q3dm17.ent`, the two files identical). Without the
copy the server reloaded from its own home, and after 20 seconds the editor
said it had not reported loading the file. With Jedi Academy multiplayer the
server likewise loaded the override and the editor reported it, but the
client then stopped rejoining the reloaded map at `Academy cgame Ghoul2
renderer requires one owned client model; attachments remain open` -- as it
does for any server map change, which a plain `rcon devmap mp/ffa1` with no
editor showed.

Jedi Academy's multiplayer client refuses to host a level in its own
process, so a scratch runner started a fnTech3 dedicated server on loopback
with `devmap mp/ffa1` and connected the client: the editor opened over the
229 entities, made one, saved `maps/mp/ffa1.ent` (230 entities) in the
client's home and, holding no rcon password, refused Apply for a server
outside the process, as designed.

For the profiles whose client cannot run a level here,
`scripts/probe_entity_override_server.py` shows the dedicated server's game
module spawning an override. It writes the level's entities with
`sv_saveentfile` (or, `--from-bsp`, takes the map's own lump), appends a
marker entity whose classname no game knows, and starts the map again: the
server must load the override and the game module must report the marker
it cannot spawn, while a control run with `sv_entityOverrides 0` must do
neither. Each run has its own home, networking off; the installation is
inventoried before and after.

| Profile | Map | Override | The game module's own report of the marker |
|---|---|---|---|
| q3-ihv | ihv_test1 | 52 entities (51 saved + marker) | `fntech3_override_marker doesn't have a spawn function` |
| jk2-mp | ffa_bespin | 446 (445 + marker) | the same, from `vm/jk2mpgame.qvm` |
| jka-mp | mp/ffa1 | 230 (229 + marker) | the same, from `jampgamex86.dll` |
| jka-sp | t1_sour | 1,417 (the BSP's 1,416 + marker) | `fntech3_override_marker is not a spawn function @(0 0 0)` |

The same three runs went through a Linux i686 dedicated server (WSL 2,
Ubuntu 24.04, GCC 13, glibc 2.39), built with the legacy Makefile from the
working tree's server sources (only another session's client-allocation
change to `sv_init.cpp` has landed since), against the Steam Quake III
installation: `sv_saveentfile` wrote q3dm17's entities with the SHA-256 the
Windows runs give (`3063891b17639c53...`), the server loaded the override
(12,009 bytes, 154 entities), Quake III's QVM game module reported the
marker, and the control run did neither. That build was given
`-D_FILE_OFFSET_BITS=64` through `CC` and `CXX`: Meson adds it to every
Linux compile, and the Makefile then did not, so the 32-bit file listing
failed with `EOVERFLOW` on the mount's 64-bit inode numbers and the server
found no pak. A separate change made the same day adds it to the Makefile
and CMake.

The editor itself ran on Linux too: an i686 client built with Meson (the
GLx renderer, bundled subprojects) from the same working tree ran Quake
III's seven steps above under an Xvfb display, through Mesa's llvmpipe,
with an isolated home and the Steam installation read through `/mnt/c`.
It made, keyed, moved, undid and redid the `weapon_bfg`, saved
`maps/q3dm17.ent`, loaded the map again for the structural Apply and
restarted it with `map_restart 0` for the value-only one, the server
loading the override both times, and its five captures show the same
interface as on Windows; the installation was unchanged.

Jedi Academy's single player was run with `--from-bsp --expect-drop`: its
module's `Init` spawns the level, and the first frame then stops at
`WE_IsShaking` (import 128), as the ledger records, before `sv_saveentfile`
could run. Jedi Outcast's single player stops earlier, at client import 3,
before its module initialises or any entity text is chosen, so it has no
such run.

Medal of Honor and the Ritual titles count `wait` in milliseconds, so the
probe times their steps in milliseconds; Medal of Honor's loading screen
needs `finishloadingscreen` and Alice's opening `skipcinematic` before their
worlds show.

A dense view checks the command budget (`--dense`): Elite Force's `hm_voy1`
(1,041 entities) seen from above all of them, every label on. With the
editor's first, engine-drawn widgets and the first bounds (320 boxes, 900
crosses, 96 labels, 256 links) the frame overflowed the buffer and the
renderer dropped the panels' commands: the capture shows stale panel pixels
from before the camera moved (labels "near", the player's position) wherever
the view left the void undrawn, and the world elsewhere. With the budget and
the ImGui interface the same view drew 48 boxes and 286 dots in 2,423
commands, about 209 of 320 KiB, nothing left out, and every panel whole;
RTCW's `mp_beach` (871 entities) drew 48 boxes and 251 dots in 2,700
commands, about 229 KiB.

Quake III's run also passed at 1024x768, 1280x720 and 2560x1080: the panels
follow the screen's height (scale 0.75 at 768 and 720 lines, 1.25 at 1080),
and the view keeps the rest -- 484 pixels wide at 1024x768, 1,660 at
2560x1080.

## Box alignment, 2026-10-06

**Observed.** In the runs above the boxes did not lie on what they marked
on any screen but a 4:3 one. In Quake III's capture at 1600x900 the
`ammo_rockets` box stood about a third farther from the centre of the view
than the ammunition it marks, the `trigger_teleport` box beside the
teleporter rather than around it; at 2560x1080 farther still, and at
1024x768 they matched. Clicks, box selection and **M** pick through the
same projection, so they missed alike.

**Cause** (source). `CL_EntityEditorAdjustView` took the view it projects
the overlay with from the view the client hands the renderer, and every
renderer then widens a 4:3-authored field of view for the screen's shape
inside `RE_RenderScene` (`R_ApplyProfileFovCorrection`, `r_fovCorrection 1`
by default in every profile but Q3Test, the IHV Test and Jedi Outcast's):
Quake III's 90 by 58.7 degrees at 16:9 is drawn at 106.26 by 73.74. The
overlay was therefore scaled about the centre by the ratio of the two
fields' tangents, 4/3 at 16:9 and 1.78 at 2560x1080. A view the editor
widened to the whole screen (Enemy Territory's limbo camera) was widened by
the renderer a second time.

**Fix.** The editor hands the renderer a field of view of its own, flagged
`RDF_NOFOVCORRECTION` so that it is drawn as given, and projects with that
same view: `cl_entityEditorFov` (90 by default, **View > Field of view**)
across a 4:3 screen, its height kept on another (`ee::FitFieldOfView`),
the shape being the one the screen shows (`glconfig.windowAspect`; the
render's own with `r_renderScale` 2 or 4). Its values are the renderers'
own for a game's 90 (106.260205 by 73.739795 degrees at 16:9, 121.284493
across 2560x1080, `tests/q3_fov_profile_tests.py`), so the editor's world
is framed as the game's is by default. A zoomed scope's or an underwater
warp's field no longer carries into the editor's camera, and every view the
editor draws, a sky portal's and the first before its camera is placed,
takes it. Inferred from the source, not measured: Medal of Honor's model
detail and Enemy Territory's foliage distance, which the renderers judge by
the field a game gave before widening it (`fovXAuthored`), judge the
editor's view by its widened field, so on a wide screen those may be drawn
a little coarser in the editor than in play.

The boxes themselves:

- **Brush entities**: the collision map spreads each brush model's bounds
  by a unit (`CMod_LoadSubmodels`); the box is now the brushes' own.
  Brush entities rotated by `angles` are drawn unrotated: of the 17,723
  brush entities in the installed Quake III, RTCW, Enemy Territory, Alice,
  F.A.K.K.2 and Medal of Honor maps, 26 have a non-zero one (`angle` on
  the others is a door's or a button's direction, not a rotation).
- **Point entities**: each game's box about the origin, where the editor
  had drawn Quake III's in every game, centred:

  | Lineage | Spawn point | Item | Other points |
  |---|---|---|---|
  | Quake III, Quake Live, Q3Test, IHV, Elite Force, the Jedi games | (-16 -16 -24)-(16 16 32), Quake III's `QUAKED` box, shared by Elite Force's and OpenJK's sources | 15 about it (`ITEM_RADIUS`) | 8 about it |
  | RTCW, Enemy Territory | the player, (-18 -18 -24)-(18 18 48) (`playerMins`, `playerMaxs` in RTCW's and ET's `g_client.c`; ET's `QUAKED` box) | (-10 -10 0)-(10 10 10), or 10 about it when it hangs (`FinishSpawningItem`, `ITEM_RADIUS` 10) | 8 about it |
  | Alice, F.A.K.K.2, Medal of Honor | (-16 -16 0)-(16 16 96) (F.A.K.K.2 SDK `PlayerStart.cpp`, OpenMoHAA `playerstart.cpp`; OpenMoHAA's player is 94 high, F.A.K.K.2's 72) | (-10 -10 0)-(10 10 20), `Item::Item`'s least box | (-8 -8 0)-(8 8 16) for models; 8 about it for `script_origin`, triggers and `info_`, `target_`, `path_` and `func_` points; `info_pathnode` (-24 -24 0)-(24 24 32) and `info_grav_pathnode` (-16 -16 0)-(16 16 32) |

  Observed in the reference sources named, used as reference observation
  only. Inferred:
  as Ritual's and 2015's game code sizes an item from its model standing on
  its origin, and their definitions stand players and path nodes on theirs,
  their other model entities stand on theirs too; the editor does not read
  the models. Path nodes are those maps' most common entity (26,432 in
  Medal of Honor's 54 maps). Alice's and F.A.K.K.2's `func_spawn`, a point
  that makes things, had been drawn as a spawn point.
- **Placement**: **M**, a new entity and a lone pasted one rest the box on
  the surface the view meets, at the grid step nearest it on its near side
  (flush with snapping off), where their origins had stood 16 units off it
  whatever their box; computed from the earlier code for the floors below,
  a spawn point made on the Alice floor would have stood 19 units above it
  and one on the Quake III floor sunk 8 units into it. **End** sweeps the
  box down until any part of it rests on the world, flush, where a point
  traced from the box's middle had left it an eighth of a unit up.
- **Framing** (**F**, a double click, `entityeditor look`) aims at the
  box's middle, where it had aimed at the origin, the feet of an entity
  that stands on it.
- **Turning** left an `angle` of -1 or -2 alone (up and down to
  `G_SetMovedir` and Ritual's `G_GetMovedir`): a turned -1 had become an
  ordinary yaw, sending a door or a push sideways.
- An origin of `nan` or `inf` stands nowhere: a brush entity's had sorted
  with a NaN distance, undefined behaviour in `std::sort`.

**Runtime** (the `meson/build/win32-dev` build named in the companion,
`scripts/probe_entity_editor.py` as above). Every run passed: Quake III at
1600x900 under GLx, Vulkan and RTX, at 2560x1080 and at 1024x768, and
Quake Live, Enemy Territory, RTCW multiplayer and single player, Elite
Force Holomatch and single player, Q3Test 1.08, Alice, F.A.K.K.2 and Medal
of Honor. In the Quake III captures the boxes lie on the ammunition, the
teleporter and the lights at each size, the world framed as before.
`.tmp/ee-align/align_probe.py`, by console commands alone, framed a
selected entity (`entityeditor look`: Alice's `garden1_start1` spawn point,
Medal of Honor's #18, Quake III's rocket launcher), each box then at the
centre of the capture, and aimed the camera straight down at a floor, made
entities and dropped one from 96 units up:

| Game, floor | Made | Origin z |
|---|---|---|
| Quake III, q3dm17 at 288 600, floor 328 | `info_player_deathmatch`, `item_health`, `weapon_railgun`, `info_notnull`; the last dropped from 432 | 352, 344, 344, 336; 336 |
| Medal of Honor, dm/mohdm1 at 848 -216, floor 0 | `info_player_deathmatch`, `item_health_25`, `info_pathnode`, `script_origin`; the last dropped from 104 | 0, 0, 0, 8; 8 |
| Alice, garden1 at -1496 -2472, floor -411.125 | `info_player_start`, `item_health_small`, `script_origin`, `enemies_boojum`; the last dropped from -312 | -408, -408, -400, -408; -411.125 |

Alice's floor lies off the grid, so what is made stands on the grid step
above it and what is dropped rests on it.

## Tests

- `fntech3_entity_editor` (C++ unit: the document, the selection and its
  group edits, `SpawnsAlike`, the draw-list conversion's clipping, winding
  and quad merging, and a headless ImGui frame through it) and
  `fntech3_entity_override` (tokenizer parity, source wiring, the Apply and
  menu rules, the pinned wrap, and the overlay's worst case against every
  renderer's command buffer and command layouts), both registered in Meson.
- Fixtures that compile the changed dispatch and adapter functions gained the
  closed editor's behaviour as stubs: views pass unchanged, entity tokens come
  from the renderer, and no event is the editor's.
- Of the build's 253 Meson entries besides the pytest wrapper, 249 passed, 2
  skip by design, and 2 timed out while another repository's builds loaded
  the machine: `fnql_audio_zone_material_map` passed alone in 9.5 s, and
  `fntech3_jka_pose_host`, which reads nothing this work changed, passed run
  directly in 43 s, beyond Meson's 30 s limit.
- The full pytest suite: 2,530 passed, 126 skipped, 62,199 subtests passed.
  Its 22 failures (14 tests and 8 subtests) lie in four files this work did
  not change: `release_packaging_tests.py` stops at another session's record,
  which links a JSON companion not yet written, and
  `curl_multi_failure_tests.py`, `stvef_sp_commands_tests.py` and
  `wolfet_map_load_tests.py` failed while other sessions edited the shared
  tree and passed alone afterwards. Every relative link in the documents this
  work changed resolves.
- The suite ran before the last edits (the full-screen view, Apply over
  rcon, menu tooltips, the filter's hint, the Duplicate button's name,
  source tests and documentation); the editor's Meson targets,
  `tests/entity_override_tests.py`, `tests/stvef_sp_menu_tests.py` (which
  pins the print handler the rcon reply passes through) and the
  documentation tests ran again on the final build, and every probe above
  ran on it.
- For the box alignment (2026-10-06), `fntech3_entity_editor` gained each
  lineage's point boxes, the up and down angles, and the editor's field of
  view against the renderers' own values, and `fntech3_entity_override`
  the view the editor hands the renderer and the brush boxes without the
  collision map's spread. Both, `tests/q3_fov_profile_tests.py` and the
  documentation tests passed on the 2026-10-06 build, on which that
  section's probes ran; the sections before it describe the 2026-10-05
  build.

## Open

- Jedi Academy multiplayer applies over rcon to a dedicated server, but its
  client cannot yet host a listen server (`Academy cgame bolt queries
  require the measured remote-client frame clock; local-server clock
  sharing is not served yet`) nor follow a server's map change (its Ghoul2
  attachment boundary), so after an Apply it must reconnect. Both are
  limits of its profile, met without the editor too.
- The IHV Test, Jedi Academy's single player and both Jedi Outcast modes
  have no editor run, because their clients stop before any level: the
  IHV client is blocked by its profile, and started here the others stop
  at startup (`jka-sp is not implemented: Jedi Academy's single-player
  engine menu boundary is open`, the same for `jk2-sp`, and `Unsupported
  jk2-mp UI import 200: Jedi Outcast's localization/Ghoul2 boundary is
  open`). The dedicated servers of all but Jedi Outcast's single player
  spawned an override's marker in their own game modules; that one stops
  before its module initialises, so it has Source and Unit evidence only.
- The client-side entity tokens following a local override are shown for
  Enemy Territory and RTCW multiplayer; Jedi Academy's need the listen
  server its client cannot yet host.
