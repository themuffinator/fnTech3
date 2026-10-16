# The Jedi single-player menus (2026-10-16)

Neither Star Wars Jedi Knight: Jedi Academy's single player (`jka-sp`) nor
Jedi Outcast's (`jk2-sp`) reached its menus: `CL_InitUI` refused both
profiles before any start-up command could run, because neither game ships a
menu module to load. Their engines run their menus themselves. This slice
gives fnTech3 its own engine menu system for them: a reader for Raven's menu
files with each engine's own keywords (`code/compat/raven_menu.hpp`) and a
runtime hosted as the client's UI module (`code/client/cl_jedi_sp_ui.cpp`).
Both games now open their own main menus and the menus reached from them, and
a game start that hits the single-player game boundary shows its reason in
the game's own error menu. No gate closes.

Evidence levels follow the [ledger](COMPATIBILITY.md): Source (retail
disassembly read here), Unit (tests), Live (engine runs).

## The menus are the engines' own (Source)

`jasp.exe` 1.01 and `jk2sp.exe` 1.02a keep the whole menu system inside the
executable and hand it a private import table (66 words in `jasp.exe`'s
UI_Init `0x494ae0`, 70 in `jk2sp.exe`'s `0x4994b0`), so nothing crosses a
module boundary. What a menu file may say is fixed by tables in each
executable, each a keyword and its handler, in this order:

| | menu keywords | script commands | item keywords |
| --- | --- | --- | --- |
| `jasp.exe` | `0x526090`, 37, 12-byte rows | `0x526258`, 33, 8-byte rows | `0x526368`, 85, 12-byte rows |
| `jk2sp.exe` | `0x4eac30`, 34 | `0x4eadd8`, 23 | `0x4eae98`, 69 |

Outcast lacks Academy's `descTextStyle`, `onAccept` and `ignoreescape` menu
keywords and eleven script commands (`playVoice`, `stopVoice`, `ffplay`,
`transition2`, `transition3`, `setitembackground`, `setitemtext`,
`setitemrect`, `delay`, `incrementfeeder`, `decrementfeeder`); it alone has
`playlooped`. `uiScript` is in neither table: a command the table does not
have goes to the game's own scripts, which read the next word as the
script's name.

Named values are null-ended name lists whose index is the value: window
styles (`0x526020`, `0x4eabc4`), item types (`0x526040`, `0x4eabe0`; only
Academy has `ITEM_TYPE_TEXTSCROLL`) and alignments (`0x526080`,
`0x4eac1c`); the one item flag is `WINDOW_INACTIVE` = 8. A feeder is read as
a number, so `FEEDER_SAVEGAMES` reads as 0, which is the save list's number.

The asset block's keywords are an if-chain of string compares. Academy has
`smallFont`, `mediumFont`, `bigFont`, `gradientbar`, the menu sounds,
`itemFocusForce`, the force and datapad move sounds, `cursor`, the fades,
`shadowX`, `shadowY`, `shadowColor`, `precacheSound` and `focuscolor`;
Outcast has `stripedFile` instead of the precache, force and datapad
keywords. A menu's `font N` is the engine's font handle, so in a clean start
it is the Nth font the assets registered.

Tokens follow `COM_ParseExt` (`jasp.exe` `0x422cd0`, whitespace `0x422c80`):
characters are signed, so a byte above `0x7f` counts as space; at most 1024
characters are kept and a word that reaches 1024 is discarded with "Token
exceeded". Numbers and colours come from the keyword's own line.

Text is painted by `0x4951a0` in `jasp.exe` and `0x4999d0` in `jk2sp.exe`:
the coordinates are truncated to whole virtual pixels, font 0 is the default
font, and the style less 2 indexes a five-entry table (`0x495210`,
`0x499a40`), so style 2 blinks, styles 3 to 6 drop a shadow, and style 1 --
which the menus' buttons use -- draws plain. Raven's released source maps
style 1 to a blink; both retail engines do not.

`UI_MainMenu` (`jasp.exe` `0x49a080`) sets `sv_killserver`, takes the keys,
opens `mainMenu` (loading `ui/menus.txt` if it is missing) and opens
`error_popmenu` while `com_errorMessage` is set. `UI_LoadMenus`
(`0x498640`) reads the list file or falls back to `ui/menus.txt`. The engine
answers `ui_cache`, `levelselect`, `ui_teamOrders`, `ui_report` and `ui_load`
at the console (`0x494a20`). Outcast's `UI_Load` (`0x49aac0`) registers its
string packages `menus0` to `menus9` with flag 4 until one is missing.

## The reader (Source, Unit)

`raven_menu.hpp` reads `ui/menus.txt`, its `loadMenu` lists and every menu
file with the keyword sets above, keeping scripts as the engines do (every
token, the longer ones quoted). Every single-player menu file of both
installations reads with no warnings:

| Game | Lists | Files | Menus | Items |
| --- | --- | --- | --- | --- |
| Jedi Academy 1.01 | `ui/menus.txt`, `ui/ingame.txt` | 36 | 36 | 1,344 |
| Jedi Outcast 1.02a | `ui/menus.txt` | 21 | 21 | 757 |

## The runtime (Source, Live)

`CL_InitUI` now hosts `cl_jedi_sp_ui.cpp` for both profiles before any
module could be looked for. It loads each game's list, registers the asset
fonts in order and the menus' sounds, and opens `mainMenu` when the client
asks for the main menu. It follows the engines' behaviour, read in Raven's
released menu sources (reference only, see `CREDITS.md`) and settled by the
executables where they differ:

- menus open over the menu with focus and give the focus back when they
  close; a menu's own scripts run on a stand-in item, as the engines run
  them;
- painting: full-screen backgrounds, window styles and borders, fades, timed
  item appearance, `transition`, `transition2` and `orbit`, item text with
  its alignment, wrapping and second line, and each item type's own drawing
  -- buttons and text, multiple choice, yes/no, sliders, edit fields, key
  bindings, list boxes with columns and scroll bars, Academy's text scroll,
  plain 3D models, and the owner draws the main menus use (the renderer's
  name, the key-binding prompt);
- input: a 640x480 cursor that hovers items and runs their mouse scripts,
  focus with its sound, keys to the focused item then the menu (arrows, tab,
  enter, escape), key-binding capture, text editing and list boxes;
- scripts: each engine's own table, `defer` and `rundeferred`, `delay`, and
  the game scripts the main menus use (`update`, the video set-up and its
  presets, `loadControls`, `saveControls`, `resetdefaults`, `clearError`,
  `LoadMods`, `RunMod`, `startgame`, `Quit` and the rest);
- the error menu after a failed start, and the engines' console commands;
  `ui_openmenu <menu>`, which the multiplayer menu modules have and these
  engines do not, is fnTech3's own for diagnostics and probes.

Presentation (AGENTS.md invariant 11): the menus' 640x480 screen reaches the
window as the Jedi multiplayer menus do (stretched, or with `cl_menuAspect 1`
in the centred 4:3 area), text goes through the Raven font path the
multiplayer hosts use on the active renderer, and every menu sound is an
interface sound (`S_StartInterfaceSound`). No retail-exact presentation
option is kept.

## Live (Live)

Each run: a client built from HEAD `0d1b7187` plus this change, windowed on a
private desktop, `net_enabled 0`, no input, an isolated home with an empty
config, the menus opened by `ui_openmenu` from a start-up config, engine
`screenshotPNG` captures. The companion lists every run's executable hash,
command, captures and outcome.

| Run | Window | Result |
| --- | --- | --- |
| Academy menus | 1280x720 | main, new game, skill, character, load, controls, setup and quit menus drawn |
| Academy | 1680x720, 960x720 | the main menu stretched and centred at both, and the setup menu at 1680x720 |
| Outcast menus | 1280x720 | main, new game, load, controls, setup, credits and quit menus drawn |
| Outcast | 1680x720 | main menu stretched and centred |
| Academy new game | 1280x720 | `map yavin1` stops at game import 128 (`WE_IsShaking`); the main menu returns with the error menu showing that message |
| Outcast new game | 1280x720 | `map kejim_post` stops at client import 3; the error menu shows that message |
| Academy, Vulkan and RTX | 1280x720 | the main menu draws on both; on RTX its centre ring's video draws white (below) |
| Academy with sound | 1280x720 | the menus run with OpenAL; no sound error |

Every run exited by itself, and every installation was unchanged. The error
runs reach the error menu through a probe-only menu in the run's isolated
home (`ui_menuFiles`): an error drop clears the command buffer, so a second
`mainMenu` there runs the capture when the engine reopens the main menu.
`com_errorMessage` is read-only, so it cannot be set from a script instead.

## Not served yet

- Character creation's species, skin and colour lists and its Ghoul2
  previews, and the saber menu's hilts (feeders 16 to 20, the `character`,
  `char_skin`, `saber_*` scripts): the lists are empty.
- Saved games: the list is empty and loading one is not offered, because the
  single-player save format is not served (the savegame gate).
- The in-game menus, datapad and loading screen, which need the running
  single-player game; menu cinematics (window style 5); `transition3`; a
  list's thumb dragged with the mouse.
- No click or key has been sent to these menus live: synthetic input for
  the Jedi profiles has not been asked for, so mouse and keyboard handling
  has source evidence only.

## Found on the way

- fnTech3's own `r_lodbias` default (-2) is none of Raven's three geometric
  detail choices, so the setup menu shows that line blank, as the original
  would for any value it does not list; `r_mode -1` does the same for the
  video mode.
- `jk2sp.exe`'s `CL_Init` registers `con_text` with flag 8 (`0x409610`);
  fnTech3's `jk2-sp` client does not yet.
- `yavin1` gives 120 `CM_AddFacetBevels... invalid bevel` warnings when it
  loads, and `kejim_post` 24.
- On the RTX renderer the main menu's video logo (`gfx/menus/videologo`, a
  `videoMap` shader drawn as a 2D picture) draws white, where GLx and Vulkan
  play it.

## Tests

`tests/raven_menu_tests.py` compiles the reader against tokenizer cases
(comments, quotes, signed characters, same-line numbers, discarded long
words), both engines' keyword tables and named values, script quoting, cvar
lists and the `cvarStrList feeder` form, and reads every installed menu file
with no warnings when the installations are present; it also checks the
runtime's text styles, script routing, main menu and string registration.
`tests/jka_module_refusal_tests.py` now checks that `CL_InitUI` hosts these
menus before any module could load, while the single-player client game
stays refused.
