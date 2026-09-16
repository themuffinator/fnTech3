# RTCW goal and movement state checkpoint

RTCW SP and MP now allocate, initialize, reset and free AI goal/movement
states through the shared executable. SP passes the previous missing
goal-allocation import 546 and reaches a captured mission briefing with clean
shutdown. This is startup evidence; the campaign is not yet playable.

## State contracts

The pools use the immutable game profile's client limit: 128 states for SP,
64 for MP, Q3 and QL. The internal storage can hold the largest profile, without
expanding the external Quake ABI or changing its client limit. Exhaustion,
invalid handles, allocation failure, repeated shutdown and resource ownership
are exercised with the actual pool functions. Freeing item weights now clears
their owned pointers, preventing a repeated free through another lifecycle call.

Checked RTCW imports cover goal allocation/free/reset/avoidance reset
(546/547/525/526), movement allocation/free/reset/initialization
(555/556/548/557), and avoidance reset/last reset/initialization (551/552/558).
Move allocation has no arguments. Goal allocation validates its client index.
The boundary checks integer width and full native/QVM record spans before
publishing converted state. Existing nonfatal invalid-handle behavior remains.

Movement initialization accepts a frozen, four-byte-aligned 68-byte record.
Fields are copied explicitly after finite-vector/time and entity/client checks.
RTCW's walk flag is 256, while the canonical Quake flag is 512. RTCW replaces
only ground, water-jump, teleport and walk flags; the canonical grapple-pull
flag is preserved. Q3/QL keep their existing initialization behavior.

Avoidance initialization clears the three avoidance arrays and preserves the
current route. RTCW avoidance reset also clears the previous area, goal area
and reachability. Q3/QL preserve those route fields. A full movement reset
clears the complete internal state. Goal reset retains the client and loaded
item-weight ownership while clearing its goal stack and avoidance records.

## Reference comparisons

The contracts are independently implemented from observations of the original
[SP](https://github.com/id-Software/RTCW-SP/tree/master/src/botlib),
[MP](https://github.com/id-Software/RTCW-MP/tree/master/src/botlib) and
[iortcw](https://github.com/iortcw/iortcw) sources. Their GPLv3 implementation
text is used only in disposable comparison fixtures with its notice retained;
the production changes extend the inherited GPLv2-or-later implementation.

`probe_rtcw_ai_state.py` compares 524,288 initialization/flag combinations and
12,288 reset operations per reference on Windows x64, Windows x86 and Linux
x86 with ASan/UBSan and leak detection. All four reference variants pass the
defined-behavior comparison. Internal route preservation is checked alongside
the changed fields. iortcw additionally passes 4,096 last-avoidance resets
per mode and target.

The original SP/MP `BotResetLastAvoidReach` checks
`avoidreachtries[MAX_AVOIDREACH]` after its loop, an out-of-bounds read. The
first unqualified reference comparison exposed this difference. The probe
explicitly excludes that original function instead of modifying or executing
it. fnTech3 retains its bounded implementation, which agrees with iortcw's
corrected function. This is an intentional safety difference, not a claim of
equivalence to the undefined original operation.

The frozen-layout probe passes 2,091 assertions per SP reference and 2,081 per
MP reference on Windows x86 and x64. Eight focused lifecycle/entity/navigation
tests pass on Windows x64 (8.991 seconds), Windows x86 (10.725 seconds), and
Linux x86 sanitizers (41.940 seconds). These include real pools, failed
allocation without publication, capacity/reuse, reset ownership, malformed
input, unaligned records, QVM end boundaries and Q3/QL preservation.

The complete suite passes 111 Meson checks with two expected skips. Python
discovery runs 1,400 tests in 364.132 seconds, with 11 expected skips and no
failures. Logs are retained as `ai-state-final-suite.log` and
`ai-state-final-testlog.txt`. `git diff --check` also passes.

## Runtime evidence and limits

Both Windows x86 client/server distributions and all three renderer modules
build. Evidence uses a dirty worktree based on commit
`c75573c7375e44c908256020ada2d7e12970cacf`; it is not a committed release.
Executable, source, reference and evidence hashes are recorded in
`.tmp/rtcw-manager/ai-state-source-identities.json`.

`ai-state-final-openal/manifest.json` records the same development executable
loading installed SP `escape1` (pure off) and MP `mp_beach` (pure on), receiving
snapshots and shutting down cleanly with OpenAL initialized. Both engine PNGs
were personally inspected: SP shows the mission briefing and “Please Wait”;
MP shows the textured deployment UI and briefing movie. No unsupported import
was reached during these probes.

A longer 600-frame SP probe in `ai-state-long-sp/` still shows the briefing.
It reports `cg_norender=1`, `g_missionStats=0`, `cl_waitForFire=0` and both
pause flags at zero. The published `AICast_CheckLoadGame` sets `cg_norender`
while waiting for cast/client readiness, then sends `rockandroll` to progress
the briefing. No `rockandroll` command is observed. This identifies a startup
path to investigate; it does not establish which readiness condition fails
inside the installed module. No setting was forced to bypass that path.

The follow-up `ai-state-startup-status/` and `ai-state-startup-timing/` probes
print server status and the module's entity list after 600 frames. They show
three soldier clients, `sv_maxclients=64`, `sv_fps=20`, `timescale=1` and
`g_reloading=1`. The installed `pak0.pk3` map contains 29 `ai_soldier`
spawners (counted in memory without staging assets). The published entity-list
command intentionally omits slot zero; its omission is not evidence of player
corruption. The 64-client campaign default matches original SP server policy
and is distinct from the 128-state capacity available to the engine. These
observations narrow the cast-readiness investigation without identifying its
root cause.

`ai-state-final-live/manifest.json` records the default executable running
Q3 `q3dm6` and QL `campgrounds` under GLx, Vulkan and RTX raster. All six pass
with personally inspected engine PNGs, expected module/renderer identity and
readable scenes/UI. The QL join menu was not entered. All probes used private,
unswitched desktops, 640×480 windowed mode, disabled input and isolated homes.
Installations were unchanged and the writable homes removed.

The source fixtures do not prove navigation queries, movement prediction or
route selection. Those contracts, cast readiness, campaign/save-load, missing
cache generation, sustained MP gameplay and peer interoperability remain open.
RTCW remains development-gated. SP startup also logs missing sound resources
and unrecognized `picmip2` shader parameters; their content/renderer causes
remain to be audited. The unrelated, previously recorded Q3/QL routing-cache
reader bounds audit remains open.

Retained evidence is under `.tmp/rtcw-manager/`: `ai-state-layout-*.json`,
`ai-state-reference-defined-*.json`, `ai-state-focused-*.log`,
`ai-state-final-*-build.log`, `ai-state-final-build.log`, and the runtime
folders above. The original failing comparison is retained separately as
`ai-state-reference-original-sp-x64.json`.
