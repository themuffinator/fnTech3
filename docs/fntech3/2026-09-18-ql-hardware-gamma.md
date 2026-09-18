# Quake Live keeps r_ignoreHWGamma as configured

FnQL's retail video bridge in `CL_InitGLimp_Cvars` set `r_ignoreHWGamma` to
`!r_ext_gamma_control` whenever it adopted Quake Live's retail video config.
Once `r_qlRetailVideoBridge` was set, that was every renderer start. The
[change that limited the bridge to the `ql` profile](2026-09-18-retail-video-bridge-profiles.md)
(`fd592c7`) left this mapping in place. Retail QL has no such mapping. A retail
config that asks for software gamma therefore drew with hardware gamma here,
and the bridge reverted a player's own `r_ignoreHWGamma` at a later renderer
start.

## Retail contract

In the QLSRP HLIL of retail `quakelive_steam.exe`:

- `r_ext_gamma_control` is registered at `0x4494dd` with default `"1"` and
  flags `0x80021`, and `r_ignoreHWGamma` at `0x449721` with default `"0"` and
  flags `0x21`.
- The hardware-gamma check (function at `0x469b00`) calls `GetDeviceGammaRamp`
  only when `r_ignoreHWGamma` is zero, and, as in Q3, not for a standalone
  driver (`0x469b64`). The gamma set path (`0x469c30`) writes the ramp only
  when the device supports gamma, `r_ignoreHWGamma` is zero and there is a
  window DC (`0x469c66`).
- `r_ext_gamma_control` is read once, in the extension setup (`0x46b060`). At
  `0x46b302` the `WGL_3DFX_gamma_control` entry points are fetched only when
  `r_ignoreHWGamma` is zero and `r_ext_gamma_control` is nonzero.
- Neither cvar is read anywhere else, and neither name is looked up as a
  string.

So `r_ext_gamma_control` only decides whether the 3Dfx extension sets the
ramp, and `r_ignoreHWGamma` alone decides whether the ramp is used. These are
Q3 1.32's `WG_CheckHardwareGamma`, `GLimp_SetGamma` and `GLW_InitExtensions`,
and the RTCW SP, RTCW MP and ET sources make the same three checks. fnTech3 has
no `WGL_3DFX_gamma_control` path at all.

The configs in the local QL installation agree. Retail QL wrote four
`qzconfig.cfg` files in two Steam profile directories. All four hold
`r_ext_gamma_control "1"`; three hold `r_ignoreHWGamma "1"` and one holds `"0"`.

## Change

- The bridge no longer writes `r_ignoreHWGamma`. It keeps the value that
  configs, the command line or the console set, in every profile.
- `r_ext_gamma_control` no longer counts as a retail video cvar. Its presence
  at the first renderer start and a later change to it no longer adopt the
  retail video config or set `r_qlRetailVideoBridge`. Without the gamma
  mapping, that adoption would only hand the window to `r_windowedMode` and
  copy `r_stereo` into `r_stereoEnabled`, which the gamma extension switch
  has nothing to do with.
- `r_ext_gamma_control` has the same description in every profile: a retail
  3Dfx gamma extension switch, kept for configs.

The rest of the QL bridge is unchanged: `r_stereo` into `r_stereoEnabled`,
`r_windowedMode`, `r_windowedWidth` and `r_windowedHeight` ownership, the
marker, and `UsesRetailVideoConfig`.

## Existing FnQL configs

fnTech3, like FnQL, writes `qzconfig.cfg` and `repconfig.cfg` at the root of
`fs_homepath`. On Windows that is `<install>/<SteamID>` by default, where
retail QL keeps its own. The installation holds two pairs that FnQL wrote. The
one in a Steam profile root has the marker set and `r_ignoreHWGamma "0"`; the
one at the installation root has the marker at `0`.

Such configs are kept as they are. A marked config holds the
`r_ignoreHWGamma` that the bridge last derived from `r_ext_gamma_control`,
which is the gamma mode the player has been running, so the next start keeps
that mode. The marked FnQL pair and the config the old build wrote from the
retail one start exactly as before (table below). A player who set
`r_ext_gamma_control 0` to get software gamma keeps it, because the config
also holds `r_ignoreHWGamma "1"`. To return to hardware gamma they now set
`r_ignoreHWGamma 0`. Only a player who set `r_ext_gamma_control 0` and quit
before any renderer restart gets hardware gamma where the old build gave
software gamma, as in the row with `+set r_ext_gamma_control 0`.

No one-time rewrite was added. A config cannot tell whether its
`r_ignoreHWGamma` was the player's choice or the bridge's, and the value the
bridge replaced is gone, since FnQL rewrote the file in place. Resetting to
the default would move players who chose software gamma to hardware gamma.
Deriving the value from `r_ext_gamma_control` once more would repeat the
mapping this change removes.

## Evidence

Clients built from the shared checkout at `4cadbdf`, before and after this
change (Windows x86 `win32-dev`, MSVC, SDL backend, GLx renderer unless noted),
ran on unswitched private desktops. Client SHA-256: before `8ded50c6…`, after
`3c5f8216…`; the renderer modules are identical. Each start used a fresh
`fs_homepath` holding only the listed configs at its root. The command line was
`com_gameProfile ql`, `fs_basepath` set to the Steam QL installation,
`com_steamIntegration 0`, `fs_skipWorkshop 1`, `cl_webuiEnable 0`,
`r_fullscreen 0`, `s_initsound 0`, mouse and joystick input off,
`com_maxfps 60` and `logfile 2`. A config passed with `+exec` waited 60
frames, printed the video and gamma cvars and quit. The `GAMMA:` line and
`r_ignoreHWGamma` come from `baseq3/qconsole.log`. All 22 starts exited with
code 0, and the installation's 761 files were unchanged in name, size and
modification time.

| Start | Before | After |
| --- | --- | --- |
| Retail `qzconfig.cfg` (`r_ext_gamma_control 1`, `r_ignoreHWGamma 1`) | 0, hardware | **1, software** |
| The same with the Vulkan renderer | 0, hardware | **1, software** |
| The config the first row's old start wrote (marker 1, `r_ignoreHWGamma 0`) | 0, hardware | 0, hardware |
| The FnQL-written pair from a Steam profile (marker 1, `r_ignoreHWGamma 0`) | 0, hardware | 0, hardware |
| The same with `+set r_ext_gamma_control 0` | 1, software | **0, hardware** |
| The config the previous row's old start wrote (`r_ext_gamma_control 0`, `r_ignoreHWGamma 1`) | 1, software | 1, software |
| The FnQL pair, then `r_ignorehwgamma 1`, `vid_restart`, `vid_restart` (at start; after each restart) | 0, hardware; 1, software; **0, hardware** | 0, hardware; 1, software; **1, software** |
| What the old build wrote after one such restart (marker 1, `r_ext_gamma_control 1`, `r_ignoreHWGamma 1`) | 0, hardware | **1, software** |
| Empty home | 0, hardware; marker 0; mode -2 | the same |
| Empty home with `+set r_ext_gamma_control 0` | 1, software; marker 1; mode 12 (1280x960) | **0, hardware; marker 0; mode -2** |

The old build kept a console `r_ignorehwgamma 1` for one `vid_restart` only
because `Cvar_Set2` returns early when the new value equals the current one.
The bridge's `"0"` was then a no-op and left the latched `"1"` in place. The
next renderer start, or the next launch from the config it wrote, returned
to `0`.

For each pair of starts, the archived cvars in the two configs written at quit
(744 to 951) were compared. Where the builds end with a different
`r_ignoreHWGamma`, that is the only difference, with one exception. The empty
home with `r_ext_gamma_control 0` also differs in the marker, `r_windowedMode`
(12 against -2) and `r_aspectRatio`: the old build adopted the retail video
config for that switch alone and drew a 1280x960 window instead of following
`r_mode`. The other pairs match entirely.
[`tests/client_cvar_compat_source_tests.py`](../../tests/client_cvar_compat_source_tests.py)
checks that no client source writes `r_ignorehwgamma` or reads
`r_ext_gamma_control`, and
[`tests/renderer_cvar_compat_source_tests.py`](../../tests/renderer_cvar_compat_source_tests.py)
that each renderer registers `r_ignorehwgamma` and gates hardware gamma on it.

## Not changed

- A retail `r_ignoreHWGamma` that FnQL's bridge overwrote stays overwritten,
  in fnTech3 and in retail QL, which reads the same file.
- The RTX renderer was checked statically only. It makes the same
  `r_ignorehwgamma` check as GLx and Vulkan.
