# Damaged maps under every map-loading profile

Commit `c71aa54` added `scripts/probe_bsp_faults.py` on 2026-09-18 without a
record of a run. This record runs it under `q3`, `ql`, `rtcw-sp` and `rtcw-mp`,
one stock map each. The first run found a crash in patch collision, fixed in
`5ba3561`. The full set then ran again on the fixed build: 409 damaged maps, no
crash and no hang, but 24 access violations that the engine caught and turned
into errors. Those 24 remain open. The
[machine-readable record](2026-09-26-bsp-faults.json) holds every variant's
input hash, outcome, first error and, for each fault, its site.

## Method

[`scripts/probe_bsp_faults.py`](../../scripts/probe_bsp_faults.py) reads the map
from the installation's own paks in memory and builds each variant from it:

- **Fixed faults.** Every lump is moved past the end of the file, moved to -4,
  made one byte longer than a whole number of records, and made to claim
  2,147,483,647 bytes; the header's version is one lower; its identifier is
  not `IBSP`; the file ends halfway; the file ends inside the lump stored
  last.
- **Seeded corruption.** 32 variants of 1 to 64 random bytes inside one lump,
  seed 20260918, taking the lumps in turn and never the entity string, which
  native game modules parse.

Each variant is written to a fresh home as `maps/bspfault.bsp` and loaded with
`map` (`map bspfault ffa` for QL) in a fresh client, which loads its
collision, spawns the level and renders 120 frames before quitting. A run is
`loaded` when the client quits normally, `rejected` when an engine error is
printed and the process is still running two seconds later, `crashed` when the
process dies with a failure status, and `hung` otherwise. This commit adds
`faulted`: a rejection whose error is the Windows exception filter's report
(`Unhandled exception caught`, `code/win32/win_main.cpp`) of an access
violation it caught. It also sets `sv_errorExit 0`: Quake Live's retail
default, 1, makes a listen server's recoverable error fatal, and a fatal
error's dialog cannot be dismissed on the probe's private desktop, so the
first QL attempt hung on its first rejected variant. Every run is windowed on
an unswitched private desktop, with sound, mouse and joystick input, Steam
integration, the WebUI, bots and pure mode off, and console commands only.
Damaged maps are rebuilt from the seed, used and deleted; only their hashes
are kept.

## Results

| Profile | Map | BSP | Lumps | Variants | Loaded | Rejected | Faulted | Crashed | Hung |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| `q3` | `q3dm1` | 46 | 17 | 101 | 19 | 75 | 7 | 0 | 0 |
| `ql` | `campgrounds` | 47 | 18 | 105 | 20 | 80 | 5 | 0 | 0 |
| `rtcw-sp` | `escape1` | 47 | 17 | 102 | 18 | 78 | 6 | 0 | 0 |
| `rtcw-mp` | `mp_beach` | 47 | 17 | 101 | 19 | 76 | 6 | 0 | 0 |

Of the 281 fixed faults, 272 were rejected: 214 by the header check that every
lump lies inside the file, the rest by the lump loaders' own size checks, the
identifier and version checks and, for QL, the advertisement lump's shape.
Nine loaded: the lightmap and light-grid lumps made one byte too long, which
their loaders tolerate, under all four profiles, and Quake Live accepting a
version-46 header, which is Quake III's own format. Of the 128 seeded
corruptions, 67 loaded, 24 faulted, and 37 were rejected by range checks and
limits: shader numbers (11), triangle indices (7), hunk exhaustion (7), the
grid guard below (3), surface types, shader lookups and fog brushes (2 each),
and one each for entity bounds, a patch's vertex range and its size.

### The crash, fixed

At `3089df2`, the first Q3 run crashed once in 101 variants: the ninth seeded
corruption of `q3dm1`'s draw-vertex lump (lump 10) killed the client with an
access violation (0xC0000005) 28 seconds into loading. Under `cdb`, the
dedicated server faulted in `CM_SubdivideGridColumns`: a patch whose control
points lay far off its curve kept gaining columns until it wrote past the
129-column grid on the stack. None of the id Tech 3, Quake3e, FnQuake3, iortcw
and OpenJK sources limit that growth. `5ba3561` refuses such a patch with
`ERR_DROP` before it writes. On the fixed build that variant is rejected with
`CM_SubdivideGridColumns: subdivided grid is > MAX_GRID_SIZE`, and so are
QL's and RTCW multiplayer's own draw-vertex corruptions (seeded variants 25
and 24), which reach the same loop. `tests/patch_grid_bounds_tests.py` holds
the case.

### The caught faults, open

Every fault reads through an index that no loader checks. Each address was
symbolized against the build's own PDBs; reproductions under `cdb` also gave
full stacks for `R_SetParent`, `R_PointInLeaf`, `R_AddWorldSurface`, both
`Hunk_Alloc` callers and `CM_BoundBrush`.

| Damaged lump | Sites | Unchecked |
| --- | --- | --- |
| 3, nodes | `R_SetParent`; `R_PointInLeaf` from the surface-light proxies `RE_LoadWorldMap` builds; `BoxOnPlaneSide` and `CM_TraceThroughLeaf` in collision | a node's plane number and children, in both `R_LoadNodesAndLeafs` and `CMod_LoadNodes` |
| 4, leafs | `memset` in `Hunk_Alloc`, from `CMod_LoadLeafs` | leaf area numbers: an area near 0x12000000 makes the area array's size pass 2 GB and turn negative, and `Hunk_Alloc`, unlike `Hunk_AllocateTempMemory`, accepts a negative size |
| 5, leaf surfaces | `CM_TraceThroughLeaf`; `R_AddWorldSurface`, `R_CullSurface` | leaf-surface values against the surface count, in collision and in the renderer |
| 7, models | `memset` in `Hunk_Alloc`, from `CMod_LoadSubmodels` | a submodel's brush and surface counts, which reach `Hunk_Alloc` the same way |
| 8, brushes | `CM_BoundBrush` in `CMod_LoadBrushes` | a brush's first side and side count |
| 13, surfaces | `ParseFace` | a face's vertex range against the vertex lump |

The exception filter turns each fault into `ERR_DROP`, so the client survived
every one. That is not a check: the access has already happened, and on a
platform without such a filter it ends the process. Which consumer reaches
bad data first also varies, so one variant can fault in collision in one run
and in the renderer in another. These are inherited: Quake3e's collision
loader lacks the same checks.

## What this does not show

- One map per profile. Enemy Territory, Elite Force, the Jedi titles, the
  Ritual titles, Medal of Honor and Q3Test load other formats and are not
  part of this probe.
- `loaded` means only that the level ran 120 frames and the client quit; a
  damaged lightmap, light grid or visibility lump that loads is not shown to be
  drawn correctly.
- Only the client and only the GLx renderer ran; dedicated servers and the
  Vulkan and RTX renderers load the same lumps through their own paths.
- Windows only.

## Identity

- Fixed build: `3089df2` with the grid guard of `5ba3561`, built from that
  working tree. Client `meson/build/win32-dev/fntech3.exe`, SHA-256
  `d3ff7129bccb2fdfa1c21540ec459181635fa4ada6c766bcbcddba23ca6b1bdc`; GLx
  renderer `fnql_glx_x86.dll`, SHA-256
  `7204d5039b375bcdc739c14f1ba0d266e3c21d43c1c353f020956831496b7291`. MSVC
  19.44 x86, debugoptimized, strict warnings.
- Crashing build: `3089df2`, client SHA-256
  `b9531242c0d971d1528f1b80a9d59666d2fab48061e6b8d780cc071fe7cba527`, the build
  of the [release-profile record](2026-09-26-release-profiles.md).
- Windows 11 x64 host. The fixed-build runs took 21:11 to 21:37 UTC; the
  crashing run 20:48 to 20:54 UTC.
- Installations: `C:\Program Files (x86)\Steam\steamapps\common\Quake 3 Arena`,
  `E:\SteamLibrary\steamapps\common\Quake Live` and
  `E:\SteamLibrary\steamapps\common\Return to Castle Wolfenstein`, read only.
