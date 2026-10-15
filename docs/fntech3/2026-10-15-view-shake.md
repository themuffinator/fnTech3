# Engine view-shake scale, 2026-10-15

The [camera-shake slice](2026-10-15-jka-camera-shake.md) found that
fnTech3, the pinned FnQuake3 and the pinned FnQL have no view-shake or
reduced-motion setting. This slice adds one engine-owned option,
`cl_viewShake`. It applies wherever the engine itself hands a view shake to a
game module, and it leaves retail's shake unchanged at its default. Only
Jedi Academy multiplayer (`jka-mp`) has such a crossing today. The profile
survey below records, for all sixteen profiles, where shakes, earthquakes
and view kicks are computed and why each profile does or does not honour
the option. The [JSON companion](2026-10-15-view-shake.json) holds the same
coverage, the commands and the outcomes. No compatibility gate closes.

Implementation is independently authored. The survey read fnTech3's own
sources and, as reference observation only, the GPL Quake III, RTCW, ET and
Elite Force (Elite-Reinforce) sources, pinned OpenJK, Ritual's F.A.K.K.2 1.02
SDK and pinned OpenMoHAA. No text from any of them is incorporated.

## The option

`cl_viewShake` is a client cvar, registered in `CL_Init` beside
`cl_menuBlur` and named on its pattern: a `cl_` comfort option with a 0..1
float range, archived, default `1`.

- `1` is retail's shake: the engine hands over exactly what it handed over
  before this slice.
- `0` removes the shake; values between scale its strength linearly.
- `CL_ViewShakeScale()` returns the factor. It holds values to 0..1, returns
  `1` before the cvar is registered, and keeps retail's shake for a value
  that is not a number. The factor is never negative, so it never flips a
  shake's direction.
- It is read at each hand-over, so a change applies from the next shake
  without a restart. Shakes already running in a module keep the strength
  they were handed.

It is not added to Quake Live's web settings menu: Quake Live has no
engine-side shake for it to change (below).

## Jedi Academy multiplayer: honoured

Observed (retail `jamp.exe` 1.01 and `cgamex86.dll` 1.01, recorded in the
[camera-shake record](2026-10-15-jka-camera-shake.md)): the engine's effects
system writes a 24-byte record to the client game's shared buffer, with the
shake intensity at `+0xc`, and calls the client game's export 31. The client
game scales that intensity by `1 - distance / radius`, holds the result at 16
and shakes its own view origin and angles by it, fading over the life.

Implemented (`code/client/client_jka_cgame_effects.hpp`,
`CL_JKACgameEffectCameraShake`): the host writes
`spawn.shakeIntensity * CL_ViewShakeScale()` into the record's intensity.
Origin, radius and life are unchanged.

- The export is still called at every scale, `0` included. The retail
  module therefore stores and replaces its shake, and makes its per-frame
  random draws, in retail's sequence; only the size of the movement changes.
  The client-side view and listener are the only things the shake moves.
- At `1` the multiplication is exact, so the record is bit-identical to the
  one handed over before this slice.
- The module's falloff and clamp come after the engine's scale (inference
  from the observed order). For a handed-over intensity of 16 or less, the
  shake the module applies is exactly the scale times retail's. Above 16
  the module's clamp can still hold a scaled shake at 16. Every installed
  CameraShake intensity is between 0.2 and 4 (camera-shake record), so the
  scale is exact for all installed effects.

## Other profiles: not honoured

| Profile | Where the shake is made | Engine crossing | Why not honoured |
| --- | --- | --- | --- |
| `q3`, `q3-ihv`, `q3test` | Client game: damage kick from the player state | None | No shake crosses an engine boundary. The engine passes the client game's refdef through unchanged. |
| `ql` | Retail client game, the same way | None | As Quake III. |
| `rtcw-sp`, `rtcw-mp`, `wolfet` | Client game: `EV_SHAKE` events and server commands set `cg.cameraShake` | None | The engine's spline camera (`code/compat/rtcw_camera.hpp`, `CG_GETCAMERAINFO`) has positions, targets, field of view, fades and an end event, but no shake. |
| `stvef-sp` | Client-game half of the game library (`CGCam_Shake`) | None | The view shake never leaves the module. The only engine-owned shake-like path is force feedback (imports 34-37), a device rumble with its own `use_ff` and `js_ffmult` settings, not a view movement. |
| `stvef-mp` | Holomatch client game | None | No shake crosses an engine boundary. |
| `jka-sp`, `jk2-sp` | Client-game half of the game library (OpenJK `CGCam_Shake`) | None served | The single-player client-module boundary is open: these clients stop before any client game runs. |
| `jk2-mp` | Engine effects system in retail `jk2mp.exe`, as in Academy (inference) | Not served | Outcast's effects imports (254-270) still refuse by name in `client_jk2_cgame.hpp`, so no CameraShake reaches the engine. A future Outcast effects host would scale its record the same way. |
| `alice`, `fakk2` | Game module: player state `damage_angles`, from the level earthquake or the damage kick | Snapshot field, copied unchanged | The shake reaches the engine only as part of the player state's `damage_angles` (Alice offset 1028, `code/compat/alice_game_abi.h`), copied through `CL_AliceGetSnapshot`. F.A.K.K.2's game writes one field for the earthquake or the damage kick (SDK `player.cpp`), and its client game subtracts it from the view (`cg_view.c`). Scaling it would change module-visible snapshot data and the damage flinch together. |
| `mohaa` | Game module: `damage_angles` from `level.earthquake_magnitude` (OpenMoHAA) | Snapshot field, delta-coded and copied unchanged | As Ritual's: `damage_angles` at offset 644 (`code/compat/mohaa_game_abi.h`), copied whole into the client game's snapshot (`cl_mohaa.cpp`). The client game subtracts it from its view (OpenMoHAA `cg_view.c`). Import 74 `get_camera_offset` returns zeros, and the force-feedback import that would play `fffx/general/screenshake1` is handed null. |

Observed: fnTech3's client, common, server and renderer sources have no
other shake, earthquake or view-kick path. Every other match for "shake" is
a network handshake. Inferred: where the shake is made inside each module,
taken from the reference sources named above, which are not the retail
modules.

## Fn presentation (invariant 11)

The shake is still computed by Academy's own client game in its own view.
Fn renders that view with its own renderer features and places Fn's spatial
audio listener where the module puts it. `cl_viewShake` is an engine
presentation option on top of that. Its default is retail's shake, and it
switches off none of Fn's features. FnQuake3 and FnQL have no shake option
to adopt, so this is new to fnTech3.

## Tests

- `tests/view_shake_tests.py` compiles the production `CL_ViewShakeScale`,
  sliced from `cl_main.cpp`, and checks it before registration and for 1,
  0.5, 0.25, 0, -0, -3, 4, plus and minus infinity and NaN. It also pins the
  registration and declaration, checks that only the Academy host reads the
  factor, and checks that this record, its companion, the ledger, the
  documentation index and the Display guide agree.
- `tests/jka_camera_shake_tests.py` runs the real Academy host. At scales
  0.5, 0.25 and 0, export 31 is still called once, with the same origin,
  radius and life and the intensity times the scale.

## Live check

A `jka-mp` listen client hosted `mp/ffa2` with the camera-shake slice's
probe and `explosions/explosion1` runners about 60 units from the eye,
within the effect's 300-unit shake radius (`.tmp/jka-camshake/ffa2-explosion-near.ent`). The
runs used an export of HEAD
`8a05e028` plus this slice, windowed, with `net_enabled 0`, no input and an
isolated home. The [JSON companion](2026-10-15-view-shake.json) records
binaries, inputs and outcomes.

Each run set `cl_viewShake` and then sampled `viewpos` 300 times, one per
frame, and took six engine screenshots (GLx). All three runs exited
normally, with no errors, an unchanged installation and unchanged binaries.

| `cl_viewShake` | Records handed over | Intensity handed over | `viewpos` off the eye | Distinct positions | Largest axis offset | Yaw range | Largest screenshot shift |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | 108 | 2.01 to 3.97 | 279 of 300 | 82 | 3 units | -182 to -177 | 1.90 px |
| 0.5 | 109 | 1.00 to 1.99 | 246 of 300 | 27 | 2 units | -181 to -178 | 0.93 px |
| 0 | 109 | 0 | 0 of 300 | 1 | 0 | -180 | 0 px |

Observed: at `1` the handed-over intensities span the effect's own 2 to 4,
as in the camera-shake slice's run. At `0.5` they are halved, and the
module's view moves less. At `0` every record still reaches export 31 with
no strength, and the view and every screenshot stay exactly still. `viewpos`
truncates to whole units, so small offsets still count as off the eye at
`0.5`. The screenshot shift is phase correlation between consecutive
captures, a Fn diagnostic.

## Not done

- Shakes computed inside a module (every profile but `jka-mp`) are
  unchanged. Changing them would mean rewriting a module's view or the
  snapshot data it reads, which the module ABI and wire contracts forbid.
- Outcast multiplayer's CameraShake waits for its effects boundary.
- No settings-menu control: the only engine-owned settings menu is Quake
  Live's, and the option does nothing there.
