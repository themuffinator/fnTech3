# Drop notices: RTCW MP and ET

## Scope

When a client leaves, `SV_DropClient` broadcasts a notice naming it and
giving the reason, then addresses a `disconnect` command to the client that
left. fnTech3 broadcast Quake III's notice, `print "%s^7 %s\n"`, in every
profile. Retail `WolfMP.exe` broadcasts `print "[lof]%s^7 [lon]%s\n"`. RTCW
MP's client game translates a print unless `[lof]` turns that off, so the
name stays as it is and the reason is translated. Without the tokens, a
client in another language cannot translate the reason on its own. Retail
Enemy Territory 2.60b broadcasts `cpm "%s^7 %s\n"`, which its client game
shows as a pop-up message. It sends no notice at all when a bot leaves a
single-player game type.

This record covers the `rtcw-mp` and `wolfet` profiles. Only the broadcast
notice changes. The `disconnect` command addressed to the client that left is
unchanged in every profile, and so is every other profile's notice. The other
retail engines were read as well; where they differ from fnTech3, the
differences are listed under Open.

## Observed

### Sources

- The [RTCW MP](https://github.com/id-Software/RTCW-MP/blob/master/src/server/sv_client.c)
  `SV_DropClient` broadcasts `print "[lof]%s" S_COLOR_WHITE " [lon]%s\n"`
  unconditionally, then sends `disconnect "%s"` to the client.
- The [ET](https://github.com/id-Software/Enemy-Territory/blob/40342a9e3690cb5b627a433d4d5cbf30e3c57698/src/server/sv_client.c)
  `SV_DropClient` broadcasts `cpm "%s" S_COLOR_WHITE " %s\n"` only when
  `!SV_GameIsSinglePlayer() || !isBot`. The tokenized `print` line is
  commented out beside it ("we want this displayed elsewhere now"). `isBot`
  is set when the client's entity carries `SVF_BOT` or its address is
  `NA_BOT`. The addressed command is `disconnect "%s"\n`.
  `SV_GameIsSinglePlayer` (`sv_game.c`) tests `com_gameInfo.spGameTypes &
  (1 << g_gametype)`, and `spGameTypes` comes from `gameinfo.dat`.
- Both games' `CG_LocalizeServerCommand` (`cg_servercmds.c`) translate by
  default: `[lof]` turns translation off and `[lon]` back on. RTCW MP's
  `print` and ET's `cpm` handlers pass their text through it.
- RTCW MP's dictionary, `scripts/translation.cfg` in `mp_pakmaps0.pk3`, has
  entries for `player kicked`, `was kicked`, `disconnected` and `timed out`.

### Retail executables

Read with capstone. Addresses are virtual addresses in each image. In every
row the notice's format is pushed with a null client and passed to the
engine's `SV_SendServerCommand`. That function holds the dedicated server's
`broadcast: %s` echo in every engine but Elite Force SP's and the two Jedi
single players', whose version hands the command to client 0 alone.

The two engines this change follows:

| | `WolfMP.exe` (`Wolf 1.4-MP`) | ET 2.60b `et.exe` | ET 2.60b `etded.exe` |
| --- | --- | --- | --- |
| SHA-256 | `7cf99a938cf18ed2f5aecd0be542cbae8868959e0cce24b4b6eea08991e321f5` | `4d1326c5936b61c5f878fc017e4aa0abddf13e008139e31fce2f13d57b26243e` | `7eb606913a56d6901e358a598fb0079120b2730e113a10fc963f90b777580cdf` |
| `SV_DropClient` | `0x434eb0` | `0x441880` | `0x427ea0` |
| Notice | `print "[lof]%s^7 [lon]%s\n"` at `0x4eeea4`, pushed at `0x434f3d`, sent at `0x434f44` | `cpm "%s^7 %s\n"` at `0x561e48`, pushed at `0x441937`, sent at `0x44193e` | the same at `0x4c3440`, pushed at `0x427f57`, sent at `0x427f5e` |
| When | always | unless `SV_GameIsSinglePlayer` (`0x442cf0`, called at `0x441918`) and the client is a bot: `SVF_BOT`, the value 8, in the entity's `r.svFlags` at `+0x128` (`0x44189c`), or address type 0, `NA_BOT` (`0x4418ac`) | the same: `0x429440` called at `0x427f38`; `0x427ebc` and `0x427ecc` |
| Addressed | `disconnect "%s"` at `0x4eee78`, pushed at `0x434fa4` | `disconnect "%s"\n` at `0x561e74`, pushed at `0x4419a1` | the same at `0x4c346c`, pushed at `0x427fc1` |
| `SV_SendServerCommand` | `0x439f40` | `0x4487c0` | `0x42f010` |

ET's `SV_GameIsSinglePlayer` shifts 1 by `g_gametype->integer` and masks
`spGameTypes` with it. Retail's `gameinfo.dat` in `etmain/pak0.pk3` sets
`spGameTypes 0` and `coopGameTypes 1`, so in retail only a bot leaving game
type 0 goes unannounced, and co-op still announces bots. fnTech3's
`SV_GameTypeIsSinglePlayer` reads the same file.

The other supported engines, which this change leaves alone:

| Profile | Executable, SHA-256 | `SV_DropClient` | Notice | Addressed |
| --- | --- | --- | --- | --- |
| `q3` | `quake3.exe` 1.32, `80b0add0e014b863cf47cab6c997e0a54939236219a56910b7a3eb77dc2d44d4` | `0x434640` | `print "%s^7 %s\n"`, `0x434710` | `disconnect "%s"`, `0x434778` |
| `q3-ihv` | IHV `quake3.exe`, `e9fcaa0a5682e7f07d540aacee08cdb77f3ea40190719f5cecf31109a5135d37` | `0x4186e0` | `print "%s %s\n"`, `0x4186f2` | `disconnect`, `0x41870e` |
| `ql` | `quakelive_steam.exe`, `c926fe9f6c851e00b3b9332e88903ad01f28fdd60454873891c0158f5ded1299` | `0x4df660` | `print "%s^7 %s\n"`, `0x4df73c`; `print "%s ^1rage^7quits\n"`, `0x4df729`, instead when the client field at `+0x25b50` is set (`0x4df716`) | `disconnect "%s"`, `0x4df782` |
| `rtcw-sp` | `WolfSP.exe`, `07d206f61da5c80562ff60d68c645bca349b31adb5d7e0e7f186d61ad46683c7` | `0x4326b0` | `print "%s^7 %s\n"`, `0x4326e6`, not for an entity with `SVF_CASTAI` (`0x4326d1`) | `disconnect`, `0x43276c`, not for `SVF_CASTAI` after the game's callback (`0x432763`) |
| `stvef-sp` | `stvoy.exe`, `8f7ccadb52b4230689c5b3275177f7ec7060a30bfaa3ade4e7b3fb0cda0dff74` | `0x424840` | `print "%s %s\n"`, `0x4248a1` | `disconnect`, `0x4248b0` |
| `stvef-mp` | `stvoyHM.exe`, `5b5b6e82ce7ac0d720a45ab1fa9df32930c846d6c51847fa0c158ebc86487dff` | `0x42faf0` | `print "%s^7 %s\n"`, `0x42fb56` | `disconnect %s`, `0x42fbbf` |
| `jka-sp` | `jasp.exe`, `9351a8de230fcd577e2b474889d08f38982450306767a6823dd659482566d6ce` | `0x438390` | `print "%s %s\n"`, `0x4383e9` | `disconnect`, `0x4383f5` |
| `jka-mp` | `jamp.exe`, `e8062d3948ff05917b48e47d8eec0ee43bcc6fb03924410712412404ddfee28b` | `0x457ce0` | `print "%s^7 %s\n"`, `0x457d6d` | `disconnect "%s"`, `0x457dd5`, formatted by `va` and passed on as the format |
| `jk2-sp` | `jk2sp.exe`, `0d3c635ee5b20ac32fc957e00cf21e65ebc1584112b11f6ae026706dcc0d0e0c` | `0x435b80` | `print "%s %s\n"`, `0x435bde` | `disconnect`, `0x435bea` |
| `jk2-mp` | `jk2mp.exe`, `2f08dcf9a089cb66062766f4922fda63a9e553de893c174b497dfefeb7a1577a` | `0x449260` | `print "%s^7 %s\n"`, `0x4492eb` | `disconnect`, `0x449354` |
| `alice` | `alice.exe`, `8aa22917b7707f0e1a7c4dbe9a3c38332b1873ae2059174ac5329de26921fc18` | `0x4529e0` | `print "%s %s\n"`, `0x4529fb` | `disconnect`, `0x452a6b` |

Addresses in the Notice and Addressed columns are where each string's address
is pushed. The [evidence file](evidence/2026-09-24-rtcw-et-drop-notices.json)
also gives each string's own address and each call. What follows from the
tables:

- Quake III 1.32, Quake Live, RTCW SP, Holomatch, Jedi Academy MP and Jedi
  Outcast MP broadcast what fnTech3 broadcasts, `print "%s^7 %s\n"`, with RTCW
  SP's cast-AI rule, which fnTech3 already has.
- The IHV test, Elite Force SP, Jedi Academy SP, Jedi Outcast SP and Alice
  broadcast `print "%s %s\n"`, without `^7`. Elite Force SP and both Jedi
  single players mark the client a zombie and call the game's
  `ClientDisconnect` before the notice rather than after it. The IHV test
  has no zombie check.
- Quake III 1.32, Quake Live, RTCW MP and Jedi Academy MP address
  `disconnect "%s"` to the client, as fnTech3 does in every profile, and ET
  `disconnect "%s"\n`. The rest send other forms, listed under Open.
- Quake Live's `+0x25b50` field is set by `0x4df830`, which game import
  192 (`0x4e26e0`, entry `0x56d280` of the table at `0x56cf80`) calls for
  a valid client. The retail `qagamex86.dll` in `bin.pk3`
  (`9bfad1b5df4cbbb3fcfb20781024fc5c0abe73ba8389c2f20be8c0a55552e83d`) calls
  that import from its `ragequit` client command, at `0x10046015`.

### Kick commands

- `WolfMP.exe`'s `kick <name>`, `kick all` and `clientkick` drop with
  `player kicked` (`0x4ee45c`, pushed at `0x43371a`, `0x43365a` and
  `0x433c0f`), and `kick allbots` with `was kicked` (`0x4ee448`, `0x4336cc`).
- Neither `et.exe` nor `etded.exe` contains a `kick` or `clientkick`
  command or either reason. ET's game module registers both
  (`g_svcmds.c`), drops with `player kicked` through `trap_DropClient` and
  bans the address for a while through the engine's `SV_GameDropClient`.

## Change

- `SV_DropClient` (`code/server/sv_client.cpp`) broadcasts
  `print "[lof]%s^7 [lon]%s\n"` in `rtcw-mp` and `cpm "%s^7 %s\n"` in
  `wolfet`. Every other profile keeps `print "%s^7 %s\n"`.
- A new `SV_AnnouncesDrop` holds when a drop is announced. RTCW SP's cast-AI
  rule moved there unchanged. ET's is new: no notice when
  `SV_GameTypeIsSinglePlayer( g_gametype )` holds and the client is a bot by
  its address or by its entity's `SVF_BOT`. The entity is read only in that
  case, and one the game-data view cannot read raises `ERR_DROP`, as
  `SV_ClientIsRTCWCastAI` does.
- Unchanged: the addressed `disconnect "%s"` in every profile, and fnTech3's
  silent drop with no reason, which sends neither command.

The RTCW, ET and Quake III sources, the qagame module and the retail
executables were read as reference observations. No text from them is
incorporated.

## Evidence

- `tests/server_drop_notice_tests.py` compiles the production
  `SV_AnnouncesDrop` and `SV_DropClient` with the real game-data view, entity
  reader, server-flag mapping and ET game-type reader, and binds frozen
  module records. It checks, in order, everything `SV_DropClient` sends:
  - every profile in the catalog, a player and a bot, in ten game types,
    with a reason and without one: RTCW MP's and ET's retail notices, Quake
    III's in every other profile, and `disconnect "%s"` everywhere. Names
    and reasons holding `%s`, `%d`, quotes and colours arrive as text.
    `gameinfo.dat` is read in ET alone;
  - ET with its records bound, for six flag sets, both address types and
    ten game types: silent only in game type 0 for a bot by address or by
    `SVF_BOT` at `+0x128`. Co-op announces. The notice is decided before the
    game hears of the drop, whatever the game then does to the entity. An
    unreadable entity or a stale binding is refused where it would be read;
    a bot's address or another game type needs no read;
  - retail `gameinfo.dat` against another that makes 0 and 3 single player,
    and against none;
  - RTCW SP keeping quiet about its cast AI, notice and command alike, and
    RTCW MP announcing every client, cast bit or not.

  Fifteen in-memory mutations of the compiled production code each fail it:
  RTCW MP without its tokens, as before; the tokens swapped; the tokens in
  RTCW SP too; ET printing its notice; ET's notice sent only to the client
  that left; an addressed command changed with the notice; a bot by address
  announced; every ET game type single player; co-op single player; Quake
  III's single-player game type deciding; every profile quiet about
  single-player bots; the address alone deciding; RTCW's cast bit deciding;
  an unreadable entity taken for a player's; RTCW MP quiet about its cast AI.
- `tests/rtcw_cast_lifecycle_tests.py`, which also compiles the production
  `SV_DropClient`, now expects RTCW MP's tokens, and passes.
- The tree of `2d35919` with this change alone was exported and built for
  Windows x86 with MSVC, `debugoptimized` and strict warnings, with the
  RTCW, ET and Elite Force development options, as `meson/build/win32-dev`
  has them. All 1,200 targets built without a warning. Its 116 Meson C++
  tests pass and two skip: OpenAL's `ALC_SOFT_loopback` was unavailable, and
  the retail factory probe found no Quake Live without
  `FNQL_RETAIL_QL_PATH`. The whole Python suite passes in the same export:
  1,833 tests, with 76 skipped. The probes below ran a dedicated-server-only
  build of the same tree, `fntech3.ded.exe`
  `9a8d7ba0f5768b09f589125d68f48675aff360b370b804406e08dd4fedbc6b76`, and of
  `2d35919` alone as the baseline,
  `faf79479ffec484c84ec2796d3db73fc79d4b6d0c161a955bec829d44eb26112`.
- Runtime, interop. `scripts/probe_drop_notice.py` runs a dedicated server
  and two retail clients, `WolfMP.exe` or `et.exe`, on loopback. Each runs on
  its own private desktop with an isolated home under `.tmp/drop-notice/`,
  and no input is injected. The client to be dropped joins first, in slot 0,
  and an observer joins after it. With both in the game, rcon execs a config
  holding `clientkick 0`, so the server's own output reaches its log. Both
  clients log every server command they execute (`developer 1`,
  `cl_showServerCommands 1`). RTCW MP played `mp_beach` in game type 5, ET
  `goldrush` in game type 2, both pure. The installations' modules and paks
  are in the evidence file, and every run left the installation's inventory
  unchanged.

  | Server | The observer executed | The dropped client executed |
  | --- | --- | --- |
  | Retail `WolfMP.exe` | `print "[lof]fntech3-drop-probe^7 [lon]player kicked\n"`, printed as `fntech3-drop-probe^7 player kicked` | the same notice, then `disconnect "player kicked"`: `Server Disconnected - player kicked` |
  | fnTech3 `rtcw-mp`, baseline | `print "fntech3-drop-probe^7 was kicked\n"` | nothing: `CL_ParsePacketEntities: end of message` |
  | fnTech3 `rtcw-mp`, candidate | `print "[lof]fntech3-drop-probe^7 [lon]was kicked\n"`, printed as `fntech3-drop-probe^7 was kicked` | nothing: the same error |
  | Retail `etded.exe` | `cpm "fntech3-drop-probe^7 player kicked\n"`, printed as `fntech3-drop-probe^7 player kicked` | the same notice, then `disconnect "player kicked"` followed by a newline |
  | fnTech3 `wolfet`, baseline | `print "fntech3-drop-probe^7 was kicked\n"` | nothing: the same error |
  | fnTech3 `wolfet`, candidate | `cpm "fntech3-drop-probe^7 was kicked\n"`, printed as `fntech3-drop-probe^7 was kicked` | nothing: the same error |

  So the candidate's notices have retail's form, word for word apart from
  the reason. The reason differs because retail kicks with `player kicked`
  and fnTech3 with `was kicked`. The baseline's were Quake III's. RTCW MP's
  client game consumed the tokens: its console shows no `[lof]` or `[lon]`.
  The dropped client's error is the first fault under Found. The observers of
  every fnTech3 server in this batch also failed with it soon after
  executing the notice (the second fault). Neither retail observer did.

## Found, not changed

These are faults of the drop path that fnTech3 had before this change. The
runs above show both on the baseline as well.

- **The dropped client is sent a snapshot it cannot read.** For a zombie
  client, `SV_WriteSnapshotToClient` (`code/server/sv_snapshot.cpp`) writes
  Quake III's empty player-state delta, a zero byte and one zero bit, in
  every profile but Holomatch, then the entity end marker. RTCW's and ET's
  `MSG_ReadDeltaPlayerstate` read five more bits after the array bit, one
  for ammo and four for ammo in clip. So a retail client reads the end
  marker five bits late, misreads the entity list and fails with
  `CL_ParsePacketEntities: end of message` before it runs the notice or its
  `disconnect`. In a one-client run with a 15-second pause before the kick,
  the ET client executed twelve commands without error and failed only
  after the kick. Holomatch already writes its own empty delta through
  `MSG_WriteDeltaPlayerstateForWireProfile`. The same error in
  `2026-09-24-wolfenstein-restart-variable-change.md`'s runs that turned
  pure mode on under a client (uncommitted when this was written) may have
  the same cause; that was not checked.
- **Other clients can fail after a drop.** In each fnTech3 run of the final
  batch, the observer executed the notice and the command clearing the
  dropped player's configstring (`cs 576 ""` in RTCW MP, `cs 689 ""` in ET),
  then failed with the same error: in RTCW MP before its next command, in ET
  after two or three more. An earlier `rtcw-mp` baseline run's observer did
  not fail. The cause is not established; the dropped player's entity
  leaving the snapshot, and the observer's own player state, are the
  obvious suspects.
- **Kicks.** fnTech3 kicks with `was kicked` in every profile, where
  `WolfMP.exe` uses `player kicked` for everything but `kick allbots`. The
  RTCW MP dictionary translates both. fnTech3 registers engine `kick`,
  `clientkick`, `kicknum`, `kickall` and `kickbots` commands in the ET
  profile. Retail ET's engine has none, and they shadow the game's own
  `kick` and `clientkick`: there is no ban, and the reason differs.
- **The console echo.** At `2d35919` a dedicated server echoes the new RTCW
  MP notice with its tokens, `broadcast: print "[lof]fntech3-drop-probe^7
  [lon]was kicked\n"`, where `WolfMP.exe` echoed `broadcast: print
  "fntech3-drop-probe^7 player kicked\n"`. The token-free echo is
  `2026-09-24-rtcw-et-broadcast-echo.md`'s change, uncommitted when this was
  written, which covers every broadcast print.

## Open

- The IHV test, Elite Force SP, Jedi Academy SP, Jedi Outcast SP and Alice
  broadcast `print "%s %s\n"`, and fnTech3 still sends them `^7` after the
  name. Elite Force SP and both Jedi single players also send the notice
  after the game's `ClientDisconnect`, where fnTech3 sends it before.
- The addressed command. `WolfSP.exe`, the IHV test, Elite Force SP, both
  Jedi single players, Jedi Outcast MP and Alice send a bare `disconnect`.
  `stvoyHM.exe` sends `disconnect %s`, unquoted. ET adds a newline after the
  closing quote, which its client's tokenizer drops: the retail client above
  showed `Server Disconnected - player kicked`. Jedi Academy MP formats the
  command twice, so a `%` in the reason is read as a format. fnTech3 sends
  `disconnect "%s"` in all of them.
- Quake Live's `ragequit`: retail announces the client as
  `print "%s ^1rage^7quits\n"` once its game has called import 192. fnTech3
  binds that import as `MarkClientGotCP`, which sets `gotCP` instead, so a
  rage quit is announced with its reason like any other drop.
- ET's single-player bot rule rests on unit tests and the disassembly. No
  ET bot survives `spawnbot` under fnTech3 yet
  (`2026-09-24-wolfet-bot-broadcasts.md`, uncommitted when this was
  written), and no run used game type 0.
- Only `clientkick` drops were run, and only on `mp_beach` and `goldrush`.
  No fnTech3 client took part: the change is server-side, and the clients
  were retail.
