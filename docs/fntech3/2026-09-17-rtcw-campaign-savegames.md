# RTCW official-map and save/load checkpoint

Existing work was committed as `1223898` before this investigation. This
checkpoint fixes failures found while launching the installed official RTCW
maps individually. RTCW remains behind `-Drtcw-development=true`. These are
bounded runtime results, not full campaign or engine compatibility evidence.
The server/state implementation is committed as `8370125`; renderer metadata
and its fixtures are committed as `3d6bf4a`. Concurrent README/logo work is
checkpointed separately from these compatibility changes.

## Fixes

- `loadgame` selects a bounded SP save name, checks its version/map/time, and
  stages an independent copy as `save/current.svg`. Short reads/writes cannot
  replace the previous current save. Traversal, absolute paths, command
  delimiters, unsupported versions and truncated headers are rejected. The
  installed module still owns serialization and validation of its private payload.
- Fresh loads and same-map restarts restore the **level** clock before game
  initialization. The separate network clock retains its policy. Local SP
  snapshots and user commands accept the rewind; other profiles keep their
  checks. Initializing the bot scheduler before settling frames fixes the
  native divide-by-zero seen on repeated `escape2` loads.
- SP binds the fixed 1,024-entity module arena separately from the active count.
  The original loader links restored entities before publishing its final count.
  Alignment, bounds and generation checks remain. The capacity expansion does
  not apply to Q3, QL or RTCW MP.
- Empty fog metadata made the native module write a zero string length followed
  by `none`, shifting its save stream. Initializing the SP fog cvars to the
  original numeric default fixes the observed `baseout` corruption.
- The native module restores the saved numeric `g_episode`, then overwrites it
  with `(null)` from an unused entity field. The SP game-cvar boundary ignores
  that exact invalid assignment. Numeric changes, other cvars and other profiles
  keep their behavior. A subsequent save retains the episode. Saves that already
  contain zero cannot recover their lost metadata.
- Manual cinematic saves can persist a raw native `cameraPortal` pointer.
  Both fnTech3 and retail WolfSP crashed loading affected `boss1`/`dam` saves in
  another process. Manual saving during an active camera is now refused with
  an explanation. Normal gameplay saves remain available. Dam verifies refusal
  during the camera and successful saving/loading after the camera finishes.
- The state adapter omitted `TR_GRAVITY_PAUSED`, shifting acceleration and
  deceleration and rejecting external trajectory 11. Dam crashed during its
  opening scripted sequence. All twelve trajectory values now have distinct,
  checked mappings; Dam completes that sequence. The contract probe checks
  ordinals against original and iortcw SP/MP headers, alongside round-trip and
  transactional-rejection tests.
- GL, Vulkan and RTX retain RTCW `sunshader`, `lightgridmulamb` and
  `lightgridmuldir` metadata. World selection reapplies cached metadata and
  resolves the sun material after parsing. Corrected captures remove the
  fallback checkerboard skies in `dam`, `dark`, `end` and `village2`.
  Q3/QL shader interpretation remains profile-isolated.
- MP map commands initialize the match state, and restart commands implement
  its state transitions, immediate default and `sv_serverRestarting` lifecycle.
  Exhaustive transition checks cover locked/unlocked teams and invalid states.
  All 15 MP maps reach the playing state after a native warmup restart, with
  advancing snapshot and acknowledged command times. The passive probe leaves
  the command buffer free between observations; long scripted waits previously
  delayed the native game's appended restart and invalidated the input test.

An unrelated render-import fixture defect retained synthetic native-memory
ranges after freeing buffers. Ownership now removes those registrations before
freeing memory, eliminating a source of intermittent fixture failures.

## Method and identity

The legitimate installation was
`E:\SteamLibrary\steamapps\common\Return to Castle Wolfenstein`. Every map
used a separate process, isolated writable home, private unswitched Windows
desktop, `r_fullscreen 0`, and 800x600 window. Installation inventories remained
unchanged. GL engine `screenshotPNG` captures were inspected; no OS screen
capture was used. Audio was focus-muted.

SP map startup uses the private hook built by
`scripts/prepare_rtcw_campaign_probe.py`. It clicks only the briefing Ready
button under the repository's standing permission. The builder restores the
original source and rebuilds the ordinary client after copying its private
executable. Fresh-process loads and initial MP captures use ordinary clients.
The later MP match probe uses the private frame observer, without movement or
attack input, to verify clocks after warmup.
`CloseLimboMenu` and `-scores` expose the MP world through native console
commands. Installed arena metadata selects checkpoint mode 7 for
`mp_destruction` and `mp_trenchtoast`, and objective mode 5 for the other maps.
Official MP does not ship AAS/bot data.

The final ordinary Windows x86 development executable is preserved locally as
`.tmp/rtcw-campaign/client-v9/bin/fntech3.exe`, SHA-256
`2345473f3eae830a86b68fdd03a7a8a98a977587a40ea261de7bb130d1f82227`.
`client-v9/binary.json` records renderer/package identities. Earlier private
probe binaries have their own hashes and source/hook fingerprints. Manifests
record the checkpoint commit and exact executable: these binaries contain the
then-uncommitted changes documented here, not `1223898` alone.

The user explicitly approved one 30-second input sequence each on `escape1`
and `mp_beach`: five seconds forward/back/left/right, then ten seconds of
attack, using frame waits at the configured 60 fps. Every control was released.
Both tests exited cleanly and produced engine captures, but MP visibly stalled
at warmup and did not fire; its initial exit-only pass is superseded by that
failure. SP used its current knife and returned from `(1654,1677,-719)` to its saved
`(1808,1600,-719)` after loading, then saved again. Records in
`approved-input-v1/` use ordinary client v7, before the episode-only correction.
That permission was consumed by these two tests; it is not standing authority.

The user separately approved one MP retest after the restart fix. The private
one-shot observer used wall-clock deadlines and left the command buffer free.
Each movement lasted 5.007–5.008 seconds; attack lasted 10.000 seconds. All four
directions changed server-confirmed position, acknowledged time advanced, the
MP40 clip fell from 32 to zero, and all controls were released. Health remained
100 and the final capture has no connection-interrupted indicator. This pass
is recorded in `approved-input-v2/`, with its binary/hook identity in
`mp-input-probe-v2/`. That permission is also consumed.

## Individual map coverage

All 47 official maps launched separately. All 32 SP maps initialized both AAS
worlds and their routing caches. All 25 playable SP maps saved, reloaded in the
same process, saved again, and loaded in an independent ordinary process.
Dedicated cutscenes and the ending were launch/capture checks; cinematic saves
are intentionally not counted as successful game saves.

| SP map | Launch + both AAS worlds | Gameplay save / same-process load | Fresh-process load |
| --- | --- | --- | --- |
| assault | Pass | Pass | Pass |
| baseout | Pass | Pass | Pass |
| boss1 | Pass | Pass | Pass |
| boss2 | Pass | Pass | Pass |
| castle | Pass | Pass | Pass |
| chateau | Pass | Pass | Pass |
| church | Pass | Pass | Pass |
| crypt1 | Pass | Pass | Pass |
| crypt2 | Pass | Pass | Pass |
| cutscene1 | Pass | Cinematic | Not tested |
| cutscene6 | Pass | Cinematic | Not tested |
| cutscene9 | Pass | Cinematic | Not tested |
| cutscene11 | Pass | Cinematic | Not tested |
| cutscene14 | Pass | Cinematic | Not tested |
| cutscene19 | Pass | Cinematic | Not tested |
| dam | Pass | Pass | Pass |
| dark | Pass | Pass | Pass |
| dig | Pass | Pass | Pass |
| end | Pass | Ending | Not tested |
| escape1 | Pass | Pass | Pass |
| escape2 | Pass | Pass | Pass |
| factory | Pass | Pass | Pass |
| forest | Pass | Pass | Pass |
| norway | Pass | Pass | Pass |
| rocket | Pass | Pass | Pass |
| sfm | Pass | Pass | Pass |
| swf | Pass | Pass | Pass |
| trainyard | Pass | Pass | Pass |
| tram | Pass | Pass | Pass |
| village1 | Pass | Pass | Pass |
| village2 | Pass | Pass | Pass |
| xlabs | Pass | Pass | Pass |

The 15 MP world captures passed individually: `mp_assault`, `mp_base`,
`mp_beach`, `mp_castle`, `mp_chateau`, `mp_dam`, `mp_depot`, `mp_destruction`,
`mp_ice`, `mp_keep`, `mp_rocket`, `mp_sub`, `mp_tram`, `mp_trenchtoast` and
`mp_village`. The later `mp-match-v2/` sweep also joins Axis as a soldier and
checks a native warmup restart and advancing match time on each map; every
resulting first-person capture was inspected. These checks do not establish
peer interoperability, objective completion or every class/weapon behavior.

## Save, navigation and regression checks

The ordinary-client cycle probe covers three consecutive reloads, cross-map
loading from `escape2` to `baseout`, autosave loading, rejected missing/truncated/
unsupported-version/traversal requests, and preservation of the previous
`current.svg`. A valid save/capture still succeeds after rejected requests.
Retail WolfSP loads fnTech3 `escape2` and `baseout` saves and rewrites them;
fnTech3 then loads and rewrites those files. This is two-map save interoperability,
not coverage of every save version or retail release. All six cycle cases also
pass with final ordinary client v9, after the MP restart changes.

Installed-data CPU probes passed on Windows x86:

| Probe | Corpus | Comparisons |
| --- | --- | --- |
| AAS8 live-loader records | 70 installed archive members | 59,894,232 |
| Routing-cache conversion | 70 installed AAS/RCD pairs | 154,935,708 travel entries |
| Routing initialization/costs | Original/iortcw SP/MP over 70 pairs | 15,938,096 matrix entries |

The corpus includes superseded archive members; the map sweep uses 64 active
SP worlds. Routing comparisons retain the binary32 arithmetic limits in the
[routing checkpoint](2026-09-16-rtcw-routing-initialization.md). Original/iortcw
implementation bodies stay in disposable GPLv3 comparison fixtures only.

The complete Windows x86 development suite passes 111 Meson tests with two
skips, including 1,482 Python tests with eleven skips. Development and default
Windows x86 builds pass. Ten focused Python checks, the state-adapter executable
and server-game-data executable pass under Linux x86 ASan/UBSan. The later
episode guard passes focused Windows and Linux x86 save/cvar checks. MP match
transitions and map policy also pass Windows and Linux x86 ASan/UBSan checks.
The final contract probe passes 2,227 SP and 2,224 MP static assertions against
each of the original/iortcw headers. Six ordinary-client Q3/QL GL/Vulkan/RTX
captures pass and were inspected: Q3 gameplay
startup and QL's native join screen.

## Evidence and remaining scope

Useful evidence is retained under `.tmp/rtcw-campaign/`:

- `sweep-v2/`: initial 47-map sweep. `sky-v1/`, `gameplay-v1/`, and `dam-v3/`
  contain corrected follow-ups.
- `mp-world-v2/`, `mp-checkpoint-v1/`: inspected MP world views, including the
  checkpoint-only maps in their advertised mode.
- `mp-match-v2/`: all 15 native warmup restarts, advancing clocks and inspected
  first-person captures. `mp-match-v1/` confirmed clocks but failed capture
  because the initial probe quit before the renderer flushed its screenshot.
- `verified-save-inputs/inputs.json`: provenance of the 25 gameplay saves.
  `cold-final/`: independent loads before episode correction.
  `cold-episode-final/`, `save-cycles-final/`: ordinary-client v8 loads with map
  and episode preservation checks. `save-cycles-v9/` repeats all six cycle
  cases on the final ordinary executable.
- `retail-load-v2/`, `retail-cinematic-v1/`: original executable observations.
  `approved-input-v1/`: the first two specifically authorized input tests,
  including the superseded MP failure. `approved-input-v2/`: authorized MP
  retest with server-confirmed motion and ammunition consumption.
- `aas-load-x86.json`, `routecache-load-x86.json`, `routing-x86.json`,
  `contracts-x86-final.json`, `linux32-v3.log`, `episode-linux32.log`,
  `suite-final-testlog.txt`, `shared-v9/`: comparison, sanitizer and regression
  evidence. Manifests record commands, hashes, map CRCs and failure logs.

Initial cinematic-save and Dam failures are retained, not counted as passes.
Gameplay saves supersede the invalid cinematic saves. Saves and screenshots are
local evidence; no proprietary installation content is committed. Completed
runtime probes remove their writable homes. Automatic approval review rejected
the final disposable-file cleanup with only “blocked by policy”; older scratch
fixtures, redundant binaries and the abandoned `sweep-v1/boss2/home` remain
inside this ignored task directory.

Remaining limits include full campaign transitions and sustained AI/combat,
historical/damaged native save payloads, MP peer/objective/class coverage,
original-runtime route decisions, dynamic-entity prediction/BSP parity,
cold-cache tactical differences, and generating/writing missing RTCW routing
caches. The official installation supplies all required SP caches; custom-map
cache generation is not added here. Renderer/audio parity and the other
[compatibility gates](COMPATIBILITY.md) remain open. Retail-module requests for
absent optional sounds/models are retained in warnings; no replacement assets
were introduced. These results do not justify a universal “flawless” claim.
