# RTCW movement-area queries

The shared executable now admits RTCW game import 553,
`BOTLIB_AI_REACHABILITY_AREA`, through a checked adapter. It owns the input
vector, validates its coordinates and the ignored entity, and checks the
returned area number. The historical `client` parameter is an ignored entity
number, so cast actors are accepted throughout RTCW's 1,024-entity contract.
Native and QVM calls use the same boundary without exposing internal records.

The underlying inherited movement lookup retains its ground, mover,
swimming, fuzzy-area and downward-trace behavior. RTCW calls before map
initialization, after teardown, with missing storage/callbacks, or with
invalid entity/model indices fail without following invalid pointers.

## Profile corrections

The source audit found two distinctions needed by the lookup:

- Original SP's `AAS_AreaReachability` returns zero for temporarily disabled
  areas. MP and Quake retain the stored count. This also corrects SP fuzzy
  movement searches that consult the same routine.
- RTCW's model-reachability search compares the complete travel-type value.
  Quake's masking of team bits remains confined to its profiles.

These policies follow the published [SP bot library](https://github.com/id-Software/RTCW-SP/tree/master/src/botlib)
and [MP bot library](https://github.com/id-Software/RTCW-MP/tree/master/src/botlib).
Both [iortcw modes](https://github.com/iortcw/iortcw) agree in this corpus.
The production changes are independently authored extensions to inherited
GPL-2.0-or-later code. GPLv3 reference functions and notices are used only in
disposable comparison programs; no reference game implementation is shipped.

## Evidence

`tests/rtcw_movement_area_tests.py` executes the actual movement routine,
fuzzy search, entity-model lookup, model-reachability iteration and AAS
geometry traces. Its controlled BSP ground callback makes world hits, entity
hits, misses and start-solid conditions reproducible. Cases include platforms,
bobbing models, falling off another entity, liquids, disabled areas, both
navigation worlds, shared entity state, partial initialization and shutdown.
Separate native/QVM checks cover unaligned vectors, exact end spans, nulls,
integer overflow, invalid coordinates, mutating backends and invalid results.

`scripts/probe_rtcw_movement_areas.py` compares every original/iortcw mode
against 32,256 movement queries, 28 direct area-count queries and 588 model
queries per configuration. Results, backend call counts and unchanged input
vectors agree on Windows x86, Windows x64 and Linux x86 with address/undefined
behavior sanitizers and leak detection. Both implementations use the engine's
same synthetic AAS geometry and controlled BSP ground trace. This is not
original-engine collision or installed/live actor-movement parity.

Sixteen focused tests pass on all three configurations. Complete Linux x86
movement, reachability, routing and server dispatch translation units compile.
Default and experimental Win32 client/server builds with all three renderers
pass. The full suite passes 111 Meson checks and 1,428 Python tests, with two
and 11 expected skips respectively.

The default executable's SHA-256 is
`1da2f9048d2881806381664feef19ae81ab837c83efc9d8420ede1bbf37572fa`;
the experimental executable's is
`4cd39960dc44bc18d7fccaf40e5af6736ba6c27b7ee575df57a299bead0e7761`.
Both include recorded uncommitted work after `a80f240`.

All eight engine screenshots were inspected. SP retains its readable paused
briefing and ready arrow; MP retains its deployment screen and briefing movie.
Q3 and QL retain readable scenes across GLX, VK and RTX. QL remains at its join
screen. Probes were windowed, used private unswitched desktops with disabled
input, and kept writable homes separate from installed content. They establish
startup and renderer evidence, not active movement or campaign completion.

Reports, logs, runtime manifests and source/binary hashes are retained under
`.tmp/rtcw-manager/movement-area-*`; generated reference programs were removed.
The default RTCW startup gate remains in place. Movement-to-goal, directional
actions, view-target and visible-position imports, sustained AI, dynamic
collision parity, save/load, transitions, demos and interoperability still
need closure. The unrelated Q3/QL routing-cache reader bounds audit remains
open; no new unrelated issue was found in this slice.
