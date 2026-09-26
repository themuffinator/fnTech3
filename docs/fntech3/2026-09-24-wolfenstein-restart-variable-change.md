# RTCW MP and ET map_restart reload the level only for a new client count

## Scope

`map_restart` normally restarts the game module in place. The level stays
loaded, and each connected client gets a `map_restart` server command rather
than a new gamestate. fnTech3's `SV_MapRestart_f` had one shared check that
reloads the whole level instead, through `SV_SpawnServer`, printing `variable
change -- restarting.`. It applied to every profile but Quake Live, whenever
`sv_maxclients`, `g_gametype` or `sv_pure` had changed since the last level
spawn, or in Elite Force Holomatch an Expansion Pack mode. Retail Return to
Castle Wolfenstein multiplayer and Enemy Territory reload only for
`sv_maxclients`. This record covers the `rtcw-mp` and `wolfet` profiles. Quake
III, Quake Live, RTCW single player, both Elite Force modes and the other
profiles keep their check.

## Observed

### The retail check

Read with capstone. Addresses are virtual addresses in each image.

| | `WolfMP.exe` (`Wolf 1.4-MP`) | ET 2.60b `et.exe` | ET 2.60b `etded.exe` |
| --- | --- | --- | --- |
| SHA-256 | `7cf99a938cf18ed2f5aecd0be542cbae8868959e0cce24b4b6eea08991e321f5` | `4d1326c5936b61c5f878fc017e4aa0abddf13e008139e31fce2f13d57b26243e` | `7eb606913a56d6901e358a598fb0079120b2730e113a10fc963f90b777580cdf` |
| `SV_MapRestart_f` | `0x432eb0` | `0x43fa20` | `0x425e70` |
| Match-state transition, which may refuse the restart | `0x432fd0..0x433010` | `0x43fa9c..0x43faf0` | `0x425f0b..0x425f5e` |
| The only reload test | `0x433016..0x43301f`: `sv_maxclients` (`0xae8f74`) `->modified`, at +`0x14` | `0x43faf6..0x43faff`: `0x1184e60`, +`0x14` | `0x425f64..0x425f6d`: `0xbfb7a0`, +`0x14` |
| Message | `sv_maxclients variable change -- restarting.\n`, string `0x4ee298`, pushed at `0x433021` | `0x5617a8`, `0x43fb01` | `0x4c2d14`, `0x425f6f` |
| Reload | `SV_SpawnServer( mapname, qfalse )`, `0x439260`, called at `0x43304e` | `0x4467f0` at `0x43fb2d` | `0x42cf20` at `0x425f9b` |
| In-place restart begins | `0x43305d` | `0x43fb3a` | `0x425fb2` |

Each image holds one string containing `variable change`, the one above.
Neither engine's `SV_MapRestart_f` reads the `modified` flag of `g_gametype` or
of `sv_pure`; `WolfMP.exe` reads `g_gametype`'s value only to keep a type the
map allows (`0x432ef6`). The GPL
[RTCW MP](https://github.com/id-Software/RTCW-MP/blob/master/src/server/sv_ccmds.c)
and [ET](https://github.com/id-Software/Enemy-Territory/blob/40342a9e3690cb5b627a433d4d5cbf30e3c57698/src/server/sv_ccmds.c)
`sv_ccmds.c` agree: `if ( sv_maxclients->modified )`, with the same message.
ET's GPL server comments `sv_gametype` out, its `modified` reset in
`SV_SpawnServer` included, and reads the game type only through `g_gameType`
in `SV_GameIsSinglePlayer` and `SV_GameIsCoop`.

`WolfMP.exe`'s in-place restart toggles the snapshot's server-count bit
(`0x43305d..0x433065`), takes a new `sv.serverId` and `sv_serverid`, sets
`sv_serverRestarting`, restarts the game module (`0x4330a8`) and runs three
frames. Then, for every client at `CS_CONNECTED` or above, it queues
`map_restart\n` (`0x43311a..0x433123`), calls `GAME_CLIENT_CONNECT` without
`firstTime`, makes the client active and enters it into the world
(`0x43316c..0x433174`). No gamestate is sent. ET's does the same with the
differences `2026-09-24-wolfet-pending-map-restart.md` lists, six settling
frames among them.

For comparison, and unchanged here: Quake III 1.32 `quake3.exe`, SHA-256
`80b0add0e014b863cf47cab6c997e0a54939236219a56910b7a3eb77dc2d44d4`, tests
`sv_maxclients` (`0x96d164`) at `0x432a50` and `g_gametype` (`0x96d160`) at
`0x432a5f`. `WolfSP.exe`, SHA-256
`07d206f61da5c80562ff60d68c645bca349b31adb5d7e0e7f186d61ad46683c7`, tests
`sv_maxclients` (`0xb99e44`) at `0x4308b6` and `g_gametype` (`0xb99e40`) at
`0x4308c5`. Both print `variable change -- restarting.`. Neither tests
`sv_pure`. fnTech3's `sv_pure` term comes from the pinned FnQuake3 baseline,
`7d8c9629`, whose `SV_MapRestart_f` tests all three and whose `sv_pure` is
`CVAR_SYSTEMINFO | CVAR_LATCH`.

### sv_pure in the retail engines

Both engines register `sv_pure` with the default `"1"` and flags 8,
`CVAR_SYSTEMINFO`, without a latch: `WolfMP.exe` at `0x43994a..0x43996c` into
`0xae8f40`, `et.exe` at `0x44601d..0x44603f` into `0x1184e00`. Every read of
either pointer takes `->integer`, at +`0x20`; none takes `->modified`:

| Reader | `WolfMP.exe` | `et.exe` |
| --- | --- | --- |
| `SV_SpawnServer`, which sets `sv_paks` and `sv_pakNames` | `0x4395d7` | `0x446b32` |
| `SV_VerifyPaks_f`: returns at once when not pure, and sets `gotCP` only when pure | `0x43597a`; `gotCP`, +`0x4c730`, at `0x435bdd` | `0x44094d`; +`0x546cc` at `0x440b75` |
| `SV_UserMove`: when pure, not proved and without a `cp`, an active client is sent its gamestate again, `%s: didn't get cp command, resending gamestate` | `0x4363ce..0x43641e` | `0x4421a1..0x4421e7` |
| `SV_UserMove`: when pure and not proved, `Cannot validate pure client!` | `0x436432..0x436461` | `0x4421fa..0x442225` |
| `SVC_Info`, key `pure` | `0x43a7d7` | `0x447274` |
| Download refusal text | `0x43568c` | `0x442896` |
| `SV_TouchCGameDLL` | none | `0x446d63` |

`map_restart` reads none of these. So a new `sv_pure` is not simply deferred
in retail. The lists of paks a client may load from, `sv_paks` and
`sv_pakNames`, wait for the next level spawn, but proof checking follows the
live value. Once `sv_pure` is on, each
active client that has not proved its paks is sent its gamestate again at its
next command and must send a `cp`. Once it is off, checking stops.

fnTech3, in every profile, copies `sv_pure` into `sv.pure` at each level
spawn. Its `SV_UserMove`, `SV_VerifyPaks_f`, download refusal and `getinfo`
read `sv.pure`, and its `SV_UserMove` resends a gamestate only to a primed
client. A gamestate's systeminfo carries `sv.pure`; the `cs 1` update that
follows a change carries the live value. So in fnTech3 a new `sv_pure` waits
for the next level spawn entirely, and before this change `map_restart` was
such a spawn in RTCW MP and ET.

### What a connected client sees

`scripts/probe_restart_variable_change.py` runs a dedicated server and one
retail client, `WolfMP.exe` or `et.exe`, joined to it over loopback UDP. Each
runs on its own unswitched private desktop with an isolated home, and no input
is injected. The probe drives the server over rcon, one config per step, and
after each step has the server `say` a marker until the client's log shows it.
The client logs every server command it executes (`cl_showServerCommands`
with `developer`), its cgame's own `CG_MapRestart` (`cg_showmiss`) and each
gamestate it loads (`CL_InitCGame`). An RTCW MP run first waits for the
game's own warmup restart, which starts play once a player has joined. RTCW MP
plays `mp_beach` and changes objective (5) to stopwatch (6); ET plays
`goldrush` and changes objective (2) to stopwatch (3).

The servers were retail `WolfMP.exe` with `dedicated 1`, retail `etded.exe`,
and two fnTech3 dedicated builds from clean exports of `2d35919`: the
baseline, `ec0348ec5f2bf6a8dc4bc8c6f2f26b8003910c374bf489815eb9603e8b09c685`,
and the candidate, the same export plus only this change,
`576488b78dc696254e086eec269858c5134642ae2b926ef869aa749cf724bd4b`. Every run
used the retail modules and paks the
[evidence file](2026-09-24-wolfenstein-restart-variable-change.json)
lists, and left the installation's inventory unchanged. The two games agreed
at every step, so each cell below holds for both.

Plan `restarts`: the server is pure from the start, so the client has proved
its paks before anything changes.

| Step | Retail | Baseline | Candidate |
| --- | --- | --- | --- |
| `g_gametype 6` (ET `3`), `map_restart` | The game initializes again, with no level spawn and no gamestate. The client executes `map_restart`, its cgame runs `CG_MapRestart`, and a serverinfo update brings the new type | `variable change -- restarting.`: a level spawn, and the client loads a new gamestate. No `map_restart` reaches it | As retail |
| `sv_pure 0`, `map_restart` | As above, with no new type | As above | As retail |
| `sv_maxclients 12`, `map_restart` | `sv_maxclients variable change -- restarting.`: a level spawn and a new gamestate | `variable change -- restarting.`, then the spawn crashes with a stack overflow | `sv_maxclients variable change -- restarting.`, then the same crash |

Plan `pure-live`: the server starts non-pure, so the client has never sent a
proof.

| Step | Retail | Baseline | Candidate |
| --- | --- | --- | --- |
| `sv_pure 1`, no restart | `didn't get cp command, resending gamestate`: the client loads its gamestate again and sends a `cp`. The outcome varied, as below | The client sees `sv_pure 1` in a systeminfo update, and nothing else happens | As baseline |
| `map_restart` | In place, as in `restarts` | `variable change -- restarting.`: a level spawn and a new gamestate | In place, as retail |

Five retail runs turned pure mode on under a connected client. In three the
new proof passed and the client stayed: RTCW MP with no restart before it and
after a game-type restart, and ET with no restart before it. In two the
client's proof named no cgame or UI pak and the server dropped it with
`Unpure client detected. Invalid .PK3 files referenced!`: RTCW MP after the
game's own warmup restart, and ET after a game-type restart. The runs do not
show what decides it. In ET the client printed `Delta from invalid frame (not
supposed to happen!)` before `Server disconnected for unknown reason`.

So `map_restart` never makes a retail client load the level again after a new
game type or pure mode. The game restarts under it, its cgame handles
`map_restart`, and a new type arrives as a configstring. Only a new
`sv_maxclients` sends it a new gamestate. Turning pure mode on can do that by
itself, with no restart, as above. On the baseline the client loaded the
level again at `map_restart` after either change.

The shared developer build before this change,
`d2c23bd15f0fe2c2bf09b340dfa978e52703a5b2a4502a78cdb2c3b889218d11`, which
also carried other sessions' uncommitted work, behaved as the baseline in all
four runs.

## Change

- `SV_MapRestart_f` (`sv_ccmds.cpp`): in `rtcw-mp` and `wolfet`, only
  `sv_maxclients->modified` reloads the level, with retail's message. A new
  game type or `sv_pure` restarts the game in place. Their `modified` flags
  stay set until the next level spawn clears them, which already happened in
  both profiles and which nothing else reads there. The test still follows
  the match-state transition, so a refused transition never reloads, as in
  retail.
- Every other profile's condition and message are unchanged, and Quake Live's
  own checks are untouched.
- `sv.pure` is untouched. A new `sv_pure` takes effect at the next level spawn
  in these profiles too, as it does in every other.

## Evidence

- `tests/restart_variable_change_tests.py` compiles the production check out
  of `SV_MapRestart_f`, from its flag to the end of its reload block, and runs
  it for all thirteen profiles against each of the sixteen combinations of a
  changed `sv_maxclients`, `g_gametype`, `sv_pure` and Holomatch mode. The
  expected reloads are literal masks: `rtcw-mp` and `wolfet` exactly when
  `sv_maxclients` changed, Quake Live never in this block, `stvef-mp` for any
  of the four, and every other profile for any of the first three. Each
  reload must spawn the current map without killing bots and print its
  profile's message; otherwise nothing is printed or spawned, and the command
  goes on to its in-place restart. Six mutations each fail it: the shared
  check restored, either profile left out, every profile ignoring game type
  and pure mode, no client-count reload in the two, and the shared message.
  It also checks that the block follows the match-state transition and
  precedes the snapshot bit's toggle.
- `tests/rtcw_match_restart_tests.py`, which compiles the command up to this
  check, passes unchanged.
- The runtime runs above. The
  [evidence file](2026-09-24-wolfenstein-restart-variable-change.json)
  holds each run's commands, hashes and per-step counts, including the
  earlier four-step runs the retail proof outcomes come from.
- Both exports built with strict warnings. The full Python suite passed on the
  candidate export with this change's test and probe added: 1,833 passed and
  76 skipped, each skip for a missing executable, installation or privilege.
  The eleven test files that read `sv_ccmds.cpp` passed again after the
  hunk's comment was reworded, which the candidate above carries. An earlier
  candidate with the first wording made the same four runs with the same
  results.

## Not established

- **Live pure-proof checking.** Retail sends each active client that has not
  proved its paks its gamestate again once `sv_pure` comes on, and stops
  checking once it goes off. fnTech3 waits for the next level spawn, in every
  profile, through `sv.pure`. Its `SV_UserMove` asks only a primed client for
  a proof, so reading the live value there would drop an active client that
  has none rather than ask it. This change does not touch it.
- **fnTech3 never completed the reload this change keeps.** Changing
  `sv_maxclients` on a running fnTech3 server crashes it, before and after
  this change, with `STACK_OVERFLOW` in `_chkstk`. In the baseline build,
  `SV_ChangeMaxClients` and `SV_AllocClients` have 527,332- and
  527,320-byte frames, each holding a `client_t` temporary, and the first
  calls the second on a 1,048,576-byte stack. The baseline's Quake III server,
  with no client, crashes the same way after `sv_maxclients 12` and
  `map q3dm1`. The runs show the message and the reload call, not a completed
  reload.
- Only `mp_beach` and `goldrush`, and only stopwatch as the new type.
  Checkpoint, capture and hold, last man standing, campaign, and ET's
  single-player and co-op types were not run. Nor was the game type RTCW MP's
  `map_restart` forces when the map forbids the one set
  (`2026-09-23-rtcw-mp-restart-game-type.md`, uncommitted when this was
  written). That record lists the reload after such a replacement as open.
  The check above now leaves it in place, but no run exercised it.
- No fnTech3 client took part. The change is server-side, and the clients
  were retail.
- ET's client stopped logging server commands after its first restart,
  because its `developer` went to 0 when a systeminfo update arrived. The
  cause was not traced. Its `CG_MapRestart` and `CL_InitCGame` lines still
  show each step.
- Quake III 1.32 and `WolfSP.exe` do not test `sv_pure`, while fnTech3's `q3`
  and `rtcw-sp` checks do. This change leaves them alone.
- iortcw and ET: Legacy were not compared.
