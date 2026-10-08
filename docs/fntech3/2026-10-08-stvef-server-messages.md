# Elite Force drops, kicks, shutdowns and bots across map commands, 2026-10-08

The [drop notice record](2026-09-24-rtcw-et-drop-notices.md) left Elite Force
open: fnTech3 sent both modes Quake III's notice and `disconnect "%s"`. Reading
the rest of each retail server's path for a client that leaves turned up more:
the order in which the client becomes a zombie, Holomatch's kick reason, its
last words when the server quits, which bots its map commands keep, and its
silence towards queries while it runs a single-player game. fnTech3 now does
each as `stvoy.exe` 1.20 and `stvoyHM.exe` 1.20 do, and a live comparison with
the retail Holomatch server matches it step for step. The
[JSON companion](2026-10-08-stvef-server-messages.json) holds identities,
addresses, commands and every observation.

## Retail

Single player, `stvoy.exe`
(`8f7ccadb52b4230689c5b3275177f7ec7060a30bfaa3ade4e7b3fb0cda0dff74`):

| What | Retail | Address |
| --- | --- | --- |
| A client leaving | already a zombie: nothing. Otherwise a zombie at once, its download closed, then the game's `ClientDisconnect`, then `print "%s %s\n"` to everyone, then a bare `disconnect` to the client. No check of the reason | `SV_DropClient` 0x424840; zombie 0x424852, game 0x42488f, notice 0x4248a1, command 0x4248b0 |
| The client hearing `disconnect` | `Server disconnected` | `CL_GetServerCommand` 0x402a15 |
| The server quitting | `print "%s"`, the message as it is, and a bare `disconnect`, to everyone; then two snapshots | `SV_FinalMessage` 0x425dc0; 0x425dc6, 0x425dd5 |

Holomatch, `stvoyHM.exe`
(`5b5b6e82ce7ac0d720a45ab1fa9df32930c846d6c51847fa0c158ebc86487dff`):

| What | Retail | Address |
| --- | --- | --- |
| A client leaving | its download closed, `print "%s^7 %s\n"` to everyone, then a zombie, then the game's `ClientDisconnect`, then `disconnect %s` to the client, the reason unquoted; a bot freed; a heartbeat if no one is left | `SV_DropClient` 0x42faf0; notice 0x42fb56, zombie 0x42fb78, game 0x42fbb9, command 0x42fbbf |
| The client hearing `disconnect` | the command's own text after the word, as the error (`Com_Error( ERR_SERVERDISCONNECT, s + 10 )`) | `CL_GetServerCommand` 0x402b7a |
| `kick <userid or name>` | refuses the host's own client, else drops with `kicked`. The engine has no other kick command | `SV_Kick_f` 0x42e8d0, `kicked` 0x4cb1bc pushed at 0x42e93f; command table 0x42e350 |
| The server quitting | `print "%s"`, the message as it is (`Server quit\n`, 0x4c2138), and `disconnect server shut down`, to everyone; then two snapshots | `SV_Shutdown` 0x434180; 0x4341b7, 0x4341c3, string 0x4cc054 |
| `devmap` | removes the bots | `SV_Map_f` 0x42e440, `killBots` 0x42e5a5..0x42e5c1 |
| `map` from a single-player game | the game type becomes free-for-all, and the bots stay | 0x42e5c2..0x42e5d3 |
| `spmap`, `spdevmap` | single player, eight players, no Expansion Pack mode, the bots removed | 0x42e53e..0x42e59e |
| A level starting | a bot is dropped, with an empty reason, when the map command removes bots or the game started the level as single player (`g_gametype` 2) | `SV_SpawnServer` 0x433b42..0x433b7b |
| `getstatus`, `getinfo`, `getchallenge` | no answer while `g_gametype` is 2, whatever `dedicated` says; no `ui_singlePlayerActive`, which neither the engine nor the Holomatch modules know | `SVC_Status` 0x434620, `SVC_Info` 0x434800, `SV_GetChallenge` 0x42ee80 |

Broadcasts reach clients from `CS_PRIMED` up (0x4343fc); a command addressed to
one client is queued whatever its state (0x434314). So in both engines the
configstrings and prints the game's `ClientDisconnect` sends never reach the
client that left, which is already a zombie, and in single player the notice
does not either.

## Change

- `SV_DropClient` (`code/server/sv_client.cpp`): single player makes the client
  a zombie and calls the game before its notice, which is `print "%s %s\n"`,
  and addresses a bare `disconnect`. Holomatch makes the client a zombie after
  its notice and before the game, and addresses `disconnect %s`. fnTech3's
  command queue takes a command only from a client in the game, so the
  client's state comes back for the addressed command and becomes a zombie
  for good at the end, as before.
- The kick commands (`code/server/sv_ccmds.cpp`) drop with `kicked` under
  Holomatch through `SV_KickedReason`, `kick` as retail does and fnTech3's own
  `kicknum`, `clientkick`, `kickall` and `kickbots` alike. Every other profile
  keeps `was kicked`.
- `SV_FinalMessage` (`code/server/sv_init.cpp`): both Elite Force modes print
  the message with one line end and send Holomatch's `disconnect server shut
  down` or single player's bare `disconnect`. A message that already ends a
  line, as the host's own `disconnect` passes, keeps its one.
- `SV_Map_f`: Holomatch's `devmap` removes the bots and its `map` from a
  single-player game keeps them. `SV_SpawnServer` drops a Holomatch bot with an
  empty reason when the level starts as single player, after the game has
  applied a latched game type.
- `SVC_Status`, `SVC_Info` and `SV_GetChallenge` (`code/server/sv_main.cpp`,
  `sv_client.cpp`) give no answer under Holomatch while `g_gametype` is 2, on a
  dedicated server too.

The Quake III 1.32, Quake Live, RTCW, ET, Jedi, Ritual, Medal of Honor, Q3Test
and IHV paths are unchanged. No retail text or code was incorporated.

## Evidence

- `tests/server_drop_notice_tests.py` compiles the production `SV_DropClient`
  for every profile in the catalog. Its stubs record whether the client was a
  zombie when the game was called and when each command was sent, and a
  command addressed to a zombie counts as lost. Both Elite Force orders and
  forms are expected; 32 mutations each fail, eight of them new for Elite
  Force: single player calling its game after the notice or while the client
  is in the game, either mode losing its command to the zombie, Holomatch
  calling its game while the client is in the game or making it a zombie
  before the notice, single player's notice with Quake III's colour reset and
  its command with the reason, and Holomatch's reason quoted.
- `tests/stvef_server_messages_tests.py` compiles the production
  `SV_KickedReason` and `SV_FinalMessage` for every profile: a remote, a local
  and a connecting client, four messages. Seven mutations each fail. It also
  checks that every kick command drops through `SV_KickedReason`, and the
  order of the map and spawn rules in the production text.
- `tests/rtcw_cast_lifecycle_tests.py` and `tests/server_zombie_snapshot_tests.py`,
  which compile or read the same functions, pass.
- Every edited file compiles with `meson/build/win32-dev`'s exact MSVC
  commands (`/W4 /WX`), client and dedicated objects. Runtimes come from a
  `git archive` export of `84bcf66b`: unchanged as the baseline, and with
  this change.
- `scripts/probe_stvef_bot_rules.py` runs one rcon sequence on retail
  `stvoyHM.exe`'s dedicated server and on fnTech3's, with no client: a bot,
  `devmap`, `spmap` and a bot in its game, `map`, a latched `g_gametype 2` and
  `map`, a free-for-all `map`, then `kick`. After each step it lists the
  server's players and every drop notice the server echoed to its console.

  | Step | Retail | Baseline | This change |
  | --- | --- | --- | --- |
  | `devmap` | bot removed, `Munro^7 ` | bot kept | as retail |
  | `spmap` | bot removed, `Munro^7 `; no `getinfo` or `getstatus` reply | removed twice with `was kicked`; both answered | as retail |
  | `map` from single player | every bot kept | every bot removed, `was kicked` | as retail |
  | latched `g_gametype 2`, `map` | bots kept: the map command makes it free-for-all | the bot removed | as retail |
  | `kick` | `Munro^7 kicked` | `Munro^7 was kicked` | as retail |

  All twelve steps of the change's run equal retail's: map, players and
  notices. The baseline differs at eleven.
- `scripts/probe_legacy_sessions.py`, which now also records the `disconnect`
  and `print` a retail client logs, ran Holomatch's main and kick sessions
  with this change: the retail client on fnTech3's dedicated and listen
  servers, and fnTech3's client on the retail server. Every step of all six
  sessions passed. On fnTech3's dedicated server the kicked retail client
  logged `print "fnprobe^7 kicked\n"` and then `disconnect kicked`, with no
  configstring between them, the commands the retail server sends it; the
  baseline's had been `was kicked`, two configstrings and `disconnect "was
  kicked"`. On the quitting server it logged `print "Server quit\n"` and
  `disconnect server shut down`, as from the retail server, where the
  baseline's had ended with `disconnect "Server quit"`. One run of fnTech3's
  client on the retail server passed every step, but the harness took its
  exit code too early; the run before it passed outright.
- Single player has only the local client, and its server frees a dropped
  client at once: a client sending `cmd disconnect` is dropped and logs no
  further command, with and without this change. The unit test is the
  evidence for single player's order and forms.

## Not changed

- The words a server quits with are fnTech3's own: `Server crashed: %s` and
  `Server fatal crashed: %s`, where Holomatch's fatal error is red
  (`^1Server fatal crashed: %s\n`, 0x4c3f40), and a host leaving its own
  listen server says `Server quit` where Holomatch's engine says `Server
  crashed: Disconnected from server`. Their wording is the engine's to choose;
  their command forms are retail's.
- No profile sends a quitting server's commands to the host's own client, as
  Quake III 1.32 does not; Elite Force's engines do. That client is in the
  quitting process.
- The other multiplayer engines in the drop notice record make a client a
  zombie before their game's `ClientDisconnect` too, Quake III 1.32 among
  them, where fnTech3 keeps Quake3e's order, so its dropped client hears that
  game's last configstrings. Only Elite Force was changed here.
- fnTech3's extra operator commands (`kicknum`, `clientkick`, `kickall`,
  `kickbots`, `say`, `tell`, the ban commands) stay in Holomatch: none is a
  command its game module handles.
- `sv_maxclients` stays within the game module's 64 client slots
  ([server record](2026-09-18-stvef-holomatch-server.md)). Retail's
  dedicated server takes 100 and starts the level with 100 slots, and at 128
  stops with `Hunk_Alloc failed` at its default memory; fnTech3 takes 64 for
  either, with an out-of-range warning. A 65th retail player would be given
  an entity the module keeps for something else, so the limit stays.
