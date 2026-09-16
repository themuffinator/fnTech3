# RTCW frame and routing initialization

The shared engine now handles RTCW game import 205 (`BOTLIB_START_FRAME`),
initializes both navigation worlds, and loads their precompiled routing data.
The installed SP module initializes both `escape1` hulls and advances to import
304 (`BOTLIB_AAS_INITIALIZED`), which remains unsupported at this checkpoint.
MP still reaches its deployment menu and briefing movie with normal shutdown.
This does not establish playable SP, live AI, or MP bot compatibility. RTCW
remains gated in default builds. Evidence describes the dirty working tree at
`c75573c7375e44c908256020ada2d7e12970cacf`, not that commit alone.

## Profile behavior

The module adapter accepts a signed 32-bit float bit pattern, rejects NaN,
infinity and oversized native arguments, and forwards the actual bot-library
status. Positive and negative zero and finite negative clocks retain their
bits. The library's existing setup check remains in force.

Each RTCW frame visits both worlds, sets their clocks, invalidates entity state,
continues initialization and resets routing-update counts. It leaves world one
selected and increments only that world's frame counter, matching the original
SP/MP and iortcw frame functions. The counter saturates at `INT_MAX` to avoid
signed overflow. Q3/QL retain their existing frame path, including stale-entity
unlinking and developer/cache diagnostics.

RTCW initialization uses the already compiled AAS graph. It does not send AAS8
through Q3 reachability generation, clustering, optimization or writing. The
RCD loader validates topology before graph construction. Missing or invalid
caches produce an explicit startup error: RTCW cache generation and writing
are still unfinished, and the world is not reported as initialized.

The independent routing helper and initializer provide:

- RTCW travel-type and contents flags, including the large-actor entry flag.
  Liquids return immediately in water/slime/lava priority order. Q3 team and
  bridge flags are not inferred from RTCW's differently assigned bits.
- Ground-slope travel costs with the original speed factors. **SP rounds
  upward; MP truncates.** Binary32 intermediate rounding is explicit, including
  in fast-math builds. The source's minimum cost and unsigned-short wrap remain.
- Reversed links in original traversal order, without Q3's 128-edge truncation.
  Byte-sized reachability offsets support up to 256 outgoing links per area.
- Aligned cost matrices, portal maximum costs, scratch indexed by every global
  area, portal scratch with the extra update record, and RTCW's 4096 KiB default
  cache budget. Invalid numeric budgets use the default without an unsafe cast.

Graph spans and destinations are checked before use. Reverse-link and cost
allocations have checked arithmetic and separate 256 MiB limits. Reserved area
zero may contain a valid outgoing span: the installed `dam_b0` does. It reserves
cost rows but contributes no reversed links, matching the reference behavior.
Matrix pointer rows occupy an aligned region separate from 16-bit samples.
Every allocation has a world owner before another allocation can fail; shutdown
and map replacement release partial initialization.

Failure injection also exposed a shared bot-variable ownership defect. Variable
records now enter their owner list before name allocation, and teardown accepts
a partially allocated name. Value replacement copies before freeing the old
string, so allocation failure preserves it and aliased input remains valid.

## Source and installed-data evidence

`scripts/probe_rtcw_routing.py` executes the actual engine AAS/RCD loaders and
frame initializer against all 70 installed navigation pairs. Both worlds and
both selectors are exercised: 280 world initializations per architecture. MP
uses a synthetic RCD12 header over SP data; this is not installed MP-cache or
live MP bot evidence.

Unchanged original SP/MP and iortcw SP/MP functions are compiled in disposable
fixtures. Each reference checks 100,000 synthetic travel-cost cases, including
integer boundaries and mixed slopes, plus travel/contents flags and frame
ordering. Those synthetic checks repeat for each installed pair. Across the
installed corpus each reference also checks 813,810 reversed links, 3,984,524
matrix entries and 2,920 portal maxima in its profile's two worlds. The four
references total 15,938,096 matrix comparisons per architecture.

These comparisons pass on Windows x64, Windows x86 and Linux x86 with ASan,
UBSan and leak detection. The production fixture uses `-O2 -ffast-math`; the
reference functions use precise arithmetic. Linux's binary32 comparison uses
`--float32-reference` (`-msse2 -mfpmath=sse`). Default x87 reference evaluation
produces a different result in synthetic case 60804: 17095 versus the explicit
binary32 result 17094. That observation is retained, not counted as a pass.
The evidence therefore establishes binary32 source agreement, not agreement
with every historical compiler's extended intermediate arithmetic.

Reports under `.tmp/rtcw-manager/` are
`routing-reference-{x64,x86,sanitizers}.json` and
`routing-reference-x87-observation.json`. They record compiler commands, source
hashes and installed input identities; proprietary inputs remain in memory.

Seven focused routing/bootstrap/cache/world tests pass on Windows x64 (9.120
seconds), Windows x86 (10.377 seconds), and Linux x86 with sanitizers (48.948
seconds). The focused Linux tests use its default floating-point target.
Coverage includes both profile roundings, malformed scalars, minimum/wrapped
costs, two-world frames, reloads, absent hulls/caches, invalid graph spans,
129/256-edge areas, reserved-area rows, every engine allocation failure,
partial teardown, native/QVM scalar arguments and Q3/QL profile boundaries.
Logs are `routing-init-{x64,x86,sanitizers}.log`.

## Build and runtime evidence

Both Windows x86 client/server builds pass with GLX, Vulkan and RTX targets.
Client SHA-256 values are:

- Default: `c88b5f604a49076f5821acdcbb0b2952b4148226df33ac68b917cebed12dbf69`.
- Experimental: `39e0a37a00032a75d3452f2710e94b57afb6d612be492d2f408aba8446861884`.

Build logs are `routing-default-build.log` and `routing-dev-build.log`.
The complete suite passes 111 Meson tests with two skips, including 1,386 Python
tests in 299.545 seconds with 11 skips. Results are recorded in
`routing-full-tests.log` and `routing-full-testlog.txt`.

`routing-map/manifest.json` records the SP cache loads, both initialization
messages and the subsequent explicit import-304 failure. There is no SP map
capture. MP exits normally; its inspected GLX screenshot shows a textured
deployment menu and briefing movie, with the world obscured by the menu.

All six inspected Q3 `q3dm6` and QL `campgrounds` engine captures pass across
GLX, Vulkan and RTX. Textured scenes, Q3 weapon/HUD and QL join menus are
present. `routing-live/manifest.json` records exact commands, installations,
module/executable/renderer hashes and capture identities. Probes were windowed,
used unswitched private desktops with input and audio disabled, and removed
their isolated writable homes. Installations were unchanged. No operating-system
screen capture or input injection was used.

## Remaining work and provenance

The next observed SP boundary is the initialized-state query. Other navigation
queries, RTCW entity/movement records, route-cache updates, missing-cache
generation/writing and live AI remain unfinished. Original routing-update
budgets and initial goal costs differ from Q3; forwarding those queries to
unchanged Q3 algorithms is not sufficient. Broader campaign/save-load, active
GPU fog, SP letterboxing, MP gameplay and peer interoperability gates remain
open in the [compatibility ledger](COMPATIBILITY.md).

The inherited Q3/QL RCD reader's unchecked serialized native sizes/pointers
remain an unrelated, pre-existing audit item. RTCW does not use that reader.

Production code is independently authored from behavior facts in the original
[SP bot library](https://github.com/id-Software/RTCW-SP/tree/master/src/botlib),
[MP bot library](https://github.com/id-Software/RTCW-MP/tree/master/src/botlib)
and [iortcw](https://github.com/iortcw/iortcw). GPLv3 function text and notices
are confined to disposable comparison fixtures. No retail modules or assets
are incorporated into the repository.
