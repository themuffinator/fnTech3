# Medal of Honor: Allied Assault profile, installation and module contracts

## Scope

Medal of Honor: Allied Assault is a tenth title and a fifteenth profile,
`mohaa` (aliases `medalofhonor`, `moh-aa`, `alliedassault`), over one `main/`
installation. It runs 2015's engine, which descends from the Ritual engine
Alice's and F.A.K.K.2's come from and from Quake III: `2015` maps, TIKI
models, `.urc` menus, `.RitualFont` fonts, Quake III's Huffman-coded
messages, and function-table game and client-game libraries whose tables and
records are 2015's own. fnTech3 serves it through adapters of its own. It is
not routed through the Ritual (Alice and F.A.K.K.2) profile test: every
branch names `mohaa` itself. It has one mode, single player and multiplayer
being game types of the same executable.

The profile is experimental. Default builds refuse it;
`-Dmohaa-development=true` lets the shared client and dedicated server
activate it for isolated probes. The runtime evidence below is from developer
builds on Windows x86 (`meson/build/win32-mohaa`). The gates are tracked in
the [compatibility ledger](COMPATIBILITY.md#medal-of-honor-gate-matrix); the
maps are described in the [map record](2026-09-24-mohaa-maps.md).

## Evidence and provenance

The user's legitimate retail copy is at `E:\Games\Medal of Honor Allied
Assault`: 1.00 US, laid down from the user's two discs without running the
installer. It was only read. No retail file is committed.

| File | Bytes | Linked | SHA-256 |
| --- | --- | --- | --- |
| `MOHAA.exe` (engine) | 2,511,445 | 2001-12-06 | `7abe43b41746c87877507d4c2938f6f4c9471f8d559448eba4553faa962316eb` |
| `system86.dll` (module allocator) | 106,545 | 2001-12-04 | `9971bbebf1692b0fabba6831563bca4bdead8b68964fe2327b2535656f29ed0b` |
| `main/gamex86.dll` (game) | 2,220,085 | 2001-12-06 | `2f37ead1a91aae850dd8cdb29c6b5774864f6ae8654fb79d3593e45d9d3fc55c` |
| `main/cgamex86.dll` (client game) | 630,838 | 2001-12-06 | `cb2be40ce4d9248234652d44bbb02c41b0438886b34de56e5de9910f16679c2c` |
| `main/Pak0.pk3` | 98,668,934 | | `6caf6d93cc996b6895d5015f1be153724e5369b8c063c73e1609578323261df6` |
| `main/Pak1.pk3` | 119,631,347 | | `60ed94c185c4f81813aefaf1603b16734e65eb1b931737ae588df84c0a2a4b80` |
| `main/Pak2.pk3` | 365,587,748 | | `00133b93d350b5d82ff2d59a7d87fe94dad18147d28b6fd59dd2b5a6c816acce` |
| `main/Pak3.pk3` | 87,555,605 | | `1e0eed04b234bae260097813755325d2533bb6082c5fc9e98332aa66c4f4db92` |
| `main/Pak4.pk3` | 34,046,379 | | `853cc70610933e83284db95e6144ed3c02d81190065b44aae94ba58ca09f3130` |
| `main/Pak5.pk3` | 87,721,422 | | `b6035d8686bed133be76cdee322c37fd4a9814ba88bbce3e91f20571ea7b2a1d` |

**`MOHAA.exe` cannot be read or run.** It is SafeDisc-wrapped: besides
`.text`, `.rdata`, `.data` and `.rsrc` it carries the loader sections
`stxt774` and `stxt371`, and its code and data are encrypted on disk.
SafeDisc's driver does not load on Windows 11, so the executable cannot run
here either, as a client or as a server. No retail session, demo, save or
frame exists to compare with, and none can be made on this machine. Every
fact about the engine itself is therefore one of:

- **measured in the two game libraries**, which are plain PEs (image bases
  0x31000000 and 0x30000000): their table copies, record sizes, call sites,
  command strings and configstring bounds, cited by address here, in
  `code/compat/mohaa_game_abi.h`, `code/compat/mohaa_cgame_abi.h` and the
  `.def` tables beside them;
- **read from the data**: the maps, the `.urc` menus, `ui/BIND.SCR`, the
  shader scripts, the sound aliases and the configurations in the paks;
- **OpenMoHAA's reconstruction** of 2015's engine
  ([openmoh/openmohaa](https://github.com/openmoh/openmohaa/tree/ab43a0def6c4feccedae36fdb58c2e007d7e4c35),
  GPL-2.0-or-later), marked **[O]** below. Its model runtime, message tables,
  protocol-6 client parse, snapshot walk, world traces and terrain collision
  are incorporated, each file keeping its licence header (see
  [`code/mohaa/README.md`](../../code/mohaa/README.md) and the notices); the
  rest of it is reference observation. It describes the later 1.11 and 2.x
  modules as well as 1.00, so where the 1.00 libraries disagree they decide,
  and the difference is written beside the member or rule;
- **inference**, marked **[I]**.

## Profile and installation

- The installation is recognised by `main/Pak0.pk3` beside `MOHAA.exe`; the
  six paks' names are capitalised. The game directory is `main`.
- `system86.dll` lies beside the executable. Both game libraries import
  `MemoryMalloc` and `MemoryFree` from it by name, and 2015's engine serves
  the modules' own `Malloc`, `Free` and file buffers from the same heap: the
  game frees file buffers through its `Free` import (gamex86 0x3103d449). The
  user's copy is loaded from the installation before either library
  (`code/mohaa/mohaa_system86.h`); fnTech3 never ships it.
- The configuration is `configs/unnamedsoldier.cfg` **[O]**, the default
  player profile's; the dedicated server's is
  `configs/unnamedsoldier_server.cfg` **[I]**.
- Protocol 6 **[O]** ("MoHAA v 1.00 uses protocol version 6"); the default
  server port is 12203 **[O]**.
- 64 clients (the client game's bound), 2,736 configstrings (the client game
  bounds an index at 2,736, 0x300234dd), the players' block at 1,684 (light
  styles are dispatched over [1652, 1684), 0x3002324e..0x30023267), 1,024
  submodels. `CS_MESSAGE` is 2 and `CS_SAVENAME` 3 in 1.00, where Quake III's
  `CS_MESSAGE` is 3; the weapons' names start past the players' block, at
  1,748.
- `g_gametype 0` is single player; a listen server starts with one slot.

## Module boundary

### Game (`main/gamex86.dll`)

`GetGameAPI` (0x310d7d10) writes API version 11 into export slot 0, then copies
0xa7 = 167 import dwords with one `rep movsd` to 0x31212d88. The export
record it fills at 0x31212ce0 is 0xa4 bytes: the version, thirty-four function
slots of which three stay null, a profiling pointer and the entity array's
bookkeeping. The entity is 852 bytes (the immediate at 0x310d7e6f) and the
client 2,272; every `LocateGameData` site pushes both (0x310d697d, 0x310d7497,
0x310ec07c). There are no bot entry points: those two slots are null, and 1.00
has no bot library.

**All 167 imports are served** (`code/server/server_mohaa_game.hpp`, with the
world in `server_mohaa_world.hpp`); the log line at load says so. An import
the engine did not serve would drop by name and number when called rather
than fault on a null.

The level archive's exports take no client-game state, unlike the later
games': `ArchivePersistant( name, loading )` 0x310d6b30, `WriteLevel( file,
autosave )` 0x310d7720, `ReadLevel( file )` 0x310d7750, `LevelArchiveValid(
file )` 0x310d7770, each reading its arguments off the stack.
`ArchiveInteger`, `ArchiveFloat`, `ArchiveString` and `ArchiveSvsTime` work on
the current archiver (0x3121370c); reading a string copies it whole into the
caller's buffer. While the game writes or reads a level it calls import 156
(`ArchiveLevel`, 0x310d7533) for the engine's own part.

The HUD draw imports are 158 to 165 (called from 0x31050335 and beside it):
they write the local client's HUD element array and call the client game's
exports 12 and 13 to register the element's shader or font.

The game's cheat commands (`give`, `god`, `noclip`, `wuss`, `face`, `tele`)
need `thereisnomonkey` set: on each client command the game registers it (0,
0x310cfe6c) and, while it is 0, sets `cheats` to 0 (0x310cfe92..0x310cfea9);
a cheat then needs `cheats`, the serverinfo latch the game registers at 0
(0x310e53fb), and otherwise answers "You must run the server with '+set
cheats 1' to enable this command." (0x310cfec4). The engine keeps `cheats`
as 2015's does **[O]** (Cvar_Init, SV_Init, SV_Map_f): registered at 1, latched
and sent to clients, and set as each map starts -- on for a one-player server
or a `devmap`, off for any other map, and left alone in developer mode
(`code/server/sv_init.cpp`, `sv_ccmds.cpp`). So `+set thereisnomonkey 1`
opens a single-player game's cheats, as it did for 1.00's players.
**Observed:** with `developer 1` and `thereisnomonkey 1` alone, `cheats` reads
1 and `give all` hands `m1l1`'s player every weapon.

Command strings the game sends the engine: `gamemap "%s"\n` (0x311d5640) at a
level's exit, `restart\n` (0x311cf5a0) after a failed mission, and, after a
death, `loadlastgame\n` (0x311de1d0) when `g_lastsave` (registered archived,
0x310e5df3) is set and `restart\n` otherwise (0x311143ec..0x3111440d). The game
sets `g_lastsave` itself to the name it is writing (0x310d7102).

### Client game (`main/cgamex86.dll`)

`GetCGameAPI` (0x300237e4) takes no argument and fills 30 export slots at
0x30265508, the last null (the profiling structure). Export 0 takes the import
table and copies 0xa8 = 168 dwords (0x30023662). **149 of the 168 imports are
served** (`code/client/cl_mohaa_cgame.cpp`); an unserved slot that is called
drops the game with its number and name, and none has been called in any probe.
**Measured:** the other 19 are the slots no instruction in the module reads
(`UNREFERENCED`, with no reader, in `code/compat/mohaa_cgame_imports.def`,
which counts every slot's readers), so every import 1.00's client game can
call is served.

The records it shares with the engine are measured in it:
a 596-byte entity state (the snapshot stride, 0x30030c2b), a 660-byte player
state (0xa5 dwords copied, 0x30031289), a 614,096-byte snapshot (two lie
0x95ed0 apart, 0x30033577), a 412-byte render entity (0x19c bytes cleared,
0x3002063f and nine more sites), a 188-byte view (0x30047b1d), a 10,324-byte
graphics configuration whose video width is at 10,280 (0x3002351c), a
42,948-byte game state whose text is 32,000 bytes (0x302a5b20, 0x30023504) --
not OpenMoHAA's 41,952 -- 256 HUD draw elements of 2,228 bytes and a 312-byte
view-model animation state. `tests/mohaa_module_abi_tests.cpp` holds the
tables dense and in slot order and the records to these offsets.

`R_DrawString` (import 96) takes a sixth argument, an integer
`virtualScreen`: 0x3001f0fd pushes the HUD element's flag at +0x6c and every
other site pushes 0. It is not OpenMoHAA's `float *` scale.

Every frame the client game's `Draw2D` (export 5, 0x3001fda9) runs a line
through import 16, `Cmd_Execute`, at `EXEC_NOW` (0x3001fc1e):
`ui_removehud hud_stopwatch` while no stopwatch runs, `ui_addhud
hud_stopwatch` while one does. The engine runs such a line alone, leaving a
pending `wait` and the rest of the buffer as they are, and echoes nothing
**[O]** (`Cbuf_ExecuteText`; `code/qcommon/cmd.c`). The echo fnTech3 inherits
printed that line under `developer` every frame, 9,950 times in a 155-second
`m5l3` probe. **Observed:** a `developer 1` probe of `m5l3` logs none.

The client game has no `SetUserCmdValue` import to hand the engine a mouse
scale. 2015's `CL_MouseMove` asks it for one on every move instead **[O]**:
export 9, `SensitivityScale`, returns the `fov_y / 75` its view set last frame
(0x30047619, 0x30048909), and a menu holding the mouse makes it 1. The engine
does the same (`CL_MOHAAMouseMove`); left at nought, the scale had turned
every mouse move into none. The move itself is fnTech3's (invariant 11): the
shared transforms -- `cl_mouseAccelStyle`, 2 by default, its acceleration and
`m_filter` -- run before the scale. A move also sets the "any key" bit, 14,
which 1.00's game tests before the move bytes when it decides a player is idle
(`g_inactiveKick`, gamex86.dll 0x3111769b) **[O]**. **Observed:** in `m5l3`,
on a private desktop, 400 counts of engine-queued motion (`in_synthesize`) at
sensitivity 2 turned the player from -180 to 168.14 degrees, 11.86 being
0.022 x 400 x 2 x 0.674, and 200 counts up pitched it to -5.93; the game's
`coord` and the client game's `viewpos` agreed.

### Level start

A single-player spawn holds the game's frames until the local client's first
snapshot; then the game's clock is set, the server-time fixups run,
`ServerSpawned` (export 19) is called and the level saves itself, except the
credits. Multiplayer starts at the spawn's end and a dedicated server at its
first `ClientBegin` **[O]** (SV_ServerLoaded). Without it level scripts never
pass `level waittill spawn`.

### Sounds a script waits on

A script's `playsound <alias> wait`, an actor's dialogue and `waittill
sounddone` wait on a sound the local client plays. 527 such waits are in 35 of
the shipped level scripts (116 in `training`, 74 in `m3l1a`, 10 in the first
briefing); none of that flow advances without the loop below.

- The game marks the sound with 0x400 on its entity number **[O]**
  (`S_FLAG_DO_CALLBACK`); the snapshot's start record carries the entity number
  in eleven bits (10 for a stop), so the flag reaches the client game, which
  passes it to `S_StartSound` unchanged.
- The client, starting such a sound, tells the local server's game which of
  its channels took it: export 34, `SoundCallback( entity, channel, name )`.
  Retail's (0x310d78a0) drops through `gi.Error` for an entity number outside
  `max_entities` (0x310d78c4) and, for an entity whose object pointer -- the
  first of the game's own members, at +760 (0x310d78ee) -- is null, raises a
  script error with `_CxxThrowException` (0x310d79e1) and no handler: the
  exception would leave the module. The engine therefore passes neither on.
- The game then asks each frame whether that channel still plays the sound,
  import 135 `S_IsSoundPlaying( channel, name )` (two sites), and wakes the
  waiting thread once it does not **[O]** (Entity::EventSoundDone). Imports 133
  and 134, `SoundLength` and `SoundAmplitudes`, have no caller in 1.00.

In fnTech3 a channel is a record the client keeps
(`code/client/cl_mohaa_sound.cpp`): the sound plays for its file's length at
its pitch, in real time as the audio does, from when it started. The length
is read from the file through the codecs, not from what the backend plays, so
a level's timing is the same with the audio muted, unfocused or off. A later
sound on the same entity channel takes the channel over and a stop ends it,
as a retail voice would; the automatic channel takes nothing over. A file the
codecs cannot read still gets a channel, which has already ended, so a wait on
a missing sound ends at once rather than never.

### Sound aliases

Both modules look a sound up in one global alias list the engine keeps
(`code/mohaa/qcommon/alias.c`, behind `MOHAA_GlobalAlias*`). The game's
`Entity::Sound` and `LoopSound` (gamex86.dll 0x310ccd12, 0x310cbec3) ask
`GetRandomAlias` (0x310cc0e0), which tries the entity's model list (import
120) and then the global list (import 126 `GlobalAlias_FindRandom`); they
try import 126 once more themselves (0x310ccd6a, 0x310cbf18). The client game
asks with import 121. Neither module empties the list: the game's imports 122
and 128 and the client game's 124 have no caller.

It is filled by parsing `ubersound/ubersound.scr` (1,094 `aliascache` and 60
`alias` lines) and `ubersound/uberdialog.scr` (69 and 5,220):

- Each module's `aliascache` and `alias` handlers (gamex86.dll 0x310449a0 and
  0x31044d00, cgamex86.dll 0x30016d9d and 0x300170a6) rebuild the parameters
  from the third argument on, quoting a subtitle in `alias`, and keep the
  line only if `bLoadForMap` passes (0x31044c50, 0x30016cc2). A level whose
  name starts with `test` takes every line (`strncmp`); otherwise a token of
  the line's `maps "..."` must be a prefix of the level's name, compared
  without case for the token's length (`_strnicmp`, 0x311803f0, 0x30063230).
  A line without `maps` is refused ("ERROR bLoadForMap: %s alias with empty
  maps specification."); every shipped line has one, and 1.00 has no
  `always`. The game reads the `mapname` cvar as it parses (0x31044c5e), the
  client game its own `maps/<mapname>.bsp` less `maps/` (0x30032674,
  0x30016cdc); both keep the same shipped lines.
- The game adds a kept line to the global list with import 125
  `GlobalAlias_Add` (0x31044bac, 0x31045000), and its `aliascache` also
  caches the file (0x310e2810, `soundindex`). The client game adds it to the
  list of the model the last `settiki` named, or to the global list with
  import 119 (0x30016c5a).
- The game's parse is `RegisterSounds`, export 20 (0x310d7c50), which only the
  engine calls and which tests neither the game type nor `dedicated`; its
  `settiki` is "Not implemented - used to stop script error on dedicated
  server" (0x311c39c8). The client game's is `CG_RegisterSounds`
  (0x30022cb2), which `CG_Init` runs straight after `CM_LoadMap` (import 38;
  0x300235e9, 0x300235f7).

**When.** 2015's engine calls the game's `RegisterSounds` for a multiplayer
game only, after the level has spawned and settled **[O]** (`sv_init.c`,
after the heartbeat); in single player the client game's parse, as it starts,
is the only one. Loading a map empties the list **[O]** (`cm_load.c`
`CM_LoadMap`), which a `restart`, or a `loadgame` of the running map, does not
do. So a level's spawn -- its script up to its first wait, and the three
frames that settle it -- looks every sound up in an empty list, and the game
prints "ERROR: Entity::Sound: %s needs an alias in ubersound.scr or
uberdialog.scr - Please fix." (0x311d4610; `LoopSound`'s, 0x311d4528) for
aliases whose `maps` do cover the level: `snd_pickup` (`maps "m dm moh obj
train"`) as `global/loadout.scr` equips the player, and a vehicle's `start`,
`doorclose`, `on` and `idle`. Retail 1.11 does the same, in two published logs
of its Linux port (not our probes): a dedicated server (icculus.org `mohaa`
list, August 2002, `obj/obj_team3`) prints "PARSING UBERSOUND (SERVER)" after
"Spawning Entities Done", and a single-player `m1l1` ("Medal of Honor Allied
Assault 1.11 linux-i386 Sep 3 2004", VOGONS, 2014-09-01) prints the
`snd_pickup` miss as its spawn ends, before its client game loads and parses.
That 1.00's engine orders these the same way is **[I]**: its executable cannot
be read, and neither 1.00 module says otherwise. fnTech3 keeps the order.
Registering before the spawn would also have the game's `aliascache` put
the 276 to 347 sounds each single-player map's lines keep into the
configstrings, which retail's single player does not send **[I]**.

In fnTech3 `SpawnLevel` empties the list where the map is loaded, after the
model cache's reset and before `SetMap`, except for a `restart` or a
`loadgame` of the running map (`code/server/server_mohaa_game.hpp`,
`server_mohaa_save.hpp`); `SV_MOHAASpawnDone` runs the game's parse for
multiplayer. The list had never been emptied, so each level's aliases answered
for every later level in the process
(`tests/mohaa_alias_registration_tests.py`). A client empties it as well when
its client game loads the map (import 38, `code/client/cl_mohaa_cgame.cpp`),
as OpenMoHAA's `CM_LoadMap` does for a client loading a map other than the
one it holds **[O]**, unless it plays on its own process's server, whose spawn
has already emptied it. It empties it for the map it already holds too, which
changes nothing: `CG_Init`, the import's only caller, registers the map's
aliases straight after. A model's
`Alias_Clear`, import 122, is served by the model list's dump, as 2015's
engine serves it **[O]** (`PF_Alias_Clear`); no 1.00 module calls it.

**Observed** (`developer 1`, windowed, muted, 2026-09-25):

- A fresh `m1l3a` misses six in its spawn, all before the client game's parse:
  `jeep_snd_doorclose`, `jeep_snd_start`, `snd_pickup`, `jeep_snd_on` and
  `jeep_snd_idle` twice. A `restart` misses none of them: the same lookups at
  the same point find what the client game registered, so the names, the
  `maps` rule and the list are right, and only the order leaves them out.
- Before the change a `gamemap m1l3b` after it missed nothing. That included
  the jeep's `start` and `on`, whose `maps "m1l2b m1l3a "` leave `m1l3b` out:
  a fresh `m1l3b` misses both in its spawn, and `start` again after its parse. Its
  client game's parse printed about 1,900 "DUPLICATE ALIASES" lines against
  the list `m1l3a` left. After the change the same `gamemap` misses exactly
  what a fresh `m1l3b` does, with no duplicate lines, and a `loadlastgame` of
  its autosave misses nothing.
- A client of a dedicated server that moves from `DM/mohdm1` to
  `obj/obj_team1`, whose lines keep 23 aliases more: before the client's
  change its client game's second parse printed 2,603 "DUPLICATE ALIASES"
  lines against the list the first map left; after it, none.
- In the soak logs of 16 single-player maps, every miss of an alias whose
  `maps` covers the map is in the spawn. After the parse the only misses are
  aliases the data does not define for that map: `pickup_explosives` (only
  `pickup_explosives1` to `3` exist, for `m1l2a m1l2b m3l3 m4l3 training`),
  `m3l2_radio_pickup` (`m3l2 dm moh obj train`), the jeep's `start` on
  `m1l3b`, `heavy_drop` (other maps) and `gate_large_open` (none).

## Wire

Protocol 6's framing and records are OpenMoHAA's protocol-6 tables **[O]**
(`code/qcommon/mohaa_msg.cpp`, `code/compat/mohaa_message_codec.hpp`), acting
directly on the 1.00 records above, so nothing is converted at the wire:

- Huffman-coded messages; a client qport; the reliable-command XOR; long
  fragment starts; messages to 49,152 bytes; 256 reliable commands stored
  (OpenMoHAA rings 512; the difference only changes when a client with that
  many unacknowledged is dropped).
- An out-of-band direction byte after the marker: 1 from a server, 2 from a
  client; the connect text starts one byte later.
- Three server opcodes past Quake III's snapshot -- a centre print (8), a
  located print (9) and a client-game message (10), which only the client
  game can read to its end -- and the end byte 11.
- A snapshot carries the server's time residual, the player state, the
  entities with 10-bit numbers, and up to 64 sounds; each move carries the
  client's eye position and a keyed command.

**The handshake** is 2015's **[O]**: a `getchallenge`, answered by
`challengeResponse <challenge>` (fnTech3's server adds the client's nonce and
protocol 6, as ioquake3's does), then a Huffman-coded `connect "<userinfo>"`
with protocol 6, the qport and the challenge, and `connectResponse
<challenge>`. fnTech3's client had read protocol 6 in a reply as an old
Quake III protocol and connected with 68, which the server refused; it now
takes either form of reply and connects with 6. **LAN listing:** `localservers`
broadcasts `getinfo` with the direction byte, and each answer fills the Join
LAN Game list directly, timed from the broadcast, as OpenMoHAA's client does
**[O]**; the list names the game type as 1.00's game module does
(`Free-For-All`, `Team-Match`, `Round-Based-Match`, `Objective-Match`, gamex86
0x1d6c80..0x1d6da4). Over UDP with both processes bound to the loopback
address, the list found a dedicated `DM/mohdm1` server and the client joined
it from the list and spawned. Only fnTech3's own client and server have
exchanged packets. **Demos** are
Quake III's container under `demos/<name>.dm_6`: the gamestate the recorder
writes carries the configstrings, the baselines the server named, as deltas
from 2015's null entity, and the end byte 11; the messages after it are copied
as received. A 3-second `m1l1` recording plays back with its HUD, and
`timedemo 1` plays it to its end in 60 frames. Re-recording during playback
keeps only the gamestate, as for Alice. No retail demo exists to play.

**Pure servers.** `sv_pure` is registered off, as 2015's server registers it
**[O]** (OpenMoHAA's `sv_init.c`: "sv_pure is disabled by default in
mohaa"). With it on, the client proves its general pak references alone:
`cp <checksum>... <mixed>`, the last being the checksum feed with every
reference and their count folded in, and no server id, no module checksum and
no `@` delimiter, as OpenMoHAA's `CL_SendPureChecksums` and `SV_VerifyPaks_f`
read the engine **[O]**. 1.00's modules are loose files in `main/`, not in a
pak, so there is no module to prove. The server takes the same checks as
Quake III's (every reference loaded by the server, none twice, the mixed
value right) and drops an unpure client (`COM_PURE_PROOF_MOHAA_REFERENCES`,
`tests/pure_profile_contract_tests.py`). The profile had no proof at all, so
a pure server dropped every client. Loose files keep loading under a pure
server, as fnTech3's game directories are always searched; 1.00 keeps 2,630
sounds and movies loose in `main/sound` and `main/video`, and OpenMoHAA's
pure client admits loose `.wav`, `.mp3` and `.ogg` files for that reason
**[O]**. **Observed:** on a dedicated `DM/mohdm1` free-for-all with
`sv_pure 1`, two clients each sent a `cp` of five references and played on,
listed by `status` 30 seconds later.

**Downloads.** A client downloads an archive the server references and it
lacks, over Quake III's download commands, which OpenMoHAA's server answers
**[O]**. **Observed:** a dedicated `DM/mohdm1` server with `sv_pure 1`,
`sv_allowDownload 1` and `sv_dlRate 500` loaded the map from an extra archive
in its home, `main/zz-fntech3-download-probe.pk3`, holding a copy of the
retail `maps/DM/mohdm1.bsp` (1,384,444 bytes; built for the probe, never
committed, deleted after it). A client with `cl_allowDownload 1` asked for
it, received 1,353 blocks in about six seconds, wrote a file whose SHA-256
matched the server's (`cc29f336...`), sent a `cp` of six references including
it and entered the battle. Over HTTP -- `sv_dlURL` naming a loopback web
server that served the server's home, and the client on `cl_allowDownload 5`
so that the UDP path could not stand in -- the client fetched the same archive
with cURL in about three seconds, its SHA-256 matching, reconnected, passed
the pure check and entered the battle; the server wrote no UDP block. No
retail client or server has taken part.

## Client

**Presentation (AGENTS.md invariant 11).** 1.00 has no menu module: its menus,
loading screens, scoreboard and HUD are the engine's, built from the 179 menus
of `ui/*.urc` (`code/client/cl_mohaa_ui.cpp`, following OpenMoHAA's uilib
**[O]**). A menu with `virtualres 1` covers the screen as retail's does. The
HUD, crosshair and message boxes scale with the screen's height from 480 lines
times `ui_hudScale`, where retail draws them at their authored pixel size;
`ui_hudScale 0` keeps retail's sizes. Sound uses fnTech3's OpenAL pipeline:
2015's alias volumes, minimum and maximum distances and pitch feed the voices'
reference and range scales in the FnQ3 distance model, with its occlusion,
EFX environments and zones; every official map ships a zone file, and a
static light file for the renderers' static map lights
([sidecar record](2026-09-25-mohaa-map-sidecars.md)). A one-shot keeps 2015's own channel, which the
engine's channels fold together (voice, dialogue and secondary dialogue are
all the engine's voice): an entity's sound on any channel but the automatic
one ends the sound it had there, as 2015's channel picker takes that channel
over **[O]** (S_OPENAL_PickChannelBase), and the client game's stop ends it;
sounds of no entity stack. The legacy mixer keeps no such channel. Sound
handle 0 is the never-loaded `***DEFAULT***` placeholder **[O]** (OpenMoHAA
snd_dma_new.cpp S_Init), as in Ritual's engines, so a sound handed 0 plays
nothing; 1.00 has no `sound/feedback/hit.wav` to stand in. A room the
level names -- the player state's
reverb type and level, which the game's `TriggerReverb` entities set
(gamex86 0x1e8020) and the client game passes through import 65 -- is one of
the 26 EAX environments; it replaces the engine's own room with the nearest
EFX preset at the send the Miles software provider gave it, the same volumes
as Alice's (OpenMoHAA's table equals `alice.exe`'s at 0x0051f944), unless
`s_reverb` is 0. No shipped map carries a reverb trigger. Music is the soundtrack manager Alice and F.A.K.K.2 share
(its moods, flags and 0.063-second start offset) plus the triggered music
(`tmstart`, `tmstartloop`, `tmstop`), streamed and mixed at 44.1 kHz; the
backend applies `s_musicVolume`, 1.00's only music volume. A few sounds are
IMA ADPCM WAV files, decoded for this profile alone; a block's data is whole
four-byte turns of each channel, and a short last block ends at the frames
every channel holds, the second channel of a stereo file holding up to four
bytes fewer than the first.

**Observed output.** OpenAL Soft 1.25.2's wave writer, loaded for one probe
from a `.tmp` folder on the `PATH` (`ALSOFT_CONF` selecting `drivers = wave`),
recorded `m1l1`'s opening mix as the engine sent it: 48 kHz stereo, silent
through the load, then between -13 and -27 dBFS a half second with the
limiter on. Cross-correlated against the retail files, the triggered cue
`sound/music/mus_03c_suspense.mp3` (decoded with the engine's minimp3) starts
0.8 s after the level's sound does (normalised correlation 0.21 against a
median of 0.004), and the captain's `dfr_M1L1_101j` ("Okay squad listen
up...") about 7 s after that (0.062, over twice the next-best peak), in time
with its subtitle. The engine mutes itself while its window is unfocused
(`s_muteWhenUnfocused` 1), so a background probe records silence unless that
is cleared. This shows what the mix holds, not how it sounds.

- **Menus.** Labels with the player's stats and bars (straight, staggered,
  circular, needle, rotator, compass with its lag, spinners), buttons, check
  boxes, sliders, fields, lists, list boxes, pulldowns, the bind list
  (`ui/BIND.SCR`), the saved-game list, the LAN game list (Server Name, Map,
  Players, GameType and Ping over 40, 15, 16.5, 22 and 6.5 per cent of its
  width less a scroll bar's sixteen, the widths OpenMoHAA records as 1.00's
  **[O]**) and the `dialog` box; the push and
  show stacks, fullscreen menus pausing a local game, menus `showmenu` or the
  client game raise over them -- which clearing the stacks leaves up, as
  OpenMoHAA's `ClearMenus` does **[O]**, so a briefing's first slide survives
  its loading screen, and which go with their level when the next load or
  the main menu begins -- and the console
  commands the menus use (`pushmenu`, `pushmenu_sp`, `pushmenu_dm`,
  `forcemenu`, `popmenu`, `showmenu`, `hidemenu`, `widgetcommand`,
  `globalwidgetcommand`, `ui_hud`, `ui_addhud`, `ui_removehud`,
  `ui_resetcvars`, `ui_checkrestart`, `finishloadingscreen`, `dialog`,
  `saveshot`, and `listmenus`, which with `visible` lists only the menus up,
  each with how it is up and how many of its widgets show). A `rendermodel`
  label draws the model its cvar names in a
  30-degree view of no world, placed and posed as OpenMoHAA's `DrawModel`
  and `CL_Draw3DModel` place it **[O]**: the team menus show the Allied and
  Axis player models in their selection idles. The pickers the menus open
  are served as small titled lists: `dmmapselect` (the Start New Server
  menu's map), `maplist` (a developer's map runner), `playermodel` (the
  multiplayer options' model choice, the Axis models being those named
  german, axis, it or sc), with `ui_getplayermodel`, `ui_applyplayermodel`
  and `ui_startdmmap`, which starts a listen server from the menu's settings
  (the client cannot become `fntech3.ded`, and says so when Dedicated is
  ticked). The briefing room's `difficultyeasy`, `difficultymedium` and
  `difficultyhard` are 1.00's own, absent from OpenMoHAA; they are taken to set
  `skill` 0 to 2 **[I]**. `launchgamespy` says the GameSpy list is gone.
  The developer menus' file pickers are served the same way, since neither
  module registers them and 2015's engine does **[O]**: `viewspawnlist`
  (`animate2`'s model viewer) and `lod_spawnlist` (`LODTool`) list the TIKI
  files under `models/` and run the game's `viewspawn` or `lod_spawn` on the
  one chosen; `soundpicker` (`ZOUND`) lists the sounds under `sound/` from
  where the last one picked lies, and sets `ui_pickedsound` to the one chosen
  and plays it. Each lists its folder's parent and subfolders and stays
  under its root (`tests/mohaa_developer_picker_tests.py`). The renderer's
  own developer tools that `decals` and `LODTool` call -- `dclc_*`,
  `lod_edit_*` and `lod_save` -- are served by the engine (see
  [Decals and level of detail](#decals-and-level-of-detail)).
  An `invmodelhand` label draws the item the player holds in that active
  item slot (2 to 5), named by its configstring and posed by its
  `global/inventory.txt` record: its model, angles, offset, scale and bob or
  spin. The labels sit in `hud_items`, the items bar, which is up while
  `ui_itemsbar` is set and `ui_weaponsbar` is 3, but for while the weapons bar
  shows **[O]** (UI_Update). Retail's menus set neither -- the advanced
  options' box only switches `ui_weaponsbar` between 0 and 1 -- so the bar
  never shows by default.
- **Loading screens.** A load shows the map's own loading menu, else
  `loading_default`, and its bar follows the map's load record,
  `maps/<map>.min` (`code/compat/mohaa_load_record.hpp`): version 3, the
  resource count and the bytes their names take, then each resource's name,
  its kind in the first letter, and the time it took. Retail's reader refuses
  a record whose version differs or whose names do not add up to the byte
  count **[O]** (UI_ArchiveLoadMapinfo, UI_ParseLoadMapinfo); all 54 retail
  records pass, from 53 resources (`briefing1`) to 942 (`m4l3`). As the load
  reaches each resource the bar advances by its time, out of the time of all
  the resources this load will reach **[O]** (UI_RegisterLoadResource): the
  kinds the engine reports -- models and their parts, skeletal animations,
  the game's entities, sounds and the two sound scripts -- less those already
  cached. The renderers' images, the fence masks and the engine's own steps
  are not reported here, so they are left out and the bar fills. Without a
  record the loading screen shows its flasher. 2015's engine keeps its UI up
  through a listen server's spawn **[O]** (CL_MapLoading and CL_FlushMemory
  flush only the client game); this engine shuts the client down for it, so
  the record's hook outlives the UI, the game's resources count while it is
  down, and the loading screen the restarted UI shows takes the bar up from
  there. **Observed** on `m1l1`: 2,987 reports during the spawn and 834 after
  it, every counted resource the record names reached, the bar at 1 by the
  Continue button, and a frame taken mid-load (the engine's `screenshot`)
  shows it red to about three fifths in the retail menu's place.
- **HUD.** Health, the ammunition menu of the weapon in hand
  (`hud_ammo_<weapon>`), the compass, the weapons bar with its timing and the
  crosshair, by UI_Update's rules **[O]**: stats 2, 3, 9 and 16, the weapon in
  active item 1, intermission at `pm_flags` bit 8 and no-HUD at bit 15. The
  client game draws the single-player objectives (`+scores`) itself, in
  screen pixels at a fixed place: its backdrop from (25, 125), 400 wide,
  "Mission Objectives:" at (30, 130) (cgamex86.dll 0x3001f3ee..0x3001f4c4),
  just under retail's 128-pixel compass. A compass the HUD scale makes larger
  would cover that heading, so it fades as the overlay comes up and is gone
  once it is half up -- its ring's shader ignores the colour's alpha -- and a
  compass at retail's size is left alone.
- **Fades and letterbox.** 2015's engine, not the client game, draws a
  level's `fadein` and `fadeout` and a cinematic's `letterbox` **[O]**
  (View3D::Draw2D): the player state's `blend` colour over the screen, added
  rather than laid over when stat 14 is set, then black bars at the top and
  bottom, each stat 15's share of 32,767 of the screen's height -- the game
  writes the letterbox there (gamex86.dll 0x3111f5bc..0x3111f5cd) and the fade
  into `blend` at +568 (0x3111c4b1..0x3111c5bf). They go over the view and
  under the client game's 2D, the prints and the HUD, which a letterbox hides.
  `m4l0` fades in from black over its first two seconds; no shipped script
  raises a letterbox. A map whose loading menu waits on Continue holds the
  game paused until it is pressed, so its opening fade is seen; one on
  `loading_default` starts at once. Both draw with the untextured picture
  registered once a renderer start (`cls.whiteShader`). They had registered it
  afresh every frame they drew, and the renderer, which answers a repeated
  name with the shader it already has, had created a new image first each
  time: a three-minute soak of `m3l1a`, whose landing flashes the screen,
  used up the renderer's 2,048 image slots and dropped the game
  (`R_CreateImage: MAX_DRAWIMAGES hit`). **Observed** after the change: the
  same soak ran its three minutes, its image count going from 350 twenty
  seconds in to 359 at the end (`tests/mohaa_white_shader_tests.py`).
- **Subtitles.** With the advanced options' "subtitles" box (`g_subtitle`)
  set, the game and the client game copy a dialogue alias's `subtitle` into
  `subtitle0`..`3` in turn, counting in `curSubtitle`, when the speaker is
  within the alias's reach (every speaker at `g_subtitle 2`; gamex86.dll
  0x310cce49..0x310ccf25); 1,227 of `uberdialog.scr`'s aliases carry one. The
  engine's view draws them **[O]** (View3D::DrawSubtitleOverlay): white over a
  black shadow from ten lines above the foot of the screen, broken at four
  fifths of its width, each for 2.5 seconds a 68 characters and 1.5 more,
  fading over 0.75 seconds at `subAlpha` (0.5), then clears the cvar. They
  scale with the HUD. Both modules read an alias record's subtitle at +0xd4,
  straight after `streamed` (gamex86.dll 0x310cce5e, cgamex86.dll
  0x30015f5a), where OpenMoHAA's record has the later games' `forcesubtitle`
  flag; the record keeps 1.00's layout, so the modules read the text and not
  the flag.
- **Key names for the game.** A listen server's game reads the local
  player's bindings through its imports 153 to 155: a level's `getboundkey1
  "+scores"` names the key in its hint ("Press ( TAB ) to see your
  objectives."). The retail handler (gamex86.dll 0x310515b0) names the first
  key only when its number is not 0 and otherwise gives an empty string; an
  unbound command's keys are -1, which reads back as "Not Bound" **[O]**. A
  dedicated server has no bindings and answers 0.
- **Messages.** A printed line's first byte routes it: 1 a game message in
  gold and 3 a bold one in white (10 s) to the box under the compass; 2 chat
  (8 s) and 4 and 5 deaths in red and green (6 s) to the box at the top.
  `con_notifytime` defaults to 0, and zero hides every notify line.
- **Chat.** 1.00's game reads one chat command, `dmmessage <target>
  <words>`, 0 to all, -1 to the team, else to a client; it has no `say`,
  `say_team` or `tell` (gamex86.dll's command names). The client game
  registers `messagemode`, `say`, `sayteam` and their kin itself, sends a
  `dmmessage` for one given words, and otherwise opens the chat through
  import 159: 300 (the kind last opened), 100 (all), 200 (team), a client's
  number from 1 to 64, or minus one of them for a single message
  (0x30046490..0x300466db); `default.cfg` binds `y` to `say` and `t` to
  `sayteam`. fnTech3 had served import 159 as nothing and registered Quake
  III's `messagemode` commands, whose line went out as `say`, so no chat
  could be typed. The engine now registers no chat command for this
  profile, and import 159 opens its chat line, the same kind again or 300
  closing it, whose line goes out as that `dmmessage`; 2015's engine opened
  a chat window with a history of its own **[O]** (OpenMoHAA `cl_ui.cpp`
  `UI_ToggleDMConsole`), where the history here is the box at the top; the
  line sits where that window's one-message form did, two thirds of the way
  down **[O]** (`getQuickMessageDMConsoleRectangle`), clear of the compass
  (`tests/mohaa_chat_tests.py`). **Observed** on `DM/mohdm1`: `sayteam`
  opens a `say_team:` line and again closes it, `say` opens `say:`,
  `messagemode` closes it, and a `dmmessage 0` from the client comes back
  as a chat line in the top box.
- **Lights for shadows.** Import 54 asks which lights reach a point; its
  one caller (0x30029b98) casts a character's foot shadows from each under
  the options' "Complex" shadows (`cg_shadows 2`, which the client game
  registers as 0 and `high.cfg` sets), falling back to a single shadow when
  none do. fnTech3 had answered none. The collision model now keeps the
  map's sphere lights and each leaf's list of them, the sun first where the
  leaf sees the sky (`code/qcommon/cm_mohaa_lights.c`), and answers as
  OpenMoHAA's renderer does **[O]** (`R_GatherLightSources`): in a leaf with
  lights, the sun if a line toward it reaches the sky, then the leaf's lights
  whose area the last view could see; elsewhere every light; each counted
  when its intensity times 7,500 over its squared distance comes to 5, the
  strongest first, at most eight, given as its origin and that measure over
  200 times its colour, no component past 1; the sun 16,384 units out with
  its colour, doubled by 2015's overbright multiplier, over 510
  (`tests/mohaa_light_sources_tests.py`): the multiplier is 2 as
  `r_mapOverBrightBits` 1 less `r_overBrightBits` 0, the defaults OpenMoHAA
  registers **[O]**, which no 1.00 configuration changes. **Observed:**
  in `m3l1a`'s landing craft, looking down, a soldier's boot casts a long,
  faint shadow across the deck with `cg_shadows 2` that `cg_shadows 1` does
  not draw; retail's has not been compared.
- **Renderer.** The engine's own white fills 2D boxes, as 2015's "white" is
  an additive sprite. Ritual's `alphaGen oneMinusVertex` has no back-end case
  in Ritual's engine (alice.exe 0x00493cc4); for this profile it keeps the
  colour pass's alpha, which the HUD art depends on. TIKI models are posed and
  skinned by the engine and drawn by all three renderers. An actor's model name
  leads with the weapon, head and skin it wears
  (`weapon|m1 garand|headmodel|head4|headskin|us_j|models/human/...tik`) and
  can pass the renderer's 63 characters; such a model is known to the renderer
  by the end of its name and a hash of all of it, so customised actors draw.
  The maps are in the [map record](2026-09-24-mohaa-maps.md).
- **Lens flares.** fnTech3's Alice lens flares, adapted to 2015's data
  (invariant 11): the same `begin <section>` ... `end` records, read from
  `global/lensflaredefs.txt` **[O]** (lens_flare::Init). A level's sun is a
  direction: its worldspawn's `sundirection` angles, or `sunflaredirection`
  for the flare alone, and the section `sunflarename` names -- "sun" when a
  level with a sun (`suncolor`, `sunlight` or `sundirection`) names none,
  none for "none" **[O]** (R_Sphere_InitLights). Seventeen of the 54 maps
  flare, all from the `sun` section (among them `m2l2a`, `m3l2`, `m5l3` and
  `mohdm1`); the rest name "none" or have no sun. The flare sits 128 units along the direction and is seen
  when a solid trace 16,384 units along it ends on sky; it draws with the sun
  just off the screen's edge, and when lost fades out with the section's
  full-screen blend over its `fullfade` **[O]** (sun_flare_class::SunTry). The
  `sun` section's blend is an additive `$whiteimage` at `fullscale .6`, which
  its shader file calls a "Nasty screen overlay": looking straight at the sun
  washes the view out, as the data intends. A dynamic light flares from the
  `dlight` section with its corona for a lens-flare or additive type **[O]**
  (R_DrawLensFlares), an entity for the lens-flare render effects; no
  retail data asks for either but `models/items/camera.tik`'s view lens
  flare. **Observed** on `m3l2` in the GL, Vulkan and RTX renderers: the
  flare with its rays and ring drawn from `flare_main.tga` 35 degrees off the
  sun, and in GL the washed-out view facing it and none facing away. The
  three renderers' copies are held to one text (`tests/mohaa_sun_flare_tests.py`,
  `tests/alice_renderer_parity_tests.py`). `m3l2`'s worldspawn also carries
  an Alice-style `sunflare` point, which 2015's renderer does not read **[O]**.
- **Swipes.** The client game's swipe imports (103 to 105) draw a TIKI
  `swipe` or `trail` as a strip of scene polygons between its successive tag
  pairs, colour and alpha fading over the swipe's life **[O]**
  (RB_DrawSwipeSurface), so every renderer takes them
  (`tests/mohaa_swipe_tests.py`). Retail 1.00 names one swipe, in
  `models/fx/fx_grenade_explosion.tik`, which nothing uses and whose debris
  model and shader do not ship; no swipe has been seen.
- **Alpha-tested second bundles (Vulkan family).** 2015's world shaders
  put the lightmap in a `nextBundle`, and its foliage cards and fences
  alpha-test the texture in that same stage (`map ... alphaFunc GE128
  nextBundle map $lightmap`); 1.00's scripts hold 225 such stages, where
  Alice's and F.A.K.K.2's hold none. The Vulkan and RTX renderers draw such a
  stage with a multitexture pipeline, and those were compiled without the
  alpha test (`shaders/compile.bat` and RTX's `build_shaders.py` built the
  USE_TX fragment variants without USE_ATEST), so the pipeline's alpha-test
  specialization had nothing to act on and each card drew its whole quad: in
  `m3l2` the far-plane clear showed through the tree-line cards
  (`textures/misc_outside/treeline_cntr_shadowed_z`) where the sky belonged,
  and the gaps between the hedges' leaves came out near-black. The
  multitexture variants of both renderers are now compiled with USE_ATEST;
  `gen_frag.tmpl` already tested the first bundle's alpha after combining
  the bundles, as GL's multitexture path tests the combined fragment, and a
  pipeline without alpha-test bits specializes the test away, so no other
  stage changes (`tests/multitexture_alpha_test_tests.py`). Every other
  embedded module still reproduces byte for byte from its source with the
  installed Vulkan SDK 1.4.313.1 glslangValidator. A first repair split each
  such stage into a texture pass and a lightmap filter pass; that fogged the
  texture before the lightmap multiplied it, so every fogged card came out
  darker than GL's, and it is gone. **Observed** (fnTech3 client, `m3l2` at
  (-4000, 1500) facing (-10, 270, 0), mean of the crop (100..1100,
  250..580)): GL (20.4, 24.2, 14.3), Vulkan (20.5, 24.3, 14.3), RTX (20.5,
  24.3, 14.3), against (16.8, 21.1, 10.2) from the split; without the far
  plane's fog, GL (13.2, 17.2, 6.8) and the split (13.3, 17.3, 6.9) had
  already agreed. The per-pixel residual between renderers (mean 4.3 of 255)
  is the same as between two Vulkan runs (4.4), the foliage's animation.
  Earlier readings that put a static-light shadow difference here were
  wrong: the probe home kept `configs/unnamedsoldier.cfg`, so cvars archived
  by earlier probes (`r_vertexLight 1` among them) carried into later runs.
  The probes now delete it before each launch. Finding this took RenderDoc
  1.43 out: it hooks the 32-bit Vulkan client and starts a capture triggered
  through its target control, but the capture aborts.
- **The RTX path tracer.** The 32-bit client's driver offers no ray-tracing
  pipeline or ray queries, so `rtx_rt_mode 1` rasterises; the path tracer
  (`rtx_rt_mode 2`) traces with its compute backend instead. **Observed** on
  `m1l2a`: a world BVH of 274,974 triangles, 105 lights from the map (53
  points and 52 surface triangles), and the whole 1280 by 720 view traced.
  The tracer then read the map's lights only as far as the first empty value
  in its entities, and none of 2015's own light keys, so this night level's
  lamps lit little of the walls the raster renderers show lit. Since
  2026-10-28 it reads all 343, with the spots and model-light rules 2015's
  compiler gave them ([light entities record](2026-10-28-mohaa-rtx-light-entities.md)).

## Decals and level of detail

2015's renderer held two developer tools the retail menus drive: the decal
editor (`ui/decals.URC`, bound to F6 by `development.cfg`) and the
level-of-detail tool (`ui/LODTool.URC`, bound to `l`). `MOHAA.exe` cannot be
read, so what each does is taken from the menus, the retail data, the two
libraries and OpenMoHAA's reconstruction (`renderergl1/tr_marks_permanent.c`,
`tr_model.cpp`, `fgame/lodthing.cpp`), each marked. The engine serves both,
for all three renderers at once (`code/client/cl_mohaa_decals.cpp`,
`code/mohaa/mohaa_render_api.cpp`, `tests/mohaa_permanent_decal_tests.py`).
The decal files themselves, and how a level draws them, are in the
[map record](2026-09-24-mohaa-maps.md).

**The client game's decal cutter.** Exports 16 to 18 are
`PermanentMark`, `PermanentTreadMarkDecal` and `PermanentUpdateTreadMark`
**[O]**. **Measured:** they read fourteen, five and six stack arguments
(0x30026dbe reads `[ebp+8]` to `[ebp+0x3c]`, 0x300275d6 to `[ebp+0x18]`,
0x30028061 to `[ebp+0x1c]`); the permanent mark traces from its origin along
its direction and writes the trace's end and plane normal back through its
first two arguments (0x30026e54..0x30026e85); both marks cut through the
helper at 0x30023b10, which asks the renderer for at most 384 points in 128
fragments, so a caller's arrays hold that many; and the tread record is at
least 152 bytes, both tread exports touching every word from 0 to 0x94 but
the shader at 8, the path export clearing 0x14..0x60 once its time at 4 is
more than 500 ms old (`code/compat/mohaa_cgame_abi.h`'s `TreadMark`).

**Decal lighting.** Every caller of client-game import 52,
`R_GetLightingForDecal`, stores its light in a colour byte, times its own
0..1 colour (`CG_ImpactMark` at 0x300251f1, the permanent mark at 0x300272c4
and 0x300274cf) or as it is (the tread decal at 0x30027bec and 0x30027ea9).
So the light is on a byte's scale, as OpenMoHAA's is **[O]**. fnTech3 had
handed it 0..1, which drew every lit mark -- bullet holes and scorches among
them -- black and would have saved black decals. It is now the renderer
grid's ambient and directed light at the point, its hue kept under 255, and
looked for up to 48 units out from the face when every grid point around
the face lies in a wall; import 53's smoke light is the same over 255, as
OpenMoHAA's is. **Observed:** re-cut on `m2l3`, `eagleglobe` takes
(147, 137, 114) where the retail file holds (147, 138, 115), `motto2`
(57, 46, 43) for (51, 42, 40) and `motto3` (116, 97, 91) for (70, 59, 55):
fnTech3's grid, not 2015's, lights them (invariant 11). A burst into
`DM/mohdm1`'s floor leaves bullet holes drawn with their texture and scorch
around them, lit, under the video options' "Wall Decals" (`cg_marks_add 1`);
the client game registers that cvar as 0 (0x30022643), and only `high.cfg`
sets it, so a default configuration leaves no bullet holes, as retail's did.

**Decals on terrain and brush models.** **Measured:** the helper at
0x30023b10 asks for the world's fragments through import 49 (eight
arguments, 0x30023b76) and then, for each brush model in the decal's bounds,
that model's through import 50 (eleven, 0x30023e56), adding its running point
count to each brush-model fragment's first point (0x30023eaf); a fragment's
third word names what it lies on, the client game drawing one above 0
through import 92, `R_AddTerrainMarkToScene`, and carrying a brush model's
with its entity **[O]**. fnTech3 had cut only the world's faces and curves:
import 49 named every fragment 0 and cut no terrain, which the renderers
build as triangle surfaces that Quake III's cutter passes over, and import
50 returned nothing. Both are now served by one cutter shared by the three
renderers (`code/renderercommon/tr_mohaa_marks_surface.h`,
`tests/mohaa_terrain_marks_tests.py`), written after OpenMoHAA's
`tr_marks.c` **[O]**: the world's faces and curves as Quake III cuts them,
32 units before a decal and 20 behind; a terrain patch's triangles 128 each
way, each fragment named by the patch's number plus one; nothing on static
models or the level's baked decals; a brush model's surfaces in the model's
own space, where the client game keeps them. **Measured,** along the
projection of the nearest decal of the same shader, the 3,371 terrain
fragments of 1.00's decal files reach 128.00 units behind their decals
(three exactly) and 128.00 before (37 exactly; 13 a little further, to
139.1), while the 2,032 on the world's surfaces stay within 32.4 before and,
bar three, 20.5 behind. A 1.00 bullet hole's half-width is 2 units (1.75
on metal), half as much again for a heavy round, give or take a fifth, and
it leaves none at all on grass, gravel, sand, foliage, puddles or grills
(0x3002d331..0x3002d63d). A decal whose squared size -- its half-widths
squared, added -- is no more than `ter_minMarkRadius` squared cuts no
terrain **[O]**, so at that cvar's default of 8, OpenMoHAA's, no bullet hole
marks terrain; no 1.00 file names the cvar, so the rule's presence in 1.00
is inferred **[I]**. **Observed:** on `DM/mohdm5`'s snow, a burst leaves no
holes at the default (35 pixels change about the crosshair, the rest the
view's own motion) and a cluster of them with `ter_minMarkRadius 0` (1,641);
on `DM/mohdm7`, a burst into a rotating door leaves holes that swing open
with it; and the decal editor's re-cut of `DM/mohdm3`, which could find no
terrain before, now cuts 296 terrain fragments covering 438,519 square units
where the retail file holds 312 covering 444,307, on the same patches,
numbered alike (`.tmp/mohaa-decals`).

**The decal editor.** With `dcl_editmode` set (latched; it takes effect
as the next level loads) a level's decal file is read as definitions, which
the renderers then leave unbuilt, and each definition is cut again by the
client game on the first frame, with its own settings; one that cuts nothing
is dropped **[O]**. The decals are drawn with the scene each frame while
`r_drawstaticdecals` is set. The menu's twelve commands are served:
`dclc_make` (a new decal where the view looks, with the `dcl_*` settings),
`dclc_reapply`, `dclc_delete`, `dclc_selectnext`, `dclc_selectprev`,
`dclc_getinfo`, `dclc_shiftup`, `dclc_shiftdown`, `dclc_shiftleft`,
`dclc_shiftright` (`dcl_shiftstep` units along the surface, as the decal
faces out of it), `dclc_randomroll` and `dclc_save` **[O]**; the `dcl_*`
cvars keep OpenMoHAA's defaults **[O]**. A change to the selected decal's
shader, size, rotation, colour or lighting cvar cuts it again at once, a new
shader resetting its radius to 16 and its scales to its picture's size over
32 **[O]**; `dcl_pathmode` lays decals along the player's path with the tread
exports **[O]**. While `dcl_showcurrent` is set, the selected decal's
fragments, the direction it faces and its width and height are outlined over
the view, pulsing. `dclc_save` writes `maps/<name>.dcl` into the home path as
version 1, 1.00's own, with a checksum of 0 and each fragment listed in the
brush model and leaf lists 1.00's renderer reads: the open leafs a unit in
front of its points and their centre. A decal's fragments that lie on one
plane and share an edge are joined into one convex polygon of at most eight
points, as 2015's editor joins them **[O]**; OpenMoHAA's own merge skips a
pair on the same surface where it means one on different surfaces, and tests
convexity against the wrong vectors, and both are set right here. **Observed:**
re-cut on `m2l3`, the 39 decals make 41 fragments, all of four points, where
the retail file holds 42, all of four, shader for shader but one
`jh_noentry1` pair it kept apart (72 fragments before the merge). The editor
differs from 2015's **[O]** in keeping no fragment on a brush model (1.00's
files hold none, and the world build draws none), in keeping the file's
order of decals, and in
drawing the outline as thin polygons over everything. **Observed** on `m2l3` with
`dcl_editmode 1`: the file's 39 decals are cut, none dropped; `dclc_make`
lays a `blastmark` where the view looks, `dcl_rotation 45` turns it and two
`dclc_shiftup`s and a `dclc_shiftright` move it, its outline following;
`dclc_save` writes a version 1 file of 40 decals in 42 fragments (26,388
bytes; the retail file, one decal fewer, is 26,296), whose 3,473 lists are
exactly the map's models and leafs, with nothing left over; and the level,
loaded again without the editor, builds all 42 fragments in 73 leaf places,
the 73 entries its lists hold; under the Vulkan renderer the editor's
decals and outline draw alike (`.tmp/mohaa-decals`).

**Level of detail.** A skeletal mesh with collapse data has a
level-of-detail record beside it, its `.skd` made `.lod` (586 ship). Its
surfaces lose vertices as the model grows small on screen, as OpenMoHAA
reconstructs 2015's renderer **[O]**: the metric is the model's radius over
its distance from the view times 100 over the horizontal field, a
character's radius being 92 units over its scale; `r_lodscale` stretches
it, `r_lodcap` caps it, a depth-hacked view weapon sits at
`r_lodviewmodelcap` instead, and `r_uselod 0` draws every model at its
curve's first cutoff; the cutoff read off the record's five-point curve
keeps the leading vertices whose collapse index reaches it, collapses the
rest through the collapse map, and ends the surface at the first triangle
whose corners meet. **Measured:** the record's constants are the curve's
arithmetic: the engine's `TIKI_CalcLodConsts` makes `barclip.lod`'s saved
ones again from its curve, to within a thousandth. The advanced options'
model detail popup sets `r_lodscale`, `r_lodcap` and `r_lodviewmodelcap`
together, from 0.25/0.25/0.25 at Lowest to 1.1/1.0/1.0 at Highest
(`ui/advancedoptions.urc`), and the detail configs set them too
(`high.cfg` 0.55/0.55/0.65); the renderers had registered `r_lodscale` as a
cheat-protected Quake III setting, so the menu could not set it, and now
register it as this profile's archived player setting, default 1.1
**[O]**. The engine skins each surface at the metric the renderer measured
from the main view (`LodMetric`, `Skin`); a static model, built once into
the world, stays whole. The loader now refuses to read a record past its
96 bytes, takes a 48-byte one (67 of the 586 ship so, without constants) as
its curve, and skips a skeleton whose collapse data points past its
vertices.
The default curve of a mesh with no record takes its last cutoff from one
index past the ten, the header's hit box count, as OpenMoHAA reads it
**[O]**; it is read by name there now. **Observed** on `m2l3`: the frame
holds 4,200 vertices and 3,220 triangles with a model at the tool's far end,
and 17,173 and 22,078 with `r_uselod 0`; and in `m1l1`'s opening the
soldiers by the truck are drawn whole, heads included.

**The level-of-detail tool.** The game's `lod_spawn` sets `lod_tool` and
`lod_tikiname`, stands the player at (-4000, 0, 0) with a field of 80, and
places a turning model at `lod_position` less `lod_zee_val` every frame
(gamex86.dll; OpenMoHAA's LODSlave **[O]**); it switches the world off.
**Inferred [I]** from the menu's comments, as no retail file this tree reads
holds the renderer's side: the sliders are the record of the mesh `lod_mesh`
picks, `lod_LOD` is the metric between the record's two ends the LOD slider
asks for, and the engine sets `lod_position` along the view where the model
measures it, the middle of its height on the view's axis; each `lod_edit_N`
button moves that slider to curve point N; `lod_metric`, `lod_tris` and
`lod_meshname` show the metric the model was last drawn at, the triangles
its mesh drew of its total, and its skeleton; `lod_save 1` writes the record,
constants made again. The sliders span the mesh's largest level-of-detail
index, or its record's largest cutoff when larger: **Measured**, 540 of
1.00's 580 records hold a cutoff past the first positive index, OpenMoHAA's
range **[O]**, which sliders of 0..1 could never have made, while 175 end
exactly on the largest and 29 go past it. **Observed** with
`allied_airborne_soldier`: `lod_meshname` names its body's skeleton, whose
record loads its last cutoff, 779.98, as 0.9308 of 838; at `lod_LOD` 0.393
the engine placed it 293 units out, and it measured 0.391; the body draws
1,730 of 1,730 triangles at the near end and 191 at the far one; a curve
point's button moves the slider to its place; and `lod_save 1` writes a
96-byte `airborne.lod` whose moved cutoff and remade constants read back.

## Savegames

The layout of 1.00's `.ssv` cannot be read, so it is fnTech3's own
(`code/compat/mohaa_savegame.hpp`, `tests/mohaa_savegame_tests.cpp`): the
fields of OpenMoHAA's `savegamestruct_t` **[O]**, little-endian, behind the
version 0x464e4d01 ("FNMH" 1) so neither engine takes the other's file for
its own, then the area portal block. A save is three files under
`save/<folder>/`, the folder being `config` without its extension, and
`unnamedsoldier` as fnTech3 keeps no `config` for this profile **[O]**:

| File | Written by | Contents |
| --- | --- | --- |
| `<name>.sav` | the game (`WriteLevel`) | the level, in the game's own format end to end |
| `<name>.ssv` | the engine | 0x114-byte header: version, save time, level time, comment, map, name, triggered music (file, loop count 0 or 1, frame offset); then the area count and every pair's portal count |
| `<name>.tga` | the client (`saveshot`) | 256 by 256 colour picture of one frame of the view alone |
| `current.spv` | the game (`ArchivePersistant`) | the player's persistent data across a `gamemap` |

The engine's part of the level archive (import 156) holds the local client's
256 HUD elements, the view-model animation and the stopwatch, in OpenMoHAA's
order **[O]**. `savegame [name]`, `autosavegame`, `loadgame <name>`,
`loadlastgame`, `gamemap <map>` and `restart` are served; a fresh
single-player level autosaves as `<map><4 digits>` with the comment "<level> -
Starting", and `savegame quick` is commented "QuickSave - ...". The picture
takes the colour reduction F.A.K.K.2's renderer uses (the same 4 by 3 sums as
OpenMoHAA's `R_ResampledScreenShot` **[O]**), from a frame drawn with no menu,
HUD, client-game drawing, print, console or menu blur.

The Load/Save menu's `FakkLoadGameClass` lists the saves as OpenMoHAA's
**[O]**: Mission, Elapsed Time and Date & Time Logged over 55.5, 17 and 27.5
per cent of its width, headed in `facfont-20`, newest first, the chosen save's
picture in the menu's `LoadSaveShot` (or `textures/menu/no_saved_games`). A
click chooses, a second click within half a second or Enter loads, Delete
asks first, and the arrows and wheel move. Removing a save asks through
`dialog` over the retail question's art, `confirm_delete` (Pak1, 256 by 64,
"ARE YOU SURE YOU WANT TO DELETE?" with YES and NO), whose YES and NO the
dialog's buttons cover; deleting clears `g_lastsave` when it named that save.
A loaded save's triggered music plays on from its place.

**Observed** on `m1l1`: saves made in play and from the menu, their pictures
in the list, the delete question and the deletion of all three files, loading
from the list in a game and from the main menu, `gamemap m1l1` to `m1l2a`
carrying the player's state through `current.spv`, `loadlastgame` after a
death, and the triggered music resuming at frame 65,536 of
`sound/music/mus_04a_suspense.mp3`. No retail save exists to read.

## Unrelated observations

OpenMoHAA, read as reference, has defects that fnTech3 does not copy, noted
where the code differs: an 8-bit packed angle read back as its scale alone;
`SkelMat4::copy` copying sixteen floats into twelve; `StatCircleTexCoord`
jumping from edge to edge at each corner; the music fade-out gain rising
rather than falling.
