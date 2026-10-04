# ET demo admission, player states and damaged inputs

The [2026-09-25 playback record](2026-09-25-wolfet-demo-playback.md) already
establishes protocol-84 spectator playback and renderer checks. This follow-up
adds reusable probes, current-build regressions, recordings made against a
pure dedicated server, both teams' player states and malformed-input runs.
The older gate-matrix wording that playback was unimplemented was stale.
The [companion JSON](2026-10-01-wolfet-demo-regressions.json) records the exact
binaries, commands, files and outcomes without redistributing retail inputs.

No engine decoder changes are part of this follow-up. The existing owning
profile selects `.dm_84` and its ET wire codec. The round-trip and damaged-demo
harnesses now accept `wolfet`, retain isolated homes and use the stable x86
executables. The optional team sequence consists of console commands; it does
not inject mouse or keyboard input. All runs are windowed on private desktops.

On `goldrush`, the client records a short spectator session, then replays it
normally and as a timedemo in fresh processes. Retail `et.exe` records against
the fnTech3 dedicated server under `sv_pure 1`; fnTech3 replays that recording
both ways. Retail ET also replays the fnTech3 recording. The same exchanges
pass with Axis and Allied admissions included. The engine captures show the
spawned player, team HUD and weapon, and the replay shows the corresponding
player state and ET's demo-controls panel. Each client exits normally, every
checked installation inventory is unchanged, and each demo envelope ends
cleanly. The dedicated test host is stopped after its recording session.

The two team recordings contain 181 messages / 19,165 bytes (fnTech3) and
225 messages / 20,197 bytes (retail). Their fnTech3 timedemos finish in 229 and
228 frames respectively. These frame counts are completion evidence, not a
retail timing comparison or a performance benchmark.

The damaged-input sweep uses seven fixed envelope/body faults and eight
seeded body corruptions. On the spectator recording, three reach the end,
with truncation notices where appropriate, and twelve are rejected. None
crashes, hangs or remains stalled. A separate sweep applies the same cases to
the player-state recording, again with three completions and twelve
rejections. Its checked installation inventory is unchanged. The companion
retains all thirty outcomes and input hashes.
The harness confirms that the main loop remains alive after completion or
rejection, then terminates the test process deliberately. Those terminations
are separate from the normal exits in the round-trip sessions.

Rendering uses fnTech3's existing GL path and the retail client module, with
the normal engine lighting, shader, fog, model and HUD paths. This adds no
retail-exact presentation option and switches off no rendering features.
Sound is disabled for these bounded format probes, so no audio evidence is
claimed. Vulkan and RTX playback observations remain in the earlier record.

The demo gate remains partial: this is one stock map and short team/spawn
sequences, without combat, objectives, a complete match, multiplayer density,
seeking, mod demos or `.dm_83`. The complete automated suite passed on these
same engine binaries in the [reported-fault record](2026-10-01-observed-compatibility-fixes.md);
the added harness coverage is established by these separate runtime runs.
