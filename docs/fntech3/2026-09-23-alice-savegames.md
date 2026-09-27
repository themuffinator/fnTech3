# Alice's savegames and the server's level flow

## Scope

This record covers how American McGee's Alice saves and loads a game and how
its server moves from one level to the next: the four files a save is made
of, the engine's 200-byte header, the commands that write and read them, the
order of its level spawn, the grey picture a save keeps, and the two game
module imports and six exports the archives go through. It also records two
server defects found on the way, because the savegames depend on both: linked
entities never reached a snapshot, and the process-wide model cache was never
emptied.

It builds on the [module record](2026-09-22-alice-module-abi.md), which named
the export and import slots, and the [model record](2026-09-23-alice-tiki.md),
which describes the model cache.

Every fact is measured against the retail November 2000 `alice.exe` (image
base `0x00400000`), `base/fgamex86.dll` and the installation's own files.
Observed facts carry the address they were read at; inferences are called
inferences. Nothing from Ritual's F.A.K.K.2 SDK or from any reconstruction is
incorporated as text.

## The files

A save is four files, each named by one routine (`0x0043a350`):
`save/<config>_<name>.<ext>`, formatted into a single static 128-byte
buffer, where `<config>` is the `config` cvar cut at its **first** dot. The
cvar defaults to `config.cfg` (`0x004398ff..0x004399c8`, through the
registry), so the usual prefix is `config`, and `F4`'s `savegame quicksave`
in `default.cfg` writes `save/config_quicksave.*`.

| Extension | Written by | Content |
| --- | --- | --- |
| `.sav` | the game module | the level, `WriteLevel`/`ReadLevel` |
| `.spv` | the game module | the persistent data, `WritePersistant`/`ReadPersistant` |
| `.ssv` | the engine | a 200-byte header and the area portals |
| `.tga` | the client | a 256-pixel grey picture of the frame |

`Com_WipeSaveGame` (`0x0043a400`) deletes them in that order. The archive the
engine works on between saves is named `current` (`0x00ac0d48`); its
persistent file is what carries Alice's inventory across a level change.

Retail writes all four into its installation: its file copy (`0x0043b7e0`)
and delete (`0x0043ed50`) build paths from `fs_basepath`, and there is no home
path. fnTech3 writes them into `fs_homepath` and reads them through the
search path, so a retail save copied into a home path's `base/save/` is read,
and the installation is never written.

## The header

The `.ssv` begins with 200 bytes written by `SV_ArchiveServerFile`
(`0x00451860`); it has no magic and no version.

| Offset | Size | Field |
| --- | --- | --- |
| `0x00` | 4 | the time of the save, a 32-bit `time_t` (`time()` `0x004e7d57`) |
| `0x04` | 4 | the level clock in milliseconds |
| `0x08` | 64 | a comment, `"%s"` of configstring 2 -- the level's title -- or `"Starting %s"` for an autosave (`0x0050db54`, `0x0051a1a4`), through `Com_sprintf` so always terminated |
| `0x48` | 64 | the BSP name without a `$` target, `strncpy`'d |
| `0x88` | 64 | the archive name itself, `strncpy`'d |

A full 64-byte map or archive name therefore has no terminator in the file.
Behind the header comes the portal block of `CM_WritePortalState`
(`0x004362a0`): the map's area count and one 32-bit reference count for every
ordered pair of areas. On a load `CM_ReadPortalState` (`0x004362e0`) drops with
`numareaportals differs from save game` (`0x00436302`) when the count differs,
and the areas are flooded again.

## Saving

`savegame [name]` (`0x00452010`) does nothing, silently, while the engine
UI's intro has not given way to a map (stage `0x005315f8` not -1) unless
`developer` is set, or while the loading screen is up (`0x006a006c`). It then
saves, and on failure pushes the `loadsave` menu (`0x0040cca0`, the same
routine the drop hook uses to push `main`). `autosavegame` (`0x00452070`)
saves with no gates at all.

`SV_SaveGame` (`0x00451d90`), with the error the menu's `CheckErrorState`
reads (`0x00b59bc0`):

| Test | Message | Error |
| --- | --- | --- |
| the server is in a game | `You must be in a game to save.` | 3 |
| `deathmatch` is zero | `Can't savegame in a deathmatch` | 2 |
| client 0's entity has a game client whose first stat is not zero (`0x00451dfc..0x00451e2d`: entity `+0x200`, client `+0xb0`) | `Can't savegame when dead` | 4 |
| at least 4 MB free (`0x00451e33`) | `Not enough free space to save` | 1 |
| the name has no `..`, `/` or `\` | `Bad savedir.` (developer print) | 5 |

The first four print with `Com_Printf`, the last with `Com_DPrintf`. With no
name -- which is what the menu's Save button and every autosave pass -- the
name is the map's with each `/` made `_` followed by four digits, the first
whose `.ssv` is missing or empty (`0x00451ce0`): `garden10000`,
`garden10001`, and when all ten thousand are taken the last is written again.

A save then copies `current.spv` to the new name (`0x00451690`, printing
`copy %s to %s`), asks the module for the level with `WriteLevel(path, 0)` --
the flag is the constant zero even for an autosave (`0x00451840`) -- writes
the header and portal block, and asks the client for the picture with
`saveshot <path> 256 256`, executed at once (`0x00451983`). It prints `Done.`
and names the archive `current` again.

## Loading

`loadgame <name>` (`0x00451b20`) does nothing, silently, before the intro has
given way (unless `developer`), with `sv_maxclients` above one, with client 0
primed, or while loading. It prints `Loading game...<name>`, refuses a name
that is not an existing `.sav` with `Savegame not found.`, clears `nextmap`,
loads the game module if none is loaded (`0x004544c0`), and asks the module's
`LevelArchiveValid`. A refusal runs, at once,
`dialog "" "" "loadbrokengame %s" 260 150 textures\menu\invalid textures\menu\okbutton textures\menu\cancelbutton`.
Otherwise it reads the header, restoring the map name and the level clock;
copies the named `.spv` over `current.spv`; and spawns the header's map with
its load flag set. `loadbrokengame` (`0x00451a50`) is the same load without
the gates, the existence test or the validity check.

Retail's `loadgame` only prints `Bad savedir.` for a name containing `..`,
`/` or `\` and goes on to use it; fnTech3 refuses it.

## Level changes and the spawn

`map` and `devmap` share one handler (`0x00450f00`). With `developer` and a
non-empty `mapdir` the name is prefixed; backslashes become slashes
(`0x0044bef0`); the BSP is the text before any `$`, and `Can't find map
maps/<bsp>.bsp` refuses a missing one; `g_gametype` 2 becomes 0; with
`developer` off `cheats` becomes 1 for `devmap` and 0 for `map`; the `current`
archive is wiped; `nextmap` is cleared; and the server spawns with the whole
argument.

`gamemap <map>` (`0x00450ce0`) is the level change the module asks for itself
(`G_ExitLevel`, `fgamex86` `0x100a4da0`, with the exit's
`"map" "garden2$garden2_start1"`): it names the archive `current`, wipes it
when the map begins with `*` and otherwise, in a game, has the module write
`current.spv`; spawns; and on a listen server queues
`wait 1500; pause ; autosavegame ; pause` (`0x00519e00`). `restart`
(`0x00450d80`) spawns the whole current argument again, never twice in one
frame, and `pause` (`0x00439800`) toggles `paused`.

`SV_SpawnServer` (`0x00454ff0`), in order: unpause; keep the whole argument
and set `nextmap` to `map <argument>`; name the BSP by the part before `$`;
load the module and call `Init` with the level clock as it stands, then
`Cleanup` (export 3, no arguments, `0x004551cf`); load the map, and set
`mapname` to the whole argument; clear the area portals; unless loading, set
configstring 8, the soundtrack, to `none`. A load then asks the module's
`ReadLevel` and, if it succeeds, reads the portals back and runs no settle
frame. Otherwise -- a fresh spawn, or a load the module refused -- the level
clock goes to zero, `SpawnEntities(mapname, entity text copy, 0)` runs, its
`error_message` is raised, the module's `ReadPersistant` reads `current.spv`,
and three frames run as `RunFrame(clock, 100)`, raising `error_message` after
each, with the clock at 0, 100 and 200 (`0x00455316..0x00455361`). There is
no fourth frame after the clients reconnect. The archive name ends as
`current`.

## The picture

The client command `saveshot` (`0x0042d570`) does nothing before the renderer
has started; otherwise it hides the console, menus and HUD for one frame,
draws it, and runs the renderer's `screenshot` with the same arguments. A
name containing `/` is used as it is. With both sizes given, the renderer
(`0x00474000`) reads the whole frame with `glReadPixels(GL_RGB)`, bottom row
first, and for each output pixel sums four samples across and three down at
`(int)((4x + c) * xs)` and `(int)((3y + r) * ys)`, the steps being floats
`vidWidth / (4 * w)` and `vidHeight / (3 * h)`. It keeps the largest channel
sum -- red when red beats both, else blue when blue beats both, else green --
divides it by twelve and writes it to all three bytes. A second pass
multiplies every byte by the float 2.55 (`0x005002d0`) on the x87 stack,
compared with 255 and truncated without first rounding to float, so 100
becomes 254. The file is an 18-byte uncompressed true-colour TGA header and
`w * h * 3` bytes: 196,626 bytes at 256 by 256.

Two consequences, both observed: anything brighter than 100 of 255 turns
white, so the picture is a high-contrast grey; and a tie between the two
largest channels falls to green even when green is the smallest.

## The game module's side

Export slots, at the table offsets the engine's call sites read:

| Slot | Export | Call site |
| --- | --- | --- |
| 3 | `Cleanup()` | `ge+0x0c`, `0x004551cf` |
| 16 | `WritePersistant(file)` | `ge+0x40`, `0x00451787` |
| 17 | `ReadPersistant(file)` | `ge+0x44`, `0x0045177a` |
| 18 | `WriteLevel(file, autosave)` | `ge+0x48`, `0x00451843` |
| 19 | `ReadLevel(file)` | `ge+0x4c`, `0x004517d4` |
| 20 | `LevelArchiveValid(file)` | `ge+0x50`, `0x00451c6c` |

The module writes its archives itself through the file imports it already
has. Each begins `ALIC`, a version and `Alice Archive Version 1`; a level
archive's first two integers are 4 and 8, a persistent archive's is 1.

Import 25 is the archive name builder (`0x0043a350`, written into the table at
`0x00454511`). The module's only call is its play log (`0x10117001`), with a
name and `log`, eight bytes cleaned; it tests the answer for null. Import 99
is `CalcCRC` (`0x0043a4c0`): CRC-16 with polynomial `0x1021` through the table
at `0x00511bb8`, starting at `0xffff` with no final xor -- the CRC-16/CCITT
parameters, `123456789` giving `0x29b1` -- and `0xffff` for an empty buffer.
The module keeps its low sixteen bits (`0x100b2020`) per script and, reading a
level archive, drops with `File '%s' has changed from when this savegame was
written` when they differ (`0x100b30c3`).

## Linked entities never reached a snapshot

fnTech3's linker for Alice's entity record computed the absolute bounds and
set the linked flag but never recorded which areas and PVS clusters the
entity touches. The snapshot walk needs both, so it culled every entity that
was not broadcast, the player's own included. Retail keeps the two areas in
its side table (written at `0x00457cec`); its slot 39 (`0x004536d0`) reads
them back and, when the second is not -1, hands both to
`CM_AdjustAreaPortalState` (`0x004361a0`). That slot was a no-op here.

The area and cluster part of `SV_LinkEntity` is now its own routine,
`SV_LinkEntityClusters`, with no change for any caller; Alice's linker calls
it, its unlinker clears what it recorded, and slot 39 adjusts the portal
between the two areas. On a developer client on `garden1`, with the server
counting each snapshot: 195 of 274 linked entities are sent, where 0 of 275
were sent with the call removed.

## The model cache at a level change

The model definition cache is one for the whole process and holds 384
definitions (`0x00459615`); the installation ships 801. Nothing emptied it, so
a long session could reach `TIKI cache full`.

Retail never empties it at once: its free-all loop (`0x0045cb80`) has no
caller. The renderer frees definitions instead, through its import slot 29,
which is `TIKI_FreeTiki` (`0x0045ca30`, written at `0x0042c5a7`): its export
`+0x20` (`0x0048a4e0`), which the engine calls as a registration ends
(`0x0040e705`) and the client game reaches as its slot 49, frees the
definition behind every model not registered again in the current sequence;
its export `+0x94` (`0x0048ab00`), the client game's slot 93, frees every
model's and loads it again by name. The server's model table (`0x006a0070`,
512 entries) is only zeroed on a full spawn (`0x00453920` from `0x004551de`).

fnTech3 empties the whole cache once per level instead, in `SV_SpawnServer`
after the old game, the client game and the renderer have shut down and
before the new game registers a model. At that point nothing holds a handle
or a definition pointer into it: the renderer's models are released with its
shutdown and rebuilt at its next start, and the client game restarts.

## A load of the running level

`loadgame` spawns with its load flag set: `SV_SpawnServer(name, 1, 0)` at
`0x00451cc3`, as `loadbrokengame` does at `0x00451b17`; `map` passes `(0, 0)`
at `0x00451126`. `SV_SpawnServer` then takes its reuse path when the game
module is loaded, the BSP is the one it last spawned (`0x00b59b80`, written at
`0x00455130`) and either flag is set (`0x004550a1..0x004550db`). That path
calls the module's `Cleanup` and skips the full spawn: the configstrings are
not freed and reset (`0x00455171..0x004551bb`), the server state is not
cleared (`0x00455190`), and the model table is not emptied (`0x004551de`).

That matters because the module archives an entity's model index as a
number: `fgamex86.dll`'s `G_ArchiveEdict` (`0x100ad0a0`) writes `s.modelindex`
(`+0xb8`) with the plain integer archiver at `0x100ad47b`, where it writes a
looping sound by name (`0x100ad18e`) and looks it up again on a load
(`0x100ad410`). An entity with a model name has `setModel` find its index
again; one without keeps the number it was saved with, which only means the
same model while the model configstrings keep their places.

fnTech3 reloads the module on every spawn. `SV_AlicePrepareSpawn` copies the
configstrings when a load names the BSP the server last spawned and a module
is loaded; `SV_AliceRestoreConfigstrings` puts them back once the spawn has
reset them, before the module is loaded; and the module's table fill
registers every kept `tik` model again (`SV_AliceRegisterKeptModels`), so the
model table holds what retail's still would.

On `garden1`, save after 12 s and load 2 s later (client, windowed, no input,
`.tmp/alice-snap/load-before` and `load-after`): before, every one of the
level's 81 model configstrings changed place across the load (`altar_jackbomb` from 10 to
21, `g_gleaf06` from 21 to 51), entity 92 kept index 21, and the client game
reported `Can't find animation 1` 468 times; after, `Alice load of the running
level keeps its 1328 configstrings`, no model configstring moved, entity 92 is
`g_gleaf06` and there is no such report. A dedicated `devmap garden1`,
`alicearchive dprobe`, `loadgame dprobe`, `alicearchive dprobe2`
(`.tmp/alice-snap/ded-load`) keeps its configstrings the same way and prints
no `handle -1 out of range`. The final build repeats both
(`.tmp/alice-snap/final-load`, `final-ded`) with the same outcome; the
installation was unchanged after every run.

A later build repeats it, 2026-09-24 (`.tmp/alice-unrel/load1`,
`meson/build/win32-alice-client`, `fntech3.exe` SHA-256 beginning
`ddd57893cc42dc97`; client, `+devmap garden1`, windowed 800x600, isolated
home, no input): `skipcinematic` 9 s after the client became active,
`savegame t1` 3 s later, `loadgame t1` 3 s after that, and a screenshot 10 s
on. The log shows `Alice load of the running level keeps its 1328
configstrings`, `Alice level read back from t1 at level time 12550` and the
client back in `CS_ACTIVE`, with no `Can't find animation` and no `Could not
find parent` line in the whole run; the screenshot shows Alice in third
person with the HUD's gauges. The run exited with code 0 and its manifest
records the installation unchanged.

## The model wrappers

Retail's model import wrappers (`0x00453940..0x004540f0`, slots 51 to 88)
each resolve the index through `0x00453870` and, when it has no definition --
index zero, or one never registered -- return their own default on `jl`
without calling the TIKI routine: zero or null, -1 from `Anim_NumForName`,
`Anim_Random`, `Surface_NameToNum` and `Tag_NumForName`, a cleared vector
from `Anim_Delta` and `Anim_AbsoluteDelta` (`0x00453ba7`, `0x00453be7`), and
nothing from `CalculateBounds`, `Frame_Delta`, `Frame_Bounds` and the alias
updates. The two tag orientations print `PF_Tag_Orientation: not a tiki
model` (or `...Ex`) as a developer print and answer a zero origin with the
axis `AnglesToAxis` makes of a zero vector, the identity (`0x00453e72`,
`0x00453f47`). fnTech3's wrappers passed the -1 on, so the TIKI code printed
`TIKI_NumAnims: handle -1 out of range` where retail prints nothing, the
orientations came back with a zero axis, and the dialog alias calls reached
the global alias list, which is what -1 means to them. They now branch as
retail's do.

## The cinematic stop

`map`, `loadbrokengame` and `loadgame` call `CIN_Stop` (`0x00425770`) right
after clearing `nextmap`: before the spawn in the first two (`0x00451118`,
`0x00451b09`) and before loading a module in the last (`0x00451c53`). It acts
only while a cinematic plays (`cls.state` 9). `gamemap` and `restart` do not
call it. fnTech3's three commands call `SV_AliceStopCinematic` at the same
places; in a client build it reaches `CL_AliceStopCinematic`, which stops the
cinematic only while one plays, and a dedicated build compiles it out.

## What fnTech3 implements

- `code/compat/alice_savegame.hpp`: the byte rules -- names, the header,
  the portal block's size, the autosave name, the saveshot and its TGA
  header, `CalcCRC` -- shared by the server, the client and the tests.
  `code/compat/alice_saveshot.h` holds the picture's reduction in C so each
  renderer can include it.
- `code/server/server_alice_save.hpp`: the commands (`map`, `devmap`,
  `gamemap`, `restart`, `savegame`, `autosavegame`, `loadgame`,
  `loadbrokengame`, `pause`), the archive routines, and the spawn steps
  `sv_init.cpp` calls, all behind `Com_GameIsAlice()`. `map_restart`, which
  Alice does not have and which cannot restart a function-table module, is
  removed for this profile only. The `config` cvar is registered with the
  profile's configuration name.
- `code/server/server_alice_game.hpp`: imports 25 and 99 served (110 of the
  111 slots; slot 100 is the null retail leaves); `GAME_INIT` calls `Init`
  alone; the six exports called at retail's shapes; the linker, unlinker and
  slot 39 as above; the module's own command registrations are skipped
  silently when the name exists, as retail's handler-less registration is.
- `code/server/sv_init.cpp`: the `$` split, `nextmap` as `map <argument>`,
  the cache flush, the level clock restored before `Init` on a load, the
  level read back or spawned fresh, retail's three settle frames and no
  fourth, and connected clients carrying their old clock until they
  acknowledge the new gamestate, as `sv_levelTimeReset` has them do.
- `code/server/sv_world.cpp`: `SV_LinkEntityClusters`.
- `code/qcommon/cm_test.c`: `CM_NumAreas`.
- The renderers: `re.CaptureAliceSaveshot` takes the picture through the same
  capture Elite Force's savegame picture uses, at the end of the frame drawn,
  from the engine's own render target.
- `code/client/cl_alice_saveshot.cpp`: the body of `saveshot`; the engine
  UI's handler asks for a frame with only the view in it and calls it.
- For the load/save menu: `SV_AliceSaveError(clear)` and
  `SV_AliceArchiveFileName(name, ext, ...)`.
- `alicearchive <name>`, a developer probe: the save without its player
  check, so a dedicated server with nobody connected can have its module
  archive a level. It is not a retail command and does nothing with
  `developer` off.

## Where fnTech3 differs, and why

- Saves live in `fs_homepath`, never in the installation.
- Retail copies names without a bound and truncates paths at 128 bytes; a
  64-character name overflows its buffers. fnTech3 refuses a name that does
  not fit, a path that would be truncated, and a traversal in the name or in
  `config`, in every command -- `loadgame` included.
- The free-space test asks `fs_homepath`'s drive, where fnTech3 writes. A
  question that cannot be answered lets the save go ahead, where retail's
  answer of 2 MB refuses it, and retail's signed comparison, which refuses on
  a disk with more than 2 TiB free, is not reproduced.
- A `.ssv` read for a load must be a whole header naming a map that exists,
  with no `..`, `\`, `:` or `$`; a portal block cut short is refused.
  Retail reads on regardless. A count below zero, which was refused too
  until 2026-09-27, is put back as retail puts it back: the engine leaves
  one when a level closes a portal nothing opened ([area portal record](2026-09-27-alice-area-portal-counts.md)).
- `gamemap` refuses a map that does not exist before leaving the level;
  retail has no such test and would drop inside the spawn.
- The `config` cvar is not kept in the registry. As in retail, it also names
  the configuration file (2026-09-25): a name the command line gives it, if
  longer than one character, is executed at startup and written back
  (`0x004398ff..0x004399c8`, `0x00439eb0`), where fnTech3 had always used
  `config.cfg` and followed the cvar for the saves alone. That name holds for
  its launch only. A name without an extension is given `.cfg`, which retail
  left off when it wrote the file and added when it read it. A name no file
  may carry, such as a library's, keeps the profile's own, as `writeconfig`
  does. The default keeps the profile's own file, so fnTech3's dedicated
  server still writes `config_server.cfg`, which retail's single executable
  does not have.
- The reuse path is reproduced only in its server state: retail keeps the
  module and its clients when a load or a `restart` names the map already
  running; fnTech3 always reloads, which is retail's full path, but a load of
  the running BSP keeps the configstrings and the model table built from them,
  as the reuse path does ([below](#a-load-of-the-running-level)).
- A dedicated server with nobody connected starts a level's clock from zero
  before `Init`, where retail hands `Init` the previous level's clock; the
  module sees the same clock once `SpawnEntities` runs.
- The picture is read from the frame as drawn. With hardware gamma, as in
  retail, that is before the gamma ramp; where fnTech3 applies gamma in its
  own post-process the frame already carries it.
- The cache is emptied whole at each level change rather than model by model
  as the renderer re-registers.

## Results

Build: `meson/build/win32-alice-save` configured with
`-Dalice-development=true`, MSVC 2022 x86, Windows 11, on the shared working
tree over `f65471e` with this change and other sessions' uncommitted work.
`fntech3.exe` SHA-256 `0461ff92...ad7c`, `fntech3.ded.exe` `2fe52f80...fdbf`,
`fnql_glx_x86.dll` `436fa015...94d5`. Installation: the retail `E:\Games\Alice`,
read only. Every run used `+set com_gameProfile alice +set fs_basepath
E:\Games\Alice +set fs_homepath <.tmp/alice-save/...> +set developer 1`; the
client ran windowed at 800 by 600 with no input sent.

Dedicated server, `+devmap garden1$garden1_start1 +wait 3000 +gamemap
garden2$garden2_start1 +wait 3000 +alicearchive probe +wait 1000 +loadgame
probe +wait 10000 +alicearchive probe2 +quit`:

(*2026-09-24:* this sequence no longer runs as written. Alice's `wait` now
counts down only in the client's pass of the frame, as retail's does --
`Com_Frame` hands its frame time to `Cbuf_Execute` at `0x0043a183` alone,
and every other call, the dedicated server's included, passes 0 -- so on a
dedicated server a wait longer than one frame never ends. A dedicated repeat
of this run needs one-frame `wait`s, or a listen server.)

- `SpawnEntities` was handed `garden1$garden1_start1` and loaded the BSP
  `garden1`; `gamemap` wrote `current.spv` (92 bytes) and the fresh `garden2`
  read it back; the cache flushed 79 and then 81 definitions.
- `probe` wrote `config_probe.sav` (1,232,679 bytes, beginning `ALIC` and
  `Alice Archive Version 1`, then 4 and 8), `config_probe.spv` (92) and
  `config_probe.ssv` (304: the 200-byte header with `Herbaceous Border`,
  `garden2`, `probe` and level time 3,250, then five areas and 25 counts).
- `loadgame probe`: `LevelArchiveValid` read the archive, the header was read,
  `probe.spv` was copied over `current.spv`, `garden2` spawned with `Init` at
  level time 3,250 and `ReadLevel` restored it (`ActiveList has 9 elems,
  SleepList has 24 elems`, as when it was written), with no settle frame.
- Ten seconds later `probe2` recorded level time 13,200: the loaded level ran
  9,950 ms, 199 frames at the default `sv_fps` of 20.
- `CalcCRC` was not called: no script was loaded by this `ReadLevel`.

Dedicated server, the refusals: `savegame t1` with nobody connected, `Can't
savegame when dead`; `loadgame` alone, the usage; a missing save, `Savegame
not found.`; `../x`, `Bad savedir.`; a `.sav` the module refuses, the retail
`dialog` line; a header cut short, `Couldn't read`; a header naming
`../../evil`, refused; `gamemap nosuchmap`, `Can't find map`; `pause` twice,
`paused` 1 then 0; after `restart`, `nextmap` is `map
garden1$garden1_start1`; a header with four areas on a five-area map,
`numareaportals differs from save game`.

Client, `+devmap garden1$garden1_start1`: the player begins, `savegame t2`
writes all three module and engine files and `config_t2.tga` (196,626
bytes), and `loadgame t2` reads the level back at level time 8,050 and the
player begins again. `gamemap garden2$garden2_start1` writes `current.spv`,
spawns `garden2`, reads it back, and 1.5 s later the queued autosave writes
`garden20000` with the comment `Starting Herbaceous Border`; a new process
started with `+loadgame garden20000` and no map loads the module, validates
the archive, spawns `garden2` at level time 1,500 and the player begins, with the
level's opening dialog subtitled on screen.

The picture: with `+set r_ignorehwgamma 1`, so that the engine's own
`screenshot` applies no gamma, `screenshot silent` and `savegame t5` in one
command pass capture the same frame. An independent script applying retail's
rule to the screenshot reproduces the saveshot exactly: 65,536 of 65,536
pixels equal, header `00 00 02 ... 00 01 00 01 18 00`.

The installation was not written.

## Tests

- `tests/alice_savegame_tests.cpp`: the names and their refusals, the header
  at its offsets with full-length and garbage fields, the portal block's size,
  the autosave names, the saveshot's sample positions, gain, clamp, green tie
  and stride, the TGA header, and `CalcCRC` against the table-driven form.
- `tests/alice_module_abi_tests.cpp`: the six export offsets and the two
  import slots.
- `tests/alice_snapshot_source_tests.py`: a load of the running level keeping
  its configstrings and model table, in the spawn's order; the cinematic stop
  after `nextmap` in `map`, `loadgame` and `loadbrokengame` and nowhere else;
  and the model wrappers compiled against stub TIKI routines, each answering
  retail's default for an index with no model without reaching them.
- `tests/alice_savegame_source_tests.py`: the retail messages, the command
  set and its place ahead of the Quake III one, every spawn step gated on the
  profile with the other profiles' frames kept, the archive orders, the
  linker and slot 39, the error unwind, the three renderers' capture with
  Elite Force's picture unchanged, and the saveshot header compiled as C.

## Still open

- No retail-written save has been compared: the installation has none, and
  making one needs retail run against a copy of the installation outside the
  repository.
- A load in a process with no level running, or of a different map, is
  retail's full path: the configstrings start empty and the module registers
  its models in the order its archive restores them. An entity whose model
  index was read back as a number then names another model -- on `garden1`,
  entity 92's index 21 was `g_gleaf06` and becomes `altar_jackbomb`, and the
  client game reports `Can't find animation 1` every frame (713 in 10 s from a
  fresh `+loadgame`). By the reading above retail's full path does the same;
  that is not established against a retail run.
- `CalcCRC` is served and unit-tested but has not yet been reached at run
  time.
- The load/save menu is the engine UI's: it has to list the `.ssv` headers,
  skip `quicksave`, and show `SV_AliceSaveError`. (*2026-09-24:* it does now;
  see [2026-09-24-alice-menus.md](2026-09-24-alice-menus.md). Loading a save
  from the menu has not been exercised at run time.)
