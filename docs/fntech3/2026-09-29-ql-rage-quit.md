# Quake Live's rage quit, announced as retail announces it

Quake Live's `ragequit` command leaves a server with a notice of its own:
`quakelive_steam.exe` tells everyone `print "%s ^1rage^7quits\n"` about a client
its game has marked, where any other drop is `print "%s^7 %s\n"` with the
reason. fnTech3 bound the game import that sets the mark, 192, as
`MarkClientGotCP`: it set `gotCP`, the pure check's record that a client has
sent `cp`, and the drop was announced as `<name>^7 disconnected`, as the
[drop notice record](2026-09-24-rtcw-et-drop-notices.md) left open. Import 192
now sets a rage-quit mark of the client's own, and the `ql` profile's
`SV_DropClient` announces a marked client as retail does. The
[machine-readable record](2026-09-29-ql-rage-quit.json) holds the addresses,
the builds and both runs.

## Observed: the retail engine, game and cgame

`quakelive_steam.exe`, SHA-256
`c926fe9f6c851e00b3b9332e88903ad01f28fdd60454873891c0158f5ded1299`:

- **Game import 192** is entry `0x56d280` of the import table at `0x56cf80`,
  the function at `0x4e26e0`. For a number from 0 to below
  `sv_maxclients->integer` it takes the client's slot,
  `svs.clients + number * 0x25b68`, and calls `0x4df830`, which stores 1 at
  the slot's `+0x25b50` and returns. It hands back the slot's address, or the
  number itself when that is out of range.
- **Nothing else touches `+0x25b50` but `SV_DropClient`.** Every instruction
  of the code section with that displacement was decoded: the store at
  `0x4df836`, and one read, at `0x4df716` in `SV_DropClient` (`0x4df660`).
  When it is set, `SV_DropClient` broadcasts `print "%s ^1rage^7quits\n"`
  (the string at `0x5452ac`, pushed at `0x4df729`) with the client's name
  alone; otherwise `print "%s^7 %s\n"` (`0x545298`, `0x4df73c`) with the name
  and the reason. The client itself is told `disconnect "%s"` with the reason
  either way (`0x4df782`).
- **A new connection clears it.** `SV_DirectConnect` sets the userinfo's `ip`
  to `localhost` for a local address (`0x4e0afc`), zeroes a client-sized
  temporary (`0x25b68` bytes, `0x4e0b15`) and later copies it over the new
  slot (`0x4e0ddd`), Quake III 1.32's `*newcl = temp`. **A bot keeps what its
  slot held**: `SV_BotAllocateClient` (`0x4dcd30`) sets the slot's entity,
  state, time, address type, rate and Steam id and leaves `+0x25b50` alone,
  so a bot that takes a rage quitter's freed slot is announced as a rage quit
  when it leaves.

The retail modules in `baseq3/bin.pk3`
(`0587e55a98516d9645d2dd7ad894822db4bc5190f53e11f82e7486876b753ce0`):

- `qagamex86.dll` (`9bfad1b5df4cbbb3fcfb20781024fc5c0abe73ba8389c2f20be8c0a55552e83d`)
  compares `ragequit` in its client-command ladder between `readyup` and
  `vote` (`0x10045ff1`). It passes the entity's `s.clientNum` to import 192
  through the table's slot `0x300` and does nothing else
  (`0x10046003`..`0x10046017`): it neither drops the client nor prints.
- `cgamex86.dll` (`310542161ae03cc09a2edf7c5933a3cb5e2f13d791f59d5a33a62721bbf39953`)
  has a `ragequit` command of its own (table entry `0x10078f58`, handler
  `0x10007960`). It sends `ragequit` to the server through cgame import 23,
  `CG_QL_IMPORT_SENDCLIENTCOMMAND`, and sets a counter at `0x10ab97c8` to 2;
  the client then quits by itself, which is how a rage quitter leaves. QLSRP's
  notes name that counter `rageQuitTime`, a latch before the queued `quit`
  (reference observation only); the runs below show the client leaving.

## Change

- `code/game/g_public.h`: slot 192 is `G_QL_IMPORT_MARK_CLIENT_RAGE_QUIT`.
- `code/server/sv_game.cpp`: `QL_G_trap_MarkClientRageQuit` sets
  `client_t::rageQuit` for a client below `sv_maxclients`, with retail's
  bound and return value, and no longer touches `gotCP`, which only a `cp`
  command sets again, as in Quake III 1.32.
- `code/server/server.h`: `client_t::rageQuit`, engine-internal. Only
  `SV_ClearClient` resets it, which a new connection runs over the whole slot,
  as retail does; the bot allocator leaves it, as retail's does.
- `code/server/sv_client.cpp`: under `ql`, `SV_DropClient` broadcasts
  `print "%s ^1rage^7quits\n"` for a marked client and every other client as
  before. The addressed `disconnect "%s"` keeps the reason. A drop without a
  reason, fnTech3's silent drop of a connection that never answered, stays
  silent. No other profile has the mark, and none reads it.

## Tests

`tests/server_drop_notice_tests.py` compiles the production import beside
`SV_AnnouncesDrop` and `SV_DropClient`. With one client slot, the import
marks client 0 and nothing else, leaves `gotCP` and `pureAuthentic` alone
and returns the slot's address; for 1, 2, -1 and both ends of `int` it marks
nothing and returns the number. Every profile drops a marked player and a marked bot:
`ql` announces them as a rage quit, with the reason still addressed to the
client, and every other profile announces them as before. An unmarked client
is announced with its reason, a drop without a reason stays silent marked or
not, and a name holding `%s` is printed as text. Seven new mutations each
fail: the import setting `gotCP` again, marking a number past `sv_maxclients`
or returning nothing, `ql` ignoring the mark, every profile reading it, the
notice carrying the reason, and a colour reset after the name. With the
earlier seventeen, all 24 fail. `tests/ui_native_bridge_source_tests.py`
checks slot 192 under its new name.

## Runtime

`scripts/probe_ql_rage_quit.py` runs a dedicated server as `quakelive` on
`campgrounds` `ffa`, pure, with the installation's retail game module, and
two clients that join it over UDP loopback, each on its own unswitched private
desktop with an isolated home. No input is injected. An observer joins first;
the rage quitter joins with `activeAction ragequit`, so the retail cgame's
`ragequit` runs on its first frame of play. Both runs used the same client,
built from `8e345c8`; only the server differed: `8e345c8` alone, and
`8e345c8` with this change alone.

| Server | Rage quitter's commands, as the server logged them | Broadcast about it | Observer |
| --- | --- | --- | --- |
| `8e345c8` | `cp`, `userinfo`, `ragequit`, `disconnect`; exited 0 | `print "fntech3-ragequitter^7 disconnected\n"` | received it, and its retail cgame printed it |
| with this change | `cp`, `userinfo`, `ragequit`, `disconnect`; exited 0 | `print "fntech3-ragequitter ^1rage^7quits\n"` | received it, and its retail cgame printed it |

In both runs the server logged the drop, no process logged an error, the
server and client were unchanged, and the installation's files kept their
sizes and times. The run with this change passes; the one without fails on
the notice alone.

## What this does not show

- No retail client or server took part: `quakelive_steam.exe` runs only under
  Steam. The retail form comes from its format string and the code around it,
  not from a capture of its traffic.
- How the observer drew the notice was not looked at.
- One map, one pure server, loopback only.
- A bot leaving a rage quitter's slot was not run, and no test allocates one
  there. That it inherits the mark rests on reading both allocators:
  retail's (`0x4dcd30`), and fnTech3's `SV_BotAllocateClientSlot`, which
  writes only the fields it names. The unit test drops a bot already marked.

## Identity

- Tested commit `8e345c8f05bf66b748e85cbe3686cde78f4f3a05`, built as a
  `git archive` export with win32-dev's options, strict warnings and every
  development option, on Windows 11 x64, 2026-09-29.
- Server without the change `53e269ac165ecd1415651d33d483d8994e8372bf35b1ebf4b045bf36d476b15e`,
  with it `91f0780d2f2a5d9fb28f67878abc74ebd151d7c2e2b2d73ae8cff5afe5abf540`;
  client `98cae58fa5806186db8dc032d01410cb6a1f901593e61e13f30b478b6f77d04f`.
- Installation `E:\SteamLibrary\steamapps\common\Quake Live`, read only.
