# Jedi HUD and menus on any screen shape

Date: 2026-10-15. Profiles: `jka-mp` (HUD and menus), `jk2-mp` (menus and
client-game fonts). Platform: Windows 11, the Windows x86 build. Companion:
[`2026-10-15-jedi-widescreen-hud.json`](2026-10-15-jedi-widescreen-hud.json).

Jedi Academy's HUD was not scaled to the window at all, at any screen shape,
and both Jedi games' menus were broken on wide screens. Both games' 2D now
reaches the window as the originals put it there, and Academy's HUD takes
fnTech3's widescreen HUD layout. No compatibility gate closes: this is
presentation, and the Outcast HUD and both single-player modes remain out of
reach.

## What was wrong (observed)

Engine captures of HEAD `750f070f` (runtime `.tmp/runtime/jka-hud-before`,
GLx, windowed, no input), local `mp/ffa1` free-for-all after `team free`:

- At 960x720, a 4:3 window, Academy's whole HUD -- health and armour gauge,
  force and ammunition gauge, score, the leader's portrait -- filled only the
  top-left 640x480 pixels (`before-play-960`). At 1680x720 the same
  (`before-play-1680`).
- At 1680x720 Academy's and Outcast's main menus were squeezed into the
  top-left corner and pushed past the window's left edge
  (`before-menu-1680`, `before-jk2menu-1680`).

## Why

Observed in the retail executables:

- `jamp.exe` 1.01 (SHA-256 `e8062d39...`) sets up its 2D projection at
  `0x48b09c..0x48b0e5`: `glViewport(0, 0, [0xfe3180], [0xfe3184])`, then
  `glMatrixMode(GL_PROJECTION)`, `glLoadIdentity` and
  `glOrtho(0, 640, 480, 0, 0, 1)` through the pointer at `0x10bc830`, which
  `0x4cc25f` stores from `GetProcAddress("glOrtho")`. That the two viewport
  words are the window's width and height is inferred: they sit at +56 and
  +60 of a record at `0xfe3148`, where the 76-byte glconfig its UI import
  copies keeps them.
- `jk2mp.exe` 1.02a (SHA-256 `2f08dcf9...`) does the same at
  `0x46da51..0x46da9a`: the viewport from `0x1a4f970`/`0x1a4f974`, then
  `glOrtho(0, 640, 480, 0, 0, 1)` through `0x1ae57a0`.

So both engines draw every 2D picture in a fixed 640x480 virtual screen
stretched over the window, and their client games, menus and fonts hand the
engine virtual coordinates. fnTech3's renderers draw 2D in window pixels, as
Quake III's lineage does, where the modules scale their own coordinates.

Inferred from the source and the captures: the Jedi hosts passed the virtual
coordinates through as pixels, which is the top-left 640x480 of any window.
The shared menu path then applied Quake III's undo of its centred 4:3 menu
(`CL_UIAdjustStretchPic`, `cl_menuAspect 0`) to coordinates that had never
been centred, which pushed the wide-screen menus off the left edge. Academy
was also missing from the HUD correction (`hudSupported` in
`code/client/cl_hud.cpp`), had no layout file, and its client game was told
the real window, so its projections would not have met the widened view.

## What changed

- `code/client/cl_hud.cpp`: Academy multiplayer joins the HUD correction.
  `CL_HudFromVirtual` maps Raven's virtual screen to the pixels of the screen
  the client game was told it has, so every later step -- the rules,
  `cl_hudAspect 0`, the dump -- is the one every other game uses.
  `CL_HudBeginTextLine`/`CL_HudEndTextLine` let the engine's own font drawing
  place a whole string as one line when a layout asks for `textRuns`.
- `code/client/client_jka_cgame_host.hpp`: client-game pictures (import 211)
  and font strings (import 62) come from the virtual screen; the client game
  reads the HUD's 4:3 frame as its screen (`CL_JKAGetGlconfig(..., true)`),
  as Elite Force single player does, so the points it projects onto its 2D
  -- the dynamic crosshair among them -- meet the widened view. Its menus
  keep reading the real window.
- `code/client/client_jka_font.hpp`: one layout loop (`CL_JKALayFont`) now
  serves drawing and measuring (`CL_JKAFontArea`), so the area handed to the
  HUD is exactly what is drawn; invalid arguments stop before a host hears of
  a line.
- `code/client/cl_ui.cpp`, `client_jka_ui_host.hpp`, `client_jk2_ui.hpp`,
  `client_jk2_ui_render.hpp`: both Jedi games' menus, their fonts and their
  own 3D views go from the virtual screen to the window: stretched as the
  originals did with `cl_menuAspect 0`, the default outside Quake Live, or in
  the centred 4:3 area with `cl_menuAspect 1`. A menu's 3D view, which the
  module placed in stretched window pixels, goes back to its virtual
  rectangle and moves with the menu.
- `code/client/client_jk2_cgame.hpp`: Outcast's client-game fonts are drawn
  from its virtual screen, stretched as the original drew them.
- `pkg/games/jka-mp/base/fntech3-hud.json`, `scripts/root_archive.py`,
  `meson.build`, `code/qcommon/files.c`: Academy multiplayer's layout ships
  in the package, and the package reader admits it for that profile only.

## Presentation

The path uses fnTech3's own presentation features (invariant 11): the HUD
layout system (`cl_hudAspect`, `fntech3-hud.json` rules, whole-line text,
centred panels, `cl_hudDump`), the 4:3 frame for a client game that projects
onto its 2D, `r_fovCorrection`'s widened view, which already covered `jka-mp`,
and `cl_menuAspect`. The retail look stays available as options:
`cl_hudAspect 0` draws the stretched HUD, `cl_menuAspect 0` (the default)
draws the stretched menus, and `r_fovCorrection 0` keeps the original field
of view. Nothing in the path is left out for Jedi.

## Academy's layout

The rules are in Raven's 640x480 units. The parts seen live come from engine
dumps (`cl_hudDump`) of `final-play-1680` and `final-panels-1680`; the
positions of parts no probe can reach yet are reference observations of the
GPL multiplayer client-game source in OpenJK `1a6a6434`
(`codemp/cgame/cg_draw.c`, `cg_scoreboard.c`, `cg_info.c`, `cg_weapons.c`),
not retail measurements.

| Rule | Matches | Placed | Evidence |
| --- | --- | --- | --- |
| `crosshair` | `gfx/2d/crosshair*` | centred, never pulled to an edge | dump; the dynamic crosshair (`cg_dynamicCrosshair 1`) follows its target anywhere on screen |
| `scoreboard` | `gfx/menus/menu_buttonback*`, a picture | the rest of the frame one centred 4:3 screen | dump and capture of `+scores`; the scoreboard's first unconditional picture (`cg_scoreboard.c`) |
| `loading_levelshot`, `loading_unknown_map` | `levelshots/*`, `menu/art/unknownmap*` | stretched, then a centred 4:3 screen | reference (`cg_info.c`), as Quake III's loading screen; not captured |
| `votes` | text starting at the left edge, y 40-120 | left, top | reference: votes at x 4 |
| `chat` | text starting at x 26-36, y 200-480 | left, bottom | capture: chat lines at x 30 |
| `selection_bars` | centre in x 100-508, y 395-465 | centred, bottom | reference: weapon, force and inventory bars reach x 508; kept whole |
| `left_hud`, `left_side`, `left_column` | the lower-left HUD; flags above it; holocrons down the left | left; bottom, bottom, top | dump: `gfx/hud/hudleft` at (0,368,112,112) and its tics and numbers |
| `right_hud`, `right_side` | the lower-right HUD and score line; pickups and fuel bars above it | right, bottom | dump: the mirrored `gfx/hud/hudleft` at (640,368,-112,112), its tics, the score line at x 468-513 |
| `upper_right_text`, `upper_right` | text ending at the right edge; the upper-right stack | right, top | dump: the leader's portrait at (575,5,60,60) and its name lines |

## Evidence

All runs: `.tmp/jka-hud/probe_hud.py`, the engine's `screenshotPNG`, a
windowed client on an unswitched private desktop, `net_enabled 0`,
`s_initsound 0`, an isolated home with an empty `jampconfig.cfg`
(`jk2mpconfig.cfg` for Outcast), no input sent, the installations
inventoried before and after (unchanged in every run). Runtimes: before is
HEAD `750f070f`, after is HEAD plus exactly this change, both built from one
export with the shared developer options; hashes are in the companion.

| Run | Window | Renderer | Result |
| --- | --- | --- | --- |
| `before-play-960`, `before-play-1680` | 960x720, 1680x720 | GLx | HUD in the top-left 640x480 pixels |
| `before-menu-1680`, `before-jk2menu-1680` | 1680x720 | GLx | both main menus squeezed and pushed off the left edge |
| `final-play-1680` | 1680x720 (21:9) | GLx | HUD in shape at the corners, view widened; `cl_hudAspect 0` mid-level draws the frame stretched, as retail; scoreboard centred |
| `final-play-1280`, `final-play-960`, `final-play-900` | 16:9, 4:3, 5:4 | GLx | HUD fills a 4:3 window and keeps to its corners on wide and tall ones |
| `final-play-1680-hudoff` | 1680x720 | GLx | `cl_hudAspect 0` from startup: the stretched HUD of the original, over the widened view |
| `final-panels-1680` | 1680x720 | GLx | a chat line at the left edge; scoreboard dump |
| `final-menu-1680`, `final-jk2menu-1680` | 1680x720 | GLx | menus stretched as the originals; centred 4:3 with `cl_menuAspect 1`, their 3D views moving with them |
| `final-play-1680-vk`, `final-play-1680-rtx` | 1680x720 | Vulkan, RTX | the same placement |

Academy's main menu globe is a video-mapped shader (`gfx/menus/videologo`,
`ui/jamp/main.menu`), whose planets leave the picture within a second of the
15-second capture; the centred menu shows them at that moment
(`after-menu-1680-centred`).

Tests: `tests/hud_aspect_tests.py` runs the real HUD module with Academy's
shipped rules (virtual mapping, the 4:3 frame, whole-string lines, the
selection bar, the corner crosshair, the scoreboard and loading panels, the
stretched fallbacks); `tests/jka_font_host_tests.cpp` checks the measured
string area against the drawn glyphs and the line hook's order and refusal;
`tests/ui_viewport_tests.py` maps menu pictures from the virtual screen,
stretched and centred, on wide and tall screens in ordinary and fast-math
builds; `tests/jka_render_host_tests.py` runs Academy's menu 3D-view mapping
on wide, 4:3 and tall screens through the production menu cases, and
`tests/jk2_ui_render_tests.py` checks Outcast's menu views take the same
mapping; `tests/root_archive_tests.py` and `tests/release_packaging_tests.py`
cover the package, and `tests/launch_and_transition_source_tests.py` the list
of corrected games.

On `750f070f` plus this change, all 269 native tests built with the shared
strict options and passed (two skipped), and the whole Python suite ran:
2,731 passed and 22 failed, all from this change's own snapshot. The
packaging tests ran before this record's companion existed, which the
packager requires for its link, and still expected 838 package assets; the
FAKK2 UI source test pinned the package reader's HUD gate; and the shared
render-record fixture had no stubs for the new menu helpers. All three were
fixed. On `aa0e7049` plus this change the build and the native suite pass --
four native tests unrelated to this change timed out while other sessions
ran their own suites, and pass when rerun alone -- and so do every Jedi
Python test and every other file this change touches (81 files: 318 passed, one skipped).
Results are in the companion.

## Not covered, and found along the way

- Outcast's HUD: its client game's renderer imports (42-218) were still
  refused, so it had no HUD to place; its fonts drew stretched from the
  virtual screen as the original's do. The
  [client services](2026-10-16-jk2-cgame-services.md) and
  [Ghoul2 and HUD](2026-10-16-jk2-cgame-ghoul2.md) slices now draw it, in
  this layout's 4:3 frame with Outcast's own rules.
- Neither Jedi single player reaches its menus or client game.
- At `750f070f`, weapon switching (`weapnext`) stops the client at
  client-game import 36, `CG_S_MUTESOUND`, which is not served there: a
  separate gap (`after-panels-1680`), served since `aa0e7049`. The
  selection bars and the loading screen were therefore placed from the
  reference layout and unit samples. A later
  [live check](2026-10-15-jka-hud-live-check.md) captured both: they land as
  placed, and each bar's name now goes down with its bar on screens taller
  than 4:3.
- The disruptor and binocular masks are full-screen pictures and stay
  stretched across the window, as the original drew them. The
  [live check](2026-10-15-jka-hud-live-check.md) found any zoom stopping the
  client at client-game import 216, `CG_R_SETRANGEFOG`; the
  [zoom slice](2026-10-15-jka-zoom.md) serves imports 214-216, and both zooms
  now draw with every piece registered to its stretched mask.
- The menu cursor in the top-left corner of the play captures is the menu
  module's own: it keeps its key catcher after `ui_openmenu none`, and
  retail releases it only when Escape closes the menus
  ([weapon-fire record](2026-10-15-jka-weapon-fire.md)).
- The siege objective's 3D icon is placed in unscaled pixels by the client
  game itself (`CG_DrawSiegeHUDItem` in the reference); with the frame it
  keeps the original's pixel size.
- Like Elite Force single player, the client game reads its screen once, so
  switching `cl_hudAspect` takes full effect at the next map or `vid_restart`.
