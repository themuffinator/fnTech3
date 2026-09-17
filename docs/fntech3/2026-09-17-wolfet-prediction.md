# Enemy Territory movement prediction

## Scope and authority

This work follows `250db12`, with a clean worktree at the start except for
another session's in-flight RTCW files. It implements ET's client movement
prediction and the world-trace result it returns, closing the last declared ET
game import. Handled imports reach **204 of 212**; the eight that remain are the
ordinals retail ET's own engine refuses. The ET client and default-release gates
are unchanged.

Interoperability facts come from the pinned original
[ET movement](https://github.com/id-Software/Enemy-Territory/blob/40342a9e3690cb5b627a433d4d5cbf30e3c57698/src/botlib/be_aas_move.c),
[stop events and the result record](https://github.com/id-Software/Enemy-Territory/blob/40342a9e3690cb5b627a433d4d5cbf30e3c57698/src/game/be_aas.h),
[the trace record](https://github.com/id-Software/Enemy-Territory/blob/40342a9e3690cb5b627a433d4d5cbf30e3c57698/src/game/botlib.h)
and [the trap](https://github.com/id-Software/Enemy-Territory/blob/40342a9e3690cb5b627a433d4d5cbf30e3c57698/src/game/g_syscalls.c).
The reference is GPL-3.0-or-later with additional terms. No implementation text
is incorporated into fnTech3.

## Why ET needs its own loop

An earlier record judged this import closable only by rewriting the loop rather
than converting a record. That judgement held. ET's prediction is not Quake's
with different settings:

| | Quake / fnTech3 | Enemy Territory |
| --- | --- | --- |
| Tracing | `AAS_TraceClientBBox` against a presence type | `AAS_Trace` against the world with a fixed box |
| Result trace | navigation trace: start-solid, fraction, end, entity, areas | world trace, adding a plane, a surface, contents and a side |
| Start | used as given | lifted 8 units at a time until clear, refused past 16 |
| Command vector | always a command | a destination when the frame count is negative |
| Friction pair | water friction while swimming | the other way round |
| Stop events above 1024 | bounding-box hit, cluster-portal touch | entity hit, stuck |
| Other stops | jump pad, teleporter, cluster portal | none of those |
| Reported presence | the type it tracked through the move | the stopping area's own type |
| End contents | an integer | a float |

Converting one result into the other would have meant reporting a plane, a
surface and contents that were never traced. Those are exactly the fields a mod
reads to decide what it hit, so the import previously failed instead.

## Contract

The loop runs up to `maxframes` frames of gravity, friction, acceleration and
clipping, sliding along up to twenty planes per frame and stepping up to the
configured step height when a wall is vertical and no jump happened in the last
two frames. Two near-identical planes in a row nudge the remaining velocity
along the second normal so a corner cannot trap the slide. Ground contact,
leaving the ground, gaps, ground damage, lava, slime, water, the caller's area,
a ground face in that area, a named entity and being stuck all end the
prediction, each with the frame count and elapsed time at that point.

Three details of the original are reproduced deliberately because a module can
observe them: ground damage reports the frame velocity rather than scaling it
back to a rate; the liquid stop leaves the trace field untouched; and the gap
stop reports the position from before that frame's move.

The result reaches the module as ET's own 128-byte record with its 84-byte world
trace. Every scalar is checked for a non-finite value first, and the surface
name is copied with a terminator rather than as a whole field, so a module never
reads past it.

## Verification

Clean Windows x86 RelWithDebInfo build with `-Dwolfet-development=true` and
`-Drtcw-development=true`; no errors or warnings. The complete automated suite
passes.

New regressions pin that the loop traces the world and not a client box while
Quake's prediction still traces client boxes, the unstick attempt and its
refusal, ET's own stop-event set with the four Quake events that must not appear,
the move-towards-a-point mode, the inverted friction pair, the bounded slide,
the result record sizes and offsets, that every scalar is checked before the
module sees it, that the surface name is terminated, and that the import passes
all thirteen arguments and drops on a rejected result.

A retail ET client still passes pure admission and joins both teams against the
rebuilt server.

## Remaining gates

**Nothing here has run.** Retail ET ships no `.aas` files in any pak, as
recorded in the [visibility record](2026-09-17-wolfet-visibility.md), so no
navigation world exists for a stock server to predict against. This is source
and unit evidence, and no movement parity is claimed against a retail ET server.

The eight ordinals retail ET leaves unimplemented stay refused, pinned by a
regression. Everything else in the
[ET matrix](COMPATIBILITY.md#wolfet-gate-matrix) remains as recorded, including
the gated ET client: its UI and cgame surfaces, MDM/MDX rendering, audio, input,
demos and reverse interoperability are untouched by this work.

## Unrelated observations

None new in this slice.
