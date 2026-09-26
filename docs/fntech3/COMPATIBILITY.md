# fnTech3 compatibility ledger

## Unified goal and supported-game scope

fnTech3's goal is one modern id Tech 3 engine distribution and shared executable
set for ten games and sixteen immutable profiles. Shared renderer, audio, input,
platform, and tooling improvements must preserve each game's own contracts.
Full compatibility is **not yet established for any profile**.

| Game / mode | Startup profile | Current availability |
| --- | --- | --- |
| Quake III Arena | `q3` / `quake3` | Enabled; partial build, fixture, live, and interoperability evidence; required gates remain open. |
| Quake III Arena — IHV Test | `q3-ihv` | Gated by default; `-Dq3-ihv-development=true` permits Windows x86 dedicated native-game probes. Client/network integration remains blocked. |
| Quake III Arena — Q3Test 1.02 to 1.08 | `q3test` | Gated by default; `-Dq3test-development=true` runs the Windows x86 developer client and server with each release's retail modules, against retail peers in both directions; see the [Q3Test gate matrix](#q3test-gate-matrix). Required gates remain open. |
| Quake Live | `ql` / `quakelive` | Enabled; retail client boundary is Windows x86; required gates remain open. |
| Return to Castle Wolfenstein — single player | `rtcw-sp` | Experimental; default startup gated; `-Drtcw-development=true` permits isolated probes; campaign and broader compatibility gates remain open. |
| Return to Castle Wolfenstein — multiplayer | `rtcw-mp` | Experimental; same build option and executable as SP; multiplayer and broader compatibility gates remain open. |
| Wolfenstein: Enemy Territory | `wolfet` / `et` | Experimental; developer builds (`-Dwolfet-development=true`) run the client and server for isolated probes; release builds still refuse both. |
| Star Trek Voyager: Elite Force — single player | `stvef-sp` | Experimental; default startup gated; `-Dstvef-development=true` permits isolated probes; the retail game module runs every single-player map on a developer server, and the retail menus and cgame run in a developer client, which plays the levels with their ambient sound at retail's own distances, saves them and moves between them; the cinematic and renderer gates remain open. |
| Star Trek Voyager: Elite Force — Holomatch | `stvef-mp` | Experimental; same build option and executable as SP; the retail QVMs run, the retail demos play to completion with bounded retail image comparisons, and fnTech3 plays with and downloads from Lilium Voyager and the retail game as client and server in a developer build; part of the renderer gate remains open. |
| Star Wars Jedi Knight: Jedi Academy — single player | `jka-sp` | Registered only; default startup gated; `-Djka-development=true` permits isolated probes. A legitimate 1.01 installation is recognized and its maps, models, motion paths and compiled scripts read. A developer dedicated server loads a retail map's collision and then refuses the single-player module boundary by name; that boundary's `GetGameAPI` table and records are unwritten. The wire, savegame, client and renderer gates are open. |
| Star Wars Jedi Knight: Jedi Academy — multiplayer | `jka-mp` | Experimental; same build option and executable as single player. On a developer dedicated server the retail multiplayer module spawns `mp/ffa1` and holds it through its frame loop with no import refused, and a bot added to it connects, begins and takes items. No network client connects, no snapshot is built and no packet is encoded: protocol 26 has no message bodies or demo format, and nothing on the client is wired. No gameplay is claimed. |
| Star Wars Jedi Knight II: Jedi Outcast — single player | `jk2-sp` | Registered only; default startup gated; `-Djk2-development=true` permits isolated probes. A legitimate 1.02a installation is recognized from `base/assets0.pk3`, and its 1,557 maps, models, skeletons, motion paths and scripts read through fnTech3's readers. No single-player module runs: `jk2gamex86.dll`'s `GetGameAPI` table is unwritten. The wire, savegame, client and renderer gates are open. |
| Star Wars Jedi Knight II: Jedi Outcast — multiplayer | `jk2-mp` | Experimental; same build option and executable as single player. On a developer dedicated server the retail `vm/jk2mpgame.qvm` loads out of the pak and runs ninety seconds of `ffa_bespin` with two bots, which connect, enter, begin and collect items, with no import refused. No network client connects and no snapshot or packet is encoded; protocol 15's delta tables are measured, but nothing encodes with them, and nothing on the client is wired. |
| American McGee's Alice | `alice` | Experimental; default startup gated; `-Dalice-development=true` permits isolated probes. This is a Ritual F.A.K.K.2 engine rather than a Quake III one. In a developer client the retail game module and client game run with 110 of 111 and 119 of 125 imports served, every unserved slot being one the module never calls. The intro's logos lead to the engine's own `.urc` menus, New Game plays `opening.roq` into `gvillage`, and `garden1` draws Alice in third person with her HUD -- on the GL, Vulkan and RTX renderers alike -- saves and loads back, and records and plays back a demo. No retail client, server, save or demo has been exchanged with fnTech3 in either direction and no frame compared with retail's, so every gate stays partial and the release gate stays closed. |
| Heavy Metal: F.A.K.K.2 | `fakk2` | Experimental; default startup gated; `-Dfakk2-development=true` permits isolated probes. Ritual's own engine, which Alice's descends from, served through Alice's adapters with F.A.K.K.2's records crossing at the module boundary. The disc's `fakk2.exe` is encrypted. The official 1.02 patch's is plain: it was read, and it runs here as a client, though its server needs the CD. The evidence is that executable, the plain retail modules, `alice.exe` and the data. Retail `fakk2.exe` plays fnTech3's demos and draws the same frames. The comparison found three differences, all now corrected: the message bodies and a snapshot byte (Alice's too), the HUD's rule, and emitter sprites. Once the handshake was corrected, retail's client joined an fnTech3 server over loopback UDP. Its movement, turns, fire buttons and commands played there, and fnTech3 plays the demo it recorded there exactly as retail does. A developer server spawns all thirty maps through the retail game module with 105 of 105 imports served. A developer client runs the retail client game with 120 of the 123 imports it could call served (the other three have no call site) and enters all thirty maps. It draws the retail menus, HUD and inventory, and takes New Game's path and the level exits with the player's state carried. It saves, lists and loads games, and records and plays back a demo. English and German installations both run. Driven by engine-queued keys and mouse, fnTech3's menus, their pulldown, text selection, save list, hover and click sounds, the inventory's cursor mode, and play with movement and fire all work. There is no session the other way, fnTech3's client on retail's server (that server needs the CD), and no retail save has been read. |
| Medal of Honor: Allied Assault | `mohaa` | Experimental; default startup gated; `-Dmohaa-development=true` permits isolated probes. 2015's engine, served by adapters of its own. The retail `MOHAA.exe` is SafeDisc-encrypted and cannot run on this machine, so the evidence is the two plain retail libraries, the data and OpenMoHAA's reconstruction. A developer server spawns all fifty-four maps through the retail game module with 167 of 167 imports served; a developer client runs the retail client game with 149 of 168 served, none of the rest having been called, and enters every map. It draws the engine's 179 retail menus and the HUD, plays every single-player map, briefing and the credits through their scripts with their music, dialogue and loading bar, carries the player through a level exit, saves, lists, deletes and loads games from the Load/Save menu, records and plays back demos, and plays free-for-all, team, round-based and objective matches over UDP, two clients fighting and scoring, on pure servers too, downloading a missing archive over UDP or HTTP. Only fnTech3's own client and server have met; no retail peer, save or demo exists to compare with, so every gate stays partial and the release gate stays closed. |

RTCW is one title with two mode profiles. A mode change relaunches the same
executable; it does not mutate the running process's profile. Catalog
registration, a development option, or a partial runtime result does not
establish supported release compatibility. The [Q3/QL matrix](#compatibility-and-gate-matrix)
and [RTCW matrix](#rtcw-gate-matrix), plus the [ET matrix](#wolfet-gate-matrix), the
[Elite Force matrix](#elite-force-gate-matrix), the
[Jedi Academy matrix](#jedi-academy-gate-matrix), the
[Jedi Outcast matrix](#jedi-outcast-gate-matrix), the
[Alice matrix](#alice-gate-matrix), the
[F.A.K.K.2 matrix](#fakk2-gate-matrix) and the
[Medal of Honor matrix](#medal-of-honor-gate-matrix), define the current
unified scope.
Elite Force is one title with two mode profiles, as RTCW is, and so are Jedi
Academy and Jedi Outcast.
Dedicated-server and online-service gates apply according to each profile's
contract; RTCW SP additionally requires campaign, AI, and save/load evidence.

## Status

The [Elite Force foundation](2026-09-18-stvef-contracts.md) registers
`stvef-sp` and `stvef-mp` behind `-Dstvef-development=true`, identifies the
retail installation, and adds MP3 decoding that reproduces every one of the
8,074 retail `#UNCOMP` sizes. The [Holomatch wire slice](2026-09-18-stvef-holomatch.md)
adds retail protocol-24 message coding and the packet scramble; all 32,675
records of the retail Holomatch demos decode through the engine. The
[Holomatch server slice](2026-09-18-stvef-holomatch-server.md) hosts the retail
game module with its bots on a developer dedicated server: free-for-all and a
complete capture-the-flag match run without errors. The
[Holomatch client slice](2026-09-18-stvef-holomatch-client.md) adds the retail
handshake, client framing, downloads, pure checksums and `.efdemo` recording
and playback. The [Holomatch cgame and UI slice](2026-09-18-stvef-holomatch-cgame-ui.md)
runs the retail cgame and UI with Raven's effect primitives in all three
renderers: the retail main menu draws, and all three retail demos play to the
end. The [Holomatch sessions record](2026-09-18-stvef-holomatch-sessions.md)
plays live with Lilium Voyager and the retail game, as client and server and
with pure servers, and records demos live that fnTech3 and Lilium play. The
[languages record](2026-09-19-stvef-languages.md) plays the German voices,
gives the UI retail's translated key names and its own key numbers, and
registers retail's three language cvars. A missing pak downloads between
fnTech3, Lilium and retail in every direction.

The [ET contract and dedicated-server slice](2026-09-17-wolfet-contracts.md)
adds a distinct immutable profile, protocol-84 codecs, frozen module records,
checked server-memory transfers, native game imports, and attachment-tag loading.
ET is not a supported client release. UI/cgame, demos (partial: see the
demo playback record below),
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
original-engine contract. Full cinematic/audio parity remains open. The
[RoQ fuzz record](2026-09-18-roq-fuzz.md) then passes 7,300 seeded corruptions
of ten installed movies through that decoder under AddressSanitizer, and 60 per
profile through one running client under `q3`, `rtcw-sp` and `rtcw-mp`.

The [RTCW dynamic-light update](2026-09-17-rtcw-dlight-shadows.md) enables the
shared Q3/QL `r_dlightMode` lighting and point-shadow path for ordinary SP/MP
lights, stabilizes flickering torch projections, and admits MDS/MDC model
casters in every raster backend. Executable tests, x86 builds and bounded
`escape1` engine captures add lighting evidence; image parity and broader
renderer/gameplay gates remain open.

The [RTCW engine scene lights record](2026-09-24-rtcw-engine-lights.md) gives
both RTCW modes the static map lights, surface-light proxies and `r_dlightTest`
lights every other profile had. They join only the per-pixel modes and never
the snooper view or `r_dynamiclight` 0 or 2; RTCW's module lights keep the 32
slots a frame retail gives them, the engine's taking only slots beyond those,
and are drawn after the engine's so the darkening and flame materials apply
over them. Frame simulations, per-renderer executed slices and seven caught
regressions hold those rules. `mp_ice` replays in GL, Vulkan and RTX draw the
lights, match the build without the change byte for byte with them off, and
Quake III's replays are byte-identical before and after. No RTCW light file
ships yet; generating one from the maps' `light` entities is open.

The [ET engine scene lights record](2026-09-24-wolfet-engine-lights.md)
gives Enemy Territory the rules RTCW's scene lights got. ET's modules keep the
first 32 lights a frame the original stores, counted apart from the engine's
static, proxy and test lights, which take only slots the modules can no longer
claim and join every shared mode but not `r_dynamiclight 0`. A forced module
light now stays drawn at `r_dynamiclight 0`, as in the original, where the
renderer used to clear it; directed and grid-only lights, still not drawn,
take one of the 32 as the original's do. Executable tests, a 20,000-frame
simulation and nine caught regressions hold the rules. Live `oasis` clients in
GL, Vulkan and RTX draw the engine's lights, and a thrown charge's glow stays
drawn at `r_dynamiclight 0` in GL; Quake III, Quake Live and RTCW MP replays
are byte-identical before and after. No ET scene with a sky portal, multiview
or portal view has been run. ET demos now replay
(`2026-09-25-wolfet-demo-playback.md`): fnTech3 and retail `et.exe` each play
the other's `.dm_84` with the same parse, message for message, and the same
view at every sampled timedemo frame, on one map with spectators only.

Since the [RTCW map lights record](2026-09-24-rtcw-map-lights.md), the package
ships that file for all 32 official SP and 15 official MP maps, 4,316 lights in all. Each
map's `light` entities are read with the keys and flags RTCW's own `QUAKED`
comment documents and q3map2's Wolf falloff, whose `fade` reading and
inverse-square law the maps' baked lightmaps confirm; each light's radius and
brightness are fitted to retail's baked curve at its mode's `r_dlightScale`.
Grid-only, non-dynamic, negative and cgame-drawn fixture lights are left out,
and 128 are kept a map across its PVS clusters. The package validator now
reads every packaged light file as the renderers would. `mp_castle` replays
in GL, Vulkan and RTX load all 128 from the package and change about 86,000
pixels; in mode 0, and with a package lacking the files, frames with
`r_staticLights` on match frames with it off byte for byte. A live `escape1`
client shows them in a lamp-lit corridor. They are engine-owned estimates added to the baked lighting; no
frame is compared with retail and no shadow they cast was seen.

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
Since the [Wolfenstein OpenAL voices record](2026-09-24-wolfenstein-openal-voices.md),
RTCW's and ET's voices are sent to these zones' rooms. Before it, the sidecars
loaded and every voice's send was zero.

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

The [client-slot stack correction](2026-09-24-client-slot-stack-overflow.md)
clears server client slots in place. Changing `sv_maxclients` on a running
server overflowed the 1 MB main-thread stack, in code every profile shares,
because `SV_ChangeMaxClients` and the `SV_AllocClients` it calls each built a
whole `client_t` temporary of over 500 KB. Q3, RTCW MP and ET dedicated servers now
spawn the level again with the new count, and retail RTCW MP and ET clients
stay connected across the `map_restart` that applies it (Fixture, and Live
inbound). Other profiles, listen servers and unoptimized builds were not run.

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
| Key numbers | Original Quake III's, converted at the UI and cgame boundary: `MWHEELDOWN` 183, `JOY1` 185, `AUX16` 232, `K_CHAR_FLAG` 1024 ([record](2026-09-19-q3-key-numbers.md)) | Retail Quake Live's, which are this engine's own: `MOUSE6`-`MOUSE9` 183-186, `MWHEELDOWN` 187, `AUX16` 236 |
| Key names | The names the console, `bindlist` and each config use, shared by every profile. They agree with both retail tables: 176 is `KP_STAR`, and 130 and 157-159 are written as `0x82` and `0x9d`-`0x9f`, unnamed as retail leaves them. Four earlier names are still read so older configs load, and are never written ([record](2026-09-20-key-names.md)) | The same table; quakelive_steam.exe's names are quake3.exe's plus `MOUSE6`-`MOUSE9` ([record](2026-09-20-key-names.md)) |
| Keypad star | All three key producers report the character `*`, as quake3.exe's Win32 and X11 producers do; RTCW SP, RTCW MP and ET report it the same way and only the Mac clients report `K_KP_STAR`. The `KP_STAR` name loads and round-trips while no producer reports the key, which is retail's own arrangement on Windows. Source/Unit; no runtime input was observed ([record](2026-09-20-keypad-star.md)) | Unobserved. quakelive_steam.exe names 176 `KP_STAR`, but what its producer reports for that key was not established ([record](2026-09-20-keypad-star.md)) |
| Shared data layout | Frozen Q3 `vmCvar_t`/trajectory/usercmd/entity/player-state boundary PODs (32-bit ABI): 272/36/24/208/468 bytes | Separate frozen QL PODs (32-bit ABI): 276/40/28/236/504 known bytes, including QL-only fields; the retail native snapshot reserves `0x250` player-state bytes and the trailing `0x58` remains unmodelled |
| Maps/assets | Q3 pak/pure rules, BSP 46 plus explicitly supported legacy formats, shaders/models/images/audio/cinematics | BSP 47, QL advertisements/material behavior, retail pak/WebUI/native-module assets |
| Platform constraint | Shared x86 binary is the initial all-in-one proof target; broader Q3 platforms remain desirable | Retail client compatibility requires Windows x86; dedicated-server claims are recorded separately |

Supported legacy protocols and demo formats must remain enumerated in code and
tests rather than inferred from the preferred value. The
[Q3 point-release ledger](Q3_POINT_RELEASES.md) now covers Q3Test, Demo Test,
the standalone demo and retail patches separately. Protocols 66/67 have
bidirectional original-engine, pure/non-pure and recording-interchange
evidence for 1.29f/g/h, 1.30 and 1.31; the existing 68/71 gates still apply.
Raw 43/45/46/48 demo formats are explicitly selected from their gamestate,
including id's protocol-46 `.dm_48` files. **Full historical Q3 support remains
open:** Q3Test 35/36/38/39/40 codecs and BSP/module contracts, old QVM APIs and
pre-1.29 live protocols are not implemented by these demo and 66/67 changes.

The [IHV ledger](Q3_IHV.md) covers the supplied February 1999 build separately.
The [dedicated game adapter record](2026-09-21-q3-ihv-game-bridge.json)
adds actual original-DLL startup on all four supplied maps, retained allocations
across map changes/restart, and shutdown/reload in one engine process. Both
normal shutdowns release all native allocations; allocation-budget and client
capacity failures exercise partial-startup cleanup. The adapter marshals native
cvars, entity imports and by-value traces, publishes IHV systeminfo before
spawning, and preserves the other profiles' lifecycle paths. The developer
option enables dedicated Windows x86 probes only; native clients, protocol-33
network sessions, gameplay parity and native rendered demo playback remain open.
The [bot client record](2026-09-21-q3-ihv-bot-clients.json) completes the
remaining five imports on native server client records that are separate from
the retail client array and player projection. Bot allocation, release and
userinfo, the bot scheduler's position and callbacks, the engine's own name,
rate and snaps update and both snapshot getters match the original functions
across 11 allocations, 33 scheduler steps, 12 userinfo cases and 6,528 getter
results. The [snapshot record](2026-09-21-q3-ihv-snapshots.json) then adds the
builder that fills that history: IHV's Quake II-style cluster record, stored for
this profile alone, and the visibility walk that selects what a client sees.
2,048 generated scenarios match the original builder on every selected entity,
area byte and repaired number. The [snapshot message record](2026-09-21-q3-ihv-snapshot-messages.json)
then adds the writer: the delta base a client asks for, its two staleness
rules, Quake II-style entity baselines and the 1,350-byte entity limit. 1,536
generated scenarios produce byte-identical protocol-33 messages, including 366
refused delta requests and 415 truncated entity lists. The
[transport record](2026-09-21-q3-ihv-transport.json) then adds the envelope:
IHV's netchan keeps Quake II's two sequence words and single reliable slot,
fragments Quake III's way and scrambles its payload, and matches the original
packet for packet across 5,202 of them. It is registered as its own wire
profile, so protocol 33 can never fall back to a Quake III contract. The
connectionless handshake and the client session that would carry these packets
remain open, as does original BSP visibility parity.
Its original server advertises protocol 33, and its native function tables and
length-only demos differ from public Q3Test and retail Q3. BSP 43 translation
now admits all four supplied maps, preserves original checksums and exact inline
brush membership, and validates malformed geometry. Twelve windowed map-load
and screenshot probes pass across GLx/Vulkan/RTX using retail Q3 QVMs.
The material follow-up adds legacy sky/cloud, entity-color and depth-write
commands and an exact-file in-memory `common.shader` repair, with another 12
asset probes and three portal follow-ups passing across the same renderers.
The [fog follow-up](2026-09-20-q3-ihv-fog.json) matches 306 original-function
results, adds flat/animated vertex-density fog to all three renderers and passes
another 12 map probes. IHV fog maps use classic dynamic lights before the fog
overlay; original lighting parity remains open.
Its complete regression run passes 121 Meson targets with two skips and no
failures, including 1,833 Python cases with 72 skips.
The [sky follow-up](2026-09-20-q3-ihv-sky.json) matches 730 original iterator
frames, preserves per-stage lightning colors in GLx/Vulkan/RTX, and passes
12 more map probes plus three capture sequences. The `tim_dm1` fallback sky
is replaced with clouds; the earlier captures retain their original scope.
The sky regression run passes 121 Meson targets with two skips and no failures,
including 1,835 Python cases with 72 skips.
The [native game boundary probe](2026-09-21-q3-ihv-native-game.json) runs the
original game DLL's Init/Shutdown against separate IHV records and bounded
host-owned allocations. Normal startup, missing bot configuration, client
count adjustment and three allocation-failure cases pass, including x86
stack/runtime checks. Exact layout and malformed-boundary tests pass in x86
and an x64 memory-checking build. These components await engine integration
and do not establish native map/gameplay or cgame behavior.
The [protocol-33 codec evidence](2026-09-21-q3-ihv-wire.json) matches 36,192
original-function entity/player/command cases, and adds separate transactional
message and length-only demo readers/writers. The supplied `sli.dm3` parses
and regenerates all 57 messages with identical decoded states; 55 messages
are byte-identical. Two full frames omit unchanged absent-baseline records.
The incomplete `berj.dm3` is rejected cleanly. Seven focused x86 targets pass,
and the two new codecs pass x64 AddressSanitizer/fast-math tests. These are
isolated components, not engine demo playback, recording or live sessions.
The [identity/link follow-up](2026-09-21-q3-ihv-identity-collision.json) registers
a separate, still-gated `q3-ihv` profile and recognizes the supplied loose
installation without executing its DLLs. Retail/IHV mixed markers are ambiguous.
IHV solid packing and link bounds match 2,740 original-function numeric cases
and reach the engine's profile-specific link branch. The reference leaf query
is stubbed empty; original PVS, collision traces and native entity integration
remain separate open gates.
The catalog separates the original engine's 256-client ceiling from its default
of eight and the native game's `maxentities / 8` allocation rule. Original-DLL
allocation and ClientConnect probes pass; retail bot/AAS entry points reject
IHV without allocating. The full regression run passes 126 Meson targets, with
two skips; its Python batch passes 1,837 cases with 72 skips. These are boundary
and regression results, not a native IHV gameplay or interoperability gate.
The subsequent [native game-data adapter](2026-09-21-q3-ihv-game-data.json)
binds the original shared entity prefix and exact player records, validates
published client pointers and preserves private module bytes. Original DLL
Init/ClientConnect/Shutdown callbacks exercise the actual engine adapter, and
the engine link path passes its geometry vectors through native record storage.
Native lifecycle dispatch, shared player projections and live sessions remain
open; these fixtures do not enable the profile.
The [server trace follow-up](2026-09-21-q3-ihv-server-trace.json) adds IHV's
world-zero result, complete startsolid/allsolid replacement, owner filtering,
state/current transforms and original sweep-bound rounding. Actual server
functions match 4,737 original sweeps and 641 point queries with synthetic
contact/CM replies. Native null owners use internal `-1` so entity 1023 remains
valid. Native record guards and all seven later profile policies are tested.
Original BSP collision, PVS/link membership and native import dispatch still
require separate evidence.
The [native configstring follow-up](2026-09-21-q3-ihv-configstrings.json) adds
protocol-33 configstring updates and preserves ordered message events. Its
20,100-byte native string record matches 512 original-parser cases, including
unchanged values, repacking, deletions and change notifications. Malformed and
overflowing messages retain prior state; native cgame dispatch remains open.
The [native client data follow-up](2026-09-21-q3-ihv-client-data.json) connects
decoded packets to exact client records and ordered callback delivery. History
getters match 33,152 original cases; complete snapshots and entity reads match
40 synthetic frames and all 56 frames of `sli.dm3`. The same packets pass the
original cgame's data callbacks with Init bypassed and packet-graph history stubbed.
Malformed packets cause no callbacks, and callback failure clears the session.
This closes isolated client data transfers, not native startup or playback.
An additional 61 original Init-prefix probes check cvar/data adapters and
failure cleanup through the collision/renderer map-import boundary, with
synthetic service replies; completed native initialization remains open.
The [model/cgame follow-up](2026-09-21-q3-ihv-cgame-runtime.json) adds validated
MD3 frame/tag storage and the native collision-model mirror that cgame reads
directly. All 243,600 pose components from 79,625 queries match the original
interpolator in the recorded x86 run, including all 111 supplied models.
An isolated host now completes original cgame Init, 56 ready frame updates and
draw callbacks, capturing 124 scene submissions with actual model metadata
and synthetic collision/resource services. Tag/scene callback failures clear
the client session. The older helper called "prediction" is packet-graph
history; the new host runs it unpatched. These observations supersede the
initialization-prefix limitation, but do not establish engine startup,
rendered playback, live prediction, actual collision or audio delivery.
The [owned scene boundary](2026-09-21-q3-ihv-scenes.json) now checks and copies
all 124 cgame submissions into separate engine-owned arrays, resolves resources
by kind and retains native effect fields. Original numeric routines confirm
262,144 area-visibility decisions and 147,456 entity-color components; IHV's
visible-area bits must be complemented for retail's blocked-area convention.
These tests use generated leaves/PVS and captured scenes. Native material,
effect, particle and backend rendering adapters, actual BSP/PVS and rendered
demo comparison remain open.
The [skin follow-up](2026-09-21-q3-ihv-skins.json) implements IHV's case-sensitive
registration, cached default-skin fallback, bounded ordered shader parsing and
surface-ordinal material selection in GLx, Vulkan and RTX, including ray geometry.
Original numeric comparisons cover 54 registrations and all three supplied skin
files; other-profile and sanitizer checks cover the shared paths. This adds
Unit/Build/Fixture evidence, not native rendered playback or a completed material
gate. It also fixes existing RTCW nonfinite-scale rejection under fast-math and
RTX negative MD3 skin indexing found during those checks.
The [particle follow-up](2026-09-21-q3-ihv-particles.json) adds owned particle
submission and per-view triangles to all three renderers, including RTX ray
geometry and camera-dependent cache invalidation. Original numeric comparisons
match 1,130,688 vertex components, the 1,024-byte procedural texture and original
blend/depth callbacks. Shader, fog and sky compatibility paths now also admit
the separate `q3-ihv` profile; other profiles keep their prior behavior. Renderer
API 27 introduced that entry point. This is Unit/Build/Fixture
evidence; native scene-adapter dispatch and rendered comparisons remain open.
The [entity follow-up](2026-09-21-q3-ihv-entities.json) adds atomic, owned entity
batches through renderer API 28. Separate native flags drive primary/portal
visibility and primary-camera axes; the origin interpolation helper and native
beam vertices match original numeric observations. All three renderers register
the case-sensitive `*beam` name, submit an unfogged red additive material and
use the entity transform; RTX ray geometry uses the same beam vertices and
tracks exact native pose changes. The scene reader now accepts null models
while still rejecting unknown nonzero resource tokens. Flag `0x80` is explicitly
rejected until sorted transform and lighting retention work across raster/ray
paths. Actual model lighting, animation controls, portal behavior and
native scene-adapter integration remain open. This remains Unit/Build/Fixture
evidence, with no rendered native playback claim.
The [animation follow-up](2026-09-21-q3-ihv-animation.json) replaces retail MD3
math for native entities in GLx/Vulkan/RTX and ray geometry. A private sine table,
combined vertex/origin interpolation, normal arithmetic and expanded cull bounds
match selected original functions: 451,584 generated animation components,
5,408,046 components from every frame of all 111 models, and 165,888 culling
cases. Surfaces stage before vertex publication; finite/span/overflow checks and
eight fast-math sanitizer fixtures pass. Other profiles retain their renderer
paths. This is Unit/Build/Fixture evidence; native scene integration and rendered
parity remain open.
The [model-light follow-up](2026-09-21-q3-ihv-model-light.json) implements native
raw lightmap sampling, BSP traversal, dynamic lights, fixed direction and diffuse
colors in all three raster renderers. Actual translated-map light loading matches
32,768 original traces across four maps; another 32,768 generated tree traces,
24,576 dynamic-light cases and 34,816 diffuse vertices match original functions.
The native `r_lerpmodels` float toggle now controls raster/ray interpolation and
invalidates affected ray geometry. These are CPU/fixture results; native scene
dispatch, sorted flag-`0x80` retention and rendered lighting parity remain open.
**Full IHV support remains OPEN:** native game/cgame adapters, protocol-33
interoperability, demo playback and legacy material/collision parity are not
established by the asset and codec probes. Original portal, clamp and lighting behavior
still need validation.

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
| Filesystem, configs, pak order, and pure checks | **PARTIAL — Unit/Live.** Q3 config order and two-module pure proof are restored. Local QVM clients pass pure admission; malformed and oversized proofs have executable regressions. | **PARTIAL — Unit/Live.** Single-native-module pure proof and profile-specific config/service paths are preserved; local native clients pass pure admission. | [UDP fixture-pak downloads](2026-09-08-udp-pak-download.md) now complete byte-for-byte with pure admission in both reference directions for both profiles, baseline and impaired transport. The [interrupted UDP retry checkpoint](2026-09-08-download-retry-and-zombie-reconnect.md) verifies replacement of a retained partial file and corrects same-endpoint zombie-slot reconnect behavior in both profiles. The [whole-file pak integrity correction](2026-09-08-whole-file-pak-integrity.md) rejects short reads and CRC-mismatched payloads before exposing buffers, with both-profile corrupt/valid config and legitimate client runtime checks. The [allocation-bounds follow-up](2026-09-08-file-allocation-bounds.md) rejects unrepresentable whole-file lengths and prevents temporary-allocator alignment/header/capacity arithmetic overflow, with synthetic oversized-pak rejection in both profiles. Writing a latched cvar's current value cancels its pending value again ([latch record](2026-09-18-cvar-latch-cancel.md)), and range checks now cover values a config set before the cvar was registered, such as Quake III's own `com_hunkMegs 56` ([range record](2026-09-18-cvar-range-checks.md)); both apply to every profile. Golden search-path/pure transcripts and clean-install probes, broader home/config/mod paths, HTTP/failure downloads, case/path behavior, streaming integrity and server-referenced paks remain required. |
| Module ABI and VM/native loading | **PARTIAL — Unit/Live.** Retail qagame, cgame, and UI QVMs load and run a local session. Guarded synthetic [cgame](2026-09-08-q3-native-cgame.md), [UI](2026-09-08-q3-native-ui.md), and [qagame](2026-09-08-q3-native-qagame.md) DLLs also pass actual legacy vmMain/syscall transfers and callbacks; qagame covers shared-entity/collision access, restart and invalid-binding recovery. The [native loopback pair](2026-09-08-q3-native-snapshot-roundtrip.md) also checks initial/updated player and entity fields in client snapshots plus a live usercmd return. Frozen POD adapters, transactional DLL loading, QVM bounds, cgame traps/extensions, UI policy, and cvar flags have executable regressions. UI and cgame key events and key imports use Quake III's own key numbers, checked against quake3.exe's key table; they had been this engine's, whose mouse wheel q3_ui's menus read as joystick buttons that press the item under the cursor ([record](2026-09-19-q3-key-numbers.md)). | **PARTIAL — Unit/Live.** Retail qagame API 10 and cgame/UI API 8 load and run through the same client. Guarded synthetic native imports now exercise config/usercmd transfers through the running client; see the [native import checkpoint](2026-09-08-native-guarded-imports.md). Its [opt-in runner](2026-09-08-native-runner.md) now supports packed installs and private-desktop normal-exit verification. Structured contracts remain separate; the native snapshot's additional `0x58` bytes remain unresolved. Modules keep this engine's key numbers, which match all 111 that quakelive_steam.exe names ([record](2026-09-19-q3-key-numbers.md)). | Renderer configuration PODs and the Q3 cgame/UI adapter are frozen; see the [glconfig checkpoint](2026-09-08-glconfig-boundary.md). Both profiles marshal frozen configstring-transfer layouts; see the [game-state checkpoint](2026-09-08-gamestate-boundary.md). Incoming bot commands now marshal Q3/QL PODs with bounds checks; see the [bot-command correction](2026-09-08-bot-usercmd-boundary.md). QL user-command outputs use the frozen 28-byte adapter at cgame/qagame boundaries; see the [user-command checkpoint](2026-09-08-ql-usercmd-boundary.md). Resolve or safely contain the QL snapshot tail, complete the boundary audit, and broaden Q3 native and packaged-client module coverage. |
| Live network codec and handshake | **PARTIAL — Unit/Live.** Local protocol-68 sessions and both directions against a freshly built clean FnQuake3 peer at the pinned baseline reach active state after pure admission (71 with fnTech3 as client, 68 with fnTech3 as server); see the [pinned-peer checkpoint](2026-09-08-pinned-q3-interop.md). Both directions also pass [controlled loopback loss, delay and duplication](2026-09-08-q3-impaired-interop.md) and [initial gamestate fragment-loss recovery](2026-09-08-q3-fragment-recovery.md). The [second-fragment checkpoint](2026-09-08-second-fragment-recovery.md) also passes and corrects earlier protocol-68-only claims. [Terminal-fragment loss](2026-09-08-terminal-fragment-recovery.md) recovers in both peer directions. Shared lineage still limits peer independence. Field parity, truncated messages, and float encodings are tested for protocols 68/71. Live traffic with one sequenced datagram in eight corrupted in both directions drops each of 20 sessions cleanly, on the client's own decoder checks or the server's user-command check, with no crash, caught fault or hang, and the server answers throughout ([corrupted-traffic record](2026-09-26-net-corruption.md)). | **PARTIAL — Unit/Live.** Local protocol-91 sessions reach active state. FnQL's four connectionless checks -- protocol 91 in `getinfo`, the FnQL nonce echo, retail's binary SteamID/ticket challenge and silence for a malformed ticket -- pass against the current server ([handshake record](2026-09-26-ql-protocol91-handshake.md)). Both directions against the existing FnQL peer also pass [periodic loss/delay/duplication and initial gamestate-fragment loss](2026-09-08-ql-impaired-interop.md), with pure admission and native join UI/world rendering; [second-fragment loss](2026-09-08-second-fragment-recovery.md) and [terminal-fragment loss](2026-09-08-terminal-fragment-recovery.md) also recover. Reference source revision and active team gameplay remain unverified. Field parity and malformed-input tests pass; invalid usercmd batches drop only their sender without partial execution. Corrupted live traffic drops each of 20 sessions the same clean way ([corrupted-traffic record](2026-09-26-net-corruption.md)). | A [client-fragment checkpoint](2026-09-08-client-fragment-recovery.md) verifies exact, ordered delivery of six reliable commands after first-client-fragment loss in all four reference configurations. [UDP fixture-pak downloads](2026-09-08-udp-pak-download.md) also pass baseline and impaired reference transport. The [reliable acknowledgement unwind fix](2026-09-08-reliable-ack-error-unwind.md) prevents a rejected header from corrupting disconnect cleanup state. Golden packet transcripts, malformed/fuzz corpus, broader fragmentation/reliable/pure/download coverage, then bidirectional retail interop for each profile remain required. |
| Demos | **PARTIAL — Unit/Live.** New local `.dm_68` recordings play to normal completion. The [shipped-demo pacing check](2026-09-08-shipped-demo-pacing.md) also verifies normal-speed `.dm_66` playback and an engine world/HUD screenshot. Shipped four.dm_66/four.dm_68 bytes are identical, limiting their independence as wire samples. Executable tests preserve protocol-71 fallback and extension behavior. | **PARTIAL — Unit/Live.** New local `.dm_91` recordings play to normal completion after fixing an incorrect `.dm_68` suffix for QL captures without voice messages. The [retail demo checkpoint](2026-09-08-ql-retail-demo-interop.md) adds normal/timedemo cross-engine playback in both directions with complete short warmup recordings and world/HUD screenshots. | The [completion handoff checkpoint](2026-09-08-demo-completion-handoff.md) fixes premature exit when nextdemo starts another recording and verifies Q3/QL truncated-header/body completion plus intact recovery playback with the same Windows x86 client. The [invalid-length runtime checkpoint](2026-09-08-demo-invalid-length.md) verifies negative and oversized envelope rejection in both profiles, with bounded post-error liveness and explicit harness termination. The [pause/resume checkpoint](2026-09-08-demo-pause-resume.md) observes fixed QL frames and Q3 world rendering during cl_freezeDemo, followed by normal resumption; The [command-history correction](2026-09-08-paused-command-history.md) removes Q3's spurious paused connection warning, with Windows/Linux executable regressions and both-profile runtime evidence; the Q3 lagometer strip that still moves while paused moves identically in retail `quake3.exe` on the same shipped demo, so it is Q3 cgame behaviour, not an engine defect; see the [paused-lagometer record](2026-09-18-q3-paused-lagometer.md). The [retail controls comparison](2026-09-08-retail-demo-controls.md) verifies that QL restores timescale to retail's 0.1 minimum, holds cl_freezeDemo frames identically, and exercises the existing advance-and-hold console sequence under both profiles. Its paused-restart follow-up verifies retained pause, fixed first-frame rendering and eventual completion under both profiles, matching retail QL. The [drop-recovery investigation](2026-09-08-demo-drop-recovery-limit.md) confirms that ERR_DROP clears queued recovery scripts under both profiles; failed recovery attempts and premature QL control completions are recorded without claiming post-error recovery. The [RTCW demo record](2026-09-18-rtcw-demos.md) makes demo protocols profile-owned: Q3 keeps 66/67/68/71 and `.dm3` in its historical order, QL keeps 91, and neither admits RTCW's 49/60; fresh local Q3 `.dm_68` and QL `.dm_91` record, replay and timedemo runs pass with the changed client. The [damaged-demo record](2026-09-18-demo-faults.md) plays fixed envelope faults and 40 seeded body corruptions of two Q3 demos and one QL demo: no run crashes or hangs, where retail `quake3.exe` 1.32 crashes on 40 of the same 94 Q3 variants. Record and play every claimed format; broaden truncation, post-error recovery, seeking, timedemo, protocol switching, and unchanged termination behavior. |
| BSP, collision, and renderer asset decode | **PARTIAL — Unit/Live.** q3dm6 loads with GLx/Vulkan/RTX raster; the recorded RTX black-polygon defect is fixed and verified on the frozen demo. Separate Q3 x64 native probes cover baked lightmap and vertex-lit world materials. See the [RTX round](2026-09-06-rtx-round.md). Models scale the light grid's ambient by `quake3.exe` 1.32's `r_ambientScale`, `0.6` (`0x0045152a`; [ambient scale record](2026-09-25-ambient-scale.md)). The replacement path tracer (`2026-09-24-rtx-path-tracer.md`) traces eight retail maps' worlds, models and weapon with their shader materials and the maps' own q3map lights, with no Vulkan validation messages; its traced direct light is within 16% of the baked lightmaps' frame brightness on all eight. Its compute backend draws the same frozen frames as the ray-tracing pipeline on six maps, and the shared x86 client traces ten retail Q3 maps with it at 0.87-1.27 of their raster frames' brightness. Stencil shadows (`cg_shadows 2`) darken by GL's 0.6 in the Vulkan and RTX renderers too ([stencil shadow record](2026-09-24-stencil-shadow-finish.md)). Of 101 damaged copies of `q3dm1`, none crashes or hangs since the patch-grid guard, but seven fault on unchecked node, leaf, leaf-surface and brush indices and are caught by the exception filter ([damaged-map record](2026-09-26-bsp-faults.md)). | **PARTIAL — Unit/Live.** campgrounds BSP 47 loads with all three raster renderers and produces engine screenshots; the final shared RTX raster probe passes. Models scale the light grid's ambient by retail's `r_ambientScale`, `10`, a player option bounded to 1 through 100 and kept in `repconfig.cfg` (`quakelive_steam.exe` `0x00449013`); fnTech3 had used Quake III's `0.6` cheat, which drew a player model on `campgrounds` at 41% of retail's brightness ([ambient scale record](2026-09-25-ambient-scale.md); `tests/ambient_scale_defaults_tests.py`). The shared x86 client's path tracer (compute backend) traces twelve retail maps without error; their live views are the join-match menu, so they are not compared. No retail QL map keeps a light entity (q3map2 leaves them out), so the tracer lights QL's worlds from their lightmaps and its models from the light grid by default; a frozen demo frame reads 0.963 of raster's (`2026-09-24-rtx-path-tracer.md`). Stencil shadows (`cg_shadows 2`) darken by GL's 0.6 in the Vulkan and RTX renderers too ([stencil shadow record](2026-09-24-stencil-shadow-finish.md)). Of 105 damaged copies of `campgrounds`, none crashes or hangs, and five fault the same way ([damaged-map record](2026-09-26-bsp-faults.md)). | Broaden maps and collision paths, check the node, leaf, leaf-surface, model, brush and face indices whose damage still faults ([damaged-map record](2026-09-26-bsp-faults.md)), compare equivalent screenshot scenes, and prove live advertisement presentation/fallback. |
| Other assets (models, shaders, images, audio, cinematics, UI resources) | **PARTIAL — Unit/Live.** WAV decoder fixtures and a measured synthetic engine audio path pass; seeded corruptions of five installed Q3 movies pass the shared RoQ decoder under AddressSanitizer and the running client ([RoQ fuzz record](2026-09-18-roq-fuzz.md)); the remaining asset inventory is not signed off. | **PARTIAL — Unit/Live.** The same WAV checks and measured engine audio path pass; broader assets and WebUI resource regression remain incomplete. | The [WAV payload correction](2026-09-08-wav-payload-integrity.md) rejects incomplete whole loads and corrects short-read stream counts, with executable Windows/Linux regressions and exact decoded-byte comparison for 631 Q3 and 538 QL retail WAV paths. The [container-bounds follow-up](2026-09-08-wav-container-bounds.md) rejects truncated/out-of-container headers and checks packed/loose seek results, while preserving terminal odd-data acceptance observed in stock Q3 assets. It repeats the identical corpus output and clarifies that the installed Q3 corpus includes 18 add-on paths. The [extensible identity correction](2026-09-08-wav-extensible-identity.md) validates full PCM/float GUIDs and extension lengths, with malformed GUID fixtures and eight exact synthetic decodes. Valid-bit/channel-layout semantics and audible/backend validation remain open. The [engine audio-output checkpoint](2026-09-08-engine-audio-output.md) measures silence controls, 997 Hz output and half-volume gain response through the same client under both profiles, using file output without speakers. The [streaming-music checkpoint](2026-09-08-engine-music-streaming.md) adds PCM16/24/float32 intro-loop handoff, channel routing, stop-to-silence and malformed-track recovery through both running profiles. The [rate-conversion investigation](2026-09-08-stream-resampling-gap.md) exposed per-chunk frame/phase drift. The [persistent resampling correction](2026-09-08-persistent-stream-resampling.md) now passes exact partition comparisons, queue/fallback checks, both-profile rate conversion and mixed-rate stop/replay probes; broader raw/device lifecycle and performance coverage remain open. The [PCM8 WAV correction](2026-09-08-wav-pcm8-stereo.md) normalizes unsigned multichannel WAV bytes at the codec boundary, preserving raw-module conventions and correcting the measured stereo silence/tone defect in both profiles. The [multichannel endian correction](2026-09-08-wav-multichannel-endian.md) swaps every complete PCM16 scalar sample across channels and partial frames, with failing-before simulated-endian regressions and Linux sanitizer coverage; full big-endian runtime remains unverified. The [extensible precision correction](2026-09-08-wav-extensible-precision.md) rejects precision larger than its container, preserves reduced-precision PCM scaling and verifies malformed-track recovery followed by measured intro/loop output in both profiles; the speaker-mask correction is recorded below. The [speaker-mask investigation](2026-09-08-wav-speaker-mask-gap.md) recorded center-to-right misrouting. The [speaker-routing correction](2026-09-08-wav-speaker-routing.md) preserves internal mask metadata, mixes non-native layouts by speaker position in both backends, aligns music reads to whole frames, and verifies both-profile center/control, static, layout-transition, six- and three-channel file output; hardware surround and legacy DMA output remain unverified. The [loaded-speaker bounds follow-up](2026-09-08-speaker-buffer-bounds.md) validates source ranges before routing and avoids metadata-size overflow after rejected conversion, with a failing-before executable regression and repeated both-profile loaded-center output. The [SDL legacy-output checkpoint](2026-09-08-sdl-legacy-output.md) verifies both-profile streamed/static speaker routing and seven-channel file output on a separately built SDL x86 engine, while recording persistent silence gaps as an open continuity issue; native hardware output remains unverified. The [legacy timing trace](2026-09-08-legacy-timing-trace.md) links the recurring 50 ms gap component to the cursor offset and records near-half-second update intervals despite at most 5 ms of pre-paint work; timing defaults remain unchanged and continuity remains open. The [client-stage trace](2026-09-08-client-screen-timing.md) places measured long stalls inside screen updates under both profiles; UI/render/presentation attribution and continuity remain open. The [renderer-stall investigation](2026-09-08-renderer-stall-localization.md) measures delays at OpenGL synchronization points and removes them in a front-buffer control on the private desktop. The [locked-session follow-up](2026-09-25-locked-session-presentation-stall.md) finds those delays only in runs made while the Windows session was locked, and none on an unlocked private desktop, so continuity with normal presentation can be measured there; it has not been. The [converted WAV short-read correction](2026-09-08-wav-short-read-alignment.md) preserves scalar alignment across positive filesystem short reads, with failing-before partition regressions, sanitizers and unchanged both-profile corpus output. The [native WAV follow-up](2026-09-08-native-wav-short-reads.md) accumulates positive short reads for native PCM as well, with simulated-endian regressions and exact streaming reconstruction of all 1,169 installed WAV paths under 509-byte filesystem reads. The [Ogg payload correction](2026-09-08-ogg-payload-bounds.md) validates decoded-size metadata before narrowing and rejects incomplete whole loads, with executable fault/cleanup regressions and both-profile retail-fixture smoke output; callback and broader Ogg corpus gates remain open. The [Ogg callback correction](2026-09-08-ogg-callback-bounds.md) bounds read/seek arithmetic and verifies real positions, with exact decoded comparison across 559 retail Ogg paths and both-profile compressed-pak playback smoke evidence; broader malformed-container gates remain open. The [Vorbis speaker-order correction](2026-09-08-vorbis-speaker-order.md) fixes observed center-to-right routing, preserves encoded-field tags and passes both-profile three/six-channel loaded/music file-output probes under OpenAL and SDL legacy; wider hardware and layout coverage remains open. The [older OpenAL fallback correction](2026-09-08-openal-unmatched-speakers.md) preserves unmatched speakers when direct remix is unavailable, with failing-before Linux center-channel output, corrected loaded/music measurements and both-profile Windows regression evidence. Per-format fixtures plus windowed runtime scenes for both profiles; confirm error handling and cross-renderer/audio consistency without redistributing assets. |
| WebUI native requests and pending avatars | **PARTIAL — Source.** QL-only activation and frame guards are preserved; this port does not enable WebUI for Q3. | **PARTIAL — Unit/Build/Fixture.** Notification scheduling, bounded Unicode transport, avatar backoff/batching, and reload cleanup are tested; fnTech3's production adapter passes an offscreen retail-runtime probe. See the [port record](2026-09-06-webui-request-port.md). The retail Video page shows its Ambient Light Scale row again, and the configuration snapshot publishes `r_ambientScale`, now a player option ([ambient scale record](2026-09-25-ambient-scale.md)). | Confirm affected Linux/Proton gameplay performance, live Steam-avatar callbacks through the engine, and windowed renderer/lifecycle stress. The source-reported DataPak document-query limitation remains open. |
| Renderer API/lifecycle | **PARTIAL — Unit/Live.** All three modules initialize, render a local session, and shut down. Q3 x64 native RT survives renderer restart and windowed resize with Vulkan validation; that evidence applies to the diagnostic Q3-only executable. The path tracer repeats restart, resize, map change and raster/RT switches with no validation messages (`2026-09-24-rtx-path-tracer.md`). | **PARTIAL — Unit/Live.** All three modules initialize, render the native join UI/world, and shut down with WebUI disabled. This machine's x86 driver lacks the ray-tracing pipeline; the shared x86 client path traces with its compute backend in `rtx_rt_require 1` runs, then restarts to raster, but no 32-bit Vulkan validation layer is installed to check it. | The shared x86 client's hardware backend on a driver that offers it, x86 validation-layer runs, broader renderer switch/restart and screenshot parity; HDR, performance, and WebUI resize/device-loss/crash stress. RTX raster startup does not prove ray tracing. |
| Steam and optional services | **PARTIAL — Unit/Build/Fixture.** AppID 2200 discovery supplies the Q3 base path without enabling QL services or adding a second Q3 Steam search tree. | **PARTIAL — Source/Unit/Live.** The [retail QL checkpoint](2026-09-08-ql-retail-auth-interop.md) observes platform authentication validation, pure admission and native join UI/world with the retail client on fnTech3. The [dedicated-server follow-up](2026-09-08-ql-dedicated-retail-interop.md) adds fnTech3 client admission and first-person warmup against the Linux x86 retail server. Broader authentication gates remain open. | Broader Q3/QL Steam-path validation; QL logged-in/offline/failure behavior; session-ticket admission both ways; callback/shutdown fault tests; live match reports, unattended Workshop download, and sustained GameServer stress. |
| Dedicated server | **PARTIAL — Build/Unit/Fixture.** [Retail bot combat](2026-09-08-retail-bot-runtime.md) now passes with the corrected incoming command boundary. q3dm6, retail qagame QVM, botlib, pure=1 info response with protocol 68, and clean shutdown pass for automatic and explicit profile selection. A running server's `sv_maxclients` change now spawns the level again instead of overflowing the stack ([client-slot stack correction](2026-09-24-client-slot-stack-overflow.md)). A map load and an immediate `map_restart` now run each settling frame, and the last one, at the current time before advancing it, as `quake3.exe` 1.32 does, so the game's first frame runs at its start time; the game prints no millisecond time that a live run could compare, so this rests on disassembly and fixtures ([settle order record](2026-09-24-settle-frame-order.md)). | **PARTIAL — Build/Unit/Fixture.** campgrounds/ffa, retail API-10 qagame, botlib, pure=1 info response with protocol 91, and clean shutdown pass using the same binary. A map load and an immediate `map_restart` now run each settling frame, and the last one, at the current time before advancing it, as `quakelive_steam.exe` does; disassembly and fixtures ([settle order record](2026-09-24-settle-frame-order.md)). | Windows x86 and native Linux i686 discovery/map probes now pass for both profiles; see the [portability checkpoint](2026-09-07-build-portability.md). Windows dedicated early and initialized fatal errors now exit with status 1 without console dismissal; see the [fatal-exit checkpoint](2026-09-08-windows-dedicated-fatal.md). The [retail bot checkpoint](2026-09-08-retail-bot-runtime.md) admits four QL bots. The [follow-up](2026-09-08-bot-trace-and-probe-corrections.md) corrects command-queue interference, localizes timestamp-only logging to the retail DLL and fixes botlib no-entity traces; the subsequent [world-link restart fix](2026-09-08-world-link-restart.md) passes a shrinking native entity-binding regression and a free-running retail QL restart with four bots, IN_PROGRESS state and a scored frag. A subsequent [retail transition series](2026-09-08-retail-transition-series.md) passes three explicit restarts and two map changes per profile with four bots at all six checkpoints; QL reports IN_PROGRESS throughout those samples. The [bot-profile default checkpoint](2026-09-08-bot-profile-defaults.md) corrects QL bot_grapple to the retail engine default 1 while retaining Q3 default 0 and explicit overrides. The [retail QL restart checkpoint](2026-09-08-retail-ql-map-restart.md) verifies one connected warmup restart per retail direction, preserving authentication and rendering after restart. The [full retail map-change checkpoint](2026-09-08-retail-map-changes.md) verifies one destination-map reload and active admission per retail direction under both profiles. Longer transition stress remains open. Connect retail clients, exercise pure admission/live messages, and broaden long/faulted sessions. A connectionless pure=1 response does not prove pure negotiation. Bots ring 64 reliable commands in every slot, as `quake3.exe` 1.32 and `quakelive_steam.exe` do; a fnTech3 Quake III bot sent a burst of addressed commands held 64 and was dropped on the 65th, before and after the window became the profile's ([bot window record](2026-09-24-bot-reliable-windows.md)). |
| Cross-profile isolation | **PARTIAL — Unit/Fixture/Live.** Active profile is immutable; config read and write select `q3config.cfg`. Both profiles start sequentially from the same staged distribution with isolated homes. With one writable home shared by `q3`, `ql`, `rtcw-sp` and `rtcw-mp`, Q3 restarts with only its own archived values and protocol 68, runs its QVMs and persists only to `baseq3/q3config.cfg`; see the [shared-home record](2026-09-18-profile-isolation.md). One pair of developer binaries also runs `q3`, `ql`, `rtcw-sp` and `rtcw-mp` in turn with separate homes: each client reaches its stock map and writes an engine screenshot, and each multiplayer server answers `getstatus` ([release-profile record](2026-09-26-release-profiles.md)). | **PARTIAL — Unit/Fixture/Live.** Active profile is immutable and QL's `qzconfig.cfg`/`repconfig.cfg` save path is preserved. In the shared home QL restarts with its own values and protocol 91 and loads native modules from its own cache; it imports Q3's `q3config.cfg` once, only when no `qzconfig.cfg` exists, as the documented one-way migration. The same pair of binaries runs QL's client and server between the other three profiles ([release-profile record](2026-09-26-release-profiles.md)). | Broaden download, Steam-state, dedicated-server and concurrent-process isolation checks. In a shared home Q3 and QL keep one `baseq3/servercache.dat`, the preserved legacy layout. |
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
The [entity box contents record](2026-09-23-entity-box-contents.md) gives RTCW
MP the temporary-box contents retail `WolfMP.exe` uses: a server sweep of a box
entity tests and reports the entity's own contents, and `CONTENTS_BODY` comes
back on every way out of the sweep. RTCW SP keeps `CONTENTS_BODY`, as
`WolfSP.exe` does. Real-trace fixtures cover all five frozen entity layouts and
a recorded-call fixture every catalog profile; no retail session was compared.
Two related differences stay open: `WolfSP.exe`'s sweep skips a carried prop,
and `WolfMP.exe`'s point contents place an entity at its `s.origin`.
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

The 2026-09-24 path tracer record (`2026-09-24-rtx-path-tracer.md`) adds the
first native RT runtime observations for both modes. In the shared Windows
x86 developer client, whose driver here lacks the ray-tracing pipeline, the
RTX path tracer's compute backend traces 25 of the 26 retail SP maps probed and
all 15 MP maps with their light entities under RTCW's compiler rules (linear
and angle-free by default, `fade`, grid-only `lightJunior`, and no light from
a light whose target names no entity, as the retail lightmaps show), the
maps' global fog, and the main view behind a portal sky, at 0.81-1.33 (SP) and
0.97-1.50 (MP) of their raster frames' brightness, with no errors and the
installation unchanged; `norway` stops in its game script, in raster too. The
earlier record's `escape1` figure compared the raster main view with itself:
its portal sky took the trace. The torch `dlight` entities add 0.95 of the
light raster's dynamic light pass adds on `crypt1`, the `negdlightshader` and
`flamedlightshader` materials multiply as the original renderer drew them,
and a retail MP demo replays traced at 1.54 of raster's brightness (vertex-lit
terrain at night). No 32-bit Vulkan validation layer is installed here.
Original-engine visual parity and campaign-length native RT runs remain open.
`norway` stopped because the probe loads a map with `devmap`, which RTCW SP
runs as free-for-all: the retail game then runs each spawn script as its
entity spawns, and `floating_ice2`'s names a `path_corner` 614 entities
later. Loaded from a campaign save, `norway` traces without error. Since the
record's fifth round the default lighting is the sky and the surfaces that
give off light, as Q2RTX lights Quake II; the brightness ratios above were
measured under the compilers' light models, now `rtx_rt_lighting 2`. Under
the new default an RTCW room lit only by light entities with no visible
fixture is lit by the daylight and emitters that reach it and their bounce,
in linear light and tone mapped as Q2RTX maps its views; a windowless one,
such as `escape1`'s opening cell, stays dimmer than its lightmap.

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

The [navigation-cache recovery record](2026-09-20-rtcw-cache-recovery.md) adds
missing/damaged-cache generation and portable RCD15/RCD12 writing. Three SP maps
regenerate both worlds, then exchange the resulting caches with retail WolfSP
in both directions. Original/iortcw visibility-generation comparisons and
allocation/write-failure tests pass on Windows and sanitized Linux. A separate
retail comparison retains small waypoint/route-cost differences as an open
parity gate; MP cache generation still has fixture evidence only.

#### RTCW gate matrix

| RTCW gate | SP | MP | Required closure |
| --- | --- | --- | --- |
| Official maps and saves | Source/Unit/Live; 32 individual map launches with both AAS worlds, 25 gameplay save/reload/fresh-process cycles, repeat/cross-map/autosave and malformed-header handling; two-map retail save interchange; cinematic saves refused; the final cutscene now ends the campaign in the endgame credits menu instead of an unsupported-import drop; see the [endgame record](2026-09-18-rtcw-sp-endgame.md) | Source/Unit/Live; 15 individual map launches, advertised modes and native warmup restarts with advancing match/usercmd clocks; separately authorized soldier movement/firing retest; `map_restart` keeps `g_gametype` to a type the map's worldspawn allows before any delay, as `WolfMP.exe` does, and one console sequence on `mp_destruction` and `mp_beach` leaves the same types on retail and fnTech3 dedicated servers, where the previous build restarted into types the game then corrected ([game-type record](2026-09-23-rtcw-mp-restart-game-type.md)) | Full campaign transitions and sustained AI/combat; broader historical/damaged save payloads; MP peers, objectives and other classes/weapons |
| Title/profile selection | Source/Unit/Build/Fixture; registered and gated; installed Windows x86 SP-to-MP menu handoff using the same executable, correct modules and inspected captures; synthetic Windows/Linux handoff; the client binary that runs Q3, QL and MP reaches the `escape1` briefing with SP's own modules ([release-profile record](2026-09-26-release-profiles.md)) | Source/Unit/Build/Fixture; installed Windows x86 MP-to-SP menu and active-briefing handoffs using the same executable, correct modules and inspected destination captures; synthetic Windows/Linux handoff; the client and server binaries that run Q3 and QL reach `mp_beach` with MP's own modules and answer `getstatus` ([release-profile record](2026-09-26-release-profiles.md)) | Broader live menu/session transitions; macOS handoff build/runtime |
| Filesystem/config/pure | Source/Unit/Live; archive/config policy, ordered module/root lookup and intentional no-cache policy integrated; original SP pure default/flags; explicit pure-proof policy unavailable; with one writable home shared with Q3, QL and MP, SP restarts with only its own `main/wolfconfig.cfg` values and protocol 49 and loads only its own modules ([shared-home record](2026-09-18-profile-isolation.md)) | Source/Unit/Build/Fixture/Live, inbound Interop subset; in the same shared home MP restarts with only its own `main/wolfconfig_mp.cfg` values and protocol 60 and loads only `_mp_` modules from its own native cache; a first MP start without its own server cache imports retail WolfMP's favorites once, checked against a byte-identical retail `WolfMP.exe` rewrite of the cache ([favorites record](2026-09-18-rtcw-mp-favorites.md)); profile-owned two-native-module proof, pinned-module pak validation and local pure admission with retail modules; original/iortcw proof and verifier comparisons; a retail MP client passes inbound pure admission on one map and reaches the world while the server withholds the loaded-pak names, so RTCW MP needs no `sv_pakNames` and keeps ET's publication scoped away from it; see the [pure-name record](2026-09-17-rtcw-mp-pure-names.md); an fnTech3 client also passes outbound pure admission to retail `WolfMP.exe` and joins both teams, after the [handshake record](2026-09-18-rtcw-mp-handshake.md) replaced a Q3 protocol-68 connect the retail server rejected; both directions then pass pure admission on all 15 stock MP maps, after the [pure-map record](2026-09-18-rtcw-mp-pure-maps.md) stopped `mp_sub` falling back to degraded pure under a Q3-sized gamestate estimate | SP pure contract; mods and architecture policy; retail mount observations, malformed and partial-install probes; iortcw MP favorites import |
| Module ABI | Source/Unit/Build/Fixture; 2,215 original/iortcw assertions on x86/x64; bounded bot-entity/movement-input/goal/movement-result/prediction/state/parser/trace/render/vertex/orientation records, 10-bit game entity limit, mode imports/exports, popup/menu/key translation, command values and native x86 game-owned animation pointer handoff; elementary bot actions match the original and iortcw `EA_` functions in 344,099 comparisons each, sanitized on Linux ([bot action record](2026-09-26-rtcw-bot-actions.md)) | Source/Unit; 2,205 original/iortcw assertions on x86/x64; bounded bot-entity/movement-input/goal/movement-result/prediction/state/parser/trace/render/vertex/orientation records, 10-bit game entity limit, mode imports/exports, popup/menu/key translation, command values and WOLFINFO; elementary bot actions match the original and iortcw `EA_` functions in 344,099 comparisons each, sanitized on Linux ([bot action record](2026-09-26-rtcw-bot-actions.md)) | An audit of all 435 SP/MP import ordinals finds 420 dispatched after the [endgame record](2026-09-18-rtcw-sp-endgame.md) added SP's `G_ENDGAME`; the fifteen refused by name are unhandled in the original engines too, deliberately refused (MP `UI_OPENURL`), or reachable only from a Mac SP build (`G_FS_COPY_FILE`). Remaining: retail UI/cgame/qagame execution coverage and live game-data binding; distinguish original/iortcw module flag/renderer contracts |
| Live wire and demos | Source/Unit/Fixture/Live; 1,143 matched native message encodings plus a decoder-only dictionary fixture; first local snapshot with installed modules and pure mode off; `.dm_49` recording under original SP's synchronous-client precondition, and SP replay refused because retail SP cannot replay SP demos (`VM_Call with NULL vm` on its own recording); see the [demo record](2026-09-18-rtcw-demos.md). A zombie's snapshot carries SP's own empty player state, in unit tests ([zombie snapshot record](2026-09-24-zombie-snapshots.md)) | Source/Unit/Fixture/Live, demo Interop subset; 1,117 matched native message encodings; local pure admission and first map snapshot with installed modules; `.dm_60` record, replay and timedemo; retail `WolfMP.exe` replays an fnTech3 demo, and fnTech3 replays demos retail recorded against the fnTech3 server, pure and non-pure; see the [demo record](2026-09-18-rtcw-demos.md); original and iortcw handshake layouts in both directions, `sv_onlyVisibleClients` with its client culling matching the original function on 100,000 cases, and outbound retail admission; see the [handshake record](2026-09-18-rtcw-mp-handshake.md). A zombie's snapshot carries MP's own empty player state, so a retail `WolfMP.exe` client that fnTech3 drops now reads it and runs the notice and `disconnect`, and every retail client reads a quitting server's last message, where both used to fail with `CL_ParsePacketEntities: end of message` ([zombie snapshot record](2026-09-24-zombie-snapshots.md)). Fixed envelope faults and seeded body corruptions of two MP demos are completed or rejected with no crash or hang; one run stopped after 16 of its 47 variants ([damaged-demo record](2026-09-18-demo-faults.md)). Live traffic corrupted one sequenced datagram in eight drops each of 20 `mp_beach` sessions cleanly, with no crash, caught fault or hang, and the server answers throughout ([corrupted-traffic record](2026-09-26-net-corruption.md)) | Sustained live state integration, netchan rules, iortcw peer runs, retail transcripts, broader demo coverage (other maps, active play, malformed RTCW demos, seeking) and bidirectional interoperability |
| Assets/renderers/audio | Source/Unit/Build/Fixture; RTCW v47 selection, stock `escape1` collision probe, frozen render records, basic scene/polygon/gradient imports, CPU 2D comparisons, MDC decoding/reference comparisons and native drawing through a synthetic Q3 fixture; indexed MD3/MDC/MDS tags, MDS poses and continuous LOD pass installed-asset CPU comparisons; native synthetic MDS draws through all three renderers; skin metadata/model lookup matches all 412 installed skins against four references; brush-surface shader lookup matches 1,925 cases per reference; explicit VM pointer/basic UI sound imports and source-compared dynamic/model lights integrated; entity types 2-8 reach the generated-surface path as in retail `WolfSP.exe`, the water ripple flat and the tapered rail core as the debug axis, in all three renderers ([ripple record](2026-09-23-rtcw-water-ripples.md)); the engine's static map lights, surface-light proxies and `r_dlightTest` in the per-pixel modes, with the modules keeping their 32 lights a frame, shown on, off and on again in a live `escape1` client in all three renderers ([engine scene lights record](2026-09-24-rtcw-engine-lights.md)); a static light file for each of the 32 official maps, fitted from its `light` entities and served from the package, drawn in a live `escape1` client in all three renderers ([map lights record](2026-09-24-rtcw-map-lights.md)); of 102 damaged copies of `escape1`, none crashes or hangs, and six fault on unchecked indices and are caught ([damaged-map record](2026-09-26-bsp-faults.md)) | Source/Unit/Build/Fixture/Live; same contracts plus rotated pictures, stock `mp_beach` collision probe and inspected French native menu captures in all three renderers; the same entity types checked against retail `WolfMP.exe`; a Thompson fired into `mp_beach`'s surf draws flat ripples in all three renderers, where the previous build stopped with `Bad reType`; `mp_beach` loads in all three renderers with the console in RTCW's own charset rather than Quake Live's fonts ([ripple record](2026-09-23-rtcw-water-ripples.md)); the same engine scene lights, drawn in `mp_ice` replays in all three renderers and byte-identical to the build without them when switched off, in mode 0 or with `r_dynamiclight 0` ([engine scene lights record](2026-09-24-rtcw-engine-lights.md)); a static light file for each of the 15 official maps, fitted for MP's `r_dlightScale`; `mp_castle` and `mp_base` replays in all three renderers load them from the package, and `mp_castle`'s frames with them on match its frames with them off byte for byte in mode 0 and with a package lacking them ([map lights record](2026-09-24-rtcw-map-lights.md)); stencil shadows darken by GL's 0.6 in the Vulkan and RTX renderers too ([stencil shadow record](2026-09-24-stencil-shadow-finish.md)); of 101 damaged copies of `mp_beach`, none crashes or hangs, and six fault the same way ([damaged-map record](2026-09-26-bsp-faults.md)) | A same-frame ripple comparison with retail, which runs the same probe to the same place but paces its frames differently; original-engine collision parity, full RTCW material flags, live corona/decal and GL fog parity, Vulkan/RTX scene-fog original-engine parity and native RT runtime validation, projected world-light GPU parity, retail MDS scene/tag and skin/model assembly validation, shader remap/GPU parity, remaining shader/fog/animation behavior, music/fade/cinematic contracts and further retail RTCW engine-render-target screenshots; MDC/MDS ray transport has CPU evidence only; the collision and renderer index checks whose absence the damaged-map record shows |
| Console and HUD text | Source/Unit/Build/Live; retail `WolfSP.exe`'s `gfx/2d/bigchars` charset, `console` background and `console2` logo layer are registered, and the logo is drawn in retail's place, in engine screenshots beside retail's own at 640x480 and at 1280x720 ([console logo record](2026-09-23-wolfenstein-console-logo.md)). Source/Unit/Live; notify lines, their `con_notifytime` default of 3 and the chat line as `WolfSP.exe` draws them, including while disconnected and over a cinematic, in unit tests and a live FFA `escape1` frame ([notify record](2026-09-23-profile-notify-lines.md)) | Source/Unit/Build/Live; retail `WolfMP.exe`'s `gfx/2d/hudchars` charset, `console-16bit` background and `console2-16bit` logo layer are registered and drawn, and match a retail engine screenshot at 640x480; the shipped HUD rules keep matching the charset handle the cgame shares, in unit tests and a live `mp_beach` frame ([console record](2026-09-23-rtcw-mp-console.md)). Notify lines with `WolfMP.exe`'s default of 7, its translated chat prompts, and nothing drawn in limbo, in unit tests and live `mp_beach` frames ([notify record](2026-09-23-profile-notify-lines.md)) | Retail's separator bar and 8x16 text cell; whether RTCW's console should default to the textured style; SP's console hidden while connecting unless `con_debug` is set |
| Navigation | Source/Unit/Build/Fixture/Live, RCD15 Interop subset; live AAS8/RCD15 loading; missing/damaged-cache generation and writing, both worlds on three installed SP maps, fresh-process reload and bidirectional retail cache interchange ([recovery record](2026-09-20-rtcw-cache-recovery.md)); checked 128-state goal/movement lifecycle; shared entity updates with owned area links, per-world blocking, checked BSP imports, frame/state/time adapters and empty-config item updates; basic/trace/route/visibility, hide/attack, movement prediction and release ShowRoute imports; SP corner search; 70-pair graph/cost, route/cache and visibility comparisons with original/iortcw; tactical/prediction synthetic comparisons and 64-member installed prediction with stated arithmetic limits | Source/Unit/Build/Fixture; checked 64-state goal/movement lifecycle; same shared entity/ownership, BSP, frame/state/time, query, prediction and empty-item contracts; MP truncating costs versus SP ceiling, grid search and tactical occupancy verified; installed prediction uses SP geometry; RCD12 loading, missing/damaged-cache generation and writing use executable fixtures; stock MP keeps bots disabled | Dynamic-entity prediction and original BSP parity; cold-cache tactical divergences, installed-map/live tactical observations; installed MP-cache observations, generated waypoint/route-cost differences from retail, x87/fast-math numeric and decision differences, original-engine runtime route and gameplay evidence |
| Gameplay and lifecycle | Open campaign; Source/Unit/Fixture for AI state lifecycle, camera evaluation, clean mission-briefing shutdown, shared movie-owner cleanup, main-menu shutdown and partial navigation-load cleanup; Windows x86 GL startup progresses through the opening cinematic to inspected first-person knife/HUD captures with either active audio backend and nonzero SP speech queries; Source/Unit for each cast AI's reliable window, 256 commands in every slot as `WolfSP.exe` rings them, where a slot no player had used rang Quake III's 64 ([bot window record](2026-09-24-bot-reliable-windows.md)); Source/Unit/Live for the settling frames of a map load and `map_restart`, each run at the current time before advancing it as `WolfSP.exe` does, from a restored save's time too: the retail game's `rocket` script timeline for 2.5 s after a load or restart matches `WolfSP.exe`'s line for line ([settle order record](2026-09-24-settle-frame-order.md)) | Source/Unit/Fixture for local map initialization, profile-specific camera timing, briefing-movie playback/shutdown and active-briefing-to-SP handoff; Source/Unit for a bot's reliable window, 256 commands in every slot as `WolfMP.exe` rings them, where a slot no player had used rang Quake III's 64 ([bot window record](2026-09-24-bot-reliable-windows.md)); Source/Unit/Live for a thrown engineer's charge coming to rest on `mp_beach` on listen and dedicated servers, where the previous build dropped at the world-owned charge's trace ([explosives record](2026-09-23-world-owned-explosives.md)); Source/Unit/Live and inbound Interop subset for `map_restart` after a variable change: only a new `sv_maxclients` reloads the level, as `WolfMP.exe` does, and a retail client connected across a new game type or `sv_pure` gets a `map_restart` command where the previous build sent it a new gamestate; live pure-proof checking is still unlike retail's ([restart record](2026-09-24-wolfenstein-restart-variable-change.md)); Source/Unit/Live for the settling frames of a map load and `map_restart`, each run at the current time before advancing it as `WolfMP.exe` does: the retail game's `mp_rocket` script timeline for 2.5 s after a load or restart matches `WolfMP.exe`'s line for line ([settle order record](2026-09-24-settle-frame-order.md)); gameplay remains open | SP movement/combat, broader character parity, audible speech/facial timing, campaign/AI/save-load/transitions; MP classes/objectives/maps; broader partial initialization, sustained play and shutdown |
| Music and fades | Source/Unit/Build/Fixture; original-source queue/fade and SP loop/mixer comparisons; both-backend clear modes with preserved music and bounded PCM/gain routing; native menu music buffers on OpenAL; legacy initialized while muted | Source/Unit/Build/Fixture; exact two-argument import; native menu music buffers on OpenAL; legacy initialized while muted | Measured output and original playback parity; game-script triggers, save-load restoration and backend/device lifecycle |
| One-shot audio | Source/Unit/Build/Fixture; checked native/QVM imports, original-source channel-policy and fixed-range gain comparisons, legacy/OpenAL lifetime and failure handling; SP client-channel replacement and auto-channel separation. Source/Unit/Live: the OpenAL backend renders SP's one-shots and loops through the engine's own features, not retail's pan. They are positional, carried by the device's distance model across retail's span with a horizon fade, traced for occlusion, sent to the zone or traced room and given a tone class by channel. On `escape1` the loops are traced to occlusion 0.84 to 1.00 and sent to the 397-zone sidecar's room, and the death cry at the listener takes the `voice` class. Retail's exact gain and pan stay in the legacy mixer ([OpenAL voices record](2026-09-24-wolfenstein-openal-voices.md)) | Source/Unit/Build/Fixture; same boundaries/backends, original MP auto-channel reuse and FIFO policy; inspected briefing startup on both backends. Source/Unit/Live: the same OpenAL rendering; on `mp_beach` the in-range loops are positional `world` sounds at retail's 89/127 with sends into the 246-zone sidecar, where the previous build played every one head-relative with no send ([OpenAL voices record](2026-09-24-wolfenstein-openal-voices.md)) | Queued/threaded timing; output-device/PCM parity; sustained game audio and resource pressure; documented safe exhaustion differences; a listening test; an in-range remote one-shot observed at runtime; HRTF on a runtime that offers it |
| Entity streams | Source/Unit/Build/Fixture; checked start/stop imports, shared decoder, eleven entity slots, queued replacement, immediate entity stop, bounded fades, both-backend synthetic stream runtime and installed introductory speech with nonzero amplitude | Source/Unit/Build/Fixture; checked start import, twenty-three entity slots, original intro-repeat loop policy, both-backend synthetic silent stream runtime | Original PCM/device timing; dialogue and mouth synchronization parity; save/load restoration; source-different SP loop correction and fade-slot admission; device recovery/resource pressure |
| Speech amplitude/default sound | Source/Unit/Build/Fixture; signed one-shot/peak-stream measurements on both backends; 299,264 matching cases per original/iortcw SP reference on three targets; installed intro completes with nonzero queries; eight-frame silent default | Source/Unit/Build/Fixture; original zero speech response; 512-frame silent default; both-backend installed briefing startup | Original clock/output/facial parity; full dialogue/codec coverage; device recovery and resource pressure |
| MP localization | Not an SP import; SP module key names now follow `cl_language` as original SP's do (5,472 original-function comparisons across both modes); see the [key-name record](2026-09-18-rtcw-key-names.md) | Source/Unit/Fixture/Live; all eight installed dictionaries (1,201 records), 818,856 translations per original/iortcw reference, Windows x86/x64 and Linux x86 sanitizers; inspected French native main menu in all three renderers; module key names in the player's language, pinned to retail `WolfMP.exe`, and a live `cl_language` change switching the spectator hint's text and key names exactly as retail does; see the [key-name record](2026-09-18-rtcw-key-names.md). A dedicated server's console echoes broadcast prints without the `[lon]` and `[lof]` tokens, as `WolfMP.exe` does, while clients still receive them, in unit tests and a live `mp_beach` log that matches retail's ([broadcast echo record](2026-09-24-rtcw-et-broadcast-echo.md)). A client's drop is announced as `print "[lof]%s^7 [lon]%s\n"`, as `WolfMP.exe` does, so a client translates the reason and leaves the name as it is; a retail observer received it from a fnTech3 server ([drop notice record](2026-09-24-rtcw-et-drop-notices.md)) | Remaining UI/cgame rendering, byte/font agreement, and menu files after a live change (`ui_restart`); optional translation-authoring diagnostics remain unimplemented |
| Cinematic startup/presentation | Source/Unit/Build/Fixture/Live; catalog-owned repeat policy and wolfintro selection, letterbox import/command, 24 fps header timing, clock handoff/rewind/wrap and inspected movie frames on all three raster renderers; [bounded decoding and same-process malformed-movie recovery](2026-09-17-roq-bounds.md), installed movie corpus and final-frame checks; seeded corruptions of `menu.roq`, `menu2.roq` and `wolfintro.RoQ` pass the decoder under AddressSanitizer, and 60 in one running SP client recover to the unmodified movie ([RoQ fuzz record](2026-09-18-roq-fuzz.md)) | Source/Unit/Build/Fixture/Live; catalog logo/intro policy, original 30 fps behavior, missing-startup-movie fallback and briefing movie startup; same bounded decoder, installed movie corpus and malformed-movie recovery; seeded corruptions of `mp_beach.roq` and `mp_rocket.roq` pass under AddressSanitizer, and 60 in one running MP client recover | JPEG no-op matches original SP/MP source. Original-engine behaviour on the corrupted movies, original visual/timing parity, audible A/V synchronization, game-script fades and full campaign movie transitions remain required |
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
- Announce a rage quit as retail does, `print "%s ^1rage^7quits\n"`. The
  retail game's `ragequit` command calls game import 192 for it, which
  fnTech3 binds as `MarkClientGotCP`; see the
  [drop notice record](2026-09-24-rtcw-et-drop-notices.md).

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
| Module ABI / server memory | Source, Unit, Live subset | 1,326 layout/constant/ordinal checks, native qagame startup, and every declared game import handled: 204 of 212, the other eight being ordinals retail ET refuses itself. See the [import](2026-09-17-wolfet-game-imports.md), [navigation](2026-09-17-wolfet-navigation.md), [visibility](2026-09-17-wolfet-visibility.md) and [prediction](2026-09-17-wolfet-prediction.md) records. The cgame surface handles 146 of 147 imports and the UI surface 114 of 121, each refusing the rest by name; see the [cgame](2026-09-18-wolfet-client-boundary.md), [UI](2026-09-18-wolfet-ui-boundary.md), [sound](2026-09-18-wolfet-audio.md), [light](2026-09-18-wolfet-light.md), [client services](2026-09-18-wolfet-client-services.md), [text and files](2026-09-18-wolfet-text-and-files.md), [camera](2026-09-18-wolfet-camera.md) [scene geometry](2026-09-18-wolfet-scene-geometry.md) and [global fog](2026-09-18-wolfet-global-fog.md) records. Six of the open UI ordinals are ones retail ET refuses itself. The navigation imports have no retail content to run against. The cgame and UI surfaces have both been reached from a developer client: the retail UI drew ET's first-run profile dialog through its font, drawing and precompiler imports, and the retail cgame called its trace, per-map fog, decal, corona and skin imports, among others, while it loaded oasis and then all six stock maps on all three renderers; see the [client run](2026-09-18-wolfet-client-run.md) and [map load](2026-09-18-wolfet-map-load.md) records. Neither record lists every import those runs reached, so the cgame and UI counts remain source and unit evidence. |
| Protocol-84 message bodies | Source, Unit, inbound Interop subset | 1,169 original-engine comparisons and inbound retail sessions; full transport and malformed peer behavior remain open. |
| Filesystem / pure / downloads | Unit, Live, inbound Interop subset | Separate etmain/home and native names. Pure admission now works: a retail client passes the proof, reaches CS_ACTIVE and joins both teams on all six stock maps; see the [pure record](2026-09-17-wolfet-pure.md). The official-pak refusal is now profile-owned and covers etmain and mp_bin; see the [download correction](2026-09-17-wolfet-downloads.md), and the www redirect has source and unit evidence; see the [redirect record](2026-09-17-wolfet-web-downloads.md). Broader maps and mods, gamestate-overflow fallback, any transferred download and the reverse direction remain open. |
| Assets / collision | Source, Unit, Live subset | Six stock BSPs and installed TAGs load, and all 21 installed MDM/MDX skeletal files decode through bounded readers, and through the renderer's own byte reader with every count agreeing; see the [model](2026-09-18-wolfet-models.md) and [renderer model](2026-09-18-wolfet-renderer-models.md) records. The shipped assets settle three things ET's own source does not: its surfaces and tags are chained rather than strided, a vertex weight names a global bone rather than the surface's bone references, and every one of the 59,400 shipped skeleton frames writes the greater corner of its box first. Every mesh paired with every skeleton -- 110 pairs, 64,284 poses and 4.1 million skinned vertices -- poses, skins, reduces and places its tags inside the bounds each frame declares; see the [drawing record](2026-09-18-wolfet-drawing.md). No image has been produced, and the remaining render assets stay open. Server sweeps test and report a box entity as its own contents, as `et.exe` and `etded.exe` do, in unit and real-trace fixtures; see the [entity box contents record](2026-09-23-entity-box-contents.md). `et.exe`'s skip of entities whose clip handle is the world is not reproduced. |
| Dedicated gameplay / snapshots | Source, Unit, Live, inbound Interop subset | All six maps initialize under explicit and auto selection; a retail client reaches CS_ACTIVE, joins Axis/Allies, respawns and receives replies to two objective-camera requests. [Callbacks/proxies](2026-09-17-wolfet-snapshots.md) and [viewpoints/server capacity](2026-09-17-wolfet-viewpoints.md) pass executable regressions. A retail client also spawns, moves about 250 units and follows a goldrush-to-battery map change under pure admission; see the [gameplay record](2026-09-17-wolfet-gameplay.md). A thrown engineer's charge now comes to rest on oasis on listen and dedicated servers; the world owns settled dynamite, landmines, satchels, tripmines and smoke, and the previous build dropped at their traces ([explosives record](2026-09-23-world-owned-explosives.md)). The dedicated console echoes broadcast prints without the `[lon]` and `[lof]` tokens, as `et.exe` and `etded.exe` do, while clients still receive them; a live goldrush log matches `etded.exe`'s ([broadcast echo record](2026-09-24-rtcw-et-broadcast-echo.md)). A client's drop is announced with `cpm "%s^7 %s\n"`, as `et.exe` and `etded.exe` do, and not at all when a bot leaves a single-player game type; a retail observer received it from a fnTech3 server. fnTech3's engine `kick` and `clientkick` also shadow the game's own, which drop with `player kicked` and ban the address ([drop notice record](2026-09-24-rtcw-et-drop-notices.md)). A zombie's snapshot carries ET's own empty player state, so a dropped retail client now reads it and runs the notice and `disconnect`, and every retail client reads a quitting server's last message, where both used to fail with `CL_ParsePacketEntities: end of message` ([zombie snapshot record](2026-09-24-zombie-snapshots.md)). `map_restart` runs while a delayed restart is pending, as `et.exe` and `etded.exe` do: a new delay replaces the pending one and its `CS_WARMUP` time, and an immediate restart leaves it to fire later. One rcon-paced sequence on goldrush makes the same restarts and match states on retail `etded.exe` and fnTech3, where the previous build refused every restart once a delay was pending ([pending restart record](2026-09-24-wolfet-pending-map-restart.md)). `map_restart` reloads the level only for a new `sv_maxclients`, as `et.exe` and `etded.exe` do, and a retail client connected across a new game type or `sv_pure` now gets a `map_restart` command rather than a new gamestate; live pure-proof checking is still unlike retail's ([restart record](2026-09-24-wolfenstein-restart-variable-change.md)). An immediate `map_restart` settles in six frames and runs its last frame, each at the current time before advancing it, and leaves bots out of a single-player or co-op restart, as `et.exe` and `etded.exe` do. The retail game's goldrush script timeline for 2.5 s after a restart matches `etded.exe`'s line for line. Retail also enters connecting and loading clients at the restart, where fnTech3 still enters them on their first command ([restart settle record](2026-09-24-wolfet-restart-settle.md)). Broadcast commands now pass over a client whose entity carries `SVF_BOT`, as `et.exe` and `etded.exe` do, so a bot gets only commands addressed to it; on a stock retail dedicated server nothing acknowledges those, and `etded.exe` drops the bot on its 257th unacknowledged command ([bot broadcast record](2026-09-24-wolfet-bot-broadcasts.md)). A bot now rings 256 unacknowledged commands in every slot, as `et.exe` and `etded.exe` do, where a slot no player had used rang Quake III's 64. On a build of the shared tree whose uncommitted bot-library work lets `spawnbot` make one, a fnTech3 ET bot sent a burst of addressed commands held 256 and was dropped on the 257th, where the same build without this change dropped it on the 65th ([bot window record](2026-09-24-bot-reliable-windows.md)). `spawnbot` now makes a bot on a `bot_enable 1` dedicated server, as `etded.exe` does: the bot library reads a character's includes from the game root and sets up Quake III's weapon, item, chat and movement configuration, and the engine registers `bot_thinktime` as 50. On goldrush the bot joins, gets no broadcast and, never thinking without navigation, is dropped once addressed commands fill its window, which fnTech3 still sizes at 64 commands to retail's 256; the previous build's retail game faulted instead ([bot character record](2026-09-24-wolfet-bot-characters.md)). A map load settles in six frames and runs its last frame, each with its bots at the current time before advancing it, and drops the bots of a single-player or co-op game type with no reason, as `et.exe` and `etded.exe` do. The retail game's goldrush script timeline for 2.5 s after a load matches `etded.exe`'s line for line ([map load settle record](2026-09-24-wolfet-map-load-settle.md)). Live landmine/proxy/brush/lean visibility, rendered cameras, dense retail exchanges, movement parity, combat, objectives, a bot's 256-command window and bots with navigation remain open. |
| Client / renderer / audio / input | Developer-build Live subset; release blocked | **The ET client has run** in a developer build: it draws ET's first-run profile dialog with ET's own fonts through the retail UI module, loads oasis through the retail cgame and draws ET's limbo panel, and quits cleanly with the installation unchanged; see the [client run record](2026-09-18-wolfet-client-run.md). That run found and fixed eight defects no unit test could see, among them every cgame trace running along the wrong line and a string leaking out of a skipped preprocessor branch for every profile. Release builds stay blocked and no gameplay is claimed. **All six official maps now load** in the developer client with ET's own configuration on all three renderers, drawing their lightmaps, foliage, decals, coronas and skies and loading their audio zones; that load found and fixed sixteen defects, among them every map drawn without its lightmaps and most of ET's shaders refused; see the [map load record](2026-09-18-wolfet-map-load.md). ET's cgame and UI records, renderer transfers, key numbers, browser numbering and 2D geometry pass regressions, and both dispatchers are wired; see the [cgame](2026-09-18-wolfet-client-boundary.md) and [UI](2026-09-18-wolfet-ui-boundary.md) boundary records. ET's sound surface is carried on both boundaries, with its own volume, loop start time and stop-sounds flag; see the [sound record](2026-09-18-wolfet-audio.md). The OpenAL backend renders ET's voices through the engine's own features: positional, retail's span as the device's distance model, traced occlusion, tone classes, and sends into the six stock maps' audio zones. On `oasis` the loops play positionally with sends into the 512-zone sidecar's hall, where the previous build sent them nothing ([OpenAL voices record](2026-09-24-wolfenstein-openal-voices.md)). Retail's loop Doppler and loop merging are not carried. No listening test has been performed. ET's dynamic light reaches all three renderers with its own radius, brightness and force flag; its global directional and grid-only lights are skipped and say so, and still take one of the 32 lights a frame its modules get, as in ET. Those 32 are the modules' alone: the engine's own scene lights take only slots the modules can no longer claim, and a forced light stays drawn at `r_dynamiclight 0` as it does in ET, shown in live `oasis` clients ([engine scene lights record](2026-09-24-wolfet-engine-lights.md)). ET's binary message channel is wired in both directions, though no payload has crossed. ET's spline camera paths are carried, though a legitimate installation ships none to follow. ET's batched scene geometry, runtime shader definitions and image lookup are carried. The renderer now carries both of ET's skeletal model types, its own surface and its own draw call in all three renderers, including the entity's torso rotation. See the [drawing record](2026-09-18-wolfet-drawing.md). ET's world-covering fog volume now loads instead of refusing the map -- every fog volume retail ET ships is one -- and a module can retune it with a fade; see the [global fog record](2026-09-18-wolfet-global-fog.md). Fog density is not claimed to match retail. ET's render-to-texture import is refused rather than approximated, for a recorded reason; retail ET calls it only from a developer example its shipped build does not define. ET's water ripple and tapered rail core reach the renderer as ET's source draws them, instead of stopping the session with `Bad reType` (Source/Unit; [ripple record](2026-09-23-rtcw-water-ripples.md)). No absent online service is reported as present. The textured console style lays `et.exe`'s `console2-16bit` logo over its `console-16bit` background once the console is half open, in the place `et.exe` draws it, in engine screenshots beside retail's own at 640x480 and at 1280x720; retail's grey separator line and 8x16 text cell are not drawn yet ([console logo record](2026-09-23-wolfenstein-console-logo.md)). |
| Demos / bidirectional interoperability | Inbound Interop subset; recording notice Source, Unit and developer-build Live; demos open | Non-pure retail spectator/team admission connects to fnTech3. Recording keeps the three read-only cvars ET's cgame draws its recording notice from, as `et.exe` does, and the retail cgame draws that notice in a developer client; see the [recording cvars record](2026-09-23-wolfet-demo-recording-cvars.md). The reverse direction and ET demo playback remain open. |
| Optional services | Unavailable | No PunkBuster, authentication or Workshop success is synthesized. |

Build with `-Dwolfet-development=true` only for isolated probes: such a build
runs the ET client and server, and release builds still refuse both.
This option is independent of `-Drtcw-development=true`; neither option closes
any release gate. Q3/QL/RTCW evidence is not ET evidence.

The 2026-09-24 path tracer record (`2026-09-24-rtx-path-tracer.md`) traces
all six retail maps with the RTX path tracer's compute backend, into ET's
main view, which ET insets by one pixel, at 0.93-0.99 of their raster
frames' brightness, with no errors and the installation unchanged. q3map2
left none of their light entities in the BSPs, so the tracer lights their
worlds from the lightmaps and their models from the light grid by default,
and traced pixels take ET's global fog. Five of the six ran in a
large-address-aware copy of the x86 developer client: after the probe's
renderer restarts the shared 32-bit client could run out of address space
reloading ET's 57 MB client game module. The engine now keeps an unloaded
native module's address range reserved until its slot loads again, and all
six pass in the shared client, the module reloading at its preferred base.
Since the record's fifth round the tracer lights ET's worlds from the sky
and the surfaces that give off light by default, not from the lightmaps. It
is renderer evidence only; no ET renderer gate closes on it.

## Elite Force gate matrix

Star Trek Voyager: Elite Force is the fifth title, with two mode profiles:
`stvef-sp` hosts Raven's single-player `efgamex86.dll`/`efuix86.dll`, and
`stvef-mp` runs the Holomatch QVMs. The [foundation record](2026-09-18-stvef-contracts.md)
pins the installation identity and startup contracts.

The [SP clock correction](2026-09-21-stvef-sp-clock.md) repairs missing
camera reference tags and adds 24 production scene reruns, three longer
bridge sequences, save-clock checks and staged manual preflights. The
[extended runtime record](2026-09-21-stvef-runtime-coverage.md) adds full
Holomatch playback on all three renderers and the observed x86 RT limit.

The [SP cadence correction](2026-09-21-stvef-sp-cadence.md) keeps simulation
ticks independent of `timescale` and sends local snapshots after new ticks,
removing the growing fixed-step snapshot lag. Native fixtures, seven-profile
compiled controls, production scene reruns and another retail save round trip
cover the change. Those captures used different frame phases and are not
synchronized retail renderer parity evidence; the later startup/capture
follow-up below aligns displayed times. Full-resolution
comparisons retract an initial contact-sheet impression of a Vulkan/RTX
corridor defect: those corridor images agree with GLx within one channel value.

The [rectangular mipmap correction](2026-09-21-stvef-image-mips.md) fixes
incomplete Vulkan/RTX texture chains and one-dimensional weighted filtering
in all renderers. Exact compiled upload checks cover all seven profiles on
Windows x86 and Linux i686; a visible, independently created texture fixture
reduces the measured Vulkan/RTX difference from GLx from 11.824 to 0.040
channel values. This shared renderer defect is separate from the retracted
corridor assessment and does not close broader retail image parity.

The [SP startup/capture follow-up](2026-09-21-stvef-sp-startup.md) corrects
the four map-settling calls to retail's current-clock ordering, with native
observations, seven-profile compiled controls, all 66 maps in the current
SP inventory and another retail save round trip. An adjusted capture script
aligns displayed snapshot times at three rates. Nine SP image comparisons
then use verified matching filters; their mean RGB differences are 1.15–2.25
out of 255. These bounded observations do not close campaign or image parity.

The [map-reload correction](2026-09-21-stvef-map-reloads.md) removes SP's
accidental multiplayer restart command and restores retail's full-map
continuation. It also fixes a shared Windows stack overflow during Holomatch
client-array resizing. Native observations, seven-profile command checks,
bounded-stack storage tests and Windows/Linux bot-retaining reloads cover
these changes. Campaign and manual acceptance remain open.

The [player-snapshot correction](2026-09-21-stvef-player-snapshots.md) includes
SP's visible local player in packet entities, restoring its third-person
model and shared animation updates used by scripted weapon changes. Native
observations, 168 compiled cases across seven profiles, the full Windows
suite and targeted runtime checks on all three renderers cover the change.
All 66 supplied SP/tour maps complete startup in retail and each baseline
renderer; the corrected GLx client repeats the complete inventory.
The [command-ordinal correction](2026-09-21-stvef-command-ordinals.md) then
resolves the observed startup event/weapon timing difference in 69 queried
snapshots. It replaces timestamp filtering and acknowledgement inference
with retail's command ordinal and bounded recovery contract. Native
fixtures, compiled Windows/Linux boundaries, save/hub and cross-engine
reloads, another complete GLx map sweep, three-renderer scene checks and
Holomatch restart controls cover the change. Campaign traversal and manual
acceptance remain open.

The [texture-compression follow-up](2026-09-21-stvef-texture-compression.md)
connects both retail menus' `r_ext_compress_textures` setting to the renderer,
with native defaults and flags, seven-profile compiled checks and live video
restarts on Windows/Linux. It also finds that the preceding SP captures had
different compression settings. With compression disabled in both engines,
the same tutorial samples have mean RGB differences of 0.074–0.145 out of 255.
GLx also honors the enabled request; Vulkan/RTX retain their uncompressed
capability. The [alpha-format correction](2026-09-21-stvef-alpha-compression.md)
then selects native DXT5 for GLx alpha images, with 72 native observations,
16,128 compiled selections per platform and Windows/Linux restart evidence.
Disabled-compression and Vulkan/RTX control captures remain pixel-identical.
The [method-selection correction](2026-09-21-stvef-compression-method.md)
also honors the native preference and fractional enable values, with 448
native observations, seven-profile Windows/Linux checks and seven complete
restart cycles. The [lightmap-control correction](2026-09-21-stvef-lightmap-controls.md)
honors native compression and precision, including merged/separate uploads
and Vulkan precision fallback, with 648 native observations and Windows/Linux
checks on all three backends. Eleven completed sessions produce 88 verified
engine captures. The full Windows suite passes 120 targets (two skips),
including 1,859 Python checks. Localized image differences, campaign play
and broader input/audio/device acceptance remain open.

The [renderer selection audit](2026-09-21-stvef-renderer-verification.md)
corrects the September 20 live probes: `r_renderer` did not select a
backend, so those runs were GLx only. Source and compiled fixtures remain
valid. New runs select `cl_renderer`, verify the loaded backend in the log,
and retain measured image differences instead of claiming image parity.

| Gate | Evidence | Remaining requirement |
| --- | --- | --- |
| Profile / identity / discovery | Source, Unit, Live subset | Both profiles, their aliases and the shared-pak mode choice pass unit checks; the Holomatch profile mounts the retail paks in a developer dedicated server. Windows x86 SP remains the native-module boundary and unsupported platforms now refuse it before filesystem startup. Linux i686 Holomatch server startup, explicit mode choice and isolation from Q3 assets are observed ([platform record](2026-09-21-stvef-linux-server.md)). The Linux i686 client also starts and completes a Holomatch demo with all three renderer modules on a private Mesa software display ([renderer audit](2026-09-21-stvef-renderer-verification.md)); six Linux client sessions also pass against Windows retail, Lilium and fnTech3 servers ([client record](2026-09-21-stvef-linux-client.md)); broader installations and physical input/device coverage remain open. |
| Audio codecs | Source, Unit, Fixture, Live subset | Every one of the 8,129 retail MP3 files decodes, and every one of the 8,074 Raven `#UNCOMP` sizes is reproduced by the retail frame rule. Single player's ambient sound sets are read and played ([record](2026-09-20-stvef-sp-ambient.md)), and every single-player sound carries retail's own distances, with a voice heard to 1,024 units, an attenuated voice silent by about 290, the sounds the player themselves make held back, and the `s_separation` spread both executables register ([record](2026-09-20-stvef-sp-sound-distances.md)). **Single player's voices are rendered by the engine's own audio pipeline**, not by retail's gain and pan: each channel's full-volume and silent distances go to the OpenAL distance model with its fade and air absorption, and every voice gets traced occlusion, zone or traced room reverb, tone classes by channel (the attenuated voice as a voice) and HRTF. The ambient loops are placed at their own points, the room tone takes the room at the listener, and a separation above the default spreads placed voices through the source's radius. `s_alDistanceModel linear_clamped` plays retail's straight-line law exactly, and the legacy mixer keeps retail's gain and pan ([features](2026-09-20-stvef-sp-sound-distances.md#the-engines-own-features)); no listening test has been made. Single player opens a speaker's mouth as retail does, measuring each voice against Raven's own `#MAXVOL` ([record](2026-09-20-stvef-sp-lipsync.md)). Bink movies play through the installed Windows x86 runtime, with track selection and live lifecycle evidence. Native-file PCM comparisons, both language tracks through the real device, and honest partial sound-initialization failure reporting are now observed ([audio record](2026-09-20-stvef-movie-audio.md), [verified backend reruns](2026-09-21-stvef-renderer-verification.md)); Linux i686 Holomatch also renders a measured tone, half-volume response, both language MP3 paths and sound restart through a private OpenAL Wave Writer ([Linux audio record](2026-09-21-stvef-linux-audio.md)). Broader listening/device parity remains unverified ([record](2026-09-20-stvef-bink.md)). |
| Holomatch wire codec | Source, Unit, Fixture | Protocol-24 bit packing, alignment, strings, entity/player/command deltas, frozen records and the packet scramble. All 32,675 retail demo records decode, and re-encoding reproduces retail's bits except where retail flagged decode-invisible changes ([record](2026-09-18-stvef-holomatch.md)). Protocol 26 now has its own Huffman message layout, checksum framing and unkeyed commands, with independent reference vectors, bounded decoding and live interoperability ([record](2026-09-20-stvef-ioef.md)). |
| Holomatch game module / server | Source, Unit, Live subset | The retail `qagame.qvm` runs with bots on a developer dedicated server: game imports, records, server flags, shield boxes, round-to-nearest float conversion, the 1.17 bot contract and retail's botlib rules for weapons and item choice, Expansion Pack modes, and server-side framing ([record](2026-09-18-stvef-holomatch-server.md)). fnTech3's, Lilium Voyager's and the retail client play on it, and pass its pure admission ([record](2026-09-18-stvef-holomatch-sessions.md)). `sv_maxclients` stays within the module's 64 client slots where retail allows 128. Bots ring 64 reliable commands in every slot, as `stvoyHM.exe` does ([bot window record](2026-09-24-bot-reliable-windows.md)). `stvoyHM.exe` addresses a dropped client `disconnect %s`, unquoted, where fnTech3 sends Quake III's quoted form ([drop notice record](2026-09-24-rtcw-et-drop-notices.md)). A map load and an immediate `map_restart` run each settling frame, and the last one, at the current time before advancing it, as `stvoyHM.exe` does; disassembly and fixtures ([settle order record](2026-09-24-settle-frame-order.md)). The Linux i686 server also hosts all three Windows clients, with protocols 24/26 and pure admission ([record](2026-09-21-stvef-linux-server.md)). |
| Holomatch client framing / demos | Source, Unit, Live subset | Retail handshake with nonce and protocol checks, raw-bit framing with the acknowledgement in snapshots, retail read-past-end semantics, `cp` without a server id, the missing-file download, the one-byte button field, and `.efdemo` naming, refusal and playback selection ([record](2026-09-18-stvef-holomatch-client.md)). All three retail demos play to the end in a developer client ([record](2026-09-18-stvef-holomatch-cgame-ui.md)). Live sessions with fnTech3's, Lilium Voyager's and the retail server, pure and not, and live demo recording that fnTech3 and Lilium play back ([record](2026-09-18-stvef-holomatch-sessions.md)). Protocol-26 negotiation, `.dm_26` recording and playback are also observed; both engines complete each recording with the same frame count ([record](2026-09-20-stvef-ioef.md)). |
| Holomatch cgame / UI | Source, Unit, Live subset | The retail cgame and UI run in a developer client: both import tables with retail's accepted and refused numbers, their records, the quiet cvar set, 3D shader registration, press-only UI key events, both server lists and the absent CD-key service ([record](2026-09-18-stvef-holomatch-cgame-ui.md)). The UI draws the main menu; the cgame plays the retail demos and live sessions ([record](2026-09-18-stvef-holomatch-sessions.md)). The UI's key events and key imports use Elite Force's own key numbers ([record](2026-09-19-stvef-languages.md)). |
| Languages | Source, Unit, Fixture, Live subset | German voices chosen by `s_language`, loaded from `sound/vox_d` at runtime; translated key names chosen by `g_language`, all 7,904 compared equal with both retail executables' tables; `s_language`, `g_language` and `k_language` registered as retail registers them ([record](2026-09-19-stvef-languages.md)). Single-player movies choose the second track for German, falling back to the first when absent; the selected PCM matches native file playback, and both languages reach the real device in verified GLx, Vulkan and RTX runs ([original audio record](2026-09-20-stvef-movie-audio.md), [corrected backend checks](2026-09-21-stvef-renderer-verification.md)). Subjective listening and other devices remain unverified. |
| SP module host / savegames | Source, Unit, Live subset | The game half of the retail `efgamex86.dll` runs on fnTech3's server through its function table: the 42 imports in retail's order, the cgame half attached before `Init`, retail's records through the server's gentity view, and every one of the 63 single-player maps initialised and run; the loopback wire carries every single-player record field exactly, compactly enough for the largest levels' gamestates ([record](2026-09-19-stvef-sp-host.md)). Savegames are retail's chunks with the engine's own around the game's: levels save and load, walk and fall back between each other, and keep their autosaves and pictures; the retail engine loads a save fnTech3 wrote and fnTech3 loads one retail wrote ([record](2026-09-19-stvef-sp-savegames.md)). New levels now start at retail's 1,000 ms; a zero start had prevented delayed reference tags from registering and broke scripted cameras. Production reruns and fresh save/hub-clock checks cover the correction ([record](2026-09-21-stvef-sp-clock.md)). Checked writes and final close now preserve previous saves on failure; real file obstruction/recovery probes verify honest completion messages and hub-transition refusal, with a fresh retail save round trip ([save-integrity record](2026-09-21-stvef-save-integrity.md)). `stvoy.exe` announces a drop as `print "%s %s\n"`, after the game's `ClientDisconnect`, and sends a bare `disconnect`; fnTech3 sends Quake III's forms ([drop notice record](2026-09-24-rtcw-et-drop-notices.md)). A map load runs each settling frame, and the last one, at the current time before advancing it, as `stvoy.exe` does ([settle order record](2026-09-24-settle-frame-order.md)). Retail has no `map_restart`, and fnTech3 registers none for single player either: its `nextmap` reloads the level in full, as retail's does ([map reload record](2026-09-21-stvef-map-reloads.md)). |
| SP menus / cgame | Source, Unit, Live subset | The retail `efuix86.dll` runs in a developer client through its function tables: the 59 imports in retail's order with the scissor slot left empty, menus opened by name with their holodeck variants, retail's check whether a menu may open, press-only keys, the connection screen's text and retail's order of drawing and refreshing; the main menu draws as retail's ([record](2026-09-19-stvef-sp-menus.md)). The cgame half of `efgamex86.dll` runs in the client through the game library the server loads: all 71 imports, single player's records, the calls the game half makes while a level spawns, the cgame shut down before the library unloads, the command number it predicts from, and snapshots seen from a camera scene or the leaning eye; the retail levels play, with their scenes, subtitles, HUD and view weapon ([record](2026-09-19-stvef-sp-cgame.md)). The savegame commands and the picture a save keeps are in the [savegame record](2026-09-19-stvef-sp-savegames.md), and the ambient sound sets, which give a room its tone and a mover its sounds, in the [ambient record](2026-09-20-stvef-sp-ambient.md). How far each of single player's sounds carries is in the [sound distance record](2026-09-20-stvef-sp-sound-distances.md). Lip-sync is measured from the playing voices ([record](2026-09-20-stvef-sp-lipsync.md)). The visible local player now reaches the packet-entity list, restoring third-person rendering and shared weapon animation; the subsequently observed startup event/weapon timing difference is resolved in 69 queried snapshots by explicit command ordinals and native recovery behavior ([player-snapshot record](2026-09-21-stvef-player-snapshots.md), [command-ordinal record](2026-09-21-stvef-command-ordinals.md)). The images the menus and cgame hand the engine now carry the brightness retail applies at its `glColor3f` boundary, and each image submitted in a client frame keeps its own pixels through the queued 2D draw ([raw-image record](2026-09-21-stvef-raw-image.md)). Both profiles carry HUD aspect presets ([widescreen record](2026-09-25-widescreen-hud-and-fov.md)), checked against the draws their retail modules issue, and every official map has a generated audio zone sidecar ([presentation-asset record](2026-09-22-stvef-presentation-assets.md)). |
| Renderer / cinematics | Source, Unit, Live subset | Holomatch's entity record and eight effect primitives, lighting flags, entity alpha forcing, environment mapping, the `random` wave and `depthFunc disable`, overbright default and console font are implemented in all three renderers; demo frames were compared with Lilium and retail ([record](2026-09-18-stvef-holomatch-cgame-ui.md)). Both modes' `noise` waves, and SP entity records, held frames, rotated pictures and lighting queries have evidence ([record](2026-09-19-stvef-sp-cgame.md)). All 201 installed RDM5 models pass the bounded reader; 2,081,643 bones decode bit-identically to retail, and skinning, LOD and lighting have native comparisons and executable regressions in all three implementations ([MDR record](2026-09-20-stvef-mdr.md)). Native vectors and compiled fixtures also cover scissor command ordering, movie fade/crop calculations, all eight effect primitives and full-length legacy shadow extrusion ([scissor](2026-09-20-stvef-scissor.md), [presentation](2026-09-20-stvef-cinematic-presentation.md), [effects](2026-09-20-stvef-effects.md), [shadows](2026-09-20-stvef-shadows.md)). The September 20 live runs used GLx regardless of their requested labels. Twenty-one correctly selected Windows reruns now verify scissor pixels on all three backends and Vulkan/RTX shadow scenes, effects, movie fades, stable held frames, level pause/resume, both audio tracks and continued video after sound failure. Three Linux i686 software-rendered demo runs complete with 107 frames each ([renderer audit](2026-09-21-stvef-renderer-verification.md)). The RTX default linear-light HDR path differs from legacy images; two SDR control runs now reproduce Vulkan held movies exactly and its shadow scenes within one channel value. Eight broader SP scenes now complete on each verified backend after the camera-tag fix, and all three effective retail Holomatch demos complete on each backend with retail's frame counts. These scene captures are not synchronized retail pixel comparisons. A 66-map startup sweep adds retail and all three baseline renderers, followed by all 66 maps on corrected GLx and targeted first/third-person checks on all three corrected renderers; it covers initial scenes, with an HD overlay, and does not establish campaign traversal or universal pixel parity ([player-snapshot record](2026-09-21-stvef-player-snapshots.md)). The tested x86 driver lacks required RT extensions: raster fallback and strict refusal are observed, while native RT remains unavailable ([clock/scene record](2026-09-21-stvef-sp-clock.md), [full demos and RT boundary](2026-09-21-stvef-runtime-coverage.md)). Broader image parity, campaign play and listening/device coverage remain open. |
| Interoperability | Live, Interop | fnTech3 plays with Lilium Voyager 1.40 and the retail `stvoyHM.exe` 1.20 as client and as server, with and without pure admission; demos recorded live play in fnTech3 and Lilium; and a missing pak downloads byte-identical between every pairing, pure or not ([record](2026-09-18-stvef-holomatch-sessions.md)). Protocol 26 passes both fnTech3/Lilium directions with pure enabled and disabled, matching downloads, cross-engine demos and valid packet checksums; protocol 24 remains independently exercised ([record](2026-09-20-stvef-ioef.md)). Ten Linux-hosted sessions and twelve cross-engine demo playbacks add dedicated-server platform evidence, including the retail client ([server record](2026-09-21-stvef-linux-server.md)). Six Linux client sessions against Windows retail/Lilium/fnTech3 servers and twelve further cross-engine demo playbacks establish the reverse platform direction ([client record](2026-09-21-stvef-linux-client.md)). |
| Input / physical devices | Source, Unit, Live disabled-path subset; manual acceptance pending | Elite Force key numbers, translated names and press-only UI events have the recorded language/input checks. The staged SP and Holomatch installations pass input-disabled preflight, but physical movement/look/firing, menu rebinding/save/load and listening await user results. SP force-feedback imports now use the installation's FFC10 runtime through an opt-in Windows x86 bridge: all 32 effects, four-slot playback rules and spring controls have native parameter and compiled boundary checks. Six production starts verify that disabled feedback does not load the DLL. Actual feedback device initialization/output remains untested ([feedback record](2026-09-21-stvef-force-feedback.md), [manual acceptance](2026-09-21-stvef-sp-clock.md)). |
| Optional services | Unavailable | No authorize/master success is synthesized. |

The [shader-wave follow-up](2026-09-21-stvef-shader-waves.md) corrects the
random wave's view clock and both profiles' deterministic noise tables, with
native SP/MP vectors, compiled checks on both platforms and production runs
on all three renderers. Two held Holomatch views gain bounded retail image
comparisons. Its later shield and explosion captures varied within retail
itself; the following investigation separates this capture-timing limit
from renderer behavior.

The [effect-capture follow-up](2026-09-21-stvef-effect-captures.md) resolves
the large portable-shield comparison mismatch: frozen cgame frames kept
adding shield quads, retail and fnTech3 consumed different wait counts, and
their screenshot commands captured at different points in the frame.
Matching those points gives 54 full-frame comparisons across nine states
in all three retail demos on every renderer, with RGB mean absolute
differences of 0.39–2.20 out of 255. Both successive captures are retained.
This establishes bounded image evidence, not pixel identity or completion
of the broader renderer gate.

The [electricity investigation](2026-09-21-stvef-electricity.md) separates
randomized ribbons from the largest remaining burst-image difference.
Instrumented retail and fnTech3 copies with the same jitter input reduce
that scene's difference to 0.65–0.67 on all three renderers; sixteen control
images remain pixel-identical to their uninstrumented runs. Native counts
confirm 45 random values per ribbon and are now checked in all renderers on
Windows x86 and Linux i686. This is separate diagnostic evidence, without
changing production randomness or closing the broader renderer gate.

The [runtime follow-up](2026-09-21-stvef-runtime-followup.md) adds a complete
fnTech3-to-retail-to-fnTech3 SP save round trip after a longer retail startup
wait, plus a 107-frame Linux Holomatch run with the ray-tracing pipeline
active on software Vulkan. These are bounded fixture/platform observations;
manual menu saves, campaign-wide acceptance and Windows x86 RT remain open.

Build with `-Dstvef-development=true` only for isolated probes. The option is
independent of the RTCW and ET options; none of them closes any release gate.

## Jedi Academy gate matrix

Star Wars Jedi Knight: Jedi Academy is the sixth title, with two mode profiles
over one `base/` installation: `jka-sp` for Raven's single-player campaign and
`jka-mp` for multiplayer. The [foundation record](2026-09-21-jka-contracts.md)
pins the installation identity and startup contracts against a legitimate
retail 1.01 installation, whose paks, modules and assets have been read.
The retail multiplayer game module now runs a retail level inside the engine
**refusing nothing**: it initializes, spawns `mp/ffa1`, and holds it open
through its frame loop with no unserved import at all. No client connects, no
snapshot is built, no packet is encoded, no frame is drawn, and no demo or
session exists for this title.

| Gate | Evidence | Remaining requirement |
| --- | --- | --- |
| Profile / identity / discovery | Source, Unit, Live subset | Both profiles, their aliases, limits and the shared-installation mode choice pass unit checks, and a legitimate 1.01 installation is recognized from `base/assets2.pk3`'s pinned identity, reports that a mode must be chosen, and resolves single player's module in its root while refusing to find multiplayer's there. A developer dedicated server starts under both profiles against that installation, writes each mode's own configuration into an isolated home path, and loads a retail map's collision in each mode before refusing the module boundary ([record](2026-09-21-jka-runtime.md)). Other editions, a Steam installation and its AppID and folder, and every platform but this one remain open. |
| Module ABI | Source, Unit, Live subset | Multiplayer's 796 module numbers are frozen, unique, reversible and refused outside their own profile, with Raven's gaps refused explicitly, and the installed modules' export tables confirm both shapes: `GetGameAPI`/`dllEntry`/`vmMain` for single player's root module, `dllEntry`/`vmMain` for the three multiplayer modules that ship inside the paks ([record](2026-09-21-jka-module-abi.md)). **The retail engine's own dispatcher confirms the game half of that table exactly**: `jamp.exe` bounds the import number at 634 and jumps through a 635-entry table, and the 329 slots that do not fall to `Bad game system trap` are the *same set* as the 329 game imports fnTech3 names -- no extras on either side, from a table built by an unrelated method. **The retail module can only ever ask for 150 of those 329**: every import it can reach has a call wrapper in its own code, and counting them bounds the whole of this title's boundary rather than estimating it ([record](2026-09-22-jka-trace.md)). Ten records are frozen to the byte -- entity state 532, player state 1,552, shared entity 864, and the 48-byte trace -- with all 376 members' offsets and sizes asserted against a measured table ([record](2026-09-21-jka-records.md)). Ninety-eight of multiplayer's imports are served, and Quake III's fourteen shared arithmetic and memory calls are handed through unchanged; linking and unlinking are served by name and never handed through, because Quake III's 34 and 35 are the bot client calls. Academy's entity state converts both ways, field by field: 29 of its fields are Quake III's and 65 are its own, and a record with a distinct value in every one of its 532 bytes returns byte for byte, so a field left unmapped fails the test rather than reading as zero. Two renumberings were measured rather than inherited: **this title's sine and gravity trajectories are 5 and 6, not Quake III's 4 and 5**, and **its trace is 48 bytes with the two solid flags and the entity number packed into the first four**, so its fraction sits where Quake III keeps a flag. Either taken on resemblance would have been silently wrong in every trace and every falling object ([record](2026-09-22-jka-trace.md)). Single player's `GetGameAPI` table and records are unwritten.
| Wire codec / demos | None | Protocol 26 and single player's loopback 40 are registered numbers. No message body, delta table, netchan behavior or demo format exists. Multiplayer's delta tables are 132 entity and 152 player fields, with separate pilot and vehicle player tables, and a mod may retune any field's width at runtime through `netf_overrides.txt` and `psf_overrides.txt`, which the installation itself ships in `assets1.pk3`, so the codec has to carry that mechanism rather than a fixed table. |
| Filesystem / pure / downloads | Source, Unit, Live subset | A developer dedicated server mounts the installation's four paks in their own order -- `assets3` over `assets2` over `assets1` over `assets0`, 23,744 files -- and executes Raven's own `default.cfg` out of them ([record](2026-09-21-jka-runtime.md)). Raven's `assets0`-`assets8` naming is refused for download in `base` and `missionpack`, and a referenced pak is marked for the native `cgamex86.dll`/`uix86.dll` pair as well as Quake's QVM names ([record](2026-09-21-jka-maps.md)), which this title needs because its multiplayer modules are themselves pak members. Pure admission against a real server, mod directories and downloads themselves remain open. |
| Assets / collision | Source, Unit, Live subset | Every one of the installation's 342 maps, models and motion paths reads: all 57 maps carry `RBSP` version 1 and translate for collision, 376,744 surfaces in all; all 127 Ghoul2 meshes and 66 skeletons read, 529,664 vertices and 131,782 frames, with each mesh checked against the bone count of the skeleton it names; and all 92 ROFF motion paths read with their note tracks ([map record](2026-09-21-jka-maps.md), [model record](2026-09-21-jka-models.md), [motion record](2026-09-21-jka-roff.md)). Every one of the 1,011 compiled ICARUS scripts the engine would open reads through a bounded container reader as well, 25,987 blocks and 51,567 members with no refusals, which matters early because the game module asks for ICARUS inside `GAME_INIT`. A second reader groups each block's members into its values under six rules that cover all 26,512 blocks, refusing anything they do not, and reads the same 1,011 scripts into 25,987 statements and 39,854 values with no refusals. The rest of what the corpus fixes about itself is recorded rather than built in: five block numbers open a body that one closes and all 1,011 scripts nest exactly, and the names divide into a map's, a single script's and a level's. No block number is named, nothing is interpreted, and every ICARUS import is still refused ([script record](2026-09-21-jka-icarus.md)). The map translation deliberately drops the lightmap styles, the other three lightmap sets and the light array, so it is collision evidence only; no map is drawn, no model is posed or skinned and nothing moves along a path. No script block is interpreted, no ICARUS import is served, and Raven's shader extensions are unimplemented. |
| Server / gameplay | Source, Unit, Live subset | A developer dedicated server runs the retail multiplayer module through `GAME_INIT`, **spawns a retail multiplayer level and holds it open through its frame loop with no import refused at all** ([record](2026-09-21-jka-runtime.md)). The engine's checked game-data view carries Raven's 864-byte shared entity and 1,552-byte player state at their measured offsets and converts its entity state both ways, so linking, unlinking, brush models, point contents, both visibility tests, area portals, area connection, the entities in a box, entity contact and the world trace are all answered from the level's own collision. The navigation graph Raven's bots walk on is held by the engine and filled by the game -- nodes, edges, shortest paths, failed nodes and blocked edges -- because **this title's bots never call one of Quake III's area-awareness imports and the installation ships no `.aas` file**. Recorded motion paths are cached and forgotten on request; starting one is refused by name, because how this engine should move an entity along a path is not measured and a guess would move every scripted lift and camera wrongly. The last word the module says is the engine's own honesty coming back: it reports being unable to save navigation data, because `G_NAV_SAVE` will not write into an installation it was asked to read, and the checksum it prints is byte for byte the one the retail `.nav` file carries. **A bot now enters the game with no import refused anywhere in the path**: `addbot` gets a client slot, writes its userinfo, allocates its goal, move and weapon states, builds its body -- an instance duplicated for it, attached to its entity number, its skeleton read back, its bolts registered by name, its bones set playing and bent -- presses its keys through seventeen elementary actions, and has its input frame converted both ways; the module then broadcasts its arrival and calls `ClientBegin`. Both bone argument lists were read out of the retail engine's own handlers rather than taken from resemblance, and the input record was measured from where the module reads its action flags. Two gaps are stated rather than hidden: the engine holds what a bone was set playing and does not work out which frame it is on, and the two elementary actions Raven inserted into Quake III's block have no counterpart in the library this engine carries, so a bot here moves, turns, jumps, crouches, fires and changes weapon but does not alt-fire or use a force power. **`G_G2_CLEANENTATTACHMENTS` now really clears what the engine holds** rather than doing nothing, which was honest only while nothing could make an attachment. **With the player state written, the bot connects, enters and plays**: a server left running showed it moving about the map and taking items -- though only while every lightsaber was quietly failing to load, which kept the game off the branch that asks where a bolt on a saber is. Repairing that loader moved the boundary earlier, which is the right trade: the bot now connects, enters and begins, and the game is refused a bolt matrix as soon as it has a saber. That record is 168 fields -- 43 Quake III's, 124 Raven's, and nineteen ammunition slots where the canonical record holds sixteen -- generated from the measured table rather than hand-written, and covered by the same byte-for-byte round trip as the entity state. The Ghoul2 store also stopped looking for a skeleton that was never a file: `*default` is a name, not a path, and is the only such name in either installation, on 41 of Academy's 127 meshes and 23 of Outcast's 71. **`G_G2_GETBOLT` is served, and the bot plays with no import refused anywhere.** The whole Ghoul2 pose was measured, first out of the shipped skeletons of both titles and then out of the retail engine itself: a fourteen-byte pose of seven words, a rotation read as `word / 16383 - 2` and a displacement as `word / 64 - 512` -- the two halves disagree about where zero is, which is Raven's and not a misreading -- composed as that transform applied to the bone's own rest matrix with no walk up the hierarchy, which holds Academy's humanoid to a median bone-length error of 0.034 units where every other candidate rule is wrong by 99% or more. A bone's frame comes from `startFrame + ((now - startTime) / 50) * animSpeed`, so a Ghoul2 frame is fifty milliseconds, and two frames blend element by element across the matrix. The bolts a game asks for are not bones: the sabre hangs off `*r_hand`, surface 60 of the mesh, a tag whose three corners are carried by the bones they are weighted to and skinned by first undoing where each bone rests -- a step that is invisible by eye and, left out, put the right hand's tag 26.7 units from the hand; with it, 0.644 units from `rhang_tag_bone` ([record](2026-09-22-jka-pose.md)). Twenty seconds of `mp/ffa1` has the bot connect, be announced, begin and take things with a sabre loaded and the bolt branch live. A bolt this engine cannot answer writes nothing and reports failure rather than handing back an identity matrix that would put the thing somewhere definite and wrong. A bot rings 128 reliable commands, as `jamp.exe` does, where it rang Quake III's 64 ([bot window record](2026-09-24-bot-reliable-windows.md)). No network client connects, no snapshot is built and no gameplay is claimed. Jedi Academy numbers its game types differently from Quake III, and the inherited `spmap` command would select the wrong one; retail multiplayer has no such command. A map load and an immediate `map_restart` run each settling frame, and the last one, at the current time before advancing it, as `jamp.exe` does, and so would a single-player map load, as `jasp.exe`'s does; disassembly and fixtures ([settle order record](2026-09-24-settle-frame-order.md)).
| Client / renderer / audio / input | None | Nothing is wired. The renderer needs a reader that keeps RBSP's lightmap styles and light array rather than the collision translation. Single player's menus live in the engine rather than in a module, which is a separate contract from every other profile. |
| Savegames | None | Single player's save format is unimplemented. |
| Optional services | Unavailable | No service success is synthesized. |

## Jedi Outcast gate matrix

Star Wars Jedi Knight II: Jedi Outcast is the seventh title, with two mode
profiles over one `base/` installation: `jk2-sp` and `jk2-mp`. Every contract
below is pinned against a legitimate retail 1.02a installation and its own two
engine binaries ([foundation record](2026-09-22-jk2-contracts.md), [records
record](2026-09-22-jk2-records.md)). The retail multiplayer module now runs a
retail level **refusing nothing**, holding `ffa_bespin` open through its frame
loop; nothing is drawn, no packet is encoded and no client exists.

| Gate | Evidence | Remaining requirement |
| --- | --- | --- |
| Profile / identity / discovery | Source, Unit, Live subset | Both profiles carry protocol numbers, configuration names, limits and module locations read from the retail binaries that enforce them: single player's loopback 40 and multiplayer's 15 from their own cvar registrations, 1,024 and 1,400 configstrings and 512 and 256 submodels from their own bounds checks, and 32 clients from multiplayer's own clamp. A legitimate 1.02a installation is recognized from `base/assets0.pk3`, which holds both configurations. Other editions, a Steam installation and every platform but this one remain open. |
| Module ABI | Source, Unit, Live subset | Retail `jk2mp.exe` dispatches imports 0 to 599 with 221 handled, through a byte index table at `0x44cc50` and a jump table at `0x44c8d8`, in runs at 0, 100, 200, 300, 400 and 500. The first two runs are Quake III's own numbers, established by what each handler does rather than by resemblance. **The retail module can only ever ask for 97 of the 221**: a QVM reaches the engine by calling a negative address, so counting the negative calls in `vm/jk2mpgame.qvm` bounds this title's whole boundary ([record](2026-09-22-jk2-records.md)). Its entity state is now **frozen whole**: `jk2mp.exe`'s own delta table names every four-byte slot from 4 to 292 with no gap, and gives the width each is sent at, which types every member -- a member sent at zero bits is a float. `number` at 0 completes it, and the record ends at 296, the size already measured from the engine's brush-model writer by an unrelated means. `ping` is at 484, read out of the one place the engine writes it. Forty-four imports are served: Quake III's twelve arithmetic and memory calls, the thirty of the first run that carry no record of this title's, the array publisher, and the Ghoul2 model initializer at 588, whose handler was read on its own rather than taken from Academy's names -- **this title's Ghoul2 run is measurably not Academy's**, while its elementary-action and AI-state blocks agree with Academy's on every argument count. Single player's `GetGameAPI` table is unwritten.
| Wire codec / demos | Source | Protocol 15 and single player's loopback 40 are registered numbers, and no message body or demo format exists; its demos are `.dm_15` by the engine's own naming. What is measured is the delta tables themselves: 73 entity fields and 105 player fields with their offsets and bit widths, read out of `jk2mp.exe` at `0xf5ab0` and `0xf5e20` ([record](2026-09-22-jk2-records.md)). Nothing encodes or decodes a packet with them yet.
| Filesystem / pure | Source, Live subset | A developer server mounts the installation's two paks, 14,685 files, and starts a retail map. Pure admission, mod directories and downloads remain open. |
| Assets / collision | Source, Unit, Live subset | All 1,557 of the installation's maps, models, skeletons, motion paths and scripts read through fnTech3's readers with no refusals: 41 `RBSP` version 1 maps, 71 Ghoul2 meshes, 21 skeletons, 68 ROFF paths and 1,356 ICARUS scripts, every format shared with Jedi Academy. Nothing is drawn, posed or moved. |
| Server / gameplay | Source, Unit, Live subset | **Two bots play a retail multiplayer match with nothing refused**: ninety seconds of `ffa_bespin` with two bots in it, both clients connecting, changing userinfo, entering, beginning and collecting seventeen items between them, with no unserved import and no error. The retail QVM loads out of the pak, initializes, publishes its arrays into the engine's checked view at its measured strides, registers its items and parses its 29 bots and 15 arenas. Its entity state, input frame and player state all convert through its own adapters, which reach none of Academy's records. Every number was read from its own engine's dispatcher rather than taken from Academy's table, and four things show why that mattered: **its world block is Quake III's numbering, which Academy's is not; its trace is Quake III's 56-byte record, which Academy's 48-byte one is not; its motion paths sit five numbers lower than Academy's; and its Ghoul2 run is different again** -- each established from the retail engine's own handlers, error strings and debug names. Its player state's four arrays were named by the engine as it walks them, with powerups read as long words because they are times. **688 of that record's 1,368 bytes have no name in any table read so far**: they are declared as unnamed spans and deliberately cannot be written, because a write would put zeroes over half a record it cannot name. A trace whose pass entity is -1 is read as having none, which is the only defined reading; retail indexes its entity array at -1 instead. Its bots are declared from evidence too: the installation ships Raven's own `botfiles/*.jkb` and `bots.txt`, none of Quake III's AI configuration sources and no `.aas` file for any of its 41 maps. Starting a motion path stays refused for the same reason it does in Academy. A bot rings 128 reliable commands, as `jk2mp.exe` does, where it rang Quake III's 64 ([bot window record](2026-09-24-bot-reliable-windows.md)). No network client connects and no snapshot is encoded. A map load and an immediate `map_restart` run each settling frame, and the last one, at the current time before advancing it, as `jk2mp.exe` does, and so would a single-player map load, as `jk2sp.exe`'s does; disassembly and fixtures ([settle order record](2026-09-24-settle-frame-order.md)).
| Client / renderer / audio / input | None | Nothing is wired. |
| Savegames | None | Single player's `.sav` format is unimplemented. |
| Optional services | Unavailable | No service success is synthesized. |

Build with `-Djk2-development=true` only for isolated probes; an ordinary build
refuses both profiles at detection. The option is independent of every other
title's.

Build with `-Djka-development=true` only for isolated probes. An ordinary
build refuses both profiles at detection and writes nothing. The option is
independent of the RTCW, ET and Elite Force options; none of them closes any
release gate, and Q3/QL/RTCW/ET/Elite Force evidence is not Jedi Academy
evidence.


## Alice gate matrix

American McGee's Alice is the eighth title and the thirteenth profile,
`alice`, over one `base/` installation. It is the only supported title outside
the Quake III line: Alice runs Ritual Entertainment's Heavy Metal: F.A.K.K.2
engine, so almost none of the shared contracts apply to it unchanged. The
[foundation record](2026-09-22-alice-contracts.md) pins the installation
identity and the startup constants against a legitimate retail November 2000
installation. Every contract below was read out of the retail `alice.exe`,
`fgamex86.dll` or `cgamex86.dll` and is cited by address in the record linked
beside it.

**Observed.** In developer builds (`-Dalice-development=true`, Windows x86)
the retail game module and client game run together in one client. `garden1`
loads, plays its opening scene and then draws Alice in third person with her
HUD's gauges and foldouts; a save made in play loads back and play resumes;
the intro's logos lead to the engine's own menus, and New Game's command plays
`opening.roq` and then loads `gvillage`; demos record and play back; and the
GL, Vulkan and RTX renderers draw `garden1` alike. Every client run was
windowed at 800x600 with an isolated home path and no mouse or keyboard
input, its frames
were taken with the engine's own screenshot command, and the installation was
left unchanged: the probes' manifests record it, and on 2026-09-24 no file or
directory in the installation had been modified this month. The rows below
say where each was observed.

**Not established.** No retail peer has taken part in any of it. No session
has run between retail `alice.exe` and fnTech3 in either direction, no
retail-written save or retail-recorded demo has been read, and no fnTech3
frame has been set beside one retail drew. Alice ships no multiplayer, so the
only channel exercised is loopback. Every gate below therefore stays partial,
and the release gate stays closed.

| Gate | Evidence | Remaining requirement |
| --- | --- | --- |
| Profile / identity / discovery | PARTIAL -- Source, Unit, Live subset | The profile, its three aliases, its limits and its single-mode title pass unit checks, and `base/pak0.pk3`'s pinned identity -- `default.cfg` at 1,322 bytes with CRC `3781148b` and `maps/garden1.bsp` at 6,323,368 bytes with CRC `cb85fdae` -- distinguishes a retail installation from every other supported title's. Protocol 1 is admitted by this profile and refused by all twelve others. Developer dedicated servers and clients start under the profile against a legitimate retail installation, write only into an isolated home path and shut down cleanly ([foundation record](2026-09-22-alice-contracts.md), [map record](2026-09-22-alice-maps.md)). `sv_maxclients` defaults to 1, which is what retail's `SV_Init` registers (`"1"`, serverinfo and latched, `0x0045566b..0x0045567c`); the 8 read earlier at `0x00454e0c` is inferred to apply only to a cvar not yet registered (`code/qcommon/game_profile.cpp`, and the correction in the [foundation record](2026-09-22-alice-contracts.md)). Other editions, the Mac release, the later digital re-releases and every platform but Windows x86 remain open. |
| Module ABI | PARTIAL -- Source, Unit, Live subset | The four table lengths are measured in the retail libraries and frozen -- 111 game imports, 26 game exports with API version 4 and a 732-byte entity record, 125 client-game imports and 7 client-game exports -- and every slot of all four tables is mapped from the engine's and the modules' own code: the game's in `code/compat/alice_game_imports.def` and `alice_game_exports.def`, the client game's in `alice_cgame_imports.def` and `alice_cgame_exports.def`, held to each other and to the engine's own table builds by `tests/alice_module_abi_tests.cpp` ([module record](2026-09-22-alice-module-abi.md), [client-game record](2026-09-23-alice-cgame.md)). Both libraries load through the function-table path (`VM_CreateTable`), and the engine checks what each publishes before calling it. **110 of the 111 game imports are served**; the other, slot 100, is the one retail's own table build leaves null and the module never calls (`code/server/server_alice_game.hpp`). **119 of the 125 client-game imports are served** (`code/client/client_alice_cgame.hpp`); the six that are not -- 0, 48, 49, 73, 88 and 116 -- are all slots the module never calls, and 0, 88 and 116 are the three retail's builder never writes. Both counts are printed at load ([client-game record](2026-09-23-alice-cgame.md)). Every unserved slot holds a thunk that drops by name, so no null is ever handed over. The client game's exports are called where retail calls them, its initializer with the import table, the server message sequence and the last executed server command. An argument count measured at a call site is not a signature, and no slot's parameter types are frozen. `CalcCRC` (game slot 99) is served and unit-tested but has not been reached at run time. |
| Wire codec / demos | PARTIAL -- Source, Unit, Live subset | **The netchan is retail's** ([wire record](2026-09-22-alice-wire.md)): Quake III's `netchan_t` and fragmenting at 1,400-byte packets and 1,300-byte fragments, but with each fragment's offset a long, so messages can pass 32 KB (`0x0044710e`, `0x00447423`), a one-sided qport, no sequence checksum or reliable-command XOR, the packet scramble Elite Force Holomatch uses step for step, and no Huffman stage. Its limits are Alice's own: 48 KB messages, 128 reliable commands of 1,024 bytes, 32 packet backups, 64 command backups and 32 usercmds of 20 bytes. There are ten server opcodes, no `svc_EOF`, no download path and no pure exchange. **The message bodies are encoded and decoded in Ritual's shape**: 79 entity and 49 player fields walked out of the engine's own delta tables and frozen by a generator, a thirty-one entry dictionary of change vectors, a masked 32-byte entity block, six 32-entry player arrays read as signed shorts ([HUD record](2026-09-24-alice-hud.md)), Ritual's float rule and an unkeyed user command, all held to their measured costs by `tests/alice_message_codec_tests.cpp`; every baseline deltas from Alice's own null entity (`0x00446460`). **The snapshot walk is retail's** (`code/server/server_alice_snapshot.hpp`, [snapshot](2026-09-22-alice-wire.md#the-snapshot)): the client's own entity first, a child judged by its attachment or bind parent, a portal's second walk, a sky origin's walk after the main view, the far-plane cull, and the portal, sky and dual flags rewritten on each add, with a frame for each client. On `garden1` the client game's `Could not find parent entity` warnings go from 2,998, with a temporary switch back to the Quake III walk, to none in the same binary, and from 3,604 in an earlier build to none in a later one. **Demos are recorded and played as `alice.exe` does** ([demos](2026-09-22-alice-wire.md#demos)): Quake III's container under `demos/<name>.dm3`, `record` refused without `g_syncronousClients`, and no recording notice; a 158-record `garden1` recording replays in a separate windowed client, and `timedemo 1` plays it in 198 frames. **A snapshot carries the server's time residual** after its time, which `alice.exe` writes (`0x00456c63..0x00456c7b`) and reads (`0x0042dc6e`). fnTech3 had sent and read neither, consistent only with itself. Retail F.A.K.K.2's reader, of the same shape, exposed the gap. Both sides now carry it ([wire record](2026-09-22-alice-wire.md#the-time-residual)). **The connect is plain text**, protocol 1, answered by a bare `connectResponse`, as Ritual's engine has it (`alice.exe` has no Huffman stage; `fakk2.exe` 1.02 shows the whole exchange). fnTech3's server had Huffman-decoded it and its client had asked for protocol 68. Both now follow retail under the two Ritual profiles, and a local `garden1` game still starts. A client that leaves is announced with `print "%s %s\n"` and sent a bare `disconnect`, as `alice.exe` does (`0x004529fb`, `0x00452a6b`), where fnTech3 had sent Quake III's forms ([drop notice record](2026-09-24-rtcw-et-drop-notices.md)); a client on the Ritual wire rings the profile's reliable window, which is Alice's 128. No packet has been exchanged with any peer but fnTech3's own loopback server, no demo has crossed between fnTech3 and `alice.exe` in either direction, and no retail-recorded demo is available to play. |
| Filesystem / pure / downloads | PARTIAL -- Source, Live subset | Developer servers and clients mount all ten of the installation's paks, 14,818 files, and execute Ritual's own `default.cfg` out of them. Retail sorts `base/*.pk3` with a case-sensitive `strcmp` (`0x0043ea00`) and fnTech3 case-insensitively; on a retail installation both orders mount the same files, because no file in `Pak2` or `Pak3` is in another pak and the paks that do overlap sort alike under both rules ([client-game record](2026-09-23-alice-cgame.md#pak-order)). A `.tga` name is looked for as `.ftx` first, as retail's image loader does (`0x00483e80`). Saves are written into `fs_homepath`, where retail writes into its installation ([savegame record](2026-09-23-alice-savegames.md)). Alice registers `fs_basepath`, `fs_cdpath`, `fs_filedir` and `mapdir` and no `fs_homepath`. The shared download refusal on Quake's `pak` stem covers `pak0`, `Pak2` and `Pak3` and misses the six suffixed names, which need a rule of their own. `sv_pure` exists as a string with no registration found, so no pure proof is claimed. |
| Assets / collision | PARTIAL -- Source, Unit, Live subset | All thirty-six maps are ident `FAKK` version 42 and translate for collision -- 3,969 shaders, 342,548 planes, 62,793 brushes and 183,673 surfaces of which 6,666 are patches -- with eleven of the thirteen fixed-size records Quake III's byte for byte ([map record](2026-09-22-alice-maps.md)). The renderers' world load carries the light grid at Alice's 192x192x320 cell, with which Quake III's derivation reproduces the point count exactly on all thirty-six maps ([client-game record](2026-09-23-alice-cgame.md)); the renderers' translation still drops the entity lights, their visibility and the light definitions (`code/qcommon/bsp_fakk.h`). The entity lights are read offline instead: their 60-byte record is the one `alice.exe`'s loader copies (`0x0049a360`), checked field by field against the source entities on all thirty-six maps, and each map's are converted into the static light file the package now ships beside its audio zones ([sidecar record](2026-09-24-alice-map-sidecars.md)). All four model formats read -- 694 `TAN `, 79 `SKL `, 1,402 `SKAN` and 801 `.tik` ([model record](2026-09-22-alice-models.md)) -- and the 797 `.cik` files are compiled definitions that the retail loader prefers to the text. One TIKI runtime per process loads them, compiles the text where there is none, and answers the queries both modules make, with retail's pose, skinning and tag arithmetic ([TIKI record](2026-09-23-alice-tiki.md)). Ritual's shader dialect goes through all three renderers' parsers: of the 2,162 shaders in the installation's 52 scripts, only the 23 stageless compiler shaders that Quake III's own rule also rejects fail, in each parser (`tests/alice_shader_parse_tests.py`). The twelve `.urc` menus read, 401 widgets (`tests/alice_urc_tests.cpp`), and so do the fourteen `.ritualfont` fonts (`tests/alice_ritualfont_tests.cpp`). The level scripts, camera paths, path nodes and dialog files are read by the retail game module itself, through the file imports. |
| Server / gameplay | PARTIAL -- Live subset | A developer server runs the retail game module on every one of the thirty-six retail maps: it spawns the level's own entities (685 spawned and 19 inhibited on `garden1`), runs the map's scripts in its own interpreter, and its entities are linked into a world that the area query, `ClipToEntity` and the trace reach ([module record](2026-09-22-alice-module-abi.md)). That sweep ran two hundred server frames a map on a build whose `wait` still counted frames; Alice's `wait` now counts milliseconds in the client's pass alone, as retail's does, so a dedicated repeat needs one-frame waits (`tests/alice_command_wait_tests.py`). Linked entities record their areas and clusters, so they reach snapshots -- 195 of 274 on `garden1`, where none had -- and slot 39 opens the portal between an entity's two areas as `0x004536d0` does. The level flow is retail's: `map`, `devmap`, `gamemap`, `restart`, `pause`, `savegame`, `autosavegame`, `loadgame` and `loadbrokengame` replace the Quake III set, with retail's spawn order and three settle frames ([savegame record](2026-09-23-alice-savegames.md)). In a developer client on `garden1` the module's own `skipcinematic` ends the opening scene, and weapons and powerups given from the console are wielded and shown on the HUD ([HUD record](2026-09-24-alice-hud.md)). `sv_maxclients` defaults to 1 and clamps at 128, `MAX_CONFIGSTRINGS` is 1,328 and `MAX_SUBMODELS` 512. There is no bot library: the maps carry 5,365 designer-placed `info_pathnode` entities for the module's own AI and no `.aas` file ([map record](2026-09-22-alice-maps.md)). No movement, combat, death or level exit has been played, because every run was driven by console commands with no input injected. Retail's reuse path, which keeps the module for a load or restart of the running map, is not reproduced, and the configstring index the players start at is unpinned. |
| Client / renderer / audio / input | PARTIAL -- Live subset | **The client plays the opening levels.** With the retail client game running, `garden1` loads, plays its letterboxed opening, and after `skipcinematic` draws Alice in third person with AliceHud's eight children -- the health and mana gauges and the weapon and powerup foldouts, with their fold, card swap, dice count and powerup timer ([HUD record](2026-09-24-alice-hud.md), [snapshot evidence](2026-09-22-alice-wire.md#the-snapshot)). **The menus are the engine's own**, hosted as the client's UI module: the twelve `.urc` menus load and every widget class they use is served -- Label, Button, CheckBox, Slider, Field, List, FakkBindList, FakkLoadGameClass, AutoScroll and Alice3D -- with retail's menu cvar rules, pause and Escape order, and the UI's commands registered once so that they outlive the UI's restarts. The main, settings, key binding, credits, quit, load/save, `zound` and `GameCamera` menus were drawn at the main menu and in play ([menu record](2026-09-24-alice-menus.md)). **The intro and cinematics run**: in one windowed client (`.tmp/alice-cin/run4`, built in `meson/build/win32-alice-cin`, executable hash not recorded), whose probe chained its steps through `nextmap`, the intro played `ealogo.roq` and `roguelogo.roq` and brought up the main menu; the New Game button's own command, entered from the console, played `opening.roq`, whose end loaded `gvillage` and drew its opening camera. `tests/alice_cinematic_tests.py` holds those steps to retail's `cinematic`, `CIN_Play`, `CIN_Finish`, `CIN_Stop` and `_intro`. An in-game `cinematic` shuts the local server down and keeps the local client, as retail's `SV_Shutdown` (`0x00455910`) does: on `garden1` the game's own ending steps played `ending.roq` and brought up the credits over the main menu, and a `nextmap` naming a level loaded it after an in-game movie (`2026-09-24-engine-input-capture-fixes.md`). **All three renderers draw Alice's levels**, with the light grid, FTX textures, TIKI models and Ritual's shader dialect: five `garden1` captures from one probe script, run on GL and seven minutes later on Vulkan and RTX from `meson/build/win32-alice-vk` (`.tmp/alice-vkshade/rx-glx`, `rx2-vk` and `rx2-rtx`, JPEG, executable hashes not recorded), differ from GL's by a mean of 2.97/255 to 3.05/255 a channel on four of them and 4.3/255 on the lens-flare capture, and Vulkan's from RTX's by 0.45/255 to 1.28/255; and `tests/alice_renderer_parity_tests.py` keeps the Vulkan and RTX front ends' Alice functions identical to GL's. **No overbright bits**, the `0` `alice.exe` registers for both `r_overBrightBits` and `r_mapOverBrightBits` (`0x00475136`, `0x0047529e`): retail shifts lightmaps, vertex colours and the light grid by its overbright bits alone (`0x0049ba40`), so each map's light draws as the map holds it. fnTech3's usual one bit over two had drawn `garden1`, `utemple` and `centipede1` 1.96, 2.22 and 2.68 times as bright on GL and on Vulkan ([overbright record](2026-09-25-alice-overbright.md); `tests/alice_overbright_tests.py`). A lightmap stage that names no `rgbGen` takes identity, as `alice.exe`'s parser gives it (`0x0048c648`); every stage Alice ships was identity already. Characters and weapons, which the game module flags `RF_EXTRALIGHT`, are lit from the map's compiled lights by `alice.exe`'s law, fitted to the renderers' own model light, and other models from the grid as the map holds it; on `utemple` the grid had drawn Alice black ([model light record](2026-09-25-ritual-model-lights.md); `tests/ritual_model_lights_tests.py`). The grid's ambient takes the `0.5` `alice.exe` registers for `r_ambientScale` (`0x004754ed`), not Quake III's `0.6`, which had drawn the props it lights 2% to 14% brighter at the same light on six opening views ([ambient scale record](2026-09-25-ambient-scale.md); `tests/ambient_scale_defaults_tests.py`). **Sound and music are served**: the client game's sound imports (30 to 41) and music imports (42 to 44) reach fnTech3's own Alice sound and soundtrack engines ([audio record](2026-09-23-alice-audio.md); `CL_AliceFillCGameImports` in `code/client/client_alice_cgame.hpp`), and in the intro run above each logo printed `Playing mp3`, the main menu loaded `sound/music/intro.mus`, and `gvillage` loaded its soundtrack and set its moods (`.tmp/alice-cin/run4/base/qconsole.log`). No listening test has been made, that run's audio was muted by the unfocused window, and every other run above had sound off. **Map sidecars**: each official map ships a generated audio zone file and static light file, served from the package; the zone compiler reads FAKK 42 with Ritual's flags from `alice.exe`'s own `surfaceparm` table (`0x005294d0`). The light files load in all three renderers and add up to eight dynamic lights a scene by default, keeping their balance with no overbright bits, and the zones give the OpenAL backend its room ([sidecar record](2026-09-24-alice-map-sidecars.md)). **Alice's voices are rendered by the engine's own audio pipeline**, not a copy of Miles: the OpenAL distance model with each sound's own min distance, air absorption, traced occlusion, zone or traced room reverb, tone classes by Ritual's channels, and HRTF, while the engine keeps retail's voice bookkeeping; a room the client game names replaces the zones while it is named, and the legacy mixer keeps retail's gain and pan. Static models compiled into the world whose shaders say `rgbGen lightingDiffuse` (745 surfaces on six maps, 670 of them on `utemple`) had drawn black, as the world entity gives them no light; all three renderers now light them with the colours their map baked on the vertices, and `utemple`'s kelp draws lit (found on F.A.K.K.2, [record](2026-09-24-fakk2-contracts.md); `tests/ritual_world_diffuse_tests.py`). The far-plane fog of `centipede1` and `utemple` now fogs an opaque surface once, as `alice.exe` does: its stages unfogged, then one pass of the fog colour with alpha min(|v - eye| / distance, 1) (0x0049ebb0, RB_BeginSurface 0x0049286e), where fogging each stage with RTCW's white neutral had brightened distant walls; while the far fog is on, no fog volume is drawn either (0x004927d3..0x004927ff; `tests/ritual_far_fog_tests.py`). Their first views moved by 0.5 of 255 on average. On `garden1` the level's sounds carry reverb sends and occlusion ([audio record](2026-09-23-alice-audio.md#the-engines-own-features)); no listening test has been made. **Input**: the user command follows retail's rules in unit tests (`tests/alice_input_tests.cpp`), but no run has been driven with mouse or keyboard input. One cinematic run started on the interactive desktop (`.tmp/alice-cin/run3`) received three presses routed to Escape that its probe never sent, which a window there can get from the desktop's own keyboard; the engine invents none, and the same flow on a private desktop played to its end (`2026-09-24-engine-input-capture-fixes.md`). No fnTech3 frame has been set beside retail's, the light definitions are dropped, and `soundpicker` and `viewspawnlist`, which the developer menus call, are not served. |
| Savegames | PARTIAL -- Source, Unit, Live subset | **Saves are written and read back on developer servers and clients** ([savegame record](2026-09-23-alice-savegames.md)). Retail's four files per save, `save/<config>_<name>.sav`, `.spv`, `.ssv` and `.tga`, go into the home path, never the installation. The engine writes retail's 200-byte header, which has no magic, and the area portal block behind it; the game module writes its level and persistent archives through exports 16 to 19 at the shapes retail's call sites push; and the grey 256 by 256 picture equals retail's rule applied to the engine's `screenshot` of the same frame in 65,536 of 65,536 pixels. The level flow follows retail's order: on `garden2` a level archived at 3,250 ms reads back at 3,250 ms and runs on, `gamemap` carries the persistent file into the next level and its queued autosave writes `garden20000` as `Starting Herbaceous Border`, and that save loads in a process with no map running. A load of the running level keeps its configstrings and model table, as retail's reuse path would: on `garden1` a save made 12 s into play and loaded 3 s later reads back at level time 12,550, and play resumes with no `Can't find animation` or `Could not find parent` line. `map`, `loadgame` and `loadbrokengame` stop a playing cinematic where retail does (`0x00451118`, `0x00451b09`, `0x00451c53`). The load/save menu lists saves from their headers without `quicksave`, saves from the list and closes, and asks before deleting ([menu record](2026-09-24-alice-menus.md)). **Open:** no retail-written save has been read and no fnTech3 save has been loaded by retail; retail's reuse path is reproduced only in its server state, because fnTech3 reloads the module on every spawn; a load from a fresh process, or of another map, reads model indexes back as numbers, and on `garden1` the client game then reports `Can't find animation 1` every frame, which by the record's reading retail's full path shares but no retail run confirms; and `CalcCRC` has not been reached at run time. |
| Optional services | Unavailable | No service success is synthesized. Ritual's `fakkmaster.ritual.com` master server has been gone for years; no master query is made. |

Build with `-Dalice-development=true` only for isolated probes. An ordinary
build refuses the profile at detection and writes nothing. The option is
independent of the RTCW, ET, Elite Force and Jedi Academy options; none of them
closes any release gate, and no other title's evidence is Alice evidence.


## F.A.K.K.2 gate matrix

Heavy Metal: F.A.K.K.2 is the ninth title and the fourteenth profile, `fakk2`,
over one `fakk/` installation. It runs Ritual's own engine, the parent of
Alice's, and fnTech3 serves it through the Alice adapters with F.A.K.K.2's
records crossing at the module boundary. The
[contracts record](2026-09-24-fakk2-contracts.md) pins the installation and the
evidence. The disc's `fakk2.exe` is LaserLock-encrypted. The official 1.02
patch's is plain, and the same build as the retail modules. It was read and
runs here as a client. Its server starts only with the CD: with an image of the
user's disc copy mounted and the machine key it reads set, both with the
user's approval, it hosted fnTech3's client. So
the engine's behaviour is read from that executable where it has been checked,
and otherwise taken from `alice.exe`, with every such inference marked in the
record.

**Observed.** In developer builds (`-Dfakk2-development=true`, Windows x86,
`meson/build/win32-fakk2`) against an installation laid down from the user's
retail disc: a dedicated server spawns all thirty maps through the retail game
module and quits cleanly, and a client enters all thirty; a client draws the main menu from the retail `.urc`
files, plays `fakkhouse`'s opening under its sky portal, saves and loads back,
and records a demo and plays it back. In play its HUD menus draw the health,
water, ammunition and boss bars and the weapons' models, its inventory opens
from `warpinv` with the held items' models, and `saveweaps` keeps a weapon group
that survives a restart. A level exit fired from the game script carries the
player to the next level with the weapon in hand and autosaves there. Every
client run was windowed at 800x600 on a private desktop with an isolated home
path. Its frames were taken with the engine's own `screenshotPNG`, and each
probe's manifest records the installation unchanged. The user's keyboard,
mouse and desktop were never used. The runs that play by input queued their
keys, characters and motion inside the engine (`in_synthesize`, enabled only
by `+set in_synthetic 1` on the command line, through
`scripts/probe_ritual_client.py --actions`). What they show:

- **Menus.** Hover sounds play, and clicks open the menus. A pulldown's list
  opens, is dragged to "Low" and sets `r_picmip 2`. The graphics report's text
  is selected by a drag and by Shift with the arrows.
- **Save list.** Menu saves are made, rows are chosen by click and by
  Up/Home/End, a save is removed by Delete, and Enter loads it.
- **Inventory.** The cursor mode opens on I, types and rows are picked by
  hover and click, and an item goes into Julie's left hand.
- **Play.** On `fakkhouse`, W, A, S, Space and mouse turns move Julie, MOUSE1
  fires the handgun and CTRL swings the sword.

**Compared with retail.** Retail `fakk2.exe` 1.02 plays an fnTech3 demo of
`fakkhouse` end to end. Frame by frame (`cl_avidemo`), it draws what fnTech3
draws. The comparison found three differences and each was corrected from the
executable: the message bodies and a snapshot byte, the HUD's rule, and the
sprites of hidden emitters. Its main menu has the same content, drawn 1:1
where fnTech3 scales it. Its `shortversion` is "1.02", which fnTech3 now
registers too. Its save pictures are in colour, and fnTech3's now are as
well. **A live session:** retail's client joins an fnTech3 dedicated server
over loopback UDP and plays. The server sees its movement, turns, fire
buttons and commands. That needed Ritual's handshake, a plain-text `connect`
with protocol 1 and a bare `connectResponse`, which fnTech3 had never run
over a network. With `g_syncronousClients 1`, retail records a demo there,
and fnTech3 plays it back as retail does, down to the same 302 warnings.

**The other way.** On 2026-09-25, with the user's approval, an image of the
user's disc copy was mounted as a CD-ROM drive, and the machine key retail's
server start reads (`Software\Ritual\FAKK2`, `PATH`) was pointed at it.
Retail then ran `fakkhouse` itself and as a dedicated server:
- **Network.** fnTech3's client joined that server over loopback UDP, played
  the opening cinematic, and played. Holding W moved Julie to the point
  fnTech3's own server puts her, within 0.01 units by retail's `whatis`, and
  its buttons armed and swung the sword.
- **Saves.** Saves crossed both ways, at the opening and armed with the sword
  in the right hand: fnTech3 loaded retail's to the same view, and retail
  loaded fnTech3's.

The key was removed and the image unmounted afterwards.

**Not established.** Every gate below stays partial, and the release gate
stays closed.

| Gate | Evidence | Remaining requirement |
| --- | --- | --- |
| Profile / identity / discovery | PARTIAL -- Source, Unit, Live subset | The profile, its three aliases and its limits pass unit checks. `default.cfg` ships loose rather than in a pak, so the installation is named by `fakk/pak0.pk3`'s `menu.cfg` (413 bytes, CRC `609b5da3`) and `maps/fakkhouse.bsp` (4,700,748 bytes, CRC `b72ef3a6`). Protocol 1 is admitted by this profile and Alice's, as both retail engines speak it, and refused by the twelve others. A copy of the retail disc is refused with a message naming the missing `fakk/gamex86.dll` and the installer. A German installation, from the same disc's `deu` pak, is the same profile: its localized menus draw, `localized.cfg` runs and its voices play. The German pak's stray `fakk/` lip-sync folder holds English leftovers that retail cannot reach either: `fakk2.exe` 1.02 asks for a sound's lip-sync at the sound's own path (0x00432d70), and its filesystem did not find a test pak's `fakk/strayprobe.cfg` by its bare name. The 1.02 patch changes only the executable and `pak2.pk3`, since its game libraries are the disc's. With that pak mounted, fnTech3 draws the patched advanced options menu with every control retail's shows, in the same state, but for the two documented presentation defaults. The disc's two libraries are Win32 x86 PE files, and the profile names them on Windows x86 alone (`Com_GameManagerNativeModuleFilename`), so another platform has no module to load. No other platform has been run. |
| Module ABI | PARTIAL -- Source, Unit, Live subset | The four table lengths are measured in the retail libraries and frozen: 105 game imports, 26 game exports with API version 4, a 708-byte entity record and 2,112-byte client record; 124 client-game imports and 6 exports. Every slot is the member Alice's table keeps at the mapped position (`code/compat/fakk2_module_abi.h`); the tag queries take F.A.K.K.2's two bone arrays and client-game slot 73 draws boxes. The records crossing the boundary -- entity state, render entity, view, snapshot, game state, command lists and mark fragments -- round-trip in `tests/fakk2_module_abi_tests.cpp`. **105 of 105 game imports and 120 of the 123 client-game imports after the version slot are served**; the three that are not have no call site in the retail client game. |
| Wire codec / demos | PARTIAL -- Live subset, retail demo playback | The framing is Alice's protocol 1, as `fakk2.exe` 1.02's readers show: no acknowledgement, and a gamestate ending at a zero byte. The bodies are F.A.K.K.2's own. Its readers walk a 73-field entity table with its own 31-entry dictionary, and a player table with `pm_flags` and `gravity` in 16 bits. These are generated from the executable (`scripts/generate_fakk2_wire_tables.py`) and coded with Alice's algorithm over F.A.K.K.2's records. Every Ritual snapshot carries the server's time residual after its time. fnTech3 had sent Alice's bodies and no residual byte, and retail refused its demos. Now retail `fakk2.exe` plays an fnTech3 demo through to its end, 207 snapshots (`tests/fakk2_message_codec_tests.cpp`, `tests/fakk2_wire_tests.py`). The 48 KB messages need Alice's long fragment offset; `oracle`'s gamestate is 40,244 bytes. The reliable window is `fakk2.exe` 1.02's 64 commands each way (0x004468dd, 0x0041ffaf), where Alice's engine rings 128. fnTech3 had rung Alice's, because protocol 1 is one wire for both. A client on that wire now rings the profile's window. On an fnTech3 dedicated server, retail `fakk2.exe` and fnTech3's client each take a 60-command burst whole. At 70, each is dropped at the 65th with `Server command overflow`, as retail's server drops. `fakk2.exe` also bounds a map at 384 inline models (0x0042575f), not Alice's 512, and the profile now holds that. On every one of the thirty maps, retail `fakk2.exe` joins fnTech3's dedicated server and enters the game: connect, gamestate, active, the module's `ClientBegin`, and an answer to its `whatis 0` (`.tmp/fakk2/sweep/sweep.py`). A client that leaves is announced with `print "%s %s\n"` and sent a bare `disconnect`, as `fakk2.exe` does (0x00443abb, 0x00443b2b), and a retail client printed another's departure in that form ([drop notice record](2026-09-24-rtcw-et-drop-notices.md)). **Live with retail:** the handshake is Quake III 1.x's (`code/qcommon/ritual_handshake.hpp`, `tests/ritual_handshake_tests.py`). There is a bare `getchallenge`, then `connect "<userinfo>"` printed as plain text with protocol 1 (0x004215ef) and read with no Huffman stage (0x00447100), and a bare `connectResponse` (0x004439fd). fnTech3's server had dropped the plain `connect` in a failed Huffman decode, and its client had asked for protocol 68. With both corrected, retail `fakk2.exe` joins an fnTech3 dedicated server over loopback UDP, and its usercmds, fire buttons and client commands drive Julie there. fnTech3's client joins the same way. A demo retail recorded on that server plays in fnTech3 as in retail: its recorder skips entity 0's baseline (0x0042039e), and both engines drop Julie and her weapon on playback, 302 times each. fnTech3's client joins retail's dedicated server the same way, with an image of the user's disc mounted for retail's CD check: it plays the opening cinematic, and its movement lands Julie where fnTech3's server does, to 0.01 units, with its attack buttons swinging the sword. |
| Filesystem / pure / downloads | PARTIAL -- Live subset | Developer servers and clients mount both paks, 12,142 files, and execute the loose `default.cfg`. Saves are written into the home path. No pure or download claim is made. |
| Assets / collision | PARTIAL -- Source, Unit, Live subset | All thirty maps are `FAKK` version 12 with Alice's records and light grid, admitted by this profile alone (`tests/bsp_profile_loader_tests.py`). Of the 1,347 shaders in the scripts, only the 15 stageless compiler shaders and two that retail also refuses fail in each of the three parsers, and the 58 live `nextBundle` stages parse alike in all three (`tests/alice_shader_parse_tests.py`). The 29 `.urc` menus load. |
| Server / gameplay | PARTIAL -- Live subset | A developer server spawns every map's entities through the retail module (931 on `fakkhouse`) and runs its scripts, camera paths and path nodes. Inline models now own their model indices as `alice.exe`'s world clear gives them (0x00457960), which keeps the module from running a model definition's init commands on a brush entity; that had removed `fakkhouse`'s sky camera. A level exit fired from the game script (`fakkhouse` to `training`) runs through the retail module's own intermission and `gamemap "<map>$<spawn target>"`. The persistent data is archived and read back, the weapon in hand carries over, and the new level autosaves. The audio zones found the new level only once their lookup dropped the spawn target, as the BSP name does. New Game's intro hands over to `fakkhouse` the same way. After a death, the module's respawn brings up the Load/Save menu, as retail's does, and a save loads back in a fresh process with the weapon in hand. Movement and combat run through the module's own developer commands: Julie jumps 630 units across `fakkhouse`, a spawned creeper kills her, and her handgun and sword wound it, 10 and about 13 a hit. That needed the entity pass of the engine's trace rebuilt as retail's: each clip model is now built as it clips, where a stale shared box had let bullets and blows through actors, and traces through doors. Played by engine-queued keys and mouse, W carries Julie 148 units along her heading, and 400 counts of mouse motion turn her 44°. A strafes her to the wall, and Space jumps her. MOUSE1 fires the handgun and CTRL swings the sword, their sounds starting. Retail's client drives her the same way on an fnTech3 dedicated server (Wire). On `creeperpens`, once the only client has left, the retail game module faults in its own frame: it posts `stopwatchingactor` to the player it no longer has (gamex86 0x10038437, faulting at 0x100a2799). Retail's `SV_Frame` also runs the game with nobody connected (0x004477af), so its own dedicated server is inferred to fault the same way **[I]**; that needs the disc to check. |
| Client / renderer / audio / input | PARTIAL -- Live subset | The retail client game runs; `fakkhouse` draws its world, actors and sky portal, alike on the GL, Vulkan and RTX renderers. The RTX renderer's ray-traced mode did not activate here, so only its raster path ran. The main menu draws from the retail files with Julie's `nextBundle` highlight and the cloud layer, whose `rgbGen global` passes read the colour each picture was drawn in, as `alice.exe`'s immediate `R_SetColor` has them (0x00498b80). Sound uses fnTech3's OpenAL pipeline and the soundtrack engine Alice uses; `fakkhouse`'s soundtrack loads and changes mood. The engine reads the cinematic, fade and letterbox stats from F.A.K.K.2's own slots (14, 15, 16, where its retail game module stores them). The HUD is UIFakkLabel's stat bars with their flash, numbers and item models over the four HUD menus. They show by `fakk2.exe` 1.02's own rule (0x0040b29b..0x0040b326): `ui_hud` set, no letterbox or menu, and no delay; the health bars always; the one-handed menu even with empty hands, unless a two-handed weapon is held; the boss bar while stat 13 is above zero; models in a view without a world take Quake III's flat light, as `fakk2.exe` 1.02 gives them (0x0048f143); `alice.exe`'s rule would leave F.A.K.K.2's lit weapon models black. The inventory (`global/inventory.scr`, `inventory`, `warpinv`, `nextinv`, `previnv`, `inv_restart`) and the weapon groups (`saveweaps`, aliases kept in the configuration) are served, an item going to the hand used by `fakk2.exe` 1.02's rule (0x00409fc9, 0x0040a209); the Alt/Ctrl tables run from `default.cfg`; the options menus' `PulldownMenuContainer`, `MultiLineEdit`, `linkstring`, menu `fadein`, `ui_checkrestart` and `r_gfxinfo` are served, the controls menu's bind list is `fakk2.exe` 1.02's three columns of pictures with an alternate key per action (Draw 0x004c1360, keys 0x004c0de0), scrollbars are its `UIVertScroll` (0x004b3ab0), sliders its `UISlider` with 15-unit arrows and a 10-unit thumb in the colour the widget's draw left (Draw 0x004b64e0), each widget is cut to its frame within its menu as its 2D window's scissor cuts it (0x0049ee80, 0x0049ec21), the menus' cvars include `joy_threshold` (0x0045768a) and `r_useglfog` (0x004616fc), a checkbox's box sits 16 units in and its title 48, as the constructor's indent puts them (0x004bae69..0x004bae7a) where Alice's is 0, and the Load/Save menu lists the saves as retail's list control draws them (`0x004ce3a0`) (`tests/fakk2_hud_tests.cpp`, `tests/fakk2_ui_source_tests.py`). Every official map ships a generated audio zone file and a static light file from its compiled 56-byte lights ([sidecar record](2026-09-24-fakk2-map-sidecars.md)); `fakkhouse` loads 343 zones and 128 lights at run time. Each light's radius follows `fakk2.exe` 1.02's model-light law at its defaults, 7500 a light (0x0048dd80, the constant at 0x004d6050) times the factor of 4 its last step gives the sum, `1 << (r_mapOverBrightBits - tr.overbrightBits)` (0x0048e090): `alice.exe`'s `|I| x 30000 / d^2`, as the files had inferred. Their colours are balanced for the default per-pixel dynamic lights: unbalanced, `towncenter_evil`'s opening averaged 123 with them and 22 without, and it averages 46 balanced. Beside retail's first views, on all three renderers, the lights leave `fakkhouse`'s outdoor opening as it is but brighten `over`'s tunnel walls to 1.20 to 1.41 of retail's and `towncenter_evil`'s room to 1.20 to 1.59, where those walls draw at 0.94 to 1.12 and 0.97 to 1.03 without them. The balance allows that, letting a light add up to half of full brightness at its origin; half the radius would spare the walls away from the lamps but give models a quarter of retail's light, so it is left open. The inventory's column, cells, gauges and models are drawn as `fakk2.exe` 1.02 draws them (`FakkInventory::Draw` 0x004088c0, `FakkItemList::Draw` 0x00409950, `DrawTilePicOffset` 0x00481dd0). Retail's client on fnTech3's server draws the same inventory from the same state. The comparison found the gauge had been drawn over the cell art instead of under it, the item's own selected picture swapped for the type's, and a model hidden where the world behind its cell was nearer: fnTech3 had kept Alice's `ui_skipDepthClear`, which F.A.K.K.2 does not have. The mix is checked from the engine's own capture, a replayed `fakkhouse` demo recorded by `video` (`scripts/probe_ritual_audio.py`). The creeper's slashes and attacks and Julie's pain cry are each found in the recorded track at the time the log gives, at their own pitch. The track never clips, and the soundtrack accounts for 3.3 dB of it. On the OpenAL backend the same replay's sources carry the engine's occlusion, reverb sends and zones. Set beside retail's frames of the same demo, the view, models, effects and HUD match, 12/255 apart on average. Emitters' sprites are now drawn as retail draws them, whatever their flags, so the chimneys smoke and blood splats show. Retail draws its menus and HUD 1:1 where fnTech3 scales them, a presentation choice. The version label reads retail's `1.02` (0x0042e4fd). The intro is `fakk2.exe` 1.02's (`_intro` 0x0040e0b0): the soundtrack, then Ritual's and Gathering's plates, each faded in, held and faded out, then the main menu. It and Escape disable BACK TO GAME outside a game (0x0040e295, 0x0040c032); fnTech3 had gone straight to the menu and always offered it. A widget's picture takes its whole foreground colour (0x0049f078), and a button's hover picture stays on the button that has the keys (0x00498c10); each change of menus gives the keys to the menu's current order (0x004a40d0), so the main menu opens with NEW GAME lit, as retail's does. fnTech3 had lit a button only under the mouse, which a retail capture of the main menu showed as a halo it lacked. Its full-screen menus fade over black, the frame this engine clears under them; `ui_menuDissolve 1` is the retail-exact option of fading them over the image retail's uncleared screen keeps (0x0040b0cf..0x0040b296), where the old menu hazes white and the next comes through. `ui_hud` is F.A.K.K.2's unsaved flag (0x004df7b8), and Alice's `ui_subtitles`, `ui_skipDepthClear`, `ui_crosshair` and `ui_jumpreticle` are Alice's alone. **Input:** a probe's engine-queued keys and mouse (`in_synthesize`, command-line gated, Ritual only; `tests/ritual_synthetic_input_tests.py`) drive every menu path. Hover sounds (`sound/dialog/menu/*.mp3`) and click sounds start. A pulldown's list opens and chooses. The graphics report's text is selected by drag and by Shift with the arrows. The save list takes clicks, Up, Home, End, Delete and Enter, and a double click loads the save, as retail's list control's does (0x004ad497). The inventory's cursor mode picks types and rows by hover and click and uses an item. The view's centre and located prints and frame counter are in `facfont-20`, the font retail builds its view with (0x0040ecb2), and the loading picture is centred at its own size on black (0x0040b14c). F.A.K.K.2 draws with no overbright bits, as `fakk2.exe` 1.02 registers it (0x00460d74): 338 of its 365 lightmap stages had taken Quake III's identity lighting, which one bit halved; they now take retail's identity (0x00476e8c), so each draws with the texture over it as one pass, as retail's does (`tests/alice_shader_parse_tests.py`). Its grass had drawn at 0.69 of retail's. The same demo's frames now match retail's region for region on the GL renderer (`tests/fakk2_overbright_tests.py`). On all thirty maps, the first view from retail's client and fnTech3's on fnTech3's server match in world, HUD and models, but for errors of fnTech3's, now corrected (the world's static models, the far-plane fog, fog volumes under it, animation frames, and models' light, now taken from the map's compiled lights), and the canal water's caustics on `over` and `under`, not traced. The first correction: static models compiled into the world, whose shaders say `rgbGen lightingDiffuse` (`training`'s banners, `towncenter_good`'s ferns, `oracle`'s cloth; 55 surfaces on `fakkhouse`, 745 on six Alice maps), drew black. Retail skips their lighting call for the world entity and keeps the colours last written (0x0047d45a); all three renderers now light them with the colours their map baked on the vertices, as Medal of Honor's static models are lit (`tests/ritual_world_diffuse_tests.py`). The far-plane fog of six maps had brightened distant lightmapped walls, because each stage was fogged on its own and one modulating the lightmap took RTCW's white neutral colour. Retail draws an opaque surface's stages unfogged and then one pass of the fog colour, alpha min(|v - eye| / distance, 1) at each vertex (FinishShader 0x004792bc..0x0047937b, 0x004948a0; `alice.exe` alike), and so now does fnTech3 for both Ritual profiles in all three renderers; a shader with a blended first stage keeps the per-stage fog (`tests/ritual_far_fog_tests.py`). On the GL renderer `fog`'s view then matches retail's region for region within 1% (the pool 4% with the fog off), and `swamp1`'s within 2%; on the Vulkan and RTX renderers the rock, ivy and cave walls match as closely, and the ground by the glowing items is 1.4 times retail's, 1.07 on the Vulkan renderer with its default bloom off. While the far fog is on, retail also draws no fog volume (RB_BeginSurface 0x0047c553..0x0047c583): `over` and `under` start the camera inside a black one, which had halved `over`'s tunnel and blackened Julie; both are lit as retail's now. An animated stage keeps retail's 64 frames (0x00476e00), where fnTech3 had kept 24 of the water caustics' 47. The canal water there is still bluer than retail's lime green: its caustics add blue in fnTech3 and none in retail, measured with overlay textures and not traced. Characters and weapons are lit from the map's compiled lights by `fakk2.exe`'s own law (7500, shifted by the map's two overbright bits, `0x0048e090`), fitted to the renderers' own model light, and other models from the grid. On `water`, where the grid is black at the dedicated server's start, Julie was a third of retail's brightness on her silhouette and is now within 7 to 9% of it; on `over` she is 17% above it ([model light record](2026-09-25-ritual-model-lights.md)). The Vulkan renderer's brighter walls are its bloom, on by default for every profile: with `r_bloom 0` it matches the GL renderer region for region. The user listened to the engine's mix of the `fakkhouse` demo and reported that it sounds right (2026-09-25); that mix is the legacy mixer's, which `video` records, not the default OpenAL path's with its EFX environments and zones. **Open:** other levels' audio, and the OpenAL path's on any level, have not been listened to; a private-desktop probe mutes its listener, so no probe here can record them. |
| Savegames | PARTIAL -- Live subset | A save made in play on `fakkhouse` writes the `.sav`, `.ssv` and `.tga` files into the home path and loads back, with the retail module archiving and restoring its level. Saves made, chosen, removed and loaded from the Load/Save menu by click and key behave as its list control's. The picture is `fakk2.exe` 1.02's colour reduction (0x0045ffa0), not Alice's grey one, which had made F.A.K.K.2's shots near white (`tests/fakk2_saveshot_tests.py`). The engine's side of the format is read from `fakk2.exe` 1.02 and is Alice's: `save/<config>_<name>.<ext>` (0x0042ebe5), the `.ssv`'s 200-byte header and area portals (0x00442a00), the persistent `.spv` and level `.sav` through the module's archive exports (0x004428f0, 0x00442940), and `gamemap`'s queued autosave (0x00441f3c); the module writes the archives' contents in both engines. Retail's load menu lists and pictures saves fnTech3 wrote, from their `.ssv` headers and `.tga` shots, as fnTech3's own list now does (newest first, the level's title). With an image of the user's disc mounted for retail's CD check, saves crossed both ways on `fakkhouse`, at the opening and armed with the sword in the right hand: fnTech3 loaded retail's saves to the view retail restores, and retail loaded fnTech3's to the same state. |
| Optional services | Unavailable | No service success is synthesized; Ritual's master server is gone and no query is made. |

Build with `-Dfakk2-development=true` only for isolated probes. An ordinary
build refuses the profile at detection and writes nothing. The option is
independent of every other title's; none of them closes any release gate, and
Alice's evidence is not F.A.K.K.2 evidence except where the record says an
inference rests on it.


## Medal of Honor gate matrix

Medal of Honor: Allied Assault is the tenth title and the fifteenth profile,
`mohaa`, over one `main/` installation. It runs 2015's engine, a descendant of
Ritual's and of Quake III's, and fnTech3 serves it through adapters of its own
rather than through the Ritual profiles'. The
[contracts record](2026-09-24-mohaa-contracts.md) pins the installation and the
evidence, and the [map record](2026-09-24-mohaa-maps.md) the maps. The retail
`MOHAA.exe` is SafeDisc-encrypted, and SafeDisc does not run on this machine,
so the engine can neither be read nor run. Every engine rule is measured in
the two plain retail libraries, read from the data, or taken from OpenMoHAA's
reconstruction, and the record marks which.

**Observed.** In developer builds (`-Dmohaa-development=true`, Windows x86,
`meson/build/win32-mohaa`) against the user's 1.00 US installation, every
client windowed at 1280 by 720 with an isolated home path, its frames taken
with the engine's own `screenshot`, and every probe driven by console commands
alone:

- **Server.** All fifty-four maps spawn through the retail game module and
  run twelve seconds each on the dedicated server with no drop.
- **Client.** All fifty-four maps load in the client and exit cleanly. The
  main menu and its submenus draw from the 179 retail menus; `m1l1`'s
  briefing waits on its Continue button; `m1l1`'s scripted opening runs, the
  truck's crew drawn once customised actors reached the renderer; the HUD
  shows health, the weapon's clip, the compass, the weapons bar and the
  crosshair. On a listen deathmatch server the team menu, chat and game
  messages appear in their boxes. Over UDP on the loopback address, the Join
  LAN Game list finds a dedicated server and the client joins it from there.
  Two clients on one dedicated server play a free-for-all (`DM/mohdm1`,
  `g_gametype 1`, `join_team freeforall`), a team match (`DM/mohdm2`,
  `g_gametype 2`), a round-based match (`DM/mohdm2`, `g_gametype 3`) and an
  objective match (`obj/obj_team1`, `g_gametype 4`): each joins its team
  (`join_team`), spawns with its team's weapon, sees the other's join
  messages and lists both teams on its scoreboard, and the round-based and
  objective matches run their clocks; the server's `status` lists both. That
  `status` had dropped the server, reading a Quake III game client where 1.00
  keeps its own record; it and `getstatus` now read 1.00's, a client's score
  being its kills stat.
- **Level flow and saves.** A level exit (`gamemap m1l1` to `m1l2a`) carries
  the player's state and autosaves. Saves made in play and from the Load/Save
  menu list with their pictures; one is deleted through the delete question;
  games load from the list in play and from the main menu; `loadlastgame`
  follows a death; triggered music resumes where it was saved.
- **Demos.** A 3-second `m1l1` recording plays back with its HUD, and
  `timedemo 1` plays it to its end. So does a network client's 8-second
  recording of a dedicated `DM/mohdm1` free-for-all (151 frames).
- **Scripted waits.** Driven by rcon over loopback, so that no probe `wait`
  holds back what a level stuffs, the first briefing shows each slide on its
  narration's timing and hands on to `m1l1`; `training`'s instructor lines
  follow one another; `m1l1`'s convoy reaches the checkpoint. Until the client
  told the game when a sound it waits on starts (export 34, with import 135
  answered from the client), every script stopped at its first such wait --
  527 of them in 35 level scripts.
- **Single-player soak.** `m3l1a`, `m4l1`, `m5l1a` and `m6l1a` each ran three
  minutes with the player under `god` and the level's AI and scripts running.
  `m3l1a` had dropped the game after about two and a half minutes: its
  screen flashes drew with a picture registered anew each frame, which used
  up the renderer's 2,048 image slots. Registered once, the same soak runs
  its three minutes with its image count steady (350, then 359). Sixteen
  more levels -- `m1l2a` to `m6l3a` -- each ran two and a half minutes on the
  Vulkan renderer with no drop, their image counts flat but for models and
  menus first shown during the run. Their other `ERROR:` lines name sound
  aliases a level plays before any are defined -- in single player 1.00's
  client game reads `ubersound/`, after the server has spawned the level
  **[O]** -- or that nothing defines (`heavy_drop`; `m6l3a`'s own aliases sit
  inside a comment of its script). Two clients on a dedicated `DM/mohdm6`
  team match stayed connected for four minutes, with no drop on either side.
- **Single-player sweep.** Each of the 43 single-player maps, briefings and
  credits included, loads, passes its loading screen by `finishloadingscreen`
  and plays 45 seconds with no drop, script error or unserved import; the
  only `ERROR:` lines are the retail game's own AI notices (leashes and far
  planes reducing ranges) and `m1l2a`'s and `m5l1b`'s "Bad voice type m".
- **Loading bar, items bar and sun.** `m1l1`'s loading bar follows its load
  record to the end, with the game's resources counted during the listen
  server's spawn, and shows red to about three fifths in a frame taken
  mid-load. With `ui_itemsbar 1` and `ui_weaponsbar 3`, `give all` (under
  `+set cheats 1 +set thereisnomonkey 1`) puts the papers and binoculars
  models in the items bar. Facing `m3l2`'s sun with the game's `face` cheat,
  the GL, Vulkan and RTX renderers draw its lens flare.

**Not established.** No retail peer can run, so no packet, demo or save has
crossed between fnTech3 and retail in either direction, and no frame has been
compared with retail's. Every gate below stays partial, and the release gate
stays closed.

| Gate | Evidence | Remaining requirement |
| --- | --- | --- |
| Profile / identity / discovery | PARTIAL -- Source, Unit, Live subset | The profile, its three aliases, its marker, protocol 6's family, the 1,024-submodel ceiling and version 19 maps pass unit checks (`tests/game_profile_tests.cpp`, `tests/profile_marker_filesystem_tests.py`). The installation is recognised by `main/Pak0.pk3`; its six paks' names are capitalised; `system86.dll` is loaded from the installation. On i686 Linux (GCC 13, 2026-09-25) the dedicated server builds and links with the profile's runtime through the legacy Makefile and CMake, and the client through meson (X11, GL renderer, `-Dmohaa-development=true`), from the tree as it stands, since three shared files GCC had refused were made portable (`be_aas_file.cpp`'s `<cstring>`, `nav_graph.hpp`'s cast, `ghoul2_server.hpp`'s record names); neither can run there, as 1.00 ships its game, client game and allocator only as Windows x86 libraries (`gamex86.dll`, `cgamex86.dll`, `system86.dll`), so this profile runs only in a Windows x86 build. Other language editions and the later patches are unexamined. |
| Module ABI | PARTIAL -- Source, Unit, Live subset | The tables and records are measured in the retail libraries and frozen: 167 game imports, a 0xa4-byte export record with API version 11, an 852-byte entity and 2,272-byte client; 168 client-game imports and 30 exports; 596-byte entity and 660-byte player states, a 614,096-byte snapshot, a 412-byte render entity, a 42,948-byte game state (`code/compat/mohaa_game_abi.h`, `mohaa_cgame_abi.h`, `tests/mohaa_module_abi_tests.cpp`). The alias record both modules read keeps 1.00's layout, its subtitle at +0xd4 where the later games put a flag. **167 of 167 game imports and 149 of 168 client-game imports are served**; the other 19 client-game slots are read by no instruction in the retail module, so every import either module can call is served, and an unserved one would drop the game by name. |
| Wire codec / demos | PARTIAL -- Source, Live subset | Protocol 6's framing and record tables are OpenMoHAA's, acting on the 1.00 records: Huffman-coded messages, a client qport, the reliable-command XOR, the out-of-band direction byte, the three extra server opcodes and the end byte 11, sounds in the snapshot, and the eye record with each move. **Over UDP**, both processes bound to the loopback address: the client's `localservers` broadcast carries the direction byte, the dedicated server answers, and the Join LAN Game list shows its name, map, players, game type and ping; the client joins from the list and plays `DM/mohdm1`. That needed the handshake read as 2015's: the client had taken protocol 6 for an old Quake III one and connected with 68, which the server refused. The client now accepts a bare `challengeResponse <challenge>` (OpenMoHAA **[O]**) or one that echoes its nonce, and connects with 6. Only fnTech3's own client and server have exchanged messages. Demos are `demos/<name>.dm_6`, recorded and played by fnTech3 alone. Retail's own acceptance of any of it is unknown. |
| Filesystem / pure / downloads | PARTIAL -- Source, Unit, Live subset | Clients and servers mount the six paks and write configurations and saves into the home path. `sv_pure` is off by default, as 2015's server registers it **[O]**; with it on, the client proves its general pak references alone, with no server id, module checksum or `@`, as OpenMoHAA reads 2015's `cp` **[O]** (`tests/pure_profile_contract_tests.py`); the profile had had no proof, so a pure server dropped every client. Two fnTech3 clients played on a pure dedicated `DM/mohdm1`, and a client downloaded an extra archive the server loaded its map from (1,384,444 bytes, over UDP and again over HTTP from `sv_dlURL`, SHA-256 matching each time), then passed the pure check with it and played ([contracts record](2026-09-24-mohaa-contracts.md#wire)). Only fnTech3's client and server have taken part. |
| Assets / collision | PARTIAL -- Source, Live subset | All fifty-four maps are "2015" version 19, translated for collision and the renderers with their terrain, static models, light grid and fence brushes ([map record](2026-09-24-mohaa-maps.md)). TIKI models, skeletal animation and sound aliases run through OpenMoHAA's model runtime (`code/mohaa`). The patch collision builder's reports are traced to their patches: `m4l3`'s 48 degenerate-plane reports are twelve flat arches with cells of no area, which OpenMoHAA's builder, compiled from its source over the same patches, reports alike **[O]**; its skipped-bevel reports, now printed only at developer 2 as 2015's are **[O]**, differ from OpenMoHAA's float builder on a few thin cylinders ([map record](2026-09-24-mohaa-maps.md#patch-collision-reports)). |
| Server / gameplay | PARTIAL -- Live subset | The retail module spawns and runs every map; single-player scripts pass `level waittill spawn` once the local client's first snapshot arrives, as 2015's engine starts them, and their waits on sounds end when the local client's sound does; AI, doors, triggers and the level exit run. Deathmatch runs on a listen server. Two clients join a dedicated server over UDP and play a free-for-all, a team match, a round-based match and an objective match together. On a `devmap` server with `thereisnomonkey 1`, the game's own `tele` and `face` cheats stand them 250 units apart on `DM/mohdm1` facing each other, and one fires its Thompson by console command: the other falls dead, both read "ProbeAllied was perforated by ProbeAxis's SMG in the upper torso", the victim's view goes to the death camera, and the server's `status` credits the kill. Chat can be typed: the game reads only its own `dmmessage`, which the client game's `say` and `sayteam` keys now reach through fnTech3's chat line, where Quake III's line had sent a `say` the game never read ([contracts record](2026-09-24-mohaa-contracts.md#client), `tests/mohaa_chat_tests.py`); a player's `dmmessage` comes back to the listen client as chat. |
| Client / renderer / audio / input | PARTIAL -- Live subset | The retail client game runs and the GL renderer draws the worlds, terrain, static models, TIKI actors, sprites, far-plane fog and sky portals; the Vulkan and RTX renderers (the latter with its ray tracing off) draw the worlds, TIKI models and the sun's flare. The engine UI serves the retail menus, HUD, scoreboard, loading screens, message boxes, bind list, saved-game list, LAN game list and `dialog`; the briefing slides a level shows, the single-player objectives overlay, hints naming the player's own keys, the levels' fades and the dialogue subtitles appear. Presentation follows invariant 11: the HUD scales with the screen (`ui_hudScale`, 0 for retail's pixel sizes); sound runs through fnTech3's OpenAL pipeline with 2015's alias distances and pitch, and a room a level's reverb trigger names (none ships) takes the nearest EFX preset. Every official map ships an audio zone file and a static light file for the renderers' static map lights, beside it in its pak's folder; the renderers now look for a map's light file there before its base name, which no other profile's maps change ([sidecar record](2026-09-25-mohaa-map-sidecars.md)). `DM/mohdm1` loads 166 zones and 119 lights, and its first view promotes eight. A level's aliases exist in 2015's order: each map starts with none, on a server and on a client of a remote one, and a single-player level gets its own from the client game's parse, so a sound its spawn plays finds none, as retail 1.11's logs show; they had carried from each level into the next ([contracts record](2026-09-24-mohaa-contracts.md#sound-aliases), `tests/mohaa_alias_registration_tests.py`); music is the shared soundtrack manager plus triggered music. The team menus draw the Allied and Axis player models in 3D, and the map and player-model pickers, Start New Server (as a listen server) and the briefing room's difficulty commands are served. The audio backend plays the levels' voices where they are (a probe's `s_alDebugDump` in `m1l1` finds the captain's briefing line playing in 3D from his truck), and the dedicated server and two clients agree on who is playing. The loading screen's bar follows each map's load record (`maps/<map>.min`, `tests/mohaa_load_record_tests.cpp`) and counts the listen server's spawn; the items bar draws the held items' models (`invmodelhand`) under `ui_itemsbar 1` and `ui_weaponsbar 3`, which retail's menus never set. A level's sun flares from `global/lensflaredefs.txt` through the Alice lens flares in all three renderers, its lights by their lens-flare or additive type (`tests/mohaa_sun_flare_tests.py`); swipes are drawn as fading scene polygons in every renderer (`tests/mohaa_swipe_tests.py`), though no retail content uses one; beams are the client game's own scene polygons. Decals reach terrain and brush models: one cutter in the three renderers serves client-game imports 49 and 50, naming each terrain fragment by its patch as the client game reads it and handing a brush model's back in the model's own space, so bullet holes swing open with `DM/mohdm7`'s doors and the decal editor re-cuts `DM/mohdm3`'s terrain decals to 296 fragments where the retail file holds 312, on the same patches; a decal no larger than `ter_minMarkRadius` cuts no terrain, so no bullet hole marks it, as OpenMoHAA's renderer rules **[O]** ([contracts record](2026-09-24-mohaa-contracts.md#decals-and-level-of-detail), `tests/mohaa_terrain_marks_tests.py`). Import 54's lights, which the client game casts a character's foot shadows from under "Complex" shadows, are the map's sphere lights and sun as the collision model keeps them, chosen as OpenMoHAA's renderer chooses them **[O]**, where none had been given (`tests/mohaa_light_sources_tests.py`): in `m3l1a`'s landing craft a soldier's boot now casts a long foot shadow under Complex that Simple does not draw, not compared with retail's. The terrain a patch's variance tree takes out, which the collision model walks through and 2015's renderer never drew **[O]**, had been drawn; it no longer is, the collision model and the renderers reading the bits through one header (21,475 of `DM/mohdm4`'s 30,976 terrain triangles; [map record](2026-09-24-mohaa-maps.md)). The RTX renderer, with its ray tracing off, draws `m3l2`, its HUD and the sun's flare; asked for ray queries (`rtx_rt_mode 1`) it falls back to rasterising, as the driver offers this 32-bit process no acceleration-structure or ray-query extensions (an RTX 4060 laptop GPU). Its path tracer (`rtx_rt_mode 2`) runs anyway, on its compute backend: `m1l2a` traces a 274,974-triangle world with 105 of the map's lights. It reads those lights as the Quake III compilers' and has not been matched to 2015's, and the level's lamps light little. Terrain is lit by its patches' lightmaps in all three renderers; it had drawn unlit, glaring white on night levels such as `m1l2a` ([map record](2026-09-24-mohaa-maps.md), `tests/mohaa_terrain_lightmap_tests.py`). The Vulkan and RTX renderers had drawn the level's alpha-tested foliage cards as whole quads, as their multitexture shaders were compiled without the alpha test; they are now compiled with it, and `m3l2`'s tree line and hedges match GL's to within the run-to-run residual of the foliage's animation, fog included (`tests/multitexture_alpha_test_tests.py`). Mouse look turns the view by the client game's own scale, export 9's `fov_y / 75`, which the engine asks for on every move as 2015's does **[O]**; left at nought, the scale had turned every mouse move into none. Engine-queued motion (`in_synthesize`, on a private desktop) turned `m5l3`'s player 11.86 degrees for 400 counts at sensitivity 2 and scale 0.674, and pitched it 5.93 for 200 (`tests/mohaa_mouse_look_tests.py`); no probe may use a real mouse, so none has driven it. A level's permanent decals (`maps/<name>.dcl`) are built into its world in all three renderers, each fragment in the open leafs in front of it, the file's own leaf lists being stale in eight of its eleven files; on the three whose lists fit, 49 of 50 fragments land in exactly the listed leafs and the fiftieth adds only a solid one; `r_drawstaticdecals`, the options' "Static Decals" box, shows and hides them, and GL, Vulkan and RTX draw `m2l3`'s alike ([map record](2026-09-24-mohaa-maps.md)). The retail decal editor (`dcl_editmode`, F6's menu, `dclc_*`) and level-of-detail tool (`lod_*`, `l`'s menu) are served for every renderer by the engine through the client game's exports 16 to 18, whose arguments and buffers were measured; skeletal models lose detail with their size on screen by their shipped `.lod` records (22,078 triangles fall to 3,220 at the tool's far end), and the options' model-detail choice now reaches `r_lodscale`, which the renderers had held cheat-protected. Import 52's decal light was handed on a 0..1 scale where every caller stores it as a byte, drawing every lit mark black; it is now on the byte scale, as OpenMoHAA's is **[O]** ([contracts record](2026-09-24-mohaa-contracts.md#decals-and-level-of-detail), `tests/mohaa_permanent_decal_tests.py`). **Open:** beams are unexamined against retail, and decal cutting against anything but 1.00's decal files, `ter_minMarkRadius` being OpenMoHAA's **[I]**; how 1.00's own editor and tool presented themselves is inferred from their menus and OpenMoHAA **[I]**; and nobody has listened to the output, though a capture of `m1l1`'s opening mix through OpenAL Soft's wave writer holds the level's triggered music and the captain's first line, found by cross-correlation with the retail files. |
| Savegames | PARTIAL -- Live subset | A save is the game's `.sav`, fnTech3's own `.ssv` layout (1.00's cannot be read; `code/compat/mohaa_savegame.hpp`, `tests/mohaa_savegame_tests.cpp`) and a colour picture of the view. Saves, autosaves, quick saves, deletion, the Load/Save list, `loadlastgame` and the triggered music's resumption work in play; a save made in the middle of Omaha Beach's landing (`m3l1a`, 275 KB) loads back into the boat, and the landing carries on. No retail save exists to read, and fnTech3's cannot be offered to retail. |
| Optional services | Unavailable | 1.00's master servers are gone; no query is made and no service success is synthesized. |

Build with `-Dmohaa-development=true` only for isolated probes. An ordinary
build refuses the profile and writes nothing. The option is independent of
every other title's, and no other title's evidence is Medal of Honor's.

## Q3Test gate matrix

The public Quake III test releases of 1999, Q3Test 1.02 to 1.08, are the
sixteenth profile, `q3test`, under the Quake III title. Each release has its
own protocol (35 to 40), map version (IBSP 44 or 45) and module tables, so the
profile pins one release at startup from the installation's own modules and
pak and never mixes two. 1.02 and 1.03 shipped as dedicated servers alone;
1.05 added the client, with menus compiled into its engine; 1.06 to 1.08 carry
menu modules. The [Q3Test ledger](Q3TEST.md) records the contracts, and the
[client and interoperability record](2026-09-25-q3test-client-interop.md) the
runs.

**Observed.** In developer builds (`-Dq3test-development=true`, Windows x86,
`meson/build/win32-q3test`), every process windowed on a private desktop with
an isolated home, frames taken with the engine's own `screenshot`, and every
probe driven by console commands alone:

- **Server.** Every release's retail game module hosts every map it ships.
  Each client release's retail client (1.05 to 1.08) connects to fnTech3's
  server and enters the game; for 1.02 and 1.03, which have no retail client,
  a headless protocol client built from the same codecs joins the retail
  server and fnTech3's alike, walks and leaves. A server takes 128 clients,
  as every release does: the protocol client in slot 120 in every release,
  and a retail 1.06 client there too. The serverinfo and systeminfo keys
  match each retail server's, but that retail 1.05 leaves its game's keys
  out.
- **Client.** fnTech3's client runs 1.05 to 1.08's retail client games,
  joins its own server, each retail server and its own listen server, and
  draws 1.06 to 1.08's retail menus and 1.05's engine menus in GLx, Vulkan
  and the RTX renderer, raster and path-traced.
- **Demos and sound.** The six shipped recordings time-demo, 1.06 to 1.08's
  to retail's frame counts; recordings made by fnTech3's client replay in
  fnTech3, and an independent decoder reads every record. Each client
  release's replay mixes its recording's effects at their logged times in
  the legacy mixer's capture. The retail engines run in a restricted demo
  mode that reads only their pinned paks, so a retail client cannot load
  fnTech3's recordings.

| Gate | Status | Evidence and remaining work |
| --- | --- | --- |
| Profile / identity | PARTIAL -- Unit, Live | Release recognition and pinning (`tests/q3test_identity_tests.cpp`); every release activates from its installation. |
| Filesystem | PARTIAL -- Live | Each release's paks mount; retail's restricted-mode pak pins are not enforced. |
| Map format / collision | PARTIAL -- Unit, Live | IBSP 44 and 45 translate for collision and every renderer; Q3Test's curve rules are unit-tested; no trace comparison against retail. |
| Module ABI | PARTIAL -- Live | Game modules of every release; client games 1.05 to 1.08; menus 1.06 to 1.08. Bots are not claimed: no release shipped a bot library. |
| Wire | PARTIAL -- Interop subset | Short one-client loopback sessions in both directions with every retail peer that exists, up to slot 120 of 128; no session with several clients, loss or a remote peer. |
| Demos | PARTIAL -- Live | Shipped recordings time-demo (1.06 to 1.08 to retail's counts); fnTech3's recordings replay and decode; retail cannot read them; 1.05's machine-dependent time-demo count is not reproduced. |
| Renderer / assets | PARTIAL -- Live | GLx, Vulkan and RTX (raster and path-traced) captures of both map formats, menus and HUD; the release shader dialect and 2D pictures read as retail read them. |
| Audio / input | PARTIAL -- Live (audio) | Replays' effects found at their logged times in the legacy mixer's capture; the OpenAL mix is not captured; no input test, because simulated buttons need the user's permission per run. |
| Gameplay | PARTIAL -- Interop subset | The protocol client moves on every retail and fnTech3 server; combat and pickups seen only in recordings; no live play. |
| Optional services | Unavailable | The 1999 master and update servers are gone; `globalservers` sends its query and reports nothing it did not receive. |

Build with `-Dq3test-development=true` only for isolated probes. An ordinary
build refuses the profile. The option is independent of every other title's,
and no other profile's evidence is Q3Test's.
