# Games Guide

fnTech3 runs one game at a time. When it starts, it picks the game -- and, for
games with separate single-player and multiplayer programs, the mode -- and
keeps it until you quit. This guide lists the games, shows how to start each
one, explains playing online, and says plainly what works in each game today.

fnTech3 contains no game files: every game needs your own installed copy, and
fnTech3 plays it without changing it. For setting fnTech3 up the first time,
start with [Getting Started](GETTING_STARTED.md).

## Games at a glance

| Game | Status | What you need |
| --- | --- | --- |
| Quake III Arena | **Playable** | A Steam or GOG copy, or a CD copy with the 1.32 patch |
| Quake Live | **Playable** on Windows | The Steam copy |
| Return to Castle Wolfenstein | Experimental | An installed copy |
| Wolfenstein: Enemy Territory | Experimental | An installed copy, such as Steam's |
| Star Trek Voyager: Elite Force | Experimental | Version 1.20 |
| Star Wars Jedi Knight: Jedi Academy | Experimental, early | Version 1.01 |
| Star Wars Jedi Knight II: Jedi Outcast | Experimental, early | Version 1.02a |
| American McGee's Alice | Experimental | An installed copy |
| Heavy Metal: F.A.K.K.2 | Experimental | An *installed* copy, not just the disc |
| Medal of Honor: Allied Assault | Experimental | Version 1.00 |

Two early test versions of Quake III Arena from 1999 are also supported for
testing: the IHV Test and the public Q3Test releases.

**Playable** games start from the normal download. **No game is finished
yet**, though: fnTech3 is still being checked against the original releases,
and the
[compatibility ledger](fntech3/COMPATIBILITY.md) records what has been tested
so far.

RTX's new liquid reflections and refraction have been compared on Quake
III's `q3dm12`. The shared path is available to the other games, whose liquid
materials still need their own checks; see the [RTX guide](RTX.md#what-you-will-see).

Dynamic-light and sun shadows now include Enemy Territory's soldiers and the
characters of Alice, F.A.K.K.2 and Medal of Honor; Return to Castle
Wolfenstein's characters also cast and receive sun shadows. Alice, F.A.K.K.2
and Medal of Honor use the sun specified by each level. Sun shadows preserve
ambient lighting and fixed-brightness artwork, and follow cutout leaves and
fences. See the [Display guide](DISPLAY.md#dynamic-lighting-and-shadowing)
for settings and the [sun-shadow comparison](fntech3/2026-10-22-csm-lighting-fidelity.md)
for the tested scenes and remaining limits.

**Every game profile is available in every build**, including the normal
download. **Experimental** means support is incomplete: expect missing features
and possible crashes, and read the game's section below for what works. No
development option is needed. Games whose retail modules exist only for Windows
still require the Windows build.

## Games and profiles

Each game, or each mode of a game, has a short name -- its **profile** -- that
you use to start it. Several other spellings are accepted too.

| Game | Mode | Name to use | Also accepted | Game folder | Settings file | Starts in |
| --- | --- | --- | --- | --- | --- | --- |
| Quake III Arena | | `quake3` | `q3`, `quake3arena` | `baseq3` | `q3config.cfg` | the normal download |
| Quake III Arena IHV Test (February 1999) | | `q3-ihv` | `quake3-ihv`, `q3ihv`, `ihv` | `baseq3` | `q3config.cfg` | the Windows download |
| Quake III Arena Q3Test 1.02 to 1.08 (1999) | | `q3test` | `q3t`, `quake3-test` | `demoq3` | `q3config.cfg` | the Windows download |
| Quake Live | | `quakelive` | `ql` | `baseq3` | `qzconfig.cfg` | the normal download |
| Return to Castle Wolfenstein | single player | `rtcw-sp` | `rtcwsp`, `wolfsp` | `main` | `wolfconfig.cfg` | the normal download |
| | multiplayer | `rtcw-mp` | `rtcwmp`, `wolfmp` | `main` | `wolfconfig_mp.cfg` | the normal download |
| Wolfenstein: Enemy Territory | | `wolfet` | `et`, `enemy-territory`, `enemyterritory` | `etmain` | `etconfig.cfg` | the normal download |
| Star Trek Voyager: Elite Force | single player | `stvef-sp` | `efsp`, `stvoy`, `eliteforce-sp` | `baseEF` | `efconfig.cfg` | the normal download |
| | Holomatch | `stvef-mp` | `stvef-hm`, `holomatch`, `efhm`, `stvoyhm` | `baseEF` | `hmconfig.cfg` | the normal download |
| Star Wars Jedi Knight: Jedi Academy | single player | `jka-sp` | `jasp`, `jediacademy-sp`, `jedi-academy-sp` | `base` | `jaconfig.cfg` | the normal download |
| | multiplayer | `jka-mp` | `jamp`, `jediacademy-mp`, `jedi-academy-mp` | `base` | `jampconfig.cfg` | the normal download |
| Star Wars Jedi Knight II: Jedi Outcast | single player | `jk2-sp` | `jk2sp`, `jedioutcast-sp`, `jedi-outcast-sp` | `base` | `jk2config.cfg` | the normal download |
| | multiplayer | `jk2-mp` | `jk2mp`, `jedioutcast-mp`, `jedi-outcast-mp` | `base` | `jk2mpconfig.cfg` | the normal download |
| American McGee's Alice | | `alice` | `americanmcgeesalice`, `mcgee-alice`, `alice2000` | `base` | `config.cfg` | the Windows download |
| Heavy Metal: F.A.K.K.2 | | `fakk2` | `heavymetal-fakk2`, `heavymetalfakk2`, `hmfakk2` | `fakk` | `config.cfg` | the Windows download |
| Medal of Honor: Allied Assault | | `mohaa` | `medalofhonor`, `moh-aa`, `alliedassault` | `main` | `configs/unnamedsoldier.cfg` | the Windows download |

The **game folder** is the folder inside the game's installation that holds
its data, and the **settings file** is where your settings are saved, inside
that folder (Quake Live keeps `qzconfig.cfg` in its profile folder instead).

Return to Castle Wolfenstein, Elite Force, Jedi Academy and Jedi Outcast each
have two modes that share one installation. Each mode is started on its own,
with its own settings file. A dedicated server also keeps its own settings
file, such as `q3config_server.cfg` for Quake III Arena and Quake Live,
`wolfconfig_server.cfg` for both Return to Castle Wolfenstein modes and
`etconfig_server.cfg` for Enemy Territory.

## Choosing the game

The [launcher](LAUNCHER.md) does all of this for you: it finds each game you
have installed and starts it with its profile and folder already named. This
section explains what it passes, for shortcuts and the command line.

### Naming the profile

Name the game with `com_gameProfile`, and its installation folder with
`fs_basepath`, when you start fnTech3:

```powershell
.\fntech3.exe +set com_gameProfile quake3 +set fs_basepath "C:\Games\Quake III Arena"
```

[Getting Started](GETTING_STARTED.md#choosing-the-game-yourself) shows how to
put this in a Windows shortcut. The game is chosen once, when fnTech3 starts;
changing `com_gameProfile` later, or in a settings file, does nothing. A game's
name without its mode, such as `rtcw`, is refused with a request to pick the
mode.

### Automatic detection

Without `com_gameProfile`, fnTech3 looks for exactly one installed game, in
this order, and stops at the first place where it finds one:

1. the folder named by `+set fs_basepath`;
2. the folder `fntech3.exe` is in;
3. Steam's copies of Quake III Arena, Quake Live, Return to Castle Wolfenstein
   and Enemy Territory.

It recognises a game by its main data file, such as `baseq3/pak0.pk3` for
Quake III Arena, and checks the file's contents rather than trusting its name.
Detection stops with a message when it finds two different games in one
place, a game whose two modes share an installation (Return to Castle
Wolfenstein, Elite Force and both Jedi Knight games always need the mode
named), a main file with the wrong contents, an experimental game in the
normal download, or nothing at all. The message lists the games this build of
fnTech3 can start. Naming the game skips detection altogether.

### Checking and switching

- `gameList` in the console lists every game and mode, marks the running one
  with `*`, and says whether this build of fnTech3 can start each one.
- The window's title names the game that is running.
- `gameMode <profile>` switches to the other mode of the same game, such as
  `gameMode rtcw-mp` from Return to Castle Wolfenstein's single player. It
  saves your settings, closes the game and starts fnTech3 again in the other
  mode, keeping the installation folder, the home folder and the
  fullscreen setting. A mod (`fs_game`) is not carried across. Return to
  Castle Wolfenstein's own menu buttons for switching mode do the same.

To play a different game, quit and start fnTech3 again with that game named.

## Where files are read and written

- **`fs_basepath`** is the game's installation. fnTech3 only reads it, as long
  as `fs_homepath` points somewhere else. If you leave it out, fnTech3 uses
  its own folder when the game is installed there, and otherwise the game's
  Steam installation where Steam sells it.
- **`fs_homepath`** is your home folder, which receives settings, saved
  games, screenshots, demos and downloads, inside the game's folder (or the
  mod's). Where it is by default:
  - **Windows:** Quake Live uses its usual profile folder,
    `<Quake Live>\<SteamID64>` -- the one named with your Steam account's long
    number -- as the original does; every other game uses its installation,
    as the original game did.
  - **Linux:** `~/.q3a` for every game. Several games use the same game
    folder names (`baseq3`, `main`, `base`), so give each one a home of its
    own.
  - Anywhere: `+set fs_homepath <folder>` when you start fnTech3 chooses
    another.
- **`fs_game`** starts a mod: a folder inside the game's installation, as in
  the original games.
- **Entity overrides.** A map's `maps/<map>.ent` file is read like any other
  game file -- from the home folder or a `.pk3` -- and the entity editor saves
  it into your home folder ([Entity Editor Guide](ENTITY_EDITOR.md)).

Give each game its own home folder, and the original installation stays
exactly as it was:

```powershell
.\fntech3.exe +set com_gameProfile rtcw-sp +set fs_basepath "D:\Games\RTCW" +set fs_homepath "D:\fnTech3-homes\rtcw-sp"
```

These files from the download stay beside `fntech3.exe`:

- `fntech3-pkg.fnz`, fnTech3's own data for each game -- sound zones, map
  lights, fog, widescreen HUD layouts and sound tuning -- kept apart per game
  so no game sees another's. It never holds any game's original files. Its
  layout is described in the [package reference](fntech3/PACKAGES.md).
- `fnql-web.pak`, fnTech3's extra settings pages for Quake Live's menus.
- The renderers `fnql_glx_x86`, `fnql_vk_x86` and `fnql_rtx_x86`.

## What fnTech3 adds to every game

Each game keeps its own rules: fnTech3 follows its game code, online play,
demos, saved games and gameplay as the original did, as far as that game's
support has come (its section below says how far). How the game *looks and
sounds* is
fnTech3's own, and every game gets the same features, using that game's own
maps, models and sounds:

- three renderers -- GLx, Vulkan and RTX -- with HDR, anti-aliasing, render
  scaling, glow, smooth per-pixel lights with shadows, lights for each map's
  own lamps, and more ([Graphics guide](DISPLAY.md),
  [RTX guide](RTX.md));
- 3D sound, with distance, sounds muffled by walls, room echo, sound zones
  and headphone surround, tuned to each game's own sound ranges
  ([Sound guide](AUDIO.md));
- a scalable console with suggestions, copy and paste
  ([Console guide](CONSOLE.md));
- better screenshots, map previews and cube maps
  ([Screenshots guide](SCREENSHOTS.md));
- an in-game entity editor, and `maps/<map>.ent` entity overrides that every
  game's server loads in place of the map's own entities
  ([Entity Editor Guide](ENTITY_EDITOR.md));
- widescreen field of view and HUD correction for every game made for 4:3
  screens -- all of them except Quake Live, which handles widescreen itself,
  and Medal of Honor's HUD, which places itself
  ([Widescreen guide](ASPECT_CORRECTION.md)).

Where a game's original look or sound differs, the original is kept as an
option you can choose, never as the default -- for example the original
software sound mixer, `s_backend legacy`. Each guide notes which features
apply to which game.

## Playing online

Multiplayer games play online the way the originals did: fnTech3 joins
servers running the original game, and players with the original game can
join fnTech3's servers, whether dedicated or started from the menu -- as far
as each game's section below says. Short sessions -- joining, chatting, map
changes, reconnecting, being kicked, a server shutting down, downloads, and
original and fnTech3 players on one server -- have been tried with the
original Quake III, Return to Castle Wolfenstein, Enemy Territory and Elite
Force ([details](fntech3/2026-10-05-legacy-server-hosting.md)).

- **Ports.** Each game uses its original port: 27960 for Quake III, Quake
  Live, Return to Castle Wolfenstein, Enemy Territory and Elite Force, 29070
  for Jedi Academy, 28070 for Jedi Outcast and 12203 for Medal of Honor.
  `net_port` changes it. `connect` without a port, and the LAN server list,
  use the game's own port. A Medal of Honor server also answers GameSpy's LAN
  queries on a second port, the first free one from 12300
  (`net_gamespy_port`; `sv_gamespy 0` turns it off), where GameSpy's server
  browsers and tools look. fnTech3's Medal of Honor LAN list looks in both
  places.
- **Server lists.** A public server (`+set dedicated 2`) announces itself to
  the original game's master server list, and the in-game browser asks the
  same lists. Many of those lists have shut down: add a community list as
  `sv_master2` to `sv_master5`, on the server and on your own copy alike.
  `sv_master 0` keeps a server off every list. Quake Live finds servers
  through Steam, and the lists for Medal of Honor and the Quake III test
  versions are gone, so those games use none.
- **Ping limits.** `sv_minPing` and `sv_maxPing` turn away internet players
  whose ping is outside them, as the original games did; LAN players are never
  turned away. Quake Live has neither.
- **Single-player game types.** A server started from the menu answers no
  LAN list and lets no other player in while it runs a single-player game
  type, as Quake III's engine does. Jedi Academy's original engine answered
  in every game type, and fnTech3 does the same for it. `spmap` starts each
  game's own single-player type, 5 in Jedi Academy and 4 in Jedi Outcast
  ([details](fntech3/2026-10-06-jedi-listen-server-game-types.md)).
  A Quake Live match started from the menu follows its server type
  instead, as the original does: offline (`sv_serverType 0`, the default)
  keeps it to you alone, LAN (`1`) lets in players on your local network,
  and Internet (`2`) anyone
  ([details](fntech3/2026-10-06-ql-listen-server-type.md)).
- **Server chat.** Type `say` on a server's console to talk to its players --
  or `svsay` in Elite Force and both Jedi Knight games, as in the originals;
  `say` works in every game.
- **Mods.** Joining a server that runs a mod switches to that mod and back
  again afterwards, keeping the console's log file and the `developer` and
  `logfile` settings.
- PunkBuster anti-cheat is not available in any game.

## Quake III Arena (`quake3`)

**What you need:** an installed Quake III Arena with `baseq3/pak0.pk3`, such
as the Steam or GOG release. Team Arena runs as a mod:
`+set fs_game missionpack`.

**Status:** playable in the normal download, on Windows and Linux, including
the dedicated server. The original game code runs, the demos that came with
the game play, and short online sessions with the original Steam release work
both ways: fnTech3 on original servers, and the original game on fnTech3's
servers. Long sessions, downloads and play across the internet are still
being checked ([details](fntech3/COMPATIBILITY.md#compatibility-and-gate-matrix)).

### Older versions of Quake III Arena

One `quake3` profile covers Quake III's releases from 1.11 to 1.32c:

| Version | What fnTech3 does |
| --- | --- |
| 1.32, 1.32b and 1.32c (Steam, GOG) | Plays, hosts and records demos (`.dm_68`), with the game's own code and demos, and plays online with the original release. This is the default. |
| ioquake3 and similar engines | Plays, hosts and records demos (`.dm_71`). Not an id release. |
| 1.31 | Joins original 1.31 servers and hosts 1.31 players; demos play in both engines. |
| 1.29f to 1.30 | The same as 1.31, including 1.29's own `pak6.pk3`. |
| 1.27g and 1.27h | Plays their demos (`.dm_48`). |
| 1.25 and 1.17 | Plays their demos. |
| 1.09 Demo Test, 1.11 and its demo, and 1.14 to 1.16 | Plays their demos (`.dm3`). |
| Q3Test 1.02 to 1.08 | Has a profile of its own, [below](#quake-iii-arena-q3test-q3test). |
| IHV Test, February 1999 | Has a profile of its own, [below](#quake-iii-arena-ihv-test-q3-ihv). |

You do not need to do anything to join an older server: fnTech3 asks the
server which version it runs and joins it as that version. To play from a
complete 1.29 to 1.31 installation, or to host for players of those versions,
choose the version's network protocol when you start: `66` for 1.29 to 1.30,
or `67` for 1.31. The installation must then hold that version's data files:
`pak0` to `pak6` for 66, and up to `pak7` for 67.

```powershell
.\fntech3.exe +set com_gameProfile quake3 +set protocol 66 +set fs_basepath "C:\Games\Quake III Arena 1.30"
.\fntech3.ded.exe +set com_gameProfile quake3 +set protocol 67 +set fs_basepath "C:\Games\Quake III Arena 1.31" +map q3dm1
```

Play with versions 1.29 to 1.31 has been tried in short sessions both ways.
Versions before 1.29 play their demos but cannot go online. The
[point-release record](fntech3/Q3_POINT_RELEASES.md) has every version's
details.

**Good to know:** fnTech3 brings sound zones, fog for Quake III and Team Arena
maps, weapon sound tuning and a widescreen HUD layout for Quake III Arena.

## Quake III Arena IHV Test (`q3-ihv`)

The test version id Software gave graphics-card makers in February 1999 is
different enough from the released game -- its own game code, network
protocol and map format -- to need its own profile.

**Status:** experimental, Windows only. The dedicated server runs the original game
code on its four maps, and the client runs the test's own client game: open
the console and type `demo sli` to play its demo, or `connect <address>` to
join a server running the original IHV Test. There is no menu beyond a title
screen. `record <name>` records a demo the original IHV Test can play too,
and `stoprecord` ends it. Commands the game itself knows, such as `kill`,
reach the server from the console. The portal on `km_portal` shows the view
through it, turning as in the original. fnTech3's own server takes players too, from the
original IHV Test or from fnTech3; name it with `hostname`, as the original does. The IHV Test cannot download files, so
every player needs the maps already, and no IHV Test server checks players'
files: `sv_pure` has no effect. The
[IHV record](fntech3/Q3_IHV.md) has the details.

## Quake III Arena Q3Test (`q3test`)

The public test versions of 1999 -- 1.02 and 1.03 as servers only, and 1.05
to 1.08 with a client -- each differ from the others and from the released
game. One `q3test` profile serves them all, and recognises which version is
installed when it starts, so keep each version in a folder of its own.

**What you need:** a Q3Test version laid out as its installer left it, with
`quake3.exe` and its `*x86.dll` files in the main folder and its data in
`demoq3/`. fnTech3 runs the version's own game code and never writes to its
folder.

**Status:** experimental, in the Windows download. Every version's server runs every map, for up
to 128 players. From 1.05 on, the client joins fnTech3's and the original
servers, draws the version's menus in every renderer -- 1.05, which shipped
none, gets fnTech3's own -- and
records and plays demos -- the ones that came with it too -- with their
sound. Controls, real matches and games with several players are not yet
checked. The [Q3Test record](fntech3/Q3TEST.md) has the details.

**Good to know:** the original Q3Test programs only read their own data, so
they cannot play a demo or a map made elsewhere; fnTech3's own demos are
checked by playing them back in fnTech3 and with a separate demo reader
instead. No Q3Test version checks players' files, so `sv_pure` has no effect
on its servers.

## Quake Live (`quakelive`)

**What you need:** the Steam release, normally in
`C:\Program Files (x86)\Steam\steamapps\common\Quake Live`. fnTech3 finds it
through Steam. `+set fs_steampath` adds another Steam copy to look in.

**Status:** playable in the normal download. Quake Live's client game code
and menus only exist for Windows, so its client runs on Windows; on Linux,
fnTech3 runs a Quake Live dedicated server with Quake Live's own Linux server
code. Short signed-in sessions with the original Quake
Live work both ways ([details](fntech3/COMPATIBILITY.md#compatibility-and-gate-matrix)).

**Good to know:**

- Quake Live's own menus run, using the web runtime in your installation, on
  Windows; they need Quake Live's own `web.pak`. fnTech3 adds pages of its
  own settings to them; elsewhere the engine's own menus are used.
- Steam features need a separately supplied Steam add-on that fnTech3's
  download does not include. Without it they say they are unavailable rather
  than pretending to work; the [Steam notes](fnql/STEAM_PROVIDER.md) explain.
- Settings are kept in `qzconfig.cfg` and `repconfig.cfg` in your Quake Live
  profile folder, as in the original. The first time fnTech3 starts without a
  `qzconfig.cfg`, it brings over the settings from an existing
  `baseq3/q3config.cfg` once.
- fnTech3 brings a sound zone and a fog setting for each of the 149 maps that
  come with Quake Live, and weapon sound tuning.
- Quake Live's engine services -- its menus, Steam, fonts and more -- are
  described for the technically minded in the
  [FnQL documents](fnql/README.md).

## Return to Castle Wolfenstein (`rtcw-sp`, `rtcw-mp`)

**What you need:** one installed Return to Castle Wolfenstein, with its
`main` folder. Single player and multiplayer share it, so always name the
mode.

**Status:** experimental, in the normal download.
Both modes run the original game code in the Windows client, with all three
renderers.

- **Single player** starts each of its 32 maps, with their menus and
  cutscenes, and saves and loads games -- including saves made with the
  original game, and the other way round. On the levels checked so far,
  reaching a level's exit takes you on to the next one with your weapons,
  ammunition and armour.
- **Multiplayer** starts each of its 15 maps, joins original servers and lets
  players with the original game join fnTech3's in short sessions, and plays
  demos recorded by the original game, which also plays fnTech3's.

Not done yet: the whole campaign from start to finish, long fights against
the computer's soldiers, and long online matches. See the
[RTCW details](fntech3/COMPATIBILITY.md#rtcw-gate-matrix).

**Good to know:**

- Each mode only loads its own data: single player skips the `mp_` files and
  multiplayer the `sp_` files. Single-player levels are started with `spmap`.
- Choosing the other mode in the main menu restarts fnTech3 in that mode
  (see [Checking and switching](#checking-and-switching)).
- fnTech3 brings sound zones and map lights for every map in both modes, and
  a widescreen HUD layout for each.

## Wolfenstein: Enemy Territory (`wolfet`)

**What you need:** an installed Enemy Territory with its `etmain` folder, such
as the Steam release.

**Status:** experimental, in the normal download, which runs both the client and the dedicated
server. The original game code draws Enemy Territory's menus and the limbo
screen, and loads all six maps in every renderer. Joining with both teams
works on all six maps, in both directions with the original game, including
servers that check players' files. Short demos play back normally and as
benchmarks, including demos swapped with the original game. Whole matches and
long play are still to be checked. See the
[Enemy Territory details](fntech3/COMPATIBILITY.md#wolfet-gate-matrix).

**Good to know:**

- Servers check players' files by default (`sv_pure 1`), as the original's
  do.
- Enemy Territory's own keys for activate, sprint, reload, zoom, lean and
  prone work as in the original.
- Sounds the game leaves silent, such as jumping, stay silent.
  `s_defaultsound 1` plays a placeholder sound for them instead, for testing.
- Demos are saved as `.dm_84`. Older `.dm_83` demos have not been checked
  yet.
- Enemy Territory comes without bot navigation files, and bots are not
  supported yet.
- **Downloads** use the server's web address when it has one, as the original
  does (`cl_wwwDownload 1`), and fall back to the normal download when that
  fails. Where the original opened your web browser for a server's download
  page (`sv_wwwFallbackURL`), fnTech3 disconnects and shows the address.
  Downloads have been tried both ways with the original game using a small
  file; large files and busy connections are still to be checked.
- PunkBuster is not available, and a server reports `sv_punkbuster 0`.
- fnTech3 brings sound zones for the six maps. On a widescreen display the HUD
  keeps its shape in the corners of the screen and the view widens; the
  scoreboard and the large command map stay as centred 4:3 screens, and the
  limbo screen is stretched, as the original drew it
  ([Widescreen guide](ASPECT_CORRECTION.md)).

## Star Trek Voyager: Elite Force (`stvef-sp`, `stvef-mp`)

**What you need:** Elite Force version 1.20, with its `baseEF` folder. Single
player and Holomatch share it, so always name the mode.

**Status:** experimental, in the normal download.
Single player needs Windows.

- **Single player** runs every level with the original game code, menus and
  sound, saves games and moves from level to level. Characters' mouths move
  as they speak, and the movies play using the movie player in your
  installation. The whole campaign plays through, from the first mission to
  the closing movie and credits, and so do the holodeck programmes and the
  Virtual Voyager tour. Matching the original's picture exactly is still to
  be checked.
- **Holomatch** runs the original game code and plays the original demos.
  fnTech3 has played with the original game and with Lilium Voyager, joining
  and hosting, including servers that check players' files, and downloaded
  from both. Every game type and Expansion Pack mode plays with bots as on
  the original server. The dedicated server also runs on Linux.

See the [Elite Force details](fntech3/COMPATIBILITY.md#elite-force-gate-matrix).

**Good to know:**

- Holomatch also understands the newer network protocol of ioEF and Lilium
  Voyager; play over it has been tried with Lilium Voyager.
- fnTech3 gives the game more memory than older settings files ask for, as
  its renderers need it.
- Elite Force's sound and music are MP3 files, which fnTech3 plays itself.
- German copies play their own voices, and the menus show key names in the
  installation's language.
- The original's default keys are X or `/` to jump and Space to Use.
  `inputstatus` in the console shows what currently has control of the mouse
  and keys, if they stop responding.
- On a widescreen display the LCARS gauges keep their shape in the lower
  corners and the view widens, and the brackets single player draws around a
  target stay on it ([Widescreen guide](ASPECT_CORRECTION.md)).

## Star Wars Jedi Knight: Jedi Academy (`jka-sp`, `jka-mp`)

**What you need:** Jedi Academy version 1.01, with its `base` folder.

**Status:** experimental and early, in the Windows download.

- **Multiplayer:** a server runs the original game code on an original map,
  with bots, and tells the game where a held saber or weapon is as the
  original server does, even while one animation blends into the next
  ([details](fntech3/2026-10-07-jka-server-pose-clock.md)). fnTech3 joins
  an original Jedi Academy server, including servers that check players'
  files, chats both ways and follows a map change. Its
  menus, the main menu's animated scene, text, some of the world and the
  player setup menu are drawn with fnTech3's renderers, and a short demo plays
  in each of them. On the new map after a map change, a player holding a
  weapon no longer stops it: the weapon is now placed in the hand as the
  original places it, though no picture of it has been checked yet
  ([details](fntech3/2026-10-06-jka-attachments.md)). Items that
  fade in, or that the game draws in a single colour, now draw too
  ([details](fntech3/2026-10-06-jka-entity-tint.md)). Each map's
  background sounds are now played as the original plays them, fading in
  and from one map's sounds to the next
  ([details](fntech3/2026-10-06-jka-ambient-sets.md)), and a map restart,
  reconnecting and the server shutting down all work. Ordinary positional
  sounds now reach fnTech3's sound system, and local hosting reaches the
  player menu and survives a map restart with sound enabled. Joining the
  match now plays the spawn effect, with its beam, the lights that climb it
  and its lightning, and play continues: a local match runs with the
  original settings, and an animation that comes to its end no longer
  stops it ([details](fntech3/2026-10-15-jka-play-blockers.md)). The see-through
  shimmer around a newly spawned player now bends and tints what is behind
  it, the way fnTech3's water does
  ([shimmer details](fntech3/2026-10-15-jka-entity-refraction.md)). On `mp/ffa2` the wall torches now burn, and a local match there
  runs to the end ([torch details](fntech3/2026-10-15-jka-particle-dynamics.md)).
  Sparks, sprays and dust from the game's effects now fly, trail and
  bounce off the floor ([effect details](fntech3/2026-10-15-jka-tails-and-physics.md)),
  and explosions now play their fireball pictures once instead of showing a
  plain grey square ([explosion details](fntech3/2026-10-15-jka-oneshot-animmap.md)).
  The lights on Hoth's doors no longer stop the game: a locked door shows
  red and an unlocked one green ([door light details](fntech3/2026-10-15-jka-setanimindex.md)),
  and `mp/siege_hoth` now plays as a free-for-all
  ([model skin details](fntech3/2026-10-16-jka-skinless-model.md)).
  The game's effects now make their sounds too, from where they happen: a
  test match has played the concussion rifle's and thermal detonator's
  muzzle flashes, a saber clash and a punch, though most guns still stop
  the game when fired ([sound details](fntech3/2026-10-15-jka-effect-sound.md)).
  After joining from the console, the Player Configuration menu that opens
  closes with Escape or `togglemenu`, `+altattack` fires a weapon's second
  mode, and the concussion rifle now plays its muzzle flash and sound when
  fired ([weapon fire details](fntech3/2026-10-15-jka-weapon-fire.md)).
  Muzzle flashes and explosions now shake your view a little when they go
  off right beside you, as in the original game: a test match has played
  the muzzle flashes of eight guns this way. A fired Bryar pistol now shows
  its whole muzzle flash
  ([shake details](fntech3/2026-10-15-jka-camera-shake.md)).
  The view weapon's muzzle flash now plays in first-person view too, with
  its sound, light and shake
  ([first-person flash details](fntech3/2026-10-15-jka-entity-effect.md)).
  `cl_viewShake` turns that shake down or off
  ([Graphics guide](DISPLAY.md#view-shake)).
  Shots now fly and are checked against a player's actual body, not just the
  box around it, as in the original: a shot through the gap beside an arm
  passes on. A test match has fired the concussion rifle's missile and beam
  and the Bryar pistol's bolt, though the concussion rifle's shots still
  stop the match when they hit a wall, at their explosion
  ([shot details](fntech3/2026-10-15-jka-g2-trace.md)).
  A Bryar pistol bolt that hits a wall now leaves a scorch mark there, as
  in the original. The game keeps the last 100 marks; older ones fade
  away. The `r_markcount` setting changes how many are kept, and `0` turns
  the marks off ([scorch mark details](fntech3/2026-10-15-jka-decal.md)).
  Longer matches, most other maps and fighting have not been checked yet
  ([spawn effect details](fntech3/2026-10-15-jka-spawn-effect.md)). The
  open-source OpenJK engine's client joins fnTech3's server. A lightsaber's
  glow and bright core are now built exactly as the original builds them,
  and a recorded match with a lit saber in it plays to its end, though no
  picture of a blade has been checked yet
  ([details](fntech3/2026-10-14-compatibility-round.md#jedi-academy-saber-glow-and-lines)).
  Siege games now start: the server keeps a round's time to beat for the
  next round, as the original does. Maps whose sky shows a distant view of its
  own now draw that view where the sky is, as the original does, and
  `mp/duel6` has been checked ([siege details](fntech3/2026-10-15-jka-siege.md)).
  The skies of `mp/duel9`, `mp/ctf3`, `mp/duel1`, `mp/duel4`, `mp/ffa2` and
  `mp/ffa3`, which showed plain grey, now draw
  ([sky details](fntech3/2026-10-15-raven-sun-keyword.md)). A siege game now opens its class
  menu when you join. Choosing a class with the mouse has not been checked
  yet; `team red` or `team blue` at the console joins a side as its first
  class. A round starts only once both sides have a player. With a bot on
  the other side `mp/siege_korriban` now plays its round
  ([round details](fntech3/2026-10-16-jka-trace-pass-none.md)), and a
  player or bot who is killed comes back into the match
  ([respawn details](fntech3/2026-10-16-jka-clear-skin-gore.md)). The game
  still stops when a dead player's body starts to fade
  ([body details](fntech3/2026-10-16-jka-surface-on-off.md)). Stormtroopers
  spawned from the console now appear, and a hit on a body is checked
  against the body itself; fights still stop on effects the client cannot
  draw yet ([collision details](fntech3/2026-10-22-jka-g2-collision.md)).
  The marks hits leave on players are now kept and cleared as the
  original keeps them, but they are not drawn yet, and fights stop before
  any mark is made ([marks details](fntech3/2026-10-17-jka-skin-gore.md)).
  `mp/siege_hoth` and `mp/siege_desert` no longer stop soon after the
  menu opens, and with a bot on the other side both players now enter
  the round instead of stopping there
  ([class menu details](fntech3/2026-10-15-jka-ui-menus.md),
  [model skin details](fntech3/2026-10-16-jka-skinless-model.md),
  [model removal details](fntech3/2026-10-22-jka-g2-surfaces.md)).
  Maps whose speakers and torches carry background sounds of their own, such
  as `mp/ctf3`, no longer stop on their first frame: those sounds now play
  where they are, and their occasional calls come at the times the game
  gives ([details](fntech3/2026-10-15-compatibility-round.md#jedi-academy-sets-heard-around-a-point)).
  The grass field on `mp/ctf3`, drawn as a black and white grid, now grows
  its grass in every renderer; `r_surfaceSprites 0` turns such grass off
  ([grass details](fntech3/2026-10-15-jedi-surface-sprites.md)).
  The blades now have the clean edges the original gives them, and no
  longer hide the grass behind them
  ([edge details](fntech3/2026-10-16-jedi-alphafunc-ge192.md)).
- **Widescreen:** the HUD and menus fill the window at any size, as the
  original's do. On a widescreen display the HUD keeps its shape at the
  screen's corners, the scoreboard is shown as a centred 4:3 screen and the
  view widens; `cl_hudAspect 0` stretches the HUD as the original did, and
  `cl_menuAspect 1` keeps the menus in a centred 4:3 area
  ([Widescreen guide](ASPECT_CORRECTION.md),
  [details](fntech3/2026-10-15-jedi-widescreen-hud.md)). The weapon, Force
  power and item bars stay whole at the bottom centre with their names
  beneath them, on taller screens too, and the loading screen's map picture
  fills the window behind a centred 4:3 screen
  ([live check](fntech3/2026-10-15-jka-hud-live-check.md)). The binoculars
  and the disruptor rifle zoom in, their sights stretched over the screen
  with every readout in place, and while zoomed you see further into a
  foggy map, as in the original
  ([details](fntech3/2026-10-15-jka-zoom.md)).
- **Single player:** the game's own menus now run -- the main menu, a new
  game's skill choice, loading, controls, setup and quitting -- but character
  creation's lists and models are still empty and there are no saved games
  to load yet. Starting a game runs the original game code, which stops on
  its first frame; the game's error box then says where
  ([details](fntech3/2026-10-16-jedi-sp-menus.md)).

The characters speak German, French or Spanish when `s_language` is
`deutsch`, `francais` or `espanol`, as the game's setup menus set it, and
English where a line has no recording in that language
([details](fntech3/2026-10-09-jka-foreign-voices.md)).

Not done yet: the campaign and saved games, the full game world and effects,
music, and playing in both directions with the original game. The original
Jedi Academy asks for disc 1 before it will connect to any server. See the
[Jedi Academy details](fntech3/COMPATIBILITY.md#jedi-academy-gate-matrix).

## Star Wars Jedi Knight II: Jedi Outcast (`jk2-sp`, `jk2-mp`)

**What you need:** Jedi Outcast version 1.02a, with its `base` folder.

**Status:** experimental and early, in the Windows download. A multiplayer server runs the original game code;
bots find opponents and fight with sabers in tested matches. Parts of the
client -- its network messages, menus and fonts -- are being built and tested
on their own. The main menu now draws its text, models and effects with
corrected fonts; client gameplay is
still incomplete ([latest audit](fntech3/2026-10-07-jedi-compatibility-round2.md)).
Its menus fill the window at any size, as the original's do
([details](fntech3/2026-10-15-jedi-widescreen-hud.md)).
Starting a multiplayer map now shows the level, your weapon, the HUD and the
scoreboard, and the HUD keeps its shape in wide windows while the view
widens ([widescreen details](fntech3/2026-10-22-fov-correction-all-profiles.md)); other players,
sabers and most effects have not been seen in a game yet
([details](fntech3/2026-10-16-jk2-cgame-ghoul2.md)).
Single player's game code loads, then stops before the game starts. Its own
menus now run -- the main menu, a new game, loading, controls, setup, the
credits and quitting -- and starting a game shows that stop in the game's
error box; there are no saved games to load yet
([details](fntech3/2026-10-16-jedi-sp-menus.md)). The
game's sounds and music, which are MP3 files, now load, and the multiplayer
menus play their sounds ([details](fntech3/2026-10-08-jk2-mp3-sounds.md)).
The characters speak German or French when `s_language` is `deutsch` or
`francais`, as the game's setup menu sets it
([details](fntech3/2026-10-08-jk2-foreign-voices.md)). In multiplayer, `+altattack` now fires a weapon's second mode, and holding a
key no longer counts as the force drain button ([details](fntech3/2026-10-15-jka-weapon-fire.md)). The
campaign, saves, demos, graphics, in-game sound and controls are still to
come. See the [Jedi Outcast details](fntech3/COMPATIBILITY.md#jedi-outcast-gate-matrix).

The ground of the Yavin levels, with its grass and reeds, now loads where
it showed the default picture, though none of it has been seen in a game
yet ([details](fntech3/2026-10-15-jedi-surface-sprites.md)).

That grass and those reeds are now cut out at their edges as the
original cuts them, though this too is still to be seen in a game
([details](fntech3/2026-10-16-jedi-alphafunc-ge192.md)).

## American McGee's Alice (`alice`)

Alice does not run on a Quake III engine but on Ritual Entertainment's
Heavy Metal: F.A.K.K.2 engine, so its maps, models, menus, saved games and
sounds are all different. fnTech3 has separate support for them.

**What you need:** an installed Alice with `base/pak0.pk3`, `base/fgamex86.dll`
and `base/cgamex86.dll`.

**Status:** experimental, in the Windows download. The original game code runs. The opening logos
lead to the menus, New Game plays the opening movie into the first level, and
the garden draws Alice and her HUD in all three renderers. Games save and
load, and demos record and play back. Nothing has yet been swapped with the
original game -- no saves, demos or online games -- and no picture has been
compared with the original's. See the
[Alice details](fntech3/COMPATIBILITY.md#alice-gate-matrix).

**Good to know:**

- The menus are the game's own menu files, drawn by the engine, as in the
  original.
- fnTech3 brings a sound zone and map lights for each of the 36 maps. On a
  widescreen display the gauges and cards keep their size at the edges of the
  screen and the view widens ([Widescreen guide](ASPECT_CORRECTION.md)).
- Alice draws each map at its own brightness, as its engine did:
  `r_overBrightBits` and `r_mapOverBrightBits` start at `0` here, where the
  other games' brighter values would make its levels two to three times too
  bright ([Graphics guide](DISPLAY.md#game-specific-renderer-settings)).
- The targeting glow is a game effect and needs no extra bloom setting
  ([effects details](fntech3/2026-10-12-alice-effects.md)).
- As in the original, `+set config <name>` picks the settings file and names
  your saved games. The original remembered the name for later; fnTech3 uses
  it for the session you name it in.

## Heavy Metal: F.A.K.K.2 (`fakk2`)

F.A.K.K.2 runs Ritual's own engine, the one Alice's grew from. fnTech3 serves
both with the same support, with F.A.K.K.2's own rules where the two differ.

**What you need:** an *installed* game with `fakk/pak0.pk3`, `fakk/gamex86.dll`
and `fakk/cgamex86.dll`. A copy of the disc is not enough: the disc keeps the
game's program files packed inside its installer.

**Status:** experimental, in the Windows download. A server starts all thirty maps with the
original game code. The client draws the menus, HUD and inventory, plays New
Game and the level exits with your progress carried over, saves and loads,
and records and plays demos. English and German copies both run. With the
official 1.02 patch, the original game plays fnTech3's demos and
joins fnTech3's server on all thirty levels; fnTech3 joins the original's
server, and saves from the first level load in both. The original's server
has only been run on that first level, and longer sessions and most of the
game's pictures have not yet been compared with the original. See the
[F.A.K.K.2 details](fntech3/COMPATIBILITY.md#fakk2-gate-matrix).

**Good to know:**

- fnTech3 brings a sound zone and map lights for each of the 30 maps. On a
  widescreen display the health gauge, the weapon rings and the boss's health
  bar keep their shape in their corners, and the view widens;
  `cl_hudAspect 0` stretches the HUD across the screen instead
  ([Widescreen guide](ASPECT_CORRECTION.md)).
- As in the original, `+set config <name>` picks the settings file and names
  your saved games. The original remembered the name for later; fnTech3 uses
  it for the session you name it in.

## Medal of Honor: Allied Assault (`mohaa`)

Medal of Honor runs the engine of 2015, Inc., the studio that made it, which
grew from both Ritual's and Quake III's. fnTech3 has separate support for it.

**What you need:** version 1.00, with `main/Pak0.pk3` to `Pak5.pk3`,
`main/gamex86.dll`, `main/cgamex86.dll`, and `system86.dll` beside
`MOHAA.exe`. fnTech3 uses your installation's copy of that file and never
includes it.

**Status:** experimental, in the Windows download. A server starts all fifty-four maps with the
original game code. The client plays every single-player level, briefing and
the credits: the game's menus, HUD, loading screens, briefing slides,
objectives, subtitles and fades, the music and speech, the sun's lens flare,
moving from level to level with your progress, saves, loads and demos. Over a LAN,
players find a dedicated server in the server list and play free-for-all,
team and objective matches. The original `MOHAA.exe` is copy-protected and no
longer runs on current Windows, so nothing has been compared with or played
against the original game. See the
[Medal of Honor details](fntech3/COMPATIBILITY.md#medal-of-honor-gate-matrix).

**Good to know:**

- Keep the default sound system (OpenAL): the older software mixer ignores the
  game's sound distances and pitch ([Sound guide](AUDIO.md)). When a level
  ends, or restarts after you are killed, the sound fades out as the game
  asks ([details](fntech3/2026-10-14-compatibility-round.md#medal-of-honor-sound-fades)).
- The menus, HUD and loading screens are the game's own menu files, drawn by
  the engine, as in the original.
- fnTech3 brings a sound zone and map lights for each of the 54 maps.
- Barrel explosions, their smoke and flying debris, and other hidden
  emitters now draw in GL, Vulkan and RTX; no extra setting is needed
  ([effects details](fntech3/2026-10-11-mohaa-effects.md)).
- RTX path tracing includes the game's terrain and static scenery,
  correcting holes in the ground on `m1l1` and the same omission elsewhere
  ([terrain details](fntech3/2026-10-11-mohaa-rtx-world.md)).
- The HUD grows with the screen's height; `ui_hudScale 0` draws it at the
  original's pixel sizes. On a widescreen display the view widens and keeps
  its 4:3 height ([Widescreen guide](ASPECT_CORRECTION.md)).
- Cheats in the console need `+set thereisnomonkey 1` when you start, as in
  the original.
- Levels show their permanent decals, and models lose detail with distance,
  as the game's own files describe; the advanced options' "Static Decals" box
  and model detail choice control them. The game's developer tools -- the
  decal editor (`dcl_editmode 1` before a level loads, then the F6 menu of
  `development.cfg`) and the level-of-detail tool (`lod_spawn`, the `l` menu)
  -- save the game's own `.dcl` and `.lod` files into your home folder.
- Decals mark the ground and moving things such as doors, and move with them.
  Bullet holes leave the ground alone, as in the original;
  `ter_minMarkRadius 0` lets every decal mark it.
- In multiplayer, the game's chat keys (`y` and `t`) open fnTech3's chat
  line; the messages appear in the box at the top of the screen.
- A multiplayer server, dedicated or started with Start New Server, answers
  GameSpy's LAN queries with its name, map, game type, players and rules; the
  menu's GameSpy box turns that off. GameSpy's internet lists are gone, so no
  server appears there ([details](fntech3/2026-10-06-mohaa-gamespy-lan-queries.md)).
  A team match started from the menu is listed and lets other players join.
- Join LAN Game's Refresh asks both a server's game port and GameSpy's
  ports, so it also finds servers started on other game ports. It lists only
  servers of version 1.00, the only version fnTech3 can join
  ([details](fntech3/2026-10-08-mohaa-gamespy-lan-search.md)).
- fnTech3's saved games use their own format, so they cannot be swapped with
  the original's; the
  [Medal of Honor record](fntech3/2026-09-24-mohaa-contracts.md) explains why.

## Related guides

- [Getting Started](GETTING_STARTED.md) for setting fnTech3 up, the console
  and settings files.
- [Launcher Guide](LAUNCHER.md) for finding your games and starting them with
  a click.
- [Build Guide](../BUILD.md) for compiling the engine; every game is included.
- [Compatibility ledger](fntech3/COMPATIBILITY.md) for every test and what is
  still open, for each game.
