# Route cache dumps checked before the botlib reads them

The [navigation record](2026-09-28-aas-index-checks.md) left one bot file
unchecked. Quake III's botlib, which routes the bots of Quake III, Team
Arena, Quake Live and Elite Force, keeps every route it works out in a
cache, writes them all to `maps/NAME.rcd` in the home when the game sets its
`saveroutingcache` variable (Quake III's game does so for
`bot_saveroutingcache 1`), and reads that file, from the home or any pak, the
next time the map loads (`AAS_ReadRouteCache` in
[`be_aas_route.cpp`](../../code/botlib/be_aas_route.cpp)). The reader
checked the header -- its ident, version, area and cluster counts and two
CRCs over the map's areas and clusters, which any file made for that
navigation matches -- and then trusted each cache: its size, which it
allocated and read into, its area, which indexes the portal caches, and its
cluster and area, which index the area caches. No installed Quake III, Quake
Live or Elite Force pak carries a dump; one appears when a server writes it
or a pak adds it.

The reader now reads the whole file into scratch, checks every cache against
the loaded map, and only then builds each one as the routing builds its own,
with `AAS_AllocRoutingCache`, and links it where routing looks for it. A file
that breaks any rule is refused whole, as the botlib refuses a dump it cannot
use: the file is closed, what was read is freed, nothing is linked, and
routing builds its caches as it goes. The dumps the engine wrote for seven
stock maps of Quake III, Team Arena and Quake Live load and stay in use,
where the old reader faulted the server on every one; Elite Force's game
never writes one, and the dumps made here for its map go through the same
reader. Each damaged dump is refused with its own message while the level
plays on. The [machine-readable record](2026-09-28-route-cache-checks.json)
holds every measurement and run.

## What the old reader did with a dump

- **It could not read the engine's own file.** It took a cache's size from
  the cache's first four bytes, but `aas_routingcache_s` begins with the
  cache's `type` in the published Quake III source (1.32) and in this tree,
  so those bytes are the type and three bytes of padding: 0 for a portal
  cache, 1 for an area cache. The reader allocated 0 or 1 byte, stored the
  size past it, and asked `FS_Read` for -4 or -3 bytes, which reads the rest
  of the file over the heap. On every dump the engine wrote below, the old
  server faulted within a second and a half of starting, in `Z_TagMalloc`
  (`common.c` line 1666), the first allocation after the overflow. The
  published Quake III source reads it the same way, as do the Quake3e,
  Spearmint, Lilium Voyager, CNQ3, WolfcamQL and OpenJK multiplayer botlibs
  in `E:\_SOURCE\_CODE`; RTCW's and Enemy Territory's keep `size` first, and
  RTCW's reader here validates its own format already.
- **It placed the reachabilities two bytes early.** It derived a cache's
  reachability bytes from its size by a formula two bytes short of where
  `AAS_AllocRoutingCache` lays them out; RTCW's original reader carries the
  corrected formula, Quake III's never did.
- **It left the caches out of the routing's accounting.** A cache read
  kept the writer's pointers in its least-recently-used links and was never
  counted in the routing's cache size, so the first route query that used
  one unlinked it through pointers the file supplied, writing where they
  pointed, and freeing it drove the size negative.
- **It trusted every number.** A size smaller than a cache allocated too
  little and read a negative length; an area past the map, or a cluster and
  area that do not belong together, indexed the portal and area caches
  outside their tables; and a reachability byte past its area's
  reachabilities sent a route query into another area's, or past the table.
- **It left the file open** on every refusal after opening it, so each map
  load with a stale dump held one more of the filesystem's handles.

## The rules

The dump is the header followed by every portal cache and then every area
cache, each the bytes the routing held in memory from its first member, so
each is `sizeof(aas_routingcache_t)` plus three bytes a travel time, the size
`AAS_AllocRoutingCache` gives it. What each rule keeps the routing from
doing, and the reader's message after the file's name:

| Rule | Without it | Message |
| --- | --- | --- |
| the file holds a whole header, read whole | the checks run on bytes never read | `is not a route cache dump` |
| a dump made for another navigation file (counts, CRCs) is left alone, silently, as before | -- | none |
| the caches after the header take at most 256 MB, the bound RTCW's reader keeps | the botlib's `int` allocation sizes wrap near 2 GB | `has N bytes of caches, more than the 268435456 read` |
| the counts are not negative, and fit the length: every portal cache takes the size of a cache of every portal, an area cache at least that of a cache without travel times | a count runs the walk, and the allocation, past the file | `cannot hold P portal and A area caches in N bytes` |
| the file reads to the length the filesystem gave | the checks run on bytes never read | `could read N of its M bytes of caches` |
| each cache has its members in the file | the members are read past the file | `CACHE starts N bytes from the end of the file, too few for a cache` |
| a portal cache has type 0, an area cache type 1 | the least-recently-used eviction unlinks a cache from the other kind's list | `CACHE has type T` |
| its area is one of the map's, not area 0 | portal caches are listed by area, area caches numbered by it | `CACHE is for area A, which the map lacks` |
| its cluster is one of the map's, not cluster 0 | area caches are listed by cluster | `CACHE is for cluster C, which the map lacks` |
| its area is in its cluster, or a portal on its border | an area cache is filed under the number another area has there, and routes to that area are answered from it, since a lookup matches travel flags alone (a portal cache's cluster is never read) | `CACHE is for area A, which is not in cluster C` |
| an area cache's area has a number inside its cluster's areas | it is linked outside the cluster's list of caches | `area cache K is for area A, whose number N in cluster C is outside its M areas` |
| its size is the size of a cache of its kind: the portals, or its cluster's reachability areas | the travel times it is indexed by lie past it, and the next cache is read from inside it | `CACHE is S bytes, not the size of a cache of N travel times` |
| it ends inside the file | its travel times and reachabilities are read past the file | `CACHE is S bytes, more than the N left in the file` |
| each reachability a route query can take is one of its area's: an area cache's at each number of its cluster, bounded by the reachabilities of the area routing gives that number (the fewest where areas share one), a portal cache's at each portal, bounded by the portal area's; 0 where no area has the number | a route from that area takes another area's reachability, or one past the table | `area cache K gives number N of cluster C reachability R, which its area lacks`, `portal cache K gives portal P reachability R, which its area lacks` |
| the file ends with its last cache | bytes no cache accounts for, as in a dump cut or grown | `has N bytes after its last cache` |

`CACHE` is `portal cache K` or `area cache K`, K counting the caches of that
kind in the file's order from 0. The botlib prints each as it prints a bad
cache, through `AAS_Error`: `Fatal: maps/q3dm7.rcd: area cache 0 has type 0`.
The two messages the old reader had, for a wrong ident and a wrong version,
are kept. A dump read prints
`route cache read from maps/q3dm7.rcd: 64 portal and 147 area caches, 98653 bytes`.

Each cache read keeps its kind, time, cluster, area, origin, start travel
time, travel flags, travel times and reachabilities; its links and its
reachability pointer are the routing's own, and its size is counted in the
routing's cache size, so the routing frees it like any other when the caches
outgrow `max_routingcache`. The caches are linked newest last in the file's
order. Every number a check reads from the map is itself checked first, so
the checks index nothing outside the map's tables, even for navigation the
[index checks](2026-09-28-aas-index-checks.md) would refuse.

### What is left alone

- **Travel times.** Any value is a travel time; a damaged one routes a bot
  badly, but nothing is indexed by it.
- **A portal cache's cluster, beyond membership.** Routing lists a portal
  cache by its area alone and never reads its cluster after making it; the
  engine writes the goal area's cluster, or a goal portal's front one.
- **Time, origin and start travel time.** Quake III's routing reads none of
  them after making a cache (RTCW's eviction reads time, but RTCW reads its
  own dumps).
- **Two caches for one area and travel flags.** Routing takes the first; the
  other is freed with the rest.
- **The pointer size.** The layout is this build's: a cache is 64 bytes
  plus its travel times with 32-bit pointers and 88 with 64-bit ones, so a
  dump written by the other kind of build fails the size rule, as it must.

## What the engine writes

[`scripts/probe_route_cache_loads.py`](../../scripts/probe_route_cache_loads.py)
loaded each map on the dedicated server with bots on, by `devmap`, since the
retail Quake III game registers `bot_saveroutingcache` as a cheat (after
`map`, setting it answers "bot_saveroutingcache is cheat protected."),
added bots, let them play, and set `bot_saveroutingcache 1`; the botlib
wrote the dump into the probe's home. The probe then checked each dump offline against the
navigation file the installation serves, by the rules above written again
in Python, and every dump passes every rule. Elite Force's Holomatch game
takes no notice of `bot_saveroutingcache`: it writes nothing, and says
nothing.

| Map | Profile | Areas, clusters, portals | Bots | Dump bytes | Portal caches | Area caches | Area cache sizes | Travel flag sets | Largest reachability | Rules |
| --- | --- | --- | --- | ---: | ---: | ---: | --- | ---: | ---: | --- |
| `q3dm7` | `q3` | 3,598, 13, 60 | 4 | 98,653 | 64 | 147 | 10 sizes, 79 to 823 | 2 | 19 | all pass |
| `q3dm1` | `q3` | 1,714, 2, 0 | 4 | 82,680 | 0 | 60 | 1,378 | 3 | 11 | all pass |
| `q3dm17` | `q3` | 2,620, 1, 0 | 2 | 76,320 | 0 | 45 | 1,696 | 2 | 20 | all pass |
| `q3ctf2` | `q3` | 9,961, 33, 233 | 4 | 339,612 | 77 | 178 | 21 sizes, 73 to 6,049 | 1 | 49 | all pass |
| `mpteam1` (Team Arena) | `q3` | 7,420, 22, 214 | 1 | 127,073 | 44 | 144 | 18 sizes, 73 to 1,141 | 1 | 25 | all pass |
| `falloutbunker` | `ql` | 7,174, 25, 96 | 2 | 83,611 | 14 | 203 | 23 sizes, 73 to 910 | 1 | 23 | all pass |
| `campgrounds` | `ql` | 1,906, 1, 0 | 2 | 78,414 | 0 | 42 | 1,867 | 3 | 28 | all pass |
| `hm_borg1` | `stvef-mp` | -- | 1 | none: the game ignores `bot_saveroutingcache` | | | | | | |

Quake III's routing leaves every reachability of a portal cache 0: only
RTCW's fills them, so a Quake III route that starts in a portal area takes
that area's first reachability, as the published source does.

## Reading them back

Each dump was then read back, from the probe's home, by two dedicated
servers built alike from one `git archive` of the source, the second with
only the new reader: the map was loaded with bots on, the bots played for
half as long, and the server wrote the dump again before it quit.

| Map | Old reader | New reader | Caches read | Kept after play | Caches written after play |
| --- | --- | --- | ---: | ---: | ---: |
| `q3dm7` | access violation in `Z_TagMalloc`, then exit 1 | read all, played | 211 | 211 | 242 |
| `q3dm1` | access violation in `Z_TagMalloc`, then exit 1 | read all, played | 60 | 60 | 67 |
| `q3dm17` | access violation in `Z_TagMalloc`, then exit 1 | read all, played | 45 | 45 | 51 |
| `q3ctf2` | access violation in `Z_TagMalloc`, then exit 1 | read all, played | 255 | 255 | 255 |
| `mpteam1` (Team Arena) | access violation in `Z_TagMalloc`, then exit 1 | read all, played | 188 | 188 | 188 |
| `falloutbunker` | access violation in `Z_TagMalloc`, then exit 1 | read all, played | 217 | 217 | 217 |
| `campgrounds` | access violation in `Z_TagMalloc`, then exit 1 | read all, played | 42 | 42 | 49 |

`kept` counts the caches read that the server wrote back unchanged, their
time and pointers aside: every cache read was live in the routing, used and
listed, and written again beside those made since. The old server never
reached the level: after the fault it either ended with status 1 on a second
error while shutting down ("recursive error") or stopped in its shutdown
until the probe ended it.

## Damaged dumps

The dump written for `q3dm7` was damaged one way at a time and read back the
same way by both servers.

| Damage | Old reader | New reader: message after `maps/q3dm7.rcd:` | New reader: level |
| --- | --- | --- | --- |
| area outside cluster | access violation in `Z_TagMalloc`, then stuck in shutdown | `area cache 0 is for area 29, which is not in cluster 2` | played |
| reachability past area | access violation in `Z_TagMalloc`, then exit 1 | `area cache 0 gives number 0 of cluster 1 reachability 4, which its area lacks` | played |
| size plus one | access violation in `Z_TagMalloc`, then stuck in shutdown | `portal cache 0 is 248 bytes, not the size of a cache of 61 travel times` | played |
| type swapped | access violation in `Z_TagMalloc`, then exit 1 | `area cache 0 has type 0` | played |
| truncated | access violation in `Z_TagMalloc`, then stuck in shutdown | `area cache 146 is 142 bytes, more than the 141 left in the file` | played |
| counts past file | access violation in `Z_TagMalloc`, then exit 1 | `cannot hold 64 portal and 1000147 area caches in 98653 bytes` | played |
| trailing bytes | access violation in `Z_TagMalloc`, then exit 1 | `has 3 bytes after its last cache` | played |

`played` means the level ran with its bots for the whole script and the
server quit with status 0; routing built its caches as it went.

## Elite Force

With no dump of its own to read, the Holomatch server on `hm_borg1` was given
two made by the probe from its navigation file's header, so the header
matches; neither is the engine's.

| Dump made here | Old reader | New reader: message | New reader: level |
| --- | --- | --- | --- |
| no caches | played, silently | `route cache read from maps/hm_borg1.rcd: 0 portal and 0 area caches, 0 bytes` | played |
| one area cache, for area 0 | access violation in `Z_TagMalloc`, then exit 1 | `maps/hm_borg1.rcd: area cache 0 is for area 0, which the map lacks` | played |

## Tests

[`tests/aas_route_cache_tests.py`](../../tests/aas_route_cache_tests.py)
compiles the real writer and reader from `be_aas_route.cpp` over the routing
of the route query fixture, whose cluster rings route with Quake III's
routing, and runs under every profile that routes with it: `quake3`,
`quakelive`, `wolfet`, `stvef-sp`, `stvef-mp`, `jka-mp` and `jk2-mp`. It
routes every area to every other under two sets of travel flags on a ring of
three clusters and three portals, a single cluster, and a ring of five,
writes the caches with `AAS_WriteRouteCache` and reads them back: every
cache must come back as the routing made it, linked in its list and in the
least-recently-used list in the file's order, its size counted, and every
route must be answered from the caches read, with none made; the file's
caches in another order load too. A stale dump is refused silently and
routing then rebuilds the same caches.

Then each rule is broken alone in the written dump, and the file must be
refused whole with its own message, closed, with nothing linked and every
allocation the read made freed, and, for the header and the counts, before
anything is allocated: a wrong ident and version, each of the four stale
header numbers, every length shorter than the header, a length the
filesystem gives short or long, a header read short, the 256 MB bound and a
file at it, negative counts, counts one past what the length holds for each
kind (and a portal count only that rule catches), counts at the limit, one
portal cache too many, one area cache too few, three trailing bytes, a body
read short, every truncation of the body, a cache too short for its members
and one running past the end; for the first portal cache and the first area
cache, the other type and type 7, areas 0, past the map, -1 and `INT_MIN`,
clusters 0, past the map, -1 and `INT_MAX`, every cluster the area is not in,
sizes one over, one under, three over and the bare structure, and the cache
laid out with pointers of the other size; a portal area's cache in a cluster
it does not join; an area whose cluster names a portal past the table or
`INT_MIN`, with a portal planted past the table that would take the cache in
were it followed; a portal numbered at, below and far past its cluster's
areas; a cluster of -1 reachability areas and a cache of the size that would
give; a reachability at its area's count (refused) and one below it (read),
in an area cache and a portal cache, one for portal 0, one for a number two
areas share (bounded by the fewer), and one for a number past the cluster's
areas.

The same binary holds 37 more readers, each missing one check or one
obligation: every rule above, or a part of one; closing the file on each
refusal; freeing the file's bytes on a refusal and after a read, and the
checks' scratch; each part of the reachability bounds; and linking a cache
read into the least-recently-used list.

Every one fails: 24 by refusing with another message, or reading the file,
where the production reader refuses it with its own; 3 by refusing a file
the production reader reads; 9 by an assertion on the file handle, the
allocations or the links, or an allocation the fixture could not make; and 1
by an access violation.

| Reader missing | Fails because it |
| --- | --- |
| a file shorter than its header is read | refuses with `cannot hold 41 portal and 58 area caches in -1 bytes` where the production reader says `is not a route cache dump` |
| a header read short is trusted | refuses silently where the production reader says `is not a route cache dump` |
| a file that is no dump stays open | fails the assertion `!fileOpen&&allocations==live&&emptyRouting()` |
| a dump of another version stays open | fails the assertion `!fileOpen&&allocations==live&&emptyRouting()` |
| a dump of another AAS file stays open | fails the assertion `!fileOpen&&allocations==live&&emptyRouting()` |
| counts that cannot fit leave the file open | fails the assertion `!fileOpen&&allocations==live&&emptyRouting()` |
| a file of any length is read whole | asks for the 2 GB the file claims, which the fixture cannot allocate |
| a negative count of portal caches is taken | refuses with `area cache 1 has type 0` where the production reader says `cannot hold -1 portal and 58 area caches in 7698 bytes` |
| a negative count of area caches is taken | refuses with `has 4658 bytes after its last cache` where the production reader says `cannot hold 41 portal and -1 area caches in 7698 bytes` |
| more portal caches than the file holds are read | refuses with `portal cache 0 is for area 0, which the map lacks` where the production reader says `cannot hold 1 portal and 0 area caches in 71 bytes` |
| more area caches than the file holds are read | refuses with `has 7698 bytes after its last cache` where the production reader says `cannot hold 41 portal and 2147483647 area caches in 7698 bytes` |
| a file read short is checked as though whole | refuses with `has 268427758 bytes after its last cache` where the production reader says `could read 7698 of its 268435456 bytes of caches` |
| the file is read without its checks | an access violation |
| a refused file keeps its bytes | fails the assertion `!fileOpen&&allocations==live&&emptyRouting()` |
| a read file keeps its bytes | fails the assertion `allocations.empty()&&!fileOpen` |
| a cache too short for its members is copied | refuses with `area cache 58 has type 0` where the production reader says `area cache 58 starts 0 bytes from the end of the file, too few for a cache` |
| a cache of the wrong kind is listed | refuses with `portal cache 41 is 79 bytes, not the size of a cache of 4 travel times` where the production reader says `portal cache 41 has type 1` |
| area 0 and below are taken | refuses with `portal cache 0 is for area 0, which is not in cluster 1` where the production reader says `portal cache 0 is for area 0, which the map lacks` |
| areas past the map are taken | refuses with `portal cache 0 is for area 13, which is not in cluster 1` where the production reader says `portal cache 0 is for area 13, which the map lacks` |
| cluster 0 and below are taken | refuses with `portal cache 0 is for area 1, which is not in cluster 0` where the production reader says `portal cache 0 is for cluster 0, which the map lacks` |
| clusters past the map are taken | refuses with `portal cache 0 is for area 1, which is not in cluster 4` where the production reader says `portal cache 0 is for cluster 4, which the map lacks` |
| an area outside the cache's cluster is taken | reads the file the production reader refuses with `portal cache 0 is for area 1, which is not in cluster 2` |
| a portal past the map's portals is followed | refuses with `area cache 4 gives number 0 of cluster 1 reachability 1, which its area lacks` where the production reader says `portal cache 0 is for area 1, which is not in cluster 1` |
| a number past the cluster is linked | refuses with `area cache 5 gives number 3 of cluster 1 reachability 1, which its area lacks` where the production reader says `area cache 0 is for area 10, whose number 5 in cluster 1 is outside its 5 areas` |
| a negative number is linked | refuses with `area cache 5 gives number 3 of cluster 1 reachability 1, which its area lacks` where the production reader says `area cache 0 is for area 10, whose number -1 in cluster 1 is outside its 5 areas` |
| a cache of any size is taken | refuses with `portal cache 1 is for area 0, which the map lacks` where the production reader says `portal cache 0 is 77 bytes, not the size of a cache of 4 travel times` |
| a cluster of negative reachability areas takes a cache smaller than any | refuses with `area cache 1 has type 0` where the production reader says `area cache 0 is 61 bytes, not the size of a cache of -1 travel times` |
| a cache past the end of the file is taken | refuses with `has -1 bytes after its last cache` where the production reader says `area cache 57 is 79 bytes, more than the 78 left in the file` |
| any reachability is taken | reads the file the production reader refuses with `area cache 4 gives number 0 of cluster 1 reachability 4, which its area lacks` |
| a portal's area gives no bound | refuses a file the production reader reads, with `portal cache 0 gives portal 1 reachability 7, which its area lacks` |
| a cluster's areas give no bound | refuses a file the production reader reads, with `area cache 4 gives number 0 of cluster 1 reachability 1, which its area lacks` |
| a portal bounds only its front cluster | refuses a file the production reader reads, with `area cache 4 gives number 4 of cluster 1 reachability 1, which its area lacks` |
| the last of the areas sharing a number bounds it | refuses with `area cache 4 gives number 3 of cluster 1 reachability 1, which its area lacks` where the production reader says `area cache 0 gives number 0 of cluster 1 reachability 4, which its area lacks` |
| numbers past a cluster are bounded by the next | refuses with `area cache 1 is 79 bytes, not the size of a cache of 6 travel times` where the production reader says `area cache 0 gives number 5 of cluster 1 reachability 1, which its area lacks` |
| bytes after the last cache are ignored | reads the file the production reader refuses with `has 79 bytes after its last cache` |
| the scratch of the checks is kept | fails the assertion `!fileOpen&&allocations==live&&emptyRouting()` |
| a read cache is not in the least-recently-used list | fails the assertion `previous==aasworld.newestcache&&bytes==aasworld.routingcachesize` |

The fixture and its 37 readers also pass and fail alike under i686 GCC 13
and Clang 18 on Linux (WSL), and `be_aas_route.cpp` with the new reader
compiles there with `-Wall -Wextra` without a warning of its own, as it does
under `win32-dev`'s `/W4 /WX`.

On a `git archive` of `83599566` with the seven files this change commits,
all 1,430 targets build with `win32-dev`'s options; `meson test` passes 175
tests and skips 2 for this machine (no OpenAL loopback device, no retail
Quake Live path set), the entry that runs the Python suite left to it; and
the Python suite passes 2,338 tests and 57,776 subtests, skipping 128 that
need a tool, retail executable, installation or symlink right the export
does not have. This change's fixture, the botlib's route, load and
navigation tests, every test that reads the ledger, the changelog or these
records, and the packaging tests -- 54 files -- also pass on the committed
tree, `b2cbf98e` with the seven files: 515 tests and 796 subtests, one
skipped for want of symlink rights.

## What this does not show

- Retail engines were not run with a dump: `quake3.exe` 1.32,
  `quakelive_steam.exe` and `stvoyHM.exe` read `maps/NAME.rcd` with the
  reader of the published source, so a dump they write would reach the
  same fault in them; that is inferred from the source, not observed.
- The runs cover the dedicated server on Windows x86 for Quake III, Team
  Arena and Quake Live on seven maps, and Elite Force Holomatch on one with
  dumps made here, not by the engine. Enemy Territory ships no navigation,
  and the Jedi profiles refuse their game modules before a map's bots load.
- `played` shows that the level ran with its bots and the process quit, and
  `kept` that the caches read were used and written back; it does not show
  that the bots move better or worse for them.
- The 256 MB bound is a choice: the largest dump written here holds
  339,612 bytes of caches.

## Found along the way

These are outside the reader and unchanged here:

- `AAS_StartFrame` writes a dump whenever `saveroutingcache` is set, before
  it has checked that routing is initialized: while a map's reachability is
  still being computed, over several frames (a navigation file without it,
  or `forcereachability`), `AAS_WriteRouteCache` walks
  `aasworld.portalcache` while it is still null. Since 2026-09-29 the writer
  writes nothing until routing is initialized
  ([record](2026-09-29-route-cache-before-routing.md)).

## Identity

- Source of both servers: a `git archive` of `484d02dc`, the stack overflow
  commit. This change's parent is `b2cbf98e`: between them, `c8d3bbbc` added
  the navigation index checks, which every stock file passes; `83599566`
  gave Quake III's routing a start row as long as the links into the goal
  area, which changes routes on four installed maps, none of them probed
  here; and `b2cbf98e` numbers a portal area in
  `AAS_PointReachabilityAreaIndex`, an export no retail game module calls.
  None touches the route cache code or its format. Configured server-only
  with `win32-dev`'s options (`-Db_vscrt=static_from_buildtype
  -Dstrict-warnings=true -Dbuildtype=debugoptimized` and its ten development
  gates, `--wrap-mode=nodownload`) and built with MSVC 19.44.35222 for x86.
- Before: `.tmp/runtime/routecache-before/fntech3.ded.exe`, SHA-256
  `d815272f87314dc1f718e0dec168166a0704a4b2b978601b18fe8471a6d63790`, the
  source as it is.
- After: `.tmp/runtime/routecache-after/fntech3.ded.exe`, SHA-256
  `ccd51684d4f7ebcb428de5421aa1bd753405527156cd6e3bbeb8551b17e41bdb`, the
  source with only the reader's region of `be_aas_route.cpp` replaced. Both
  hashes were the same at the start and end of each run.
- Runs: `python scripts/probe_route_cache_loads.py --before <before> --after
  <after> --installation q3=<Quake III> --installation ql=<Quake Live>
  --installation stvef=<Elite Force> --output .tmp/route-cache/sweep`,
  2026-09-28T06:59:21Z to 2026-09-28T07:11:30Z; and `--only
  stvef-mp:hm_borg1 --output .tmp/route-cache/sweep-ef`,
  2026-09-28T07:12:34Z to 2026-09-28T07:14:44Z.
- Installations, read only: Quake III Arena (Steam) under `C:\Program Files
  (x86)\Steam\steamapps\common\Quake 3 Arena`, Quake Live under
  `E:\SteamLibrary\steamapps\common\Quake Live`, Elite Force 1.20 under
  `E:\Games\Star Trek Voyager Elite Force`.
- Windows 11 Home 10.0.26200; x86 dedicated servers on private desktops with
  `net_enabled 0`.
- Fixture: Clang 20.1.7 for `i686-pc-windows-msvc` in the Visual Studio 2022
  x86 developer shell; WSL Ubuntu 24.04's GCC 13.3.0 and Clang 18.1.3 with
  `-m32`.
