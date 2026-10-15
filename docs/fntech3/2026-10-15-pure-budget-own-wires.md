# The pure gamestate budget of Medal of Honor, the IHV Test and Q3Test

The [census-gaps record](2026-10-15-census-small-gaps.md) left three profiles
whose pure gamestate budget still measured Quake Live's message: Medal of
Honor (`mohaa`), the Quake III IHV Test (`q3-ihv`) and Q3Test (`q3test`). This
record settles them. Medal of Honor's budget now measures the gamestate its
clients are sent: its own 48 KiB message, its own baselines, and the sounds
its multiplayer game registers after the budget was first taken, which on
every stock map make that gamestate larger than Quake Live's whole message.
The IHV Test and Q3Test have no pure mode in any of their engines, and their
clients are sent a systeminfo that never carries a pak list, so no budget
applies to them; `sv_pure` now says it has no effect there instead of
measuring a list nothing sends. The changes were built and tested as HEAD
plus this record's files alone, without another session's uncommitted work.
The [JSON companion](2026-10-15-pure-budget-own-wires.json) has the
addresses, commands and outcomes. No compatibility gate closes.

The retail executables read here, none of them run:

| Executable | SHA-256 | Size | PE stamp |
| --- | --- | --- | --- |
| IHV Test `quake3.exe` (February 1999) | `e9fcaa0a5682e7f07d540aacee08cdb77f3ea40190719f5cecf31109a5135d37` | 725,042 | `0x36c4a216` |
| Q3Test 1.02 `quake3.exe` | `86da36081878d5586d5621709f48a7e621db3c24d33fa4b45838b08d01020d4a` | 688,180 | `0x37228c46` |
| Q3Test 1.03 `quake3.exe` | `313ed8d1b71296b834b806b64c0f5929eb5bdd05eb7ce96b9c7fdb7369ecbf79` | 692,276 | `0x3727f07d` |
| Q3Test 1.05 `quake3.exe` | `05579377e09286d658aef430d6fb45dc6c4fc4578988cea83afa51459768fcf8` | 696,372 | `0x3736948c` |
| Q3Test 1.06 `quake3.exe` | `a18c714fac103c4634fc08c88b2dd1c7b63720c8ddb8ec0f408e00d7ccb63db3` | 651,264 | `0x378fd2ba` |
| Q3Test 1.07 `quake3.exe` | `a507b42aa346dd7c67f0fdd33c42e88c3ee144d91bce3dea0a3fb06aa7368962` | 651,264 | `0x37940afc` |
| Q3Test 1.08 `quake3.exe` | `89a7dfb431c6738a13e1e44d0c2d76d69107c6e0791a64f6a49830e1a51c547b` | 655,360 | `0x37a67da7` |

`MOHAA.exe` 1.00 is SafeDisc-encrypted and cannot be read, so Medal of
Honor's engine side comes from OpenMoHAA's reconstruction (pinned at
`ab43a0de`), marked **[O]**, and from a published console log of the retail
1.11 engine's Linux port. Its game module, `gamex86.dll` 1.00
(`2f37ead1a91aae850dd8cdb29c6b5774864f6ae8654fb79d3593e45d9d3fc55c`, PE stamp
`0x3c0fa7ef`), and its data ran in fnTech3's dedicated server for the
measurements below, read-only from the installation.

## What the budget is for

A pure server lists the checksums of its loaded paks in `sv_paks` unless
`SV_RemainingGameState` says the list would not fit in the gamestate; then it
runs degraded pure and its clients are not checked. **Inferred**, as the
census-gaps record found: the budget comes from Quake3e and is an engine
policy, not a retail contract. Its only job is to keep a server from a
gamestate overflow that would drop every client, so it has to measure the
gamestate those clients are sent, in their wire's encoding and inside their
wire's message limit.

## The IHV Test and Q3Test: no budget applies

**Observed.** None of the seven executables above contains the string
`sv_pure` or `sv_paks`; the only `pure` in any of them is the C runtime's
"pure virtual function call". Each engine's client-command table, the verbs
its server answers from a client, names no pak proof:

| Engine | Table | Verbs |
| --- | --- | --- |
| IHV Test | `0x4623a0` | `userinfo`, `disconnect`, `download`, `nextdl` |
| Q3Test 1.02 | `0x469fc8` | `userinfo`, `disconnect`, `download`, `nextdl` |
| Q3Test 1.03 | `0x46b160` | `userinfo`, `disconnect`, `download`, `nextdl` |
| Q3Test 1.05 | `0x46b438` | `userinfo`, `disconnect`, `download`, `nextdl` |
| Q3Test 1.06 | `0x462158` | `userinfo`, `disconnect` |
| Q3Test 1.07 | `0x462188` | `userinfo`, `disconnect` |
| Q3Test 1.08 | `0x463188` | `userinfo`, `disconnect` |

There is no `cp`, so no client of these engines proves its paks and no
server asks. Their clients' systeminfo is their own: the IHV Test's carries
`serverid` and `mapChecksum` (`0x41986e..0x4198bd`, as the
[IHV ledger](Q3_IHV.md) records), and each Q3Test release's its server id,
`timescale` or `cheats` and its game's systeminfo cvars (`SV_Init` 1.02
`0x41bed0`, 1.06 `0x41cf90`, 1.07 `0x41d030`; [Q3Test ledger](Q3TEST.md)).

**fnTech3 before this change.** Both profiles default `sv_pure` to 0. Set to
1, the spawn measured the pak list as Quake Live and set `sv_paks` when it
fitted. But the clients of both are served by their own sessions:
`SV_PacketEvent` hands every packet to `SV_IHVNetPacketEvent` or
`SV_Q3TestPacketEvent`, whose command handlers know only the verbs above, and
their systeminfo is built from their releases' keys
(`SV_UpdateSysteminfoConfigstring`, `SV_Q3TestSysteminfo`). The list reached
no client and nothing was checked, and nothing said so.

**Change** (`code/server/sv_init.cpp`): the pure-pak step is now
`SV_SetPurePaks`. For these two profiles it publishes nothing and makes no
estimate, and when `sv_pure` is set it prints, once per level, that the game
has no pure mode, so `sv_pure` has no effect. Nothing a client is sent
changes.

## Medal of Honor

**Reference [O].** OpenMoHAA's `SV_SpawnServer` publishes `sv_paks` whenever
`sv_pure` is set, with no budget (`code/server/sv_init.c` lines 881-895), and
registers `sv_pure` off by default (1081-1083). Its gamestate
(`code/server/sv_client.c` `SV_SendClientGameState`, 824-897) carries every
configstring through `MSG_WriteScrambledBigString`, which for protocol 6 is
the plain big-string writer (`code/qcommon/msg.cpp` 586-608), then each
baseline as a delta from the null entity, the end byte, the client number and
the checksum feed; the server frame time follows only from protocol 15
(3965-3971). `MAX_MSGLEN` is 49,152 (`code/qcommon/qcommon.h` 251). After the
systeminfo is published (916) and the heartbeat sent (928), a multiplayer
game registers its sounds (933).

**Published observation.** A retail 1.11 dedicated server's log ("Medal of
Honor Allied Assault 1.11 linux-i386 Jul 23 2002", icculus.org `mohaa` list,
August 2002, `obj/obj_team3`) prints "PARSING UBERSOUND (SERVER)" straight
after "Spawning Entities Done". That 1.00's engine registers the sounds after
publishing `sv_paks`, as OpenMoHAA does, is **inferred**; nothing printed
between the two lines.

**Measured in the retail module.** 1.00's `RegisterSounds` (export 20,
`0x310d7c50`) tests neither the game type nor `dedicated`, and its
`aliascache` handler caches each kept line's file through `soundindex`
(`0x310e2810`), so each such sound takes a configstring of the gamestate
([contracts record](2026-09-24-mohaa-contracts.md)).

**fnTech3 before this change** measured Medal of Honor as Quake Live inside
16 KiB. Its baselines are its own entity states, kept by the game adapter
and written by `SV_MOHAAWriteBaselines`; the server's own baseline list stays
empty for it, so the measure counted none of them. And the measure ran
before the game registered its sounds.

**Probe.** An instrumented export of HEAD with these changes, a dedicated
server only, loaded each of the eleven stock multiplayer maps from the retail
1.00 installation (`sv_pure 1`, `net_enabled 0`, an isolated home, game type 1
for `dm/`, 4 for `obj/`). It printed, at each measure, HEAD's own
`SV_RemainingGameState` beside the new one, and at the end of the spawn the
gamestate `SV_SendClientGameState`'s own text writes for client 0. The
instrumentation was never in the tree; the companion has the build, the
binary's hash and every line.

| Map | Baselines (bytes) | Configstrings before the sounds | Sounds | Client gamestate | HEAD's measure, bytes free | New, before the sounds | New, after the sounds | Left with the list |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `dm/mohdm1` | 57 (955) | 154 | 364 | 20,550 | 10,722 | 42,536 | 27,891 | 27,816 |
| `dm/mohdm2` | 21 (464) | 144 | 364 | 19,866 | 10,915 | 43,219 | 28,574 | 28,500 |
| `dm/mohdm3` | 16 (319) | 141 | 364 | 19,707 | 10,929 | 43,379 | 28,733 | 28,659 |
| `dm/mohdm4` | 60 (1,070) | 189 | 364 | 20,723 | 10,664 | 42,362 | 27,717 | 27,643 |
| `dm/mohdm5` | 13 (264) | 136 | 364 | 19,585 | 10,975 | 43,479 | 28,856 | 28,781 |
| `dm/mohdm6` | 40 (821) | 168 | 364 | 20,481 | 10,635 | 42,583 | 27,959 | 27,885 |
| `dm/mohdm7` | 89 (1,502) | 189 | 364 | 21,179 | 10,640 | 41,906 | 27,261 | 27,187 |
| `obj/obj_team1` | 22 (485) | 141 | 381 | 20,766 | 10,734 | 43,018 | 27,674 | 27,600 |
| `obj/obj_team2` | 149 (2,866) | 212 | 381 | 23,776 | 10,106 | 40,008 | 24,664 | 24,590 |
| `obj/obj_team3` | 89 (1,862) | 238 | 381 | 24,615 | 8,263 | 39,169 | 23,826 | 23,751 |
| `obj/obj_team4` | 112 (2,073) | 284 | 381 | 24,796 | 8,293 | 38,988 | 23,645 | 23,570 |

The pak list was 72 bytes with its key: the installation's six paks. Every
map published it before and after this change. The measures move by a byte
or so between runs, with the random server id and checksum feed.

What the table shows:

- **The gamestate is bigger than Quake Live's whole message.** A client of
  any stock map is sent 19,585 to 24,796 bytes. The 364 sounds of the `dm/`
  maps and the 381 of the `obj/` maps take 14,623 to 15,344 bytes of it, all
  registered after the budget was taken. HEAD's measure, taken again after
  the sounds, finds its own 16 KiB full (it reads -511 or -512).
- **HEAD was right on the stock maps only by luck.** It measured 4.9 to
  7.6 KB of gamestate, configstrings without baselines, against 16 KiB, and
  credited 8,263 to 10,975 free bytes; the real room under Medal of Honor's
  48 KiB is 23,570 to 28,781 bytes after the list. **Inferred:** a map whose
  configstrings before the sounds come to two or three times the stock maps'
  would have fallen to degraded pure under HEAD's measure, though its own
  message had room.
- **A budget taken before the sounds can be wrong on Medal of Honor's own
  wire too.** The test's tight level (below) fits its list before its sounds
  and not after: without a second measure its clients' gamestate overflows
  the 48 KiB message and every client would be dropped.

**Change.** `SV_RemainingGameState` (`code/server/sv_client.cpp`) measures
Medal of Honor on its own wire: its 48 KiB limit, its configstring encoding,
its end byte (11), and its baselines through `SV_MOHAAWriteBaselines`, as
`SV_SendClientGameState` sends them. In `SV_SpawnServer`, after
`SV_MOHAASpawnDone` -- where a multiplayer game registers its sounds --
`SV_SetPurePaks` measures the list again with the sounds in and the
systeminfo is republished; a list that no longer fits is dropped, and the
server runs degraded pure. The first measure and the systeminfo keep
OpenMoHAA's place in the spawn. No client has been sent a gamestate by then,
so the second measure changes nothing a client sees on the stock maps, where
both measures publish.

## Tests

`tests/gamestate_budget_wire_tests.py` compiles the production
`SV_RemainingGameState` with the gamestate writers it shares with
`SV_SendClientGameState`, the gamestate part of `SV_SendClientGameState`
itself, Medal of Honor's baseline writer (`server_mohaa_net.hpp`,
`server_mohaa_game.hpp`), `SV_SetPurePaks` and the end of Medal of Honor's
spawn (`sv_init.cpp`), the engine's message codecs, Medal of Honor's codec
(`code/qcommon/mohaa_msg.cpp`) and the real profile catalog:

- For Outcast, Enemy Territory, RTCW multiplayer, Academy multiplayer, Quake
  Live and now Medal of Honor, the estimate equals, to the byte, the client's
  message limit less the 512-byte reserve and its own gamestate plus the 256
  command bytes and closing opcode the estimate adds, at three levels; Quake
  III's must not exceed its client's. Medal of Honor's levels carry 0, 64 and
  300 baselines of its own (48,218, 46,108 and 40,422 bytes free).
- A Medal of Honor spawn with 150 other configstrings and 381 sounds keeps a
  72-character list, with 28,653 bytes left. A spawn whose list fits only
  before its sounds (13,655 bytes of them, 15,522 bytes of room before them
  and a 2,858-byte list) drops it once they are in, and its gamestate leaves
  1,866 bytes.
- On the IHV Test and Q3Test a pure server publishes nothing, asks for no pak
  list and prints the warning; without `sv_pure` it says nothing.
- Quake III still publishes a list that fits through the new `SV_SetPurePaks`.

Each of five mutations restores an earlier behaviour and fails its own check:

| Mutation | Fails |
| --- | --- |
| Enemy Territory and Outcast measured as Quake Live | 6 of the 21 estimate comparisons |
| Medal of Honor measured as Quake Live | its three estimates (15,451 against 48,218 free with no baselines), and the room spawn, which falls to degraded pure |
| Medal of Honor's estimate without its own baselines | its 64- and 300-baseline estimates (47,032 and 44,810 against 46,108 and 40,422) |
| No second measure after Medal of Honor's sounds | the tight spawn, which keeps its list, so that its clients' gamestate overflows the 48 KiB message |
| The IHV Test and Q3Test measured as pure servers | both no-pure checks: each publishes the list |

`tests/rtcw_gamestate_budget_tests.py` pins the new list of profiles measured
on their own wires.

## Presentation

No audio, rendering, input or interface path changes. The warning is a
console line. No retail-exact option is added.

## Validation

A `git archive` export of `aa0e7049` with this record's files, configured as
the shared Windows x86 build with strict warnings, built all 1,543 targets.
Its 272 native tests passed but for 2 skipped. The complete Python suite
passed 2,731 tests and skipped 131. Its 30 failures and errors were all in
the documentation, player manual and packaging tests
(`generate_docs_tests.py`, `player_manual_tests.py`,
`release_packaging_tests.py`), and came from the export itself: its shared
documents had been written from a later HEAD's text, whose docs index links a
record (`2026-10-15-jka-camera-shake.md`) the `aa0e7049` tree lacks, so the
player manual could not be built. HEAD then moved by five commits from other
sessions, which touch none of this record's code or tests. An export of the
new HEAD, `15b0766f`, with these files, committed in its own repository,
passed every test that reads a file this record changes, and the
documentation, manual and packaging tests: 74 files, 676 tests passed and one
skipped, a symlink this account lacks the privilege to create. Two more
commits landed meanwhile; an export of `8779edb5` with these files passed the
documentation, manual and packaging tests and the budget, framing and spawn
tests again (133 passed, the same one skipped). The commit sits on
`4747f6f4`; it and `dc26d8a8`, the two commits since, touch none of these
files but the shared documents. The JSON companion has each command and
result.

## Open

- Quake III and Quake Live keep Quake Live's conservative measure, as
  before. Elite Force, Jedi Academy and Jedi Outcast single player are still
  measured as Quake Live; they serve only a loopback client and default
  `sv_pure` to 0, and were not part of this record.
- Configstrings a game sets once its level runs are covered only by the
  512-byte reserve, for every profile.
- 1.00's own engine order around the sounds is inferred from OpenMoHAA and
  the 1.11 log; its executable cannot be read.

## Unrelated observations

- A Medal of Honor dedicated server started with `net_enabled 0` and a
  startup script of `map`, `wait 6000`, `echo` and `quit` loaded the map but
  ran neither the `echo` nor the `quit` within 40 seconds; the probe stopped
  it once its log had the level's last line. Not investigated.
