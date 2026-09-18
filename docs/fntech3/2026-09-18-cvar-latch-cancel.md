# Writing a latched cvar's current value cancels its pending value

Since `f92f90a`, `Cvar_Set2` has returned first thing when the new value
equals the current one, and since `cafdef7` also when a reset finds the cvar at
its reset value. That return keeps a config that replays a startup-only value
quiet: it comes before the write-protection checks, so there is no misleading
"write protected" message. It also came before Quake3e's latch handling, so
writing a latched cvar's current value no longer cancelled a pending latched
value, forced or not. Starting from 0, the console sequence
`r_ignorehwgamma 1`, `r_ignorehwgamma 0`, `vid_restart` ended at 1 with
software gamma. The cvar code is shared, so every profile did this; the `q3`
runs below show it outside Quake Live. Until `974fd52`, the QL video bridge's
write of `r_ignoreHWGamma` relied on this; see the
[hardware gamma change](2026-09-18-ql-hardware-gamma.md).

## Change

The early return now applies only when no latched value is pending. When one
is pending, a write of the current value, or a reset of a cvar that holds its
reset value, goes through the protection checks and then through Quake3e's
latch handling, which cancels the pending value quietly.

A cancel also does two things Quake3e does not do. It marks the cvar's flags
modified, so that an archive config written while the value was pending is
written again. Configs store the latched value, and Quake3e, like fnTech3,
delays config writes (`DELAY_WRITECONFIG`), so without this a cancelled value
could return at the next launch. The cancel also republishes the cvar to the
WebUI, whose cvar cache held the pending value.

Unchanged:

- With nothing pending, writing the current value is still a quiet no-op,
  including for `CVAR_INIT`, `CVAR_ROM` and cheat-protected cvars. So is a
  reset of a cvar that holds its reset value.
- `Cvar_CheckRange` still validates the current value itself (`4cadbdf`). A
  valid current value still leaves a pending latched value alone.
- A cvar whose protection refuses the write keeps its pending value, and the
  refusal prints its usual message, as in Quake3e. Hardly anything gets there.
  Read-only and write-protected cvars never have a pending value, because
  `Cvar_Get` applies one whenever it registers flags, and `Cvar_SetCheatState`
  drops cheat cvars' pending values when the client joins a server without
  cheats. That leaves a developer-only latched cvar changed while `developer`
  was on and written again after it was turned off.

## Other engines

| Engine | Writing the current value while a latched value is pending |
| --- | --- |
| Q3 1.32, RTCW SP and RTCW MP (GPL sources) | No-op; the pending value stays |
| Retail `quakelive_steam.exe` (QLSRP HLIL, `Cvar_Set2` at `0x4cce90`) | No-op, forced or not. The compare at `0x4ccf6a`–`0x4ccf97` returns before the protection, latch and force code |
| ET (GPL source) | Unforced: latched again as the current value, with "will be changed upon restarting". Forced: cancelled |
| Quake3e, FnQuake3, iortcw MP, wolfcamql | Cancelled quietly |
| fnTech3 from `f92f90a` to `974fd52` | No-op; the pending value stays |
| fnTech3 now | Cancelled quietly, as in Quake3e |

Retail QL keeps the pending value, like Q3 1.32 and RTCW, so the Quake3e
behaviour here is an engine choice, not retail parity. Modules do not see
latched values, except ET's through `trap_Cvar_LatchedVariableStringBuffer`,
which reads the same value after the cancel as after ET's own re-latch.

## Other writers

An audit of the engine's writes to latched cvars found these whose result
changes. Each now ends with the value it writes, not with an older pending
value. All of them were checked by reading the source only.

- **QL video bridge.** When the retail video config is in charge,
  `CL_InitGLimp_Cvars` copies `r_stereo` into `r_stereoEnabled` before the
  backend registers `r_stereoEnabled`. After each renderer start `r_stereo` is
  set from `r_stereoEnabled`, so the copy is normally a write of the current
  value. A console `r_stereoEnabled` value pending at a renderer start used to
  survive it, and then went back into `r_stereo`. The copy now cancels it, so
  `r_stereo` is the switch, as
  [RENDERER_CVAR_COMPATIBILITY.md](../fnql/RENDERER_CVAR_COMPATIBILITY.md)
  describes. This is the same loophole as the removed `r_ignoreHWGamma` write.
  The other profiles never run the bridge.
- **Command line.** `Com_StartupVariable` replays `+set` values after the
  configs, forced, and its comment says they must win over configs. They did
  not win when the command-line value equalled the current value and a config
  had latched another one. They now do (the `com_zoneMegs` runs below).
- **`spmap`.** `Cvar_SetLatched( "sv_maxclients", ... )` now cancels an older
  pending `sv_maxclients` when the current value already matches.
- **QL factories.** Applying a factory (`SV_FactoryPrepareMap`) and restoring
  its snapshot (`Cvar_RestoreFactoryValue`) write forced. A factory value equal
  to the current one now discards an operator's pending value. Before, that
  pending value replaced the factory's at the next registration. Retail keeps
  it.
- The window-resize request, the WebUI's `set` and `reset`, module
  `trap_Cvar_Set` (forced) and `trap_Cvar_Reset`, and `cvar_restart` likewise
  end with the value they write.

Not affected: `Cvar_Get` clears a pending value before it applies it.
`Cvar_CheckRange` never writes the current value. `Cvar_SetCheatState` drops
pending values first. The status writes after a renderer start (`r_stereo`,
`r_aspectRatio`) follow the `Cvar_Get` that applied their pending values. The
`s_khz`, `cl_renderer` and `fs_game` resets act only on values that differ
from the reset value.

## Evidence

[`tests/cvar_range_check_tests.py`](../../tests/cvar_range_check_tests.py) runs
the real cvar code through these cases:

- the console sequence, including re-registration;
- the forced write, the reset and the forced reset;
- the modified flags and the WebUI publication after a cancel;
- quiet replays with nothing pending, and of the pending value;
- quiet replays of `CVAR_INIT`, `CVAR_ROM` and cheat-protected values;
- a protected cvar with a pending value.

Against `974fd52`'s `cvar.c` the cancel checks fail.

Windows x86 `win32-dev` clients (MSVC, GLx renderer) were built from the shared
checkout at `974fd52`, before and after this change: client SHA-256
`3c5f8216…` before, `a4ba13a9…` after. The renderers, package and web pak are
identical. Each run used a fresh `fs_homepath` on an unswitched private desktop
with `r_fullscreen 0`, `com_steamIntegration 0`, `fs_skipWorkshop 1`,
`cl_webuiEnable 0`, `s_initsound 0`, `in_mouse 0`, `in_joystick 0`,
`in_nograb 1`, `cl_renderer glx`, `logfile 2` and `com_introplayed 1`. The
console commands came from the command line (`+wait 60`, the writes,
`+vid_restart`, `+wait 60`, a print, `+quit`). `ql` used the Steam
installation on `E:`, and `q3` the Steam installation on `C:`, whose own
`baseq3/q3config.cfg` sets `r_ignorehwgamma 1`. Values come from
`baseq3/qconsole.log` and from the configs written at quit.

| Run | Before | After |
| --- | --- | --- |
| `ql`, empty home: `r_ignorehwgamma 1`, `0`, `vid_restart` | `latched: "1"` after the 0; then 1, `GAMMA: software`; `qzconfig.cfg` holds 1 | **nothing pending after the 0; then 0, `GAMMA: hardware`; holds 0** |
| `q3`, the same writes (it starts at 1, so nothing is pending when the 0 comes) | 0, hardware | 0, hardware; console and config identical |
| `q3`: `r_ignorehwgamma 0`, `1`, `vid_restart` | `latched: "0"` after the 1; then 0, hardware | **nothing pending; then 1, software; `q3config.cfg` holds 1** |
| `ql`, home config `seta com_zoneMegs "96"`, `+set com_zoneMegs 64` | 64, `latched: "96"`; config holds 96 | **64, nothing pending; config holds 64** |
| `q3`, the same in `baseq3/q3config.cfg` | 64, `latched: "96"`; config holds 96 | **64, nothing pending; config holds 64** |

All ten runs exited with code 0 and printed no protection message. Beyond
these values, the before and after console logs differ only in timestamps,
the working directory, native-module cache paths and UI load times. The
configs written at quit differ only in the values shown. The installations'
761 (QL) and 3736 (Q3) files were unchanged in name, size and modification
time.

## Not covered

The bridge, `spmap`, factory, window-resize, WebUI and module paths were
checked statically only. A stereo pixel format was not requested at runtime.
RTCW and ET were not run: the change is in the shared cvar code, and the `q3`
runs above exercise it outside Quake Live.
