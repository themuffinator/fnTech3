# Getting Started

fnTech3 is a modern engine for Quake III Arena, Quake Live and other games of
the id Tech 3 era. You keep your own copy of the game; fnTech3 takes the place
of the program that runs it, and adds sharper graphics, 3D sound, widescreen
support and many small comforts. This guide takes you from the download to
your first match, and explains the console and settings that the other guides
build on.

## What you need

- **A Windows or Linux PC.** fnTech3 is a 32-bit (x86) program, which runs on
  64-bit Windows and Linux too. The Windows version is the one tested most
  for playing; on Linux, fnTech3 is mainly tested as a dedicated server.
- **Your own copy of a game.** fnTech3 contains no game files. Quake III Arena
  (Steam, GOG, or a CD copy updated with the 1.32 Point Release patch) and
  Quake Live (Steam) play with the normal download. The
  other games are still experimental; the [Games guide](GAMES.md) explains
  what that means.
- **Up-to-date graphics drivers** with OpenGL or Vulkan. Ray tracing needs a
  little more; see the [RTX guide](RTX.md).

> [!NOTE]
> Quake Live's own menus and client game code only exist for Windows, so its
> client runs on Windows. On Linux, fnTech3 hosts Quake Live servers with
> Quake Live's own Linux server code.

## Install fnTech3

1. Download the latest release from the
   [Releases page](https://github.com/themuffinator/fnTech3/releases). On
   Windows take `fntech3-<version>-windows-msvc-x86.zip`; the `mingw` zip is
   the same engine built with another compiler. On Linux take
   `fntech3-<version>-linux-x86.tar.gz`.
2. Unpack it into a folder of its own, such as `C:\Games\fnTech3`.
3. Keep every file together. `fntech3.exe` looks beside itself for
   `fntech3-pkg.fnz`, `fnql-web.pak`, the `fnql_glx`, `fnql_vk` and
   `fnql_rtx` renderer files and the libraries that came in the zip.

Nothing is installed into Windows, and fnTech3 never changes the game's data
files; it does save your settings in the game's folder by default, as the
original game did (see [Where your files are saved](#where-your-files-are-saved)).
To update, unpack the new release over the old one; to remove fnTech3, delete
its folder.

## Start your first game

### The quick way

fnTech3 looks in Steam for Quake III Arena, Quake Live, Return to Castle
Wolfenstein and Enemy Territory. If it finds **just one**, and that one is
Quake III Arena or Quake Live, running `fntech3.exe` starts it. If it finds
more than one, it stops with an `ambiguous` message; name the game as below.

### Choosing the game yourself

Naming the game always works, and is the reliable way to start. On Windows,
make a shortcut:

1. Right-click `fntech3.exe` and choose **Create shortcut** (on Windows 11,
   under **Show more options**).
2. Right-click the new shortcut, choose **Properties**, and add the game's
   name and folder to the end of **Target**, after the closing quote.

For Quake III Arena from Steam, the whole **Target** reads:

```text
"C:\Games\fnTech3\fntech3.exe" +set com_gameProfile quake3 +set fs_basepath "C:\Program Files (x86)\Steam\steamapps\common\Quake 3 Arena"
```

Quake Live is found through Steam once it is named:

```text
"C:\Games\fnTech3\fntech3.exe" +set com_gameProfile quakelive
```

On Linux, the same settings go on the command line:

```sh
./fntech3 +set com_gameProfile quake3 +set fs_basepath "/path/to/Quake III Arena"
```

`com_gameProfile` is the game to start and `fs_basepath` is the folder the
game is installed in -- the one that holds `baseq3` for Quake III Arena. Every
game's name is in the [Games guide](GAMES.md#games-and-profiles).

> [!TIP]
> Add `+set r_fullscreen 0` to start in a window. **Alt+Enter** switches
> between a window and fullscreen at any time.

### If fnTech3 cannot find your game

fnTech3 stops with a message when it cannot tell which game to start:

| The message says | What to do |
| --- | --- |
| `not found` | Name the game and its folder, as above. |
| `unknown profile` | The game's name is misspelt; check it against the [list of names](GAMES.md#games-and-profiles). |
| `ambiguous` | More than one game was found; name the one you want with `com_gameProfile`. |
| `choose the game mode` | This game has single-player and multiplayer versions, such as `rtcw-sp` and `rtcw-mp`; name one. |
| `unrecognized base pak` | The folder looks like the game but its main file is not the one expected. Check the folder, or reinstall or verify the game. |
| `engine adapters are not yet complete` | The game is one of the experimental ones, which the normal download does not start. See the [Games guide](GAMES.md). |

## The console and settings

Almost everything in fnTech3 can be changed with **settings**: short names
with a value, like `r_fullscreen 0`. The menus change some of them; the rest
are typed into the console.

### Opening the console

Press the key **below Esc**, to the left of **1** (`~` on US keyboards), or
**Shift+Esc** on any keyboard. Type a command and press **Enter**. Press the
same key again to close it.

- Type a setting's name and a value to change it: `r_gamma 1.2`.
- Type just the name to see its value and, for most settings, what it does:
  `r_gamma`.
- Start typing and fnTech3 suggests matching commands and settings; **Tab**
  picks one. The [Console guide](CONSOLE.md) has more.

### Settings that need a restart

Some settings only take effect when part of the engine restarts:

- graphics settings: type `vid_restart`;
- sound settings: type `snd_restart`.

Each guide says when one is needed. Neither restart leaves your game.

### Keeping your settings

Settings you change are saved in the game's configuration file when you quit
-- `q3config.cfg` for Quake III Arena and `qzconfig.cfg` for Quake Live -- so
they are there next time.

To keep your own set of settings that is never overwritten, put them in a text
file called `autoexec.cfg` in the game's folder inside your home folder: for
Quake III Arena, the `baseq3` folder beside `q3config.cfg`; for Quake Live,
the `baseq3` folder inside your Quake Live profile folder. Every line is a
command, as you would type it in the console, and `seta` saves a setting too:

```cfg
// My fnTech3 settings
seta r_mode "-2"
seta r_fullscreen "1"
seta s_alHrtf "auto"
```

The examples throughout these guides use that form, so you can copy them into
`autoexec.cfg` or type them into the console a line at a time.

## Where your files are saved

fnTech3 saves your settings, screenshots, demos, saved games and downloads in
a **home folder**:

- **Windows:** the game's own installation folder, as the original game did.
  Quake Live uses its usual profile folder inside its installation, the one
  named with a long number.
- **Linux:** `~/.q3a`. Several games use the same folder names inside it, so
  give each game a home of its own, as below.

The game's own folder name sits inside the home folder, so Quake III Arena's
screenshots go in `baseq3\screenshots`.

> [!TIP]
> To keep fnTech3's settings apart from the original game's, give it a home
> folder of its own by adding this to the shortcut:
> `+set fs_homepath "C:\Users\<you>\Documents\fnTech3\quake3"`. Use a
> different folder for each game.

## Going back to the original game

fnTech3 does not change the game, so the original game still starts as
before. If both use the same home folder they also share the settings file, so
a setting changed in one appears in the other; a home folder of fnTech3's own
avoids that.

## Getting help

- The [Sound guide](AUDIO.md#troubleshooting),
  [GLx guide](GLX.md#troubleshooting) and
  [RTX guide](RTX.md#troubleshooting) each have a troubleshooting section.
- To report a problem, open an issue on
  [GitHub](https://github.com/themuffinator/fnTech3/issues). Say which game
  you played, what you did and what happened. Type `condump report.txt` in the
  console straight after the problem and attach the file it writes into the
  game's folder in your home folder; it holds the console's messages, which
  usually show the cause.
