# Elite Force's engine registrations beside retail's, 2026-10-23

Elite Force is the title with the fewest open gates outside Quake III and Quake
Live: Holomatch plays both ways with the retail game, the campaign plays
through and savegames load in both engines. This round compared every console
command and cvar the two retail executables register with what fnTech3
registers under `stvef-sp` and `stvef-mp`, at the main menu, in a level and on
a Holomatch dedicated server. It found nineteen differences that a module, a
menu, a savegame, the wire or the player can see, and this change makes all
of them follow retail. The [JSON companion](2026-10-23-stvef-retail-registrations.json)
holds the identities, addresses, commands and before-and-after lists. This is
Source, Unit and Live evidence. No compatibility gate closes.

## How the census was taken

The retail executables are the ones every Elite Force record names:
`stvoy.exe` 1.20 (`8f7ccadb…`, PE stamp `0x3adb539a`) and `stvoyHM.exe` 1.20
(`5b5b6e82…`, `0x3adde3c8`). Both were read statically with capstone and
pefile: every direct call to `Cmd_AddCommand` (`stvoy.exe` 0x410b90,
`stvoyHM.exe` 0x4198d0, found by `Cmd_AddCommand: %s already defined`) and
to `Cvar_Get` (0x413450 and 0x41ca90, found by `Cvar_Get: NULL parameter`),
with the name, default and flags each one pushes. The renderer registers its
own through a function pointer, so its cvars come from the runtime lists alone.

Each engine then ran windowed on a private desktop with no input and no
sound, from isolated folders, and printed `cmdlist` and `cvarlist` at its main
menu and again after `devmap voy2` or `devmap hm_borg1`; Holomatch's
dedicated server printed both as well. Retail read the staged copy of the four
retail paks through a scratch `fs_basepath`, fnTech3 the stock layout under
`.tmp/stvef-stock`. The installation was unchanged after every run.

Most of what fnTech3 registers beyond retail is its own: renderer, audio,
input and diagnostic features, and services retail never had. Those stay, as
invariant 11 asks. The differences below are the ones a retail module, menu,
config, save or peer reads, or a player meets.

## What changed

| Difference | Retail | Before | Now |
| --- | --- | --- | --- |
| `+careful`, `-careful` (SP) | button 6, sent as bit 64 (handlers 0x405310/0x405320; `CL_CmdButtons` 0x405940) | unknown command | button 6 |
| `rserverinfo` (Holomatch) | prints the server's serverinfo when active, else `Not connected to a server.` (0x40daa0, registered 0x40fcad) | unknown command | the same |
| `cl_yawspeed`, `cl_pitchspeed` | archived (`stvoy.exe` 0x40866d..0x408697, `stvoyHM.exe` 0x40f902..0x40f927) | cheat-protected | archived |
| `cl_anglespeedkey` | archived; both controls menus set it | cheat-protected, not archived | archived |
| `cl_maxpackets` | `30`, archived; Holomatch holds it to 15..100 where it sends (0x40a5f1..0x40a614) | `125`, cheat-protected, 15..125 | `30`, 15..100 |
| `cl_run` | SP `0` (0x4086ef), Holomatch `1` | `1` | as retail |
| `sensitivity` | `5` | `2` | `5` |
| Holomatch userinfo | nine keys (`CL_Init` 0x40fabc..0x40fb4a) | also `cl_anonymous`, `cl_guid` | nine keys |
| SP userinfo, which every savegame keeps | seven engine keys (0x408872..0x4088db) and the game's two | also `cl_anonymous`, `cl_guid`, `rate`, `password`, `cg_predictItems` | retail's nine |
| SP model keys | userinfo only, not archived (0x408894..0x4088b9) | archived | not archived |
| SP serverinfo, kept in every full save | `fs_game`, `mapname`, `protocol`, `version` (`SV_Init` 0x425c70..0x425dad, `fs_game` 0x415fb7) | seventeen keys, `fs_game` not among them | retail's four |
| SP `sv_cheats` | systeminfo, writable (0x41404c, 0x425cb3) | read-only | writable |
| SP `spawntarget` | registered empty beside `nextmap` (`SV_Init` 0x425d3c) | made by the first level transition | registered |
| Holomatch map command's `g_gametype` | serverinfo and latch, not userinfo (`SV_Map_f` 0x42e4b8..0x42e4c4) | also userinfo, as Quake III 1.32 | serverinfo and latch |
| Holomatch `bot_thinktime` | cheat-protected (0x42de06) | unprotected | cheat-protected |
| Holomatch `bot_maxdebugpolys` | `128` (0x42dd75) | `2` | `128` |
| Holomatch `timelimitWinningTeam` | registered empty and temporary beside `timelimit` (0x433de8..0x433df7) | left to the game | registered |
| `s_khz`, `in_joystick`, `joy_threshold`, `joy_xbutton`, `joy_ybutton`, `r_lowEndVideo` | registered (`S_Init` 0x41fad7 and 0x429abc, `IN_Init` 0x42d53c..0x42d5c6 and 0x43ec8f..0x43ed00); the sound, controls and video menus of both modes show them | not registered on Windows | registered with retail's defaults |
| SP `s_volumeVoice` | `1.0`, archived (0x41fa95); the sound menu's voice slider | not registered | registered |

The single-player `sv_cheats` difference is a live one. In retail, `map voy2`
and then `sv_cheats 1` and `god` answers `godmode ON`; fnTech3 had answered
`sv_cheats is read only.` and kept cheats off. Now it answers as retail does.

`s_volumeVoice` is registered and nothing more. `stvoy.exe` stores the cvar
at 0x792b10, and no instruction in the executable reads that address again.
The menus never pass it to the game either. Retail's voice slider changes
nothing, and fnTech3's does the same: voices play at the sound volume in both
engines. The other menu cvars are registered for the same reason Alice's
are: the menus show, change and keep them. Only fnTech3's legacy mixer reads
`s_khz`, as its mix rate (OpenAL chooses its own), its renderers do not read
`r_lowEndVideo`, and its Windows build has no joystick support
(`USE_JOYSTICK` is set only for Linux), so those settings otherwise change
nothing here. Physical devices are not tested, as before. (Windows builds
read joysticks since 2026-10-24, and `in_joystick` and `joy_threshold` now
apply there, as do `joy_xbutton` and `joy_ybutton` in the native WinMM
backend; see the [joystick record](2026-10-24-windows-joystick.md).)

## Where the code changed

- `code/client/cl_input.cpp`: `+careful` and `-careful` in the profile
  binding table, and Elite Force's input cvars in `CL_InitInput`, beside
  Alice's.
- `code/client/cl_main.cpp`: `CL_STVEFInfoKeys`, run at the end of `CL_Init`
  for both modes, takes the keys retail lacks out of the userinfo;
  `CL_RemoteServerinfo_f` and its registration; the menu cvars.
- `code/qcommon/game_profile.h`, `game_profile.cpp`: the appearance policy's
  new `unarchivedModels`, set for single player.
- `code/server/sv_init.cpp`: `SV_STVEFSPInfoKeys`, the writable single-player
  `sv_cheats` and `spawntarget`, and Holomatch's `timelimitWinningTeam`.
- `code/server/sv_ccmds.cpp`: Holomatch's `g_gametype` flags in `SV_Map_f`.
- `code/server/sv_bot.cpp`: Holomatch's `bot_thinktime` and
  `bot_maxdebugpolys`.

Every other profile registers what it did before. No retail text or code was
incorporated, apart from the short console messages `rserverinfo` prints,
which are reproduced as retail's interface text.

## Left as they are

- `rate` `25000` and `snaps` `40`, where retail registers `3000` and `20`:
  both are within what both retail servers accept, and a Holomatch server
  holds `snaps` to 30 anyway ([server clock record](2026-10-09-stvef-holomatch-server-clock.md)).
- Renderer and audio presentation defaults, among them `r_picmip`,
  `r_textureMode`, `r_lodBias`, `r_subdivisions`, `r_flares`, `r_flareSize`,
  `s_volume` and `s_musicvolume`, and the raised hunk and zone sizes.
- `vm_game`, `vm_cgame` and `vm_ui` at `2`, where retail's `0` tries a
  native library first: fnTech3 loads a Holomatch mod's native module only
  when asked.
- The cheat flags of the renderer's `r_clear`, `r_noportals` and `r_showsky`,
  and of `timedemo`, `viewlog` and `com_dropsim`.
- What no retail module reads: MIDI input, IPX, `setenv`, `rawshot`,
  `conAlpha`, `username`, the Windows handle cvars, `sys_cpuid` and its
  kin, and `cl_updateInfoString`. Holomatch's game reads `fs_cdpath` only to
  pass the bot library its CD folder, and both menus read `fs_restrict`
  only to recognise a demo copy; unregistered, both read as empty, which is
  what they hold in a full retail installation.
- Client commands: single player's engine knows only `userinfo` and
  `disconnect` (`stvoy.exe`'s table at 0x4929b0), and Holomatch's has no
  `locations` (0x4cb400). fnTech3's server answers `cp`, `vdr`, the four
  download commands and its own `locations` diagnostic in both modes. No
  retail game module names any of them, and fnTech3's single-player client
  sends the pure proof and `vdr` only to a pure server, which single player
  never is.
- Serverinfo key order: retail lists the newest registration first, fnTech3
  its own order; every reader looks keys up by name
  ([serverinfo record](2026-10-08-stvef-serverinfo.md)).

## The listener in a camera scene

The [sound distance record](2026-09-20-stvef-sp-sound-distances.md) left
open which origin retail's single-player client game passes as the listener
during a cinematic. It passes the render view's own: `efgamex86.dll`
calls its listener import, 31, with `cg.refdef.vieworg` and `viewaxis`
(0x2003649a..0x200364a5), after the view is calculated (0x200363fb) and from
the same record it renders from. fnTech3's OpenAL world places the listener
at exactly that origin and axis (`Q3SoundWorld::Respatialize`), so both
engines hear a camera scene from the same point. fnTech3's traced occlusion
of a voice from there is its own feature, and has no retail counterpart.

## Evidence

- `tests/stvef_retail_registration_tests.py` compiles `CL_STVEFInfoKeys`,
  `SV_STVEFSPInfoKeys` and `CL_RemoteServerinfo_f` with stubbed engine
  services. They must leave Holomatch's nine userinfo keys and single
  player's seven (with the game's two), single player's four serverinfo keys
  and its three systeminfo keys, and print retail's lines in the right
  states. The tests also pin where `CL_Init`, `SV_Init` and `SV_Map_f` apply
  them, the writable single-player `sv_cheats`, `spawntarget`,
  `timelimitWinningTeam`,
  `rserverinfo`'s registration and removal, and the menu cvars.
  Dropping `cl_guid` from the filter, or keeping `sv_cheats` read-only, each
  fails them.
- `tests/input_system_source_tests.py` (`EliteForceInputSourceTests`) pins
  `+careful` on button 6, single player's eight-button send, and the input
  cvars' defaults, flags and send clamp; `tests/profile_appearance_tests.py`
  both modes' model keys; `tests/bot_profile_defaults_tests.py` the bot cvars
  for every profile.
- Live, with the client and server built from a `git archive` of `f4d07ef0`
  with this round's files alone (`fntech3.exe` `49cf555c…`,
  `fntech3.ded.exe` `55a1f3dc…`), the same binaries whose full suite is
  recorded below, after the investigation's runs on the shared developer
  build had shown the same:
  the userinfo and serverinfo key sets equal retail's in all four states
  checked, single player and Holomatch at the menu and in a level; `sv_cheats
  1` then `god` answers `godmode ON` in single player; `rserverinfo` prints
  the serverinfo on `hm_borg1` and refuses at the menu.
- Savegames: retail `stvoy.exe` and fnTech3 each `devmap voy2` and `save
  census` from the console, and the two saves' chunks were read with the
  frame layout of `code/compat/stvef_sp_savegame.hpp`. Both list the same
  nine userinfo cvars (`g_gravity`, `g_spskill`, `handicap`, `headModel`,
  `legsModel`, `name`, `sex`, `snaps`, `torsoModel`), retail newest first and
  fnTech3 in its own order, where the [savegame record](2026-09-19-stvef-sp-savegames.md)
  found fourteen in fnTech3's. Both keep 88 configstrings, and the serverinfo
  among them names the same four keys, `gamename`, `mapname`, `protocol` and
  `version`; an empty `fs_game` is left out by both.

## Validation

The commit's tree alone was built and tested: a `git archive` of `f4d07ef0`
with this round's files and no other session's work, configured as the
developer build with strict warnings. It built all 1,548 targets. Its 283
native tests passed but for the two opt-in skips, the audio loopback and the
retail factory probe. Its 627 Python test files passed: 2,905 tests with 134
skips and 69,047 subtests, in 31 minutes. A code comment and the records
were reworded after that run; the changelog, documentation, packaging and
player-manual tests and this round's own were run again on the final files.

## Limits

The census covers registrations, not what each engine does with a cvar. The
Windows build's missing joystick support, the physical keyboard, mouse and
listening checks the user chose not to run, and every other gate's open
items are unchanged.
