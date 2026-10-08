# Sustained mixed sessions under packet loss with retail peers, 2026-10-08

The [legacy-server record](2026-10-05-legacy-server-hosting.md) passed short
sessions with each retail multiplayer engine. Each put a retail client and an
fnTech3 client on one server for a chat line, a map change and another line.
Sustained sessions and loss were open. These runs keep both clients on the
server for ten minutes while fnTech3's side drops a tenth of the packets it
receives. They exchange chat every 20 seconds, with a map change halfway, in
both directions with retail Quake III, RTCW, ET and Elite Force. Q3Test 1.08,
a profile those sessions had not covered, joins them for five minutes. The
[JSON companion](2026-10-08-legacy-sustained-sessions.json) holds identities,
commands and every round.

## Method

`scripts/probe_legacy_sessions.py` gains a `sustained` session and `--hold`:

- each 20-second round, both clients say a fresh random line, and each must
  hear the other's within 10 seconds;
- a round that fails is said once more;
- the server changes map at the halfway round, and both clients must enter
  the new level.

`--fntech3-set net_dropsim 10` makes each fnTech3 process drop 10% of the
packets it receives. The retail engines have no such setting, so:

- with a retail server, loss falls on what reaches fnTech3's client;
- with fnTech3's server, it falls on what reaches the server from both
  clients.

The runs were on loopback, windowed, with private homes or writable copies
under `.tmp`. Every installation was unchanged afterwards.

For Q3Test the script stages release 1.08 from its laid-down installation in
the Q3Test probe's fixed runtime slot. Its writable base is the run's own
folder, since Q3Test has no home path.

## Results

| Profile | Direction | Rounds heard both ways | Map change | Exits |
| --- | --- | --- | --- | --- |
| Quake III (`quake3.exe` 1.32) | fnTech3 client on the retail server | 30 of 30 | both entered | clean |
| Quake III | retail client on fnTech3's server | 30 of 30 | both entered | clean |
| RTCW MP (`WolfMP.exe` 1.4) | fnTech3 client on the retail server | 30 of 30 | both entered | clean |
| RTCW MP | retail client on fnTech3's server | 30 of 30 | both entered | clean |
| ET (`etded.exe` / `et.exe` 2.60b) | fnTech3 client on the retail server | 30 of 30 | both entered | clean |
| ET | retail client on fnTech3's server | 30 of 30 | both entered | clean |
| Elite Force (`stvoyHM.exe` 1.20) | fnTech3 client on the retail server | 30 of 30 | both entered | clean |
| Elite Force | retail client on fnTech3's server | 30 of 30 | both entered | clean |

Every round was heard at the first attempt, over the 600 seconds each
session was held.

**Q3Test 1.08.** Both clients joined and followed the map change in both
directions. A retail 1.08 server carried fnTech3's client, and fnTech3's
server carried a retail 1.08 client. The retail client heard every one of
the fnTech3 client's 15 lines, through the retail server and through
fnTech3's.

No line went the other way, because the retail client could not be told to
say one. The probe feeds a running client by writing command files, and retail
Q3Test reads its loose files once, at startup. It cannot find a file made
later. A placeholder made before it starts stays as it was first read, even
when rewritten at the same length. So it never sent its line, and it never
received its `quit`: the probe stopped it, with exit code 1.

These runs therefore show a retail Q3Test server and fnTech3's Q3Test server
each holding a retail and an fnTech3 client under loss, through a map change,
with chat delivered from fnTech3's client to the retail one.

`scripts/probe_legacy_discovery.py` lists the 1.08 installation and stages it
in the slot `scripts/probe_q3test.py` keeps (`.tmp/q3-point-releases/r/24`).

## Open

Real networks, combat and objectives, more than two clients and a
retail-originated chat line from Q3Test remain open. No profile gate closes.
