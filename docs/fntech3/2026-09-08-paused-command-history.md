# Preserve command history during demo pause

The [previous pause probe](2026-09-08-demo-pause-resume.md) showed stock Q3's
Connection Interrupted warning appearing while a local demo was paused.
fnTech3 now preserves the user-command ring position during demo pause while
continuing to sample current input. Both profiles resume normally, and the Q3
warning no longer appears in the repeated engine screenshot probe.

The [machine-readable record](2026-09-08-paused-command-history.json) pins the
source baseline `c616e36`, changed source hashes, executable identities, inputs,
commands, screenshots, tests and outcomes.

## Cause and correction

The observed stock Q3 CG_DrawDisconnect condition compares the oldest retained
user command with the current snapshot's acknowledged command time. Previously,
CL_CreateNewCommands advanced the ring on every real-time frame while the demo
simulation was stopped. After enough paused frames, the older history had been
replaced entirely by unacknowledged commands at the frozen time, triggering the
HUD's lost-connection condition.

CL_CreateNewCommands now keeps the ring position while a demo is paused through
cl_freezeDemo or timescale zero. It still builds the current command each frame
and updates its slot, preserving input sampling and clock rebasing. Live play
and unpaused demo playback continue advancing the counter normally. This does
not change external Q3/QL structure layouts, command ordinals or wire codecs.
The Q3 source was used to observe the diagnostic condition; no external
implementation text was incorporated.

An executable regression extracts the production function and checks sustained
pause beyond the ring length, the oldest-command condition used by Q3, current
input updates, resume, timescale zero, live-play isolation, null freeze cvar,
counter wrap and pre-gamestate suspension. The pre-fix implementation fails the
new executable assertion. The focused three-test file passes on Windows and
Linux. The rebuilt Windows x86 suite passes 110 tests, zero failures and one
skip; its Python aggregate passes 1,296 tests with five skips (160.452 seconds).

## Runtime evidence and remaining limits

The rebuilt client repeated the isolated windowed pause/resume probes on the
same Q3 and QL fixtures. All four runs (both profiles, both controls) captured
three engine screenshots, reached demo completion and exited 0 normally.
No input control, RCON or OS capture was used. Raw assets, demos and images remain
untracked, and installations remain read-only inputs.

With cl_freezeDemo, QL's complete paused frames remain pixel-identical. Q3's
paused comparison changes only 299 pixels inside the lagometer rectangle
x=1185..1273, y=896..905; the warning text is absent. After resuming, 126,223 Q3
pixels and 14,852 QL pixels differ, consistent with the visually observed return
of simulation/animation. This is not a claim that Q3's entire HUD stops updating.

The separate timescale-zero Q3 run also removes the warning; 298 lagometer pixels
differ while paused. In QL, timescale zero did not sustain a pause: 11,348 pixels
changed, including demo time/animation. The older packaged client reproduced that
behavior (11,662 differing pixels), so it is not established as a regression from
this correction. The cause remains unisolated; cl_freezeDemo is the verified QL
pause control here. Native interactive controls, frame advance, seeking and long
or repeated pause cycles remain open.

Unrelated diagnostics remain: Q3 lagometer updates during pause and QL's
pre-existing timescale-zero behavior. This correction does not claim to resolve
either, or the previously recorded optional asset/audio/service issues.
