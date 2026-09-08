# Demo completion handoff and damaged-recording runtime

When a nextdemo action started a replacement recording and enabled
cl_quitOnDemoCompleted, the previous playback's completion handler immediately
queued quit. Both Q3 and QL loaded the replacement modules and then exited before
that recording finished. The handler now defers automatic quit while another demo
is playing, allowing the replacement to reach its own completion. Plain completion,
null/disabled quit policy and timedemo completion remain covered.

An executable regression extracts the production completion function and checks
this sequence. It fails with the old quit condition and passes with the correction
on Windows and Linux. The Windows x86 incremental build and full configured suite
pass: 110 targets, zero failures and one skip. The structured
[evidence record](2026-09-08-demo-completion-handoff.json) records the Python count,
source hashes, commands, binary identities and before/after outcomes.

Runtime uses the same Windows x86 client with legitimate read-only assets and
locally recorded dm_68/dm_91 files. For each profile, four damaged-input variants
exercise EOF between records, one-byte and seven-byte partial trailer headers,
and a final payload missing one byte. All eight cases pass before and after the
fix: inter-record EOF is tolerated silently, each incomplete header/body reports
one truncation warning, the completion marker runs, and the process exits zero
without forced termination.

A separate recovery sequence starts intact playback after the shortened payload.
Both profiles fail to reach the final marker before the fix despite exiting zero.
Afterward both play the intact replacement to completion and exit zero normally.
Logs report two completed timedemo frame counts, plus the recovery-start and final
completion markers. This verifies continued playback and module reinitialization;
no rendering screenshot comparison or timedemo performance claim is made.

All clients run windowed on private unswitched desktops with input disabled and
isolated writable homes. No keyboard/mouse control, OS capture, installation
modification or proprietary file staging occurs. Demo bytes and raw runtime output
remain ignored. This extends the [active QL demo checkpoint](2026-09-08-ql-active-demo.md).
All claimed legacy formats, runtime oversize rejection, seeking, longer stress and
independent retail-engine demo exchange remain open; no full gate is promoted.
Existing QL temporary-hunk shutdown messages remain outside this correction.
No external implementation was incorporated.
