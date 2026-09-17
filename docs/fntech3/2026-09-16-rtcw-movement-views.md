# RTCW movement routing and view queries

The shared executable now admits RTCW game imports 554,
`BOTLIB_AI_MOVEMENT_VIEW_TARGET`, and 573,
`BOTLIB_AI_PREDICT_VISIBLE_POSITION`, through checked native/QVM adapters.
The goal's 56-byte external layout is frozen independently. View adapters
project only the origin, area and, for visibility, ignored entity that these
queries read. Unused bounds and metadata can remain uninitialized; RTCW goal
flag bit 4 is never mistaken for Quake's dropped-item flag.

Goals, origins and target storage are owned while a query runs. Unaligned
records, overlapping inputs/outputs and exact VM end spans are handled without
exposing internal objects. Null goals retain their false result. An original
movement-view query can change its target before eventually returning false;
the adapter preserves that partial result. Untouched caller output bytes,
including nonfinite representations, remain untouched on failure. Invalid
returned scalars or changed target coordinates are rejected before publication.

## Movement routing policy

A private selector implements the distinct original SP/MP rules over the
engine's existing routing tables:

- Immediate movement and subsequent routing have separate travel masks.
  Ordinary and large do-not-enter restrictions at either endpoint are added
  to both masks. The previous area is excluded only when the goal is unchanged.
- SP excludes disabled destinations and destinations with no outgoing links,
  rejects a route whose first step returns to the current area, and adds only
  the candidate link's travel time. A same-area route deliberately consults
  stored record zero for that original loop check.
- MP adds the time needed to reach the candidate link's start. Equal costs
  retain the first candidate in both modes. The originals' disabled avoid-list
  code and Quake avoid spots do not influence RTCW selection.

Zero remains the original link iterator's sentinel. An empty or invalid link
span is rejected, rather than following another area's storage through the
legacy iterator. Incomplete worlds and invalid coordinates fail safely. Deep
geometry/cache-index validation remains owned by AAS initialization.

View traversal preserves original RTCW float normalization, complete travel
type comparisons, the teleporter stop, and the omission of jump-pad, elevator
and bobbing-platform span lengths. Quake retains its masked comparisons and
weapon-jump stops. RTCW traversal has a 4,096-step work bound for cycles that
make no distance progress; a partial target remains observable at that limit.

Visible-position queries retain the original 20-hop limit. Each step traces
from the goal's origin toward the link's start, then its end, ignoring the
goal entity. Entering the destination area returns the link's end even if both
traces were occluded. A query already in the destination area returns false
without publishing a target. These semantics are distinct from the existing
SP route-cache visibility service.

## Evidence and limits

The implementation follows observations of the original
[SP movement code](https://github.com/id-Software/RTCW-SP/blob/master/src/botlib/be_ai_move.c),
[SP routing code](https://github.com/id-Software/RTCW-SP/blob/master/src/botlib/be_aas_route.c)
and [MP movement code](https://github.com/id-Software/RTCW-MP/blob/master/src/botlib/be_ai_move.c).
Production extends inherited GPL-2.0-or-later code independently. Original and
[iortcw](https://github.com/iortcw/iortcw) GPLv3 bodies and notices execute only
in disposable reference programs.

`tests/rtcw_goal_route_tests.py` exercises controlled route costs and real
independent routing worlds, including loop/entry rules, equal-cost choices,
disabled/terminal areas, malformed spans and teardown.
`tests/rtcw_movement_view_tests.py` exercises actual movement state, selectors,
view traversal, BSP trace wrappers and module boundaries. Its controlled
visibility callback receives legitimate null point-trace bounds. Native/QVM
tests cover unused fields, aliases, partial publication, integer/float limits,
owned inputs, absent interfaces and malformed results. Deliberately cyclic
fixtures establish the work limits; unbounded original loops are not run.

`scripts/probe_rtcw_movement_views.py` uses two independently mutable routing
worlds with identical synthetic graphs. Per reference it compares 20,736
selector queries, 33,984 movement views and 16,992 visible-position queries.
Original SP and MP agree exactly under strict Windows x86/x64 arithmetic and
Linux x86 SSE2 with address/undefined-behavior sanitizers and leak detection.
Both sides share engine routing, geometry and controlled visibility services;
this does not establish original-engine routing or physical-collision parity.

iortcw SP has removed the separate movement-mask argument. Omitting that
unsupported argument in its reference call yields 4,300 differing selector
choices in this corpus. Strict iortcw SP/MP also yield 395/410 differing view
targets, with unchanged view booleans and visible-position results. These are
recorded diagnostics, not the compatibility target. Fast-math Windows x86
produces 698/711 differing view targets against original SP/MP, with unchanged
selector choices, view booleans and visible-position results. Its iortcw view
differences are 8,281/8,308; SP retains the 4,300 selector differences.

The goal ABI probe passes 2,187 SP and 2,177 MP assertions against all four
reference header sets on Windows x86 and x64. Sixteen focused tests pass on
both Windows architectures and sanitized Linux x86. The full regression suite
passes 111 Meson tests with two skips; its Python suite passes 1,436 tests with
11 skips in 398.627 seconds. Complete Linux x86 movement/routing/server
translation units compile. Default and experimental Win32 client and
dedicated-server builds with all three renderers pass.

The default executable SHA-256 is
`1371ab752f65ccce089474377b68067c629d6c4ea30577ab0c48675b9b2a3baf`;
the experimental executable SHA-256 is
`01954226ac2a0c2bb97d1acdda380e1bf948ce57d2571bbac44619a9bff9310e`.
Both include recorded uncommitted work after `a80f240`. Evidence is retained
under `.tmp/rtcw-manager/movement-view-*`.

Eight engine screenshots were personally inspected: readable SP briefing and
ready arrow, MP deployment UI/movie, and Q3/QL scenes on GLX, VK and RTX. The
SP map initializes 29 soldier clients and emits `rockandroll`, but remains at
its paused mission briefing. MP retains pure admission; QL remains at its
join screen. Runs were windowed on private desktops, with input disabled,
isolated writable homes and unchanged installations. These observations do
not establish active actor movement or an entered QL match.

Movement-to-goal remains unadmitted pending its result contract, travel
execution, stuck detection and future-view policies. Installed/live actor
movement, moving-entity physics, cold-cache generation, campaign save/load and
transitions, demos and peer interoperability remain unverified. The default
RTCW startup gate remains closed; no full compatibility claim is made.

The unrelated Q3/QL routing-cache reader bounds audit remains open. No new
unrelated production issue was found in this slice.
