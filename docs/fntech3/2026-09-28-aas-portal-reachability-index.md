# A portal area's reachability index

## Scope

`AAS_PointReachabilityAreaIndex`
([`code/botlib/be_aas_sample.cpp`](../../code/botlib/be_aas_sample.cpp))
gives every reachability area of the loaded navigation a number of its own,
in one run across the clusters: each cluster's reachability areas follow
those of all the clusters below it. Asked about no point, it returns how many
numbers there are. The bot library exports it to game modules, as the sixth
entry of `aas_export_t`.

A cluster portal belongs to two clusters and is numbered in both; for a point
inside a portal's area the function means to take its number in the front
cluster. It replaced the portal's number with that front cluster first, and
then read the portal record again through the cluster:
`portals[-frontcluster].clusterareanum[0]`, a record before the portal table.
The work on the navigation loaders' index checks found it on 2026-09-28 (the
[index check record](2026-09-28-aas-index-checks.md), under "Found along the
way"). The function now keeps the portal's number in a local and reads both
fields through it.

This record establishes which game modules can reach the function, under
which profiles, and whether any retail module's behaviour for a portal area
changes: none does, because no retail game module calls it. The evidence
JSON is [2026-09-28-aas-portal-reachability-index.json](2026-09-28-aas-portal-reachability-index.json).

## Observed: the defect is id's, in both retail engines that export it

The lines are the ones id released (`code/botlib/be_aas_sample.c` in the
Quake III Arena GPL source, `AAS_PointReachabilityAreaIndex`), and both
retail engines whose modules could ask carry them:

- **`quake3.exe` 1.32** (Steam, 909,312 bytes, SHA-256 `80b0add0...44d4`,
  linked 2002-10-07). `GetBotLibAPI` (`0x00487a80`, found by its
  `Mismatched BOTLIB_API_VERSION` message) fills the navigation exports in
  `0x00487b70`, whose slot `+0x14`, the function's place in
  `aas_export_t`, is `0x004a94f0`. There, from `0x004a955f`, a negative
  cluster is multiplied by 20 (the portal record's size) and taken from the
  portal table's address (`[0x00c68e54]`); the front cluster is read at
  `+4`; at `0x004a9574` to `0x004a957c` that cluster is multiplied by 20,
  taken from the table's address again, and `+0xc`, `clusterareanum[0]`, is
  read.
- **`quakelive_steam.exe`** (2,107,904 bytes, SHA-256 `c926fe9f...1299`,
  linked 2016-06-03). `GetBotLibAPI` is `0x004a83c0`, its filler
  `0x004a7fc0`, and slot `+0x14` is `0x00495540`. The same two subtractions
  and reads are at `0x004955f1` to `0x0049560d`.

For a front cluster k the read lands 20k - 12 bytes before the portal table.
**Inferred** from id's allocator, which fnTech3's bot library keeps
([`code/botlib/l_memory.cpp`](../../code/botlib/l_memory.cpp)): each lump is
a hunk block behind a four-byte tag, so for cluster 1 the word read is the one
just below the tag, and larger clusters reach further into whatever the hunk
holds before the table. What either retail engine returns for a portal area
was not measured.

## Observed: which modules can reach it

Only two profiles route a game module's call to it. The numbers are the
engine's own tables: `code/game/g_public.h` for Quake III and Quake Live, and
the module call tables in `code/compat` for the rest.

| Profile | The import's number | What fnTech3 does with it |
| --- | --- | --- |
| `q3` (Quake III, Team Arena) | 577 | `SV_GameSystemCalls` calls the export ([`sv_game.cpp`](../../code/server/sv_game.cpp)) |
| `ql` | slot 68 of the native table, and fnTech3's own compatibility slot 207 + 577 | the slot's handler reaches the same case |
| `rtcw-sp`, `rtcw-mp` | 578 | refused: `Unsupported <profile> game import 578 (BOTLIB_AAS_POINT_REACHABILITY_AREA_INDEX)`; RTCW's released engine source has no case for it |
| `wolfet` | 578 | refused: `Unsupported WolfET game import 578`; Enemy Territory's engine leaves it unhandled ([`server_wolfet_botlib.hpp`](../../code/server/server_wolfet_botlib.hpp)) |
| `stvef-mp` | none: Holomatch's imports end at 572 | `Bad game system trap` |
| `jka-mp` | 577 | refused: `Jedi Academy game import ... is not served yet` |
| `jk2-mp` | no table; its numbers are served only where named | refused: `Jedi Outcast game import 577 is not served yet` |
| `q3-ihv`, `q3test`, `stvef-sp`, `alice`, `fakk2`, `mohaa` | none | their games take import tables of their own, with no navigation query |
| `jka-sp`, `jk2-sp` | none | no game module runs yet |

Nothing inside the engine or the bot library calls the function.

[`scripts/scan_game_import_calls.py`](../../scripts/scan_game_import_calls.py)
then read every retail game module of the profiles that number it, from the
retail paks (and loose where retail ships the module loose), without running
anything. A QVM's code segment is decoded instruction by instruction, and each
`CONST -(N+1)` followed by `CALL` is a call of import N. A native module of
Quake III's older kind is disassembled whole with Capstone; each call through
the pointer its `dllEntry` stores is found and N read from the push before it,
and every `push N` in the code is counted besides. Quake Live's `dllEntry`
stores its import table (`[0x104b13ac]`); each of the 2,193 loads of that
pointer is followed through the slots it reads until the register changes,
and every read of slot 68's offset through any register is traced back to
what loaded that register.

| Profile | Module | Builds (dated in their paks, or linked) | Import calls | Highest import | Calls to this import |
| --- | --- | --- | ---: | ---: | ---: |
| `q3` | `baseq3/pak0.pk3` to `pak3.pk3` `vm/qagame.qvm` | four, 1999-11-21 to 2000-04-25 | 1,122 to 1,197 | 572 | 0 |
| `q3` | `baseq3/pak4.pk3`, `pak6.pk3`, `pak7.pk3` | three, 2000-12-04 to 2001-12-11 | 1,172 to 1,175 | 576 | 0 |
| `q3` | `baseq3/pak8.pk3` | 1.32, 2002-09-30 | 1,175 | 576 | 0 |
| `q3` | `missionpack/pak0.pk3` to `pak3.pk3` | Team Arena, four, 2000-12-04 to 2002-09-30 | 1,282 to 1,285 | 576 | 0 |
| `ql` | `baseq3/qagamex86.dll` | linked 2016-06-03 | 2,198 slot reads of 165 slots | 206 | 0 (slot 68's offset is read 4 times, never through the table) |
| `rtcw-sp` | `qagamex86.dll` | linked 2002-05-01 | 185 | 583 | 0 (no `push 578`) |
| `rtcw-mp` | `Main/qagame_mp_x86.dll` | linked 2002-11-13 | 143 | 573 | 0 (no `push 578`) |
| `wolfet` | `etmain/qagame_mp_x86.dll` | 2.60b, linked 2005-03-10 | 122 | 584 | 0 (no `push 578`) |
| `stvef-mp` | `BaseEF/pak0.pk3` to `pak2.pk3` `vm/qagame.qvm` | three, 2000-08-17 to 2001-04-18 | 1,306 to 1,439 | 572 | no such import |
| `jka-mp` | `base/assets2.pk3`, `assets3.pk3` `jampgamex86.dll` | linked 2003-07-22 and 2003-11-06 | 149, 150 | 633, 634 | 0 (no `push 577`) |
| `jk2-mp` | `base/assets0.pk3` `vm/jk2mpgame.qvm` | the installation's one, 2002-03-04 | 917 | 599 | 0 at 577, Quake III's number |

Every call site found had its number read; none was unresolved. No Quake III
or Team Arena QVM holds the constant -578 at all. The same scan for
`BOTLIB_AAS_POINT_AREA_NUM`, which these games do use, found it in every
Quake III, Team Arena and Holomatch QVM, in each RTCW and Enemy Territory
module (once, their wrapper) and five times in Quake Live's (each through the
table); in neither Jedi module, whose bots, as the ledger already records for
Jedi Academy, use no Quake III navigation import.

**No retail game module calls the function**, so no retail module's behaviour
for a portal area, or any other, changes. Quake III's games through 1.32 never
call an import above 576 (`BOTLIB_AAS_PREDICT_ROUTE`); id's 1.32 game source
keeps its three uses commented out: `Cmd_Stats_f`'s level coverage, the
area bits `ClientEndFrame` would set, and `ClientConnect`'s allocation for
them. QLSRP's reconstruction of the Quake Live game calls it at two places in
`ai_dmq3.c`, and uses the result as an area number; the retail module does
not.

## Changed

The portal branch reads both fields through the portal's own number:

```c
if (cluster < 0)
{
	const int portalnum = -cluster;
	cluster = aasworld.portals[portalnum].frontcluster;
	areanum = aasworld.portals[portalnum].clusterareanum[0];
}
```

Only a module built from source that asks about a point reaches the change:
a Quake III mod restoring id's level-coverage code, or a game built from
QLSRP. For a point inside a portal area with reachabilities it now gets the
portal's number in its front cluster after the clusters below it, which is
below the count the function returns for no point, as every other area's
number is. Before, it got the clusters below it plus a word read before the
portal table, in the retail engines as in fnTech3, and nothing kept that
number below the count that id's commented coverage code sizes its bits by.

This change adds no check of its own. The loaders' index checks, which landed
just before it, already keep the portal branch's reads inside their tables for
every file they accept: an area whose cluster is negative is the area of the
portal it names, that portal's front cluster is a cluster, and its number in
that cluster is one of the cluster's numbers.

## Tests

[`tests/aas_reachability_index_tests.py`](../../tests/aas_reachability_index_tests.py)
compiles `AAS_PointAreaNum` and `AAS_PointReachabilityAreaIndex` from
`be_aas_sample.cpp` and `AAS_AreaReachability` from `be_aas_reach.cpp`
against a synthetic world of six slabs, solid past them: two clusters joined
by two portals, one facing each way, numbered as `AAS_NumberClusterAreas`
numbers them, with an area without reachabilities. The four records before the
portal table are poisoned, and each portal area's own settings carry a
number the portal does not have. Each point finds its slab's area; the count
is 7; the ordinary areas number 0, 1 and 4, portal 1 (front cluster 1) 2 and
portal 2 (front cluster 2) 4 + 2; the area without reachabilities, solid and
an uninitialized world give 0; the five indexes are distinct and below the
count; and neither portal's number in the cluster behind it is the one taken.
Run on the source before this change it fails at the first portal: the old
code numbers the two portal areas 1003 and 1006, the poisoned records'
numbers, past the count of 7.

No other fixture compiles the function. The ten that slice other functions
from `be_aas_sample.cpp` (`rtcw_visibility_tests.py`,
`rtcw_aas_query_tests.py`, `rtcw_aas_trace_tests.py`,
`rtcw_bot_entity_tests.py`, `rtcw_entity_collision_tests.py`,
`rtcw_move_goal_tests.py`, `rtcw_navigation_tests.py`,
`rtcw_prediction_tests.py`, `rtcw_routecache_build_tests.py` and
`rtcw_travel_tests.py`) pass with the new test, 26 tests. The file compiles
with `win32-dev`'s own command (`/W4 /WX /permissive-`).

[`tests/scan_game_import_calls_tests.py`](../../tests/scan_game_import_calls_tests.py)
checks the scanner's parts that need no installation: the numbers it takes
from the engine's tables (for this import 577 in `q3` and `jka-mp`, 68 in
`ql`, 578 in both RTCW modes and `wolfet`, none for Elite Force; for the
control 307 in `q3`, Holomatch and `jka-mp`, 67 in `ql`, 308 in RTCW and
`wolfet`), its reading of enum rows,
and a QVM written for the test, in which it finds three syscalls, counts the
import's address pushed without a call apart, and steps over a call of the
QVM's own function and the operands of `LOCAL` and `ARG`. The scanner loads
Capstone and pefile only to read a native module, so the test runs where
neither is installed.

On a `git archive` export of `8e8de10` with the fix, its fixture and drafts
of the rest, configured as
`meson/build/win32-dev` is (strict warnings, all ten development options, the
three renderers), all 1,430 targets built; the Meson suite passed 175 tests
with 2 skipped and none failed (leaving out the entry that runs the Python
suite), and the Python suite passed 2,328 tests with 128 skipped and none
failed. `484d02d` landed meanwhile, in none of these files. On an export of it
with this commit's six files, documents included, all 1,430 targets built
again, the Meson suite passed 175 tests with 2 skipped, and the Python suite
passed 2,334 tests with 128 skipped and none failed. `c8d3bbb`, the loaders'
index checks, landed next, in none of these files and in nothing the fixture
includes or compiles; on an export of it with this commit's files, the two new
tests, the ten fixtures above, `tests/aas_index_checks_tests.py`,
`tests/rtcw_aas_load_tests.py` and the release packaging, release workflow
and generated-docs tests passed. `8359956`, the route caches' goal links,
landed last and adds three lines to `be_aas_def.h`, which the fixture
includes; on an export of it with this commit's files the same set passed
again, with that commit's `tests/aas_goal_links_tests.py`. The fixture also
builds without a warning under `-Wall -Wextra` and passes as i686 Linux with
GCC 13 and Clang 18 (WSL), and on Windows' default 64-bit target.

## Not established

- No module that asks about a point was run; the changed numbers are shown by
  the fixture alone.
- What `quake3.exe` and `quakelive_steam.exe` return for a portal area was
  not measured. It depends on what their hunks hold before the portal table,
  which no retail module can observe, since none calls the function.
- The scan reads call sites from the code. A module that computed an import
  number at run time, or called through a copy of the pointer or table the
  scan does not follow, would be missed; every call site it found had a
  readable number.
- Jedi Outcast's number for the import is taken to be Quake III's; fnTech3
  refuses every Jedi Outcast number it does not name, so its game cannot
  reach the function whatever the number is.
- Installed maps were not measured for how many portal areas they hold.

## Identity

- Written on `8e8de10fd07e2f8953d4a0020c71c27e6884302d`, last verified on
  `83599566b3b6ea343b940395b9d6958626188ceb`; Windows 11 Home 10.0.26200, x86
  (MSVC 14.44 through the Visual Studio x86 developer shell; Clang 20.1.7 for
  the fixtures), Python 3.11.3 with Capstone 5.0.6 and pefile 2024.8.26.
- Installations, only read: Quake III 1.32 at
  `C:\Program Files (x86)\Steam\steamapps\common\Quake 3 Arena` and Team
  Arena's four paks at `E:\Games\Quake III Arena\missionpack` (its `baseq3`
  paks are the same builds as Steam's); Quake Live, RTCW and Enemy Territory
  2.60b under `E:\SteamLibrary\steamapps\common`; Elite Force 1.20, Jedi
  Academy 1.01 and Jedi Outcast 1.02a under `E:\Games`. Every module's
  SHA-256 is in the JSON.
- Commands: `python scripts/scan_game_import_calls.py --installation
  q3=<Steam Quake III> --installation q3=<E:\Games\Quake III Arena>
  --installation ql=... --installation rtcw=... --installation wolfet=...
  --installation stvef=... --installation jka=... --installation jk2=...`,
  once as is and once with `--import BOTLIB_AAS_POINT_AREA_NUM`;
  `python -m pytest -q tests/aas_reachability_index_tests.py` and the ten
  fixtures above.
