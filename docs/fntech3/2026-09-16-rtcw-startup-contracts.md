# RTCW startup queries, appearance and animation handoff

Status: **partial Source / Unit / Build / Fixture evidence, not playable support**.
The shared Windows x86 development executable now takes the installed SP
`escape1` map through both navigation-world initializations, server startup,
local client connection and animation model loading. The next observed failure
is cgame import 34, `CG_S_CLEARLOOPINGSOUNDS`. MP still opens its deployment
menu and briefing movie. Default RTCW startup remains gated.

This checkpoint uses dirty worktree commit
`c75573c7375e44c908256020ada2d7e12970cacf`; it is not a committed release.
Evidence is under `.tmp/rtcw-manager/`.

## Implemented contracts

The game manager now owns appearance defaults, userinfo keys, archive policy
and the `model` command's head-selection behavior:

| Profile | Body | Head key/default | Color keys | `model` also selects head |
| --- | --- | --- | --- | --- |
| Q3 / QL | `sarge` | `headmodel` / `sarge` | `color1=4`, `color2=5` | Yes |
| RTCW SP | `bj2` | `head` / `default` | `color=4` | No |
| RTCW MP | `multi` | `head` / `default` | `color=4` | No |

Q3/QL retain their team appearance keys and existing default-archive policy.
RTCW archives its body/head defaults and does not register Quake's team keys.
Registration preserves existing user selections. This fixes the observed SP
request for the nonexistent `models/players/sarge/wolfanim.cfg`.

RTCW imports 304 and 306 call the actual AAS initialized-state and clock
queries. They use the selected world's state and preserve the clock's exact
32-bit float return representation, including negative zero. Import 542 calls
the actual item updater. RTCW does not initialize Quake's goal-item
configuration: the updater expires any existing temporary items, then returns
without entity scans when that configuration is absent. Nonempty Quake goal
configuration and general RTCW item AI are not covered by this adapter.

SP cgame import 125 now requests animation metadata through game export 14.
The engine's semantic export has a separate internal identifier; it cannot
alias a Q3/QL export, and MP's absent export is rejected rather than reported
as a successful ignored call. The bridge requires legacy x86 native game and
cgame modules, validates client numbers against SP's 128-slot contract, and
copies a terminated model name of at most 63 bytes. An aligned local pointer
slot permits unaligned caller output and nested callbacks. The game-owned
record remains opaque, including its nested pointers: the engine neither
copies that structure nor retains or frees it. The two native modules must
agree on their own animation structure. No pointer is passed into a QVM.
An absent local game returns false without examining caller buffers.

SP import 56, `CG_RB_ZOMBIEFXADDNEWHIT`, follows the original renderer's
unconditional no-op, including its `(-1, NULL, NULL)` initialization request.
It does not inspect vectors or create renderer state. **iortcw enables this
effect; that different behavior is not implemented or claimed here.**

## Validation

Eleven focused tests pass on Windows x64 (11.476 seconds), Windows x86
(13.289 seconds), and Linux x86 with ASan, UBSan and leak detection (69.701
seconds). Logs are `startup-bridge-{x64,x86,sanitizers}.log`. The renderer-import
test separately passes on those platforms in 0.781, 1.354 and 5.814 seconds,
recorded in `zombie-import-{x64,x86,sanitizers}.log`.
The cgame-state fixture also passes on all three platforms (0.607, 1.241 and
5.208 seconds), exercising import 125's exact three arguments and return value;
see `model-dispatch-{x64,x86,sanitizers}.log`.

Coverage executes the production registration, AAS functions, item updater,
VM export translator and native handoff. It checks both worlds, exact float
bits, empty item configuration and timeout boundaries, preserved user choices,
output guards, nested callbacks, shared record identity, malformed/null
arguments, module API restrictions, absent servers and other-profile isolation.
Windows x64 explicitly rejects the native pointer handoff; positive native
handoff evidence comes from Windows and Linux x86.

Both Windows x86 client/server builds pass with GLX, Vulkan and RTX targets.
Client SHA-256 values are:

- Default: `581324b2196a778df45d88e362ec1545f83761a9d0f27083c4d4bbb76f6beb14`.
- Experimental: `cfb58a64309fc866ff4bb90757ac4a1f9504cde4a65c295f91973324e3fbe1cc`.

Build logs are `startup-default-build.log`, `model-info-dev-build.log` and
the final incremental `zombie-dev-build.log`.
The complete suite passes 111 Meson tests with two skips, including 1,390
Python tests in 371.298 seconds with 11 skips. Results are retained in
`startup-full-recheck.log` and `startup-full-recheck-testlog.txt`. The first
run exposed a missing service in the cgame test fixture; the corrected fixture
now checks the new dispatch rather than merely supplying a link stub.

`startup-bridge-map/manifest.json` records exact installed-module identities,
commands, renderer/executable hashes and outcomes. SP stops explicitly at the
sound-reset import and has no map capture. MP exits normally; its inspected
engine PNG shows the deployment menu and briefing movie with the world
obscured. Earlier observations remain in `navigation-state-map`,
`item-update-map`, `appearance-map` and `model-info-map`.

All six inspected Q3 `q3dm6` and QL `campgrounds` captures pass across GLX,
Vulkan and RTX. `startup-live/manifest.json` records module, renderer, asset and
executable identities and the visual audit. Q3 shows textured worlds, weapon
and HUD; QL shows the native join menu and world. Spawn/view differences are
not image-parity evidence. Probes were windowed on unswitched private desktops,
with input and audio disabled. Their isolated writable homes were removed;
installed RTCW files were unchanged. No input injection or OS capture was used.

## Remaining work and provenance

SP sound clearing differs from MP: mode 1 clears effects/streams while keeping
music; mode 2 also clears music and its queue. SP loop submission additionally
quantizes the current master fade into submitted volume. The original mixer
also applies that fade when it is below one, so both stages need comparison.
The original SP stop-loop dispatcher, like MP's, is deliberately inactive.
These contracts,
entity streaming, further navigation/entity/movement imports, campaign and
save/load behavior, active GPU fog and peer/demo interoperability remain open.
The [compatibility ledger](COMPATIBILITY.md) retains the broader gates.

Source inspection used original [SP client](https://github.com/id-Software/RTCW-SP/tree/master/src/client),
[SP game](https://github.com/id-Software/RTCW-SP/tree/master/src/game),
[SP renderer](https://github.com/id-Software/RTCW-SP/tree/master/src/renderer),
[MP client](https://github.com/id-Software/RTCW-MP/tree/master/src/client),
the original SP/MP bot libraries and [iortcw](https://github.com/iortcw/iortcw).
`startup-source-identities.json` pins the inspected and implemented files.
These are behavior observations; the new adapters and fixtures are independently
authored. No GPLv3 implementation text or retail modules/assets are incorporated.

No new unrelated issue was found. The previously recorded inherited Q3/QL RCD
reader audit remains open; RTCW uses its separate checked reader.
