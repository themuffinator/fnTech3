# RTCW entity collision selection

RTCW SP and MP now own their entity-hull policy in the immutable game profile.
An entity with `SVF_CAPSULE` supplies a capsule for point, box and capsule
queries. Previously the shared server applied its QL rule, which selects that
target capsule only for capsule sweeps. Q3 and QL retain their existing policy.
Explicit brush models continue to use their inline collision model.

This corrects the hull selected by server and bot-library traces. It does not
establish original-engine collision geometry or live movement parity.

## Source and boundary evidence

The original [SP server](https://github.com/id-Software/RTCW-SP/blob/master/src/server/sv_world.c),
[MP server](https://github.com/id-Software/RTCW-MP/blob/master/src/server/sv_world.c)
and both [iortcw modes](https://github.com/iortcw/iortcw) choose the target hull
from the entity flag without a sweep-shape condition. Their GPLv3 function
bodies run only in disposable comparison fixtures with their notices.
Production policy and tests are independently written around fnTech3's
inherited GPL-2.0-or-later server; no reference implementation is incorporated.

`scripts/probe_rtcw_entity_hulls.py` compares each reference with the actual
server selector and game-data adapter. The reference reads the same frozen
external entity record directly. Each reference passes 1,024 combinations of
brush values, flags, query shapes, model numbers and bounds on Windows x86,
Windows x64 and Linux x86 with address/undefined-behavior sanitizers and leak
detection. Selected handles, capsule arguments, bounds, backend call counts
and unchanged padded game memory agree. The backend records model selection;
it does not implement collision geometry.

`tests/rtcw_entity_collision_tests.py` also exercises the actual
`SV_ClipToEntity` and `BotImport_EntityTrace` through all four native game-data
layouts. It verifies inline-model rotation, origin and bounds, contents-mask
filtering, entity numbering, invalid handles and bindings, and preservation
of private game fields. A second fixture uses the actual AAS entity-collision
and link-list paths to verify nearest hits, equal-fraction list ordering,
pass-entity filtering, start-solid results, unchanged output on no hit, and
independent links in both navigation worlds. Its backend hit results are
synthetic; installed moving-entity prediction remains open.

The internal botlib surface representation remains unchanged. Original RTCW
stores the incoming surface flags in `surface.value`, while the inherited
engine and iortcw use `surface.flags`. These are internal records rather than
the frozen game-module result. This checkpoint does not claim consumer-level
equivalence of those representations.

## Verification

Eleven focused tests pass on Windows x86/x64 and sanitized Linux x86. Complete
Linux x86 `sv_world.cpp` and `game_profile.cpp` translation units also compile.
Both default and experimental Win32 client/server builds, with all three
renderers, pass. The full suite passes 111 Meson checks and 1,426 Python tests,
with two and 11 expected skips respectively.

The experimental executable's SHA-256 is
`10798ead727a5fb1650ca12b54de057c7727e096dae5d8f261cfd73a4c521243`;
the default executable's is
`6ae3b6ccca54fc83742ce06f5a5f4d6a7425c15d5bf13fc2aacf823c6f55f940`.
Both were built from commit `a80f240` plus the recorded uncommitted changes.

All eight engine-written PNGs were inspected. SP reaches a readable paused
mission briefing and ready arrow with 29 soldier clients and `rockandroll`;
MP reaches the deployment screen and movie with pure mode enabled. Six Q3/QL
renderer probes retain readable worlds and interfaces. QL remains at its join
screen. Probes used windowed private desktops, disabled input and isolated
writable homes; temporary homes were removed and installed content unchanged.
These are startup/renderer observations, not live AI or gameplay evidence.

Evidence is under `.tmp/rtcw-manager/entity-collision-*`, including three
`hulls` reports, build/test logs, `map` and `live` runtime manifests and the
`source-identities.json` checkpoint. The initial fixture's C++ boolean type
error is retained separately from passing results. Generated reference
programs have been removed; no proprietary inputs are committed.

Dynamic collision geometry, moving-entity prediction, campaign AI, save/load,
transitions, demos and bidirectional interoperability remain open. RTCW
startup remains development-gated. The unrelated Q3/QL routing-cache reader
bounds audit is still open; no new unrelated issue was found in this slice.
