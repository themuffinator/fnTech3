# Jedi Outcast's client game: Ghoul2, drawing and the widescreen HUD (2026-10-16)

After the [services and effects slice](2026-10-16-jk2-cgame-services.md),
Jedi Outcast multiplayer's (`jk2-mp`) client game stopped at client-game
import 285, the Ghoul2 model initializer, before drawing anything. This slice
serves its Ghoul2 imports and draws its Ghoul2 models, accepts the two render
flags its first frames send, registers the string packages its engine
registers at start, and gives its HUD the 4:3 frame and placement rules
Academy's has. A local `ffa_bespin` start now plays: the world, the first
person weapon, the HUD and the scoreboard draw at 4:3, 16:9 and 21:9, with
the HUD's dials round and in the corners. No gate closes.

Evidence levels follow the [ledger](COMPATIBILITY.md): Source (retail
disassembly read here), Unit (tests), Live (engine runs).

## Ghoul2 imports (Source, Unit)

Outcast's client-game Ghoul2 imports (handlers `0x402b3f`..`0x402e45` in
`jk2mp.exe` 1.02a) reach the same cores as its game imports, which the server
already serves through the shared store (`server_jk2_game.hpp`):

| Client | Game | Core | What it is |
| --- | --- | --- | --- |
| 281 | 584 | `0x4a7890` | whether any model is still there |
| 283, 284 | 586, 587 | `0x4a7620` | a bolt's matrix; 284 skips the rebuild |
| 285 | 588 | `0x4a6e90` | load a model into the module's field |
| 286 | 599 | `0x4a6e00` | empty a field |
| 287 | 591 | `0x4a7520` | bend a bone |
| 288 | 592 | `0x4a7440` | play a clip on a bone |
| 289 | 593 | `0x4a7d80` | the skeleton's name |
| 291 | 595 | `0x4a7bf0` | copy one model into another instance |
| 292 | 596 | `0x4a7ce0` | duplicate a whole instance |
| 295 | 589 | `0x4a75a0` | add a bolt |
| 296 | 590 | `0x4a75f0` | hang a model from a bolt |

The client keeps its own store, with the capabilities the server's Outcast
store opts into and retail's case-blind names (`0x4e2480`). The client game is
a QVM, so each field is a 32-bit word that holds the handle itself. Four
client-only imports are served too: 280 (`0x4a78d0`, which only returns),
282 (`0x4a78e0`, one column of a bolt matrix by Raven's orientation number:
the origin, then the x, z and y axes, then the same negated; any other number
writes nothing), 293 (`0x4a72a0`, whether a field's model at an index is
present) and the init's fourth word, a skin handle, which `0x4a6e90` keeps as
the model's own skin (`0x4a7051`, `+0x38`). Model removal (294), the root
surface (297), surface visibility (298) and a new origin (299) have no store
operation yet and are refused by name; 278, 279 and 290 have no call site.

## Drawing Ghoul2 models (Source, Live)

Retail's drawing pass for an instance (`0x478320`) culls it by its largest
model scale (`0x478dc0`), places it from the record's angles and origin
(`G2_GenerateWorldMatrix` `0x4abdb0`), not its axes, and poses each model at
the scene's time: it rebuilds a model whose last build time is not the
view's (`0x478704..0x478718`). It picks each model's material as the
record's custom shader (`+0x4c`), else the model's own skin, else the
record's skin (`+0x88`) (`0x478651..0x4786b0`).

fnTech3 queues an entity with an instance at import 201, poses every waiting
instance at the time of the view import 207 hands over, and turns each model
into its own Fn skeletal entity through the runtime model path Academy's
client uses (`client_jka_cgame_render.hpp`), so Fn's lighting, shadows and
materials apply to it. The material order and the angle placement are
retail's. A surface the model file switches off is not drawn. Not served:
model scales other than 1, retail's frame-to-frame bone smoothing
(`0x478978..0x478a6a`), and its surface and root-surface overrides.

## Render flags (Source, Unit)

The first frames send two flags outside the plain model set:

- `0x400` forces the record's alpha on single-texture stages, blended: the
  stage path at `0x488e7a..0x488e93` writes the record's alpha (`+0x53`) to
  every vertex and draws with state `0x65`, as Academy's renderer does, so
  it crosses as the engine's existing forced-alpha field.
- `0x800` reaches nothing: no test or mask in the renderer
  (`0x466000..0x4b0000`) carries that bit. It is accepted and dropped.

## String packages (Source, Unit, Live)

`CL_Init` (`0x40c600`) registers four string packages as soon as it prints
its banner (`0x40c5c0`, flag 8): `con_text`, `mp_ingame`, `mp_svgame` and
`sp_ingame`. The client game registers none itself (its import 272 has no
call site), so without them every `INGAMETEXT` lookup answered `??`. fnTech3
now registers them for `jk2-mp` at client start under their own owner, which
neither module's reset removes. Live, the portrait's "Leader" and the
scoreboard's "1st place (of 1) with 0" read correctly.

## The widescreen HUD (Live, Unit)

Outcast draws its HUD in Raven's virtual 640x480 screen, as Academy does. It
now reads the HUD's 4:3 frame as its screen through its glconfig import (219),
so its view is widened by the engine while its 2D is placed in that frame,
and `pkg/games/jk2-mp/base/fntech3-hud.json` moves the pieces to the
screen's edges. The regions come from `cl_hudDump` captures of live play at
1280x720 (`.tmp/jka-hud/runs`):

| Rule | Region (640x480) | Placed |
| --- | --- | --- |
| health and armour dial | 0-112, 395-480 | lower left |
| force and ammunition dial | 528-640, 395-480 | lower right |
| score text | 480-545, 455-480 | lower right, with the dial |
| portrait, name and leader text | 520-640, 0-120 | top right |
| weapon, force and item bars | 112-528, 395-470 | centred, bottom |
| a bar's name | 150-480, 455-480 | centred, bottom |
| crosshair | `gfx/2d/crosshair*` | centred |
| scoreboard | `gfx/menus/menu_buttonback*` | one centred 4:3 screen |

The zoom masks, rocket lock and loading screen take Academy's rules for the
same pictures, which Outcast ships under the same names; none of them has
been captured in Outcast yet.

Live, each run a `jk2-mp` client built from HEAD `35e01aa1` plus this
change, windowed on a private desktop, `net_enabled 0`, no sound, no input,
isolated home, `+devmap ffa_bespin`, `team free`, then `togglemenu` to close
the player-setup menu the client game opens on joining (the engine's own
Escape command), engine `screenshotPNG` captures:

| Window | Result |
| --- | --- |
| 1280x720 | dials round in the lower corners, score beside the right dial, portrait top right; scoreboard centred |
| 1680x720 | the same at the edges of a 21:9 window |
| 960x720 | the original's 4:3 layout |

Each run exited by itself with no refusal and left the installation
unchanged. `fntech3.exe` was `534724b6...` in all three; the capture hashes
are in the companion.

## Open

- Ghoul2 model removal, root surface, surface visibility and new origin
  (294, 297-299); model scales; bone smoothing.
- Rotated pictures (212, 213), brush-model vertices (217), the effects line
  (218), the forced view (227, 228), client ROFF playback (276), the direct
  effect primitives (266-270).
- Other entity types (sprites, beams, saber glow, lines) and Raven view flags
  have not been reached on this map yet.
- The zoom overlays, rocket lock and loading screen under the HUD rules;
  a match with other players; demos.

## Found on the way

- The shader `gfx/effects/whiteFlash` uses `depthFunc disable`, which Fn's
  parser warns about as unknown.
- The probe harness's `ui_test` (Outcast's close-all) left a blank full-screen
  UI state in play, where `togglemenu` closes the join menu cleanly.

## Tests

`tests/jk2_cgame_ghoul2_tests.py` checks the import table against both
dispatchers' cores, the named refusals, the scene routing and material
order, the HUD frame, rules order and string packages, and runs import 282
and the render flags against a QVM data segment. `tests/jk2_ui_render_tests.py`
now sees the menu module's entity path take the same two flags the same way,
since both reach the one renderer. `tests/jk2_string_tests.cpp`
registers the engine's packages and shows the modules' resets leave them.
`tests/hud_aspect_tests.py` places Outcast's HUD at 21:9 through its own
rules; the packaging tests admit its rules file.
