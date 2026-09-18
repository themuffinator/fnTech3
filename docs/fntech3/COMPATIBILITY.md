# fnTech3 compatibility ledger

## Unified goal and supported-game scope

fnTech3's goal is one modern id Tech 3 engine distribution and shared executable
set for four games and five immutable profiles. Shared renderer, audio, input,
platform, and tooling improvements must preserve each game's own contracts.
Full compatibility is **not yet established for any profile**.

| Game / mode | Startup profile | Current availability |
| --- | --- | --- |
| Quake III Arena | `q3` / `quake3` | Enabled; partial build, fixture, live, and interoperability evidence; required gates remain open. |
| Quake Live | `ql` / `quakelive` | Enabled; retail client boundary is Windows x86; required gates remain open. |
| Return to Castle Wolfenstein — single player | `rtcw-sp` | Experimental; default startup gated; `-Drtcw-development=true` permits isolated probes; campaign and broader compatibility gates remain open. |
| Return to Castle Wolfenstein — multiplayer | `rtcw-mp` | Experimental; same build option and executable as SP; multiplayer and broader compatibility gates remain open. |
| Wolfenstein: Enemy Territory | `wolfet` / `et` | Experimental; `-Dwolfet-development=true` permits dedicated probes only; client startup remains gated in every build. |

RTCW is one title with two mode profiles. A mode change relaunches the same
executable; it does not mutate the running process's profile. Catalog
registration, a development option, or a partial runtime result does not
establish supported release compatibility. The [Q3/QL matrix](#compatibility-and-gate-matrix)
and [RTCW matrix](#rtcw-gate-matrix), plus the [ET matrix](#wolfet-gate-matrix), define the current unified scope.
Dedicated-server and online-service gates apply according to each profile's
contract; RTCW SP additionally requires campaign, AI, and save/load evidence.

## Status

The [ET contract and dedicated-server slice](2026-09-17-wolfet-contracts.md)
adds a distinct immutable profile, protocol-84 codecs, frozen module records,
checked server-memory transfers, native game imports, and attachment-tag loading.
ET is not a supported client release. UI/cgame, demos,
pure admission, gameplay, and bidirectional interoperability remain open.
ET's skeletal meshes now have a model type, a surface and a draw call in all
three renderers, verified by arithmetic over every installed model. A
developer-build ET client now runs against a retail installation, draws ET's
own menus and limbo panel through the retail modules, and quits cleanly; see
the [client run record](2026-09-18-wolfet-client-run.md). A
retail ET client reaches an active non-pure spectator session on the experimental
server and both processes exit normally; the reverse direction remains gated.
The [ET snapshot follow-up](2026-09-17-wolfet-snapshots.md) adds checked per-client
callbacks and single/multiple visibility proxies with executable regressions.
Retail Axis/Allies admission and same-binary Q3/QL startup still pass; live
landmine/proxy behavior remains open. The
[viewpoint and capacity follow-up](2026-09-17-wolfet-viewpoints.md) adds ET
origin-only brush visibility, self/exclusive cameras and lean, and removes the
incorrect 512-entity server cap while preserving the frozen cgame limit.
4,936 finite camera/lean cases match the original-source calculation; live
visibility and rendered-camera parity remain open. The
[game-import follow-up](2026-09-17-wolfet-game-imports.md) raises handled ET
game imports from 52 to 189 of 212, covering every import the retail game
module can issue. It adds the binary message channel, temporary bans and their
connection refusal, ET's three-argument drop-client import, preferred-slot bot
allocation, the dedicated sound stubs, and the elementary-action, AI-state,
bot-library and navigation surfaces. Eight of the 23 unhandled ordinals
reproduce refusals retail ET makes itself; the other 15 need bot-library
exports or record conversions without evidence. No live ET bot session is
claimed. The [pure follow-up](2026-09-17-wolfet-pure.md) then opens ET pure
admission: it restores retail's `sv_pure 1` default, admits ET wherever the
RTCW native module pair was handled, and publishes the loaded-pak names retail
ET clients require but fnTech3 had left empty. A retail ET client now passes
pure admission, reaches CS_ACTIVE and joins both teams on all six stock maps;
one client and one installation do not close the broader pure, download or
interoperability gates. The
[gameplay follow-up](2026-09-17-wolfet-gameplay.md) adds two more bounded
inbound runs under pure admission: a retail client spawns on Axis and moves
about 250 units through three distinct view origins, and a goldrush-to-battery
map change keeps that client connected and re-entering the world. Movement
parity, combat, objectives and dense exchanges remain open. The
[official-pak correction](2026-09-17-wolfet-downloads.md) then replaces the
compile-time download test that could only match Quake's directory, so ET and
RTCW stop offering their own retail paks; no ET download has been transferred
end to end. The [navigation follow-up](2026-09-17-wolfet-navigation.md) raises
handled ET game imports to 198 of 212, corrects a blocking-entity rule the
retail game module reaches, and adds ET's chat-match record. The six still
refused each need navigation data or trace results this engine does not
produce. The [redirected-download record](2026-09-17-wolfet-web-downloads.md)
adds ET's server-side www download: the redirect message, the fallback URL and
the client progress protocol, kept unknown to the other four profiles. No ET
download has been transferred over any path. The
[visibility follow-up](2026-09-17-wolfet-visibility.md) derives ET's per-area
waypoints and visibility from the loaded navigation world and adds its five
reachability floods, reaching 202 of 212 game imports. Retail ET ships no
navigation files, so none of that surface is reachable from stock content and
no live behaviour is claimed. The
[prediction follow-up](2026-09-17-wolfet-prediction.md) then writes ET's own
movement prediction and the world-trace result it returns, completing the
declared ET game import surface at 204 of 212: the eight that remain are the
ordinals retail ET's own engine refuses. The
[model record](2026-09-18-wolfet-models.md) adds bounded MDM and MDX readers
and decodes all 21 skeletal files in a legitimate installation; decoding is not
rendering and the client stays gated. The
[cgame boundary record](2026-09-18-wolfet-client-boundary.md) then opens ET's
client-side records, its renderer imports, its key vocabulary and the dispatcher
that routes them, reaching 106 of the 147 cgame imports, and the
[UI boundary record](2026-09-18-wolfet-ui-boundary.md) reaches 104 of the 121 UI
imports, six of which retail ET refuses itself. The
[sound record](2026-09-18-wolfet-audio.md) then carries ET's own sound values --
a volume with every one-shot, a start time with every loop and a stop-sounds
flag on fades -- and the [light record](2026-09-18-wolfet-light.md) carries its
dynamic light. The
[client services record](2026-09-18-wolfet-client-services.md) then wires its
binary message channel in both directions, its screen-space polygons, its saved
view and its in-game popups, and the
[text and files record](2026-09-18-wolfet-text-and-files.md) adds its
localisation lookup and its own delete, taking those two surfaces to 136 and
114, and the [camera record](2026-09-18-wolfet-camera.md) adds its spline
paths and the
[scene geometry record](2026-09-18-wolfet-scene-geometry.md) its batched
geometry and runtime shaders, and the
[texture identity record](2026-09-18-wolfet-texture-id.md) its image lookup,
taking the cgame surface to 143. Of the seven UI imports still open, six
are ordinals retail ET refuses itself and the seventh is a deliberate refusal to
open a module-supplied URL; all seven still open on the cgame side are renderer
features. Nothing in any of
them has run inside a client.

The [RoQ bounds and recovery checkpoint](2026-09-17-roq-bounds.md) adds checked
movie framing, dimensions, codebooks, VQ/motion reads and partial shutdown. It
fixes synchronous first-frame hangs and terminal-frame accounting, verifies
27 distinct installed movies, and passes malformed-movie recovery under all four
profiles through the same Windows x86 client. Original RTCW also ignores JPEG
chunks, so that preserved policy is no longer listed as an unimplemented
original-engine contract. Full cinematic/audio parity remains open.

The [RTCW dynamic-light update](2026-09-17-rtcw-dlight-shadows.md) enables the
shared Q3/QL `r_dlightMode` lighting and point-shadow path for ordinary SP/MP
lights, stabilizes flickering torch projections, and admits MDS/MDC model
casters in every raster backend. Executable tests, x86 builds and bounded
`escape1` engine captures add lighting evidence; image parity and broader
renderer/gameplay gates remain open.

The [FOV/HUD restoration](2026-09-17-aspect-correction.md) restores FnQuake3's
HUD rule/dump system for Q3 and both RTCW profiles, extends the shared FOV
policy to RTCW, and leaves retail QL presentation untouched. Separate stock
SP/MP HUD presets ship in their profile namespaces. Executable layout/import
tests and windowed engine captures add presentation evidence; they do not
close the broader renderer, mod-layout, campaign, or multiplayer gates.

The [profile-scoped package update](2026-09-17-profile-packages.md) separates
engine assets under `games/<profile>/<gamedir>/` in `fntech3-pkg.fnz` and
incorporates the complete reviewed FnQuake3 package, including Team Arena.
Executable resolver tests cover all four profiles; same-binary dedicated
reads verify Q3/QL isolation and Q3 missionpack precedence. This adds package
lookup/parser evidence without closing the broader filesystem or asset gates.

The [RTCW audio-zone generation](2026-09-17-rtcw-audio-zones.md) adds sidecars
for all 32 official SP and 15 official MP maps, including SP patches and the
official MP map packs. Profile-specific material tests preserve Q3/QL behavior;
all 47 files pass parser/lookup audits. Coverage warnings remain on 24 maps,
and no listening or live OpenAL parity gate is closed by these generated assets.

The following record describes the Q3/QL merger work. References to "both
profiles" in that history mean Q3 and QL. Later RTCW evidence and remaining
blockers are recorded in the [RTCW section](#rtcw-sp-and-mp-shared-executable-implementation-in-progress);
earlier Q3/QL results must not be promoted to RTCW evidence.

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
The [UDP total-size guard](2026-09-08-download-total-size.md) rejects excess
bytes and premature EOF before writing or finalizing a file; valid downloads
from both reference servers still reach pure admission and active state.
The [server read-failure checkpoint](2026-09-08-server-download-read-failure.md)
bounds reads to the declared total and verifies explicit disconnect delivery to
both reference clients after a generated server pak is truncated mid-transfer.
The [two-client isolation checkpoint](2026-09-08-download-failure-isolation.md)
shows continued reliable-command delivery and rendering by an already active
client after a second download fails, for Q3/QL reference clients and self-pairs.
The [download write correction](2026-09-08-download-write-errors.md) prevents
UDP acknowledgement after a failed write and propagates short writes through
both HTTP callbacks, with controlled failure regressions.
Its Q3 HTTP follow-up also verifies automatic disconnect/reconnect, pure
admission and gameplay after removing pending test-script actions that delayed
the queued reconnect command.
The [QL download checkpoint](2026-09-08-ql-downloads.md) extends UDP and automatic
HTTP reconnect evidence to QL with matching add-on hashes and native pure
admission. It also records a newly observed hosted CI billing/spending block.
The [HTTP error and cURL cleanup checkpoint](2026-09-08-http-errors-and-curl-cleanup.md)
records 404/truncated-response rejection and fixes handle release before
connection reset. Successful HTTP reconnect still passes in both profiles;
error-probe shutdown remains incomplete and required forced termination.
The [manual cancellation checkpoint](2026-09-08-download-cancellation.md)
verifies removal of an observed partial file and same-process retry while the
first HTTP response is still open, for both profiles with normal shutdown.
The [download initialization cleanup](2026-09-08-download-begin-cleanup.md)
releases partial transfer state after an extension-only name is rejected,
allowing an immediately following valid command in both profiles.
The [general HTTP downloader checkpoint](2026-09-08-manual-http-downloads.md)
fixes trailing-slash URL joining and verifies manual Q3/QL split-header downloads
and same-process recovery from an empty response with temporary-file cleanup.
The [HTTP split-header correction](2026-09-08-http-split-headers.md) accepts
arbitrary body callback boundaries in both download paths, rejects incomplete
headers before rename, and passes delayed-header Q3/QL runtime probes.
The [cURL multi-stack error correction](2026-09-08-curl-multi-errors.md) stops
both HTTP polling paths after a transport-stack failure, with controlled
Windows/Linux regressions and Q3/QL HTTP success non-regressions.
The [Linux i686 HTTP checkpoint](2026-09-08-linux-http-runtime.md) verifies
runtime-loaded cURL, a matching generated Q3 add-on with UDP fallback disabled,
automatic reconnect, pure admission, rendering and normal shutdown.
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

The [map-loading checkpoint](2026-09-14-map-load-times.md) records faster
`devmap mcsarges ffa` loads, explicit asset/snapshot timing, and same-binary
Q3/QL pure sessions with Vulkan and GLx. It adds bounded decoder/cache tests
and Windows/Linux x86 asset-reader evidence. The measured reload improvements
do not close remote network, complete renderer parity or interoperability gates.

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

### Quake III Arena and Quake Live

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

### Return to Castle Wolfenstein

| Contract | Single player (`rtcw-sp`) | Multiplayer (`rtcw-mp`) |
| --- | --- | --- |
| Installation identity | `main/pak0.pk3` with SP marker evidence; shared RTCW Steam AppID 9010 where applicable | `main/mp_pak0.pk3` with MP marker evidence; same title/AppID |
| Configuration and module selection | `wolfconfig.cfg`; `sp_` archives selected and `mp_` archives excluded from the mode policy | `wolfconfig_mp.cfg`; `mp_` archives selected and `sp_` archives excluded from the mode policy |
| Preferred network/demo contract | Protocol 49; sustained wire/demo validation remains open | Protocol 60; retail peer and demo validation remain open |
| Module and state boundary | Separate SP legacy-native imports/exports and frozen SP records; campaign AI and savegame lifecycle | Separate MP legacy-native imports/exports and frozen MP records; native-module pure proof and multiplayer state |
| Maps/assets | RTCW BSP 47, MDC/MDS and RTCW materials, cinematics, audio, and navigation data | RTCW BSP 47 with mode-specific module/asset behavior and multiplayer objectives |
| Runtime availability | Gated by default; explicit development builds use the shared Windows x86 client for recorded installed probes | Same executable and development gate; switching from SP starts a new process profile |

Contract facts come from the [game-manager foundation](2026-09-14-rtcw-contracts.md)
and subsequent checkpoints below. An installation containing both modes requires
an explicit profile choice. The [RTCW gate matrix](#rtcw-gate-matrix) records
which contracts have runtime evidence and which still need closure. The
[pure-name investigation](2026-09-17-rtcw-mp-pure-names.md) establishes that the
retail MP client, unlike retail ET, needs no `sv_pakNames`; that check is the
only RTCW behaviour the ET pure work left open, and it required no change.

## Compatibility and gate matrix

Status values are `PASS`, `PARTIAL`, `OPEN`, or `BLOCKED`. A `PASS` applies only
to the evidence level named in the same cell. This matrix covers Q3 and QL;
the [RTCW gate matrix](#rtcw-gate-matrix) covers the other two profiles and is
equally required for unified sign-off.

| Surface | Q3 status/evidence | QL status/evidence | Required promotion gate |
| --- | --- | --- | --- |
| Immutable early profile selection | **PARTIAL — Unit/Build/Fixture.** Explicit aliases, marker precedence, ambiguity rejection, and immutability have tests; automatic and explicit dedicated startup pass against a legitimate installation. Canonical profile mirrors survive startup-variable replay. [Startup command overflow](2026-09-08-startup-command-bounds.md) now fails explicitly before a trailing profile override can be discarded. | **PARTIAL — Unit/Build/Fixture.** Same seam and successful automatic/explicit dedicated startup against retail assets. | Bounded known-retail pak metadata recognition now rejects filename-only placeholders and swapped archives; see the [identity checkpoint](2026-09-08-pak-identity.md). Payload integrity, broader archive variants, mixed trees, and restart behavior remain open. Q3/QL Steam discovery now has Unit/Build/Fixture evidence; see the [discovery checkpoint](2026-09-07-steam-discovery.md). |
| Project identity and packaging | **PARTIAL — Unit/Build/Fixture.** Canonical metadata and primary output names are migrated. The engine-only x86 stage passes dependency/root-archive checks and profile probes. | **PARTIAL — Unit/Build/Fixture.** The same artifacts use the profile-scoped engine package and retain bound renderer/WebUI sidecar names; the proprietary provider is excluded. | Provider staging is removed and known proprietary libraries are rejected; see the [packaging correction](2026-09-07-engine-only-packaging.md). Validate clean CI, reproducible artifacts and remaining packaging paths without renaming external contracts. |
| Filesystem, configs, pak order, and pure checks | **PARTIAL — Unit/Live.** Q3 config order and two-module pure proof are restored. Local QVM clients pass pure admission; malformed and oversized proofs have executable regressions. | **PARTIAL — Unit/Live.** Single-native-module pure proof and profile-specific config/service paths are preserved; local native clients pass pure admission. | [UDP fixture-pak downloads](2026-09-08-udp-pak-download.md) now complete byte-for-byte with pure admission in both reference directions for both profiles, baseline and impaired transport. The [interrupted UDP retry checkpoint](2026-09-08-download-retry-and-zombie-reconnect.md) verifies replacement of a retained partial file and corrects same-endpoint zombie-slot reconnect behavior in both profiles. The [whole-file pak integrity correction](2026-09-08-whole-file-pak-integrity.md) rejects short reads and CRC-mismatched payloads before exposing buffers, with both-profile corrupt/valid config and legitimate client runtime checks. The [allocation-bounds follow-up](2026-09-08-file-allocation-bounds.md) rejects unrepresentable whole-file lengths and prevents temporary-allocator alignment/header/capacity arithmetic overflow, with synthetic oversized-pak rejection in both profiles. Golden search-path/pure transcripts and clean-install probes, broader home/config/mod paths, HTTP/failure downloads, case/path behavior, streaming integrity and server-referenced paks remain required. |
| Module ABI and VM/native loading | **PARTIAL — Unit/Live.** Retail qagame, cgame, and UI QVMs load and run a local session. Guarded synthetic [cgame](2026-09-08-q3-native-cgame.md), [UI](2026-09-08-q3-native-ui.md), and [qagame](2026-09-08-q3-native-qagame.md) DLLs also pass actual legacy vmMain/syscall transfers and callbacks; qagame covers shared-entity/collision access, restart and invalid-binding recovery. The [native loopback pair](2026-09-08-q3-native-snapshot-roundtrip.md) also checks initial/updated player and entity fields in client snapshots plus a live usercmd return. Frozen POD adapters, transactional DLL loading, QVM bounds, cgame traps/extensions, UI policy, and cvar flags have executable regressions. | **PARTIAL — Unit/Live.** Retail qagame API 10 and cgame/UI API 8 load and run through the same client. Guarded synthetic native imports now exercise config/usercmd transfers through the running client; see the [native import checkpoint](2026-09-08-native-guarded-imports.md). Its [opt-in runner](2026-09-08-native-runner.md) now supports packed installs and private-desktop normal-exit verification. Structured contracts remain separate; the native snapshot's additional `0x58` bytes remain unresolved. | Renderer configuration PODs and the Q3 cgame/UI adapter are frozen; see the [glconfig checkpoint](2026-09-08-glconfig-boundary.md). Both profiles marshal frozen configstring-transfer layouts; see the [game-state checkpoint](2026-09-08-gamestate-boundary.md). Incoming bot commands now marshal Q3/QL PODs with bounds checks; see the [bot-command correction](2026-09-08-bot-usercmd-boundary.md). QL user-command outputs use the frozen 28-byte adapter at cgame/qagame boundaries; see the [user-command checkpoint](2026-09-08-ql-usercmd-boundary.md). Resolve or safely contain the QL snapshot tail, complete the boundary audit, and broaden Q3 native and packaged-client module coverage. |
| Live network codec and handshake | **PARTIAL — Unit/Live.** Local protocol-68 sessions and both directions against a freshly built clean FnQuake3 peer at the pinned baseline reach active state after pure admission (71 with fnTech3 as client, 68 with fnTech3 as server); see the [pinned-peer checkpoint](2026-09-08-pinned-q3-interop.md). Both directions also pass [controlled loopback loss, delay and duplication](2026-09-08-q3-impaired-interop.md) and [initial gamestate fragment-loss recovery](2026-09-08-q3-fragment-recovery.md). The [second-fragment checkpoint](2026-09-08-second-fragment-recovery.md) also passes and corrects earlier protocol-68-only claims. [Terminal-fragment loss](2026-09-08-terminal-fragment-recovery.md) recovers in both peer directions. Shared lineage still limits peer independence. Field parity, truncated messages, and float encodings are tested for protocols 68/71. | **PARTIAL — Unit/Live.** Local protocol-91 sessions reach active state. Both directions against the existing FnQL peer also pass [periodic loss/delay/duplication and initial gamestate-fragment loss](2026-09-08-ql-impaired-interop.md), with pure admission and native join UI/world rendering; [second-fragment loss](2026-09-08-second-fragment-recovery.md) and [terminal-fragment loss](2026-09-08-terminal-fragment-recovery.md) also recover. Reference source revision and active team gameplay remain unverified. Field parity and malformed-input tests pass; invalid usercmd batches drop only their sender without partial execution. | A [client-fragment checkpoint](2026-09-08-client-fragment-recovery.md) verifies exact, ordered delivery of six reliable commands after first-client-fragment loss in all four reference configurations. [UDP fixture-pak downloads](2026-09-08-udp-pak-download.md) also pass baseline and impaired reference transport. The [reliable acknowledgement unwind fix](2026-09-08-reliable-ack-error-unwind.md) prevents a rejected header from corrupting disconnect cleanup state. Golden packet transcripts, malformed/fuzz corpus, broader fragmentation/reliable/pure/download coverage, then bidirectional retail interop for each profile remain required. |
| Demos | **PARTIAL — Unit/Live.** New local `.dm_68` recordings play to normal completion. The [shipped-demo pacing check](2026-09-08-shipped-demo-pacing.md) also verifies normal-speed `.dm_66` playback and an engine world/HUD screenshot. Shipped four.dm_66/four.dm_68 bytes are identical, limiting their independence as wire samples. Executable tests preserve protocol-71 fallback and extension behavior. | **PARTIAL — Unit/Live.** New local `.dm_91` recordings play to normal completion after fixing an incorrect `.dm_68` suffix for QL captures without voice messages. The [retail demo checkpoint](2026-09-08-ql-retail-demo-interop.md) adds normal/timedemo cross-engine playback in both directions with complete short warmup recordings and world/HUD screenshots. | The [completion handoff checkpoint](2026-09-08-demo-completion-handoff.md) fixes premature exit when nextdemo starts another recording and verifies Q3/QL truncated-header/body completion plus intact recovery playback with the same Windows x86 client. The [invalid-length runtime checkpoint](2026-09-08-demo-invalid-length.md) verifies negative and oversized envelope rejection in both profiles, with bounded post-error liveness and explicit harness termination. The [pause/resume checkpoint](2026-09-08-demo-pause-resume.md) observes fixed QL frames and Q3 world rendering during cl_freezeDemo, followed by normal resumption; The [command-history correction](2026-09-08-paused-command-history.md) removes Q3's spurious paused connection warning, with Windows/Linux executable regressions and both-profile runtime evidence; the Q3 lagometer strip that still moves while paused moves identically in retail `quake3.exe` on the same shipped demo, so it is Q3 cgame behaviour, not an engine defect; see the [paused-lagometer record](2026-09-18-q3-paused-lagometer.md). The [retail controls comparison](2026-09-08-retail-demo-controls.md) verifies that QL restores timescale to retail's 0.1 minimum, holds cl_freezeDemo frames identically, and exercises the existing advance-and-hold console sequence under both profiles. Its paused-restart follow-up verifies retained pause, fixed first-frame rendering and eventual completion under both profiles, matching retail QL. The [drop-recovery investigation](2026-09-08-demo-drop-recovery-limit.md) confirms that ERR_DROP clears queued recovery scripts under both profiles; failed recovery attempts and premature QL control completions are recorded without claiming post-error recovery. The [RTCW demo record](2026-09-18-rtcw-demos.md) makes demo protocols profile-owned: Q3 keeps 66/67/68/71 and `.dm3` in its historical order, QL keeps 91, and neither admits RTCW's 49/60; fresh local Q3 `.dm_68` and QL `.dm_91` record, replay and timedemo runs pass with the changed client. Record and play every claimed format; broaden truncation, post-error recovery, seeking, timedemo, protocol switching, and unchanged termination behavior. |
| BSP, collision, and renderer asset decode | **PARTIAL — Unit/Live.** q3dm6 loads with GLx/Vulkan/RTX raster; the recorded RTX black-polygon defect is fixed and verified on the frozen demo. Separate Q3 x64 native probes cover baked lightmap and vertex-lit world materials. See the [RTX round](2026-09-06-rtx-round.md). | **PARTIAL — Unit/Live.** campgrounds BSP 47 loads with all three raster renderers and produces engine screenshots; the final shared RTX raster probe passes. | Broaden maps and collision paths, exercise malformed lumps, compare equivalent screenshot scenes, and prove live advertisement presentation/fallback. |
| Other assets (models, shaders, images, audio, cinematics, UI resources) | **PARTIAL — Unit/Live.** WAV decoder fixtures and a measured synthetic engine audio path pass; the remaining asset inventory is not signed off. | **PARTIAL — Unit/Live.** The same WAV checks and measured engine audio path pass; broader assets and WebUI resource regression remain incomplete. | The [WAV payload correction](2026-09-08-wav-payload-integrity.md) rejects incomplete whole loads and corrects short-read stream counts, with executable Windows/Linux regressions and exact decoded-byte comparison for 631 Q3 and 538 QL retail WAV paths. The [container-bounds follow-up](2026-09-08-wav-container-bounds.md) rejects truncated/out-of-container headers and checks packed/loose seek results, while preserving terminal odd-data acceptance observed in stock Q3 assets. It repeats the identical corpus output and clarifies that the installed Q3 corpus includes 18 add-on paths. The [extensible identity correction](2026-09-08-wav-extensible-identity.md) validates full PCM/float GUIDs and extension lengths, with malformed GUID fixtures and eight exact synthetic decodes. Valid-bit/channel-layout semantics and audible/backend validation remain open. The [engine audio-output checkpoint](2026-09-08-engine-audio-output.md) measures silence controls, 997 Hz output and half-volume gain response through the same client under both profiles, using file output without speakers. The [streaming-music checkpoint](2026-09-08-engine-music-streaming.md) adds PCM16/24/float32 intro-loop handoff, channel routing, stop-to-silence and malformed-track recovery through both running profiles. The [rate-conversion investigation](2026-09-08-stream-resampling-gap.md) exposed per-chunk frame/phase drift. The [persistent resampling correction](2026-09-08-persistent-stream-resampling.md) now passes exact partition comparisons, queue/fallback checks, both-profile rate conversion and mixed-rate stop/replay probes; broader raw/device lifecycle and performance coverage remain open. The [PCM8 WAV correction](2026-09-08-wav-pcm8-stereo.md) normalizes unsigned multichannel WAV bytes at the codec boundary, preserving raw-module conventions and correcting the measured stereo silence/tone defect in both profiles. The [multichannel endian correction](2026-09-08-wav-multichannel-endian.md) swaps every complete PCM16 scalar sample across channels and partial frames, with failing-before simulated-endian regressions and Linux sanitizer coverage; full big-endian runtime remains unverified. The [extensible precision correction](2026-09-08-wav-extensible-precision.md) rejects precision larger than its container, preserves reduced-precision PCM scaling and verifies malformed-track recovery followed by measured intro/loop output in both profiles; the speaker-mask correction is recorded below. The [speaker-mask investigation](2026-09-08-wav-speaker-mask-gap.md) recorded center-to-right misrouting. The [speaker-routing correction](2026-09-08-wav-speaker-routing.md) preserves internal mask metadata, mixes non-native layouts by speaker position in both backends, aligns music reads to whole frames, and verifies both-profile center/control, static, layout-transition, six- and three-channel file output; hardware surround and legacy DMA output remain unverified. The [loaded-speaker bounds follow-up](2026-09-08-speaker-buffer-bounds.md) validates source ranges before routing and avoids metadata-size overflow after rejected conversion, with a failing-before executable regression and repeated both-profile loaded-center output. The [SDL legacy-output checkpoint](2026-09-08-sdl-legacy-output.md) verifies both-profile streamed/static speaker routing and seven-channel file output on a separately built SDL x86 engine, while recording persistent silence gaps as an open continuity issue; native hardware output remains unverified. The [legacy timing trace](2026-09-08-legacy-timing-trace.md) links the recurring 50 ms gap component to the cursor offset and records near-half-second update intervals despite at most 5 ms of pre-paint work; timing defaults remain unchanged and continuity remains open. The [client-stage trace](2026-09-08-client-screen-timing.md) places measured long stalls inside screen updates under both profiles; UI/render/presentation attribution and continuity remain open. The [renderer-stall investigation](2026-09-08-renderer-stall-localization.md) measures delays at OpenGL synchronization points and removes them in a front-buffer control on the private desktop; normal presentation and continuity remain unresolved. The [converted WAV short-read correction](2026-09-08-wav-short-read-alignment.md) preserves scalar alignment across positive filesystem short reads, with failing-before partition regressions, sanitizers and unchanged both-profile corpus output. The [native WAV follow-up](2026-09-08-native-wav-short-reads.md) accumulates positive short reads for native PCM as well, with simulated-endian regressions and exact streaming reconstruction of all 1,169 installed WAV paths under 509-byte filesystem reads. The [Ogg payload correction](2026-09-08-ogg-payload-bounds.md) validates decoded-size metadata before narrowing and rejects incomplete whole loads, with executable fault/cleanup regressions and both-profile retail-fixture smoke output; callback and broader Ogg corpus gates remain open. The [Ogg callback correction](2026-09-08-ogg-callback-bounds.md) bounds read/seek arithmetic and verifies real positions, with exact decoded comparison across 559 retail Ogg paths and both-profile compressed-pak playback smoke evidence; broader malformed-container gates remain open. The [Vorbis speaker-order correction](2026-09-08-vorbis-speaker-order.md) fixes observed center-to-right routing, preserves encoded-field tags and passes both-profile three/six-channel loaded/music file-output probes under OpenAL and SDL legacy; wider hardware and layout coverage remains open. The [older OpenAL fallback correction](2026-09-08-openal-unmatched-speakers.md) preserves unmatched speakers when direct remix is unavailable, with failing-before Linux center-channel output, corrected loaded/music measurements and both-profile Windows regression evidence. Per-format fixtures plus windowed runtime scenes for both profiles; confirm error handling and cross-renderer/audio consistency without redistributing assets. |
| WebUI native requests and pending avatars | **PARTIAL — Source.** QL-only activation and frame guards are preserved; this port does not enable WebUI for Q3. | **PARTIAL — Unit/Build/Fixture.** Notification scheduling, bounded Unicode transport, avatar backoff/batching, and reload cleanup are tested; fnTech3's production adapter passes an offscreen retail-runtime probe. See the [port record](2026-09-06-webui-request-port.md). | Confirm affected Linux/Proton gameplay performance, live Steam-avatar callbacks through the engine, and windowed renderer/lifecycle stress. The source-reported DataPak document-query limitation remains open. |
| Renderer API/lifecycle | **PARTIAL — Unit/Live.** All three modules initialize, render a local session, and shut down. Q3 x64 native RT survives renderer restart and windowed resize with Vulkan validation; that evidence applies to the diagnostic Q3-only executable. | **PARTIAL — Unit/Live.** All three modules initialize, render the native join UI/world, and shut down with WebUI disabled. Native RT remains unverified: this machine's x86 driver lacks required extensions. | Shared x86 native RT, broader renderer switch/restart and screenshot parity; HDR, performance, and WebUI resize/device-loss/crash stress. RTX raster startup does not prove ray tracing. |
| Steam and optional services | **PARTIAL — Unit/Build/Fixture.** AppID 2200 discovery supplies the Q3 base path without enabling QL services or adding a second Q3 Steam search tree. | **PARTIAL — Source/Unit/Live.** The [retail QL checkpoint](2026-09-08-ql-retail-auth-interop.md) observes platform authentication validation, pure admission and native join UI/world with the retail client on fnTech3. The [dedicated-server follow-up](2026-09-08-ql-dedicated-retail-interop.md) adds fnTech3 client admission and first-person warmup against the Linux x86 retail server. Broader authentication gates remain open. | Broader Q3/QL Steam-path validation; QL logged-in/offline/failure behavior; session-ticket admission both ways; callback/shutdown fault tests; live match reports, unattended Workshop download, and sustained GameServer stress. |
| Dedicated server | **PARTIAL — Build/Unit/Fixture.** [Retail bot combat](2026-09-08-retail-bot-runtime.md) now passes with the corrected incoming command boundary. q3dm6, retail qagame QVM, botlib, pure=1 info response with protocol 68, and clean shutdown pass for automatic and explicit profile selection. | **PARTIAL — Build/Unit/Fixture.** campgrounds/ffa, retail API-10 qagame, botlib, pure=1 info response with protocol 91, and clean shutdown pass using the same binary. | Windows x86 and native Linux i686 discovery/map probes now pass for both profiles; see the [portability checkpoint](2026-09-07-build-portability.md). Windows dedicated early and initialized fatal errors now exit with status 1 without console dismissal; see the [fatal-exit checkpoint](2026-09-08-windows-dedicated-fatal.md). The [retail bot checkpoint](2026-09-08-retail-bot-runtime.md) admits four QL bots. The [follow-up](2026-09-08-bot-trace-and-probe-corrections.md) corrects command-queue interference, localizes timestamp-only logging to the retail DLL and fixes botlib no-entity traces; the subsequent [world-link restart fix](2026-09-08-world-link-restart.md) passes a shrinking native entity-binding regression and a free-running retail QL restart with four bots, IN_PROGRESS state and a scored frag. A subsequent [retail transition series](2026-09-08-retail-transition-series.md) passes three explicit restarts and two map changes per profile with four bots at all six checkpoints; QL reports IN_PROGRESS throughout those samples. The [bot-profile default checkpoint](2026-09-08-bot-profile-defaults.md) corrects QL bot_grapple to the retail engine default 1 while retaining Q3 default 0 and explicit overrides. The [retail QL restart checkpoint](2026-09-08-retail-ql-map-restart.md) verifies one connected warmup restart per retail direction, preserving authentication and rendering after restart. The [full retail map-change checkpoint](2026-09-08-retail-map-changes.md) verifies one destination-map reload and active admission per retail direction under both profiles. Longer transition stress remains open. Connect retail clients, exercise pure admission/live messages, and broaden long/faulted sessions. A connectionless pure=1 response does not prove pure negotiation. |
| Cross-profile isolation | **PARTIAL — Unit/Fixture.** Active profile is immutable; config read and write select `q3config.cfg`. Both profiles start sequentially from the same staged distribution with isolated homes. | **PARTIAL — Unit/Fixture.** Active profile is immutable and QL's `qzconfig.cfg`/`repconfig.cfg` save path is preserved. | Broaden restart, config persistence, cached-module, download, Steam-state, protocol-state, and sidecar isolation checks. |
| Release/build matrix | **PARTIAL — Build/Fixture.** [Clean Windows x86 MSVC build at 4383039](2026-09-08-clean-windows-4383039.md) completes all 1,152 steps and passes the full suite (110 checks, one skip), including native-module runtime opt-ins. Staged client/dedicated/renderer probes pass their bounded startup checks. A separate diagnostic Q3-only x64 configuration builds and exercises native RT. | **PARTIAL — Build/Fixture.** The same clean x86 artifacts also pass QL HTTP download/reconnect, pure admission and native join UI/world checks. Earlier startup/demo/RTX raster evidence remains scoped to its recorded artifacts. The Q3-only x64 executable rejects QL and cannot establish this release contract. | The [Linux filesystem/package refresh](2026-09-08-linux-filesystem-package.md) clean-builds d5c7fbd, passes 109 targets with two skips, and verifies extracted Q3 client/QL dedicated plus corrupt/oversized pak rejection. Its six ELF files require GLIBC_2.38 and fail the configured GLIBC_2.31 release baseline; local runtime and archive-layout success do not close that portability gate. The [Ubuntu 20.04 follow-up](2026-09-08-glibc-baseline-build.md) produces new i686 binaries requiring at most GLIBC_2.29, passes the baseline checker and complete suite, and verifies extracted Q3 client/QL dedicated runtime inside the older userspace. It also corrects statistics decimal rounding and test-harness compiler compatibility. The [post-audio Linux baseline refresh](2026-09-08-linux-audio-build-refresh.md) clean-builds 2c33a2b with no source changes, passes 108 targets with one skip, verifies all six binaries against GLIBC_2.31 and repeats Q3 pure-join rendering plus QL dedicated startup from an unchanged deterministic 268-file package. The [post-codec Linux refresh](2026-09-08-linux-post-codec.md) clean-builds commit 218ab89 in 928 steps, passes 109 targets with two skips and no failures, and repeats Q3 pure-join rendering plus QL dedicated startup using the new binaries. The [current Linux i686 checkpoint](2026-09-08-current-linux-i686.md) builds client/server and all renderer modules, passes 109 checks (2 skips) after making a release-notes test archive-portable, and verifies Q3 GLx client plus QL dedicated runtime. Earlier Make/CMake and bundled-SDL evidence remains scoped to its recorded commits. Windows MSVC/MSYS2 x86 release validation also passes; see the [reference-peer checkpoint](2026-09-07-q3-reference-interop.md). The [current Windows package checkpoint](2026-09-08-current-windows-package.md) stages the latest filesystem/allocation corrections, validates all evidence links and six PE dependencies, produces identical repeated archives, and passes both profiles from the same unchanged 224-file extraction. The [local Windows package checkpoint](2026-09-08-windows-package-runtime.md) validates extracted same-binary Q3/QL runtime and deterministic ZIP creation from fixed inputs; the [packaged evidence correction](2026-09-08-packaged-evidence-documents.md) now includes fnTech3 Markdown/JSON evidence and validates its internal file links. Reproducible compiler artifacts, broader packaged runtime gates and visual/service failures remain open; see the [portability checkpoint](2026-09-07-build-portability.md). |

## Current blockers

### RTCW SP and MP: shared-executable implementation in progress

RTCW SP and MP target the same `fntech3.exe`, with two immutable mode profiles
under one title. The [game-manager/contract foundation](2026-09-14-rtcw-contracts.md)
adds catalog and Steam discovery, mode-specific configuration/archive policies,
verified external PODs, canonical state adapters, larger profile-owned storage
limits, native-module search/call tables, transactional cgame state transfers
and source-compared protocol 49/60 message bodies. A validated controller now
handles RTCW's menu mode commands by relaunching the same executable; synthetic
Windows/Linux process tests exercise both directions without game input.
**Default RTCW startup remains gated; full gameplay and compatibility sign-off
remain open.** An explicit Meson `-Drtcw-development=true`
build permits isolated developer probes through the same shared executable and
prints an experimental-build warning. It does not close any compatibility gate.
Native SP/MP UI probes now pass cache imports, explicit VM pointer conversion
and basic sound registration. MP translation and its two-argument background
music import are integrated; the legacy updater reports its service unavailable.
The [SP music/menu checkpoint](2026-09-15-rtcw-music-menus.md) adds a shared
OpenAL/legacy music controller and exact SP music/fade imports. Both modes now
boot their legitimate native main menus through the same `fntech3.exe`, with
inspected textured English SP/French MP captures in GLX, Vulkan and RTX raster.
Both initialize muted audio; OpenAL queues decoded music. Dynamic-light
submission, projected passes and model-lighting policy have source/fixture
evidence. World-light/fog rendering, measured audio parity and gameplay remain
open.

The [server-world checkpoint](2026-09-15-rtcw-server-world.md) adds checked
world/entity imports and profile-owned server defaults, including SP campaign
AI slot allocation. MP's installed map passes initial entity spawning. Both
installed map probes advanced past import 202 through the
[bot-library bootstrap checkpoint](2026-09-15-rtcw-botlib-bootstrap.md). At that
checkpoint SP stopped at import 206 (`BOTLIB_LOAD_MAP`). The
[navigation-world checkpoint](2026-09-15-rtcw-navigation-worlds.md) adds two-world
ownership and area blocking, corrects MP's bot default, and advances MP through
server initialization, sound/entity parsing and weapon/model loading. The
[client-animation checkpoint](2026-09-15-rtcw-client-animation.md) fixes partial
foot-tag queries and the animation-speed handoff. The
[MP pure-admission checkpoint](2026-09-15-rtcw-mp-pure.md) gives the game manager
an explicit pure-proof policy and verifies both native module paks. MP now
reaches `CS_ACTIVE` and receives its first local map snapshot. The
[fog-controller checkpoint](2026-09-15-rtcw-fog-controller.md) adds shared
SP/MP state transitions and renderer API 23; active GPU fog remains guarded.
The [spatial-audio checkpoint](2026-09-15-rtcw-spatial-audio.md) adds checked
entity/listener imports. The [MP loop checkpoint](2026-09-15-rtcw-mp-loop-audio.md)
adds range/volume loops to both audio backends and the interpolated viewpoint.
The [limbo/cinematic checkpoint](2026-09-15-rtcw-limbo-cinematics.md) adds chat
history and cinematic imports, fixes movie-file shutdown ownership, and passes
MP local boot through the deployment-menu briefing movie with normal exit in
all three renderers. Its initial visual defects are corrected by the
[material-directive checkpoint](2026-09-15-rtcw-material-compression.md), whose
three inspected captures show textured deployment menus and briefing movies.
The menu obscures the world, so this does not establish playable-map support.
The [live AAS8 checkpoint](2026-09-15-rtcw-aas-loading.md) connects the validated
reader to owned navigation arrays and two-world map loading. The installed SP
module loads both `escape1` hulls. All 70 installed AAS files pass record conversion
checks for both profiles/worlds. The [BSP-query checkpoint](2026-09-16-rtcw-bsp-queries.md)
adds checked entity/property queries with source-compared string padding and
numeric parsing. At that checkpoint SP stopped at import 205 (`BOTLIB_START_FRAME`).
The [routing-cache checkpoint](2026-09-16-rtcw-routing-caches.md) adds bounded
RCD12/15 decoding, topology validation and per-world native cache/visibility
ownership. All 70 installed SP caches pass reference-reader and engine-loader
comparisons; MP version selection uses synthetic headers. The
[routing-initialization checkpoint](2026-09-16-rtcw-routing-initialization.md)
connects import 205 to both worlds, profile-specific slope costs and bounded
routing tables. All 70 installed navigation pairs pass binary32 source
comparisons; extended x87 reference arithmetic has a documented difference.
The [startup-contract checkpoint](2026-09-16-rtcw-startup-contracts.md) adds
selected-world queries, empty item updates, profile-owned appearance defaults
and SP's native animation-model handoff. Original SP's disabled zombie-hit
import is honored; iortcw's enabled effect remains a distinct open contract.
At that checkpoint SP stopped at cgame import 34 (`CG_S_CLEARLOOPINGSOUNDS`).
The [SP loop-audio checkpoint](2026-09-16-rtcw-sp-loop-audio.md) adds both-backend
reset modes, music preservation and source-compared loop/mixer fade stages.
The game manager now owns pure-mode defaults and flags: original SP defaults
off, MP on, with explicit settings preserved. SP pure verification remains
explicitly unavailable. With pure mode off, installed SP initializes both
`escape1` worlds, loads animation models and receives its first local snapshot;
observed boundaries are cgame import 31 (`CG_S_STARTSOUND`) and game import
207 (`BOTLIB_UPDATENTITY`) in separate runs.
The [one-shot-audio checkpoint](2026-09-16-rtcw-one-shot-audio.md) adds checked
SP/MP plain, extended-flag and local starts; both backends share source-compared
cutoff/reuse/FIFO rules and linear spatial gains with bounded exhaustion.
Legacy dynamic positions survive sound clears and update independently of
loop submissions. Both final SP audio runs receive an `escape1` snapshot and
stop at game import 207 (`BOTLIB_UPDATENTITY`); MP briefing captures remain valid
with both backends. The [AI entity checkpoint](2026-09-16-rtcw-ai-entities.md)
adds checked entity updates, world-zero shared records with explicit area-link
ownership, and profile-owned AI client-slot allocation/free. Original and
iortcw SP/MP each pass 153,334 update/record/callback comparisons on Windows
x86/x64 and Linux x86 sanitizers. Installed SP now passes entity updates and
client allocation, reaching game import 546 (`BOTLIB_AI_ALLOC_GOAL_STATE`).
The subsequent [AI state checkpoint](2026-09-16-rtcw-ai-state.md) implements
checked goal/movement allocation, initialization, reset and free, with SP's
128-state capacity and MP/Q3/QL's 64-state capacity. Defined original/iortcw
behavior passes 524,288 initialization and 12,288 reset comparisons per mode,
reference and target; iortcw's bounded last-avoidance reset is separately
verified. SP now passes import 546 and exits cleanly after a mission-briefing
capture. A longer probe still reports `cg_norender=1`; initial cast/save-load
readiness is unresolved. MP briefing and all six Q3/QL renderer smoke checks
pass with inspected engine captures. No SP playable-map capture is claimed.
The subsequent [cast/action checkpoint](2026-09-16-rtcw-cast-actions.md)
identifies repeated AI reliable-command overflows in those earlier briefing
logs; their boot-pass labels did not establish cast readiness. The harness
now rejects that diagnostic. Checked cast classification restores RTCW's
broadcast/configstring exclusions, and all 28 elementary-action imports plus
AI command delivery are implemented. Original/iortcw SP/MP each pass 344,099
action comparisons per target. SP now advances to unsupported import 308
(`BOTLIB_AAS_POINT_AREA_NUM`) without those overflows; MP's inspected deployment
capture still passes. Campaign readiness and play remain open.
The subsequent [AAS/tag checkpoint](2026-09-16-rtcw-aas-tags.md) implements
five checked basic navigation queries and the server-to-cgame tag callback.
Each original/iortcw SP/MP reference passes 527,849 query comparisons per target.
The installed SP game now spawns all 29 soldiers, sends `rockandroll` and
displays its normal ready arrow. It then pauses in the pregame menu; no input
or playable-world capture is claimed. MP and all six Q3/QL renderer checks
still pass with inspected engine captures.
The [cast lifecycle checkpoint](2026-09-16-rtcw-cast-lifecycle.md) adds SP-only
snapshot and packet-timeout exemptions plus silent cast disconnect notices.
Executable tests cover all four layouts, callback flag changes, cleanup,
authentication, admission deadlines and zombie reclamation. Sustained cast
behavior in a playable campaign remains unverified.
The [area-trace checkpoint](2026-09-16-rtcw-aas-traces.md) adds checked import
309 with owned endpoints, bounded staged output arrays and nonlocal-error
cleanup. Original/iortcw SP/MP traces each pass 786,560 comparisons per target;
shared Q3/QL positive-capacity behavior and safe empty traversal are covered.
The [route-query checkpoint](2026-09-16-rtcw-route-queries.md) adds checked
import 317, RTCW goal costs and per-link update limits, portal first-step
selection, and RTCW cache expiry. Original/iortcw SP/MP each agree on 66,176
synthetic route queries and 17,920 queries over the 70 installed navigation
pairs on Windows x86/x64 and Linux x86 with sanitizers. The MP corpus uses
synthetic RCD12 headers over SP data.
These are source/fixture comparisons, not live AI or retail peer evidence.
The [visibility checkpoint](2026-09-16-rtcw-visibility.md) connects cached
RCD visibility and SP import 323, with distinct SP and MP fuzzy-area searches.
Original/iortcw references agree on 10,816 synthetic visibility queries,
87,552 fuzzy queries and, for SP, 32,768 route-position queries. Each also
passes visibility and fuzzy checks over all 70 installed navigation pairs;
SP adds 17,493 route-position comparisons with routing endpoints. The 427
non-routing endpoint cases are explicitly excluded from route parity because
the engine's existing safety guards reject them earlier. Windows and Linux
are each compared with their own reference binaries; cross-platform numeric
identity and sustained live tactical behavior remain open.
The [cast-query callback checkpoint](2026-09-16-rtcw-cast-query-callbacks.md)
adds the server-to-game callbacks for hide and attack queries. Both modes use
their exact five-argument exports 11 and 12, with owned position vectors and
explicit native-module availability. Synthetic native dispatch, nested calls
and profile isolation are verified.
The [tactical navigation checkpoint](2026-09-16-rtcw-tactical-navigation.md)
implements imports 321/322 with original SP/MP frame budgets, flags, occupancy
and entity-area policies. Per reference, 4,096 hide and 4,096 warm-cache attack
queries agree with full callback arguments. Original-game normalization and
iortcw MP's disabled occupancy are explicit comparison conditions. Cold-cache
diagnostics differ in 215 of 4,096 attacks and retain subsequent route
differences; retail investigation and live callback use remain open. Search
storage is independent of route-cache work lists and survives nonlocal errors
through map-owned cleanup. Complete tests, both builds, SP/MP briefing probes
and six Q3/QL renderer checks pass; this does not establish live campaign AI.
The [movement-prediction checkpoint](2026-09-16-rtcw-movement-prediction.md)
adds imports 319/320, the exact 80-byte prediction record and original SP/MP
physics/event policies. Both game ABIs use 10-bit entity numbers; cast/tactical
guards now use that limit instead of SP's larger internal botlib allocation.
Each original reference agrees on 7,267 defined synthetic predictions under
strict SSE arithmetic. Original start-solid undefined behavior is excluded;
default Linux x87 strict arithmetic still fails exact numeric agreement, and
fast-math diagnostics show drift without changed decisions in this corpus.
iortcw deliberately differs in friction and normalization.
The [installed-prediction checkpoint](2026-09-16-rtcw-installed-prediction.md)
compares 64 distinct installed AAS members with their matching BSP contents.
Each original mode agrees on 36,771 defined predictions with strict Clang
Windows x86/x64, MSVC x86 and Linux x86 SSE2 sanitizer builds. Original undefined
trace paths remain excluded. The broader fast-math diagnostic has one changed
decision per original reference, so unchanged decisions are not a general
guarantee. Dynamic entities, original BSP/retail numeric parity and live AI
remain open; MP policy uses SP geometry in this fixture.
The [entity-collision checkpoint](2026-09-16-rtcw-entity-collision.md) corrects
RTCW target-capsule selection for box and point queries through an immutable
profile policy. Each original/iortcw selector agrees in 1,024 cases across
Windows x86/x64 and sanitized Linux x86. Actual server/AAS fixtures verify
external entity layouts, nearest hits, pass filtering and both navigation
worlds, with synthetic collision backends. Physical collision and live
moving-entity prediction remain open. Both builds, the full suite and eight
inspected startup/renderer captures pass; Q3/QL shape selection is unchanged.
The [movement-area checkpoint](2026-09-16-rtcw-movement-areas.md) admits checked
import 553, preserves SP-only disabled-area behavior and RTCW model-travel
selection, and rejects invalid entity/model accesses or incomplete worlds.
Each original/iortcw reference agrees on 32,256 movement queries plus direct
area/model queries across Windows x86/x64 and sanitized Linux x86. Synthetic
BSP ground responses isolate lookup policy; active campaign movement remains
unverified.
The [directional-movement checkpoint](2026-09-16-rtcw-directional-movement.md)
admits checked import 550 and preserves RTCW grounded-flag, slime, normalization
and setup-setting behavior. Each original/iortcw reference agrees in 51,200
queries under strict Windows x86/x64 and sanitized Linux x86 SSE2 arithmetic,
using shared engine AAS/prediction/action services. Fast math changes 25,744
combined records but no success decisions in this corpus; this does not
supersede the installed prediction diagnostic's decision difference. Failed
prediction is rejected before subsequent actions. Builds, the full suite and
eight inspected startup/renderer captures pass; active AI remains unverified.
The [movement-view checkpoint](2026-09-16-rtcw-movement-views.md) adds checked
imports 554/573, a frozen goal layout and distinct original SP/MP next-link
selection. It preserves partial targets on failure, original distance/travel
rules and visible-position semantics, with bounded traversal and checked
storage. Original SP/MP agree in 71,712 synthetic selector/view cases each
under strict Windows x86/x64 and sanitized Linux x86 SSE2 arithmetic; iortcw signature/normalization and
fast-math differences remain explicit diagnostics. Module boundaries, focused
tests, both builds, the full suite (111 Meson tests and 1,436 Python tests, with
two and 11 skips respectively), and eight inspected engine captures pass.
Live actor movement remains unverified.
The [travel-execution checkpoint](2026-09-16-rtcw-travel-execution.md) adds
original SP/MP policies for 22 travel routines. Each original agrees in
126,720 synthetic cases under strict Windows x86/x64 and sanitized Linux x86
SSE2 arithmetic, including actions, state changes and defined result fields.
Undefined original output words are excluded explicitly; shared engine
services and controlled BSP responses limit the evidence. Compiler/iortcw
numeric differences remain diagnostics, with no compared decision changes.
Both builds, 14 focused tests on three targets, the full suite (111 Meson and
1,440 Python tests, with two and 11 skips), and eight inspected captures pass.
Active actor travel remains unverified.
The [movement-to-goal checkpoint](2026-09-16-rtcw-movement-to-goal.md) adds
checked import 549 with a frozen 52-byte result, explicit goal/result bit
translation, owned buffers and validation. Shared orchestration retains
original route lifetime, jump-pad recovery and distinct SP/MP future views;
ladder fallback and traversal bounds handle partial or malformed state.
Original SP/MP agree in 18,432 synthetic cases each under strict Windows
x86/x64 and sanitized Linux x86 SSE2 arithmetic. iortcw has explicit decision
differences; original fast-math differences are numeric in this corpus.
Both builds, 18 focused tests on three targets, the complete suite (111 Meson
and 1,446 Python tests, with two and 11 skips), and eight inspected captures
pass. Active actor movement remains open.
The [jump run-up checkpoint](2026-09-16-rtcw-jump-runstart.md) adds RTCW
normalization/slime policy and stops jump actions on failed prediction.
All four original/iortcw references agree in 9,476 defined helper cases each
under strict Windows x86/x64 and sanitized Linux x86 SSE2 arithmetic, with
960 failed predictions and four nonfinite reference velocities excluded
explicitly. Fast-math differences and its six excluded velocities are
recorded separately. Both builds, 13 focused tests on three targets and eight
inspected captures pass. The complete suite passes 111 Meson tests (two
skips), including 1,449 Python tests (eleven skips).
The [entity-stream checkpoint](2026-09-16-rtcw-entity-streams.md) adds checked
start/stop imports, a shared pool/decoder, both-backend queued replacement and
SP entity stops/fades. Original-source allocation, stop and fade comparisons
agree on Windows x86/x64 and sanitized Linux x86; 26 focused audio tests pass
on all three. Both builds, four muted SP/MP stream probes and six Q3/QL renderer
probes pass, with all ten engine captures inspected. The complete suite passes
111 Meson tests (two skips), including 1,455 Python tests (eleven skips).
SP entity loops intentionally stay on their entity lane, correcting the
released source's music-channel bug. PCM and live-dialogue parity remain open.

At the entity-stream checkpoint, voice amplitudes and live dialogue,
missing-cache generation and AI, letterboxed movies, active GPU fog and
broader gameplay/import coverage remained open. Later startup, material and
speech evidence is recorded below; campaign play and save/load remain unverified.

The [first approved SP ready-click probe](2026-09-16-rtcw-autosave-write.md)
reached the game-owned player-start action, then failed its initial autosave:
the engine discarded the write byte count and returned zero. The adapter now
preserves RTCW game/cgame write results and admits bounded, non-truncating
save-file replacement. Focused tests and both builds pass. The first approved
post-fix retest wrote matching 218,188-byte autosave and current-save files,
then hit the explicit missing scene-fog GPU guard. Its engine screenshot shows
that error dialog; playable SP and save restoration remain unverified. The
full suite passes (111 Meson tests/two skips, 1,457 Python tests/eleven skips).
At that checkpoint, ordinary RTCW menu mouse-coordinate adaptation
also remained open; that test delivered its one approved click directly to
the native UI using RTCW's relative-delta contract. The subsequent
[profile-specific menu mouse correction](2026-09-17-menu-mouse.md) restores
relative UI/cgame motion for Q3 and RTCW across SDL, Win32 and X11, while
preserving QL absolute input. Source, executable unit fixtures and Windows
builds cover that correction; live menu hover/click validation remains open.

Subsequent [approved startup retests](2026-09-16-rtcw-coronas.md) passed the new
GL/GLSL scene-fog path, silent-backend voice response, RTCW XYZST decal boundary
and corona imports in sequence. Retest 5 then stopped at `CG_LOADCAMERA` (98).
All five approved startup clicks are consumed, and their engine screenshots
remain error dialogs. At that checkpoint, SP had no capture after the intro.
Active voice amplitude and save restoration remain open; Vulkan/RTX retain
the active scene-fog guard. The decal POD probes now pass 2,215 assertions per
SP reference and 2,205 per MP reference on Windows x86/x64. Current default and
development client/server/renderer builds and seven focused Linux x86
sanitizer checks pass; all six post-change Q3/QL renderer captures were inspected
and audited. The full suite passes 111 Meson tests with two skips, including
1,465 Python tests with eleven skips (529.459 seconds).

The [camera implementation](2026-09-16-rtcw-cameras.md) adds an owned 64-slot
manager, bounded file parsing, fixed/interpolated/spline evaluation, target and
FOV events, and checked camera imports. Original SP and MP timing policies are
selected by profile; iortcw MP's use of SP timing is recorded separately.
Final source comparisons cover 320,880 sample pairs per Windows x86/x64
architecture, with exact original-SP and finite original-MP results; undefined
MP outputs and iortcw numerical differences are recorded separately. A new,
separately authorized 90-second windowed `escape1` test progressed through ten
camera commands and reached the first-person knife/health-HUD view with a valid
snapshot, camera mode cleared and no engine error. All four engine screenshots
were inspected. Character/attachment defects and missing skin-shader warnings
remain; the test used disabled audio and no movement/combat input. The sole
camera-test click is consumed. Full SP/MP gameplay compatibility remains open.
The final camera suite passes 111 Meson tests with two skips, including 1,470
Python tests with eleven skips (511.371 seconds). Nine focused Windows x86/x64
and Linux x86 sanitizer tests pass, and all six final Q3/QL renderer captures
were inspected and audited against the final ordinary executable.

The [RTCW UI browser query boundary](2026-09-17-rtcw-ui-browser.md) now dispatches
the supported LAN query imports and translates SP/MP list IDs explicitly. The
installed MP Join Server menu opens without its prior unsupported-import-75
error. One focused RTCW boundary test and 32 shared UI tests pass, and the
Windows x86 client rebuild passes. The 1600x1200 no-CSM screenshot set includes
SP after the intro and MP first-person local warmup; all selected runs report
`r_picmip 0`. This adds menu and rendering observations, without closing remote
browser/join, favorites editing, sustained gameplay or interoperability gates.

The [character-material correction](2026-09-17-rtcw-character-materials.md)
adds SP character mip selection, RTCW conditional texture alternatives, MP's
`nomipmap` alias and normal-based vertex alpha across all three renderers.
The alpha evaluator agrees on 819,000 cases per original/iortcw SP/MP reference
on Windows x86/x64 and sanitized Linux x86. Installed SP/MP briefing probes
report zero unknown shader directives. The SP opening cinematic now shows
assembled character heads/bodies and reaches first-person startup; `r_picmip`
and `r_picmip2` remain zero. Six final Q3/QL raster-renderer regression captures
were inspected. The complete Windows x86 development suite passes 111 Meson
tests with two skips, including 1,471 Python tests with eleven skips.
Broader character/material GPU parity, campaign gameplay and
interop remain open.

The [activation/pickup/eyes correction](2026-09-17-rtcw-player-interactions.md)
adds RTCW activation bindings and lean/any-key encodings, publishes the pickup
preference to the server, and applies SP/MP blink rules to MD3/MDC attachments
as well as MDS models. Failing-before regressions cover all three renderers;
windowed installed probes show eyes in GL/Vulkan/RTX raster and verify default
and explicitly disabled automatic-pickup userinfo. Live door/weapon interaction,
native ray-tracing GPU output and broader gameplay
remain open.

The [RTCW eye-rendering follow-up](2026-09-17-rtcw-eye-rendering.md) corrects
Vulkan descriptor state lost across bloom, which could remove a repeated head
draw while leaving its eyelids visible. It also aligns RTCW MD3 packed normals
with the already-correct MDC decoder in all raster backends and native RTX
geometry, preserving Q3/QL calculations. Failing-before regressions, Windows
and Linux tests, installed SP/MP head captures and an SP cinematic eye
transition add evidence; all-character original-engine and native RT GPU
parity remain open.

The [engine command audit](2026-09-17-rtcw-engine-commands.md) accounts for 183
original SP/MP command names and adds the remaining named action bindings,
notebook/help, UI/translation reload, limbo chat, recommended settings and SP
music queue dispatch. Executable regressions cover profile ownership, button
values, input recovery and QL-only chat export isolation. Same-client installed
SP/MP startup probes verify registrations and safe utility dispatch. Optional
authoring tools and legacy integrations are explicitly classified; MP master
match reporting returns an honest unavailable diagnostic. Actual gameplay and
broader compatibility gates remain open.

The [transparent-fog correction](2026-09-17-rtcw-transparent-fog.md) makes RTCW
additive and filter passes fade toward their blend-neutral color. Compiled
compositing regressions fail before and pass after in all three backends; nine
fog tests pass on Windows/Linux x86. Inspected installed-SP engine captures
show the torch rectangles removed in GL, Vulkan, and RTX raster. RTX's overall
fog brightness, original-engine visual parity, and native RT runtime evidence
remain separate open items.

The [speech checkpoint](2026-09-17-rtcw-speech.md) removes the active-SP
voice-amplitude guard with real legacy/OpenAL sample measurements and preserves
MP's disabled talking-animation result. The measurement probe agrees with
299,264 cases per original/iortcw SP reference on Windows x86/x64 and sanitized
Linux x86. RTCW also owns its silent default sound, fixing a legacy intro
failure caused by the missing Q3 fallback asset. Both audio backends complete
the installed SP intro with nonzero speech results and inspected first-person
captures; output was focus-muted, so audible synchronization remains open.
Six ordinary-binary Q3/QL/MP audio startup checks and both Windows x86 builds
pass. The full development suite passes 111 Meson tests with two skips and
zero failures, including 1,475 Python tests with eleven skips. The 28 focused
audio checks also pass on Windows x64 and sanitized Linux x86. Original
device/PCM timing, facial synchronization, sustained playback and full gameplay
remain open.

The [startup/movie checkpoint](2026-09-17-rtcw-cinematic-presentation.md) moves
default movie selection into the immutable profile catalog, admits SP's
letterbox flag/console mode and header frame rate, and repairs the shared
first-frame clock transition. Windowed engine captures show the SP film through
48 seconds on GL, Vulkan and RTX raster. Installed MP's absent startup movies
fall through to its menu without changing archive isolation. Movie audio,
complete codec validation and original-engine parity remain open. The later
[RoQ checkpoint](2026-09-17-roq-bounds.md) establishes that original SP/MP also
ignore JPEG chunks; it preserves that policy and adds bounded decoder,
installed-corpus and same-process recovery evidence.

The [campaign/save checkpoint](2026-09-17-rtcw-campaign-savegames.md) launches
all 47 official maps separately, initializes both AAS worlds on all 32 SP maps,
and exercises same-process plus fresh-process saves on all 25 playable SP maps.
It repairs save staging/timing, bot scheduling after restart, saved entity
capacity, fog/episode metadata, cinematic-save protection, all twelve trajectory
ordinals, and authored sun/light-grid directives. Inspected MP world captures
cover all 15 maps with the advertised objective/checkpoint mode. Two specifically
authorized movement/attack probes were run: SP passed; MP passed a separately
authorized retest after repairing match-state restarts and the probe's command
scheduling. All 15 MP maps also pass native warmup restarts with advancing
snapshot/usercmd clocks. Seventy installed AAS/RCD pairs pass
loader/conversion/cost comparisons. This does not close complete campaign,
custom-cache generation, sustained navigation/gameplay or peer-interoperability
gates; RTCW remains an explicitly enabled development profile.

The [Vulkan/RTX fog and menu-resize correction](2026-09-17-rtcw-vulkan-fog-resize.md)
supersedes the historical Vulkan/RTX active-fog guard above. Both raster
backends now apply per-material scene/sky fog; installed `escape1` probes reach
the opening 3D scene, retain the paused briefing across resolution changes,
and capture controlled fog-on/off results. An actual SDL window-resize event
also refreshes and restores the briefing. Native ray fog is implemented and
compiled, but the x86 driver rejects required ray-tracing extensions, so native
RT runtime and original-engine visual parity remain open. Q3's resized main
menu was inspected; isolated QL evidence covers UI reload/state/dimensions and
its splash only, with WebUI disabled.

The [empty-model registration correction](2026-09-17-rtcw-empty-model-names.md)
extends the RTCW bridge's silent handle-0 result to empty names as well as null
pointers, matching original SP/MP behavior. The import regression fails before
the correction and passes afterward; focused Windows x86 checks and seven
installed runtime probes pass. SP `escape1` cinematic probes show zero
`RE_RegisterModel: NULL name` warnings in GL/Vulkan/RTX raster. This is a
bounded model-registration correction; broader compatibility gates remain open.

The [shader animation limit correction](2026-09-17-rtcw-animmap.md) restores
RTCW SP/MP's sixteen-frame ordinary `animMap` limit in GL/Vulkan/RTX. Actual
parser regressions cover all profiles, bounds and the existing shaderx
extension; fourteen focused Windows x86 checks and the strict development
build pass. Six installed SP/MP map-startup probes report zero excess-animation
warnings. These establish registration behavior, not fire-effect visual parity.

The [demo record](2026-09-18-rtcw-demos.md) makes demo protocols profile-owned.
RTCW recordings had been renamed `.dm_68`, and RTCW profiles searched Q3 demos
and `.dm3` while refusing their own `.dm_49`/`.dm_60`. RTCW MP now records,
replays and timedemos `.dm_60`, and exchanges demos with retail `WolfMP.exe` in
both directions, including a retail recording made under pure admission. SP
records only with `g_synchronousClients` set, as original SP requires, and
refuses replay: retail `WolfSP.exe` fails fatally on its own SP demo because the
SP cgame needs a running game module. Q3 and QL record/replay pass unchanged.
Short spectator recordings on one map per profile do not close active-play,
malformed-demo or seeking gates.

The [handshake record](2026-09-18-rtcw-mp-handshake.md) fixes outbound RTCW MP:
against retail `WolfMP.exe` the client had read the reply with Q3's token
layout and connected as protocol 68, which retail rejected. Client and server
now use RTCW's own original and iortcw layouts, including MP's
`sv_onlyVisibleClients` and the client culling it requests, name `wolfmp`/`wolfsp`
in `getchallenge`, and no longer ask Q3's authorize server about RTCW keys. An
fnTech3 client passes pure admission on the retail server and joins both teams;
retail clients still join fnTech3; Q3's handshake is unchanged. One map and
loopback UDP only; no iortcw binary was run.

#### RTCW gate matrix

| RTCW gate | SP | MP | Required closure |
| --- | --- | --- | --- |
| Official maps and saves | Source/Unit/Live; 32 individual map launches with both AAS worlds, 25 gameplay save/reload/fresh-process cycles, repeat/cross-map/autosave and malformed-header handling; two-map retail save interchange; cinematic saves refused; the final cutscene now ends the campaign in the endgame credits menu instead of an unsupported-import drop; see the [endgame record](2026-09-18-rtcw-sp-endgame.md) | Source/Unit/Live; 15 individual map launches, advertised modes and native warmup restarts with advancing match/usercmd clocks; separately authorized soldier movement/firing retest | Full campaign transitions and sustained AI/combat; broader historical/damaged save payloads; MP peers, objectives and other classes/weapons |
| Title/profile selection | Source/Unit/Build/Fixture; registered and gated; installed Windows x86 SP-to-MP menu handoff using the same executable, correct modules and inspected captures; synthetic Windows/Linux handoff | Source/Unit/Build/Fixture; installed Windows x86 MP-to-SP menu and active-briefing handoffs using the same executable, correct modules and inspected destination captures; synthetic Windows/Linux handoff | Broader live menu/session transitions; macOS handoff build/runtime |
| Filesystem/config/pure | Source/Unit; archive/config policy, ordered module/root lookup and intentional no-cache policy integrated; original SP pure default/flags; explicit pure-proof policy unavailable | Source/Unit/Build/Fixture, inbound Interop subset; profile-owned two-native-module proof, pinned-module pak validation and local pure admission with retail modules; original/iortcw proof and verifier comparisons; a retail MP client passes inbound pure admission on one map and reaches the world while the server withholds the loaded-pak names, so RTCW MP needs no `sv_pakNames` and keeps ET's publication scoped away from it; see the [pure-name record](2026-09-17-rtcw-mp-pure-names.md); an fnTech3 client also passes outbound pure admission to retail `WolfMP.exe` and joins both teams, after the [handshake record](2026-09-18-rtcw-mp-handshake.md) replaced a Q3 protocol-68 connect the retail server rejected; both directions then pass pure admission on all 15 stock MP maps, after the [pure-map record](2026-09-18-rtcw-mp-pure-maps.md) stopped `mp_sub` falling back to degraded pure under a Q3-sized gamestate estimate | SP pure contract; mods and architecture policy; retail mount observations, malformed and partial-install probes; legacy MP favorites import |
| Module ABI | Source/Unit/Build/Fixture; 2,215 original/iortcw assertions on x86/x64; bounded bot-entity/movement-input/goal/movement-result/prediction/state/parser/trace/render/vertex/orientation records, 10-bit game entity limit, mode imports/exports, popup/menu/key translation, command values and native x86 game-owned animation pointer handoff | Source/Unit; 2,205 original/iortcw assertions on x86/x64; bounded bot-entity/movement-input/goal/movement-result/prediction/state/parser/trace/render/vertex/orientation records, 10-bit game entity limit, mode imports/exports, popup/menu/key translation, command values and WOLFINFO | An audit of all 435 SP/MP import ordinals finds 420 dispatched after the [endgame record](2026-09-18-rtcw-sp-endgame.md) added SP's `G_ENDGAME`; the fifteen refused by name are unhandled in the original engines too, deliberately refused (MP `UI_OPENURL`), or reachable only from a Mac SP build (`G_FS_COPY_FILE`). Remaining: retail UI/cgame/qagame execution coverage and live game-data binding; distinguish original/iortcw module flag/renderer contracts |
| Live wire and demos | Source/Unit/Fixture/Live; 1,143 matched native message encodings plus a decoder-only dictionary fixture; first local snapshot with installed modules and pure mode off; `.dm_49` recording under original SP's synchronous-client precondition, and SP replay refused because retail SP cannot replay SP demos (`VM_Call with NULL vm` on its own recording); see the [demo record](2026-09-18-rtcw-demos.md) | Source/Unit/Fixture/Live, demo Interop subset; 1,117 matched native message encodings; local pure admission and first map snapshot with installed modules; `.dm_60` record, replay and timedemo; retail `WolfMP.exe` replays an fnTech3 demo, and fnTech3 replays demos retail recorded against the fnTech3 server, pure and non-pure; see the [demo record](2026-09-18-rtcw-demos.md); original and iortcw handshake layouts in both directions, `sv_onlyVisibleClients` with its client culling matching the original function on 100,000 cases, and outbound retail admission; see the [handshake record](2026-09-18-rtcw-mp-handshake.md) | Sustained live state integration, netchan rules, iortcw peer runs, retail transcripts, broader demo coverage (other maps, active play, malformed RTCW demos, seeking) and bidirectional interoperability |
| Assets/renderers/audio | Source/Unit/Build/Fixture; RTCW v47 selection, stock `escape1` collision probe, frozen render records, basic scene/polygon/gradient imports, CPU 2D comparisons, MDC decoding/reference comparisons and native drawing through a synthetic Q3 fixture; indexed MD3/MDC/MDS tags, MDS poses and continuous LOD pass installed-asset CPU comparisons; native synthetic MDS draws through all three renderers; skin metadata/model lookup matches all 412 installed skins against four references; brush-surface shader lookup matches 1,925 cases per reference; explicit VM pointer/basic UI sound imports and source-compared dynamic/model lights integrated | Source/Unit/Build/Fixture; same contracts plus rotated pictures, stock `mp_beach` collision probe and inspected French native menu captures in all three renderers | Original-engine collision parity, full RTCW material flags, live corona/decal and GL fog parity, Vulkan/RTX scene-fog original-engine parity and native RT runtime validation, projected world-light GPU parity, retail MDS scene/tag and skin/model assembly validation, shader remap/GPU parity, remaining shader/fog/animation behavior, music/fade/cinematic contracts and further retail RTCW engine-render-target screenshots; MDC/MDS ray transport has CPU evidence only |
| Navigation | Source/Unit/Build/Fixture; live AAS8/RCD15 loading, both installed `escape1` worlds initialize; checked 128-state goal/movement lifecycle; shared entity updates with owned area links, per-world blocking, checked BSP imports, frame/state/time adapters and empty-config item updates; basic/trace/route/visibility, hide/attack, movement prediction and release ShowRoute imports; SP corner search; 70-pair graph/cost, route/cache and visibility comparisons with original/iortcw; tactical/prediction synthetic comparisons and 64-member installed prediction with stated arithmetic limits | Source/Unit/Build/Fixture; checked 64-state goal/movement lifecycle; same shared entity/ownership, BSP, frame/state/time, query, prediction and empty-item contracts; MP truncating costs versus SP ceiling, grid search and tactical occupancy verified; installed prediction uses SP geometry; RCD12 selection uses synthetic headers; stock MP keeps bots disabled | Dynamic-entity prediction and original BSP parity; cold-cache tactical divergences, installed-map/live tactical observations; missing-cache generation/writing, installed MP-cache observations, x87/fast-math numeric and decision differences, original-engine runtime route and gameplay evidence |
| Gameplay and lifecycle | Open campaign; Source/Unit/Fixture for AI state lifecycle, camera evaluation, clean mission-briefing shutdown, shared movie-owner cleanup, main-menu shutdown and partial navigation-load cleanup; Windows x86 GL startup progresses through the opening cinematic to inspected first-person knife/HUD captures with either active audio backend and nonzero SP speech queries | Source/Unit/Fixture for local map initialization, profile-specific camera timing, briefing-movie playback/shutdown and active-briefing-to-SP handoff; gameplay remains open | SP movement/combat, broader character parity, audible speech/facial timing, campaign/AI/save-load/transitions; MP classes/objectives/maps; broader partial initialization, sustained play and shutdown |
| Music and fades | Source/Unit/Build/Fixture; original-source queue/fade and SP loop/mixer comparisons; both-backend clear modes with preserved music and bounded PCM/gain routing; native menu music buffers on OpenAL; legacy initialized while muted | Source/Unit/Build/Fixture; exact two-argument import; native menu music buffers on OpenAL; legacy initialized while muted | Measured output and original playback parity; game-script triggers, save-load restoration and backend/device lifecycle |
| One-shot audio | Source/Unit/Build/Fixture; checked native/QVM imports, original-source channel-policy and fixed-range gain comparisons, legacy/OpenAL lifetime and failure handling; SP client-channel replacement and auto-channel separation | Source/Unit/Build/Fixture; same boundaries/backends, original MP auto-channel reuse and FIFO policy; inspected briefing startup on both backends | Queued/threaded timing; output-device/PCM parity; sustained game audio and resource pressure; documented safe exhaustion differences |
| Entity streams | Source/Unit/Build/Fixture; checked start/stop imports, shared decoder, eleven entity slots, queued replacement, immediate entity stop, bounded fades, both-backend synthetic stream runtime and installed introductory speech with nonzero amplitude | Source/Unit/Build/Fixture; checked start import, twenty-three entity slots, original intro-repeat loop policy, both-backend synthetic silent stream runtime | Original PCM/device timing; dialogue and mouth synchronization parity; save/load restoration; source-different SP loop correction and fade-slot admission; device recovery/resource pressure |
| Speech amplitude/default sound | Source/Unit/Build/Fixture; signed one-shot/peak-stream measurements on both backends; 299,264 matching cases per original/iortcw SP reference on three targets; installed intro completes with nonzero queries; eight-frame silent default | Source/Unit/Build/Fixture; original zero speech response; 512-frame silent default; both-backend installed briefing startup | Original clock/output/facial parity; full dialogue/codec coverage; device recovery and resource pressure |
| MP localization | Not an SP import | Source/Unit/Fixture; all eight installed dictionaries (1,201 records), 818,856 translations per original/iortcw reference, Windows x86/x64 and Linux x86 sanitizers; inspected French native main menu in all three renderers | Remaining UI/cgame rendering, byte/font agreement and language changes during a live session; optional translation-authoring diagnostics remain unimplemented |
| Cinematic startup/presentation | Source/Unit/Build/Fixture/Live; catalog-owned repeat policy and wolfintro selection, letterbox import/command, 24 fps header timing, clock handoff/rewind/wrap and inspected movie frames on all three raster renderers; [bounded decoding and same-process malformed-movie recovery](2026-09-17-roq-bounds.md), installed movie corpus and final-frame checks | Source/Unit/Build/Fixture/Live; catalog logo/intro policy, original 30 fps behavior, missing-startup-movie fallback and briefing movie startup; same bounded decoder, installed movie corpus and malformed-movie recovery | JPEG no-op matches original SP/MP source. Complete malformed codec/fuzz coverage, original visual/timing parity, audible A/V synchronization, game-script fades and full campaign movie transitions remain required |
| Shared release | Open | Open | Q3/QL non-regression, clean supported-platform builds, complete suite and same-artifact runtime evidence |

The existing Q3/QL blockers below remain in force. Their evidence does not
transfer to either RTCW mode.

1. **Boundary ABI verification.** Q3 adapters and profile-specific server-memory
   access are integrated, with Q3 QVM and QL native client/server observations.
   Complete client and extension audits, broader Q3 native module coverage, and
   runtime probes are still required.
2. **Live wire evidence.** Live usercmd/entity/playerstate call sites select
   explicit wire profiles. Field-table parity and transactional malformed-input
   regressions pass. Short bidirectional retail sessions, authenticated QL
   admission and full map changes now pass for both profiles, including the
   [clean Windows x86 package](2026-09-08-clean-windows-retail-package.md).
   Complete packet-flow coverage, Internet impairment and sustained sessions
   remain open; the observed private-host/WSL tests do not close those gates.
3. **QL snapshot tail is unresolved.** The known QL player-state fields occupy
   `0x1f8` bytes, while the retail native snapshot reserves `0x250`. The
   remaining `0x58` bytes must not be labelled padding or exposed without
   retail evidence and bounded handling.
   The [snapshot marshalling correction](2026-09-07-ql-snapshot-marshalling.md)
   now converts the frozen known prefix explicitly and tests zeroed unknown
   bytes and destination guards. The [guarded native snapshot probe](2026-09-08-native-snapshot-containment.md)
   exercises exact output bounds, rejected-request preservation and zero-tail
   containment through the running client; the tail's meaning remains unverified.
   The [retail consumer layout inspection](2026-09-08-retail-snapshot-layout.md)
   independently pins the 592-byte copy, snapshot-buffer separation and entity
   offsets/stride to the installed module. It confirms that the tail is copied,
   without establishing its producer values or downstream semantics.
   The [retail producer and delta-reader checkpoint](2026-09-08-retail-snapshot-producer.md)
   traces the full history copy and, in controlled CPU emulation, establishes
   zero tail bytes from a null baseline and preservation of a supplied nonzero
   tail across field updates. Other writers and tail semantics remain open.
   [Live retail tail sampling](2026-09-08-retail-snapshot-tail-live.md) observes
   490 valid records across all 32 slots with zero tails during a connected
   map-change session. Finite sampling does not resolve those remaining gaps.
   The [compressed player-state payload checkpoint](2026-09-08-retail-playerstate-wire.md)
   runs retail Huffman initialization and real bit IO in CPU emulation, checks
   six states in both encoding directions, and pins two accepted retail
   movement-byte variants in Windows/Linux codec regressions. It is not live
   peer interoperability or exhaustive packet coverage.
   Those retail-derived vectors also pass all eight starting bit offsets and
   reject every shorter framed byte prefix without modifying separate or
   aliased output state (392 combinations, Windows x86 and Linux i686).
   The [retail field corpus](2026-09-08-retail-playerstate-field-corpus.md)
   expands differential checks to 478 synthetic transitions in both directions,
   including every scalar field and array slot, and retains 58 retail-generated
   scalar boundary fixtures. Windows x86 and Linux i686 decode all retail
   outputs correctly; that corpus alone does not prove full-process interoperability.
   The [retail entity-wire checkpoint](2026-09-08-retail-entity-wire.md)
   similarly checks 402 entity writer cases (all byte-identical), 400 decoded
   payloads, and two intentional no-output cases. It pins 58 observed field
   descriptors and adds retail-generated fixtures passing Windows/Linux codec
   regressions, without claiming a live snapshot exchange.
   The [retail keyed-command checkpoint](2026-09-08-retail-usercmd-wire.md)
   corrects QL movement-byte preservation while retaining Q3's clamp. All 24
   writer payloads match retail. The subsequent
   [key-mask checkpoint](2026-09-08-retail-usercmd-key-mask.md) traces the extra
   key bit through retail batch acceptance and GetUsercmd, and matches that
   QL behavior without changing Q3. Its controlled branch and finite corpus
   do not establish complete live command interoperability.
   A [post-correction runtime check](2026-09-08-post-codec-runtime.md) passes
   both Q3/QL self-pairs with the same Windows x86 binaries, exact split-response
   HTTP downloads, pure-to-active admission, engine screenshots and normal
   shutdown. QL reaches its join panel; active match gameplay is not claimed.
   The later [QL active-match checkpoint](2026-09-08-ql-active-match.md)
   joins campgrounds/ffa through an engine command, with a first-person player
   HUD, five participants, `IN_PROGRESS` samples and bot score progression.
   It remains a short self-pair; keyboard/mouse combat and independent retail
   engine interoperability are not exercised.
   The [active QL demo checkpoint](2026-09-08-ql-active-demo.md) records a joined
   FFA session and replays all 564 messages in normal/timedemo modes with native
   HUD screenshots, EOF completion and normal shutdown. Seeking and independent
   broader retail-engine demo coverage remain unverified. Short cross-engine normal
   and timedemo playback now passes both directions in the
   [retail demo checkpoint](2026-09-08-ql-retail-demo-interop.md).
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
   packaging paths and complete release evidence remain open. The
   [fresh Windows package](2026-09-08-clean-windows-retail-package.md) now passes
   a clean build/full suite, deterministic archiving, both-profile retail map
   changes and unchanged extraction checks. This does not close independent
   compiler reproducibility or the complete multi-platform release workflow. The
   [clean Linux media refresh](2026-09-08-linux-media-build-refresh.md) builds the
   later WAV/Ogg corrections in Ubuntu 20.04, passes the full configured suite
   and glibc ceiling, and repeats deterministic packaging plus Q3 pure-client
   and QL API 10 dedicated smoke checks.
   The [clean Windows media refresh](2026-09-08-windows-media-build-refresh.md)
   packages e114ded from a fresh export, repeats the full suite and all four
   retail map-change directions, and verifies both-profile file audio from
   identical packaged binaries. Hosted CI remains blocked by the account issue.
   The [engine-only correction](2026-09-07-engine-only-packaging.md) removes
   inherited closed-source provider staging. Subsequent clean CI now validates
   that packaging path; see the [regression checkpoint](2026-09-08-regression-checkpoint.md).
   Reproducibility and packaged runtime gates remain open.
6. **Q3 coverage remains incomplete.** Local QVM startup, pure admission, world
   rendering, and HUD are observed. The RTX raster black geometry defect is fixed
   and verified on the frozen demo; broader formats, native modules, remote peers,
   and packaged runtime paths remain open.
7. **QL coverage remains incomplete.** Unified native-module startup, local pure
   admission, and join UI/world rendering are observed. Short authenticated retail
   sessions now pass both directions, including first-person warmup on the retail
   dedicated server; see the
   [dedicated checkpoint](2026-09-08-ql-dedicated-retail-interop.md). Authentication faults,
   WebUI presentation, remote peers, broader gameplay, and lifecycle stress still
   require new fnTech3 evidence.
8. **Retail Q3 interop now has short bidirectional live evidence.** Launching
   the installed retail executable with isolated writable homes passes pure
   admission, active play and engine screenshots in both directions against
   Windows x86 fnTech3. The
   [retail checkpoint](2026-09-08-q3-retail-interop.md) retains earlier startup
   and short-lifetime probe failures separately. Remote, long-session and
   broader protocol coverage remain open.

The [reliable-command transaction correction](2026-09-08-server-command-transaction.md)
rejects truncated commands before history/hash publication and removes signed
overflow from duplicate detection. Both-profile executable boundary tests, Linux
sanitizers and short retail-server map changes pass; malformed live packets and
sustained counter-wrap interoperability remain open.
The [reliable-history follow-up](2026-09-08-reliable-history-wrap.md) bounds demo
command emission and module history lookup across signed counter wrap, with
executable boundary tests, Linux sanitizers and both-profile re-record/replay
controls. Live counter-wrap interoperability remains unverified.

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

### Return to Castle Wolfenstein

- Complete the [RTCW gate matrix](#rtcw-gate-matrix) for both modes using
  legitimate installations and the same Windows x86 engine binaries.
- Extend individual SP map/save probes to full campaign transitions,
  sustained AI/combat, and broader save/load compatibility.
- Extend local MP map probes to sustained classes, weapons, objectives,
  pure admission, and bidirectional retail/reference peer interoperability.
- Validate claimed demos, renderer/material/fog behavior, audio/input, and
  partial initialization/recovery/shutdown for each mode.
- Broaden real SP/MP menu handoffs while preserving mode-specific configs,
  modules, filesystem rules, and process-lifetime profile isolation.

### Shared release

- Run the full automated suite and supported build matrix on a clean checkout.
- Run all five profiles from the same packaged binaries with isolated homepaths
  and no file replacement between launches. RTCW's development gate must be
  resolved through evidence before full release compatibility can be claimed.
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

The unified goal is complete only when every required row in the Q3/QL, RTCW
and WolfET matrices passes at its required evidence level for `q3`, `ql`,
`rtcw-sp`, `rtcw-mp` and `wolfet`, all applicable blockers and open live gates
are closed, and the same release artifacts pass:

1. automatic and explicit profile selection;
2. exact module API/ABI and asset loading;
3. client and dedicated-server networking in both interoperability directions
   where applicable;
4. all claimed demo formats and malformed-input gates;
5. representative gameplay/render/audio/input sessions using legitimate data,
   including RTCW SP campaign/AI/save-load and RTCW MP objectives;
6. cross-profile isolation and deterministic packaging; and
7. complete automated, build, and documented windowed runtime gates.

Sign-off must identify the exact release artifacts and evidence bundle. A
partial profile, one-way client connection, inherited FnQL result, or source
claim cannot satisfy the goal. A release may make a narrower, explicitly
documented profile/platform claim, but that does not complete the unified goal.

## WolfET gate matrix

Enemy Territory is the fifth immutable profile and fourth title. Its initial
[implementation record](2026-09-17-wolfet-contracts.md) separates original-source
checks, generated packet fixtures and installed dedicated observations.

| Gate | Evidence | Remaining requirement |
| --- | --- | --- |
| Profile / identity / discovery | Source, Unit, Live | Default/client gates preserved; broader installs and platforms remain open. |
| Module ABI / server memory | Source, Unit, Live subset | 1,326 layout/constant/ordinal checks, native qagame startup, and every declared game import handled: 204 of 212, the other eight being ordinals retail ET refuses itself. See the [import](2026-09-17-wolfet-game-imports.md), [navigation](2026-09-17-wolfet-navigation.md), [visibility](2026-09-17-wolfet-visibility.md) and [prediction](2026-09-17-wolfet-prediction.md) records. The cgame surface handles 144 of 147 imports and the UI surface 114 of 121, each refusing the rest by name; see the [cgame](2026-09-18-wolfet-client-boundary.md), [UI](2026-09-18-wolfet-ui-boundary.md), [sound](2026-09-18-wolfet-audio.md), [light](2026-09-18-wolfet-light.md), [client services](2026-09-18-wolfet-client-services.md), [text and files](2026-09-18-wolfet-text-and-files.md), [camera](2026-09-18-wolfet-camera.md) [scene geometry](2026-09-18-wolfet-scene-geometry.md) and [global fog](2026-09-18-wolfet-global-fog.md) records. Six of the open UI ordinals are ones retail ET refuses itself. The navigation imports have no retail content to run against, and no cgame or UI import has been reached from a running client. |
| Protocol-84 message bodies | Source, Unit, inbound Interop subset | 1,169 original-engine comparisons and inbound retail sessions; full transport and malformed peer behavior remain open. |
| Filesystem / pure / downloads | Unit, Live, inbound Interop subset | Separate etmain/home and native names. Pure admission now works: a retail client passes the proof, reaches CS_ACTIVE and joins both teams on all six stock maps; see the [pure record](2026-09-17-wolfet-pure.md). The official-pak refusal is now profile-owned and covers etmain and mp_bin; see the [download correction](2026-09-17-wolfet-downloads.md), and the www redirect has source and unit evidence; see the [redirect record](2026-09-17-wolfet-web-downloads.md). Broader maps and mods, gamestate-overflow fallback, any transferred download and the reverse direction remain open. |
| Assets / collision | Source, Unit, Live subset | Six stock BSPs and installed TAGs load, and all 21 installed MDM/MDX skeletal files decode through bounded readers, and through the renderer's own byte reader with every count agreeing; see the [model](2026-09-18-wolfet-models.md) and [renderer model](2026-09-18-wolfet-renderer-models.md) records. The shipped assets settle three things ET's own source does not: its surfaces and tags are chained rather than strided, a vertex weight names a global bone rather than the surface's bone references, and every one of the 59,400 shipped skeleton frames writes the greater corner of its box first. Every mesh paired with every skeleton -- 110 pairs, 64,284 poses and 4.1 million skinned vertices -- poses, skins, reduces and places its tags inside the bounds each frame declares; see the [drawing record](2026-09-18-wolfet-drawing.md). No image has been produced, and the remaining render assets stay open. |
| Dedicated gameplay / snapshots | Source, Unit, Live, inbound Interop subset | All six maps initialize under explicit and auto selection; a retail client reaches CS_ACTIVE, joins Axis/Allies, respawns and receives replies to two objective-camera requests. [Callbacks/proxies](2026-09-17-wolfet-snapshots.md) and [viewpoints/server capacity](2026-09-17-wolfet-viewpoints.md) pass executable regressions. A retail client also spawns, moves about 250 units and follows a goldrush-to-battery map change under pure admission; see the [gameplay record](2026-09-17-wolfet-gameplay.md). Live landmine/proxy/brush/lean visibility, rendered cameras, dense retail exchanges, movement parity, combat and objectives remain open. |
| Client / renderer / audio / input | Developer-build Live subset; release blocked | **The ET client has run** in a developer build: it draws ET's first-run profile dialog with ET's own fonts through the retail UI module, loads oasis through the retail cgame and draws ET's limbo panel, and quits cleanly with the installation unchanged; see the [client run record](2026-09-18-wolfet-client-run.md). That run found and fixed eight defects no unit test could see, among them every cgame trace running along the wrong line and a string leaking out of a skipped preprocessor branch for every profile. Release builds stay blocked, no gameplay is claimed, and the command map image, ET's implicit shader keywords and ET's hunk size remain open. ET's cgame and UI records, renderer transfers, key numbers, browser numbering and 2D geometry pass regressions, and both dispatchers are wired; see the [cgame](2026-09-18-wolfet-client-boundary.md) and [UI](2026-09-18-wolfet-ui-boundary.md) boundary records. ET's sound surface is carried on both boundaries, with its own volume, loop start time and stop-sounds flag; see the [sound record](2026-09-18-wolfet-audio.md). No sound has been played and no listening test performed. ET's dynamic light reaches all three renderers with its own radius, brightness and force flag; its global directional and grid-only lights are skipped and say so. ET's binary message channel is wired in both directions, though no payload has crossed. ET's spline camera paths are carried, though a legitimate installation ships none to follow. ET's batched scene geometry, runtime shader definitions and image lookup are carried. The renderer now carries both of ET's skeletal model types, its own surface and its own draw call in all three renderers, including the entity's torso rotation; nothing has been drawn, because the client stays gated. See the [drawing record](2026-09-18-wolfet-drawing.md). ET's world-covering fog volume now loads instead of refusing the map -- every fog volume retail ET ships is one -- and a module can retune it with a fade; see the [global fog record](2026-09-18-wolfet-global-fog.md). No image has been produced and fog density is not claimed to match retail. ET's decal and render-to-texture imports are refused rather than approximated, each for a recorded reason; retail ET calls the last of those only from a developer example its shipped build does not define. No absent online service is reported as present. |
| Demos / bidirectional interoperability | Inbound Interop subset; demos open | Non-pure retail spectator/team admission connects to fnTech3; reverse direction and ET demo playback remain open. |
| Optional services | Unavailable | No PunkBuster, authentication or Workshop success is synthesized. |

Build with `-Dwolfet-development=true` only for isolated dedicated probes.
This option is independent of `-Drtcw-development=true`; neither option closes
any release gate. Q3/QL/RTCW evidence is not ET evidence.
