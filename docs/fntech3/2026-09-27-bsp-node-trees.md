# A node-tree check in every map loader

The [index-check record](2026-09-27-bsp-index-checks.md) left one way for a
map's nodes to defeat the loaders with every index inside its table: a child
that leads back to its own node or to one above it. No descent that reaches
such a child ends. The collision code's point and box queries and its traces,
which a server runs as soon as the level's entities spawn, go round it until
they hang or their stack runs out, and each renderer's `R_SetParent`, which
recurses down the whole tree as the world loads, recurses until its stack runs
out. A server can send a client such a map.
The collision loader and the three renderers' world loaders now walk a map's
nodes once from node 0, without recursion, before anything else follows them,
and refuse the map if the walk reaches a node twice or goes more than 2048
nodes deep. Every tree of every installed game was measured first: each one
reaches every node once and none is deeper than 137 nodes, so the rule refuses
none of them, and no installed map loads differently in any renderer. Three
new damaged variants of each of four stock maps carry such trees: without the
walk the two cycles crashed every client and the share hung RTCW's two, and
with it all twelve are refused as the map's collision loads. The
[machine-readable record](2026-09-27-bsp-node-trees.json) holds every
measurement, load and variant.

## The rule

`BSP_NodeTreeFault` in
[`bsp_index_checks.h`](../../code/qcommon/bsp_index_checks.h) walks the tree
as every descent does, from node 0, keeping the path it is on and a bit for
each node it has reached. That scratch, at most 2048 steps of path and a bit a
node, comes from the temporary hunk and is released before any refusal. It
refuses three shapes:

| The walk meets | What it does to a descent | The loaders' message |
| --- | --- | --- |
| a child that is its own node or a node above it | no descent that takes the child ends | `child C of node N leads back to that node or one above it` |
| a child already reached below another node | a descent of both walks that subtree twice, and twice again for each such share below it | `child C of node N is already below another node` |
| a path through more than 2048 nodes | every descent recurses, or keeps a stack, that deep | `child C of node N is more than 2048 nodes deep` |

It leaves alone two things no installed map has, because no descent is hurt by
them: a leaf that two nodes name, since a leaf ends every descent (the IHV
translator's own check allows these too), and a node that node 0 does not
reach, since no descent follows one. Nor does the order of the nodes' numbers
matter: every installed tree numbers each child after its parent, but a tree
numbered otherwise loads. A child outside its table is refused, as before, by
the index checks, which run first; the walk refuses one as well.

The collision loader walks the tree at the end of `CMod_LoadNodes`
([`cm_load.c`](../../code/qcommon/cm_load.c)), once every child is known to be
the map's, and each renderer's `R_LoadNodesAndLeafs` walks it just before
`R_SetParent`. Both refuse with `ERR_DROP`, as the index checks do. Every
format the loaders take reaches these two places: the IBSP 46 and 47 of Quake
III, Quake Live, RTCW, Enemy Territory and Elite Force as they are, and the IHV's
IBSP 43, Jedi Academy's and Jedi Outcast's RBSP, Alice's and F.A.K.K.2's FAKK
(both the collision loader's translation and the renderers'), Medal of Honor's
2015 v19 and Q3Test's IBSP 44 and 45 through their translators. Medal of
Honor's world rebuild ([`tr_mohaa_world.c`](../../code/renderercommon/tr_mohaa_world.c))
copies the translated nodes unchanged, and the point lookup it makes before the
renderers' loaders run already stops after 4096 steps. The IHV translator's
`BSP43_NodeTreeValid`, which refuses cycles, shared subtrees and trees deeper
than 2048 anywhere in an IBSP 43 map before translating it, stays.

### Why these three

A cycle check alone would have been the rule had any installed map shared a
subtree, since refusing shares would then have refused real maps. None does.
A share that is not a cycle lets every descent end, but a walk of the whole
tree doubles at each one: in the damaged variant below that shares the next
subtree at every node down the deepest path of a stock map, such a walk visits
2^32 − 1 nodes of `q3dm1` and 2^53 − 1 of `mp_beach`. `R_SetParent` walks the
whole tree on every load, and a box straddling the shared planes walks it in
`CM_BoxLeafnums_r`. So shares are refused, as the IHV translator refuses them.

The limit of 2048 is the IHV translator's, and its model lighting's stack
holds as many; it is fifteen times the deepest installed tree. Each descent
below recurses once a level, and at 2048 levels the deepest of them takes
152 KB of the 1 MB of stack that both executables reserve for their main
thread (`SizeOfStackReserve` 1,048,576). Measured from the before build's
program databases (each frame, its saved registers, return address and
arguments):

| Descent | What runs it | Stack a level | At 2048 levels |
| --- | --- | ---: | ---: |
| `CM_TraceThroughTree` | collision traces | 76 bytes | 152 KB |
| `R_BoxSurfaces_r`, `R_CollectDlightShadowWorldCasters_r` | marks; dynamic-light shadow casters | 48 bytes | 96 KB |
| `R_RecursiveWorldNode` | the world each frame | 44 bytes GLx, 28 Vulkan and RTX | 88 KB |
| `R_RTCWMarkCollect` | RTCW's marks | 36 bytes | 72 KB |
| `CM_BoxLeafnums_r`, `CM_HeadnodeVisible`, `R_MOHAAMarkCollect` | leaf lists; IHV visibility; Medal of Honor's marks | 28 bytes | 56 KB |
| `R_RecursiveLightNode` | dynamic lights, Vulkan and RTX | 20 bytes | 40 KB |
| `R_SetParent` | the world as it loads | 16 bytes | 32 KB |

`CM_PointLeafnum_r` and `R_PointInLeaf` loop rather than recurse, so a cycle
would hang them instead.

## What the installed maps carry

[`scripts/audit_bsp_indexes.py`](../../scripts/audit_bsp_indexes.py) now
measures each tree as the loaders present it, through the engine's own
translators, and runs the loaders' own `BSP_NodeTreeFault`, compiled into its
shim, over each. Of 607 maps and 673 translations:

| Title | Maps | Translations | Deepest tree | Median depth | Most nodes | Nodes reached twice | Nodes not reached | Leafs named twice | Walk faults |
| --- | ---: | ---: | --- | ---: | --- | ---: | ---: | ---: | ---: |
| Quake III | 69 | 69 | 137, `grabtex` | 38 | 8,689, `mpteam5` | 0 | 0 | 0 | 0 |
| Quake Live | 149 | 149 | 49, `overgrowth` | 29 | 9,662, `ragnarok` | 0 | 0 | 0 | 0 |
| RTCW | 47 | 47 | 67, `crypt2` | 42 | 7,356, `mp_destruction` | 0 | 0 | 0 | 0 |
| Enemy Territory | 6 | 6 | 58, `radar` | 43 | 7,156, `radar` | 0 | 0 | 0 | 0 |
| Elite Force | 99 | 99 | 96, `voy16` | 37 | 9,503, `scav2` | 0 | 0 | 0 | 0 |
| Quake III IHV | 4 | 4 | 34, `km_portal` | 21 | 3,029, `km_portal` | 0 | 0 | 0 | 0 |
| Jedi Academy | 57 | 57 | 56, `mp/siege_hoth` | 31 | 7,399, `taspir1` | 0 | 0 | 0 | 0 |
| Jedi Outcast | 41 | 41 | 90, `yavin_canyon` | 39 | 9,212, `artus_detention` | 0 | 0 | 0 | 0 |
| Alice | 36 | 72 | 72, `centipede1` | 49 | 8,328, `garden2` | 0 | 0 | 0 | 0 |
| F.A.K.K.2 | 30 | 60 | 80, `swamp2` | 47 | 6,374, `homes2evil` | 0 | 0 | 0 | 0 |
| Medal of Honor | 54 | 54 | 76, `m4l1` | 29 | 5,481, `m3l1a` | 0 | 0 | 0 | 0 |
| Q3Test | 15 | 15 | 43, `q3test1` (1.02) | 38 | 6,215, `q3test1` (1.02) | 0 | 0 | 0 | 0 |

Depth counts the nodes on the longest path from node 0 to a leaf. No tree has
a cycle, and every walk of a whole tree visits exactly as many nodes as the
tree has. Every translation also has leafs that no node names, and in every
one they number exactly as many as the map's models, the world's included:
that is the compiler's reserved leaf 0 and one leaf for each inline model,
which q3map, and the compilers descended from it, emit for the model and never
link into the world's tree (the counts are observed; which leafs they are is
inferred from q3map's source). They are never walked, and the rule leaves them
alone.

## Every installed map, before and after

[`scripts/probe_retail_map_loads.py`](../../scripts/probe_retail_map_loads.py)
loaded every installed map under its own profile, as in the index-check
record: in GLx all 607, in Vulkan and RTX the 505 of the profiles with a
client, before the change and after it, on the same build folder, one sweep at
a time, three maps at once.

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
| `jka-sp` | 34 | 34 rejected | 34 rejected | -- | -- | -- | -- | 0 |
| `jka-mp` | 23 | 21 loaded, 2 rejected | 21 loaded, 2 rejected | -- | -- | -- | -- | 0 |
| `jk2-sp` | 26 | 26 rejected | 26 rejected | -- | -- | -- | -- | 0 |
| `jk2-mp` | 15 | 15 loaded | 15 loaded | -- | -- | -- | -- | 0 |
| `alice` | 36 | 35 loaded, 1 rejected | 35 loaded, 1 rejected | 35 loaded, 1 rejected | 35 loaded, 1 rejected | 35 loaded, 1 rejected | 35 loaded, 1 rejected | 0 |
| `fakk2` | 30 | 30 loaded | 30 loaded | 30 loaded | 30 loaded | 30 loaded | 30 loaded | 0 |
| `mohaa` | 54 | 54 loaded | 54 loaded | 54 loaded | 54 loaded | 54 loaded | 54 loaded | 0 |
| `q3test` | 15 | 15 loaded | 15 loaded | 15 loaded | 15 loaded | 15 loaded | 15 loaded | 0 |

`--compare` finds no map whose outcome, first engine error or count of faces,
meshes, triangle surfaces and flares differs between the builds, in any
renderer. The loads that do not end in `loaded` end as they did in the
index-check record, for reasons outside the loaders: the Jedi single-player
modes' module boundary, Jedi Academy's siege import `G_G2_CLEANMODELS`,
Alice's `wforest`, whose game stops at a negative area-portal count (both
builds predate `95dd1cc`, which lets it load), and RTCW's `norway`, whose
script names an entity it cannot find. Every before sweep also gives, map for
map, the same outcome, first error and counts as that record's after sweeps
did; its build differed from this one's before build only in the wording of a
comment.

## The damaged maps again

[`scripts/probe_bsp_faults.py`](../../scripts/probe_bsp_faults.py) gains
three variants built from each stock map's own tree, each keeping every index
inside its table. `tree-cycle` points the last node's front child at node 0
and `tree-self` at the last node itself; `tree-shared-path` makes each node
down the deepest path from node 0 name the next node on that path with both
children, so every subtree below is shared and a walk of the whole tree visits
2^32 − 1 nodes of `q3dm1`, 2^28 − 1 of `campgrounds`, 2^48 − 1 of `escape1`
and 2^53 − 1 of `mp_beach`. Every other variant is byte for byte the
index-check record's.

Without the walk, the before build ran the three alone:

| Profile | Map | `tree-cycle` | `tree-self` | `tree-shared-path` |
| --- | --- | --- | --- | --- |
| `q3` | `q3dm1` | crashed after 15.3 s | crashed after 14.0 s | loaded in 10.8 s |
| `ql` | `campgrounds` | crashed after 16.2 s | crashed after 17.6 s | loaded in 4.9 s |
| `rtcw-sp` | `escape1` | crashed after 17.7 s | crashed after 17.7 s | hung, stopped at 90 s |
| `rtcw-mp` | `mp_beach` | crashed after 15.4 s | crashed after 17.6 s | hung, stopped at 90 s |

All eight crashes are an access violation (`0xC0000005`) at one address, the
stack probe `_chkstk` below `Com_sprintf` in the engine's exception filter:
the stack had already run out when the filter tried to report the fault. The
first one's dump (`q3`, `tree-cycle`) holds 1,281 return addresses into
`CM_TraceThroughTree` on that stack, so a collision trace went round the cycle
until the stack overflowed; each crashing run's console stops after the
server's game initialisation, as the level's first frames run and before the
client loads the world. The shared path lets each descent end, but late: the
damaged copies of `q3dm1` that load took a median of 4.4 s in the index-check
record's run, this one 10.8 s, and both RTCW maps loaded their worlds and then
did not finish the probe's 120 frames within its 90 s. `campgrounds`' 2^28 − 1
cost nothing visible.

With the walk, every variant ran again:

| Profile | Map | Variants | Loaded | Rejected | Faulted | Crashed | Hung | In the index-check record |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| `q3` | `q3dm1` | 104 | 17 | 87 | 0 | 0 | 0 | 101: 17 loaded, 84 rejected |
| `ql` | `campgrounds` | 108 | 18 | 90 | 0 | 0 | 0 | 105: 18 loaded, 87 rejected |
| `rtcw-sp` | `escape1` | 105 | 17 | 88 | 0 | 0 | 0 | 102: 17 loaded, 85 rejected |
| `rtcw-mp` | `mp_beach` | 104 | 17 | 87 | 0 | 0 | 0 | 101: 17 loaded, 84 rejected |

The twelve tree variants are refused as the map's collision loads, each by the
fault it carries: `q3dm1`'s by `child 0 of node 2106 leads back to that node
or one above it`, `child 2106 of node 2106 leads back to that node or one
above it` and `child 540 of node 539 is already below another node`, and the
other maps' likewise, each after 3 to 4 s. Every other variant ends as it did
in the index-check record, with the same first error. Three of its fuzzed node
lumps (`campgrounds`' 002 and 018, `mp_beach`'s 017) share a subtree too, but
the index checks, which run first in the same loop, refuse each of them for a
bad child or plane, as before.

## Tests

[`tests/bsp_index_bounds_tests.py`](../../tests/bsp_index_bounds_tests.py)
now also compiles `BSP_NodeTreeFault` alone and walks it over synthetic trees:
cycles through a node itself, node 0 and a node deep in a line of 1,500; a
share of both children, of two nodes and of 64 nodes in a line that each name
the next twice (2^64 paths, found at the first share); a line of 2048 nodes,
which loads, and of 2049, which does not, down either child and with a cycle
closing past the limit; children outside their tables; and a full tree of
65,535 nodes, walked whole, with a share and a cycle at its last node. The
scratch it is given is exactly the size it asks for, filled with a pattern it
must clear, with a guard after it. Leafs two nodes name, nodes node 0 does not
reach (one naming itself and node 0) and a tree numbered children first load.
The collision loader and each renderer's loader, compiled from their own
source, refuse a child naming itself, one naming node 0, a child both of a
node's sides name and a line of 2049 nodes with the messages above, and load a
shared leaf, a line of 2048, an unreached node naming itself and node 0 (which
gets no parent in the renderers) and a tree numbered children first, with
their parents set as before; each releases its scratch before refusing.

Each of 18 mutations fails the tests: the check skipped in the collision
loader or in any one renderer (the renderers then die in `R_SetParent` of a
stack overflow, `0xC00000FD`), its scratch kept in any of the four, and in the
walk itself no test for a node reached before, cycles reported as shares, the
limit one node deeper or shallower, no test for a child outside its table,
node 0 left unmarked, the scratch not cleared, the second child skipped, the
scratch one word short, and a node never marked as reached.

The whole Python suite passes on `2d999a9` with this change (2,320 tests, 129
skipped), and `meson test` passes on the after build (175 native tests, two
skipped, besides the wrapper that runs the Python suite).

## What this does not show

- The limit of 2048 is a choice, not the point at which a descent fails: a
  tree between 2048 nodes deep and the depth at which the stack would run out
  loaded before and is refused now. No installed tree is deeper than 137, and
  the IHV translator already held IBSP 43 maps to 2048.
- A share whose repeated walks cost little, such as one node naming the same
  small subtree twice, loaded before and is refused now; no installed map has
  one.
- A listen server loads a map's collision before the client's renderer loads
  its world, so every damaged tree was refused by the collision loader; the
  renderers' walks refused damaged trees only in the unit tests. In the sweeps
  each renderer's walk ran over every installed map. The damaged maps ran
  under GLx only, as in the earlier records.
- One crash dump of the eight (`q3`'s `tree-cycle`) kept the overflowed stack
  that names `CM_TraceThroughTree`; the other seven crashed at the same
  address in the exception filter, and the two of their dumps read the same
  way kept no frames there.
- `loaded` means the level ran and the client quit; it does not show that a
  map is drawn correctly.
- The walk covers the BSP's node tree. Bots' navigation files (`.aas`) carry
  trees of their own, loaded by the botlib from the server's own files; they
  are not checked here, but in the
  [navigation record](2026-09-28-aas-index-checks.md).
- Windows only for the runs; the header was also compiled with i686 GCC 13
  and Clang, as C and C++, under `-Wall -Wextra -Werror`.

## Identity

- Build without the walk: `2d999a9`, a clean tree, rebuilt in
  `meson/build/win32-dev` with every development gate on (as VS Code's "setup
  all games" task configures it) at 20:04 UTC, after another session had
  relinked its renderers during a first attempt at the before sweep, which was
  discarded. Client `fntech3.exe` SHA-256
  `da4e26d7fd44226714f57c20534ca1dfc674d8486e1a6f1aa0581f2084557eb4`, server
  `fntech3.ded.exe` `09632eff02d6b81bd1131d9f5613ea686be9d83307023a2a5d0662e6843a5444`,
  renderers `fnql_glx_x86.dll`
  `0c62e45ef71a692b3639bbda1f3a49fec7b44137c2f97e2a1cf5c5782ed6cfb6`,
  `fnql_vk_x86.dll` `dcde4219ec78dbdc981fd8185b67bb7cc281acdfe7ba1bbc42d865afc69cc0ba`
  and `fnql_rtx_x86.dll`
  `27b876533e7cdc516db78ab58a8d0d32487c7648e458eec6986042c4292952d4`.
- Build with the walk: `2d999a9` with this change, built in the same folder at
  21:05 UTC from the working tree, which held nothing else. Client
  `39263d585c19fb76a19b3032bcb45bf84d3a49147eb73c33e4da1833d2b0fd62`, server
  `8562479061790fc0e6ba48e8192063b86a0df4ad04a70e56a3bd7f803bcec8bf`,
  renderers `de450e3b14c52de44e92473840423bc5d9d314d4b33bdee86c430dc05cac7c76`
  (GLx), `ef5c4e19544b9b048c969d852f116758019f47ec1a9d35637afad19d23fa992b`
  (Vulkan) and `e9568c37493908e5252a726470d3f2e2caf97edaa766b3c3a72944306bbd440c`
  (RTX). Every sweep hashed its binaries at its start and end and found them
  unchanged. `95dd1cc`, another session's, landed during the after runs; the
  after Vulkan and RTX sweeps and the damaged maps record it as the commit
  they ran at, but neither build contains it. This commit sits on it.
- MSVC 19.44 x86, debugoptimized, strict warnings. Windows 11 x64 host, NVIDIA
  GeForce RTX 4060 Laptop GPU.
- Runs, 2026-09-27 UTC: without the walk, GLx 20:04-20:26, Vulkan
  20:26-20:43, RTX 20:43-20:57 and the tree variants 20:57-21:03; with it,
  GLx 21:18-21:44, Vulkan 21:44-22:02, RTX 22:02-22:19 and the damaged maps
  22:19-22:47. The tree audit ran at 20:21 against the same installations.
- Installations, read only: `C:\Program Files (x86)\Steam\steamapps\common\Quake 3 Arena`,
  `E:\SteamLibrary\steamapps\common\Quake Live`,
  `E:\SteamLibrary\steamapps\common\Return to Castle Wolfenstein`,
  `E:\SteamLibrary\steamapps\common\Wolfenstein Enemy Territory`,
  `E:\Games\Star Trek Voyager Elite Force`, `E:\Games\Quake 3 IHV`,
  `E:\Games\Jedi Academy`, `E:\Games\Jedi Outcast`, `E:\Games\Alice`,
  `E:\Games\Medal of Honor Allied Assault`; F.A.K.K.2 and Q3Test from the
  layouts under `.tmp/fakk2-install/game` and `.tmp/q3test/install`.
