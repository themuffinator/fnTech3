# RTCW live AAS8 loading

This checkpoint connects the validated AAS8 reader to the shared engine's bot
library. The installed SP module now loads both `escape1` navigation hulls and
passes game import 206 (`BOTLIB_LOAD_MAP`). It next stops at import 311
(`BOTLIB_AAS_NEXT_BSP_ENTITY`). Campaign gameplay and AI routing remain open;
default RTCW startup is still gated. Evidence is from the dirty worktree at
`c75573c7375e44c908256020ada2d7e12970cacf`, not that commit alone.

## Implementation and contract boundaries

The checked import copies a bounded map name before invoking the real bot
library. Null retains the library's no-op update. RTCW map names must fit both
derived filenames, and absolute paths, parent/current-directory components,
empty components, backslashes and control characters are rejected before map
state changes.

`AAS_LoadMap` selects a dedicated RTCW path. It retires both old worlds, clears
world-zero entity links, loads the shared physical BSP once, and attempts
`maps/<map>_b0.aas` and `maps/<map>_b1.aas`. Original SP and MP allow either hull
to be missing or rejected; success requires at least one valid hull. The last
world remains selected, including if neither file loaded. Each successful world
owns its geometry, area links and alternative-route scratch. The sole entity
table remains in world zero. Quake's existing single-file loader is retained.

The AAS8 parser validates identity, version, checksum, extents, record lengths,
overlaps, finite geometry, indices and acyclic navigation trees before any disk
record becomes live navigation data. The input file is bounded to 256 MiB.
Every scalar and vector is decoded explicitly into the inherited engine types;
no disk structure aliases engine storage. RTCW's additional ground-steepness
float is retained in a separate per-world array. Contents and travel-type bits
are preserved for the future RTCW routing consumer; they are not reinterpreted
as Quake's differing content flags.

In-progress file handles, input storage and each converted allocation belong
to the world before subsequent allocations. Shutdown can therefore reclaim a
partial load even when an engine error bypasses C++ stack unwinding. Normal
failures close the file and discard partial data immediately. Replacement
arrays use reclaimable heap allocations, while inherited link/scratch
allocation policies are retained.

Loading does **not** claim initialized routing. It preserves the precompiled
RTCW reachabilities without invoking Quake's reachability generator, and AAS8
writing is explicitly unavailable. RTCW frame initialization, movement rules,
ground-steepness travel costs, route tables, entity updates and the remaining
navigation/AI imports still need implementation and runtime evidence.

Loading also exposed an inherited link-pool bounds bug: capacities zero and one
indexed before or after the allocation, and the old float-to-int conversion was
unbounded. A capacity of one now forms a valid single-element free list;
nonfinite, nonpositive and oversized values fall back to 6,144. The maximum is
1,048,576 links. Positive fractional values retain truncation. The final code
decodes the bounded IEEE-754 significand with integer operations, because
Linux x86 sanitizers demonstrated that fast-math can speculate a NaN conversion
past an ordinary floating-point range check. Shutdown also clears free-list
and count state.

## Validation

`tests/rtcw_aas_load_tests.py` executes the production loader and map lifecycle
with synthetic Q3/AAS8 files. It covers both, either and neither RTCW hull,
malformed companion hulls, map reloads, shared BSP ownership, preserved entity
storage, short reads, every truncated input length, identity/version/checksum
errors, signed and unsigned checksum text, path bounds and allocation failure
at each load stage. The bootstrap fixture also exercises the real native/QVM
import with bounded names, null, return-code preservation and invalid extents.

`scripts/probe_rtcw_aas_load.py` reads all 70 installed AAS files in memory and
loads each into both worlds under both RTCW profiles. It compares every
converted record against the pinned disk layout, including the separate
ground-steepness values and the 16-bit travel times. The two nonsemantic
padding bytes in each reachability are excluded. There are 280 world loads
and 59,894,232 record comparisons per architecture run. This complements the
earlier original/iortcw layout and point-query comparisons; it does not compare
live AI decisions against the original engine.

Six focused tests pass on Windows x64 (6.524 s), Windows x86 (8.399 s), and
Linux x86 with ASan/UBSan and leak detection (31.974 s). The link-pool fixture
uses `-O2 -ffast-math`, including direct nonfinite values, boundary capacities,
fractional values and repeated allocation/reset/free cycles. Logs are
`aas-load-final-{x64,x86,sanitizers}.log` in `.tmp/rtcw-manager/`.

All three installed-file CPU reports pass 70 files and 59,894,232 comparisons:
`aas-load-installed-{x64,x86,sanitizers}.json`. The earlier failed sanitizer
run, `aas-load-sanitizers.log`, records the floating-point conversion issue;
the final integer conversion supersedes it. Loader behavior is unchanged by
that link-pool fix.

Both final Windows x86 client/server builds pass. The experimental client is
SHA-256 `d3afa6b15cdec011ed5f0fb411d004124f5f2398d2a616756c9ce4746ee70855`;
the default client is
`27582490b5afb4c9401ec4d839635d5efae8de061873a59bae2ba9817dca1aa1`.
Build logs are `aas-load-final-{dev,default}-build.log`.

`aas-load-final-map/manifest.json` records the native SP/MP map runs through the
same experimental executable. SP loads 5,026 areas / 7,753 reachabilities for
`escape1_b0` and 3,886 / 6,239 for `escape1_b1`, then reports unsupported import
311. This remains a failed map boot, with no SP gameplay capture. MP reaches its
textured deployment menu and decoded briefing movie and exits normally. The
probes use windowed private desktops without switching desktops or injecting
input, read-only installed assets and isolated writable homes.

Six refreshed Q3/QL scene probes pass with the final default executable in GLX,
Vulkan and RTX raster. All six engine captures were inspected, and their module,
renderer and windowed-mode checks pass (`aas-load-final-live/manifest.json`).
Q3 shows its world, weapon and HUD, including successful AAS initialization and
shutdown in the log. QL shows its native join UI and world; no match was joined.
The final MP engine capture was also inspected and its manifest records the
passing visual result. Its deployment UI obscures the world.

The final full suite passes 111 Meson checks with two skips and no failures.
Its Python aggregate passes 1,379 tests in 327.824 seconds, with 11 skips.
The logs are `aas-load-final-full-tests.log` and
`aas-load-final-full-testlog.txt`. Executable hashes still match both final
runtime manifests, and `git diff --check` passes.

## Next boundary

Import 311 requests the next BSP entity; imports 312–315 request string, vector,
float and integer properties. These are observations from the original server
dispatch, not completed RTCW adapters. The shared BSP table is now present, but
the checked boundary must still preserve return values, output extents and
missing-property behavior. Source inspection also shows that the inherited
vector getter uses `Com_Split`/`Q_atof`, whereas original RTCW uses three `%lf`
conversions with initially zero components. Its numeric parsing must therefore
be compared explicitly. The inherited entity iterator's unconditional `ent++`
also needs an overflow check before accepting arbitrary module integers.

## Provenance

The implementation is independently authored from layout and lifecycle facts
in the original [SP bot library](https://github.com/id-Software/RTCW-SP/tree/master/src/botlib),
[MP bot library](https://github.com/id-Software/RTCW-MP/tree/master/src/botlib)
and [iortcw](https://github.com/iortcw/iortcw). These references are GPLv3;
their implementation text is not incorporated into the GPL-2.0-or-later engine.
No proprietary navigation files are staged or retained by the CPU probe.
