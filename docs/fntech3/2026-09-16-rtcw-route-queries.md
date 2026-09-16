# RTCW route queries and cache updates

The shared engine now implements RTCW game import 317,
`BOTLIB_AAS_AREA_TRAVEL_TIME_TO_GOAL_AREA`, using the selected navigation world
and RTCW routing rules. This completes a routing slice, not RTCW gameplay or AI
support. The development gate and the remaining compatibility gates stay open.
Evidence applies to the dirty working tree at
`c75573c7375e44c908256020ada2d7e12970cacf`, not that commit alone.

## Behavior

The native/QVM boundary checks all three signed integer arguments and the
optional origin span. It copies unaligned vectors into owned storage and
rejects nonfinite coordinates or coordinates outside the navigation bound of
±131072. A null origin retains its distinct meaning: omit the initial
within-area cost. Calls preserve the selected world and return the actual
bot-library result, including zero before initialization or after shutdown.

The inherited routing engine now selects these RTCW policies:

- Area-cache updates start with the goal area's first outgoing cost row.
  SP's ceiling and MP's truncation remain part of the existing cost contract.
- Every eligible reversed link consumes one update. An ordinary cache miss
  is deferred after the frame count exceeds 100; cache hits remain available.
  Portal construction can finish its dependent area caches beyond that limit.
  The counter saturates instead of overflowing a signed integer.
- Portal updates retain their first-reachability byte. Cross-cluster queries
  choose that first step even when the caller omits an origin, skip disabled
  portals, and respect both ordinary and large-actor entry restrictions.
- Cache expiry removes up to one ordinary area cache and one portal cache per
  pass. Equal timestamps use cluster/area/table order. Area caches whose goal
  is a portal remain protected. Existing ownership/LRU links and byte accounting
  are maintained through invalidation, expiry and shutdown.

Q3/QL retain their previous start row, update accounting, route policy and LRU
expiry. Their paths are executed by the focused tests. The initialized
same-area result remains one, before index validation, matching the published
query ordering. Invalid unequal area numbers return zero. Geometry assigned
to cluster zero and absent goal cost rows fail without indexing an empty
table; these are explicit safety bounds on otherwise undefined reference
accesses, not a claim to reproduce undefined behavior.

## Source and fixture evidence

The independently authored profile changes use observations from the original
[SP routing source](https://github.com/id-Software/RTCW-SP/blob/master/src/botlib/be_aas_route.c),
[MP routing source](https://github.com/id-Software/RTCW-MP/blob/master/src/botlib/be_aas_route.c)
and [iortcw](https://github.com/iortcw/iortcw). No GPLv3 implementation text is
incorporated into the production engine. The probe extracts unchanged reference
functions and their notices into disposable GPLv3 comparison fixtures.

`scripts/probe_rtcw_route_queries.py` gives each reference independent mutable
caches and update queues. It compares the result, cost, first step, frame count,
cache identity/order, entry costs, reachability bytes and size accounting.
Geometry and cost matrices use the separately verified initializer from the
[routing-initialization checkpoint](2026-09-16-rtcw-routing-initialization.md).
Reference travel flags are initialized by each source's own function/constants.
Symbol-only macro bindings avoid C++ ADL ambiguity; reference bodies are not
rewritten.

Each of the four references passes 66,176 synthetic queries on Windows x86/x64
and Linux x86 with ASan, UBSan and leak detection.
The sequences cover two worlds, mixed travel types/slopes/liquids, restricted
areas, disabled portals, excluded cluster-local areas, null origins, frame
budgets at 100/101, cache pressure and timestamp ties. A fan-in graph has 130
incoming links, and a separate case exercises unsigned-short cost wrapping.
Each 32-bit reference compares 3,273,412 synthetic cache records and 15,379,855
entries. The x64 figures are 3,271,805 records and 15,370,138 entries: native
pointer sizes alter when the byte budget forces cache expiry. Each target is
compared against a reference built for that same target.

The actual checked AAS/RCD loaders and initializer also load all 70 installed
navigation pairs into both worlds. Each reference passes 17,920 queries over
that corpus; the 32-bit targets compare 473,829,136 SP cache entries and
473,822,768 MP entries, and x64 compares 471,220,826 SP and 471,214,458 MP. The MP
selector uses a synthetic RCD12 header over installed SP data. This establishes
the algorithm's MP policy on those graphs, not retail MP navigation or bots.
Installed inputs are held in memory and identified by hashes in the reports.

The five focused route/initialization tests pass on Windows x86 (9.867 seconds),
Windows x64 (9.766 seconds), and Linux x86 with ASan, UBSan and leak detection
(80.710 seconds). They execute actual functions and cover native/QVM malformed
arguments, exact-end/unaligned spans, selected-world behavior, cache-hit versus
miss budgets, protected portal caches, allocation failures and teardown.

Reports are `.tmp/rtcw-manager/route-query-installed-{x86,x64,linux32}.json`.
They include the complete synthetic and installed comparisons, compiler
commands and input/source identities. The complete Linux i686 routing
translation unit also passes a Clang C++17 syntax check.

## Build and runtime evidence

Both Windows x86 client/server builds pass with GLX, Vulkan and RTX targets.
Client SHA-256 values are:

- Default: `b3c83a3088383eb4bbefe354928567e05629180fddd677d4a01da5c7df349167`.
- Development: `3e87b87ed5d524b67f82ca2a0467634655eea5205fefaa4f03126d6af344898a`.

The complete suite passes 111 Meson tests with two skips, including 1,413
Python tests in 337.983 seconds with 11 skips. Build logs are
`route-query-{default,dev}-build.log`; complete results are
`route-query-full-suite.log` and `route-query-testlog.txt`.

Windowed boot probes use the same development executable for both modes.
SP initializes both installed `escape1` navigation worlds, retains all 29
soldier clients, issues `rockandroll`, and displays the readable mission
briefing with its ready arrow. It remains paused with `cg_norender=1`.
MP reaches the deployment menu and briefing movie with `sv_pure=1`.
Both exit normally. These probes verify continued loading and teardown;
they do not establish live gameplay use of import 317.

The same default executable passes six Q3/QL scene checks across GLX, Vulkan
and RTX raster mode. Every engine PNG was personally inspected: Q3 shows its
world, weapon and HUD; QL shows its native join UI and world, without joining.
No renderer image-parity or ray-transport claim is made.

Runtime manifests and captures are in `route-query-map/` and `route-query-live/`.
Tests use private unswitched desktops, 640×480 windows, isolated writable home
paths, disabled input and muted/disabled audio. Installed files remain unchanged
and temporary homes are removed. There was no input injection or OS capture.
`route-query-source-identities.json` verifies the source/reference hashes,
tested binaries, full-suite results and runtime manifests for this checkpoint.

## Remaining scope

Live tactical queries and movement, missing-cache generation/writing, save/load,
sustained campaign/MP play, transitions, demos and peer interoperability remain
open. The existing unrelated Q3/QL routing-cache reader bounds audit is still
open. No new unrelated issue was found in this slice.
