# The IHV Test's console commands, 2D colour and overlay, 2026-10-08

Three gaps in the developer client, found while driving the IHV Test's client
game from console commands alone. All three are now closed:

- every console command the client forwarded to a server failed;
- the build's 2D art lost the colour its client game asked for;
- the overlay square was never drawn.

The [JSON companion](2026-10-08-q3-ihv-commands-and-2d.json) holds identities,
commands and results.

## Console commands

A command the engine does not know, such as `kill` or `give all`, is forwarded
to the server as a client command. Commands queue in the engine's reliable
command ring until the IHV session moves them into the channel's single
reliable block, opcode 3 and a string as `CL_SendCmd` (`0x403360`) sends
them. The IHV wire profile gave that ring a size of 0, so every forwarded
command stopped the connection with "Client command overflow", and the rest of
the probe's script was dropped with it. The live and server-session records'
runs used no such commands, so neither had shown it.

The ring now holds 64 commands for this profile. That is the same staging
fnTech3 uses for Q3Test up to 1.05, which also sends one reliable block. The
build's own wire still carries one block at a time. Measured:

- on fnTech3's server and on the original server, `kill` reaches the game: the
  console logs "fnTech3 killed himself.", and the death camera and scoreboard
  follow;
- the game refuses a second `kill` within five seconds of a spawn ("Only one kill
  every five seconds");
- `give all` (cheats are on by default) sets health 100, ammunition 999, armour
  200 and a holdable item;
- `give quad` and `give Quad Damage` gave nothing visible.

## 2D colour

**Observed in the original.** `Draw_StretchPic` (`0x43fd60`) binds the
shader's first stage image and draws it in the current colour, the one
`RE_SetColor` (`0x43fd20`) passes to `glColor4fv`. It ignores the stage's own
colour rules. The client game's fill (`0x407250`) sets a colour and draws
shader handle 0, the default shader, sampling its corner texel, which is white.

The build's 2D art is exactly its `nomipmaps` shaders (`scripts/gfx.shader`).
Several give `rgbGen identity` (`bigchars`, `crosshair`, `select`), and the
counters' digits give no colour rule at all.

**What fnTech3 drew.** fnTech3 draws 2D through its own shader pipeline, so
these stages ignored the colour. A trace of the client game's 2D calls showed
it setting green (0.2, 1, 0.2) before the ammunition and armour digits, and
blue and yellow before the lagometer's lines. All were drawn white. The
scoreboard's bar for the local player, a blue fill, came out as an opaque white
bar over its white text.

**The fix.**

- For this profile, a `nomipmaps` stage with an identity colour or alpha takes
  the vertex (2D) colour instead (`code/renderercommon/tr_q3_ihv_2d.h`, in all
  three renderers' `FinishShader`). Stages that already colour themselves keep
  their rule, and fnTech3's shader features are otherwise unchanged.
- The client game's handle 0 draws the renderer's own white.
- The profile now has client art of its own. Its data has no `white`,
  `console`, recording icon or cursor, so fills use the built-in white and the
  console draws `gfx/2d/conback`, as Q3Test's branch already does.

**Results.** Captured with `kill` on fnTech3's server, on GLx, Vulkan and RTX:

- the counters are green, as the original's capture shows them;
- the scoreboard draws "1st" in blue and a blue bar reading "-1 <ping> fnTech3",
  matching the original's "-1 0 Original" bar;
- the lagometer is blue and yellow;
- the title screen's second line is yellow, as its host draws it.

## Overlay square

**Observed in the original.** `R_BlendBlobs` (`0x442700`) runs after the scene
(`0x42ce73`) while `gl_polyblend`, default 1, is set (`0x442716`) and the
overlay colour has alpha. It binds the overlay shader's first image and resets
the modelview: the call at `0x442779` is `glLoadIdentity`, loaded at
`0x43033c` and stored at `0x43d64c4`. It then draws one square at eye depth 10,
centred at ten times the first two geometry words with half-size ten times the
third, its first texture corner top left, under the scene's projection.

**In fnTech3.** `View::OverlayRect` (`code/compat/q3_ihv_scene.hpp`) computes
the screen rectangle that square covers. The client draws it there in the
overlay colour after the scene, while `gl_polyblend` is set; with handle 0 it
draws the white. `tests/q3_ihv_scene_tests.cpp` checks the projection.

No scene in these runs used it: not `sli.dm3`, live play, `give all`, `kill`
or the scoreboard. So the square is implemented from the code and not yet seen
drawn. The scene dump (`cl_ihvSceneDump`) now prints the overlay when it is set.

## Runs

These runs used HEAD (`29b915e0`) plus this change. All were windowed, on
private desktops, driven by console commands only. The installation was only
read.

| Run | Result |
| --- | --- |
| fnTech3 client, `kill`, fnTech3 server: GLx, Vulkan, RTX | green counters, blue scoreboard bar with its text, blue "1st" |
| fnTech3 client, `give all`, fnTech3 server | health 100, ammunition 999, armour 200; no overlay |
| original client, `kill`, fnTech3 server | the original's own scoreboard: blue bar, "-1 0 Original" |
| fnTech3 client, `kill`, original server | the command reaches the original's game |
| `sli.dm3` in fnTech3 | plays to its end, digits now coloured |

## Not changed

During `sli.dm3`'s opening, IHV particles draw as large white discs, before this
change as after. A capture of the original at the same moment showed its
console instead, so that comparison remains open.

## Open

The overlay seen drawn, the particle comparison above, the build's menus and
input-driven play remain open. The profile stays a Windows x86 developer path,
and release builds still refuse it.
