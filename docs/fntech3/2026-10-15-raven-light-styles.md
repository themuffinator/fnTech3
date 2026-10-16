# Raven's light styles, 2026-10-15

The renderers' reader for Raven's maps refused any map with a light style
other than the first, the [JKA record](2026-10-02-jka-compatibility.md)'s
"extra-style maps still refuse". The
[Raven map shader flags slice](2026-10-15-raven-bsp-shader-flags.md) counted
45 installed maps refused that way or near it. This slice serves the styles
by composing them into the one light channel Fn's renderers draw, with
retail's arithmetic and retail's starting colours. Every installed Jedi map
now loads in the renderers. The
[JSON companion](2026-10-15-raven-light-styles.json) records the counts,
identities, commands and outcomes. No compatibility gate closes.

Implementation is independently authored. Pinned OpenJK's renderer and
client game were read for the composition rules (reference observation; no
text incorporated), and the installed maps with short readers.

## What a style is

A Raven surface record carries four lightmap channels and four vertex-colour
channels, each with a style number; a light-grid row carries four channels
too. A style's colour is set by the client game, which animates it from the
game's configstrings.

### Reference observation: how retail combines them

From OpenJK's `rd-vanilla` and `cgame`, which keep the retail engine's
structure:

- **Lightmaps.** `FinishShader` gives each lightmap channel after the first
  a stage of its own, while the styles run unbroken below 254.
  - Each such stage adds (`GL_ONE, GL_ONE`) its lightmap tinted by its
    style's colour (`CGEN_LIGHTMAPSTYLE`).
  - A channel whose lightmap is `-3` (vertex light) adds the style's colour
    through a white image.
  - A channel with no lightmap at all stops the map.
  - The first channel keeps its own colour.
- **Vertex light.** A surface whose first lightmap is `-3` takes, per
  vertex, the sum of each vertex channel's colour times its style's colour,
  shifted down eight bits (`ComputeFinalVertexColor`).
- **The light grid.** Each channel whose style is not 255 adds its ambient
  and directed light times its style's colour over 255; a first style of 255
  is a hole.
- **The colours.** The renderer starts every style at white. The client
  game keeps a style white until a configstring names it, then steps
  through its letters every 50 ms (`CG_RunLightStyles`).

### Observed: the installed maps

98 maps, 57 Academy and 41 Outcast:

| | Count |
| --- | --- |
| Lightmap channels after the first, on a lightmapped surface | 8,719 |
| ... at the first channel's coordinates moved by whole texels | 6,772 |
| ... at the first channel's own coordinates | 1,814 |
| ... on a surface with no vertices | 66 |
| ... whose lightmap is vertex light | 67 |
| Vertex-lit surfaces with more than one vertex channel | 2,150 |
| Light-grid rows with a second style, in 16 maps | 39,669 |
| Styles used | 1 to 44 |

- **Unset rectangles.** The surface records' lightmap rectangles are unset
  (width 0 on 250,483 lightmapped surfaces), so each channel's place is
  read from the vertices' coordinates. No drawn channel lies elsewhere than
  its first channel moved whole.
- **Shared blocks.** Surfaces share lightmap blocks. Of the 488,590 texels
  inside the bounding boxes of styled surfaces' coordinates, 89,018 are also
  inside another surface's. 32,355
  of those surfaces light them alike, 52,172 are unstyled neighbours, and
  4,491 differ.
- **A refusal that was not one.** 60,482 vertex-lit or unlit surfaces carry
  style 255 in their unused first lightmap channel. Retail never reads it,
  but Fn's reader refused it.
- **`yavin_temple`.** Outcast's `yavin_temple` has 11 flares in fog 0 of a
  map with no fog. OpenJK's `ParseFlare` reads the number unchecked; Fn
  refused the map.

## What fnTech3 does now

The RBSP reader ([`tr_raven_bsp_format.h`](../../code/renderercommon/tr_raven_bsp_format.h))
composes every style into the projection the renderers load:

- **Lightmaps.** Each later channel's texels are added, tinted by its
  style's colour, into the first channel's texels of the projected pages.
  - The first channel's texels are those its triangles touch (a patch's
    control grid counts as triangles).
  - Surfaces on one page whose channels agree are composed once together,
    so a block they share takes each channel once.
  - A vertex-light channel adds its style's colour, as retail's white image
    does.
- **Vertex light.** A vertex-lit surface with more than one vertex channel
  takes retail's eight-bit sum. One with its first channel alone keeps the
  map's colour, as before.
- **The light grid.** Each row sums its styled channels; a hole stays a
  hole.
- **Refusals.** A drawn channel that is not its first moved whole, a style
  of 64 or more, and a channel with no lightmap are refused before anything
  is allocated. The first lightmap channel's style is no longer checked.
- **Flares.** A flare whose fog the map lacks is drawn unfogged.
- **The colours.** The colours are white, the renderer's own start and a
  style no game has named. `FN_RavenBspProjectStyles` takes other colours.
- **The original map.** The world keeps the original map with every
  channel.

### FnQ3/FnQL features (invariant 11)

- **What uses the light.** The composed light reaches each renderer's own
  lightmap, vertex-light and light-grid paths and the features built on
  them (entity lighting from the grid, the lightmapped materials).
- **RTX.** RTX's path tracer lights the world from the map's light
  entities and surfaces, not its baked lightmaps, so the styles reach it
  only through its raster fallback.
- **Left out, with reasons.**
  - **Animated and switched styles.** Composing at load fixes each style's
    colour. The client game's colours (Academy's client-game imports 219
    and 220 keep them in the client) do not reach the renderer, and the
    single-player client games do not run yet. Every style is drawn at
    full, as retail draws a style no game names.
    The [animated light styles slice](2026-10-16-raven-light-styles-animated.md)
    (2026-10-16) carries them to the renderers.
  - **Stage order.** Retail adds a later channel's stage after whatever
    stage the lightmap is in. Where a shader puts its lightmap after its
    texture, retail's later channels are not multiplied by the texture,
    while the composed light is **[I]**.
  - **Overlaps.** In the 4,491 shared texels where surfaces with different
    channels overlap, each surface's light reaches the other's.
  - **The 255/256 shift.** Retail's vertex-light sum darkens a lone first
    channel by 1/256. Fn keeps such colours as the map has them.

## Evidence

### Tests

- **`tests/raven_bsp_render_tests.cpp`** (Meson's `fntech3_raven_bsp_render`)
  builds the production reader.
  - The style refusals are now acceptances.
  - A second lightmap channel adds into the texels its triangle touches and
    nowhere else, at white and at a given colour. A second surface lighting
    the same texels alike adds them once.
  - A vertex-light channel adds the style's colour, and channels after an
    unused style are not drawn.
  - A channel with no lightmap, or one not moved whole, is refused, as is
    a style of 64.
  - Vertex channels sum with retail's shift, and a lone first channel keeps
    its colour.
  - Grid channels sum, and a flare in a missing fog projects as unfogged.
  - It passes with MSVC x86 (`/W4 /WX`, the export build) and Clang x64.
- **The installed maps.** All 98 maps project; before, 45 were refused.
  - The composition changes 16 maps: 5 Academy, `kor1`, `kor2`,
    `t1_fatal`, `t3_bounty` and `t3_stamp`, and 11 Outcast, among them
    `ffa_ns_streets`.
  - It adds light to 284,449 texels, 26,454 vertex colours and 45,680
    light-grid cells.
  - None of the 53 maps projected before changes.
- **The fixtures that slice the reader.** `bsp_profile_loader_tests.py`,
  `raven_bsp_renderer_dispatch_tests.py` and `stvef_mdr_render_tests.py`
  pass.

### Live

- **Runtimes.** Both runtimes were built from the export of `f8e1ee98`
  with this day's two flag slices; the after runtime adds this change, and
  only the renderer libraries differ.
- **Probe.** Each run was a `jka-mp` client, windowed on an unswitched
  private desktop, with `net_enabled 0`, its own home, a spectator,
  `togglemenu`, and no input. Captures are the engine's `screenshotPNG`;
  the installation and binaries were unchanged after every run.
- **Before.** `devmap kor1` stopped with "Raven RBSP rendering
  maps/kor1.bsp: RBSP surface lightmap styles are not served yet (lump 13
  record 18 slot 1 style 1)".
- **After.**
  - `kor1` draws in GLx, Vulkan and RTX.
  - `t1_fatal` and `t3_bounty` draw in GLx.
  - No Outcast map was drawn: Outcast's multiplayer client still stops at
    client-game import 300.

## Remaining and unrelated findings

- **Animated styles.** A style's colour should reach the renderer from
  the client game (Academy's imports 219 and 220, Outcast's equivalent)
  and recompose the affected pages, vertices and grid cells.
