# Recording and replaying an active QL match

At clean commit `2d66a79`, the current Windows x86 shared client records a
joined campgrounds/ffa match with four retail bots, then successfully plays
that recording in both normal and timedemo modes. The
[record](2026-09-08-ql-active-demo.json) pins executable and fixture identities,
commands, sanitized configuration, log hashes, screenshots and completion
results. The client binary matches the one validated with the
[QL command-codec correction](2026-09-08-retail-usercmd-key-mask.md).

The recording extends the [active-match probe](2026-09-08-ql-active-match.md).
Its engine command sequence joins FFA, waits for the joined state, stops any
pre-existing recording, records `codec-match`, stops recording, takes an engine
screenshot and quits. The screenshot shows a first-person world, weapon and
player HUD. The recording session completes in 41.610 seconds; both client
and dedicated server exit zero without forced termination.

The `.dm_91` file is 20,163 bytes, SHA-256
`8a4a545e59d3153413f90f5d5da750a5ddfb64a3bc3ac370106b461a3ea273ed`.
Its 564 length-prefixed messages have bounded payloads and a final `(-1, -1)`
trailer, with no bytes left over. The first and last message sequences are
171 and 735. This structural check accompanies runtime playback; it does not
substitute for parsing and rendering the messages.

Each playback gets a fresh isolated writable home containing the recording
and generated fixture PK3, while legitimate retail game assets remain read-only.
Normal playback completes in 19.172 seconds. Timedemo completes in 5.329 seconds
of process wall time and reports `313 frames, 0.99 seconds: 315.5 fps` internally.
The latter is a diagnostic from this probe, not a comparative performance
benchmark. Both reach the queued completion marker and exit zero without
forced termination, truncation warnings or demo parser errors.

Engine `screenshotPNG` images from both playback modes show the recorded world,
weapon and player HUD with retail demo controls. The normal image displays
playback time `0:01.823`; timedemo displays `0:03.050`. These are different frames,
not a claim of pixel-identical rendering. Raw images and the gameplay recording
remain ignored because they contain retail-derived content.

All clients are windowed at 640 by 480 on private desktops that are never
switched to the input desktop. No keyboard/mouse control or operating-system
capture is used. Steam integration, audio and WebUI remain disabled. Only the
recording stage uses a loopback server; replay requires no peer. The temporary
server RCON password is redacted from committed configuration.

Seeking remains unverified. Inspection did not identify an exposed engine seek
path or the searched seek/rewind command strings in the installed retail engine
and cgame; that negative search does not prove seeking impossible. Normal and
timedemo completion are not treated as seek evidence. Independent retail-engine
demo interoperability, other profiles/versions, long recordings, malformed
streams and broader visual parity remain separate requirements.

No engine implementation changed in this checkpoint and no full automated-suite
rerun is claimed. Packaged evidence links validate. No unrelated engine defect
was established. The demo compatibility gate remains open.
