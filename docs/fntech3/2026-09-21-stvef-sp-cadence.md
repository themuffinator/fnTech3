# Elite Force SP simulation and snapshot cadence

Elite Force single player now publishes local snapshots after new simulation
ticks, independently of wall-clock speed. It also keeps retail's fixed tick
interval when `timescale` changes. The previous shared scheduler could run
several SP ticks while withholding their snapshots, or publish the same state
again on a render frame without a new tick.

## Native contract and correction

The user's `stvoy.exe` 1.20 is authoritative. Its `SV_Frame` at `0x4265c0`
clamps its game-time argument to 1,000 ms, computes an integer tick interval
as `1000 / sv_fps`, accumulates the residual, and returns before snapshots
when there is no full tick. It runs all due ticks before sending one update.
The loop does not multiply that interval by `timescale`: elapsed game time
has already been scaled before the call. The pause check at `0x4265a0`
returns without consuming the residual.

`SV_SendClientMessages` at `0x4287e0` checks the native level clock against
the client's next update. `SV_SendMessageToClient` at `0x427eb0` makes a
loopback client immediately eligible again by setting that deadline to
level time minus one (`0x427f16`). Together, these give one local snapshot
after each server frame that advances the simulation, even if that frame
runs several ticks.

fnTech3 previously used its shared time-scaled tick interval and checked
`svs.time - lastSnapshotTime` against `snapshotMsec` for every client. That
clock is real elapsed time. With `fixedtime 50` and a faster rendering loop,
SP's game advanced 50 ms per frame while snapshots were held back by the
real-time threshold.

The correction is limited to `stvef-sp`:

- `SV_Frame` uses the fixed integer tick interval and the native upper bound
  on the supplied game-time step, and returns when the residual cannot run
  a tick. The shared `Com_ModifyMsec` limits remain in force.
- A local SP client bypasses the wall-time snapshot interval. Queued messages,
  unsent fragments and byte-rate limits still prevent a new snapshot, keeping
  delta compression safe.
- Other profiles and remote clients keep their existing snapshot scheduling.
  `svs.time` and the meaning of `lastSnapshotTime` remain unchanged, including
  their connection, userinfo and pure-check resets. No second clock domain is
  introduced into those fields.

These are independently observed contracts and independently written changes.
No proprietary executable or reference implementation text is incorporated.
The SP SDK's snapshot-overlay field names helped identify the displayed
records. The Android SP engine's timing behavior differs from this retail
executable and was not used as the implementation authority.

## Executable regression

The checked fixture contains 780 native observations covering six supported
server rates (10, 20, 30, 40, 60 and 125), thirteen game-time steps through
1,001 ms, five initial residuals, pause/unpause, and two initial level clocks.
The local observation process maps the retail executable without running its
startup. It retains the native frame loop, pause check, snapshot eligibility
and loopback deadline instructions. Owned callbacks record game tick times
and suppress payload construction, network I/O, test saves, timeouts and ping
accounting. These replacements and the executable hash are in the fixture.

`tests/stvef_sp_cadence_tests.py` compiles the production frame loop, pause
check, snapshot scheduler and byte-rate check. Each native observation runs
against six time scales and three wall-time increments: 14,040 comparisons
per platform. It also checks all seven profiles for queue/rate guards and
client admission states, the unchanged behavior of the other six profiles,
pause with a partial tick, a save/map clock rewind, and a stopped server.

The regression fails against the preceding production code and passes with
the correction on Windows x86 and Linux i686. Linux executes the new probe
under AddressSanitizer, UndefinedBehaviorSanitizer and float-cast-overflow
checks. All 64 focused SP tests pass there. Windows and Linux client/dedicated
builds complete. The full Windows suite passes 120 targets with two skips and
no failures; its Python aggregate passes 1,852 tests with 72 skips in
697.127 seconds. The [manifest](2026-09-21-stvef-sp-cadence.json)
records individual target results. The wider Linux suite was not rerun and
its previously recorded failures remain open.

## Production observations

Twelve scene sessions complete normally and produce 36 engine screenshots:
retail and each of GLx, Vulkan and RTX on `tutorial` and `voy1`, plus paired
retail/GLx tutorial runs with 25 ms steps at `timescale .5` and 100 ms steps
at `timescale 2`. Every requested fnTech3 backend is verified in the log.
RTX uses SDR raster output here. The `voy1` views show the opening ship scene;
these short runs are not full bridge or campaign coverage.

At cumulative wait samples 20, 80 and 160 in the normal tutorial setup:

| Executable | Displayed snapshot times | Displayed snapshot numbers |
| --- | --- | --- |
| Retail | 2550, 5550, 9550 | 24, 84, 164 |
| fnTech3 before correction, GLx | 2600, 5450, 9350 | 8, 23, 44 |
| fnTech3 after correction, all three renderers | 2600, 5600, 9600 | 24, 84, 164 |

The growing lag is gone. The slow setup advances 30 then 40 snapshots over
the two sample intervals in both engines; the fast setup advances 60 then
80 in both. The wait-based capture setup still has startup/frame-phase
differences: normal fnTech3 times are 50 ms ahead, fast times are 100 ms
ahead, and slow snapshot numbering differs by one. Their causes have not
been isolated. These images do not establish synchronized pixel parity.

The later [startup/capture follow-up](2026-09-21-stvef-sp-startup.md) isolates
the screenshot phase and aligns displayed snapshot times with a revised
capture script. Its new observations do not retroactively synchronize the
captures described here.

Three further production sessions exercise saves and transitions with normal
elapsed-time scheduling. A tutorial save at `TIME=5300` reloads and re-saves
at `5800`. Transition to `borg2` starts a fresh clock (`2850`), a missing
tutorial hub save falls back to a new level (`2650`), and return to the saved
Borg hub restores its clock (`3250`). Retail loads the `5300` save and writes
`11750`; fnTech3 loads that result and writes `12100`. All generated save
files parse to their final framed chunk, and both returning engines write
in-game screenshots. This is a bounded round trip, not manual menu or
campaign-wide save acceptance.

A separate Linux i686 GLx session starts a local Holomatch `hm_borg1` server,
renders an engine screenshot on private Mesa/Xvfb, and exits normally. The
prepared manual engine under `E:/Games/fnTech3-tests/stvef-manual` is refreshed
to the tested artifacts from source commit `a56255c`. Both staged profiles
pass input-disabled preflight: SP loads the retail tutorial save and Holomatch
starts a local match with Munro. Existing manual homes, launchers, settings
and saves are preserved; no manual launcher was executed.

## Corrected image assessment and limits

The initial contact-sheet inspection incorrectly identified diagonal corridor
bands as a Vulkan/RTX difference from GLx. Full-resolution RGB comparisons
of the retained images disprove that assessment. At samples 80 and 160, both
Vulkan and RTX agree with GLx within **one channel value at every pixel**;
their mean absolute differences are 0.002663 and 0.002692 out of 255. The
earlier visual assessment is retracted, and no backend-specific corridor
blocker is established by these images. The manifest preserves the initial
assessment and its quantitative correction.

Two Vulkan controls disabled `cg_shadows` or `r_dynamiclight` separately.
Both corridor samples remain pixel-identical to their Vulkan baseline; the
shadow control does change the early Tuvok view. Five subsequent sessions
show lightmaps alone in GLx/Vulkan, and fullbright, increased picmip, and
anisotropic filtering in Vulkan. The two lightmap-only images have an RGB
mean difference of 0.001160. These controls and measurements are retained,
without treating resized contact sheets as pixel evidence.

Other scene differences remain bounded rather than dismissed: tutorial sample
20 has a mean GLx/Vulkan difference of 0.008628 but a maximum channel difference
of 57, and later `voy1` ship/effect frames have larger differences. Their
dynamic states have not been synchronized. Neither those captures nor the
native startup/frame-phase differences establish full renderer parity.
No renderer code changes are included in this cadence correction.

The tests run windowed on a private desktop that is never activated, with
mouse, joystick and force feedback disabled. They use independent writable
homes under `E:/Games/fnTech3-tests` and the copied legitimate assets. All
captures come from the engines' `screenshot` commands. The manifest records
commands, settings, source/build identities, save records and log/image
hashes; proprietary inputs, executable bytes and game images are not committed.
Useful local evidence is retained under `.tmp/stvef-gaps`.

A later [mipmap validation run](2026-09-21-stvef-image-mips.md) reused six
normal fnTech3 scene homes. Six logs and eight of their eighteen screenshots
changed; ten screenshots still match their original hashes. This record
preserves the historical observations and numerical comparisons, and its
manifest records the retention check. Native controls, slow/fast runs, save
evidence and the compiled fixture are unaffected.

Native controls also report the early `Janeway` affect warning and muted-audio
diagnostics; they are not newly attributed to fnTech3. Previously recorded
corrupt `st_21.bik`, wider Linux-suite failures and Windows x86 RT limits remain
open. Physical input, menu rebinding/save-load, listening, feedback-device
acceptance and broader campaign behavior still require their own evidence.
Neither Elite Force profile is promoted out of its development gate.
