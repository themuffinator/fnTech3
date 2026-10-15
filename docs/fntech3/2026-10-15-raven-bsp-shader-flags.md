# Raven's map shader flags, 2026-10-15

The [sun keyword slice](2026-10-15-raven-sun-keyword.md) found RTX's path
tracer reading `mp/duel9`'s snow as the compiler's sky. RTX marks a shader
as the sky its map compiler saw when the shader's record in the map has
Quake III's sky bit, `0x4`. Jedi Academy's and Jedi Outcast's maps keep the
same record, but Raven moved every bit in its two flag words, and `0x4` is
part of a material number there. This slice reads both words with Raven's
own bits under the four Jedi profiles: `jka-sp`, `jka-mp`, `jk2-sp` and
`jk2-mp`. The [JSON companion](2026-10-15-raven-bsp-shader-flags.json)
records the binaries, identities, addresses, counts, commands and outcomes.
No compatibility gate closes.

Implementation is independently authored. The retail executables were read
with capstone and pefile, and the installed maps with a short reader of
their shader and surface records. OpenJK's `surfaceflags.h` at the pinned
commit was compared with the retail tables (reference observation; no text
incorporated).

## Where the renderers read a map's flags

Every map's shader lump holds one 72-byte record per shader: a 64-byte name,
then the surface flags and the content flags the compiler wrote. The three
renderers load it into `s_worldData.shaders` (`R_LoadShaders`). They read
those flags in five places, all with Quake III's bits:

| Reader | Renderers | Bit read | Effect |
| --- | --- | --- | --- |
| `ParseMesh` | GLx, Vulkan, RTX | nodraw, `0x80` | a nodraw patch is skipped |
| `ShaderForShaderNum` | RTX | sky, `0x4` | `rtCompiledSky`: the compiler's sky |
| `ShaderForShaderNum` | RTX | translucent, `0x20000000` | `rtCompiledTranslucent`: casts no compiler shadow |
| `ShaderForShaderNum` | RTX | alphashadow, `0x10000` | the surface filters the path tracer's light by its alpha |
| `ShaderForShaderNum` | RTX | lightfilter, `0x8000` | the surface filters the path tracer's light by its colour |

- **Nothing else reads them.** The shader flags the renderers test
  elsewhere (`shader->surfaceFlags`, `shader->contentFlags`) are the
  parser's own, from the shader's `surfaceparm` lines.
- **The collision model.** The collision model hands the map's flags to the
  game and client-game modules as they are; that is their contract, and it
  is not changed here.
- **Elite Force.** Elite Force's maps are Quake III's own format, and its
  flags were not part of this slice (see the findings below).

## What the retail engines read

### Observed: Raven's bits

Each Jedi renderer has one `infoparm` table that turns a `surfaceparm` word
into flags. The four tables agree with each other and with OpenJK's
`surfaceflags.h`:

| Engine | Table | Read at |
| --- | --- | --- |
| `jamp.exe` 1.01 | `0x56cfc8` | `0x4b4c42` |
| `jasp.exe` 1.01 | `0x4f3440` | `0x47ad82` |
| `jk2mp.exe` 1.02a | `0x502200` | `0x48f5a6` |
| `jk2sp.exe` 1.02a | `0x4cf5a8` | `0x47f1e6` |

| Word | Raven | Quake III |
| --- | --- | --- |
| `sky` | surface `0x2000` | `0x4` |
| `slick` | surface `0x4000` | `0x2` |
| `metalsteps` | surface `0x8000` | `0x1000` |
| `nodamage` | surface `0x40000` | `0x1` |
| `noimpact` | surface `0x80000` | `0x10` |
| `nomarks` | surface `0x100000` | `0x20` |
| `nodraw` | surface `0x200000` | `0x80` |
| `nosteps` | surface `0x400000` | `0x2000` |
| `nodlight` | surface `0x800000` | `0x20000` |
| `lava` | content `0x2` | `0x8` |
| `water` | content `0x4` | `0x20` |
| `fog` | content `0x8` | `0x40` |
| `playerclip` | content `0x10` | `0x10000` |
| `monsterclip` | content `0x20` | `0x20000` |
| `botclip` | content `0x40` | `0x400000` |
| `trigger` | content `0x400` | `0x40000000` |
| `nodrop` | content `0x800` | `0x80000000` |
| `slime` | content `0x20000` | `0x10` |
| `detail` | content `0x8000000` | `0x8000000` |
| `trans` | content `0x80000000` | `0x20000000` |

- **Raven's own words.** `forcefield` (`0x10000`), `nomiscents`
  (`0x1000000`), Academy's `forcesight` (`0x2000000`), and the contents
  `shotclip`, `terrain`, `ladder`, `abseil`, `outside` and Academy's
  `inside` have no Quake III meaning.
- **Opaque.** Every solid brush carries content `0x8000`, Raven's opaque
  default, which `nonopaque` clears. In Quake III `0x8000` is an area
  portal.
- **Materials.** The low five surface bits are a material number, 0 to 31
  (`MATERIAL_MASK` `0x1f` in OpenJK); none of the tables sets them.
- **No filter words.** None of the four tables has `alphashadow`,
  `lightfilter`, `pointlight` or `nolightmap`. Quake III's `alphashadow` and
  `lightfilter` bits are Raven's `forcefield` and `metalsteps`.

### Observed: the one map-flag read

Each retail renderer's `ParseMesh` skips a patch whose shader record has
surface flag `0x200000`, Raven's nodraw:

| Engine | Test |
| --- | --- |
| `jamp.exe` 1.01 | `0x48d610` |
| `jasp.exe` 1.01 | `0x457644` |
| `jk2mp.exe` 1.02a | `0x471253` |
| `jk2sp.exe` 1.02a | `0x45e630` |

Each is `test dword ptr [shaders + num*72 + 0x40], 0x200000`, followed by
the store of the skipped surface. A scan of each executable's code for
tests at a record's flag offsets finds no test of the sky (`0x2000`) or
translucent (`0x80000000`) bit (inference: retail has no reader of the
compiler's sky). The compiler's sky and its shadow rules are fnTech3's own
path tracer's reading of the map.

## The installed maps

Every installed map was read: 57 in Jedi Academy and 41 in Jedi Outcast,
all RBSP 1.

| | Academy | Outcast |
| --- | --- | --- |
| Shader records | 4,819 | 3,828 |
| Records with Raven's sky `0x2000` | 54 | 30 |
| Other records with Quake III's sky `0x4` | 213 | 14 |
| Surfaces read as sky by `0x4` | 15,371 in 42 maps | 635 in 9 maps |
| Real sky surfaces | 1,129 | 720 |
| Nodraw patches drawn | 619 in 13 maps | 7 in 1 map |
| Surfaces read as light filters | 3,449 | 10,690 |
| Translucent surfaces read as opaque | 7,922 | 12,256 |

- **The sky set.** All 38 shaders with `0x2000` are skies by name
  (`textures/skies/*`, `textures/mp/s_*sky`, ...).
- **Materials read as flags.** Rock (23), snow (14), ice (15), glass (10)
  and the other materials with bit `0x4` read as sky; those with `0x80`
  would read as nodraw, but no material reaches it.
- **The sun withheld.** id's q3map takes the sun from the first sky
  surface, and RTX does the same for a map it lit. Where a misread surface
  comes before the real sky and the sky has a sun, that sun is lost:
  - in Academy, `mp/ctf2`, `mp/ctf3`, `mp/ffa2`, `mp/ffa3`,
    `mp/siege_hoth`, `hoth2`, `t1_rail`, `t3_hevil` and `yavin2`;
  - in Outcast, no map: none of its 9 maps with a misread comes before a
    sky with a sun;
  - on `mp/duel9`, `s_hothsky` comes first (surface 333, its snow at 344),
    so its sun survives. Before the sun keyword slice the sky shader was
    defaulted, which is why the snow was the first sky the path tracer saw.
- **Nodraw patches.** Most are caulk, clip and trigger patches.

## What fnTech3 does now

- **One translation.** The RBSP reader that projects a Raven map into the
  renderers' Quake III layout ([`tr_raven_bsp_format.h`](../../code/renderercommon/tr_raven_bsp_format.h))
  now also rewrites each shader record's two flag words into Quake III's
  bits (`FN_RavenBspFlagsToFn`), with the table above.
  - Raven's own meanings, materials and the opaque default are left out of
    the projected record.
  - The world keeps the original map beside the projection, every bit
    included.
- **Every reader follows.** With the projected records in Quake III's bits,
  all five readers above read Raven's maps correctly without a change of
  their own:
  - the three renderers' `ParseMesh` skips a nodraw patch, as all four
    retail engines do;
  - RTX's compiler sky is the real sky, so its sun is found;
  - RTX takes Raven's `trans` brushes as casting no compiler shadow;
  - RTX filters light through no Raven surface, as no Raven word sets
    those bits.
- **The other profiles.** Only a Raven map reaches the projection, and only
  the four Jedi profiles load Raven maps
  ([`game_profile.cpp`](../../code/qcommon/game_profile.cpp)). Every other
  profile's records reach the same readers untouched.

### FnQ3/FnQL features (invariant 11)

- **The path tracer.** The path tracer lights a Raven map with its real
  sky, its sun, and its compiler's shadow rules, which are Fn's features
  reading the profile's own data.
- **GLx, Vulkan.** The nodraw skip is a format contract the retail
  renderers share; no presentation changes otherwise.
- **Left out, with a reason.** Raven's `forcefield`, `forcesight` and
  materials stay unread by the renderers. No Fn feature uses them yet; the
  original records keep them for one that will.

## Evidence

### Tests

- **The projection.** `tests/raven_bsp_render_tests.cpp` (Meson's
  `fntech3_raven_bsp_render`) builds the production RBSP reader.
  - Each Raven bit becomes the Quake III bit named in `surfaceflags.h`, and
    every other bit, each of the 32 material numbers and Raven's opaque
    default become nothing.
  - The flag words of six retail `mp/duel9` records -- rock, snow, the sky,
    caulk, the pit fog and clip -- reach the projected lump in Quake III's
    bits, while the world's original keeps Raven's.
  - It passes with MSVC x86 (the export build's strict flags) and Clang
    x64.
- **The readers.** New `tests/raven_bsp_shader_flags_tests.py` runs a
  Raven map with those records through the production projection and each
  renderer's own `R_LoadShaders`.
  - In all three renderers only caulk is nodraw, the test `ParseMesh`
    makes, and only the sky is sky.
  - For RTX it runs `ShaderForShaderNum`, `RT_ShaderIsSky` and `RT_FindSun`
    too: only the sky is the compiler's sky, only the fog is translucent,
    nothing filters, and the sun is the sky's, with nothing printed.
  - A Quake III shader lump through the same loaders keeps its own bits,
    `0x4` sky included.
  - Against the reader before this change, all three renderers fail it.
- **The installed maps.** All 98 maps through the MSVC test's `--stdin`
  mode: 53 project, and 45 single-player maps are refused by checks made
  before the shader lump is read (see the findings below).

### Live

- **Runtimes.** Two runtimes were built from a `git archive` export of
  `f8e1ee98`. They differ only in the three renderer libraries, and the
  after runtime adds this change's reader.
- **Probe.** Each run was an RTX client, windowed on an unswitched private
  desktop, with `net_enabled 0`, its own home, a spectator, `togglemenu`
  to close the join menu, and no input. Captures are the engine's
  `screenshotPNG`.
  - The installation and the binaries were unchanged after every run.

| `jka-mp`, RTX | Before | After |
| --- | --- | --- |
| `mp/duel9` sun | `q3map sun 86.6 86.6 102.9 toward -0.133 0.112 0.985` | the same |
| `mp/duel9` traced triangles | opaque 1,695, special 6,669 | opaque 8,344, special 20 |
| `mp/duel9` capture mean RGB | `(114.7, 120.2, 127.2)` | `(130.4, 135.1, 139.4)` |
| `mp/ffa3` sun | "the first sky surface's shader `textures/desert/outerhull` has no q3map_sun" | `q3map sun 98.0 112.1 91.9 toward 0.310 0.145 0.940` |
| `mp/ffa3` world triangles | 56,302 | 56,234 |
| `mp/ffa3` traced triangles | opaque 55,400, special 902 | opaque 56,204, special 30 |
| `mp/ffa3` capture mean RGB | `(104.1, 107.4, 93.0)` | `(170.3, 180.2, 153.6)` |

- **`mp/duel9`.** Its sun was already found, as its sky comes first.
  Before, 6,669 of its 8,364 world triangles, its snow and rock among
  them, were left to the rasterizer as sky. The path tracer did not trace
  them, and its rays saw sky through them.
  - After, all but the sky's 20 are traced. The snow in front of the
    camera is lit by the sun where it was drawn flat and blue.
  - 24.7% of the pixels change by more than 8 levels; two captures of one
    runtime differ in 1.8%.
- **`mp/ffa3`.** Its sun is `h_tatsky`'s line,
  `sun 0.874291 1.000000 0.819403 175 25 70`: the unit colour times 175,
  yaw 25, elevation 70.
  - Before, the path tracer took the hull's surface, a material, as the
    first sky and found no sun, and the map was lit by its lamps and sky
    alone.
  - After, the sun lights the courtyard and its disc shows in the sky;
    77.7% of the pixels change (2.9% between two captures of one runtime).
  - Its two clip patches, Raven nodraw, are no longer drawn: 68 triangles.
- **Not run live.** No Outcast map: its multiplayer client stops at
  client-game import 300 before a map is drawn, and no Outcast map loses
  its sun. GLx and Vulkan read the same projected records; their only
  change is the nodraw skip, which the reader test above covers.

## Remaining and unrelated findings

- **Elite Force's flags.** Elite Force's maps are Quake III's format with
  Raven's own flag meanings (see `game_profile.cpp`'s note). The Elite
  Force SDK's `surfaceflags.h` (the `Elite-Reinforce` reference tree) keeps
  Quake III's values for all five bits the renderers read, so nothing
  points to a misread; `stvoy.exe` itself was not read for this.
- **Ritual and 2015 maps.** Alice, F.A.K.K.2 and Medal of Honor maps reach
  the same readers through their own loaders; their flag meanings
  were not compared in this slice.
- **Refused single-player maps.** The renderers' RBSP reader refuses 45
  installed single-player maps before it reads their shader lump: 43 for
  surface light styles and one for vertex light styles that are not served
  yet, and Outcast's one for a surface index range it finds invalid. Every
  multiplayer map is read.
