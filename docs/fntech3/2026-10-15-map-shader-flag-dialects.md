# Map shader flags of Ritual's and 2015's maps, 2026-10-15

The [Raven map shader flags slice](2026-10-15-raven-bsp-shader-flags.md)
listed the other titles' map flags as not compared. This slice compares them.
Every map's shader records hold the surface and content flags the compiler
wrote. The three renderers read them in two places: every `ParseMesh` skips a
nodraw patch, and RTX takes the compiler's sky, translucency and light
filters from them (`ShaderForShaderNum`). Both read Quake III's values. Alice,
F.A.K.K.2 and Medal of Honor wrote other values for the light filters, so RTX
took their wood, rock and metal for filters. The
[JSON companion](2026-10-15-map-shader-flag-dialects.json) records the
binaries, addresses, counts, commands and outcomes. No compatibility gate
closes.

Implementation is independently authored. The retail executables were read
with capstone and pefile, and the installed maps and shader scripts with short
readers. Ritual's F.A.K.K.2 SDK and OpenMoHAA's `surfaceflags.h` were read
for the two encrypted executables (reference observation; no text
incorporated).

## What each engine writes

### Observed: the infoparm tables

Each renderer that parses `surfaceparm` has one infoparm table that turns the
word into flags, the values its map compiler writes. The five values the
renderers read from a map:

| Engine | Table | `sky` | `nodraw` | `trans` | `alphashadow` | `lightfilter` |
| --- | --- | --- | --- | --- | --- | --- |
| `quake3.exe` 1.32 | `0x4d0168` | `0x4` | `0x80` | `0x20000000` | `0x10000` | `0x8000` |
| `quakelive_steam.exe` | `0x55c650` | `0x4` | `0x80` | `0x20000000` | `0x10000` | `0x8000` |
| `WolfSP.exe` 1.32 | `0x4f2878` | `0x4` | `0x80` | `0x20000000` | `0x10000` | `0x8000` |
| `WolfMP.exe` 1.4 | `0x4f9600` | `0x4` | `0x80` | `0x20000000` | `0x10000` | `0x8000` |
| `et.exe` 2.60b | `0x596450` | `0x4` | `0x80` | `0x20000000` | `0x10000` | `0x8000` |
| `stvoy.exe` 1.20 | `0x49adf0` | `0x4` | `0x80` | `0x20000000` | `0x10000` | `0x8000` |
| `stvoyHM.exe` 1.20 | `0x4d4a70` | `0x4` | `0x80` | `0x20000000` | `0x10000` | `0x8000` |
| `alice.exe` | `0x5294d0` | `0x4` | `0x80` | `0x20000000` | `0x800` | none |

- **Alice's other words.** `0x10000` is `wood`, and `metal`, `rock`,
  `dirt`, `grill`, `organic` and `squishy` follow it to `0x400000`.
  `castshadow` is `0x40`, `nolightmap` `0x400`, `nosteps` `0x2000`,
  `nonsolid` `0x4000`, `nodlight` `0x800000`, `hint` `0x1000000` and
  `slide` `0x2000000`. `0x8000` is unused.
- **No table.** The IHV Test's and the Q3Test releases' executables hold no
  `surfaceparm` words; their renderers never parse one.
- **Raven's Jedi engines.** Their table moves every bit, as the
  [Raven slice](2026-10-15-raven-bsp-shader-flags.md) records.

### Not measured in the engine: F.A.K.K.2 and Medal of Honor

- **F.A.K.K.2.** The disc's `fakk2.exe` is encrypted. Ritual's F.A.K.K.2 SDK
  `surfaceflags.h` has Alice's values for all of the words above
  (reference observation), so `alice.exe`'s table is taken for it
  **[I]**.
- **Medal of Honor.** `MOHAA.exe` is encrypted. OpenMoHAA's `surfaceflags.h`
  has `alphashadow` at `0x200`, `nolightmap` `0x100`, `nosteps` `0x400`,
  `nonsolid` `0x800`, `nodlight` `0x20000000` and `hint` `0x40000000`.
  `0x8000` is its `metal` and `0x10000` its `rock`, among fifteen material
  bits from `0x2000` to `0x8000000` (reference observation).

### Observed: the installed maps

| | Alice | F.A.K.K.2 | Medal of Honor |
| --- | --- | --- | --- |
| Maps | 36 | 30 | 54 |
| Shader records | 3,969 | 7,082 | 13,587 |
| With `0x10000`, read as alphashadow | 0 | 766 | 8,413 |
| With `0x8000`, read as lightfilter | 0 | 0 | 3,272 |
| With the title's own alphashadow | 37 (`0x800`) | 5 (`0x800`) | 580 (`0x200`) |

- **Medal of Honor's alphashadow.** 374 of the 580 records with `0x200` are
  shaders whose script says `surfaceparm alphashadow`, such as
  `textures/algiers/fort_canopy` (`0x10002200`); the other 206 name
  shaders whose installed script no longer says it. Its `0x10000` records
  are stone surfaces such as `textures/algiers/afrik_wall1a`, and most of
  its `0x8000` records are under `general_industrial`, `common`, `das_boot`
  and `mohcommon`.
- **Alice's.** All 37 records with `0x800` are shaders whose script says
  `alphashadow`.
- **F.A.K.K.2's.** Its `0x10000` records are mostly wooden surfaces, such
  as `textures/creeperpens/edenbarn`. Its 5 `0x800` records are
  `textures/eden/fl_chainfloor`, a chain floor; no installed F.A.K.K.2
  script says `alphashadow` at all.
- **The other read bits.** `sky` (`0x4`), `nodraw` (`0x80`) and `trans`
  (`0x20000000`) are in Quake III's place in all three titles.
  - Alice's and F.A.K.K.2's records with `0x4` are all named as skies; 54 of
    Medal of Honor's 70 are, and the other 16 are caulk and similar shaders
    on sky faces.
  - F.A.K.K.2's and Medal of Honor's compilers write a record per shader and
    face flags, so a record with `0x80` is often an ordinary texture's
    hidden face, which makes no drawn surface (inference from the compiler
    lineage).

## What fnTech3 does now

- **One translation.** Right after `R_LoadShaders`, each renderer's world
  load rewrites a Ritual (`FAKK`) or Medal of Honor map's surface flags into
  Quake III's bits ([`tr_map_shader_flags.h`](../../code/renderercommon/tr_map_shader_flags.h)).
  - Words in Quake III's place are kept: `nodamage`, `slick`, `sky`,
    `ladder`, `noimpact`, `nomarks` and `nodraw`.
  - Each title's `alphashadow`, `nolightmap`, `nosteps`, `nonsolid`,
    `nodlight` and `hint` move to Quake III's bits.
  - Materials, `castshadow`, `slide` and Medal of Honor's backside and patch
    bits are left out.
  - The content words are untouched: `trans` is Quake III's in all three.
- **The collision model.** The collision model and the game modules still
  get the map's own flags. Only the renderers' copy is translated.
- **The other formats.** Every other format's flags are returned unchanged.

### FnQ3/FnQL features (invariant 11)

- **The path tracer.** RTX's light filters now follow each title's own
  `alphashadow` surfaces, which is Fn's feature reading the profile's data.
  No title here has a `lightfilter` word, so nothing filters by colour.
- **GLx, Vulkan.** Nothing they draw changes: their only map-flag read,
  nodraw, is in Quake III's place in these titles.

## Evidence

### Tests

- New `tests/map_shader_flag_dialects_tests.py` runs the production
  translation and RTX's own `ShaderForShaderNum` over records of each
  dialect.
  - Ritual wood and Medal of Honor rock and metal no longer filter light,
    and each title's alphashadow filters by alpha.
  - Sky, nodraw and translucency are kept, and the other words land in
    Quake III's bits.
  - Every other format keeps all 32 bits.
  - It also checks that all three renderers call the translation right
    after `R_LoadShaders`.
- The export build compiled all three renderers with `/W4 /WX`.

### Live

- **Runtimes.** Both runtimes were built from the export of `f8e1ee98` with
  the Raven slice's reader. The after runtime adds this change; only the
  three renderer libraries differ.
- **Probe.** Each run was an RTX client, windowed on a private desktop, with
  its own home and no input.
  - Medal of Honor ran with `net_enabled 0`. Its steps were
    `finishloadingscreen`, `join_team allies`, `primarydmweapon rifle`,
    then `tele` and `face` to the first deathmatch spawn, with
    `thereisnomonkey 1`.
  - F.A.K.K.2 used `scripts/probe_ritual_client.py`.
  - Captures are the engine's `screenshotPNG`, and the installations were
    unchanged after every run.

| RTX | Before | After |
| --- | --- | --- |
| `DM/mohdm6` traced triangles | opaque 13,233, masked 13,936 | opaque 19,697, masked 13,554 |
| `DM/mohdm6` filtering the path tracer's light | 6,474 | 386 |
| `DM/mohdm6` capture | mean `(88.2, 74.4, 57.2)` | mean `(89.4, 75.4, 58.0)` |
| F.A.K.K.2 `swamp3` traced triangles | opaque 47,523 | opaque 126,005 |
| `swamp3` filtering the path tracer's light | 78,482 | 0 |
| `swamp3` capture | mean `(34.9, 49.5, 46.2)` | the same |

- **`DM/mohdm6`.** The 386 triangles left filtering are its `alphashadow`
  canopies and fences. Before, its stone and metal let light through.
  - At the spawn view 4.1% of the pixels change by more than 8 levels;
    two captures of one runtime differ in 2.7%.
- **`swamp3`.** Its wood had filtered light; no installed F.A.K.K.2 shader
  is an alpha shadow. The view captured, by the water, changes in 0.3% of
  its pixels.
- **Alice.** Alice's maps carry no `0x10000` record and its 37 alphashadow
  records were not read before; no Alice map was run.

## Remaining and unrelated findings

- **Medal of Honor's remaining words** (`nolightmap`, `nosteps`, `nonsolid`,
  `nodlight`, `hint`) are taken from OpenMoHAA alone; no renderer reads
  them from a map today.
- **Medal of Honor's sun under RTX.** RTX takes the sun from the first sky
  shader's `q3map_sun` and reports "the first sky surface's shader
  `textures/sky/mohday1` has no q3map_sun" on `DM/mohdm6`. 2015's maps give
  their sun in worldspawn instead: 46 of the 54 have `sundirection` and 43
  `suncolor` (`DM/mohdm6`: `-90 90 0`, `85 85 85`), which the path tracer
  does not read.
