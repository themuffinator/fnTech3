# Launcher Guide

The fnTech3 launcher finds the games on your computer and starts the one you
choose with a click. Each game it finds gets a row with its icon, its name,
where it came from -- Steam, GOG, a disc and so on -- and, underneath in faint
text, the folder it is installed in. You never have to type a game's name or
folder, and the launcher never changes a game's files.

## Starting the launcher

On Windows, open `fntech3-launcher.exe` in your fnTech3 folder. Keep it
beside `fntech3.exe`: the launcher starts the fnTech3 that sits next to it. On
Linux, run `./fntech3-launcher` from the same folder when your download
includes it.

The first time, the launcher spends a second or two looking for games. Later
it shows the list it found last time straight away and checks again in the
background; **Rescan** (or **F5**) looks again at any time.

## The game list

Every row is one installation of one game. If you have a game twice -- say
from Steam and from an old disc -- you get two rows, each with its own folder.

The label beside the name says where the installation came from:

| Label | Where it came from |
| --- | --- |
| Steam | A Steam library. Every library on every drive is checked. |
| GOG | GOG Galaxy, a GOG installer, or a folder with GOG's own game files. |
| Heroic, Lutris | Installed with the Heroic or Lutris game launcher on Linux. |
| EA app | Installed with the EA app. |
| Retail | Registered by the game's own installer, usually from a disc. |
| Local folder | Found in a games folder, with no sign of a store. |
| Portable | fnTech3 itself is installed inside this game's folder. |
| Added manually | A folder you added yourself. |

A few more tags can appear:

- **Running**: the game is open. A green dot also shows on its icon.
- **Experimental**: the game's support in fnTech3 is still being built. Every
  build can start it on a compatible platform, but parts may be missing.
- **Unverified**: a folder you added even though fnTech3 did not recognise
  the game's files. It may not start.
- **Folder not found**: a folder you added is no longer there, for example
  because its drive is unplugged.
- **Mod: ...**: the mod this row starts (see [Name, mod and
  options](#name-mod-and-options)).

Games your copy of fnTech3 cannot start yet are listed separately under
**Not playable in this build**, with the reason when you point at them.
Click that heading to fold the section away, or hide it in the **Sort**
menu. The [Games guide](GAMES.md) explains which games are experimental.

Point at a game's name or folder to see the whole folder path, how it was
found and when you last played it from the launcher.

### Searching and sorting

Type in the search box to narrow the list. It matches a game's name, its
label (try `steam` or `gog`) and its folder. **Esc** clears the search.

The **Sort** button orders the list by **Name**, **Recently played** or
**Location**, and turns the extra sections on and off.

## Playing a game

Click **Play**, double-click the game's row, or select it and press
**Enter**.

Games with separate single-player and multiplayer versions -- Return to
Castle Wolfenstein, Elite Force, Jedi Academy and Jedi Outcast -- have a
switch beside **Play** for the version to start, such as **Single Player** and
**Multiplayer** (Elite Force calls its multiplayer **Holomatch**). The
launcher remembers your choice for each game. In a narrow window the switch
moves into the game's menu.

The launcher starts fnTech3 with the game and its folder named, exactly as a
shortcut does (see [Getting Started](GETTING_STARTED.md#choosing-the-game-yourself)),
so fnTech3 never has to guess. While the game runs, its row shows
**Running**. If a game closes again straight after starting, the launcher
says so; fnTech3's own window or console log says why.

What happens to the launcher when a game starts is up to you: it can stay
open, minimize itself and come back when you quit the game, or close. See
[Settings](#settings).

## Where the launcher looks

Without being asked, the launcher looks in:

- every Steam library, including the Flatpak and Snap versions of Steam on
  Linux;
- GOG Galaxy and GOG's installers, and on Linux the Heroic and Lutris
  launchers;
- the registrations disc installers leave in Windows, and Windows' list of
  installed programs;
- the usual game folders: `Games`, `GOG Games` and `Program Files` on every
  drive in Windows; `~/Games`, `~/GOG Games`, `/usr/share/games` and `/opt` on
  Linux, and the Windows game folders inside Wine, Bottles and PlayOnLinux;
- the folder fnTech3 is in, and the folder above it;
- folders you add yourself.

A folder appears in the list only when it holds a game's data that fnTech3
recognises, checked the same way fnTech3 checks it when it starts. So a row
in the list is a game fnTech3 will find.

## Adding a game yourself

If a game is somewhere the launcher does not look, click **Add game** (or
press **Ctrl+O**) and choose the game's folder. You can also drag the folder
from your file manager and drop it on the launcher's window.

Choose the game's own folder: the one that holds its data folder, such as
`baseq3` for Quake III Arena, `main` for Return to Castle Wolfenstein or
`base` for Jedi Academy. Choosing the data folder itself works too.

If the launcher does not recognise the folder, it says so and lets you choose
another folder, or add it anyway as the game you pick. A game added that way
is marked **Unverified**: fnTech3 starts it as that game, but its files are
not ones fnTech3 knows, so it may not work.

To have a whole folder of games searched every time -- for example
`D:\Games` -- add it under **Also search these folders** in
[Settings](#settings) instead.

### Hiding and removing games

Each row has a menu: click **...** at its end, or right-click the row.

- **Hide from the list** tucks a game away. A link at the bottom of the
  window shows hidden games again, and **Show in the list** brings one back.
- **Remove from the list** appears for folders you added yourself. It only
  forgets the folder; the game's files are not touched. If the launcher also
  finds the game by itself, the game stays in the list.

## Name, mod and options

**Name, mod and options...** in a game's menu changes how its row starts the
game:

- **Name in the list**: your own name for the row, such as `Clan night`.
- **Game mode**: the version to start, for games that have more than one.
- **Mod**: a mod to play instead of the game itself. The list holds the
  folders beside the game's data folder that contain `.pk3` files; Quake III
  Arena's `missionpack` folder is shown as **Team Arena**.
- **Extra command line**: anything else to give fnTech3 when it starts, in
  the same form as a shortcut, for example `+set r_fullscreen 0` to start in a
  window or `+connect example.com` to join a server. Put a value with spaces
  in double quotes: `+set name "Player One"`.

The box underneath shows the whole command the launcher will run. Copy it
to make your own desktop shortcut that starts the game without the launcher;
**Copy the launch command** in the game's menu does the same.

The menu also has **Open the game's folder**, **Copy the folder's path** and,
on Windows when the dedicated server is beside the launcher, **Start a
dedicated server**, which opens fnTech3's server for the game in its own
console window.

## Settings

Click the sliders button at the top right, or press **Ctrl+,**.

- **Appearance**: **Match the system** follows Windows' or your desktop's
  light or dark mode; **Dark** and **Light** choose one.
- **When a game starts**: **Keep the launcher open**, **Minimize it** (it
  comes back when the game closes) or **Close it**.
- **Game list**: show or hide the games this build cannot start yet, and the
  games you have hidden.
- **Also search these folders**: folders whose games are searched every time.
- **Engine**: the fnTech3 the launcher starts. Normally that is the one beside
  the launcher; **Choose...** picks another build, and **Use the one beside the
  launcher** goes back. The dedicated server is expected in the same folder.

## Keyboard shortcuts

| Keys | What they do |
| --- | --- |
| **Up**, **Down**, **Page Up**, **Page Down** | Move through the list |
| **Enter** | Play the selected game |
| **Ctrl+F** or **/** | Search |
| **Esc** | Clear the search, or close a dialog |
| **F5** or **Ctrl+R** | Look for games again |
| **Ctrl+O** | Add a game folder |
| **Ctrl+,** | Settings |
| **Menu** key or **Shift+F10** | The selected game's menu |

## Where the launcher keeps its settings

The launcher saves its settings -- the folders you added, your names, modes
and mods, hidden games and the window's size -- in a file of its own:

- Windows: `%APPDATA%\fnTech3\Launcher\launcher.json`
- Linux: `~/.local/share/fnTech3/Launcher/launcher.json`
- macOS: `~/Library/Application Support/fnTech3/Launcher/launcher.json`

To keep everything beside the launcher instead, for example on a USB stick,
create an empty file called `fntech3-launcher.json` next to it; the launcher
then uses that file.

fnTech3 itself never reads this file, and the games' own settings are not in
it. If the file is ever damaged, the launcher starts afresh and keeps the
damaged copy beside it with `.bad` added to its name.

## If something goes wrong

**"fnTech3's engine was not found next to the launcher"**: the launcher is
not in the same folder as `fntech3.exe` (`fntech3` on Linux). Move it back,
or choose the engine in [Settings](#settings).

**A game is missing from the list**: click **Rescan**. If it is still
missing, add its folder with **Add game**. If the launcher says it does not
recognise the folder, check that you chose the game's own folder; a game
with damaged or unusual files may need repairing or reinstalling.

**A game is under "Not playable in this build"**: point at it to see why.
Experimental status does not prevent launch. On Linux, games
whose own game code exists only for Windows -- Quake Live's menus, the Jedi
Knight single-player games, Elite Force's single player, Alice, F.A.K.K.2 and
Medal of Honor -- start only in the Windows version.

**A game starts fullscreen and you want a window**: press **Alt+Enter** in
the game, or add `+set r_fullscreen 0` to its extra command line.

**"The extra command line has a quote that is never closed"**, **"... more
than 32 startup commands"** or **"... longer than the engine accepts"**: the
extra command line needs fixing. Put long lists of settings in a config file
and run it with `+exec`, as [Getting Started](GETTING_STARTED.md) describes.

How the launcher finds games and starts fnTech3 is described for developers
in the [architecture overview](fntech3/ARCHITECTURE.md#launcher).
