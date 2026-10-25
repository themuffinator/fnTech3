# Joystick input in SDL3 builds, 2026-10-25

Until this change only Windows builds and native Linux builds read a
joystick. SDL3 builds on Linux, macOS and other systems compiled the SDL3
producer out, although it uses nothing but SDL3's own joystick and gamepad
calls. Every SDL3 build now compiles it. Two changes to the producer come
with it. It no longer starts SDL3's joystick and gamepad subsystems while
joystick input is off. And a gamepad that SDL3 recognises now gives every
game but Quake Live the `JOY` keys and arrow keys its own controls menu can
bind, where it gave keys named `PAD0_A` and so on that only Quake Live's
modules can receive. The [Windows record](2026-10-24-windows-joystick.md)
describes the producers and the WinMM path; the
[JSON companion](2026-10-25-sdl3-joystick.json) holds the identities,
commands and results. This is Source, Unit, Build and Live evidence without
a device. macOS was not built. No compatibility gate closes.

## Every SDL3 build compiles it

`code/sdl/sdl_input.cpp`'s joystick code sits under `#ifdef USE_JOYSTICK`.
It calls only SDL3: no platform header, no `#ifdef` for an operating system.
On 2026-10-24 the build files defined `USE_JOYSTICK` for every Windows
client and for native Linux. SDL3 builds elsewhere left it out, and
`code/sdl/sdl_glw.h` still carried Quake3e's commented-out define.

| Build | Before | Now |
| --- | --- | --- |
| Meson | `if build_client and (win32 or (linux and not use_sdl))` | `if build_client and (win32 or linux or use_sdl)` |
| CMake | `IF(WIN32 OR (CMAKE_SYSTEM_NAME STREQUAL "Linux" AND NOT USE_SDL))` | `IF(WIN32 OR USE_SDL OR CMAKE_SYSTEM_NAME STREQUAL "Linux")` |
| Makefile, Unix with SDL3 | none | `$(B)/client/sdl_input.o: CXXFLAGS += -DUSE_JOYSTICK` |
| `code/sdl/sdl_glw.h` | `//#define USE_JOYSTICK` | a comment: the build files define it |

Only native Unix builds other than Linux leave it out: their X11 backend has
no joystick producer (`code/unix/linux_glimp.cpp` stubs it).

Who this reaches. SDL3 is the Linux default of Meson (`-Dsdl=auto`) and Make
(`USE_SDL=1`). The Linux builds the workflows run tests on use the native X11
backend (`-Dsdl=disabled`), which already read `/dev/input/js*`; the
workflow only compiles a bundled-SDL3 client. macOS builds are SDL3 only. [BUILD.md](../../BUILD.md)
calls macOS disabled and unsupported, but the build files keep its SDL3
path; leaving joystick input out there would have kept a special case for a
platform nothing tests. The change makes no claim about macOS: no macOS
build or run was possible here.

## SDL3 waits for `in_joystick`

On 2026-10-24 the Windows SDL3 probe saw
`SDL_InitSubSystem(SDL_INIT_JOYSTICK)` and `(SDL_INIT_GAMEPAD)` at every
start, `in_joystick 0` included: FnQL's producer started both subsystems to
fill `in_availableJoysticks`, and kept them while input was off so hot-plug
events could refresh that list.

Observed:

- Retail Quake III 1.32 (`code/win32/win_input.c:806` of the GPL release)
  and its Linux joystick code (`code/unix/linux_joystick.c:77`) return
  before any device call when `in_joystick` is 0. fnTech3's WinMM producer
  (`IN_StartupJoystick`) and Linux producer (`code/unix/linux_joystick.cpp`)
  do the same.
- SDL3 3.4.0, the subproject, starts device monitoring with the joystick
  subsystem: a `SDL_joystick` detection thread on Windows
  (`src/joystick/windows/SDL_windowsjoystick.c:287`), a udev callback
  (`src/joystick/linux/SDL_sysjoystick.c:1052`) or an inotify watch on
  `/dev/input` (line 1085) on Linux.
- Nothing in the tree reads `in_availableJoysticks`: no menu, no module, no
  web page. No retail module knows the name.
- Other SDL ports differ. iortcw (`SP/code/sdl/sdl_input.c:512`) and Lilium
  Voyager (`code/sdl/sdl_input.c:628`) start both subsystems to list devices
  and release the gamepad one when input is off; OpenJK
  (`shared/sdl/sdl_input.cpp:577`) releases the joystick one too; Spearmint
  keeps the joystick one. iortcw re-enumerates on hot-plug only while input
  is on.

Decision: with `in_joystick 0` the SDL3 producer now starts neither
subsystem and lists nothing, as the other producers and the retail engines
do. `in_availableJoysticks` is still registered, read-only, and emptied at
every input start, so a list from an earlier start cannot linger after
joystick input is switched off. With `in_joystick 1` the producer takes one
reference to each subsystem, lists and opens devices, and re-enumerates on
SDL3's hot-plug events as before. Those events arrive only while the
subsystems run, and the event handler now acts on them only while input is
on. `in_joystick` stays archived and latched, so switching it takes an
`in_restart`, which every controls menu that shows the switch already runs
(see the Windows record).

## A recognised gamepad's keys follow the game

SDL3 recognises most gamepads through its mapping database and presents
them with one layout. FnQL's producer queued their controls as `PAD0_*`
keys. In this engine those are keys 248 to 278, past the compatibility list
that ends at `K_AUX16` (236); from 256 up they are also past `K_LAST_KEY`,
which old modules loop to. Observed in the key adapters:

- Quake III, Jedi Outcast multiplayer and Q3Test 1.06 to 1.08
  (`code/compat/q3_key_adapter.h`), Elite Force, RTCW, Enemy Territory and
  Jedi Academy multiplayer have no number for any `PAD0_*` key, so their
  modules are never handed one (`CL_ModuleKeyEvent` in `cl_keys.cpp`, and
  `vm.c` for RTCW and ET). Their controls menus could not bind a gamepad
  button; only a console `bind` worked.
- Quake Live's modules take this engine's numbers unchanged, and its web
  controls page lists every engine key by its name
  (`CL_WebHost_BuildConfigBindJson` in `cl_webui.cpp` walks all `MAX_KEYS`).
- The retail engines whose code or imports were read took joysticks
  through WinMM (Windows record; Medal of Honor's and F.A.K.K.2's
  executables are encrypted), so their modules met joystick controls as
  `JOY1`-`JOY32` and the arrow keys.

Decision: Quake Live keeps FnQL's named keys. Every other game gets keys
from Quake III's own joystick table, `joyDirectionKeys` in
`code/win32/win_input.cpp`, chosen by role. `CL_ModulesTakeGamepadKeys`
(`cl_keys.cpp`) answers which, and the producer picks its table when it
opens a gamepad; the profile cannot change while it runs.

| Gamepad control | Quake Live | Every other game | Why that key |
| --- | --- | --- | --- |
| A, B, X, Y | `PAD0_A`, `PAD0_B`, `PAD0_X`, `PAD0_Y` | `JOY1`-`JOY4` | Buttons 0-3 of SDL3's own XInput joystick layout (the `xinput` mapping in `src/joystick/SDL_gamepad_db.h`) |
| Left, right shoulder | `PAD0_LEFTSHOULDER`, `PAD0_RIGHTSHOULDER` | `JOY5`, `JOY6` | Buttons 4 and 5 there |
| Back, Start | `PAD0_BACK`, `PAD0_START` | `JOY7`, `JOY8` | Buttons 6 and 7 |
| Left, right stick click | `PAD0_LEFTSTICK_CLICK`, `PAD0_RIGHTSTICK_CLICK` | `JOY9`, `JOY10` | Buttons 8 and 9 |
| Guide | `PAD0_GUIDE` | `JOY11` | Button 10 |
| Misc (share, microphone, capture) | `PAD0_MISC1` | `JOY12` | Free in Quake III's table |
| Touchpad | `PAD0_TOUCHPAD` | `JOY13` | Free |
| Left stick | `PAD0_LEFTSTICK_*` | arrow keys | Quake III's X and Y axes |
| Left, right trigger | `PAD0_LEFTTRIGGER`, `PAD0_RIGHTTRIGGER` | `JOY16`, `JOY17` | Quake III's Z axis, the one axis Windows' legacy interface gives both of an Xbox pad's triggers |
| Right stick left, right | `PAD0_RIGHTSTICK_LEFT`, `_RIGHT` | `JOY18`, `JOY19` | Quake III's R axis |
| Right stick up, down | `PAD0_RIGHTSTICK_UP`, `_DOWN` | `JOY20`, `JOY21` | Quake III's U axis |
| D-pad up, down, right, left | `PAD0_DPAD_*` | `JOY24`, `JOY25`, `JOY26`, `JOY27` | Quake III's POV hat |
| Paddles 1-4 | `PAD0_PADDLE1`-`4` | `JOY28`-`JOY31` | Free |

`JOY1` to `JOY4` are Enter in Quake III's and Elite Force's menus, and the
arrow keys move through them, so the left stick and A work their menus as
the original's joystick code did. With `in_joystickUseAnalog 1` an axis
whose key is bound to a movement or turning command still drives the
matching analog axis, whichever key set the game uses. A device SDL3 does
not recognise keeps ioquake3's raw mapping in every game: buttons `JOY1` to
`JOY16`, further axes `JOY17` to `JOY28`, the first hat `JOY29` to `JOY32`.

Inferred, not checked on a device: that the Windows download's WinMM path
gives an Xbox controller the same keys. Microsoft documents that the legacy
DirectInput view puts both triggers on one axis, one each way
([XInput and DirectInput](https://learn.microsoft.com/en-us/windows/win32/xinput/xinput-and-directinput)),
but not which trigger takes which side; retail Quake Live's WinMM profile
reads R as horizontal and U as vertical look (`win_input.cpp`), which is
where the right stick's keys come from here; and the WinMM button order of
an Xbox controller was not read.

Invariant 11 applies to input presentation. The SDL3 path keeps FnQL's
gamepad producer: SDL3's recognition and layout, the dead zone, the analog
option, hot-plug and its input barriers. Only the names of the keys follow
the game's own module contract, the profile's data the producer adapts to;
Quake Live keeps FnQL's.

## Evidence

- **Source.** `tests/input_system_source_tests.py` checks the three build
  files' conditions, which UI sources take `sdl_input.cpp`, the header
  comment, the order in `IN_InitJoystick` (list emptied, `in_joystick`
  tested, then subsystems, listing and opening) and that hot-plug
  re-enumerates once and only while input is on.
- **Unit.** `tests/sdl_joystick_tests.py` cuts the joystick statics, key
  tables, `IN_InitJoystick`, `IN_ShutdownJoystick`, `IN_GamepadMove`,
  `IN_JoyMove` and their helpers out of `sdl_input.cpp` and compiles them
  with a stand-in for SDL3's calls. With `in_joystick 0`: no subsystem
  call, no listing, a stale list emptied, nothing to release. With 1: one
  reference to each subsystem however often devices are listed again, the
  list filled, the chosen device opened as a gamepad, both released at
  shutdown. Outside Quake Live each of the 21 buttons queues its expected
  key once per change, SDL3's buttons past the touchpad queue nothing, and
  the sticks, triggers and threshold give the keys in the table, a crossing
  released before it presses. A reset re-presses a held button. An arrow
  key bound to `+forward` drives the forward axis instead. Quake Live gets
  the same `PAD0_*` keys as before. An unrecognised device keeps the raw
  mapping. Every key the joystick table queues is distinct and has a number
  in the Quake III, Elite Force, RTCW, ET and Jedi Academy adapters, and no
  `PAD0_*` key has one in any of them. Six mutations of the production code
  were each caught: the `in_joystick` gate removed, the stale list kept,
  the d-pad's left and right swapped, the profile choice inverted, the
  named axes used in every game, and the triggers swapped.
- **Build.** Linux i686 under WSL (Ubuntu 24.04): CI's bundled-SDL3 client
  build (`--cross-file misc/meson/linux-x86.ini`,
  `--force-fallback-for=sdl3,fontstash`, `-Dsdl=enabled`, Meson 1.10.2,
  GCC 13.3) built `fntech3`, an ELF 32-bit Intel 80386 executable, with
  `sdl_input.cpp` compiled under `-DUSE_JOYSTICK` at the project's
  `warning_level=3` and no warning from it. One-file checks of
  `sdl_input.cpp` and `cl_keys.cpp` with GCC 13.3 and Clang 18.1.3
  (`-m32 -Wall -Wextra -Werror -DUSE_JOYSTICK`) against the base and this
  change found one warning the change had exposed, the raw joystick loop's
  signed and unsigned comparison, inherited from ioquake3 and never before
  compiled on Linux; it is fixed. The other, an unused parameter at
  `cl_keys.cpp:109`, is the base's own. Windows: an MSVC x86
  debugoptimized SDL3 client (`-Dsdl=enabled`, SDL3 3.4.0 subproject,
  `-Dstrict-warnings=true`, so `/W4 /WX`) built clean.
- **Live, no device.** The Windows SDL3 client ran `stvef-mp` against the
  Elite Force 1.20 installation with the Windows record's harness, windowed
  at 640 by 480 on a private desktop that never had focus, with sound,
  network, Steam and the web UI off, an isolated home folder and empty
  configs. No input was sent. A start with `in_joystick 0` printed
  `Joystick is not active.` and no `SDL_InitSubSystem` line, where the
  2026-10-24 build had started both subsystems; after `set in_joystick 1`
  and `in_restart` both started, `0 possible joysticks` and
  `No joysticks found.` followed. A start with 1 did the same. A start
  with 1 switched to 0 by `in_restart` printed `Joystick is not active.`
  and started nothing more. `in_availableJoysticks` stayed empty. All three
  runs quit through their own configs without errors; the installation and
  the executable were unchanged.

## Not run

No controller or force-feedback device is attached here, and the user chose
not to run hands-on device checks: nothing here read a real gamepad, so the
table is checked against SDL3's layout and the adapters, not against a
controller. macOS was not built or run. The CMake and Make SDL3 builds were
not built: WSL has no SDL3 package for them, and only Meson's subproject
build was. Linux was not run, only built.
