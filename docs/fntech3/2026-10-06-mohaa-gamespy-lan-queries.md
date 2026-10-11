# Medal of Honor's GameSpy LAN queries, 2026-10-06

A Medal of Honor: Allied Assault server answers two kinds of browser. The
game port, 12203, takes Quake III's `getinfo` and `getstatus`; beside it, a
port of its own from 12300 takes GameSpy's query protocol, which GameSpy's
browsers, the later games' in-game lists and third-party server tools speak.
fnTech3's `mohaa` server answered only the first. It now answers GameSpy's LAN
queries too, and a listen server hosting a team match, which had answered
neither browser nor any remote player, is now listed and joined. The
[JSON companion](2026-10-06-mohaa-gamespy-lan-queries.json) records the
source, runtimes, commands and every check. No gate closes: `MOHAA.exe` is
SafeDisc-encrypted and cannot run here, and no GameSpy client is installed,
so no retail program has read these answers.

## Evidence

The facts below are of three kinds, marked as the
[contracts record](2026-09-24-mohaa-contracts.md) marks them: read from 1.00's
data or plain game library, **[O]** read from
[OpenMoHAA](https://github.com/openmoh/openmohaa/tree/ab43a0def6c4feccedae36fdb58c2e007d7e4c35)'s
reconstruction of 2015's engine, and **[I]** inferred.

**What 1.00's own menus ask** (`Pak0.pk3`, read):

- Join LAN Game (`ui/joinlangame.urc`) is a `LANGameClass` list whose Refresh
  button runs `localservers`, and whose Join button joins the chosen server.
  OpenMoHAA's `CL_LocalServers_f` broadcasts `getinfo` to the game ports, and
  its `UILANGameClass` lists the `infoResponse`s **[O]**. fnTech3's client and
  server already did both ([contracts record](2026-09-24-mohaa-contracts.md#wire)).
- Join Internet Game (`ui/joininternetgame.urc`) offers Connect to IP Address
  and Launch GameSpy (`launchgamespy`), which started GameSpy Arcade, a
  separate program.
- No menu in `Pak0.pk3` to `Pak5.pk3` makes a `FAKKServerList`, the GameSpy
  list OpenMoHAA's `cl_uiserverlist.cpp` serves. That list's LAN search
  (`ServerListLANUpdate`, ports 12300 to 12316) belongs to the later games'
  menus **[O]**. The [legacy-server record](2026-10-05-legacy-server-hosting.md)
  and the ledger had taken it for 1.00's LAN list. 1.00's LAN list asks the
  game port, which fnTech3's dedicated servers answer, so their conclusion
  that a retail client could reach one only by address does not follow
  **[I]**; no retail client has run to show it either way.
- Each Start New Server menu (`ui/multiplayerstart_*.urc`) has a check box
  linked to `ui_gamespy`, which `MENU.CFG` archives as 1; OpenMoHAA's
  `UI_StartDMMap_f` passes it to the server as `sv_gamespy` **[O]**.

**What the server answers on its query port [O].** OpenMoHAA's server
(`code/gamespy/sv_gamespy.c`) opens GameSpy's query-and-reporting SDK at
`SV_Startup` on the first free of a hundred ports from `net_gamespy_port`
(12300), closes it at `SV_Shutdown`, and leaves it shut for a single-player
game or `sv_gamespy 0`. The SDK files it carries
(`sv_gqueryreporting.c`, `gserverlist.c`, `gserver.c`) are GameSpy's own code
and were read only to learn the protocol; no text of them or of OpenMoHAA is
used. The protocol, as written down in `code/compat/gamespy_query.hpp`:

- A query is one datagram, read up to 255 bytes, naming backslash-framed
  words: `\status\`, or any of `\basic\`, `\info\`, `\rules\`, `\players\`,
  and `\echo\<text>`. A first byte `;` is GameSpy's CD-key service;
  `\secure\<challenge>` asks a server to prove itself to a master.
- Whatever order a query names them in, the parts are answered basic, info,
  rules and players (all four for `\status\`), then the echo. The reply is
  `\key\value` pairs in datagrams of at most 1,400 bytes, each ending
  `\queryid\<queries answered>.<packet from 1>`; the last carries `\final\`
  before it. A part that fits a packet is not cut across two.
- A LAN search broadcasts the eight bytes `\status\` to each port from 12300
  to 12316, takes every source whose reply holds `\final\` for a server,
  asks it `\status\` again at that port, and times the first packet for its
  ping. A browser reads a server's `hostname`, `hostport` (the game port it
  connects to), `mapname`, `gametype`, `numplayers`, `maxplayers` and
  `gamever`, the last against its own version.
- The keys: basic `gamename` (`mohaa`), `gamever`, `location` (`sv_location`,
  1); info `hostname`, `hostport` (`net_port`), `mapname` (after its folder),
  `gametype` (`g_gametypestring`), `numplayers`, `maxplayers` (less private
  slots), `gamemode` (`openplaying`), `gametype_i` (`g_gametype`),
  `dedicated`, `pure`; rules `timelimit`, `fraglimit`, `rankedserver`
  (`g_rankedserver`); and per player `player_N`, `frags_N` (the kills stat,
  21), `deaths_N` (the deaths stat after it, 22) and `ping_N`. An empty cvar's
  key is left out, as the engine's info strings leave it.

**Where fnTech3 departs from OpenMoHAA, and why.**

- `sprinton` (`sv_sprinton`) and `realism` (`g_realismmode`) are left out:
  1.00's `gamex86.dll` registers neither (its cvar names hold
  `g_rankedserver`, `g_gametypestring`, `timelimit`, `fraglimit` and
  `dmflags`, and no such pair). `allowlean`, OpenMoHAA's `dmflags` bit 18,
  is left out because what that bit means to 1.00's game is not
  established. `minplayers` and `botskill` are OpenMoHAA's own, for its bots.
- `gamever` is `1.00` **[I]**: OpenMoHAA reports its target game's `1.12`
  and its own version. A browser of another version marks the server as
  such, which is right: protocol 6 is 1.00's alone.
- `dedicated` is whether the server is fntech3.ded, where OpenMoHAA's listen
  server reports its menu's `ui_dedicated`; fnTech3's client cannot become a
  dedicated server.
- Players are numbered from 0 as listed. OpenMoHAA does the same and notes
  that 2015's engine numbered them by slot, leaving gaps a tool stops at
  **[O]**. A client counts once connected, as `getinfo` counts it; players in
  private slots are listed and counted too, as OpenMoHAA's are, where
  `getinfo`'s count leaves them out, while `maxplayers` excludes the private
  slots in both.
- A query naming nothing it answers -- the CD-key service, `\secure\` alone,
  the SDK's `\packets\` demonstration, anything else -- gets no reply, where
  the SDK sent a lone `\final\`. There is no master's key here to answer
  `\secure\`, so a `\validate\` is never sent: GameSpy's masters are gone and
  nothing tells one of the server (AGENTS.md invariant 10).
- Answers go out as a query arrives, where the SDK answered at the next
  server frame: the query socket is read with the game's in `NET_Sleep`, so a
  browser's ping is the network's. Each address is held to `getinfo`'s limit,
  in the same per-address buckets (a burst of ten, then one a second), and all
  answers together to a burst of ten, then ten a second, since a `\status\`
  answer is many times its query.

## What was wrong

- **No GameSpy answers.** Nothing listened from 12300, so GameSpy's browsers
  and tools saw no fnTech3 Medal of Honor server, on a LAN or by address.
- **A listen server's team match passed for single player.** The engine's
  single-player test (`SV_GameTypeIsSinglePlayer`) compared `g_gametype` with
  Quake III's single-player type, 2. Medal of Honor's is 0; its 2 is
  Team-Match: 1.00's `gamex86.dll` sets `g_gametypestring` from `g_gametype`
  with a switch at `0x310e5a50` (0 Single-Player, 1 Free-For-All, 2
  Team-Match, 3 Round-Based-Match, 4 Objective-Match). On a listen server
  that test silences `getinfo`, `getstatus` and `getchallenge`, so a team
  match started from Start New Server was invisible to 1.00's own Join LAN
  Game list and refused every remote player, while a single-player game
  answered them. A dedicated server never applies the test.
- **No deaths.** The engine's reading of 1.00's player record kept the kills
  stat as the score and dropped the deaths stat after it.
- **The GameSpy box did nothing.** Start New Server read `ui_gamespy` and
  ignored it.

## What changed

- **A query socket** (`code/qcommon/net_ip.c`, `net_query.h`): a second IPv4
  UDP socket beside the game's, on `net_ip`'s interface, bound on demand to
  the first free of a given run of ports. `NET_Sleep` waits on it with the
  game's sockets and hands each datagram, at most 64 a wake, to its owner's
  handler; answers go out from it, so they come from the port asked. It is
  never opened through a SOCKS relay or without IPv4, and `net_restart` and
  shutdown close it. Windows' reports of an unreachable earlier answer, and
  of a datagram larger than 1,400 bytes, are skipped. Every other profile
  leaves it closed.
- **The protocol** (`code/compat/gamespy_query.hpp`): the parsing and the
  answers above, as a header of its own with no engine dependency.
- **The Medal of Honor server** (`code/server/server_mohaa_gamespy.hpp`,
  included by `sv_main.cpp`): each server frame of the `mohaa` profile opens
  the port while a multiplayer game runs and `sv_gamespy` is set, from
  `net_gamespy_port` (12300), and prints `GameSpy LAN queries answered on UDP
  port <n>`; a change of either cvar takes effect at once, `net_restart` is
  followed, a failed search is tried again every ten seconds, and
  `SV_Shutdown` closes it. `sv_location` (1) is the basic answer's region.
  All three cvars are OpenMoHAA's names **[O]**; none is latched here. The
  other profiles register none of them.
- **The single-player test** takes 0 for Medal of Honor. Its other callers
  serve Enemy Territory alone or the match report RTCW's and Enemy
  Territory's games send, none of which a Medal of Honor server reaches.
  Jedi Academy and Jedi Outcast number their game types differently again
  (OpenJK's Jedi Academy puts single player at 5 and Jedi Master at 2
  **[O]**); they are not changed here.
- **Deaths** (`server_game_data.cpp`): 1.00's stat 22 is kept as the engine's
  `PERS_KILLED`, beside the kills as `PERS_SCORE`; only the GameSpy player
  list reads it.
- **Start New Server** sets `sv_gamespy` from its GameSpy box, as OpenMoHAA
  does **[O]**.

The client's LAN list is unchanged. It asks the game ports with `getinfo`, as
1.00's does, which every 1.00 server and OpenMoHAA's answer; GameSpy queries
would add only servers whose game port lies beyond the four it scans, at the
cost of a second answer from every server to merge. (Since the
[October 8 record](2026-10-08-mohaa-gamespy-lan-search.md) the list makes
GameSpy's search as well, as a second route to a server.)

No audio, rendering, input or interface path changed (AGENTS.md invariant
11).

## Checks

**Unit** (`tests/mohaa_gamespy_query_tests.py`, compiled with clang): query
parsing (framing, the 255-byte limit, the first NUL, the CD-key, `\secure\`
and `\packets\` queries), pairs (empty values, backslashes), the map name
after its folder, the exact answer and keys for a sample server, the order of
parts whatever the query's, the echo, silence for what is not answered,
packets for a full 64-player server (each at most 1,400 bytes, numbered from
1, pairs never cut, `\final\` in the last alone), a pair too long for any
packet left out, a part of exactly a packet's room sent whole; the
single-player test, sliced from `sv_main.cpp`, for Quake III and Medal of
Honor; and, from the source, that only the `mohaa` profile opens the port,
where the rate limits stand, that nothing is sent to a master, where the
socket is read and closed, and the menu's `sv_gamespy`.
`tests/server_drop_notice_tests.py`, which compiles the same test, gains the
profile predicate it now calls.

**Live** (`scripts/probe_mohaa_gamespy.py`): every process bound to
127.0.0.1 with IPv4 alone, on a private desktop with its own home, windowed,
silent, with no input; the installation was only read, its inventory equal
before and after. On this machine a limited broadcast from a loopback
socket reaches loopback sockets, so the LAN search is a real broadcast. Two
runtimes, built from one clean export of HEAD (`5a05abe9`) with Windows x86
MSVC 2022, strict warnings and every development gate: `before`, HEAD itself,
and `after`, HEAD with this change's engine source (its hash and files are in
the companion). One run of both took 46 checks, every one as expected. The
`after` runs:

| Case | Checked |
| --- | --- |
| Two dedicated servers, `dm/mohdm1`, free-for-all | The second finds game port 12204 and query port 12301 by itself. A LAN search finds exactly 127.0.0.1:12300 and :12301. Each `\status\` is one packet with the basic, info and rules keys alone, in order, with the expected values: `hostport` 12203 and 12204, `mapname` `mohdm1`, `gametype` `Free-For-All`, `gamever` `1.00`, `dedicated` 1. `\basic\`, `\info\`, `\rules\`, `\players\` (empty: `\final\` alone) and `\info\\basic\` (basic first) answer their keys; `\echo\fnTech3probe` comes back. The CD-key, `\secure\`, `\packets\`, a 308-byte query and plain text go unanswered. Fifteen queries at once from one address get ten answers. `getinfo` on the game port is answered as before. By rcon, `sv_gamespy 0` frees the port and `1` opens it again; `net_gamespy_port 12350` moves it there and back; a single-player game type (`g_gametype 0`, then `map`) leaves it shut; `quit` closes both. |
| A client hosting a team match, `dm/mohdm2`, `g_gametype 2` | `getinfo` on its game port is answered; the LAN search finds it at 12300; its `\status\` reports `dedicated` 0, `Team-Match`, `gametype_i` 2, `hostport` 12223 and `player_0` ProbeHost. A second client connects from another process and enters the game, and the player list names both. The host joins the Allies, takes a rifle and, after the game's five seconds, `kill`s itself: the list then reads `frags_0` -1 and `deaths_0` 1. |

The `before` runs: the dedicated server answers `getinfo` but nothing from
12300, so the LAN search finds nothing; the listen team match answers neither
`getinfo` nor the LAN search, and the second client never enters.

## What stays open

- **Retail acceptance.** No retail client, GameSpy Arcade or third-party
  browser has read these answers; that 1.00's engine reported the same keys,
  `gamever` and `location` is OpenMoHAA's reading of later engines and this
  record's inference.
- **Real networks.** Every packet stayed on loopback; a LAN broadcast
  between machines, a server bound to `0.0.0.0`, and a remote address are
  unexercised.
- **Masters and authorization.** No heartbeat is sent and `\secure\` is not
  answered; GameSpy's masters are gone. A community GameSpy-compatible master
  would need both, and its key; neither is attempted. CD-key checks
  (`gcd_*`) are not served.
- The client's own LAN list stays on `getinfo`, as above; the
  [October 8 record](2026-10-08-mohaa-gamespy-lan-search.md) has since added
  GameSpy's search to it.
