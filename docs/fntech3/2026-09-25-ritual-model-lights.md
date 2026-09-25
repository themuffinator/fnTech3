# Model light from Ritual maps' compiled lights

## Scope

American McGee's Alice (`alice`) and Heavy Metal: F.A.K.K.2 (`fakk2`) drew
every model with the light grid. Since Alice moved to retail's no overbright
bits ([overbright record](2026-09-25-alice-overbright.md)), her models had
looked 2.9 to 3.2 times darker, and F.A.K.K.2's Julie was nearly black on
`water` and `otto` (`2026-09-24-fakk2-contracts.md`). The open question was
retail's compiled light list, which both renderers use for models and fnTech3
did not.

This record reads both retail engines for that use. It measures how far
fnTech3's models were from what retail's law gives them, and gives both
Ritual profiles fnTech3's own use of the data in all three renderers. No other
profile changes, nor does anyone's static lighting: the static-light sidecars,
their cap and their balance are as they were. The evidence is in
[2026-09-25-ritual-model-lights.json](2026-09-25-ritual-model-lights.json).

The F.A.K.K.2 profile is not yet committed. The code chooses each game's law
by the map's own version, 42 or 12, so F.A.K.K.2's takes effect when that
profile lands; its measurements here were made with the shared tree's
uncommitted profile.

## Observed: which models retail lights from the compiled lights

Not every model. Both renderers decide per entity, by its render flags
(`alice.exe` 0x004abc50, `fakk2.exe` 1.02 0x0048e170, each `test eax,
0x8000200`). An entity with neither bit set takes the light grid (0x004aaac0,
then 0x004ac7e0). An entity with either takes the compiled lights and never
samples the grid. The two bits are Ritual's `RF_EXTRALIGHT` (0x200) and
`RF_INVISIBLE` (0x8000000); Ritual's F.A.K.K.2 SDK, read for reference only,
gives those names. The game modules set the first on every sentient and every
weapon:

| Module | Sentient constructor | Weapon constructor |
| --- | --- | --- |
| Alice `base/fgamex86.dll` | 0x101387b0: `renderfx` (+0x1c0) gets 0x200, 0x800 and 0x4000000 | 0x10165e40: 0x200 |
| F.A.K.K.2 `fakk/gamex86.dll` | 0x100fc040: `renderfx` (+0x1ac) gets 0x200, 0x800 and 0x4000000 | 0x1012a6d2: 0x200 |

So Alice, Julie, every actor and every weapon are lit from the compiled
lights, and every other model from the grid. fnTech3's client already passes
the module's whole flag word to the renderer (`CL_AliceBaseEntity`,
`code/client/client_alice_cgame.hpp`); the renderers had not read these two
bits. This corrects the overbright record's "models now take the light grid as
the map holds it, as retail's grid reading does", which holds only for
unflagged models; that record is amended. The Alice sidecar record's "retail
lights a model from this list" (`2026-09-24-alice-map-sidecars.md`, not yet
committed) likewise holds only for flagged ones.

## Observed: the law

Read with capstone in `alice.exe` (SHA-256 `8aa22917...`, never run) and
`fakk2.exe` 1.02 (`681bc51c...`). The two engines differ only where the table
says so.

| Step | Alice | F.A.K.K.2 |
| --- | --- | --- |
| The map's lights: lump 17 read to the engine's list, a whole number of records or ERR_DROP, at most 1,532 or ERR_DROP (the error import is `0xe9d6c4`; `0xe9d6c0` is the print one) | 60-byte records with a trailing `cap` (0x0049a360) | 56 bytes, no cap (0x004830c0) |
| Lump 18: for each leaf in order, the numbers of the lights that reach it, ended by -1; one terminator a leaf lists nothing | 0x0049a4d0 | 0x00483220 |
| `ambientlight`, `suncolor` and `sundirection`, from anywhere in the entity text, the last one found winning; `sundirection` is angles | 0x004ac3b0 | 0x0048e8d0 |
| The lighting point: the TIKI frame's middle, as for the grid, and a reach of half the frame's radius, scaled | 0x004aaac0 | 0x0048cff0 |
| Up to four distinct leaves with an area, at the point and at eight points a reach away from it: up, down, +X, +Y, -X, -Y, and along X+Y both ways | 0x004aaca0 | 0x0048d1d0 |
| Six directions, the entity's own +X, -X, +Y, -Y, +Z, -Z, start at `ambientlight` plus the shader colour (`RF_LIGHTOFFSET`, 0x8000) or 32 (`RF_MINLIGHT`, 0x20000). With no lights or no leaf the model is drawn at the identity light | 0x004aaea0 | 0x0048d3d0 |
| The sun: its colour by the cosine on the directions facing it, if a point trace 16,384 units toward it ends on sky | 0x004aa820 | 0x0048cd50 |
| Candidates: the gathered leaves' lists, less lights in areas the view's area mask hides; every light, unmasked, on a map without visibility | 0x004ab000 | 0x0048d530 |
| Per light: a positive light skips an `RF_INVISIBLE` entity and a negative one lights only such; a light that needs a trace is dropped if a solid point trace from the lighting point is blocked | 0x004ab850 | 0x0048dd80 |
| A point light: power = \|intensity\| x K / d^2, dropped below 1. Each direction D takes power x colour x the cosine between D and the light seen from a point a reach behind the entity along D, so a near light spills onto the sides | K = 30000 (0x005004a0), clamped to `cap` | K = 7500 (0x004d6050), no clamp |
| A spot: in its cone, whose last 32 units fade, the directions facing against it take power x the cosine, with no cap; a negative radius by distance is a disc of that radius, lit from its point nearest the entity, under a cone of 1.25 | 0x004ab460 | 0x0048d990 |
| The shift, then each direction's brightest channel kept to 255, hue kept | `1 << r_overBrightBits`, 1 at the default (0x004abb80) | `1 << (r_mapOverBrightBits - r_overBrightBits)`, 4 at the defaults (0x0048e090; the cvar at 0x00d3e420) |
| `suncolor` is also shifted as it is read | 0x004ac467 | 0x0048e98b |
| Drawing: each vertex takes the sum of three directions weighted by its normal's components, kept to 255; dynamic lights are added per vertex; the shadow direction is the six directions summed by their lengths | 0x004ac1c0, 0x004abdd0, 0x004abad0 | the direction 0x0048dfe0; the rest not read |

Each engine shifts the model light as it shifts its own lightmaps and grid:
Alice by its overbright bits alone (as the overbright record found for its
maps), F.A.K.K.2 by Quake III's map bits less overbright bits. With both
engines at their defaults, F.A.K.K.2's 7500 times 4 equals Alice's 30000. The
maps were authored for that: on the 22 of F.A.K.K.2's 30 maps that name them,
`ambientlight` runs 2 to 4 and `suncolor` 11 to 56, where Alice's six such
maps use `ambientlight` 6 to 32 and `suncolor` 223 or 224.

The debug cvars `r_light_emphasize`, `r_light_emphasizePercent`,
`r_light_nolight` (and in F.A.K.K.2 `r_light_sun_line` and `r_light_lines`)
default to off. F.A.K.K.2 also registers `r_light_int_scale` at `0.05`, and
nothing reads it.

## Measured before: how far the grid is from retail's law

A developer client from `meson/build/win32-alice-client` ran each map's opening
view: windowed at 800 by 600, on a private desktop, with an isolated home and
an empty `base/config.cfg`, `ui_subtitles 0` and `net_enabled 1`. The steps
were `devmap`, `wait 12000`, `skipcinematic`, `wait 4000`, then three engine
`screenshotPNG` captures 600 ms apart: compiled lights, `r_ritualModelLights
0` (the grid, as before), and compiled lights again. No input was sent, and
the installation was unchanged after every run. The Windows session was
unlocked. `r_ritualModelLightsDebug 1` printed Alice's light at her lighting
point each second, with the grid's at the same point.

| Map | Lighting point | Grid, ambient / directed | Retail's law, the six directions (+X -X +Y -Y +Z -Z, red) | Fitted, ambient / directed |
| --- | --- | --- | --- | --- |
| `garden1` | -1368 -2472 -400 | 22 19 16 / 101 89 78 | 83 53 60 29 160 0 | 61 51 39 / 142 124 104 |
| `utemple` | -64 -5592 1265 | 15 25 27 / 88 143 157 | 148 150 101 149 149 0 | 148 205 252 / 107 149 183 |
| `centipede1` | 1136 -1712 -406 | 69 61 65 / 232 224 247 | 180 182 199 244 171 18 | 228 204 213 / 82 161 225 |
| `jlair2` | -848 416 98 | 153 130 96 / 255 217 161 | 255 255 255 255 255 0 | 278 231 158 / 161 131 85 |

The grid's ambient is its stored ambient times `r_ambientScale`, then 0.6,
Quake III's default; `alice.exe` and `fakk2.exe` register 0.5, which the
Ritual profiles have used since
([ambient scale record](2026-09-25-ambient-scale.md)). The fitted ambient is
printed before the renderer keeps it to 255. An independent
Python model of the law, written from the disassembly before the engine's,
gave `garden1`'s six directions within one unit of the engine's at Alice's
start.

The grid is darkest where the law is brightest. On `utemple` the grid's light
comes from beside and below Alice, so her back takes ambient alone, 15 25 27,
where the law lights that side at 150 204 255. On `garden1` the law gives her
back 53 44 34 against the grid's 22 19 16. On `jlair2` both are near full.

Alice's dress and back, as mean luma in the overbright record's boxes (for
`jlair2`, 378,420 to 422,540), GL, grid against compiled lights, then the
repeated compiled capture:

| Map | Grid | Compiled | Ratio | Repeat | Static lights off: grid, compiled |
| --- | --- | --- | --- | --- | --- |
| `garden1` | 15.5 | 26.7 | 1.72 | 1.72 | 14.9, 26.1 |
| `utemple` | 12.0 | 70.5 | 5.88 | 6.21 | 11.8, 72.2 |
| `centipede1` | 37.1 | 81.6 | 2.20 | 2.19 | 26.8, 71.2 |
| `jlair2` | 74.3 | 92.0 | 1.24 | 1.13 | 73.5, 79.2 |

These runs came from a build made before the shift in the table above was
added; at Alice's defaults its factor is 1 either way, and a second run with
the final build gave the same ratios (1.72, 5.92, 2.20 and 1.20). `jlair2`'s
Jabberwock moves and breathes fire, so its pairs vary. Elsewhere the frames
match: the whole frame's mean luma differs by 0.2 to 1.2 on the first three
maps, from the models and the animated water.

With `r_ritualModelLights 0` the probe build draws the old build's frames. The
build before the change, captured first with the same steps, gave the same
Alice boxes (15.4, 12.5, 37.5), and 98.9%, 69.5% and 99.8% of the frames'
pixels were identical on GL; `utemple`'s bubbles and water animate.

Vulkan and RTX drew the same ratios on `garden1` (1.72) and `utemple` (6.19,
6.18). On `centipede1` they drew 3.03 and 3.02 against GL's 2.20: their bloom,
on by default, lights up Alice's now-bright apron. With `r_bloom 0` all three
renderers agree within 0.03 on all three maps (Vulkan and RTX 1.72, 5.90,
2.19).

## Measured against retail: F.A.K.K.2

`alice.exe` cannot run here, but `fakk2.exe` 1.02's client can: the F.A.K.K.2
sweep (`.tmp/fakk2/sweep/frames.py`) had joined retail's client to fnTech3's
dedicated server on `water`, `otto` and `over` and taken each first view. The
same join was repeated with the probe build's dedicated server and client,
with the three captures above. Julie's silhouette is where fnTech3's grid and
compiled captures differ and its two compiled captures agree; on retail's
frame it was moved to where its gradients matched best. Mean luma on it, GL:

| Map | Retail | Grid | Compiled | Compiled, static lights off | Match |
| --- | --- | --- | --- | --- | --- |
| `water` | 81.1 | 26.6 | 88.5 | 88.0 (retail 82.5) | 0.69, unmoved |
| `over` | 45.3 | 43.1 | 55.8 | 51.5 (retail 44.0) | 0.75, unmoved |
| `otto` | 28.3 | 30.9 | 44.3 | 24.5 (retail 24.3) | 0.41 and 0.54, moved 23 and 32 pixels |

On `water`, where the grid is black at the start, Julie goes from a third of
retail to within 7 to 9% of it. On `over` she is 17% brighter than retail on
the compiled light alone and 23% with the static lights. `otto`'s poses differ
(her arms are up in one frame and down in the other), and its matches are too
weak to rank. In side-by-side frames the compiled light gives `otto`'s suit
and skin retail's blue and warmth, where the grid had darkened both. Vulkan
and RTX light Julie on `water` the same way, brighter by their bloom (1.36 and
1.39 of retail on their own silhouettes).

Before the shift was read, a first build gave F.A.K.K.2's six directions a
factor of 1, as at Alice's defaults. Julie then drew darker than on the grid
on `otto` and `over`, and plainly darker than retail. That is why the shift
was looked for.

## The change: fnTech3's own way

Retail is authoritative for which models take the compiled lights, which
lights reach a model and how much of each, and from where: the flags, the map
data and the law above as far as the shift and the clamp. How a model is
shaded with that light is presentation, and stays fnTech3's (AGENTS.md
invariant 11).

- **The data.** `code/renderercommon/tr_ritual_model_light.h` reads lumps 17
  and 18, the leaves' areas and the worldspawn keys from the FAKK file itself,
  since the translation to IBSP 46 keeps none of them. Each renderer's
  `R_LoadWorldMap` reads them before it translates and keeps them on the hunk
  once the world is in (`tr_ritual_model_light_bsp.h`). A malformed lump,
  which retail refuses the map for, leaves the map's models on the grid with a
  warning.
- **The law.** `RitualModelLight_Cube` computes it, through the clamp, for an
  entity: the renderer answers the leaf, trace and sky questions
  (`tr_ritual_model_light_render.h`), with its own node walk and
  `ri.CM_BoxTrace`. The shift is the one the renderer gives the map's
  lightmaps and grid, `r_mapOverBrightBits` less the overbright bits. At each
  profile's defaults that is retail's: 1 for Alice and 4 for F.A.K.K.2. The
  profile and its constant come from the map's version, 42 or 12.
- **The presentation.** `RitualModelLight_Fold` turns the six directions into
  the ambient light, directed light and direction the renderers shade every
  model with, by a least-squares fit over all directions along retail's own
  light direction. One light comes back exactly, and the mean over all
  directions always does. Everything after that is fnTech3's as for any
  model: `RB_CalcDiffuseColor` in all three renderers, the dynamic lights
  (per pixel by default), the static-light sidecars' per-pixel supplement,
  the shadows' light direction, Vulkan's and RTX's bloom and tone mapping,
  and the fog.
- **Left out.** Retail's per-vertex evaluation of the six directions, its
  per-vertex dynamic lights (fnTech3's own light the model instead), its
  `r_light_*` debug cvars and its fast reciprocal square root.
- **Where.** `R_SetupAliceEntityLighting`, which only the Ritual profiles'
  TIKI path calls, takes the compiled light for a flagged model and the grid
  as before for any other. `R_AddAliceTikiSurfaces` hands it the lighting
  point in the model's frame and half the frame's radius. A full-bright model,
  a view without a world, a map without the data or a model the TIKI point
  cannot be found for keeps the path it had.
- **The option.** `r_ritualModelLights` (default `1`, archived) is registered
  only by the Ritual profiles; `0` lights every model from the grid, as
  before. `r_ritualModelLightsDebug` (cheat) prints a flagged model's six
  directions, its fit and the grid's light at the same point once a second.
  No retail-exact option is added.

The static-light sidecars keep the renderers' cap of 128 a map and 8 a scene.
Their balance was computed against the lightmaps, and they now add to the
compiled light on flagged models as they add to the baked light on the world:
on Alice's box by 0.6 units on `garden1` and about ten on `centipede1`. Alice's
and F.A.K.K.2's light files are not yet committed, so at this commit neither
game has any. No other profile's static lighting changes.

## Tests

`tests/ritual_model_lights_tests.py`:

- the law and the reader on maps built in memory against hand-worked values:
  the power law and its spill, the drop below 1, Alice's cap and
  F.A.K.K.2's constant, the sign rules, traces, the area mask with and
  without visibility, leaf de-duplication, the added light, the identity
  light, the sun, spots and discs, the 255 clamp and the shift; and ten
  malformed maps the reader refuses;
- the fit: one direction exact, opposite directions even, and the mean over
  all directions kept for 2,000 random sets;
- each renderer's own `R_SetupAliceEntityLighting`, sliced from its
  `tr_light.c` with the real render glue: flagged, turned, shifted,
  min-lit, invisible and debug-printed models, and every path that keeps the
  grid or the flat light;
- the hunks the three renderers share, as text;
- with the installations present, all 36 Alice maps (10,184 lights) and all
  30 F.A.K.K.2 maps (10,267) through the reader, and the law at all 75 player
  starts through the C code and an independent Python model, traces aside.
  The map bytes are piped to the host.

`tests/alice_renderer_parity_tests.py` now expects the new lighting calls in
the Vulkan and RTX renderers' `R_AddAliceTikiSurfaces`, and
`tests/bsp_profile_loader_tests.py` stubs the world loader's two new steps.

## Built alone

A `git archive` of the base commit, `d1d98ba`, with only this change applied
was configured as `win32-dev` plus `alice-development`. With strict warnings
it built every target. Its 169 Meson tests outside the Python suite passed
but for two skipped. Its first full Python run failed three checks in two
existing tests that pinned the old lighting call
(`tests/alice_renderer_parity_tests.py`) and the world loader's stubs
(`tests/bsp_profile_loader_tests.py`); both are updated here, and the second
run passed 2,005 tests and skipped 76 that need tools or installations this
machine lacks. Its own client lit Alice on `garden1` with the same six
directions as above, 26.2 against the grid's 15.0; the Alice static-light
sidecars, not yet committed, were absent from its package.

## Open

- **No Alice frame beside retail's.** `alice.exe` cannot run here. Alice's law
  is its code; F.A.K.K.2 is the only live comparison.
- **F.A.K.K.2's `over` is 17% brighter than retail** on the compiled light
  alone. The fit is one candidate: it spreads a near light's spill into the
  ambient, which lifts sides retail leaves dark. The textures and the rest
  of the shading are others. `otto` is unranked.
- **A view without a world.** There `alice.exe`'s gathering step
  (0x004aaca0) fills the six directions with the identity light and returns
  before the drawing function is chosen, so a flagged model in such a view
  (a menu's) is drawn by whichever function the last model left [I]. fnTech3
  keeps its earlier rule for these views: Alice starts from no light, and
  F.A.K.K.2 from its flat light. Not traced further.
- **Traces cost time** per flagged model per frame, as in retail: one point
  trace per listed light that needs one, and one toward the sun. The frame
  cost was not measured.
- Only four Alice views and three F.A.K.K.2 views were measured, all at a
  level's start.
