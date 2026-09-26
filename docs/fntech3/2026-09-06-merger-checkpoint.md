# 2026-09-06 merger checkpoint

**Evidence checkpoint, not full compatibility sign-off.** This records the
observations available during the resumed merger session on 2026-09-06. The
working tree was dirty at HEAD
`e3cfc5914cfbd8eaeea7134fe102e8338778dfdc`. That commit alone does not reproduce
the tested binaries. The initial observations preceded further Q3
pure-negotiation, profile, and VM float-boundary fixes. A later section records
successful Q3 probes after those fixes. Final cross-profile validation and
artifact staging were still pending at the initial draft. Subsequent sections
record the packaged runtime and final verification results separately.

Neither profile has passed the complete compatibility contract. The
[compatibility ledger](COMPATIBILITY.md) owns gate status. The observations
below support particular Build, Unit, Fixture, and limited Live evidence;
they do not establish retail interoperability or release readiness.

## Evidence provenance and binary identity

Local manifests and logs are disposable investigation outputs under `.tmp/`.
They are intentionally not committed, and no retail modules, assets,
credentials, or screenshot pixels are incorporated into this document. Each
manifest retains the exact executable path, argument vector, installation,
isolated writable home path, and result. These paths may be rebuilt or removed
later: the SHA-256 recorded at execution time is the relevant identity.

| Observation set | Executable at execution time | Recorded executable SHA-256 |
| --- | --- | --- |
| Four dedicated profile probes | `.tmp/meson-msvc-x86/fntech3.ded.exe` | `b00020f7151f8ea788eab9300df78cf06fed51b9809e23d8f580ab9f15951724` |
| QL GLx client smoke | `.tmp/meson-msvc-x86-release/fntech3.exe` | `cc01eae85b076fc1582500b21ac54cf6abf8e7ecb9a760afcd2775c0155daf2c` |
| QL Vulkan and RTX-raster client smokes | `.tmp/meson-msvc-x86-release/fntech3.exe` | `489ca70ed1f53abbe1223e468115c8c1cfcaa5aa7779d25acd882e6195edf147` |
| Later Q3 GLx and Vulkan pure-session smokes | `.tmp/meson-msvc-x86-release/fntech3.exe` | `ff8da716e1e5f7f5d369b4aba455350f36b0344cb336d9e9fb573106af0af30f` |

Sources: dedicated manifest (`.tmp/profile-probe-boundaries/manifest.json`),
QL GLx manifest (`.tmp/client-ql-glx-release/manifest.json`),
QL Vulkan manifest (`.tmp/client-ql-vk-release/manifest.json`), and
QL RTX manifest (`.tmp/client-ql-rtx-release/manifest.json`).

All runs were on Windows x86 engine binaries. The release build configuration
records MSVC, `buildtype=release`, `strict-warnings=true`,
`b_vscrt=static_from_buildtype`, `default_library=static`, dynamically loaded
`glx,vk,rtx` renderer modules, and GLx as default. See the
release setup log (`.tmp/resume-release-setup.log`). The dedicated binary
above came from the earlier build directory, not that final release build.

The initial client manifests do **not** record renderer-DLL hashes. This limits their
use as exact packaged-renderer evidence. Do not substitute hashes of later
files at the same paths. Final evidence must capture the client, dedicated
server, renderer DLLs, and sidecars from the actual staged distribution.

## Shared dedicated-binary probes

The same recorded dedicated executable passed all four probes below. Each
used a fresh temporary home path, a loopback-only address and ephemeral port,
`dedicated=1`, `sv_pure=1`, Steam integration disabled, Workshop skipped, demo
autorecord disabled, and `r_fullscreen=0`. The command queued a map, waited
300 frames, and quit; no user input was injected.

| Profile / selection | Map | Module observed | Connectionless result | Exit / elapsed |
| --- | --- | --- | --- | --- |
| Q3 / `auto` | `q3dm6` | `vm/qagame.qvm`, compiled and loaded; botlib initialized | Protocol 68, requested map, `pure=1`, zero clients | 0 / 15.531 s |
| Q3 / explicit `q3` | `q3dm6` | `vm/qagame.qvm`, compiled and loaded; botlib initialized | Protocol 68, requested map, `pure=1`, zero clients | 0 / 15.266 s |
| QL / `auto` | `campgrounds`, factory `ffa` | Retail `qagamex86.dll`, API 10; botlib initialized | Protocol 91, requested map, `pure=1`, zero clients | 0 / 7.812 s |
| QL / explicit `ql` | `campgrounds`, factory `ffa` | Retail `qagamex86.dll`, API 10; botlib initialized | Protocol 91, requested map, `pure=1`, zero clients | 0 / 7.875 s |

The console logs record game shutdown and AAS shutdown for all four runs.
Evidence: Q3 automatic (`.tmp/profile-probe-boundaries/q3-auto-console.log`),
Q3 explicit (`.tmp/profile-probe-boundaries/q3-q3-console.log`),
QL automatic (`.tmp/profile-probe-boundaries/ql-auto-console.log`), and
QL explicit (`.tmp/profile-probe-boundaries/ql-ql-console.log`).

Installation identities recorded by the manifest:

| Profile | Legitimate local installation | Base-pak SHA-256 |
| --- | --- | --- |
| Q3 | `C:\Program Files (x86)\Steam\steamapps\common\Quake 3 Arena` | `7ce8b3910620cd50a09e4f1100f426e8c6180f68895d589f80e6bd95af54bcae` |
| QL | `C:\Program Files (x86)\Steam\steamapps\common\Quake Live` | `7d5fd8d4786f9e140804b7279ce2589c8a7babdcf2886763a63a992ad9ef634b` |

These are map/module startup and connectionless-query observations. There were
no admitted clients in these probes. A reported `pure=1` does not prove pure
checksums, client admission, delta exchange, downloads, or bidirectional retail
interoperability. The final dedicated binary needs repeat probes after the
remaining compatibility fixes.

## QL client renderer smokes

All three QL runs selected profile `ql`, used the legitimate Quake Live
installation, and launched `devmap campgrounds ffa` with a fresh writable home.
The command requested a 640 x 480 window and disabled mouse capture, joystick,
audio, WebUI presentation, Steam integration, Workshop, and downloads. It
queued `screenshotPNG profile-smoke`, waited three more frames, then quit.
Exact command vectors are in the linked client manifests above.

Each console log records native qagame API 10, cgame API 8, and UI API 8 exports,
a transition from `CS_PRIMED` to `CS_ACTIVE`, the engine writing
`screenshots/profile-smoke.png`, and client/server shutdown. Each manifest
records exit code 0 and an output screenshot. The copied PNGs have 640 x 480
IHDR dimensions. This documents engine render-target output and local-session
startup; no OS screen capture was used.

| Renderer | Observed device/API | Scope of observation | Local console log |
| --- | --- | --- | --- |
| GLx | NVIDIA GeForce RTX 4060 Laptop GPU; OpenGL 4.6.0, NVIDIA 591.74 | GL46 renderer bootstrap, active local session, PNG, clean exit | GLx log (`.tmp/client-ql-glx-release/console.log`) |
| Vulkan | Same GPU, device `0x28a0`; Vulkan API 1.4.325, driver 591.74.0.0 | Vulkan raster path, active local session, PNG, clean exit | Vulkan log (`.tmp/client-ql-vk-release/console.log`) |
| RTX | Same Vulkan device/API/driver | **Raster path only.** Capability gate reports `requested=disabled (0), active=disabled (0), require=0`; ray tracing was not exercised | RTX log (`.tmp/client-ql-rtx-release/console.log`) |

Screenshot fingerprints identify the locally retained evidence; the images are
not distributed with the repository:

| Local file | SHA-256 |
| --- | --- |
| `.tmp/client-ql-glx-release/profile-smoke.png` | `260b96b70ba9d818757ac8a2a2ff5b6e93190934e99af1e0e3a917aef72ae683` |
| `.tmp/client-ql-vk-release/profile-smoke.png` | `2aee9cb539be41ce036284f2d29c3320ef6a9a5e146692584ddbfeede7298fdc` |
| `.tmp/client-ql-rtx-release/profile-smoke.png` | `f2244e4158f5bf4d2f6051473c3dabc7314e4e163e53e850c2378cb97b4f3a1e` |

No quantitative visual-parity, performance, HDR, ray-tracing, restart-stress,
audio, or WebUI gate is closed by these smokes. The console logs include
nonfatal conflicting initial-value warnings for WebUI resource-bridge cvars;
WebUI presentation was disabled, so this path still requires separate work.

The contemporaneous Q3 client attempt in
the Q3 GLx manifest (`.tmp/client-q3-glx-final/manifest.json`) used client
hash `489ca70ed1f53abbe1223e468115c8c1cfcaa5aa7779d25acd882e6195edf147`,
timed out, and produced no screenshot. Earlier Q3 logs also record repeated
dropped gamestates. Despite the investigation directory's `-final` suffix,
this is failed intermediate evidence, not a final Q3 client result. The later
Q3 probes below followed profile/framing/pure and VM float-boundary fixes.

## Later Q3 pure-session probes

The later GLx manifest (`.tmp/client-q3-glx-pure/manifest.json`) and
Vulkan manifest (`.tmp/client-q3-vk-pure/manifest.json`) record the same
rebuilt client SHA-256
`ff8da716e1e5f7f5d369b4aba455350f36b0344cb336d9e9fb573106af0af30f`.
Both selected `q3`, used the legitimate Quake III installation with fresh
writable homes, and launched `devmap q3dm6` with the same window, input,
audio, WebUI, Steam, and download constraints as the QL probes. Both exited 0.

The GLx log (`.tmp/client-q3-glx-pure/console.log`) and
Vulkan log (`.tmp/client-q3-vk-pure/console.log`) record fallback from
unavailable native DLLs to the installed `qagame.qvm`, `ui.qvm`, and
`cgame.qvm`, followed by successful compilation and initialization. After the
client's Q3 `cp` checksum command, both sessions advance from `CS_PRIMED` to
`CS_ACTIVE`, announce player admission, write the requested PNG, and shut down
the game, AAS, client, and renderer. This is observed local pure-session
admission, with no inference of remote or retail bidirectional interoperability.

| Renderer / local PNG | Dimensions | SHA-256 |
| --- | --- | --- |
| GLx / `.tmp/client-q3-glx-pure/profile-smoke.png` | 640 x 480 | `8da429efe64e692e0ad565e13e27c7aa8fdb995eb59e0ac17ef27ff89675b784` |
| Vulkan / `.tmp/client-q3-vk-pure/profile-smoke.png` | 640 x 480 | `edbd06e89f5d339ffb86bda3e0a38b4d802ee934c188f3d0ab1fbba45deb050b` |

The logs still show nonfatal missing-image/model warnings and QL-named font
fallback diagnostics under Q3. These successful startup probes do not resolve
visual parity or profile-specific resource cleanup. QL reruns against this
same executable and the other final artifact checks were pending at this update.

## Source and automated-test checkpoint

The resumed work integrates frozen Q3/QL ABI layouts and adapters, explicit
server-memory views, and profile-selected usercmd/entity/player-state codecs.
It adds executable regression coverage for native-loader rollback, QVM bounds,
Q3 cgame usercmd and extension traps, UI API/cvar-flag policy, malformed usercmd
batches, profile config selection, and Q3 viewport-FOV policy. These source
changes are not all represented by every binary hash in the observation table.
See [credits](../../CREDITS.md) for the pinned FnQuake3/FnQL lineage and the
independently implemented behavior informed by those references.

Project identity now comes from `version/fntech3_version.h`; client/dedicated
outputs and release archive prefixes use fnTech3 naming. Existing renderer,
provider, and content-sidecar names remain explicit loader contracts. Legacy
metadata imports and environment aliases remain available. Version stamping,
build/launch helpers, and generated landing documents use the canonical
metadata.

| Dated checkpoint | Observed result | Limitation |
| --- | --- | --- |
| 2026-09-06 full pytest with built `FNQL_AUDIOZONESC` | **1210 passed, 3 skipped, 25,481 subtests; 59.08 s** | Pre-final source checkpoint. Two opt-in cgame runtime probes skipped; one Windows symlink-privilege test skipped. Local log (`.tmp/resume-pytest-final.log`) |
| Later 2026-09-06 full pytest after pure/profile/float fixes, with built `FNQL_AUDIOZONESC` | **1212 passed, 3 skipped, 25,481 subtests; 63.71 s** | Settled-source Python-suite checkpoint. The same two opt-in cgame runtime probes and one Windows symlink-privilege test skipped. Final artifact/runtime validation remains separate. Local log (`.tmp/resume-pytest-settled.log`) |
| 2026-09-06 complete Meson test attempt | **100 passed, 1 failed, 1 skipped** | Audio authoring audit lacked its generated `.azb` fixture. Preserved run log (`.tmp/resume-meson-tests.log`) |
| Follow-up audio fixture/audit | Project-authored fixture generated; focused audit **1/1 passed** | `build_by_default: true` now creates the fixture during normal compilation. |
| Complete Meson rerun after fixture repair and pure/profile/float fixes | **101 passed, 0 failed, 1 skipped** | Audio loopback test returned its supported skip code 77. The Python aggregate also passed. Preserved run log (`.tmp/resume-final-meson-tests.log`) |

The static Windows dependency audit found a client importing `z-1.dll` in an
earlier debugoptimized build. Release CI already selected static zlib; the
canonical Meson fallback and Windows developer helpers now also select static
zlib, and the MSVC helper selects the static CRT. This is a build-policy fix;
final PE dependency verification belongs to the staged artifacts. No missing
fallback DLL was copied into a distribution to bypass the policy.

## Packaging and remaining gates

A dry run of the existing runtime-only install route selected exactly eight
project outputs: `fntech3.exe`, `fntech3.ded.exe`, `fnql-audiozonesc.exe`, the
three `fnql_<renderer>_x86.dll` modules, `FnQL-pkg.fnz`, and `fnql-web.pak`.
The command used `meson install --no-rebuild --skip-subprojects --tags runtime`
with an explicit `.install/windows-x86` destination. Import libraries,
subproject development files, and the proprietary provider were excluded.
This draft records a **dry run**, not completed deterministic packaging or
final-artifact validation.

The inherited public Windows release route still requires `fnql_steam.dll`.
Its [binary distribution notice](../fnql/STEAM_PROVIDER_BINARY_NOTICE.txt)
authorizes official FnQL Windows releases; permission for a fnTech3
distribution has not been established here. Keep that route separate from
engine-only local staging until the permission/provenance issue is resolved.
Unsupported macOS packaging helpers also retain legacy FnQL identities and
have not been revalidated for the merger.

Required work beyond this checkpoint remains:

- Broaden the complete-suite and exact-artifact observations recorded below
  to a clean source checkout and the supported platform/build matrix.
- Extend the Q3/QL client/module/renderer/pure admission observations with
  broader malformed inputs and shutdown/fallback behavior.
  The QL native snapshot's additional `0x58` bytes remain unresolved.
- Complete bidirectional reference/retail client-server interoperability,
  supported protocol/demo formats, pure/download flows, and long/faulted
  sessions. Local devmap and connectionless probes are narrower observations.
- Complete WebUI presentation/lifecycle/resize/restart and renderer-switch
  stress, screenshot parity, audio, HDR, and enabled ray-tracing gates.
- Validate real Steam authentication and offline/failure behavior, Workshop,
  statistics, and service shutdown without synthesizing service success.
- Validate installation detection beyond marker existence, Q3 Steam discovery,
  separate profile homes/configs/caches, deterministic packaging, and the
  supported platform matrix. Resolve provider permission and legacy packaging
  identity issues before any broader release claim.

## Same-client cross-profile observations

After the pure/profile/float fixes, all six profile/renderer runs used client
SHA-256 `ff8da716e1e5f7f5d369b4aba455350f36b0344cb336d9e9fb573106af0af30f`.
Each reached `CS_ACTIVE` following its profile-specific pure command, wrote
an engine PNG, and exited 0. The Q3 GLx/Vulkan images show the world and HUD;
the QL images show the native join menu over the world. No user input was
controlled, and these are local sessions without service authentication.

Evidence is under `.tmp/client-{q3,ql}-{glx,vk,rtx}-pure/`, with an exact
argument vector and temporary home path in each manifest. The Q3 installation
contains fifteen paks, including local additions; this is legitimate-fixture
evidence and must not be described as a clean retail installation.

The Q3 RTX raster PNG contains large black polygons across the right wall and
foreground. This is an observed visual defect despite successful admission
and shutdown. The GLx/Vulkan shots use different random spawn positions, so
they do not establish an equivalent-scene comparison or isolate the cause.
RTX reports active ray tracing disabled in both profiles. A renderer-parity
gate remains open pending an equivalent demo/frame comparison.

QL logs retain `Hunk_FreeTempMemory: not the final block` diagnostics and mixed
image-flag warnings. Q3 retains missing-resource/font fallback diagnostics.
These observations are recorded independently of the successful session exits.

## Engine-only staging and dedicated revalidation

The runtime-tag Meson install was executed to `.install/windows-x86` with
`--no-rebuild --skip-subprojects --tags runtime`. It staged exactly the eight
project outputs listed above. All hashes matched the release build, all six
PE files were I386 and passed the static dependency policy, and the root
archive validated with 149 audio-zone sidecars, 149 fog sidecars, and one sound
shader. The proprietary provider, retail assets/modules, and development
dependency outputs were absent.

The staged manifest (`.tmp/staged-artifacts.json`) records the install
command, dirty HEAD, complete artifact hashes, and validation outcomes. The
same staged dedicated binary had SHA-256
`b5e6ce00bf49a9b0d36134f32583d839e80ea8aefab4dcc2ab2480493c5ccd26`.
All four packaged dedicated probes (`.tmp/profile-probe-packaged/manifest.json`)
passed with fresh homes: Q3 auto/explicit in 15.891/15.313 seconds and QL
auto/explicit in 7.844/7.938 seconds. Each returned the requested map,
protocol, and challenge, loaded its game module, and shut down cleanly.
These dedicated probes had no admitted clients; local client admission is
separate evidence above.

## Final config/demo correction and packaged runtime checkpoint

The final staged client includes two additional runtime-driven corrections: Q3
writes `baseq3/q3config.cfg` instead of the QL split files, and protocol-91
recordings always keep their `.dm_91` suffix. Executable regressions preserve
QL config-save retry behavior and Q3 protocol-68/71 demo fallback behavior.

Final artifact SHA-256 values (all eight matched the build and remained
unchanged across the six packaged client runs):

| Artifact | SHA-256 |
| --- | --- |
| `fnql-audiozonesc.exe` | `8609d6d6e912d382b4425305915a4e1963bfd8e04438eb9389088827011624f2` |
| `FnQL-pkg.fnz` | `1d1ff86488c3134a936486790c24954bef87abd22177e1ca8b5542cc2f06115a` |
| `fnql-web.pak` | `6c210165880fff668900fd3e4e3c90137623043e1b5315f24b693f367f756382` |
| `fnql_glx_x86.dll` | `353dbb51b178cabe3ff0112c365c1a6df80ae7c860a7e3fc6789cef8c00088a0` |
| `fnql_rtx_x86.dll` | `855cdb3aad1caacf0c8bc3200ce66dc2547bfcdffd1417d8cd749ff2cf09f0b5` |
| `fnql_vk_x86.dll` | `771f8c18c481ac3798e709467de26bf93071117c86d53bf361a0e28b58e83cff` |
| `fntech3.ded.exe` | `ac59be52db0d01c44ce2a4e50bb501c4b5f88d18fbd2ecfaa29b3c8bfd76b2a6` |
| `fntech3.exe` | `7f2e388947ec00ee6f9fae08450a84c3e00717f78689a765201dcf2f5644eddd` |

The dependency and root-archive audits pass for this set. The final
dedicated manifest (`.tmp/profile-probe-config-final/manifest.json`)
records four more successful runs: Q3 auto/explicit in 15.782/15.281 seconds;
QL auto/explicit in 7.859/7.891 seconds. All exited 0 and advertised the
expected map, protocol, and challenge.

The packaged client summary (`.tmp/packaged-runtime-summary.json`)
records all six Q3/QL by GLx/Vulkan/RTX combinations. Each manifest now
includes all loaded engine/renderer/sidecar hashes, the base-pak hash, the
exact command, and its isolated home. All reach `CS_ACTIVE` after pure
admission, write a 640 x 480 engine PNG, write the correct profile config
paths, and quit normally. Q3 RTX still exhibits the recorded visual failure;
these are successful startup checks, not six visual-parity passes.

| Profile / renderer | Engine PNG SHA-256 |
| --- | --- |
| q3 / glx | `f73bc2f5bedda28909b86e0e65a96800166086d095150fca3ca154bdd5243d3c` |
| q3 / vk | `cdd6ced5a48d39e924dd626998cd33100b18594cbc0ee75c50efb1b26d7db13a` |
| q3 / rtx | `56e462336b97c4a52c35536f0c9366e868197dceb87dd5d6fcba8bb5a270e561` |
| ql / glx | `f3585adb630a649b693650fcdbc44736ec5c34f3d60f93b9a6d6fb325f8e8b6e` |
| ql / vk | `2aee9cb539be41ce036284f2d29c3320ef6a9a5e146692584ddbfeede7298fdc` |
| ql / rtx | `f2244e4158f5bf4d2f6051473c3dabc7314e4e163e53e850c2378cb97b4f3a1e` |

### Final demo and config probes

The Q3 demo manifest (`.tmp/demo-q3-staged-final-config/manifest.json`)
and QL demo manifest (`.tmp/demo-ql-staged-final-config/manifest.json`)
use the final staged client hash above. Both profiles record a short local
session and play the resulting demo in another process to normal
`CL_NextDemo` completion. All four processes exit 0, with clean shutdown,
correct message envelopes and terminal `(-1, -1)`, and no truncation warning.
Q3 writes only `baseq3/q3config.cfg`; QL writes home-root `qzconfig.cfg` and
`repconfig.cfg` in both phases. Broader formats, seeking, and malformed-demo
stress remain open.

### Renderer failure isolation

A frozen Q3 demo frame reproduces the black polygons with RTX while Vulkan
renders the same view cleanly. The artifacts persist with each of `r_vbo=0`,
`cg_drawGun=0`, `cg_shadows=0`, `r_dynamiclight=0`, `r_depthFade=0`, and
`r_fastsky=1`. All eight probes exited 0 and used engine screenshots; ray
tracing remained disabled. Evidence is under `.tmp/renderer-demo-*-frozen*/`.
The inspected RTX backend/shader/Vulkan/sky implementation text matches the
pinned FnQL baseline after newline normalization. This narrows the affected
code history but does not prove retail or original-FnQL runtime behavior.
No speculative renderer change was made. Isolating the offending draw batch
or material is the next useful renderer investigation.

### Retail Q3 interoperability blocker

Both directions were prepared with legitimate retail `quake3.exe` and base
paks copied into isolated temporary fixtures outside the repository. Processes
used a private Windows desktop, never switched to the user input desktop,
with windowed and input-disabled settings. The retail executable failed
before engine logging or network startup: its error text was `Failed to find
Steam`. No interop is claimed. The
local blocker summary (`.tmp/q3-interop-result.json`) retains fixture,
process, and diagnostic provenance. All probe processes were stopped.

### Final automated verification

The complete Meson suite after the config/demo corrections passed **101
tests, 0 failed, 1 skipped**. The skipped OpenAL loopback test reported
`ALC_SOFT_loopback` unavailable. Its Python unittest aggregate ran 1,210
tests with three supported skips. See the
final Meson log (`.tmp/resume-config-demo-meson-tests.log`). This was a
fresh release build configuration followed by incremental fixes, on the
dirty source state identified above; it is not a clean-checkout release
reproducibility claim.

The final full Python suite separately passed **1,214 tests and 25,483
subtests, with 3 skips**, in 102.43 seconds. The skips are two opt-in graphical
cgame harness probes and a Windows symlink-privilege case. The direct runtime
probes above provide separate module observations; they do not relabel the
skipped harness checks as passed. See the
final pytest log (`.tmp/resume-pytest-delivery.log`). `git diff --check`
also passed.

### Local delivery artifact

The engine-only development package is `.install/windows-x86`, with the eight
verified runtime outputs, license/credits/notices, documentation, and a compact
artifact hash manifest. Its ZIP is
`.install/fntech3-0.1.0-windows-x86-dev-20260906.zip`. Repeated ZIP creation from
the same stage is checked for identical bytes. This establishes deterministic
packaging of this staged set, not reproducible compilation or a public release.
No proprietary provider, retail game module/asset, runtime screenshot, demo,
or credential is included.
