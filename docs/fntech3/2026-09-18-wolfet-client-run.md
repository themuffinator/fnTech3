# The Enemy Territory client runs

## Scope and authority

This work follows `89ac8cc`. Until now nothing on ET's client side had run: the
boundaries, the drawing and the fog were source, unit tests and arithmetic over
retail assets. This slice opens the ET client in developer builds, runs it
against a retail installation with the retail cgame and UI modules, and fixes
what that run found.

**The ET client has now run.** In a developer build it starts, loads ET's own UI
module, draws ET's first-run profile dialog with ET's own fonts, loads the oasis
map with ET's own cgame, draws ET's limbo panel over it, and quits cleanly.

Behaviour facts come from the pinned original
[Enemy Territory](https://github.com/id-Software/Enemy-Territory/tree/40342a9e3690cb5b627a433d4d5cbf30e3c57698)
and, for the precompiler and the fog, the original RTCW multiplayer source it
grew from. [ETe](https://github.com/etfdevs/ETe) was consulted for how an ET
renderer is structured in this engine's lineage. All three are GPL-3.0; no
implementation text from any of them is incorporated into fnTech3.

## Opening the client

The developer build (`-Dwolfet-development=true`) now lifts the runtime block for
ET's client as well as its server, as the RTCW developer build already does for
RTCW. Release builds are unchanged: the profile's own runtime blocker still
refuses ET. Nothing about the profile contract moves.

## What the first runs found

Each of these stopped the client outright; none was visible to a unit test,
because each is a disagreement between two pieces that were each correct on
their own terms.

1. **ET's UI and cgame exports had no translation.** The engine could call ET's
   game module but not its UI or cgame. Both are now translated, including ET's
   own menu numbering — sixteen menus, five of which the engine did not have and
   now does, at the end of its enumeration so no existing value moves — key
   numbers in both key calls, ET's event handling, and the demo flag ET's cgame
   takes as a fourth initialisation argument. A cgame popup names a menu by ET's
   number, which is turned back into the engine's before the export translates it
   again, so it is not translated twice.

2. **The window icon table had no row for ET.** The table stopped at the other
   Wolfenstein profile, so ET's index read past its end and handed a stray
   pointer to the path constructor: an access violation before the first frame.
   ET has its row now, and all three icon tables assert at compile time that they
   have one row per profile.

3. **An initialisation failure lost its own message.** A disconnect-class error
   during start-up jumps to the start-up handler without printing, which then
   reported only "Error during initialization". The handler now carries the
   original message. This applies to every profile.

4. **ET's menus did not preprocess.** Two separate defects, found with a harness
   around the engine's own precompiler:
   - ET reads a macro's arguments by different rules from Quake 3's. It counts the
     call's own parenthesis as the first level, keeps a nested pair inside an
     argument, and expands a macro named inside an argument before collecting it.
     Quake 3's rules drop the nested "(" and end the call at the nested ")", so
     every argument after it is lost. ET's menus rely on all three. A source now
     reads by ET's rules only when ET's client loads it; every other profile's
     scripts read exactly as before.
   - Independently, and for every profile, **a string inside a skipped `#ifdef`
     branch leaked out.** The look-ahead that joins adjacent strings ran before the
     check for a skipped branch, so a string's look-ahead read straight through
     the `#else` that ended the branch and the skipped string was then returned.
     ET's own precompiler checks first; this one now does too.

   Under ET's rules all 58 of ET's menus preprocess with no error or warning,
   with its front-end define set and without it — 116 runs.

5. **ET's menu background used a wave the renderer refused.** Its lightning
   layers use a noise alpha wave. ET's renderer evaluates one as the colour wave
   does and — unlike the colour wave — does not clamp it before storing the byte,
   so values outside zero and one wrap; the flicker in ET's menu is drawn that
   way. All three renderers now evaluate it the same way. Every other profile's
   renderer refused this wave outright, so no shipped content of theirs changes.

6. **ET's fonts were never found.** ET names its pre-rendered fonts after the font
   and size alone, `fonts/ariblk_27.dat`, where the Quake lineage prefixes
   `fontImage_`. Under the wrong name every glyph fell back to a plain block, and
   the first capture showed every string as a solid bar. Under ET's name every
   glyph is found.

7. **ET's per-map fog was refused.** The cgame boundary mapped ET's fog command
   onto the engine's Wolfenstein fog, which the renderer then refused outside the
   other Wolfenstein profile. ET's fog is that profile's multiplayer fog with one
   difference in ET's own renderer, the colour scaled by the identity light as it
   is stored; ET's scene selection and backend fog are otherwise the same, line
   for line. ET now takes that path, with its own view record's portal and sky
   flags.

8. **Every cgame trace was run along the wrong line.** ET passes a trace's end
   point before its box, as the engine's own trace does, but the ET trace import
   declared the box first. Every cgame trace therefore ran from its start towards
   its box's maximum, with the real end point taken as the box's minimum, and a
   point trace — which has no box — was refused outright. The import now reads
   ET's order.

## Evidence

A new probe, `scripts/probe_wolfet_client.py`, runs the developer client on a
private desktop, windowed at 640×480, with an isolated home that is removed
afterwards; it injects no input, captures only with the engine's own
`screenshotPNG`, and checks the installation's every file unchanged. Captures
are kept under `.tmp`, never in the repository, because they are derived from
retail assets.

- **UI scene:** the client starts, loads the retail `ui_mp_x86.dll`, and ET's
  first-run *Create Profile* dialog is drawn in ET's own fonts: the introductory
  text, the player alias field, video quality, resolution, the brightness and
  mouse sensitivity sliders, the default-settings and PunkBuster buttons, and the
  quit and create buttons. Exit code 0, no engine error, windowed, installation
  unchanged.
- **Map scene:** with a 256 MB hunk, the client loads oasis, loads the retail
  `cgame_mp_x86.dll`, runs 240 frames and draws ET's limbo panel: the command map
  grid with its three spawn flags and troop counts, mission time and
  reinforcements, the player setup with team and class selection, experience and
  skills, the objective text, and a rendered objective camera. Exit code 0, no
  engine error, installation unchanged.

A new executable regression, built around the engine's own precompiler, checks a
nested pair and a nested macro inside an argument under ET's rules; that the two
rules differ on a nested pair and agree without one; a string in a skipped
branch never returned under either rule, whichever branch is taken; and adjacent
strings still joining outside one. Source regressions check that every UI and
cgame export ET declares is translated, that ET's sixteen menus are contiguous in
ET's numbering and present in the engine's, that a popup is not translated
twice, that the cgame receives its demo flag, that a trace reads its end before
its box, that ET's fonts are found by ET's names, that ET's fog is the
multiplayer fog scaled by the identity light, that every renderer evaluates a
noise alpha wave, that every profile has an icon row, that an initialisation
failure keeps its message, and that the client is opened for developer builds
alone.

## Remaining gates

- **No gameplay is claimed.** The client has reached the limbo panel; no team has
  been joined, no player has spawned or moved in the client, and nothing has
  been fired.
- **The command map draws black.** The map grid, flags and labels are drawn, but
  not the overhead image beneath them.
- **The map needs more hunk than ET's own configuration asks for.** The retail
  `etconfig.cfg` sets `com_hunkMegs 56`, sized for ET's original engine; this
  engine runs out loading oasis at that size, and the map probe runs at 256 MB.
- **ET's implicit shader keywords are not yet understood.** Many of ET's map
  surfaces are declared with `implicitMap`, `implicitMask` or `implicitBlend`;
  the renderer warns and draws those with its default shader.
- Only the GL renderer, one map and one installation have been run. The decal
  and render-to-texture imports remain refused; demos and the reverse direction
  remain open.
- Everything else in the [ET matrix](COMPATIBILITY.md#wolfet-gate-matrix)
  remains as recorded.

## Touched outside ET

- The precompiler's skipped-branch fix applies to every profile, as above.
- The start-up error handler keeps the original message for every profile.
- The engine's menu enumeration gains ET's five menus at its end, and its UI and
  cgame export enumerations gain engine-side numbers for ET's extra exports; no
  existing value moves.
- A noise alpha wave is evaluated in every renderer; no other profile's shipped
  content uses one.
- The window icon tables gain ET's row and a compile-time check.
- Seven regressions that compile slices of the engine were taught what changed
  beside them: four that build the export path now include ET's key and menu
  adapters, one that looked up the renamed ET export function now finds it, the
  fog scene's stub gains the identity light, and one that pinned "ET's UI accepts
  no export" now pins that an export only Quake Live's UI has is refused. That
  last fixture's two RTCW menu tables were sized by the old menu count and read
  past their ends once ET's menus were added; they gain ET's five entries, as
  absent, and assert at compile time that they have one entry per menu.

## Unrelated observations

ET's noise alpha wave is not clamped before it is stored as a byte, while its
noise colour wave is; whichever was intended, ET's menu is drawn by the first.
