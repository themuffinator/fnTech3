# All-game compatibility round — 2026-10-02

This round fixes malformed filesystem-list output handling across all sixteen
profiles and closes false-positive server identity evidence. It verifies bounded
startup paths for all ten titles using one unchanged Windows x86 client/server
pair and GLx, Vulkan and RTX modules. No complete compatibility gate closes.
The [JSON companion](2026-10-02-all-games-round.json) records exact commands,
installation/map identities, binary hashes, outcomes and retained artifact paths.

## Corrections and evidence

`FS_GetFileList` and `FS_GetModList` wrote their terminator before checking output
capacity. The production fixture reproduced that write into a zero-size canary
buffer before the correction. Null output and non-positive capacity now return
zero before enumeration or any write; `FS_ListFiles` also rejects a null count
destination. Normal file packing, mod descriptions, base-game exclusions and
root renderer shader behavior remain unchanged. Four native tests execute the
actual functions against the real catalog of sixteen profiles. The corrected
tests pass on Windows x86 and Linux x86 under address/undefined-behavior
sanitizers. Removing each of the three guards separately makes the malformed
output regression fail. These are API-boundary facts, not retail gameplay proof.

The initial nullable-query failure was a fixture error: its string stub rejected
null where production `Q_stricmp` handles it. The fixture was corrected to execute
that actual function; no nullable-query production correction is claimed.

The Q3/QL identity probe accepted odd or duplicate info fields, reused one
challenge and did not require the child server's observed port. Both identity
and release probes now use one strict parser. Duplicate keys include aliases
whose case differs, as the engine's key lookup ignores case; embedded NUL,
empty keys and incomplete pairs are refused. Every identity run uses a fresh
nonce and queries the bound port after the engine's collision fallback.
Twelve probe tests pass, including real loopback UDP nonce/source checks. The
same dedicated binary then passes Q3 and QL under auto and explicit selection,
with exact installed pak/module observations and matching requested/bound ports.
The final probe commands explicitly select IPv4 for dedicated queries and pin
both address families to loopback, including when a shared client executable is
used as the server. The four identity cases and twelve probe regressions pass
again with that explicit binding policy.
This is connectionless identity and startup evidence, not peer admission.

## Runtime coverage

The final 41 cases comprise 36 client captures (12 per renderer) and five
dedicated cases. Two dedicated results are expected single-player module
refusals; they are not playable launches. All launches use private unswitched
desktops, windowed 800×600 rendering, disabled mouse/joystick and sound, isolated
writable homes and console startup commands only. No gameplay input is injected.
Installation file-size/mtime inventories and runtime hashes stay unchanged.
Every capture comes from the registered `screenshotPNG` command and was inspected.

| Profile | Map(s) | Runtime path | Result and remaining scope |
| --- | --- | --- | --- |
| `q3` | `q3dm1` | GLx / Vulkan / RTX | Passive startup; Broader retail/remote protocol-68/71, pure/download, demos and renderer/gameplay acceptance remain open. |
| `ql` | `campgrounds` | GLx / Vulkan / RTX | Passive startup; Reverse retail client admission, broader protocol-91/demos, online services and device/WebUI recovery remain open. |
| `rtcw-sp` | `escape1` | GLx / Vulkan / RTX | Passive startup; Campaign transitions, sustained AI/combat, broad save exchange and live audio/input remain open. |
| `rtcw-mp` | `mp_beach` | GLx / Vulkan / RTX | Passive startup; Sustained classes/weapons/objectives and broader bidirectional peer, pure/demo and audio/input evidence remain open. |
| `wolfet` | `battery` | GLx / Vulkan / RTX | Passive startup; Navigation and live cache comparisons, lossy/larger transfers, HTTP/redirect behavior and sustained gameplay/audio/input remain open. |
| `stvef-sp` | `borg2` | GLx / Vulkan / RTX | Passive startup; Physical input acceptance, listening, cinematic and broader renderer/campaign comparisons remain open. |
| `stvef-mp` | `hm_borg1` | GLx / Vulkan / RTX | Passive startup; Physical jump/mouselook acceptance, listening and remaining renderer comparisons remain open. |
| `q3-ihv` | `ihv_test1` | dedicated | Passive startup; Dedicated native-module subset only; client and network paths remain blocked. |
| `jka-sp` | `t1_sour` | dedicated | Expected module refusal; GetGameAPI table/record adapter is unwritten; no game module, client/wire/demo/savegame or rendering runs. |
| `jka-mp` | `mp/ffa1` | dedicated | Passive startup; Dedicated module/frame loop subset only; client, live packet/snapshot/demo and gameplay boundaries remain unwritten. |
| `jk2-sp` | `kejim_post` | dedicated | Expected module refusal; GetGameAPI table/record adapter is unwritten; the legacy/QVM attempts refuse this profile, with a generic final VM error. |
| `jk2-mp` | `ffa_bespin` | dedicated | Passive startup; Dedicated retail QVM/frame loop subset only; measured delta tables are not wired to packet/snapshot/demo or client paths. |
| `alice` | `garden1` | GLx / Vulkan / RTX | Passive startup; Retail bidirectional peer, save/demo exchange, sustained campaign/gameplay and listening remain open. |
| `fakk2` | `fakkhouse` | GLx / Vulkan / RTX | Passive startup; Later-level reverse peer/save sessions, broader campaign/gameplay and listening comparisons remain open. |
| `mohaa` | `dm/mohdm1`, `m1l1` | GLx / Vulkan / RTX | Passive startup; No runnable retail SafeDisc engine peer here; peer/save/demo comparisons and broader gameplay/audio/input remain open. |
| `q3test` | `q3test1` | GLx / Vulkan / RTX | Passive startup; This runtime round uses 1.08 only; every release remains separately pinned, with broader demo/renderer/gameplay evidence open. |

Q3/RTCW SP/Elite Force and MOHAA MP captures show their worlds/HUDs; QL and RTCW
MP show their join/limbo UI, ET a spectator view, Alice/F.A.K.K.2 their scripted
opening cameras, MOHAA SP its briefing and Q3Test its arena. These captures prove
that the selected renderer produces the observed startup scene. They do not
compare retail frames, exercise native ray tracing, or establish player controls,
combat, campaign, savegames, demo or network interoperability.

Initial harness misses are retained separately in the companion: the IHV fixture
needed loose-map lookup; Ritual's millisecond waits needed a longer interval
after queuing a screenshot; MOHAA needed its match settings in the config to stay
below the engine's 32-startup-command limit; and JK2 SP needed its documented
native/QVM refusals recognized. These are not engine regressions.

## Build, tests and tooling

`meson compile -C meson/build/win32-dev -j 8` passes with all development profile
options and all three renderers enabled. The existing `debugoptimized` preset
is reused in place; this is a dirty developer build, not a clean release proof.
Build source digest: `831289353a11aa65b3615cc81ab3252e88d3dc62f9203fb4aacaeb3e78633285`.

Full Meson suite: **178 passed, 1 failed, 2 skipped**. The one failure is the resampler retention fixture during concurrent audio edits. Its current four-test suite passes. Both initially skipped checks pass with the explicit Quake Live path and staged OpenAL Soft runtime. The 147-test affected set has one Ritual parking/gain assertion during those edits; that exact current test passes on rerun. Generated-document checks pass (4/4). These reruns resolve the observed fixture failures; a fresh complete suite on a frozen worktree is still required for release evidence.
The command is `meson test -C meson/build/win32-dev --no-rebuild --print-errorlogs
--timeout-multiplier 3`; individual fixture timeouts remain unchanged.
The VS Code inventory remains 162 configurations across all sixteen profiles;
the largest has 22 startup commands and none exceeds 32. Jedi SP/client
boundary probes remain explicitly distinct from implemented server paths.

Concurrent audio work changed 10 source/test files after the build checkpoint.
The two probe scripts also gained explicit address-family and loopback bindings,
verified by the final identity rerun.
The 41 map/boundary runs use one unchanged runtime set, whose identities are
pinned in the companion. The final identity-probe rerun records its dedicated
binary separately. A subsequent audio rebuild replaces the stable-path binaries
after the map probes; it does not rewrite their captured identities.
This round preserves those edits and does not claim
that the complete shared worktree was frozen for the suite. The audio changes
are tracked by the [audio audit](2026-10-02-audio-system-audit.md).

## Artifacts and unrelated observations

Useful evidence is retained under
`.tmp/all-games-round-2026-10-02/`, including before/after regressions, mutation
and sanitizer results, challenged UDP replies, and `evidence/` console logs and
engine captures. No retail library or asset is added to tracked files.
Task-owned runtime homes and native fixture executables were removed after containment and link checks; useful logs and captures remain in `evidence/`.

Ninja recovered a prematurely ended dependency cache during the build; the cause
was not investigated. JK2 SP still follows named native/QVM profile refusals
with a generic `VM_Create on game failed`; its underlying table boundary remains
unwritten. Neither observation establishes a new gameplay failure.
The shared release checklist now names every applicable path of all sixteen
profiles instead of its stale five-profile scope. Existing development gates
and the ledger's full-compatibility restrictions remain in place.
