# fnTech3 compatibility ledger

## Status

fnTech3 does **not** yet claim full Quake III Arena or Quake Live
compatibility. Immutable profile selection, Q3/QL module adapters, server-memory
views, and profile-specific live delta codecs are integrated and tested. The
same Windows x86 client now loads legitimate Q3 QVMs and QL native modules,
passes local pure admission, reaches `CS_ACTIVE`, and writes engine screenshots
under both profiles. Dedicated probes load maps and advertise protocols 68/91
under automatic and explicit selection. These observations do not close the
complete renderer, demo, authentication, or bidirectional interoperability gates.
The [2026-09-06 checkpoint](2026-09-06-merger-checkpoint.md) records exact
executable identities, intermediate failures, test totals, and limitations.
The subsequent [RTX work round](2026-09-06-rtx-round.md) fixes the recorded Q3
black-polygon defect and establishes separate native-RT Q3 x64 evidence. The
shared x86 GPU/driver path still lacks native-RT capability on this machine.
The subsequent [WebUI request port](2026-09-06-webui-request-port.md) records
FnQL's request/avatar retry corrections, the preserved QL-only boundary, and
new fnTech3 build and offscreen retail-adapter evidence. Gameplay performance
and the broader WebUI lifecycle gates remain open.
The [2026-09-07 Steam discovery checkpoint](2026-09-07-steam-discovery.md) adds
profile-owned Q3/QL discovery, bounded metadata parsing, and same-binary
dedicated startup without explicit installation paths.
The [build-portability checkpoint](2026-09-07-build-portability.md) adds clean
Linux Make/Meson/CMake and renderer-build evidence, native Linux dedicated
startup for both profiles, and corrections to Windows compiler probes.
The [Q3 reference-peer checkpoint](2026-09-07-q3-reference-interop.md) adds
bidirectional protocol-68 pure admission and active sessions against existing
FnQuake3 binaries. Their source revision is unknown, so retail interoperability
and the broader network gate remain open.
The [engine-only packaging correction](2026-09-07-engine-only-packaging.md)
removes inherited proprietary provider staging and rejects known provider and
Steam runtime libraries in release trees and archives.
The [console-profile correction](2026-09-07-console-profiles.md) restores Q3's
stock console shader and verifies console rendering for both profiles with the
same rebuilt Windows x86 client; other asset warnings remain open.
The [protocol-71 checkpoint](2026-09-07-q3-protocol71.md) adds bidirectional
reference-peer sessions, a complete local suite pass after the snapshot fixes,
and clean CI evidence for the earlier engine-only packaging correction.
The [shipped Q3 demo checkpoint](2026-09-07-q3-shipped-demos.md) adds successful
playback of retail-provided `.dm_66`, `.dm_68` and two `.dm3` fixtures. Visual
parity, other formats and malformed-input coverage remain open.
The [QL truncated-demo probe](2026-09-07-ql-truncated-demo.md) adds bounded
recovery evidence for an independently confirmed incomplete `.dm_91` payload;
it is not counted as complete external-demo playback.
The [demo isolation correction](2026-09-07-demo-profile-isolation.md) prevents
format selection and automatic fallback from crossing the active Q3/QL
profile boundary, with executable search tests and runtime rejection probes.
Both profiles also preserve active local sessions after explicit cross-profile
demo rejection, continuing snapshots and rendering without reconnecting.
The [partial-header correction](2026-09-07-demo-header-truncation.md) reports
truncated demo headers consistently with truncated payloads while preserving
historical EOF handling, with shared reader tests and both-profile probes.
The [QL reference-peer checkpoint](2026-09-07-ql-reference-interop.md) adds
bidirectional protocol-91 pure/native-module sessions with existing FnQL
binaries. Retail executable and Steam-authenticated interoperability remain open.
The [shared x86 renderer-restart checkpoint](2026-09-07-renderer-restart.md)
adds one successful full restart for each profile with GLx, Vulkan and RTX
raster, including renewed pure proofs, snapshots and engine screenshots.
Repeated stress, renderer switching and WebUI/audio lifecycle remain open.
The same checkpoint now also records one successful GLx → Vulkan → RTX raster
→ GLx cycle per profile, preserving the connection and renewing pure proofs.
All six directed renderer transitions now pass three repetitions per profile
in the checkpoint's follow-up. Long-duration stability and the remaining
WebUI/audio/native-RT lifecycle gates remain open.
The [config migration correction](2026-09-07-config-migration.md) prevents
legacy Q3 QVM selectors from breaking first-launch QL native-module startup.
Alternating client launches now retain separate settings in a shared home.
The [Q3 native-module checkpoint](2026-09-07-q3-native-modules.md) adds a
legacy-native non-pure session and restores Q3's required QVM selection at
pure admission. QL pinned-native admission still passes with the same client.
The [shader light-RGB correction](2026-09-07-shader-light-rgb.md) accepts the
bare triples observed in a Q3 add-on while preserving parenthesized syntax,
with malformed-input tests and both-profile playback in all three renderers.
The [2026-09-08 regression checkpoint](2026-09-08-regression-checkpoint.md)
records the complete local suite after these corrections and clean CI for
the earlier config-migration revision, keeping their evidence scopes separate.
The [profile path-conversion correction](2026-09-08-profile-path-conversion.md)
contains unrepresentable candidate paths before filesystem marker queries,
with Windows/Linux tests and both-profile automatic dedicated startup.
The [filesystem recovery correction](2026-09-08-filesystem-recovery-config.md)
keeps QL profile-root configs out of Q3's recovery queue, with executable
client/dedicated tests; end-to-end invalid-filesystem recovery remains open.
The [packaged-profile checkpoint](2026-09-08-packaged-profiles.md) verifies
byte-identical archives after timestamp changes and both profiles running
from one unchanged extraction. Independent-build reproducibility remains open.
The [audio restart checkpoint](2026-09-08-audio-restart.md) adds muted legacy
and OpenAL initialization/restart sessions for both profiles. Audible output
remains unverified. Its pinned x86 OpenAL Soft follow-up passes 41 synthetic
PCM assertions and both profiles' muted HRTF-enabled restart probes; complete
game-audio parity and device-loss recovery remain open. The same checkpoint
also records both profiles passing the connected-device recovery guard and
forced live reopening of a working default fallback without sound restart.
The [pak download correction](2026-09-08-pak-download-policy.md) scopes QL's
`pak00` retail exclusion to QL, restoring Q3 add-on download eligibility and
covering both profiles' missing-pak planning with executable regressions.
The [download chunk guard](2026-09-08-download-chunk-bounds.md) rejects truncated
bodies before file writes or acknowledgements; its executable handler tests
cover both profiles without claiming remote fault-injection closure.
The [download write correction](2026-09-08-download-write-errors.md) prevents
UDP acknowledgement after a failed write and propagates short writes through
both HTTP callbacks, with controlled failure regressions.
Its Q3 HTTP follow-up also verifies automatic disconnect/reconnect, pure
admission and gameplay after removing pending test-script actions that delayed
the queued reconnect command.
The [QL download checkpoint](2026-09-08-ql-downloads.md) extends UDP and automatic
HTTP reconnect evidence to QL with matching add-on hashes and native pure
admission. It also records a newly observed hosted CI billing/spending block.
The [HTTP interruption checkpoint](2026-09-08-http-interruption.md) verifies
that short responses leave no installed add-on and that both profiles can
download successfully in a new process reusing the same isolated home.
The [rename fallback correction](2026-09-08-rename-fallback.md) preserves source
files when fallback copying fails, including download finalization, with
Windows/Linux executable failure regressions.
The [clean local Linux checkpoint](2026-09-08-clean-linux.md) builds the i686
dedicated configuration at `b5776c9`, passes its configured suite (84 pass,
one skip), and runs both profiles' native Steam-discovery/map-startup probes.
It does not replace the hosted client/renderer release matrix.
The [local Linux client/renderer extension](2026-09-08-linux-renderers.md) also
builds the non-SDL i686 client and all three renderer modules from that clean
checkout, with 107 configured tests passing and two skips. Linux client
runtime remains a separate gate. The [bundled-SDL build checkpoint](2026-09-08-linux-sdl.md)
also builds the i686 client with embedded GLx and passes its configured suite
(84 pass, one skip), without claiming SDL runtime validation.
The subsequent [native Linux client probe](2026-09-08-linux-client-runtime.md)
reaches Q3 pure admission but finds SDL texture corruption and an X11 shutdown
signal fault masked by exit status zero. Linux client runtime remains open.
The [Unix exit correction](2026-09-08-unix-fatal-exit.md) makes fatal errors
report failure without recursive debug assertions; it does not close the
rendering or GLX shutdown defect.
The [X11 callback lifetime correction](2026-09-08-x11-extension-lifetime.md)
then identifies premature DGA library unloading and fixes the reproduced
shutdown fault: native i686 Q3 pure admission, snapshots and normal client/server
exit pass. SDL texture corruption and broader Linux runtime gates remain open.
The [SDL console context correction](2026-09-08-sdl-console-context.md) then
resolves the reproduced SDL texture corruption by restoring the game's GL
context after console work. Q3 runtime and a scripted renderer restart pass;
full visual parity and broader platform/profile runtime gates remain open.
The [Vulkan descriptor correction](2026-09-08-vulkan-descriptor-restart.md)
fixes a native Linux crash during the pure-join resource rebuild by reallocating
all attachment descriptor sets before updating them. Linux Q3 rendering and a
renderer restart gain runtime evidence without closing broader parity gates.
The [renderer-switch checkpoint](2026-09-08-renderer-switch.md) adds four-step
GLx/Vulkan transition sequences on Linux Q3 and Windows Q3/QL, including Windows
same-renderer fast restarts. All three probes render and exit normally; changed
modules still take a full unload, so cross-module window retention is not claimed.
The [Linux RTX checkpoint](2026-09-08-linux-rtx.md) adds i686 Q3 raster and
software native-RT dispatch/restart evidence. Native RT output remains noisy,
Khronos validation layers were unavailable, and this does not close the shared
Windows x86 Q3/QL native-RT gate.

The target is one shared, drop-in binary set. It must select `q3` or `ql` before
compatibility-sensitive initialization and then use that profile's complete,
internally coherent contracts for the rest of the process.

## Evidence vocabulary

| Level | Meaning |
| --- | --- |
| Source | A pinned implementation/reference comparison establishes a contract or identifies a gap. |
| Unit | Deterministic tests exercise a contract without launching the game or using proprietary inputs. |
| Build | A supported configuration compiles and links the shared targets. |
| Fixture | The built engine is exercised against legitimate local modules/assets or recorded non-proprietary data. |
| Live | A real client/server or renderer session exercises the contract. |
| Interop | Both directions work with the intended retail/reference peer. |

Evidence does not automatically promote upward. In particular, Source + Unit +
Build is not a live-compatibility claim. Every closed gate should link or record
the commit, executable, platform, profile, command, inputs, and result.

## Profile contract summary

| Contract | Quake III Arena (`q3`) | Quake Live (`ql`) |
| --- | --- | --- |
| Installation identity | `baseq3/pak0.pk3`; Q3 installation/Steam identity including AppID 2200 where applicable | `baseq3/pak00.pk3`, `bin.pk3`, and `web.pak` as required; retail Steam AppID 282440 |
| Preferred network/demo | Protocol 68, with the supported Q3/ioquake3 legacy set kept explicit; Q3 demo extensions selected by their protocol | Retail protocol 91 and `.dm_91` |
| Module boundary | Q3 QVM and legacy native trap/syscall contracts: GAME API 8, CGAME import API 4, UI API 6 with the known API-4 compatibility path | Exact retail structured qagame/cgame/UI tables and x86 layouts: native GAME API 10, CGAME API 8, and UI API 8; legacy fallback only where explicitly supported |
| Shared data layout | Frozen Q3 `vmCvar_t`/trajectory/usercmd/entity/player-state boundary PODs (32-bit ABI): 272/36/24/208/468 bytes | Separate frozen QL PODs (32-bit ABI): 276/40/28/236/504 known bytes, including QL-only fields; the retail native snapshot reserves `0x250` player-state bytes and the trailing `0x58` remains unmodelled |
| Maps/assets | Q3 pak/pure rules, BSP 46 plus explicitly supported legacy formats, shaders/models/images/audio/cinematics | BSP 47, QL advertisements/material behavior, retail pak/WebUI/native-module assets |
| Platform constraint | Shared x86 binary is the initial all-in-one proof target; broader Q3 platforms remain desirable | Retail client compatibility requires Windows x86; dedicated-server claims are recorded separately |

Supported legacy protocols and demo formats must remain enumerated in code and
tests rather than inferred from the preferred value. The imported references
identify Q3 protocols 68/71 and legacy Q3 demo handling (including the existing
`.dm_66`, `.dm_67`, `.dm_68`, `.dm_71`, and supported `.dm3` paths), but every
format still needs verification through the fnTech3 profile boundary.

## Compatibility and gate matrix

Status values are `PASS`, `PARTIAL`, `OPEN`, or `BLOCKED`. A `PASS` applies only
to the evidence level named in the same cell.

| Surface | Q3 status/evidence | QL status/evidence | Required promotion gate |
| --- | --- | --- | --- |
| Immutable early profile selection | **PARTIAL — Unit/Build/Fixture.** Explicit aliases, marker precedence, ambiguity rejection, and immutability have tests; automatic and explicit dedicated startup pass against a legitimate installation. Canonical profile mirrors survive startup-variable replay. | **PARTIAL — Unit/Build/Fixture.** Same seam and successful automatic/explicit dedicated startup against retail assets. | Bounded known-retail pak metadata recognition now rejects filename-only placeholders and swapped archives; see the [identity checkpoint](2026-09-08-pak-identity.md). Payload integrity, broader archive variants, mixed trees, and restart behavior remain open. Q3/QL Steam discovery now has Unit/Build/Fixture evidence; see the [discovery checkpoint](2026-09-07-steam-discovery.md). |
| Project identity and packaging | **PARTIAL — Unit/Build/Fixture.** Canonical metadata and primary output names are migrated. The engine-only x86 stage passes dependency/root-archive checks and profile probes. | **PARTIAL — Unit/Build/Fixture.** The same artifacts retain explicitly bound renderer, root-package, and WebUI sidecar names; the proprietary provider is excluded. | Provider staging is removed and known proprietary libraries are rejected; see the [packaging correction](2026-09-07-engine-only-packaging.md). Validate clean CI, reproducible artifacts and remaining packaging paths without renaming external contracts. |
| Filesystem, configs, pak order, and pure checks | **PARTIAL — Unit/Live.** Q3 config order and two-module pure proof are restored. Local QVM clients pass pure admission; malformed and oversized proofs have executable regressions. | **PARTIAL — Unit/Live.** Single-native-module pure proof and profile-specific config/service paths are preserved; local native clients pass pure admission. | [UDP fixture-pak downloads](2026-09-08-udp-pak-download.md) now complete byte-for-byte with pure admission in both reference directions for both profiles, baseline and impaired transport. Golden search-path/pure transcripts and clean-install probes, broader home/config/mod paths, HTTP/failure downloads, case/path behavior and server-referenced paks remain required. |
| Module ABI and VM/native loading | **PARTIAL — Unit/Live.** Retail qagame, cgame, and UI QVMs load and run a local session. Guarded synthetic [cgame](2026-09-08-q3-native-cgame.md), [UI](2026-09-08-q3-native-ui.md), and [qagame](2026-09-08-q3-native-qagame.md) DLLs also pass actual legacy vmMain/syscall transfers and callbacks; qagame covers shared-entity/collision access, restart and invalid-binding recovery. The [native loopback pair](2026-09-08-q3-native-snapshot-roundtrip.md) also checks initial/updated player and entity fields in client snapshots plus a live usercmd return. Frozen POD adapters, transactional DLL loading, QVM bounds, cgame traps/extensions, UI policy, and cvar flags have executable regressions. | **PARTIAL — Unit/Live.** Retail qagame API 10 and cgame/UI API 8 load and run through the same client. Guarded synthetic native imports now exercise config/usercmd transfers through the running client; see the [native import checkpoint](2026-09-08-native-guarded-imports.md). Its [opt-in runner](2026-09-08-native-runner.md) now supports packed installs and private-desktop normal-exit verification. Structured contracts remain separate; the native snapshot's additional `0x58` bytes remain unresolved. | Renderer configuration PODs and the Q3 cgame/UI adapter are frozen; see the [glconfig checkpoint](2026-09-08-glconfig-boundary.md). Both profiles marshal frozen configstring-transfer layouts; see the [game-state checkpoint](2026-09-08-gamestate-boundary.md). Incoming bot commands now marshal Q3/QL PODs with bounds checks; see the [bot-command correction](2026-09-08-bot-usercmd-boundary.md). QL user-command outputs use the frozen 28-byte adapter at cgame/qagame boundaries; see the [user-command checkpoint](2026-09-08-ql-usercmd-boundary.md). Resolve or safely contain the QL snapshot tail, complete the boundary audit, and broaden Q3 native and packaged-client module coverage. |
| Live network codec and handshake | **PARTIAL — Unit/Live.** Local protocol-68 sessions and both directions against a freshly built clean FnQuake3 peer at the pinned baseline reach active state after pure admission (71 with fnTech3 as client, 68 with fnTech3 as server); see the [pinned-peer checkpoint](2026-09-08-pinned-q3-interop.md). Both directions also pass [controlled loopback loss, delay and duplication](2026-09-08-q3-impaired-interop.md) and [initial gamestate fragment-loss recovery](2026-09-08-q3-fragment-recovery.md). The [second-fragment checkpoint](2026-09-08-second-fragment-recovery.md) also passes and corrects earlier protocol-68-only claims. [Terminal-fragment loss](2026-09-08-terminal-fragment-recovery.md) recovers in both peer directions. Shared lineage still limits peer independence. Field parity, truncated messages, and float encodings are tested for protocols 68/71. | **PARTIAL — Unit/Live.** Local protocol-91 sessions reach active state. Both directions against the existing FnQL peer also pass [periodic loss/delay/duplication and initial gamestate-fragment loss](2026-09-08-ql-impaired-interop.md), with pure admission and native join UI/world rendering; [second-fragment loss](2026-09-08-second-fragment-recovery.md) and [terminal-fragment loss](2026-09-08-terminal-fragment-recovery.md) also recover. Reference source revision and active team gameplay remain unverified. Field parity and malformed-input tests pass; invalid usercmd batches drop only their sender without partial execution. | A [client-fragment checkpoint](2026-09-08-client-fragment-recovery.md) verifies exact, ordered delivery of six reliable commands after first-client-fragment loss in all four reference configurations. [UDP fixture-pak downloads](2026-09-08-udp-pak-download.md) also pass baseline and impaired reference transport. Golden packet transcripts, malformed/fuzz corpus, broader fragmentation/reliable/pure/download coverage, then bidirectional retail interop for each profile remain required. |
| Demos | **PARTIAL — Unit/Live.** New local `.dm_68` recordings play to normal completion. Executable tests preserve protocol-71 fallback and extension behavior. | **PARTIAL — Unit/Live.** New local `.dm_91` recordings play to normal completion after fixing an incorrect `.dm_68` suffix for QL captures without voice messages. | Record and play every claimed format; cover truncation, oversize, seeking, timedemo, protocol switching, and unchanged termination behavior. |
| BSP, collision, and renderer asset decode | **PARTIAL — Unit/Live.** q3dm6 loads with GLx/Vulkan/RTX raster; the recorded RTX black-polygon defect is fixed and verified on the frozen demo. Separate Q3 x64 native probes cover baked lightmap and vertex-lit world materials. See the [RTX round](2026-09-06-rtx-round.md). | **PARTIAL — Unit/Live.** campgrounds BSP 47 loads with all three raster renderers and produces engine screenshots; the final shared RTX raster probe passes. | Broaden maps and collision paths, exercise malformed lumps, compare equivalent screenshot scenes, and prove live advertisement presentation/fallback. |
| Other assets (models, shaders, images, audio, cinematics, UI resources) | **OPEN.** No fnTech3 end-to-end inventory is signed off. | **PARTIAL — inherited source/fixture evidence.** Retail paths exist, including WebUI resources, but unified regression is incomplete. | Per-format fixtures plus windowed runtime scenes for both profiles; confirm error handling and cross-renderer/audio consistency without redistributing assets. |
| WebUI native requests and pending avatars | **PARTIAL — Source.** QL-only activation and frame guards are preserved; this port does not enable WebUI for Q3. | **PARTIAL — Unit/Build/Fixture.** Notification scheduling, bounded Unicode transport, avatar backoff/batching, and reload cleanup are tested; fnTech3's production adapter passes an offscreen retail-runtime probe. See the [port record](2026-09-06-webui-request-port.md). | Confirm affected Linux/Proton gameplay performance, live Steam-avatar callbacks through the engine, and windowed renderer/lifecycle stress. The source-reported DataPak document-query limitation remains open. |
| Renderer API/lifecycle | **PARTIAL — Unit/Live.** All three modules initialize, render a local session, and shut down. Q3 x64 native RT survives renderer restart and windowed resize with Vulkan validation; that evidence applies to the diagnostic Q3-only executable. | **PARTIAL — Unit/Live.** All three modules initialize, render the native join UI/world, and shut down with WebUI disabled. Native RT remains unverified: this machine's x86 driver lacks required extensions. | Shared x86 native RT, broader renderer switch/restart and screenshot parity; HDR, performance, and WebUI resize/device-loss/crash stress. RTX raster startup does not prove ray tracing. |
| Steam and optional services | **PARTIAL — Unit/Build/Fixture.** AppID 2200 discovery supplies the Q3 base path without enabling QL services or adding a second Q3 Steam search tree. | **PARTIAL — inherited Source/Unit/Live.** QL provider boundaries and a retail join exist in FnQL, not yet as fnTech3 closure. | Broader Q3/QL Steam-path validation; QL logged-in/offline/failure behavior; session-ticket admission both ways; callback/shutdown fault tests; live match reports, unattended Workshop download, and sustained GameServer stress. |
| Dedicated server | **PARTIAL — Build/Unit/Fixture.** [Retail bot combat](2026-09-08-retail-bot-runtime.md) now passes with the corrected incoming command boundary. q3dm6, retail qagame QVM, botlib, pure=1 info response with protocol 68, and clean shutdown pass for automatic and explicit profile selection. | **PARTIAL — Build/Unit/Fixture.** campgrounds/ffa, retail API-10 qagame, botlib, pure=1 info response with protocol 91, and clean shutdown pass using the same binary. | Windows x86 and native Linux i686 discovery/map probes now pass for both profiles; see the [portability checkpoint](2026-09-07-build-portability.md). Windows dedicated early and initialized fatal errors now exit with status 1 without console dismissal; see the [fatal-exit checkpoint](2026-09-08-windows-dedicated-fatal.md). The [retail bot checkpoint](2026-09-08-retail-bot-runtime.md) admits four QL bots. The [follow-up](2026-09-08-bot-trace-and-probe-corrections.md) corrects command-queue interference, localizes timestamp-only logging to the retail DLL and fixes botlib no-entity traces; the subsequent [world-link restart fix](2026-09-08-world-link-restart.md) passes a shrinking native entity-binding regression and a free-running retail QL restart with four bots, IN_PROGRESS state and a scored frag. A subsequent [retail transition series](2026-09-08-retail-transition-series.md) passes three explicit restarts and two map changes per profile with four bots at all six checkpoints; QL reports IN_PROGRESS throughout those samples. The [bot-profile default checkpoint](2026-09-08-bot-profile-defaults.md) corrects QL bot_grapple to the retail engine default 1 while retaining Q3 default 0 and explicit overrides. Longer transition stress remains open. Connect retail clients, exercise pure admission/live messages, and broaden long/faulted sessions. A connectionless pure=1 response does not prove pure negotiation. |
| Cross-profile isolation | **PARTIAL — Unit/Fixture.** Active profile is immutable; config read and write select `q3config.cfg`. Both profiles start sequentially from the same staged distribution with isolated homes. | **PARTIAL — Unit/Fixture.** Active profile is immutable and QL's `qzconfig.cfg`/`repconfig.cfg` save path is preserved. | Broaden restart, config persistence, cached-module, download, Steam-state, protocol-state, and sidecar isolation checks. |
| Release/build matrix | **PARTIAL — Build/Fixture.** Fresh Windows x86 MSVC release configuration plus incremental compatibility fixes builds; complete Meson tests pass with one unavailable OpenAL-loopback skip. Staged client/dedicated/renderer probes pass their bounded startup checks. A separate diagnostic Q3-only x64 configuration builds and exercises native RT. | **PARTIAL — Build/Fixture.** The same shared x86 artifacts pass QL startup/demo/RTX raster checks. The Q3-only x64 executable rejects QL and cannot establish this release contract. | Clean Linux i686 CI now passes Make/Meson/CMake, all renderer builds, bundled SDL3 and 107 configured tests (2 skips). Windows MSVC/MSYS2 x86 release validation also passes; see the [reference-peer checkpoint](2026-09-07-q3-reference-interop.md). Reproducible artifacts, packaged runtime gates and visual/service failures remain open; see the [portability checkpoint](2026-09-07-build-portability.md). |

## Current blockers

1. **Boundary ABI verification.** Q3 adapters and profile-specific server-memory
   access are integrated, with Q3 QVM and QL native client/server observations.
   Complete client and extension audits, broader Q3 native module coverage, and
   runtime probes are still required.
2. **Live wire evidence.** Live usercmd/entity/playerstate call sites select
   explicit wire profiles. Field-table parity and transactional malformed-input
   regressions pass, but complete packet flows and bidirectional retail peers
   are still required. Local active sessions exercise more than connectionless
   info queries but do not establish remote-peer interoperability.
3. **QL snapshot tail is unresolved.** The known QL player-state fields occupy
   `0x1f8` bytes, while the retail native snapshot reserves `0x250`. The
   remaining `0x58` bytes must not be labelled padding or exposed without
   retail evidence and bounded handling.
   The [snapshot marshalling correction](2026-09-07-ql-snapshot-marshalling.md)
   now converts the frozen known prefix explicitly and tests zeroed unknown
   bytes and destination guards. The [guarded native snapshot probe](2026-09-08-native-snapshot-containment.md)
   exercises exact output bounds, rejected-request preservation and zero-tail
   containment through the running client; the tail's meaning remains unverified.
   [Snapshot history validation](2026-09-07-snapshot-history.md) also rejects
   malformed/stale module requests before writes. The subsequent
   [entity-history correction](2026-09-07-entity-history-wrap.md) covers parser
   increments and client/demo indexing, with executable wrap tests and both
   profiles' record/playback probes; sustained network-wrap evidence is open.
4. **Detection is not yet authoritative.** Known retail central-directory
   metadata now replaces filename-only evidence; it remains an identity hint,
   not payload authentication or proof of a complete installation. Profile-owned Q3/QL Steam discovery
   now passes bounded Unit/Build/Fixture checks; archive identity and complete
   profile-specific asset validation remain open.
5. **Release packaging remains open.** Canonical metadata and primary artifacts
   now use fnTech3 identity. Retained external sidecar names, unsupported legacy
   packaging paths and reproducible/package-runtime evidence remain open.
   The [engine-only correction](2026-09-07-engine-only-packaging.md) removes
   inherited closed-source provider staging. Subsequent clean CI now validates
   that packaging path; see the [regression checkpoint](2026-09-08-regression-checkpoint.md).
   Reproducibility and packaged runtime gates remain open.
6. **Q3 coverage remains incomplete.** Local QVM startup, pure admission, world
   rendering, and HUD are observed. The RTX raster black geometry defect is fixed
   and verified on the frozen demo; broader formats, native modules, remote peers,
   and packaged runtime paths remain open.
7. **QL coverage remains incomplete.** Unified native-module startup, local pure
   admission, and join UI/world rendering are observed. Service authentication,
   WebUI presentation, remote peers, broader gameplay, and lifecycle stress still
   require new fnTech3 evidence.
8. **Retail Q3 interop probe is blocked before engine startup.** The legitimate
   Steam retail executable, copied with base paks into an isolated temporary
   fixture, reported `Failed to find Steam`. No retail engine log or connection
   was observed; this is a probe environment/dependency blocker, not evidence
   of a network failure or success.

## Open live gates

Hosted CI currently cannot start new jobs because GitHub reports an account
payment or spending-limit issue; see the [QL checkpoint](2026-09-08-ql-downloads.md).
This is an external release-gate blocker, not a reported build/test regression.
Local compatibility work can continue while the account owner resolves it.

### Quake III Arena

- Windowed clean-install startup and engine-screenshot smoke scenes using
  legitimate Q3 assets.
- Q3 QVM and supported native module loading with exact trap/API behavior.
- Protocol 68/71 golden and live sessions, including pure/download flows.
- Broaden the [bidirectional FnQuake3 peer result](2026-09-07-q3-reference-interop.md)
  to reproducible pinned peers, retail clients/servers and remote network cases.
- Record/playback for every claimed Q3 demo format and representative malformed
  inputs.
- Renderer/collision/asset coverage across the claimed BSP and media formats.

### Quake Live

- Re-run legitimate retail qagame/cgame/UI module probes through the fnTech3
  `ql` profile and packaged binaries.
- Broaden retail protocol-91 packet captures/server coverage and prove a retail
  QL client can join the fnTech3 dedicated server.
- Complete `.dm_91` seek/timedemo and malformed message-body stress coverage.
- Exercise live advertisements and deterministic missing-service fallback in
  every enabled renderer.
- Stress WebUI resize, renderer restart/switch, device loss, child crash, and
  repeated recovery while preserving native-UI fallback.
- Complete live match-report conversion, logged-in unattended Workshop
  subscription/download, provider loss/fault injection, and sustained
  GameServer callback tests.

### Shared release

- Run the full automated suite and supported build matrix on a clean checkout.
- Run both profiles from the same packaged binaries with isolated homepaths and
  no file replacement between launches.
- Validate only windowed sessions unless fullscreen permission is explicit.
  Rendering evidence must come from the engine `screenshot` command, never an
  operating-system capture. Automated probes must not control user input
  without narrowly scoped permission.
- Archive non-proprietary logs, manifests, packet/demo fixtures, checksums, and
  test summaries. Do not archive retail assets, modules, runtimes, tickets, or
  credentials.

## Evidence snapshot: merger foundation

The following evidence establishes the starting point, not compatibility:

- FnQuake3 source baseline pinned at
  `7d8c9629cdc7979f09697266470359101ffd9eda`: its existing Python suite passed
  624 tests with 9 skips and 440 subtests during the import audit. The dirty
  local worktree was catalogued separately and was not treated as committed
  baseline evidence.
- FnQL source baseline pinned at
  `fce44fe714564621e6ceb79e3da7c53089646af2`: its existing Python suite passed
  1,192 tests with 7 skips and 24,957 subtests during the import audit.
- On 2026-08-21, a fresh Windows x86 MSVC Meson configuration of the merged
  baseline compiled all 1,142 build steps, including the client, dedicated
  server, renderers, and the focused fnTech3 game-profile test executable.
- The game-profile test executable separately passed under a strict C++17
  compile. No fnTech3 game runtime was launched for this foundation snapshot.
- The frozen Q3/QL ABI headers passed standalone C11 checking and strict C++17
  layout tests, including i686 size/offset parity against their pinned source
  structures. This proves the declarations, not their complete runtime use.

Later evidence must be appended or linked with its commit and environment; do
not silently reinterpret these baseline results as proof for subsequent code.

## Full-compatibility sign-off

The initial goal is complete only when every required matrix row is `PASS` at
its stated highest evidence level for both profiles, all blockers and open live
gates are closed or explicitly removed from the supported contract, and the
same release artifacts pass:

1. automatic and explicit profile selection;
2. exact module API/ABI and asset loading;
3. client and dedicated-server networking in both interoperability directions;
4. all claimed demo formats and malformed-input gates;
5. representative gameplay/render/audio/input sessions using legitimate data;
6. cross-profile isolation and deterministic packaging; and
7. complete automated, build, and documented windowed runtime gates.

Sign-off must identify the exact release artifacts and evidence bundle. A
partial profile, one-way client connection, inherited FnQL result, or source
claim cannot satisfy the goal.
