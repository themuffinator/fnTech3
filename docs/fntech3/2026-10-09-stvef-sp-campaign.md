# Elite Force's single-player campaign played through, 2026-10-09

fnTech3's developer client runs the whole Elite Force single-player campaign
on the retail game and menu modules: from `borg1`, which New Game starts, by
each level's own exit, through 45 levels and 44 transitions to `voy20`, whose
epilogue plays the closing movie and the credits. Every level loads, settles,
writes its autosave and carries the player's health, armour and ammunition to
the next. All eight of the campaign's movies play to their end. The campaign
also runs with nothing but the retail paks and libraries. The three holodeck
programmes, both brig visits and the Virtual Voyager tour go there and back,
22 of the game's menus open, and all 99 of its maps load on Vulkan. No device
input is involved. The [JSON companion](2026-10-09-stvef-sp-campaign.json)
holds every level's result, identities and the warnings.

## How it is played

`scripts/probe_stvef_campaign.py` starts the developer client windowed, with
no sound and an isolated home, and drives it over loopback rcon. Every exit in
the campaign is a `target_level_change`, whose use sends the engine
`maptransition <map> <target>`, or `loadtransition` for a hub
(`efgamex86.dll` 0x2007dba0, `G_ChangeMap` 0x20083740). Once a level is in play
and any opening movie has ended, the probe fires the exit with the game's own
`use <targetname>` command (0x200833b0), which the retail menus send too;
where `use` cannot reach an exit, it moves the player onto the exit's trigger.
The exits and their triggers were read from each map's entity lump and its
ICARUS scripts in the installation's paks. For each level it records the
load, the `playersave` and `playerammo*` values the engine carries, the
autosave's map, warnings and errors, and an engine screenshot. It does not
play the levels: no enemy is fought and no puzzle solved.

## Runs

| Run | Client | Content | Result |
| --- | --- | --- | --- |
| Campaign, GL | `ef59fafa…` (`45444bd3`) | installation | 45 of 45 levels, 44 autosaves, 8 movies, credits |
| Campaign, GL | `83a7723e…` (`37b87dce`'s save-line fix) | retail paks and libraries only | 45 of 45, 8 movies, credits |
| Campaign, GL | `fd4f979d…` (the [frame-time](2026-10-08-stvef-frame-time.md) and [serverinfo](2026-10-08-stvef-serverinfo.md) changes) | installation | 45 of 45, 8 movies, credits |
| Holodeck `voy5`, `voy7`, `voy13`; brig `voy1`, `voy4` | `ef59fafa…`, then `fd4f979d…` | installation | each there and back, both times |
| Virtual Voyager tour | `83a7723e…`, then `fd4f979d…` | installation | 11 decks, both times |
| 22 menus by name | `ef59fafa…` | installation | all open and are captured |
| Map loads, Vulkan (`scripts/probe_stvef_hunk_maps.py`) | `83a7723e…` | retail paks and libraries only | 99 of 99 (66 single player, 33 Holomatch), each to its first snapshot and an engine screenshot at the profile's smallest hunk |

The installation's `BaseEF` also holds `zSTVEFtex.pk3`, a third-party HD
texture pack from 2023. The stock layout under `.tmp/stvef-stock` holds only
the four retail paks, hard links with the installation's hashes, and copies
of `efgamex86.dll`, `efuix86.dll` and `binkw32.dll`.

Details:

- The movies are `st_01`, `st_06`, `st_07`, `st_09`, `st_10`, `st_13`, `st_15`
  and `st_21`, each 512 by 384, and each ends with `Bink finished`. `st_21`,
  the closing movie, plays its 160 frames; an earlier record's report that it
  was refused came from a Python zip reader rejecting its stored CRC, which the
  engine does not check.
- The holodeck trips leave by the level's `hololevel` and return by the
  programme's `endsim`; the return restores the `exitholodeck` save the game
  writes on leaving, once the game allows a save (it refuses one during the
  opening camera scene, as `stvoy.exe` does). The brig trips go by `gotobrig`
  and return by `load *respawn`, the brig's own way back.
- The tour starts with `cg_virtualvoyager 1`, as the menu's tour button sets
  (`efuix86.dll` 0x40017000), and takes every turbolift as a hub exit: a deck
  left is kept as `hub/<deck>` and restored when visited again, as decks 02 and
  04 are. A first visit has no hub save yet, and the engine says so in
  `stvoy.exe`'s own words (`Failed to open savegame`, 0x49347c) before loading
  the deck fresh.
- The menus are `ui_holodeck`, `ui_turbolift`, `ui_transporter`, `ui_library`,
  `ui_log`, `ui_navigation`, `ui_ops`, `ui_padd`, `ui_tactical`,
  `ui_astrometrics`, `ui_cargo`, `ui_diseases`, `ui_engineeringdata`,
  `ui_engineeringstatus`, `ui_weaponlibrary`, `ui_shootingrange`,
  `ui_hazardgame`, `ui_initialsetup`, `genericmenu`, `genericholomenu`,
  `endholomenu` and `ui_closingcredits`, opened from the tour's deck 02 by
  `scripts/probe_stvef_sp_menus.py`; each gives a distinct engine screenshot.
- Saving, listing and loading through the menus themselves, and play with
  input, are in the [input and menus record](2026-10-08-stvef-input-menus.md).

## Warnings

No level reports an error. The campaign's warnings, by kind:

| Warning | Count (final run) | What it is |
| --- | --- | --- |
| `CM_AddFacetBevels... invalid bevel`, `CM_SetBorderInward: mixed plane sides` | 2,208; 58 | retail map data; `stvoy.exe`'s own texts (0x48af64, 0x48b0cc) |
| `R_RegisterMD3: couldn't load .../..._flash...md3` | 868 | weapon flash models the paks do not hold; a developer notice here, where retail's loader fails silently |
| `Unable to load ambient sound` | 614 | the runs have no sound system; `stvoy.exe` prints the same text (0x491800), and with sound on `borg1` prints none |
| `Too many subwaves on set` | 225 | retail data; `stvoy.exe`'s own text (0x491844) |
| `reused image ... with mixed flags` | 44 | the renderer's form of retail's reused-image warnings (0x49a73c..0x49a7b0) |
| `cvar "cg_stereoSeparation" given initial values` | 45 | fnTech3 keeps this cvar read-only at 0 because its renderers own stereo separation (`r_stereoSeparation`), as FnQuake3 does; the game's `0.4` is refused |

## Not covered

- A person playing the levels: each is left through its exit, not won.
- Listening and physical devices, which the user chose not to run.
- A synchronized pixel comparison with the retail renderer.
