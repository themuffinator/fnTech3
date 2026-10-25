# Joystick input in the Windows client, 2026-10-24

Until this change the Windows client had no joystick or gamepad input in any
profile. Its joystick code was in the tree, in both Windows input backends,
but no Windows build compiled it. The retail engines read one: Quake III's,
RTCW's and Enemy Territory's released sources call WinMM's joystick
functions, and so do the import tables of Elite Force's, Jedi Academy's,
Jedi Outcast's and Alice's executables (Medal of Honor's and F.A.K.K.2's are
encrypted on disk and were not read). Their controls menus show a joystick
switch and threshold, which under fnTech3 did nothing. Windows builds now
compile it: the native backend, which the release packages use, reads devices
through WinMM, and a build that uses SDL3 reads them through SDL3's gamepad
API. Joystick input stays off until `in_joystick` is 1, which is retail's
default and latch. A follow-up the same day makes the WinMM producer honour
Elite Force's "X/Y axis as button" switches and its joystick turn scale
([below](#elite-forces-axis-switches-and-turn-scale)). The
[JSON companion](2026-10-24-windows-joystick.json) holds the identities,
addresses, commands and results. This is Source, Unit, Build and Live
evidence without a device. No compatibility gate closes.

## Why Windows had none

The joystick code sits under `#ifdef USE_JOYSTICK` in `code/win32/win_input.cpp`
and `code/sdl/sdl_input.cpp`. These are the observed facts:

- Quake3e, the base of FnQuake3, never defines `USE_JOYSTICK` in any build
  file. `code/sdl/sdl_glw.h` carries it commented out (`//#define USE_JOYSTICK`),
  so no Quake3e platform reads a joystick by default.
- FnQuake3 at the pinned `7d8c9629` is the same: the macro appears only in
  `#ifdef` lines and that commented-out define.
- FnQL's input hardening, `dd9618a` "Harden input/audio init across all
  backends" (2026-07-31, `dd9618a1` in this tree's history), hardened the
  WinMM, SDL3 and Linux joystick producers. It then added `-DUSE_JOYSTICK` to
  Meson, CMake and the Makefile only for native Linux builds without SDL3. Its
  message gives no reason for leaving Windows out. FnQL's own
  [input record](../fnql/INPUT_COMPATIBILITY.md) describes the opt-in Quake
  Live WinMM profile, and its [web UI notes](../fnql/WEBUI_BACKEND.md) say
  the WinMM gamepad rows appear on Windows. With the code compiled out, those
  rows never appeared and that profile never ran.

Inference: the omission was inherited, not chosen for a recorded cause. No
build, platform or licence reason keeps it out. WinMM is part of Windows,
and the client already links `winmm` for its timers and MIDI input.

Retail registered the switch as fnTech3 does: `in_joystick` `"0"`, archived
and latched, in Quake III 1.32's `win_input.c`, RTCW single player's, and
Elite Force's `IN_Init` (`stvoyHM.exe` 0x43ec85, `stvoy.exe` 0x42d532).

## WinMM natively, SDL3 under SDL3, no XInput producer

The native backend reads WinMM. It is the interface every retail engine here
used. Its producer was already in the tree and hardened: it caps buttons at
the device's own count, reads only the axes the device reports, recovers from
read failures, and drops its state at input resets. It also carries FnQL's
opt-in Quake Live mapping. Windows presents XInput pads through WinMM as well:
both triggers share the Z axis, and the guide button and rumble are not
available that way. Its buttons arrive as `JOY1` onwards, numbers every
Quake III-family menu knows, so a controls menu can bind them as the original
did.

A separate XInput producer is not added. It would be a second native mapping,
written without a device to check it against. An SDL3 build already reaches
XInput through SDL3: the SDL3 3.4.0 subproject, as configured here, enables
its XInput, Windows.Gaming.Input, GameInput and DirectInput drivers, and
leaves out HIDAPI and raw input.

## What changed

| Build | Before | Now |
| --- | --- | --- |
| Meson, Windows, native or SDL3 | `-DUSE_JOYSTICK` only `if linux and build_client and not use_sdl` | `if build_client and (win32 or (linux and not use_sdl))` |
| CMake (Windows is native only) | `IF(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND NOT USE_SDL)` | `IF(WIN32 OR (CMAKE_SYSTEM_NAME STREQUAL "Linux" AND NOT USE_SDL))` |
| Makefile, MinGW | none | `sdl_input.o` with SDL3, `win_input.o` without it, each `CXXFLAGS += -DUSE_JOYSTICK` |
| Linux and macOS with SDL3 | none | still none; every SDL3 build since 2026-10-25 ([SDL3 record](2026-10-25-sdl3-joystick.md)) |

The build change touched no source. Two follow-ups below, both added
afterwards, do: the native backend's hot-plug, and a change to the WinMM
producer and `CL_JoystickMove` for Elite Force alone. Otherwise the
behaviour is the producers' own:

- With `in_joystick 0`, the native backend makes no WinMM call, at start or
  per frame. An SDL3 build starts SDL3's joystick and gamepad subsystems to
  list devices in `in_availableJoysticks`, as FnQL's SDL3 backend and
  ioquake3 do, and opens none. Since 2026-10-25 it starts neither until
  `in_joystick` is 1 ([SDL3 record](2026-10-25-sdl3-joystick.md)); the
  runs below predate that.
- With `in_joystick 1`, the native backend probes WinMM's sixteen slots
  when input starts, takes the first device found, and reads it each frame
  while the window has focus and is neither minimized nor suspended.
  Quake III's controls menu runs `in_restart` when it saves its settings
  (`q3_ui/ui_controls2.c`), and Elite Force's menus carry the same command
  (Holomatch `vm/ui.qvm`, single player `efuix86.dll`); otherwise
  `in_restart` in the console applies the latched switch. A device
  unplugged is closed with an input reset, which releases what it held. A
  device plugged in later is looked for again once Windows reports the
  change ([hot-plug](#hot-plug-in-the-native-backend)); the SDL3 backend
  reopens on SDL3's hot-plug events.
- The native mapping is Quake III's. Buttons are `JOY1` onwards, up to
  sixteen. The X and Y stick directions are the arrow keys, Z is `JOY16` and
  `JOY17`, R is `JOY18` and `JOY19`, and the hat's four directions are
  `JOY24` to `JOY27`. U and V together move the view as a trackball, scaled
  by `in_joyBallScale`. `joy_threshold` is the dead zone. `in_joystickProfile 1`
  selects FnQL's Quake Live mapping: analog movement on X and Y, view on R
  and U. Elite Force's switches can make X or Y analog instead (below).
- The SDL3 mapping is ioquake3's. A recognised gamepad's buttons are
  `PAD0_A` and so on, and its axes act as keys or, with
  `in_joystickUseAnalog 1`, as `j_*` axes. Quake III-family modules have no
  number for a `PAD0_*` key, so their menus never see one. A console `bind`
  still works. Since 2026-10-25 only Quake Live keeps those keys; in every
  other game a recognised gamepad queues `JOY` keys and the arrow keys
  ([SDL3 record](2026-10-25-sdl3-joystick.md)).
- Every profile shares the producers, so the joystick switch and threshold
  that Quake III's, RTCW's and Elite Force's controls menus show now take
  effect on Windows. `ui_joyavail` now exists in native Windows builds; the
  WinMM producer sets it to 1 when it opens a device.

Invariant 11 applies to input presentation. This change adds no mapping of
its own: the WinMM path keeps FnQ3's, the SDL3 path FnQL's, and both are the
FnQ3/FnQL systems the profiles adopt.

## Hot-plug in the native backend

Added later the same day. Until then the native backend probed WinMM's
slots only when input started, so a controller plugged in after that needed
`in_restart`, while SDL3 builds reopened on SDL3's own hot-plug events. The
README promises controllers that can be plugged in at any time, and the
release packages are native builds (`-Dsdl=disabled` in
`.github/workflows/release.yml`).

- Windows sends `WM_DEVICECHANGE` with `DBT_DEVNODES_CHANGED` to every
  top-level window when a device is added or removed; no registration is
  needed. `MainWndProc` (`code/win32/win_wndproc.cpp`) passes it to
  `IN_DeviceNodesChanged` and then on to `DefWindowProcW`, as before. The
  call is outside `USE_JOYSTICK`, because the Makefile defines that for
  `win_input.o` alone; the function's body is inside it.
- `IN_DeviceNodesChanged` only notes the time and that two probes are owed.
  It makes no WinMM call. While `in_joystick` is 0, or not yet registered,
  it ignores the notice.
- `IN_Frame` calls `IN_ProbeChangedJoystick` every frame, ahead of the focus
  check, so a pad plugged in while the window is in the background is open
  when the player returns. That step reruns `IN_StartupJoystick`, the same
  sixteen-slot probe `in_restart` runs, 300 ms after the last notice of a
  burst and again 2 s after it, and then stops. A new notice restarts both.
  It probes nothing while `in_joystick` is 0 or a device is open. A frame
  that arrives after the 2 s mark, after a map load say, makes its probe the
  last. Opening a device, or a start with `in_joystick` 0, drops any probes
  still owed.
- An unplugged pad, which `IN_JoyMove` already closes at once, is followed
  by the two probes its removal notice owes, which open a second pad still
  connected, if there is one. Notices that arrive while a device is open
  stay owed until it closes. `IN_JoyMove` reads only a focused window, so a
  pad swapped while the game is in the background is closed, and the new
  one looked for, when the player returns; if the delays have passed by
  then, the probe runs at once. A device closed after three failing reads
  is looked for again only when a notice has come since the last probe, at
  most twice per notice, so a faulty device cannot keep the slots probed.
- The delays are those of SDL2's Windows joystick detection
  (`src/joystick/windows/SDL_windowsjoystick.c`), which sets timers at
  300 ms and 2 s after a HID arrival or removal "to ensure all APIs have
  updated status". That is reference observation; no SDL text is used. SDL
  registers for HID interface notices on a hidden window instead.
  `DBT_DEVNODES_CHANGED` needs no registration but also follows unrelated
  devices, such as a USB drive, so such a change costs at most two probes,
  and only while a joystick is wanted and none is open.
- Probing every frame would cost this every frame. Measured on this machine
  (Windows 11 26200, no controller, a 32-bit program calling WinMM
  directly): the first sixteen-slot probe in a process took 119 to 227 ms,
  while `winmm.dll` loads DirectInput (its strings name `DINPUT.DLL`,
  `DirectInputCreateW` and a `DINPUTWINMM` key); later ones took about
  0.2 ms idle, and 1.3 ms on average, up to 7 ms, while a build ran. The
  first probe is the one input start already makes. Slower empty slots on
  other Windows versions were reported to this task but not checked here.

Open: no controller was plugged in, so it is not observed that WinMM lists
one connected after its first call. The new probes call exactly what
`in_restart` calls, in the same process, so they find a late device
whenever `in_restart` would; the guide still names `in_restart` if one is
not found. If WinMM turns out to keep the list it made at its first call,
`joyConfigChanged(0)` before the probe is the call to try. Microsoft
documents that it makes Windows send a message to every top-level window,
so it is not used without that evidence.

The evidence:

- **Source.** `tests/input_system_source_tests.py` checks the window
  procedure's case, its `<dbt.h>` include and that no `#if` surrounds it;
  the declaration in `win_local.h`; that the notice calls no WinMM and checks
  `in_joystick` first; the probe step's delays, its gates and its call of
  `IN_StartupJoystick`; that a start with `in_joystick` 0 and an opened
  device drop owed probes; and that `IN_Frame` calls the step before the
  focus check and queries WinMM nowhere itself.
- **Unit.** `tests/winmm_joystick_tests.py` now also cuts out
  `IN_DeviceNodesChanged` and `IN_ProbeChangedJoystick` and drives them with
  a fake clock. With `in_joystick` 0 a notice leads to no WinMM call, and
  switching it on owes nothing for it. With no device, a burst of three
  notices gives one sixteen-slot probe 300 ms after the last and one at 2 s,
  and none in the 27 s after. A pad WinMM lists only by the second probe
  opens then and reads at once. One found by the first probe leaves nothing
  owed; closed later by failing reads, with no notice since, it is not
  probed for. Notices probe nothing while a device is open but stay owed:
  closed by failing reads 10 s later, it is probed for once, at once, and
  not again. Unplugging it is followed by a probe that opens a second pad. A late frame probes once, a start with `in_joystick` 0
  drops owed probes, the frame step checks `in_joystick` itself, the delays
  hold across the millisecond clock's wrap, and nothing happens before
  `in_joystick` is registered. Twelve mutations were each caught: either
  `in_joystick` check dropped, the open-device check dropped, notices
  dropped while a device is open, a first delay of 0, a last delay of
  300 ms, no late-frame rule, a single probe, owed probes kept on opening or
  on a start with 0, a burst that does not restart the clock, and a 64-bit
  time difference that breaks at the wrap.
- **Build.** The native client (`-Dsdl=disabled`, MSVC x86 debugoptimized,
  `/W4 /WX`) built with both changed files compiled with `USE_JOYSTICK`.
  MSYS2's i686 GCC 15.2 compiled them with the Makefile's MinGW flags plus
  `-Wextra`, `win_input.cpp` with and without `USE_JOYSTICK`, with no
  warning from the new code; without it `IN_DeviceNodesChanged` is still
  defined, unmangled, for the window procedure to call.
- **Live, no device.** `.tmp/joystick-hotplug/probe_device_change.py` starts
  `stvef-mp` as the start-up runs below do, windowed on a private desktop that
  never had focus, with an isolated home and empty configs, once with
  `in_joystick` 1 and once with 0. After `PROBE ready` it watched the log for
  3 s, then sent three `WM_DEVICECHANGE` `DBT_DEVNODES_CHANGED` notices, 100
  and 150 ms apart, with `SendMessageTimeoutW`, from a thread attached to that
  desktop, to the client's own top-level windows only: the game window, its
  console, and the helper windows the graphics driver and Windows' text input
  make in the process. That is the message Windows broadcasts; it carries no
  keyboard, mouse or controller input, and none was sent. With 1: no probe in
  the quiet 3 s; `Devices changed; looking for a joystick.` and `joystick not
  found -- no valid joysticks (a5)` at 328 ms and again at 2,016 ms after the
  last notice; none in the 4 s after. With 0: `Joystick is not active.` at
  start and nothing after the notices. The harness then stopped both clients.
  No errors; the executable and installation were unchanged.

## Elite Force's axis switches and turn scale

Elite Force's controls menus also show "X axis as button" and "Y axis as
button" (`joy_xbutton`, default 1, and `joy_ybutton`, default 0). fnTech3
registered both ([registrations record](2026-10-23-stvef-retail-registrations.md))
but its producer read neither, so the stick's four directions were always the
arrow keys. Read statically in the two 1.20 executables:

- `stvoyHM.exe` `IN_Init` stores the cvars at 0x6b4914 (`in_joystick`),
  0x6b491c (`in_joyBallScale`), 0x6b4938 (`in_debugjoystick`), 0x6b4930
  (`joy_threshold`), 0x6b4910 (`joy_xbutton`, registered at 0x43ece0) and
  0x6b4918 (`joy_ybutton`, 0x43ecf6).
- Its `IN_JoyMove` (0x43f3a0) loops over X, Y, Z and R, up to the device's
  axis count. Each axis is `v = (position - 32768) / 32768`, held to -1..1.
  For X it reads `joy_xbutton` (0x43f66f), for Y `joy_ybutton` (0x43f6d1).
  When the switch is 0 it queues `SE_JOYSTICK_AXIS` on axis 0 (X) or 1 (Y)
  every frame, with `-v * 127` passed through `ftol` (0x43f6a0, 0x43f715),
  which truncates. From `-joy_threshold` to `joy_threshold`, both ends
  included, it queues 0. When the switch is 1 it sets Quake III's direction
  bits for `v < -joy_threshold` and `v > joy_threshold`. Z and R always use
  direction bits.
- `stvoy.exe` does the same: `JoyToF` at 0x42d880, `joy_xbutton` registered
  at 0x42d5a3 and read at 0x42da67, `joy_ybutton` at 0x42d5bc and 0x42dad5,
  the axis events at 0x42dac3 and 0x42db40, `ftol` at 0x46db60.
- Holomatch's `CL_JoystickMove` (0x409f10) multiplies `cl_yawspeed` and
  `cl_pitchspeed` by 0.01 (the float at 0x4b7430) before the joystick axis
  and the frame's angle speed. Unless `+strafe` is held, axis 0 turns the
  view; with it, axis 0 is added to the side move. With `+mlook` axis 1
  pitches the view, otherwise it is added to the forward move. Axis 2 is
  always added to the up move. Single player's (`stvoy.exe` 0x405640) is the
  same, with its own 0.01 at 0x47e4a8. A full stick therefore turns at 1.27
  times the key speed in both modes. fnTech3's `CL_JoystickMove` had Quake
  III 1.32's form, without the factor.

What changed, for `stvef-sp` and `stvef-mp` only:

- With the Quake Live mapping off (`in_joystickProfile 0`, the default), the
  WinMM producer now reads the two switches. With one off,
  `IN_JoyEliteForceAxes` takes that axis's direction bits back out after
  Quake III's loop and queues the axis as `SE_JOYSTICK_AXIS` on axis 0
  (`AXIS_SIDE`) or 1 (`AXIS_FORWARD`), as retail does.
  `fnql::input::EliteForceJoystickAxis` computes the value. `v * 127` is
  exact in float for every WinMM position, so the truncated result is
  retail's integer.
- `IN_Init` registers the switches beside `joy_threshold` with `CL_Init`'s
  defaults and flags, which are retail's. `CL_Init` keeps its registration,
  so builds without joystick input still keep the menus' settings.
- `CL_JoystickMove` scales both turning speeds by 0.01 for Elite Force.
- A held axis is queued every frame, as retail's is; a resting one is queued
  once. The client clears its axes at each new gamestate, a level change or
  a loaded save, without an input reset, and the next frame gives a held
  stick back. A first version queued only changes, which left a player who
  held the stick fully forward through a level change standing still; the
  review below found it.
- One departure, under invariant 12, which changes no value a module, peer
  or file receives: retail stops queueing an axis when its switch is turned
  back on, leaving a held stick's last value turning or moving the player
  until the level changes. fnTech3 queues a 0 once instead. An input reset
  clears the client's axes and the producer's snapshot together, so a stick
  still held is sent again.
- Inference from the retail code, not checked with a device: with `+strafe`,
  the stick pushed left moves the player right, because the inverted X value
  is added to the side move; with `+mlook`, pushing it forward looks down.
  fnTech3 keeps both, as retail's.
- Every other profile is unchanged: the producer never calls the Elite Force
  helper there, and the turn factor applies to Elite Force alone. Within
  Elite Force, `in_joystickProfile 1` keeps the Quake Live lanes in the
  producer and the SDL3 backend does not read the switches. The factor
  sits in `CL_JoystickMove`, as retail's does, so it scales every Elite
  Force axis whichever producer queued it: the Quake Live lane's full stick
  now turns at 177.8 degrees a second at `cl_yawspeed 140`, where it turned
  at 17,780, and an SDL3 build's analog axes at a hundredth of their old
  rate.

The Elite Force lane is the profile's own switch adapted into FnQ3's WinMM
producer (invariant 11). No retail-exact option is kept beside it, since the
lane's values are retail's.

## Evidence

- **Source.** `tests/input_system_source_tests.py` checks the three build
  files' conditions and that SDL3 builds elsewhere still leave the define
  out (the SDL3 record's change replaced that check). It checks that both backends register `in_joystick` as `"0"`,
  archived and latched. It checks that WinMM is untouched before that cvar
  is read, and that the frame update polls only a focused, unminimized,
  unsuspended window.
- **Unit.** `tests/winmm_joystick_tests.py` cuts `IN_StartupJoystick`,
  `IN_JoyMove` and their helpers out of `win_input.cpp` and compiles them
  with fake `joyGetNumDevs`, `joyGetPosEx` and `joyGetDevCaps`. It checks that
  `in_joystick 0` makes no WinMM call, and that no driver or sixteen empty
  slots give a quiet start. A pad in the third slot opens. Its buttons,
  stick directions, trigger axis and hat give the keys above, once per change,
  and buttons past the device's count are ignored. A broken threshold
  disables the directions. A reset re-presses a held button. Two failed
  reads are forgiven and the third closes the device with one input reset.
  Unplugging closes it at once. The Quake Live profile gives analog movement
  and view turns. Five mutations of the production code were each caught:
  the `in_joystick` gate removed, a read-failure budget of one, a moved hat
  bit, the button cap dropped, and the reset on loss dropped.
- **Unit, Elite Force.** The same test also cuts `IN_JoyEliteForceAxes` out
  of `win_input.cpp`, and `CL_SafeMoveValue`, `CL_AddViewAngleDelta`,
  `CL_JoystickEvent` and `CL_JoystickMove` out of `cl_input.cpp` (with
  `ClampCharMove` from `q_math.c`), so the simulated device's events reach
  the client's own movement code. With retail's defaults, X stays the arrow
  keys and Y full forward queues axis 1 at 127, once. Half back queues -63,
  truncated where rounding would give -64, and at exactly the threshold the
  axis is 0. With `joy_xbutton 0`, X releases the right arrow and queues
  axis 0 at -126 for full right and 127 for full left. Z and R stay keys.
  A held axis is queued again every frame. A switch turned back on while
  the stick is held queues 0 and presses the key, and turned off again
  releases it. An input reset re-sends a held axis, and so does the next
  frame after the client's axes are cleared as a new gamestate clears them.
  A broken threshold rests it, and a one-axis device never reports Y. The
  Quake Live profile keeps its own lanes, and the Quake III profile its arrow
  keys, with both switches off. In the client, a full stick at
  `cl_yawspeed 140` turns 17.78 degrees in a 100 ms frame under Elite Force,
  and 1,778 under Quake III's form. `+mlook` pitches by the same amount, and
  `+strafe` makes the stick's left a side move of 127. Ten mutations of the
  production code were each caught: the Elite Force gate dropped, the Quake
  Live profile gate dropped, rounding instead of truncation, an exclusive
  threshold, the sign not inverted, no release when the switch is turned on,
  a held axis queued only on change, the direction bits kept, the turn
  factor dropped, and the factor applied to every profile. `tests/input_compat_tests.cpp` checks the helper's bounds,
  non-finite values and threshold edges.
- **Source, Elite Force.** `tests/input_system_source_tests.py` checks that
  `IN_Init` and `CL_Init` register the switches with the same defaults and
  flags, that nothing but their declarations, `IN_Init` and the helper names
  them, that the helper queues a held axis every frame, that `IN_JoyMove`
  calls it once,
  for Elite Force in the legacy profile, between Quake III's direction bits
  and the hat's, and that `CL_JoystickMove` applies the factor to both
  turning speeds under `Com_GameIsSTVEF()` alone.
- **Build and Live, Elite Force.** The native client (`-Dsdl=disabled`) and
  dedicated server were built again with `/W4 /WX` as an MSVC x86
  debugoptimized build in the follow-up's worktree, with no warnings, and
  rebuilt after the review's fix. That client started with `in_joystick 1`
  and no device, as below, once under each of `stvef-mp`, `stvef-sp` and
  `quake3`, and listed `joy_*`. Both Elite Force modes showed
  `joy_threshold "0.15"`, `joy_xbutton "1"` and `joy_ybutton "0"`, each
  archived. Quake III showed `joy_threshold` alone.
  Each run reported no valid joystick, quit through its own config within
  19 seconds without an error, and left its installation unchanged. With
  no device the producer never polls, so these runs show the registrations,
  not the switches at work.
- **Review.** An independent read of the change found the level-change gap
  above, now fixed and tested, and that this record had said the factor
  left Elite Force's Quake Live lane unchanged; the factor is kept, as
  retail's, and the record corrected. It found no null dereference, no other
  profile's behaviour changed, no state left held across switch changes,
  resets or device loss, and no `/W4` hazard.
- **Build.** Both Windows clients were built with strict warnings (`/W4 /WX`)
  as MSVC x86 debugoptimized builds in this worktree: native (`-Dsdl=disabled`,
  as the release workflow configures) and SDL3 (`-Dsdl=enabled`, the
  subproject's SDL3 3.4.0).
- **Live, no device.** Each client ran `stvef-mp` against the 1.20
  installation, windowed at 640 by 480 on a private desktop that never had
  focus. Sound, network, Steam and the web UI were off, the home folder was
  isolated, and an empty `hmconfig.cfg` shadowed the installation's. No
  input was sent. One run started with `in_joystick 0`, listed the joystick
  cvars, then ran `set in_joystick 1` and `in_restart` and listed them again.
  The other started with `in_joystick 1`. Both quit through their own
  configs without errors, and the installation was unchanged.
  - Native: `Joystick is not active.` at start; after the restart, and at a
    start with 1, `joystick not found -- no valid joysticks (a5)`. Windows 11
    answered `JOYERR_PARMS` for an empty slot. `ui_joyavail` stayed 0.
  - SDL3: in every run, `SDL_InitSubSystem(SDL_INIT_JOYSTICK)` and
    `(SDL_INIT_GAMEPAD)` passed and `0 possible joysticks` followed, the
    `in_joystick 0` start included. Then `Joystick is not active.` at a start
    with 0, and `No joysticks found.` after the restart and at a start with 1.
    `in_availableJoysticks` stayed empty.

## Not run

The user chose not to run hands-on device checks, and this machine has no
controller or force-feedback device. Nothing here reads a real device:
mapping, dead zones, hot-plug and loss are covered only by the simulated
WinMM device, the producers' existing tests and the device-change notice
run; no controller plugged in after start was seen to open. The engine's
synthetic input (`in_synthesize`) drives keys and the mouse, not a
joystick, so it was not used. Linux and macOS builds with SDL3 had no
joystick input until the [SDL3 record](2026-10-25-sdl3-joystick.md)'s
change. Elite Force's switches and turn scale were not run against retail
or in a live fnTech3 session; their evidence is the static reads and the
unit checks above. The SDL3 backend does not read the switches.
