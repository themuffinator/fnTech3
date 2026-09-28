# Bot navigation files checked before the botlib follows them

The [node-tree record](2026-09-27-bsp-node-trees.md) left the bots'
navigation files unchecked: the botlib's Quake III loader, which reads the
`.aas` files of Quake III, Team Arena, Quake Live and Elite Force, swapped a
file's bytes and handed its tables to every caller as they were. Nothing
checked that a node's plane or children, an area's faces, a face's plane and
edges, an edge's vertexes, an area's reachabilities and cluster, a portal's
area and clusters, or a cluster's portals stayed inside the tables they name.
The point query walks node children from node 1 with its bounds checks
compiled out, so a child out of range read past the nodes and a cycle never
returned; routing indexes its caches by the numbers clusters give their
areas; and a negative lump length reached the hunk as a wrapped size. On
damaged copies of stock files, a node naming itself hung the Quake III,
Quake Live and Elite Force servers, a child or plane out of range faulted
them, and a negative lump dropped the map. RTCW's loader already refused most
of this in its own reader, but not through the dummy records it skipped
(face 0, area 0, portal 0), a face index of 0, or a cluster listing a portal
that is not on its border; a copy of `escape1` whose portal 0 names a cluster
past the table faulted its client.

Both loaders now check every number the botlib follows from one table into
another before the file is marked loaded, and walk the node tree once from
node 1, refusing a node reached twice. Quake III's loader also refuses a lump
outside the file before allocating any, and the reachability ranges that
would overflow its routing tables. A refused file is reported as the botlib
reports any navigation load error, and the map runs without bots: every one
of the damaged copies above now is. Before choosing a rule, every navigation
file of every installed game, 344 of them, was measured offline; every one
passes every rule and loads through both loaders, and loaded again with bots
on the stable `meson/build/win32-dev` binaries before and after the change,
none of the 275 maps with navigation changed its navigation, routing, bot or
outcome. The [machine-readable record](2026-09-28-aas-index-checks.json)
holds every measurement, load and variant.

## The rules

[`code/botlib/aas_index_checks.hpp`](../../code/botlib/aas_index_checks.hpp)
holds them, as `AAS_IndexFault`, which the Quake III loader
([`be_aas_file.cpp`](../../code/botlib/be_aas_file.cpp)) calls after swapping
a file's tables and RTCW's ([`rtcw_aas_loader.hpp`](../../code/botlib/rtcw_aas_loader.hpp))
after decoding its version 8 records, each before setting `aasworld.loaded`.
Each check is written so that no sum overflows. What each one keeps the
botlib from doing:

| Rule | Without it | The loaders' message |
| --- | --- | --- |
| Quake III only: every lump lies inside the file | a negative length reached the hunk as a wrapped size, which `Hunk_Alloc` refuses by dropping the whole map; an oversized one allocated what the file never holds | `lump N is outside the file` |
| Quake III only: the file holds its whole header | the rest of the header was left unset | `is not an AAS file` |
| node 1 exists | every point query, trace and entity link starts there | `has no root node` |
| planes come in pairs | the traces turn a hit plane round (`planenum ^ 1`) and the ladder test takes a face's back plane | `plane N has no opposite plane` |
| as many area settings as areas | an area's number indexes both tables | `has N area settings for M areas` |
| each node's plane, from node 1 | every descent reads it | `node N has a bad plane` |
| each node's children: a node, an area as `-area`, or 0 for solid | every descent follows them into the nodes, the area settings, the areas and the entity links | `node N has a bad child` |
| from node 1, no node is reached twice | a cycle never lets the point query, a trace or an entity link end; a subtree two nodes share is walked twice by an entity link, and twice again at each share below | `child C of node N leads back to that node or one above it`, `child C of node N is already below another node` |
| each area's face range, area 0's too | the trace-end face, the ladder test, alternative routing and ET's routing walk an area's faces | `area N has a bad face range` |
| each face index names a face, never face 0 | the same walks follow `abs(index)` into the faces | `face index N names no face` |
| each face's plane, edge range and two areas, from face 1 | the face tests read the plane and walk the edges; alternative routing and clustering cross into the areas | `face N has a bad plane`, `face N has a bad edge range`, `face N has a bad area` |
| each edge index names an edge, never edge 0 | the face walks follow `abs(index)` into the edges | `edge index N names no edge` |
| each edge's two vertexes, from edge 1 | the face walks read both | `edge N has a bad vertex` |
| each cluster's counts: areas and reachability areas not negative, the second no more than the first | routing sizes each cluster's caches and travel times by them | `cluster N has bad area counts` |
| each cluster's portal range | routing walks a cluster's portals through the portal index | `cluster N has a bad portal range` |
| each portal index names a portal | the same walk | `portal index N names no portal` |
| each portal's area and two clusters, portal 0's too | routing reads the portal area's settings and travel times, and the clusters' caches | `portal N has a bad area`, `portal N has a bad cluster` |
| each portal's number in each cluster it joins: not negative, and one of that cluster's numbers (portal 0 joins none) | routing indexes a cluster's caches and travel times by it, bounding it only from above | `portal N has a bad number in cluster C` |
| each area's reachability range, area 0's too | routing lays out rows for every area's reachabilities and reverses them | `area N has a bad reachability range` |
| each area's cluster: its own (positive) or the portal it is (negative) | routing indexes the clusters, or the portals by `-cluster` | `area N has a bad cluster` |
| each area's number in its cluster | routing indexes the cluster's caches by it | `area N has a bad number in cluster C` |
| an area that is portal P is P's area, and P's area is portal P | routing hands a portal's two numbers to the area it names | `area N is portal P, which is another area`, `portal P is area A, which is not that portal` |
| an area with reachabilities, but area 0, is in a cluster or is a portal, when the file has clusters | Quake III's routing takes such an area through cluster 0 and portal 0, whose caches have no room for it | `area N has reachabilities but no cluster` |
| a cluster lists only portals on its border | routing numbers each listed portal inside the listing cluster | `cluster C lists portal P, which is not on its border` |
| each reachability's area, reachability 0's too | routing and movement index the areas and their settings by it | `reachability N leads to a bad area` |
| the clusters hold no more numbers than one an area and two a portal | routing lays out a cache pointer for each number | `clusters hold more numbers than N areas and P portals can take` |
| each cluster number names one area | a cache and a reachability offset kept for one area are read for the other, whose offset then runs past its reachabilities, up to 127 records past the table | `area N shares number K in cluster C with another area` |
| Quake III only: the reversed links fit their table | `AAS_CreateReversedReachability` writes one link for each of an area's first 128 reachabilities into a table of one link a reachability, which overlapping ranges overflowed | `areas lay out more reachabilities than the file's N` |
| Quake III only: the travel times take at most 64 MB | `AAS_CalculateAreaTravelTimes` sizes the table in an `int`, which overlapping ranges can wrap | `routing would need more than 67108864 bytes of travel times` |

The Quake III loader prints the message after the file's name, as it prints
its other load errors (`Fatal: maps/q3dm1.aas: child 1 of node 1 leads back
to that node or one above it`), and returns `BLERR_CANNOTREADAASLUMP`; RTCW's
prints it as its reader prints its own (`Error: Invalid RTCW AAS
maps/escape1_b0.aas: portal 0 has a bad cluster`). Either frees the file's
tables. The map goes on without navigation, as it does when the file is
missing: the game refuses a bot (`Fatal: AAS not initialized`, then `Sarge
BotAISetupClient failed`), and RTCW's single-player cast has no hull to walk.

### Which records are dummies

Every table's record 0 is a dummy, but not every dummy is left alone. Node 0
stands for solid, which a child names as 0, so no descent reads it, and it is
not checked. Face 0 and edge 0 cannot be named, because the face and edge
indexes are signed and 0 carries no sign; the rule that no index names them is
what lets their contents be ignored. That matters: 236 of the installed files
(all of Quake Live's and Elite Force's, and 54 of Quake III's) are compiled
without vertexes, and their dummy edge names vertex 0 of an empty table. Area
0, portal 0, cluster 0 and reachability 0 are read -- every routing table is
laid out from 0, and an area outside every cluster is routed through portal
0 -- so they are checked like the rest, except that portal 0 joins no
cluster, so its numbers need only not be negative. RTCW's own reader checked
faces, areas, portals and clusters from 1; these are the dummies it let
through.

### What is left alone

- **No depth limit.** Every descent of the tree keeps its own bounded stack
  or none: the point query loops, and each trace and entity link keeps a
  stack of 127 or 128 entries that stops with an error when full. No
  recursion can overflow, so a deep tree is not refused; the installed trees
  go 554 nodes deep (Quake Live's `cliffside`), as the
  [RTCW bootstrap record](2026-09-15-rtcw-botlib-bootstrap.md) already found
  `crypt1`'s trees deeper than the traces' stacks. Quake Live's `arcanecitadel`
  fills the entity link's stack in both builds (`AAS_LinkEntity: stack
  overflow`), which is where that stack was written to stop.
- **Areas several leaves name.** The tree's planes cut an area into pieces,
  and the entity link expects to meet an area twice.
- **Nodes node 1 does not reach.** No descent follows one; their numbers are
  still checked, as the BSP loaders check every node's. Every installed tree
  reaches every node.
- **The order of the nodes' numbers**, and **overlapping reachability
  ranges**: RTCW gives area 0 area 1's reachabilities in five installed files.
  Only Quake III's routing, which lays out a link for every reachability of
  every range, needs them to fit.
- **A file with no clusters.** The botlib builds clusters for it as it loads
  (`AAS_InitClustering`), resetting every area's cluster first, so an area
  with reachabilities and no cluster is refused only in a file that has
  clusters.
- **Reachability face and edge numbers**, which jump pads, elevators and
  func_bobbing reuse for a model, a height and velocities; the model number
  reaches `CM_InlineModel`, which already refuses one out of range.
- **Anything RTCW's reader already refused**, which it still does first:
  finite coordinates and bounds, `areanum` matching each area, and its
  routing's own table limits.

## What the installed files carry

[`scripts/audit_aas_indexes.py`](../../scripts/audit_aas_indexes.py) reads
every navigation file in every pak and loose, including the 36 that later
paks supersede, measures every number above against its table, walks each
tree from node 1, and runs the loaders' own checks, compiled with Clang, over
each file twice: `AAS_IndexFault` alone, and the whole `AAS_LoadAASFile` of
the botlib fixtures under the file's own profile. Enemy Territory ships no
navigation files, nor do the IHV test, the Jedi games, Alice, F.A.K.K.2,
Medal of Honor and Q3Test.

| Title | Files (served) | Versions | Deepest tree | Median depth | Most nodes | Most areas | Most reachabilities | Most reachabilities of one area | Most links into one area | Largest travel-time table |
| --- | ---: | --- | --- | ---: | --- | --- | --- | ---: | --- | --- |
| Quake III and Team Arena | 92 (62) | 88 v5, 4 v4 | 350, `mpteam8` | 66 | 81,243, `mpterra2` | 25,037, `awoken_b2` | 49,712, `mpterra2` | 55 | 134, `mpq3tourney6` | 1,056,244 bytes, `mpterra2` |
| Quake Live | 149 (149) | v5 | 554, `cliffside` | 78 | 85,900, `zen` | 25,993, `zen` | 38,564, `overgrowth` | 113 | 198, `blackcathedral` | 885,288 bytes, `overgrowth` |
| RTCW | 70 (64) | v8 | 161, `crypt1_b0` | 70 | 24,872, `forest_b0` | 7,383, `trainyard_b0` | 15,714, `forest_b0` | 75 | 43, `dam_b0` | 285,944 bytes, `forest_b0` |
| Elite Force | 33 (33) | v5 | 146, `hm_for1` | 55 | 85,273, `ctf_stasis` | 18,723, `ctf_for1` | 15,521, `hm_temple` | 42 | 355, `hm_cam` | 333,608 bytes, `hm_temple` |

Every file passes every rule, and both of the loaders' own passes load every
file. No tree reaches a node twice, leaves a node unreached or loops; every
lump lies inside its file, in order, with no partial record; node 0, area 0,
face 0, edge 0, portal 0 and cluster 0 are zeros in every file, and area 0's
settings name an empty range at reachability 1, but in RTCW's five files,
where they name area 1's first reachabilities; no face or edge index is 0; no
reachability but the dummy leads to area 0, again but in those five; and
every number a cluster gives is given once. Depth counts
the nodes on the longest path from node 1 to a leaf; the travel-time sizes
are for x86, and 1,530,632 bytes is the largest with 64-bit pointers. The 236
files without vertexes also carry a single dummy face and no face indexes, so
every area's face range is empty.

## Every installed map, before and after

[`scripts/probe_retail_bot_loads.py`](../../scripts/probe_retail_bot_loads.py)
loaded every installed map that has navigation, 275 of them, with the bot
library on, under its own profile, on the stable `meson/build/win32-dev`
binaries: the dedicated server for Quake III, Team Arena, Quake Live and
Elite Force Holomatch, whose game adds one bot (`addbot sarge`, `xaero`,
`anarki`, `alexandria`), and the client for RTCW's single player, whose cast
walks both hulls. Each run recorded the files the botlib loaded (with RTCW's
area and reachability counts), every botlib error, whether routing finished
initialising, whether the bot was listed, and how the process ended. RTCW's
multiplayer maps ship no navigation; the retail game keeps its bots off.

| Profile | Maps | Before | After | Navigation loaded, before / after | Routing initialised, before / after | Bot listed, before / after | Changed |
| --- | ---: | --- | --- | --- | --- | --- | ---: |
| `q3` | 61 | 61 loaded | 61 loaded | 61 / 61 | 61 / 61 | 61 / 61 | 0 |
| `ql` | 149 | 149 loaded | 149 loaded | 149 / 149 | 149 / 149 | 148 / 148 | 0 |
| `stvef-mp` | 33 | 33 loaded | 33 loaded | 33 / 33 | 33 / 33 | 33 / 33 | 0 |
| `rtcw-sp` | 32 | 31 loaded, 1 rejected | 31 loaded, 1 rejected | 31 / 31 | 31 / 31 | -- | 0 |

`--compare` finds no map whose outcome, first engine error, navigation files
(with RTCW's area and reachability counts), botlib errors, routing or bot
differ between the builds. The exceptions are the same in both. RTCW's
`norway` stops on its own script (`G_Scripting: can't find entity with
"targetname" = "ice2_spot2"`) before its navigation loads, as in the
[index-check record](2026-09-27-bsp-index-checks.md); Quake Live's
`qztraining` refuses `addbot` in training; and `arcanecitadel`'s entity link
fills its stack, as above. The first before sweep ran Quake Live's
`qzpractice1` and `qzpractice2` as free-for-all, which its map pool does not
offer them in, so their level never started; both ran again before the
change as race, the one type offered, as the after sweep ran them, and the
comparison takes those runs.

## The damaged files at runtime

[`scripts/probe_aas_faults.py`](../../scripts/probe_aas_faults.py) loads four
stock maps over damaged copies of their navigation, placed in the probe's home
where the server finds them before the paks, with bots on as above. Each
variant changes one number of the stock file and keeps its header, so the map
checksum still matches: node 1's back child naming node 1 (`root-cycle`) or a
node 1,000 past the table (`child-outside`), node 1's plane at 16,777,216
(`plane-outside`), a node lump length of -12 (`lump-negative`), the first face
index naming face 0 (`face-index-zero`, only in RTCW's files, the others having
no face indexes) and portal 0 naming a cluster five past the table
(`portal-zero-cluster`); RTCW's two hulls are damaged alike. `intact` is the
stock file, copied: the copy in the home is the one the server loads.

| Variant | `q3dm1` before | after | `campgrounds` before | after | `hm_borg1` before | after | `escape1` before | after |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `intact` | loaded, bot | loaded, bot | loaded, bot | loaded, bot | loaded, bot | loaded, bot | loaded | loaded |
| `root-cycle` | hung (90 s) | refused, ran on | hung (90 s) | refused, ran on | hung (90 s) | refused, ran on | refused, ran on | refused, ran on |
| `child-outside` | faulted | refused, ran on | faulted | refused, ran on | faulted | refused, ran on | refused, ran on | refused, ran on |
| `plane-outside` | faulted | refused, ran on | faulted | refused, ran on | faulted | refused, ran on | refused, ran on | refused, ran on |
| `lump-negative` | map dropped | refused, ran on | map dropped | refused, ran on | map dropped | refused, ran on | refused, ran on | refused, ran on |
| `face-index-zero` | -- | -- | -- | -- | -- | -- | loaded | refused, ran on |
| `portal-zero-cluster` | loaded, bot | refused, ran on | loaded, bot | refused, ran on | loaded, bot | refused, ran on | faulted | refused, ran on |

Before the change, the root cycle hung each Quake III-lineage server until
the probe stopped it at 90 s: the point query goes round node 1 for ever for
any point behind its plane. The child and the plane out of range faulted each
server 2.3 to 3.0 s in, right after its navigation file loaded and before any
bot joined, as the first point queries ran: the engine's exception filter
caught access violations at `fntech3.ded.exe@3537b` and
`@35384`, `AAS_PointAreaNum+0x4b` and `+0x54` in the before build's program
database. The negative lump reached the hunk as a wrapped size, which
`Hunk_Alloc` refuses with `ERR_DROP` (`Hunk_Alloc: invalid size -7`), so the
whole map dropped. Portal 0's bad cluster loaded, harmless until the game
disables an area outside every cluster. On RTCW the reader refused the first
four already, and still does first, with its own words (`cyclic or shared node
tree`, `invalid geometry index`, `lump outside file`); but portal 0's bad
cluster faulted the client 6.7 s in, at `fntech3.exe@44bc0`,
`AAS_RemoveRoutingCacheInCluster+0x50`, reached from
`AAS_RemoveRoutingCacheUsingArea` when the game disabled an area outside
every cluster, and a face index of 0 loaded unnoticed. After the change every
damaged copy is refused as it loads, with the messages of the table above
(`node 1 has a bad child`, `node 1 has a bad plane`, `lump 10 is outside the
file`, `portal 0 has a bad cluster`, `face index 0 names no face`); each level
then ran its frames and quit normally, and each game refused its bot.

## Tests

[`tests/aas_index_checks_tests.py`](../../tests/aas_index_checks_tests.py)
compiles the checks and the loaders from their own source. The helpers bound
ranges and indexes at their edges and at `INT_MIN`/`INT_MAX`. The tree walk
runs over lines of 100,001 nodes, which load, and cycles through a node
itself, node 1 and nodes far up those lines; a share of both children, of two
nodes, and of 64 nodes in a line (2^63 paths, refused at the first share it
meets); children outside the tables; and a full tree of 65,535 nodes with a
share and a cycle at its last nodes. Leaves naming one area twice, unreached
nodes naming themselves and node 1, and a tree numbered children first load.
Every rule is then broken alone in a small valid file of two clusters joined
by a portal, and must be refused with its own message by both loaders' rule
sets, or only Quake III's; the cases that must load include the shape of the
installed files without vertexes, dummies holding anything, RTCW's area 0 with
reachabilities, a portal back onto its own cluster, a file with no clusters,
a hundred empty clusters, 200 reachabilities in one area with overlapping
ranges beside it, and travel times at the limit, with 16 areas and with
100,000, a row below the size at which they are refused. Every table and the
scratch sit flush against a page nothing may read, so a check that read past
a table would crash the test rather than pass it. The loaders, compiled from
their source with the botlib fixtures, load valid version 4, 5 and 8 files and
a file without vertexes, and refuse lumps outside the file, a short header, a
cycle, a share, a bad plane, face range, cluster number and overlapping ranges
(Quake III), and a face index 0, portal 0's cluster, a portal off its
cluster's border and area 0's face range (RTCW), each with its message, each
file closed and its tables freed. A source test checks that each loader
checks the file before marking it loaded.

Each of 69 mutations, made one at a time in a copy of HEAD with this change,
fails the tests: every rule and every loader check skipped in turn (35);
each exemption and limit changed -- node 0, face 0 or edge 0 checked,
portal 0 numbered in its clusters or given negative numbers, area 0 required
to have a cluster, a file with no clusters refused, a portal back onto its
own cluster given two numbers, the 128-link cap removed, and Quake III's
routing limits given to RTCW or taken from Quake III, in the checks or in
either loader (13); in the tree walk, no test for a node reached before or a
child outside, cycles reported as shares, node 1 left unmarked, the second
child skipped, a node never marked, its scratch not cleared, a word short or
its path half as long (9); in the helpers, a range's start or end unchecked,
index 0 or a child of `INT_MIN` allowed (4); the checks' scratch a first
number or a link count short, or the loader's kept (3); the travel-time total
without its row pointers, its area pointers or its padding (3); and a refused
file's tables kept by either loader (2).

The checks' fixtures also pass under i686 GCC 13 and Clang 18 on Linux (WSL)
with `-Wall -Wextra -Werror`, and on the default 64-bit Windows target. With
the loaders changed, `rtcw_aas_load_tests.py` (whose loader fixture the new
tests build on, now rewriting both of the loader's own includes),
`rtcw_routing_tests.py`, `rtcw_routecache_load_tests.py` and
`rtcw_aas_tests.py` pass, and
[`scripts/probe_rtcw_aas_load.py`](../../scripts/probe_rtcw_aas_load.py)
loads all 70 installed RTCW files through the changed loader, 59,894,232
record comparisons.

The whole Python suite passes on `8e8de10f` with this change, as a clean
export (2,333 tests, 128 skipped, 57,723 subtests). The same export, built in
its own folder with `win32-dev`'s options (all 1,430 targets, strict warnings),
passes `meson test`: 175 native tests, two skipped (`fnql_audio_loopback`,
`fnql_retail_factory_probe`), besides the wrapper that runs the Python suite.
After `484d02dc` landed, the tests this change touches and the release
packaging, documentation, BSP index and stack-overflow tests passed again on
the exact tree of this commit (87 tests, one skipped for want of symlinks).

## What this does not show

- The limit of 64 MB for Quake III's travel times is a choice, not the point
  at which routing fails: a file needing more loaded before and is refused
  now. The largest installed one needs 1.06 MB.
- A file the loaders refuse runs without bots; the bots it would have had
  are not replaced by anything.
- Retail engines are not compared: `quake3.exe`, `quakelive_steam.exe` and
  `stvoyHM.exe` load these files with the same unchecked code, so their
  behaviour with a damaged file is whatever the damage makes of it.
- The damaged files ran on four stock maps, under the dedicated server for
  Quake III, Quake Live and Elite Force and the client for RTCW, not on every
  map.
- `loaded` means the level ran with its bot and the process quit; it does not
  show that the bots move well.
- Windows only for the runs.

## Found along the way

These are outside the loaders and unchanged here:

- Quake III's routing starts each area cache from a stack array of 128 travel
  times (`startareatraveltimes` in `AAS_UpdateAreaRoutingCache`), indexed by
  the goal area's incoming links. Four installed files have an area with more
  (134 in `mpq3tourney6`, up to 198 in Quake Live, 355 in Elite Force's
  `hm_cam`), so routing to those areas reads past the array; RTCW uses the
  area's own row instead.
- `AAS_PointReachabilityAreaIndex` reassigns `cluster` to a portal's front
  cluster and then reads `portals[-cluster]`, a negative index, for every
  point inside a portal area.
- Quake III's route cache reader (`AAS_ReadRouteCache`, `maps/NAME.rcd`)
  checks only a header whose counts and CRCs any file can match, then trusts
  each cache's size, area and cluster; no installed Quake III, Quake Live or
  Elite Force pak carries one, so it reads only a file the server wrote or a
  pak added. RTCW's reader validates its own.
- `AAS_Optimize`, which runs only when the botlib writes a file it has
  completed itself, remaps every reachability's face and edge number through
  the optimiser's tables, including the model numbers and velocities jump
  pads and movers keep there.

## Identity

- Build without the checks: the sources of `484d02dc`, rebuilt in
  `meson/build/win32-dev` with every development gate on (as VS Code's "setup
  all games" task configures it) at 05:52 UTC on 2026-09-28. HEAD was then
  `8e8de10f`, and the tree held the three source files `484d02dc` committed
  later that morning (`common.c`, `qcommon.h`, `win_main.cpp`, another
  session's stack-overflow reporting), byte for byte as committed; this
  change's two loader files were checked out as HEAD had them. The folder had
  been relinked earlier with a draft of this change, so it was rebuilt before
  the first run; neither binary contains the new messages. Client
  `fntech3.exe` SHA-256
  `865878308f6c54f40ff7a08d87d764e8a5c6b40715f9bffec5b7f587cfe1daa3`, server
  `fntech3.ded.exe` `a4a71fa17ca86220ca97e1356f0cc389c204b882e65c4590777cafca3a73549e`,
  renderer `fnql_glx_x86.dll`
  `6782eed179ac6452fb2d76ee76ba862a84c37b860d96ce9de5ec8400343d086d`.
- Build with the checks: the same tree with this change restored, rebuilt in
  the same folder at 06:19 UTC. Client
  `1d2e7729582da94409ae541b514d064f9b025328896248bab90e94f12a7ac5e3`, server
  `308f01a710ef905d1a244f57aa7851d4428cbd2ff56206fddf12e19818f927d5`, renderer
  `a066b95f2304f5a36e6a404c0f6f5ce3fb9fead420c44091923a73fe661ad57d` (relinked
  with no source change; MSVC links are not reproducible). The two builds'
  sources differ only in this change. Every run hashed its binaries at its
  start and end and found them unchanged. This commit sits on `484d02dc`.
- MSVC 19.44 x86, debugoptimized, strict warnings. Windows 11 x64 host, Intel
  Core i7-13700H, NVIDIA GeForce RTX 4060 Laptop GPU.
- Runs, 2026-09-28 UTC: without the checks, the sweep 05:53-06:14, Quake
  Live's two race maps 06:14-06:15 and the damaged files 06:15-06:18; with
  them, the sweep 06:20-06:42 and the damaged files 06:42-06:44. The final
  audit ran at 06:45 against the same installations, with the loaders of this
  change.
- Installations, read only: `C:\Program Files (x86)\Steam\steamapps\common\Quake 3 Arena`,
  `E:\SteamLibrary\steamapps\common\Quake Live`,
  `E:\SteamLibrary\steamapps\common\Return to Castle Wolfenstein`,
  `E:\SteamLibrary\steamapps\common\Wolfenstein Enemy Territory` (no
  navigation files) and `E:\Games\Star Trek Voyager Elite Force`.
