# RTCW navigation area traces

Status: **partial Source / Unit / Build / Fixture evidence**. Game import 309 now
traces navigation areas through the selected SP/MP world using the engine's
existing AAS traversal. Campaign gameplay remains unverified.

## Boundary behavior

The adapter owns both endpoint vectors and stages the area numbers and
optional entry points. It validates native/QVM spans, integer width, finite
inputs, result count, area indices and output coordinates before publishing.
Unaligned caller buffers and overlapping output arrays retain per-hit write
order. A zero-result trace preserves the original first-area zero sentinel;
unused output entries remain untouched. Scratch storage is released before
the engine's nonlocal error exit.

Zero capacity returns zero without accessing caller buffers. Negative capacity
and capacity above 65,535 are rejected. This storage bound matches the checked
RTCW AAS8 graph limit; it is an explicit safety restriction on the caller's
capacity argument. Native allocation validity remains the module's obligation,
while QVM spans receive exact bounds checks. The fixture's QVM argument coverage
does not establish supported RTCW QVM game modules.

The shared traversal also now returns immediately for nonpositive capacity
and checks for an empty stack before decrementing its pointer. The latter
avoids forming a pointer before the stack array. Defined positive-capacity
results preserve the original algorithm, including its pending-segment stack
limit. Deep single-sided paths remain distinct from stack occupancy.

## Evidence

`tests/rtcw_aas_trace_tests.py` executes the real trace and dispatcher against
both selected worlds, covering truncation, coplanar endpoints, opposite-facing
planes, solid leaves, optional points, exact-end and overlapping spans,
unaligned storage, malformed inputs/results and shutdown. It also runs shared
Q3/QL traversal cases. A separate test uses the engine-style nonlocal exit,
so sanitizer leak checks exercise scratch release without C++ unwinding.

`scripts/probe_rtcw_aas_queries.py --queries trace` compares unchanged
original and iortcw SP/MP trace functions in disposable fixtures. Each reference
passes **786,560 comparisons** per target, including complete output/guard
bytes, null point arrays, capacity limits, unloaded graphs, both worlds,
random synthetic splits and deep traversal/stack exhaustion. Original
nonpositive-capacity behavior is excluded from these defined-input comparisons.

Final evidence is retained under `.tmp/rtcw-manager/`:

- `aas-trace-fixed-reference-{x86,x64,linux32}.json`: four sources per target;
  Linux x86 uses ASan, UBSan and leak detection.
- `aas-trace-fixed-focused-{x86,x64,sanitizers}.log`: dispatcher, basic queries,
  world ownership and nonlocal cleanup tests; four pass per target in 6.542,
  6.112 and 28.766 seconds respectively.
- `aas-trace-basic-regression-x86.json`: the probe's default basic-query mode
  still passes 527,849 comparisons for each of its four references.
- `aas-trace-{dev,default}-build.log`: Windows x86 client/server and all three
  renderer targets build successfully.
- `aas-trace-full-suite.log` and `aas-trace-testlog.txt`: 111 Meson checks pass
  with two skips; 1,411 Python tests pass in 306.549 seconds with 11 skips.
- `aas-trace-map/manifest.json`: SP and MP boot cleanly through the same
  experimental executable. SP has all 29 soldiers and the ready arrow;
  MP passes local pure admission and displays the deployment UI/movie.
- `aas-trace-live/manifest.json`: all six Q3/QL renderer checks pass with
  personally inspected engine captures. QL remains at its join screen.
- `aas-trace-source-identities.json`: verified source, reference, build and
  evidence hashes for this dirty-worktree checkpoint.

All runtime probes used 640×480 windowed mode, isolated writable homes,
disabled input, muted/off audio and unswitched private desktops. Captures
came from the engine. Temporary homes were removed and installations remained
unchanged. Neither RTCW run establishes playable-world behavior.

Experimental executable SHA-256:
`c2e9fcb44a14c8f914077e0cef29869f049c6f8c6ba5610df3228f1b09c8ea8f`.
Default executable SHA-256:
`0c07e53b28e9237fb777ea4347240c1745781443abb907762a5bc5808aff893e`.

## Provenance and limits

The original [SP](https://github.com/id-Software/RTCW-SP/tree/master/src/botlib),
[MP](https://github.com/id-Software/RTCW-MP/tree/master/src/botlib) and
[iortcw](https://github.com/iortcw/iortcw) functions supply behavior evidence.
Their GPLv3 text and notices are confined to disposable comparison fixtures.
The checked adapter is independently authored and production retains the
inherited GPL-2.0-or-later traversal with the two boundary corrections above.

This adds one query contract. Route updates, movement prediction, tactical
queries, missing-cache generation, campaign play/save-load, renderer/audio
parity, demos and peer interoperability remain open in the
[compatibility ledger](COMPATIBILITY.md).

No new unrelated issue was found. The inherited Q3/QL routing-cache reader
bounds audit remains open.
