# RTCW tactical navigation

This checkpoint implements SP/MP hide-position and attack-position imports
321 and 322. It follows the visibility and cast-query callback checkpoints
after `a80f24099284604326ec41dc41b4f73d7442ecd0`. Full navigation compatibility
and live campaign AI remain open, including the cold-cache differences below.

## Contracts and implementation

The original [SP routing implementation](https://github.com/id-Software/RTCW-SP/blob/master/src/botlib/be_aas_route.c),
[MP routing implementation](https://github.com/id-Software/RTCW-MP/blob/master/src/botlib/be_aas_route.c),
their entity/reachability helpers and [iortcw](https://github.com/iortcw/iortcw)
provide source observations. Production code is independently authored under
GPL-2.0-or-later. Original GPLv3 bodies are retained only in disposable comparison
programs with their notices; they are not incorporated into the engine.

`rtcw_tactical.hpp` owns each search's work list separately from routing-cache
updates. Scratch belongs to the selected navigation world and is freed with its
routing caches. A token prevents reentry into that world's active search and
detects teardown during a game callback. Returning callbacks restore the selected
world. Shutdown releases scratch and clears tokens even after `Com_Error` leaves
through `longjmp`; no stack destructor is required for cleanup.

Mode policy follows the original games:

| Policy | SP | MP |
| --- | --- | --- |
| Hide frame gate | Previous search's 3,000-work limit; negative source encoding forces another search | One search per time value, 4,000-work limit |
| Hide default flags | 20713406, including jump pads and slime | 20451262, excluding jump pads and including slime |
| Ladder areas | Excluded | Permitted |
| Occupied hide area | Original helper intentionally disabled | Adds 1,000 travel-time units for another solid entity |
| Attack entity area | Fuzzy point search | Linked areas, checked against selected-world ownership |
| Attack frame gate | One search per time value, 200-work budget; callback costs ten | Same |

Hide queries preserve the original raw 16-bit cost tracking, equal-cost revisits,
enemy-distance constraints, candidate visibility cache and callback arguments.
Attack queries search outward from the range actor and rank candidates by the
source actor's route time. The implementation preserves a zero route-time result
as a possible score, matching the source. Original-game vector normalization is
used for these tactical calculations.

The module adapter checks complete native/QVM spans, integer widths, game
entity limits, finite coordinates and nonnegative finite range. A subsequent
[prediction boundary audit](2026-09-16-rtcw-movement-prediction.md) corrected
the SP limit: both module ABIs use 10-bit entity numbers (1,024 entities).
SP's internal allocation of 2,048 botlib slots does not enlarge that contract.
The historical evidence below predates this correction; the prediction
checkpoint verifies the corrected guards. Owned vectors tolerate unaligned and
overlapping arguments. Results are staged and validated before publication;
failed searches leave caller output unchanged. Missing game callbacks cannot
prove that a position is hidden or attackable. Quake profiles reject these
imports before argument access, and their botlib POD layouts remain unchanged.

Fast-math testing exposed an optimized-away infinity check when an integer
float-bit classification was followed by floating-point comparisons. The shared
RTCW routing helper now reads the exponent bits through a volatile integer;
NaN/infinity rejection passes with the actual optimized build flags.

## Source comparisons and their limits

`scripts/probe_rtcw_tactical.py` executes independently owned engine/reference
caches, visibility decoders and work lists over 16 synthetic graphs in both
navigation worlds. Synthetic game callbacks compare their full arguments and
return deterministic position-dependent answers. Per reference, 4,096 hide
queries and 4,096 attack queries with warm caches agree on return values,
published vectors and callback sequences.

Original SP/MP and both iortcw modes pass on Windows x86 and Linux x86 with
AddressSanitizer, UndefinedBehaviorSanitizer and leak detection. Windows x64
passes both iortcw modes; original decoders require 32-bit pointers and are
explicitly skipped there. Final reports use `--normalization original`:
the original-game normalization is supplied to both sides to isolate tactical
algorithms. Unmodified iortcw reciprocal-square-root normalization produces a
callback-order difference in the diagnostic corpus. This is not a claim of
identical iortcw floating-point behavior.

Both iortcw modes disable the occupancy helper. The original MP corpus includes
a solid range actor; the iortcw MP corpus makes that actor intangible to isolate
the remaining algorithm. The report records this distinction rather than
treating iortcw as authoritative for original MP occupancy.

Cold-cache attack behavior is **not an agreement gate**. Each reference receives
4,096 additional cold-cache attack queries: 215 differ in result, output or
callback sequence. Subsequent route queries differ in 144 of 49,152 SP cases
and 150 of 49,152 MP cases. The first recorded SP route difference is 213 versus
342 travel-time units; MP is 209 versus 338. Source inspection shows the original
attack search and nested cache updates sharing the same area-update records;
the new implementation deliberately separates that storage. The observations
are consistent with that interference but do not establish retail behavior.
Each diagnostic resets caches before the next strict warm-cache comparison.
Runtime investigation of these differences remains required; the reports'
`passed` field applies to their strict comparisons, not to cold-cache parity.

## Validation

Five new executable tests cover both modes and worlds, repeated-frame limits,
forced SP searches, zero-cost cycles and exhausted budgets, nested routing and
world changes, occupied/cyclic/invalid links, callback-driven teardown,
allocation failures, real `setjmp`/`longjmp` cleanup, native/QVM spans and false
output publication. Together with routing and cast-boundary tests, nine focused
tests pass on Windows x86 in 16.964 seconds, Windows x64 in 15.646 seconds and
Linux x86 with sanitizers and leak detection in 149.954 seconds. Complete botlib
routing and server-bot translation units also pass Linux x86 syntax checks.

Both Win32 release configurations build the client, dedicated server and all
three renderers. The full Meson gate passes 111 targets with two skips; its
Python discovery gate passes 1,422 tests with 11 skips in 364.608 seconds.

Windowed SP/MP map probes pass using the same experimental executable, isolated
writable homes and unchanged installed content. Inspected engine captures show
SP's ready-arrow briefing and MP's deployment UI/movie. SP has 29 soldier clients
and reaches `rockandroll` while paused; MP retains pure mode. No input was
supplied and these probes do not demonstrate live tactical calls or gameplay.
All six Q3/QL renderer smoke probes pass with inspected engine captures, correct
module/renderer selection and removal of temporary home paths. QL remains at
its join UI.

Evidence lives under `.tmp/rtcw-manager/tactical-*`. The source identity record
is `tactical-source-identities.json`; final comparison reports are
`tactical-cold-diagnostic-x86.json`, `tactical-compared-x64.json` and
`tactical-compared-linux32.json`. Initial fast-math and iortcw numeric failures
remain separately identified. Default client SHA-256:
`6177808b6902cd3f9334aebb339230c6d45dea9d8c79948e31dcb0cab8109759`;
RTCW development client:
`a1b5a310ddf41f05ef0bec55ef097480783715ad086ab2e29f603c46a9f9dae3`.
Proprietary modules and assets are not tracked.

## Remaining scope

Movement prediction and release ShowRoute gained checked imports in the
[next checkpoint](2026-09-16-rtcw-movement-prediction.md). Installed-map tactical
comparisons, cold-cache retail observations,
cache generation/writing, installed MP-cache evidence, live campaign AI,
save/load, transitions, demos and bidirectional interoperability remain open.
Default RTCW startup stays gated. The existing unrelated Q3/QL routing-cache
reader bounds audit remains open; no new unrelated issue was found.
