# Only Quake Live adopts its retail video config

FnQL added a bridge in `CL_InitGLimp_Cvars` for Quake Live's retail video
cvars. When `r_windowedMode`, `r_windowedWidth`, `r_windowedHeight`,
`r_stereo` or `r_ext_gamma_control` already held a value at startup, it copied
`r_stereo` into `r_stereoEnabled` and `!r_ext_gamma_control` into
`r_ignoreHWGamma`, freed `r_windowedMode` from `r_mode`, and archived
`r_qlRetailVideoBridge 1` so that later starts did the same. The bridge was
never limited to the `ql` profile. Q3, RTCW and ET configs archive `r_stereo`
and `r_ext_gamma_control` as well, so for those profiles an ordinary retail
config started it, and so did any `+set r_windowedMode` on the command line.

## Retail contracts

- **Cvar names.** Among the retail executables, only `quakelive_steam.exe`
  contains `r_windowedMode`, `r_windowedWidth` and `r_windowedHeight`.
  `quake3.exe`, `WolfSP.exe`, `WolfMP.exe`, `et.exe` and `quakelive_steam.exe`
  all contain `r_stereo`, `r_ext_gamma_control` and `r_ignorehwgamma`.
- **Configs.** Every config in the four installations and their paks that sets
  `r_stereo` or `r_ext_gamma_control` sets them to `"0"` and `"1"`: 34 Q3,
  45 RTCW, 20 ET and 6 QL configs. `r_ignorehwgamma` is `1` in 44 RTCW, 3 ET
  and 1 Q3 config, and `-1` in ET's `sonder.cfg` and `fragviper_mp.cfg`
  (`etmain/pak0.pk3`).
- **Menus.** RTCW SP's system and options menus, in-game copies included
  (`sp_pak1.pk3`, `sp_pak3.pk3`), test `r_ignorehwgamma` to decide which gamma
  controls to show. QL's option menus (`pak00.pk3`) edit `r_windowedMode`. No
  retail menu or UI module refers to `r_stereo` or `r_ext_gamma_control`.
- **Hardware gamma.** In the QLSRP HLIL of retail `quakelive_steam.exe`,
  `GetDeviceGammaRamp` runs only when `r_ignoreHWGamma` is zero (function at
  `0x469b00`). `r_ext_gamma_control` is read only to decide whether to use
  `WGL_3DFX_gamma_control` (function at `0x46b060`), as in Q3 1.32.

## Correction

[`UsesRetailVideoConfig`](../../code/client/client_cvar_compat.hpp) returns
false for every profile except `ql`, and for QL computes the old expression
unchanged. Only QL registers `r_qlRetailVideoBridge`, so
`CL_UpdateRetailVideoBridgeOwnership` never claims the retail cvars elsewhere.
In Q3, RTCW and ET, `r_windowedMode` follows `r_mode` at every renderer start,
and `r_ignorehwgamma` and `r_stereoEnabled` keep their configured values. A
marker left by an earlier build stays in those configs, but nothing reads it.

The probe scripts that sized their window with `r_windowedMode -1` now use
`r_mode -1` with `r_customWidth` and `r_customHeight`, which gives the same
window in every profile started from an empty home.

## Evidence

The same private-desktop start ran on clients built from the shared checkout
at `50c7b36` before and after this change (Windows x86 `win32-dev`, SDL
backend, GLx renderer). Each start used an isolated home, `r_fullscreen 0`,
`s_initsound 0` and `com_maxfps 30`, waited 150 frames, printed the cvars and
quit.

| Start | Before | After |
| --- | --- | --- |
| `wolfet`, `etmain/pak0.pk3:sonder.cfg` (`r_mode 7`, `r_ignorehwgamma -1`) | `r_ignorehwgamma 0`, hardware gamma, mode 12 (1280x960), marker `1` written | `-1`, software gamma, mode 7 (1152x864), no marker |
| `wolfet`, empty config, `+set r_mode 3`, `r_windowedMode -1` at 800x600 and `r_ignorehwgamma 1` | `r_ignorehwgamma 0`, hardware gamma, mode -1 (800x600) | `1`, software gamma, mode 3 (640x480) |
| `wolfet`, the config the first start wrote before this change | mode 12, hardware gamma | mode 7; `r_ignorehwgamma` stays 0 because that config no longer holds `-1` |
| `rtcw-mp`, `Main/mp_pak0.pk3:killme.cfg` (`r_mode 3`, `r_ignorehwgamma 1`) | `r_ignorehwgamma 0`, hardware gamma, mode 12 | `1`, software gamma, mode 3 |
| `q3`, the installation's `baseq3/q3config.cfg` (`r_mode -1`, `r_ignorehwgamma 1`) | `r_ignorehwgamma 0`, hardware gamma, mode 12 | `1`, software gamma, mode -1 (1566x1124) |

Four QL starts ended identically before and after: a `qzconfig.cfg` that
retail QL wrote for an installed Steam profile, an empty home, an empty home
with `+set r_windowedMode -1`, and the config the first of these wrote. The
gamma and mode lines, the watched cvars, and every archived cvar in the configs
each start wrote (774 to 802) matched.
[`tests/client_cvar_compat_tests.cpp`](../../tests/client_cvar_compat_tests.cpp)
checks all 32 QL input combinations against the old expression, and
[`tests/client_cvar_compat_source_tests.py`](../../tests/client_cvar_compat_source_tests.py)
that the client routes the decision through it.

## Not changed

- QL still copies `!r_ext_gamma_control` into `r_ignoreHWGamma`, which the
  retail code above does not do. The retail `qzconfig.cfg` holds
  `r_ignoreHWGamma "1"` and ends at `0` with hardware gamma here, before and
  after this change; by that code, retail QL would draw it with software gamma.
- In Q3, RTCW and ET, a window resized by dragging keeps its size for the
  session. The next restart that recreates the window returns to `r_mode`.
  Upstream Quake3e's resize handler does not write `r_mode` either.
