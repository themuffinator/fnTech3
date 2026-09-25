# Heavy Metal: F.A.K.K.2 map sidecars

Every one of F.A.K.K.2's thirty official maps ships two sidecars in the engine
package, under `games/fakk2/fakk/maps/`: an audio zone file (`<map>.azb`) and a
static light file (`<map>.lights.json`). They give the profile fnTech3's room
acoustics and static map lights, as Alice's sidecars give Alice
([Alice's record](2026-09-24-alice-map-sidecars.md)), whose pipeline this
extends. The machine-readable record is
[2026-09-24-fakk2-map-sidecars.json](2026-09-24-fakk2-map-sidecars.json).

The maps were read from `fakk/pak0.pk3` of the installation laid down from the
user's retail disc (see the [contracts record](2026-09-24-fakk2-contracts.md)),
which only carries maps; the English and German `pak1.pk3` carry none. The
installation was only read, and no retail file is committed.

## What the maps say

- **Format.** All thirty maps are `FAKK` version 12, with Alice's header, lumps
  and records. The zone compiler now reads version 12 under `--profile fakk2`
  and version 42 under `--profile alice`, and refuses either under the other.
- **Surface flags.** F.A.K.K.2's bits are Ritual's: wood, metal, rock, dirt,
  grill, organic and squishy at bits 16 to 22, castshadow at 6, nodraw at 7,
  nodlight at 23 and hint at 24, as Ritual's SDK (`fgame/surfaceflags.h`, read
  for reference only) declares. The compiled maps agree where they can: the two
  `surfaceparm metal` shaders carry 0x20000, the two `wood` ones 0x10000 and the
  three `sky` ones 0x4. These are `alice.exe`'s own table's bits (0x005294d0)
  but for Alice's `slide`, so the compiler reads F.A.K.K.2's with Alice's rules.
- **Compiled lights.** Lump 17 is Ritual's 56-byte `mapspherel_t` -- origin,
  colour, intensity, leaf, needs-trace, spot, spot direction, spot radius by
  distance -- which the SDK's `qcommon/qfiles.h` declares. Every map's lump
  divides by 56 and only two by Alice's 60: F.A.K.K.2 has no `cap`.

## Audio zones

`scripts/generate_stock_fakk2_audio_zones.py` extracts each map into a fresh
directory under `.tmp`, compiles it with `fnql-audiozonesc --profile fakk2` and
the F.A.K.K.2 material map, audits it, and publishes only once all thirty pass.

Like Alice's, F.A.K.K.2's maps carry almost no material surfaceparms (four
shaders in thirty maps), so materials come from shader names. The compiler's own
tokens classify 42.0% of the retail maps' 141,179 draw surfaces;
`scripts/fakk2-audio-materials.txt` adds F.A.K.K.2's texture vocabulary -- the
cemetery's and Eden's masonry, caves and crystal, Otto's machine hall, the swamp
mud, trees, hay, sheds and cloth, and its `wa_` water -- and raises that to
87.6%. Checked against every shader name the maps use, the rules disagree with a
compiler token only on the Water level's walls, statues and glass tubes (which
the compiler's `water` token calls liquid) and on the church's wood trim.

All thirty pass: 11,073 zones in 2,148,519 bytes, eight graded excellent,
twenty-one good and one fair -- `intro`, a three-zone cinematic box. Eleven maps
reach the 512-zone cap. The audit's warnings are the generated kind Alice's and
RTCW's carry too: 21 maps resolve most lookups to map-scale zones, six have
zones covering more than half their bounds, one has no portal hints and one low
portal confidence.

## Static lights

`scripts/generate_stock_fakk2_lights.py` reads each map's compiled lights with
F.A.K.K.2's record and hands them to the Alice generator's rules: surface-light
emitters (spots with a negative radius by distance), non-positive and black
lights are excluded; each light's radius makes the renderers' model-light law
equal `fakk2.exe` 1.02's at its defaults, which is `alice.exe`'s
`|I| x 30000 / d^2`; and up to 128 a map are chosen, spread across its PVS
clusters. No light is capped.

**The law is `fakk2.exe`'s own.** It was read on 2026-09-25 with capstone from
the plain `fakk2.exe` 1.02 of the official patch (SHA-256 `681bc51c...`; the
disc's copy is encrypted, see the [contracts record](2026-09-24-fakk2-contracts.md)).
The per-light code at 0x0048dd80 multiplies `|I|` by the 7500.0 stored at
0x004d6050 and by `Q_rsqrt(d^2)` squared (0x0043c0b0, at 0x0048ded6), leaves
out a light whose power at the model is under 1.0, and adds the rest into six
directions; no cap clamps a light. The spot path (0x0048d990, at 0x0048db75),
the per-vertex path (0x0048e5d4) and the dynamic lights (0x0048f283) multiply
by the same constant. The dispatcher at 0x0048e170 takes that path for a model
flagged 0x200 or 0x8000000 (Ritual's SDK calls them `RF_EXTRALIGHT` and
`RF_INVISIBLE`; read for reference only), calls it for every light it
gathers, and ends at 0x0048e090, which multiplies each summed direction by
`1 << (r_mapOverBrightBits - tr.overbrightBits)` before scaling it down to 255
at most. That is the shift the engine gives all its map lighting: the light
grid (0x00483b07) and the other lighting loaders (0x00484720, 0x00485063,
0x00485422) shift their bytes by the same. `r_mapOverBrightBits` is
registered "2" (0x00460edb) and `r_overBrightBits` "0", from which 0x0046ff70
sets `tr.overbrightBits`, so at the defaults the sum is multiplied by 4:
`|I| x 30000 / d^2`. `alice.exe`'s matching code (0x004ab850) multiplies by
30000 (0x005004a0) and clamps to its record's `cap` (0x004ab9c9), and its
last step (0x004abb80) shifts by `tr.overbrightBits` alone, 0 at its
defaults: the same 30000. fnTech3's F.A.K.K.2 profile keeps both defaults, so
its light grid takes the same factor of 4, while a static light reaches a
model as a dynamic light, unshifted, and needs the whole law in its radius.

So the inference the files were generated under holds: they already carried
F.A.K.K.2's own law, and they are unchanged. The generator now states that law
(7500 a light, times `1 << (2 - 0)`) and checks it against the Alice rules'
constant. Read without the final factor, the per-light constant alone suggests
half the radius; that variant was generated and measured against retail
(below), and not shipped. The factor was pointed out by the model-lighting
work (`2026-09-25-ritual-model-lights.md`).

Of 10,267 compiled lights, 229 are emitters and 8 have no positive intensity;
the emitters account for every map's records beyond its `light` entities
(`creeperpens` 436 records for 421 entities, `training` 497 for 426). 3,707 are
selected; 26 maps reach the 128-light limit. Three selected lights -- `blood`'s
two of intensity 99,999 and one of `oracle`'s at 50,000 -- come out wider than
the 8,192 units every renderer clamps a static light's radius to; the files keep
the law's value and the renderers clamp it.

**The colours are balanced** by the Alice generator's `balance_colors`. The
renderers' default per-pixel pass adds a light's colour times
`smoothstep(1 - d^2/R^2)`, with R half the radius. That lands nearly the whole
colour anywhere well inside R, on top of lighting the map has already baked,
and eight such lights stack. Each light's colour is scaled so that the eight
strongest contributions at its origin, its own included, add at most half of a
surface's brightness. 3,680 of the 3,707 lights are scaled, the smallest to
0.0656. The rule counts the other lights at their unbalanced colour: with
every light at its published colour, the eight strongest contributions exceed
0.51 at 802 light origins, 0.628 at the 95th percentile and 0.918 at most.
Before the balance, the opening of `towncenter_evil` averaged 123 of
255 with the lights and 22 without; `water` 80 and 48; `blood` 53 and 31. The
flying metal debris of `intro`, lit by a lattice of 96 fill lights of intensity
5,000 and 10,000, drew pure white. Balanced, those openings average 46, 61 and
42, and the debris draws as textured metal.

## Reproducibility and package

Four runs of the zone generator -- the last after the material map's line
endings were normalized -- and three of the light generator gave byte-identical
files for all sixty. On 2026-09-25 the light generator, restated with
`fakk2.exe`'s law, reproduced all thirty light files byte for byte twice more.
The package's required inventory is 579 files, sixty of them F.A.K.K.2's
(`scripts/root_archive.py`, `tests/release_packaging_tests.py`).

## At run time

On `meson/build/win32-fakk2` (GL renderer, windowed 800x600, isolated home, no
input), `devmap fakkhouse` with sound loads 343 zones from
`maps/fakkhouse.azb` and 128 static lights from `maps/fakkhouse.lights.json`;
in play the renderer promotes eight of them into the view, two casting shadows,
and the frame draws as before. The installation was unchanged. On 2026-09-25
the same probe with `r_staticLightDebug 1`, run on the shipped files and on
the half-radius variant below alike, loads 128 lights (111 points, 17 spots)
and promotes eight in play with two shadowed.

## Against retail

On 2026-09-25 three maps' first views were set beside retail's. fnTech3's
dedicated server started each map; retail `fakk2.exe` 1.02 joined it, then
fnTech3's client, and each skipped the opening cinematic and captured the view
with the far fog on (the steps of `.tmp/fakk2/fog-light/compare.py`, which
measured the far-fog corrections, run by
`.tmp/fakk2-light-law/compare_frozen.py`; windowed 800x600 on a private
desktop, isolated homes, the session unlocked, no input). fnTech3 ran from two
frozen runtimes, identical but for the package's thirty F.A.K.K.2 light files:
the shipped files (*shipped*), the same lights at half the radius (*half*),
and the shipped files with `r_staticLights 0` (*off*). Each figure is
fnTech3's region mean over retail's on the GL renderer:

| Map and region | Shipped | Half | Off |
|---|---|---|---|
| `over`, left wall | 1.20 | 0.94 | 0.94 |
| `over`, walkway | 1.40 | 0.97 | 0.97 |
| `over`, wall by the lamp | 1.41 | 1.38 | 1.12 |
| `over`, canal water | 1.52 | 1.27 | 1.18 |
| `towncenter_evil`, arch | 1.20 | 1.00 | 0.97 |
| `towncenter_evil`, door | 1.27 | 1.01 | 1.01 |
| `towncenter_evil`, wall between the lamps | 1.28 | 1.12 | 1.03 |
| `towncenter_evil`, wall by the right lamp | 1.55 | 1.39 | 1.00 |
| `towncenter_evil`, console | 1.59 | 1.36 | 1.01 |
| `fakkhouse`, stone, metal band and path | 1.00 to 1.01 | 1.00 to 1.01 | 1.00 to 1.01 |

The Vulkan and RTX renderers agree within 0.03 on every `over` and
`towncenter_evil` wall, door and console above, and within 0.08 on
`towncenter_evil`'s arch and floor; the RTX renderer's canal water was 1.31
shipped, 1.22 at half the radius and 1.19 off. On `fakkhouse` both draw the
stone at the left, the path and the window 1.10 to 1.23 times retail's in all
three variants alike: their default bloom, not the lights.

Over the whole view, the shipped files brighten 82% of `over`'s pixels by more
than 5%, and half the radius 37%, the lamp's wall and the water by Julie; the
frame averages 44.9 of 255 shipped, 39.3 at half and 35.7 off (retail 32.6).
`towncenter_evil`'s goes from 67% to 53%, and from 53.1 to 47.7 (43.2 off, and
retail 43.2). On `fakkhouse`'s outdoor opening the lights brighten only 2.3%
of the pixels shipped and 0.8% at half.

So at the law's radii the static lights brighten the world past retail's:
`over`'s tunnel walls to 1.20 to 1.41 and `towncenter_evil`'s room to 1.20 to
1.59, where without them those walls draw at 0.94 to 1.12 and 0.97 to 1.03.
Half the radius would leave the walls away from the lamps as they are without
the lights, and walls beside a lamp would still draw up to 1.39; but the
radius is also what the lights give models, and half of it would give them a
quarter of retail's light. The brightening is the balance's: it lets a light
add up to half of full brightness at its origin, over walls whose baked light
is dimmer than that.

Retail gives these lights to its models (the code above) and draws the world
from its lightmaps, so `r_staticLights 0` is the retail-exact look for the
world. The lights stay on by default as fnTech3's own feature, the renderers'
per-pixel static lights with their shadows (AGENTS.md invariant 11).

## Open

No one has listened to the zones. The lit frames have been set beside
retail's on three maps, not the other twenty-seven. The zone rules are naming
heuristics, as Alice's are. The balance is computed for the renderers'
defaults (`r_dlightScale` 0.5, eight static lights a frame); other settings
change how bright the lights draw. It measures a light against full
brightness, not against the surface's own baked light, so at the law's radii
the lights draw `over`'s and `towncenter_evil`'s walls up to 1.6 times
retail's. Whether the world's share should follow the baked light, while
models keep the law's radius, is a presentation choice for a change of its
own.
