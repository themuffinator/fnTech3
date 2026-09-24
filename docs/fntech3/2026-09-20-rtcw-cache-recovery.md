# RTCW navigation-cache recovery

RTCW no longer refuses navigation initialization when a map's routing cache is
missing, damaged, or belongs to another navigation file. It rebuilds the cache
from the loaded AAS8 navigation and BSP collision services, saves it in the
writable home, and can load it on subsequent starts. This closes the
missing-cache generation/writing implementation gap; it does not close the
campaign, collision-parity, or multiplayer gameplay gates.

The [machine-readable record](2026-09-20-rtcw-cache-recovery.json) records
the binaries, commands, reference identities, results, and remaining differences.
The source base is `9e1841d`, with this task's uncommitted changes. No retail
installation was modified, and no modules or assets are committed.

## Implementation

- Validate the map's cluster/portal topology independently of a cache file,
  before allocating or indexing its routing tables. An invalid cache is a
  warning followed by regeneration; invalid navigation still fails explicitly.
- Generate per-area waypoints and visibility with profile-specific behavior.
  SP drops a normal-presence hull against solid, player-clip and monster-clip
  contents and uses the PVS before visibility traces. MP drops against solid
  contents, traces both directions, and does not use the PVS.
- Preserve the original SP and iortcw SP generator's asymmetric visibility:
  its reverse-result table never fills off-diagonal bits, so the generated
  lower triangle stays zero. Existing cache visibility is loaded unchanged.
- Populate route caches in goal/source order with the retail generation flags
  `0x01380fbe`. Both retail-generated `escape1` cache files use this value.
  This also agrees with MP and iortcw; the released original SP header's
  unparenthesized `TFL_DEFAULT` macro would retain the jump-pad bit. Retail
  observations take precedence over that header discrepancy.
- Keep all allocations and partially generated visibility rows owned by the
  selected navigation world. Trace failures and allocation failures remain
  safe to shut down. Internal generation can calculate routes without
  reporting `AAS_Initialized` to the game before initialization completes.
- Write explicit little-endian RCD15 (SP) or RCD12 (MP) records. Obsolete
  process pointers and padding are zero, and native structure sizes never
  enter the file. The checked reader validates the encoded image before any
  destination file is opened.
- Keep the in-memory result when writing is denied or short. A partial file
  fails validation and is regenerated at the next start. Writing uses the
  engine's normal home-path filesystem policy.

The map still needs valid, precompiled AAS8 data; this is not an AAS compiler.
Cold generation has the original quadratic area-pair work and can take longer
on large maps. Existing valid caches continue through the loaded-cache path.

## Automated and source evidence

`tests/rtcw_routecache_build_tests.py` executes the actual generator, route
queries, serializer and loader. It checks both profiles, malformed/missing
caches, two-world ownership, repeated replacement, every allocation failure
in the synthetic generation path, invalid trace results and topology,
read-only/short-write recovery, and validation before overwriting a good file.

Fourteen focused navigation tests pass on Linux x86 with AddressSanitizer and
UndefinedBehaviorSanitizer. The Windows x64 focused run also passes.

`scripts/probe_rtcw_cache_generation.py` compiles the unmodified original and
iortcw visibility/compression functions in disposable fixtures with their
licence notices. Each of the four references agrees on **2,455,116** waypoint
coordinates and visibility decisions per target, including missing ground,
start-solid, a landing outside its area, PVS rejection, blocked rays, absent
rows and run-length encoding. Trace/PVS call counts also agree. Windows x86,
Windows x64 and sanitized Linux x86 all pass: **29,461,392** comparisons.
These controlled collision responses establish generator behavior, not
original-engine BSP collision equivalence.

Strict Windows x86 development and default builds pass for the client,
dedicated server and all three renderer modules. The complete development
suite passes **120 Meson tests with two skips and no failures**, including
**1,827 Python tests with 72 skips** in 625.194 seconds. Binary identities and
the same-binary profile startup results are recorded in the companion JSON.

## Live cache interchange

`scripts/probe_rtcw_cache_recovery.py` uses the installed retail SP game module
and maps. Each process has an isolated home, runs dedicated on an unswitched
private desktop with windowed rendering forced, and receives no input events.
Sound is disabled. All 15 processes exit normally, and the installation
inventory is unchanged. Generated caches and extracted native module copies
are removed after recording hashes and logs.

| Map | fnTech3 regenerates both worlds | fnTech3 reloads its files | Retail reads fnTech3 files | Retail generates, then fnTech3 reads |
| --- | --- | --- | --- | --- |
| `escape1` | Pass | Pass | Pass | Pass |
| `baseout` | Pass | Pass | Pass | Pass |
| `boss1` | Pass | Pass | Pass | Pass |

Both files are initially replaced with invalid headers in the isolated home.
Cold runs rebuild both worlds; reload/interchange runs initialize both worlds
without regenerating or changing the supplied files. Filesystem diagnostics
identify the home files retail actually reads, excluding an accidental fallback
to the installation's prebuilt caches. This proves RCD15 interchange and map
initialization, not sustained AI or combat.

The same development client draws Q3 `q3dm1`, QL `campgrounds`, the RTCW SP
`escape1` briefing, and the RTCW MP `mp_beach` spectator world, using the engine's
own PNG screenshots. All four captures were inspected. The same dedicated
server starts Q3, QL and RTCW MP and answers loopback `getstatus`; all seven
processes exit normally. No mouse/keyboard input or OS screen capture is used.

## Remaining differences and gates

A separate comparison of independently generated `escape1` caches found:

| World | Route records | Different records | Different visibility rows | Different waypoints | Largest coordinate difference |
| --- | --- | --- | --- | --- | --- |
| `escape1_b0` | 3,970 | 2 | 0 | 23 | 0.027099609375 units |
| `escape1_b1` | 3,254 | 1 | 0 | 12 | 0.001251220703125 units |

Headers and record keys agree. Differences exclude obsolete pointers, padding
and timestamps. Differing travel times differ by one unit, and two reachability
choices differ in world zero. This is not byte-for-byte or behavioral parity.
The established collision/arithmetic parity gate stays open; this task does not
attribute those differences to a specific cause without further evidence.

MP cache generation and RCD12 writing have executable fixture evidence, not
an installed MP bot/cache runtime result. Full campaign transitions, sustained
AI/combat, MP classes/objectives/peers, and broader renderer/audio/input parity
remain open in the [compatibility ledger](COMPATIBILITY.md). RTCW stays behind
`-Drtcw-development=true`.

An unrelated diagnostic issue was observed: `fs_debug 1` with `logfile 2` can
reopen and truncate `qconsole.log` during shutdown, leaving only the final
configuration-write message. The initial verbose probe therefore could not
validate its logs despite successful cache creation. The repeated runs use
`logfile 4` (append/flush), preserve the complete logs and pass. That logging
issue remains unfixed.

Retained logs, engine screenshots and reports are under
`.tmp/rtcw-cache-recovery/`; proprietary cache outputs and native module copies
are removed. Source reference functions existed only in automatically deleted
fixture directories. See [credits](../../CREDITS.md) for provenance.
