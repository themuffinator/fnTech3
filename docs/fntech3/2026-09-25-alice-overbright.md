# Alice's overbright defaults

## Scope

American McGee's Alice (`alice`) drew with fnTech3's usual defaults, one
overbright bit (`r_overBrightBits 1`) over two map bits
(`r_mapOverBrightBits 2`). Retail `alice.exe` registers `0` for both. This
record reads what `alice.exe` does with the two settings and measures what the
difference did to Alice's frames. It moves the `alice` profile to retail's
defaults in all three renderers. It also gives Alice retail's default colour for
a lightmap stage that names none. No other profile's defaults change; the
evidence JSON is [2026-09-25-alice-overbright.json](2026-09-25-alice-overbright.json).

Heavy Metal: F.A.K.K.2 made the same kind of move earlier
(`2026-09-24-fakk2-contracts.md`, "The world's light"). Its retail engine
registers no overbright bits over two map bits, and its lightmap stages mostly
name no colour. Alice's case differs on both counts, below.

## Observed: `alice.exe`

The retail executable (2,209,835 bytes, SHA-256
`8aa22917b7707f0e1a7c4dbe9a3c38332b1873ae2059174ac5329de26921fc18`) is
SafeDisc-wrapped and cannot run on this machine, but its code section reads.
It was read with capstone; addresses are virtual addresses in the image.

| What | Where | Reading |
| --- | --- | --- |
| `r_overBrightBits` | `0x00475136..0x00475147` | registered `"0"`, flags `0x21` (archived, latched); the pointer is kept at `0x00f01a20` |
| `r_mapOverBrightBits` | `0x0047529e..0x004752b8` | registered `"0"`, flags `0x20` (latched); the pointer is kept at `0x00f02f40`, and no instruction reads it |
| `R_SetColorMappings` | `0x00484d20` | the overbright bits are `r_overBrightBits`, or 0 without hardware gamma or outside fullscreen, at most 2 (1 at 16-bit colour); `identityLight` is `1 / (1 << bits)` |
| Colour shift | `0x0049ba40` | each channel shifted left by the overbright bits, then scaled back by the brightest channel when one passes 255; called for every lightmap texel by the lightmap loader (`0x0049afa0`, at `0x0049b542`) and for vertex colours by a surface parser (`0x0049bb70`, at `0x0049bde4`) |
| The same shift, inline | `0x0049acb0` at `0x0049adf7..0x0049ae15`; `0x0049c1a0` at `0x0049c385`; `0x0049c4e0` at `0x0049c731` | the light grid (Alice's 192 x 192 x 320 cell) and two more surface parsers' vertex colours |
| Model lights | `0x004abb80` | six light colours multiplied by `1 << bits`, so not at all at 0 |
| A stage's default colour | `0x0048c63e..0x0048c684` | with no `rgbGen`, a stage whose first bundle is `$lightmap` takes identity, Ritual's colour 1 (`0x0048c648..0x0048c678`), whatever its blend; any other stage takes identity lighting, colour 2, when blended from zero, one or source alpha, and identity otherwise |

So at retail's default nothing is shifted, `identityLight` is 1 and no bit is
added on output. A lightmapped surface draws lightmap times texture, a
vertex-lit one its vertex colours times texture, and a model its light-grid
light, each as the map holds it.

Quake III's colour shift is by `r_mapOverBrightBits - r_overBrightBits`, and
fnTech3's renderers keep it. With Alice's two defaults at 0 it shifts by
nothing, as retail does.

## Observed: the shader scripts

Alice's 52 scripts hold 1,387 stages drawing `$lightmap`, all on their first
bundle and none followed by `nextBundle`. 1,369 of them say `rgbGen identity`.
The other 18 name no colour and are blended: `GL_DST_COLOR GL_ZERO` 7,
`GL_ONE_MINUS_SRC_ALPHA GL_ONE` 6, `GL_ZERO GL_SRC_COLOR` 4 and `filter` 1.
Quake III's own rule gives those 18 identity too, since none is blended from
zero, one or source alpha. So every lightmap stage Alice ships is identity.
F.A.K.K.2's differ: 338 of its 365 name no colour, and those had taken
Quake III's identity lighting.

Under the old defaults every one of those stages drew its lightmap shifted up
a bit and then had the whole frame doubled on output: up to four times the
lightmap times the texture, less where the shifted lightmap or the frame
saturates.

## Measured

Two builds, each copied to a fixed folder so no rebuild could replace it
mid-run: before the change, `.tmp/runtime/alice-overbright-before` (from
`meson/build/win32-fakk2`), and after it, `.tmp/runtime/alice-overbright-after`
(from `meson/build/win32-alice-client`). Both were built from the shared
working tree on base commit `b68782b`, which also held other work not yet
committed, Alice's map sidecars among it; the JSON gives every binary's hash.
Every run was windowed at 800 x 600 on a private desktop through
`scripts/probe_ritual_client.py --profile alice`, with an isolated home
holding an empty `base/config.cfg`, so that the installation's own
`config.cfg` (`r_texturebits 16`) did not apply, and `ui_subtitles 0`. Each
took `devmap <map>`, waited 12 s, ran `skipcinematic`, waited 4 s and took the
engine's own `screenshotPNG`. No input was sent. The installation was
unchanged after every run. The Windows session was unlocked when checked
before and after the runs, with no lock logged between. The client is
Windows x86 on Windows 11, drawing on an NVIDIA GeForce RTX 4060 Laptop GPU.

Before the change, the same build with the old defaults against
`+set r_overBrightBits 0 +set r_mapOverBrightBits 0`, as the ratio of mean
brightness (the HUD's outer columns left out):

| Map | GL | Vulkan | Darkest pixels, GL median | Near-white pixels | Alice | HUD gauges |
| --- | --- | --- | --- | --- | --- | --- |
| `garden1` | 1.96 | 1.98 | 2.00 | 3 against 0 | 3.16 | 1.59, 1.50 |
| `utemple` | 2.22 | 2.23 | 2.39 | 0 against 0 | 2.91 | 1.60, 1.51 |
| `centipede1` | 2.68 | 2.68 | 2.87 | 165 against 0 | 2.88 | 1.63, 1.51 |

"Darkest pixels" are those between 6 and 60 in the new frame, which the old
defaults could not clip; their ratios ran from about 1.1 to 3.3. Alice
herself was measured over her dress and back, and the HUD over its left and
right gauges, which pass through the same doubled output.

- **The static lights keep their balance.** Captured on and off a second
  apart in one run, on floor away from anything animated, the map's static
  lights add the same share under both settings: `hatter2` 1.352 and 1.346
  before, 1.356 and 1.354 after; `jlair2` 1.207 and 1.214 both times. The
  per-pixel pass adds the light's own colour before the output scale
  (`code/renderer/tr_arb.c`), so it loses the doubling along with the baked
  light, and the balance the sidecar generator computed
  (`2026-09-24-alice-map-sidecars.md`) holds. The files need no regeneration.
- **Vulkan's bloom is not a factor here.** The Vulkan renderer turns
  `r_bloom` on by default with a threshold of 0.75. On these three views it
  changes the frame by 1.000 to 1.003 under either setting. Brighter
  effects could still bloom.
- **The menus are unchanged.** The main menu, pushed with `pushmenu main`
  and no level loaded, is 0.997 as bright under the old defaults as under the
  new, its animated mirror aside. Of the 2D measured, only the HUD's gauges
  moved.
- **After the change**, all three renderers print `GAMMA: hardware w/ 0
  overbright bits` under the profile's defaults. GL's frames at the defaults
  equal an explicit `0` and `0` (ratios 1.000, 0.996 and 1.000, 98.5% of
  pixels identical, the rest animated). An explicit `1` over `2` gives back
  the old frames (1.959, 2.225 and 2.679). Vulkan and RTX draw 1.5% to 2.6%
  brighter than GL, as Vulkan did at `0` and `0` before the change.
- **The change alone.** A `git archive` of the base commit with only this
  change applied was configured as `win32-dev`, plus `alice-development`.
  With strict warnings it built all 1,351 targets. Of its 169 Meson tests
  outside the Python suite, 167 passed and two were skipped. Its Python
  suite passed 2,001 tests and skipped 76 that need tools or installations
  this machine lacks.
  Its own client started `garden1` with no overbright bits and drew the same
  frame (0.998).

## The change

- `r_overBrightBits` and `r_mapOverBrightBits` default to `0` under the
  `alice` profile in all three renderers (`code/renderer/tr_init.c`,
  `code/renderervk/tr_init.c`, `code/rendererrtx/tr_init.c`). Both stay
  latched and archived, so a player's own values still apply.
  `tests/alice_overbright_tests.py` compiles each renderer's two defaults and
  evaluates them for every profile: Alice takes `0` and `0`, Elite Force keeps
  one map bit, and every profile outside the Ritual engines keeps one bit over
  two.
- A stage that names no `rgbGen` and ends on a first bundle of `$lightmap`
  takes identity under the `alice` profile, as `alice.exe`'s parser gives it
  (`R_RitualLightmapStageIdentity` in
  `code/renderercommon/tr_alice_shader.h`; `tests/alice_shader_parse_tests.py`).
  It changes no stage Alice ships. With no overbright bits identity and
  identity lighting are one colour, so the rule changes no frame. It lets
  such a stage collapse with the texture over it, as retail's do. It also
  keeps such a stage as bright as the named ones if a player raises
  `r_overBrightBits`.
- The [client-game record](2026-09-23-alice-cgame.md) said the grid loader
  colour-shifts each point exactly as Quake III does. It shifts by the
  overbright bits alone; that record is corrected.

## Contract and presentation

Retail is authoritative here for how Alice's data is read (AGENTS.md
invariant 11). That covers the two registered defaults and the parser's
default colour, so that a map's lightmaps, vertex colours and light grid mean
what the map holds, times the texture. The rest is fnTech3's presentation and
keeps its defaults: the FBO pipeline, whose output scale is now 1; the
per-pixel dynamic lights and the map's static lights with their shadows,
still balanced as generated; the Vulkan and RTX renderers' HDR and bloom; and
the fog. The two settings remain player options, and `1` over `2` is the old
look. No retail-exact option is added.

## Open

- **No retail frame.** `alice.exe` cannot run here, so retail's brightness is
  read from its code, not seen. No fnTech3 frame has been set beside
  retail's.
- **Models are now darker.** Before, Alice drew 2.9 to 3.2 times as bright
  as now, more than the world around her, since the grid was shifted up a bit
  and then doubled on output. Models now take the light grid as the map holds
  it, as retail's grid reading does. Retail also
  lights a model from the map's compiled light list (`0x004ab850`), which
  fnTech3 does not. fnTech3 gives only the static lights' per-pixel supplement,
  at most eight lights from each map's 128. So models may now be darker than
  retail's wherever the compiled lights carry them. On `garden1` Alice's dress
  and back average 18, 15, 14.
- **A non-default `r_overBrightBits`.** Raised to 1, retail shifts every light
  up a bit and doubles the output, brightening the world. fnTech3's Quake III
  shift moves the bit into the output instead: lightmapped surfaces keep their
  brightness, while `rgbGen vertex` surfaces draw at half. That is
  presentation of a non-default setting and is not reproduced.
- Only three opening views, two static-light views and the main menu were
  measured.
