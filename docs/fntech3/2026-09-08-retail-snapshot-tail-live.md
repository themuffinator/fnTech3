# Live retail snapshot-tail sampling

A retail Windows x86 QL client supplied 490 distinct valid history records
across all 32 slots during an authenticated connection to fnTech3 and a
Campgrounds-to-Overkill map change. Every sampled 88-byte tail was zero.
The [record](2026-09-08-retail-snapshot-tail-live.json) pins executable and
asset hashes, engine commit, commands, counts and private evidence hashes.

The [producer inspection](2026-09-08-retail-snapshot-producer.md) identifies
history RVA `0x1072874`, record stride `0x298`, and player-state offset `0x3c`.
Four instruction-operand checks were repeated against the pinned executable.
The sampler resolves the actual loaded executable base through PSAPI before
reading record metadata and bytes at record offset `0x234`, length `0x58`.
It reads no known player-state prefix. Metadata is the first 16 record bytes;
validity must equal one. Metadata is read before and after the tail, and a
changed pair is rejected. Slot plus metadata identifies distinct observations.
There were no failed reads or detected metadata changes. This check reduces
races but does not make the unsuspended process an atomic snapshot.

Both processes exited normally with code zero after 34.625 seconds. The server
recorded two game initializations, both map-change markers and one successful
platform authentication. Engine screenshots before and after the transition
show first-person worlds and HUDs. Sampling spans the session; no exact
per-map partition was recorded, so counts must not be attributed individually
to either map. Client input and audio were disabled, rendering was windowed
on an unswitched private desktop, and isolated writable homes left the retail
installation unchanged. No process writes, input injection or OS capture were
used. Raw logs, memory-derived tails and retail screenshots are not committed;
the retained aggregate hash equals SHA-256 of 88 zero bytes.

This adds live evidence to the controlled decoder's null-baseline result.
It does not establish the tail's meaning, prove all retail states remain zero,
or exclude other writers. The decoder's demonstrated inheritance of supplied
nonzero bytes remains relevant. The module ABI gate stays partial, and the
tail must not be called padding. No engine behavior changed and no full-suite
rerun is claimed for this evidence-only checkpoint. Documentation packaging
and link validation passed. No external implementation was incorporated.
No new unrelated defect was established; existing optional service, UI and
asset-warning limitations remain open.
