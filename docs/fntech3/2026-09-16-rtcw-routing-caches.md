# RTCW routing-cache decoding and ownership

The bot library now has a profile-selected RCD reader and native record loader.
It reads SP version 15 and MP version 12, validates them against the selected
AAS world, and owns the converted caches, visibility and waypoints per world.
SP still stops at game import 205 (`BOTLIB_START_FRAME`): frame initialization,
travel rules and routing queries remain unfinished. This checkpoint does not
claim campaign, live AI or MP bot compatibility. RTCW remains gated in default
builds. Evidence describes the dirty tree at
`c75573c7375e44c908256020ada2d7e12970cacf`, not that commit alone.

## Format and validation

`code/compat/rtcw_routecache_format.hpp` reads little-endian fields independently
of alignment and host pointer size. The header is 36 bytes. Each cache contains
a 52-byte legacy structure plus three bytes per routing entry. Meaningful
16-bit travel times start at byte 48; reachability bytes start at byte 52 plus
twice the entry count. The serialized pointers and unused trailing words are
never interpreted as addresses or additional travel entries.

The reader checks identity, selected version, area/cluster counts, all three
map CRCs, record sizes and extents, finite bounded origins/waypoints, finite
clock values and start costs in `[0, 65535]`. Files are capped at 256 MiB.
It rejects extra trailing bytes and preserves the destination view on failure.
The AAS loader retains CRCs of the original little-endian area, cluster and
reachability lumps, including disk padding. This makes CRC agreement independent
of native structure padding and host byte order.

Topology validation checks cluster membership, unique local area indices,
portal sides, expected entry counts and used reachability offsets. Index-table
allocation is bounded by the actual area/portal counts. A goal's self-distance
may legitimately have no outgoing edge. Unused entries with a zero travel time
do not interpret their reachability byte.

Visibility is a sequence of value/count byte pairs. Nonzero values expand to
one, matching the original reader; absent rows expand to zero. Counts must be
nonzero and sum exactly to the area count. Truncated, overlong and zero-repeat
rows fail before modifying caller output. Decoder input and output are separate
buffers, as in the original library.

## Native ownership

`code/botlib/rtcw_routecache_loader.hpp` selects the SP/MP format before decoding.
It validates the entire input before replacing existing records. Invalid or
missing input leaves existing cache tables intact. Successful conversion links
each record into the engine's real cluster/portal tables and cache list, with
native sizes and allocation accounting. Serialized process pointers never enter
these lists.

Compressed visibility rows, decompression scratch and waypoints belong to the
selected navigation world. Every allocation is published before another engine
allocation can fail, so shutdown can release partial conversion even when an
engine error bypasses C++ stack unwinding. Topology scratch is released before
engine allocations begin. Both map replacement and shutdown retire these
resources. Existing area blocking invalidates the converted cache links.

The shared allocator now increments cache accounting only after successful
allocation. Q3/QL keep their existing reader. RTCW cache writing is explicitly
unavailable, preventing the inherited Q3 writer from producing another game's
file format. Missing-cache generation and writing remain work to do.

## Evidence

All 70 installed SP RCD files match their companion AAS counts and CRCs:
64 in `pak0.pk3` and six in `sp_pak4.pk3`, totaling 134,377,347 uncompressed bytes.
Inputs stay in memory and no retail data is committed.

`scripts/probe_rtcw_routecache.py` compares unchanged original SP/MP and iortcw
SP/MP readers in disposable fixtures. Each reference matches 172,264 cache
records, 38,733,927 travel-time/reachability pairs and 48,823 visibility rows
(207,666,441 expanded bytes). All four references pass on Windows x86 and
Linux x86 with ASan/UBSan and leak detection. On Windows x64 both iortcw readers
pass; the original pointer-dependent implementations are deliberately confined
to x86. These are record/visibility comparisons, not route-choice comparisons.

`scripts/probe_rtcw_routecache_load.py` executes the real engine loader against
all 70 files in both worlds. The MP selector uses a synthetic version-12 header
over the SP corpus; this is explicitly not an installed MP-cache observation.
Each architecture checks 280 world loads, 689,056 native cache records,
154,935,708 routing entries and 950,468 visibility/waypoint records. Reports are
`routecache-load-installed-{x64,x86,sanitizers}.json`; reference reports are
`routecache-reference-{x64,x86,sanitizers}.json`, under `.tmp/rtcw-manager/`.

Six focused reader/loader/AAS/world tests pass on Windows x64 (8.025 seconds),
Windows x86 (9.554 seconds) and Linux x86 with sanitizers (41.215 seconds).
They cover malformed sizes, every truncation, partial reads, invalid numeric
fields and RLE, pointer/padding independence, unaligned input, profile/version
selection, topology errors, both worlds, replacement, blocking, map changes and
every engine allocation failure during initial and replacement loading. The
fixtures run at `-O2 -ffast-math`. Logs are `routecache-load-{x64,x86,sanitizers}.log`.

Both default and experimental Windows x86 client/server builds pass, with all
three renderer targets. Client SHA-256 values are:

- Default: `549aa3aecdb2aa9401c9b7bae04e4e0de4b9a13de59e596d7d1642425d7bf2b1`.
- Experimental: `d6476f54f394218f208146fc26bced1560cd3e93247d381d8fa304a58bf2628d`.

The complete suite passes 111 Meson tests with two skips, including 1,383 Python
tests in 340.521 seconds with 11 skips. Logs are `routecache-dev-build.log`,
`routecache-default-build.log`, `routecache-full-tests.log` and
`routecache-full-testlog.txt`.

All six inspected Q3 `q3dm6` and QL `campgrounds` engine screenshots pass across
GLX, Vulkan and RTX: textured maps, Q3 weapon/HUD and QL join menus are present.
The experimental RTCW MP map boot exits normally and its inspected GLX capture
shows the textured deployment menu and briefing movie. SP loads both `escape1`
AAS8 hulls and still reports unsupported import 205; it has no map-rendering
evidence. The new cache loader is exercised by the CPU probes above, not by
that blocked SP frame call.

`routecache-live/manifest.json` and `routecache-map/manifest.json` record exact
commands, installation/module identities, executable/renderer hashes and
captures. Probes used windowed unswitched private desktops, disabled input and
audio, isolated writable homes and unchanged installations. These are module
and rendering smoke checks, not gameplay, peer interoperability or pixel parity.

## Next boundary and unrelated findings

The frame call must visit both worlds and initialize RTCW travel flags, slope
costs, reversed links and routing scratch. It must also handle absent caches
honestly. Forwarding it to Q3 initialization would apply different contents
flags, rounding and movement rules. Cache/visibility loading alone does not
make that initialization correct.

The inherited Q3/QL cache reader still trusts serialized native structure sizes
and pointers. That pre-existing reader is outside this RTCW decoder change and
needs a separate format/bounds audit; the new RTCW path does not use it.

## Provenance

The production implementation is independently authored from format and behavior
facts in the original [SP bot library](https://github.com/id-Software/RTCW-SP/tree/master/src/botlib),
[MP bot library](https://github.com/id-Software/RTCW-MP/tree/master/src/botlib)
and [iortcw](https://github.com/iortcw/iortcw). Reference GPLv3 functions and
notices exist only in disposable comparison fixtures. No GPLv3 implementation
text, retail modules or assets are incorporated into the engine. The CRC
calculation uses the same standard CRC-CCITT parameters as the inherited
GPL-2.0-or-later bot library.
