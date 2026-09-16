# RTCW AI entity and client allocation checkpoint

RTCW SP and MP now share checked entity updates and profile-owned AI client
allocation in the same executable. SP passes the previous import-207 boundary
and allocates its first cast client. The next observed missing import is 546,
`BOTLIB_AI_ALLOC_GOAL_STATE`. Playable SP gameplay is still unavailable.

## Contracts and ownership

`BOTLIB_UPDATENTITY` (207 in both modes) accepts a frozen 112-byte record.
The boundary copies unaligned native/QVM inputs, validates the entire QVM span,
integer width, game entity range, finite vectors and translated BBOX bounds,
then marshals fields into an engine record. Null input is rejected; published
RTCW dereferences it and defines no removal contract. Internal Quake entity
removal still works. BSP bounds are checked before publishing RTCW state or
removing existing links.

Both RTCW navigation graphs share world zero's entity records, initialization
status, limits and first-frame relink decision. Time and new area links still
come from the selected navigation world. Each record now retains the owner of
its area links, allowing movement, invalidation and internal removal to unlink
from the correct heap and preserve the caller's selected world. Map replacement
clears those owner references before disposing the heaps. Failed entity-table
replacement leaves a null allocation instead of a freed pointer.

The original RTCW update leaves the event field unchanged and does not replace
BBOX angles. Both behaviors are preserved; Q3/QL keep their existing event and
angle updates. Entity enumeration also handles `INT_MAX` without signed overflow.

The game manager's bot policy now owns the first allocatable server client slot,
bot rate and snapshot-interval initialization. RTCW starts at slot 1 and uses
rate 16384 without changing the existing snapshot interval. Q3/QL retain slot 0,
rate 0 and their frame-rate-derived interval. Allocation/free use the existing
profile-aware entity adapters, preserving private module tails and unknown
server-flag bits. Failed entity validation does not publish a partially changed
client state. RTCW allocate/free imports work independently of bot-library setup.

## Source and automated evidence

The authoritative behavior observations are from id Software's
[SP entity implementation](https://github.com/id-Software/RTCW-SP/blob/master/src/botlib/be_aas_entity.c),
[MP entity implementation](https://github.com/id-Software/RTCW-MP/blob/master/src/botlib/be_aas_entity.c),
and their server bot allocators. [iortcw](https://github.com/iortcw/iortcw)
provides supporting comparison evidence. These references are GPLv3; their
unchanged functions and notices are confined to disposable comparison fixtures.
The engine changes are independently authored against the inherited
GPLv2-or-later implementation.

`scripts/probe_rtcw_contracts.py` now checks the bot entity record against all
four reference header sets on Windows x86 and x64. Totals are 2,067 assertions
per SP reference and 2,057 per MP reference.

`scripts/probe_rtcw_bot_entities.py` compares 153,334 updates, complete entity
records and model/link callback arguments per reference. Original SP, original
MP, iortcw SP and iortcw MP pass on Windows x64, Windows x86 and Linux x86 with
ASan/UBSan and leak detection. Synthetic model bounds isolate update behavior;
the probe does not establish retail routing or collision parity.

The 11 focused tests pass on Windows x64 (14.517 seconds), Windows x86
(21.560 seconds) and Linux x86 sanitizers (68.802 seconds). They exercise actual
two-world link heaps, shared queries, relink ownership, lifecycle failures,
malformed native/QVM input, actual server entity conversion, slot exhaustion,
and Q3/QL policy preservation.

The complete Meson suite passes 111 checks with two expected skips. Its Python
discovery suite runs 1,399 tests in 392.091 seconds with 11 expected skips and
no failures. Results are retained in `entity-final-suite.log` and
`entity-final-testlog.txt`. `git diff --check` also passes.

Evidence is under `.tmp/rtcw-manager/`: `entity-layout-{x64,x86}.json`,
`entity-reference-*.json`, `entity-final-{x64,x86,sanitizers}.log`, and
`entity-source-identities.json`. Reference comparison reports pin their source
and implementation hashes. Source/Unit evidence does not close gameplay gates.

## Builds and runtime observations

Both Windows x86 client/server distributions and all three renderers build.
The default executable SHA-256 is
`790353e9ed8f08532d373835517bdfdc925fa2c625641e480ba0d2d8b4af810a`;
the RTCW-development executable is
`fb6175f50c5487e7ee642983581b04093e1cf6ae7b5e3c21782552b1acefa084`.
This is a dirty-worktree checkpoint based on commit
`c75573c7375e44c908256020ada2d7e12970cacf`, not a committed release claim.

`entity-final-openal/manifest.json` records installed SP `escape1` with pure
off and MP `mp_beach` with pure on, using the same development executable.
Both initialize OpenAL. SP receives its first snapshot and reaches missing
goal-state allocation import 546; it has no playable-map capture. MP exits
cleanly after an engine PNG capture of the textured, readable deployment UI
and briefing movie. That image was personally inspected. AI shutdown during
SP's unsupported-call error remains part of the incomplete campaign path.

`entity-final-live/manifest.json` records the default executable running Q3
`q3dm6` and QL `campgrounds` under GLx, Vulkan and RTX raster. All six pass with
personally inspected engine PNGs and expected module/renderer identity. The
QL join menu was not entered; these are startup/rendering checks.

All live runs used private, unswitched desktops, 640×480 windowed rendering,
disabled input and isolated writable homes. Installations were unchanged and
temporary homes removed. No OS capture or input injection was used.

## Remaining work

Goal and movement state allocation/free/reset need RTCW adapters and SP's
128-client capacity; the inherited pools currently use 64. Remaining AI queries,
movement/routing, missing-cache generation, campaign/save-load transitions,
entity-stream audio and multiplayer interoperability remain open. RTCW remains
development-gated.

The previously recorded unrelated Q3/QL routing-cache reader bounds audit
remains open; RTCW uses its separately checked cache reader.
