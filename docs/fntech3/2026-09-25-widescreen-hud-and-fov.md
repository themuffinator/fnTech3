# Widescreen HUD and FOV for every title

## Scope

Before this record only Quake III Arena and both RTCW modes had FnQuake3's
world FOV correction (`r_fovCorrection`) and HUD correction (`cl_hudAspect`
with `fntech3-hud.json` rules). Quake Live's and Medal of Honor's modules
already place their HUDs for any screen. Quake Live's also fit their field
of view, but only to four screen shapes (see Quake Live below). Every other title stretched its HUD with the screen and kept
its 4:3-authored horizontal field of view, so on a wider screen the view was
cropped at the top and bottom.

This record extends both corrections to Enemy Territory, both Elite Force
modes, Alice and F.A.K.K.2, and the FOV correction to Medal of Honor. They are
FnQuake3 presentation features (AGENTS.md invariant 11). No module ABI, wire,
demo, save or pak contract changes. `cl_hudAspect 0` and
`r_fovCorrection 0` restore retail's stretched presentation. The Quake III
IHV Test and the Q3Test releases, and Jedi Academy and Jedi Outcast (no client
yet), are not covered.

## What the engine does

- **FOV** (`code/renderercommon/tr_q3_fov.h`, all three renderers). The
  correction keeps the vertical framing of a horizontal FOV authored for a
  4:3 screen. It now serves `wolfet`, `stvef-sp`, `stvef-mp`, `alice`,
  `fakk2` and `mohaa` besides `quake3`, `rtcw-sp` and `rtcw-mp`.
  - The authored shape is now the view's shape on a 4:3 screen, not always
    4:3. A game sizes its view with the screen, so a view it cuts in one
    direction only (a letterbox) was authored at that cut's 4:3-screen shape.
    A full-screen view gives exactly the old result for Q3 and RTCW, and a 4:3
    display is never changed (`tests/q3_fov_profile_tests.py`).
  - Menu scenes keep their authored FOV in Elite Force as in Quake III
    (`cl_ui.cpp`, `client_stvef_sp_ui.hpp`).
  - Alice's, F.A.K.K.2's and Medal of Honor's views reach the renderer with
    only their world and hyperspace flags, and ET's and Elite Force's through
    per-profile flag translation (`render_record_adapter.hpp`). No module flag
    can collide with the engine's `RDF_NOFOVCORRECTION` bit.
- **HUD rules** (`code/client/cl_hud.cpp`). They now serve Enemy Territory,
  both Elite Force modes and both Ritual titles. Three additions:
  - A rule mode `extend`: uniform placement, then each edge touching the
    640x480 frame carried out to the screen's edge. It is for masks beside a
    centered picture: a scope's side bars, a cutscene's letterbox bars.
  - A per-frame game panel state set by the client (`CL_HudSetGamePanel`).
    Centered pauses the rules, so a screen of the game's own stays one 4:3
    composite. Stretched draws the frame as retail did.
  - A 4:3 frame reported to a client game (`CL_HudModuleScreen`). The HUD
    code then reads that game's pixels in its frame and places its world
    views, as described under Elite Force single player below.
- **Alice's HUD** (`code/client/cl_alice_hud.cpp`). Each widget's
  whole-screen view goes through the rules and becomes a 4:3 screen's. The
  model is sized for that view as retail sizes it for a 4:3 screen. Retail's
  rule, `(4/3) / (W/H)` times the widget's scale, drew each model 1.75 times
  larger at 21:9 and spread the widgets apart.

## Per title

### Enemy Territory (`wolfet`)

- *Observed* in the GPL 2.60 source, used for reference only (GPLv3; no text
  incorporated):
  - The retail cgame stretches its 640x480 draws: its widescreen offset is
    compiled out.
  - `CG_CalcFov` derives `fov_y` from the view's pixel shape.
  - The HUD's rectangles are in `cg_draw.c` (bottom block at y 388..476,
    compass 508,8,124,124, timer boxes from y 152).
  - The limbo panel replaces `CG_Draw2D`. It holds a world view at
    287,382,128,96, and `cg.showGameView` reaches the engine as bit 1 of
    `CG_SETUSERCMDVALUE`'s flags.
  - The scoreboard is drawn inside `CG_Draw2D` with no key catcher change.
  - The expanded command map slides in over 250 ms.
  - The debriefing replaces the HUD at intermission (`pm_type` 5,
    `PM_INTERMISSION`).
- *Observed* at runtime: a `cl_hudDump` of `oasis` at 960x720 gave the stock
  HUD's groups, which the preset's regions enclose.
- The preset anchors the corner blocks, keyed to their own art wherever that
  art is shared. Popups, prompts, chat, votes and center text stay centered,
  because the rules classify each glyph by its center and an anchored line
  would split where it crossed a boundary.
- The client sets the panel state (`CL_WolfETHudPanel`, `cl_cgame.cpp`):
  - limbo: stretched, as retail;
  - intermission, `+scores` and `+mapexpand`: centered, held 250 and 300 ms
    after release for the fade and the slide.

### Elite Force single player (`stvef-sp`)

- *Observed* in Raven's single-player SDK and the Elite-Reinforce port (reference
  only):
  - The cgame projects world points onto its 2D (the crosshair's target
    brackets, the tricorder's labels) with the `fov_x`/`fov_y` it computed
    from its view's pixel shape.
  - Its LCARS gauges animate in about three seconds after spawning.
  - Cutscene letterbox bars are drawn over a full view, in pixels.
- *Inferred*: correcting the renderer's FOV alone would put those projected
  points off by the screen's aspect over 4:3 vertically, 1.75 times at 21:9.
- With `cl_hudAspect 1` the cgame's `CG_GETGLCONFIG` reports the 4:3 frame
  (`640*scale` by `480*scale`), so it computes a 4:3 view and 4:3
  projections. The engine then:
  - fills the screen with its full view (the renderer widens it);
  - keeps a smaller view (`cg_viewsize`) centered at its 4:3 shape with the
    FOV it was given;
  - maps its pixels, the loading screen's picture included.

  Lilium Voyager handles its Holomatch interface the same way (reference
  only). The setting is read at client-game start.
- The preset anchors the gauges by their own art, since the slider art is
  also the talk box's and the zoom ticks'. It extends the cutscene letterbox
  bars to the screen's edges. The talking head stays with its talk box, which
  is centered.

### Elite Force Holomatch (`stvef-mp`)

- No Holomatch cgame source was available. The layout comes from the
  runtime `cl_hudDump` of `hm_voy1`: the same gauges as single player, and a
  score box at 605..640, 389..413.
- *Inferred*: no projected HUD labels are known in Holomatch, so its cgame is
  not given the 4:3 frame.

### Alice (`alice`) and F.A.K.K.2 (`fakk2`)

- Alice's widgets are views of the whole screen shifted by their offsets
  (`docs/fntech3/2026-09-24-alice-hud.md`). The preset matches each view's
  center: health and mana at the left and right edges centered vertically,
  and the cards at the bottom corners.
- F.A.K.K.2's HUD menus were covered in the previous step
  (`2026-09-24-fakk2-contracts.md`).
- Both titles' world views now take the FOV correction.

### Quake Live (`quakelive`)

- *Observed* (a temporary log of the view QL's retail client game hands the
  renderer, `cg_fov 100`, `campgrounds`):

  | View | fov_x | fov_y | `r_aspectRatio` |
  | --- | --- | --- | --- |
  | 960x720 (4:3) | 100.000 | 83.582 | 0 |
  | 1024x640 (16:10) | 110.074 | 83.582 | 2 |
  | 1280x720 (16:9) | 115.634 | 83.582 | 1 |
  | 1680x720 (21:9) | 115.634 | 83.582 | 1 |

  QL keeps a 4:3 view's vertical field and widens the horizontal one for the
  preset its engine publishes (`cl_main.cpp` CL_InitRenderer, retail's
  thresholds). Forcing each preset at 21:9 gave 100.000 for 0, 110.074 for 2
  and 96.340 for 3, all with 83.582. So preset 0 widens nothing, as the retail
  HLIL of `0x1004c5f0` shows; QLSRP's "auto" reading of 0 is not retail's.
- The pair fits the view only on a preset's own shape. At 21:9 the 16:9
  field is spread across 21:9, stretching the world sideways by 1.31 times.
- *Observed* in the retail HLIL: the client game writes its view's field at
  `0x1004d85b` and reads it nowhere else. It projects points onto its 2D
  through the renderer's own transforms (`R_TransformModelToClip`,
  `R_TransformClipToWindow`, import slots 120 and 121), after rendering the
  scene (`0x10011680`). QLSRP's `CG_WorldCoordToScreenCoord`, which uses
  the field itself, is not retail's path.
- The renderer now registers `r_fovCorrection` for QL with a rule of its
  own (`R_CorrectQLViewportFov`). It widens QL's horizontal field by the
  view's shape over the preset's and keeps its vertical field and underwater
  wobble. On a preset's own shape it changes nothing; `0` draws retail's
  field. QL's projected markers and teammate icons follow, since they come
  through the renderer.
- *Observed* at runtime from the same spot, comparing the middle 960 columns
  of each capture with 4:3's:
  - 21:9 now matches 16:9 (mean difference 0.5, against 9.97 before);
  - 3:2, which publishes the 4:3 preset and was stretched too, now matches
    4:3;
  - the 4:3 and 16:9 captures still differ from each other by texture
    filtering at a different render width, with no edge out of place.

### Medal of Honor (`mohaa`)

- Its modules place its HUD for any screen, so the HUD rules stay off.
- Its world view takes the FOV correction. The level-of-detail tool still
  receives the view's authored `fov_x`.
- *Observed*: the widened field made the first-person arms vanish at 21:9
  while the rifle stayed. With `r_nocull 1` they returned. A temporary log in
  the renderer's MoHAA model path showed why:
  - The culled model was `models/player/american_army_fps.tik`, the
    first-person body.
  - Its unposed bounds box, the standing body's, lies 8 to 16 units behind
    the eye; only its pose brings the arms forward.
  - Behind the eye, a frustum's side planes reject more as the field
    widens. At 80 degrees across, the box's front corner was still inside the
    left plane (`CULL_CLIP`). At the corrected 111 degrees it was outside it
    (`CULL_OUT`).
- The renderers no longer frustum-cull a depth-hacked (first-person) MoHAA
  model (`tr_mohaa.c`). OpenMoHAA culls these models by their posed skeleton
  instead (reference only).
- The model-detail metric, and ET's foliage distance, now read the field the
  game gave (`tr.refdef.fovXAuthored`), not the widened one. A model then
  keeps the detail it has at 4:3, where it fills the same share of the
  screen's height. With the cull fixed, the 4:3 capture is byte-identical to
  the one before either change.

## Evidence

Working tree over `d1d98ba` (other sessions' uncommitted work present),
Windows x86.

| Build | fntech3.exe | fnql_glx_x86.dll |
| --- | --- | --- |
| `meson/build/win32-fakk2` (every title but MoHAA) | `86e7c91b215ead20...` | `46ba339fcb24ea5d...` |
| `meson/build/win32-mohaa` | `a4248a5d5494eeb6...` | `7f556c7b4d31ff82...` |

| Preset | SHA-256 |
| --- | --- |
| `wolfet/etmain/fntech3-hud.json` | `e85ad0d256284275...` |
| `stvef-sp/baseEF/fntech3-hud.json` | `2b662981cd200686...` |
| `stvef-mp/baseEF/fntech3-hud.json` | `fe7b0f369f6b87f4...` |
| `alice/base/fntech3-hud.json` | `78e298c8f12346e6...` |
| `fakk2/fakk/fntech3-hud.json` | `041e2d006bb77449...` |

**How the probes ran.**
- Each probe was windowed on an unswitched private desktop, with an isolated
  home and mouse, joystick and sound off.
- Every step was a console command from a config in that home; no input was
  sent.
- Captures used the engine's `screenshotPNG`, at 960x720 (4:3) and 1680x720
  (21:9). The two sizes share a height, so a correct HUD is the same pixel
  size in both.
- Every installation was inventoried unchanged, and every run exited cleanly
  with no errors.

**Harnesses and levels.** Both sizes of a title are taken from the same place.
- `.tmp/hud-showcase/hud_showcase.py` drove the Quake III-engine titles. For
  those that spawn at random, it moved the player with `setviewpos` to a spawn
  point from the map's own entities (9 units up) and waited for the view to
  settle:
  - Quake III (`q3dm6`) and Quake Live (`campgrounds ffa`, `team f`): the same
    spot on the same map, (-208, 448) facing 0;
  - RTCW MP (`mp_beach`): the red bunker spawn, facing 270;
  - ET (`oasis`, `team b 2 8 7`): an Allied spawn, facing the courtyard;
  - Holomatch (`hm_voy1`): its first deathmatch spawn.

  RTCW SP (`forest`, `give all; weapon 3`) and Elite Force SP
  (`_holodeck_firingrange`) start in place.
- `scripts/probe_ritual_client.py` drove Alice (`garden1`) and F.A.K.K.2
  (`fakkhouse`).
- `.tmp/hud-showcase/mohaa_hud.py` drove Medal of Honor (`m1l2a`, timed by rcon
  on loopback).

| Title | 4:3 capture | 21:9 capture | Observed at 21:9 |
| --- | --- | --- | --- |
| Quake III | `adb1ae96...` | `f2a2452b...` | as before this change |
| Quake Live | `6c7d4770...` | `1a2e3cf6...` | retail's own HUD; its 16:9 view extended at the sides, no longer stretched (client `bfbb2bc4ce0482bd...`, GLx `ad36ade72ddf1230...`) |
| RTCW SP | `4c1575b8...` | `d0e8f003...` | as before (the 4:3 capture is byte-identical to the previous build's) |
| RTCW MP | `9f93f94b...` | `7af5f9eb...` | as before |
| ET | `c8e84aed...` | `b99450ca...` | compass, minimap and boxes at the upper right; status block, head and skills at the lower left; weapon block at the lower right, all at 4:3 size; wider world |
| Elite Force SP | `9f06192e...` | `e12e1a94...` | gauges at the lower corners at 4:3 size; wider world |
| Holomatch | `9cce50f6...` | `2d39156a...` | gauges and score box at the lower corners; wider world |
| Alice | `16f92670...` | `b9870879...` | gauges at the edges and cards at the corners at 4:3 size; Alice the same height; wider world |
| F.A.K.K.2 | `7cf45e7a...` | `f6f1a45f...` | HUD menus at their corners; Julie the same height; wider world |
| MoHAA | `9370efbb...` | `b30de95e...` | the world keeps its 4:3 vertical framing, widened; arms and rifle as at 4:3; its HUD unchanged |

With `cl_hudAspect 0` each corrected title's 21:9 capture stretched its HUD
as retail did.

ET's own screens at 21:9 were captured from the same build: the scoreboard
(`+scores`) and the expanded command map (`+mapexpand`) as centered 4:3
composites, and the limbo panel (`openlimbomenu`) stretched as retail, its
world view in its slot.

**Automated coverage.**
- `tests/hud_aspect_tests.py` runs the HUD module for ten profiles:
  - each preset against its stock coordinates;
  - `extend`, both panel states, and the world view ET keeps;
  - Elite Force single player's frame, its world views, the stretch of that
    frame, and a restart;
  - Alice's widget views;
  - Quake Live's and MoHAA's isolation.
- `tests/q3_fov_profile_tests.py` checks the letterbox shapes, every
  profile's registration, QL's rule on each preset and between them, the authored field kept beside the corrected one,
  and MoHAA's first-person models kept from the cull.
- `tests/root_archive_tests.py` probes the rule filenames' isolation for ten
  profiles.
- The source tests pin the call sites.

## Not covered

- Full-screen overlays drawn as one picture stay stretched to the frame, as
  the aspect guide's policy has it: ET's binoculars, Elite Force's zoom
  mask, teleport and assimilation effects.
- Text lines that the rules would split stay centered, so they sit inset
  from the screen's edge on a wide display: ET's popups and prompts, Elite
  Force's talk box.
- Elite Force single player's projected brackets and labels were not seen at
  runtime; the probes aimed at no target.
- Holomatch's powerups, lagometer and scoreboard were not drawn in any
  probe. They are placed by default, centered.
- ET's foliage distance was not seen at runtime; `oasis` has no foliage.
- A mod with its own HUD layout needs rules of its own.
