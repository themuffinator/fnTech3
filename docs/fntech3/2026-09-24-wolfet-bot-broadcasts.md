# Enemy Territory leaves bots out of broadcasts

## Scope

A server command sent to no particular client is a broadcast: the engine
queues it in the reliable window of every primed client. ET 2.60b passes over
a client whose entity carries `SVF_BOT`, as its GPL server does. fnTech3
passed over only RTCW's cast AI, so an ET bot was queued every broadcast. This
record establishes what that does to a bot, then makes the `wolfet` profile
skip it as retail does. It closes the bot item that
`2026-09-24-rtcw-et-broadcast-echo.md` left open.

Only the `wolfet` profile changes. RTCW's cast-AI rule is unchanged, and so is
every other profile's loop, including its bots. Commands addressed to one
client, the dedicated console's `broadcast:` echo and the long-command rule
are unchanged in every profile.

## Observed

### Sources

- ET's [`SV_SendServerCommand`](https://github.com/id-Software/Enemy-Territory/blob/40342a9e3690cb5b627a433d4d5cbf30e3c57698/src/server/sv_main.c)
  skips a client below `CS_PRIMED`, then one whose `gentity` is set and has
  `SVF_BOT` in `r.svFlags`, with Ridah's comment that AI need no messages.
  `SV_AddServerCommand` drops a client whose 257th command is unacknowledged:
  `MAX_RELIABLE_COMMANDS` is 256 in ET's `qcommon.h`.
- [`SV_BotGetConsoleMessage`](https://github.com/id-Software/Enemy-Territory/blob/40342a9e3690cb5b627a433d4d5cbf30e3c57698/src/server/sv_bot.c)
  is the only engine code that acknowledges a bot's commands. Its copy into
  the caller's buffer is commented out.
- The game's [`BotAI`](https://github.com/id-Software/Enemy-Territory/blob/40342a9e3690cb5b627a433d4d5cbf30e3c57698/src/botai/ai_main.c)
  drains that function every time a bot thinks, with Gordon's comment that
  this must be done to acknowledge the commands. On any frame that updates
  the bot library, `BotAIStartFrame` returns before bots think if no
  navigation file is loaded.
- [`G_AddBot`](https://github.com/id-Software/Enemy-Territory/blob/40342a9e3690cb5b627a433d4d5cbf30e3c57698/src/game/g_bot.c)
  and the bot branch of `ClientConnect` in `g_client.c` set `SVF_BOT`.
  `g_svcmds.c` has the `addbot` dispatch commented out and keeps `spawnbot`.
- Five more engine tests read `SVF_BOT`. `SV_DropClient` counts a flagged
  client as a bot. Configstring updates skip one in single-player and co-op
  game types only. `SV_CalcPings` gives it ping 0. `SV_SendClientMessages`
  sends it nothing, and `SV_SendClientSnapshot` returns once the snapshot is
  built.

### Retail engines: the broadcast loops

Read with capstone. Addresses are virtual addresses in each image.

| | ET 2.60b `et.exe` | ET 2.60b `etded.exe` | `WolfMP.exe` (`Wolf 1.4-MP`) | `WolfSP.exe` (`Wolf 1.32`) | `quake3.exe` (`Q3 1.32`) |
| --- | --- | --- | --- | --- | --- |
| SHA-256 | `4d1326c5936b61c5f878fc017e4aa0abddf13e008139e31fce2f13d57b26243e` | `7eb606913a56d6901e358a598fb0079120b2730e113a10fc963f90b777580cdf` | `7cf99a938cf18ed2f5aecd0be542cbae8868959e0cce24b4b6eea08991e321f5` | `07d206f61da5c80562ff60d68c645bca349b31adb5d7e0e7f186d61ad46683c7` | `80b0add0e014b863cf47cab6c997e0a54939236219a56910b7a3eb77dc2d44d4` |
| Loop | `0x448875..0x4488ad` in `SV_SendServerCommand` (`0x4487c0`) | `0x42f0e0..0x42f118` in `0x42f010` | `0x439fe2..0x43a018` in `0x439f40` | `0x4378bd..0x4378f3` | from `0x4397e6` |
| Primed test | `cmp dword ptr [esi], 3`, `jl` past the client, at `0x448875` | `0x42f0e0` | `0x439fe2` | `0x4378bd` | `0x4397e6` |
| Entity | `mov eax, [esi+0x48848]` at `0x44887a`; null goes on to the add | `0x42f0e5` | `[esi+0x40840]` at `0x439fe7` | `[esi+0x854]` at `0x4378c2` | none |
| Flag test | `test byte ptr [eax+0x128], 8` at `0x448884`, `jne` past the client | `0x42f0ef` | `test byte ptr [eax+0x248], 0x10` at `0x439ff1` | `0x4378cc`, the same | none; every primed client gets the command |

`0x128` is `r.svFlags` in fnTech3's frozen `fntech3_wolfet_sharedEntity_t`,
after its 288-byte entity state, `r.linked` and `r.linkcount`. `0x248` is
`r.svFlags` in both `fntech3_rtcw_sp_sharedEntity_t` and
`fntech3_rtcw_mp_sharedEntity_t`. In all three, `8` is `SVF_BOT` and `0x10`
is RTCW's `SVF_CASTAI`, which ET's source renames `SVF_POW`. So the RTCW loops
are exactly `SV_ClientIsRTCWCastAI`'s rule, and neither tests `SVF_BOT`: an
RTCW bot that is not cast AI gets every broadcast. Quake III 1.32 tests no
flag.

`etded.exe` has six `test byte ptr [eax+0x128], 8`: the broadcast loop, and
the five tests above. `SV_DropClient` has one at `0x427ebc`. The configstring
update at `0x42d568..0x42d584` calls the single-player and co-op game-type
tests before its flag test. `SV_CalcPings` sets ping 0 at `0x42e100..0x42e109`.
`SV_SendClientMessages` skips the client at `0x42fea7`. `SV_SendClientSnapshot`
tests the flag at `0x4300d2`, after building. `et.exe` has six as well, at
`0x44189c`, `0x446e33`, `0x447910`, `0x448884`, `0x4495d7` and `0x4497ee`.

### What a bot's reliable window holds, and what empties it

| | `et.exe` | `etded.exe` |
| --- | --- | --- |
| `SV_AddServerCommand` | `0x447820`; `cmp eax, 0x101` at `0x44783b` | `0x42e010`; `cmp eax, 0x101` at `0x42e02b` drops the client on its 257th unacknowledged command, and `movzx eax, cl`, `shl eax, 0xa` at `0x42e095` index 256 slots of 1,024 bytes at `+0x404` |
| `SV_BotGetConsoleMessage` | `0x43f050` | `0x425420` |
| Its only caller | the game import dispatch, `0x44363c..0x44364e` | `0x429d8c..0x429d9e`, reached by import 210 alone through the tables at `0x42b820` and `0x42b4bc` |

Import 210 is `BOTLIB_GET_CONSOLE_MESSAGE` in ET's `g_public.h`.
`SV_BotGetConsoleMessage` stores the server time as the client's last packet
time (`+0x48a28`). It returns zero when the acknowledgement (`+0x40408`) equals
the sequence (`+0x40404`). Otherwise it advances the acknowledgement by one
and returns whether that slot's first byte is set. It never touches the
buffer it is given.

The installation's `qagame_mp_x86.dll`
(`160a6218397865ad61071faa6316053713c03b9be80e5d05c7a5be2cab7f630b`) uses
import 210 in one place: `trap_BotGetServerCommand`, `0x20076950` (`push 0xd2`
at `0x2007695f`). That function has one caller, `BotAI` (`0x2001e830`), whose
loop at `0x2001e8d0..0x2001e8e5` calls it with a 1,024-byte stack buffer
until it returns zero. What reaches that loop:

- `BotAI` is called only from `BotAIThinkFrame` (`0x20020160`), at
  `0x2002035b`. The caller first checks that the bot's client is connected:
  `cmp dword ptr [eax+0x5ac], 2` at `0x20020343`.
- `BotAIThinkFrame` has two callers. `BotAIStartFrame` calls it at
  `0x20020688` when `bot_enable` is set and the server is dedicated, or no
  local client has thought for 200 ms. `ClientThink` calls it at `0x200392bf`
  for a listen server's client 0.
- `BotAIStartFrame` (`0x200203b0`) returns before that call whenever the bot
  library's accumulated time has reached the think time and
  `trap_AAS_Initialized` (`0x200769b0`, import `0x130`) is false, by the
  `je 0x20020690` at `0x200204b7`.

The installation's three paks ship no `.aas` file. ET's dedicated server
calls the game's bot frame once per server frame, with the time advanced in
whole 50 ms server frames at the default `sv_fps` (`SV_Frame` in the GPL
`sv_main.c`), and `bot_thinktime` defaults to 50. So on a stock dedicated
server the accumulated time reaches the think time on every call, no bot ever
thinks, and nothing acknowledges a bot's commands. That chain is an inference
from the addresses above and the source; the live run below confirms its
outcome.

### Who makes an `SVF_BOT` client

Only the game sets the flag. In the retail module:

- `G_AddBot` (`0x2003bb20`) writes `SVF_BOT` into the new client's entity at
  `0x2003beb3`: `g_entities` is at `0x206be1a0` with a stride of `0x57c`. It
  adds `SVF_POW` at `0x2003bec2` for a prisoner of war. Both writes sit
  between the "All player slots are in use" print (`0x2003bc1a`) and "Unable
  to add bot" (`0x2003bf04`).
- `ClientConnect` sets it at `0x2003fdb6` for a bot, beside its
  "BotConnectFailed (no respawn)" (`0x2003fe13`) and "BotConnectfailed"
  (`0x2003fe48`) returns. `G_AddBot` calls it that way. The engine does too,
  for each bot client it reconnects when a map loads or restarts, as the GPL
  `sv_init.c` and `sv_ccmds.c` do and fnTech3's `SV_SpawnServer` and
  `SV_MapRestart_f` do.
- `G_AddBot`'s only caller is `G_SpawnBot` (`0x2003bf40`), at `0x2003c851`.
  `G_SpawnBot` is called from two places. One is `Svcmd_SpawnBot`
  (`0x2003c8a0`, call at `0x2003c986`), which `ConsoleCommand` runs for
  `spawnbot` at `0x200760cc..0x200760de`. The other is one script action
  (`0x20022270`), named in the map scripts' table as `spawnbot` (entry at
  `0x200b394c`) and in the bot scripts' table as `SpawnBot` (`0x200af20c`).
- The module has no `addbot` command. `G_AddBot` takes its bot from
  `scripts/bots.txt`, which the game loads only when `bot_enable` is set at
  game start. `pak0.pk3`
  (`712966b20e06523fe81419516500e499c86b2b4fec823856ddbd333fcb3d26e5`) ships
  it, with one `WolfBot` entry. No map script in the three paks says
  `spawnbot`.

So a stock ET server has `SVF_BOT` clients only when it starts with
`bot_enable 1` and a server command, map script or bot script spawns one.

### Retail at run time

`scripts/probe_wolfet_bot_broadcast.py --retail-server` runs `etded.exe` on a
private desktop, with an isolated home, loopback networking and no clients.
Its config loads `goldrush` with `bot_enable 1`, runs `spawnbot`, then runs two
stages:

- **Burst.** 300 console `say` lines in one frame, each a broadcast `chat`.
  That is more than the 256-command window.
- **Flood.** 300 steps, ten console frames apart. Each changes
  `sv_hostname`, a configstring update addressed to each primed client, and
  says one line.

The game printed "Fatal: AAS not initialized" and still connected and began
the bot as client 1. `status` listed it with ping 0 and address `bot`.

| Stage | What `etded.exe` did |
| --- | --- |
| Burst | Nothing overflowed and the bot stayed. Its last-message age went from 400 to 750 ms, so nothing had refreshed its packet time. |
| Flood | When the hostname reached `fntech3-flood-248`, the server printed its pending commands and disconnected the bot (`ClientDisconnect: 1`). The final `status` lists no client. |

The pending commands are numbered from 1, so the bot had acknowledged none
since it was made. The 256 stored ones are all addressed:

- 248 `cs 0` serverinfo updates, carrying hostnames `fntech3-flood-0` to
  `fntech3-flood-247`;
- `cs 22`, `cs 23`, `cs 24`, `cs 690` and `cs 755`;
- two `tinfo`s and one `aftc`.

None of the 248 `chat "console: floodN"` broadcasts said in those steps is
among them. The 257th line repeats the first, because the dump reads the ring
past its end. The 258th is the update that overflowed. The run quit with exit
code 0, and the installation's file inventory was unchanged. The
[evidence file](evidence/2026-09-24-wolfet-bot-broadcasts.json) holds the
commands, the listings and the counts.

So, in retail:

- **Whether a bot's window fills and drops it:** it does, with addressed
  commands alone. Configstring updates, `tinfo` and `aftc` fill it, and the
  server drops the bot on the 257th.
- **Whether anything acknowledges its commands:** only the game's bot AI.
  That AI does not think on a stock dedicated server, so nothing does.
- **What broadcasts add:** nothing. None ever enters a bot's window.

### fnTech3 before this change

`SV_SendServerCommand` left out only a client for which
`SV_ClientIsRTCWCastAI` held, and that is false outside RTCW. So an ET bot got
every broadcast.

Its window is `SV_ReliableCommandLimit`, which fnTech3 takes from the client's
wire profile. A bot never sets up a channel, so it keeps whatever the slot
last held. A slot no network client has used keeps the legacy Quake III
profile, 64 commands. A slot an ET player left keeps ET's 256.

A scratch harness compiled the production `SV_ReadGameEntity`,
`SV_ClientIsRTCWCastAI`, `SV_ClientIsWolfETBot`, `SV_ExpandNewlines`,
`SV_AddServerCommand` and `SV_SendServerCommand` with the real game-data view.
It bound an ET record carrying `SVF_BOT`, added one player that acknowledged
everything and a bot that acknowledged nothing, and broadcast `chat` commands:

| Loop | Bot's slot | Window | Bot dropped after |
| --- | --- | --- | --- |
| `2d35919` | fresh | 64 | 65 broadcasts |
| `2d35919` | used by an ET player | 256 | 257 broadcasts |
| this change | either | 64 or 256 | not within 10,000 |

fnTech3 cannot yet make an ET bot at all, so the difference has no live
effect today. The same probe, with `--server`, ran two `fntech3.ded.exe`
builds from `git archive` exports for Windows x86: `2d35919`
(`b8a2156ee8255fa8b69508f665275659bf2e604e02f1c85fd34d91544308b86c`), and
`2d35919` with this change
(`d65a87a99938cef80c79202592cedfb5c58c644394c622a611787b00be2b8cf9`). On
both, `spawnbot` failed the same way. The bot library printed
`file botfiles\chars.h not found` for `bots/default_c.c`, whose line 9 is
`#include "botfiles/chars.h"`, and then could not load any skill. The retail
game then faulted at `qagame_mp_x86.dll+0x90ae6`, inside its string
formatter. Neither server made a bot, reached the burst or quit by itself.
Each was stopped at the 90 s timeout with the installation unchanged. See
Open.

## Change

- `SV_ClientIsWolfETBot` (`code/server/sv_game.cpp`) is false outside
  `wolfet`, and for a client with no entity. Otherwise it reads the client's
  entity through the checked game-data view, as `SV_ClientIsRTCWCastAI` does,
  and reports `SVF_BOT`. ET's module flags pass through the RTCW flag table,
  where `0x8` is `SVF_BOT`. That table does not map ET's `0x10` to RTCW's cast
  bit. An entity the view cannot read ends the map with `ERR_DROP`, as it
  does for the cast test, rather than being taken for a player.
- `SV_SendServerCommand` (`code/server/sv_main.cpp`) passes over a primed
  client that is an ET bot, in the same test as a cast AI and after the
  primed test, as retail orders it. The flag decides, not the address type,
  as in retail: an engine bot slot whose entity lacks the flag still gets
  broadcasts, and a network client whose entity has it does not.

The ET, RTCW, Quake III sources, the retail executables and the retail module
were read as reference observations. No text from them is incorporated.

## Evidence

- `tests/wolfet_bot_broadcast_tests.py` compiles the six production functions
  above with the real game-data view and the entity-flag tables. It binds ET
  records and writes each client's flags at `+0x128`, where `et.exe` reads
  them.
  - Its nine clients: a player; bots flagged `SVF_BOT` and `SVF_BOT|SVF_POW`;
    a player carrying every bit but `SVF_BOT`, RTCW's cast bit included; an
    engine bot slot whose entity is unflagged; a flagged network client and a
    flagged bot, both still primed; and a connected and a free client holding
    pointers no view could read.
  - It sends 2,800 `print` and `chat` broadcasts, half as a dedicated and
    half as a listen server, and a 1,106-byte broadcast as each. The players
    get each one, and the flagged clients get none, even with long-string
    support. No window overflows though no bot acknowledges anything.
  - Commands addressed to a bot still reach it, and the module records are
    never written. An unreadable entity or a stale binding ends the map.
  - In every other profile, `SV_ClientIsWolfETBot` reads nothing and is
    false. Outside RTCW, every primed client gets 300 broadcasts with no
    entity readable at all. With bound records, a flagged bot still gets them
    in Quake III and RTCW MP, and RTCW MP still leaves its cast AI out.

  Eight in-memory mutations of the production code must each make it fail,
  and do:

  - no ET skip, as before;
  - a skip in every profile;
  - the address type deciding instead of the flag;
  - RTCW's cast bit deciding;
  - any flag deciding;
  - unprimed clients being read;
  - an unreadable entity counting as a player;
  - addressed commands leaving bots out too.
- `tests/rtcw_cast_messages_tests.py` now compiles the production
  `SV_ClientIsWolfETBot` too and passes unchanged for Quake III, Quake Live,
  RTCW SP and RTCW MP. `tests/server_broadcast_echo_tests.py` gets a stub for
  the new function and passes. `tests/rtcw_cast_lifecycle_tests.py`,
  `tests/untrusted_asset_bounds_tests.py` and
  `tests/input_system_source_tests.py`, which read these functions, pass.
- The retail run, the fnTech3 runs and the scratch harness above. The
  [evidence file](evidence/2026-09-24-wolfet-bot-broadcasts.json) records
  each server's hash, command line and outcome.
- Both dedicated servers were built from a `git archive` of `2d35919` for
  Windows x86 with MSVC, `debugoptimized` and strict warnings, with the RTCW,
  Elite Force and ET development options on. The candidate added only this
  change and built without a warning.
- A second export of `2d35919`, with this change and nothing else, built all
  1,200 targets the same way without a warning. `fntech3.exe` had SHA-256
  `d99f2341f27c4efbf12cf985daa0db72135ff319d04637e57a70642e36d97232` and
  `fntech3.ded.exe`
  `7c2e0697bd27f6dbf2f85565e358fd03b87fd9b58eb85326aa0b76cd0c2424ed`. Its 116
  Meson C++ tests pass and two skip: `fnql_audio_loopback`, because OpenAL's
  `ALC_SOFT_loopback` was unavailable, and `fnql_retail_factory_probe`, which
  finds no Quake Live without `FNQL_RETAIL_QL_PATH`. The whole Python suite
  passes in the same export: 1,833 tests, with 76 skipped. The scratch harness
  gave the same counts there as in the shared tree.

## Open

- **ET bots in fnTech3.** `spawnbot` cannot load ET's own bot character.
  ET's loader sets the script base folder to `botfiles` only while it opens
  the file, then clears it before parsing (`BotLoadCharacterFromFile` in the
  GPL `be_ai_char.c`). So the file's `#include "botfiles/chars.h"` resolves
  from the game root. fnTech3's bot library keeps `botfiles` set, as Quake
  III's does, and looks for `botfiles/botfiles\chars.h`. The retail game's
  failure path then faults. By the GPL source, the likely cause is that
  `BotAISetupClient` prints `bot_settings_t.skill`, a `float`, with `%d` before
  a `%s`; that is an inference. Until ET bots can be made, this change is
  verified by the unit test and by retail's behaviour, not by an fnTech3 bot.
- **A bot's window depends on its slot's history.** It is 64 or 256 commands
  where retail always has 256, in every Wolfenstein profile. An unacknowledged
  ET bot in a fresh slot still overflows on its 65th addressed command, where
  retail allows 256.
- **The other five `SVF_BOT` tests.**
  - fnTech3 sends configstring updates to ET bots in every game type. Retail
    skips them in single-player and co-op.
  - fnTech3's `SV_DropClient`, `SV_CalcPings` and snapshot sender key on the
    address type, not the flag. They agree for an engine bot the module has
    flagged.
  - fnTech3 builds a snapshot for every bot each frame, asking ET's snapshot
    callback about the entities a bot would see. Retail's
    `SV_SendClientMessages` builds nothing for a flagged client.
- **The console-message copy.** `SV_BotGetConsoleMessage` copies each command
  into the module's buffer, where ET's copies nothing. The retail module
  never reads the text.

## Unrelated observations

- **The long-command rule differs from both retail engines.**
  - ET drops any command longer than 1,022 bytes before it chooses
    recipients, addressed or broadcast: `cmp ecx, 0x3fe` at `et.exe`
    `0x4487fa` and `etded.exe` `0x42f059`.
  - `WolfMP.exe`'s `SV_SendServerCommand` has no length test at all: it goes
    from formatting straight to the recipient test (`0x439f66..0x439f77`).
  - fnTech3 still sends such a command to long-string clients in every
    profile.
