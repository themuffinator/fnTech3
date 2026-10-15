# Enemy Territory's hide search follows `etded.exe`

The [census-gaps record](2026-10-15-census-small-gaps.md) left one search open:
fnTech3's `AAS_WolfETNearestHideArea`, which a game module reaches through
import 325 (`BOTLIB_AAS_NEARESTHIDEAREA`) to find the nearest area its enemy
cannot see, differed from retail's `AAS_NearestHideArea` in the ways that
record listed. This record reads the whole retail function and its callees,
finds six more differences beside them, and rebuilds the search to match.
Stock Enemy Territory ships no navigation files and its own game module never
calls the import (below), so the search runs only for a mod that brings both
navigation and bots; the evidence is static disassembly and executable
fixtures over generated worlds. No compatibility gate closes. The
[JSON companion](2026-10-15-wolfet-hide-search.json) has the addresses,
constants, mutations and commands.

| Executable | SHA-256 | Size | PE stamp |
| --- | --- | --- | --- |
| `etded.exe` (ET 2.60b, Steam) | `7eb606913a56d6901e358a598fb0079120b2730e113a10fc963f90b777580cdf` | 894,464 | `0x60f7269e` |
| `etmain/qagame_mp_x86.dll` (ET 2.60b) | `160a6218397865ad61071faa6316053713c03b9be80e5d05c7a5be2cab7f630b` | 880,640 | `0x4230a420` |

Both were read with capstone, statically. Neither was run, and nothing was
written to the installation. id's GPL source of Enemy Territory
(`src/botlib/be_aas_route.c`) was read beside the binary as a reference; it has
the same function, and where this record cites an address, the binary is the
evidence.

## Retail's search

**Observed, `etded.exe`.** `Init_AAS_Export` (`0x437200`) puts
`AAS_NearestHideArea` (`0x44a6d0`) at `+0x70` of the navigation exports. In
order, it:

1. returns 0 when the world has no area visibility (`aasworld+0x11ec`,
   `0x44a6db`);
2. turns a negative source number into `~srcnum` (`not`, `0x44a6f2`) and
   zeroes its stored frame time (`0xbebdb8`);
3. returns 0 when that time equals `AAS_Time()` and its step count
   (`0xbebdbc`) is at least 3,000 (`0x44a71b`); a new time zeroes the count
   (`0x44a745`), and the time is stored (`0x44a759`);
4. clears, or allocates, the travel times (`+0x11d4`, two bytes an area) and a
   byte an area at `+0x11f4` that keeps the game's answers (`0x44a75f..0x44a7c6`);
5. with an enemy area, asks `AAS_AreaTravelTimeToGoalArea` (`0x447e40`) for
   the travel time from the start to it and drops the answer (`0x44a7df`);
6. takes the direction to the enemy with `VectorNormalize` (`0x402f20`),
   whose result is the length it divided by;
7. asks the game whether the enemy sees the start: `call [0xcc7a20]` at
   `0x44a832`, with the enemy's origin and number, the start, the source
   number and 0. `0xcc7a20` is slot 24 of the import table `GetBotLibAPI`
   copies to `0xcc79c0` (`0x4370f7..0x437109`); the engine fills that slot
   with `0x424c30` (`0x42558e`), which calls the game's export 12,
   `BOT_VISIBLEFROMPOS` (`0x424c42`);
8. fills the start record's area and position (`0x44a852..0x44a867`), and
   only then returns 0 for area 0 (`0x44a86a`);
9. then, for every link of every record it takes off its queue:
   - skips a travel type or contents the flags forbid (`0x44a909`,
     `0x44a921`), a ladder or disabled area (`test byte [settings+4], 0xa`,
     `0x44a93a`) and the enemy's area (`0x44a945`);
   - with a distance point, skips a step whose end lies farther from it than
     the limit (`0x44a94e..0x44a96d`); a limit of 0 is not exempt;
   - adds up the travel time, then counts a step (`inc [0xbebdbc]`,
     `0x44a998`);
   - skips an area already reached in less time, though an equal time goes on
     (`jb`, `0x44a9c6`), and stores the time (`0x44a9cc`) before any of the
     costs below;
   - drops its best area if this is it (`0x44a9d0..0x44a9e9`), then skips a
     time not under the best (`0x44a9f9`);
   - projects the enemy onto the step (`ProjectPointOntoVector`, `0x402480`)
     and measures from that point, unless on some axis it lies more than 0.1
     (a double at `0x4bed50`) beyond both ends, when it measures from the
     step's end (`0x44aa16..0x44aad8`);
   - once the enemy is more than 32 units from the source (`0x4c4d54`): skips
     a step whose measured point lies within 256 units of the enemy and nearer
     to it than the source (`0x4c1ac0`, `0x44ab09..0x44ab1e`), skips one whose
     end lies more than halfway from the source towards the enemy (0.5 at
     `0x4be35c`, `0x44ab5a..0x44ab84`), and adds ten (`0x4c7688`) for each
     unit its measured point lies nearer the enemy than its start, truncated
     with `cvttss2si` (`0x44abd9..0x44ac05`), skipping it if that is no longer
     under the best (`0x44ac21`);
   - adds 1,000 when `AAS_IsEntityInArea(source, -1, area)` (`0x43c550`,
     called at `0x44ac30`) finds a valid, solid entity linked into the area:
     the links are the current world's, the entity records the default
     world's (`[0x4da050]+0x128`, solid at `+0x64` of a `0x94`-byte record);
   - with an enemy area, and the start unseen, skips an area the enemy's area
     sees (`0x44ac45..0x44ac67`);
   - when the time is under the best (`0x44ac6d..0x44ac81`): for an area that
     has a visibility row (`0x44ac97`) and that the enemy's area does not see
     (`AAS_AreaVisible`, `0x447ec0`), takes the stored answer (`0x44acbc`) or
     asks the game whether the enemy sees the area's waypoint (`0x44ace3`),
     then stores 2 and makes it the best if not (`0x44ad69..0x44ad77`), or
     stores 1 (`0x44acff`);
   - counts a second step and, past 3,000 (`cmp ecx, 0xbb8; jg`,
     `0x44ad19`), queues nothing; otherwise fills the record and queues it
     unless it is already queued (`0x44ad4d`);
10. returns its best area once its queue is empty (`0x44adb3`).

Every rule in the GPL source's function is in the binary with these
constants, and the binary has no other. Retail's only other caller is its
bot library's own test frame, behind the `bot_testhidepos` variable
(`0x4365d5`, in the test export `0x4364b0`).

**Observed, `qagame_mp_x86.dll`.** The stock game module never calls import
325: `scripts/scan_game_import_calls.py` finds no call and no push of it among
its 122 import call sites, where the control, `BOTLIB_AAS_POINT_AREA_NUM`, is
called once (`0x200769ea`).

**Inferred.**

- The step count carries one search's work into the next: a frame's searches
  stop being answered once it reaches 3,000. Where retail counts its steps is
  therefore something a game sees, and a search's last steps go on past the
  limit.
- The dropped travel-time query is not free. It builds the routing cache
  towards the enemy's area and counts against the frame's routing updates, so
  it can defer a later route query in the same frame.
- The game's answers last one search. The source area is not marked as
  reached, so reached back it can be a candidate too.

**fnTech3 before this change** differed in twelve ways. The census-gaps record
had listed the first six:

- it returned as soon as it had taken 3,000 steps, counting one for every link
  before testing it and none at the queue;
- it entered ladder and disabled areas;
- it measured the distance limit to the area's waypoint, and only for a
  positive limit;
- it refused a step whose measured point lay within 40 units of the enemy, a
  rule retail does not have;
- it asked the game nothing about a candidate, so kept no store of answers:
  an area was hidden when the enemy's area did not see it;
- it set up its start record before asking about the start, so it refused
  area 0 without asking;
- it had neither of retail's rules against passing near the enemy and heading
  more than halfway towards it;
- it charged an approach to the enemy at any distance;
- it charged nothing for an occupied area;
- it skipped an area reached again in equal time, and stored a time after its
  costs;
- it kept its best area when it reached it again;
- it made no travel-time query.

**Change** (`code/botlib/wolfet_routing.hpp`; the answer store in
`code/botlib/be_aas_def.h`, freed in `code/botlib/wolfet_visibility.hpp`).
The search is rebuilt in retail's order with retail's tests and constants.
The game's answers are kept a byte an area in `wolfetHideVisibility`,
allocated by the first search and freed with the world's visibility.
Occupancy walks the world's own area links and reads the entity world's
records, as retail does. An area has a visibility row exactly when it sees
itself, which fnTech3's matrix records for every area with a waypoint, and
the game is asked about that waypoint. The enemy's distance is taken with
`VectorLength`, which gives the length retail's `VectorNormalize` returns, bit
for bit; fnTech3's `VectorNormalize` returns the squared length times its
reciprocal square root, which can differ in the last bit. The penalized time
is truncated as `cvttss2si` truncates it, a value `int` cannot hold becoming
0. The game is still asked through the engine callback, `AAS_WolfETCallbacks`,
which copies each position before calling the module: the bot library holds
no pointer into the game module, and the module never sees the bot library's
storage, where retail hands it the waypoint itself.

**Not reproduced.**

- Retail fills record 0's area and position before refusing area 0. Nothing
  reads that position, and an area cache update rewrites the area, so
  fnTech3 refuses first. For an area outside the world retail writes past its
  records; fnTech3 refuses it after asking about the start, as it refuses
  area 0.
- Retail's `AAS_AreaVisible` (`0x447ec0`) compares the area it is asked about
  with the index of the row it last decompressed (`+0x11e8`), which starts at
  0, so until it decompresses a row it answers for area 0 from whatever its
  row buffer holds. fnTech3 answers that area 0 sees nothing, as retail does
  once any row has been read.
- An enemy area outside the world is read past retail's tables; fnTech3
  answers that it sees nothing.
- With no game callback registered fnTech3 answers no area, as before; retail
  always has one.

**Kept without a fixture.** Retail's 0.1-unit projection margin is kept, but no
fixture separates it. It only lets the enemy's projection stand for the
step's end when it lies a fraction of a unit beyond it, which shortens the
measured distance by less than 0.03 over twice that distance. A step the
distance can decide lies more than 32 units from the enemy (nearer, it is
refused either way), so the margin moves its distance by under 0.0005 units
and its cost by under 0.005: it decides a step only that close to a
threshold.

**Tests.** `tests/wolfet_tactical_queue_tests.py` now runs the hide search over
the same nine-area corridor, through the production search, the production
`VectorNormalize` and `AAS_ProjectPointOntoVector`, and the production link
heap. The probe answers for the game and records every question, checking
each one's arguments. Fifteen scenarios cover the enemy beside the source
(24 steps a search, 125 searches a frame, a negative source restarting the
count), a search passing 3,000 steps that still finds its area while
emptying its queue, ladder and disabled areas, the distance limit at each
step's end and a limit of 0, the area and game visibility together, an
unseen source, occupancy (including the searcher's own and a non-solid
entity), each of the three enemy rules, an equally cheap second path round
an added ring, area 0 and areas outside the world, the routing cache the
dropped query builds, and a missing game callback. Nineteen mutations each
restore one earlier behaviour or drop one retail rule, and each must fail in
its own scenario; the five earlier queue mutations must fail in the queue
checks. The mutations are built in parallel. Each fails where it should:

| Mutation | Fails in | Probe |
| --- | --- | --- |
| No game question per candidate | enemy beside the source | area 2 instead of 9 |
| A 40-unit rule | enemy beside the source | area 0 |
| Approach charged with the enemy beside the source | enemy beside the source | area 6 asked about too |
| Counting each link before its tests, returning once over | frame budget | 188 searches, not 125 |
| Returning once the steps run out | past the limit | area 0 instead of 9 |
| Counting a step before its tests | ladder and disabled | 167 searches, not 177 |
| Entering ladder and disabled areas | ladder and disabled | area 5 instead of 0 |
| Limit at the waypoint, positive limits only | distance limit | area 5 instead of 0 |
| An unseen source stepping into view | unseen source | area 5 instead of 0 |
| No occupancy cost | occupied area | area 7 instead of 4 |
| The searcher counted as an occupant | occupied area | area 4 instead of 7 |
| Passing within 256 units of the enemy | past the enemy | area 8 instead of 0 |
| Heading more than halfway towards it | halfway to the enemy | area 6 instead of 0 |
| No approach cost | approach cost | area 5 instead of 9 |
| No answer store | ring | area 8 asked about twice |
| Skipping an equal time | ring | 143 searches, not 137 |
| Keeping the best area when reached again | ring | 143 searches, not 137 |
| Refusing area 0 before asking | area 0 | no question asked |
| No travel-time query | routing cache | no cache built |

## Presentation

No audio, rendering, input or interface path changes. Enemy Territory keeps
fnTech3's renderers, spatial audio, input and interface features; no
retail-exact option is added.

## Validation

In the shared tree the new probe passed and its 24 mutations failed as
listed, in 48 seconds, and `be_aas_route.cpp`, `be_aas_main.cpp` and
`be_aas_entity.cpp` compiled with the shared Windows x86 build's own strict
commands. A `git archive` export of `8a05e028` with this record's ten files
(before a last pass over wording, which changed only a comment in the code),
configured as that build with both the client and the dedicated server, built
all 1,543 targets. 267 of its 273 native tests passed and 2 were skipped; 4
timed out at about 31 seconds in a row while the machine was loaded
(`fntech3_audio_codec_profile`, `fntech3_jka_ui_ghoul2`,
`fntech3_jka_ui_lifecycle` and `fntech3_jka_shader_files`) and passed when run
alone. The complete Python suite passed 2,755 tests and skipped 131, with none
failing. HEAD then moved by two commits (`0e99502a`, `33c75dfd`) that touch
none of this record's code; an export of `33c75dfd` with the final code built
all 1,543 targets, 271 of its 273 native tests passed and 2 were skipped, and
the tests that build the bot library's fixtures passed. Three more commits
(`77f0ab93`, `15b0766f`, `8779edb5`) changed no bot-library code but did
change the ledger, the docs index and the changelog. On an export of
`8779edb5` with the final files, the bot library's three files compiled with
the strict commands, and the documentation, changelog, packaging and
hide-search tests passed, with every test that reads these documents. The
JSON companion has each command and result.

## Unrelated observations

- fnTech3's `VectorNormalize` (Quake3e's) returns the squared length times its
  reciprocal square root, where retail Enemy Territory's returns the square
  root (`0x402f20`); the two can differ in the last bit. Other Enemy
  Territory paths that use the returned length were not checked.
- Quake III's own `AAS_NearestHideArea` in `code/botlib/be_aas_route.cpp`
  remains unreachable, as the census-gaps record found.
