# Raven's surface sprites, 2026-10-15

After the [sun keyword slice](2026-10-15-raven-sun-keyword.md), Raven's
`surfaceSprites` stage word was what still made Fn's shader parser reject
the most Jedi definitions: 39 under Jedi Academy and 22 under Jedi Outcast,
each then drawn with the default image. A sprite stage does not paint its
surface. It scatters grass, reeds, rain splashes and the like over the
surface's triangles, and both Jedi games use it on their outdoor ground.
This slice measures how all four Jedi engines read the word and its `ss`
companions, has the four Jedi profiles read them so, and draws the sprites
through fnTech3's own pipeline in all three renderers. The
[JSON companion](2026-10-15-jedi-surface-sprites.json) records the
binaries, identities, addresses, counts, commands and outcomes. No
compatibility gate closes.

Implementation is independently authored. The retail executables were read
with capstone and pefile. Pinned OpenJK (`rd-vanilla` `tr_shader.cpp` and
`tr_surfacesprites.cpp`) was used to find the code and to name the
generator's steps; the retail binaries were checked against it and outrank
it (reference observation; no text incorporated). OpenJK's 256-entry random
table is not used.

## What the retail parsers do

### Observed: the stage words

`ParseStage` in all four engines tests `surfaceSprites`, then any word whose
first two letters are `ss` in any case, after every other stage keyword and
before its unknown-keyword failure.

| Engine | `surfaceSprites` | `ss` prefix | Sprite line | Optional words |
| --- | --- | --- | --- | --- |
| `jamp.exe` 1.01 | `0x4b3fef` | `0x4b409d` | `0x4b2cb0` | `0x4b2f80` |
| `jasp.exe` 1.01 | `0x47a1a6` | after it | `0x478de0` | `0x4790d0` |
| `jk2mp.exe` 1.02a | `0x48d712` | `0x48d7c1` | `0x48e510` | `0x48e7e0` |
| `jk2sp.exe` 1.02a | `0x47d542` | after it | `0x47e340` | `0x47e610` |

- **The rest of the line.** Both branches join the rest of the line, one
  `COM_ParseExt( text, qfalse )` word at a time with a space after each,
  into a 1,024-byte buffer, and hand it to the sprite parser. Retail's
  join (`strcat`) has no bound. An `ss` word is first copied whole into a
  128-byte buffer, also without a bound.
- **Never a failure.** Every bad or unknown sprite word prints a warning and
  returns; `ParseStage` reads on. No sprite word can fail a shader.
- **Where the record lives.** Academy hangs a 0x48-byte record off the stage
  (`stage + 0x84`), allocated cleared on the first sprite word or on a good
  `surfaceSprites` line. Outcast keeps the same 0x48 bytes inside its stage
  at `+0xf0`. The field order is the same in all four.

### Observed: `surfaceSprites <type> <width> <height> <density> <fadedist>`

A summary of each sprite parser (string references, compare constants,
flag tests and stores) is identical in the four engines, apart from the
warnings' texts and Academy single player's extra type.

- **Types.** `vertical` 1, `oriented` 2 and `effect` 3 in all four.
  `jasp.exe` alone knows `flattened` (`0x478e4f`), numbered 5. Any other
  word warns "invalid type", so `jamp.exe`, `jk2mp.exe` and `jk2sp.exe`
  leave a `flattened` stage an ordinary stage.
- **Checks.** Each number is `atof`, stored as a float. Width, height and
  density must be above 0, and the fade distance at least 32. A missing word
  warns "missing surfaceSprites params". A failed line stores nothing.
- **Defaults.** A good line stores the five values, a fade maximum of 1.33
  times the fade distance (a double multiply, `0x4b2eaa`), a fade scale,
  wind and idle wind of 0, no variance, normal facing, a 1,000 ms effect
  duration, no growth, and an alpha range of 1 to 0. It leaves the vertical
  skew as it was.

### Observed: the optional words

| Word | Numbers | Check | Field |
| --- | --- | --- | --- |
| `ssFademax` | 1 | above the fade distance | fade maximum |
| `ssFadescale` | 1 | none | fade scale |
| `ssVariance` | 2 | each at least 0, the first stored before the second is read | variance |
| `ssHangdown`, `ssFaceup`, `ssAnyangle` | 0 | only on a record still facing normally | facing 2, 1, 3 |
| `ssWind` | 1 | at least 0; also sets the idle wind while that is still 0 or less | wind |
| `ssWindidle` | 1 | at least 0 | idle wind |
| `ssVertskew` | 1 | at least 0 | vertical skew |
| `ssFXDuration` | 1 | above 0 | effect duration |
| `ssFXGrow` | 2 | each at least 0 | growth |
| `ssFXAlphaRange` | 2 | each 0 to 1 | alpha range |
| `ssFXWeather` | 0 | only on an `effect` | type 4 |

Any other word starting with `ss` warns "invalid optional surfacesprite
param". The words match without regard to case. The comparison constants
were read from the binary: 0 (`0x565fac`, `0x566110`), 32 (`0x566228`) and
1 (`0x566038`).

### Observed: drawing

- **Skipped as a pass.** The stage loops skip a stage with a sprite type
  (`jamp.exe` `0x4aef88`, `jk2mp.exe` `0x488dad`).
- **Drawn after the surface.** After the surface's passes, each stage loop
  draws the sprite stages from the second stage on: `jamp.exe` walks stages
  1 to `numUnfoggedPasses - 1` (`0x4af48f`, calling `0x4c0df0`), and
  `jk2mp.exe` walks stages 1 to 7 (`0x48867a`). A sprite stage in the first
  place never draws.
- **Normals.** `jk2mp.exe` and `jk2sp.exe` mark a shader with a good sprite
  line as needing normals (`0x48e7a8`, `0x47e5d8`); the sprite code tests
  each vertex normal's height.
- **Cvars.** `r_surfaceSprites` "1" and `r_surfaceWeather` "0" in all four;
  `r_windSpeed`, `r_windAngle`, `r_windGust`, `r_windPointForce`,
  `r_windPointX` and `r_windPointY` "0", and `r_windDampFactor` "0.1".
- **The generator.** `jamp.exe`'s sprite code (`0x4be700`-`0x4c1000`) uses
  the constants OpenJK's generator names: the 250-unit fade band, the
  750-unit wind point radius (562,500 squared), 1.33, the 0.075 sway, the
  0.2 and 0.15 skew of a vertical sprite's top, 0.1, 0.4, 0.5, 2.5, 10,000,
  80, 40 and 0.001. The steps were not traced one by one; the record
  treats the reference's steps as matching where these constants sit.

### Inferred

- Retail's global wind comes from Academy's weather effects (world-effect
  commands from the client game) or from the `r_wind*` cvars, all 0 by
  default. Without either, sprites move only by their idle sway.
- The sprite batches draw with the stage's image and state bits alone
  (OpenJK's `CQuickSpriteSystem`); the stage's own `rgbGen`, `alphaGen`,
  `tcGen` and `tcMod` lines do not apply to sprites.

## The installed shaders

| Title | Definitions | Sprite stages by type | Maps naming one |
| --- | --- | --- | --- |
| Jedi Academy | 39 | vertical 35, effect 20, flattened 7, oriented 6 | `mp/ctf3`, `t2_dpred`, `t2_rancor`, `t2_trip`, `yavin1`, `yavin1b`, `yavin2` |
| Jedi Outcast | 22 | vertical 23, effect 12, oriented 6 | `yavin_canyon`, `yavin_courtyard`, `yavin_swamp`, `yavin_temple` |

- **The words.** Every `surfaceSprites` line has five words. The optional
  words used are `ssFademax`, `ssFadescale`, `ssVariance`, `ssWind`,
  `ssWindidle`, `ssFaceup`, `ssHangdown` and the four `ssFX` words.
- **Flattened.** All seven `flattened` stages are in Academy single-player
  shaders (`textures/imperial/dp_ground`, `textures/quicktrip/*`).
- **A first-stage sprite.** `textures/system/rainsurf` is three effect
  stages; its first never draws in retail, and does not in fnTech3.
- **`mp/ctf3`.** The only multiplayer map is Academy's `mp/ctf3`. Its 24
  `textures/yavin/ground_grasssprite` surfaces carry a vertical grass stage
  (`surfaceSprites vertical 32 24 42 500`, `ssFademax 1500`,
  `ssFadescale 1`, `ssVariance 1 2`, `ssWind 0.5`).

## What fnTech3 does now

- **Parser.** `code/renderercommon/tr_jedi_sprite_shader.h`, included by
  each renderer's `tr_shader.c`, reads both words under `jka-sp`, `jka-mp`,
  `jk2-sp` and `jk2-mp` as the tables above say. The record is
  `jediSurfaceSprite_t` (`tr_jedi_sprite.h`), kept inside each stage in
  retail's field order. `flattened` is read under `jka-sp` only. The line
  buffer stops at its 1,024 bytes and the word buffer at 128, where retail
  overruns them. Every other profile is untouched: none of them knows the
  words, and they still fail the shader as their engines do.
- **FinishShader.** Once the sort is chosen, the sprite stages leave the
  surface's own passes. Each drawn one, from the second stage on, becomes a
  shader of its own (`<name>#sprite<n>`). That shader is two-sided, its
  texture stretched once over each sprite, and its colour and alpha are
  the sprite's vertices. The surface's shader keeps a list of them, asks
  for normals and stays off the static world buffers, since sprites are
  built from its triangles. A stage whose sprite line failed stays an
  ordinary stage, as in retail.
- **Drawing.** `code/renderercommon/tr_jedi_sprite_draw.h`, called at the
  end of each renderer's `RB_EndSurface`, copies the batch's triangles,
  generates the sprites and draws them as their own batch, then reopens the
  surface's batch as it was. Light passes, shadow views and the 2D pass get
  none.
  - From retail: the normal tests, the triangle and grid rules, the fade
    between the fade distance and the fade maximum with each sprite's own
    start inside a 250-unit band, the field-of-view scale of the fade
    distances, variance, mirroring, fade scale, skew, idle sway, the three
    sprite shapes, the effect cycle with its growth and alpha range, the
    additive stage's brightness and the weather rule.
  - **`r_surfaceSprites`** (default 1, archived) turns them off.
    `r_surfaceWeather` (default 0) shows `ssFXWeather` effects, which
    retail otherwise shows only while its weather effects run.
  - An engine bound of 65,536 grid points per triangle stops a tiny density
    on a huge triangle from stalling a frame. Retail has none.

### FnQ3/FnQL features (invariant 11)

- **Used.** Sprites draw through each renderer's own batch path: blending
  and the alpha test as Fn draws them, Fn's fog adjustment for blended
  stages, Vulkan's pipelines and RTX's raster pass, which composites them
  over the traced scene.
- **Fn's own presentation.**
  - The light is the surface's full vertex colour; retail took its blue
    channel as grey.
  - The random numbers are a hash seeded from each triangle, so every
    sprite keeps its place from frame to frame, in every renderer alike.
- **Left out.**
  - Retail's depth-equal fog pass over sprites inside a fog volume. A
    sprite in fog fades as Fn's blended stages do.
  - Dynamic lights do not light sprites, as in retail.
  - The RTX path tracer does not trace sprites; they cast no shadows.
  - There is no global wind: fnTech3 has no Academy weather effects, and
    the `r_wind*` cvars, all 0 in retail, are not registered.

## Evidence

### Tests

`tests/jedi_surface_sprite_tests.py` (2 tests, 3 renderer subtests):

- **The whole production parser** (`alice_shader_parse_tests`' host) for
  each renderer under all sixteen profiles. A grass stage stores every
  field as written. The default fade maximum follows the double multiply.
  The words match in any case. The rest of the line belongs to the sprite
  word, so a `depthWrite` on it never applies. Each bad line warns and
  stores nothing, and the shader still loads. `flattened` is
  Academy single player's alone. A `surfaceSprites` line resets the facing
  and idle wind written before it but keeps the skew. A given idle wind
  outlasts a later `ssWind`. Each optional check keeps what was stored
  before it, and unknown `ss` words warn. `R_JediTakeSpriteStages` takes a
  sprite stage out of the passes and asks for normals, drops a first-stage
  sprite, and leaves a failed one in place. Every other profile still
  refuses both words.
- **The production generator** over a 512-unit floor with the renderer's
  types reduced to the fields it uses. A floor in range takes 145
  grass sprites standing 24 units tall, all opaque, and the batch returns
  to the surface's shader. The same view places the same sprites. Past the
  fade maximum none draw; inside the band some fade. Walls take no upright
  sprites, and hanging ones need a ceiling. Variance only enlarges a
  sprite. Idle sway moves a sprite's top with time but not its foot.
  `r_surfaceSprites 0`, light passes and shadow views draw none. Weather
  effects wait for `r_surfaceWeather`. Effects fade in and out over their
  cycle; an additive stage keeps full alpha. A dense stage overflows the
  batch and restarts it.

### Every installed Jedi definition through the production parser

`.tmp/raven-sun/corpus.py`'s census, run on an export of `27cd1166` and on
the same export with this slice's files alone:

| Renderer | Academy (3,391) | Outcast (1,187) | Quake III control |
| --- | --- | --- | --- |
| GLx, Vulkan, RTX | 79 to 40 | 47 to 25 | unchanged |

- **Exactly the sprite shaders.** The definitions that now load are
  exactly the 39 and 22 sprite definitions, in all three renderers, under
  both profiles of each title. No other definition changes.
- **What is left.** All 40 of Academy's remaining refusals and all 25 of
  Outcast's are stageless definitions, which retail refuses too.

### Live

Before and after runtimes from one export of `b80e6f6e`, differing only in
the three renderer DLLs (`.tmp/runtime/surface-sprites-before` and
`-after`). Each ran `jka-mp` `mp/ctf3` windowed on a private desktop with
`net_enabled 0` and no input, joined as a spectator and stood at
`setviewpos -1088 -1150 150 90` over the map's grass field, then took
engine `screenshotPNG` captures.

- **Before.** The field draws as the default image's black and white grid
  in all three renderers.
- **After.** The field draws its ground texture with a field of grass
  blades over it, in GLx, Vulkan and RTX. RTX traces the ground and lays
  the raster grass over it.
- **Pixels.** In the field region, 38% of the pixels change between before
  and after in GLx and Vulkan, and 49% under RTX. GLx and Vulkan place the
  same blades: 0.04% of the region differs between them.
- **The setting.** With `r_surfaceSprites 0` the field is its plain ground
  texture, 33% of the region differing from the grass.
- **Clean runs.** The installation and binaries were unchanged and the
  runs ended cleanly.

Not checked live: both single-player profiles still stop at their module
boundary, and the Outcast multiplayer client at client-game import 300, so
`flattened`, the effect sprites and Outcast's maps have only the unit
tests.

## Remaining and unrelated findings

- **Raven's `alphaFunc GE192`.** It is unknown to Fn's parser: it warns and
  draws the stage without an alpha test. 45 Academy and 23 Outcast stages
  use it, including `mp/ctf3`'s grass. Both engines know it (`jamp.exe`
  `0x4b24c9`, `jk2mp.exe` `0x48dc60`). Fn's arbitrary-reference alpha test
  (the Ritual `alphatest` bits) could carry it, once the static and GLX
  paths keep such stages on the CPU path.
- **Global wind.** It needs Academy's weather effects, which fnTech3 does
  not run.
- **A broken test at `b80e6f6e`.** `tests/raven_bsp_shader_flags_tests.py`
  imports `engine_fixture_args` from `tests/compiler_support.py`, which
  only the shared tree's uncommitted edit has, so the file fails to collect
  at that commit.
