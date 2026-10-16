# Medal of Honor's worldspawn sun under RTX, 2026-10-16

The [map shader flag dialects slice](2026-10-15-map-shader-flag-dialects.md)
left one finding open: RTX's path tracer takes its sun only from a sky
shader's `q3map_sun`, and on `DM/mohdm6` it reported "the first sky surface's
shader `textures/sky/mohday1` has no q3map_sun" and lit the map with no sun.
2015's maps give the sun in worldspawn instead. This slice pins what those
keys mean and has the path tracer use them under the `mohaa` profile only.
The [JSON companion](2026-10-16-mohaa-worldspawn-sun.json) records the
counts, fits, commands, binaries and outcomes. No compatibility gate closes.

Implementation is independently authored. `MOHAA.exe` is SafeDisc-encrypted,
so the keys were read from the plain retail `gamex86.dll`, from the installed
maps' entity text and lightmaps, and from OpenMoHAA (GPL-2.0-or-later,
`ab43a0de`, reference observation; no text incorporated). Fn's own readers of
the same keys, `code/qcommon/cm_mohaa_lights.c`,
`code/renderercommon/tr_mohaa_world.c` and the renderers' `R_MOHAASunKey`,
were compared.

## The keys

### Observed: where the sun is named

- **No shader has a sun.** None of 1.00's shader scripts in `Pak0.pk3` to
  `Pak5.pk3` has a `q3map_sun` or `q3map_skyLight`; 24 have
  `q3map_surfacelight` and four `q3map_lightimage`.
- **54 maps, 46 suns.** 46 worldspawns have `sundirection`, 43 `suncolor` and
  4 `sunlight`. Every map with a direction has a colour under one of the two
  names, and none has a colour without a direction; the eight without either
  are the six briefings, `credits` and `void`. 21 have `sundiffuse` (0.4 to 3),
  8 `sundiffusecolor`, 45 `ambientlight`, 29 `sunflarename`, 4 `sunflare` and
  5 `overbright`. `DM/mohdm1` names both colours, `suncolor` "65 60 45" before
  `sunlight` "255 243 171".
- **Retail's own words.** `gamex86.dll`'s event table documents the keys the
  game receives with the worldspawn (strings from `0x1ed30c`): `suncolor`
  "Set the color of the sun", `sunlight` "Set whether or not there should be
  sunlight", `sundirection` (`sunlightDirection`) "Set the direction of the
  sunlight", `sundiffuse` (`factor`) "Sets the fraction of the sunlight to use
  for diffuse sun" and `sundiffusecolor` (`diffusecolor`) "Sets an alternate
  sun color to use for clcing diffuse sun". The game only stores them; the
  compiler and the renderer use them.
- **2015's renderer.** OpenMoHAA's `R_Sphere_InitLights` reads `suncolor` and
  `sunlight` alike as a colour, matched exactly, times the overbright
  multiplier, 70 in each channel until one is given, and `sundirection` as
  angles through `AngleVectorsLeft`, whose forward vector is `AngleVectors`'
  (reference observation). Fn's three readers agree.

### Observed: what the compiler stored

The lightmaps hold what 2015's compiler made of these keys. Each map's planar
lightmapped surfaces were grouped by normal; a luxel's world point is
`lightmapOrigin + s * lightmapVecs[0] + t * lightmapVecs[1]` for luxel
(`s`, `t`), checked against the vertices' lightmap coordinates.

- **The direction points toward the sun.** `AngleVectors(sundirection)` is
  above the horizon on every map (pitch -90 to -28, or 238 to 335), and on
  34 of the 37 maps with opposite walls to compare, the walls facing it are
  the brighter; the other three saturate or are lit at night.
- **The colour is stored as is, times the cosine.** A wall and the wall
  opposite it see the same sky, so their brightest common unsaturated level
  differs by the sun alone. Over 405 such pairs on 37 maps,
  (lit - opposite) / (suncolor x cos) has a median of 0.98; over the 28 maps
  whose floors saturate under a fifth of the time the median of the per-map
  medians is 0.99 (quartiles 0.84 and 1.04). The maps that read low are
  saturated (`DM/mohdm1`, `m2l2a` to `m2l3`) or lit at night by their lights
  (`m1l1`, `m1l2a`, `m1l2b`, `m6l1a`, `m6l1c`). `ambientlight` is stored as
  is too: it is each map's commonest luxel ("20 19 18" on `DM/mohdm6`).
- **Saturation keeps the hue.** Without `overbright`, a luxel's brightest
  channel stops at 127 and the others are scaled with it: `DM/mohdm6`'s
  sunlit floors read 127 126 125.
- **`sunlight` is a colour.** The three maps with `sunlight` alone
  (`DM/mohdm3`, `m1l3c`, `m3l2`) store it as `suncolor` (`DM/mohdm3` 0.985
  over 20 pairs, `m3l2` 1.03 over 17), against the game's "whether or not"
  wording. Which of `DM/mohdm1`'s two the compiler took cannot be seen: the
  two colours have nearly the same hue and its sunlit luxels saturate.
- **The diffuse sun.** A shaded wall keeps about 0.55 to 0.6 of the diffuse
  colour over `ambientlight` whether `sundiffuse` is absent or 1 (`training`
  0.57, `obj_team1` 0.59, `obj_team4` 0.59, `m5l2b` 0.55), 0.23 at 0.4
  (`m4l2`, `m4l3`), and no more at 3 (`m3l2` 0.57, `m3l3` 0.59, `m2l1` 0.42;
  `m3l1b`'s unsaturated open floors 0.85 of its colour). The brightest open
  floors keep 0.6 to 1.0 of it besides the sun (`m6l1b` 0.99, `m6l1a` 0.91,
  `m1l3a` 0.90).

### Inferred

- **[I]** The compiler's sun gives a surface `suncolor x cos` in lightmap
  bytes where it sees the sky, which is what id's q3map gives for a
  `q3map_sun` of that colour times intensity.
- **[I]** `sundiffuse` defaults to 1, and the compiler holds it at 1 or less:
  the five maps that set 1.5 or 3 store no more diffuse light than those that
  set 1. Whether a larger value was clamped or ignored is not settled.
- **[I]** The diffuse sun is a sky that gives an open level floor
  `sundiffuse x sundiffusecolor` (the sun's colour without one). The
  brightest floors reach that; most see part of the sky.
- **[I]** With both colour keys, the later one is the colour, as 2015's
  renderer and Fn's readers take it.

## What changed

`code/rendererrtx/rt/rt_lights.c`:

- `RT_ParseMOHAASun` reads the worldspawn's sun: `suncolor` or `sunlight` (70
  in each channel when neither parses), `sundirection` through
  `AngleVectors`, and the diffuse sun as `sundiffuse` (1 when absent, held to
  0..1) of `sundiffusecolor` (the sun's colour when absent). Keys are matched
  exactly, a repeated key keeps its last value, a value that is not three
  finite numbers counts as absent, and without a direction there is no sun.
- `RT_FindMOHAASun` gives the path tracer that sun unscaled, in q3map units,
  which the renderer's lightmap scale turns into lightmap units as it does a
  `q3map_sun`. The diffuse sun becomes RT's dome of suns (the one q3map2's
  `q3map_skyLight` uses), eight iterations, its value set so an open level
  floor receives the diffuse sun exactly (`RT_SkyDomeFloorShare`, the dome's
  mean elevation sine). Under `rtx_rt_lighting 3`, the emitters alone, it is
  instead what the even sky asks for (`RT_FindSkyLightColor`).
- `RT_BuildLights` calls it in place of `RT_FindSun` when the profile is
  `mohaa`, in every lighting mode; the other fifteen profiles keep
  `RT_FindSun` unchanged. The load prints the sun as a "worldspawn sun" and
  the dome as a "worldspawn diffuse sky light".

Invariant 11: this adapts 2015's own data into Fn's path tracer -- its sun,
sky dome and traced bounces -- rather than reproducing 2015's lightmaps. The
compiler's `ambientlight` floor is not added in the default mode, which
traces bounced light in its place, as for every other profile.

## Evidence

- **Unit.** `tests/rtx_mohaa_sun_tests.py` compiles the production
  `RT_ParseMOHAASun`, `RT_SkyDomeFloorShare` and `RT_FindMOHAASun` against a
  bounded host: `DM/mohdm6`, `m4l2`, `m3l2`, `DM/mohdm1` and `training`'s
  keys, the defaults, malformed and non-finite values, a missing direction,
  case, a second entity, a cut-off worldspawn, black suns, the dome's floor
  share against `rt_sample_sky_light`'s suns, adding to a shader's dome, the
  emitters' mode and a missing world; and that only the `mohaa` profile takes
  this path. The fixtures that slice `RT_FindSun`, `RT_FindSkyLight`,
  `RT_BuildLights` and the light scene pass unchanged
  (`rtx_light_rules_tests.py`, `raven_bsp_shader_flags_tests.py`,
  `rtx_light_entity_tests.py`, `rtx_emitter_lighting_tests.py`,
  `rtx_renderer_contract_source_tests.py`).
- **Live subset.** An export of HEAD `dfe1b9d4` was built for Windows x86
  (`.tmp/runtime/mohaa-sun-before`), then its `rt_lights.c` replaced by this
  change and the RTX renderer relinked (`.tmp/runtime/mohaa-sun-after`,
  `fnql_rtx_x86.dll` `727e570b...`; the engine, package and other renderers
  are the same files). Each run was a windowed 800x600 client on a private
  desktop with `net_enabled 0`, no input, an empty home config and
  `thereisnomonkey 1`, the view fixed by console steps (`tele`, `face`; 2015's
  `wait` counts milliseconds) and taken by the engine's `screenshotPNG`
  (`.tmp/mohaa-sun/probe.py`). The installation and binaries were unchanged
  after every run. Levels are the mean sRGB of a fixed patch of the last
  capture.

| Map, view | Before: RTX log | After: RTX log | Before | After | GL, baked |
| --- | --- | --- | --- | --- | --- |
| `DM/mohdm6`, balcony at (-891, 756, 500), pitch 30 yaw 270 | "the first sky surface's shader `textures/sky/mohday1` has no q3map_sun" | "worldspawn sun 85.0 85.0 85.0 toward 0.000 -0.000 1.000", "worldspawn diffuse sky light 144.4 144.4 144.4 from 197 suns" | floor 8.0 5.8 3.6 | floor 87.2 68.4 48.7 | floor 110.3 87.9 62.7 |
| `m5l1a` (single player), street at (-2667, -418, 110), pitch 25 yaw 0 | "... `textures/sky/m5l2` has no q3map_sun" | "worldspawn sun 90.0 90.0 90.0 toward 0.459 0.265 0.848", "worldspawn diffuse sky light 152.9 152.9 152.9 from 197 suns" | sunlit street 12.9 12.7 13.5, shade 15.5 15.2 16.2 | sunlit 96.4 95.7 92.6, shade 48.7 48.0 46.7 | sunlit 80.0 79.5 77.5, shade 37.8 37.4 37.0 |
| `DM/mohdm6`, lamp-lit floor at (554, -455, -150), pitch 25 yaw 90 | as above | as above | floor 212.5 182.1 141.5 | floor 206.7 177.1 137.9 | floor 186.4 160.5 126.4 |

  With no sun, RTX lit `m5l1a` with its one light entity and `DM/mohdm6`'s
  balcony with none of its 293 lamps. With the worldspawn sun both are
  sunlit, and the building's shadow across `m5l1a`'s street and the
  balcony's shadow bands fall where the baked lightmaps put them. The traced
  levels are within a quarter of the baked ones; the walls in shade stay
  darker than baked, because the default mode traces bounced light in place
  of the compiler's `ambientlight` floor. The lamp-lit floor, which sees no
  sky, keeps its level within 3 %.

## Unrelated observation

`obj/obj_team2`'s commonest luxel is 35 31 28, its worldspawn's `ambient`
35 times `_color` "1.0 0.9 0.8", not its `ambientlight` 5.1: 2015's compiler
reads id's `ambient` and `_color` too. RTX's baked modes
(`rtx_rt_lighting 1` and `2`) read that pair, but not `ambientlight`, the
floor most maps use.
