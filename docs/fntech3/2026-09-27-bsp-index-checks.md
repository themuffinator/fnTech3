# Index checks in every map loader

The [damaged-map record](2026-09-26-bsp-faults.md) left 24 damaged maps open:
each read through an index that no loader checked, and Windows' exception
filter turned the access violation into an error. The collision loader and the
three renderers' world loaders now check every index of that kind before they
follow it, and refuse a map whose index leaves its table. Before choosing
between refusing and repairing, every map of every installed game, 607 of
them, was measured and loaded under the build without the checks; loaded again
with them, not one load changed, in any of the three renderers. The same
damaged maps now fault, crash and hang nowhere: of 409, 69 load and 340 are
refused. The [machine-readable record](2026-09-27-bsp-index-checks.json)
holds every load and every variant.

## The checks

[`code/qcommon/bsp_index_checks.h`](../../code/qcommon/bsp_index_checks.h)
holds the tests: a range of records inside its table, an index inside its
table, a node's child as a node or a leaf, a leaf's area as none or one of the
256 the area bits hold, and a leaf's cluster as none or a row. Each is written
so that no sum overflows. The collision loader
([`cm_load.c`](../../code/qcommon/cm_load.c)) and every renderer's
`tr_bsp.c` take them from there, so an index they both read meets the same
rule in each; each checks the indexes it reads itself.

| Lump | What is checked | Collision loader | Renderers |
| --- | --- | --- | --- |
| 3, nodes | plane; children, as nodes or leafs | `CMod_LoadNodes` | `R_LoadNodesAndLeafs` |
| 4, leafs | area; cluster; leaf-surface range | `CMod_LoadLeafs`, `CMod_LoadLeafSurfaces` | `R_LoadNodesAndLeafs` |
| 4, leafs | leaf-brush range | `CMod_LoadLeafBrushes` | -- |
| 5, leaf surfaces | each names a surface | `CMod_LoadLeafSurfaces` | `R_LoadMarksurfaces` |
| 7, models | brush and surface ranges, before the collision loader sizes a model's lists by them; the renderers read only the surfaces | `CMod_LoadSubmodels` (inline models; collision never reads the world model's) | `R_LoadSubmodels` (every model, the world's included) |
| 8, brushes | side range, and the six sides `CM_BoundBrush` reads | `CMod_LoadBrushes` | `R_LoadFogs`, a fog brush's six sides |
| 9, brush sides | plane | `CMod_LoadBrushSides` | `R_LoadFogs`, a fog brush's sides |
| 13, surfaces | a face's and a triangle surface's vertex and index ranges; each face index against the face's vertices; a patch's size against the arrays it is copied to, and its vertex range | -- (patches were already checked) | `ParseFace`, `ParseTriSurf`, `ParseMesh` |
| 16, visibility | a partial header; a table with no row for a cluster the leafs name, or rows too short for its clusters | `CMod_LoadVisibility` | `R_LoadVisibility` |

A damaged index is refused with `ERR_DROP`, as the loaders' other checks
refuse a bad shader number or patch vertex range. Two cases keep today's
behaviour instead. A visibility table that does not cover the leafs' clusters
leaves the map unvised, as a table that does not fit its payload already did in
the collision loader, and now in the renderers too. A leaf brush that names no
brush is still repaired to brush 0 by `CMod_CheckLeafBrushes`.
[`Hunk_Alloc`](../../code/qcommon/common.c) now refuses a negative size, as
`Hunk_AllocateTempMemory` already did: a wrapped count used to reach it and
clear memory off the hunk. Its space check no longer sums the banks, which
could wrap too.

Medal of Honor's world rebuild
([`tr_mohaa_world.c`](../../code/renderercommon/tr_mohaa_world.c)) reads the
leafs' surface lists before any renderer loader does, to add each leaf's
terrain, static models and decals to its list. It already skipped entries
outside the map's list, but copied each leaf's list into one the size of the
map's without checking that they fit together, so a damaged map whose lists
overlap would write past it, and a huge count stepped through billions of
entries to find none. It now takes only the part of each list inside the
map's, and when the parts overlap it declines to rebuild, with a warning,
before anything is allocated, as its contract already allowed for a build that
fails; the map then loads as translated. Every installed Medal of Honor map's
lists partition its list exactly.

## Refuse or repair: what the installed maps carry

[`scripts/audit_bsp_indexes.py`](../../scripts/audit_bsp_indexes.py) presents
every map of every installation as the loaders see it, through the engine's own
translators for the IHV's IBSP 43, Jedi Academy's and Jedi Outcast's RBSP,
Alice's and F.A.K.K.2's FAKK (both the collision loader's and the renderers'
translation), Medal of Honor's 2015 and Q3Test's IBSP 44 and 45, and measures
every index the loaders follow. Of 607 maps, 673 translations, none fails any
check above. The values out of range that retail maps do carry are all ones
the loaders leave alone, because the code that reads them bounds them:

- fog 0 on flare surfaces in maps with no fog volume: 3,682 flares in 33 Quake
  III maps, 6,569 in 26 RTCW maps, 2,634 in 37 Elite Force maps and 11 in one
  Jedi Outcast map. The flare code tests the fog against the map's count.
- lightmap numbers past the map's lightmaps: Enemy Territory's six maps, whose
  external lightmaps these are, and Team Arena's `texturegrab`.
- faces of more than 64 vertices: 84 in Enemy Territory's `fueldump` and
  `radar`, where that profile allows a batch, and two of 109 in Elite Force's
  `scav5`, which the `stvef-sp` renderer cuts to 64 and draws with its default
  shader, as before. A face's indexes are therefore checked against the
  vertices it declares, not the 64 kept.

Every map's node tree also numbers each child after its parent, so no retail
tree has a cycle; the loaders do not check that yet (see below). No installed
map fails a check, so refusing costs none, and every check refuses. The two
cases above that do not refuse follow what the collision loader already did.

## Every installed map, before and after

[`scripts/probe_retail_map_loads.py`](../../scripts/probe_retail_map_loads.py)
loads each map of an installation in a fresh process with an isolated home,
under the profile that owns it: a client for every profile with one, loading
the map with `map` (`map NAME ffa` for Quake Live), letting it run and quitting;
the dedicated server for the IHV and Jedi profiles and for Q3Test 1.02 and
1.03, which shipped no Win32 client game. The Jedi single-player modes stop by
name at their module boundary once the map's collision has loaded, so that is
how their maps end. Each home carries empty configurations under every
profile's names, because the installation's own would otherwise be run: the
Steam Quake III install's `missionpack/q3config.cfg` sets a 56 MB hunk, which
failed Team Arena's first map on the probe's first attempt. Windowed on
unswitched private desktops, sound, mouse and joystick off, networking off.

| Profile | Maps | GLx before | GLx after | Vulkan before | Vulkan after | RTX before | RTX after | Changed |
| --- | ---: | --- | --- | --- | --- | --- | --- | ---: |
| `q3` | 69 | 69 loaded | 69 loaded | 69 loaded | 69 loaded | 69 loaded | 69 loaded | 0 |
| `ql` | 149 | 149 loaded | 149 loaded | 149 loaded | 149 loaded | 149 loaded | 149 loaded | 0 |
| `rtcw-sp` | 32 | 31 loaded, 1 rejected | 31 loaded, 1 rejected | 31 loaded, 1 rejected | 31 loaded, 1 rejected | 31 loaded, 1 rejected | 31 loaded, 1 rejected | 0 |
| `rtcw-mp` | 15 | 15 loaded | 15 loaded | 15 loaded | 15 loaded | 15 loaded | 15 loaded | 0 |
| `wolfet` | 6 | 6 loaded | 6 loaded | 6 loaded | 6 loaded | 6 loaded | 6 loaded | 0 |
| `stvef-sp` | 66 | 66 loaded | 66 loaded | 66 loaded | 66 loaded | 66 loaded | 66 loaded | 0 |
| `stvef-mp` | 33 | 33 loaded | 33 loaded | 33 loaded | 33 loaded | 33 loaded | 33 loaded | 0 |
| `q3-ihv` | 4 | 4 loaded | 4 loaded | -- | -- | -- | -- | 0 |
| `jka-sp` | 34 | 0 loaded, 34 rejected | 0 loaded, 34 rejected | -- | -- | -- | -- | 0 |
| `jka-mp` | 23 | 21 loaded, 2 rejected | 21 loaded, 2 rejected | -- | -- | -- | -- | 0 |
| `jk2-sp` | 26 | 0 loaded, 26 rejected | 0 loaded, 26 rejected | -- | -- | -- | -- | 0 |
| `jk2-mp` | 15 | 15 loaded | 15 loaded | -- | -- | -- | -- | 0 |
| `alice` | 36 | 35 loaded, 1 rejected | 35 loaded, 1 rejected | 35 loaded, 1 rejected | 35 loaded, 1 rejected | 35 loaded, 1 rejected | 35 loaded, 1 rejected | 0 |
| `fakk2` | 30 | 30 loaded | 30 loaded | 30 loaded | 30 loaded | 30 loaded | 30 loaded | 0 |
| `mohaa` | 54 | 54 loaded | 54 loaded | 54 loaded | 54 loaded | 54 loaded | 54 loaded | 0 |
| `q3test` | 15 | 15 loaded | 15 loaded | 15 loaded | 15 loaded | 15 loaded | 15 loaded | 0 |

A load is compared by its outcome, its first engine error and the renderer's
count of faces, meshes, triangle surfaces and flares; for Medal of Honor that
count includes the terrain, static models and decals its rebuild adds. No load
differs between the builds, in any renderer. The loads that do not end in
`loaded` end the same way in both, for reasons outside the loaders: the Jedi
single-player modes' module boundary (34 Jedi Academy and 26 Jedi Outcast
maps), the game import `G_G2_CLEANMODELS` that Jedi Academy's two siege maps
reach and the engine does not serve yet, Alice's `wforest`, whose game stops at
a negative area-portal count, and RTCW's `norway`, whose game stops at a script
naming an entity it cannot find (`ice2_spot2`).

Two first results were corrected on the same binaries. RTCW's `norway` was
first read as hung, because its error message ends in a newline, which the
probe's pattern did not expect. Three Elite Force single-player maps (`scav1`,
`tour/deck10`, `tour/deck15`) stopped the Vulkan run without the checks with a
zone allocation failure, an `ERR_FATAL`, while five clients ran at once, one
of them an extra run started by mistake; run again alone, all three loaded.
The probe now reads such messages and reports an `ERR_FATAL` as `fatal` at
once; every later run used it.

## The damaged maps again

[`scripts/probe_bsp_faults.py`](../../scripts/probe_bsp_faults.py) ran again,
unchanged, with the same maps, seed and variants as on 2026-09-26.

| Profile | Map | Variants | Loaded | Rejected | Faulted | Crashed | Hung | On 2026-09-26 |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| `q3` | `q3dm1` | 101 | 17 | 84 | 0 | 0 | 0 | 19 loaded, 75 rejected, 7 faulted |
| `ql` | `campgrounds` | 105 | 18 | 87 | 0 | 0 | 0 | 20 loaded, 80 rejected, 5 faulted |
| `rtcw-sp` | `escape1` | 102 | 17 | 85 | 0 | 0 | 0 | 18 loaded, 78 rejected, 6 faulted |
| `rtcw-mp` | `mp_beach` | 101 | 17 | 84 | 0 | 0 | 0 | 19 loaded, 76 rejected, 6 faulted |

Every variant is byte for byte the one of 2026-09-26, as each input hash shows.
Each of the 24 that faulted is now refused by the check for the index it
damaged: a node's child or plane (seven), a leaf surface naming no surface
(seven), a brush's side range (five), a leaf's area (two), an inline model's
brush or surface range (two) and a face's vertex range (one).

Seven variants that loaded on 2026-09-26 are refused now. Each damages an index
that the level's 120 frames happened not to follow: the world model's surface
range, twice in `q3dm1`, which the renderers read when anything draws the world
model or Enemy Territory projects its world decals; a leaf's area or cluster
(two) or leaf-brush range (one) in `campgrounds` and `mp_beach`; a leaf surface
naming no surface in `escape1`; and face indexes naming no vertex of their face
in `mp_beach`. Eighteen variants that were refused before are refused now by
the check for the index they damaged, where a later symptom used to refuse
them: an allocation too large for the hunk (seven), a triangle surface's index
(four), a brush side's shader, now read after its plane (four), and a surface
type, a shader number and an entity's bounds (one each).

## Tests

[`tests/bsp_index_bounds_tests.py`](../../tests/bsp_index_bounds_tests.py)
compiles the engine's own loader functions -- the collision loader's, and each
renderer's node, leaf, leaf-surface, model, visibility and surface parsers --
and `Hunk_Alloc`, and feeds them a small synthetic map: a valid one loads, each
index moved out of its table is refused with its own message, and the edges
that must load do (area -1 and 255, the last leaf as a child, an empty range at
the end of a table, a brush claiming fewer than six sides whose six are there,
a face over the vertex limit, a patch that is never drawn). It also runs
Medal of Honor's rebuild over leaf lists that partition the map's, start or
end outside it, or overlap. Disabling any one of the 33 checks alone -- 14 in
the collision loader and `Hunk_Alloc`, 18 in each renderer, tried in all
three, and the rebuild's -- fails the tests.
`tests/rtcw_collision_model_tests.py` passes the new surface lump to
`CMod_LoadSubmodels`.

## What this does not show

- A node whose child leads back to one of its ancestors is not refused: every
  index is inside its table, and a descent never ends. No installed map has
  one, nor any damaged variant that passes the checks; Quake III's point and
  trace code would hang on it and the renderers' `R_SetParent` would overflow
  its stack. The IHV profile's own translator already refuses cyclic trees.
- Medal of Honor's rebuild met damaged leaf lists only in the unit test; the
  installed maps show it builds every map as before.
- Fog numbers and lightmap numbers are not checked at load, for the reasons
  above; a damaged one is bounded where it is read.
- `loaded` means the level ran and the client quit; a damaged lightmap, light
  grid or visibility lump that loads is not shown to be drawn correctly.
- The damaged maps ran under the GLx renderer only, as in 2026-09-26; the
  Vulkan and RTX loaders were loaded only with installed maps. RTX ran with its
  ray tracing off, its default, which does not change what its loader reads.
- Windows only.

## Identity

- Build without the checks: `3fe5f45`, a clean tree, built in
  `meson/build/win32-dev` with every development gate on, as VS Code's "setup
  all games" task configures it. Client `fntech3.exe` SHA-256
  `6e169d45111b0b46a298bb0c77f5bc05d727ed56d76dda7824cc9bb2731c5cb8`, server
  `fntech3.ded.exe` `8e8a7a0f4aaf8481bf1cdf04ff1a8000c090ddb8d6ecbef221ac1751ff34f227`,
  renderers `fnql_glx_x86.dll`
  `2b61d62858ab6db26efd461321827056da2d5627bd39e45e763611f92ff910b1`,
  `fnql_vk_x86.dll` `2739e6a2c5b4281dd19e8544d0642f56e9d6458008958759ea7af73473ab961b`
  and `fnql_rtx_x86.dll`
  `3fa22f4db4e2ce7f59f2bd64de524deef5eaa69e942451b2cd6f3bf087987797`.
- Build with the checks: `3fe5f45` with this change, built in the same folder
  from the working tree, which held nothing else. Client
  `7d25ae2f9922190c94a22a4641494173f193ce330dffc9ca563f1977d4fc1b88`, server
  `688c27ca0b18455e9661a3b2f6bfe896ba2700816c93cc0fdaa7afc6edbe99c4`, renderers
  `edfbafa55078d3e7ca95c704d7050014a3bb644767ad6d3668faeb6972ed4072` (GLx),
  `cdfaa1a1a69537b749f8ebb8c646e6bf9bc828b4be141048063666033aca4011` (Vulkan)
  and `53a99141932192edfd78df6581ce1dca6e922efdf38bc49318122157478f38b3`
  (RTX). The commit differs from that tree only in the wording of one comment
  in `bsp_index_checks.h`.
- MSVC 19.44 x86, debugoptimized, strict warnings. Windows 11 x64 host, NVIDIA
  GeForce RTX 4060 Laptop GPU.
- Runs, 2026-09-27 UTC: without the checks, GLx 05:46-06:07, Vulkan
  06:07-06:22 and its three maps again at 17:39, RTX 17:41-17:57; with them,
  the damaged maps 17:59-18:27, GLx 18:28-18:51, Vulkan 18:51-19:08 and RTX
  19:08-19:28. The index audit ran on 2026-09-27 against the same
  installations.
- Installations, read only: `C:\Program Files (x86)\Steam\steamapps\common\Quake 3 Arena`,
  `E:\SteamLibrary\steamapps\common\Quake Live`,
  `E:\SteamLibrary\steamapps\common\Return to Castle Wolfenstein`,
  `E:\SteamLibrary\steamapps\common\Wolfenstein Enemy Territory`,
  `E:\Games\Star Trek Voyager Elite Force`, `E:\Games\Quake 3 IHV`,
  `E:\Games\Jedi Academy`, `E:\Games\Jedi Outcast`, `E:\Games\Alice`,
  `E:\Games\Medal of Honor Allied Assault`; F.A.K.K.2 and Q3Test from the
  layouts under `.tmp/fakk2-install/game` and `.tmp/q3test/install`.
