# RTCW client animation checkpoint — 2026-09-15

The shared Windows x86 development executable now completes the installed MP
cgame's initialization. It remains unable to enter the map: pure verification
still selects Quake Live's native-module pak name. SP remains blocked by game
import 206 (`BOTLIB_LOAD_MAP`). These are development observations, not a
compatibility or gameplay claim.

The subsequent [MP pure-admission checkpoint](2026-09-15-rtcw-mp-pure.md)
resolves the module-pak selection failure and records the next runtime blocker.

## Implementation

The tag import reads RTCW's model/frame fields separately from a submitted
scene entity. Original `CG_CalcMoveSpeeds` initializes only the fields required
for a foot-tag query. Scene-only enum, boolean and material fields therefore
must not be interpreted by this import. Normal scene submission retains its
existing checks.

MDS tag evaluation now computes the requested bone and its ancestors. Torso
frames, interpolation and axes are required only where that calculation uses
them. Whole-mesh evaluation still validates the complete pose. Failed queries
leave the destination unchanged. This independently authored change uses
original/iortcw contract observations, not their implementation text.

SP cgame import 89 and MP import 88 now send animation speeds to the local game
module. A distinct engine semantic call maps to game export 13 in both modes;
QL's coincident export 13 remains a separate operation. The bridge checks the
signed entity value, bounds the source string to the original 10,000-byte
buffer, and passes a local copy through the normal VM call path. No local game
means no callback. The target must be a legacy x86 game module; QVM and
structured-native targets never receive the engine pointer. The game module
retains ownership of parsing and applying animation speeds.

MP's loop-clear import now discards the preceding frame's looping submissions
through the existing audio service. It ignores the legacy kill word, matching
the original MP dispatcher. Its stop-loop import intentionally does nothing,
also matching that source. SP's additional channel-clear modes and the richer
RTCW loop submission contract remain open.

## Evidence

`scripts/probe_rtcw_mds_pose.py` passes against original SP, original MP,
iortcw SP and iortcw MP on Windows x64, Windows x86 and Linux x86 with
ASan/UBSan. Each reference covers all 34 installed MDS files, 5,727,837 bone
poses, 1,114,279 weighted vertices, 21,545 normal tags and 7,745 partial foot-tag
queries. Invalid unused torso fields are exercised by fnTech3; the reference
fixture uses valid unused frame indices to avoid undefined pointer arithmetic
in the original code. Reports are
`.tmp/rtcw-manager/partial-tags-reference-{x64,x86,sanitizers}.json`.

The seven focused renderer/tag tests pass on Windows x64/x86 and sanitized
Linux x86. The nine handoff/export/client-state/music tests pass in 7.182
seconds on Windows x64, 7.166 seconds on Windows x86 and 42.225 seconds on
sanitized Linux x86. They cover minimal import arrays, native and exact QVM
source extents, missing terminators, maximum payloads, nested callbacks,
callee writes, unavailable servers, incompatible targets and Q3/QL isolation.
Logs are `partial-tags-*` and `animation-focused-*` / `animation-sanitizers.log`
under `.tmp/rtcw-manager/`.

The complete default Windows x86 release suite passes 111 Meson checks with
zero failures and two skips. Its Python suite passes 1,364 tests in 338.354
seconds with 11 skips (`animation-full-tests.log`,
`animation-full-testlog.txt`). The default client SHA-256 is
`cc2d5ac1dc77544ee5eadc796467530be1aad75ea21c8eb1e511134dd22f0a5b`.
Q3 and QL pass GLX, Vulkan and RTX raster scene probes; all six engine captures
were inspected and audited in `animation-live/manifest.json`. Q3 shows the
world, weapon and HUD. QL shows the native join UI over the world; no match was
joined. `git diff --check` also passes.

The bounded MP map probe in `animation-clear-map/manifest.json` records
`CL_InitCGame: 0.49 seconds`, followed by the fatal diagnostic
`CL_SendPureChecksums: no pak file for cgamex86.dll`. Its harness reached the
45-second timeout during fatal shutdown and forcibly closed the process. No
engine screenshot was produced. The executable SHA-256 is
`4d737a5c42a99b57af18b52de131d1dffa5f6594c22e73e86eac643837ba1952`.
The preceding SP/MP probe (`animation-map/manifest.json`, executable
`05573242a408975d71496a82291b0e80b9096d53b88610c05aff5af240c4941c`)
records SP's import-206 blocker and MP's then-unsupported loop-clear import.

Both probes used the legitimate Steam RTCW installation, windowed unswitched
private desktops, disabled input/audio and isolated writable homes. The homes
were removed and installation metadata remained unchanged. The same executable
contains both mode contracts; neither run establishes gameplay, audio output
parity or live interoperability. The recorded source commit is
`c75573c7375e44c908256020ada2d7e12970cacf` with an uncommitted worktree.

Original and iortcw reference functions are GPL-3.0-or-later and are compiled
only in disposable comparison fixtures with their notices. Production changes
are independently authored or extend the inherited GPL-2.0-or-later engine.
No retail modules or assets are incorporated.
