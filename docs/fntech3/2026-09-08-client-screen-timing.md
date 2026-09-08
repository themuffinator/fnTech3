# Locate legacy playback stalls within client frames

The [legacy timing investigation](2026-09-08-legacy-timing-trace.md) now has
separate paint/submission and client-stage measurements. `s_debugTiming 1`
reports `paint_ms` after submission; `work_ms` still excludes painting and
logging. `com_speeds 4` adds `client_timing` rows dividing ordinary client frames
into preparation, screen update, audio update and tail work. Both diagnostics
remain opt-in. Audio scheduling defaults and playback policy are unchanged.

On the same Windows x86 SDL engine, both Q3 and QL seven-channel music probes
place every measured stage exceeding 100 ms inside `SCR_UpdateScreen`.
Those screen updates take about 440–460 ms. An earlier combined frame/paint
probe measured at most 1 ms of active-music painting/submission in both profiles.
This narrows the investigation to screen processing, including UI, renderer and
presentation; it does not identify which call blocks or establish a correction.
Setting `r_swapInterval 0` explicitly is only a control because zero was already
the default. Timing instrumentation and the private desktop can affect scheduling.

Both clients exited zero normally. Probes used windowed input-disabled clients
on private unswitched desktops, isolated homes, legitimate installations and
SDL disk playback with capture disabled. No input control, OS recording or
speaker playback was used. Runtime stages and source identities, commands,
synthetic fixture configuration, timing maxima and log hashes are pinned in the
[structured evidence](2026-09-08-client-screen-timing.json). Raw audio and logs
remain ignored; no proprietary inputs are distributed.

Native Windows and SDL x86 builds pass. The complete SDL suite passes 110 targets
with zero failures and one skip. Continuity and hardware-output gates remain
open. No unrelated issue was discovered and no external code was incorporated.
