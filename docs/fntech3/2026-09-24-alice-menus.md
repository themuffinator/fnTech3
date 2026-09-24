# Alice's engine menus: every widget class the twelve menus use

## Scope

American McGee's Alice ships no menu module. Its engine reads twelve `.urc`
resources from `base/pak0.pk3` at start-up (animate2, animate3,
cinemaquickmenu, cinematicmenu, controls, credits, gamecamera, loadsave, main,
newgame, quit, zound) and builds its menus from them. fnTech3 hosts that UI as
the client's UI module (`code/client/cl_alice_ui.cpp`). The main, newgame and
quit menus were already served. This record covers the rest: every widget
class the twelve menus use, the commands they send, the menu cvar rules, the
UI's pause, and three fixes made alongside them (commands that outlive the
UI's restart, Escape's order, and a bound on `.urc` includes).

Addresses are into `alice.exe`, image base `0x00400000`, and were read from
the retail binary with a disassembler (capstone) during this work. Observed
facts and inferences are kept apart, and inferences say so. Nothing from
Ritual's F.A.K.K.2 SDK or from any reconstruction is incorporated as text;
both were read for orientation only.

| File | SHA-256 |
| --- | --- |
| `alice.exe` | `8aa22917b7707f0e1a7c4dbe9a3c38332b1873ae2059174ac5329de26921fc18` |
| `base/pak0.pk3` | `6a63b59cd627902660bd4261461f904bf984e05891eb749d47bcc7fc8cedda62` |
| `base/pak1_small.pk3` | `25535bbb0dc6da4484b7931e221775aaa9817ad295f8aaed615f975b55c44da0` |

## Observed: the widget framework

- **Classes.** A `.urc` `resource` names its class. `Label` becomes
  FakkLabel; every other class is "UI" plus the token, looked up without
  regard to case (`0x0050ba68`). The twelve menus use Label, Button, CheckBox,
  Slider, Field, List, FakkBindList, FakkLoadGameClass, AutoScroll and
  Alice3D. `code/compat/alice_urc.hpp` reads the grammar.
- **The virtual slots the host serves**: 3 Draw, 4 FrameInitialized, 12
  KeyEvent, 13 CharEvent, 14 UpdateData (the widget writes its cvar) and 15
  UpdateUIElement (the widget reads its cvar).
- **A menu's show** (`0x004c4d50`) goes through its enabled widgets. For
  each one it calls the virtual slot at +0x20 with 1 and the motion routine
  (`0x004bb990`), then UpdateUIElement and then UpdateData
  (`0x004c4d97..0x004c4dae`). So a
  checkbox or slider writes back the value it has just read, and runs its
  stuff command. Then its `showcommand` lines ("a command to be executed
  when the widget is shown") are appended to the command buffer
  (`0x004bfd00`; the loop at `0x004c4db3` is its only caller).
  - Inferred: the retail menus' `showcommand` lines sit outside any
    resource and reach the buffer by the same path, so they run after the
    whole show.
  - fnTech3 appends a menu's show commands before it updates the widgets.
    Both orders only append, so the commands run after the show either way.
- **Cvar reads prefer the latched value.** The UI's read (`0x0040e1a0`)
  returns the latched string when there is one, and reads a latched number
  as a whole number (`0x004c1890`). Writes go through UI_SetCvar
  (`0x0040e2d0`), which is `Cvar_Set2` without force, so a latched cvar only
  latches.
- **Drawing.** DrawBox (`0x004b5a40`) scales the 640x480 menu units to the
  screen and cuts them to whole pixels. The background fill takes its alpha
  twice: once in the widget's Display (`0x004bd8ea`) and again in DrawBox
  (`0x004b59a0`). Draw3DBox (`0x004b5b00`) draws six one-unit strips.
  `3D_BORDER` is raised and `INDENT_BORDER`, the default, is sunken. `RAISED`
  is not a keyword, so a `.urc` that writes it keeps the default.
- **Border colours** (`0x004bc700`). `bgcolor` makes them from the
  background: dark is 2/3 of it (`0x00500598`), really dark is 1/3
  (`0x00500594`), and light is its HSV with saturation times 0.75
  (`0x0050036c`) and value times 4/3 (`0x00500368`). The HSV conversions
  (`0x004cdad0`, `0x004cd950`) treat a saturation within 0.035 of zero as
  grey (`0x005007f8`).
- **Waits.** A widget's `waitcommand`s fire only while the widget is drawn
  (`0x004bd740`). They are appended to the command buffer (`0x004bd7fd`
  calls the UI's import at `0x00aa667c`, which `0x0040e515` points at
  `0x00437a50`, the same append `skipcinematic` uses at `0x00429e37`). So
  they run after anything already queued.

## Observed: the classes

- **CheckBox** (constructor `0x004e0200`). It is drawn as a box, raised
  unless pressed, with the checked or unchecked shader (`0x004e0af0`). A
  release over it toggles the tick and runs the checked or unchecked command
  (`0x004e0780`). Its UpdateData writes the cvar as 0 or 1 and then runs the
  stuff command with `" 1\n"` or `" 0\n"` after it (`0x004e05e0`,
  `0x0052cf4c`, `0x0052cf48`). The space bar ticks it without the checked
  commands (`0x004e0c90`).
- **Slider** (constructor `0x004d9880`).
  - Properties: `setrange` (`0x004da530`), `slidertype` (`0x004da660`) and
    `stepsize` (`0x004da5f0`).
  - It is always drawn opaque (`0x004db440`).
  - Mouse: a press sets the value under the cursor (`0x004daea0`), a drag
    follows it (`0x004dac70`), and leaving the widget during a drag puts the
    old value back (`0x004dabb0`). The arrows step and write (`0x004da8b0`,
    `0x004da940`) and repeat while held (`0x004daa10`, `0x004daae0`).
  - UpdateData writes `"%d"` or `"%f"` (`0x004db120`), and the stuff command
    gets `"%d"` or `"%.3f"` (`0x004db1e5`, `0x004db282`).
  - The clamp after a step assumes a positive step. With a zero or negative
    step, the value is clamped to [max, min]. Retail's own controls.urc gives
    `cg_cameradist` `stepsize -1` over 76..192, so each arrow press there
    lands on 192. This is retail behaviour and is kept.
- **Field** (constructor `0x004df730`). It holds up to 255 characters; a full
  buffer refuses more (`0x004dfb76`). CharEvent inserts at the cursor
  (`0x004dfaa0`, `0x004dfb5f`). KeyEvent handles Backspace, Delete, the
  arrows, Home and End (`0x004dfc00`). The drawn text starts at the first
  character that keeps the cursor in view (`0x004df7c0..0x004df903`).
- **List** (constructor `0x004de400`). `additem <value> [text]` adds an item
  (`0x004de170`). The arrows are 15 units wide (`0x004de455`) and stop at the
  ends (`0x004dec80`, `0x004decd0`); a click between them goes round
  (`0x004deca0`). UpdateUIElement finds the cvar's item with `strcmp`
  (`0x004df0e0`, `0x0044fcb0`). A cvar that does not exist yet takes the
  first item.
- **FakkBindList** (controls.urc).
  - Its bind file, `ui/bind.scr`, is read through the bind listener
    (`0x00412f10`): the `bind_*` numbers (`0x00415960..0x00415cd0`) and
    `binditem "<name>" <command> [anim [looped [rotation [shader]]]]`
    (`0x004155f0`). Retail's file lists 29 items.
  - A key's name is shown through keybind.scr's aliases (`0x004d1810`,
    compared with `_stricmp` at `0x004d1f1d`/`0x004e8760`). Retail's file
    has 27 aliases.
  - A row shows the first two keys bound to its command, found by looking at
    all 256 keys (`0x00429450`). A key button waits for a key: any key but
    Escape takes the command (`0x004d1650`), and a clear removes it
    (`0x004d1700`). The window manager's binding functions are `0x004b4da0`
    and `0x004b52d0`.
  - Changing rows plays the row's animation on the menu's `alice3d` widget
    (`0x004e3c60..0x004e3d47`, `0x004e42c0`). The list draws its headers,
    rows and scrollbar in `0x004e4960` and `0x004d87b0`, and takes keys in
    `0x004e4330`.
  - It uses the normal binding table only. Alice's alt and ctrl binding
    tables do not appear in it.
- **FakkLoadGameClass** (constructor `0x00419dc0`, loadsave.urc).
  - Its refresh (`0x0041a210`) reads each `save/<config>_*.ssv` header. It
    leaves out `quicksave`, compared with `_stricmp` (`0x0041a68f`,
    `0x004e8760`), and sorts oldest first by the save's time as a number
    (`0x0041a7ac`, `0x0041a980`).
  - A page shows six saves. The select handler (`0x0041af20`) fills the six
    pictures, the page number, the big picture, the map title from
    maplist.scr (`0x0040b950`), and the date as `ctime()` without its newline
    (`0x0041adc6`). A slot with no save shows `ui/load/bigDefault.tga`
    (`0x0050d6ec`).
  - Events (the class's table is at `0x004199f0`):
    - `resetpage`, `refreshtab`, `selecttab`, `nextpage` and `prevpage`;
      the page arithmetic is at `0x0041b0c2..0x0041b10a`, `0x0041be30`,
      `0x0041be70` and `0x0041bde0`;
    - `loadgame` (`0x0041bcb0`);
    - `savegame` (`0x0041bec0`), which runs the save from inside the event;
    - `removegame` (`0x0041bbe0`), which deletes the four files in
      `fs_homepath`, as Com_WipeSaveGame (`0x0043a400`) does;
    - `CheckErrorState` (`0x00419ff0`), which shows a full disk and then
      forgets the error.
- **AutoScroll** (constructor `0x004c0340`, credits.urc).
  - Properties: `scrollspeed` (default 0.2, `0x004c0393`), `addline`,
    `addimage <shader> <x> <line> <w> <h>` (`0x004c0700`) and `clearlist`.
  - Lines are a font height plus the line spacing apart. Anything within 40
    units of the widget's edges fades, and the fade replaces the text
    colour's alpha (`0x004c0a40`, `0x004c0b23..0x004c0bbd`,
    `0x004c0c6d..0x004c0d1c`).
  - `resetscroll [line]` puts that line, or the first, at the top
    (`0x004c0490`).
- **Alice3D** (constructor `0x004d2ca0`, Draw `0x004d3310`, controls.urc).
  - The view covers the whole screen with a 30-degree field of view. The
    model's distance is its largest extent times its scale times 1.8656716
    (`0x004ff45c`); on a tie the later axis wins (`0x004d33d8..0x004d3416`).
    The entity's origin is moved by (0, -29, -43) (`0x004d2fc0`).
  - The animation runs at 0.02 frames a millisecond (`0x00500a2c`). A
    non-looping animation that ends goes back to `idle_stand_rocktoes`
    (`0x0052bfb8`). The eyes close for 4 frames in every 100 (`0x004d32a9`).
  - The view is `RDF_NOWORLDMODEL` alone (`0x004d3869`). The cvar pointer
    for `ui_skipDepthClear` (`0x00531044`) is read only by UI_Render3DModel
    (`0x004083e0`, besides its registration at `0x004105a5`), so Alice3D
    clears depth.
  - **Two lights.** Retail offsets both from (distance, centre Y, centre Z):
    - A steady light at (+250, -300, -200), with intensity
      `sin(t * 0.00314159) * 300 + 1000` (`0x004d34bd..0x004d353a`).
    - A lightning light at (-250, +300, -200) (`0x004d3738..0x004d378a`). Its
      intensity is renderer noise at `t * 0.001 * 3` times 4000
      (`0x004d3540..0x004d355f`). Above 2000 it brings a thunder sound, at
      most every five seconds (`0x004d3568..0x004d372c`).
    - The model faces the camera, so the steady light is behind it. **Between
      lightning flashes the model's front is dark by design.** The
      screenshots below catch both states.

## Observed: menu cvars, commands, pause and Escape

- **Menu cvars.** A push saves every linked cvar of the menu (SaveCvars
  `0x004c4540`, from `0x004c3bf6`). Escape, and `popmenu 1`, put them back
  (RestoreCvars `0x004c4830`, pop `0x004c3f20`). `ui_resetcvars` resets them
  (`0x004c4a00`). The `ui_*cvars [group]` commands are `0x0040f230`,
  `0x0040f260` and `0x0040f290`. `ui_setcvars` first checks for a restart
  (`0x004c4910`). A linked cvar whose waiting (latched) value differs from
  the one saved asks for its system's restart: `"vid_restart; in_restart\n"`
  for video (`0x0052b5cc`) or `"snd_restart; in_restart\n"` for sound
  (`0x0052b5b0`).
- **Commands.** UI_Init registers the UI's commands once, from `pushmenu` at
  `0x004105fc` to `toggleconsole` at `0x0041080b`, and retail never shuts
  that UI down.
  - Besides the menu commands, UI_Init registers `togglemenu`, `loadmenu`,
    `maplist`, `viewspawnlist`, `soundpicker`, `notepad`, `editscript`,
    `editshader`, the inventory commands, `ui_saveconsolepos`,
    `ui_loadconsolepos`, `ui_testlist`, `ui_checkrestart`, `saveweaps` and
    `toggleconsole`.
  - Of these, the retail menus use `togglemenu` (newgame), `viewspawnlist`
    (animate2, animate3) and `soundpicker` (zound). The rest appear only in
    the developers' own `configs/*.cfg` in the paks, not in `default.cfg`.
  - fnTech3 serves `togglemenu`, `clear` and `toggleconsole`, but none of
    the others: `viewspawnlist`, `soundpicker`, `loadmenu`, `maplist`,
    `notepad`, `editscript`, `editshader`, the inventory commands, the
    console-position and test-list commands, `ui_checkrestart` and
    `saveweaps`.
- **The pause** (`0x0053162c`, UI_Update `0x0040c800`). None of this runs
  while the loading screen is up (`0x0040c8cb`).
  - When a full-screen menu is current and the state is 0, UI_Update
    (`0x0040c92e..0x0040c95e`) toggles the pause and stores 1 if the game is
    not paused, or stores 2 if it already is.
  - With no full-screen menu, in play (`0x0040c9b3..0x0040c9dd`) or in a
    cinematic (`0x0040c9fb..0x0040ca15`), it unpauses only when the state is
    1 and `paused` is set, then stores 0.
  - UI_EndLoad (`0x0040ecf0`) stores 0 and unpauses whoever paused.
  - The toggle is `pause`'s (`0x00439800`).
  - Only these seven instructions name `0x0053162c` directly (an
    absolute-address scan; it does not rule out an access through a
    register).
- **Escape** (CL_KeyEvent `0x00429d9d..0x00429e63`), in this order:
  1. It is ignored while developer is off and the intro has not given way
     (`0x00429dbe`).
  2. A second press within 1000 ms, outside a cinematic, is a menu escape
     (`0x00429dce..0x00429e09`).
  3. A menu that is up takes it.
  4. Only then does a scripted cinematic in play get `skipcinematic`
     (`0x00429e29..0x00429e3c`).
  5. Otherwise the menus take it, or a cinematic is stopped.

## What fnTech3 does

- `code/compat/alice_menu.hpp` (new) holds the pure rules, each with its
  address: colours, the 3D box, the slider arithmetic, lists, the field's
  edit line, the bind file and key aliases, the save rows and pages, and the
  scroller.
- `code/client/cl_alice_ui.cpp` draws each class and feeds it keyboard and
  mouse input through the existing key and mouse event paths.
  - It adds the class events, the menu cvar rules, the load/save flow and the
    Alice3D view.
  - Its header comment lists the served addresses and what is left out.
- **Commands.** Every UI command is registered once for the process and
  survives the hosted UI's restarts, as retail's are.
  - `Shutdown` removes none of them.
  - A `pushmenu`, `forcemenu` or `showmenu` issued while the UI is down (a
    map load or a video restart) is queued for the restart. `popmenu` takes
    the last one queued.
  - The other commands act on the current menu, and there is none while the
    UI is down.
- **The pause** has retail's three states in `uiPauseState`.
  `CL_AliceFirstSnapshot`'s release stays unconditional, as UI_EndLoad's is.
- **Escape.** `CL_ToggleMenuInternal` no longer sends `skipcinematic` ahead
  of the engine UI. Every Escape with no menu holding the keys goes to
  `EscapeKey`, which follows retail's order, so a double press during a
  scripted cinematic reaches the menus.
- **Includes.** `alice_urc.hpp` now bounds its work. Retail has no bound: a
  cycle there recurses until the stack runs out.
  - One parse follows at most 64 includes and 4 MB of included text, besides
    the existing depth limit of 8. An include past either limit is refused
    with a diagnostic.
  - A parse keeps at most 256 diagnostics and counts the rest.

## Deviations from retail

- **Saving.** The load/save list's `savegame` event runs `savegame`
  immediately in retail. fnTech3 must not run a command immediately from
  inside the UI (`tests/alice_cinematic_tests.py` forbids `EXEC_NOW` in the
  hosted UI). It inserts `savegame` at the front of the command buffer
  instead, and reads the result on the next UI frame. With no error, that
  refreshes the list, selects the newest save and closes the menus; a full
  disk is shown instead.
  - Inferred, not probed: if a `wait` is holding the command buffer when the
    save is chosen, the result is read before the save runs, so the menus
    close without the refreshed list.
- **The save picture.** The save's picture is written a frame after the save
  (the savegame code's order), so a list refreshed at once shows the default
  picture for the newest save until the next refresh.
  - Inferred, not probed: the renderer caches images by name, so a save name
    reused after a delete may show the earlier picture until the renderer
    restarts.
- **Pausing only in play.** A full-screen menu pauses the game only when the
  client is in play (`CA_ACTIVE`). Retail's decision reads no connection
  state. The change is needed because fnTech3's pause stops the local
  server's snapshots (`SV_CheckPaused`): a pause taken before the first
  snapshot, as a menu restored by a video restart could take, would never be
  released. The state machine is otherwise retail's.
- **Shutdown.** The hosted UI restarts with the renderer, which retail's
  never does, so `Shutdown` gives back only the pause its menu took, by the
  same rule. The restored menu pauses again once the game is back in play,
  as the runtime log below shows.
- **`initdata`** writes its cvar once per process. Retail reads the menu
  files once, when UI_Init runs.
- **`ui_restorecvars`** resets instead of restoring when the current menu
  was put up by `showmenu` (`0x004c52c4`). fnTech3's `showmenu` pushes, so
  that case does not arise.
- **Not served:**
  - the bind list scrollbar's arrows do not repeat while held;
  - UIWidget3D does not turn the model's head to follow the mouse (bone
    `Bip01 Head`, `0x004d31ed`);
  - `soundpicker`, `viewspawnlist` and the developer tools they open, so
    zound's Sound button and animate2/animate3's spawn list report an
    unknown command.
- **Inferred: the restart check tells sound cvars by name.** It treats a
  cvar whose name starts `s_` as a sound cvar. Retail tells the two kinds
  apart by the cvar's flags: 0x20 for video and 0x1000 for sound. Its sound
  cvars all start `s_` (registered at `0x0044c738..0x0044c75b` and
  `0x0046e4dd`). This engine latches both kinds with one flag.

## Tests

- `tests/alice_menu_tests.cpp` (new, `fntech3_alice_menu`) checks the pure
  rules: colours, the 3D box, slider travel, steps and the sign quirk, lists,
  field edits and the visible start, the bind script, key aliases, the save
  rows, pages and dates, and the scroller's fade. It also reads retail's
  `ui/bind.scr` (29 items) and `ui/keybind.scr` (27 aliases) out of
  `pak0.pk3` when an installation is present.
- `tests/alice_urc_tests.cpp` gains `TestIncludeBudget`:
  - a file that includes itself three times reads exactly 64 files;
  - a thousand missing includes ask the reader 64 times and keep 256
    diagnostics;
  - two 2 MB includes refuse the second;
  - 300 stray lines keep 256 diagnostics and count 44.
- `tests/alice_menu_source_tests.py` (new) pins:
  - that the commands are registered once and never removed, with the
    command and handler tables in step;
  - that stack commands queue while the UI is down;
  - the pause's three states, the Shutdown rule and UI_EndLoad's release;
  - that saves and loads go through the command buffer;
  - that urc diagnostics past the cap are counted.
- `tests/input_system_source_tests.py` now pins that Escape goes to the
  engine UI and that `EscapeKey` keeps retail's order, with `skipcinematic`
  after the double press.

Results on this tree (the uncommitted working tree over `2d35919`, Windows
x86, MSVC, `meson/build/win32-alice-ui`):

- `fntech3_alice_menu` and `fntech3_alice_urc` pass. The C++ run covered 20
  Alice tests, and all 20 pass.
- `fntech3_alice_sound` does not compile at the time of writing. The failure
  is outside this change: `tests/alice_sound_tests.cpp`'s `FakeDevice` is
  abstract after another session's audio edit. Later the same day it built
  and passed again (`meson/build/win32-alice-content`).
- 344 Python tests pass: every `tests/alice_*_tests.py`, plus
  `webui_wiring_tests.py` and every test that reads `cl_alice_ui.cpp` or
  `cl_keys.cpp`.

## Runtime evidence

Build: `fntech3.exe` SHA-256
`61b976c1c586106e0804946503dfa1e173f09b5aa48ac096ab45c0f981f06851`,
`fnql_glx_x86.dll`
`e187dc1fdfa144ef07a5041928102f53af4d40a017c46c6e1581ff5e7bd371b1`, both
from `meson/build/win32-alice-ui` (`-Dalice-development=true`).

Each run was windowed, 800x600. It ran on a private desktop with no input
injected, and a home path under `.tmp/alice-ui2/<run>/home`:

```
fntech3.exe +set com_gameProfile alice +set fs_basepath E:\Games\Alice
  +set fs_homepath <run>\home +set fs_game "" +set fs_cdpath ""
  +set r_fullscreen 0 +set r_mode -1 +set r_customWidth 800
  +set r_customHeight 600 +set in_mouse 0 +set in_joystick 0 +set in_nograb 1
  +set s_initsound 0 +set com_steamIntegration 0 +set fs_skipWorkshop 1
  +set cl_webuiEnable 0 +set cl_renderer glx +set logfile 4
  +set net_ip 127.0.0.1 +set net_port 0 +set developer 1
  +exec alice-probe.cfg (+vstr probeAction | +devmap garden1)
```

- **How the runs are driven.** Every step is a console command from
  `alice-probe.cfg`: `pushmenu`, `forcemenu`, `widgetcommand`, `savegame`,
  `pause`, `paused` and `vid_restart`, with `wait` in milliseconds.
  - Frames are validated only through the engine's `screenshotPNG`.
  - Show and wait commands are appended behind the probe's own queue, as
    retail appends them, so the probe sends the ones it needs explicitly.
  - Both runs exited with code 0, and the installation's file inventory was
    identical before and after each run.
- **At the main menu** (`.tmp/alice-ui2/final-ui`):
  - `m01-main`;
  - the controls groups, `m02-controls` to `m05-game`: the main group,
    video, audio, and game with its checkboxes, sliders and lists;
  - `m06-bind`: the bind list with key aliases, its scrollbar, and Alice in
    the mirror lit by a lightning flash;
  - `m07-credits`: the scroller with its edge fade and pictures;
  - `m08-quit`, `m09-loadsave-empty`, `m10-zound` and `m11-gamecamera`;
  - `m12-bind-dlightmode0`, after `r_dlightMode 0; vid_restart`: the menus
    came back after the restart, and Alice is caught between flashes.
    Developer mode's console also opens (see Open items).
- **In play on `garden1`** (`.tmp/alice-ui2/final-garden1b`):
  - `g00-garden1`;
  - `g01-loadsave-one`: one save, made by the console's `savegame`, with its
    picture, "Dry Landing" and its date;
  - `g02-after-menu-save`: the list's `savegame` event wrote a second save
    and closed the menus;
  - `g03-loadsave-two` (the newest save selected), `g04-loadsave-tab1` and
    `g05-loadsave-delete` (the confirmation);
  - `g06-controls-bind`, `g07-main-ingame` (with Back to Game),
    `g08-zound` and `g09-gamecamera`.
  - The console log shows `Saving game...garden10000` and `...garden10001`.
- **The pause, from the same run's console log:**

  | Step | `paused` |
  | --- | --- |
  | before any menu | 0 |
  | `main` up | 1 |
  | `main` closed | 0 |
  | after the player's `pause` | 1 |
  | `main` over that pause | 1 |
  | `main` closed | 1 (the player's pause is kept) |
  | after `pause` again | 0 |
  | `main` up before `vid_restart` | 1 |
  | the same menu after `vid_restart` | 1 |
  | closed | 0 |

  An earlier run, `.tmp/alice-ui2/final-garden1`, repeats the first seven
  rows. Its `g04` shows the probe artifact above: the button's
  `selecttab` wait had not run yet.

## Open items

- Alice's `soundpicker` and `viewspawnlist` developer tools.
- The bind list scrollbar's arrow repeat.
- UIWidget3D's head turn.
- With developer on, a `vid_restart` at the main menu opens the console: the
  client's menu restore asks for `UIMENU_MAIN`, and that request takes the
  drop path. Retail's UI does not see a video restart at all.
- `code/compat/alice_savegame.hpp` describes the `quicksave` test at
  `0x0041a68f` as `strcmp`, but the call is `_stricmp` (`0x004e8760`). The
  menu's list compares without regard to case, as retail does.
