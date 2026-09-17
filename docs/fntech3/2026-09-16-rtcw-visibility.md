# RTCW visibility and route-position checkpoint

This is a Source/Unit/Build/Fixture checkpoint for the shared executable. It
does not establish playable campaign AI or full RTCW compatibility. Work starts
from `a80f24099284604326ec41dc41b4f73d7442ecd0`, the requested commit of all
419 then-current worktree files. The changes below are subsequent work.

## Behavior

The routing engine now reads RTCW's already validated RCD visibility rows. Each
navigation world owns its compressed rows, decoded buffer and cached source
area. Repeated lookups reuse that row; a missing or malformed row leaves the
previous decoded row intact. Invalid indices, absent storage and unloaded
caches fail safely. Quake profiles retain the inherited visibility behavior.

Fuzzy area selection retains the shared direct-point and short upward probes.
The subsequent search follows the selected mode's published policy:

| Profile | Horizontal probes per layer | Horizontal extent | Vertical extent |
| --- | --- | --- | --- |
| Q3/QL | 3 by 3 grid | 8 | 12 |
| RTCW SP | 4 corners | 256 | 200 |
| RTCW MP | 3 by 3 grid | 16 | 24 |

Both RTCW modes still consider upper, level and lower layers in that order.
The original and iortcw MP comparisons caught an initial mistaken assumption
that MP used SP's larger corner search; the final implementation distinguishes
them. Q3/QL retain their prior probe order, extents and fallback behavior.

SP import 323 now finds the first visible position along a route. Same-area
and directly visible queries return the supplied source point. A route that
enters the destination returns the preceding position; an earlier visible area
returns its entry point. The destination must have a visibility row before
route traversal. Costs must strictly decrease, and traversal stops after 200
steps. Missing routes leave the caller's output unchanged.

The server owns input vectors, checks native/QVM spans and signed integer
width, limits coordinates to the navigation domain, and publishes only a
validated successful result. Inputs and output may be unaligned or overlap.
The engine-private entry point does not extend Quake's botlib export POD.
MP has no corresponding import and rejects the semantic call before reading
arguments. Partial initialization and shutdown return failure safely.

## Source comparisons

The original [SP](https://github.com/id-Software/RTCW-SP/tree/master/src/botlib),
[MP](https://github.com/id-Software/RTCW-MP/tree/master/src/botlib) and
[iortcw](https://github.com/iortcw/iortcw) sources supply behavior evidence.
Their GPLv3 implementation text is not incorporated into production code.
`scripts/probe_rtcw_visibility.py` compiles unchanged reference functions and
notices in disposable fixtures. Each reference has independent route caches,
update queues and decoded visibility. Geometry and cost tables come from the
previously verified loader and routing initializer.

Each tested reference passes 10,816 synthetic visibility lookups and 87,552
fuzzy-point comparisons. Each SP reference also passes 32,768 route-position
comparisons, including 16,501 successful results. Cases cover missing rows,
cached rows, solid leaves, non-reachable fallback areas, varied slab spacing,
two worlds, portal routes, restricted travel and frame budgets.

All 70 installed navigation pairs are loaded into both worlds through the
actual AAS8/RCD loaders and initializer. Per reference, the corpus passes
950,468 visibility lookups and 17,920 fuzzy-point queries. SP additionally
passes 17,493 route-position comparisons. Full decoded-buffer comparisons
cover 4,161,692,472 bytes for SP and 4,102,456,308 for MP, counting repeated
lookups. Installed inputs stay in memory. The MP corpus uses synthetic RCD12
headers over SP assets, so it is not installed MP navigation evidence.

Route comparisons exclude 427 pairs whose fuzzy endpoints have no outgoing
route or routing cluster. This is an explicit limit, not a parity claim for
those cases. An initial diagnostic on `assault_b0` observed equal failure
results but 0 versus 24 cache updates: the engine's previously documented
cluster-zero guard stops before original SP's extra cache work. Original SP
and iortcw themselves differ in early rejection of non-reachable endpoints.
Fuzzy lookup still compares fallback areas; checked failure paths are covered
by executable tests.

Original SP/MP and iortcw pass on Windows x86 and Linux x86 with ASan, UBSan
and leak detection. Windows x64 compares iortcw only; the originals' visibility
decompressor narrows pointers to 32 bits and is explicitly skipped there.
Every target agrees with its own compiled references. Installed SP success
totals are 1,168 on Windows and 1,169 on Linux, with differences across several
map pairs. Cross-platform numeric identity is not established.

Reports: `.tmp/rtcw-manager/visibility-checked-installed-{x86,x64,linux32}.json`.
They record compiler commands, source/input hashes, exclusions and counts.
Earlier failed reports are retained as investigation evidence and are not the
final parity records.

## Validation and remaining scope

Ten focused tests pass on Windows x86 (15.434 seconds), Windows x64 (15.339
seconds), and Linux x86 with sanitizers and leak detection (107.835 seconds).
They cover actual routes and imports, malformed buffers and backend results,
cache ownership, bounded traversal, partial setup and shutdown. Both complete
botlib translation units also pass a Linux i686 Clang C++17 syntax check.
Logs are `visibility-focused-{x86,x64,sanitizers}.log` and
`visibility-linux-translation-units.log` under `.tmp/rtcw-manager/`.

Both Windows x86 client/server builds pass with GLX, Vulkan and RTX targets.
Client SHA-256 values are:

- Default: `ce81d677eca0009870e905b07d949567f39bcfd0f5f0f07164e16aed385b221c`.
- Development: `c90225c0fb4c9e369808e151e13757931267311a5f85dbab8bf19babf09aa791`.

The complete suite passes 111 Meson targets with two skips, including 1,416
Python tests in 329.362 seconds with 11 skips. Logs are
`visibility-{default,dev}-build.log`, `visibility-full-suite.log` and
`visibility-testlog.txt`.

The same development executable boots both RTCW modes and exits normally.
SP initializes both `escape1` navigation worlds, retains all 29 soldier
clients, sends `rockandroll`, and displays the readable mission briefing and
ready arrow. It remains paused with `cg_norender=1`. MP reaches its deployment
UI and briefing movie with `sv_pure=1`. Both engine captures were personally
inspected. These checks establish loading and shutdown, not live use of the
new tactical query or playable campaign behavior.

The default executable passes all six Q3/QL scene checks across GLX, Vulkan
and RTX raster mode. Every engine PNG was personally inspected: Q3 shows the
world, weapon and HUD; QL shows its native join UI and world without joining.
The probes use private unswitched desktops, 640-by-480 windows, disabled input,
isolated writable home paths and muted/disabled audio. Installations remain
unchanged and temporary homes are removed. No input injection or OS capture
was used. Manifests and images are in `visibility-map/` and `visibility-live/`.
`visibility-source-identities.json` verifies the source/reference hashes,
tested binaries, complete suite and runtime manifests for this checkpoint.

Remaining work includes the other tactical queries, movement prediction,
missing-cache generation/writing, live campaign/MP play, save/load,
transitions, demos and peer interoperability. The existing unrelated Q3/QL
routing-cache reader bounds audit remains open; no new unrelated issue was
found in this slice.
