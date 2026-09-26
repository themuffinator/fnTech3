# Games Guide

fnTech3 runs one game per process. At startup it selects a **game profile**:
the set of contracts one game, or one mode of a game, needs from the engine --
its installation layout, config file names, module interface, network and demo
protocol, asset formats and optional services. The profile is chosen before
the filesystem, network, modules or renderer start, and it cannot change while
the process runs.

This guide lists the games, how fnTech3 chooses between them, where it reads
and writes files, and what each game can do today. fnTech3 is engine-only:
every game needs your own legitimate installation, and its modules, assets and
proprietary runtimes stay there. **No profile has full compatibility yet.**
The [compatibility ledger](fntech3/COMPATIBILITY.md) is the authority on what
has been shown to work; this guide summarizes it.

## Games and profiles

| Game | Mode | Profile | Also accepted | Game directory | Config file | Available in |
| --- | --- | --- | --- | --- | --- | --- |
| Quake III Arena | | `quake3` | `q3`, `quake3arena` | `baseq3` | `q3config.cfg` | every build |
| Quake III Arena IHV Test (February 1999) | | `q3-ihv` | `quake3-ihv`, `q3ihv`, `ihv` | `baseq3` | `q3config.cfg` | `-Dq3-ihv-development=true`, dedicated server only |
| Quake III Arena Q3Test 1.02 to 1.08 (1999) | | `q3test` | `q3t`, `quake3-test` | `demoq3` | `q3config.cfg` | `-Dq3test-development=true`, Windows x86 |
| Quake Live | | `quakelive` | `ql` | `baseq3` | `qzconfig.cfg` | every build |
| Return to Castle Wolfenstein | single player | `rtcw-sp` | `rtcwsp`, `wolfsp` | `main` | `wolfconfig.cfg` | `-Drtcw-development=true` |
| | multiplayer | `rtcw-mp` | `rtcwmp`, `wolfmp` | `main` | `wolfconfig_mp.cfg` | `-Drtcw-development=true` |
| Wolfenstein: Enemy Territory | | `wolfet` | `et`, `enemy-territory`, `enemyterritory` | `etmain` | `etconfig.cfg` | `-Dwolfet-development=true` |
| Star Trek Voyager: Elite Force | single player | `stvef-sp` | `efsp`, `stvoy`, `eliteforce-sp` | `baseEF` | `efconfig.cfg` | `-Dstvef-development=true` |
| | Holomatch | `stvef-mp` | `stvef-hm`, `holomatch`, `efhm`, `stvoyhm` | `baseEF` | `hmconfig.cfg` | `-Dstvef-development=true` |
| Star Wars Jedi Knight: Jedi Academy | single player | `jka-sp` | `jasp`, `jediacademy-sp`, `jedi-academy-sp` | `base` | `jaconfig.cfg` | `-Djka-development=true`, dedicated server only |
| | multiplayer | `jka-mp` | `jamp`, `jediacademy-mp`, `jedi-academy-mp` | `base` | `jampconfig.cfg` | `-Djka-development=true`, dedicated server only |
| Star Wars Jedi Knight II: Jedi Outcast | single player | `jk2-sp` | `jk2sp`, `jedioutcast-sp`, `jedi-outcast-sp` | `base` | `jk2config.cfg` | `-Djk2-development=true`, dedicated server only |
| | multiplayer | `jk2-mp` | `jk2mp`, `jedioutcast-mp`, `jedi-outcast-mp` | `base` | `jk2mpconfig.cfg` | `-Djk2-development=true`, dedicated server only |
| American McGee's Alice | | `alice` | `americanmcgeesalice`, `mcgee-alice`, `alice2000` | `base` | `config.cfg` | `-Dalice-development=true` |
| Heavy Metal: F.A.K.K.2 | | `fakk2` | `heavymetal-fakk2`, `heavymetalfakk2`, `hmfakk2` | `fakk` | `config.cfg` | `-Dfakk2-development=true` |
| Medal of Honor: Allied Assault | | `mohaa` | `medalofhonor`, `moh-aa`, `alliedassault` | `main` | `configs/unnamedsoldier.cfg` | `-Dmohaa-development=true` |

Quake III Arena and Quake Live start in every build. Every other game, and
each Quake III test build, is
**experimental**: it starts only in a developer build made with the option
shown, which the [Build Guide](../BUILD.md#game-development-builds) describes.
Release packages are built without those options and refuse those games with an
explanation. A developer build is for testing; it does not claim that the game
is compatible.

Return to Castle Wolfenstein, Elite Force, Jedi Academy and Jedi Outcast are
each one game with two modes. Both modes of a game share one installation and
the same `fntech3.exe`, but each mode is its own profile with its own config,
modules and protocol. Changing mode starts the executable again with the other
profile.

A dedicated server keeps its own config apart from the client's, such as
`q3config_server.cfg` for Quake III Arena and Quake Live,
`wolfconfig_server.cfg` for both RTCW modes and `etconfig_server.cfg` for
Enemy Territory.

## Choosing the game

### Naming the profile

Name the profile on the command line:

```powershell
.\fntech3.exe +set com_gameProfile quake3 +set fs_basepath "C:\Games\Quake III Arena"
```

`com_gameProfile` accepts the profile names and the other accepted names in the
table above. It is read once, at startup; setting it later, or in a config,
changes nothing. A game's own name without its mode, such as `rtcw`, is refused
with a request to choose the mode.

### Automatic detection

The default, `com_gameProfile auto`, looks for exactly one installed game, in
this order, and stops at the first place that has one:

1. the folder named by `+set fs_basepath`;
2. the folder that holds the executable;
3. the Steam libraries of the games Steam sells: Quake III Arena, Quake Live,
   Return to Castle Wolfenstein and Enemy Territory.

A game is recognized by its own base archive, such as `baseq3/pak0.pk3` for
Quake III Arena or `etmain/pak0.pk3` for Enemy Territory. fnTech3 reads a known
file inside it rather than trusting the file name. Startup stops with an error
naming the reason when:

- two different games are found in the same place (`ambiguous`);
- one installation holds both modes of a game (`mode required`): Return to
  Castle Wolfenstein, Elite Force, Jedi Academy and Jedi Outcast always need
  the mode named;
- a base archive has the right name but not the right contents
  (`unrecognized pak`);
- the game found needs a development option this build does not have
  (`incomplete engine`);
- nothing is found (`not found`).

The error names the profiles this build can start. Naming the profile always
avoids detection, and is the recommended way to launch.

### Checking and switching

- `gameList` prints every game and mode, marks the running one with `*`, and
  says whether this build can start each.
- The console title and window title name the running game.
- `gameMode <profile>` switches to another mode of the same game, such as
  `gameMode rtcw-mp` from RTCW single player. It saves the current mode's
  config, closes the game, and starts the same executable again with the
  other profile, keeping `fs_basepath`, `fs_homepath` and `r_fullscreen`. It
  does not carry `fs_game` across. RTCW's own menu buttons
  (`startSingleplayer`, `startMultiplayer`) use the same path.

Switching to a different game is done by quitting and starting again with a
different profile.

## Where files are read and written

- `fs_basepath` is the game's installation. fnTech3 reads it and never needs
  to write there if `fs_homepath` points elsewhere. When it is not set, it
  defaults to the executable's folder when that folder holds the selected game,
  and otherwise to the game's Steam installation where Steam sells it.
- `fs_homepath` receives configs, saves, screenshots, demos and downloads,
  inside the profile's game directory (or `fs_game`'s). Its default depends on
  the platform:
  - Windows: Quake Live uses its retail profile folder,
    `<Quake Live>\<SteamID64>`, as the retail game does. Every other game uses
    the installation itself, as its original executable did.
  - Linux: `~/.q3a` for every game. Because several games share game directory
    names (`baseq3`, `main`, `base`), give each game its own home.
  - Both: `+set fs_homepath <folder>` on the command line overrides the default.
- `fs_game` selects a mod directory inside the selected game, as in the
  original engines.

Use a separate `fs_homepath` for each game when you test, and keep the retail
installation read-only:

```powershell
.\fntech3.exe +set com_gameProfile rtcw-sp +set fs_basepath "D:\Games\RTCW" +set fs_homepath "D:\fnTech3-homes\rtcw-sp"
```

Keep these files beside the executable; fnTech3 looks for them there:

- `fntech3-pkg.fnz`, the engine package. It holds fnTech3's own data for each
  game -- audio zones, static light files, fog presets, HUD layouts, sound
  tuning -- under a separate `games/<profile>/<game directory>/` folder per
  game, so one game's files are never seen by another. It never holds retail
  content. See the [package layout](fntech3/PACKAGES.md).
- `fnql-web.pak`, fnTech3's settings overlay for Quake Live's WebUI.
- The renderer modules `fnql_glx_x86`, `fnql_vk_x86` and `fnql_rtx_x86`.

## What fnTech3 adds to every game

Each game keeps its own contracts: its modules, network protocol, demos,
saves, file formats and gameplay behave as its original engine requires.
How the engine *presents* that game is fnTech3's own. The same renderer,
audio, input, console and screenshot systems serve every game, fed with that
game's own data:

- three renderers, GLx, Vulkan and RTX, with HDR, anti-aliasing, render
  scaling, bloom, per-pixel and shadowed dynamic lights, static map lights and
  post-processing ([Display Guide](DISPLAY.md), [RTX Renderer Guide](RTX.md));
- spatial audio through OpenAL -- distance falloff, occlusion, environmental
  reverb, audio zones and HRTF -- fed with each game's own sound distances,
  channels and loops ([Audio Guide](AUDIO.md));
- the scalable console, completion and clipboard ([Console Guide](CONSOLE.md));
- engine screenshots, levelshots and cube maps
  ([Screenshot Guide](SCREENSHOTS.md));
- widescreen FOV and HUD correction for every game whose modules assume a
  4:3 screen -- all of them but Quake Live, whose modules fit both themselves,
  and Medal of Honor's HUD, which its modules place
  ([Aspect Handling Guide](ASPECT_CORRECTION.md)).

Where a game's original presentation differs, the original is kept only as an
explicit option, never as the default -- for example the legacy software
mixer (`s_backend legacy`) for retail sound. Which features apply to which
game is noted in each guide.

## Quake III Arena (`quake3`)

**Installation:** a Quake III Arena installation with `baseq3/pak0.pk3`, such
as the Steam or GOG release. Team Arena runs as the `missionpack` mod
(`+set fs_game missionpack`).

**Status:** enabled in every build, client and dedicated server, on Windows
and Linux. The retail QVM modules run, the shipped demos play, and short pure
sessions with the retail Steam executable pass in both directions: fnTech3's
client on a retail server and a retail client on fnTech3's server. Sustained
play, downloads and remote networks are among the gates still open in the
[ledger](fntech3/COMPATIBILITY.md#compatibility-and-gate-matrix).

**Versions:** one `quake3` profile spans Quake III's releases, from the 1.11
retail game to 1.32c, by protocol:

| Release | Protocol | What fnTech3 does |
| --- | --- | --- |
| 1.32, 1.32b, 1.32c (Steam, GOG) | 68 | The default: plays, hosts and records `.dm_68`, with the retail QVMs, shipped demos and pure sessions against the retail executable. |
| ioquake3 and compatible ports | 71 | Plays, hosts and records `.dm_71`. Not an id release. |
| 1.31 | 67 | Joins original 1.31 servers, pure or not, and serves original 1.31 clients; recordings play in both engines. |
| 1.29f, 1.29g, 1.29h, 1.30 | 66 | The same for these releases, including 1.29's own `pak6.pk3` variants. |
| 1.27g, 1.27h | 48 | Plays their recordings (`.dm_48`) with the 1.32 modules. |
| 1.25p, 1.25y | 46 | Plays their recordings (`.dm3`, and those 1.27 reshipped as `.dm_48`). |
| 1.17 | 45 | Plays its recordings. |
| 1.11 retail and demo, Demo Test 1.09, 1.14 to 1.16n | 43 | Plays protocol-43 recordings (`.dm3`), the 1.11 demo's own among them. |
| Q3Test 1.02 to 1.08 | 35 to 40 | A profile of its own, [below](#quake-iii-arena-q3test-q3test). |
| IHV Test, February 1999 | 33 | A profile of its own, [below](#quake-iii-arena-ihv-test-q3-ihv). |

A 1.32 client asks an older server for its protocol and joins it as it is.
To run from a complete 1.29 to 1.31 installation, or to serve those releases'
clients, pick the protocol at startup; it sets the paks the installation must
hold (`pak0` to `pak6` for 66, to `pak7` for 67):

```powershell
.\fntech3.exe +set com_gameProfile quake3 +set protocol 66 +set fs_basepath "C:\Games\Quake III Arena 1.30"
.\fntech3.ded.exe +set com_gameProfile quake3 +set protocol 67 +set fs_basepath "C:\Games\Quake III Arena 1.31" +map q3dm1
```

Sessions with 1.29 to 1.31 engines are short tested ones in both directions,
not sustained play. Releases before 1.29 play back but cannot go online:
their network protocols and older game modules are not implemented. The
[point-release ledger](fntech3/Q3_POINT_RELEASES.md) records every release's
evidence and remaining gates.

**Notes:**

- The engine package carries Quake III audio zones, fog presets for Quake III
  and Team Arena maps, weapon sound tuning and a widescreen HUD layout, all from
  FnQuake3.

## Quake III Arena IHV Test (`q3-ihv`)

The February 1999 test build given to hardware vendors has its own modules,
protocol 33 and BSP version 43, none of which is retail Quake III.

**Status:** developer builds with `-Dq3-ihv-development=true` run the original
game DLL on a Windows x86 dedicated server with its four maps. Its client, its
network play and its demo playback are not implemented; the server drops
every packet, so run it with networking off. See the [IHV ledger](fntech3/Q3_IHV.md).

## Quake III Arena Q3Test (`q3test`)

The public tests of 1999 -- 1.02 and 1.03 as dedicated servers, 1.05 to 1.08
with a client -- each have their own protocol (35 to 40), map version (IBSP 44
or 45) and module tables. One `q3test` profile serves them all and pins one
release at startup from the installation's own modules and pak, so a game
directory holds one release.

**Installation:** a Q3Test release laid out as its installer did: `quake3.exe`
and the `*x86.dll` modules in the root and the paks in `demoq3/`. fnTech3 runs
the release's own modules; it reads the installation and never writes to it.

**Status:** developer builds with `-Dq3test-development=true`, Windows x86
only, run every release's dedicated server and, from 1.05, its client, with
the retail modules: the server hosts every map for up to 128 of the release's
retail clients, the client joins fnTech3's and the retail servers, draws the
release's menus (1.06 to 1.08's own, and 1.05's as engine menus, since 1.05
shipped none) in every renderer, and records and plays `.dm3` demos, the
shipped ones included, with their sound. 1.02 and 1.03 have no retail client;
their servers are exercised with a protocol probe that joins retail and
fnTech3 servers alike. Input, live gameplay and sessions with several players
are not yet verified. See the [Q3Test ledger](fntech3/Q3TEST.md) for the
evidence and what remains open.

**Notes:**

- The retail engines themselves ran in a restricted demo mode that reads only
  their own paks, so a retail client cannot load a recording or a map made
  elsewhere; fnTech3's recordings are checked by replaying them in fnTech3 and
  with an independent decoder instead.
- Menus, HUD and world are drawn with fnTech3's renderers and features, with
  each release's shader dialect (`fogParms` with five values, `fogonly`,
  `clampTexCoords`) read as that release read it.

## Quake Live (`quakelive`)

**Installation:** the Steam release, normally
`C:\Program Files (x86)\Steam\steamapps\common\Quake Live`. fnTech3 finds it
through Steam when it is not beside the executable. `+set fs_steampath` names
an extra Steam copy to search.

**Status:** enabled in every build. The retail client modules in `bin.pk3`
are 32-bit Windows DLLs, so the Quake Live client runs only in the Windows x86
build. The Linux i686 dedicated server loads the retail `qagamei386.so`.
Short authenticated sessions with retail Quake Live pass in both directions;
the remaining gates are in the
[ledger](fntech3/COMPATIBILITY.md#compatibility-and-gate-matrix).

**Notes:**

- Protocol 91. The engine services only Quake Live has -- factories, the
  WebUI, Steam, host-drawn fonts and map advertisements -- are described in
  the [FnQL baseline documents](fnql/README.md), which remain the reference
  for this profile.
- The retail WebUI menus run through the Awesomium runtime in your
  installation, on Windows x86 only. `fnql-web.pak` adds fnTech3's settings
  pages; retail `web.pak` is still required. Elsewhere the engine's own menus
  are used.
- Steam services need a separately supplied Steam provider, which fnTech3
  packages do not include. Without it they report themselves unavailable
  rather than pretending to succeed. See the
  [Steam provider notes](fnql/STEAM_PROVIDER.md).
- Settings live in `qzconfig.cfg` and `repconfig.cfg` at the root of the
  profile folder, as in retail. The first start in a home without
  `qzconfig.cfg` imports an existing `baseq3/q3config.cfg` once.
- The engine package carries an audio-zone and fog preset for every one of the
  149 maps in the retail package, plus weapon sound tuning.

## Return to Castle Wolfenstein (`rtcw-sp`, `rtcw-mp`)

**Installation:** one Return to Castle Wolfenstein installation with `main/`.
Single player and multiplayer share it; always name the mode.

**Status:** experimental; developer builds with `-Drtcw-development=true`.
The retail modules run in both modes in the Windows x86 client with all three
renderers. Single player starts each of its 32 maps from its menus and
cinematics, and saves and loads, including saves exchanged with the retail
game. Multiplayer starts each of its 15 maps, admits retail clients to
fnTech3's servers, and exchanges demos with the retail game in both
directions. The campaign as a whole, sustained AI and combat, and play from
fnTech3's client on retail servers are still open; see the
[RTCW gate matrix](fntech3/COMPATIBILITY.md#rtcw-gate-matrix).

**Notes:**

- Single player uses protocol 49 and loads levels with `spmap`; multiplayer
  uses protocol 60. Each mode loads only its own archives: single player skips
  `mp_*` paks and multiplayer skips `sp_*` paks.
- Changing mode from the main menu restarts the executable in the other mode
  (see [Checking and switching](#checking-and-switching)).
- The engine package carries audio zones and static light files for all 32
  single-player and 15 multiplayer maps, and a widescreen HUD layout for each
  mode.
- The profile rules and mode switch are recorded in the
  [RTCW contracts](fntech3/2026-09-14-rtcw-contracts.md).

## Wolfenstein: Enemy Territory (`wolfet`)

**Installation:** an Enemy Territory installation with `etmain/`, such as the
Steam release.

**Status:** experimental; developer builds with `-Dwolfet-development=true`
run the client and the dedicated server. The retail modules draw ET's menus
and limbo panel and load all six stock maps on each renderer, and retail ET
clients have joined fnTech3 servers, pure or not, on all six maps. Demo
playback is not implemented, and play from fnTech3's client on a retail
server remains open. See the
[ET gate matrix](fntech3/COMPATIBILITY.md#wolfet-gate-matrix).

**Notes:**

- Protocol 84. ET defaults to `sv_pure 1`, as retail does.
- PunkBuster is not available: an ET server reports it unavailable and keeps
  `sv_punkbuster 0`.
- The engine package carries audio zones for the six stock maps, and HUD
  rules: on a widescreen display the HUD keeps its 4:3 proportions at the
  screen's corners and the world view widens. The scoreboard and the
  expanded command map are centered 4:3 screens, and the limbo panel is drawn
  stretched, as retail drew it ([Aspect Handling Guide](ASPECT_CORRECTION.md)).
- The [ET implementation record](fntech3/2026-09-17-wolfet-contracts.md)
  describes the profile's contracts.

## Star Trek Voyager: Elite Force (`stvef-sp`, `stvef-mp`)

**Installation:** an Elite Force 1.20 installation with `baseEF/`. Single
player and Holomatch share it; always name the mode.

**Status:** experimental; developer builds with `-Dstvef-development=true`.
Single player needs Windows x86.

- Single player: the retail game module runs every single-player map, and the
  retail menus and client game run in the client, which plays the levels with
  their ambient sound, saves them and moves between them. Speakers' mouths
  move with their voices, and the Bink movies play through the installation's
  own Windows x86 Bink runtime. Image parity with retail and whole-campaign
  play remain open.
- Holomatch: the retail QVMs run and the retail demos play. fnTech3 has played
  with, and downloaded from, Lilium Voyager and the retail game, as client and
  as server, including pure servers, and with Lilium Voyager also over its
  protocol 26. The dedicated server also runs on Linux i686.

See the [Elite Force gate matrix](fntech3/COMPATIBILITY.md#elite-force-gate-matrix).

**Notes:**

- Single player uses protocol 40 and the `efgamex86.dll` and `efuix86.dll`
  libraries in the installation folder. Holomatch uses protocol 24, and also
  accepts ioEF's and Lilium Voyager's protocol 26.
- Elite Force's sounds and music are MP3. The MP3 decoder is built by default
  (`-Dmp3=true`) and registered only for the games that need it: Elite Force,
  Alice and F.A.K.K.2.
- German installations play their own voices, and the menus use the key names
  of the installation's language.
- On a widescreen display the LCARS gauges keep their shape in the lower
  corners and the world view widens. Single player's client game is given the
  4:3 frame, so the brackets it draws around a target stay on it
  ([Aspect Handling Guide](ASPECT_CORRECTION.md)).

## Star Wars Jedi Knight: Jedi Academy (`jka-sp`, `jka-mp`)

**Installation:** a Jedi Academy 1.01 installation with `base/`.

**Status:** experimental and **dedicated server only**; developer builds with
`-Djka-development=true`. A multiplayer server runs the retail module on a
retail map with bots. No client can connect yet, nothing is drawn, and the
network protocol has no message bodies. A single-player server loads a map's
collision and then stops at the single-player module, which is not
implemented. See the
[Jedi Academy gate matrix](fntech3/COMPATIBILITY.md#jedi-academy-gate-matrix).

## Star Wars Jedi Knight II: Jedi Outcast (`jk2-sp`, `jk2-mp`)

**Installation:** a Jedi Outcast 1.02a installation with `base/`.

**Status:** experimental and **dedicated server only**; developer builds with
`-Djk2-development=true`. A multiplayer server runs the retail
`jk2mpgame.qvm` with bots. No client connects and no packet is encoded yet,
and no single-player module runs. See the
[Jedi Outcast gate matrix](fntech3/COMPATIBILITY.md#jedi-outcast-gate-matrix).

## American McGee's Alice (`alice`)

Alice does not run a Quake III engine: it runs Ritual Entertainment's
Heavy Metal: F.A.K.K.2 engine, so its maps, models, menus, network messages,
saves and sounds are Ritual's formats, served by their own adapters in
fnTech3.

**Installation:** a retail Alice installation with `base/pak0.pk3` and the
game libraries `base/fgamex86.dll` and `base/cgamex86.dll`.

**Status:** experimental; developer builds with `-Dalice-development=true`.
In the Windows x86 developer client the retail game and client-game libraries
run. The intro's logos lead to the menus, New Game plays the opening movie into
the first level, and `garden1` draws Alice in third person with her HUD on all
three renderers. Games save and load back, and demos record and play back. No
client, server, save or demo has yet been exchanged with the retail game, and
no frame has been compared with its own. See the
[Alice gate matrix](fntech3/COMPATIBILITY.md#alice-gate-matrix).

**Notes:**

- The menus are the engine's own `.urc` resources, as in retail; there is no
  menu module.
- The engine package carries an audio zone and a static light file for each of
  the 36 maps, and HUD rules: on a widescreen display the gauges and cards keep
  their 4:3 size at the screen's edges, and the world view widens
  ([Aspect Handling Guide](ASPECT_CORRECTION.md)).
- Alice draws with no overbright bits, as its own engine does:
  `r_overBrightBits` and `r_mapOverBrightBits` default to `0`, so each map's
  light is drawn as the map holds it. The other games' `1` over `2` draws
  Alice's levels two to three times as bright
  ([Display Guide](DISPLAY.md#game-specific-renderer-settings)).
- As in retail, `+set config <name>` picks the configuration file and names
  the saves. Retail kept that name in the registry for later launches;
  fnTech3 keeps it for the launch that names it.

## Heavy Metal: F.A.K.K.2 (`fakk2`)

F.A.K.K.2 runs Ritual's own engine, the one Alice's descends from. fnTech3
serves it through Alice's adapters, with F.A.K.K.2's own records where the two
differ.

**Installation:** an *installed* game with `fakk/pak0.pk3`,
`fakk/gamex86.dll` and `fakk/cgamex86.dll`. A copy of the disc is not enough:
the disc keeps the game libraries inside its InstallShield cabinets.

**Status:** experimental; developer builds with `-Dfakk2-development=true`.
A developer server spawns all thirty maps through the retail game module. The
Windows x86 developer client runs the retail client game, draws the menus, HUD
and inventory, follows New Game and the level exits with the player's state
carried, saves and loads, and records and plays back demos. English and German
installations both run. Against the official 1.02 patch's `fakk2.exe`, the
retail game plays fnTech3's demos and joins fnTech3's server on all thirty
levels. fnTech3's client joins the retail game's server, and saves made on the
first level load in both. The retail server has run only that first level,
and longer sessions and most of the game's frames have not been compared with
retail. See the
[F.A.K.K.2 gate matrix](fntech3/COMPATIBILITY.md#fakk2-gate-matrix).

**Notes:**

- The engine package carries an audio zone and a static light file for each of
  the 30 maps, and HUD rules that keep the health gauge, the weapon rings and
  the boss's bar in shape and in their corners on a widescreen display, where
  the world view also widens. `cl_hudAspect 0` stretches the HUD with the
  screen instead ([Aspect Handling Guide](ASPECT_CORRECTION.md)).
- The disc's `fakk2.exe` is encrypted on disk, but the official 1.02 patch's
  is not, and fnTech3 follows it. The
  [F.A.K.K.2 contracts record](fntech3/2026-09-24-fakk2-contracts.md) cites
  what was read from it and marks the one question still taken from Alice's
  engine.
- As in retail, `+set config <name>` picks the configuration file and names
  the saves. Retail kept that name in the registry for later launches;
  fnTech3 keeps it for the launch that names it.

## Medal of Honor: Allied Assault (`mohaa`)

Medal of Honor runs 2015's engine, which grew from Ritual's and from Quake
III's. fnTech3 serves it with adapters of its own.

**Installation:** version 1.00 with `main/Pak0.pk3` to `Pak5.pk3`,
`main/gamex86.dll`, `main/cgamex86.dll` and, beside `MOHAA.exe`,
`system86.dll`, the allocator both game libraries load. fnTech3 loads the
installation's copy and never ships it.

**Status:** experimental; developer builds with `-Dmohaa-development=true`.
A developer server spawns all fifty-four maps through the retail game module.
The Windows x86 developer client runs the retail client game and plays every
single-player map, briefing and the credits through their scripts: the
engine's retail menus, HUD, loading screens with their bar, briefing slides,
objectives, subtitles and fades, the levels' music and dialogue, the sun's
lens flare, a level exit carrying the player's state, saves, loads and demos.
Over UDP, clients find a dedicated server in the LAN list and play
free-for-all, team and objective matches, fighting and scoring. The retail
`MOHAA.exe` is encrypted and cannot run on current Windows, so nothing has been
exchanged with or compared against the retail game. See the
[Medal of Honor gate matrix](fntech3/COMPATIBILITY.md#medal-of-honor-gate-matrix).

**Notes:**

- The menus, HUD and loading screens are the engine's own, built from the
  game's `ui/*.urc` files, as in retail; there is no menu module.
- The engine package carries an audio zone and a static light file for each of
  the 54 maps.
- The HUD scales with the screen's height; `ui_hudScale 0` draws it at
  retail's pixel sizes. On a widescreen display the world view widens to keep
  its 4:3 vertical framing ([Aspect Handling Guide](ASPECT_CORRECTION.md)).
- The game's console cheats need `+set thereisnomonkey 1`, as retail's did;
  fnTech3 sets the game's `cheats` switch as 2015's engine does.
- Levels draw their permanent decals, and models lose detail with distance
  by the game's own level-of-detail records; the advanced options' "Static
  Decals" box and model detail choice control them. The developer decal
  editor (`dcl_editmode 1` before a level loads, then the F6 menu of
  `development.cfg`) and level-of-detail tool (`lod_spawn`, the `l` menu)
  write the game's own `.dcl` and `.lod` files into the home path.
- Decals reach terrain and moving brushes such as doors, whose marks move
  with them. Bullet holes leave terrain alone, as the original renderer's
  do; `ter_minMarkRadius 0` lets every decal mark it.
- In multiplayer, the game's `say` and `sayteam` (`y` and `t`) open fnTech3's
  chat line; the chat itself appears in the box at the top of the screen.
- Savegames are fnTech3's own header beside the game's level file, so they
  cannot be exchanged with retail's; the
  [Medal of Honor contracts record](fntech3/2026-09-24-mohaa-contracts.md)
  says why and lists every other case where the retail libraries or
  OpenMoHAA's reconstruction stand in for the unreadable engine.

## Related documentation

- [Build Guide](../BUILD.md) for developer builds and the Visual Studio Code
  launches for each game.
- [Compatibility ledger](fntech3/COMPATIBILITY.md) for the evidence and open
  gates of every profile.
- [Architecture overview](fntech3/ARCHITECTURE.md) for how profiles, adapters
  and shared subsystems fit together.
