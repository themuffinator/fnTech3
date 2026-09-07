# QL native snapshot marshalling — 2026-09-07

The QL native snapshot boundary previously copied `playerState_t` and assigned
`entityState_t` directly into retail memory. Their current layouts match the
source-known QL fields, but engine-internal changes could silently alter this
external contract. The snapshot now owns frozen `fntech3_ql_entityState_t`
elements and uses explicit field conversion for entities, trajectories and the
known player-state prefix.

The player-state adapter writes only the frozen 504-byte prefix into the
592-byte retail envelope. The remaining 88 bytes are zeroed on every call,
preserving current behavior without treating unknown bytes as padding or
claiming to know their meaning. Existing snapshot offsets, capacity, total
size and metadata handling remain unchanged. Q3 keeps its separate adapter.
No external implementation was incorporated.

Validation at `8d813ea` plus this patch:

- Windows MSVC x86 client/dedicated build passed. ABI layout, ABI adapter and
  server-game-data executables passed (3/3).
- Native Linux i686 ABI adapter build/test passed (1/1).
- Adapter tests compare every current QL prefix/entity byte against the
  pre-change layout using distinct finite values, exercise signed movement
  bytes, check zero tail bytes, and check guards around destination buffers.
- Native bridge source checks passed (31/31), including use of the adapters
  at the production snapshot boundary.
- The same rebuilt Windows x86 client passed Q3 QVM and QL native-module local
  pure sessions, reached `CS_ACTIVE`, wrote engine screenshots and exited
  normally. Screenshots were inspected for world/UI/console rendering.

The [runtime record](evidence/2026-09-07-ql-snapshot-marshalling.json) preserves
binary hashes, commands, generated configs and decisive log lines. Probes used
isolated homes, an unswitched private desktop, windowed GLx, disabled input
capture/audio/Steam/WebUI and engine `screenshotPNG`; no input injection or OS
screen capture was used. Retail assets and modules are not committed.

This is bounded containment and non-regression evidence. It does not resolve
the unknown QL tail, prove all snapshot failure paths, complete the remaining
native-import audit, or close retail interoperability. Existing optional
resource warnings and incomplete clean-CI evidence remain open.
