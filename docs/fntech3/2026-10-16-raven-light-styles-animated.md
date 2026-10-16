# Raven's light styles, animated, 2026-10-16

The [light styles slice](2026-10-15-raven-light-styles.md) composed every
Raven light style into the one light channel Fn's renderers draw, each at
full white, and left "animated and switched styles" open: the client game's
colours stopped in the client. This slice carries them to all three
renderers. The client game sets a style's colour; the renderer recomposes,
before the next world view, the lightmap texels, vertex colours and light-grid
cells that style lights, and re-uploads the touched lightmap pages. The
[JSON companion](2026-10-16-raven-light-styles-animated.json) records the
retail addresses, counts, identities, commands and outcomes. No
compatibility gate closes.

Implementation is independently authored. The four retail Jedi engines were
read with capstone (measured); pinned OpenJK's renderer and client games were
read for the colour rules (reference observation; no text incorporated).

## What retail does

### Measured: the four engines

Each engine's renderer keeps 64 four-byte colours and exports a getter and a
setter; each client game reaches them through two imports:

| Engine | Get import | Set import | Get / Set | Colours |
| --- | --- | --- | --- | --- |
| `jamp.exe` 1.01 | 219 | 220 | `0x4a31d0` / `0x4a3200` | `0x1053650` |
| `jasp.exe` 1.01 | 96 | 97 | `0x46a700` / `0x46a730` | `0xcb2458` |
| `jk2mp.exe` 1.02a | 215 | 216 | `0x480100` / `0x480130` | `0x1a66fb8` |
| `jk2sp.exe` 1.02a | 86 | 87 | `0x46f8d0` / `0x46f900` | `0x12f81c8` |

- **The setter.** It takes the style and the colour word itself (an x86
  little-endian RGBA word), refuses a style of 64 or more with a fatal error,
  and stores the word if it differs. The single-player setters also mark the
  style changed (`0xcb2418`, `0x12f8188`).
- **The getter.** It copies the word to the address the client game gives.
- **The imports.** `jamp.exe` 220 (`0x41328d`) hands `args[1]` and `args[2]`
  to `re.SetLightStyle` (`call [0x8af0f4]`); 219 (`0x41326e`) hands the
  style and `VM_ArgPtr(args[2])`. `jk2mp.exe` 216 and 215 (`0x402623`,
  `0x402604`, dispatcher `0x4017a0`) do the same. The single-player engines'
  `CL_CgameSystemCalls` (`0x401530`, `0x401570`) pass the arguments as they
  are.
- **The start.** `jamp.exe`'s `R_Init` (`0x4a2cc0`) sets all 64 colours to
  `0xffffffff`.
- **Who calls them.** Academy's `cgamex86.dll` (assets3) calls 220 once
  (`0x3005eada`) and never 219; Outcast's `cgame.qvm` calls 216 once and
  never 215; `jagamex86.dll` and `jk2gamex86.dll` call 97 and 87 from one
  wrapper each (`0x20144480`, `0x20046070`).
- **`r_debugStyle`.** The single-player renderers alone register
  `r_debugStyle`, default `-1`, cheat (`jasp.exe` `0x469e07`). When it is 0 or
  more, every view sets all 64 styles black and that one white (`jasp.exe`
  `0x46e6c2`).

### Reference observation: what the colours do

From OpenJK's client games and renderers:

- **The colours.** `CG_RunLightStyles` sets every style each frame. A style
  whose configstrings are unset is white; one with a string steps through its
  letters every 50 ms (`a` black, `z` white). Academy's game names styles 1
  to 31 from built-in flicker, pulse and candle strings, which a map's
  worldspawn may replace, and switchable lights turn their styles to `a` or
  `z`.
- **The light.** As the earlier record found: a later lightmap channel adds
  its texels tinted by its style's colour, a vertex-lit surface sums its
  channels times their styles' colours, and the grid sums its channels. The
  first lightmap channel is never tinted.

## What fnTech3 does now

### The client

- **Academy (220).** The client keeps the colour, as before, and now hands
  it to the renderer's new `SetRavenLightStyle`. Style 0 takes any colour:
  the client no longer refuses a coloured style 0.
- **Outcast (215, 216).** The multiplayer client serves both imports from
  the same 64-colour store, with retail's bounds, and hands each colour to
  the renderer.
- **Single player.** Neither single-player client game runs yet, so 96, 97,
  86 and 87 are pinned only.

### The renderers

`tr_raven_bsp_format.h` names what each style lights;
[`tr_raven_light_styles.h`](../../code/renderercommon/tr_raven_light_styles.h),
shared by GLx, Vulkan and RTX, keeps the colours and recomposes.

- **What a style lights.** At load the reader records:
  - the lightmap groups: surfaces on one page whose later channels agree,
    with the texels their first channels touch, as the load-time
    composition groups them;
  - vertex-lit surfaces whose channels name a style other than 0;
  - grid cells whose row names a style other than 0.
- **The colours.** The renderer keeps 64 colours, white until set. They
  outlive a map, as retail's outlive everything but `R_Init`; a renderer
  restart starts them white again.
- **A change.** Before each world view, every style whose colour changed is
  recomposed:
  - **Lightmaps.** On each page a changed style lights, the texels it lights
    start again from the original page, and every group on the page adds
    its light back there. Adding saturates, so the order does not matter and
    the result is what composing the whole map at those colours gives. The
    page is processed as at load and re-uploaded: its own image, or its cell
    of a merged atlas, borders included.
  - **Vertex light.** Faces, triangle soups and patches take the composed
    colour, through the map's overbright shift. They are left out of the
    static world vertex buffer (GLx and Vulkan, and RTX's raster path), as
    their colours change.
  - **The grid.** Each recorded cell's ambient and directed light.
- **Patches.** A styled vertex-lit patch's control points carry their patch
  coordinates in its projected lightmap coordinates, which a vertex-lit
  surface never samples. After tessellation the renderer reads where each
  vertex lies, gives the patch its own coordinates back, and lights each
  vertex by evaluating every channel there, as retail's grids carry every
  channel. A patch sharing control points with another surface keeps them
  (none of the 123 installed ones does).
- **A lone channel.** A vertex-lit surface with one channel now scales by its
  style's colour over 255, so a switched light turns it off. At white it
  keeps the map's colour, as before.
- **`r_debugStyle`.** Retail single player's diagnostic is served for every
  Jedi world: default `-1`, cheat. At 0 or more, each world view sets all 64
  styles black but the one named, white.

### FnQ3/FnQL features (invariant 11)

- **What uses the light.** The styled light reaches each renderer's own
  lightmap, vertex-light and light-grid paths, and so the features built on
  them: the lightmapped materials, merged lightmap atlases, and entity
  lighting from the grid.
- **RTX.** RTX's path tracer lights the world from the map's light sources,
  not its baked light, so the styles reach it only through its raster path
  (`rtx_rt_mode 0`).
- **Left out, with reasons.**
  - **Style 0 alone.** What only style 0 lights is not recorded. Every Jedi
    game keeps style 0 white (`z`), and recording it would take every
    vertex-lit surface out of the static vertex buffer. A coloured style 0
    (or `r_debugStyle`) reaches only light that also carries another style.
  - **`r_vertexLight`.** When lightmaps are drawn as vertex light, the
    lightmapped surfaces' vertex channels are not restyled.
  - **Lightmap averages.** The per-page average colours some Fn features
    take at load are not recomputed.
  - **Patch colours.** A styled patch's vertex colours are evaluated on the
    curve, where the load-time tessellation averaged bytes; they may differ
    by a unit or two **[I]**.
  - **Retail's limits.** A style of 64 or more stops with an error, as in
    retail; `r_debugStyle` above 63 sets every style black, where retail's
    renderer would stop.

## Evidence

### Tests

- **`tests/raven_bsp_render_tests.cpp`** (Meson's `fntech3_raven_bsp_render`)
  gains `ChangingStyles()` on the production reader:
  - the groups, their pages, styles and texels, with overlapping groups;
  - recomposing a page after each of four colour changes equals composing
    the whole map at those colours, and white again equals the load;
  - a style nothing uses changes nothing;
  - vertex-lit surfaces, a lone switched style and the grid cells are
    recorded, and style 0 alone is not;
  - a styled patch carries its coordinates, is lit at its control points as
    the vertices are, and gives back its own coordinates; one that shares
    its control points keeps them;
  - allocation failure and missing arguments are refused.
  - It passes with MSVC x86 (`/W4 /WX`) and Clang x64.
- **`tests/raven_light_styles_tests.py`** compiles
  `tr_raven_light_styles.h` against a small renderer world:
  - the first view composes everything once and uploads only the styled
    page; an unchanged or unused style does no work;
  - style 1 recomposes and uploads its page alone, style 5 the vertex-lit
    face and patch alone, and the grid cells follow;
  - merged atlases upload the page's cell; no lightmaps, no world, or a
    world with no records does nothing;
  - `r_debugStyle`, and out-of-range styles and a missing colour refused.
  - It also checks each renderer's wiring, and runs Outcast's 215 and 216
    case text against the shared store.
- **`tests/jka_cgame_host_tests.py`** checks that 220 hands the colour to the
  renderer byte for byte, that style 0 takes a colour, and that refused
  indexes reach no renderer. `tests/jka_lightstyle_tests.cpp` follows the
  store. `tests/bsp_profile_loader_tests.py` stubs the new load call.

### The installed maps

All 98 installed Jedi maps still project, through the production reader:

- **Records.** 18 maps have styled records: 5 Academy and 13 Outcast. In all:
  4,399 lightmap groups, 2,150 vertex-lit surfaces, 54,675 grid cells and 123
  styled patches. None of the patches shares its control points.
- **Cost.** Building the records takes at most 45 ms (`t3_bounty`), and
  recomposing every styled page at once at most 1.4 ms.
- **Round trip.** Recomposing every page at a colour and back at white
  reproduces the load-time pages byte for byte, in every map.

### Live

A `jka-mp` client of the export build (HEAD `6442621b` with this slice) ran
`devmap kor1` and `devmap t3_bounty` with the retail game and client-game
modules: windowed on a private desktop, `net_enabled 0`, no input, engine
`screenshotPNG` captures. On `kor1` the camera stood at `setviewpos -2900 3800
-2860 180`, where the lamps along a slope are lit by style 1, Academy's first
flicker:

| Renderer | Lamp lit by style 1, white / black | Repeated states |
| --- | --- | --- |
| GLx | 79.1 / 0.0 | identical |
| Vulkan | 105.3 / 26.4 | identical |
| RTX, raster (`rtx_rt_mode 0`) | 104.8 / 25.9 | identical |

- **The client game's colours.** With `r_debugStyle -1` the client game's
  flicker steps the lamp between two values a second apart (75.8 and 79.2 in
  GLx; 102.0 and 105.3 in Vulkan), and consecutive frames differ in those
  pixels alone.
- **RTX's path tracer.** Its default traced frames change from frame to frame
  by their own sampling, by as much as style 1 does: the styles do not reach
  the tracer, as above.
- **`t3_bounty`.** In GLx, the switched styles' purple light on the pit wall
  goes at `r_debugStyle 0`. With style 32 alone, the vertex-lit pillar and
  beams and the ship lit by the grid go dark.
- **Outcast.** No Outcast map has been drawn live with the styles: a local
  `ffa_bespin` start stops at client-game import 285, Ghoul2, before its
  client game first sets a style
  ([client services record](2026-10-16-jk2-cgame-services.md)).
