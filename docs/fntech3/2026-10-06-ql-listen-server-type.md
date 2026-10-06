# Quake Live listen servers and their server type, 2026-10-06

The [Jedi listen-server record](2026-10-06-jedi-listen-server-game-types.md)
found that fnTech3 took Quake Live's game type 2, Race, for single player, so
a Race game started from the client answered no remote query. Retail
`quakelive_steam.exe` never asks the game type when it answers a packet, nor
the `ui_singlePlayerActive` flag its local match sets: its server type,
`sv_serverType`, decides. fnTech3 had kept every client-hosted Quake Live
server from every remote player, whatever its server type. It now follows the
server type. The [JSON companion](2026-10-06-ql-listen-server-type.json) holds
identities, addresses, command lines and every check. No gate closes: no
retail program ran.

## Identities

| File | SHA-256 |
| --- | --- |
| `quakelive_steam.exe` (2,107,904 bytes) | `c926fe9f6c851e00b3b9332e88903ad01f28fdd60454873891c0158f5ded1299` |
| `fntech3.exe`, runtime `ql-listen-before` | `206cdd57bf30bfc7e240dfa63d3b2815ed286922072c653e71dc3dac4e80b878` |
| `fntech3.ded.exe`, runtime `ql-listen-before` | `ef91c9aa7e0abeb82cd664501c7641037fe55c01814926af85c5c11f376fd113` |
| `fntech3.exe`, runtime `ql-listen-after` | `9b679783c1b97039bc4292d432391a988c81e1d82d3ce125b9c7361ba5b1a30e` |
| `fntech3.ded.exe`, runtime `ql-listen-after` | `38117580f9be9aeba8b81594460e2dc42614574eaae1e46b42c136fb9a2fe4f5` |

Both runtimes were built from a clean export of HEAD `5603d244` (MSVC 2022
x86, Meson `debugoptimized`, strict warnings, every development gate on);
`after` adds this change's three engine files. The installation was read only
and its `baseq3` inventory was unchanged after every run.

## Retail evidence

Read with pefile and Capstone; addresses are virtual addresses. No retail code
is incorporated. The installation holds no retail dedicated server, so
everything here is the client executable's own server.

- **Its server answers three kinds of packet without a connection.** The
  dispatcher (0x4e4340) takes `getchallenge` (to 0x4df430), `connect` (to
  0x4e0750) and `disconnect`; anything else is a `bad connectionless packet`.
  There is no `getinfo`, `getstatus`, `rcon` or `ipAuthorize`: Quake Live's
  browser lists servers through Steam.
- **An offline server hears only its own client.** `SV_PacketEvent`
  (0x4e4500) returns before anything else -- out-of-band and in-game packets
  alike -- while `sv_serverType` is 0 and the packet is not from the loopback
  (address type 2). The cvar is registered with `0` and archived (0x4e3b6a,
  pointer at 0x13e1818).
- **A LAN server refuses an outsider's challenge.** `SV_GetChallenge`
  (0x4df430) first tests `sv_serverType` 1 against the address's LAN test
  (0x4ee570: the loopback, 127.0.0.1, or a local interface's network) and
  answers `print\nServer is for LAN clients only.\n` (0x4df476). It then asks
  for a Steam auth token (`No Steam auth token.`, `Auth token too large.`) and
  answers `challengeResponse %i`. With 2 nothing is refused.
- **Neither reads the game type or the flag.** `g_gametype`'s pointer
  (0x13e1804) is read at 0x4682b2, 0x4688ab, 0x468b6b, 0x4de7e8 (the
  training-arena command, `scripts/%s.sp_arena`) and 0x4e375a, never in these.
  Every engine reference to `ui_singlePlayerActive` sets it: 0 at
  start-up (0x4cc67e) and shutdown (0x4e4018), three times from the client
  (0x4b8c7a, 0x4b8cfe, 0x4b8d4f), and 1, with `ui_priv`, when the map command
  runs on a client (`dedicated` 0; 0x4ddfff to 0x4de01e).
- **Race is a factory's game type.** The map command (0x4ddc20) sets
  `g_gametype` from the chosen factory (0x4ddf62); there is no `spmap`. The
  installed `race` factory's base type is Race, and `campgrounds` lists it.

The two tests give the server types their meaning: 0 offline, 1 LAN, 2
Internet, as fnTech3's description of the cvar already read. The QLSRP
reconstruction reads them the other way round (0 public, 2 offline,
`src/code/server/server.h:282` **[reference]**); retail's packet test
contradicts it, and retail outranks it.

## What fnTech3 did, and does now

| On a client-hosted Quake Live server | HEAD `5603d244` | Now | Retail client |
| --- | --- | --- | --- |
| Race, game type 2 | closes `getinfo`, `getstatus`, `getchallenge` | closes nothing | closes nothing |
| `ui_singlePlayerActive`, set by the map command | closes all three | closes `getinfo` and `getstatus` | read by nothing in the server |
| `sv_serverType` 0 (offline, default) | ignored | every packet from outside dropped | every packet from outside dropped |
| `sv_serverType` 1 (LAN) | ignored | a challenge from outside the LAN refused, retail's text | the same |
| `sv_serverType` 2 (Internet) | ignored | nothing refused | nothing refused |
| UDP `rcon` | answered | answered unless offline | not handled |

- `SV_GameTypeHidesListenServer` (`code/server/sv_main.cpp`) never hides a
  Quake Live server, as it never hides a Jedi Academy one.
  `SV_GameTypeIsSinglePlayer` still answers 2 for Quake Live; its remaining
  Quake Live uses are the map command's rule that turns 2 into a free-for-all
  before the factory sets the game type, and fnTech3's own `spmap` alias,
  which retail lacks. Neither changes here.
- `SV_ServerTypeDropsPacket` is the first test of `SV_PacketEvent`, before
  Steam's packet handling and the out-of-band dispatcher, in client builds
  only: no retail dedicated server was read, and fnTech3's dedicated Quake
  Live servers, which start at 0, would otherwise hear no one.
- `SV_ServerTypeRefusesChallenge` answers an outsider of a LAN server with
  retail's text, before rate limiting or any challenge work, in every build,
  since only a server set to 1 asks for it. Which addresses are on the LAN is
  fnTech3's own test (ioquake3's, which also counts the private ranges);
  where it and retail's differ was not measured.
- Quake Live's `getchallenge` no longer reads `ui_singlePlayerActive`. The
  map command still sets it, as retail's does, and fnTech3's own `getinfo`
  and `getstatus` answers, which retail's server never gives, still stay
  closed while it is set.

## Tests

- `tests/ql_server_type_tests.py` compiles production
  `SV_ServerTypeDropsPacket` and `SV_ServerTypeRefusesChallenge` in a client
  and a dedicated build and runs all sixteen profiles over server types -1 to
  3 and four addresses (the local client, a bot, a LAN address, an Internet
  address): only Quake Live reads its server type, an offline client build
  drops all but its own client, a dedicated build never drops, and a LAN
  server refuses only outsiders. Nine mutations each change the table. It
  also pins where the engine asks each, retail's text, and the flag's place.
- `tests/jedi_listen_server_tests.py` now expects Quake Live's listen rule
  never to hide, its Race still single player by the old rule, and a
  mutation hiding Race to fail.

## Live runs

`scripts/probe_ql_listen.py`, every process on a private desktop with its own
home, windowed, silent, without Steam (`com_steamIntegration 0`), IPv4 alone
on 127.0.0.1, with no input; packets paced at 1.1 seconds for the server's
per-address bucket.

Run `20261006-202245`, 38 checks, each run checked against its own build's
expected behaviour, all as expected. Every server ran Race on `campgrounds`
(`g_gametype` 2, printed by the host once the map loaded); the queries came
from 127.0.0.1, an address on the LAN; ProbeGuest is a second fnTech3 client.

| Case | `getinfo`, `getstatus`, `getchallenge`, `rcon` before (HEAD) | after | ProbeGuest before | after |
| --- | --- | --- | --- | --- |
| client hosts, `sv_serverType 0` | silent, silent, silent, answered | all silent | cannot join | cannot join |
| client hosts, `sv_serverType 1` | silent, silent, silent, answered | silent, silent, **answered**, answered | cannot join | **joins and enters** |
| client hosts, `sv_serverType 2` | silent, silent, silent, answered | silent, silent, **answered**, answered | cannot join | **joins and enters** |
| `fntech3.ded`, `sv_serverType 0` | all answered | all answered | -- | -- |

An earlier run of the same matrix (`20261006-201207`) failed its two LAN
cases, HEAD's and this change's alike: each host stopped logging seven lines
after its own player entered and handled no packet at all. Each followed a
host whose exit was still under way when the next started, the probe having
moved on after Windows refused to stop a process already exiting. The probe
now waits for every process to exit before the next case; the matrix above is
the run after that.

Every address on this machine is on the LAN, 127.0.0.1 included, so a LAN
server's refusal of an outsider was not seen live; the unit test covers it.

## What this does not establish

- No retail program ran: a retail client needs Steam's auth token, and no
  retail client has asked an fnTech3 Quake Live server anything here.
- Retail's dedicated server was not read; fnTech3's keeps its earlier rule.
- Which server type each Start Match choice sets was not read from the web
  UI; fnTech3's bridge starts the cvar at 0 (`code/client/cl_webui.cpp`).

## Presentation (invariant 11)

Not applicable: this changes which packets a server answers, not how anything
is drawn, heard or controlled.
