# Medal of Honor: Allied Assault map sidecars

Each of Medal of Honor: Allied Assault's fifty-four official maps now ships two
sidecars in the engine package: an audio zone file (`<map>.azb`) and a static
light file (`<map>.lights.json`). They live under `games/mohaa/main/maps/`,
each beside its map in the folder the pak keeps it in (`DM/`, `obj/`,
`briefing/`). They give the profile fnTech3's room acoustics and static map
lights (AGENTS.md invariant 11), as
[F.A.K.K.2's](2026-09-24-fakk2-map-sidecars.md) and
[Alice's](2026-09-24-alice-map-sidecars.md) sidecars do for those games. The
machine-readable record is
[2026-09-25-mohaa-map-sidecars.json](2026-09-25-mohaa-map-sidecars.json).

The maps were read from `main/Pak5.pk3` of the 1.00 US installation (see the
[contracts record](2026-09-24-mohaa-contracts.md) and the
[maps record](2026-09-24-mohaa-maps.md)), which is the only retail pak that
carries maps. The installation was only read: its 2,663 files hash as laid
down after every probe. No retail file is committed. Marks: **[O]**
OpenMoHAA's reading (reference observation only), **[I]** inference.

## What the maps say

- **Format.** All fifty-four maps are "2015" version 19. Each shader (140
  bytes), surface (108), leaf (64), brush side (12), terrain patch (388),
  terrain index (2) and sphere light (56) lump divides into its record on
  every map.
- **Surface flags.** 2015 numbers fifteen materials from bit 13: paper, wood,
  metal, rock, dirt, grill, grass, mud, puddle, glass, gravel, sand, foliage,
  snow and carpet. Hint is bit 30 and nodlight bit 29; 0x100 and 0x200 are
  nolightmap and alphashadow, not Quake III's hint and skip
  (OpenMoHAA's `surfaceflags.h` **[O]**). **Measured:** every one of the
  retail maps' 394,739 faces (387,469 surfaces and 7,270 terrain patches)
  carries exactly one material flag. Rock takes 141,041, metal 133,313, wood
  90,318 and snow 11,548; puddle takes none. The flags are set per face:
  `textures/common/caulk` appears with 77 different flag values. A face whose
  shader names no material is rock: `carpet_fancy1`, which has no shader
  script, the `fence`-only `industrialgrate1` and `caulkshadow` all compile
  so. The compiler's default is therefore taken to be rock **[I]**.
- **Sphere lights.** Lump 19 holds the lights the compiler kept for lighting
  models: origin, colour 0..1, intensity, leaf, needs-trace, spot, spot
  direction and spot radius by distance (`code/qcommon/bsp_mohaa.h`). The
  fifty-four maps hold 5,188 of them, at most 423 (`m6l2a`). Ten have none:
  the six briefings, `credits`, `void`, `m5l2a` and `m5l2b`. None needs a
  trace, none has a non-positive intensity, and 626 of the 1,298 spots have
  a negative radius by distance. 2015 lights a model with
  `|I| x 7500 / d^2`, leaving a light out where that falls under 5; a
  negative-radius spot moves its origin across a disc of that radius and
  never passes its cone test (OpenMoHAA `tr_sphere_shade.cpp` **[O]**).
  fnTech3's MoHAA light grid already reads that law (`MOHAA_LIGHT_SCALE`,
  `tr_mohaa_world.c`).

## Audio zones

`fnql-audiozonesc --profile mohaa` now reads 2015 maps: the 28-lump directory,
the four longer records, and each leaf's terrain patches, which vote as drawn
ground. It refuses a 2015 map without the profile, and any other map with it.
Metal and grill vote metal; rock and glass vote stone; puddle votes liquid;
the other ten materials vote soft. The grouping is fnTech3's own. Surfaces
the compiler kept only to cast shadows (`nodraw`, 5,225 of them) do not vote.
With a flag on every face, the flag decides a face's material, and the
compiler's shader-name tokens are skipped for it. On the retail maps those
tokens disagree with the flags on 2,905 of 389,514 drawn faces (0.75%):
`forstsnow_rock256` is snow, and `stset_2sand` is rock. So
`scripts/generate_stock_mohaa_audio_zones.py` passes no material map. It
extracts each map into a fresh directory under `.tmp`, compiles and audits it,
and publishes only once all fifty-four pass.

All fifty-four pass: 5,817 zones in 1,375,982 bytes, twenty-two graded
excellent and thirty-two good; no map reaches the 512-zone cap. The briefings,
`credits` and `void` are one zone each. The warnings are the generated kind
the other profiles' zones carry: 32 maps resolve most lookups to map-scale
zones, and 14 have zones covering more than half their bounds. 1,067 zones
are outdoor and 18 underwater.

The compiler change is scoped to 2015 maps. Every other shipped zone file that
could be regenerated here was: Alice's 36, F.A.K.K.2's 30 and RTCW's 47. The
new compiler reproduced every one byte for byte.

## Static lights

`scripts/generate_stock_mohaa_lights.py` reads each map's sphere lights and
hands them to the Alice generator's rules. It excludes negative-radius spots
as emitters, and would exclude non-positive and black lights, though the
retail maps have none. It keeps up to 128 a map, spread across its PVS
clusters, with each spot's cone from its radius by distance. What changes is
the law. Each radius is where 2015's `|I| x 7500 / d^2` falls to 16,
`sqrt(7500 I / 16)`, a quarter of Alice's `30000` and so half its reach. The
colour balance (Alice's `balance_colors`: the eight strongest contributions
at a light's origin add at most half a surface's brightness) is computed over
those radii. A light's name is its map's base name and record number, and the
file's `map` keeps the folder.

Of 5,188 sphere lights, the 626 emitters are excluded and 3,349 selected;
twelve maps reach the 128-light limit. The selected radii run from 21.7 to
750 units. All but one selected light is balanced, the smallest to 0.1319.

**Where the renderers look.** Every renderer named a map's light file by its
base name, `maps/<name>.lights.json`, dropping the folder that the client's
audio zone lookup keeps. So `maps/DM/mohdm1.bsp` looked for
`maps/mohdm1.lights.json`. All three now look beside the map first,
`maps/DM/mohdm1.lights.json`, and take the base-name file only when that is
absent (`R_StaticMapLightsFilename`, `tests/static_map_lights_source_tests.py`).
For a map directly under `maps/` the two names are the same, so no other
profile's lookup changes.

## Reproducibility and package

Two runs of each generator gave byte-identical files for all 108. The
package's required inventory is 735 files, 108 of them Medal of Honor's
(`scripts/root_archive.py`, `scripts/stock_mohaa_maps.py`,
`tests/release_packaging_tests.py`, `tests/mohaa_static_lights_tests.py`).

## At run time

The probes used a copy of `meson/build/win32-mohaa`'s client and a GL renderer
relinked from that build's objects with the new `tr_bsp.c`
(`.tmp/runtime/mohaa-sidecars`). They loaded a package of `pkg/` plus these
files, windowed at 1280x720 on a private desktop, silent, with no input,
`net_enabled 0` and an isolated home. On `devmap`:

| Map | Zones loaded | Lights loaded | In the first view |
| --- | --- | --- | --- |
| `dm/mohdm1` | 166, `maps/dm/mohdm1.azb` | 119, `maps/dm/mohdm1.lights.json` | 8 promoted, 2 shadowed |
| `m1l1` | 68 | 116 | 8 promoted, 2 shadowed |
| `obj/obj_team2` | 139 | 128 | 8 promoted, 2 shadowed |
| `briefing/briefing1` | 1 | 0 | none |
| `m6l2a` | 261 | 96 | 8 promoted, 2 shadowed |

The zone files are found through the archive's case-insensitive lookup:
the map was named `dm/mohdm1`, the pak spells it `DM/mohdm1`. With the build's
own renderer, `dm/mohdm1` loads its zones but reads no light file, and `m1l1`
loads both.

**Frames.** Each map was also drawn with `r_staticLights 0`, and the engine
`screenshot`s compared by mean luminance of 255. The DM, objective and
`m6l2a` openings measured 89.2, 89.4 and 81.6 either way. `m6l3b`'s
lamp-lit corridor measured 59.8 against 57.5, and `m1l2b` 49.0 against 48.6.
The lamp's pool on `m6l3b`'s walls and floor is brighter, and nothing washes
out. MoHAA's lights reach half as far as Alice's, and their colours are
halved, so they add little beyond the lightmaps.

## Open

No one has listened to the zones, and no frame has been compared with
retail's, which cannot run here. The model-light law is OpenMoHAA's reading
**[O]**, and the rock default is an inference **[I]**. The balance is computed
for the renderers' defaults (`r_dlightScale` 0.5, eight static lights a frame).
The maps' static models do not vote in the zones, and the sun, which the
sphere-light lists name for leaves that see it, is not a static light. The
RTX path tracer does not read these files; it lights from the entity text
([COMPATIBILITY.md](COMPATIBILITY.md)).
