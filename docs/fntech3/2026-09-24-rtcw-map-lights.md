# Static light files for RTCW's official maps

The engine package now carries a static light file, `maps/<map>.lights.json`,
for each of RTCW's 47 official maps: 32 under `games/rtcw-sp/main/maps/` and
15 under `games/rtcw-mp/main/maps/`, beside their audio zones. Together they
hold 4,316 lights in 843,617 bytes, 2,533 for SP and 1,783 for MP. They feed the
renderers' static map lights (`r_staticLights`, on by default), which RTCW
takes in the per-pixel dynamic-light modes under the rules of the engine
scene lights record (`2026-09-24-rtcw-engine-lights.md`).

The [machine-readable record](2026-09-24-rtcw-map-lights.json) pins the
installation, base commit, generator and its constants, every map's input and
output hashes and counts, the lightmap evidence, the packages and every
runtime run's command, counters and capture hashes. Only derived data is
shipped: no map, texture or retail module is incorporated, and the
installation was only read.

Reproduce, with a legitimate installation:

```powershell
python scripts/generate_stock_rtcw_lights.py "E:/SteamLibrary/steamapps/common/Return to Castle Wolfenstein"
python scripts/build_root_archive.py --output .tmp/rtcw-map-lights/fntech3-pkg.fnz
python scripts/verify_release_layout.py .tmp/rtcw-map-lights/fntech3-pkg.fnz
python scripts/probe_rtcw_light_falloff.py "E:/SteamLibrary/steamapps/common/Return to Castle Wolfenstein" --output .tmp/rtcw-map-lights/falloff.json
```

The generator reads the same official paks as the audio zones, with the same
inventory, patch precedence and refusals ([audio-zone
record](2026-09-17-rtcw-audio-zones.md)), and publishes only once all 47 maps
have converted, replacing each file atomically and leaving other files alone.
Three runs produced 47 byte-identical files.

## Where the lights come from

RTCW's IBSP 47 maps keep their compiled light entities in the entity lump.
Across the 47 maps that is 10,125 `light`, 4,460 `lightJunior`, 331 `dlight`
and 3,132 `corona` entities (and five `misc_spotlight`). The keys on the
`light` entities are `light` (10,112), `_color` (10,098), `spawnflags`,
`fade` (4,779), `target` (2,136), `radius` (1,491), `angle` (1,442), `_fade`
(21), `style` and `targetname` (9 each) and `noise` (8).

RTCW's own game source documents them in its `QUAKED light` comment
(`g_misc.c`, [SP](https://github.com/id-Software/RTCW-SP/blob/master/src/game/g_misc.c)
and [MP](https://github.com/id-Software/RTCW-MP/blob/master/src/game/g_misc.c)):
the default intensity is 300; lights are linear unless spawnflag 1,
`nonlinear`, gives inverse-square falloff; spawnflag 2 adds the angle term;
4 and 8 are negative lights; 16, `q3map_non-dynamic`, keeps a light out of
the light grid, "and therefore will not light dynamic models"; a light aimed
at a target is a spotlight whose `radius` (default 64) is the cone's at the
target; and `fade` adjusts a linear light's slope. `lightJunior` is the
opposite of spawnflag 16: it lights the grid only, never the lightmaps.

The numbers come from q3map2's Wolf lighting model (`wolfLight`, in
`light.c` and `light_ydnar.c` of [NetRadiant](https://gitlab.com/xonotic/netradiant)'s
`tools/quake3/q3map2`), which reproduces RTCW's compiler: a light's
photons are its value times 7500; a linear light gives
`photons / 8000 - fade * d`, so `0.9375 * light - fade * d`, falling to zero
at `0.9375 * light / fade`; an inverse-square light gives
`photons / d^2`; every distance is at least 16; a spotlight is always
inverse-square with the angle term, its cone's radius at a distance d along
its axis is `(radius + 16) / dist * d`, softened over the last 32 units. Both
sources were read as reference observation only; no text from either is
incorporated.

### What the maps' own lightmaps show

The shipped lightmaps were baked from these entities, so
`scripts/probe_rtcw_light_falloff.py` reads them back: each planar surface's
lightmap texels get their world positions from the surface's draw vertices,
and a texel is compared with a light only when it faces the light and the
segment between them crosses no solid world brush. Patches, models and
surface lights are not modelled, so single texels are noisy and only medians
over many lights are read.

- **`fade` multiplies the distance.** `mp_keep`'s 31 lights of 140 with
  fade 2.5 reach 52 units that way, 131 without fade and 328 if fade divided
  it. The texels 50 to 200 units from them sit 3 to 4 levels above ambient
  (medians over 21 and 29 lights), where no fade would give 56 at 75 units
  and division 101 and 71. The `QUAKED` text ("multiply the run of the
  slope by fade") reads either way; the lightmaps settle it.
- **Spawnflag 1 is inverse-square at 7500.** Over 154 such lights in `dig`,
  `boss2` and `mp_base`, the median ratio of baked light to
  `7500 * light / d^2 * cos` is 1.05 (quartiles 0.14 and 2.09).
- **Not established:** that a fade below 1 extends a light's reach (40 small
  lights with fade 0.2 in `escape1` and `mp_destruction`: 8.2 levels between
  the two readings' reaches against 9.0 beyond both, lost in the surrounding
  light), and that spotlights are confined to their cones (85 spots: a median
  15 levels inside the cone against 10 outside, brighter inside for 41). Both
  follow q3map2's model here.

A relight of retail maps with NetRadiant's q3map2 in Wolf mode, which would
have compared whole lightmaps, did not finish: without the compile-time
`.srf` file its lightmap setup ran single-threaded for over ten minutes on
`cutscene1` and was stopped.

## Which lights become static lights

Of the 10,125 `light` entities, 8,143 are usable. Left out:

| Reason | Lights | Why |
| --- | --- | --- |
| `nonDynamic` | 1,579 | Spawnflag 16. Retail keeps the light out of the grid, so it lights no model; a static light lights models first of all, the world already holding its baked light. `mp_ice` has 190 of its 203, `forest` 77 of 81. |
| `atCgameLight` | 244 | Within 64 units of a `dlight` entity: that fixture's baked light, usually in the same colour. The cgame already draws a light there, 300 units across at its style string's middle value (`CG_Dlight`, [SP cgame](https://github.com/id-Software/RTCW-SP/blob/master/src/cgame/cg_ents.c)). |
| `unlit` | 112 | Never a whole level at the compiler's 16-unit minimum distance: 65 lights of 40 with fade 2.5 (`37.5 - 2.5 * 16`), 46 of 12 or 15 with fade 1 and one of 30 with fade 2, in ten maps. |
| `outsideClusters` | 36 | In no PVS cluster: inside solid. |
| `styled` | 9 | `style` 32 to 36 with a `targetname`, in `mp_depot` and `mp_keep`: switchable styles whose baked form is not established. |
| `negative` | 2 | Spawnflag 4 or 8, in `sfm`. |

`dlight` entities, which the cgame submits itself, and grid-only `lightJunior`
entities are not read as lights at all. 102 lights name a target no entity
has; the compiler lights them as point lights with their own flags, and so
does the conversion. No light carries `_sun`. Keys the compiler does not read
(`angle`, `_fade`, `noise`) are ignored, and numbers are read as its `atof`
reads them, so the nine `fade` values `,25` and `0,2` are 0, which means 1.

The usable lights are 4,758 linear, 1,497 inverse-square and 1,888 spots.

## The conversion

The renderers light a surface from a dynamic light with
`colour * smoothstep(1 - d^2 / r^2)` (default `r_dlightFalloff 1`), `r` being
the file's radius times `r_dlightScale`, on top of the baked lightmap. A spot
is drawn as a segment of the file's radius along its direction, lit to the
scaled radius around it; `r_spotShadows` is off by default, so nothing limits
it to its cone.

Each light's radius and brightness are the least-squares fit of that
falloff to retail's baked curve, sampled 200 times out to where retail's
light ends (a linear light's zero, an inverse-square light's one level),
each distance weighted by the area it covers on a surface through the light,
along the axis for a spot. Retail's curve is in lightmap levels as RTCW
draws them: with `r_mapOverBrightBits 2` over `r_overBrightBits 1` a
lightmap value is shifted left once, so 128 lights a surface as brightly as a
dynamic light of colour 1, and anything above 128 is clamped. The brightness
multiplies the light's `_color`, normalised to its brightest channel as the
compiler normalises it.

| Retail light | Fitted brightness | Drawn radius |
| --- | --- | --- |
| linear 30, fade 0.2 (reach 141) | 0.147 | 139 |
| linear 300 (reach 281, clamped near the light) | 1.0 | 308 |
| inverse-square 100 | 0.895 | 185 |
| spot 300, SP | 0.809 | segment 131 + 131 |
| spot 300, MP | 0.791 | segment 162 + 81 |

An earlier conversion matched only each light's reach at full brightness;
in `escape1`'s long corridor it washed the wall out (470,442 pixels changed,
where retail's lightmap shows dim cone patches), because the renderers'
falloff stays near full brightness until close to its radius while retail's
inverse-square light halves within twice its clamped distance.

**Per mode.** Every dynamic light's radius is scaled by `r_dlightScale`,
which RTCW SP defaults to 1.0 (retail's own cvar, which also scales SP's
module lights) and MP to 0.5. Each file is fitted at its own mode's default,
so a point light's MP file radius is twice its SP one and both draw the same
light. A spot's segment is the file radius unscaled, so MP's fit differs a
little: a longer segment, a thinner light. Each file names its `profile` and
the `dlightScale` it was fitted at; the renderers skip both keys.

`intensity` is written equal to the radius, the parser's own default, so the
light's shadow priority multiplier is 1 in both modes. Every light has
`castsShadows` (the renderers shadow at most two promoted point lights a
scene); spots carry their direction and `outerAngle`, the arctangent of their
radius by distance. Radii are clamped to the renderers' 8,192.

### Choosing 128 a map

The renderers keep 128 lights a map. 27 maps have more usable lights than
that; 3,827 lights are dropped in all. They are chosen round-robin across the
map's PVS clusters, as for Alice: each round takes every cluster's strongest
remaining light, strongest first, by fitted brightness times drawn reach, so
no area loses its main light to a brighter area's fifth. The other 20 maps
keep every usable light, from 3 on each cutscene map to 116 on `dam`.

## Package and validator

`scripts/stock_rtcw_maps.py` now lists both sidecars of every map, which
feeds the Meson package inputs, and `scripts/root_archive.py`'s required
inventory grows from 579 to 626. The package validator
(`validate_root_archive`, run by `verify_release_layout.py` and the release
packager) now also reads every packaged `maps/*.lights.json` as the
renderers would and refuses one that one of them would reject or cut short:
over 256 KB or 65,536 tokens, a repeated key, a non-finite number or an
exponent, not version 1, more than 128 lights, a light that is neither point
nor spot, lacks an origin, has a malformed colour, is a spot without a
direction, or has neither a positive radius nor a positive intensity. All
113 packaged light files -- Alice's 36, F.A.K.K.2's 30 and RTCW's 47 -- pass.

Package entries outrank ordinary game data, as they already did for zones and
fog, so for these 47 maps a `maps/<map>.lights.json` in a mod or home
directory is no longer read. Probes that place a fixture light file for one
of them, such as `scripts/probe_rtcw_engine_lights.py`, need a package
without it.

## Runtime

A client runtime copied from `meson/build/win32-dev` (built with
`rtcw-development`, and carrying the engine scene lights change) ran from
`.tmp/runtime/rtcw-map-lights` with the new package, and the same binaries
from `.tmp/runtime/rtcw-map-lights-control` with a package identical but for
RTCW's 47 light files. Every run was windowed at 800 by 600 on an unswitched
private desktop, with no input, an isolated home, `net_enabled 0`, and only
the engine's `screenshotPNG` (`scripts/probe_rtcw_map_lights.py`). The
installation holds no light file, no home gained one, and the installation's
file inventory did not change.

- **The files load from the package.** In every run `r_staticLightReload`
  reloaded exactly the package's count, 128 on each map probed, and the
  control package's runs loaded none.
- **`mp_castle`, one demo replayed with `timedemo 1`.** With the lights on,
  eight are promoted each frame, and the frame changes in 85,748 to 87,323
  pixels -- GL, Vulkan and RTX within 1% of each other -- warming the floor,
  a pillar by its lamp and the first-person weapon, by about 22, 13 and 8
  levels a channel where it changes. With `r_dlightMode 0` the frames with
  and without the lights are byte-identical in all three renderers.
- **The change is the light files.** The control runtime's frames with
  `r_staticLights 1` are byte-identical to its frames with 0, and to the
  candidate's frames with 0, in all three renderers.
- **`mp_base`**: eight promoted, two shadowed; about 21,000 pixels change,
  mostly the weapon and the lit end of the corridor. Turning
  `r_staticLightShadows` off changes nothing there, as it changes nothing in
  `mp_castle`: no model stands between a shadowed light and a surface in
  view.
- **`escape1`, live.** At the level's start the nearest light is a small
  spot 92 units away that reaches 48, and the next a fill light 400 units
  away that reaches 106. The eight promoted lights change nothing measurable
  in Vulkan and RTX (10 pixels); GL's 4,813 are its own frame-to-frame
  motion, which differs as much between two captures with the lights on.
  Moved into the lamp-lit corridor (`setviewpos -3150 250 200 90`), the
  lights change 120,588 to 142,039 pixels by about 4 levels a channel, at
  most 21, on the wall retail already lights, and retail's cone patches stay
  visible.

## FnQ3/FnQL features (invariant 11)

RTCW's lights are adapted into the engine's own static map lights: the
sidecar format, PVS promotion under `r_staticLightMaxLights`, the per-pixel
pass with its smoothstep falloff, the shadow atlas for up to
`r_staticLightShadowMaxLights` point lights, and the engine's spot form. The
retail curve is evidence for each light's reach and brightness, not for its
presentation: the engine draws it per pixel, on models as well as the world,
with shadows. Left out: RTCW's original projected-light mode
(`r_dlightMode 0`) and `r_dynamiclight` 0 and 2, which take no engine light
by the engine scene lights rules. There is no retail-exact option, since retail
has no such lights; `r_staticLights 0` returns to the baked lighting alone.

## What is only an engine-owned estimate

- A static light adds to a world that already holds the same light in its
  lightmap, so where a light is drawn the surface is lit about twice.
- The fit matches retail's curve along one line from each light, facing it;
  retail's linear lights have no angle term unless spawnflag 2, while the
  renderers always apply one.
- Spots are drawn as segments without a cone, and the fit follows only the
  axis. Retail's cones are unconfirmed by the lightmaps.
- Which 128 lights a map keeps, the 64-unit rule for a `dlight`'s fixture
  and leaving out non-dynamic lights are choices made here.
- Nothing is compared with retail frames: retail has no such lights.

## What this does not claim

No frame is compared with retail, no shadow cast by these lights was seen,
and only `mp_castle`, `mp_base` and two views of `escape1` were run. The
lightmap tests confirm `fade` and the inverse-square law only in the medians
above. Native RTX ray tracing is not exercised for RTCW.
