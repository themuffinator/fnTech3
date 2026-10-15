# Jedi Academy's selection bars, loading screen and zoom masks, live

Date: 2026-10-15. Profile: `jka-mp`. Platform: Windows 11, the Windows x86
build. Companion:
[`2026-10-15-jka-hud-live-check.json`](2026-10-15-jka-hud-live-check.json).

The [widescreen HUD record](2026-10-15-jedi-widescreen-hud.md) placed three
parts of Academy's HUD from the OpenJK reference layout and unit samples
alone: the weapon, force-power and inventory selection bars, the loading
screen, and the zoom masks. This record checks them in engine captures and
HUD dumps. The bars themselves and the loading screen land as the rules
intended. Each bar's name did not: on a screen taller than 4:3 it was drawn
over the bar's centre icon. A new rule keeps it with its bar. Both zoom
masks are out of reach: any zoom stops the client first. No gate closes.

## How the screens were reached

All runs used `.tmp/jka-hud-verify/probe.py`. It runs a windowed `jka-mp`
client on an unswitched private desktop with `net_enabled 0`, `s_initsound 0`
and an isolated home holding an empty `jampconfig.cfg`. No input is sent;
every step is a console command from a config in that home. Captures are the
engine's `screenshotPNG`, and `cl_hudDump` records the HUD's draws in the
client game's 640x480 units. The installation was inventoried before and
after each run and never changed.

The runtime was built from HEAD `8779edb5` with the shared developer
options. A second runtime has the same executable and renderers and a
package that differs only by the layout file
(`.tmp/runtime/jka-hud-verify` and `-fix`; hashes are in the companion).
Of the commits after `8779edb5`, only the decal slice (`4782edce`) touches a
file on the HUD path, adding decal hooks beside it; the picture and font
imports, the 4:3 frame and the HUD module are unchanged.

- **Bars:** `devmap mp/ffa1`, `team free`, `give all`, then `weapnext`,
  `invnext` or `forcenext`. The inventory bar draws only once the server
  echoes the selection, so its shot waits 20 frames. `forcenext` falls back
  to the inventory when the player has no usable power. The force bar needs
  the `forcepowers` userinfo set before joining; Luke's retail bot
  configuration was used (`7-1-332212000220001333`, `botfiles/Luke.jkb`).
  The force menu the client game opens must be closed before `team free`,
  or closing it sends the menu's own team choice and the player stays a
  spectator.
- **Loading screen:** the Quake III recipe. `+set sv_packetdelay 2500` holds
  the client on its loading screen ("Awaiting snapshot...") while it
  reconnects, and a shot is taken every 30 frames.
- **Zoom:** the menu's key catcher must be released first with
  `togglemenu`, as for weapon fire. While the catcher holds, the client sends
  the talk button, and the game then ignores every other button. Then
  `+button2` uses the selected binoculars, after five `invnext`; `+useitem`
  is a Wolfenstein-only alias in fnTech3. The disruptor is `weapon 4` and
  `+altattack`.

## Selection bars (observed)

The dumps match the reference layout to the unit. The weapon bar's centre
icon is (280,400,80,80) and its side icons are 40x40 at y 420, from x 124 to
516. The force bar's centre is (290,410,60,60), with 30x30 side icons at
y 425. The inventory bar's centre is (280,400,80,80), with 40x40 side icons
at y 420. Its last icon, (488,420,40,40), is centred on x 508.0, exactly on
the edge of the `selection_bars` region. Every icon matched that rule
(centre, bottom). At 21:9 (1680x720) all three bars are whole and centred at
the bottom. So are the inventory bar at 16:9 (1366x768, an inexact scale of
1.6) and the weapon bar at 4:3 (960x720), both captured with the fixed
rules, which move no icon.

Each bar's name is one centred string below it. A string is matched by its
whole line, the union of its glyphs:

| Name | Line (x1, y1, x2, y2) |
| --- | --- |
| E11-Blaster Rifle (weapon) | 237, 461, 404, 476 |
| Seeker Drone (inventory) | 261, 462, 380, 476 |
| Heal (force) | 302, 462, 340, 475 |

Each line's centre, y 468.5 to 469, lies below the `selection_bars` region
(y up to 465). It matched no rule and took the default placement, centre and
middle. Where the 4:3 frame fills the window's height, middle and bottom
coincide, so 21:9, 16:9 and 4:3 looked right. At 5:4 (900x720) the frame is
900x675 with 22.5 pixels to spare above and below. The bars went to the
bottom (side icons at y 635.6 to 691.9) while their names stayed in the
middle (y 670.8 to 691.9), across the lower half of each centre icon
(`weapsel-900`, `invsel-900`, `forcesel-900`).

The region cannot simply grow downwards. The right HUD's score line ("Score:
0", line 468 to 513.5 by 464.9 to 475.4) has its centre at y 470.15, little
more than a unit below the names', and must stay right-aligned. The names
are centred on x 320, so they get a narrow rule of their own.

## The fix

`pkg/games/jka-mp/base/fntech3-hud.json`:

- `selection_names` (new): text whose line centre lies in x 300 to 340 and
  y 440 to 480 goes centre, bottom, with its bar. In the reference client
  game nothing else draws a centred line there during play. The spectator's
  two lines, centred near y 436 and 453.5 in a dump, already fall in
  `selection_bars`, which places them centre, bottom too.
- `selection_bars`: its region's width grows from 408 to 416 (x 100 to 516),
  so the inventory bar's last icon is not one rounding error away from the
  right HUD. The nearest right-HUD centre at those heights is x 549.

Live, after the fix: at 5:4 each name sits under its bar, with the same
spacing as at 4:3, in GLx and Vulkan (`fix-*-900`, `fix-weapsel-900-vk`).
Placement at 21:9, 16:9 and 4:3 is unchanged (`fix-*-1680`,
`fix-invsel-1366`, `fix-weapsel-960`).

## Loading screen (observed)

With `mp/ffa1` the dump shows `levelshots/mp/ffa1` at (0,0,640,480), matched
by `loading_levelshot`. The rest of the frame follows it: the info lines, the
load bar's surround `gfx/hud/mp_levelload` (112,448,416,32), and its ticks
and caps. At 21:9 the levelshot is stretched over the whole window and the
composite fills the centred 960x720 (`loading-1680`). At 5:4 the levelshot
fills 900x720 and the composite the centred 900x675, its tick row at
y 669 to 681 (`loading-900`). `yavin1` is the one installed map without a
levelshot. It drew `menu/art/unknownmap_mp`, matched by
`loading_unknown_map`, stretched under the same composite
(`loading-unknown-1680`). Both rules hold as written.

## Zoom masks (not reachable)

Both zoom commands reach the game, and the client stops at once on
`Unsupported jka-mp cgame import 216 (CG_R_SETRANGEFOG)`
(`binoculars-1680`, `disruptor-1680`). No run without a zoom reaches that
import on `mp/ffa1`. In the reference, the client game
sets ranged fog every frame while zoomed (`cg_view.c`), before it draws the
2D, so neither mask is ever drawn. The disruptor mask also draws its rotating
insert and ticks with `CG_DrawRotatePic2` (import 215). The rocket lock draws
its wedges with `CG_DrawRotatePic` (214). Both are still refused by name.

Inferred from the reference, for when 216 is served: the binocular mask
(0,0,640,480) stays stretched by default, as expected. Its companions would
not stay with it on a wide screen. The binocular window (34,48,570,362) and
its number strip would be centred, the circle at (82,94) would be taken by
`left_column`, and the arrow at (4, 282 minus the zoom level) by
`left_column` or `left_side`. The zoom needs its own captures and rules once
the client game can draw it.

Update: imports 214, 215 and 216 are now served, and both zooms draw live
with every piece stretched with its mask at 21:9 and 5:4 under new rules
([zoom record](2026-10-15-jka-zoom.md)).

## Presentation

This changes only Academy's HUD rules, which are fnTech3's own presentation
(invariant 11). The HUD layout keeps the bars, their names and the loading
composite whole on any screen shape, and `cl_hudAspect 0` still draws the
original's stretched HUD.

## Tests

`tests/hud_aspect_tests.py` runs the shipped rules through the real HUD
module. New Academy samples cover the inventory bar's last icon at x 508,
and a draw a few units further right that stays with the bar. They cover the
unknown-map loading frame, and at 900x720 a bar icon and its name line
placed at the bottom with the loading composite centred over its stretched
levelshot. With the old rules the test fails at the widened edge (x 2227
instead of 1667). With the widened region but no name rule it fails at the
name (y 670.78 instead of 693.28). With the new rules it passes. Results of
the commit's test runs are in the companion.

## Found along the way

- `devmap yavin1` under `jka-mp`: in the first of two runs, the retail
  `jampgamex86.dll` raised an access violation at `+0x4a293` right after the
  client game initialised, and the server shut down
  (`crash-loading-unknown-1680`). The second run did not crash. Not
  investigated; retail `jamp.exe` was not compared. `yavin1` is a
  single-player map.
- At 900x720 the client game's 4:3 frame is 900x675, and its dump holds a
  `gfx/2d/backtile` group. Inferred from the reference: the client game
  rounds its view height down to 674 and tile-clears the one-pixel strip,
  whose boxes lie outside the window. Nothing shows in the captures.

## Not covered

- Both zoom masks, while cgame import 216 (and 215 for the disruptor's
  insert) was refused. Since served and captured in the
  [zoom record](2026-10-15-jka-zoom.md).
- The force bar was seen with three powers. The weapon and inventory bars
  cover the widest extents, x 124 to 528.
- The loading screen was held by delayed packets from a local server; a
  remote server was not used.
