# Enemy Territory routing cache limits and eviction

This round resolves three source-observed ET routing gaps. It records Source
and Unit evidence; it does not establish live ET navigation or close a profile
gate. The stock installation used in earlier rounds has no AAS/RCD files.

The original [ET source release](https://github.com/id-Software/Enemy-Territory/tree/40342a9e3690cb5b627a433d4d5cbf30e3c57698)
sets the default routing cache budget to 16,384 KiB, captures
`bot_frameroutingupdates` during map routing initialization, and defers an
ordinary area-cache miss only when the frame's eligible-link update count is
strictly greater than that limit. Cache hits remain available, and portal
construction forces its dependent area caches to complete. Server setup supplies
1,000 for multiplayer/co-op and 9,999,999 for a game type classified as single
player, preserving an existing cvar. The existing game-info classification is
used, including installation-defined single-player types.

fnTech3 previously gave ET the Quake III 12 MiB default and did not defer its
ordinary misses. Its ET path now samples both limits at map initialization,
receives the server's selected default or user override, and follows the strict
threshold. A new field belongs to the internal AAS world, with no module or file
record layout change. Q3, QL and Holomatch retain their cache setting behavior;
RTCW keeps its existing 4 MiB default and 100-update threshold.

The original ET eviction pass selects the oldest ordinary area cache by table
order and also selects the oldest portal cache. It retains area caches whose
goal is itself a portal. fnTech3 previously selected one entry from Quake III's
LRU list; ET now uses the existing independently implemented RTCW selection,
including its tie order and two-cache pass. Existing ownership, accounting and
cleanup are retained.

The setting converters preserve finite nonnegative values, truncate before
converting KiB to bytes, and allow zero. Nonfinite, negative and overflowing
numbers use ET's defaults, so a float-to-int conversion cannot overflow or depend
on fast-math assumptions. This invalid-setting fallback is an engine robustness
policy, not an observation of retail behavior. The original source remains
GPLv3 reference material; no implementation text was incorporated.

Four new production-routine regressions pass with `-O2 -ffast-math`: defaults,
overrides, map snapshots and numeric bounds; threshold equality, cache hits,
deferred misses and forced portal dependencies; both eviction kinds, table-order
ties and protected portal targets; and server defaults, existing overrides and
disabled/uninitialized bot-library setup. The existing RTCW route-query suite
passes both tests across affected profiles, and both ET generated-area-cache
tests still match the original executable fixtures. Three RTCW initialization
and routing tests and five ET travel-cost tests also pass, for 16 targeted tests
with no failures or skips. The JSON companion records
commands and outcomes. The full build, complete suite and runtime controls are
recorded separately in the [round record](2026-10-02-compatibility-round.md).

A separate unresolved tactical navigation gap was found by source inspection.
`wolfet_routing.hpp`'s common `Work::Push` refuses a record carrying `inlist`,
while the original `AAS_AvoidDangerArea` deliberately omits that check to avoid
stale markers excluding routes, and `AAS_Retreat` also queues without it.
fnTech3's early-returning tactical searches can leave pending markers, making
the next avoidance/retreat search potentially omit an area. No tactical
implementation change or live validation is included here. Loaded ET navigation,
retail cache-query/eviction comparisons and live bot paths remain open.

No presentation path changes. ET continues to use fnTech3's existing rendering,
spatial audio, input and interface features; no retail-exact option is added.
