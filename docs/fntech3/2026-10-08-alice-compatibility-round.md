# Alice compatibility round

## Scope

This round took every open item in the Alice gate matrix and in the Alice
records, and either closed it with evidence, recorded why retail does the same
thing, or recorded why it cannot be closed here. The module signatures are a
record of their own ([2026-10-08-alice-module-signatures.md](2026-10-08-alice-module-signatures.md));
everything else is below. The machine-readable companion is
[2026-10-08-alice-compatibility-round.json](2026-10-08-alice-compatibility-round.json).

Retail `alice.exe` is wrapped in SafeDisc and cannot run on this machine, so
nothing here sets fnTech3 beside a running retail Alice. Its behaviour is read
from `alice.exe` itself (SHA-256 `8aa22917...`), never run; F.A.K.K.2's
patched `fakk2.exe` 1.02 (SHA-256 `681bc51c...`), which does run, is the
live retail peer where the two engines share the code. Every Alice gate stays
partial and the release gate stays closed.

## The runs

Every runtime result below was taken again on one build at the end of the
round, with `scripts/probe_ritual_client.py` (and, for the retail peer and
the debugger runs, the dedicated server):

- **Build:** a `git archive` of `e53ba098` with this round's changes laid
  over it, Windows x86, MSVC, configured like `win32-dev` with every
  development option; `fntech3.exe` SHA-256 `d6754d7c...`, `fntech3.ded.exe`
  `ae75e304...`, `fnql_glx_x86.dll` `ca205c70...`. The isolated full-suite
  export used `fab8e1a0`; the three intervening commits change only paths
  gated to Elite Force. This is a tested base, not a claim that the round
  has been committed.
- **Installations:** the retail Alice installation (`base/pak0.pk3` as the
  [contracts record](2026-09-22-alice-contracts.md) pins it) and the
  laid-down F.A.K.K.2 game under `.tmp/fakk2-install/game`; each probe's
  manifest records the installation unchanged.
- **Conditions:** windowed at 800x600 on a private desktop, an isolated home
  under `.tmp/alice-gaps/final/<run>`, no mouse or keyboard input, frames
  from the engine's `screenshotPNG`, `developer 1` unless a run says
  otherwise.

All twenty client runs exited by themselves with code 0, found every log line
they expected and none they refused; the JSON lists each run's command,
captures and lines.

The isolated full Python suite's previously unreported result is:
**2,639 passed, 129 skipped, 65,346 subtests passed**, with no failures
(`.tmp/alice-gaps/suite.log`, completed 2026-10-09). The signature-only
export also passed its 84 relevant Python test files (615 tests) in the
Visual Studio x86 developer shell. These checks do not replace the live
compatibility limits below.

## A load or a restart of the running level

**Observed.** Retail's `SV_SpawnServer` has a reuse path
(`0x004550a1..0x004550df`). It is taken when the game module is loaded, the
BSP is the one the server last spawned (`0x00b59b80`, written at
`0x00455130`) and the spawn was asked for by `loadgame` or `loadbrokengame`
(`SV_SpawnServer(name, 1, 0)`) or by `restart` (`(name, 0, 1)`); `map`
always takes the full path. On the reuse path the module stays loaded and is
asked to `Cleanup` (`0x004551cf`); the map, the configstrings, the model table,
the baselines and the server state are kept (`0x004551d8` jumps past
`0x004551de..0x0045523b`); a new server id is made and the old one becomes the
restarted id (`0x004553cf..0x004553e0`); the snapshot's server-count bit
flips, the area portals close and the world is cleared
(`0x00455240..0x00455285`); the level is read back or spawned afresh with
configstring changes reaching the clients as they are made (`sv.restarting`,
`0x0045528a..0x004552b1`); and each connected client is admitted again without
a new gamestate and enters the world at once (`0x004554eb..0x0045557c`).
F.A.K.K.2 1.02 makes the same choice (`0x00445f41..0x00445fb3`). Every spawn,
reuse or full, also clears each loaded model's per-actor dialog history and
re-sorts its alias list (`0x00462460` from `0x00455285`, `0x004229b0`).

The client keeps its client game and puts up the loading screen
(`CL_MapLoading(0)` at `0x0045513f`). `CL_SetCGameTime` tests each frame's
snapshot for a changed server-count bit before its time
(`0x00424b9f..0x00424bf1`), and on a change `0x0042d890` stops every sound but
the music (`S_StopAllSounds(0)`), ends the loading screen and releases a menu's
pause (`UI_EndLoad`, `0x0040ecf0`), cancels the sound fade (`0x0040eed0`) and
clears the view angles (`0x00c36428..0x00c36430`); a server time behind the
last frame's is then allowed, which only such a spawn can send.

**Changed.** fnTech3 had reloaded the module on every spawn and kept the
configstrings across a load of the running level as a stand-in. It now takes
the reuse path itself (`SV_AliceSpawn`, `SV_AliceReuseSpawn`,
`code/server/server_alice_save.hpp`), clears the dialog actors at every spawn
(`AliceTiki_ResetDialogActors`), and its client answers a changed server count
as retail's does (`CL_AliceSnapshotServerCount`, `CL_AliceMapLoadingKeep`).
The configstring stand-in is gone.

**Measured.** `reuse` (`garden1`): a save, `loadgame t1` and `restart`; the
log shows `Alice keeps its game module for garden1` for both, `Alice level
read back from t1 at level time 12300`, and `Alice: server count changed` on
the client each time. `reuse-fakk2` (`fakkhouse`) shows the same under the
F.A.K.K.2 profile. `death`, `pure1` and `pure-fakk2` load the running level
through the same path.

## Configstrings and the map checksum

- **The players start at configstring 896** (`0x380`): the game module writes
  a client's name there plus its number (`fgamex86.dll` `0x100a5556`, before
  `setConfigstring` at `0x100a555e`), where the client game's 32 light styles
  end (`cgamex86.dll` `0x30023da2`), and its 128 reach the item block at
  `0x400`. The profile carried 0 for this; it now carries 896
  (`code/qcommon/game_profile.cpp`, `tests/game_profile_tests.cpp`).
- **The map checksum is the header's.** Ritual's `CM_LoadMap` returns the
  checksum the BSP header carries after its version and computes none
  (`alice.exe` `0x00430a1a..0x00430a20`, `fakk2.exe` 1.02
  `0x0042640a..0x00426410`). fnTech3 had computed a block checksum of the file;
  for a FAKK map it now returns the header's (`code/qcommon/cm_load.c`), which
  is what `sv_mapChecksum` publishes.

## Pure servers

**Observed.** `SV_Init` registers `sv_pure` at `"0"` in the systeminfo
(`0x004556d7..0x004556e8`), which corrects the contracts record. A pure server
publishes its paks' checksums as `sv_paks`, `%i ` each (`0x0043e3c0`,
`0x00455455..0x00455472`). The client applies that list whenever no demo
plays, its own local server included (`0x0042df09..0x0042df19`), keeping the
first 64 (`0x0043e426`; `fakk2.exe` 1.02 `0x00432516`). Under a list its file
open skips every pak not on it and every loose file but configuration, the
savegames and their pictures: a name ending `.cfg` or `.dat` or whose last
three letters are `ssv`, `sav`, `spv` or `tga` (`0x0043bffe..0x0043c17b`;
F.A.K.K.2 the same list at `0x00430614..0x0043072e`). There is no pure proof:
neither engine has a `cp` command, and the server's client-command table holds
only `userinfo` and `disconnect` ([wire record](2026-09-22-alice-wire.md)).

**Changed.** Under both Ritual profiles the client applies `sv_paks` from its
own server, keeps 64, and serves loose files by retail's list while a list is
in force (`code/client/cl_parse.cpp`, `code/qcommon/files.c`); it sends no
`cp`, and the server asks for none (`code/client/cl_main.cpp`,
`code/server/sv_client.cpp`).

**Measured.** With a copy of a retail map laid loose in the home as
`maps/zzpure.bsp`: `pure0` loads it (`Server: zzpure`); `pure1` refuses it
(`Can't find map maps/zzpure.bsp`) and reports `Connected to a pure server.`;
`pure-fakk2` publishes its two paks
(`\cheats\1\sv_paks\1638961842 990210246\sv_pure\1\...`). All three load a
save back.

## Infostrings

**Observed.** Alice's and F.A.K.K.2 1.02's `SV_Init` give the serverinfo
`dmflags`, `fraglimit`, `timelimit`, `g_gametype`, `sv_keywords`, `protocol`,
`mapname`, `sv_privateClients`, `sv_hostname`, `sv_maxclients`, `sv_maxRate`
and `sv_floodProtect` (`alice.exe` `0x004555b5..0x004556ab`), besides
`com_blood`, `fs_game` and `version` from elsewhere; their systeminfo is
`cheats`, `sv_serverid`, `sv_pure`, `sv_paks` and `timescale` (Cvar_Init
`0x0043b4f5`, `0x004556c1..0x004556fe`; F.A.K.K.2 `0x00446471..0x004465ae`).
`sv_allowdownload` is registered with no flag at all (`0x0045579e`). Their
clients read `sv_serverid`, then `cheats` -- clearing the cheat cvars when it
is 0 -- then `sv_paks`, and set every systeminfo pair as a cvar
(`alice.exe` `0x0042deb0`, `fakk2.exe` 1.02 `0x00423aa0`).

**Changed.** fnTech3 had published its own `sv_cheats`, `sv_referencedPaks`
and `sv_referencedPakNames` in the systeminfo and `sv_allowDownload`,
`sv_dlRate`, `sv_minRate` and `sv_playingDemo` in the serverinfo, so a retail
client would have created them as cvars and a retail server's `cheats` would
not have reached fnTech3's client. Under the Ritual profiles those keys stay
local (`SV_RitualInfoKeys`, `code/server/sv_init.cpp`) and the client takes
the cheat state from `cheats` (`code/client/cl_parse.cpp`).

**Measured.** `infostrings` prints a systeminfo of exactly `cheats`,
`sv_pure`, `sv_serverid` and `timescale` (`sv_paks` is empty and so left out)
and a serverinfo without fnTech3's keys; every later systeminfo in the sweep's
logs reads `\cheats\1\sv_pure\0\sv_serverid\...\timescale\1`. Retail
`fakk2.exe` 1.02 joined the export's F.A.K.K.2 dedicated server over loopback
(`fakk2-live`): challenge, connect, gamestate, `CS_ACTIVE` and the module's
`ClientBegin`, in play on `fakkhouse` with its HUD; its own `cvarlist` holds
`sv_serverid "325"` from the server and none of fnTech3's keys.

## `CalcCRC`

The module computes a CRC in one routine (`fgamex86.dll` `0x100b1f20`, the
call at `0x100b201a`), which loads a script file and keeps the CRC of its
text. It is entered from three places (`0x100b1619` in `0x100b12e0`,
`0x100b2a2c`, `0x100b30aa`). The level's own scripts take another route: the
loader at `0x101761c0`, which prints `Adding script` and computes no CRC.

Under cdb, with breakpoints on that routine, on each of the three entries, on
`0x101761c0` and on fnTech3's `SV_AliceCalcCRC`, a dedicated server ran
`devmap garden1` and a change to `garden2` (`final/crc`), and in a second run
loaded a save made on `garden1` (`final/crc2`, `Alice level read back from g1
at level time 11750`). Only `0x101761c0` was hit, once per spawn; nothing
reached the CRC routine. So `CalcCRC` is served and tested, but no fresh level,
level change or save load of the shipped game reaches it. What would reach the
routine is not established.

## Light definitions

Lump 19 of a FAKK map, the light definitions, is never read by retail's world
load. `RE_LoadWorldMap` (`0x0049afd0`) copies the map's 172-byte header to
its stack (`0x0049b04c..0x0049b057`) and reads each lump it uses from that
copy, the entity lights (17) at `0x0049b887` and their visibility (18) at
`0x0049b8b2` among them; no instruction reads the twentieth entry. fnTech3's
renderers already use 17 and 18 for flagged models
([model light record](2026-09-25-ritual-model-lights.md)), so the ledger's
"the renderers' translation still drops ... the light definitions" names
nothing retail uses.

## Menus and the engine UI

**Changed, each from `alice.exe` and, where both engines have it, `fakk2.exe`
1.02:**

- **The developer pickers.** `soundpicker`, `viewspawnlist` and `maplist`
  open retail's file picker (a 400 by 300 window at 100, 50 with three columns
  that sort), which hands on `viewspawn <path>`, `map <name>` or the
  `ui_pickedsound` cvar (`code/compat/alice_picker.hpp`). Retail lists a pak's
  files through a folder test that misses them, so on shipped data its own
  pickers would list nothing useful [I]; fnTech3 lists the paks' files.
- **The bind list's and load list's scrollbars** repeat while an arrow is
  held, and the thumb drags, as `UIVertScroll` does.
- **The controls menu's Alice turns her head** after the cursor: a pitch and
  a yaw of -45 times the cursor's place on `Bip01 Head`, unsmoothed
  (`0x004d31db..0x004d3296`).
- **`vid_restart`** no longer reopens the menus through the drop path, which
  opened the console for a developer: retail's UI never sees a video restart
  (`0x0042b590`), and its restart unpauses outright
  (`0x0042b5b2..0x0042b5c6`, `fakk2.exe` 1.02 `0x00421312..0x00421326`).
- **`loadmenu <name>`** reads `ui/<name>` with its extension made `.urc`, as
  every menu file is read (`0x0040e0c0`, `fakk2.exe` 1.02 `0x0040cac0`); a
  name already loaded stays behind the menu loaded first, which retail's
  lookup finds (`0x004c40f0`).
- **The inventory commands** `inventory`, `warpinv`, `nextinv`, `previnv`,
  `inv_restart` and `saveweaps` are registered under Alice as its `UI_Init`
  registers them (`0x004106cc..0x00410715`, `0x004107f7`), the script loaded
  as it starts (`0x004105f2`). Alice ships `global/inventory.scr` empty, so the
  inventory commands only print `- No inventory -` (`0x004085ae`), as retail's
  would; `saveweaps` stores the hands' weapons as `weapgroup<n>` by retail's
  rule (`0x0040fed0`).
- `alice_savegame.hpp` called the load list's `quicksave` test `strcmp`; it is
  `_stricmp` (`0x004e8760`, called at `0x0041a68f`).

**Observed, and not changed:**

- A menu's show commands go to the end of the command buffer in retail too
  (`0x004bfd43` through `Cbuf_AddText` `0x00437a50`), so a probe whose waits
  are already queued sees a menu before its `activategroup` runs. That, and
  not a missing feature, is why an earlier capture of the controls menu showed
  every group at once; issued in order, the menu shows its main group.
- The controls menu's Alice is a dark figure against the mirror's lightning.
  Retail gives an unflagged model in a view with no world no ambient or
  directed light (`0x004acbfc..0x004acc24`), so the menu's two lights -- a
  steady one behind her and the lightning in front -- are all that reach her.
  fnTech3 draws her by the same rule.
- `notepad`, `editscript` and `editshader` (in-engine text editors),
  `ui_testlist` (a test list window) and `ui_saveconsolepos` and
  `ui_loadconsolepos` (the position of retail's windowed console) are not
  served. No shipped menu, script or default configuration calls them; one
  developer's configuration in `pak0.pk3` (`configs/turq.cfg`) binds
  `notepad`. fnTech3's console is its own, so the console-position commands
  have nothing to move.

**Measured.** `pickers` opens `soundpicker` over `zound` at
`sound/world/door/`, and `viewspawnlist` and `maplist` over `animate2`.
`vid-restart` raises `developer` at the main menu and restarts: the menu comes
back with no console (an earlier build, run the same way, opened the console).
`controls` shows the main group. `ui-commands` shows `- No inventory -` and
`Saved Weapon Group 1` on screen, `listmenus` lists `controls` twice after
`loadmenu controls`, and `loadmenu nosuchmenu` reports `UI: couldn't read
ui/nosuchmenu.urc`.

## Play without input

- `death`: a save, `kill`, the death fade, and `loadgame` reads the level back
  (`level time 11750`).
- `combat-exit`: Alice's attack state forced from the console, then her
  origin set into `garden1`'s exit, which loads `garden2$garden2_start1`.
  The module prints `Can't find function 'LevelComplete' in program` on the
  way out; no shipped script defines that name, and the earlier build printed
  it too, so it is the module's own developer line.
- `mallet`: the mallet given, wielded and swung (`AliceHud: weapon foldout:
  item 7 "mallet"`).
- `menuload`: a save on `garden1`, `map garden2`, then the load/save menu's
  own widget command loads it: `Server: garden1`, `Alice level read back from
  m1 at level time 11750`.

Walking, turning and firing by the player's own buttons need synthesized input,
which needs the user's permission for each run; they were not run in the
twenty-run round above. An authorized October 9 follow-up exited normally
and left the installation unchanged, but its input began during the opening
cinematic and its script used Quake's unsupported `+attack` binding rather
than Alice's `+attackright`. Its four engine captures and log therefore do
**not** establish gameplay input. They remain under
`.tmp/claude-continuation/alice-input/`; a corrected recipe waits for separate
per-run authorization. No engine input defect is inferred from that probe.

## The HUD

- **Renderers.** `hud-glx`, `hud-vk` and `hud-rtx` draw the HUD with the
  mallet's foldout on all three.
- **Draw order.** The eight children are drawn in the order they are built.
  The widget constructor zeroes the layer bits (`+0x184`, `0x004bb229`), only
  the two setters change them (`0x004bf440`, `0x004bf490`), and the HUD's
  builder (`0x004110d0..0x00411bf0`) calls neither, so the parent's re-sort,
  which runs only when a child's layer bits differ from its predecessor's
  (`0x004bf7c0`), never moves them. This settles the HUD record's open item.

## Model light in a view with no world

The model-lights record left open how retail draws a flagged model in such a
view. No shipped view reaches it [I]: the functions that build and draw the
UI's and the HUD's models (`UIAlice3D` `0x004d2fc0` and `0x004d3310`,
`UIWidget3D` `0x004d29c0` and `0x004d2b10`, the riser `0x00411d50`, the
foldouts `0x004121c0` and `0x00412a00`) hold no `RF_EXTRALIGHT` (`0x200`) or
`RF_INVISIBLE` (`0x8000000`) immediate, so their entities go unflagged, and
an unflagged model there takes no ambient or directed light (`0x004acbfc`),
which fnTech3 already does for Alice.

## Area portals

The [area portal record](2026-09-27-alice-area-portal-counts.md) left four of
Alice's script closes untraced:

| Map | Close | Reading |
| --- | --- | --- |
| `funhouse` | the Tweedle scene's door, again from its skip thread | Played (`tweedle`): the scene opens the door's portal, closes it, and a skip during Dee's first line closes it again, `CM_AdjustAreaPortalState: negative reference count`, and play resumes |
| `hatter1` | the clock room's doors (`Close_ClockRoom_Door`, a `trigger_once`) | Played (`hatter`): the trigger stands on the same side as the lever that opens them, and entered first it closes from zero, reported the same way; whether the level's route can reach it first was not established |
| `hatter1` | `NORETURN`, a `trigger_once` behind the wall it closes | Read: only the second gear lever opens that portal (`Gear_Bridge_Two`, once), and the trigger lies past the wall, so the close follows the open [I] |
| `skool1` | the observatory lift's star doors | Played on the last pass (`skool1$skool1_start2`, `skool`): the activation trigger opens them and only then makes the lift's trigger usable; the lift's thread closes them and the lift starts, with no report |
| `pandemonium` | the mine's end door, from the minecart thread or its skip | Read: the level opens it once at setup, the two closes are in a thread and its skip thread, which kills that thread, and the module runs a skip only in cinematic mode (`fgamex86.dll` `0x101171b2..0x101171d8`, the flag set and cleared at `0x100aef6a` and `0x100af083`), which the thread leaves in the same frame as its close, so one close follows the one open [I] |

F.A.K.K.2's unguarded closes were not traced.

## Collision: the patch arithmetic

fnTech3's patch planes and bevels are computed in double precision, as for
every profile; retail's are single-precision floats, evaluated on the x87.
Over the thirty-six maps the difference is eight of about 400 invalid-bevel
reports ([maps record](2026-09-22-alice-maps.md)). It is left as it is: the
arithmetic is shared by every profile, and without retail running no trace can
show which bevels differ in play. Retail's x87 evaluation would also need its
compiler's order and precision to reproduce exactly.

## Platform and editions

The retail game and client-game modules are Win32 x86 DLLs, so only a Windows
x86 build can host Alice's game; other builds compile the Alice code and
refuse the modules ([module ABI record](2026-09-22-alice-module-abi.md)). Only
the Windows retail release was available; the Mac release and the digital
re-releases were not examined.

## Not established

- **No retail Alice has run.** No session between retail `alice.exe` and
  fnTech3 in either direction, no retail-written save or retail-recorded
  demo, no fnTech3 save or demo played by retail, no frame beside retail's.
  Each needs `alice.exe` running, which SafeDisc prevents here.
- Movement, turning and firing by the player's own buttons (above).
- No listening test of Alice's audio; the dialog path, obstruction, a named
  room and the pause have unit evidence and runs that start them, not a
  recorded comparison.
- What reaches the module's CRC routine, if anything shipped does.
- F.A.K.K.2's module tables are not typed, and its unguarded portal closes
  were not traced.
