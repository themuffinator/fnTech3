# Holomatch's game unlinks the world's own record, 2026-10-09

A free-for-all Elimination game on fnTech3's dedicated Holomatch server stopped
with `SV_SvEntityForGentity: bad gEnt` during the mode sweep of the
[frame-time record](2026-10-08-stvef-frame-time.md), where the retail server's
game ran to `Last Man Standing.` Run again under a debugger, the server stopped
the same way in the seventh of seven games. The game had asked the server to
unlink `g_entities[ENTITYNUM_WORLD]`, the world's own record, which lies in its
entity array past the count it publishes. `stvoyHM.exe` accepts that record;
fnTech3 had bound only the published entities for Holomatch, and refused it.
The server now binds the game's whole entity array, as it already did for RTCW
and Enemy Territory. The [JSON companion](2026-10-09-stvef-world-record.json)
holds identities, addresses and the runs.

## Retail

`stvoyHM.exe` 1.20 (`5b5b6e82ce7ac0d720a45ab1fa9df32930c846d6c51847fa0c158ebc86487dff`):

| What | Retail | Address |
| --- | --- | --- |
| A game's entity pointer | `VM_ArgPtr`: NULL for 0, else the VM's data base plus the offset, without a mask | 0x43b3b0 |
| `SV_SvEntityForGentity` | `bad gEnt` only for NULL or a record whose number is outside 0 to 1023; it never asks where the pointer lies | 0x43170c..0x431728, and the inlined copies at 0x4355f1 and 0x435e52 |

Holomatch's `vm/qagame.qvm` (`pak2.pk3`) keeps `g_entities` at VM address
0x65c4c (stored into `level.gentities` at instruction 1268) and publishes it to
`trap_LocateGameData` (instruction 1333) with 624-byte records and 764-byte
clients. The world's record is entity 1022, numbered 1022 by its spawn.

## What the game does

Under cdb (`fntech3.ded.exe` `663f4162…`), the stopped server's stack ran from
`SV_BotFrame` through the game's compiled code to `SV_GameSystemCalls`
(import 31, `trap_UnlinkEntity`), `SV_UnlinkEntity` and
`SV_SvEntityForGentity`, with a pointer 0x9bb20 bytes past the bound entity
base, 624 bytes apart: record 1022, of 278 published.

With the change, cdb counted the game unlinking that record three times in one
Elimination game and once in another, without error. Under the interpreted VM
(`vm_game 1`), the QVM's own frames give the call chain at such an unlink: the
game's client think (function 86996, a bot's command run inside the bot
frame) calls its holdable-item code (function 86118, which places detpacks,
force fields and decoys), which calls function 166204, which calls function
154997, whose unlink (instruction 155366) takes the world's record. Function
154997 unlinks, traces, makes a temporary entity and links again, as Quake
III's `TeleportPlayer` does; that name is an inference.

## Change

- `SV_GameEntityArena` (`code/server/sv_game.cpp`) gives Holomatch the whole
  array, `MAX_GENTITIES` records, and `ServerGameDataView::BindNative`
  (`code/server/server_game_data.cpp`) admits that arena above the published
  count for Holomatch, as for RTCW and ET. A pointer must still be the start
  of a record inside the array, whose number is below 1024; snapshots still
  send only the published entities.

The other profiles are unchanged. No retail text or code was incorporated.

## Evidence

- `tests/server_game_data_tests.cpp` binds a 1024-record Holomatch arena with
  the QVM's 624-byte records and 64 published: the world's record reads by
  index and by pointer, the end of the arena is refused, a capacity of 1025
  is refused and a span one byte short fails. `tests/world_owner_trace_tests.py`
  expects `MAX_GENTITIES` for Holomatch and no arena for the Jedi profiles.
  Without the binding exception the first fails.
- Before the change: the mode sweep's free-for-all Elimination game and one
  of seven debugger runs stopped with `bad gEnt`.
- With it (`fntech3.ded.exe` `bada7c65…`, a `git archive` export of
  `8da1234c` with this change, the frame-time change and the
  [serverinfo change](2026-10-08-stvef-serverinfo.md)):
  ten free-for-all Elimination games all ended on `Last Man Standing.`, and of
  ten team Elimination games seven did and three ran out of time unfought,
  each with two bots in one team and two spectating, which is the game's own
  team rule. No game reported an error.
- The retail installation was unchanged by every run.
