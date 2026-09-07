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

## Follow-up: stock asset isolation

At `fc19056`, inspection found an additional `zz-q3netpack.pk3` in the live
installation. The initial runs above used shipped demo bytes but did not
isolate the surrounding assets from that add-on. This limits their use as
stock-rendering evidence.

All four demos were rerun against the existing external fixture containing
only pak0 through pak8, with fresh writable homes. All four again reached
normal completion, processed frames and exited cleanly. The
[stock-fixture record](evidence/2026-09-07-q3-stock-demo-isolation.json) includes
the complete nine-pak hash inventory, demo hashes, commands and outcomes.
Logs remain in `.tmp/demo-q3-builtin-stock-*/`.

The missing-parenthesis warnings disappear in these isolated runs. The add-on
overrides the named shaders with unparenthesized `q3map_lightRGB` directives;
the current renderer parser expects parentheses for that directive. Thus the
earlier warnings are evidence of add-on parsing behavior, not a stock Q3
shader-syntax defect. The collision-bevel warnings remain in the stock fixture
and still need separate reference comparison. No installation files or renderer
behavior were changed during this follow-up.

## Follow-up: collision warning reference check

At `7aa8153`, the existing FnQuake3 x64 reference client played `demo001.dm3`
from the same pak0–pak8 fixture. It processed the same 1,346 frames, reached
the completion marker and exited cleanly. Both engines emitted exactly two
`CM_AddFacetBevels... invalid bevel` warnings. Their current `cm_patch.c`
source files also match after line-ending normalization. The
[reference record](evidence/2026-09-07-q3-bevel-reference.json) preserves the
binary hash, commands, outcomes and comparison.

The first reference probe completed playback but did not honor fnTech3's
quit-on-completion cvar, and required timeout cleanup. The successful rerun
used `nextdemo` with an explicit `quit` command. Both attempts remained
windowed with input capture disabled; no input injection was used.

These observations support classifying the warning as inherited reference
behavior rather than a newly observed merger regression. They do not prove
collision parity or that every discarded bevel is harmless. The reference
binary's source revision is still unknown; matching current checkout source
does not establish that artifact's build provenance. No collision code or
diagnostic severity was changed.
