# RTCW cast messaging and elementary actions

Status: **partial Source / Unit / Build / Fixture evidence**. SP and MP still
use the same experimental executable. SP campaign startup now reaches game
import 308, `BOTLIB_AAS_POINT_AREA_NUM`; campaign play and save/load remain open.

## Startup failure and correction

The earlier mission-briefing captures were not proof of successful cast
initialization. Their logs contain repeated `Server command overflow` drops
for soldiers. The previous boot harness did not classify that diagnostic as
an error. The harness now rejects reliable-command overflow/loss, as well as
unsupported imports and initialization errors. Earlier boot-pass labels must
be read with this limitation; their readable briefing captures remain valid
rendering observations.

The `cast-readiness-clock` probe established that server time advanced from
650 to 10,350 milliseconds while the briefing remained at `Please Wait`.
The original source excludes cast AI from broadcast messages, and SP also
excludes them from configstring updates. Queuing those unconsumed commands
in fnTech3 exhausted their reliable windows and dropped the AI clients.

The shared server now classifies cast AI through the checked game-entity
adapter. RTCW broadcasts exclude cast AI, while direct commands and ordinary
bots keep their existing delivery. SP configstring updates exclude cast AI;
MP applies that rule only to its single-player game type. Deferred updates
follow the same policy. Q3/QL never interpret their flags as RTCW cast flags.
The optional `status debug` report exposes server/host clocks, residual time
and client/entity capacities without changing normal `status` output.

After this correction, the installed SP game advances first to import 424
(`BOTLIB_EA_VIEW`), then to import 211 (`BOTLIB_USER_COMMAND`), and now to
import 308. These runs contain no server-command overflows. They do not yet
reach the game's `rockandroll` readiness command or a playable scene.

An isolated launch of the installed original `WolfSP.exe` exited with code 5
before writing an engine log. This supplies no original-runtime readiness
comparison. Its command, executable/module hashes and unchanged-installation
check are retained in `original-sp-readiness/manifest.json`.

## Action boundary

All 28 RTCW elementary-action imports, 400–427, have explicit argument
handling. The 40-byte input record has independently frozen fields and flag
conversion. The engine retains canonical action flags; reserved external
bits are stored separately so reset/get round trips preserve them. No Quake
bot export structure is expanded.

RTCW jump/up and crouch/down aliases are preserved. Its previous-jump marker
is also its left-movement bit, 128; jump gating and reset reproduce that
observed behavior only for RTCW. Q3/QL retain their distinct flags. Reset with
a record restores its complete state; reset without one clears transient
movement and preserves view/weapon state. Movement clamps speed to ±400;
restoring a record does not clamp its speed. The original end-regular action
is an unconditional no-op after the boundary's argument validation.

The boundary checks the initialized allocation and profile client limits,
integer width, finite scalars/vectors, complete QVM spans, required native
pointers, and string termination. It stages records before changing action
state and validates get-input output storage before updating think time.
Invalid arguments do not publish output or action-state changes.

Import 211 copies SP/MP's distinct command records through the existing
converters before invoking the real `SV_ClientThink`. It checks both server
and profile bounds and supports unaligned native/QVM records. Inactive
clients retain the original last-command update without a game callback;
active clients receive exactly one think callback. These are AI-generated
game commands, unrelated to operating-system input injection.

## Evidence

All retained evidence is under `.tmp/rtcw-manager/`:

- `actions-reference-{original,iortcw}-{sp,mp}-{x64,x86,linux32}.json`:
  344,099 comparisons per reference/mode/target, using unchanged published
  functions. Linux x86 includes ASan, UBSan and leak detection. The cases cover
  all 16,384 defined flag combinations with reserved bits, action composition,
  reset/get, jump history, speed/view/weapon state and command text.
- `actions-layout-{x64,x86}.json`: all four reference-header probes pass,
  with 2,121 SP and 2,111 MP static assertions, including the new input record
  and action flags.
- `cast-actions-final-{x64,x86,sanitizers}.log`: nine focused tests pass in
  11.309, 23.048 and 66.621 seconds. They execute the production functions and
  real entity/command adapters. Coverage includes sustained unacknowledged
  cast traffic, ordinary clients/bots, direct messages, download disconnects,
  deferred/chunked configstrings, malformed arguments, partial initialization,
  shutdown and other-profile isolation.
- `cast-actions-dev-build.log` and `cast-actions-default-build.log`: Windows
  x86 client/server builds pass with GLX, Vulkan and RTX renderer targets.
- `cast-actions-full-suite.log` and `cast-actions-testlog.txt`: 111 Meson
  checks pass, two skip; the recursive Python suite passes 1,403 tests in
  331.573 seconds, with 11 skips and no failures.
- `cast-actions-map/manifest.json`: the experimental executable stops SP at
  unsupported import 308; MP passes local pure admission and exits cleanly.
  Its inspected engine PNG shows the deployment UI and briefing movie, with
  the game world obscured. No SP map capture is claimed.
- `cast-actions-live/manifest.json`: all six Q3/QL GLX, Vulkan and RTX raster
  checks pass. Personally inspected engine PNGs show Q3's world/weapon/HUD
  and QL's native join UI/world. QL was not joined; image parity is not claimed.

The development executable is SHA-256
`e9953929a434d9b0e4cd5ace342d60fdf7127a2b8daea2ab1f0cf7419c9ec744`;
the default executable is
`20a1342c432a52a386af56d19cb9d4ec54954d4b356bd6e57b3342af4464fcef`.

Runtime probes are windowed on unswitched private desktops, with input
disabled and audio muted. Captures use the engine render target. Isolated
writable homes are removed and the RTCW installation remains unchanged.
`cast-actions-source-identities.json` pins production, test, reference,
executable and evidence identities for this checkpoint.

## Remaining work and provenance

The next observed SP gap is the AAS point-area query. Further navigation,
cast lifecycle policies, campaign readiness, save/load, entity-stream audio,
letterbox rendering, broader renderer parity and peer/demo interoperability
remain open. The [compatibility ledger](COMPATIBILITY.md) keeps the shared
release and gameplay gates open. Missing SP sound resources and shader
keyword warnings also require investigation.

Behavior observations come from the original [SP server and bot library](https://github.com/id-Software/RTCW-SP/tree/master/src),
the [MP source](https://github.com/id-Software/RTCW-MP/tree/master/src), and
[iortcw](https://github.com/iortcw/iortcw). The new policy, adapters and tests
are independently authored. GPLv3 reference functions and their notices are
used only in disposable comparison fixtures; no reference implementation text
or retail modules/assets are incorporated into production.

No new unrelated issue was found. The previously recorded inherited Q3/QL
routing-cache reader bounds audit remains open.
