# Localize legacy playback delays without changing defaults

The [SDL legacy output checkpoint](2026-09-08-sdl-legacy-output.md) found silence
gaps despite correct routing. Repeating its seven-channel fixture with
`s_mixPreStep 0` removes the recurring 50 ms intervals in that run, but longer
gaps remain. The experimental stage is byte-identical to the baseline runtime.
This establishes a relationship with the cursor offset, not a complete remedy;
the default 0.05 timing contract is unchanged.

The legacy backend now offers `s_debugTiming 1`, a temporary developer cvar that
is off by default. Each actual mixer update can report milliseconds, the elapsed
interval used for adaptive mixing, playback cursor, painted counter on entry,
requested paint range, queued raw end and sample rate. `work_ms` measures work
through background decoding/queue preparation, before the diagnostic log and
before painting/submission. It intentionally does not claim to measure the whole
frame, rendering, callback synchronization, or diagnostic logging overhead.
`previous_end` is the painted counter on entry; an unchanged-cursor update can
already have moved that counter. Compare consecutive requested `end` values when
reasoning about submitted ranges instead of treating that field as a confirmed
previous hardware submission endpoint.

Both profiles show long intervals between updates: active-music maxima are
509 ms for Q3 and 495 ms for QL. Pre-paint work has a median of zero milliseconds
at the timer's resolution and a maximum of 5 ms in both runs. This points away
from loop-file reopening/decoding as the primary source of those long intervals.
Painting, synchronization, logging and the rest of the frame still need separate
timing evidence. Requested paint ranges after stalls can also extend beyond the
16,384-frame raw queue; the record retains both requested-range gaps and raw-queue
shortfalls without claiming their exact correspondence to every output gap.

All experiment processes exit zero normally with unchanged runtime stages.
They use private unswitched desktops, windowed input-disabled clients, isolated
homes and SDL disk playback with capture disabled. No speakers, OS recording,
input control or retail installation writes are used. Raw output/logs remain
ignored. Q3 startup UI signal may occur beyond the first second; its earliest
silence interval need not be a music interruption. Diagnostic logging itself
can perturb scheduling, so the uninstrumented zero-offset comparison remains
separate evidence. The silence-gap issue remains open. No unrelated issue was
found, and no external implementation was incorporated.

The native Windows and SDL x86 engines both build. The complete native-Windows
suite passes 110 targets with no failures and one skip, including 1,303 Python
tests with five skips. The [structured evidence](2026-09-08-legacy-timing-trace.json) pins
source and binary identities, commands, timing summaries, gaps and log hashes.
The SDL runtime probes use the diagnostic build; its earlier full-suite result
remains scoped to the preceding checkpoint.
