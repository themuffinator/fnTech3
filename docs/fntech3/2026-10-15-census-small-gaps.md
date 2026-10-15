# Small gaps from the October 15 census

The [October 15 round](2026-10-15-compatibility-round.md) ended with a census
of small open gaps in its [JSON companion](2026-10-15-compatibility-round.json).
This record settles three of them against the retail executables: Enemy
Territory's tactical queue marks, Jedi Outcast multiplayer's pure gamestate
budget, and Elite Force Holomatch's `sv_fps` range, which HEAD already served.
Settling the first two found three more gaps in the same code, and they are
fixed here too: Enemy Territory's update records were too few for its tactical
searches, its range search left marks behind where retail leaves none, and its
pure gamestate budget measured Quake Live's message rather than its own. The
changes were built and tested as HEAD plus this record's files alone, without
another session's uncommitted work. The [JSON companion](2026-10-15-census-small-gaps.json)
records the addresses, commands and outcomes. No compatibility gate closes.

The retail executables read here are the ones earlier records name:

| Executable | SHA-256 | Size | PE stamp |
| --- | --- | --- | --- |
| `etded.exe` (ET 2.60b, Steam) | `7eb606913a56d6901e358a598fb0079120b2730e113a10fc963f90b777580cdf` | 894,464 | `0x60f7269e` |
| `jk2mp.exe` (1.02a) | `2f08dcf9a089cb66062766f4922fda63a9e553de893c174b497dfefeb7a1577a` | 1,126,400 | `0x3c832b11` |

Both were disassembled with capstone, statically. Neither was run, and
nothing was written to either installation.

## Enemy Territory's tactical searches

The [routing-limits record](2026-10-02-wolfet-routing-limits.md) found by
source inspection that fnTech3's common queue refused a record already marked
as queued, while the original danger and retreat searches queue without
asking. Retail confirms it, and shows two more differences beside it.

**Observed, `etded.exe`.** `GetBotLibAPI` (`0x4370f0`, found by its
`Mismatched BOTLIB_API_VERSION` string at `0x4c5fa4`) fills the navigation
exports through `Init_AAS_Export` (`0x437200`), whose stores follow the
original `aas_export_t`: the range search at `+0x5c` (`0x44a450`), the danger
search at `+0x60` (`0x447f30`), the retreat at `+0x64` (`0x44b790`) and the
hide search at `+0x70` (`0x44a6d0`). The routing update record is 40 bytes,
with its queued mark at `+0x1c` and its links at `+0x20` and `+0x24`.

| Search | Queues a successor at | Reads the mark first |
| --- | --- | --- |
| Danger (`AAS_AvoidDangerArea`) | `0x448166..0x448182` | no |
| Retreat (`AAS_Retreat`) | `0x44ba85..0x44ba9e` | no |
| Range (`AAS_ListAreasInRange`) | `0x44a663..0x44a685` | yes, `cmp [ecx+0x1c], 0` |
| Hide (`AAS_NearestHideArea`) | `0x44ad4d..0x44ad83` | yes, `cmp [ecx+0x1c], 0` |

- The danger search returns as soon as an area whose travel time is below
  1,000 is far enough from the danger (`cmp ebx, 0x3e8; jb 0x4481bc`), and
  the retreat as soon as an area is far enough from both (`0x44ba36`).
  Neither empties its queue, so whatever is still queued keeps its mark.
- When the range search's list is full (`0x44a637`), it does not stop: it
  jumps back into its loop (`0x44a6aa..0x44a6b2`) and keeps flooding until
  its queue is empty, writing each further visible area past the caller's
  array through a pointer it never bounds (`0x44a60e..0x44a634`), and it
  returns a count above the caller's limit.
- `AAS_InitRoutingUpdate` (`0x44a1d0`, called from `0x44a0f9`) allocates one
  update record for every area, `numareas * 0x28` bytes (`0x44a1ed..0x44a208`),
  through the routing allocator `0x44bb00`, which jumps to the cleared
  allocator `0x4600f0`. The tactical searches address these records by area
  number; the area cache update addresses the same records by an area's
  number within its cluster.

**Inferred.** A mark left by an early return keeps a later range or hide
search, or an area cache update that reaches the same record, from going on
through that record. The danger and retreat searches cannot lose an area to
it. The range search leaves no mark of its own: every record it queues was
unmarked, and its continued flood takes each one off its queue again.

**fnTech3 before this change** differed in three ways:

- Every search refused a marked record, so a mark left by an earlier search
  could cut a danger or retreat search short of the area retail would find.
- Enemy Territory took Quake III's update-record count, the largest
  cluster's number of reachability areas, while its tactical searches
  address the records by area number. Any area numbered at or past that
  count was written past the end of the array. Every multi-cluster map has
  such areas, so a mod's bots running a tactical search there would have
  corrupted the heap. No fixture saw it, because the routing fixtures
  allocated the records themselves, one per area.
- The range search stopped when its list was full and left its remaining
  queue marked, where retail leaves none.

**Change** (`code/botlib/wolfet_routing.hpp`, `code/botlib/be_aas_route.cpp`):

- The queue keeps its checked `Push` for the range and hide searches and
  gains an unchecked `Append`, which the danger and retreat searches use, as
  retail's do.
- `AAS_InitRoutingUpdate` gives Enemy Territory at least one update record
  per area. For any valid world that is retail's `numareas`, since a
  cluster's reachability areas are some of the world's areas.
- A range search that stops at its limit takes what is still queued off the
  queue, which clears the marks. The marks it leaves are then retail's. It
  still stops at the caller's limit and returns at most that count: writing
  past the caller's array, and returning more than the caller allowed for,
  is not reproduced.
- Kept as retail has them: the marks a danger or retreat search leaves when
  it returns early, which later range and hide searches still honour, and
  the travel time a successor inherits from whatever last wrote its record.

Quake III, Quake Live and Holomatch keep Quake III's record count, and none of
them addresses the records by area number from a reachable path. RTCW has its
own initialization, one record per area, and its own tactical nodes; nothing
of RTCW's changes.

**Tests.** `tests/wolfet_tactical_queue_tests.py` compiles the production
allocation, the production visibility accessors and the whole of
`wolfet_routing.hpp` into the existing routing fixtures. It checks the record
counts for Enemy Territory and three other navigation profiles on a
three-cluster world, then runs the danger, retreat and range searches over a
nine-area corridor with a side branch: unmarked, with a mark left on the
corridor's only way on, after a danger search's early return, and after a
range search stopped at its limit. Five mutations each restore one earlier
behaviour, and each fails at its own assertion: Quake III's record count, the
danger search's check, the retreat's check, the range search's leftover marks,
and a range search that ignores the mark.

### The hide search still differs (open)

The hide search keeps retail's mark check, but its reconstruction is not
retail's in other ways. Retail (`0x44a6d0`) skips ladder and disabled areas
(`0x44a93a`, `test [..+4], 0xa`), measures the distance limit from the
reachability's end (`0x44a94e..0x44a96d`), counts a loop step for each
candidate after its travel time (`0x44a998`) and, past 3,000 steps, stops
queuing but goes on emptying its queue (`0x44ad19`, `cmp ecx, 0xbb8; jg`).
It caches each area's visibility to the enemy in a byte per area (`+0x11f4`,
1 or 2) and asks the game for each new candidate (`call [0xcc7a20]` at
`0x44ace3`), after its start test (`0x44a832`). fnTech3's version returns as
soon as it has taken 3,000 steps, has no ladder or disabled test, measures the
distance limit from the area's waypoint, rejects an endpoint within 40 units of
the enemy, and asks the game only once, after its start record is set up. The
original source's other conditions (its 0.1-unit projection margin, its
approach and occupancy penalties) are source observations not yet checked in
retail. Matching the hide search was left open here; the
[hide-search record](2026-10-15-wolfet-hide-search.md) has since done it.

## The pure gamestate budget of Jedi Outcast and Enemy Territory

A pure server publishes the checksums of its loaded paks in `sv_paks` unless
`SV_RemainingGameState` says they would not fit in the gamestate; then it runs
degraded pure and its clients are not checked. The [protocol 15 record](2026-10-02-jk2-protocol-15.md)
left Jedi Outcast measured as Quake Live.

**Observed.** Neither retail engine has a budget. `jk2mp.exe`'s
`SV_SpawnServer` tests `sv_pure` (`0x44dbce`), publishes `sv_paks` from
`FS_LoadedPakChecksums` (`0x42e080`, set at `0x44dbe5`), warns when it is
empty (`0x44dbf8`) and publishes `sv_pakNames` from `FS_LoadedPakNames`
(`0x42e0d0`, set at `0x44dc10`), with no size test. `etded.exe` does the same
(`0x42d271`; `0x4171f0`, set at `0x42d284`; warning `0x42d29b`; `0x417240`,
set at `0x42d2cc`). The [pure-maps record](2026-09-18-rtcw-mp-pure-maps.md)
found RTCW publishing unconditionally too.

**Inferred.** fnTech3's budget comes from Quake3e and is an engine policy, not
a retail contract. Its only job is to keep a server from a gamestate overflow
that would drop every client, so it has to measure the gamestate those
clients are sent: their wire's encoding, inside their wire's message limit.

**fnTech3 before this change** measured both games as Quake Live inside
Quake III's 16 KiB. Outcast's message limit is the same 16 KiB, but its
entity fields come in Raven's order, with `modelindex` 47th of 73 where
Quake Live has it 31st of 58, so an item or mover baseline costs Outcast
about two bytes more. Enemy Territory's message is 32 KiB with tables of its
own. The budget probe below gives, for one level with no item baselines, one
with 64 and one with 300:

| Profile | Measured as Quake Live (bytes free) | Its clients' gamestate leaves |
| --- | --- | --- |
| `jk2-mp` | 15,449 / 12,983 / 5,933 | 15,449 / 12,855 / 5,332 |
| `wolfet` | 15,449 / 12,983 / 5,933 | 31,833 / 29,268 / 21,852 |

On the 300-item level Quake Live's measure credited an Outcast server with 601
bytes its gamestate spends, more than the 512 bytes the estimate holds back, so
a pak list in the last 89 bytes the measure allowed would have overflowed the
gamestate, and the server would have dropped its clients with `gamestate
overflow`. The window opens at about 256 such baselines and widens with the
map. An Enemy Territory server was credited with half its space. None of the
six stock maps comes near that limit (the [pure record](2026-09-17-wolfet-pure.md)
admits retail clients on all of them), but a bigger map would have run
degraded pure although its own message had room, as RTCW's `mp_sub` did
before its own measure.

**Change** (`code/server/sv_client.cpp`): the estimate measures Enemy
Territory and Outcast on their own wires and limits, as it already measured
RTCW, Holomatch, Alice and Academy. Quake III and Quake Live keep Quake Live's
measure. Medal of Honor, the Quake III IHV Test and Q3Test are still measured
as Quake Live: Medal of Honor's baselines are its own records, written by
`SV_MOHAAWriteBaselines`, and need a measure of their own, which is open.

**Tests.** `tests/gamestate_budget_wire_tests.py` compiles the production
`SV_RemainingGameState` with the configstring, baseline and game-directory
writers it shares with `SV_SendClientGameState`, the gamestate part of
`SV_SendClientGameState` itself, the engine's message codecs and the real
profile catalog. For Outcast, Enemy Territory, RTCW multiplayer, Academy
multiplayer and Quake Live, the estimate must equal the client's message
limit less the 512-byte reserve and the bytes of that client's own gamestate,
plus the 256 command bytes and closing opcode the estimate adds, at all three
levels; it does, to the byte. Quake III's estimate must not exceed its
client's, and equals it here. Measuring Enemy Territory and Outcast as Quake
Live again fails five of the eighteen comparisons.
`tests/rtcw_gamestate_budget_tests.py` pins the new list of profiles.

## Holomatch's `sv_fps` was already served

The census repeated the [frame-time record](2026-10-08-stvef-frame-time.md)'s
note that `sv_fps` stays within 10 to 125 where `stvoyHM.exe` takes any value
from 1. Commit `60d3a85a`, the [server clock record](2026-10-09-stvef-holomatch-server-clock.md),
already registers it from 1 to 1,000 for both Elite Force profiles
(`SV_Init`, `code/server/sv_init.cpp`), citing `stvoy.exe` 1.20 at `0x426633`
and `stvoyHM.exe` at `0x43523b`, which set 10 for a value below 1; past 1,000
a tick is 0 ms. The census entry is stale. Nothing changes here.

## Not taken this round

The census's other entries remain as listed. The Quake III-only x64 build
still compiles the x86-only Academy ABI tests
([RTX entity shadows record](2026-10-11-rtx-entity-shadows.md)); other
sessions are editing those tests, so it was left for later.

## Presentation

No audio, rendering, input or interface path changes. Enemy Territory and Jedi
Outcast keep fnTech3's renderers, spatial audio, input and interface
features; no retail-exact option is added.

## Validation

A `git archive` export of `fbde92d4` with this record's files, configured as
the shared Windows x86 build with strict warnings, built all 1,541 targets.
Its 266 native tests passed and 2 were skipped. The complete Python suite
passed 2,724 tests and skipped 131; its one failure, in
`tests/rtcw_entity_collision_tests.py`, was Windows refusing to delete a
finished probe while cleaning up its folder, after the probe had passed, and
the file passed on a rerun. HEAD then moved by three commits that touch none of
this record's code; an export of the new HEAD with these files passed this
record's tests, the routing, budget and framing tests around them and the
release documentation tests. The JSON companion has each command and result.

## Unrelated observations

- Quake III's own `AAS_NearestHideArea` (`code/botlib/be_aas_route.cpp`)
  also addresses the update records by area number while Quake III sizes them
  by cluster, as in id's source. Nothing exports or calls it, so it is
  unreachable.
- `jk2mp.exe` publishes `sv_pakNames` beside `sv_paks` (`0x44dc10`), and
  fnTech3 publishes the names only for Enemy Territory. Whether a retail
  Outcast client needs them has not been tested.
