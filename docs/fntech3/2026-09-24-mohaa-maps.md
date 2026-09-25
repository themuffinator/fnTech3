# Medal of Honor: Allied Assault maps

## Scope

The fifty-four maps of the retail 1.00 installation
([contracts record](2026-09-24-mohaa-contracts.md)), how fnTech3 loads them
for collision and for the three renderers, and what the sweeps observed. The
format's lump order and records follow OpenMoHAA's `qfiles.h`
([openmoh/openmohaa](https://github.com/openmoh/openmohaa/tree/ab43a0def6c4feccedae36fdb58c2e007d7e4c35),
GPL-2.0-or-later) as reference; every size was re-measured on the retail maps.
The terrain collision is incorporated from OpenMoHAA's `cm_terrain.c`
(`code/qcommon/cm_mohaa_terrain.c`, with its licence header). Marks: **[O]**
OpenMoHAA's reading, **[I]** inference.

## The maps

All fifty-four are in the paks (`Pak5.pk3` holds the campaign's): seven
deathmatch maps (`DM/mohdm1`..`mohdm7`), four objective maps
(`obj/obj_team1`..`4`), six briefing rooms (`briefing/briefing1`..`6`), the
campaign's thirty-four levels from `m1l1` to `m6l3e`, `training`, `credits`
and `void`. Each is a "2015" map: ident `2015`, version 19, a checksum after
the version as Ritual's maps have, and 28 lumps. On every one, each lump
divides into its record size below, and the directory tiles the file up to an
84-byte table of counts that no lump names.

Most records are Quake III's own, byte for byte: planes, nodes, leaf brushes,
leaf surfaces, brushes, models, draw vertices and indexes, the entity text
and the visibility. Four differ:

| Record | Bytes | Beyond Quake III's |
| --- | --- | --- |
| shader | 140 | Quake III's 72, a subdivision count, and the name of a fence mask image a point trace through a fence brush consults |
| surface | 108 | Quake III's 104 and a subdivision count, as Ritual's |
| leaf | 64 | Quake III's 48 and two ranges: into the terrain indexes and into the static model indexes |
| brush side | 12 | Quake III's 8 and an index into the side equations, which map a fence brush's side onto its mask |

Eight lumps have no Quake III counterpart: the side equations, the terrain
patches and their per-leaf indexes, the sphere lights and their visibility,
the per-surface light definitions, and the static models with their vertex
colours and indexes. The light grid is a palette, per-cell offsets and packed
data rather than Quake III's eight bytes a point.

## Loading

`code/qcommon/bsp_mohaa.h` presents a version 19 map to the existing
collision loader as the IBSP 46 layout it reads, and carries what collision
needs beyond it -- each shader's subdivision count, each leaf's terrain range,
each brush side's equation, the terrain patches and their indexes -- after the
last lump, where an IBSP 46 reader never looks. The static models (their
definitions, per-leaf ranges, indexes and vertex colours), the light grid and
the sphere lights are carried the same way for the renderers. The light
definitions are dropped.

**Terrain.** A patch covers 8 by 8 squares of 64 units, 512 by 512 from its
corner, with a 9 by 9 heightmap of two-unit steps above its base. Each square
is two triangles whose shared diagonal alternates across the patch and flips
with the patch's flag; the renderer's variance tree, read only for the bits
that take a square's halves out, decides whether a square collides as one
plane, two, a single triangle or not at all **[O]**. Point traces walk
the squares a segment crosses; a capsule trace tests each square's triangles
against its lower sphere; a box trace sends a point from each lower or upper
corner. The renderers build every patch at full detail into ordinary world
triangles split along the same diagonals; the texture coordinates
interpolate the patch's corners and the lightmap coordinates take a block of
`8 * lmapScale` texels **[O]** (`code/renderercommon/tr_mohaa_world.c`).
Each patch's surface names the patch's lightmap, and the renderers light it
with that lightmap
(`tests/mohaa_terrain_lightmap_tests.py`): Quake III's loader had lit every
triangle surface by vertex, and a terrain vertex is white, so the terrain of
every map drew unlit. No map's own surface lump holds a triangle surface, so
the rule reaches only the surfaces built here. **Observed:** at `m1l2a`'s
start, a night level whose terrain lightmaps average 23 of 255, the sand that
had glared white is dark but for the lamps' pools, on the GL renderer.

**Taken-out terrain.** The variance tree's two bits on each of its last
nodes take a triangle out of the terrain: the collision model walks through
it, and 2015's renderer never drew it (a triangle whose parent node has the
bit is allocated undrawable, OpenMoHAA `R_AllocateTri` and
`RB_DrawTerrainTris` **[O]**). The renderers had built every triangle, so
the ground they drew was not the ground walked on. Both now read the bits
through one header (`code/qcommon/mohaa_terrain_squares.h`, the collision
model's own table, moved there), and the build leaves out what the
collision model does; a patch keeps its surface, in the map's order, however
few triangles are left, so a surface names its patch
(`tests/mohaa_terrain_marks_tests.py`). **Measured** over 1.00's 36 maps
with terrain: they take out from 1.6% of their terrain's triangles
(`m6l1c`, 174 of 10,624) to 80% (`m5l3`, 5,737 of 7,168) -- `DM/mohdm4`
69%, 21,475 of 30,976, and 72 of its 242 patches whole. Of the 29,401
taken out of the six deathmatch maps with terrain, a point two units above
each one's centre lies inside a solid brush for 4,811 and outside the map
for 4,738; of the other 19,852, 767 lie under a solid brush within 128
units and 19,085 in open air (read from the maps' brushes, not drawn).
**Observed:** `DM/mohdm3` logs "terrain: 2689 of 2816 triangles in 22
patches", the count the bits give. Where the ground that had been drawn in
open air shows from the playable area is unexamined.

**Static models.** Each is a TIKI definition the map places, skinned once in
the first frame of its first animation, placed by its origin, angles and
scale, and coloured by the map compiler's vertex colours in the order its
surfaces list their vertices **[O]**; a model given no colours is lit white.
They join the leafs the map lists them in as world triangles, so every
renderer draws them with its own lighting, visibility and batching. They keep
their full detail, where 2015's renderer lowered it with distance **[O]**:
that is presentation, which invariant 11 leaves to fnTech3, and a static
model is scenery the game never reads, so no contract depends on it.
Entities, which the tools and menus measure, do lose detail by their
records (the
[contracts record](2026-09-24-mohaa-contracts.md#decals-and-level-of-detail)).

**Permanent decals.** A level's `maps/<name>.dcl` holds the decals its
designers laid with 2015's decal editor (`code/qcommon/dcl_mohaa.h`). 1.00
ships twelve, all in `Pak5.pk3`, all version 1 with a checksum of 0; eleven
have a map (`m1l3a_e3`'s has none). **Measured:** a 20-byte header ("DCL ",
version, checksum, decals, fragments); each decal's 124-byte definition
(shader, position, projection, radius, height and width scales, rotation,
colour, lighting), its projection being the normal of the surface it was
laid on (1,678 of the 1,772 fragments of five files wind clockwise around
their nearest decal's projection; the rest lie on another face of a corner
or on terrain); each
fragment's 76-byte record (shader, fog 0, what it lies on, point count) and
its points as Quake III polygon vertices, at most eight, their colour already
lit; then one list for each brush model and each leaf of the fragments it
holds. No 1.00 fragment lies on a brush model; `DM/mohdm3`, `DM/mohdm7` and
`m1l3a_e3` hold terrain fragments. Version 2, which later engines write
**[O]**, adds the map's `map_time` worldspawn key after the header so a file
cut against an older compile is refused; 1.00's maps carry that key, its
files do not.

The lists fit their map only in `DM/mohdm4`, `m2l3` and `obj/obj_team4`: the
other eight files were saved before their map's last compile, and number
leafs it no longer has (`DM/mohdm1`'s name 3,482 lists for a map of 1,920
models and leafs). So the renderers do not read them. Each fragment is built
into the world as a fan of triangles with its own shader, listed in every
open leaf that one of its points lies in, a unit in front of the fragment,
and in the leaf of their centre -- where 2015's editor links a fragment it
has cut **[O]** (`code/renderercommon/tr_mohaa_world.c`,
`tests/mohaa_permanent_decal_tests.py`). **Observed:** on the three files
whose lists fit, that places 49 of the 50 fragments in exactly the leafs the
file lists; the fiftieth, `m2l3`'s fragment 3, the file also lists in leaf
122, which is solid and never drawn. Its points' colours are kept as the file
has them, as the scene's polygons are, 2015's own marks among them
(`ParseTriSurf` leaves these surfaces unshifted in all three renderers); a
version 2 file with another map time is refused, a malformed file is refused
whole, and a fragment with fewer than three points, no area, a brush model
under it or no open leaf is left out. The advanced options' "Static Decals"
box sets `r_drawstaticdecals`, which the detail configs set too (`high.cfg`
and `medium.cfg` 1, `low.cfg` 0, `Pak0.pk3`); the renderers skip the built
surfaces while it is 0. Nothing is built while `dcl_editmode` is set, when
the decal editor draws the decals instead (the
[contracts record](2026-09-24-mohaa-contracts.md#decals-and-level-of-detail)).
**Observed:** `m2l3` builds 42 of 42 fragments into 72 leaf places; before
its `motto3` and `bunkervent_1` decals, the GL, Vulkan and RTX renderers'
`screenshot`s hold 11,251, 11,297 and 11,290 dark pixels in the decals' area
with `r_drawstaticdecals 1`, and none with 0 (`.tmp/mohaa-decals`). Under
the RTX renderer's path tracer (`rtx_rt_mode 2`), a decal's shader, having
`polygonOffset`, belongs to the raster pass drawn over the traced frame, so
no decal enters the traced geometry; the decals draw over the traced frame
and `r_drawstaticdecals 0` hides them there too.

**Light.** The grid's palette, offsets and data and the sphere lights become
a Quake III light grid baked as the map loads: ambient from the grid scaled
by 0.18 plus the map's `ambientlight`, the directed part from the leaf's
sphere lights and the sun, times 1.3, at a spacing that grows from 64 units
until the grid fits **[I]**; `gridsize` is written into the worldspawn text.
`DM/mohdm1`'s grid is 71 by 80 by 23 points at 64 units. Entities take it as
diffuse light; static models and world vertices keep their compiled colours
(`rgbGen lightingSpherical` and `lightingGrid` stages). The collision model
keeps the sphere lights and each leaf's list of them too, for the client
game's "Complex" shadows (`code/qcommon/cm_mohaa_lights.c`; the
[contracts record](2026-09-24-mohaa-contracts.md#client)).

**Shaders.** Ritual's shader dialect is read for this profile, with 2015's
additions: `alphaGen distFade`, `oneMinusDistFade`, `sCoord`, `tCoord`;
`rgbGen dot`, `oneMinusDot`, `sCoord`, `tCoord`; `tcGen environmentModel`;
`tcMod wavetrans`, `wavetrant`, `bulge`; `deformVertexes flap` and
`lightglow`; `blendFunc alphaadd`; `noMerge`; `animMapPhase`; `clampmapx` and
`clampmapy`; `depthFunc always` (`code/renderercommon/tr_mohaa_shade.h`). A
`nextbundle $lightmap` counts as the shader's lightmap stage. The far plane's
fog and cull and the sky portal use the Ritual view path.

## Observed

- **Server:** all fifty-four maps spawn and run twelve seconds each through
  the retail game module on the developer dedicated server, with no drop.
- **Client:** all fifty-four load in the developer client (GL renderer,
  1280 by 720, windowed), draw a frame the engine's `screenshot` command
  writes, and exit cleanly (`.tmp/mohaa/client-maps-report.txt`). The logs
  carry the retail data's own complaints -- sound aliases missing from
  `ubersound.scr`, surfaces a TIKI names that its skeleton lacks, script
  labels that do not exist -- which retail's own modules print. A second
  sweep on 2026-09-25 (`.tmp/mohaa/client-maps2`) drew every map again; the
  aliases it reported missing are missing from the data or excluded by their
  `maps` filter (`snd_step_equipment1` loads for `"m dm moh obj train"`, which
  no briefing's name contains), `gate_large_open` is defined only inside a
  `/* ... */` block of `maps/m6l3a.scr`, and `gfx/2d/BattleCry.tga`, which the
  console shader names, is in no pak.
- **Briefings:** the six briefing rooms captured black in both sweeps. The
  sweep's own `wait`s were the cause: a command a level stuffs joins the
  command buffer behind a pending `wait`, as in 2015's engine **[O]**, so the
  slides' `showmenu` never ran before the sweep's `quit`. Driven instead by
  rcon over loopback (`.tmp/mohaa/rcon_probe.py`), `briefing/briefing1` shows
  every slide in turn on its narration's timing and hands on to `m1l1` with
  `spmap`, whose loading screen waits on its Continue button, once the
  client tells the game when its waited sounds start (the
  [contracts record](2026-09-24-mohaa-contracts.md#sounds-a-script-waits-on)).
  Before that, the briefing stopped for good on its second slide.
- **Scripted flow:** with the same fix, `training`'s instructor lines follow
  one another up to the first one that waits on the player, and `m1l1`'s
  opening drives the convoy to the checkpoint, where the guard steps out.
- The doors of the deathmatch maps were drawn as TIKI weapons until the
  server named the inline models `*1`..`*N-1` as the first model
  configstrings, as 2015's `SV_ClearWorld` does **[O]**; the client game
  draws a general entity (type 5) by its model index (cgamex86 0x300200f6,
  jump table 0x30020dbf).

## Patch collision reports

Each map's colliding patches (shader contents in 0x000f0001, at least 16
subdivisions, as the loader keeps them) were exported from the retail paks and
run through fnTech3's `CM_GeneratePatchCollide` in a harness, and through
OpenMoHAA's builder, compiled from its source for this comparison only
**[O]**:

- `m4l3`'s 48 `CM_GridPlane unresolvable` reports come from twelve 5 by 3
  patches of `textures/algiers/lightplaster1` (surfaces 16 to 27), four each:
  flat arches, each in one plane, whose repeated rows and columns of control
  points leave cells with no area. OpenMoHAA's builder makes the same 48, so
  2015's engine, whose builder it reads as Quake III's, would meet the same
  patches **[O]**. The report is always printed, in both.
- The `CM_AddFacetBevels ... invalid bevel` reports -- a bevel plane whose
  chop leaves nothing of its facet, which the builder skips -- are 12 on
  `DM/mohdm4` (two telephone poles) and 140 on `m4l3` (poles, a barrel and
  two flat plates). OpenMoHAA's builder skips 18 and 156: they differ on thin
  cylinders where a chop comes within the builder's 0.1-unit epsilon, since
  fnTech3's builder works its planes in double precision for every profile.
  2015's engine prints this report only at developer 2 **[O]**, as Alice's
  does, and so does fnTech3's for this profile now. A dedicated server loading
  `DM/mohdm4` makes 12 at developer 2 and none at developer 1; the 13,816 once
  counted there are not reproduced by this build.

## Sidecars

Every map ships an audio zone file and a static light file beside it, the
zones read from these records and the lights from the sphere lights
([sidecar record](2026-09-25-mohaa-map-sidecars.md)).

## Open

- No frame has been compared with retail's, which cannot run here.
