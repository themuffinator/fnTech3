# Console commands, screenshots, automatic demos, stray keys and Alice's in-game cinematics

## Scope

Five client and engine questions came out of the Alice probes of 2026-09-23
and 2026-09-24. Two needed changes for every profile: a `screenshot` request
could vanish without a word, and `cl_autoRecordDemo` printed a refused
`record` every frame. The Alice cinematic's server shutdown was changed for
the `alice` profile only. The other two were findings with no engine defect:
the `putawayweapon` refusal is retail's own, and the stray keys came from a
run on the user's desktop. `tests/engine_input_capture_tests.py` covers all
five.

Retail inputs, read only, with capstone and pefile:

| File | SHA-256 |
| --- | --- |
| `E:\Games\Alice\alice.exe` | `8aa22917b7707f0e1a7c4dbe9a3c38332b1873ae2059174ac5329de26921fc18` |
| `E:\Games\Alice\base\fgamex86.dll` | `ae5a572728fbb2cbf8935d633ef9e23f023edad986d7dd9ee506d589eb261aea` |
| `E:\Games\Alice\base\pak0.pk3` | `6a63b59cd627902660bd4261461f904bf984e05891eb749d47bcc7fc8cedda62` |
| `E:\Games\Alice\base\Pak3.pk3` | `a779bf348edcfb1d8711d1aad08537b30cf5794e6f8149168f4ca7a415aab8e4` |

Runtime probes used `meson/build/win32-alice-client` (`-Dalice-development=true`,
Windows x86, MSVC), windowed at 800 by 600. Each ran on an unswitched private
desktop (`tests/windows_private_process.py`), with no input sent, sound off,
`developer 1` and an isolated home under `.tmp/engine-input-capture/<run>/`.
Images came from the engine's own `screenshotPNG`. Every run left the
installation's file inventory unchanged, and the executable's hash was the
same at the start and end of each run. The harness is
`.tmp/engine-input-capture/probe.py`, next to the cfgs it copies in. The tree
was HEAD `2d35919` plus the shared checkout's uncommitted work, including the
uncommitted Alice profile.

| Build | `fntech3.exe` | Renderer |
| --- | --- | --- |
| Before (built 08:44 from the shared tree, before these changes) | `14a5f486816c078b0d948aaa6346793b9414d5f2407fdf312c5d3408582d90f7` | glx `12343009171bb14466c1891d531f4581edd047d5fec0baa21965aafae0e4e939` |
| After | `76726fa37472d749a579d08cdc5dbea8ba7175645875c1595107470356163278` | glx `2fc7033588b38c7ba13455531a745a64af736157884188e3e0683ca9a5c7627f`, vk `ff9b99ffcfa1ff1373b252c27a36de42979eae4951afca829bb42e954ad25be6`, rtx `1d53a2c13960a4bb833e9f514358c1865d565a8be7786c20bdddde2bb5364bbb` |

## 1. `putawayweapon` from the console

### Observed

The game module decides alone. No flag or argument from the engine matters:

| Address | Behaviour |
| --- | --- |
| `fgamex86` `0x100af390` | `G_ClientCommand( ent )`: one argument, handed to `0x100af400`; a false return makes the text a chat line (`0x100af3d0`) |
| `fgamex86` `0x100af320` | `G_ConsoleCommand`: the same `0x100af400` for `g_entities[0]` |
| `fgamex86` `0x100af400` | `G_ProcessClientCommand`. After the engine-level table (`say`, `eventlist` and the rest at `0x101c6b80`), a known event name becomes an `Event`. At `0x100af591..0x100af5a3` its info word is `(info & ~8) | 4`, setting the source field (bits 2 and 3) to 1, *from the console*, for every client command |
| `fgamex86` `0x100d86b1..0x100d86c1` | `Listener::ProcessEvent`: when the source is the console (`(info & 0xc) == 4`) and neither flag bit `0x10` nor `0x20` is set (the SDK's `EV_CONSOLE` and `EV_CHEAT`), it sends `print "Command not available from console.\n"` (string at `0x101ca894`, pushed at `0x100d86f3`) and processes nothing |
| `fgamex86` `0x100f3220` | `putawayweapon` is registered with flags `-1` (`push -1`, `0x100f322f`). It is the module's only definition of that name |
| `fgamex86` `0x100d697e..0x100d698b` | A first registration with `-1` stores flags 0 |
| `alice.exe` `0x00452f11..0x00452f21` | `SV_ExecuteClientCommand` pushes the client's entity (client +`0x20430`) and calls export +`0x24`, `ClientCommand`. Nothing else is pushed |
| `alice.exe` | Holds no `putawayweapon` string. The client forwards the unknown command like any other |

`putawayweapon` also names an animation in `models/alice.tik`. The player's
command is `holster`: `alice.exe` appends `holster\n` at `0x0040ffcc` and
registers `+holster` and `-holster`, and pak0's `configs/test.cfg` binds `h`
to it.

### Conclusion

Retail refuses the typed command too. fnTech3 calls `ClientCommand( entity )`
exactly as retail does (`code/server/server_alice_game.hpp`,
`GAME_CLIENT_COMMAND`), so the refusal is the module's own and the behaviour
stays as it is.

## 2. A screenshot request that wrote nothing

### Cause

`screenshot`, `screenshotPNG`, `screenshotTGA`, `screenshotJPEG` and
`screenshotBMP` only schedule a capture. The end of the next rendered frame
writes it and prints `Wrote <name>`. Each renderer keeps one waiting capture
per file type, and two paths dropped requests without a word, in all three
renderers:

- `R_ScreenShot_f` returned silently when a capture of the same type was
  already waiting (`// check if already scheduled`). Alice's `wait N` counts
  milliseconds, so `screenshotPNG a; wait 5; screenshotPNG b` usually puts
  both in one frame.
- A capture still waiting when the renderer shut down was lost:
  `RE_Shutdown`, then `R_Init`'s `Com_Memset( &backEnd, ... )`. A map load
  (`CL_ShutdownAll`), a video restart and `quit` all shut the renderer down.
  `.tmp/alice-lead/feed1` is such a run: `wait 300; screenshotPNG
  alice-probe; wait 5; quit` quit before the capture frame. The Vulkan and
  RTX renderers also cleared waiting captures silently while minimized
  without a framebuffer (`R_IssueRenderCommands`).

Before, `.tmp/engine-input-capture/before-shots` (`devmap garden1`, then
`wait 5000; screenshotPNG s1; screenshotPNG s2; wait 2000; screenshotPNG s3;
wait 5; quit`) wrote `s1.png` only. Its log has `Wrote screenshots/s1.png`
and nothing about `s2` or `s3`.

### Change

`code/renderercommon/tr_screenshot_pending.h` names every such request, and
the GL, Vulkan and RTX renderers use it:

- **A second request** for a waiting type prints `WARNING: screenshotPNG s2
  not taken: screenshots/s1.png is already waiting for the next frame.` A
  request that names the waiting file is covered by that capture and prints
  nothing. The check still runs before the request touches the waiting
  capture, so a clipboard `screenshotBMP` cannot redirect a waiting file.
- **`RE_Shutdown`** names every capture still waiting (PNG, TGA, JPEG, BMP or
  clipboard, a levelshot, a cubemap) with `WARNING: <name> was not written:
  the renderer shut down before a frame was captured.`, then clears them. The
  minimized Vulkan and RTX path gives `the window is minimized and cannot be
  captured`.

A successful capture is unchanged: the same file, the same `Wrote` line and
the same `silent` handling. A request that used to vanish now prints a
warning, and nothing else changes. The Elite Force thumbnail and the AVI frame
bit are not screenshot requests and are not reported.

### Runtime

After, the same sequence, with a repeated `screenshotPNG s1` added
(`after-shots`, GL):

```
^3WARNING: screenshotPNG s2 not taken: screenshots/s1.png is already waiting for the next frame.
Wrote screenshots/s1.png
...
RE_Shutdown( 3 )
^3WARNING: screenshots/s3.png was not written: the renderer shut down before a frame was captured.
```

The repeated `s1` printed nothing and `s1.png` was written. The Vulkan and
RTX renderers (`after-shots-vk`, `after-shots-rtx`) printed the same three
lines.

## 3. `cl_autoRecordDemo` and a refusing `record`

### Cause

With `cl_autoRecordDemo 1`, `CL_Frame` runs `record` on every frame of play
that is not being recorded. RTCW SP and Elite Force Holomatch refuse without
`g_synchronousClients`, and Alice refuses without `g_syncronousClients`
(`alice.exe` `0x0042a476`). So the refusal printed every frame. In
`before-autorecord` (`garden1`, three seconds) it printed 187 times.

### Change

`CL_RecordRefusedForSynchronousClients` (`code/client/cl_main.cpp`) states
`CL_Record_f`'s two refusal conditions. `fntech3::client::demo::AutoRecordAttempt`
(`code/client/demo_auto_record.hpp`) then lets an attempt that will be refused
run once. It prints `CL_Record_f`'s own message, and later frames skip it. The
latch `cl_autoRecordRefused` is cleared by `CL_ClearState`, which runs on
every gamestate and every disconnect, so each level and each connection gets
one attempt. An attempt that will not be refused clears the latch too, so
setting the cvar mid-level starts recording on the next frame.

The manual `record` command, its messages, and every profile whose `record`
only warns are untouched. Those profiles still retry every frame, including
after any other failure, such as a file that cannot be opened.

### Runtime

- `after-autorecord`: one refusal on `garden1`. `set g_syncronousClients 1`
  then started `recording to demos/20260924092105-localhost-garden1.dm3.`
  on the next frame. The `map garden1` that followed stopped that recording
  and started a new one on the new level.
- `after-autorecord-twolevels`: with the cvar left at 0, `garden1` twice gave
  exactly two refusals, one per level.
- `after-manual-record`: `record` typed twice after the automatic refusal
  printed the refusal both times, three in all.

## 4. Keys nobody sent

### Observed

The run behind the report is `.tmp/alice-cin/run3`. Its log shows
`CL_ToggleMenuInternal: catcher 0x0000 state 9` during `opening.roq`, which
stopped the movie, and then two more calls with `catcher 0x0002 state 1`.
The transcript of the session that made it shows the launch as a plain
PowerShell `Start-Process` of `meson/build/win32-alice-cin/fntech3.exe`. That
run was on the interactive desktop, not a private one, so its window could
receive real keyboard input. The last two calls had the UI catcher set, so
only a real `K_ESCAPE` press reaches `CL_ToggleMenuInternal` there, and one
Escape key-down does it; the any-key rule needs catcher 0. `run4`, launched
the same way minutes later, played the movie to its end.

Every path that queues a key-down was read:

- **SDL** (`code/sdl/sdl_input.cpp`, the Windows client's layer:
  `SDL using driver "windows"`). Key-downs come only from SDL key, mouse,
  wheel, joystick and gamepad events. The one exception is
  `IN_QueueHeldModifiers`, which rebuilds only `K_CTRL`, `K_SHIFT`, `K_ALT`,
  `K_MODE` and `K_SUPER` or `K_COMMAND` from `SDL_GetModState()` after an
  input reset, such as focus returning. The engine sets no SDL hint, so
  SDL 3.4's raw keyboard and its low-level keyboard hook, which alone see
  `VK_ESCAPE` outside the window, stay off (`SDL_windowsvideo.c`,
  `SDL_windowswindow.c`). SDL's focus handling reads mouse buttons with
  `GetAsyncKeyState`, which returns 0 when the calling desktop is not the
  input desktop.
- **Win32** (`code/win32`, used only without SDL). This layer's key-downs come
  from window messages, raw input, DirectInput, MIDI and joystick polling.
  `WIN_QueueHeldModifiers` rebuilds only modifiers, from `GetAsyncKeyState`.
- **Client.** `Key_ClearStates`, `CL_ResetMouseInputState` and the Alice
  cursor-mode release (`CL_AliceReleaseMouseButtons`) send releases only.
  Nothing in `cl_alice_ui.cpp` or `alice_input.hpp` sends a press.

A cinematic stops on Escape, or on any key below 128 or `MOUSE1` that no
catcher takes (`CL_KeyDownEvent`, alice.exe `0x00429d56..0x00429d8d`). Every
rebuilt modifier is numbered 128 or above.

### Runtime

- `noinput-opening`: `cinematic opening.roq` from the console, on a private
  desktop, with the default mouse settings and `in_keyboardDebug 1`. The movie
  played to its end, 132 s for the whole run, with no key line and no
  `CL_ToggleMenuInternal`.
- `noinput-newgame`: run3's sequence, on a private desktop: `pushmenu main`,
  `pushmenu newgame`, then the medium button's own command
  `seta skill 1; seta nextmap gvillage; cinematic opening.roq; togglemenu
  newgame;`. The movie ended by itself, `gvillage` loaded and its first frame
  was captured, with no key line and no toggle.

### Conclusion

No engine path invents a key-down. Nothing was changed. A probe must be
launched on a private desktop, as `probe_alice.py` does. `Start-Process`
puts the window on the user's desktop, where it takes the focus and any key
the user presses.

## 5. The local client and an in-game cinematic

### Observed

| Address | Behaviour |
| --- | --- |
| `alice.exe` `0x004273df` | `CIN_Play` calls `SV_Shutdown( "" )` (`0x00455910`), for any movie that exists |
| `0x00455910..0x004559b9` | `SV_Shutdown` returns unless `sv_running`. Otherwise it prints, sends the final message (`0x004558a0`: `print "%s"` and `disconnect`, then a snapshot to each client of state 2 or more), calls `0x00451680` (an empty `ret`), the master shutdown `0x00455cc0` and the game's shutdown `0x00454430`, clears `sv`, frees the clients (`0x00444ed0`), clears `svs` and sets `sv_running` to 0. It never calls `CL_Disconnect` (`0x0042abb0`) |
| `0x0042750d` | `CIN_Play` sets `cls.state` to 9 (`CA_CINEMATIC`). The client's cgame and connection state are left as they were |
| `0x004257fe` | `CIN_Finish` sets `cls.state` to 1 (`CA_DISCONNECTED`), then runs `nextmap` (see `client_alice_cinematic.hpp`) |
| `0x0042ab17..0x0042ab26` | The next map load clears `nextmap` and calls `CL_Disconnect`, which drops what was left of the connection |

`maps/qlair.scr` (pak0) ends the game with `setcvar( "nextmap", ";forcemenu
main;forcemenu credits\n" )` and `stuffcmd( "cinematic ending.roq" )`. So a
retail player sees `ending.roq` full screen, then the main menu with the
credits on top. The client is disconnected, with no server running and no
map named. When `nextmap` names a map, its load follows the movie, and so
does `_intro` (inferred from `CIN_Finish`).

fnTech3's `SV_Shutdown` ends with `CL_Disconnect( qfalse )`, unless
`sv_killserver` is 2. So the Alice movie's server shutdown also shut the
client game down, wiped `clc` and `cl`, reset the pure paks and set
`sv_cheats 1`, all before the movie opened. The final message's loopback
packets then arrived for a connection that no longer existed
(`loopback:sequenced packet without connection`).

### Change

`AliceCinematicHost::ShutdownServer` (`code/client/cl_cin.cpp`) now sets
`sv_killserver` to 2 around `SV_Shutdown( "" )`. That is the request
`CL_PlayDemo_f` already makes, so the local client stays as retail leaves it
until the movie takes the screen. Only the Alice profile's `cinematic` and
its intro reach this binding, and `SV_Shutdown` itself is unchanged. The stale
connection goes with the next `CL_Disconnect`: `CL_MapLoading` runs it for
the next level, and `quit` runs it too.

### Runtime

`devmap garden1`, then the ending's own two steps from a cfg: `set nextmap
";forcemenu main;forcemenu credits;exec after.cfg"` and `cinematic
ending.roq`. The trailing `exec` only takes the screenshots after the movie.

| Run | Build | During the movie | After it |
| --- | --- | --- | --- |
| `before-ending` | before | `ending.roq` full screen; `loopback:sequenced packet without connection` | `USAGE: map <map>`, then the credits over the main menu |
| `after-ending` | after | `ending.roq` full screen; no unconnected-packet line | the same `USAGE` line, and the same credits over the main menu |

What the player sees was already retail's outcome, and it is unchanged. The
client state through the movie is now retail's as well. In
`after-nextmap`, `set nextmap "gvillage"` and an in-game `cinematic
ealogo.roq` ended in `Server: gvillage`, `Alice intro: state -1`,
`CL_FirstSnapshot: maps/gvillage.bsp` and a captured first frame of its
rabbit-hole opening, so the kept connection is dropped cleanly when the next
level loads.

Both ending runs used `developer 1`, and in both the console came down once
the movie ended. `CL_Frame`'s generic menu restore fires on the frame the
movie ends, before the appended `forcemenu` commands run: it sees
`CA_DISCONNECTED`, no UI catcher yet, no server and a UI VM, and Alice's
`SetActiveMenu( UIMENU_MAIN )` opens the console for a developer. The hosted
Alice UI has no `UI_MENUS_ANY_VISIBLE` answer, so `CL_UIMenusAreVisible` sees
only the catcher. Retail opens the console only on its drop path
(`0x0042ac50`), which this flow never takes, so a retail developer would see
the credits without the console (inferred). With `developer 0`,
`SetActiveMenu` finds the credits menu current and does nothing. This is left
as an open item for the menu code.

## Tests

`tests/engine_input_capture_tests.py`:

- **Item 1:** the `ClientCommand` glue passes only the entity, and the retail
  bytes above are checked when the installation is present.
- **Item 2:** `tr_screenshot_pending.h` is compiled with `q_shared.c` and
  checked message by message. The source is checked to show that each
  renderer uses the header where it drops requests, that the renderers share
  the flag values it assumes, and that the `Wrote` lines are unchanged.
- **Item 3:** the real `CL_Record_f`, `CL_RecordRefusedForSynchronousClients`,
  `CL_ClearState` and `CL_Frame`'s automatic block are compiled together and
  driven frame by frame for seven profiles. The predicate matches
  `CL_Record_f`'s refusal for every profile at values 0, 0.5 and 1. RTCW SP,
  Holomatch and Alice refuse once per level and once per connection, and
  record on the frame after the cvar is set. Q3, QL, RTCW MP and ET still
  retry every frame, as does a refusing profile once its cvar is set. Putting
  back the old unconditional attempt fails the test.
- **Item 4:** both held-modifier rebuilds queue only modifier keys, and a
  compiled check confirms every modifier is numbered 128 or above and is not
  `MOUSE1`.
- **Item 5:** the binding order around `SV_Shutdown`, `SV_Shutdown`'s
  `sv_killserver` gate, `CL_MapLoading`'s disconnect, and the retail call
  sites of `CIN_Play`, `SV_Shutdown`, `SV_FinalMessage`, `CIN_Finish` and
  `CL_MapLoading`.
