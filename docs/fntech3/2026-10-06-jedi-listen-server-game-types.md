# Jedi listen servers and single-player game types, 2026-10-06

A server started from fnTech3's client keeps quiet while it runs a single-player
game: it answers no remote `getinfo`, `getstatus` or `getchallenge`, so LAN
browsers do not list it and remote players cannot join. Which game type counts
as single player came from Quake III, 2, for every profile but Enemy Territory
and Medal of Honor. In both Jedi Knight games 2 is Jedi Master. And the map
commands used the same number: `spmap` started Jedi Master, and `map` turned
Jedi Master into a free-for-all. This record reads each retail Jedi engine's
own rules, which differ from each other, and the change that follows them. The
[JSON companion](2026-10-06-jedi-listen-server-game-types.json) holds the
identities, addresses, command lines and every check. No gate closes: no
retail program ran.

## Identities

| File | SHA-256 |
| --- | --- |
| `jamp.exe` 1.01 | `e8062d3948ff05917b48e47d8eec0ee43bcc6fb03924410712412404ddfee28b` |
| `base/assets3.pk3:jampgamex86.dll` (1.01) | `b9e5ad492d35a3ec2fc7f6933df42ce9d3b05d3ec437e3bf12243a5d0d690850` |
| `jk2mp.exe` 1.02a | `2f08dcf9a089cb66062766f4922fda63a9e553de893c174b497dfefeb7a1577a` |
| `fntech3.exe`, runtime `jedi-listen-before` | `c0b75c458950a842684fbdbeb4587ab26a57365b0d3baed08c098d4086ab5b93` |
| `fntech3.exe`, runtime `jedi-listen-after` | `b6ab7e5cd6ae704bb728ec7246520902e47f6d63238936fd12b8a8d42c0ab4b9` |

Both runtimes were built from a clean export of HEAD `192c321f` (MSVC 2022 x86,
Meson `debugoptimized`, strict warnings, every development gate on); `after`
adds this change's four engine files. The installations were read only and
their inventories were unchanged after every run.

## Retail evidence

Read with pefile and Capstone; addresses are virtual addresses. No retail code
is incorporated.

**Jedi Academy, `jamp.exe` 1.01.**

- `SV_Map_f` (0x4560b0) is registered for `map`, `devmap`, `spmap`,
  `spdevmap`, `devmapmdl` and `devmapall`, unconditionally (0x456fef to
  0x457044). A command starting `sp` sets `g_gametype` to 5.0 (0x45617f),
  `g_doWarmup` to 0 and a latched `sv_maxclients` of 8, with cheats off. Any
  other command changes no game type. The
  [foundation record](2026-09-21-jka-contracts.md) said retail multiplayer has
  no `spmap`; it has.
- `SVC_Info` (0x45fa10) and `SV_GetChallenge` (0x457080) return while
  `ui_singlePlayerActive` is not 0 (0x45fa18 to 0x45fa3c, 0x457087 to
  0x4570ad), and **neither reads `g_gametype`** for it. `SVC_Status`
  (0x45f830) tests nothing before building its reply. The engine reads
  `g_gametype` only in `SV_Map_f`, its registration (pointer kept at
  0xb93474) and the info reply's `gametype` key and duel weapon choice
  (0x45fb35, 0x45fbaa). Every engine write of `ui_singlePlayerActive` sets 0
  (0x41d8d1, 0x41d947, 0x41d9f2, 0x438983, 0x45f41c).
- **The retail game plays no Jedi Master.** `jampgamex86.dll`'s cvar
  registration (0x2008ea00) prints `This gametype is not supported.` for
  game types 1, 2 and 9 -- Holocron, Jedi Master and Capture the Ysalamiri --
  and sets `g_gametype` back to 0. None of the installation's 23 arena
  entries lists either (`assets0.pk3`, `scripts/*.arena`: ffa team, duel
  powerduel, ctf team, siege). A mod's game can play them: OpenJK's accepts
  any game type in range (`codemp/game/g_main.c:149`) **[O]**.

**Jedi Outcast, `jk2mp.exe` 1.02a.**

- `SV_Map_f` (0x447910), registered for `spmap` and `spdevmap` too (0x4478b4,
  0x4478c3), sets `g_gametype` to 4.0 for a command starting `sp` (0x4479c0),
  with `g_doWarmup` 0, a latched `sv_maxclients` of 8 and cheats off. Any
  other command turns game type 4 into 0 (0x447a2c to 0x447a43), as Quake III
  turns its 2.
- `SVC_Info` (0x44e7b0) and `SV_GetChallenge` (0x448750) return at
  `g_gametype` 4.0 (constant at 0x4e43fc) or a set `ui_singlePlayerActive`;
  `SVC_Status` (0x44e5f0) at `g_gametype` 4.0 alone.
- Its arenas (`assets0.pk3`, `scripts/arenas.txt`) list Jedi Master on eleven
  maps, `ffa_bespin` among them, and its game plays it (below).

**Reference** (OpenJK `1a6a643`, a maintained port of Academy): its
`codemp/game/bg_public.h:235` numbers free-for-all 0, Holocron 1, Jedi Master
2, duel 3, power duel 4, single player 5, team 6, siege 7, capture the flag 8
and Capture the Ysalamiri 9, agreeing with the 5 above and the game's
refusals. Its `getinfo`, `getstatus` and `getchallenge` handlers keep their
game-type tests inside comments (`codemp/server/sv_main.cpp:432`, `:483`,
`sv_client.cpp:68`), leaving `ui_singlePlayerActive` alone, as `jamp.exe`
does. No Jedi Outcast multiplayer source is in the reference tree: its 4 is
the executable's own, and the name of its 3 (one-on-one duel) is not
established **[I]**.

## What fnTech3 did, and does now

| | HEAD `192c321f` | Now | Retail |
| --- | --- | --- | --- |
| `jka-mp` single player | 2 | 5 | 5 |
| `jka-mp` listen server hides | game type 2 | never, by game type | never, by game type |
| `jka-mp` `spmap` | 2 (the game refuses it: free-for-all) | 5 | 5 |
| `jka-mp` `map` | turns 2 into 0 | keeps every game type | keeps every game type |
| `jk2-mp` single player | 2 | 4 | 4 |
| `jk2-mp` listen server hides | game type 2 (Jedi Master) | game type 4 | game type 4 |
| `jk2-mp` `spmap` | 2, Jedi Master | 4 | 4 |
| `jk2-mp` `map` | turns 2 into 0 | turns 4 into 0 | turns 4 into 0 |

- `SV_GameTypeIsSinglePlayer` (`code/server/sv_main.cpp`) answers 5 for
  `jka-mp` and 4 for `jk2-mp` (`SV_JKA_GT_SINGLE_PLAYER`,
  `SV_JK2_GT_SINGLE_PLAYER` in `server.h`). Both single-player profiles keep
  Quake III's rule, which their `g_gametype` 0 never meets.
- The new `SV_GameTypeHidesListenServer` is what the three listen-server tests
  ask (`SVC_Status`, `SVC_Info`, `SV_GetChallenge`, all in client builds
  only): never for `jka-mp`, `SV_GameTypeIsSinglePlayer` for every other
  profile. A Jedi Academy game started with `spmap` answers remote players,
  as retail's does.
- `SV_Map_f` (`code/server/sv_ccmds.cpp`) gives `spmap` and `spdevmap` each
  Jedi game's own number, keeps every Academy game type on `map`, and turns
  Outcast's 4, not 2, into a free-for-all.
- `ui_singlePlayerActive` still hides a listen server in every profile, as
  before; `jamp.exe`'s `getstatus` ignores it and `jk2mp.exe`'s reads only the
  game type. fnTech3's engine sets it only for Quake Live's local match
  ([legacy-server record](2026-10-05-legacy-server-hosting.md)); what the
  Jedi games' own modules do with it, all of which name it, was not examined.
  (Since the [Quake Live server-type record](2026-10-06-ql-listen-server-type.md),
  neither the flag nor Race hides a Quake Live server's challenge: its
  `sv_serverType` does, as retail's.)
- Unchanged: fnTech3's `spdevmap` turns cheats on in every profile
  (ioquake3's rule), where both Jedi engines leave them off, and Academy's
  `devmapmdl` and `devmapall` are not registered.

With the retail game modules, HEAD's defect showed differently in each game.
In Academy a Jedi Master game never runs -- `map` turned 2 into a
free-for-all and the game refuses 2 -- so `spmap` gave a free-for-all instead
of single player, and the listen-server test could hide a game only for a mod
that plays Jedi Master. In Outcast `map` at 2 ran a free-for-all, while Jedi
Master reached by `map_restart` or `spmap` hid the server.

### Every caller of `SV_GameTypeIsSinglePlayer`

| Caller | Profiles that reach it | Effect of this change |
| --- | --- | --- |
| `SVC_Info`, `SVC_Status` (`sv_main.cpp`), `SV_GetChallenge` (`sv_client.cpp`) | all, client builds | now through `SV_GameTypeHidesListenServer`; `jka-mp` and `jk2-mp` change |
| `SV_Map_f`'s `map` rule (`sv_ccmds.cpp`) | all but ET, Medal of Honor and `jka-mp` | `jk2-mp` compares 4; every other profile still 2 |
| `SV_MasterGameCompleteStatus` (`sv_main.cpp`) | RTCW MP and ET (`gameCompleteStatus`) | none |
| `SV_AnnouncesDrop` (`sv_client.cpp`), `SV_MapRestart_f` twice (`sv_ccmds.cpp`), `SV_BotLibSetup` (`sv_bot.cpp`), `SV_SpawnServer`'s bot drops (`sv_init.cpp`) | ET | none |
| `SV_MOHAAGameSpyFrame` (`server_mohaa_gamespy.hpp`) | Medal of Honor | none |

## Tests

- `tests/jedi_listen_server_tests.py` compiles production
  `SV_GameTypeIsSinglePlayer` and `SV_GameTypeHidesListenServer` and runs
  all sixteen profiles over game types -1 to 10: only `jka-mp` and `jk2-mp`
  differ from Quake III's rule (ET's from its gameinfo, Medal of Honor's 0).
  Five mutations each change the table: Academy hiding its single player,
  Outcast never hiding, either game taking Quake III's 2, and the rules
  reaching `jka-sp`. It also checks that the three listen tests ask the new
  function and that none asks the old one.
- `tests/rtcw_server_policy_tests.py` runs production `SV_Map_f` for
  `jka-mp` and `jk2-mp`: `spmap` and `spdevmap` set 5 and 4 with eight
  latched slots, and `map` and `devmap` keep game types 0 to 6 but Outcast's 4.
- `tests/mohaa_gamespy_query_tests.py` adds the Jedi numbers to its rule
  check; `tests/server_drop_notice_tests.py` and
  `tests/wolfet_map_load_tests.py` follow the new calls.

## Live runs

`scripts/probe_jedi_listen.py`, every process on a private desktop with its
own home, windowed, silent, IPv4 alone on 127.0.0.1, with no input. A
probe paces its packets at 1.1 seconds: `getinfo`, `getstatus`,
`getchallenge` and rcon share one per-address bucket on the server, ten at
once and then one a second, which a faster probe empties.

Run `20261006-190020`, 70 checks, each run checked against its own build's
expected behaviour, all as expected:

| Case | `before` (HEAD) | `after` |
| --- | --- | --- |
| `jka-listen`: a client hosts, `+set g_gametype 2 +map mp/ffa1` | its own client enters; `map` hands the game 0; the client game then stops at the bolt clock and the server with it | its own client enters; the game refuses 2 and runs 0; the same stop |
| `jka-host`: the client executable with `dedicated 1`, `+set g_gametype 2 +map mp/ffa1` | game type 0 by `map`; all three queries answered; ProbeGuest enters and `getinfo` counts one client | game type 0, the game refusing 2; all three answered; ProbeGuest enters, one client |
| `jka-host-sp`: the same, `+spmap mp/ffa1` | `spmap` sets 2, which the game refuses: 0; all three answered; ProbeGuest enters | **game type 5**; all three answered; ProbeGuest enters, one client |

The game-type steps, on the same dedicated-mode server (answered: `getinfo`,
`getstatus` and `getchallenge` all replied; silent: none did; rcon read the
running `g_gametype`):

| Step | `jka-mp` before | `jka-mp` after | `jk2-mp` before | `jk2-mp` after |
| --- | --- | --- | --- | --- |
| `+set g_gametype 2 +map` | 0, answered | 0 (the game refuses 2), answered | 0, answered | **2, answered** |
| `g_gametype 2`, `map_restart 0` | 0 (refused), answered | 0 (refused), answered | **2, silent** | 2, answered |
| `spmap` | 0 (2, refused), answered | **5, answered** | **2, silent** | 4, silent |
| `map` again | 0, answered | 5, answered | 0, answered | 0, answered |

Jedi Outcast's maps are `ffa_bespin`, Academy's `mp/ffa1`. HEAD hid an
Outcast Jedi Master game reached by `map_restart` and started Jedi Master for
`spmap`; this change answers in Jedi Master and keeps single player to
itself. In Academy HEAD's `spmap` fell back to a free-for-all; it now starts
single player, which answers remote players as `jamp.exe`'s does.

**A true Jedi Academy listen server cannot run yet.** Its host's own client
game stops at its first Ghoul2 bolt query (`Academy cgame bolt queries require
the measured remote-client frame clock; local-server clock sharing is not
served yet`), an open boundary of the
[client record](2026-10-02-jka-compatibility.md), and takes its server down,
before and after this change alike. So the queries and the second client went
to the client executable's own server run with `dedicated 1`. That is the
same compiled server: the three listen tests are compiled out of
`fntech3.ded` only, and nothing in them reads `dedicated`. Its second client,
ProbeGuest, joined and entered on the first map and logged no error in the
three seconds watched; fnTech3's Academy client game still stops at a Ghoul2
attachment after a map change
([live-join record](2026-10-06-jka-live-join.md)), so no client was kept
through one. Jedi Outcast's fnTech3 client stops at UI import 200, so its
servers were only asked.

## What this does not establish

- No retail program ran. Retail clients stand where the
  [legacy-server record](2026-10-05-legacy-server-hosting.md) left them: a
  retail `jamp.exe` lists fnTech3's servers but needs disc 1 before it
  connects, and a retail `jk2mp.exe` lists fnTech3's dedicated server; no
  retail client has asked an fnTech3 listen server anything.
- No Jedi Master game was played in Academy: the retail game refuses it, and
  no mod game was run.
- No game was played: servers ran their first map with spectators.

## Presentation (invariant 11)

Not applicable: this changes which game type a map command selects and
whether a server answers a query, not how anything is drawn, heard or
controlled.
