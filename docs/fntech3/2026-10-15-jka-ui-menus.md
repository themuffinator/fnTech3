# Jedi Academy's menus the client game opens, 2026-10-15

The [siege slice of the same day](2026-10-15-jka-siege.md) left every
Academy siege game (`g_gametype 7`) stopping on the client when the client
game opened UI menu 11, Raven's class menu. fnTech3 then served the client
game's menu requests for menus 0 to 5 only and refused the rest by number.
This slice serves the request as retail `jamp.exe` 1.01 serves it: the
client game's number goes to the UI module's set-active-menu export
unchanged, and the module's own switch decides. The
[JSON companion](2026-10-15-jka-ui-menus.json) records the binaries, retail
identities, call sites, commands and outcomes. No compatibility gate closes.

Implementation is independently authored. Retail `jamp.exe` 1.01 and the
1.01 modules in `assets3.pk3` were read with capstone and pefile. Pinned
OpenJK was used only to name routines (reference observation; no text
incorporated).

## Retail

### Observed, `jamp.exe` 1.01

- The client game's dispatcher (`0x412170`) bounds the import at `0x15e`
  and jumps through two tables. Import 238 (`CG_OPENUIMENU`) is index 118,
  served at `0x413032`.
- There the engine loads the UI module's VM pointer (`0xb28224`), pushes the
  client game's word and 7, and calls `VM_Call` (`0x44ae90`). It answers 0.
  Nothing between the import and the call reads the number: no table, no
  bound.
- `0xb28224` is the UI module: `CL_InitUI` (`0x4254c0`) stores
  `VM_Create( "ui", ... )` there (`0x42550c`) and `CL_ShutdownUI` clears it
  (`0x4254b4`). Export 7 is `UI_SET_ACTIVE_MENU`.
- `VM_Call` stops the process with `ERR_FATAL` ("VM_Call with NULL vm",
  `0x44aea0`) when the pointer is null. Otherwise it calls the module's
  entry point with the call number and sixteen words.
- Of the 133 calls to `VM_Call`, nine hand the UI module export 7:

  | Site | Menu | From |
  | --- | --- | --- |
  | `0x41303d` | the client game's word | client-game import 238 |
  | `0x41694f`, `0x41d53d` | 0 | `0x41d53d` on a disconnect (`ebx` cleared at `0x41d4a6`) |
  | `0x41c423` | 2 | Escape in a game (`CL_KeyEvent`, `0x41c290`) |
  | `0x41c448` | 1 | Escape otherwise (`ebx` set to 1 at `0x41c2c2`) |
  | `0x41ceec`, `0x41f132`, `0x422f57` | 1 | |
  | `0x418a5d` | 9 | the `voicechat` command (`0x418a50`, registered at `0x41af38`) |

  Three more calls push 7 as the call number to the client game
  (`0x419849`) and the game (`0x45980b`, `0x459b32`).

### Observed, `uix86.dll` 1.01

- `vmMain` (`0x400178f0`) sends export 7 to `_UI_SetActiveMenu`
  (`0x40010de0`).
- That returns at once when no menu is loaded (`Menu_Count`, `0x40010de6`)
  and when the number is above 11, compared unsigned, so negative numbers
  too (`0x40010dfa`). Otherwise it jumps through a 12-entry table
  (`0x40011008`):

  | Menu | Raven's name | What the case does |
  | --- | --- | --- |
  | 0 | `UIMENU_NONE` | takes the UI's key catcher off, clears keys, unpauses, closes every menu |
  | 1 | `UIMENU_MAIN` | opens `main`, and `error_popmenu` when there is an error message |
  | 2 | `UIMENU_INGAME` | pauses, opens `ingame` |
  | 3 | `UIMENU_PLAYERCONFIG` | opens `ingame_player` |
  | 4 | `UIMENU_TEAM` | opens `team` over what is open |
  | 5 | `UIMENU_POSTGAME` | opens `endofgame` |
  | 6 | `UIMENU_PLAYERFORCE` | opens `ingame_playerforce` |
  | 7 | `UIMENU_SIEGEMESSAGE` | opens `siege_popmenu` |
  | 8 | `UIMENU_SIEGEOBJECTIVES` | opens `ingame_siegeobjectives` |
  | 9 | `UIMENU_VOICECHAT` | opens `ingame_voicechat` when `g_gametype` is at least 6 |
  | 10 | `UIMENU_CLOSEALL` | closes every menu and leaves the key catcher alone |
  | 11 | `UIMENU_CLASSSEL` | opens `ingame_siegeclass` |

  Every case that opens a menu takes the UI's key catcher; all but 4 close
  the open menus first.

### Observed, `cgamex86.dll` 1.01

- One wrapper (`0x3005ed30`) pushes 238 and calls the engine. Seven sites
  call it, each with a constant: 10, 8 and 7 (`0x30027e12`, `0x30027e33`,
  `0x30027e3e`), 3 (`0x30039ea8`, `0x3005d194`, `0x3005d23e`) and 11
  (`0x3005d15e`).
- The 1.00 module in `assets2.pk3`, which `assets3.pk3` shadows, has the
  same seven sites with the same numbers.
- By OpenJK's names, 10 then 8 or 7 is a siege briefing or message, 11 the
  server's `scl` command, and 3 the profile menu.

### Inferred

The menu number belongs to the UI module. The retail engine passes any
number and keeps no list of its own; the bound that exists is the module's.
A menu module that defines more menus would receive them too.

## fnTech3 before

`CG_OPENUIMENU` translated the client game's number into the engine's menu
vocabulary (`FNTech3_JKAUIMenuToInternal`, 0 to 5). The Academy branch of
`VM_Call` then translated it back for export 7. Any other number dropped
with "Jedi Academy cgame UI menu %ld is not served yet".

## Implemented

- **A request of the module's own.** `UI_SET_MODULE_MENU` (`0x10003`) joins
  the engine-only requests beside `UI_GET_ACTIVE_MENU` in
  `code/ui/ui_public.h`. `code/compat/jka_ui_exports.h` maps it to export 7
  with one word. `VM_Call`'s Academy branch translates only
  `UI_SET_ACTIVE_MENU`, so this number reaches the module as the client game
  gave it. `vm.c` is unchanged.
- **The import.** `code/client/client_jka_cgame_host.hpp` hands `args[1]` to
  the UI module through that request and answers 0, as `0x413032` does.
  `FNTech3_JKAUIMenuToInternal` is gone.
- **The engine's own requests** keep their vocabulary: the engine still asks
  for its menus by its own names, translated to 0 to 5.
- **Numbers outside 0 to 11** go to the module unchanged, as retail sends
  them. Retail's `uix86.dll` ignores them.
- **Partial initialization** is refused before any module entry, as before:
  no UI module, one whose `UI_INIT` has not finished, an error in progress,
  a module that is not the legacy native UI, a missing entry point,
  structured exports, a 64-bit host, or a client game without its entry
  point. Retail stops the process on a null module and otherwise calls it;
  its module ignores every number while no menu is loaded. A client game
  runs only after the UI's `UI_INIT`, so the two differ only in states a
  game does not reach.
- **Presentation.** The menus are the retail UI module's own. They draw
  through Fn's existing 2D path from Raven's virtual 640x480 screen, placed
  by `cl_menuAspect`, with Fn's menu blur (`cl_menuBlur`) behind them; the
  diag3 capture below shows the blur, and the other probes turn it off for
  clean captures. This slice adds no presentation feature and leaves none
  out.

## Tests

- `tests/jka_ui_menu_tests.py` (new) compiles `VM_Call`'s Academy UI branch
  as written, with the production header. The module request reaches export
  7 with each of 17 numbers (0 to 12, 255, -1, `INT_MIN`, `INT_MAX`)
  unchanged and nothing after it. The engine's own requests still translate,
  and their unknown menus still drop. A wrong word count, a missing entry
  point and structured exports drop before the module. A nested call keeps
  the outer call's counters.
- `tests/jka_cgame_host_tests.py` runs the production case: the same 17
  numbers each reach the module request once, and every guard refuses menu
  11 with nothing called.
- `tests/jka_ui_tests.cpp` pins the request to export 7 and one word, and
  the engine vocabulary to 0 to 5, each once.
- `tests/jka_sp_vm_export_tests.py` drops its stub of the removed mapping.
- Six mutations, run in an export of `8779edb5` with this slice, are each
  caught: `vm.c` translating the module request, the request mapped to
  another export, the request without its word count, the host bounding the
  number again, the host sending the engine request, and the host without
  its readiness guard.

## Live

Clients ran windowed at 800x600, on a private desktop, with no network
sockets, sound off and no input; console commands came from the probe's own
config. Each ran on its own home, and the installation was unchanged.
Captures are the engine's `screenshotPNG`. The pair below was built from an
export of `80d15494`, without and with this slice. The commits between it
and this commit's parent leave the menu path alone; the decal commit
(`4782edce`) changed two other cases of the same host file.

| Map, siege | Before | After |
| --- | --- | --- |
| `mp/siege_hoth` | stops at UI menu 11 | the class menu opens; stops at client-game import 312 |
| `mp/siege_desert` | stops at UI menu 11 | the class menu opens; stops at client-game import 312 |
| `mp/siege_korriban` | stops at UI menu 11 | the class menu opens; plays to the probe's end |
| `mp/siege_korriban`, `team red` | stops at UI menu 11 | joins as "Jedi Warrior"; waits for the other team |
| `mp/siege_korriban`, `team red`, `addbot Tavion 1 blue` | -- | the round counts down; the server stops at a trace with pass entity -1 |

- **The class menu.** Each map's server sends `scl` and the client game asks
  for menu 11. On `mp/siege_korriban` the captures show the module's
  `ingame_siegeclass` screen: the map's objectives, "Join a Team" with
  Light Jedi, Dark Jedi, Spectator and Auto-Join, Cancel and Okay.
- **Past it, from the console.** `team red` sets the player's class to the
  team's first, "Jedi Warrior" (`model jedi_kdm`, saber `dual_1`), and the
  next configstring puts the player back among the spectators. A siege round
  begins only when both teams have a player. With `addbot Tavion 1 blue` it
  counts down (`cs 293 "2|1000"`), and the server stops at "Jedi Academy
  trace cannot read pass entity -1 or the entities it touched".
- **The first pair.** The same runs on an export of `8779edb5`, before the
  [frame-index slice](2026-10-15-jka-setanimindex.md), stopped
  `mp/siege_hoth` and `mp/siege_desert` at renderfx `0x80040` after the menu
  opened. `mp/siege_korriban` behaved as above.

### Diagnostic runs

Three throwaway runtimes, never committed, took the same export further on
`mp/siege_korriban` with `team red`, the bot and the client game's
`briefing` command:

| Runtime | Patch | Outcome |
| --- | --- | --- |
| diag | the trace reads pass entity -1 as none, as Outcast's does | the round begins; the client stops at import 215 (`CG_R_DRAWROTATEPIC2`) |
| diag2 | and import 215 answers without drawing | `briefing` asks for menus 10 and 8, both served; the client stops at import 315 (`CG_G2_CLEARSKINGORE`) after the server's `ircg` |
| diag3 | and import 315 answers | plays to the probe's end; menus 10 and 8 are asked for twice |

The diag3 capture shows the module's `ingame_siegeobjectives` screen, Mission
Objectives with Korriban's primary objective and map, over the game. These
runs stand only for what lies beyond each stop; the patches are not
proposals.

## Validation

- **The full run.** An export of `643016c6` with this slice was configured
  as the Windows x86 developer build with strict warnings. It built all
  1,486 targets with no warning. Advanced to `4782edce` and then
  `30c1286d`, path for path the tree a commit on each would have had, it
  rebuilt with no warning each time.
  - On `30c1286d` the complete Python suite ran one file to a process:
    2,773 tests and 67,314 subtests passed, and none failed. 131 were
    skipped for tools, installations and privileges this machine lacks.
  - On `4782edce` the native suite passed 273 of its 276 tests and skipped
    two: OpenAL loopback, which this machine's library lacks, and the
    optional Quake Live factory probe. The third,
    `fntech3_jka_render_host`, failed the same way without this slice: the
    decal commit's new call had no stub in that fixture, which `b589caf8`
    added.
  - A first single-process run on `4782edce` passed its first 1,417 tests,
    then waited in a pipe read with no child process left. It was stopped
    and replaced by the per-file run.
- **Advancing with HEAD.** HEAD moved during validation. The export was
  advanced with this slice to `f8e1ee98`, `922e725a` and `f5381f0e`, path
  for path the tree a commit on each would have had, and rebuilt with no
  warning each time.
  - On `f8e1ee98` and `922e725a` the native suite passed 274 of 276 tests
    and skipped the same two.
  - On `922e725a`, 162 test files passed 1,238 tests and 1,501 subtests,
    with 2 skipped: every file that reads this slice's code, `vm.c`,
    `ui_public.h` or the documents, the shader parser `f8e1ee98` changed,
    and the tests of `f8e1ee98` and `922e725a`.
  - On `f5381f0e` the native suite passed 274 of 276 with the same two
    skipped, and 98 of 100 test files passed: those that read the
    documents, this slice's tests and the tests of the three newer commits.
    The other two, `map_shader_flag_dialects_tests.py` and
    `raven_bsp_shader_flags_tests.py`, fail to import on that tree with or
    without this slice: their `engine_fixture_args` helper was not
    committed yet. With the shared tree's helper copied in, both pass.
- **The commit's content.** Last, the export was advanced to `95f6d6d3`
  with this slice and rebuilt with no warning. Its native suite passed 274
  of 276 tests with the same two skipped, and 23 of 24 test files passed:
  this slice's tests, the Academy and release files that read the
  documents, and the tests of the three newer commits. The 24th was
  `raven_bsp_shader_flags_tests.py`, still without its helper, which
  `c45b50d8` then committed. This commit sits on `c45b50d8`; it and
  `540fe30e` change none of this slice's files.

## Remaining and unrelated findings

- **Siege's next stops.** `mp/siege_hoth` and `mp/siege_desert` stop at
  client-game import 312 (`CG_G2_SKINLESSMODEL`). OpenJK names its caller as
  the general entities' Ghoul2 path (`CG_General`). `mp/siege_hoth` in a
  free-for-all reached the same import after the frame-index slice.
- **A trace with pass entity -1.** With a bot playing, the server stops at
  its Academy trace. Retail `jamp.exe` compares the pass entity only with
  1023 (`0x4624f8`): any other number, -1 included, reads that record's
  owner (`+0x294`, `0x462504`) and flags (`+0x238`, `0x462522`), so -1 reads
  the record before the entity array. Outcast's trace already reads -1 as
  no pass entity ([JK2 records](2026-09-22-jk2-records.md)). OpenJK's bot
  code traces with -1.
- **Beyond it.** In these runs, client-game imports 215
  (`CG_R_DRAWROTATEPIC2`, which OpenJK's client game uses for the siege
  radar) and 315 (`CG_G2_CLEARSKINGORE`, on the server's `ircg` after a
  respawn) were refused. A concurrent zoom slice serves 215.
- **The `voicechat` command.** Retail's engine registers `voicechat`, which
  opens menu 9 (`0x418a50`). fnTech3 does not register it.
- **Pictures the menus name.** Opening the class menu logs "Shader image
  unavailable" for `*team1_objective7_mapicon` and
  `*team2_objective7_mapicon`, and the objectives screen for
  `*siege_objective6_gfx`, `*siege_objective7_gfx` and their map icons. Not
  examined.
- **The engine's names for 3, 4 and 5.** Academy's engine paths no longer
  send `UIMENU_JKA_PLAYERCONFIG`, `UIMENU_TEAM` or `UIMENU_POSTGAME`; the
  retail engine never sends 3, 4 or 5 either. The names remain in the
  engine vocabulary.
