# Audio zones and static lights for Alice's official maps

The engine package now carries two generated sidecars for each of American
McGee's Alice's thirty-six official maps, under `games/alice/base/maps/`: an
audio zone file, `<map>.azb`, and a static light file, `<map>.lights.json`.
Together they are 72 files: 13,506 zones in 2,627,694 bytes and 3,968 lights
in 697,339 bytes.

The [machine-readable record](2026-09-24-alice-map-sidecars.json) pins the
installation, the base commit and the dirty-tree status, both generators, the
compiler, the material map, every map's input and output hashes, the audit
results and the runtime probes. Only derived data is shipped: no map, sound,
texture or retail module is incorporated, and the installation was only read.
See the [map record](2026-09-22-alice-maps.md) for the FAKK 42 format itself.

Reproduce, with a legitimate installation and a compiler built from this
change:

```powershell
python scripts/generate_stock_alice_audio_zones.py "E:/Games/Alice" --tool meson/build/win32-dev/fnql-audiozonesc.exe
python scripts/generate_stock_alice_lights.py "E:/Games/Alice"
python scripts/build_root_archive.py --output .tmp/alice-map-sidecars/fntech3-pkg.fnz
python scripts/verify_release_layout.py .tmp/alice-map-sidecars/fntech3-pkg.fnz
```

Both generators read only `base/pak0.pk3`, the one official pak with maps,
and refuse a changed inventory, a case-ambiguous or unsafe map name, and a
custom pak named like a retail one. They extract nothing into the
installation, and they publish only once every map has passed, replacing each
file atomically and leaving other files alone.

## Audio zones

### Reading Alice's maps

The zone compiler, `fnql-audiozonesc`, read only IBSP 46 and 47. It now reads
FAKK 42 too: the twenty-lump directory after the ident, version and checksum,
renumbered into the Quake III slots it already reads, and the shader and
surface records at 76 and 108 bytes, one trailing field longer than Quake
III's with every field it reads at Quake III's offset. A FAKK map needs
`--profile alice`, and `--profile alice` needs a FAKK map; either mismatch is
refused before anything is written, as is F.A.K.K.2's own version 12, a
truncated directory, a lump outside the file, or a record lump that is not a
whole number of records.

### Ritual's flags

Alice's surface flags are Ritual's, not Quake III's. Their values come from
`alice.exe` itself: its shader parser's `surfaceparm` table at `0x005294d0`
is 36 entries of name, clear-solid, surface flags and contents. The
material bits run from bit 16: wood `0x10000`, metal `0x20000`, rock
`0x40000`, dirt `0x80000`, grill `0x100000`, organic `0x200000` and squishy
`0x400000`. The table also holds one name Ritual's F.A.K.K.2 SDK lacks,
`slide` at `0x2000000`, and has no `skip`. The liquid contents are Quake
III's: water `0x20`, slime `0x10`, lava `0x8`.

Under `--profile alice`:

- metal and grill vote metal, rock votes stone, and wood, dirt, organic and
  squishy vote soft. The grouping is this engine's heuristic, the same one it
  applies to RTCW's and ET's wood and ground;
- Quake III's flesh bit (`0x40`) is Ritual's `castshadow`, and its metal-steps
  bit (`0x1000`) is unused, so neither votes;
- an undrawn brush side is `nodraw` (`0x80`) or Ritual's `hint`, which is bit
  24. Quake III's hint (`0x100`) and skip (`0x200`) bits mean nothing here,
  and RTCW and ET give bit 24 to rubble, so the mask follows the profile.

Sky (`0x4`) keeps its meaning.

### Alice's textures

The retail maps barely use material flags: of 3,969 shader records, eight
carry one, all `metal`, all on `textures/skin/flehblobber`. Materials
therefore come from shader names. The compiler's own name tokens name 54.7%
of the retail maps' 183,673 draw surfaces. `scripts/alice-audio-materials.txt`
adds 28 rules for Alice's own vocabulary -- `/rok`, `brk`, `marb`, `drt`,
`grnd`, `bark`, `hedgewall`, `gear`, `bronze` and the like -- and raises that
to 76.3%. Each rule was checked against every shader name the retail maps
use. None matches a name the compiler's tokens give a different material,
except `bark_water`, which is both. Three candidates were dropped for that
reason: `moss` matched mossy brick and rock, `hedge` matched a liquid shader
and some metal bolts, and `icewall` matched `alicewall`. Every rule has
weight 1, on a par with the compiler's own tokens.

### Results

All thirty-six maps generated and audited: 14 graded excellent, 20 good and 2
fair (`fortress2`, `potears3`). Seventeen reach the 512-zone ceiling. Nineteen
carry the warning that most lookups resolve to map-scale zones, and four that
zones cover more than half of the sidecar bounds. As for RTCW and ET, the
audit ran without `--strict`, so these heuristic warnings are recorded rather
than treated as failures.

| Preset | Zones | | Material | Zones |
| --- | --- | --- | --- | --- |
| small-room | 8,453 | | stone | 5,244 |
| hallway | 1,130 | | soft | 3,962 |
| underwater | 952 | | metal | 1,941 |
| hall | 928 | | neutral | 1,066 |
| stone-room | 783 | | liquid | 952 |
| outdoors | 780 | | sky | 341 |
| room | 480 | | | |

Two results were checked against the maps, because both look wrong at first
sight:

- `garden1`, `garden2` and `gvillage` have no outdoor zone. None has a sky
  surface: their open areas are walled and fogged, so no zone sees sky.
- 478 of `utemple`'s 512 zones are underwater. The level is submerged: 14
  `textures/liquid/water_nodraw` and slime brushes hold it, and its far
  plane is a teal fog.

Four runs over two builds of the compiler, the Meson target and a separate
`/O2` compile, produced 36 byte-identical files.

### Heard through the engine's own room

Alice's OpenAL backend plays the room these zones give, as it does for every
profile. It first kept retail's reverb instead: none unless `s_reverb 1`,
which retail ships at 0, and then only the room the client game names, which
no shipped map does (the `TriggerReverbs` section of each of the thirty-six
`maps/*.snd` files is empty), so the zones loaded and changed nothing. Under
the project's rule that every profile runs on fnTech3's own features, Alice's
voices now take the backend's distance model, occlusion, zone reverb and
tone, and a room the client game names replaces the zones only while it is
named; see the [audio record](2026-09-23-alice-audio.md#the-engines-own-features).
On `garden1`, `s_info` reports `Audio zones: 512 loaded from maps/garden1.azb,
active auto-a1-hall-511->auto-a1-underwater-499 0.45, material soft`, and the
level's sounds carry reverb sends.

## Static lights

### What the renderers do with them

A `maps/<map>.lights.json` feeds the renderers' static map lights
(`r_staticLights`, on by default). They keep up to 128 lights per map. For
each scene with a world, they promote up to eight of the lights in the
potentially visible set (`r_staticLightMaxLights`) into dynamic lights,
ranking each by brightness times intensity, weighted by distance. Up to two
of them (`r_staticLightShadowMaxLights`) become shadow candidates. GL,
Vulkan and RTX raster share the file format; RTX's parser is the strictest,
refusing a duplicated key, a file over 256 KB, or more than 65,536 tokens.

### Where the lights come from

Each FAKK map carries its compiled light list in lump 17. Until now nothing
had established that lump's record. `alice.exe`'s loader establishes it: at
`0x0049a360` it refuses a lump that is not a whole number of 60-byte
records, refuses a map with more than 1,532 lights, and copies each record's
fields into the engine's light array, as follows:

| Offset | Field |
| --- | --- |
| 0 | origin, three floats |
| 12 | colour, three floats |
| 24 | intensity |
| 28 | leaf |
| 32 | needs trace (0 or 1) |
| 36 | spot (0 or 1) |
| 40 | spot direction, three floats |
| 52 | spot radius by distance |
| 56 | cap |

The first 56 bytes are Ritual's `mapspherel_t` as the F.A.K.K.2 SDK publishes
it (consulted as reference only), and the last float is one Alice adds. The
fields were then checked against the source entities on all thirty-six maps.
All 10,184 records name a leaf in range, and 10,124 lie inside it. 8,790 sit
at the origin of exactly one `light` entity. Of those, 8,331 match that
entity exactly:

- intensity is its `light` value, defaulting to 300;
- colour is its `_color`, defaulting to white;
- the trailing float is its `cap` key.

Needs-trace is spawnflag 4 (`ENTITY_TRACE`) on every one. The spot flag is
set exactly when the entity has a `target`, `radius`, `angles` or `spot_dir`,
and on all 71 targeted spots the direction is the unit vector toward the
target. The radius by distance is `(radius + 16) / distance`, Quake III's
q3map convention. Lights flagged `NO_ENTITIES` are left out, which is why
`garden4` has 135 records for 160 lights.

Two exceptions are not explained. 458 records compiled black although their
entity has no `_color`; the cause is not established, but retail lights no
model with them. One light spells its key `CAP` and compiled uncapped, so the
compiler reads `cap` in lower case. The remaining 1,367 records have no
entity at all. All but one are spots whose radius by distance is negative:
the compiler's surface-light emitters, over sky, flame and slime shaders.
What the negative value encodes is not established.

### How retail uses them, and the conversion

Retail lights a model from this list at `0x004ab850`. It drops a light that
fails its trace, passes a spot to `0x004ab460`, and for a point light takes
`power = |intensity| x 30000 / d²` (the constant is at `0x005004a0`). It
drops the light when that is below 1, and clamps it to `cap` when a cap is
set.

Each static light's radius is `sqrt(30000 x intensity / 16)`, the distance
where retail's own power falls to 16, which is where the renderers' own
vertex law for a dynamic light, `16 x radius² / d²`, ends too: 750 units for
the default 300. The renderers scale every dynamic light's radius by
`r_dlightScale`, 0.5 by default, so by default a light ends where retail's
power has fallen to 64, a quarter of full. An earlier version of this record
said that the radius made a light follow retail's law on a model. It does
only at `r_dlightScale 1`, and only in `r_dlightMode 1`: the default mode,
2, lights world and models per pixel instead (below).

A dynamic light cannot clamp, so a capped light's colour is scaled by
`cap / 255`. Near the light it then never gives more than retail's clamp.
Beyond the clamp's reach, `sqrt(30000 x intensity / cap)`, it is dimmer than
retail by the same factor. That is a choice made here: retail's rule is
followed exactly only for uncapped lights. A spot's outer angle is the
arctangent of its radius by distance, and its direction is the record's.

### The balance

The default per-pixel pass adds `colour x smoothstep(1 - d² / R²)` to a
surface, `R` being the radius times `r_dlightScale`: nearly the whole colour
anywhere well inside `R`, however bright the light, and on top of the baked
light, which already holds the same light. The first files wrote every light
at its own colour, and eight overlapping lights -- the default
`r_staticLightMaxLights` -- washed surfaces white: on `jlair2`, 23 lights of
which 17 have an intensity of 5,000 or more, the frame's mean brightness
went from 39.6 with `r_staticLights 0` to 67.6, and the Jabberwock and Alice
turned pale. The F.A.K.K.2 session found the same on its maps.

So each light's colour is now scaled until the eight strongest per-pixel
contributions at its own origin, its own included, add at most half the
surface's own brightness there (`balance_colors`, computed for the default
`r_dlightScale` and budget). An isolated light at full colour is halved; eight
at one point share that half; a dim light is never raised. 3,162 of the
3,968 lights are scaled, on `jlair2` all 23, down to 0.077. The static lights
are a supplement to the baked light -- dynamic shading and shadows -- not a
second copy of it.

The balance does not depend on the overbright bits. The per-pixel pass adds
its colour before the output scale, as the baked light is drawn, so the two
lose a doubling together. Alice moved to retail's no overbright bits on
2026-09-25 ([overbright record](2026-09-25-alice-overbright.md)). On
still floor the lights then still brightened `hatter2` 1.35 times and
`jlair2` 1.21 times, as under one bit.

The generator leaves out the surface emitters (1,366), the black lights
(460) and the one light with a negative intensity, which retail gives only
specially flagged models. That leaves 8,357 usable lights. Where a map has
more than 128, lights are taken round-robin across its PVS clusters: each
round takes the strongest remaining light of every cluster, so no area loses
its main light to a brighter area's fifth. 26 maps reach 128; the other ten
keep every usable light, from 1 on `potears1`, whose lighting is almost all
sky emitters, to 125 on `utemple`. The 3,968 shipped lights include 69 spots
and 1,187 capped lights.

Numbers are written in fixed point, never with an exponent, one light per
line. Three runs produced 36 byte-identical files.

## Package and filesystem

The package reader served only `maps/*.azb` and `maps/*.fog` out of
`games/<profile>/<gamedir>/`. It now serves `maps/*.lights.json` as well
(`FS_RootArchiveAllowsMapSidecar`, `code/qcommon/files.c`). Package entries
outrank ordinary game data, as they already did for zones and fog. Alice is
the only profile that ships a light file, so no other profile's lookup
changes. `alice` joins the package validator's profile keys, and the
required inventory grows from 447 to 519 assets.

## Runtime

A developer client, `meson/build/win32-alice-content` configured with
`-Dalice-development=true`, was built from the base commit plus the shared
tree's uncommitted work and this change, with its own `fntech3-pkg.fnz`
carrying the 72 files. Every run was windowed at 800 by 600 on an
unswitched private desktop, with no input injected, an isolated home path,
and sound initialised. The executable, renderer and package hashes and the
command are in the JSON record. No file under the installation changed.

- **Both files load from the package in all three renderers.** On `hatter2`,
  GL, Vulkan and RTX each printed `Loaded 284 audio zones from
  maps/hatter2.azb` and `static lights file:maps/hatter2.lights.json
  version:1 loaded:100 point:98 spot:2 spatial:100/0 skipped unsupported:0
  invalid:0 overflow:0`.
- **Every map's files load.** A single RTX client walked the maps with
  `devmap`, reloading the lights on each. For 35 maps both files loaded,
  every light count equals what the generator wrote and every zone count
  equals the audit's, with nothing skipped. On `potears1` the client never
  became active: the server resent its gamestate until the next `devmap`,
  with a 20-second wait too, and on a build predating this change whose
  package had no Alice files. The F.A.K.K.2 session found the cause:
  fnTech3 wrote Alice's fragment offsets as shorts where retail writes longs,
  dropping the later fragments of any message over 32 KB
  ([wire record](2026-09-22-alice-wire.md)), and `potears1`'s gamestate is
  38,135 bytes. Once that was fixed, a GL client reached `CL_FirstSnapshot:
  maps/potears1.bsp` with no gamestate resent, and loaded `potears1`'s 512
  zones and its one light with nothing skipped.
- **The lights change the frame where one is near.** These runs used the
  first files, before the balance. On `hatter2` an orange light of intensity
  250 stands 84 units from the start. With `r_staticLights 1` it lights the
  floor and Alice: the mean colour of the frame is 86.5, 69.7, 42.4, then
  76.3, 63.0, 39.3 with `r_staticLights 0`, and 90.8, 71.9, 42.9 with it back
  on. The animated disc on the left differs between every capture. On
  `garden1` the nearest light is 810 units from the start and scaled to 0.17
  by its cap, and the difference is within the frame-to-frame noise.
- **The balance keeps them a supplement.** With the balanced files, GL,
  captures just after the opening scene: `jlair2`'s mean colour is 42.5,
  34.4, 25.8 with the lights on, against 39.6, 32.1, 24.2 off and 67.6, 55.7,
  38.7 with the first files, and its near-white pixels are 5,069 against
  3,487 off and 15,185 before; nothing washes out. `hatter2`'s is 80.5, 64.2,
  37.7 on and 78.2, 65.4, 41.4 off. The RTX sweep above parsed the first
  files; the balanced ones differ only in their colours.
- The zones load and `s_info` lists them. In this run the environment still
  read `generic`, because Alice's backend did not yet consult a zone; it now
  does (see above and the [audio record](2026-09-23-alice-audio.md)).

## What this does not claim

No listening test has been made.
No light has been set beside retail's: retail lights models from this list
and bakes the world into lightmaps, so a static light adds to a world that
already holds its light, as it does for any profile; the balance bounds that
addition but is a choice made here, computed for the renderers' defaults. The cap is approximated,
the surface emitters are left out, and the shadows the promoted lights cast
were not inspected. As for every generated sidecar, these are engine-owned
estimates, not authored intent.
