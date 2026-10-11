# Medal of Honor's LAN list asks GameSpy's ports too, 2026-10-08

The [October 6 GameSpy record](2026-10-06-mohaa-gamespy-lan-queries.md) made
fnTech3's `mohaa` server answer GameSpy's LAN queries on the first free port
from 12300, and left fnTech3's own Join LAN Game list asking the game ports
alone. The list now also makes GameSpy's LAN search, so it finds a server
from either side, including one whose game port no `getinfo` scan reaches.
The [JSON companion](2026-10-08-mohaa-gamespy-lan-search.json) records the
source, runtimes, commands and every check. No gate closes: `MOHAA.exe` is
SafeDisc-encrypted and cannot run here, and no GameSpy browser is
installed, so no retail program has sent or answered these queries.

## Evidence

Facts are marked as the [contracts record](2026-09-24-mohaa-contracts.md)
marks them: read from 1.00's data or plain game library, **[O]** read from
[OpenMoHAA](https://github.com/openmoh/openmohaa/tree/ab43a0def6c4feccedae36fdb58c2e007d7e4c35)'s
reconstruction of 2015's engine, and **[I]** inferred. OpenMoHAA's GameSpy
SDK files are GameSpy's own code and were read only to learn the protocol;
no text of them or of OpenMoHAA is used.

**Why the list asks both.**

- 1.00's Join LAN Game list (`Pak0.pk3` `ui/joinlangame.urc`, read) refreshes
  only when its Refresh button runs `localservers`; opening the menu does not
  scan. `localservers` broadcasts `getinfo` to the default game port and the
  three after it, 12203 to 12206, in OpenMoHAA as in fnTech3 **[O]**
  (`PORT_SERVER`, `NUM_SERVER_PORTS` 4). A server on any other game port is
  not found that way, and that the broadcast's form -- the direction byte
  after the marker -- is 1.00's is OpenMoHAA's reading **[O]**.
- 1.00's servers answered GameSpy's browsers: Start New Server has a GameSpy
  box (`ui_gamespy`, archived as 1; read, see the October 6 record), the
  discs carried GameSpy Arcade, and OpenMoHAA's server answers GameSpy's
  queries from 12300 **[O]**. That a 1.00 server did so on 12300 is **[I]**
  from these.
- So GameSpy's search is a second route to a server, by the protocol GameSpy
  itself spoke: a server found by both is listed once, and one the `getinfo`
  scan misses -- another game port, or a server that takes no `getinfo` of
  fnTech3's form -- is listed from its GameSpy answer.

**GameSpy's LAN search [O]** (the SDK's `gserverlist.c`):
`CreateServerListLANSocket` opens a socket of its own and allows it to
broadcast; `SendBroadcastRequest` sends the eight bytes `\status\` once to
each port of the run (OpenMoHAA's list asks 12300 to 12316);
`ServerListLANList` takes every source whose datagram holds `\final\` for a
server, for `LAN_SEARCH_TIME`, three seconds, and then shuts the socket.

**How OpenMoHAA's browser judges an answer [O]** (`cl_uiserverlist.cpp`
`UpdateServerListCallBack`, `cl_instantAction.cpp`): a server is joined at
its answer's address and `hostport`, the game's default 12203 without one;
an answer without `gamever` is taken for 1.00; a version starting with `d`
is a demo's; one more than a tenth from the browser's own is another
version's. Spearhead's servers name themselves `mohaas` and Breakthrough's
`mohaab` (`sv_gamespy.c`).

**Two keys the brief named.**

- `password`: 1.00's game turns away a client whose userinfo `password`
  differs from the server's `password` cvar ("Invalid password",
  `gamex86.dll` 0x310d9b92..0x310d9bd7, read), so a 1.00 server can be
  closed. OpenMoHAA's server reports no `password` key to GameSpy **[O]**;
  only its instant-action client reads one, taking 0 when it is missing. So
  fnTech3's server reports none, as before, and its list reads none.
- `gamemode` `exiting`: OpenMoHAA sets it only as the server stops, for the
  heartbeat that tells the masters, and then shuts the query socket **[O]**,
  so no LAN query can see it. fnTech3 sends no heartbeat, and its query
  socket closes with the server, as the October 6 record has it.

## What changed

- **A search socket** (`code/qcommon/net_ip.c`, `net_query.h`): a third IPv4
  UDP socket, a browser's, on `net_ip`'s interface and a port the system
  picks, allowed to broadcast. It opens for a given time and closes itself at
  the first `NET_Sleep` after it, or with the network (`net_restart`,
  shutdown); it is read with the game's sockets, at most 64 datagrams a wake,
  by the same reader as the query socket, which now serves both. It is never
  opened through a SOCKS relay or without IPv4. Datagrams sent from it go to
  an IPv4 address or as a broadcast.
- **A browser's side of the protocol** (`code/compat/gamespy_query.hpp`):
  a reply datagram's pairs, `\final\` noted and `\queryid\` left out; each
  source's answer gathered over its datagrams until its `\final\`, without a
  player's keys, at most 128 sources and 64 keys each; and the judgement
  above, for a 1.00 browser: the game GameSpy names `mohaa`, a version
  within a tenth of 1.00 or none, not a demo's, at a `hostport` from 1 to
  65535 or the default. The game type is `gametype_i` where it is one of
  1.00's five, else `gametype`'s name as 1.00's game module spells it.
- **The list** (`code/client/cl_mohaa.cpp`, `cl_main.cpp`): for the `mohaa`
  profile alone, `localservers` makes the `getinfo` broadcast as before and
  then GameSpy's search: `\status\` once to each of 12300 to 12316, the
  answers taken for three seconds while the list is the local one. A server
  is listed at the answer's address and `hostport`, with its name, map,
  players, game type and the time since the search began as its ping. A
  server that answers `getinfo` on its game port is listed from that answer,
  whichever comes first, so GameSpy's answers only add. A developer
  (`developer 1`) is told which answer listed each server and why an answer
  was not listed.

Where fnTech3's list departs from GameSpy's browser:

- One round: the search's own answers are listed, timed from the search. The
  SDK's browser asked each server again for its details and ping; fnTech3's
  `getinfo` list already times its broadcast's answers the same way.
- Only servers 1.00 can join are listed. OpenMoHAA's browser lists another
  version too, marked as such; a 1.00 list has no column for it, and every
  other version speaks another protocol.
- A GameSpy answer names a map after its folder (`mohdm1`), as GameSpy's
  protocol does, where a `getinfo` answer keeps it (`dm/mohdm1`); the list
  shows each as it came.

No master is asked anything: GameSpy's are gone, and no list from one is
pretended (AGENTS.md invariant 10). No audio, rendering or input path
changed; the LAN list's presentation is the same columns as before
(invariant 11). Other profiles open no search socket.

## Checks

**Unit** (`tests/mohaa_gamespy_query_tests.py`): reply parsing (pairs,
`\final\`, `\queryid\`, empty values, a key with no value, the first NUL,
text not starting with a backslash, the 1,499-byte limit); gathering
(across packets, a player's keys dropped, sources apart, later values
replacing earlier, 128 sources and 64 keys); the judgement (each other game
name, versions within and beyond a tenth, a demo's, none, `hostport`
missing, malformed and out of range, the game type by number and by name,
missing counts and names); fnTech3's own 66-player answer, in several
packets, listed whole at its game port; and from the source, that only the
`mohaa` profile's `localservers` searches, after its `getinfo` broadcast, that its
handler takes answers only while the local list is being made, that a
`getinfo` answer takes a GameSpy listing over, that nothing goes to a
master, and where the search socket is opened, waited on and closed.

**The sockets themselves**: the same file compiles `net_ip.c` whole with a
small stand-in for the rest of the engine and runs its query and search
sockets over the loopback address: nothing opens without a handler, ports
or a lifetime; a search's query reaches the query socket from the search's
port and the answer comes back from the query port; the game's own socket
sees neither; a hundred datagrams arrive 64 at the first wake and the rest
at the next; the search reads a datagram a moment before its end and
neither reads nor sends once it has ended; without IPv4 no search opens;
the network's end closes both. It passes on Windows x86 (clang, the
Windows SDK's Winsock) and on i686 Linux (Ubuntu 24.04 under WSL2, GCC 13
and Clang 18, `-m32`), where the whole test file also passes under
`unittest`; the protocol fixture builds there with `-Wall -Wextra -Werror`,
and `net_ip.c` (client and dedicated) and `cl_mohaa.cpp` with
`-Wall -Werror`. The edited engine files build with the shared Windows
build's strict MSVC options.

**Suites.** On a clean export of HEAD (`f99e8c31`) with this change, built
with every development gate and strict warnings: Meson's 255 C++ tests, 253
passed and 2 skipped; the whole Python suite, 2,603 passed and 129 skipped
for want of an optional tool, binary or installation, none failed. HEAD then
moved to `ae6dba1d` (the IHV Test's client, which edits `cl_main.cpp`
elsewhere); on it with this change every target builds, the C++ tests pass
again (253 and 2 skipped), and the 133 Python test files that read the
client's main file, the network file, Medal of Honor's or GameSpy's code,
the documentation, or that commit's own tests, pass: 1,022, with 2 skipped.

A limited broadcast (255.255.255.255) is delivered differently by the two
systems, so the socket test reports it rather than checking it: its search,
bound to 127.0.0.1, was answered once on Windows and not on Linux. Plain UDP
sockets on both machines show why: on Windows a broadcast from a socket
bound to 127.0.0.1 reaches one bound to 127.0.0.1 or to every interface; on
Linux a broadcast reaches only a socket bound to every interface, whatever
the sender's binding. Bound to every interface, as `net_ip` is by default,
a search reaches a server on the same host on both.

**Live** (`scripts/probe_mohaa_gamespy.py`, cases `dedicated`, `listen` and
`search`): every process bound to 127.0.0.1 with IPv4 alone, on a private
desktop with its own home, windowed, silent, with no input; the
installation was only read, its inventory equal before and after. Two
runtimes, built from one clean export of HEAD (`f99e8c31`) with Windows x86
MSVC 2022, strict warnings and every development gate: `before`, HEAD
itself, whose server already answers GameSpy (`--before-has answers`), and
`after`, HEAD with this change. The `search` case runs two dedicated
servers -- A on game port 12210, which the `getinfo` scan cannot reach, its
GameSpy port 12300; B on 12203 and 12301 -- and, on 12302 to 12306, stand-ins
the probe answers `\status\` from, as servers fnTech3 cannot run here would:
version 1.11; Spearhead's `mohaas`; a demo's `d1.00`; a 1.00 answer in two
packets without `gametype_i` (C, game port 12217, an objective match with
two of twelve players); and one that never sends `\final\`. Their text is the
probe's own, modelled on OpenMoHAA's keys; no retail server sent it. A client
opens Join LAN Game, runs the command its Refresh button runs
(`localservers`), and takes the engine's own screenshot.

One run of both took 98 checks, every one as expected:

| Case | `before` (HEAD) | `after` |
| --- | --- | --- |
| `search` | The client makes no GameSpy search and no stand-in is asked anything (9 checks); its screenshot shows Join LAN Game listing B alone. | The client searches once, from a port of its own (not its game port 12233), and each stand-in is sent one `\status\` from it. The list holds A, B and C, each once: A from GameSpy's answer from 12300, B from its own `getinfo` answer, C from its two-packet answer at 127.0.0.1:12217 as an objective match. The 1.11, `mohaas` and `d1.00` answers are not listed, each for its reason, and the unfinished one is neither listed nor judged. The engine's screenshot shows Join LAN Game listing B (`dm/mohdm2`, Team-Match), A (`mohdm1`, Free-For-All) and C (`obj_team2`, 02/12, Objective-Match), "Ready" beneath (15 checks). |
| `dedicated` | The October 6 record's 29 checks, every one passed: the server's answers, the queries left unanswered, the rate limit, `sv_gamespy`, `net_gamespy_port`, single player and quit. | The same 29, on the query socket's new shared reader. |
| `listen` | The October 6 record's eight checks: the team match is listed, joined and counted, a death included. | The same eight. |

## Probe manifest follow-up, 2026-10-09

`Session.start` had recorded the temporary rcon password unchanged: its
redaction expression returned the same argument on both branches. It now
redacts the value following `+set rconPassword`, including differently
cased or repeated settings in the command tail. The launched process still
receives the real value. `tests/mohaa_gamespy_probe_tests.py` verifies both
paths with a mocked process, without starting a server. Existing local probe
logs were not rewritten; the published evidence already used redacted values.

## What stays open

- **Retail acceptance.** No retail client, server, GameSpy Arcade or other
  GameSpy browser has taken part; the stand-ins stand in for none of them.
  That a 1.00 server answered on 12300 with `gamename` `mohaa`, a `gamever`
  near 1.00 and its `hostport` is OpenMoHAA's reading of later engines and
  this record's inference.
- **Real networks.** Every packet stayed on one machine: on Windows over
  the loopback address, on Linux in the socket test alone. A LAN of several
  machines is unexercised.
- **Masters and authorization.** As before: no heartbeat, no `\secure\`, no
  CD-key service, no internet list.
- **A hosting client.** A client hosting a listen server reads `getinfo`
  answers through its server, which drops them; GameSpy's answers come to the
  search socket and are listed, but no live check has hosted and searched at
  once.
- **IPv6.** GameSpy's protocol is IPv4; the `getinfo` list's IPv6 multicast
  is unchanged.
