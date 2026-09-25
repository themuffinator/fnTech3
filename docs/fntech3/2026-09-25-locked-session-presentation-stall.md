# A locked session, not the private desktop, stalls GL presentation

Timed probes saw fnTech3's GL renderer stop for about 450 ms roughly once a
second, inside `re.BeginFrame`. It was seen in the Q3 and QL audio runs of the
[renderer-stall localization](2026-09-08-renderer-stall-localization.md) and
again on 2026-09-25 in F.A.K.K.2 menu-fade captures (`2026-09-24-fakk2-contracts.md`,
not yet committed). Both put it down to the probes' private desktop. It is not
the private desktop, and no fnTech3 code path is implicated: every stalling run
was made while the Windows session was locked. Unlocked, the same executable,
renderer, steps and private desktop run at a steady 60 frames a second. No
engine change is made; visible-window behaviour is untouched.

## Observed

The lock timeline is Winlogon's own log (`Microsoft-Windows-Winlogon/Operational`,
event 811, subscriber `<Sens>`): 4 is a lock and 5 an unlock. The codes read
off the log itself: each morning logon (2, then 12) is followed within a second
by 4, as automatic restart sign-on locks the session, and minutes later by 5.
The session was locked from 2026-09-08 07:29:52 to 18:08:18 and from
2026-09-25 00:45:58 to 07:56:48.

| Runs | Session | Result |
| --- | --- | --- |
| 19 fnTech3 GL runs presenting normally: Q3 and QL on 2026-09-08 16:30-16:49, F.A.K.K.2 on 2026-09-25 02:46-03:37 | Locked | All 19 stalled: 185 stalls of 399-477 ms (median 444) in 7,569 frames, usually 39 frames apart (127 of 166 gaps were 38-41) |
| 2 runs with `r_drawBuffer GL_FRONT`, Q3 and QL, 2026-09-08 16:54 | Locked | No stall in 958 frames |
| 1 F.A.K.K.2 run with `r_swapInterval 1`, 2026-09-25 02:52 | Locked | 33 of 70 frames took 104-606 ms |
| 20 runs, 2026-09-25 08:03-08:29: fnTech3 F.A.K.K.2 on GL and Vulkan on both GPUs, Q3 on GL and Vulkan, retail `fakk2.exe` 1.02 on both GPUs, and a bare WGL swap loop on both GPUs | Unlocked | No periodic stall in 12,523 frames; two frames over 100 ms, both in the first second (152 ms, and a 176 ms first `glGetError`) |

The strongest pair is one run repeated. `opt0` stalled at 03:37 while the
session was locked: one 470 ms frame in its 53 timed frames, and its twin
`opt1` had two. The same `fntech3.exe` (SHA-256 `18a9ce89...`),
`fnql_glx_x86.dll` (`5c1e59b1...`), steps and cvars ran clean at 08:06,
unlocked. Its six captures, 150 ms apart through the options menu's opening,
kept changing: the mean change between neighbours below the console was 32,
80, 27, 65 and 26 levels. Locked, the same six changed by 38, 24, 17, 2.4 and
2.1. Locked `fn17` asked for six and wrote two, and those two were identical.
Retail's steady ~10 ms frames (2026-09-25 02:56, locked) came from 83 frames,
about one second, which is shorter than one stall cycle, and from the Intel GPU.

Which GPU runs GL decides which driver presents. This laptop has an NVIDIA
GeForce RTX 4060 Laptop GPU (driver 32.0.15.9174, GL `4.6.0 NVIDIA 591.74`)
and an Intel Iris Xe (31.0.101.5590), which drives the display.
`code/win32/win_main.cpp` exports `NvOptimusEnablement`, and every run of the
built executable put GL on NVIDIA. Retail `fakk2.exe` exports neither Optimus
symbol and runs on Intel.
`SHIM_MCCOMPAT=0x800000001` in a process's environment puts retail, or Python,
on NVIDIA. It does not move fnTech3. Nor does setting the export to 0 in a copy
of the executable. Renaming the export in that copy gives Intel: driver 591.74
honours the export's presence, not its value. The Vulkan renderer picks its GPU
by `r_device` (`-1` NVIDIA, `-2` Intel).

`r_drawBuffer GL_FRONT` draws to the front buffer, so `GLimp_EndFrame` skips
the swap. Unlocked, F.A.K.K.2 engine captures taken with it match `GL_BACK`
captures. Below the developer console, the menus differ by at most 2 and 7
levels in 255. A `fakkhouse` view differs by a mean of 1.0, all of it in
animated water, smoke and sky. The setting read `GL_FRONT` before and after
`devmap fakkhouse`.

## Inferred [I]

- The trigger is the locked session, or the display switching off after the
  lock; these runs cannot tell the two apart. Every locked run began more than
  a minute after the lock, and the console-lock display timeout is a hidden
  setting here, so the Windows default of 60 s applies.
- The delay is in the NVIDIA OpenGL driver's presentation under that
  condition, not in fnTech3. It appears in three profiles and removing the
  swap removes it. Skipping the error query moved it to `EndFrame`, as the
  2026-09-08 record shows. `SDL_GL_SwapWindow` is a bare `wglSwapBuffers`
  (SDL 3.4.0, `SDL_windowsopengl.c`). The only fnTech3 state that follows the
  lock is `gw_offscreen`, which only mutes audio.
- The retail comparison did not isolate the engine: it changed the GPU as well,
  and it was too short to include a stall.

## Consequences for probes

Frame-timed series need an unlocked session: Ritual `wait` chains, the spacing
of captures, and audio continuity measured from frame timing. A locked session
lets one stall swallow several short waits and merge several captures into one.
To check a past run, compare its log time with the Winlogon 4 and 5 events. To
check a live one, read `WTSSessionInfoEx` (`SessionFlags`: 0 locked, 1
unlocked).

Where a timed run cannot wait for an unlocked session, `+set r_drawBuffer
GL_FRONT` removed the stall in the locked Q3 and QL runs. That was not tried on
a locked F.A.K.K.2 run; that it holds for every profile is inferred from the
shared presentation path. Unlocked F.A.K.K.2 captures taken with it are sound.
It is a cheat cvar and a diagnostic, not a player setting. Menus and `devmap`
keep it. A plain `map` joins a server without cheats, and `Cvar_SetCheatState`
then resets it (`code/client/cl_parse.cpp`).

A comparison with retail F.A.K.K.2 should state each side's GPU.
`SHIM_MCCOMPAT=0x800000001` in retail's environment matches fnTech3's NVIDIA.

## Open

- The matrix was not repeated under a lock: the user declined on 2026-09-25.
  So it is untested whether Vulkan on NVIDIA, fnTech3 GL on Intel (the
  renamed-export copy), retail `fakk2.exe` on NVIDIA, or a bare WGL loop on
  NVIDIA stalls while locked. The harness waits for a lock, runs every case,
  then runs a control pass after the unlock (`.tmp/gl-private-stall/lock_watch.py`).
- Whether a display that times out in an unlocked session (30 minutes on mains
  power here) causes the same stall is untested.

## Evidence

The [structured evidence](2026-09-25-locked-session-presentation-stall.json)
lists the lock timeline and each run: log, time, session state, executable and
renderer hashes, command, steps, frame counts and every stall. It also records
the GPU-selection runs, the capture comparison, and the tool hashes.
`.tmp/gl-private-stall/build_evidence.py` rebuilds it from the run folders.

All runs were windowed on unswitched private desktops with isolated homes, at
800x600 (the 2026-09-08 runs used `r_mode 3`), and none sent input. fnTech3's
screenshots came from its own `screenshotPNG`. Nothing in the installations
changed: the F.A.K.K.2 layout under `.tmp/fakk2-install/game` (the probe's
inventory) and the Steam Q3 install (no file newer than the runs).
fnTech3 was `meson/build/win32-fakk2` (a working-tree build on top of
`b68782b`) with `-Dfakk2-development=true`. The 2026-09-08 runtimes are pinned
in their own record. Retail `fakk2.exe` 1.02 (SHA-256 `681bc51c...`) ran from a
task copy of its runtime at `.tmp/runtime/fakk2-retail-stall`, with sockets
off. No retail files are committed.
