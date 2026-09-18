# Range checks now cover values set before registration

`Cvar_CheckRange` forced its initial check by setting the cvar to its own
current value. Since the config-replay no-op of commits `f92f90a` and
`cafdef7`, `Cvar_Set2` returns before validating a value equal to the current
one, so that check validated nothing. Configuration files run before most
engine cvars are registered, so a value they set was never checked. Quake
III's own `missionpack/q3config.cfg` sets `com_hunkMegs "56"`, and the `q3`
profile ran with a 56 MB hunk (`meminfo`: 58720256 bytes) although
`Com_InitHunkMemory` asks for at least 64.

`Cvar_CheckRange` now validates the current value itself and sets only a value
that validation changed. Replaying an identical value stays quiet, including
for write-protected cvars. `Cvar_Set` forces, so a clamp takes effect at once,
also for latched cvars: the same start now allocates 67108864 bytes. A valid
current value leaves a pending latched value alone. The one path check,
`fs_game`'s, now applies at filesystem start too: `+set fs_game ../outside` is
reset to the default with a warning, where it used to pass, and `missionpack`
is kept.
[`tests/cvar_range_check_tests.py`](../../tests/cvar_range_check_tests.py) runs
the real cvar code through these cases.

Retail Quake Live's bounded registration (`Cvar_GetBounded` in the QLSRP
reconstruction) also re-sets the current value through validation, so bounds
now clamp at registration as they do there. The QL cgame range import now
clamps before it fills the module's `vmCvar`, as retail does, so cgame starts
from the bounded value. The qagame import already did.

## Method

- **Defaults.** All 678 range registrations in the engine and renderers,
  including QL's renderer table, were paired with the default registered for
  their cvar. Every default lies inside its own range in every profile, and no
  literal `Cvar_Set` of a range-checked cvar is outside its range. Nothing
  relies on an out-of-range sentinel value.
- **Retail configurations.** Every `set`/`seta` line of 271 retail configs was
  checked against each range registered for that cvar. They were the
  installations' configs (Q3 3, QL 14, ET 2) and the players' configs shipped
  in the paks (Q3 46, QL 5, RTCW 151 across `pak0.pk3` and `mp_pak0.pk3`,
  ET 50).
- **Runtime.** A client with a temporary log line in `Cvar_CheckRange` ran
  each profile on unswitched private desktops. Each run started from an empty
  home, the installation's configs or shipped player configs, and went to the
  menu, a map (`q3dm17`, `campgrounds ffa`, `escape1`, `mp_beach`, `oasis`),
  then the menu again, reading back what fnTech3 wrote (48 runs). Dedicated
  servers for `q3`, `ql`, `rtcw-mp` and `wolfet` also ran, from empty homes and
  with the retail server configs (14 runs). QL map runs include the retail
  cgame and qagame bounded registrations. The log line and the scratch scripts
  are not kept.

## Result

| Cvar | Range | Retail values outside it | Outcome |
| --- | --- | --- | --- |
| `com_hunkMegs` | at least 64, dedicated 48 | 20–60 in 71 configs, including Q3's `missionpack/q3config.cfg` and ET's `etconfig.cfg` | Raised to the minimum. This is the fix. ET runs used the client's own 128 MB ET floor, applied before the check. |
| `sensitivity` | was 0.1–10 | 10.06–40 in 90 player configs shipped with all four games | **Range removed**; a numeric check remains. No retail engine bounds it (QL registers it with the plain `Cvar_Get`), and 10 would change these players' aim. |
| `in_mouse`, X11 backend | was 0–1 | -1 in 29 Q3 and RTCW configs, 2 in 6 QL configs | **-1 to 2**, as on Win32 and SDL. The backend enables the mouse for any nonzero value; -1 would have disabled it. |
| `r_ignorehwgamma` | was 0–1 | -1 in ET's `fragviper_mp.cfg` and `sonder.cfg` | **-1 to 1** (committed with `c42ca2c`). Retail and all three renderers skip hardware gamma for any nonzero value; 0 would invert it. |
| `r_picmip`, `s_doppler` | integer | `3.981250`, `1.0` | Stored as `3`, `1`: the same integer. |
| `com_maxfps` | 0–1000 | 2000 in RTCW's `foolkiller.cfg` | 1000: 1 ms frames either way. |
| `s_musicVolume` | 0–2 | -0.05 in Q3's `danh.cfg` | 0. |
| `r_flareSize` (GLx, Vulkan) | 1–40 | 0 in Q3's `q3atari.cfg` | 1. Flares are off by default and retail made it a cheat cvar. |
| `r_customWidth`, `r_customHeight` | at least 4 | empty in ET's `FragViper.cfg` | The default. |

With the three ranges corrected, the runtime runs changed only `com_hunkMegs`
and `s_doppler`'s `1.0`. No QL value changed in any run, including under the
retail cgame and qagame bounds. No dedicated run changed a value, and no third
start changed what the first had written.

## Observation

Until `fd592c7`, the QL retail video bridge in `CL_InitGLimp_Cvars` also ran
for Q3, RTCW and ET. Their configs archive `r_stereo` and
`r_ext_gamma_control`, so it set `r_ignoreHWGamma` from `r_ext_gamma_control`
and `sonder.cfg`'s `-1` ended as 0 whatever its range. With the bridge limited
to Quake Live, an ET start with `sonder.cfg` keeps `-1`.

## Not covered

The X11 backend is not built on Windows, so its range was checked statically
only. Runtime runs used the GLx renderer with sound disabled; the Vulkan and
RTX renderers and the audio backends were checked statically.
