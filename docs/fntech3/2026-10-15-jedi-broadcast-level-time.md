# Jedi multiplayer snapshot broadcasts and level time, 2026-10-15

The [siege slice](2026-10-15-jka-siege.md) left two differences between
fnTech3's Jedi Academy multiplayer server and retail `jamp.exe` 1.01. Its
snapshot rule left an entity flagged `SVF_BROADCASTCLIENTS` (`0x2`) out for
every client the entity's mask does not name, as OpenJK's source does. And a
level after a map change started its game at time 0, where retail carried
on from the last level's time. This slice matches both to retail in
`jka-mp` and `jk2-mp`, and a third difference read in the same retail walk:
an entity the game marks permanent, which retail never sends. The
[JSON companion](2026-10-15-jedi-broadcast-level-time.json) records the
binaries, identities, commands and outcomes. No compatibility gate closes.

Implementation is independently authored. Retail `jamp.exe` 1.01,
`jk2mp.exe` 1.02a and their modules were read with capstone and pefile.
Pinned OpenJK and id's GPL Quake III, RTCW and Enemy Territory sources were
used only to name routines and fields (reference observation; no text
incorporated).

## The snapshot walk

### Observed, retail `jamp.exe` 1.01

`SV_AddEntitiesVisibleFromPoint` (`0x460e90`) walks every entity
(`0x460f70..0x4613bf`), in this order:

- An unlinked entity is skipped (`+0x230`, `0x460f8a`).
- So is one whose state's `eFlags` has `0x80`, Raven's `EF_PERMANENT`
  (`0x460f90`).
- A wrong `s.number` is repaired with a developer print (`0x460f9b`).
- The flags word (`+0x238`) is read once (`0x460fae`) and tested for
  `0x1` (no client), `0x100` (single client) and `0x800` (all but one
  client), against `singleClient` (`+0x23c`).
- An entity already added is skipped (`0x460ffb`).
- `SVF_BROADCAST` (`0x20`, `0x461011`), the viewer's own number
  (`0x461028`) and a two-word `broadcastClients` mask naming the viewer
  (`+0x298`, `0x46104f`) each add the entity past every visibility test.
- So does `isPortalEnt` (`+0x108`, `0x46105c`), as the siege slice found.
- The rest is a random-map distance test (`0x46107a`, OpenJK's `com_RMG`)
  or the area and cluster tests, a distance cull skipped while its value is
  `-1.0` (`0x46123e`, OpenJK's `g_svCullDist`), the add and the portal
  recursion (`0x461346`).

The flags word is never tested for `0x2`, and the mask only adds. OpenJK's
`codemp/server/sv_snapshot.cpp:437` adds an exclusion retail does not have.
Before the walk, `SV_BuildClientSnapshot` marks the viewer's own entity as
already sent (`0x4614cd`), as Quake III does, so the own-number add never
fires there; fnTech3 marks it too.

Permanent entities reach the client another way. `SV_CreateBaseline`
(`0x45e2c0`) keeps every linked entity from 1 up as a baseline, permanent
ones included. The client engine answers client-game import 231
(`CG_GETDEFAULTSTATE`, `0x412f95`) only for a baseline marked `0x80`
(`0x4118f0`, the test at `0x411909`), and the 1.01 client game adds those
in a loop of its own after the snapshot's (`cgamex86.dll`
`0x30035907..0x3003593c`, from the
[compatibility round](2026-10-15-compatibility-round.md)). An entity sent
as well is added twice a frame.

The installed game module (`jampgamex86.dll` 1.01) sets bits in the mask
(the word addressed at `0x2007051c`, `or` at `0x20070531`; another at
`0x20070642`). Of its 40 instructions that combine a constant into a word
at `+0x238`, none sets `0x2`. Its 98 stores from a register were not
traced, so that it never sets the flag is an inference.

### Observed, retail `jk2mp.exe` 1.02a

Outcast's walk (`0x44fcf0`) runs the same tests without Academy's own:
linked (`+0x128`), number repair, flags (`+0x130`) for `0x1`, `0x100` and
`0x800` against `singleClient` (`+0x134`, read at `0x44fd93` and
`0x44fdaa`), already added, then the three adds: `SVF_BROADCAST`
(`0x44fdde`), the viewer's number (`0x44fdf1`) and a two-word mask at
entity `+0x190` (`0x44fe1a`). There is no permanent skip, no portal-scene
add, no random-map test or cull, and no test of `0x2`.

The retail module (`vm/jk2mpgame.qvm` in `assets0.pk3`) fills that mask.
Its `G_UpdateClientBroadcasts` (procedure at instruction 96613) clears the
eight bytes at entity `+400` through import 100, then sets one bit per
client in two passes. One runs only in game type 2, the Jedi Master's,
for a client whose word at `+604` is set (instruction 96387). The other sets
the bit of every client whose active powers (`+836`) include bit 14, Force
Sight in Raven's numbering (instruction 96163). The stores are at
instructions 96354 and 96582.

fnTech3 read Outcast's `0x2` as Quake III's client mask, testing
`singleClient` as a mask, and never read the broadcast mask. A player the
module marked for another's Force Sight, or the Jedi Master, was sent only
when the viewer could see them.

### Implemented

- `fntech3::jka::snapshot::Broadcast()` (`code/server/server_jka_snapshot.hpp`)
  takes the state's `eFlags`. A permanent entity is never sent, ahead of
  everything that would force it. The `0x2` exclusion is gone; the
  broadcast, own-number, mask and portal adds are unchanged.
- `fntech3::jk2::snapshot::Broadcast()` is Outcast's rule: the same three
  adds, no permanent skip and no portal add.
- `SV_JediSnapshotBroadcast` (`code/server/sv_game.cpp`, formerly
  `SV_JKASnapshotBroadcast`) serves both profiles. Academy's native record
  is read in place; Outcast's, usually a QVM's, through the checked game-data
  view (`GetJK2BroadcastClients`). A record the view cannot read is left out.
- The snapshot walk calls it for both profiles, and Quake III's client-mask
  test on `0x2` is skipped for both (`code/server/sv_snapshot.cpp`).
- Outcast's engine half grows from 104 to 112 readable bytes, a 408-byte
  minimum record: `FNTECH3_JK2_R_BROADCASTCLIENTS` is `+104`, and
  `singleClient` (`+12`) is now evidenced (`code/compat/jk2_game_abi.h`).
  The retail module's records are 828 bytes.

### Retail and fnTech3 side by side

A synthetic game module (`.tmp/jka-broadcast-time/probe/jedi-probe.cpp`)
allocates a bot client, stands its player state at the map's first spawn
point and links thirteen entities, each isolating one rule: at that point
("in view") or at the first spawn point outside its PVS ("out of view").
It reads the bot's built snapshot back through game import 259
(`BOTLIB_GET_SNAPSHOT_ENTITY`) at frames 6, 120 and 130 of each stage. The
first frame after a map loads may owe up to 5,000 ms, run as up to 100 game
frames before any snapshot is built, so frame 6 can find none.

Retail tells a bot from its entity's `SVF_BOT` (`0x461969`), so the module
sets it; without it retail sends the bot its snapshot and the import reads
the next, empty frame. fnTech3's Academy server refuses import 259 by name,
and the retail module never calls it (`scan_game_import_calls.py`: 0
sites). fnTech3's two runtimes ran with one diagnostic case serving it,
identical in both and never committed, as retail serves it (`0x45b860`,
`0x455e10`).

| Entity | Placed | Retail `jamp.exe` 1.01 | fnTech3 before | fnTech3 after |
| --- | --- | --- | --- | --- |
| 40 | ordinary, in view | sent | sent | sent |
| 41 | ordinary, out of view | -- | -- | -- |
| 42 | `0x2`, mask names nobody, in view | sent | **left out** | sent |
| 43 | `0x2`, mask names nobody, out of view | -- | -- | -- |
| 44 | `0x2`, mask names the bot, out of view | sent | sent | sent |
| 45 | mask names the bot, out of view | sent | sent | sent |
| 46 | mask names client 7, out of view | -- | -- | -- |
| 47 | `EF_PERMANENT`, in view | -- | **sent** | -- |
| 48 | `EF_PERMANENT` and `SVF_BROADCAST` | -- | **sent** | -- |
| 49 | `SVF_BROADCAST`, out of view | sent | sent | sent |
| 50 | `isPortalEnt`, out of view | sent | sent | sent |
| 51 | `SVF_BROADCAST` and `0x2` naming nobody | sent | **left out** | sent |
| 52 | `EF_PERMANENT` and `isPortalEnt` | -- | **sent** | -- |

Every sample from frame 120 on, in all four stages (the first level, a map
change, a `map_restart` and `killserver` then `map`), at the wall clock and
at `fixedtime 75`, gave retail and fnTech3 after the same seven entities,
`40,42,44,45,49,50,51`. fnTech3 before gave `40,44,45,47,48,49,50,52`.

### Live

`.tmp/round-2026-10-15/jka-local-sets/probe_live.py` ran a windowed client
on a private desktop, with no network sockets and no input, as a spectator
of `mp/ctf3` (`g_gametype 8`), on runtimes built from the same export
before and after. It stood 147 units, then 14.6 units, from the speaker
whose `yavin_exterior` set the compatibility round heard twice, and dumped
the OpenAL world each time.

| Distance | fnTech3 before | fnTech3 after |
| --- | --- | --- |
| 147 units | loop gain 1.00: two calls of 209 summed and held at 255 | loop gain 0.82: one call, 209 of 255 |
| 14.6 units | 1.00 | 1.00 |

The speaker is permanent. The client game now names it once a frame, from
its baseline, as it would against a retail server. Both runs played to the
probe's end and exited normally with no refusal (Live subset). The
compatibility round's reading that the doubled call summed "as in retail's
mixer" holds for the mixer, but a retail server never sends the speaker
twice.

Outcast's retail server stops at its disc check (`0x44d00c`), so it has no
runtime comparison. The retail QVM ran Jedi Master (`g_gametype 2`) with
two bots for 60 seconds on fnTech3's before and after dedicated servers.
On both it bound its 828-byte records, both bots entered, and no error was
printed (Live subset). A no-input run sets up no Force Sight or Jedi Master
situation to watch, so the mask's effect was not observed live.

## The level clock

### Observed, retail `jamp.exe` 1.01 and `jk2mp.exe` 1.02a

Both run their game on the server's own clock, `svs.time` (`0xc317c4`,
`0xb0d674`), outside the level structure each spawn clears
(`0xb934e0..0xc317bc` at `0x45e73c`, `0xb176b8..0xb7a0d4` at `0x44d5bd`).
Academy's level structure has no time of its own: after the restart time
(`0xc317a8`) come four fields of Raven's.

| Use | `jamp.exe` | `jk2mp.exe` |
| --- | --- | --- |
| `GAME_INIT` | `0x45dd50` | `0x44cf54` |
| settling frames, +100 each | `0x45eaee..0x45eb62` | `0x44da5c..0x44da94` |
| `map_restart`'s frames | `0x456396..0x4563c6`, `0x4564ae..0x4564cf` | `0x447c03..0x447c2f`, `0x447cf2..0x447d14` |
| game frames, `svs.time += frameMsec` | `0x460850..0x46085e` | `0x44f374..0x44f37f` |
| snapshot server time | `0x460c69` | `0x44f916` |

Only `SV_Shutdown` clears it, with the rest of the server's static state
(Academy's at `0x45f406`). So a level after `map` starts where the last left
off, a `map_restart` likewise, and only `killserver` or a new process starts
at 0. Both write the clock itself into every snapshot, with no offset for a
client still loading.

fnTech3 kept Quake3e's per-level clock, `sv.time`, for both. `SV_ClearServer`
kept it unless `sv_levelTimeReset` is set, but `SV_SpawnServer` set it to 0
whenever no client was connected, so a dedicated server with no players
started every map at 0.

### Implemented

- `FRAME_KEEP_LEVEL_TIME` joins the per-profile clock rules
  (`code/qcommon/profile_frame_time.h`), set for `jka-mp` and `jk2-mp` only.
- `SV_KeepsLevelTime()` (`code/server/sv_init.cpp`) skips the empty-server
  reset for those profiles. `SV_LevelTimeResets()` keeps `sv_levelTimeReset`
  from clearing their clock or giving their clients an old time, so a
  snapshot carries the level time itself, as retail's does.
- `SV_Frame`'s silent restart of an empty server after twelve hours of level
  time treats those profiles as never resetting
  (`code/server/sv_main.cpp`). That restart is an inherited FnQL
  safeguard, not retail; it acts only when no human is connected.

### Retail and fnTech3 side by side

The same module prints the time each stage's `GAME_INIT` is handed and the
time at frame 130, when it issues the next stage's command. Its runs at
the wall clock and at `fixedtime 75` gave the same numbers on every
engine.

| Stage | Retail `jamp.exe` 1.01 | fnTech3 before | fnTech3 after |
| --- | --- | --- | --- |
| first level, `GAME_INIT` | 0 | 0 | 0 |
| after `map` | 6700 | **0** | 6700 |
| after `map_restart 0` | 13400 | 6700 | 13400 |
| after `killserver`, `map` | 0 | 0 | 0 |

Each `GAME_INIT` after the first takes exactly the time of the last frame
before the command: 6700 and 13400 at frame 130, where before's map change
started at 0 and its restart, carrying on from that, at 6700. The siege
slice's 5400 and 6550 came from its module issuing commands at frame 5,
inside the first frame after a load, which owes the load's time up to
5,000 ms.

## Other profiles

Both changes are scoped to `jka-mp` and `jk2-mp`. The snapshot walk asks
`SV_JediSnapshotBroadcast` only for them; every other profile keeps Quake
III's client mask on `0x2` and its walk unchanged, as a fixture checks for
`q3`. The clock rule is one bit in the table, and a test pins every
profile's other rules and that bit for all seventeen ids.

The other retail engines were surveyed, not changed:

- **Observed (source and binary):** id's GPL Quake III 1.32
  (`sv_game.c:909`, `sv_init.c:442`), RTCW SP (`sv_game.c:937`), RTCW MP
  (`sv_game.c:934`) and Enemy Territory (`sv_game.c:1054`) hand `GAME_INIT`
  `svs.time`. The installed `quake3.exe` 1.32, `WolfSP.exe`, `WolfMP.exe`,
  `et.exe` and `stvoyHM.exe` each advance a single clock in their spawn's
  settling frames; WolfSP, WolfMP and ET push that clock to `GAME_INIT`.
- **Inference:** the clears a scan found in `WolfSP.exe`'s, `WolfMP.exe`'s
  and `stvoyHM.exe`'s spawns do not cover their clock (in `quake3.exe` and
  `et.exe` it found none), so each likely keeps it across a map change as
  the Jedi engines do, where fnTech3's empty-server reset applies. Not run;
  recorded as open below.
- **Observed:** the single-player engines clear their clock in their spawn
  (`stvoy.exe` `0x425a9b`, `jasp.exe` `0x439d0d`, `jk2sp.exe` `0x437226`).

## Tests

- `tests/jka_snapshot_tests.cpp`: both decision functions for every viewer,
  the mask's words, every bound, and permanent entities against every add.
- `tests/jedi_snapshot_walk_tests.py`: the production walk with the
  production `SV_JediSnapshotBroadcast` and header over a fake game-data
  view: Academy sends `1,3,4,8,9` of the fixture's thirteen, Outcast
  `1,3,4,6,7,9`, Quake III `6,7,9`, or `1,6,7,9` with its mask bit; an
  unreadable Outcast record is left out.
- `tests/jedi_level_time_tests.py`: the production spawn block and
  `SV_ClearServer` for every profile, with `sv_levelTimeReset` on and off, a
  client connected or not, and a restart pending or not; and every
  profile's clock rules.
- `tests/server_game_data_tests.cpp`: the view reads Outcast's mask at
  `+104`, refuses a record that stops inside it, and answers for no other
  profile. `tests/jk2_abi_layout_tests.cpp` pins `+0x134`, `+0x190` and the
  408-byte record.
- `tests/wolfet_snapshot_tests.py` stubs the renamed hook.

Nine mutations, each applied to an export holding this slice and
reverted, were each caught: the OpenJK exclusion restored, the permanent skip removed
(also an unused parameter under strict warnings), Academy's `eFlags` not
passed, Outcast's client-mask skip removed, Outcast's mask unread, the clock
bit dropped for Academy, the empty-server reset or `sv_levelTimeReset`
applied to the Jedi profiles, and the view reading four bytes early.

## Validation

An export of `b589caf8` with this slice's files built all 1,545 targets
under strict warnings. Its meson tests ran 274 passes and two
environment skips of 276, with the Python suite run on its own: 2,775
passes, 131 skips and no failures. The changelog entry was added after
that run; the changelog, release, packaging, manual and documentation
tests then passed again. The edited engine files also compile with
`win32-dev`'s own commands. The runtimes above were built from an export
of `643016c6`, before and after this slice, and the commit's parent is
newer still; the code and test files this slice changes are the same in
all three.

## Remaining and unrelated findings

- **Other profiles' level clock.** Quake III, RTCW, Enemy Territory and
  Holomatch appear to keep one clock across map changes too (above), where
  fnTech3 restarts an empty server's level at 0. Quake Live was not read.
  Each needs its own measurement before its profile changes.
- **The server clock.** Retail's `svs.time` is the game clock and advances
  only with each tick. fnTech3's advances with real time for these
  profiles, as it does for every profile but Holomatch; that moves snapshot
  deadlines and timeouts, not the level time.
- **Time wrap.** Retail stops at `svs.time > 0x70000000` with
  `SV_Shutdown` and then queues `map_restart 0` (`0x460728`, `0x46073a`),
  which would find no server running (inference). fnTech3 restarts the map
  past `0x78000000`. Both are 21 days or more away.
- **Portal recursion.** Retail `jamp.exe`, `jk2mp.exe` and id's Quake III
  1.32 source recurse into a portal seen through another portal, bounded by the
  already-added test (`0x46139f`, `0x44ff5d`). This said fnTech3 recursed
  only from the viewer's own pass; it did not. The
  [portal recursion record](2026-10-15-snapshot-portal-recursion.md) shows
  fnTech3 already recursed as retail does, and corrects three other
  differences found there.
- **Academy's random-map and cull tests.** Retail's walk has a random-map
  distance test and a distance cull (`0x46107a`, `0x46123e`); fnTech3 has
  neither. Not examined further.
- **Game import 259.** Academy's `BOTLIB_GET_SNAPSHOT_ENTITY` stays refused
  in fnTech3; retail serves it and its module never calls it.
- **Baselines.** fnTech3's `SV_CreateBaseline` starts at entity 0 and
  retail's at 1; entity 0's baseline has number 0 and is never sent.
