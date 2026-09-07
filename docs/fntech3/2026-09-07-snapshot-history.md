# Snapshot history validation — 2026-09-07

The legacy, Q3 and QL native snapshot readers now share validation before
writing module-owned memory. Signed snapshot-number subtraction could overflow
on malformed requests, and ring-slot validity alone did not prove that the
slot contained the requested snapshot. The shared lookup uses the existing
31-bit sequence helpers, checks exact slot identity and validates the entity
history range with unsigned counter distance. Negative requests, stale slots,
negative counts, expired entity history and counts extending beyond recorded
history are rejected. Future requests retain the drop-error behavior.

Entity-array indexing also uses unsigned addition before applying its ring
mask, avoiding signed overflow in the three readers. No external layout,
successful snapshot marshalling or profile selection changes.

Validation at `7fc00d5` plus this patch:

- An executable test extracts the production lookup and covers valid history,
  `INT_MIN`, negative requests, future requests, stale slot identity, invalid
  flags/counts, history boundaries and both sequence/entity-counter wraps.
  It passed on Windows and native Linux.
- Snapshot/history and native bridge tests passed together (32 tests).
- Windows MSVC x86 client compilation passed. Both profiles passed local pure
  admission, reached active state, produced inspected engine screenshots and
  exited normally. The [runtime record](evidence/2026-09-07-snapshot-history.json)
  contains hashes, commands, configs and log excerpts. Isolated homes,
  windowed GLx, an unswitched private desktop and disabled input capture were
  used; no user input injection or OS screen capture occurred.

This is reader-boundary evidence, not complete network-wrap sign-off. The
parser still increments and indexes its entity-history counter with signed
arithmetic, and that producer path needs a corresponding audit. Broader
malformed packet/demo coverage and other compatibility gates remain open.
Existing shader/model warnings are unchanged. Separately, Vulkan verification
at `8d813ea` completed successfully after the FreeType endpoint change
([run 34163993922](https://github.com/themuffinator/fnTech3/actions/runs/34163993922));
that build result does not establish renderer runtime compatibility.
