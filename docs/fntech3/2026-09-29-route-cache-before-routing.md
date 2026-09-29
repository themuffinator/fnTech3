# No route cache dump before routing exists

The Quake III botlib writes its route caches to `maps/NAME.rcd` when the game
sets the `saveroutingcache` library variable, which Quake III's and Quake
Live's games do for `bot_saveroutingcache 1`. `AAS_StartFrame` wrote the dump
whenever the variable was set, before it had checked that routing existed, as
the [route cache record](2026-09-28-route-cache-checks.md) found along the way.
With `bot_forcereachability 1` the botlib computes a map's reachability again,
over many frames. Until that is done the areas are counted, but the cache
tables `AAS_WriteRouteCache` walks are not made: asking for a dump then faulted
the dedicated server in `AAS_WriteRouteCache` on Quake III's `q3dm7` and Quake
Live's `campgrounds`. The writer now writes nothing until routing is
initialized, says so, and the level plays on. The
[machine-readable record](2026-09-29-route-cache-before-routing.json) holds the
builds, the fault's symbol and the runs.

## What the writer read

- `AAS_LoadMap` clears `aasworld.initialized` and frees the old map's routing
  (`AAS_FreeRoutingCaches`, which nulls `portalcache` and `clusterareacache`)
  before it loads the new map's navigation. `AAS_ContinueInit` then makes the
  routing, `AAS_InitRouting`, only once reachability is complete, and sets
  `initialized` after it.
- `AAS_StartFrame` runs `AAS_ContinueInit` and then, if `saveroutingcache` is
  set, `AAS_WriteRouteCache`, whatever `AAS_ContinueInit` did. The writer
  counted the portal caches of every area through `portalcache[i]` and the
  area caches of every cluster through `clusterareacache[i][j]`.
- While reachability is computed, `numareas` and `numclusters` hold the
  file's counts and both tables are null, so the first read faulted. Quake III
  1.32's source has the same order; that retail engines fault too is inferred
  from it, not observed.
- A map without navigation dumps its data (`AAS_DumpAASData`, all counts 0)
  and the writer wrote a dump of a header and no caches, for a map it could
  never be read on.

## Change

`AAS_WriteRouteCache` (`code/botlib/be_aas_route.cpp`) returns before it
reads anything unless `aasworld.initialized` is set, printing
`no route cache written: routing is not initialized` as a botlib warning.
`AAS_StartFrame` still clears `saveroutingcache` after the call, so a request
made too early is answered once and not held for later. RTCW's writer, which
`AAS_StartFrame` never calls, and every dump written once routing exists are
unchanged. The profiles this reaches are those whose bots route with Quake
III's botlib: Quake III, Quake Live and Elite Force Holomatch, whose game
never sets the variable.

## Tests

`tests/aas_route_cache_tests.py` compiles the real writer over the route query
fixture's routing, under every profile that routes with Quake III's botlib. It
asks for a dump with the routing only marked uninitialized, then with the
tables freed as `AAS_LoadMap` leaves them and the areas and clusters still
counted, then with no navigation loaded: no file is opened in any of them.
With the routing made again the dump is written, header and all. Two writers
of `WRITER_MUTATIONS` each fail a check, never a fault: one without the new
check, and one that writes whenever navigation is loaded. The earlier 37
reader mutations fail as before.

## Runtime

`scripts/probe_route_cache_unready.py` runs the dedicated server on a private
desktop with networking off. Its console script sets
`bot_forcereachability 1`, loads the map with `devmap` (the game registers
`bot_saveroutingcache` as a cheat) and waits 40 frames. It then sets
`bot_saveroutingcache 1`, waits 40 more, lists the clients and quits. The two
servers were built alike from `8e345c8`, one with this change alone.

| Map | Server | Reachability when asked | Outcome |
| --- | --- | --- | --- |
| Quake III `q3dm7` | `8e345c8` | 2.4% | faulted: `ACCESS_VIOLATION` at `fntech3.ded.exe@2fc80`, `AAS_WriteRouteCache+0x50` by the build's PDB; the level ended and with it the script |
| Quake III `q3dm7` | with this change | 2.4% | warned once, wrote no dump, computed on to 4.3% and quit with status 0 |
| Quake Live `campgrounds` | `8e345c8` | 1.4% | faulted at the same address |
| Quake Live `campgrounds` | with this change | 1.4% | warned once, wrote no dump, computed on to 2.3% and quit with status 0 |

Neither server wrote a dump into its home, both executables were unchanged
after their runs, and no crash dump was written: the engine's exception
filter caught each fault.

## What this does not show

- A map without navigation was not run: every installed Quake III and Quake
  Live map has its `.aas`. That case rests on the fixture.
- Retail engines were not run with a forced reachability; that they fault
  follows from the published source's order, not from a run.
- A dump asked for once routing exists is unchanged, and was not run here
  again; the [route cache record](2026-09-28-route-cache-checks.md) wrote and
  read such dumps.
- Windows x86 only for the runs.

## Identity

- Tested commit `8e345c8f05bf66b748e85cbe3686cde78f4f3a05`, built as a
  `git archive` export with win32-dev's options, strict warnings and every
  development option, on Windows 11 x64, 2026-09-29.
- Server without the change `53e269ac165ecd1415651d33d483d8994e8372bf35b1ebf4b045bf36d476b15e`,
  with it `359f5b853e87cdf33603b1e3158ac6100702fa40dca8e28070beb0f18ff0cf0a`.
- Installations `C:\Program Files (x86)\Steam\steamapps\common\Quake 3 Arena`
  and `E:\SteamLibrary\steamapps\common\Quake Live`, read only.
