# RTCW SP cast lifecycle

Status: **partial Source / Unit / Build / Fixture evidence**. SP cast clients now skip snapshot
construction and transport timeout accounting. Their disconnects suppress
player-facing notices while retaining module callbacks and resource cleanup.
The runtime and release gates remain open.

## Behavior

The SP rules use the existing checked game-entity adapter. Raw RTCW entity
offsets and flags never enter the shared server code. Q3, QL and RTCW MP keep
their existing snapshot, timeout and disconnect paths; ordinary SP bots
still build snapshots for their own queries.

Clock correction and expired-zombie reclamation occur before the cast
timeout exemption. SP also postpones normal timeout accounting for an absent
game entity, matching the original campaign policy. The existing short
admission deadline still reclaims abandoned network connections with no
bound entity. Platform authentication failures and the empty-server exit
timer retain their authority.

A cast disconnect still closes downloads, demo recording, network queues,
statistics and any real platform session, calls the game module, frees bot
slots and clears user information. Its broadcast and direct notices each
read the current cast classification. That second read matters when the
game callback changes the flag or clears the entity. Silent drops retain
their existing shared semantics.

## Evidence

`tests/rtcw_cast_lifecycle_tests.py` executes the actual snapshot sender,
timeout loop, client destructor and disconnect function against all four
frozen game-entity layouts. It covers cast and ordinary bot/human addresses,
private entity strides, missing and invalid bindings, message overflow,
callback flag changes, repeated zombie drops, authentication, short admission
deadlines, six-frame timeout hysteresis, clock correction, reclamation and
empty-server timers. The existing cast command/configstring fixture also
passes.

Evidence is retained under `.tmp/rtcw-manager/`:

- `cast-lifecycle-focused-x86.log`: four tests pass in 14.817 seconds.
- `cast-lifecycle-focused-x64.log`: four tests pass in 8.117 seconds.
- `cast-lifecycle-focused-sanitizers.log`: four tests pass on Linux x86 with
  ASan, UBSan and leak detection in 20.305 seconds.
- `cast-lifecycle-{dev,default}-build.log`: Windows x86 client/server and
  GLX, Vulkan and RTX renderer targets build successfully.
- `cast-lifecycle-full-suite.log` and `cast-lifecycle-testlog.txt`: 111 Meson
  checks pass with two skips; 1,409 Python tests pass in 313.724 seconds with
  11 skips.
- `cast-lifecycle-map/manifest.json`: SP and MP boot cleanly through the same
  experimental executable. SP retains all 29 soldiers and the ready arrow;
  MP passes local pure admission and displays its deployment UI/movie.
- `cast-lifecycle-live/manifest.json`: all six Q3/QL renderer checks pass
  with personally inspected engine captures. QL remains at its join screen.
- `cast-lifecycle-source-identities.json`: source, reference, build and
  verification identities for this dirty-worktree checkpoint.

Runtime checks used isolated homes, 640×480 windowed mode, disabled input,
muted/off audio and unswitched private desktops. Installed files are unchanged;
temporary homes are removed. No playable RTCW world is claimed.

Experimental executable SHA-256:
`d89fabd4e179b503c30614e3e0103405d5b42c2b311d7a16e962e3c1cc2a8a0c`.
Default executable SHA-256:
`f64ac8f1724b83e1f8177702d804fb9289ca109e2a98766f520c6ed400baae5a`.

## Provenance and limits

Behavior was checked against the original
[SP server](https://github.com/id-Software/RTCW-SP/tree/master/src/server),
[MP server](https://github.com/id-Software/RTCW-MP/tree/master/src/server) and
[iortcw](https://github.com/iortcw/iortcw) snapshot, timeout and disconnect
functions. These guards and tests are independently authored; no GPLv3
implementation text is incorporated into the GPL-2.0-or-later engine.

The preceding [AAS/tag checkpoint](2026-09-16-rtcw-aas-tags.md) establishes SP
ready-screen initialization. Campaign play, sustained cast behavior,
navigation movement/routing, save/load, rendering/audio parity and peer
interoperability are still unverified. See the [gate ledger](COMPATIBILITY.md).

No new unrelated issue was found. The inherited Q3/QL routing-cache reader
bounds audit remains open.
