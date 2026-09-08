# Shared demo pause/resume probe

On 2026-09-08, the Windows x86 client at source baseline `38ff5ce` exercised
`cl_freezeDemo` with a local Q3 `.dm_68` and the identified retail-written QL
`.dm_91` from the [demo interoperability checkpoint](2026-09-08-ql-retail-demo-interop.md).
Both profiles remained responsive to queued commands, resumed playback, reached
demo completion and exited 0 normally. QL's paused screenshots were pixel-identical;
Q3's world stayed fixed but its stock HUD displayed a connection warning.
This is partial pause/resume evidence, not complete seeking or rendering closure.

The [machine-readable record](2026-09-08-demo-pause-resume.json) pins the inputs,
client binary, configs, logs, screenshots, outcomes and image comparisons.

## Method and findings

The startup activeAction waits 30 frames, sets cl_freezeDemo to 1, waits five
frames and requests screenshot A. After another 90 frames it requests screenshot
B, waits three frames, unfreezes, waits 30 frames and requests a resumed image.
Normal demo completion then emits the completion marker and exits. Screenshots
come from the engine render target; clients run windowed and input/audio-disabled
on private, unswitched Windows desktops with isolated writable homes.
No keyboard/mouse control, RCON or OS capture was used. Raw inputs and images
remain ignored; retail installations are read-only inputs.

The first probe omitted the three-frame wait between screenshot B and unfreezing.
Because the screenshot is captured during later rendering, it included a resumed
frame. Its comparisons do not prove drift while paused. The corrected probes
separate screenshot submission from the unfreeze command.

| Profile | Paused A/B comparison | Paused B versus resumed |
| --- | --- | --- |
| QL, 640x480 | 0 differing RGB pixels across the complete frame | 14,753 differing pixels |
| Q3, 1280x960 | 10,516 differing pixels; connection-warning text and lagometer changes | 153,001 differing pixels |

For Q3, the full-width world rectangle x=0, y=250, width=1280, height=600
contains zero differing pixels between the paused images. This selected region
supports frozen world rendering only; it does not discount or hide the whole-frame
HUD differences. Visual inspection shows `Connection Interrupted` appearing while
paused even though the source is a local demo. Resuming removes that message and
advances item/weapon animation and health. The cause of the HUD warning remains
unresolved and requires follow-up at the Q3 module/command-time boundary.

## Limits and validation

No dedicated seek command was found in current client command registration or
the inspected retail executable command strings. That search is not proof that
every module/UI seeking mechanism is absent. Seeking, frame advance, pause through
native input controls, timescale changes and long/repeated pause cycles remain
unverified. The probe deliberately uses the existing cl_freezeDemo interface.

No engine code changed, so the full suite was not repeated. The preceding
[build/suite checkpoint](2026-09-08-glibc-baseline-build.md) remains applicable.
This documentation update passed release-document staging and link checks.
Unrelated optional asset/audio diagnostics remain; no audio functionality is
claimed. The newly observed Q3 connection warning is recorded as an open issue,
not a passed part of the pause gate.

## Follow-up correction

The [command-history checkpoint](2026-09-08-paused-command-history.md) fixes
the Q3 connection-warning condition while preserving input sampling and both
profiles' resumption. Lagometer updates and broader pause/seek coverage remain open.
