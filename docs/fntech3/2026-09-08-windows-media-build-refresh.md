# Clean Windows package after the media corrections

A fresh export of `e114ded` builds and packages the current WAV, Ogg and older
OpenAL fallback corrections. The extracted Windows x86 package passes short
retail map-change sessions in both directions for both profiles. Exact copies
of its client, GLx renderer and sidecars also pass both-profile file-audio checks.
This refreshes the earlier b57c0d6 package evidence without closing the full
compatibility or release gates.

All 1,528 exported tracked files remain unchanged after the work. The MSVC x86
release build completes 1,152 steps with static runtime/dependencies, strict
warnings and separately loaded GLx, Vulkan and RTX renderers. Dependency sources
may come from Meson's cache; no compiled outputs are copied into the new build.
The complete suite passes 110 targets, zero failures and one skip. Its Python
aggregate runs 1,307 tests with five skips in 279.323 seconds. Legitimate module
probe opt-ins point to the newly built client and dedicated binaries.

Production staging uses the exported release implementation and documentation.
All six PE files pass dependency checks. The package contains 304 files. Two ZIPs
are byte-identical after every staged timestamp is changed: 8,670,609 bytes each.
The archive is extracted into a fresh directory and validated again. All 304
files retain their hashes after runtime testing, with no added files. This proves
deterministic archiving of these inputs, not independent compiler reproducibility.

| Retail pairing | Transition | Result |
| --- | --- | --- |
| Packaged Q3 client, retail Q3 server | q3dm6 to q3dm7 | Normal exits, destination world/weapon/HUD |
| Retail Q3 client, packaged Q3 server | q3dm6 to q3dm7 | Normal exits, destination world/weapon/HUD |
| Packaged QL client, retail Linux QL server | Campgrounds to Overkill | Normal exits, destination world/weapon/HUD |
| Retail QL client, packaged QL server | Campgrounds to Overkill | Normal exits, destination world/weapon/HUD |

Engine screenshots were inspected for all four destination scenes. QL scenes
remain warmup/awaiting players; this is not prolonged match evidence. These
existing synchronized probes exercise admission, module/assets, map renewal and
rendering together. Proprietary provider/API files remain external to the package.

A separate isolated runtime uses byte-identical packaged client/renderer/sidecar
copies and the external OpenAL Soft 1.25.2 Wave File Writer. The independently
encoded six-channel center Ogg produces balanced 997 Hz output in loaded-sound
and music windows under Q3 and QL, followed by a silent final second. Both clients
exit zero. That file-output control uses diagnostic front-buffer presentation;
hardware audio and normal presentation continuity remain unverified.

Clients run windowed with input disabled on private unswitched desktops and use
isolated writable homes. Retail inputs remain unchanged. Map-change tests disable
audio; audio probes write files only. The retail Linux server runs unprivileged.
No mouse/keyboard injection, RCON, OS capture or speaker playback is used.

The [structured evidence](2026-09-08-windows-media-build-refresh.json) pins source,
recipes, artifacts, commands, outputs and validation identities. Raw logs,
screenshots, audio and retail content remain untracked. This report is added after
testing and is not retroactively included in the tested archive.

Hosted CI was rechecked on e114ded: GitHub's
[Linux verification run](https://github.com/themuffinator/fnTech3/actions/runs/34256989292)
again reports that jobs could not start because recent account payments failed or
the spending limit needs increasing. This existing external release blocker is
still unresolved. No new unrelated engine issue was found. Existing optional
asset, muted-audio and retail-service diagnostics persist; they do not establish
service success. Full compatibility and the public multi-platform release remain
unproven.
