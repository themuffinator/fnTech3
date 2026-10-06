# Hosting and joining legacy servers, 2026-10-05

This round asks, for every profile that has multiplayer, whether fnTech3 can
host the game's legacy clients and join its legacy servers: be found by
their browsers and masters, admit them, carry a session through its ordinary
events and hand them files. Each was checked against the retail executable
wherever one runs here. The [JSON companion](2026-10-05-legacy-server-hosting.json)
records source and binary identities, commands, per-step results and the
retail executables' hashes. No full compatibility gate closes.

## What was wrong

**Discovery.** fnTech3 inherited the Quake Live baseline's empty master list
for every profile, so a public server (`dedicated 2`) told no master about
itself and the in-game Internet browser had nothing to ask. Every profile
also sent Quake III's heartbeat and listened on Quake III's port.

- A retail RTCW client drops every info reply without `gamename\wolfmp`
  (`WolfMP.exe` 1.4 `CL_ServerInfoPacket`, 0x40ee19 to 0x40ee47), and
  fnTech3's RTCW server sent none: no retail RTCW browser could list an
  fnTech3 server. Live, the retail client's `ping` of the HEAD server logged
  `Different game info packet`; of the fixed server, a ping time.
- RTCW's and ET's servers name their own game in a heartbeat
  (`Wolfenstein-1`, `EnemyTerritory-1`) and announce a shutdown once with
  `WolfFlatline-1` and `ETFlatline-1` (`WolfMP.exe` 0x43a2e0, `etded.exe`
  0x42eec0). fnTech3 sent `QuakeArena-1` for all of them.
- Jedi Academy listens on 29070 and Jedi Outcast on 28070 (`jamp.exe` 1.01
  0x4526db, `jk2mp.exe` 1.02a 0x44414a); their masters on 29060 and 28060.
  fnTech3 used 27960 and 27950 for both, so their LAN scans and a `connect`
  without a port missed each other's servers, and heartbeats and the
  browser's queries went to the wrong master port (Live: HEAD's servers
  bound 27960 and heartbeat 27950).
- The fnTech3 Elite Force client dropped every Holomatch info reply as a
  different protocol: Holomatch answers on one line, `infoResponse "<info>"`,
  and `stvoyHM.exe` 1.20 reads `Cmd_Argv(1)` (0x40ef10). Its browser could
  list no Holomatch server, retail or fnTech3, and asked Quake III's master
  port for them. Live, HEAD's client logged `Different protocol info
  packet:` with an empty info string for the retail and the fnTech3 server
  alike, and `Requesting servers from 127.0.0.1 (127.0.0.1:27950)` where
  Raven's masters listen on 27953.
- Retail Holomatch ends a shutdown with `\heartstop\<port>\gamename\STEF1\`
  after a last heartbeat (0x4345f0 to 0x434613); fnTech3 sent two heartbeats.

**Hosting from the client.** fnTech3's `map` set `ui_singlePlayerActive` for
every non-dedicated server, a Quake Live baseline rule for its local match.
While it is set, a server refuses info and challenge queries, so a player
hosting a game from the client turned away every remote client in every
profile. Retail Quake III's `SV_Map_f` sets no such flag.

**Mod servers.** Joining a server that names another game directory runs
`Com_GameRestart`, which closed every file handle -- the console log's
among them, without forgetting it -- and reset `developer` and `logfile`.
The client then wrote its log to a closed handle: nothing more was logged
after joining any modded server. A retail engine only restarts its
filesystem there (`FS_ConditionalRestart`).

**Enemy Territory web downloads.** An ET server can send a client that
advertises `cl_wwwDownload` to a web server for a file (download block -1:
URL, size, flags). fnTech3's client advertised nothing and refused any
redirect, including the fallback page servers send every client.

**Console chat on Raven's servers.** Retail Elite Force, Jedi Academy and
Jedi Outcast servers have no `say`: their console chat is `svsay`, which
their operators' scripts and rcon tools send. `stvoyHM.exe` 1.20 and
`jk2mp.exe` 1.02a send every client `chat "console: <text>\n"` (0x42ecc0,
0x448580) and `jamp.exe` 1.01 `chat "Server: <text>\n"` (0x456c80), each
refusing on a server that is not dedicated or not running. fnTech3 had only
Quake III's `say`, so `svsay` was an unknown command.

**Reconnecting to a pure server.** A client clears its pak references when
it disconnects; whatever its menus then read until the next gamestate is
read with no pure list in force, so a pak the next server lacks can be
referenced. When that gamestate needs no filesystem restart -- the same
server, so the same checksum feed -- the reference survived into the pure
proof, and a retail server drops a client whose proof names a pak it did
not load. Live, retail `stvoyHM.exe` dropped fnTech3's client as unpure
when it connected again in mid-session, on an installation that holds a
non-retail texture pack (`zSTVEFtex.pk3`) the server's layout lacked; the
first connection, a map change and `map_restart` had all passed.

**A server's configured client count.** A server whose own cfg sets
`sv_maxclients` (the usual `+exec server.cfg +map <map>`) latches the value,
the cvar being registered already. Every retail engine's `SV_Startup` takes
the latched value before it sizes its client array (`quake3.exe` 1.32
0x438b5f, `WolfMP.exe` 1.4 0x438e98, `etded.exe` 2.60b 0x42d398). fnTech3,
like Quake3e, sized the array from the old value; the game module's own
registration of the cvar then applied the new one, so the first `map_restart`
reloaded the whole map for it. RTCW's and ET's games restart at the end of
every warm-up, so on fnTech3's servers the first warm-up ended with every
client loading the map again. Live, every fnTech3 RTCW and ET server printed
`sv_maxclients variable change -- restarting.` there, and retail `WolfMP.exe`
and `etded.exe` never did.

**Ping limits.** Every retail engine of Quake III, RTCW, ET, Elite Force,
Jedi Academy and Jedi Outcast registers `sv_minPing` and `sv_maxPing`,
publishes each once set and refuses an internet client whose challenge
round trip falls outside them (`quake3.exe` 1.32 0x4391d8). fnTech3 had
neither. Quake Live's retail engine has neither either.

## What changed

- **Per-profile master contract** (`game_profile.cpp`): each profile's
  default server port, `sv_master1` default, master port, heartbeat and
  shutdown game names, read from the retail executables:

  | Profile | Server port | `sv_master1` | Master port | Heartbeat / shutdown |
  | --- | --- | --- | --- | --- |
  | `q3` | 27960 | `master.quake3arena.com` (0x439345) | 27950 | `QuakeArena-1` twice |
  | `rtcw-mp` | 27960 | `wolfmaster.idsoftware.com` (0x439a53) | 27950 | `Wolfenstein-1` / `WolfFlatline-1` once |
  | `wolfet` | 27960 | `etmaster.idsoftware.com` (0x42c856) | 27950 | `EnemyTerritory-1` / `ETFlatline-1` once |
  | `stvef-mp` | 27960 | `master.stef1.ravensoft.com:27953` (0x434091) | 27953 | `\heartbeat\` / `\heartstop\`, `STEF1` |
  | `jka-mp` | 29070 | `masterjk3.ravensoft.com` (0x45f1b0) | 29060 | `QuakeArena-1` twice |
  | `jk2-mp` | 28070 | `masterjk2.ravensoft.com` (0x44dff3) | 28060 | `QuakeArena-1` twice |

  Quake Live (listed through Steam), Medal of Honor (GameSpy), Q3Test (whose
  1999 masters are gone), the IHV test and the single-player profiles keep
  no default master. The masters in the table are the engines' own
  defaults; many no longer answer, and an operator adds a community master
  as `sv_master2` to `sv_master5` (the maintained ports use
  `master.ioquake3.org`, `master.iortcw.org`, `efmaster.tjps.eu` and
  `master.jkhub.org`, among others).
- **Info replies** carry each retail engine's extra keys: RTCW's
  `sv_allowAnonymous`, `gameskill`, `friendlyFire`, `maxlives`, `tourney`,
  `punkbuster 0`, `gamename wolfmp` and `g_antilag` (0x43a878 to 0x43a994);
  Jedi Academy's `needpass`, `truejedi`, `wdisable` (its duel set in duel and
  power duel) and `fdisable`; Jedi Outcast's `sv_allowAnonymous`; Quake III's
  `punkbuster 0`; and `minPing`/`maxPing` once set. `sv_allowAnonymous` is
  registered as serverinfo where retail registers it.
- **Match completion**: RTCW's and ET's game modules end a match with
  `gameCompleteStatus`; a public server now sends every master that status
  report (`WolfMP.exe` 0x43a2a2, `etded.exe` 0x42ed17) where RTCW's was a
  stub and ET's an unknown command. Both retail engines print that they
  send it, but none arrived from them (Live): the report keeps its own copy
  of the master addresses and resolves it only when `sv_master<n>` has
  changed, which the server's first heartbeat has already noted, so it goes
  to an address never resolved (as the GPL RTCW and ET sources also read).
  fnTech3 sends it to the masters its heartbeats reach, in retail's format.
- **Client**: `globalservers` sends to the profile's master port; Holomatch
  info replies are read from the command line.
- **Listen servers**: only Quake Live's local match sets the single-player
  flag. (Corrected by the
  [Quake Live server-type record](2026-10-06-ql-listen-server-type.md):
  retail Quake Live's server never reads that flag; its `sv_serverType`
  decides who may join, and fnTech3's Quake Live challenge now follows it.)
- **Mod switch**: `Com_GameRestart` closes the log first, so it reopens in
  the new game directory; `developer` and `logfile` survive the switch.
- **ET web downloads**: the client registers `cl_wwwDownload 1` (archived,
  userinfo, as `et.exe` 2.60b 0x40d652), answers a redirect with `wwwdl
  ack`, fetches the file with the engine's cURL transfer while it stays
  connected, and reports `wwwdl done`; a failed or impossible fetch reports
  `wwwdl fail`, after which the server sends the gamestate again and the file
  in blocks. A fallback page (flag bit one) ends the connection with an
  error naming the URL, where retail opens a browser and quits.
- **`svsay`** for the Elite Force, Jedi Academy and Jedi Outcast
  multiplayer profiles, with each retail engine's prefix, newline and
  refusals. Quake III's `say` stays beside it.
- **Pure proof**: when a server's pak list arrives, references held by paks
  it does not list are cleared (`FS_PureServerSetLoadedPaks`). No file is
  read from such a pak while the list is in force, so its references can
  only predate the list; with them gone the proof names only paks the
  server loaded. This applies to every profile, and changes nothing when the
  client has no pak the server lacks.
- **Client count at start-up**: `SV_Startup` takes a latched
  `sv_maxclients` before it allocates clients (`SV_BoundMaxClients`), as
  retail does, in every profile.
- **Ping limits**: `sv_minPing`/`sv_maxPing` for those six profiles,
  measured from the challenge reply to the connect (the challenge itself
  keeps no state, so the time of the latest reply to each of the 64 most
  recent addresses is kept), never applied to LAN addresses, with retail's
  refusal texts.

The constants above are read from the retail executables. The original
Quake III, RTCW, ET and OpenJK sources were used as reference observations
for where to look (Quake III and OpenJK are GPLv2; RTCW and ET are GPLv3);
no text from them is incorporated.

## Evidence

All runs were on this Windows 11 machine over UDP on 127.0.0.1, every
process windowed on a private desktop with its own home and no sound, and
no input injected. Four runtimes took part, each built in a clean export
(Windows x86, MSVC 2022, strict warnings, every development gate on):
`before`, from HEAD (`8cf605c7`); `final`, from HEAD plus this round's patch;
`final2`, which adds `svsay` and the pure-proof change, both found by the
`final` runs; and `final3`, which adds the start-up client count, found by
the `final2` runs, and whose engine source is the commit's. Discovery, the
Quake III sessions, the RTCW `join` and `control` sessions and the first ET
downloads ran on `final`; the Elite Force and Jedi Academy sessions, ET's
`join` and `control`, a Quake III and RTCW `join` check of the pure-proof
change and the leave-while-fetching downloads on `final2`; the RTCW and ET
`host` and `listen` sessions and a Quake III `host` check on `final3`. The
JSON companion names every binary's SHA-256 and every command line, and
labels each session with its runtime. Retail installations stayed
unchanged; each run compares their inventories before and after.

**Discovery** (`scripts/probe_legacy_discovery.py`): a fake master on each
profile's own master port, and a second on Quake III's 27950 to catch
heartbeats sent there; a public server (`dedicated 2`) naming the master
without a port and binding its own default port; a retail client and an
fnTech3 client `ping` it (each prints a ping time only once its
`CL_ServerInfoPacket` accepts the reply); the fnTech3 client asks the
master with `globalservers`; for RTCW and ET an rcon `gameCompleteStatus`;
then an rcon `quit`.

| Server | Bound | Sent to the profile's master port | Retail client | fnTech3 client (`final`) | Its master list | Match report |
| --- | --- | --- | --- | --- | --- | --- |
| `jk2-mp-before` | 27960 | nothing (and 3 heartbeats to 27950) | lists it | no answer | none | -- |
| `jk2-mp-final` | 28070 | `heartbeat QuakeArena-1` x3 | lists it | no answer | none | -- |
| `jka-mp-before` | 27960 | nothing (and 3 heartbeats to 27950) | lists it | lists it | 1 | -- |
| `jka-mp-final` | 29070 | `heartbeat QuakeArena-1` x3 | lists it | lists it | 1 | -- |
| `jka-mp-retail` | 29070 | `heartbeat QuakeArena-1` x3 | lists it | lists it | 1 | -- |
| `q3-before` | 27960 | `heartbeat QuakeArena-1` x3 | lists it | lists it | 1 | -- |
| `q3-final` | 27960 | `heartbeat QuakeArena-1` x3 | lists it | lists it | 1 | -- |
| `q3-retail` | 27960 | `heartbeat QuakeArena-1` x3 | lists it | lists it | 1 | -- |
| `rtcw-mp-before` | 27960 | `heartbeat QuakeArena-1` x3 | drops it: different game | lists it | 1 | none arrived |
| `rtcw-mp-final` | 27960 | `heartbeat Wolfenstein-1` x1, `heartbeat WolfFlatline-1` x1 | lists it | lists it | 1 | arrived, 35 keys |
| `rtcw-mp-retail` | 27960 | `heartbeat Wolfenstein-1` x1, `heartbeat WolfFlatline-1` x1 | lists it | lists it | 1 | none arrived |
| `stvef-mp-before` | 27960 | `\heartbeat\27960\gamename\STEF1\` x3 | lists it | lists it | 1 | -- |
| `stvef-mp-final` | 27960 | `\heartbeat\27960\gamename\STEF1\` x2, `\heartstop\27960\gamename\STEF1\` x1 | lists it | lists it | 1 | -- |
| `stvef-mp-retail` | 27960 | `\heartbeat\27960\gamename\STEF1\` x2, `\heartstop\27960\gamename\STEF1\` x1 | lists it | lists it | 1 | -- |
| `wolfet-before` | 27960 | `heartbeat QuakeArena-1` x3 | lists it | lists it | 1 | none arrived |
| `wolfet-final` | 27960 | `heartbeat EnemyTerritory-1` x1, `heartbeat ETFlatline-1` x1 | lists it | lists it | 1 | arrived, 38 keys |
| `wolfet-retail` | 27960 | `heartbeat EnemyTerritory-1` x1, `heartbeat ETFlatline-1` x1 | lists it | lists it | 1 | none arrived |

The client column is the `final` client throughout, so the `before` rows
show HEAD's servers. A second run put HEAD's own client against the retail
and the `final` Holomatch servers (the companion's `headClientDiscovery`):
it dropped both replies and found no server through the master.

Every key a retail info reply carries is now in fnTech3's, in every profile.
fnTech3 also sends ioquake3's `g_humanplayers` and `g_needpass`, and for Jedi
Academy `pure`. The status replies' serverinfo still differs by a few cvars
each engine registers itself: retail has `sv_punkbuster` (Quake III, RTCW),
`sv_pure` (Elite Force), `g_voteFlags` (ET, an engine-registered `0` beside
the game's live `voteFlags`, which both send) and Jedi Academy's
`g_maxHolocronCarry` spelled so (fnTech3 has the game's `g_MaxHolocronCarry`;
info keys match without regard to case); fnTech3 has `sv_dlRate`,
`sv_minRate` and `sv_playingDemo`, and for RTCW and ET `dmflags` and
`sv_allowDownload`. Every retail client that runs here listed fnTech3's
servers regardless.

**Sessions** (`scripts/probe_legacy_sessions.py`): `join` is fnTech3's
client on the retail dedicated server, `host` the retail client on fnTech3's
dedicated server, `listen` the retail client on an fnTech3 client that
started the game with `map`, `control` retail on retail. `main` joins, sends
a chat line each way, changes map, runs `map_restart`, reconnects and quits
the server under the client; `kick` joins and is kicked; `download` puts a
generated pak in the server's mod folder, so the client must fetch it over
UDP before it may enter, then plays pure inside the mod; `mixed` puts a
retail and an fnTech3 client on one server, each sees the other's chat,
through a map change. Every step waits for the server's game log or a
client's console log.

| Profile | join | host | listen | control |
| --- | --- | --- | --- | --- |
| `q3` | pass (`final`): main, kick, download, mixed | pass (`final`): main, kick, download, mixed | pass (`final`): main, kick, download | pass (`final`): main, kick, download |
| `rtcw-mp` | pass (`final`): main, kick, download, mixed | pass (`final3`): main, kick, download, mixed | pass (`final3`): main, kick, download | pass (`final`): main, kick, download |
| `wolfet` | pass (`final2`): main, kick, download, mixed | pass (`final3`): main, kick, download, mixed | pass (`final3`): main, kick, download | pass (`final2`): main, kick, download |
| `stvef-mp` | pass (`final2`): main, kick, download, mixed | pass (`final2`): main, kick, download, mixed | pass (`final2`): main, kick, download | pass (`final2`): main, kick, download |
| `jka-mp` | FAIL (`final2`): main (failed: server-chat, map-change), kick, download (failed: server-ready) | -- | -- | -- |

- **Jedi Academy `join`**: fnTech3's client joined retail `jamp.exe` with
  pure admission and its chat reached the server; the server's chat line
  then made its client game call import 38 (`CG_S_STARTLOCALSOUND`), a
  named refusal of the unfinished client, which ended the main session. The
  kick passed, the client reading retail's `@@@WAS_KICKED`. The download
  session's server never started: retail `jamp.exe` loads its game library
  from the mod's own folder, which the generated mod lacks. The retail
  client needs disc 1, so no other direction ran, and Jedi Outcast ran none.
- In every `join` session but `mixed` that started a client, the probe
  stopped fnTech3's client itself, its script having ended with the
  server's disconnect or the mod switch (see below).
- The pure-proof check, `join` `main` on `final2`: every step passed for
  Quake III and RTCW alike. RTCW's run is marked failed only because the
  client the probe stopped had not reported an exit within the probe's
  5-second wait, since made 15:

| Profile | join | host | listen | control |
| --- | --- | --- | --- | --- |
| `q3` | pass (`final2`): main | -- | -- | -- |
| `rtcw-mp` | FAIL (`final2`): main | -- | -- | -- |

- The start-up check, Quake III `host` `main` on `final3`:

| Profile | join | host | listen | control |
| --- | --- | --- | --- | --- |
| `q3` | -- | pass (`final3`): main | -- | -- |

The `final` run's Elite Force and Jedi Academy sessions are superseded by
`final2`'s. They sent Quake III's `say`, which Raven's servers lack (their
chat is `svsay`), and `clientkick`, which Holomatch lacks (it kicks by name),
so those steps failed on the retail servers; the Jedi Academy run also never
found its client's slot, that game logging no userinfo; and the Elite Force
reconnect was dropped as unpure, which `final2` fixes. On `final` and `final2`, every
fnTech3 RTCW and ET server reloaded its map at the end of its first warm-up;
on `final3` none did. One ET `listen` kick on `final2` missed the probe's
20-second window for a client to enter (it took 22 seconds after such a
reload), since made 40; on `final3` all passed.

**Enemy Territory downloads** (`scripts/probe_wolfet_download.py`): a
generated pak behind a local web server (`sv_wwwDownload 1`,
`sv_wwwBaseURL`), fetched over HTTP, then the client joins pure; `missing`
cases serve 404, so the client must decline and take the pak in blocks; `udp`
cases run without web downloads; `fallback-url` checks that a fallback page
is refused with its URL.

| Case | Client / server | Passed | Redirected | HTTP requests | Blocks over UDP | File matches | Left to fetch | Refused |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `fn-fn-fallback-url` | fnTech3 / fnTech3 | yes | no | 0 | no | no | -- | yes |
| `fn-fn-www` | fnTech3 / fnTech3 | yes | yes | 1 | no | yes | -- | no |
| `fn-retail-missing` | fnTech3 / retail | yes | yes | 1 | yes | yes | -- | no |
| `fn-retail-udp` | fnTech3 / retail | yes | no | 0 | yes | yes | -- | no |
| `fn-retail-www` | fnTech3 / retail | yes | yes | 1 | no | yes | -- | no |
| `fn-retail-www-disconnected` | fnTech3 / retail | yes | yes | 1 | no | yes | no | no |
| `retail-fn-missing` | retail / fnTech3 | yes | yes | 1 | yes | yes | -- | no |
| `retail-fn-udp` | retail / fnTech3 | yes | no | 0 | yes | yes | -- | no |
| `retail-fn-www` | retail / fnTech3 | yes | yes | 1 | no | yes | -- | no |
| `retail-fn-www-disconnected` | retail / fnTech3 | yes | yes | 1 | no | yes | yes | no |

The `*-disconnected` cases set `sv_wwwDlDisconnected 1`, letting a client
leave while it fetches. fnTech3's client stays connected, which the server
allows; retail `et.exe` answers `wwwdl bbl8r`, leaves, fetches and connects
again. The companion recomputes `clientLeftToFetch` from each server's log:
the run's own marker looked for a drop reason the server does not print.

**Automated suite**: a full build of every target with strict warnings in
the export of `final3`'s source; `meson test` there, all 248 project tests:
246 passed and 2 skipped (no audio loopback device or retail QL factory
here), with timeouts tripled for a loaded machine, one having timed out at
its 30 seconds under load earlier and passed alone; and the Python suite on
the exact commit tree: 2,527 passed and 130 skipped, and one failed only as
its temporary folder was removed, Windows still holding an executable it had
just built; it passed when run again. New tests:
`tests/legacy_server_discovery_tests.py` (ports, masters, heartbeat names,
info keys and shutdown for every profile; retail gate keys; ping limits
compiled from source and driven through both refusals and the LAN
exemption; the listen-server flag; match completion; `svsay` per profile,
compiled; the pure list forgetting references to paks it leaves out,
compiled; the start-up client count) and
`tests/wolfet_web_download_client_tests.py` (the redirect parser and both
outcomes, compiled from source). Seven existing tests follow the changed
functions and probes. `make` built the Linux i686 dedicated server from an
earlier state of the patch, and GCC i686 then checked every file changed
since, client and dedicated builds alike.

The ping limits have Unit evidence only: like retail Quake III's, fnTech3's
server never applies them to a LAN address, loopback among them, and every
peer here is local.

## Retail behaviour met on the way

- `stvoyHM.exe` 1.20 overflows its stack (exit `0xC00000FD`) before writing
  any log when started with both `developer 1` and `logfile 2`; either alone
  starts. The probes open the log first and set `developer` afterwards.
- Retail RTCW MP cannot load a pak whose name begins `zz_`: its
  `FS_AddGameDirectory` renames `mp_` paks to `zz_` to sort them, then
  renames every `zz_` pak back to `mp_` and opens that name.
- A retail server's flood protection (`sv_floodProtect 1`) silently drops a
  client's second reliable command within a second, such as a chat line just
  after entering.
- A retail Quake III server took over 20 seconds to exit after one `quit`.
- Retail Elite Force 1.20 has `kick <name>` but no `clientkick`, and ET
  2.60b's engine has neither (its kick is the game module's); fnTech3 keeps
  Quake III's set in every profile.
- ET's game counts a warm-up down after every map load and then restarts
  the map into play. Retail `etded.exe` ignores every message a client sends
  with the server id from before a `map_restart` unless that client is
  already downloading, so a download request sent just before the restart
  is never answered and the client waits for ever. The probes start ET
  clients once the server reports `gamestate 0`. The server decides by
  server id alone, so a retail client would meet the same race
  **[inferred]**.

## Differences recorded, not changed

- fnTech3's `Com_Error` clears the command buffer for every error, a
  server's disconnect included (Quake3e), so a client script stops when a
  server drops the client; a retail client keeps running it.
- Joining a server that runs another game directory restarts the game
  (ioquake3's `Com_GameRestart`): cvars made with `set` during the session
  are removed and the configs run again, so archived settings return and
  others do not. A retail engine only restarts its filesystem there and
  keeps every cvar. Each fnTech3 client's scripted loop ended this way on
  the download sessions' mod servers, and was stopped by the probe. Its log
  then continues in the mod's directory, so the copy the probe keeps of
  those sessions starts at the restart; the first part stays in the run's
  client home.
- PunkBuster is unavailable in every profile. CD-key authorization was not
  exercised: the sessions ran on loopback, which Quake III's engines treat
  as LAN and do not authorize. Retail Elite Force does not exempt it: its
  server waits about 20 seconds for its authorize server, long gone,
  before admitting a client, and the sessions allow for that.

## What stays open

- **Real networks.** Every session ran on loopback: latency, loss, NAT, a
  remote peer and a live master listing are unexercised, and no real master
  was contacted. Several of the retail default masters no longer answer.
- **Sustained play.** Sessions last minutes, with at most two clients.
- **Quake Live** was not run against retail this round (it needs Steam);
  of this round's changes only the mod-switch log fix and the pure-proof
  change reach it.
- **Jedi Academy**: the retail client needs disc 1 to connect, so only
  `join` ran, and there fnTech3's client stops at client-game import 38
  (`CG_S_STARTLOCALSOUND`) when the first chat line plays its sound.
  **Jedi Outcast**: fnTech3's client stops at UI import 200 and the retail
  server checks for the disc, so no session ran in either direction; only
  discovery did.
- **Medal of Honor**: no retail peer runs here. Its LAN browser asks GameSpy
  query ports 12300 to 12316 (OpenMoHAA **[O]**), which fnTech3's server does
  not answer, so a retail client could reach an fnTech3 server only by
  address; GameSpy's internet list is gone. (Corrected by the
  [GameSpy record](2026-10-06-mohaa-gamespy-lan-queries.md): 1.00's own LAN
  list asks the game port with `getinfo`, which was answered; the GameSpy
  ports serve GameSpy's browsers, and fnTech3's server now answers them.)
- Q3Test and the IHV test were not part of this round: Q3Test still
  publishes to no master by default (its releases' own masters were not
  examined), and the IHV test's network path stays blocked.
- ET web downloads of files larger than the generated pak, a redirect to a
  web server on another host, and a transfer cut off part way.
- A retail-exact listing of the serverinfo keys each engine registers
  itself (`sv_punkbuster`, Elite Force's `sv_pure`, ET's `g_voteFlags`),
  which no retail client here needed.
- PunkBuster, CD-key authorization and the ping limits on a real network.
