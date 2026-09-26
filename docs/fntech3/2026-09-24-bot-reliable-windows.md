# A bot rings its profile's reliable window

## Scope

The server keeps each client's reliable commands in a ring, and drops a
client whose unacknowledged commands would overflow it. fnTech3 took a
client's ring size, its window, from the client's wire profile. A bot never
sets up a channel, so it rang whatever wire profile its slot last held.
This record pins each profile's bot window to its retail engine, makes every
bot ring that window, and keeps the ring's indices consistent when a bot
takes a slot a network client left. It closes the window item that
`2026-09-24-wolfet-bot-broadcasts.md` left open.

RTCW SP, RTCW MP and ET bots now ring 256 commands in every slot, and Jedi
Academy and Jedi Outcast multiplayer bots 128. Quake III, Quake Live and
Elite Force bots ring 64 as before: every wire those profiles accept rings
64. Network clients, their windows and every rule that keys on a wire
profile are unchanged, Quake Live's `%` rule included.

## Observed

### Retail engines: one window for every client

Read with capstone. Addresses are virtual addresses in each image. Each
engine's `SV_AddServerCommand` counts the new command, compares the
unacknowledged count with a constant, and indexes its ring with the matching
mask. Its bot reader, `SV_BotGetConsoleMessage`, which is how a bot's AI
acknowledges its commands, indexes with the same mask. No engine chooses
the window per client.

| Engine | SHA-256 | `SV_AddServerCommand` | Overflow test | Index | Window | Bot reader | Bot allocator |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `quake3.exe` (`Q3 1.32`) | `80b0add0e014b863cf47cab6c997e0a54939236219a56910b7a3eb77dc2d44d4` | `0x4395b0` | `cmp ecx, 0x41` at `0x4395c7` | `and 0x3f` | 64 | `0x432620` | `0x4318c0` |
| `quakelive_steam.exe` | `c926fe9f6c851e00b3b9332e88903ad01f28fdd60454873891c0158f5ded1299` | `0x4e40c0` | `cmp ecx, 0x41` at `0x4e40db` | `and 0x3f` | 64 | `0x4dda50` | `0x4dcd30` |
| `WolfSP.exe` (`Wolf 1.32`) | `07d206f61da5c80562ff60d68c645bca349b31adb5d7e0e7f186d61ad46683c7` | `0x437770` | `cmp ecx, 0x101` at `0x43778d` | `and 0xff` | 256 | `0x430480` | `0x42f850` |
| `WolfMP.exe` (`Wolf 1.4-MP`) | `7cf99a938cf18ed2f5aecd0be542cbae8868959e0cce24b4b6eea08991e321f5` | `0x439e80` | `cmp ecx, 0x101` at `0x439e9d` | `and 0xff` | 256 | `0x432be0` | `0x431fc0` |
| `et.exe` (ET 2.60b) | `4d1326c5936b61c5f878fc017e4aa0abddf13e008139e31fce2f13d57b26243e` | `0x447820` | `cmp eax, 0x101` at `0x44783b` | `movzx eax, cl` | 256 | `0x43f050` | `0x43ef20` |
| `etded.exe` (ET 2.60b) | `7eb606913a56d6901e358a598fb0079120b2730e113a10fc963f90b777580cdf` | `0x42e010` | `cmp eax, 0x101` at `0x42e02b` | `movzx eax, cl` | 256 | `0x425420` | `0x4252f0` |
| `stvoyHM.exe` (1.20) | `5b5b6e82ce7ac0d720a45ab1fa9df32930c846d6c51847fa0c158ebc86487dff` | `0x434280` | `cmp edx, 0x41` at `0x434296` | `and 0x3f` | 64 | `0x42e070` | `0x42d330` |
| `jamp.exe` (1.01) | `e8062d3948ff05917b48e47d8eec0ee43bcc6fb03924410712412404ddfee28b` | `0x45f490` | `cmp ecx, 0x81` at `0x45f4a2` | `and 0x7f` | 128 | `0x455db0` | `0x455050` |
| `jk2mp.exe` (1.02a) | `2f08dcf9a089cb66062766f4922fda63a9e553de893c174b497dfefeb7a1577a` | `0x44e2e0` | `cmp ecx, 0x81` at `0x44e2f7` | `and 0x7f` | 128 | `0x4476e0` | `0x4469c0` |

Every ring above holds 1,024-byte commands (`shl 0xa`), except `WolfSP.exe`'s,
which reaches its commands through a helper (`0x4366a0`) with the same
`and 0xff` index. `quake3.exe` also inlines the add twice into
`SV_SendServerCommand` (`0x439660`), with the same test and mask both times.

The engines with no bot library ring commands too, and have no
`SV_BotFreeClient` error string, as the others do:

- `stvoy.exe` (`8f7ccadb52b4230689c5b3275177f7ec7060a30bfaa3ade4e7b3fb0cda0dff74`,
  `0x425eb0`), `jasp.exe`
  (`9351a8de230fcd577e2b474889d08f38982450306767a6823dd659482566d6ce`,
  `0x43a200`) and `jk2sp.exe`
  (`0d3c635ee5b20ac32fc957e00cf21e65ebc1584112b11f6ae026706dcc0d0e0c`,
  `0x437700`) keep 64 copied command strings, indexed with `and 0x3f`. Each
  compares before counting the new command (`cmp ecx, 0x40`, `jle`), so it
  refuses only a 66th.
- `alice.exe` (`8aa22917b7707f0e1a7c4dbe9a3c38332b1873ae2059174ac5329de26921fc18`)
  rings 128 at `0x455a10` (`cmp edx, 0x81`, `and 0x7f`), as its wire profile
  already records.
- The IHV test's `quake3.exe`
  (`e9fcaa0a5682e7f07d540aacee08cdb77f3ea40190719f5cecf31109a5135d37`) has no
  `Server command overflow` string. Its netchan carries one reliable block at
  a time, and its bots live in the game's own client table
  (`code/compat/q3_ihv_server_clients.hpp`), never in a server client slot.

### What a bot's slot keeps

Each allocator in the table takes the first free slot, from slot 1 in RTCW
and ET, or the slot ET's game asks for. It writes the entity handle, its
number, the state, the last packet time, the address type and the rate.
None writes the reliable counters. `quake3.exe` writes `+0x1083c`,
`+0x10908`, `+0x14ab8` and `+0x14aa0` and leaves the sequence at `+0x10404`
and the acknowledgement at `+0x10408`; `etded.exe` writes `+0x48848`,
`+0x48a28`, `+0x546d8` and `+0x546c0` and leaves `+0x40404` and `+0x40408`. So a bot takes over whatever its slot's last
occupant left unacknowledged. Retail rings every client alike, so those
commands stay at the indices the bot reader looks for. A bot reads what the
bot before it left unread.

### fnTech3 before this change

`SV_ReliableCommandLimit` (`code/server/server.h`) returned
`Netchan_ReliableCommandLimit( client->netchan.wireProfile )`.
`Netchan_Setup` is the only code that assigns `wireProfile`, and
`SV_DirectConnect` calls it for a network client after clearing the slot.
`SV_BotAllocateClientSlot` sets neither, so a bot rang the slot's history:
Quake III's 64 where no network client had been, and the last network
client's window otherwise.

The wires a network client can bring depend on the profile's protocol
families and `SV_DirectConnect`'s rule that a protocol at or below 68 is
framed as the legacy contract unless it names one of its own. By that code:

| Profile | Protocols | Wires, and their windows |
| --- | --- | --- |
| `quake3` | 66, 67, 68, 71 | Quake III's four, all 64 |
| `quakelive` | 68, 71, 91 | Quake III's and Quake Live's, all 64 |
| `stvef-mp` | 24, 26 | Holomatch's and, for 26, Quake III's: 64 |
| `rtcw-sp`, `rtcw-mp` | 49 and 50, 60 and 61 | RTCW's 256; iortcw's 50 and 61 get Quake III's 64 |
| `wolfet` | 84 | ET's 256 |
| `jka-mp`, `jk2-mp` | 26, 15 | Quake III's 64 |

A compiled harness measured every profile in the catalog. It ran the
production `SV_ReliableCommandLimit`, `SV_BotAllocateClientSlot`,
`SV_AddServerCommand` and `SV_SendServerCommand`, and counted the addressed
commands a bot held before "Server command overflow":

| Profiles | Fresh slot, before | After a network client, before | Any slot, now |
| --- | --- | --- | --- |
| `quake3`, `quakelive`, `stvef-mp` | 64 | its wire's: 64 for every wire they accept | 64 |
| `rtcw-sp`, `rtcw-mp`, `wolfet` | 64 | its wire's: 256 for RTCW's and ET's, 64 for iortcw's | 256 |
| `jka-mp`, `jk2-mp` | 64 | 64 | 128 |

`stvef-sp`, `jka-sp`, `jk2-sp` and `alice` offer their games no bot allocation,
and `q3-ihv` keeps its bots in its own table, so none of them seats a bot in a
client slot. The harness's numbers for every wire in every profile are in
the [evidence file](2026-09-24-bot-reliable-windows.json).

Before this change the index math stayed consistent only because a bot kept
both the slot's wire profile and its counters. A window taken from the
profile instead would read what a network client left under the client's
own window. An iortcw client in an RTCW slot rang 64, so a bot there with
256 would read those commands from the wrong indices. Where the client's
window was the larger, its leftovers alone could overflow a new bot's first
command.

### Quake Live's `%` rule

`SV_AddServerCommand` stores `.` for every `%` in a command for a client whose
wire is Quake Live's, so what retail Quake Live clients read, and hash, agrees
with what the server stored. `quakelive_steam.exe` stores every command as
given, with `Q_strncpyz` (`0x4d8f40`, called at `0x4e415f`), and its bot
reader hands a bot that text (`0x4dda50`). A Quake Live bot keeps its slot's
wire, so its text depends on its slot: as given in a fresh slot, with `.`
after a protocol 91 player. This change leaves that as it was; see Open.

## Change

- `comGameBotPolicy_t` (`code/qcommon/game_profile.h`) gains
  `reliableCommands`, the window of a bot's slot. The catalog
  (`code/qcommon/game_profile.cpp`) sets it from the table above: 64 for
  `quake3`, `quakelive`, `stvef-sp`, `stvef-mp`, `jka-sp` and `jk2-sp`; 128
  for `jka-mp`, `jk2-mp` and `alice`; 256 for `rtcw-sp`, `rtcw-mp` and
  `wolfet`; and 0 for `q3-ihv`, which rings no commands.
- `SV_ReliableCommandLimit` returns the profile's window for a client whose
  address type is `NA_BOT`, and the wire's for every other client, as before.
  Every reader of a bot's ring uses it: `SV_AddServerCommand`,
  `SV_BotGetConsoleMessage` and the location rewrite in `SV_InjectLocation`.
- `SV_BotAllocateClientSlot` (`code/server/sv_bot.cpp`) seats no bot when the
  profile's window is zero. It still leaves the slot's counters and wire
  profile as it finds them, as retail does, but compares the window the slot
  last rang with the bot's. When they differ, the pending commands were
  indexed under another mask, so it acknowledges them all rather than read
  them from the wrong slots. A bot after a bot, or after a network client of
  the same window, is handed what that occupant left, as in retail.

The retail executables were read as reference observations. No text from
any of them, or from any source, is incorporated.

## Evidence

- `tests/bot_reliable_window_tests.py` compiles the production
  `SV_ReliableCommandLimit`, the profile catalog, `SV_BotAllocateClientSlot`,
  `SV_BotAllocateClient`, `SV_BotFreeClient`, `SV_BotGetConsoleMessage`,
  `SV_ExpandNewlines`, `SV_AddServerCommand` and `SV_SendServerCommand`, and
  runs every profile in the catalog:
  - Each profile's window is the retail one pinned in the test, and the test
    fails for a profile with none pinned. Each non-zero window is a power of
    two within the engine's 256-command storage.
  - In a fresh slot, a bot holds exactly its window of unacknowledged
    commands, its AI is handed them in order, and the next overflows it. Its
    wire profile is left as the slot held it.
  - After a network client of every ringed wire, from either kind of
    address, a bot rings its profile's window. Leftovers from the same window
    are handed to it and count against it; leftovers from another window are
    dropped.
  - After a bot, the next bot is handed what the first left, as in retail.
  - A profile whose window is zero seats no bot and touches no slot, for
    `q3-ihv` and for a zero-window copy of every profile.
  - A network client rings its wire's window, for every wire value.
  - A Quake Live bot's `%` still follows its slot's wire.

  Eight in-memory mutations must each make it fail, and do: a bot ringing its
  slot's wire, as before; every client ringing its profile's window; a bot
  keeping leftovers from another window; a bot never handed a previous bot's
  leftovers; a zero-window profile seating bots; the bot reader indexing by
  the slot's wire; a bot being given its profile's own wire; and RTCW MP
  pinned at Quake III's window.
- Two existing fixtures that seat a bot by hand now set its profile's
  window: `tests/rtcw_cast_messages_tests.py` and
  `tests/wolfet_bot_broadcast_tests.py`. A third,
  `tests/server_broadcast_echo_tests.py`, seats no bot and now defines
  `Com_GameProfile` for the link. All three pass, and the second's eight
  mutations still fail as before.
  `tests/rtcw_bot_client_tests.py`, `tests/rtcw_cast_lifecycle_tests.py`,
  `tests/server_drop_notice_tests.py`, `tests/untrusted_asset_bounds_tests.py`
  and `tests/input_system_source_tests.py`, which read these functions, pass
  unchanged.
- **At run time.** `scripts/probe_bot_reliable_window.py` runs a fnTech3
  dedicated server on a private desktop with an isolated home, loopback
  networking and no clients. It loads a map, makes one bot with the game's
  own command, and sends that bot 300 `tell` lines in one frame. Each `tell`
  is a `chat` command addressed to the bot alone, so the burst overflows the
  bot's window, and the server prints what was pending. Two servers were
  built from one snapshot of the shared working tree for Windows x86. The
  baseline lacks this change's four source files' hunks, and nothing else
  differs.

  | Server | Profile | Bot | Held | Window | Overflowed on |
  | --- | --- | --- | --- | --- | --- |
  | baseline | `wolfet`, goldrush | slot 1, `spawnbot` | 9 game commands, 55 `tell`s | 64 | the 65th |
  | this change | `wolfet`, goldrush | slot 1, `spawnbot` | 9 game commands, 247 `tell`s | 256 | the 257th |
  | baseline | `quake3`, q3dm1 | slot 0, `addbot sarge 1` | 64 `tell`s | 64 | the 65th |
  | this change | `quake3`, q3dm1 | slot 0, `addbot sarge 1` | 64 `tell`s | 64 | the 65th |

  The nine game commands are the ET game's configstring updates (`cs 22`,
  `cs 23`, `cs 24`, three `cs 690`, `cs 755`), an `aftc` and a `tinfo`, still
  unacknowledged when the burst began. With no navigation file loaded,
  nothing acknowledged the ET bot's commands, as on retail `etded.exe`, which
  dropped its bot on the 257th (`2026-09-24-wolfet-bot-broadcasts.md`).
  Every run quit cleanly with the installation unchanged, and each server's
  hash was the same before and after its runs. The snapshot carried other
  sessions' uncommitted
  work, including the bot-library changes of
  `2026-09-24-wolfet-bot-characters.md`, which let `spawnbot` make an ET bot at
  all. The [evidence file](2026-09-24-bot-reliable-windows.json)
  records the snapshot, both servers' hashes, each command line and the
  counts.
- The shared tree built with the change for Windows x86 with MSVC,
  `debugoptimized` and strict warnings. The one target that failed was
  another session's new Alice test. Its 167 Meson C++ tests pass and two
  skip.
- An export of the staged index alone, `2d35919` with the other sessions'
  staged work, took this change's hunks: all of them but the six catalog
  entries and two Jedi rows that exist only in unstaged work. It built all
  1,200 targets the same way without a warning. Its 116 Meson C++ tests pass
  and two skip, and its whole Python suite passes: 1,837 tests, with 76
  skipped. The new test also passes built for x64, where its fixtures take
  the Quake III-only configuration.

## Open

- **Quake Live bots' text.** A Quake Live bot's commands keep `%` in a fresh
  slot and get `.` after a protocol 91 player, where retail hands every bot
  the command as given. The rule exists for retail clients' readers, which
  a bot does not have.
- **RTCW and the Jedi titles at run time.** No RTCW bot was flooded: an RTCW
  SP dedicated server with no player spawns no cast AI, and RTCW MP's paks
  ship no bot files. Their windows rest on the retail disassembly and the
  unit test, as do Jedi Academy's and Jedi Outcast's, which need their own
  development builds. None of the fourteen retail engines read here has a
  `tell` command, so no retail window was flooded here.
- **Leftovers across windows.** Dropping a network client's leftovers when a
  bot with another window takes its slot has no retail counterpart, since
  retail windows never differ. Only the iortcw protocols in RTCW and the
  Jedi profiles' protocols reach it.

## Unrelated observations

- **Some protocols are framed as Quake III's.** RTCW SP and MP accept
  iortcw's protocols 50 and 61, and Jedi Academy and Jedi Outcast
  multiplayer accept 26 and 15. All four are at or below 68, so
  `SV_DirectConnect` treats them as legacy connections, and
  `Netchan_SelectWireProfile` and `fnql::protocol::Find` give them Quake III's
  wire and contract: Quake III's message layout and a 64-command window. That
  is a reading of the code; no such client was connected.
