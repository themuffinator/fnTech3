# Localize the private-desktop playback stall to presentation

The [client-stage checkpoint](2026-09-08-client-screen-timing.md) located long
audio-update intervals inside screen processing. Additional opt-in
`com_speeds 4` rows now distinguish drawing from `EndFrame`, and split drawing
into renderer `BeginFrame`, scene preparation, menu refresh and overlays.
The diagnostics use wall-clock milliseconds and include nested work in each
stage. They do not change rendering, audio defaults or module contracts.

Both Windows x86 profiles put the long delays inside `BeginFrame`. A temporary
renderer probe then measured the `glGetError` call using `ri.Microseconds`.
Its maxima were 461 ms for Q3 and 459 ms for QL. The first attempt used
`ri.Milliseconds`, which returns game-frame time after startup; that measurement
cannot establish intra-frame elapsed time and is excluded. Existing renderer
frame counters using that callback must not be interpreted as wall-clock proof.

Returning before the query when `r_ignoreGLErrors` is enabled did not remove the
delay: it moved to `EndFrame`. That experimental code was reverted. A subsequent
`r_drawBuffer GL_FRONT` control, using the original renderer error behavior and
unchanged timing defaults, removed the observed long stalls:

| Experiment | Q3 maximum screen time | QL maximum screen time |
| --- | ---: | ---: |
| Field-stage trace, normal back buffer | 460 ms | 460 ms |
| Skip ignored error query, temporary | 457 ms | 461 ms |
| Front-buffer control | 6 ms | 14 ms |

This supports a presentation-path dependency in this environment, not a claim
that error checking itself causes the delay. It does not distinguish driver,
desktop occlusion, compositor or GPU scheduling behavior. Front-buffer drawing
is not a production remedy and visual output was not validated in that control.
Normal presentation remains unchanged.

The front-buffer control completed both seven-channel synthetic music fixtures:
997 Hz intro and 2111 Hz loop windows were found, center routing stayed balanced,
and the final second was silent. Neither output had an exact-zero interval of
at least 5 ms after the first second and before the last nonzero frame. This is
a bounded file-output continuity observation; Q3 startup UI audio can overlap
the analysis interval. Normal back-buffer continuity and hardware output remain
open. Raw audio, retail inputs and logs remain ignored.

All ten accepted probe processes exited zero normally with unchanged staged
files. They used private unswitched desktops, windowed input-disabled clients,
isolated homes and SDL disk audio with capture disabled. No OS recording,
input control, speaker playback or retail installation writes were used. The
[structured evidence](2026-09-08-renderer-stall-localization.json) pins binary
identities, commands, configurations, timing rows and hashes. Temporary query
instrumentation and query-skipping variants are identified separately from the
final source. Both native Windows and SDL builds pass; the complete SDL suite
passes 110 targets, zero failures and one skip.

A separate harness issue was discovered: the engine accepts only 32 startup
command segments and silently drops later ones. The first front-buffer attempt
added an argument that dropped `exec probe.cfg`; Q3 was terminated at its
45-second deadline, and QL was not launched. That attempt is excluded from music
evidence. The accepted repeat removed the redundant swap-interval argument and
ran after the suite completed. The startup truncation issue remains open. No
external implementation was incorporated.
