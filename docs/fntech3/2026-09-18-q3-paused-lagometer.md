# Q3 lagometer while a demo is paused matches retail

The [command-history correction](2026-09-08-paused-command-history.md) removed
Q3's spurious connection warning during `cl_freezeDemo`, but left one
difference recorded as open in the Demos row of the
[compatibility ledger](COMPATIBILITY.md): Q3's paused frames still changed in
the lagometer, while QL's paused frames were identical. Whether that was an
fnTech3 defect or Q3's own behaviour had not been checked against the retail
engine. This record compares them directly. No engine code changed; the
[machine-readable record](2026-09-18-q3-paused-lagometer.json) holds identities,
commands and results.

## Method

[`scripts/probe_q3_paused_demo.py`](../../scripts/probe_q3_paused_demo.py) plays
the installation's own shipped demo `four` in retail `quake3.exe` 1.32 and in
the fnTech3 client under the `q3` profile. Both engines opened
`demos/four.dm_66` from `pak6.pk3`, which is byte-identical to `pak8.pk3`'s
`four.dm_68`.
Each engine freezes the demo with `cl_freezeDemo 1`, takes an engine screenshot,
waits 90 frames, takes a second, then resumes and takes a third. Both run
windowed on unswitched private desktops with isolated homes, sound and
mouse/joystick input disabled, `cg_lagometer 1` and `con_notifytime 0` so a
capture's own console notice does not appear in the next one. Console commands
only; no injected input and no operating-system capture.

## Result

| Engine | Resolution | Pixels changed between the two paused frames | Where |
| --- | --- | --- | --- |
| retail `quake3.exe` | 640 × 480 | 42 | x 593–638, y 448–451: 92.7–99.7 % across, 93.3–94.0 % down |
| fnTech3 | 1280 × 960 | 284 | x 1185–1274, y 896–901: 92.6–99.5 % across, 93.3–93.9 % down |

In both engines the only change while paused is the same strip of the
lagometer graph in the lower-right corner; everything else, including the
first-person view and the scoreboard header, holds. Resuming changes most of
the screen in both (305,074 and 1,219,573 pixels). Both engines show the same
demo moment, and both exit normally after the demo.

The inference is that the lagometer moves because Q3's own cgame keeps adding a
frame sample on every rendered frame, even with the demo's time held, which is
not an engine contract fnTech3 should change. The ledger's remaining Q3
lagometer note is therefore closed as matching retail. Why QL's paused frames
were identical in the earlier probe was not examined here.

## Unrelated observation

Retail `quake3.exe` also prints each `screenshot` command's own
`Wrote screenshots/shotNNNN.tga` notice into the next capture unless
`con_notifytime` is 0; the first run of this probe recorded that before the
setting was added.
