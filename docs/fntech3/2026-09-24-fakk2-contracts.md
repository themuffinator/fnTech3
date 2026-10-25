# Heavy Metal: F.A.K.K.2 profile, installation and module contracts

## Scope

Heavy Metal: F.A.K.K.2 is a ninth title and a fourteenth profile, `fakk2`
(aliases `heavymetal-fakk2`, `heavymetalfakk2`, `hmfakk2`), over one `fakk/`
installation. It runs Ritual Entertainment's own engine, the one American
McGee's Alice's descends from, so it shares Alice's lineage rather than
Quake III's: `FAKK` maps, TIKI models, `.urc` menus, `.ritualfont` fonts and
function-table game and client-game libraries. fnTech3 serves it through the
Alice adapters, each told the running title's record layouts and table
positions, and crosses every record whose shape differs at the module boundary.
It has one mode, so the title key names the profile directly.

The profile is experimental. Default builds refuse it;
`-Dfakk2-development=true` lets the shared client and dedicated server activate
it for isolated probes. The runtime evidence below is from developer builds on
Windows x86. The gates are tracked in the
[compatibility ledger](COMPATIBILITY.md#fakk2-gate-matrix).

## Evidence and provenance

The user's legitimate retail copy at `E:\Games\Heavy Metal FAKK2` is the disc
tree as shipped: `pak0.pk3` and the localized `pak1.pk3` files lie loose under
`fakk\`, `fakk\enu\` and `fakk\deu\`, and everything else -- including both game
libraries -- is inside the InstallShield 6 cabinets `data1.cab`/`data2.cab`
described by `data1.hdr`. Every loose file matches the MD5 the cabinet header
stores for it. An installed layout was laid down from it without running the
installer, into `.tmp/fakk2-install/game`: the file groups the "FAKK .exe",
"PAK Files" and English localized-PAK components install, taken out of the
cabinets with their stored MD5 checked or copied from the disc tree, 19 files in
all. The disc tree was only read. No retail file is committed.

| File | Bytes | Linked | SHA-256 |
| --- | --- | --- | --- |
| `fakk2.exe` (engine, 1.0.0.1) | 1,744,384 | 2000-07-31 | `a1980fbf35c6b195e5e7db029ea30a4c5e8cdd8df8d9514223c6b8f1fa90cf88` |
| `fakk/gamex86.dll` (game) | 1,740,800 | 2000-08-22 | `f7f54f7239718c2cd534e05d4243a6acfdbe1531b2ba7c5ba59bf31c4bc25637` |
| `fakk/cgamex86.dll` (client game) | 339,968 | 2000-08-22 | `0ba4dc3e71897cc358e3042a534c88189d677ae64aeafbadfc13cc3477675f3d` |
| `fakk/pak0.pk3` | 340,402,238 | (stored 2000-07-31) | `c9777e42a32853048d4cc11916d8a5785a56f3533aef41ed84a46916af8da414` |
| `fakk/pak1.pk3` (English) | 37,092,091 | (stored 2000-07-31) | `8b08b576d5eaa780693c6af83f94ced6263f04f70adeaffd6943594290bfec02` |
| `fakk/default.cfg` | 1,798 | (stored 2000-07-21) | `2831db1b06f8b84dc0f72f1a4042a248f966498f58687059e3bb33e8c3ff6e72` |

**The disc's `fakk2.exe` cannot be read.** It is LaserLock-wrapped: its code
and data sections are encrypted on disk and its imports are obfuscated.

**The 1.02 patch's `fakk2.exe` can be read, and its client runs.** The user
supplied Ritual's official patch, `fakk2_102.exe` (2,996,683 bytes, SHA-256
`03cbfb06748552576c99b8d116811f83ea08dd2e35db82e8b1644996eeff8d73`). It was
unpacked by reading it, never by running it: a Microsoft cabinet (MSZIP) at
offset 0x292a1 holds an InstallShield 6 setup, whose game component gives
the files below with their stored MD5 checked. The two game libraries are
byte for byte the disc's. So the patched `fakk2.exe`, built the same day as
they were, is their own engine. It is plain: no LaserLock, with readable
code, data and imports.

| File (1.02 patch) | Bytes | Linked | SHA-256 |
| --- | --- | --- | --- |
| `fakk2.exe` (engine, "Heavy Metal: FAKK2 1.02") | 1,318,912 | 2000-08-22 | `681bc51c3343d01d772c5ae4aae7523ae4547350b243c875156e138043335785` |
| `fakk/pak2.pk3` | 9,736 | (stored 2000-08-14) | two scripts, the menu shader, the advanced options menu and a lip file |

So a patched installation differs from the disc's only by that executable and
`pak2.pk3`; the patch's `autoexec.cfg` is the disc's too. The pak holds
`maps/cliff2_precache.scr`, `models/weapon_chaingun.tik` and `.cik`,
`scripts/menu.shader`, `sound/player/taunt/laugh.lip` and
`ui/advancedoptions.urc`. Its advanced options menu is rebuilt: it adds a
Shadows pulldown (`cg_shadows` 0, 1 or 3), Screen Size (`viewsize`), Special
Effects Detail (`cg_effectdetail`), an OpenGL Driver field (`r_glDriver`), GFX
Info, and Mini-Console, Ctrl and Alt Bindings boxes. The engine or the retail
client game registers every cvar it links. With the pak laid into an fnTech3
home, fnTech3 draws that menu with every control retail's shows, in the same
state. The exceptions are the texture filter and the curve error, the
presentation defaults below. Retail's capture is from its own runtime, which
carries the pak.

It runs on this machine as a retail client, windowed, silent (`s_initsound
0`) and on a private desktop, with no sockets (`net_noudp 1`, `net_noipx 1`)
or, to join fnTech3's server, on the loopback address alone (`net_ip
127.0.0.1`). It runs from a runtime folder of its own, so the laid-down game
is never written. It can play demos, draw its menus and join a server.
Without the disc it cannot start a server, so it has no level or save of its
own then. `SV_Startup` (0x00445cd0) calls
a CD check (0x004584c0). The check reads `HKLM\Software\Ritual\FAKK2\PATH`
and wants `fakk2.exe` there on a CD-ROM drive. It raises `Com_Error` class 3,
"Game CD not in drive", which puts up the "Please Insert CD" dialog. The
check was never bypassed. On 2026-09-25, with the user's approval, an image of
the user's own disc copy was mounted as a CD-ROM drive (F:) and the machine
key was set to it with one elevated `reg add`. Retail then started servers for
the tests under Open, and afterwards the key was deleted and the image
unmounted. A per-user copy of the key could not stand in: UAC registry
virtualization redirects nothing for programs started from this session.

The evidence for the engine is:

- **`fakk2.exe` 1.02**, read and run as above. The wire, HUD and sprite
  findings under Client are cited from it, and so are its demo playback and
  the frames it drew.

- **the two retail libraries**, which are ordinary PEs. Their table copies,
  record sizes, call sites and configstring bounds are measured directly and
  cited by address below and in `code/compat/fakk2_module_abi.h` and
  `fakk2_game_abi.h`.
- **`alice.exe`**, the descendant engine, already read for the Alice profile.
  Where F.A.K.K.2's engine behaviour is taken from Alice's, that is an
  inference through the lineage and is marked **[I]**. It is supported, not
  proven, by the modules agreeing with Alice's engine at every call site.
  These inferences came before `fakk2.exe` 1.02 could be read. Those it has
  since checked are cited from it below. It overturned five: the wire's
  bodies, the HUD's rule, the sprites' render flags, the save picture and the
  version label. It also showed the intro, BACK TO GAME and Escape rules that
  Alice's engine lacks. It has since settled the rest: the HUD bar's first
  flash, the inventory column's size, `ui_hud`, the menu fade, the save
  list's keys, the reliable-command window, the German pak's stray lip-sync
  folder, and the slider and the submodel bound, whose inferences it
  overturned. One inference of another kind stays marked, under Open.
- **Ritual's F.A.K.K.2 v1.02 SDK**, shipped under Ritual's own non-free
  licence. It is read as reference observation only and no text of it is
  incorporated. The copy consulted is the one inside the OpenAlice tree
  (`reference/reverse-engineering/upstream/fakk2-sdk/v1.02`); the public mirror
  at [Sporesirius/fakk2](https://github.com/Sporesirius/fakk2) matches it file
  for file once line endings are normalized. The SDK is later than the retail
  1.0 modules, so where the two disagree the modules win.
- **The retail data**: the maps, models, shader scripts, menus and configs in
  the two paks.

## Profile and installation identity

| Contract | Value | Evidence |
| --- | --- | --- |
| Game directory | `fakk` | the installer's file groups (`PAK Files`, `Game DLLs`, `CFG Files` all install into `<target>\fakk`) |
| Base pak | `pak0.pk3` | the disc and the installer |
| Game module | `fakk/gamex86.dll`, exporting `GetGameAPI` | the library's export table |
| Client game module | `fakk/cgamex86.dll`, exporting `GetCGameAPI` | the library's export table |
| Menu module | none | no third library ships; the menus are 29 `.urc` resources in `pak1.pk3` |
| Client configuration | `config.cfg`, or the file the `config` cvar names | `fakk2.exe` 1.02's `Com_Init` literal (0x0042e16a). A command-line `config` longer than one character replaces it and is saved in the registry; without one, the registry's name is used (0x0042e175..0x0042e228). The configuration is written back to the cvar's file (0x0042e710), and the saves are named after it (0x0042eba0). fnTech3 does the same, as Alice's engine does, but keeps no registry: a name holds for the launch that gives it. It gives a name `.cfg` if it has no extension, and keeps the profile's own file for a name no file may carry. `+set config julie.cfg` wrote `julie.cfg` and `save/julie_test1.*`, left `config.cfg` alone, and a second launch naming it read `sensitivity 7.5` back |
| Maps | 30, all `FAKK` version 12 | every `maps/*.bsp` in `pak0.pk3` |
| Protocol | 1 | `fakk2.exe` 1.02 puts it in its connect's userinfo (0x0042161d); the SDK's `qcommon.h` agrees |

`default.cfg` is not in a pak: the installer lays it down loose in `fakk/`, so
unlike every other title's recognizer this one cannot key on it. The pinned
companion members are both in `pak0.pk3` instead: `menu.cfg` at its root, 413
bytes with CRC `609b5da3`, and `maps/fakkhouse.bsp`, the campaign's first level,
at 4,700,748 bytes with CRC `b72ef3a6`. `pak0.pk3` has 9,610 entries (9,058
files) and the English `pak1.pk3` 2,532 (2,445 files); together they mount
12,142 files.

A copy of the disc is not an installation: its `fakk\` has no
`gamex86.dll`. A server started over one stops at the game module; the
installation has to be laid down by Ritual's setup or from the cabinets first.

**Languages.** The disc carries two `pak1.pk3` files, and setup installs
either one as `fakk/pak1.pk3`: English (`fakk\enu\`, above) or German
(`fakk\deu\`, 40,777,026 bytes, 3,843 entries). Everything the recognizer
reads is in `pak0.pk3`, which both share, so a German installation is the same
profile. Its pak localizes 716 voice recordings and their 861 lip-sync files,
53 menu pictures and the options, advanced options and controls menus, and
adds a `localized.cfg`,
which the configuration order already runs. The controls menu's list of
actions stays English in both, since only `pak0.pk3` ships `ui/bind.scr`.
1,136 of its lip-sync files sit under a stray `fakk/` folder inside the pak.
Each has a namesake at its proper path, and each is byte for byte the English
pak's file: 862 differ from the German lip-sync at the proper path and 274
match it, being lines the German pak leaves unlocalized. They are English
leftovers that no name the game asks for reaches, in retail as in fnTech3.
`fakk2.exe` 1.02 asks for a sound's lip-sync at the sound's own path, its
extension replaced by `.lip` (0x00432d70). Its filesystem matches whole
names. With a test pak of this project's own in the retail runtime, `exec
strayprobe.cfg` answered `Can't find strayprobe.cfg` for a file stored as
`fakk/strayprobe.cfg`, and only `exec fakk/strayprobe.cfg` found it, while a
file at the pak's root was found by its name. A German level's lip-sync was
not probed live, because a retail level needs the disc. Its two `sounds.ls`
lists are read by nothing the retail modules or `alice.exe` contain.

## Map format

Every retail map is `FAKK` version 12: the header and all twenty lumps have the
same layout and the same fixed-size records as Alice's `FAKK` 42 -- shaders 76
bytes, planes 16, surfaces 108, draw vertices 44, fogs 72, models 40, light
definitions 52 -- on all thirty maps, and every directory ends exactly at its
file's size. The profile admits version 12 and nothing else; Alice's admits 42
alone (`code/qcommon/bsp_fakk.h`, `code/qcommon/game_profile.cpp`). The light
grid's cell is Alice's 192 by 192 by 320, with which Quake III's derivation
reproduces every map's point count exactly. Collision and all three renderers
translate the maps through the same Ritual path, with the shader-subdivision
and version flags of a version-12 map.

`fakk2.exe` 1.02 refuses a map of more than 384 inline models
(`MAX_SUBMODELS exceeded`, 0x0042575f), where Alice's engine admits 512. It
answers handle 383 with its temporary box and has no capsule (0x00426702).
The profile carried Alice's 512 until the 1.02 engine was read; it now holds
384, so the box has retail's number. As in every profile, fnTech3 keeps the
top two handles for its temporary hulls, so it refuses a map's 383rd and
384th inline models, which retail would load over its own box. No retail map
has more than `blood`'s 201, and `blood` loads.

The level-design FAQ in the SDK says models flagged `make_static` become world
geometry at compile time; the game module's spawner skips such entities
entirely, so the 56 `make_static` models in `fakkhouse`'s entity lump -- its
sky box among them -- are drawn from the compiled world.

## Module ABI

The retail libraries are the parents of Alice's. Every one of the 105 game and
124 client-game import slots is the member Alice's table keeps at the same
position -- the game's first hundred in place, its last five one place lower,
because Alice's engine left slot 100 null -- and the argument count every
called slot's sites push agrees with Alice's module at the mapped slot, except
the tag queries below.

| Table | F.A.K.K.2 | Evidence |
| --- | --- | --- |
| Game imports | 105 | `GetGameAPI` (0x10083650) copies `mov ecx, 0x69` dwords |
| Game exports | 26, API version 4 | the same routine writes 4 into slot 0 and the rest as Alice's does |
| Game entity record | 708 bytes | export slot 22's `0x2c4`, and the immediates at all three `LocateGameData` sites (0x1008291c, 0x1008345d, 0x1009c907) |
| Game client record | 2,112 bytes | the same three sites |
| Client-game imports | 124 | the first export (0x30022470) copies `mov ecx, 0x7c` dwords |
| Client-game exports | 6 | `GetCGameAPI` (0x300224e0); Alice's seventh is Alice's own |

The differences at the boundary:

- **Tag queries.** F.A.K.K.2's `Tag_Orientation` (game 78, client game 115) and
  its client-game `Tag_OrientationEx` (116) and `Tag_NamedOrientation` (117) take
  the entity's bone tags and bone quaternions as two arrays -- eight pushes at
  every site -- where Alice's take one pointer to five 36-byte controllers.
- **`R_DrawBox`** (client game 73) is called by F.A.K.K.2's client game, at four
  arguments at both sites (0x3001f65e, 0x3001f793), and never by Alice's. It is
  served as a box of the current colour filled with the renderer's white image.
- **Records.** The entity state is 488 bytes to Alice's 512: bone tags, angles and
  quaternions as three arrays, 32 surface bytes, and no second render-flags word;
  the player state is Alice's 1,048. The render entity is 364 bytes, the view
  444, a snapshot 127,188 and the game state 1,200 configstrings over Alice's
  string data. A model's command list is at most 128 commands of ten arguments
  (5,636 bytes), and a mark fragment Quake III's eight bytes. Each crosses into
  Alice's record at the boundary (`code/compat/fakk2_convert.hpp`) and back.
- **Configstrings.** 1,200 of them: models from 32 (384), sounds from 416 (256),
  images from 672 (64), light styles from 736 (32), players from 768, items from
  896 (64) and ammo from 960 -- the client game's bound (0x300222be) and dispatch
  (0x30022057..0x3002211e) and the game's item and ammo reads, and the SDK
  header's blocks.

The engine's half of the entity record is Alice's, in Alice's order, starting
where F.A.K.K.2's shorter state ends; the server's snapshot walk reads each
title's own offsets (`kFAKK2Offsets` in `code/server/server_alice_snapshot.hpp`).

All 105 game imports are served, and 120 of the 123 client-game imports that
follow the version slot. The three that are not -- 48, 49 and 88 -- have no call
site in the retail client game. An unserved slot drops by name if it is ever
called.

**The netchan is Alice's**, and so is its fragment offset: a long, where
Quake III's is a short (`alice.exe` writes it at 0x0044710e and reads it at
0x00447423). `fakk2.exe` 1.02 does the same: 1,300-byte fragments
(0x0043ae9d), the offset written as a long and the length as a short
(0x0043aeae, 0x0043aeb9), and read back so after the sequence and the
client's port (0x0043b178..0x0043b1cd). F.A.K.K.2 needs that more than Alice
has so far. `oracle`'s
gamestate is 40,244 bytes, so its fragments run past 32,767. With a short
offset every later fragment arrived negative and was dropped, and the client
waited for the gamestate forever while the server resent it.

**The netchan's limits are `fakk2.exe` 1.02's.** Its messages are 48 KB:
`MSG_Init` is given 49,152 bytes on the server (0x00443bad, 0x004479d2) and
the client (0x0041e8c9, 0x00420305, 0x00420533). It rings 64 reliable
commands of 1,024 bytes each way, where Alice's engine rings 128. The server
drops a client at its 65th unacknowledged command, `Server command overflow`
(0x004468dd), and rings them with `and 0x3f` (0x004468f5). The client
refuses a 65th of its own (0x0041ffaf) and errors on a server command 64
behind the newest (0x0041cdaa). The SDK header's `MAX_RELIABLE_COMMANDS`
agrees, but the ledger had carried it as an inference. fnTech3 had rung 128:
protocol 1 is one wire for Alice and F.A.K.K.2, and the wire's window was
Alice's. A client on that wire now rings the profile's window, the count the
profile already held for a bot (`SV_ReliableCommandLimit` in
`code/server/server.h`, `CL_ReliableCommandLimit` in `code/client/client.h`).

A burst on an fnTech3 dedicated server shows the result. The client enters
`fakkhouse`, then sends `rcon exec` for a file of `say` lines, which the
server runs in one frame. With 60 lines, retail `fakk2.exe` and fnTech3's
client each receive all 60 and stay connected. With 70, the server drops each
at the 65th with `Server command overflow`, printing the 64 pending commands
and the 65th, as retail's server does. Each client then stops with
`CL_GetServerCommand: a reliable command was cycled out`: the drop's notices
arrive past a 64-command ring. With the old window, fnTech3's server would
have sent all 70, and only a retail client would have cycled out. Retail's
own server was not run for this, as it needs the disc for its CD check.

**The message bodies are F.A.K.K.2's own, read out of `fakk2.exe` 1.02.** Its
framing is Alice's, as its readers show. `CL_ParseServerMessage` (0x00423d60)
reads no acknowledgement and stops where the data ends. Its commands are 1
nop, 2 gamestate, 5 server command, 7 snapshot, 8 centre print and 9 located
print, with 3 and 4 only inside a gamestate and 6 refused. `CL_ParseGamestate`
(0x00423b60) stops at a zero byte, reads no client number or checksum feed
after it, and caps configstring text at 16,000 characters. What differs from
Alice is the delta-coded records, which fnTech3 had sent in Alice's shape:

- The entity delta (`MSG_ReadDeltaEntity` 0x00439e70) runs Alice's algorithm
  over F.A.K.K.2's own 488-byte record, a 73-field table (0x004ea228) and a
  31-entry dictionary of changed-field vectors (0x004e6220). The first 47
  fields are Alice's. Then come five `bone_tag` and fifteen `bone_angles`
  words (Alice's controller records, with their scale, are 25). Then the tail
  at F.A.K.K.2's own offsets, with no second render-flags word. The masked
  32-byte block is `surfaces`, at +0x178.
- The player delta (`MSG_ReadDeltaPlayerstate` 0x0043a680) has Alice's 49
  fields and six masked arrays, but sends `pm_flags` and `gravity` in 16 bits
  where Alice sends 32.
- The null entity the baselines are coded from (0x0043a220) holds Alice's
  values in F.A.K.K.2's record.

`scripts/generate_fakk2_wire_tables.py` writes the tables and dictionary from
the executable (`code/compat/fakk2_wire_fields.def`, `fakk2_delta_patterns.def`;
its `--check` is `tests/fakk2_wire_tests.py`). `code/compat/fakk2_message_codec.hpp`
runs Alice's codec over them, and the engine converts to and from F.A.K.K.2's
record at that boundary (`msg.cpp`, `fakk2_convert.hpp`;
`tests/fakk2_message_codec_tests.cpp`).

**A snapshot carries the server's time residual**, in F.A.K.K.2 and in Alice
alike. After the server time, `SV_WriteSnapshotToClient` (0x00447aa0) writes
`min(sv.timeResidual, 255)`, the milliseconds `SV_Frame` (0x00447650) has run
past its last frame. Both retail clients read that byte before the delta
byte: `fakk2.exe` 0x0042385e and `alice.exe` 0x0042dc6e. fnTech3 sent
neither, so a retail reader took the delta byte for the residual and the
flags for the delta. Both of fnTech3's snapshot writers now send it, and its
client reads it and, like retail's `CL_SetCGameTime`, does not use it.

**Retail F.A.K.K.2 plays fnTech3's demos.** Before these changes `fakk2.exe`
refused an fnTech3 `fakkhouse` demo at its gamestate ("CL_ParseGamestate: bad
command byte"). With F.A.K.K.2's bodies it read the gamestate, started the
retail client game, and then overran the first snapshot. With the residual it
plays the whole demo, 207 snapshots from the opening to the creeper's attack,
with no error. Its `cl_shownet 3` trace decodes every baseline
with plausible values (origins, loop sounds, model indices, solids). Frames
from both engines are compared under Client.

**Retail F.A.K.K.2 plays on fnTech3's server.** The connectionless handshake
is Quake III 1.x's, and `code/qcommon/ritual_handshake.hpp` records it:

- The retail client asks with a bare `getchallenge` (0x004216d3). The server
  answers `challengeResponse <challenge>` (0x00443236).
- The client reads only the first argument, from whatever address answered
  (0x00421a6d).
- It then prints `connect "<userinfo>"` as plain text, with protocol 1, its
  qport and the challenge (0x004215ef).
- The server reads that line with no Huffman stage (0x00447100) and answers a
  bare `connectResponse` (0x004439fd). The client takes that reply only from
  the server's address (0x00421b2c).

fnTech3 had never run this handshake: every Ritual session so far was a local
game. Its server Huffman-decoded every `connect` but Elite Force's. The plain
text failed to decode, so the retail client's packet was dropped before it was
even logged, and the retail client was left waiting. fnTech3's own client
compressed its `connect` and asked for protocol 68, which its server refused.
Under both Ritual profiles the server now reads a plain `connect` and answers
the bare keyword. The client prints its `connect`, asks for protocol 1, and
accepts either the bare reply or one that echoes its challenge
(`tests/ritual_handshake_tests.py`). Alice's engine has no Huffman stage at
all, so the same holds for it.

With that, `fakk2.exe` 1.02 joins an fnTech3 dedicated server over loopback
UDP and plays:

- The server logs `getchallenge`, `connect`, the game module's `ClientConnect`,
  then the gamestate and the client going `CS_CONNECTED`, `CS_PRIMED` and
  `CS_ACTIVE`. The retail client loads its client game and `fakkhouse`, and
  draws the opening camera from the server's snapshots.
- Its console buttons move Julie on the server:
  - `+forward` for 1.5 s carries her 137 units along her 135° heading.
  - `+right` for 0.7 s turns her 97°, which is `cl_yawspeed`'s 140° a second
    (0x004225ae).
  - `+moveleft` moves her 54 units at right angles.
  The game module's `whatis 0` reports each position back to the retail
  client.
- Its client commands (`skipcinematic`, `wuss`, `use "HandGun" left`) reach
  the game module in order.
- Three `+attackleft` presses fire the handgun, which the ammunition arc in
  retail's own screenshots shows: 185 pixels before, 88 after. `+attackright`
  with an empty right hand leaves it as it was.

fnTech3's own client joins the same server by the same handshake and plays:
after a mouse turn, a walk on W carries Julie along the new heading.

**Retail records on fnTech3's server, and fnTech3 plays the recording as
retail does.** With `g_syncronousClients 1`, which retail's `record` demands,
the retail client recorded 70,409 bytes of `demos/livedemo.dm3` while playing.
fnTech3 plays that demo through. Retail's recorder skips entity 0's baseline,
as Quake III's does (0x0042039e). So when either engine plays the demo back,
Julie decodes against an empty baseline, with parent 0. The weapon parented
to her is then dropped with her. Retail's `CL_GetSnapshot` (0x0041c6d0) and
fnTech3's say "Not all children could find their parents" exactly 302 times
each. Both draw the same frames, with no Julie and the handgun only in the
HUD. The live session itself never printed it.

## Server and level flow

`wait` counts milliseconds, as Alice's does. `fakk2.exe` 1.02 stores its
argument, or 1 (0x0042c410), and before every line the command buffer takes the
frame's milliseconds off it and stops while any are left, a wait of 1 lasting one
frame (0x0042c5b4..0x0042c5da, looping back at 0x0042c661). `Cbuf_ExecuteText`
(0x0042c540) runs an `EXEC_NOW` line alone, leaving a pending wait as it is,
and echoes nothing (0x0042c57b), as Alice's does (alice.exe 0x00437b6b); the
engine no longer prints the developer echo it inherits for either game.

The level flow and the engine's savegame files are Alice's, as `fakk2.exe`
1.02's own code shows:
- `gamemap` (0x00441eb0) names the game `current`, writes the persistent
  file when leaving a level (0x00441f19), spawns (0x00441f26), and off a
  dedicated server queues `wait 1500; pause ; autosavegame ; pause` (0x00441f3c).
- Files are named `save/<config>_<name>.<ext>`, the config cut at its first
  dot, in a 128-byte buffer (0x0042ebe5..0x0042ec33).
- The persistent `.spv` and the level `.sav` go through the game module's
  archive exports (0x004428f0, 0x00442940).
- The `.ssv` is a zeroed 200-byte header then the area portals
  (0x00442a00): save time, level time, the comment `Starting %s` or `%s` at
  +0x08, the map at +0x48 and the name at +0x88. Its picture comes from
  `saveshot <path> 256 256`, run at once (0x00442b33).

What the game module writes inside `.sav` and `.spv` ("FAKK Archive Version 4",
`fakk/gamex86.dll`) is the module's own, the same module in both engines. No
save retail wrote has been read here.

The snapshot walk is Alice's with F.A.K.K.2's entity offsets.
`SV_AddEntitiesVisibleFromPoint` is 324 instructions in both executables
(`fakk2.exe` 0x00448090, `alice.exe` 0x004571e0), and they differ only in 28
field offsets. F.A.K.K.2's `renderfx` is 20 bytes before Alice's and the
fields after it 24 bytes before (0x1ac for 0x1c0, 0x1f8 for 0x210, 0x234..0x240
for 0x24c..0x258). Those are the offsets `kFAKK2Offsets` reads
(`code/server/server_alice_snapshot.hpp`). Two engine behaviours were found missing while bringing this
title up, and both are retail's in `alice.exe`, so both profiles now have them:

- **Inline models own their model indices.** Retail's `SV_ClearWorld`
  (`alice.exe` 0x00457960) ends by naming every inline model but the world's --
  `*1` up to the last -- through the model block's find-or-add (0x00454c00), in
  order, so each brush model's index is its own number and every model
  definition is indexed after them. Without it a definition took a brush model's
  index; the game module then found a definition behind the brush entity
  (`IsModel`) and ran its init commands on it. On `fakkhouse` that ran
  `fx_plantseeds.tik`'s on the sky camera -- `hide`, `ghost`, `classname
  Animate` -- which removed it, so no sky origin reached the client and the sky
  portal drew black. On Alice's `garden1`, in one binary with the naming
  switched off and on, both runs spawned the level's 685 entities, the module
  reported 504 entities twelve seconds in without the naming and 494 with it,
  and the opening frame at that moment looked the same.
- **The sky origin** then arrives: `skycam`'s `rendereffects +skyorigin` holds,
  the walk sends it and walks again from it, and the client game points the sky
  portal at it. `fakkhouse` draws its night sky, cloud layers, moon and planet.

**Clipping against entities** is retail `alice.exe`'s, and F.A.K.K.2 found it
broken. The game module's traces build each entity's clip model as they clip
(`SV_ClipHandleForEntity` 0x004577b0, from the entity pass at 0x0045821b): a
brush model's inline model, or the temporary box rebuilt from the entity's own
bounds and contents. This engine had kept a handle from linking. That handle
named whichever entity's box was built last, and linking kept none for a brush
model, so bullets and blows went through actors, and traces through doors and
platforms. The entity pass now also skips what retail skips (0x00458130..0x004582e5). The
state's `solid`, which the client game unpacks to clip against an entity, is
packed as retail packs it (0x0044a000) instead of carrying the box's handle
(`code/compat/ritual_solid.hpp`, `tests/ritual_world_tests.py`). Alice shares
all of it.

**A level exit** is the retail module's own, and the engine's side of it is
Alice's `gamemap` (`alice.exe` 0x00450ce0). A `trigger_changelevel` begins the
intermission (`fakk/gamex86.dll` 0x100837a0): in single player it ends a second
later (5 in deathmatch), with the screen and sound fading. The player's think
raises the exit flag once the level time passes that, with no button pressed
outside deathmatch (0x100c6561..0x100c65a9). The next `G_RunFrame` (0x10083d50)
calls `G_ExitLevel` (0x10083b30), which sends `stopsound` and
`gamemap "<map>$<spawn target>"`. Fifteen of the 27 exits in the thirty maps
name a spawn target; none of Alice's 31 do.

Retail's spawn keeps that whole argument as the server's `mapname`
(`alice.exe` 0x00455223), and its client names the map `maps/<mapname>.bsp`
from it (0x004248f5). This engine does the same. The retail client game strips
the target itself before it loads collision and the world, but fnTech3's
audio-zone lookup, which builds its path from that client name, asked for
`maps/training$spawn_fakkhouse.azb` and found nothing. Under both Ritual
profiles it now takes the part before the `$`, as the BSP name does
(`tests/audio/ritual_audio_zone_path_tests.py`). Other profiles keep the whole
name.

## Client

The client game's view, entities and sound go through Alice's client adapters.
The rest of the client is Alice's engine code with these title differences:

| Contract | F.A.K.K.2 | Evidence |
| --- | --- | --- |
| Console charset | `gfx/2d/bigchars` | the only character sheet in the paks |
| UI default font | `verdana-12` | `fakk2.exe` 1.02's menu loader starts every `.urc` widget with it (InitFrame at 0x004aad5b..0x004aad7e, 10, 10, 100 by 64); Alice's starts them in `courier-14`. A bare `UIFont` is `verdana-14` (0x004b0253), which this profile had taken for the widgets' font, so its menus' text was drawn a size too large |
| Menu soundtrack | `music/intro` | `fakk2.exe` 1.02 starts it in the intro (0x0040e0e1) and on a disconnect outside a game (0x00421338) |
| Intro | the soundtrack, then Ritual's plate and Gathering's, then the main menu | `fakk2.exe` 1.02's `_intro` (0x0040e0b0); see below |
| Loading screen | `textures/menu/loading` at the picture's own size, centred on black | `fakk2.exe` 1.02 registers it as the UI starts (0x0040ece7). While a level loads it fills the screen black (0x0040acd0) and draws the picture white at the size the renderer gives (slots 48 and 49), centred on the view's full-screen frame (0x0040b14c..0x0040b222). Retail counts that size in pixels; this profile scales it with the window, as it does the menus |
| View text | `facfont-20`: the frame counter, the centre print and the located print | `fakk2.exe` 1.02 builds the full-screen `View3D` with it (0x0040ecb2). Alice's is `courier-14` (0x00410840). This profile had drawn them in the UI's default `verdana-14`; a retail frame of `saveweaps 1` with `fps 1` now matches in glyphs and place |
| `shortversion` | `"1.02"`, read-only | `fakk2.exe` 1.02 registers it so (0x0042e4fd); the main and quit menus show it. Alice's is `"1.00"` (0x00439c9e) |
| Widget pictures | drawn in the whole foreground colour, the hover picture on the button that has the keys | `fakk2.exe` 1.02's `UIWidget::Display` (0x0049ef30); see below |

**The intro and BACK TO GAME** are `fakk2.exe` 1.02's, and neither is Alice's.

Its `_intro` (0x0040e0b0) keeps Alice's "wait N; _intro" chain and plate
drawing (0x0040ad90), but runs its own steps:

1. The soundtrack `music/intro`, then 3 seconds.
2. `textures/menu/ritual`, then `textures/menu/gathering` (0x0040eda1,
   0x0040ee60). Each is faded in over a second, held three and faded out over
   one, at 1.5 times its size.
3. The main menu.

No movie plays. A level that loads during it only puts the plate away.
fnTech3 had gone straight to the main menu, from a note that the paks ship no
movie and no legal plate. They do ship these two plates (`.ftx`, in
`pak1.pk3`), which it had missed.

The intro's last step (0x0040e295..0x0040e2eb) disables the main menu's
`backtogame` outside a game. Escape does the same (0x0040c032..0x0040c0e9)
whenever it leaves the main menu up: disabled outside a game, enabled in one.
Alice's main menu does this itself with `showifalive`. F.A.K.K.2's `main.urc`
has no such line, so its engine does it, and fnTech3 now follows the engine
(`tests/alice_screen_tests.cpp`, `tests/fakk2_ui_source_tests.py`).

F.A.K.K.2's Escape (0x0041fb3d..0x0041fbf0) keeps Alice's order, with three
differences:

- It blocks nothing while the intro runs.
- A second press within a second opens the menu even in a movie.
- It stops no movie.

A capture of the intro shows each plate in turn and ends on the main menu
without BACK TO GAME.

**Widget colour.** `alice.exe` draws a widget's picture white at the foreground
alpha (0x004bd9ab..0x004bd9d8). `fakk2.exe` 1.02's `UIWidget::Display`
(0x0049ef30) draws it in the whole foreground colour, its alpha times the
widget's (0x0049f078..0x0049f0a2), and so does this profile. F.A.K.K.2's menus
include `ui/common.inc`, whose last widget is a full-screen `textures/menu/clouds`
label with a 0.2 grey foreground; that shader is two additive, scrolling
`rgbGen global` passes, and drawn white they wash the whole main menu out.

**The lit button.** The same function draws a widget's `hovershader` over its
picture while a flag in the widget is set (0x0049f103..0x0049f135). A button
sets the flag when the mouse comes onto it (0x004ae400) or presses it
(0x004ae0a0), and puts it out when the mouse lets go off it (0x004ae2d3). The
button's mouse-exit response is empty (0x00404200), so the flag stays when the
mouse goes. The window manager puts it out on the control it takes the keys
from when another takes them (`ActivateControl`, 0x00498c10), so the lit
button is the one that has the keys.

After every change of menus (0x0040ab60), the top menu gives the keys to its
current order: the widget whose `ordernumber` it last had, 1 at first
(0x004a40d0 -> 0x0049f8d0). Every other widget is put out (0x0049f900), and Up
and Down step the order (0x004980d1..0x004980ec). The main menu therefore opens
with NEW GAME lit, pulsing with its `_selected` shader's `rgbGen wave`.

Alice's buttons put the flag out when the mouse leaves (`alice.exe`
0x004cfc60), and this UI draws Alice's hover pictures under the mouse, as
before. F.A.K.K.2's are now lit by those rules. A probe (`.tmp`, not kept)
opened the main menu, pressed Down, then pointed at CONTROLS and away. It
found NEW GAME lit, then LOAD\SAVE, then CONTROLS, which stayed lit after the
cursor left it (`tests/fakk2_ui_source_tests.py`).

**SetColor is immediate.** `alice.exe`'s `R_SetColor` (0x00498b80) stores the
colour `rgbGen global` and `alphaGen global` read and calls `glColor4ubv`, so each
picture takes the colour it was drawn in. fnTech3's renderers batch pictures and
read the colour when a batch is drawn; all three now draw a Ritual batch whose
shader reads that colour before the colour changes. Before, the clouds took the
white the menu code set after drawing them, for Alice as for F.A.K.K.2.

**Shader dialect.** F.A.K.K.2's scripts are Alice's dialect plus one keyword
Alice's shipped scripts never use: `nextBundle`, which 59 F.A.K.K.2 stages use
-- the menu's Julie art, the HUD gradients, water, blood and glass. In
`alice.exe` (0x0048c378) it moves the stage's image and texture-coordinate
keywords to the second texture bundle and sets the stage's multitexture bits to
1, which the draw turns into `GL_MODULATE` (2 being `GL_ADD`, 0x00492d8f); a card
without multitexture, or a third bundle, fails the shader. All three parsers now
do this (`code/renderercommon/tr_alice_shader.h`). Of F.A.K.K.2's 1,347 shaders,
17 are refused in every parser: the 15 stageless compiler shaders Quake III's
rule also rejects, `crank`, whose stage lost its opening brace, and
`textures/aldiewater`, which says `deform`; an unknown general keyword fails a
shader in retail too (`alice.exe` 0x0048d141). 58 `nextBundle` stages parse
alike in all three (`tests/alice_shader_parse_tests.py`); the 59th sits in an
`if 0`.

**Configuration and keys.** Ritual's `Com_Init` runs `default.cfg`, then
`menu.cfg`, then the saved configuration unless in safe mode, then
`localized.cfg` and `autoexec.cfg` (`alice.exe` 0x004398eb..0x00439a13);
F.A.K.K.2's `menu.cfg` holds its camera, skill and binding defaults, so both
Ritual profiles now run that order. `default.cfg`'s `altbind`/`ctrlbind` lines
need the Alt and Ctrl tables' commands before the first configuration runs, as
retail's `Key_Init` registers them (0x00429b60), so they are registered with the
key commands. The saved configuration now also keeps the aliases, as Ritual's
does (0x004380a0), which is where the weapon groups live (below).

### The player state the engine reads

The engine reads three stats itself: the cinematic flags (whose bit 0 skips a
cinematic on Escape, hides the HUD and closes the inventory), the fade kind and
the letterbox. Alice keeps them at 5, 6 and 7; F.A.K.K.2's game module stores
them at 14, 15 and 16 of the player state's stats, observed where it writes the
client's player state (stats at +0xb0):

| Stat | F.A.K.K.2 | Retail store (`fakk/gamex86.dll`) |
| --- | --- | --- |
| Boss health, at least 1 while positive | 13 | +0xe4 = 1 at 0x100d133c |
| Cinematic flags | 14 | +0xe8 = 0, then 1 for a cinematic and +2 for an actor camera, 0x100d1349..0x100d1376 |
| Additive fade | 15 | +0xec = 1 at 0x100cc3a4 |
| Letterbox, fraction x 32767 | 16 | +0xf0 at 0x100d1502 and 0x100d158f, 0 at 0x100d1516 |

Before this, the F.A.K.K.2 client read Alice's slots: the letterbox from the left
weapon's maximum ammunition, the fade kind from the water level, and the
cinematic flag from the right hand's clip, so Escape could send `skipcinematic`
instead of opening the menu. The screen, the HUD gate and Escape now read the
running title's slots (`code/client/client_alice_ui.hpp`).

## HUD

F.A.K.K.2's HUD is four of its `.urc` menus -- `healthhud`, `weaphud`,
`dualweaphud`, `bosshud` -- built from UIFakkLabel, which every `Label` in a
Ritual `.urc` is: the loader turns the word into `FakkLabel` (`alice.exe`
0x0040e240, table 0x0050ba68). The label's nine keywords (responses 0x0050d820)
and its draw (0x0041ef00) are served for both profiles; Alice's menus use none of
them. The pure part is `code/compat/fakk2_hud.hpp`, pinned by
`tests/fakk2_hud_tests.cpp`.

- **What a label shows.** The first of its stat, item and model slots that is
  set decides, and its title is then never drawn. A stat without `statbar` is a
  number, centred in the label in its foreground colour, which no menu fade
  touches.
- **The bar** (0x0041eb90) fills to the stat over its `maxplayerstat` stat, or
  over max less min; the value is never offset by min. A full or empty bar
  clamps, and an empty one (0 over 0 when no weapon is held) draws nothing. The
  `*_tileshader` keywords set the widget's own tile flag, which `shader` clears:
  tiled, a vertical bar fills from the bottom over a texture that stays put;
  stretched, a horizontal bar squeezes the whole image (bosshud's). A stretched
  vertical bar is retail's own inverted one and is kept; no shipped file uses it.
- **The flash** restarts on any change of the fraction and fades linearly over
  750 ms (0x004ff964), drawn additively over the bar with the same quad.
  `fakk2.exe` 1.02 does the same (0x004182ab..0x004182cc, rate 0x004d5608). Its
  label's constructor (0x004177f0) never sets the last fraction or the start,
  and its class allocator takes plain heap memory (0x00438eef), so a bar's
  first appearance flashes, as the port's sentinel has it. An empty bar draws
  nothing and leaves the flash alone (0x00418270..0x004182a5).
- **The model** is the inventory item in an active-item slot -- looked up again
  only in the second half of each real-time second (0x0041f0d5) -- drawn by its
  `hud*` placement, bobbing or spinning, at the distance its largest extent
  fills a 30 degree view from (0x004ff45c), in a view of its own with no world
  and no light (UI_Render3DModel 0x00408210).
- **Which menus show**, from `fakk2.exe` 1.02's UI update, which finds the
  four menus by name as the UI loads (0x0040f0fe..0x0040f312). The HUD shows
  while `ui_hud` is set, the client is in the game, the view has no letterbox
  (`View3D`'s flag, set while stat 16 is above zero, 0x00419a90) and no menu
  up (0x0040b29b..0x0040b2d1). It has no delay, and it ignores the cinematic
  flag that Alice's HUD reads. While it shows (0x0040b2d7..0x0040b326):
  - the health and water bars, always;
  - `dualweaphud` while the two-handed slot (activeItems[4]) holds an item,
    and otherwise `weaphud`, whatever the hands hold, so its two empty rings
    show from the start;
  - `bosshud`, sliding in, while stat 13 is above zero.
  An earlier reading, made before this executable could be read, kept the
  one-handed menu for a weapon in hand and waited 550 ms behind Alice's gate.
  Retail's frames showed the rings and fnTech3's did not. The menu names are
  the ones the files declare, and two are crossed: `ui/weaphud.urc` declares
  `dualweaphud`. They are shown, never pushed, under the menus.
- **Units.** The HUD menus draw in the menus' 640 by 480 units, scaled to the
  screen, as every Ritual menu does here. Retail draws them in pixels instead:
  at 800x600 its 128-pixel health gauge sits in the corner at 128 pixels, and
  its 640 by 480 main menu sits centred with black borders. fnTech3 keeps its
  own scaling as presentation, so the menus keep their proportion at any
  resolution. Content and layout are retail's.
- **Widescreen.** Scaled with the screen, the HUD's round gauges stretched on
  a wider one. The four HUD menus now take FnQuake3's HUD correction
  (`cl_hudAspect`, `cl_hud.cpp`; AGENTS.md invariant 11), which only Quake III
  Arena and RTCW had: while the UI draws them (`CL_AliceUIDrawMenus`), each
  picture, fill and glyph is first cut to its widget, menu and screen as
  before, then placed by the first `fntech3-hud.json` rule its art's name and
  centre match, uniform and anchored, and a model label's view follows its
  ring (`CL_Fakk2RenderModel`). The package's preset
  (`pkg/games/fakk2/fakk/fntech3-hud.json`, SHA-256 `041e2d00...`) has three
  rules, each the place of a stock menu from `pak1.pk3` with a 16-unit
  margin: `healthhud` (128 by 128, `align top left`) to the upper left, both
  192 by 96 weapon menus (`align top right`) to the upper right, and
  `bosshud` (132 by 32, `align left centery`, sliding in from the left) to
  the left, centred. Every other menu, the inventory and the client game's
  2D layer stay stretched. `cl_hudAspect 0` restores the stretch, and
  `cl_hudDump`, `hud_reload` and the package's HUD file names are now
  registered and read for F.A.K.K.2 as for those two games.
  - *Observed* (2026-09-25, working tree over `b68782b`, Windows x86
    `meson/build/win32-fakk2/fntech3.exe` SHA-256 `207a3f41...`, GLx
    `50c9268d...`, laid-down English install, `probe_ritual_client.py
    --profile fakk2 --map fakkhouse`, windowed on a private desktop):
    after `skipcinematic`, `wuss`, `use "GreenSword" right` and
    `set bosshealth 0.6`, engine `screenshotPNG` captures at 1680x720 show the
    health gauge, both rings with the sword's model and the boss's bar round
    and in their places; with `cl_hudAspect 0` the same frame stretches them
    by 1.75 across. At 960x720 the two settings draw the HUD alike. A
    `cl_hudDump 1` second at 1680x720 dumped 17 groups, each suggesting the
    anchor its rule gives, the right hand's model view among them. The
    installation was unchanged.
  - *Not covered:* a turned glyph (`re.DrawQuad`) is drawn unplaced; no HUD
    menu shipped with the game turns its text.
  - Since 2026-09-25 the world view also takes the FOV correction, as every
    title's written for 4:3 does
    ([widescreen record](2026-09-25-widescreen-hud-and-fov.md)).
- **`ui_hud [1|0]`** (0x0040fbe0): `alice.exe` keeps the handler but never
  registers it, and its `ui_hud` is an archived cvar (0x00410564). F.A.K.K.2
  registers the command as its UI starts (`fakk2.exe` 1.02
  0x0040ec07..0x0040ec11). Its handler (0x0040e010) sets the flag the HUD gate
  reads (0x004df7b8, 1 at start), and shows or hides the HUD menus at once.
  No cvar holds the flag and nothing saves it. fnTech3 holds it in `ui_hud`,
  which it now registers unsaved for F.A.K.K.2; it had archived it, so a HUD
  turned off stayed off in the next session.
- **The UI's cvars.** `fakk2.exe` 1.02's UI registers `ui_drawcoords`,
  `ui_minicon`, `ui_consoleposition`, `ui_inventoryfile`, `ui_console`,
  `ui_newvidmode` and `dlg_badsave` (UI_Init 0x0040e9df..0x0040ea63), and none
  of Alice's `ui_subtitles`, `ui_skipDepthClear`, `ui_crosshair` or
  `ui_jumpreticle`. fnTech3 had registered Alice's four for both titles, and
  saved three into F.A.K.K.2's configuration; now only Alice registers them.

**Lighting a model in a view without a world.** `alice.exe` starts such an entity
from no light at all (0x004acbfc..0x004acc26), and UI_Render3DModel is passed no
light. F.A.K.K.2's weapon models use lit shaders, so under that rule its HUD and
inventory would draw black silhouettes -- which is what the first probe showed.
F.A.K.K.2's renderer kept Quake III's flat light there: `fakk2.exe` 1.02 lights
an entity from the grid unless the view has no world, the world has no grid or
the entity is `RF_FULLBRIGHT` (0x0048ed3d..0x0048ed61). Otherwise it gives it
ambient and directed light of 150 times the identity light from the sun's
direction (0x0048f143..0x0048f19f), and `RF_MINLIGHT` adds 32 times it
(0x0048f1a5). This profile does the same in all three renderers
(`R_SetupAliceEntityLighting`); Alice's rule is unchanged.

## Inventory and weapon groups

The inventory is the engine UI's, read from `ui_inventoryfile`
(`global/inventory.scr`) as the UI starts and on `inv_restart` (`alice.exe`
0x0040eeb0, 0x00402b00). Alice ships that file empty; F.A.K.K.2's lists six
types and 25 items, and it loads with no warning
(`tests/fakk2_hud_tests.cpp`). The grammar -- keywords without regard to case,
the words on the keyword's line as its arguments, retail's messages and checks,
an error emptying the whole inventory, `hudmodel` defaulting to `model` -- and
the selection's steps and the commands are `code/compat/fakk2_inventory.hpp`;
the pop-up is `code/client/cl_fakk2_inventory.cpp`. Served for F.A.K.K.2 only.

- **Commands** (registered once, outliving UI restarts): `inventory` opens it
  with the cursor on the first type (0x00408550); `warpinv <type>` opens it on
  that type's first item (0x00408510); `nextinv`/`previnv` open it on the first
  type and step (0x004084b0, 0x004084e0); `inv_restart`; `saveweaps`.
- **Keys while it is open** go to it before the menus' catcher sees them, F1 to
  F12 aside (0x004091b0): the type's `warpinv` key again steps through that
  type's items, the attack keys use the picked item with that hand, `nextinv`
  and `previnv` step through held items across types, the arrows and wheel move
  in cursor mode, and any other key does what it is bound to once an item is
  picked. It closes five seconds after the last change (a key it takes adds five
  more), after a use from the keys, in a cinematic, when a menu comes up, and on
  Escape before anything else (0x0040d4e1).
- **Drawing** is `fakk2.exe` 1.02's. Its keyword handlers show where each
  picture is kept: a type's `button_shader`, `hover_shader` and `sel_shader`
  at +0x08, +0x10 and +0x14 (0x00404312, 0x00404462, 0x004045b2), an item's
  `bgshader`, `barshader` and `selitem_shader` at +0xd4, +0xd8 and +0xdc.
  - The type column (`FakkInventory::Draw` 0x004088c0; `alice.exe`'s is a bare
    `ret`) draws one cell a type: its button picture, or, for the current type,
    its hover picture while the window manager finds the column under the
    cursor, and its selected picture otherwise. The cursor counts in either
    mode, so after `nextinv` has warped it onto the column the current type
    shows its hover picture. fnTech3 had tested the pointer only in cursor
    mode.
  - A row of the list (`FakkItemList::Draw` 0x00409950) draws, in order: the
    ammunition gauge; the cell's `bgshader` over it; the `selitem_shader` of
    the picked item unless it goes on top; a held item's model in its
    `modelwindow`, moving only while picked; then an on-top `selitem_shader`.
    fnTech3 had drawn the gauge over the cell art. It had also drawn the
    type's hover picture where an item's own `selitem_shader` belongs, when
    that picture does not go on top.
  - The model's view (0x00407dd0) is a 30-degree view with no world and
    nothing else set. F.A.K.K.2 has no `ui_skipDepthClear`, so the view clears
    the depth as every view does. fnTech3 had kept Alice's rule, which leaves
    the depth alone, so a model was hidden wherever the world behind its cell
    was nearer. The light sword in the bottom cell stood over the courtyard
    floor and drew nothing.
- **The ammunition gauge** is the same function's. With an ammunition kind
  and a bar picture, the amount over the kind's maximum, held to 0..1 (none
  with no maximum), fills from the cell's foot:
  - a `barwidth` by `barheight`-times-the-fraction box when either size is
    given, else the cell's width by its height times it (0x00409a22..0x00409b36);
  - drawn with the renderer's `DrawTilePicOffset` (0x00481dd0; slot 22 of the
    table `GetRefAPI` fills at 0x00461da1), a texel a unit, sampling the
    picture where the box sits in the list and only then moving the box by the
    bar offsets.

  All 14 gauges in the script are the same, 27 by 27 raised by 4 over the
  64 by 64 `textures/hud/in_grad_bottom`, which repeats, so each arc fills
  the copper groove at the cell art's lower left, under the art
  (`inv::AmmoGauge`, `tests/fakk2_hud_tests.cpp`).
- **Compared with retail.** Retail's client, playing on fnTech3's server, and
  fnTech3's client opened the inventory from the same state (`wuss`, then
  `warpinv guns`, `nextinv`, `warpinv swords`, `inventory`). The two draw the
  same columns, cells, gauges and models, retail at one pixel per unit and
  fnTech3 scaled to the window.
- **Using an item** is `fakk2.exe` 1.02's: the left hand's handler
  (0x00409e50; send 0x00409fc9..0x0040a026) and the right's (0x0040a090; send
  0x0040a209..0x0040a266). A click goes by the button's binding, as before;
  an item not held, equipped `none` or out of checked ammunition is refused.
  Otherwise the handler sends, in order:
  - the item's `command`;
  - `use "<name>" dualhand` for a two-handed weapon;
  - `use "<name>" lefthand` or `righthand` if the item fits the hand used;
  - `use "<name>"` for an item equipped by `use`;
  - nothing else, so a sword (`right` only) used from the left hand is
    refused.

  `alice.exe` sends only the `command` or `use "<name>"`, from either button.
  This profile had inferred a fallback to the other hand, in the words
  `saveweaps` writes. It now sends retail's `hand` words, which
  `fakk/gamex86.dll` accepts (0x101784c4..0x101784f4). A probe picked the
  GreenSword with the cursor: a left click left Julie's hands empty, and a
  right click put the sword in her right hand.
- **Weapon groups.** `saveweaps <n>` (0x0040ffe0) makes alias `weapgroup<n>`
  of the weapons in hand -- `use "<x>" dual`, `dualwield "<l>" "<r>"`, `use
  "<x>" left|right` or `holster` -- and centre-prints `Saved Weapon Group <n>`;
  `default.cfg` binds Ctrl+digit to save and Alt+digit to recall. Retail's
  configuration wrote the alias quoted, one letter short (0x004380f4), so a
  group with a quoted name did not survive a restart. Here a Ritual alias holding
  quotes is written bare and read back whole: the Ritual `alias` command takes
  the rest of its line as typed when given more than one word (retail's joins
  them, 0x00437e90). Other profiles' aliases are unchanged.

## Menu widgets

F.A.K.K.2's options, advanced options, controls and graphics menus use widgets
and keywords Alice's never do. All are served for both profiles, since retail's
UI serves them for both; only F.A.K.K.2's files reach them.

**The menus' cvars.** Both Ritual clients register `com_blood` (`1`, archived,
in the userinfo and serverinfo), `s_milesdriver` (`auto`) and `s_speaker_type`
(`0`) beside `cg_predictItems`. Their sound inits register `s_khz` (`22`), and
their input registers `joy_threshold` (`0.15`, archived). Evidence: `fakk2.exe`
1.02 0x00422806..0x0042284e, 0x0043e6b8 and 0x0045768a, `alice.exe`
0x0042cafd..0x0042cb39 and 0x0046adba.

F.A.K.K.2's game module reads `com_blood` for its gore; the options menus show
the rest. fnTech3 had none of them outside its legacy mixer, so EXCESSIVE
VIOLENCE drew unticked and the driver, speaker and quality fields blank. Its
Windows input has no joystick layer, so the controls menu's joystick threshold
slider sat at 0. It now registers all five for both titles. This engine's audio
uses only `s_khz`, and that only in its legacy mixer. Its input reads
`joy_threshold` only where it has joystick support, which Windows builds
gained on 2026-10-24 ([joystick record](2026-10-24-windows-joystick.md)).

The 1.02 patch's `pak2.pk3` replaces `ui/advancedoptions.urc`, adding an
OpenGL driver field (`r_glDriver`, `opengl32` here) and renaming Curve Detail to
Curve Error. With it, the menu matches retail's in its fields. Three values
differ in the menus, each from this engine's shared defaults:
- the texture filter reads Trilinear, where retail's is Bilinear;
- Curve Error shows `r_subdivisions` 1, where retail registers 4
  (`fakk2.exe` 1.02 0x00460e77..0x00460e8f). Every profile's renderers
  tessellate curves this finely.
- The options menu's resolution field reads `-2`, the mode number of this
  engine's default, the desktop's size, or `-1` for a custom size. The field
  names only modes 0 to 9 (`ui/options.urc`'s `linkstring`s), and shows any
  other value as it is. Its pulldown sets modes 3 to 9, whose sizes match
  retail's list (`cl_main.cpp`'s mode table), after which the field names
  them.

| Widget or keyword | Behaviour | Evidence |
| --- | --- | --- |
| `PulldownMenuContainer` | an arrow per entry at its picture's size; a press opens the list below it at once, dragging highlights a row, letting go over the list runs that row's `command` or `cvar` string as console text and closes it, letting go elsewhere just closes it; no keys | 0x004e2940, 0x004e2ad0, 0x004e2960; list 0x004e5600, 0x004e58c0, 0x004e5b40 |
| `MultiLineEdit` | lines split at newlines only and printed whole from any line whose top is in the box, a 16-unit scrollbar at the right, selection by mouse and keys, Ctrl+Ins copy, Shift+Ins paste; `edit 0` stops only typing; `updatecvar` reads the linked cvar once, as its line is read | 0x004dc4f0, 0x004dd180, 0x004dd810, 0x004dc4b0; `fakk2.exe` 1.02 prints each line whole (0x004b760b, 0x004b76df), and the widget's cut ends it at the box |
| `linkstring value words` | a linked Label shows the words of the first value its cvar's waiting value matches, case aside, else the value | 0x004d54a0, 0x004d5810 |
| `tileshader` | the picture repeats once per image size from the widget's corner; Alice's, drawn over the whole screen in pixels, repeats from the screen's corner | 0x004ba610, 0x004bcfe0; `fakk2.exe` 1.02 draws each widget in a 2D window of its own, its corner the origin (`Set2D` 0x0049ee80 -> 0x0049eee0), and tiles from (0, 0) there (0x0049f0b5..0x0049f0cb) |
| menu-level `fadein` | the whole menu fades in and out over it; a pushed menu shows once the last one's time has passed, and F.A.K.K.2's fades over what its screen kept (below) | 0x004b8120 on the menu's container; `fakk2.exe` 1.02 hides the old menu at once and posts the new one's show after its longest motion time (PushMenu 0x004a3020, 0x004a3155..0x004a3186; the time 0x004a4240); the hide moves every item out (0x004a4120), and a fade runs out as it runs in (0x0049de30) |
| menu header motion | the menu slides in from the edge its header names (bosshud from the left over a second) | `fakk2.exe` 1.02's `menu` handler gives the menu's container the direction (`from_left` 1, `from_right` 2, `from_bottom` 3, `from_top` 4) and a fifth argument's time (0x004ab2c3..0x004ab356) through a widget's own setters (0x0049f260, 0x0049f290), which the widget `direction` keyword uses too (0x0049bc23) |
| `aliascache` | the alias joins the global list and its sound is cached; hover and click sounds resolve through the list | 0x004baa30, 0x0044ce10 |
| `ui_checkrestart` | the Apply buttons' restart check | 0x0040eee0 |
| `r_gfxinfo` | the renderer report, written as the UI starts in retail's lines, those this engine has, one `GL_EXTENSIONS:` line an extension | 0x00474b40; `fakk2.exe` 1.02 cuts the extension string at its spaces and writes each word longer than one character (0x00460813..0x00460868) |
| `ui_newvidmode` | -1 with no menu up, `r_mode` once one is | 0x0040c3f0 |
| `FakkLoadGameClass` shown | the list control draws the saves: "Location", "Elapased Time" (retail's spelling) and "Date & Time" over 40, 20 and 40 per cent of the width, each header a 0.3-grey box with a one-unit white edge and its title in white; a line a save in the widget's colours, the chosen one yellow on blue; the map, the level's seconds as `H:MM:SS`, and `ctime()`; and a scrollbar always at its right, its arrows greyed while the saves fit. The chosen row is the save LOAD GAME and DELETE GAME take, and the keys and a click choose it: Up, Down, Left and Right step, Page Up and Page Down go to the shown page's first or last row and a page on from there, Home and End go to the ends, and the wheel scrolls two rows without choosing; a click below the last row chooses the last; the same row clicked again within half a second and two units loads it, and Enter loads and Delete removes the chosen save; the menu's one picture, `LoadSaveShot`, shows its shot, or `textures/menu/no_saved_games` with none chosen. Alice's list is `neverdraw` behind six pictures, and its headers are 0.75 grey in black | `fakk2.exe` 1.02: `UIFAKKLoadGameClass` over `UIListCtrl` (0x00414430, 0x004ac730); headers 0x004ac8e0 (0.3 grey 0x004ac941, white 0x009b2ad8, the box 0x0049a650); the scrollbar `UIVertScroll` 0x004b3ab0; the shot 0x00415390..0x00415431; delete and load read the chosen row (+0x160) at 0x004155b0 and 0x00415660; keys 0x00415710, then `UIListCtrl::KeyEvent` 0x004af9a0 (Page Up 0x004afa57, Page Down 0x004afa83, the wheel 0x004af9e3 and 0x004afa05); a row set is held to the count (0x004af7a8) and shows its save (`listbase_item_selected` -> 0x00415390); the double click 0x004ad497..0x004ad4fe (2.0 at 0x004d61d8), `listbase_item_doubleclicked` -> 0x00415660. `alice.exe`: draw 0x004ce3a0 (headers 0x004cde00, rows 0x004ce010), columns 0x0041a150, row strings 0x0041a9a0, colours 0x004cd760..0x004cd93e |

A linked label now shows its cvar's waiting (latched) value, as retail's does
(0x0040e1a0), for both titles. The popup defaults to the profile's UI font where
`alice.exe` names `courier-14`, which F.A.K.K.2 does not ship.

**Checkboxes** are indented. Both engines' `UICheckBox::Draw` put the 16-unit
box at an indent the constructor sets, and the title at twice that indent past
the box. `alice.exe` sets it to 0 (0x004e0290). `fakk2.exe` 1.02 sets it to 16
(0x004bae69..0x004bae7a, Draw 0x004bb6e0..0x004bb84b), so its box sits 16 units
in and its title 48. fnTech3 had drawn F.A.K.K.2's by Alice's rule, box at the
widget's edge and title 16 in. In the advanced options menu, every one of the
15 checkbox titles now starts where retail's does, at 49 units with the
glyph's own bearing; they had started at 17. The same menu's "Use GL Fog" links
`r_useglfog`, which both renderers register as 1, archived (`fakk2.exe`
0x004616fc, `alice.exe` 0x00475b06). It turns on GL's own fog for a level's
distance fog (0x0047c599). fnTech3 had not registered it, so the box showed
unchecked. The client now registers it for both titles, as it does the Miles
driver the audio menu shows. It is described as unread, since fnTech3's
renderers draw that fog their own way.

**Sliders** are `fakk2.exe` 1.02's `UISlider`. fnTech3 had drawn only a
thumb on the widget's groove, 8 units wide, and no arrows. That inference
held that the paks have no arrow art, but `pak1.pk3` has it.
- The constructor (0x004b4aa0) makes the arrows 15 units wide and the thumb
  10. It names six pictures: `textures/menu/arrow_left`, `arrow_right`,
  `slider_thumb`, and a `_sel` form of each.
- `thumbshader`, `leftshader` and `rightshader` replace a picture and its lit
  form; a lone name serves as both (0x004b4ec0, 0x004b5160, 0x004b5400). No
  retail menu uses them.
- The track is the widget's own shader. Draw (0x004b64e0) puts the thumb at
  15 + 152t on a 192-unit slider, lit while dragged, then each arrow at its
  end, lit while held.
- An arrow whose two pictures are one name is drawn as a 3D-box button, sunk
  while held, with the picture 2 units inside it (0x004b660b, 0x004b66df).
- The pictures take the colour the widget's own draw left set
  (`UIWidget::Display` 0x0049ef30): white at the widget's alpha, then the
  fill's, a 3D border's last, light strip, then the foreground under a shader
  (0x0049f0a2). Every slider the game's own menus show has that shader and a
  white foreground. The dev menus' sliders have none, and keep the light
  shade of their indented border. Their `borderstyle "RAISED"` is a word
  neither `fakk2.exe` (0x0049cb5d..0x0049cbac) nor fnTech3 reads, so the
  default, indented, stays.
- The arrows add or take away a step and hold the value to min, then max,
  whatever the step's sign (0x004b5a20, 0x004b5a70). Alice's hold a reversed
  range from max up to min.
- The mouse and keys are Alice's (0x004b5f40, 0x004b5d10, 0x004b64c0,
  0x004b5c60, 0x004b5ca0, 0x004b5ac0):
  - a press on an arrow steps, repeating after 0.2 s and then every 0.1 s
    while the cursor stays on it;
  - a press or drag elsewhere sets the value from x;
  - a drag that leaves the widget puts the value back;
  - Left and Right step.
- The cvar is written with `%d` or `%f`, and the stuff command with `%d` or
  `%.3f` after a space (0x004b61c0).

Captures of the options, advanced options and controls menus match retail's
slider for slider. The joystick threshold matched once `joy_threshold` was
registered. Curve Error differs by its value (above).

**Each widget is cut to its frame.** `fakk2.exe` 1.02 draws each widget in a
2D window of its own (0x0049ee80). The window's viewport and projection are the
widget's frame, and the renderer's scissor (0x009b21a8, renderer slot 28) is
that frame cut to its parent's (0x0049ec21..0x0049eceb; `noparentclip` skips
the cut, and no retail menu uses it). Nothing a widget draws runs past it. The
graphics report's last line stops at its box, where fnTech3 had drawn half a
line over the frame below. fnTech3 now cuts every F.A.K.K.2 widget's pictures,
fills and text to the widget within its menu and the screen, texture
coordinates alike. Text turned by `fontangle` is drawn whole; no retail menu
turns its text. The popups and the press-a-key picture are windows of their
own and are not cut. Alice's widgets are cut only by `forcescissor`, as before.
Captures of every main menu and the HUD are otherwise unchanged.

**What the screen keeps under full-screen menus.** `fakk2.exe` 1.02's UI
update clears nothing while a full-screen menu is up (0x0040b0cf..0x0040b296).
It blacks the screen only for loading, before a game and after one. So each
frame's menus are drawn over the last frame's image. Every full-screen menu
fades over half a second and fills itself white (`bgcolor 1 1 1 1`) under its
pictures, both at the fade's alpha.
- As a menu goes, it hazes toward white over its own image.
- At the switch the screen still holds that hazed menu.
- The next menu comes up through the haze, and covers the old image within a
  few frames.
- The picture depends on the frame rate, since each frame blends again. The
  retail captures were taken at about 95 frames a second.

Retail captures of the main menu giving way to the options show this at 302,
363, 503, 636, 780 and 924 ms after the push (timed from its `com_speeds`
log). fnTech3 fades both menus through black.

That is how retail's engine presents the menus' data, not the data itself
(AGENTS.md invariant 11), so fnTech3 offers it as an option,
`ui_menuDissolve 1` (archived, F.A.K.K.2 only). By default the menus fade over
the black this engine clears under them. With their white fills, a going menu
then turns grey, and the next comes up from it.

With the option on, fnTech3's frames still keep no image, so it holds the
kept image as shares of what was drawn into it:
- the black, or the game's view, the screen started from;
- each menu's fill and pictures.
A draw at alpha a leaves every share 1 - a of itself and adds a to its own
layer's. Each frame draws the shares back, lowest first, each at the alpha
that leaves it its share (`code/compat/alice_menu.hpp`; tests in
`tests/alice_menu_tests.cpp`). A gone menu's shares stay until others cover
them; its kept pictures fire none of its waits. With the option and a fixed
frame time, the main menu hazes as retail's does. Stalls in a locked Windows
session (see Open) kept fnTech3's fade-in from being sampled cleanly. Alice's
menus keep black under them, as before.

**The controls menu's bind list** is F.A.K.K.2's own `UIFakkBindList`. Alice's
draws two text columns, and fnTech3 had drawn F.A.K.K.2's that way.
`fakk2.exe` 1.02 builds it from labels and key buttons (children 0x004c0390,
Draw 0x004c1360):
- `ui/bind.scr`'s third words are pictures: the headers' `textures/bind/action`
  and so on, the rows' `textures/bind/leftattack` and so on, and the fill
  strips' `textures/menu/F_A`.
- The name, primary and alternate columns stand at 0, 144 and 224, with
  16-unit fill strips after each and the scrollbar at the list's right.
- Each cell draws its picture in its foreground colour: the highlight colours
  for the chosen row, and the select colours, or the active ones while it
  waits for a key, for the chosen cell. A waiting cell has a 2-unit border in
  `activebordercolor`, and `textures/menu/presskey` shows centred.
- A key cell shows `textures/bind/<key name>` if the paks hold it (MOUSE1,
  CTRL, the arrows, None...), else `emptykey` with the name printed over it,
  cut where it overruns (`MWHEELDOW`).
- The primary cell shows the first key bound to the command and the
  alternate the second (the window manager's key record, 0x004997a0,
  checked case-sensitively). The list starts on the primary cell.
- Keys (0x004c0de0): Enter waits for a key only on a key cell; Backspace and
  Delete clear that cell's key; Left and Right move between the key cells.
- Clicks: a name click chooses the row; a click on a key cell chooses it, and
  a second click on the chosen cell waits for a key (0x004bc140). While
  waiting, any mouse button binds, wherever the cursor is.
- The alternate cell binds as `BindKeyToCommand` does for it (0x00499cc0): its
  own key again changes nothing, the first key clears it, and any other key
  replaces it.

A probe opened the controls menu, pressed Right, Enter and K, and read `bind k`
as `+attackleft` with MOUSE1 still bound. Backspace then unbound K. The
captures match retail's controls menu cell for cell (`tests/fakk2_ui_source_tests.py`,
`tests/alice_menu_tests.cpp`).

**Scrollbars** are `fakk2.exe` 1.02's `UIVertScroll` (Draw 0x004b3ab0), since
F.A.K.K.2 ships none of Alice's `ui/control/slider2_*` art:
- The track is the widget's 0.75 grey inside a one-unit black edge (the
  constructor's black, 0x004b341d; the box 0x0049a650).
- The arrows are 16-unit buttons of the same with marlett's `5` and `6`
  (0x004b34e0). They are bevelled, light along the top and left and dark along
  the right and bottom, only while there is something to scroll
  (0x004b366c -> 0x0049a4b0). Their glyphs are white then, and in the grey's
  dark shade otherwise (0x004b355f..0x004b3588).
- The thumb is 0.5 grey (0x009b2a78), black-edged and bevelled in its own
  shades, sized and placed as before (0x004b36b0).

This profile had drawn them as Ritual's later UI library does, darker and without
the black edges. A retail capture's arrow button, pixel for pixel, gave the
layout.

## Presentation

The profile presents through fnTech3's own systems, fed F.A.K.K.2's data (AGENTS.md
invariant 11):

- **Rendering** is fnTech3's GL, Vulkan and RTX renderers with the Ritual
  shader dialect; the frame composition, 2D pictures and fonts are the engine
  UI's paths shared with Alice. All three ran the level-exit probe below, with
  captures that differ in under 0.3% of pixels. The RTX renderer ran its raster
  path: with
  `rtx_rt_mode 2` its gate still reported `active=disabled` on this machine's
  RTX 4060, so no ray-traced frame has been seen for this profile.
- **Audio** is fnTech3's shared OpenAL path through the Ritual sound imports,
  with fnTech3's generated room acoustics: every official map ships an audio
  zone file, and a static light file that feeds the renderers' static map
  lights ([sidecar record](2026-09-24-fakk2-map-sidecars.md)); `fakkhouse` loads
  343 zones and 128 lights. Their colours are balanced for the default
  per-pixel dynamic lights, which fall off far more gently than retail's
  inverse-square model-light law and stack eight lights on top of the baked
  lighting. Unbalanced, `towncenter_evil`'s opening averaged 123 with them and
  22 without, and `intro`'s metal debris drew white. Balanced, it averages 46,
  and the debris is textured metal.
- **Input** is fnTech3's input layer with Ritual's cursor mode, Alt and Ctrl
  tables and menu-bound function keys. A probe started with `+set
  in_synthetic 1` can also queue keys, characters and mouse motion into the
  engine's event queue with `in_synthesize`, under both Ritual profiles and
  Medal of Honor only.
  The cvar is command-line only, so no server, menu or config can turn it on.
- **Interface** is the retail `.urc` menus and inventory script read by the
  engine UI; nothing of FnQ3's or FnQL's own menus replaces them. The UI
  scales them to the screen, where retail draws them 1:1. The full-screen
  menus fade over black, as this engine's frame is cleared under them.
  Retail's dissolve over its uncleared screen is kept beside that as a
  retail-exact option, `ui_menuDissolve 1`, off by default (below).
- **Left out:** fnTech3's web UI and menu blur do not apply to the engine-owned
  Ritual UI; the FPS counter's renderer statistics lines are not kept.

## Compared with retail

Retail `fakk2.exe` 1.02 ran as a client, as described under Evidence and
provenance, from `.tmp/runtime/fakk2-retail` with its captures copied out.
It played an fnTech3 demo of `fakkhouse`: the opening, then a creeper spawned
beside Julie. Both engines captured the same demo with `cl_avidemo 5`, one
frame per 200 ms of demo time, which lines their sequences up one frame
apart. The two draw the same frames: the opening's camera path over Julie's
house, the fade into play, Julie, the Edenlander at the door, and the
creeper's bites and acid. Their mean difference, at 160x120, was 12/255 a
channel, and 2.5/255 on the fade. Retail's frames showed four differences,
and each was traced into its executable and corrected:

- **The weapon HUD with empty hands.** Retail shows `weaphud`'s two rings from
  the first frame of play, and its health bars with no delay after the
  letterbox. The rule is under HUD. fnTech3 now draws the same menus from the
  same frame, less the one frame retail's letterbox flag lags behind, which
  comes from where its UI update reads it.
- **Smoke from the chimneys.** Each `SFX_Smoke_Smokestack` is a hidden
  (`RF_DONTDRAW`) entity whose client-side emitter spawns `explsmokewhite.spr`
  puffs, and the puffs carry the hidden flag. fnTech3 dropped any sprite with
  that flag, as its Alice path does. `fakk2.exe`'s `RE_AddRefSpriteToScene`
  (0x0047b760) queues every sprite it is given, whatever its flags, and so
  does fnTech3's F.A.K.K.2 path now. The smoke draws from both stacks. The
  blood splat (`bloodsplat.spr`) under a bitten Julie now draws in the
  same frames as retail's.
- **The world's light.** The splat drew a darker red because the grass
  under it was darker. Set region by region against retail's frames of the
  same demo, fnTech3's grass and floor stood at 0.69 to 0.74 of retail's,
  its walls at 0.85, and its sky alike.
  - `fakk2.exe` 1.02 registers `r_overBrightBits` as `0` (0x00460d74) and
    `r_mapOverBrightBits` as `2` (0x00460ed4). This engine used one
    overbright bit for every profile.
  - 338 of the 365 `$lightmap` stages in F.A.K.K.2's scripts name no
    `rgbGen`. fnTech3 gave them Quake III's identity lighting, which each
    overbright bit halves. With `r_mapOverBrightBits 3` and one bit, the
    grass came out at retail's level while the walls went to 1.6 times it:
    those stages had lost one bit. (Retail gives them identity, as fnTech3
    now does, under Every level's first view below; with no overbright bits
    the two are the same colour.)
  - F.A.K.K.2 now defaults to no overbright bits in all three renderers
    (`tests/fakk2_overbright_tests.py`). The same frames then match
    retail's at 1.00 in every region on the GL renderer, and the splat's red
    is within 5% of retail's, inside the demo's frame alignment.
  - The Vulkan renderer's grass matches too, but it draws `fakkhouse`'s walls
    1.1 times retail's (0.85 before), with dynamic lights on or off. That is
    its bloom: the Vulkan renderer turns `r_bloom` (and `r_hdr`) on by default
    for every profile, where the GL renderer leaves them off. With `r_bloom 0`
    the same `fakkhouse` view on the Vulkan renderer matches the GL
    renderer's at 1.000 in every wall and ground region (the moving sky 0.985),
    so the two renderers draw F.A.K.K.2 alike and the bloom is a presentation
    default.
- **The retail wire**, above: retail refused the demos until the bodies and
  the time residual were F.A.K.K.2's.

**Saves across the two.** fnTech3 saved twice on `fakkhouse`. Its `save/config_fnsave*` files, copied into
the retail runtime, show in retail's load menu, from their `.ssv` headers:
"Julie's House" with each save's time and date, newest first, and the chosen
one's colour picture. fnTech3's list then differed in four ways, each read from
`fakk2.exe` and corrected (the save list row under Menu widgets): the map's name
where retail shows the title, oldest first, the chosen row yellow on blue, and
a font a size too large. Loading one of these saves in retail needs its server,
and so the CD.

A live session between the two, retail's client on fnTech3's server, and a
demo retail recorded there are described under Module ABI. They needed the
handshake corrected first.

The main menu, pushed in both at 800x600 with `pushmenu main`, has the same
content: the art, the logo, the six buttons, BACK TO GAME, the Ritual logo,
the cursor and the version label. `pushmenu` leaves BACK TO GAME alone. The
intro and Escape, reading `fakk2.exe`, disable it outside a game (Client). The
label read `1.00` here until `fakk2.exe`'s own `shortversion` was read; both
now read `1.02`. Two differences were found, one kept and one corrected:

- **Scale.** Retail draws the 640 by 480 menu centred at one pixel per unit,
  and fnTech3 scales it to the window. That is presentation, kept.
- **The buttons' halo**, now explained and corrected. Retail's NEW GAME
  carried a violet halo and fnTech3's a thin light edge. Three retail captures
  a quarter of a second apart showed the halo pulsing (blue 255, 137, 80 at
  one rim pixel) on NEW GAME alone. The other buttons' rims held steady.
  - A test menu drew `b_newgame` over flat colours in retail, as a Label,
    and as Buttons with and without a hover shader or `ordernumber`. All drew
    the thin rim of one straight-alpha draw except the Button with
    `ordernumber 1`, which glowed. NEW GAME's halo is its `_selected` shader:
    `b_newgame` once more, then `b_newgame2` added under `rgbGen wave`. Retail
    lights it because NEW GAME has the keys when the menu opens (Client).
  - fnTech3 drew a hover picture only under the mouse. It now opens the main
    menu with NEW GAME lit, pulsing, as retail does
    (`.tmp/fakk2/lit/newgame-open-compare.png`, not kept).
  - One difference is left, and kept as presentation. Retail's driver here
    stored this menu art at four bits a channel (`r_texturebits 0`; the texels
    arrive in steps of 17). That rounds the faintest rim texels' alpha up to
    1/15 and widens the glow a little. fnTech3 keeps the art's eight bits.

**Every level's first view** (2026-09-25, `.tmp/fakk2/sweep/frames.py`). On
all thirty maps, fnTech3's dedicated server started the map. Retail's client,
then fnTech3's, joined, skipped the opening and took a screenshot at 800x600.
The world, the HUD and the models are the same on every map; the two
cinematic maps, `intro` and `gruff_cinema`, were caught a moment apart. Three
differences were fnTech3's errors, and are corrected; tracing the second found
a fourth:

- **Static models in the world were black.** The map compiler turns a static
  model (`make_static`) into vertex-lit world triangles, which keep the
  model's shader. Many of those shaders say `rgbGen lightingDiffuse`:
  `training`'s balcony banners (`banner2`, `banner4`), `towncenter_good`'s
  ferns, plant leaves and grass, `oracle`'s green cloth (`statcloth`), lamps
  and tree tops. They are 55 surfaces on `fakkhouse` alone, and 745 over six
  Alice maps. `lightingDiffuse` lights by the current entity, and the world
  entity has none, so fnTech3 drew them black: `training`'s banner a black
  rectangle, `towncenter_good`'s ferns and `oracle`'s cloth lost against the
  dark behind them. Retail parses the keyword as its rgbGen 10 (0x00476551).
  Its colour step calls the lighting function the current entity chose
  (0x0047d45a). The world entity has no model to choose one with
  (0x00471639..0x00471653), so the call is skipped, and the stage keeps the
  colours last written. Those are usually the full-white colours of the
  lightmapped surfaces drawn before it. That is an accident of the draw
  order, not a rule. fnTech3 now lights these surfaces in all three renderers
  with the colours the compiler baked on their vertices, times the identity
  light, as it does Medal of Honor's static models
  (`RB_CalcDiffuseColor`, `tests/ritual_world_diffuse_tests.py`). Models keep
  their entity lighting. The banner, the ferns and the cloth draw where
  retail's do; Alice's `utemple` kelp draws lit.
- **Far-plane fog brightened distant walls.** Six maps (`fog`, `over`,
  `swamp1`, `swamp2`, `swamp3`, `under`) and two of Alice's (`centipede1`,
  `utemple`) fog toward a colour at a distance their worldspawn gives
  (`fog`: 2,000 units, 0.1 0.1 0.05, configstring 9 `1 2000 0.1000 0.1000
  0.0500`). fnTech3 fogged each stage with GL fog, and it had given a stage
  that modulates the one below it RTCW's white neutral. A texture over a
  lightmap therefore faded toward white rather than the fog, and on `fog` the
  central rock stood brighter and more purple with the fog on (62, 38, 72)
  than off (53, 26, 39), where retail's is dark.
  - Retail fogs such a surface once. FinishShader appends one pass to a copy
    of the stages (0x004792bc..0x0047937b): the fog colour, blended
    `GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA`, with alpha min(|v - eye| /
    distance, 1) at each vertex (0x004948a0, factors 0x00494910).
    RB_BeginSurface draws that copy, its stages unfogged (0x0047c5ee). Only a
    single-pass `rgbGen lightingDiffuse` skin is GL-fogged instead, to the
    same colour, when `r_useglfog` is on (0x004795c0..0x004795d7,
    0x0047c591..0x0047c5bb). `alice.exe` does the same (0x0049286e,
    0x0049ebb0), its flag also following the developer `r_toggle`, off by
    default (0x0048ee7c).
  - A first correction gave every stage the fog colour instead. It darkened
    the rock as retail's, but blackened `fog`'s hanging ivy (an alpha-tested
    texture, then its lightmap as a filter) and `swamp1`'s water (a lightmap,
    the water as a filter, an environment map added), where retail shows both
    lit through the haze.
  - Both Ritual profiles now draw an opaque surface's stages unfogged and the
    one pass over them, in all three renderers (`tr_ritual_far_fog.h`,
    `tests/ritual_far_fog_tests.py`). `fog`'s view then matches retail's
    region for region within 1% on the GL renderer (the pool 4% with the fog
    off; the rock 36, 25, 23 in both, the ivy dim with no black pixel).
    `swamp1`'s water, haze and ground are within 2%. On the Vulkan and RTX
    renderers `fog`'s rock, ivy and cave walls match as closely, and the
    ground by the glowing items is 1.4 times retail's: their default bloom,
    1.07 on the Vulkan renderer with `r_bloom 0`. Alice's `centipede1` and
    `utemple` first views moved by 0.5 of 255 on average.
  - A shader whose first stage is blended keeps the per-stage fog, which fades
    an additive stage to nothing and a filter to white. Retail fades the
    blends it can (0x00478c3c..0x00478df6) and draws the pass over the rest
    (0x00479494..0x004794af), so its distant filter decals show the fog across
    their polygons. The pass keeps to the surface's own pixels by depth, as
    Quake III's fog pass does, where retail takes the last stage's depth test
    and so fogs the cut-out parts of a lone alpha-tested stage. Medal of Honor
    shares the far-plane view and keeps RTCW's per-stage rule.
- **A black fog volume darkened `over` and `under`.** Both maps put a
  `textures/we_cemetary/blackfog` volume (`fogParms 0 0 0 256`) around the
  start, with the camera inside it. fnTech3 drew it as well as the far fog:
  `over`'s tunnel stood at half of retail's (its lightmaps alone at 0.49),
  the room beyond the volume matched, and Julie inside it was nearly black.
  While the far fog is on, retail draws no fog volume: RB_BeginSurface gives
  every 3D surface but an uncut sky the far fog's own number in place of its
  fog (0x0047c553..0x0047c583; `alice.exe` 0x004927d3..0x004927ff). Both
  Ritual profiles now do the same (`RB_RitualSurfaceFogNum`). `over`'s view
  then shows Julie lit, as retail does, and its walls at 0.94 to 1.12 of
  retail's with fnTech3's static lights off, 1.2 to 1.4 with them on (its own
  lighting). With `r_farplane_nofog 1` retail's tunnel is 0.8 of fnTech3's
  volume-free frame, which fits retail drawing the volume once the far fog
  is off [I]; fnTech3 draws it then, darker. Only `over` and `under` combine
  a far plane with fog volumes; `cemetery` (9 volumes), `cliff1`, `cliff2`
  and `oracleway` have volumes and no far plane.
- **Retail's default colour for a lightmap stage** (found tracing the fog). A
  stage that names no `rgbGen` and ends on a first bundle of `$lightmap`
  takes identity (0x00476e8c..0x00476ebc), where Quake III's rule, which
  fnTech3 applied, gives an unblended stage identity lighting. Ritual numbers
  identity 1 and identity lighting 2 (0x00476454, 0x00476475) and collapses
  two stages only when their colours match (FinishShader 0x00478fa8). So
  retail's `textures/fog/wl_swamprock` (`$lightmap`, then its texture with
  `GL_DST_COLOR GL_ZERO`) is one multitexture pass where fnTech3's GL renderer
  drew two (`shaderlist`: `2 L E`). F.A.K.K.2's stages now take retail's
  default in all three parsers (`R_RitualLightmapStageIdentity`,
  `tests/alice_shader_parse_tests.py`). With no overbright bits the two
  colours are the same. `alice.exe` has the same rule (0x0048c648..
  0x0048c678); Alice takes it with its own overbright defaults, a separate
  change.
- Presentation, and kept: fnTech3's curves are finer (`blood`'s thorns are
  smooth where retail's are angular; `r_subdivisions`, under the defaults
  above), and models are lit by fnTech3's own lighting, so Julie is darker
  or lighter than retail's from map to map. The trainer on `training` had
  drawn blue-grey beside the blue wall, where retail shows skin tones and
  blonde hair. She has her own skins (`models/npc/edenfemale/random1_1` and
  `_2`, loaded as the implicit lit shaders), so the tint was the lighting.
  Settled by the compiled model lights below: on 2026-10-24 she averaged
  60.8, 63.1, 64.0 beside retail's 55.8, 58.4, 59.5, with the same tones and
  hair ([playability round](2026-10-24-fakk2-playability-round.md#the-trainer-on-training)).
- Settled: models were too dark where a map's light grid is dark. On `water`
  and `otto` fnTech3 drew Julie nearly black where retail lights her; on
  `water`, where the dedicated server starts the player, the light grid is
  black (ambient 0, directed at most 4). Retail never lights a character or
  weapon from the grid: the game module flags them `RF_EXTRALIGHT`
  (gamex86 0x100fc040, 0x1012a6d2), and `fakk2.exe` lights them from the
  map's compiled lights, |intensity| x 7500 / d^2 within each leaf's list,
  shifted by the map's two overbright bits (0x0048e170, 0x0048dd80,
  0x0048e090). fnTech3 now does the same in all three renderers, for this
  profile and Alice's, with no change to the static-light files or their
  cap. Julie on `water` went from a third of retail's brightness to within
  7 to 9% of it; on `over` she is 17% above it
  ([model light record](2026-09-25-ritual-model-lights.md)).
- Settled on 2026-10-23: the canal water on `over` and `under` is lime green
  in retail and was a bluer green in fnTech3. Its last stage adds 47 caustics
  frames (`textures/sprites/caustics/caus*.ftx`, blue, average 3, 22, 47)
  tinted `rgbGen constant 0.5 0.8 0.5` with `GL_ONE GL_ONE`. Laid over retail
  and fnTech3 alike as a generated `pak9.pk3`, black frames take away (0.8,
  16.7, 0) of retail's water and white frames add (101.6, 159.5, 0), where
  fnTech3 added the constant itself (127, 200, 127). Retail parses the
  constant with its blue (0x004765c0..0x0047660f) and copies all four bytes
  to each vertex (0x004940c0), and its alpha step leaves the colour alone
  (0x0047d716); the blue is lost before the draw, when FinishShader folds the
  lightmap and water stages and writes their combined colour one stage and
  one byte on at a time (0x00479112), so the caustics stage's blue takes the
  fold's 0. fnTech3 now leaves the colours where that fold does
  ([gap round](2026-10-23-fakk2-gap-round.md#canal-water-and-the-stage-fold)).
  fnTech3 had also kept 24 of the 47 frames, so its caustics cycled in half
  the time; it now keeps retail's 64 (`MAX_IMAGE_ANIMATIONS`; 0x00476e00,
  `alice.exe` 0x0048c5bc, `tests/shader_animation_tests.py`).
- Settled on 2026-10-24: on `creeperpens`, the pile beside the green glowing
  lumps (`obj_poopile.tik`, surfaces `material1` `pooshine` and `material2`
  `cartasteroid`) is dark green in retail and was plain brown in fnTech3.
  `pooshine` is its brown texture (`pooskin`, average 67, 43, 15) under
  `rgbGen lightingDiffuse` and an environment map; the light grid there is
  neutral to warm, and the nearest compiled lights are yellowish, so the
  green is not from either. It is the level's `haypoo` light, the client
  game's green additive light inside the pile, which retail puts on the pile
  by its rule for every dynamic light (0x0048f23d..0x0048f395) and no
  per-pixel light can from inside. fnTech3 now uses that rule for a light
  the model holds, and the pile is green
  ([playability round](2026-10-24-fakk2-playability-round.md#the-dung-pile-on-creeperpens)).
  The light is additive, which retail adds to the world whatever the
  surface; fnTech3's per-pixel pass now lights the world from white for such
  a light, and the wall behind the pile takes its green glow
  ([playability round](2026-10-24-fakk2-playability-round.md#additive-lights-on-the-world)).

## Observed at run time

On a developer build (`meson/build/win32-fakk2`, Windows x86, debugoptimized),
every run was windowed at 800x600 on a private desktop, with an isolated home
path. Frames were taken with the engine's own `screenshotPNG`, and the
laid-down installation stayed unchanged (each probe's manifest records it).
The user's keyboard, mouse and desktop were never touched. Where a run below
presses keys or buttons, it queued them inside the engine with `in_synthesize`
(`scripts/probe_ritual_client.py --actions`):

- A dedicated server spawns every one of the thirty maps through the retail
  game module, with all 105 imports served, and quits cleanly.
- A client enters every one of the thirty maps from `devmap`, reaches its first
  snapshot, and draws the level's opening; each run quits cleanly with no error.
  `oracle` did so only after the fragment-offset fix above.
- Movement, through the module's own movement code: `script $player jumpxy 400
  0 300` twice carries Julie about 630 units along `fakkhouse`'s courtyard, from
  (3160, -1176) to (3023, -560), and she lands back at ground height.
- Combat, both ways. `actor creeper.tik` spawns a creeper. Left alone, it
  knocks Julie down and kills her: health 100, 88.8, then 0, with a knockdown
  animation and the `failure` mood. Julie's own attacks are driven through her
  torso state machine from the game script, since the console refuses
  `forcetorsostate` ("Command not available from console"). With the creeper
  placed in front, the handgun (`ATTACK_RIGHT`) takes it from 200 to 150 over
  ten shots, 10 a hit. The sword's combo (`SWORD_COMBO_ATTACK1` to `3`) takes it
  from 200 to 160 in three swings. Before the trace fix below, four
  point-blank shots left it at 200.
- Those ran from the module's developer commands. Play by keys and mouse
  follows below.
- After the trace fix the thirty-map sweep enters every map again with no
  error. It prints four more developer "stuck in world" lines than before
  (35, against 31). Each names an actor or item that spawns overlapping a solid
  entity, which the old trace could not see. On `oracle`, Tyler stands inside
  a heart model at scale 5; on `otto`, a zealot stands on a pod; on
  `homes2good`, a child stands under the `kid_watch_object` brush; and on
  `cliff1` the plasma ammunition lies inside a grawlix. Ritual's SDK source
  (reference only) is consistent with all four. There, script models and
  objects stay solid until spawnflag 1 clears that after spawning, an actor
  checks its spawn against solid contents, and an item drops with bodies in
  its clip mask. Alice's `garden1` and `potears1` print the same five lines
  as before the fix.
- Audio, checked from the engine's own capture rather than by ear, since a
  client on a private desktop mixes into a silent buffer.
  `scripts/probe_ritual_audio.py` records a demo on `fakkhouse`: the opening,
  then a creeper spawned beside Julie that kills her. It replays the demo
  through the legacy mixer with `video`, which writes the mix beside the
  frames. The 19.9-second track (22,050 Hz, stereo) never clips: it peaks at
  0.81, and its 100 ms levels run from -28.7 to -14.6 dBFS. Each effect the
  playback started (`s_show_sounds`, retail's switch) is then looked for in the
  track, using its own WAV from the paks. The creeper's seven slashes, its two
  attacks and Julie's pain cry are each found at the time the log gives, with
  a normalised cross-correlation of 0.63 to 0.68. Elsewhere in the track the
  same sounds reach at most 0.06 to 0.11 (99th percentile). The three effects
  also agree on a single offset between the log's server times and the track.
  A match needs the sound at its own pitch and speed. Distant sounds, such as
  the shgliek and the weed mixer, don't stand out from the level's loops.
  Replaying with `s_musicVolume 0` takes 3.3 dB out of the track: that is the
  soundtrack's share, which changed mood four times during the replay. On the
  default OpenAL backend, the same replay's `s_alDebugDump` lists the
  creeper's sounds and the acid hits at gain 1.0 with reverb sends of 0.11 to
  0.15. The occluded loops show occlusion of 0.84 to 0.89, and 343 zones are
  loaded with the outdoors-to-room blend active.
- Death: after `kill` the view drops to the overhead death camera, the health
  bar empties, blood splats, and the soundtrack turns to its `failure` mood.
  A single-player respawn, which a button press sends in retail, has the
  module send `forcemenu main; forcemenu loadsave` (`fakk/gamex86.dll`
  0x100c268b). Posted from the game script (`script $player respawn`), it
  brings up the main menu with the Load/Save menu over it.
- New Game's own path: `map intro` plays the intro's cinematic, and
  `skipcinematic` runs its skip thread's `map fakkhouse`. The module turns that
  into `gamemap fakkhouse`, and the first level loads and autosaves
  `fakkhouse0000`.
- The client's main menu draws from the retail `.urc` files over the menu
  soundtrack: Julie, the logo, the sky art and its clouds, and the six buttons.
- `devmap fakkhouse` loads the retail client game, plays the opening camera over
  Julie's house under its sky portal, with its actors, and quits cleanly; after
  `skipcinematic` the third-person view plays.
- In play the health and water bars draw inside their frame; `wuss` and
  `use "GreenSword" right` bring up `weaphud` with the sword's model in the
  right hand's window, `use "Rocket Launcher" dual` swaps in `dualweaphud` with
  the launcher's and its ammunition arcs, and `set bosshealth 0.6` slides the boss
  bar in at the left. `warpinv swords` opens the column at the right edge, 96
  down, with the swords' list and lit models to its left; `nextinv` moves the
  pick.
- A level exit without input: after `wuss` and `use "GreenSword" right` on
  `fakkhouse`, `script trigger $training_trigger` fires the exit to `training`.
  The module reports that `fakkhouse.scr` has no `LevelComplete` label to run,
  then fades the sound. The report gives the script line as -1431655766
  (`0xAAAAAAAA`), a field the module never sets, read from memory the engine's
  zone filled when it was freed. Retail's `Malloc` import (`alice.exe`
  0x00454420, `Z_TagMalloc` 0x00444fa0) hands out unfilled CRT memory, so
  retail's number would be whatever that memory held. A second later it sends
  `gamemap "training$spawn_fakkhouse"`, and the engine archives the persistent
  data to `save/config_current.spv`. The new level's module reads it back
  (2,155 bytes), `music/training.mus` plays, and the autosave writes
  `training0000`'s `.sav`, `.spv`, `.ssv` and `.tga`. `saveweaps 9` then gives
  `use "GreenSword" right`: the sword stayed in the right hand. With sound on,
  the new level loads 512 audio zones from `maps/training.azb` and 128 static
  lights from `maps/training.lights.json`. The probe's remaining steps ran from
  `activeAction`, because the module's own commands queue behind any step still
  waiting in the buffer. The same run on the Vulkan and RTX renderers took the
  same exit and kept the sword. Its two captures differ from GL's in under 0.3%
  of pixels by more than 24 levels.
- `saveweaps 3` with the launcher in hand makes `weapgroup3 "use "Rocket
  Launcher" dual"`; the configuration written at quit ends with
  `alias weapgroup3 use "Rocket Launcher" dual`, and executing that line in a new
  run gives the same alias back.
- A German installation, laid down from the same disc with its `deu` pak, draws
  its own main menu ("NEUES SPIEL" to "BEENDEN"), options, advanced options
  ("Texturfilter" reading "Trilinear", "Schatten" reading "Einfach") and
  controls menus, runs `localized.cfg`, plays `fakkhouse` with the HUD, and
  `play sound/dialog/misc/julie/sword.mp3` starts its German line. In both
  languages the advanced options' speaker-type and sound-quality labels are
  blank and the 8-bit box changes nothing. They name the Miles settings
  (`s_speaker_type`, `s_khz`, `s_loadas8bit`), which fnTech3's OpenAL path
  does not register, as for Alice, whose retail 8-bit, 22 kHz output is not
  emulated either.
- The Load/Save menu lists the saves in the home, oldest first, with their
  map, level time and date, and the newest chosen with its shot beside it. It
  had listed nothing: this engine drew the list for neither title, and only
  Alice's menu hides it. Loading `training0000` in a fresh process brings the
  level back with the sword still in the right hand. The shots were made as
  `alice.exe` makes them (0x00474000): the brightest channel, times 2.55, in
  grey. F.A.K.K.2's bright rooms came out almost white, a mean of 233.
  `fakk2.exe` 1.02 makes them another way (0x0045ffa0, from `screenshot
  <path> <width> <height>` at 0x004603a4). It reads the frame and takes the
  same twelve samples, but keeps each channel's own average, stored blue,
  green, red (0x0046014f..0x00460189): a colour picture with no gain. Then it
  applies its hardware gamma table when overbright bits are on, which
  `R_SetColorMappings` allows only in a fullscreen mode (0x0046ff7f..0x0046ffa2).
  All three renderers now make F.A.K.K.2's that way, with the gamma their
  screenshots keep (`code/compat/alice_saveshot.h`,
  `tests/fakk2_saveshot_tests.py`). The `fakkhouse` shot is now the
  view-only frame in colour, a mean of (81, 81, 73). The Load/Save menu shows
  it in colour.
- The options menu shows its pulldown arrows and its linked labels as words
  ("Default", "High"); the advanced options' pulldowns read "Trilinear" and
  "Simple"; the sliders show their arrows and their thumbs where retail's do;
  the controls' bind list has its scrollbar; the graphics menu shows the
  renderer report, its last line cut at the box's edge.

### Played by input

These runs drove the menus and play with synthetic keys, characters and
motion, with sound initialised but muted (the window is never focused) and
`s_show_sounds` naming each sound that starts. How the probe's input reaches
the engine shaped two of its rules, both in `probe_ritual_client.py`:

- The engine runs its event loop a second time just before the client frame.
  So a console line queued in the same frame as a button's release was
  executed before the menus saw the release. The probe therefore waits 100 ms
  after each key or button before chaining its next phase.
- The event queue merges mouse motion queued back to back
  (`code/qcommon/common.c`). So the probe's move to an absolute point waits
  a frame between the move to the corner and the move out.

- **Menus by the mouse.** On the main menu, hovering NEW GAME, CONTROLS and
  VIDEO\AUDIO starts each button's `hoversound` alias
  (`sound/dialog/menu/newgame.mp3`, `controls.mp3`, `videoaudio.mp3`). Clicks
  play `select.wav` where a button names it, and run their commands: VIDEO\AUDIO
  opens the options, ADVANCED the advanced options, GFX Info the graphics
  report, and each BACK returns. The highlight on a hovered button pulses,
  because the `_selected` shader's second pass is `rgbGen wave sin .5 .5 0 1`.
  A capture at the wave's trough shows none.
- **The pulldown's list.** A press on the texture-detail arrow opens its list
  under the arrow. Dragging onto "Low" highlights that row, and letting go runs
  `set r_picmip 2`: the cvar is latched at 2 and the label reads "Low". The
  options' BACK (`Popmenu 1`) puts the cvar back as the menu found it.
- **The text box's selection.** On the graphics report, a drag from the top
  left selects six lines, white on dark blue. Shift with Right, Right and Down
  then extends the selection by a line.
- **The save list.** Escape, LOAD\SAVE and SAVE save and close the menus, as
  the save event does. The first Escape instead skipped `fakkhouse`'s opening
  camera, which is retail's Escape rule. The list then shows the saves, the
  newest chosen:
  - A click on a row chooses it. A click below the rows changes nothing.
  - Up, Home and End move the choice.
  - Delete removes the chosen save's files, and the list follows.
  - Enter loads `fakkhouse0000`, and the level comes back.
- **The inventory's cursor mode.** I (`inventory`) opens the column at the
  right edge with the swords' list and warps the cursor onto the first type.
  The change sound plays at each step:
  - Hovering over the column lights the current type.
  - A press on the second type shows the defense list. A second press picks
    its first item. Moving down picks the next.
  - A click on the list uses the picked item in the left hand: the select
    sound plays and the sling appears in `dualweaphud`.
  - Up, Right, Down and Left then step the picks as the key rules say.
  - Each key taken adds five seconds before the list closes (0x00409ae4).
- **Play by keys and mouse** on `fakkhouse`, after Escape skips the opening
  camera. Positions are the game module's `whatis 0`:
  - W (`+forward`) for 1.5 s carries Julie 148 units along her 135° heading.
  - 400 counts of mouse motion turn her 44°, to 91°.
  - A strafes her until the house wall stops her.
  - Space jumps her. 1.35 s later she stands 88 units higher beside the wall,
    and walking back brings her down to the ground again.
  - After `wuss`, `use "GreenSword" right` and `use "HandGun" left`, MOUSE1
    (`+attackleft`) fires the handgun twice (`gun/fire.wav`, then the casings).
    CTRL (`+attackright`) swings the sword (`sword/fast1.wav`).
  - S with mouse motion walks her back while she turns.

## Open

Retail `fakk2.exe` 1.02 has played fnTech3's demos, drawn frames beside
fnTech3's, played on fnTech3's server and recorded a demo there that fnTech3
plays as retail does.

**Retail as the server** (2026-09-25, with the disc image mounted and the
machine key set, as under Evidence and provenance; `.tmp/fakk2/live/live_retail_server.py`):
- Retail ran `fakkhouse` as a listen server and as a dedicated server bound to
  the loopback address. fnTech3's client joined the dedicated one: challenge,
  connect, gamestate, primed, then active. It played the opening cinematic
  under its letterbox and, after Escape, played with its HUD.
- Holding W for 1.5 s took Julie from (3160, -1176) to (3055.09, -1071.10)
  by retail's `whatis 0`, where fnTech3's own server takes her to
  (3055.08, -1071.09). `wuss` and `use "GreenSword" right` armed her, and
  CTRL swung the sword with its trail.
- Retail's CD-key authorization lookup failed to resolve on the loopback
  address and timed out ("authorize server timed out"); the client was let in.

**Saves across the two, both ways.** Retail saved `fakkhouse` at the opening
and armed with the sword in the right hand (`savegame rtsave1`, `rtarmed`).
fnTech3 loaded both: the same view and brightness as retail's own reload
(mean RGB 95, 88, 75 against 95, 89, 77), and the armed one with the sword in
her right hand and on the weapon HUD. Retail loaded fnTech3's saves of the
same two states (`fnsave3`, `fnarmed`) the same way.

These are single sessions on one level. Retail's server has not run the other
levels, and retail's saves in later levels, with their persistent variables,
have not crossed.

**Retail as the client on every level** (2026-09-25, no disc needed;
`.tmp/fakk2/sweep/sweep.py`). For each of the thirty maps, fnTech3's dedicated
server started the map on the loopback address and retail `fakk2.exe` joined:
connect, gamestate, primed, active, then the module's `ClientBegin`. Thirty
seconds later its `whatis 0` came back with Julie's origin from the module,
and it quit. Every map got that far. Retail's only complaints were on `zoo`,
about files its own paks do not ship (`textures/sky/cliffsky.tga`,
`models/item_healthmush_glowing.tik`). One server faulted, after the client
had left. On `creeperpens` the retail game module, running frames with no
player, posts `stopwatchingactor` to a player object that is NULL: a member
at +0xac of its caller (`gamex86.dll` 0x10038310, the post at 0x10038437).
It faults in `Listener::PostEvent` (`gamex86.dll` 0x100a2799), reached from
`RunFrame`, as cdb showed. Retail's own `SV_Frame` also runs the game while nobody is
connected (`RunFrame` at 0x004477af, with no empty-server exit before it), so
retail's dedicated server is inferred to fault the same way **[I]**. The
event names come from their definitions in the module (0x100bb976,
0x100bb916). Checking it needs retail's server, and so the disc.

A client that leaves is announced as both Ritual engines announce it,
`print "%s %s\n"` with no colour code after the name, and the client that
left is sent a bare `disconnect` (0x00443abb, 0x00443b2b; see the
[drop notice record](2026-09-24-rtcw-et-drop-notices.md)). With retail
`fakk2.exe` and fnTech3's client both on an fnTech3 server, fnTech3's client
quit and retail printed `fnprobe disconnected`.

One [I] inference is marked: that retail's own dedicated server faults on
`creeperpens` once its player has left, as above. The last of the earlier
ones, that retail cannot reach the German pak's stray lip-sync folder, was
settled by the lip loader's own path rule and a test pak in the retail
runtime (see Languages). The others have been checked against `fakk2.exe`
1.02 and cited from it.

During the menu-fade probes, fnTech3's GL renderer stalled for about 450 ms
roughly once a second, inside the renderer's frame start. Another session
traced this to the Windows session being locked, not to the private desktop:
unlocked probes run a steady 60 frames a second, and `r_drawBuffer GL_FRONT`
avoids the stall. The stalls let short waits pass together, so the fade
captures of fnTech3 were not spaced as retail's are; `fixedtime` did not hide
them.

The user listened to the engine's own mix of the `fakkhouse` demo
(`.tmp/fakk2/audio/fakkhouse-engine-mix.wav`, recorded by `video` through
fnTech3's audio) on 2026-09-25 and reported that it sounds right. That is one
demo's mix, and `video` records the legacy mixer's: the default OpenAL path,
with its EFX environments and audio zones, was not in it. Other levels' rooms
have not been listened to. The OpenAL mix cannot be recorded from a probe
here: a client on a private desktop sets its listener's gain to 0, so even
OpenAL Soft's wave writer records silence, and a window on the visible
desktop could take the user's keyboard focus.

The release gate stays closed.
