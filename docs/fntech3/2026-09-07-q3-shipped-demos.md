# Shipped Q3 demo playback — 2026-09-07

The unified Windows x86 client successfully played four demos shipped in the
legitimate local Q3 installation. These provide reference inputs independent
of recordings made by fnTech3. Each run processed frames, reached its explicit
completion marker and exited normally without a truncation warning or reported
demo-message error.

| Archive | Demo | Result |
| --- | --- | --- |
| pak6.pk3 | demos/four.dm_66 | Completed |
| pak8.pk3 | demos/four.dm_68 | Completed |
| pak0.pk3 | demos/demo001.dm3 | Completed |
| pak0.pk3 | demos/demo002.dm3 | Completed |

The [evidence record](evidence/2026-09-07-q3-shipped-demos.json) identifies the
clean checkout `b72e5a5`, reused client hash, exact demo-member hashes, startup
commands, generated configs, outcomes, timedemo frame counts and warnings.
The binary is the same build used for the preceding entity-history probes;
this is not a fresh-build claim. Demos were read from their installation paks,
with separate writable temporary homes. No demo or retail asset bytes are
committed. Full logs remain in `.tmp/demo-q3-builtin-*/`.

All runs used windowed GLx, disabled mouse/joystick capture, disabled audio and
startup-authored console commands. No input injection or screen capture was
used. Timedemo mode provides completion/frame-processing evidence; its local
FPS measurements are not a performance benchmark, visual-parity check or
normal-speed pacing validation.

The old demos produced missing shader parenthesis and invalid collision bevel
warnings, alongside existing optional model/image warnings. Successful playback
does not resolve those warnings or prove rendering/collision parity. Coverage
of `.dm_67`, extension-bearing `.dm_71`, malformed/truncated bodies, seeking and
broader QL recordings remains open. These four fixtures do not close the full
demo compatibility gate.
