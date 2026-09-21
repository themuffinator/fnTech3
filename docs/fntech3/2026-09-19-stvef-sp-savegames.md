# Elite Force single-player savegames

## Scope

This record covers Elite Force single player's savegames and the commands
around them: `saves/<name>.sav`, the chunks it is made of, the picture a save
keeps of the screen, the levels a player walks or falls back between, and the
autosave a level is entered with. The game module writes its own half of a
save through three engine imports, which now work; ambient sound sets, Bink
movies and lip-sync belong to later slices. The profile stays behind
`-Dstvef-development=true`.

The savegames are `code/server/server_stvef_sp_save.hpp` over the pure
framing in `code/compat/stvef_sp_savegame.hpp`; the picture is
`code/client/client_stvef_sp_screen.hpp` over
`code/renderercommon/tr_stvef_thumbnail.h`. The game host that calls them is
described in the [game host record](2026-09-19-stvef-sp-host.md), and the
menus that list saves in the [menu record](2026-09-19-stvef-sp-menus.md).

The [save-integrity follow-up](2026-09-21-stvef-save-integrity.md) corrects
publication after an ignored chunk-write error and checks final close/flush
errors. Failed saves preserve the previous file, suppress completion messages
and prevent an attempted hub write failure from advancing the level. Real
filesystem obstruction/recovery and a fresh retail save round trip are recorded.

## The file

| Contract | Retail `stvoy.exe` 1.20 | fnTech3 |
| --- | --- | --- |
| Where a save lives | `saves/<name>.sav`, the name built into one of eight rotating buffers (`0x4267a0`) | The same path in the writable game directory; a save is looked up through the search paths, so an installation's own saves are read where they are |
| A chunk | Its four-letter id, its data's length and the data's block checksum, then the data and the magic `0x1234ABCD`, each word little-endian (`SG_Append` `0x427a40`) | The same, written as three words, the data and the magic; a short write prints `^1Failed to write %s chunk` and now prevents the entire transaction from replacing the previous save |
| Reading a chunk | By id, and by length unless the caller passes zero for any length. A chunk that is not the one asked for is `Loaded chunk ID (%s) does not match requested chunk ID (%s)`; the checksum, the magic and the byte count are each an error of their own (`SG_Read` `0x427b60`) | The same checks and messages in the same order. A save is held whole in memory rather than read from an open file, because the map spawns in the middle of a read and clears the hunk. A chunk whose length runs past the end of the save is refused before anything is read |
| An optional read | Reads the id, and where it is another chunk's seeks back and answers nothing (`0x427e50`) | The same, without a seek: the read leaves the place it started at. The end of a save answers as an id of zero, so an optional read there is quiet |
| Memory for a chunk | Without an address the engine allocates the chunk's length, which the game frees with the engine's own `Free` (`0x427ce8`) | The same allocator, with one byte more, always zero, so that a chunk of text from a damaged save still ends |
| Error unwind | A dropped error closes the save being written and wipes `current` (`Com_Error` `0x411204`), and every error forgets the kept picture of the screen (`0x4111ae`) | The same, for the single-player profile |
| `sv_testsave` | With the cvar set, the game's saving code runs every frame and no chunk is written; a real save clears it while it writes (`SG_TestSave` `0x427e70`, `0x427373`) | The same, with the cvar left alone: a real save writes whatever the cvar says |

## What the engine writes

A save is the engine's chunks, then the game's `WriteLevel`
(`SG_WriteSavegame` `0x427350`).

| Chunk | Contents |
| --- | --- |
| `COMM` | 128 bytes. An autosave is labelled `-------> <map>`; any other save takes the comment the menus stored, or the time, the date and the map when that is empty (`0x426f40`) |
| `SHOT` | 256 by 256 RGBA pixels, the bottom row first. A save made in play keeps the screen; a save made as a level begins keeps the level's own `levelshots/<map>.tga`, fitted and turned over, and the screen where a level has none (`0x427690`) |
| `MPCM` | 1,024 bytes holding the map's name |
| `CVCN`, `CVAR`, `VALU` | The userinfo cvars: how many, then each one's name and value with their terminators (`0x426c90`) |
| `GAME` | Whether the save was made as a level began |
| `CVSV`, `AMMO`, `ADPT` | Only for such a save: what the player carries, as the cvars `playersave`, four `playerammo%d` and 32 `borgadapt%d`, each a 1 KB string (`0x427580`) |
| `TIME`, `TIMR`, `PRTS` | Only for a full save: the level's time and its remainder, and the area portals' reference counts (`CM_WritePortalState` `0x40b8b0`) |
| `CSCN`, `CSIN`, `CSDA` | Only for a full save: how many configstrings hold anything, then each one's index and text. The system info is skipped: it belongs to the server that is running (`0x426da0`) |

Reading a save reads the same chunks, spawns the map between `MPCM` and the
level's own state, and ends with the game's `ReadLevel` (`0x427750`). The
engine tells the game how the level began — fully loaded, loaded as it began,
or neither — and whether it is a hub transition. A client already connected
when the map spawns is connected afresh whatever a save has set, as retail
connects it (`0x425bba`), because the save restores it afterwards.

What the player carries is read from the live player state: health, armour,
weapons, the weapon and its state and the view angles as
`%i %i %i %i %i %f %f %f`, the ammunition and the Borg adaptation counts
(`SV_Player_EndOfLevelSave` `0x423ea0`).

## The commands

Single player has retail's own server commands and none of Quake III's map
aliases (`0x4240e0`).

| Command | Behaviour |
| --- | --- |
| `map`, `devmap` | Clear `playersave` and `spawntarget`, forget the kept picture, start the level and set `sv_cheats` (`0x424200`). Starting a level drops the autosave unless the level's name begins with an underscore — a holodeck or the brig, which the player returns from — and sets the player's models from the `sex` cvar (`0x424040`, `0x423e20`) |
| `maptransition <map> [target]` | Keeps the screen and what the player carries, sets the spawn target and starts the level (`0x4241c0`) |
| `loadtransition <map> [target]` | The same, and keeps this level whole as `hub/<map>`, then restores `hub/<target map>` where there is one and otherwise starts the level (`0x423fc0`) |
| `save <name>` | Refuses without a running level, on a holodeck or in the brig, while dead, and for the names `current`, `..`, `/` and `\`. `quik*`, `auto*` and `exitholodeck` take a fresh picture of the screen and a dated comment, and lose their `*`; `quik*` is `virtual` while `cg_virtualvoyager` is set (`0x426ad0`) |
| `load <name>` | Refuses `current` and the same names. `*respawn` takes the level's own autosave, or the save last read where that autosave belongs to another level (`0x426960`) |
| `wipe <name>` | Deletes a save, except `auto` and `exitholodeck` (`0x426870`) |

Entering a level writes the autosave, unless the level's name begins with an
underscore: a level walked into keeps `auto` as it began, and a hub
transition keeps `hub/<map>`, `auto` and, with `cg_virtualvoyager`,
`virtual`, all of them whole (`SV_UserMove` `0x424ead`).

## The picture

Retail reads the screen whenever a level is being played and no menu is up,
and averages it to 256 pixels square: each of them takes four pixels across
by three down, sampled at `x · width / (4 · 256)` and
`y · height / (3 · 256)`, and the rows stay in the order the screen is read,
the bottom one first (`SCR_PrecacheScreenshot` `0x40a360`, `0x44fd80`). The
one picture is what a save keeps, what the menus show and what the loading
screen draws.

fnTech3 asks the renderer for the picture at the end of the next frame and
draws one, so the picture is of a frame that was drawn whole rather than of
whatever a buffer still held. All three renderers take it where they write a
levelshot, before the frame is shown, and give it the screen's own gamma. A
save made as a level begins loads the level's picture through the renderer's
image loader and fits it the way retail's renderer fits one (`0x44ff50`).

## Results

A developer client with the `stvef-sp` profile, 1,024 by 768 in a window,
with an isolated writable home path:

- `borg2` saved and loaded: the loaded frame is the saved view, weapon,
  health and ammunition. The save holds the engine's chunks and 4,376 chunks
  in all, and parses to its last byte.
- `tutorial` begun writes `auto` with the level's own levelshot in it; a
  quick save in play holds the gameplay view with the HUD.
- `map tutorial`, `maptransition borg2`, `loadtransition tutorial`,
  `loadtransition borg2` leave `saves/auto.sav`, `saves/hub/borg2.sav` and
  `saves/hub/tutorial.sav`, and the last of them restores `borg2` where it
  was left. The transition to a level with no hub save prints retail's
  `^1Failed to open savegame "hub/tutorial"` and starts the level instead.
- `save quik*` writes `quik`, `load *respawn` loads `auto`, and `wipe auto`,
  `save current`, `load current` and `load ../evil` are refused with retail's
  messages.

Savegames are interchangeable with the retail engine, which ran with the
installation's paks through a short `fs_cdpath` and a scratch `fs_basepath`:
retail `stvoy.exe` 1.20 loads a save fnTech3 wrote, and fnTech3 loads a save
retail wrote, both restoring the level at the other engine's player position
with its health and armour. The two engines' saves of the same level have the
same engine chunks, the same 484-byte portal state and the same 76
configstrings; fnTech3 keeps 14 userinfo cvars where retail keeps 9, because
this engine registers more of them. The
[runtime record](evidence/2026-09-19-stvef-sp-savegames.json) holds each
save's chunks. The installation was left unchanged.

## Tests

- `stvef_sp_savegame_tests` compiles the framing, the comments and both
  pictures and checks them: a chunk written and read back, every way a frame
  can be wrong, the end of a save answering an optional read, the box filter
  and the turn a picture takes, and the screen averaged four by three into
  the bottom-first rows. It pins the order both halves of a save are written
  and read in, retail's messages, the commands and their refusals, the
  autosave rules, the configstring bounds a save cannot reach past, the
  renderers' capture and the error unwind.
- `server_user_move_tests` covers the hook that saves a level the player has
  entered.

## Remaining

Ambient sound sets, which also give movers their sounds; Bink movies;
lip-sync; single player's scissor; and a comparison with the retail renderer
of the lighting values and of the characters, whose bodies are Raven's RDM5
skeletal models. The menus' own load and save screens draw from these
functions but have not been driven from a test.
