# Alice's FAKK 42 maps and their collision translation

## Scope

This record establishes Alice's map format and builds the translator that
presents it to fnTech3's existing collision loader. It covers the thirty-six
maps a legitimate retail installation ships, and nothing else: no map is drawn,
no lighting is read, and no module runs. See the
[foundation record](2026-09-22-alice-contracts.md) for the profile itself and
the [compatibility ledger](COMPATIBILITY.md#alice-gate-matrix) for what remains
open.

*Update, 2026-09-24:* that scope describes the record as first written. All
three renderers now draw these maps through the same translator, asked to
carry the light grid as well (`FAKK_TRANSLATE_LIGHTGRID` in
`code/qcommon/bsp_fakk.h`), and the retail modules run on them; the ledger holds
that evidence. This record remains the collision evidence.

## The format

Alice's maps carry ident `FAKK` at version 42. The header is not Quake III's:
after the ident and version comes a 32-bit checksum, and only then a directory
of **twenty** lumps, so the first lump begins at 172 rather than 144.

The lump order is Heavy Metal: F.A.K.K.2's, not Quake III's. Ritual published
that order and the record layouts in their own v1.02 SDK
(`qcommon/qfiles.h`), and every one of them was re-measured here against the
retail maps before any of it was written into fnTech3. The SDK was read as
reference observation only; no implementation text is incorporated from it.

| FAKK lump | Contents | Record | Quake III lump |
| --- | --- | --- | --- |
| 0 | shaders | 76 bytes | 1 |
| 1 | planes | 16 bytes | 2 |
| 2 | lightmaps | 128×128×3 | 14 |
| 3 | surfaces | 108 bytes | 13 |
| 4 | draw vertices | 44 bytes | 10 |
| 5 | draw indexes | 4 bytes | 11 |
| 6 | leaf brushes | 4 bytes | 6 |
| 7 | leaf surfaces | 4 bytes | 5 |
| 8 | leafs | 48 bytes | 4 |
| 9 | nodes | 36 bytes | 3 |
| 10 | brush sides | 8 bytes | 9 |
| 11 | brushes | 12 bytes | 8 |
| 12 | fogs | 72 bytes | 12 |
| 13 | models | 40 bytes | 7 |
| 14 | entity text | bytes | 0 |
| 15 | visibility | header and bit sets | 16 |
| 16 | light grid | 8 bytes, 192x192x320 cell | dropped, see below |
| 17 | entity lights | not established | dropped |
| 18 | entity light visibility | not established | dropped |
| 19 | light definitions | 52 bytes | dropped |

Eleven of the thirteen fixed-size records are Quake III's own, byte for byte:
planes, nodes, leafs, leaf brushes, leaf surfaces, brushes, brush sides, fogs,
models, draw vertices and draw indexes. The entity text and the visibility lump
are Quake III's too. Only two records differ, and each differs by a single
trailing field:

- a **shader** carries a `subdivisions` count after the name and the two flag
  words, so it is 76 bytes rather than 72;
- a **surface** carries a `subdivisions` float after the patch dimensions, so
  it is 108 bytes rather than 104.

## How the layout was verified

The layout was not assumed from the SDK. Every one of the thirty-six retail
maps was checked against it, and all thirty-six pass:

- **The directory tiles the file.** Every lump lies inside the file, no two
  overlap, none reaches back into the 172-byte header, and the highest lump end
  is the file's own length. That is what fixes the lump count at twenty.
- **Every lump divides by its record size**, for all thirteen fixed-size lumps,
  on every map.
- **Every cross-reference lands in range.** Node plane numbers index the plane
  lump and node children index either a node or a leaf; leaf surface and brush
  spans lie inside their lumps; every leaf-brush and leaf-surface entry names a
  real brush or surface; brush and brush-side plane and shader numbers are in
  range; surface vertex and index spans lie inside theirs, with a fog number of
  -1 or a real fog; and model spans lie inside the surface and brush lumps.
- **The geometry is numerically sane.** Every sampled plane normal is unit
  length to 1e-3 and every node's bounds are ordered.
- **The shader names are clean paths**: NUL-terminated, printable, with no
  bytes after the terminator.
- **The entity lump is the entity text**: it begins with `{` and contains
  `"classname"` on every map.
- **The visibility lump is Quake III's.** Its first two words are the cluster
  count and the bytes per cluster, and `8 + clusters × bytes` equals the lump
  length exactly on every map, with the highest cluster any leaf names being
  one less than the count.
- **The light definitions are per surface.** The lump divides by 52 on every
  map, and the quotient equals that map's surface count exactly — 3,533 for
  `centipede1`, 3,802 for `garden1`, 4,660 for `keep`, and so on for all
  thirty-six. That is an independent confirmation of both the surface record
  size and the lump order.
- **Every patch is consistent.** All 6,666 patch surfaces have
  `patchWidth × patchHeight == numVerts`, with both dimensions odd and at least
  three, which is what a Quake III patch mesh requires.

The surface `subdivisions` float is read and discarded. The shader
`subdivisions` count is not a mere compiler hint: retail patch collision is
cut by it (see [Patch collision](#patch-collision) below). Across the
thirty-six maps the count on a colliding patch's shader runs from 3 to 758,
with a median of 18.

### What the light grid gave up, and what it did not

The light grid is half established, which is why the translator drops it. Its
**record** is Quake III's own 8-byte point -- `byte ambient[3]`,
`byte direct[3]`, `byte latLong[2]` -- on three counts: the lump length divides
by 8 on all thirty-six maps; unlit regions repeat the byte pattern
`00 00 00 00 00 00 80 00` at a stride of exactly 8, which is a zero colour pair
with Quake III's straight-up default direction; and in lit regions the byte
positions separate the way that record predicts, with bytes 0-2 spanning the
low end, bytes 3-5 saturating at 255, and bytes 6-7 clustering as a packed
latitude and longitude rather than as colour.

Its **dimensions** are established, and the way they were found is the point.
Searching the data for them did not work: with Quake III's own 64x64x128 cell
the derivation overcounts by roughly twenty times, no `gridsize` key appears in
any of the thirty-six worldspawns, and a search over every cell size from 16 to
512 units per axis found none that reproduced even one axis's divisor. The one
thing that search did establish was a nearly constant ratio of world volume to
point count, 10.6 to 11.9 million cubic units per point, which said a fixed
cell had to exist.

Reading the engine that consumes the lump settled it in one step. `alice.exe`
carries its own renderer, and the light grid loader is at `0x0049acb0`. Before
it touches the lump it writes six constants into adjacent globals:

```
mov dword [0x9d70e8], 0x43400000    ; 192.0
mov dword [0x9d70ec], 0x43400000    ; 192.0
mov dword [0x9d70f0], 0x43a00000    ; 320.0
mov dword [0x9d70f4], 0x3baaaaab    ; 1/192
mov dword [0x9d70f8], 0x3baaaaab    ; 1/192
mov dword [0x9d70fc], 0x3b4ccccd    ; 1/320
```

That is Quake III's `lightGridSize` and `lightGridInverseSize` pair, written as
immediates because Alice reads no `gridsize` key, and Alice's cell is
**192 x 192 x 320** rather than Quake III's 64 x 64 x 128. The volume of that
cell is 11.80 million cubic units, which falls inside the measured 10.6 to 11.9
million the ratio above had already bracketed.

With the cell known, Quake III's own derivation reproduces the lump exactly.
Against the world model's bounds, with both ends snapped inward to the grid --
the low end up, the high end down, so a world that overhangs its grid gains no
point at either end:

```
mins[i]   = size[i] * ceil ( bounds[0][i] / size[i] )
maxs[i]   = size[i] * floor( bounds[1][i] / size[i] )
counts[i] = ( maxs[i] - mins[i] ) / size[i] + 1
```

the product of the three counts equals the lump length divided by eight on
**all thirty-six maps**, with no exceptions and no rounding slack: `gvillage`
at 16x22x9 for 3,168 points, `grounds2` at 75x71x16 for 85,200, `tower3` at
15x15x10 for 2,250. A single wrong axis would have missed on every map, so
thirty-six exact hits is the confirmation.

The translator still drops the lump, because carrying it into IBSP-46 needs the
renderer to use 192x192x320 rather than Quake III's default. Quake III takes
that pair from the worldspawn's `gridsize` key, so the translation can carry
the grid by copying lump 16 into `LUMP_LIGHTGRID` and writing
`gridsize "192 192 320"` into the entity text it emits. That is the next
increment here; nothing about the grid is unknown any more.

The entity light and entity-light-visibility records are not established
either. Their lumps divide by nothing obvious and nothing cross-references
them.

## The translator

`code/qcommon/bsp_fakk.h` presents a FAKK 42 map to the collision loader as the
IBSP 46 layout it already reads, in the same shape as the IHV and RBSP
translators beside it. It validates the whole directory before reading any
record, counts the two lumps whose record size differs and refuses a remainder,
renumbers each lump into Quake III's index, repacks shaders and surfaces, and
copies everything else byte for byte so the stored little-endian values survive
on any host. The collision translation also carries each shader's
subdivision count, in shader order, in a table past the last lump where no
IBSP 46 reader looks; the renderers' translation does not ask for it. The
map's own checksum is taken from the installed file before translation,
because that is the identity peers advertise.

`tests/alice_bsp_tests.cpp` covers it with independently authored fixtures; no
retail map is used. It asserts the record sizes, that every lump lands at
Quake III's index with its bytes intact, that the four dropped lumps' contents
appear nowhere in the output, that shaders and surfaces keep every
collision-bearing field, and that a malformed map is refused rather than
reinterpreted: a wrong ident, F.A.K.K.2's own version 12 and four other
versions, a truncated header, six shapes of out-of-range lump, a lump length
that is not a whole number of records, a missing allocator, and a refused
allocation. An empty but well-formed map still translates. It also asserts
the subdivision table's values, order and placement, and that the renderers'
light-grid translation carries no table.

## The installed maps

Every one of the thirty-six loads its collision through a developer dedicated
server against the retail installation, one server start per map, into an
isolated writable home. All thirty-six reach the module boundary and stop
there, by name, because fnTech3 has no Alice game-module adapter:
`VM_LoadDll(qagame) rejected invalid native module for the American McGee's
Alice profile`. No map reports a format, translation or collision error.

| Map | Bytes | Shaders | Surfaces | Vertices | Brushes | Leafs | Patches | Clusters |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| centipede1 | 4,562,996 | 71 | 3,533 | 18,122 | 1,149 | 5,610 | 44 | 2,717 |
| centipede2 | 3,317,608 | 48 | 3,128 | 15,035 | 950 | 2,659 | 12 | 1,366 |
| facade | 5,667,704 | 119 | 4,350 | 23,973 | 1,330 | 2,435 | 139 | 1,046 |
| fortress1 | 4,503,220 | 153 | 5,353 | 27,213 | 1,585 | 2,109 | 60 | 1,102 |
| fortress2 | 5,836,804 | 119 | 7,056 | 37,071 | 2,119 | 4,914 | 145 | 2,320 |
| funhouse | 7,145,896 | 137 | 7,809 | 40,451 | 2,281 | 5,637 | 308 | 2,777 |
| garden1 | 6,323,368 | 52 | 3,802 | 17,630 | 1,077 | 6,684 | 20 | 3,563 |
| garden2 | 8,172,832 | 60 | 6,349 | 30,854 | 1,914 | 8,413 | 37 | 3,919 |
| garden3 | 3,429,376 | 66 | 2,400 | 13,420 | 1,336 | 4,144 | 33 | 1,550 |
| garden4 | 5,763,900 | 90 | 3,469 | 18,702 | 1,617 | 5,716 | 61 | 2,590 |
| grounds1 | 2,521,040 | 89 | 1,600 | 9,308 | 421 | 1,448 | 163 | 712 |
| grounds2 | 4,891,724 | 39 | 2,986 | 15,635 | 1,375 | 3,269 | 98 | 1,511 |
| gvillage | 4,013,332 | 153 | 4,031 | 21,886 | 980 | 4,593 | 77 | 2,139 |
| hatter1 | 6,951,076 | 129 | 10,538 | 51,977 | 3,111 | 4,029 | 125 | 1,675 |
| hatter2 | 2,182,600 | 87 | 2,801 | 15,317 | 692 | 1,577 | 141 | 887 |
| hedge1 | 2,678,552 | 166 | 2,728 | 18,619 | 1,001 | 1,941 | 569 | 780 |
| hedge2 | 5,257,304 | 197 | 6,106 | 35,602 | 3,173 | 4,512 | 614 | 2,010 |
| hedge3 | 9,370,732 | 362 | 9,868 | 63,924 | 3,090 | 5,740 | 1,644 | 2,707 |
| jlair1 | 7,741,340 | 66 | 7,668 | 38,310 | 1,918 | 5,427 | 2 | 2,586 |
| jlair2 | 1,921,312 | 33 | 3,872 | 17,814 | 951 | 128 | 0 | 46 |
| keep | 4,501,380 | 156 | 4,660 | 27,016 | 1,547 | 2,678 | 380 | 1,177 |
| pandemonium | 5,675,452 | 134 | 6,253 | 31,196 | 1,500 | 4,843 | 44 | 2,234 |
| potears1 | 7,165,892 | 158 | 6,398 | 32,885 | 3,585 | 4,821 | 208 | 2,475 |
| potears2 | 3,777,348 | 86 | 5,092 | 23,771 | 1,170 | 1,660 | 32 | 797 |
| potears3 | 1,014,216 | 28 | 1,403 | 6,790 | 562 | 467 | 8 | 195 |
| qlair | 1,441,292 | 56 | 1,776 | 8,933 | 836 | 794 | 66 | 355 |
| rchess1 | 5,344,008 | 156 | 5,899 | 31,816 | 2,665 | 4,640 | 132 | 2,300 |
| skool1 | 6,402,760 | 119 | 6,720 | 36,259 | 2,513 | 6,636 | 42 | 2,931 |
| skool2 | 6,374,344 | 107 | 6,757 | 35,309 | 2,827 | 6,708 | 34 | 3,197 |
| tower1 | 2,183,428 | 42 | 2,342 | 12,821 | 720 | 1,138 | 150 | 594 |
| tower2 | 3,209,560 | 64 | 3,595 | 19,034 | 1,221 | 2,561 | 123 | 1,174 |
| tower3 | 3,404,896 | 53 | 4,397 | 20,810 | 878 | 1,315 | 32 | 527 |
| utemple | 8,102,944 | 93 | 6,667 | 68,230 | 1,618 | 6,291 | 159 | 2,707 |
| wchess1 | 7,012,340 | 150 | 7,745 | 42,781 | 3,563 | 5,665 | 123 | 2,719 |
| wchess2 | 5,664,880 | 169 | 6,986 | 42,517 | 3,029 | 3,526 | 795 | 1,472 |
| wforest | 6,998,372 | 162 | 7,536 | 36,886 | 2,489 | 5,464 | 46 | 2,573 |

Across all thirty-six: 180,525,828 bytes of map, 3,969 shaders, 342,548 planes,
137,750 nodes, 140,192 leafs, 344,505 leaf brushes, 368,328 leaf surfaces,
62,793 brushes, 526,024 brush sides, 183,673 surfaces of which 6,666 are
patches, 1,007,917 draw vertices, 1,824,531 draw indexes, 2,406 models, 11
fogs, 65,430 visibility clusters and 2,278,540 bytes of entity text.

`jlair2` is worth naming: 3,872 surfaces in 128 leafs and 46 clusters, with no
patches at all. It is a valid map and translates like the rest, but it is the
one that would break a reader that assumed leaf counts scale with surfaces.

## Patch collision

Retail builds patch collision differently from Quake III in two ways, both
read from `alice.exe` (image base 0x00400000) with a disassembler, and both
now followed for Alice's maps (`CMod_LoadPatches` in `code/qcommon/cm_load.c`,
constants in `code/qcommon/bsp_fakk.h`):

- **Some patches never collide.** Retail's `CMod_LoadPatches`
  (0x00430683..0x004306bc) reads the patch's shader contents and, unless bit 0
  or any of bits 16-19 is set, leaves the surface slot empty and builds
  nothing. Those bits are solid and the player, monster, camera and weapon
  clips in Ritual's content numbering (the names are those of Ritual's
  F.A.K.K.2 v1.02 SDK headers, consulted as reference only; the mask itself is
  the binary's). id's loader has a `FIXME` in the same
  place and builds every patch. On the retail maps this drops 296 of the
  6,666 patches, among them `garden1`'s two `textures/liquid/flowingwater1`
  water patches and `gvillage`'s four `textures/skin/flehblobber` patches,
  whose contents are 0. So a trace that masks water or any other content no
  longer strikes a patch retail never built.
- **Patches are cut by their shader's count.** Retail keeps its 76-byte
  shader records whole (`CMod_LoadShaders`, 0x0042fc86..0x0042fcad), takes
  the record's `subdivisions` at +0x48, raises it to 16 when smaller
  (0x00430780..0x0043078c), and passes it to `CM_GeneratePatchCollide`
  (0x004307a9), which hands it to `CM_SubdivideGridColumns` (0x00432283,
  0x004322ac). There `CM_NeedsSubdivision` compares a curve midpoint's
  distance from its chord with it, `>=` (0x00432877), where id compares with
  a fixed 16. Of the 6,370 patches that collide, 3,219 have a count above
  16, up to 758, and on those retail's collision follows the curve more
  loosely than Quake III's would; the facet total falls from 99,917 to
  41,169.

Retail has the same edge-bevel check as id's and would make the same report,
`WARNING: CM_AddFacetBevels... invalid bevel` (string 0x00510f88), but only
at `developer 2`: it tests `developer->integer == 2` before calling
`Com_DPrintf` (0x004320b4..0x004320c4; 0x00bd1c58 is the `developer` cvar,
set at 0x004398d7). fnTech3 now does the same for Alice, and keeps id's
`developer 1` report for every other profile but Enemy Territory, which drops
it. `too many bevels` and `bevel plane already used` go through `Com_DPrintf`
in retail (0x00431d1c, 0x00431f99, 0x00431fd5) and through `Com_Printf` in
fnTech3; neither occurs on any retail map, so the difference is not visible.

To see what each rule does, fnTech3's own patch generator was compiled on its
own and fed every patch of the thirty-six maps (a scratch harness, not
committed). It reproduces the runtime counts exactly — 24 reports on
`garden1` and 64 on `gvillage` before, 2 and 30 after at `developer 2`. A
second build replaced fnTech3's double-precision helpers with Quake III's
single-precision ones, which is how retail computes:

| Rules | Patches built | Facets | Invalid bevels | Maps with any |
| --- | --- | --- | --- | --- |
| Quake III's (before), double | 6,666 | 99,917 | 1,166 | 25 |
| Alice's (now), double | 6,370 | 41,169 | 390 | 15 |
| Alice's, single precision (retail-like) | 6,370 | 41,169 | 398 | 17 |

So retail also meets invalid bevels on these maps, about as many, and keeps
them to `developer 2`. The largest are `hedge3` (152 now, 164 single
precision), `fortress1` (56, 48) and `hedge1` (36, 32). The remaining
difference, a few bevels per map either way, comes from fnTech3's
double-precision patch arithmetic, which every profile shares and Quake III's
retail engine did not have either; it is not an Alice divergence and is left
as it is. `hedge2` also prints ten `WARNING: CM_GridPlane unresolvable` under
every rule and precision; retail prints that one through `Com_Printf` too
(0x004335ac).

Runtime probe, windowed, isolated home, developer client
(`meson/build/win32-alice-content`, `+devmap`, 1,000 frames after skipping the
intro cinematic), before and after the change, installation unchanged both
times:

| Map | Before, `developer 1` | After, `developer 1` | After, `developer 2` |
| --- | --- | --- | --- |
| `garden1` | 24 | 0 | 2 |
| `gvillage` | 64 | 0 | 30 |

`tests/alice_patch_collision_tests.py` compiles the engine's own
`CMod_LoadPatches` and patch generator against synthetic patches: the loader
skips water, shootable-only and empty patches and cuts the rest by their
count, never under 16, while every other map's patches are all built at 16;
the generator cuts an arch whose midpoint is 32 units off at a limit of 32 but
not at 32.1; and two twisted patches report invalid bevels for Quake III at
`developer 1`, for Alice only at `developer 2`, and never for Enemy
Territory. This replaces an earlier note here that the warning ran "between
zero and forty" per map and was "not Alice's": under Quake III's rules the
harness counts up to 444 on one map (`hedge3`), and retail's own code has the
check.

## What the entity text says the game module must spawn

The entity lumps translate as text and are not interpreted, but they were
read once, because they say more about this title's gameplay gate than
anything else available without running a module. Across the thirty-six maps
there are **23,244 entities, 252 distinct classnames and 75 distinct keys**.

| Classname | Count |
| --- | --- |
| `light` | 8,908 |
| `info_pathnode` | 5,365 |
| `info_splinepath` | 1,724 |
| `script_object` | 1,053 |
| `info_waypoint` | 689 |
| `func_spawn` | 418 |
| `trigger_once` | 412 |
| `script_model` | 266 |

One of those rows is a finding rather than a statistic. **Alice's AI navigates
a graph of 5,365 designer-placed `info_pathnode` entities**, with 689
`info_waypoint` beside them. It has no compiled navigation mesh: there is no
`.aas` file in any retail pak, and this is the third independent sign that
this engine carries no Quake bot library, after the absent `bot_enable` cvar
family and the game module leaving its `BotBegin` and `BotThink` export slots
null. Whatever serves this title's AI reads entities, not a navigation file.

The 1,053 `script_object` and 266 `script_model` entities are the other thing
worth naming: this title's level logic is script-driven from the map, so the
`.scr` interpreter is on the gameplay critical path in the way Jedi Academy's
ICARUS is on its. The 73 installed scripts were surveyed on that account, and
the shape of that gate is worth recording before anyone starts it.

They are a real language, not a data format: 28,740 non-blank lines declaring
**2,034 functions**, with `if`, `else`, `for` and `while`, and with coroutines
-- 1,169 `thread`, 4,296 `wait` and 1,824 `waitfor`. A script blocks and
resumes, so an interpreter needs a scheduler, not just an evaluator. Sixty of
the 73 are brace-nested code; the other thirteen, in `ui/`, are line-based
key-and-value layouts that share the extension and nothing else.

The part that bounds the work is the vocabulary. Of 1,425 distinct bare call
names, **1,388 are functions the scripts themselves declare** -- content, not
engine surface -- leaving **37 builtin calls**. Alongside them are **169
distinct method names** invoked on 2,005 named receivers, and those receivers
are map entity targetnames rather than engine objects. So the binding surface
an interpreter has to provide is about **206 names**, led by `anim`, `time`,
`show`, `hide`, `remove`, `bind`, `triggerable` and `movedown` on entities, and
`trigger`, `cuecamera`, `cam_fadein`, `waitfordialog`, `level_ai`, `killthread`
and `skipthread` as bare calls. That is a bounded number, comparable to the
ICARUS surface this tree already records for Jedi Academy, rather than an open
-ended one.

## What the shader scripts ask of a renderer

The installation's 52 `.shader` scripts were read once for the same reason the
entity text was: they size a gate. They hold **2,162 shaders and 5,017 stages**,
written in Quake III's shader syntax, using 27 distinct general directives and
15 stage directives.

fnTech3's own shader parser already names all but eleven of the general ones
and three of the stage ones, and most of that remainder is not a renderer's
problem: `qer_editorimage` (1,590 uses), `qer_trans`, `qer_nocarve`,
`q3map_globaltexture`, `q3map_backshader` and `surfacelight` are editor and
compiler keywords. What is actually Ritual's, and actually missing, is small:

| Directive | Uses | Where |
| --- | --- | --- |
| `nodepthtest` | 124 | stage |
| `spritegen` | 112 | general |
| `fogonly` | 24 | general |
| `alphatest` | 21 | stage |
| `endif` | 9 | general |
| `portalsky` | 7 | general |
| `depthmask` | 3 | stage |
| `surfacecolor` | 1 | general |

Two of those are worth a sentence. `portalsky` pairs with the `SetSkyPortal`
import named in the [module record](2026-09-22-alice-module-abi.md), so the sky
portal is a contract the shader script and the game module both reach into.
And an `endif` with no matching keyword among the other twenty-six means this
shader language has conditionals, which a parser has to handle before it can
read the file at all rather than after.

## What this does not claim

The translation is lossy by design and is collision evidence only. The light
grid, the entity lights and their visibility, and the per-surface light
definitions are dropped, so a renderer cannot be built on this reader: it will
need one that keeps them. No map is drawn, no shader script is parsed, no
entity is spawned, and no module has run.

The patch collision rules above are read from the retail binary and checked
against fnTech3's own generator; no trace has been compared with the retail
engine's, and retail's single-precision patch arithmetic is not reproduced.
