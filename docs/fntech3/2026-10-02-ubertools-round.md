# UberTools compatibility checks and gap resolutions, 2026-10-02

This round covers the experimental `alice`, `fakk2` and `mohaa` profiles in
the shared Windows x86 development build. Its
[JSON companion](2026-10-02-ubertools-round.json) records the dirty-tree source
identity, binary hashes, commands, legitimate installation identities and
results. All three release gates remain closed; these checks do not establish
complete campaign, presentation or retail interoperability compatibility.

## Ritual module bounds

Alice's retail configstring accessors bound their index by 1,328
(`alice.exe`, set `0x00454a5b`, get `0x00454b09`); the official F.A.K.K.2
1.02 executable's use 1,200 (set `0x004458cb`, get `0x00445979`). These are
direct, hash-pinned native observations. The corresponding light-style bases
are 864 and 736, and both retail wrappers delegate to their bounded setter.

The shared Ritual game wrappers previously admitted the engine's internal
2,736-slot storage. Get-configstring could therefore read beyond the selected
title's contract. Set-configstring and light-style wrappers also used the
internal bound, although the shared writer later refused ordinary oversized
writes. The wrappers now use the selected Ritual layout. Light styles are
bounded before adding their base, preventing signed overflow. Refusing negative
styles and overflowing arguments is fnTech3's input policy, rather than a claim
to reproduce retail wrapping.

Three regressions run the production wrapper bodies and verify native bytes.
The production-body regression fails before the correction and passes
afterwards; the two native-byte checks pass on both versions. They cover both layouts,
last-valid and first-invalid indices, null values, negative indices and integer
extremes with no invalid write side effects. The ten savegame, ten snapshot
and seven F.A.K.K.2 wire controls also pass. This is Source/Unit and native
observation evidence; ordinary runtime startup does not exercise invalid imports.

OpenAlice's GPLv3 Ghidra corpus helped locate Alice's accessors, after which the
retail executable bytes directly confirmed them. It is reference observation
only. No external implementation text, SDK code, retail module or game data is
incorporated.

## MOHAA client-game message framing

MOHAA's retail 1.00 client game reads each client-game message type as six
bits, and reads a one-bit continuation before returning from export 10
(`cgamex86.dll`, type `0x3002f943/0x3002f945`, continuation
`0x30030836/0x30030838`, return `0x3003084c`). These are direct native module
observations. Queue capacity and storage behavior come from the pinned
OpenMoHAA reconstruction, and are not presented as retail-engine measurements.

The server queued its closing false bit through the same bounded payload
queue. A final accepted datum could fill all 4,096 bytes, causing that closing
bit to be refused and the retail parser to read into the following opcode.
The writer now emits the closing bit directly into the wire message after
replaying the accepted payload. Queue capacity, payload order, ordinary
overflow refusal, empty-queue behavior and reset semantics are preserved.
Malformed zero widths and widths outside +/-32 are refused before mutation
or negation; -32 remains valid because this queue normalizes it before replay.

The regression uses the production queue/replay code and Huffman/message
codec. It fails on the old full-buffer writer and passes after the correction:
4,093 through 4,096 bytes, all 64 valid signed widths, invalid widths and integer
extremes, empty/reused queues, capacity refusal and the following MOHAA EOF
byte 11 are covered. All 56 MOHAA Python controls pass. A mutation restoring
the queued terminator also fails.

OpenMoHAA's GPL-2.0-or-later header was verified; this correction is independently
written within the already attributed adapter. No new external implementation
text is incorporated. Whole-message rollback when module payload overflows is
still open. SafeDisc prevents the local retail engine from running, so this is
not a retail-engine interoperability result.

## Ritual lighting input validation

The shared GL, Vulkan and RTX Ritual model-light reader used floating-point
self-comparisons for finite validation. Fast-math compilation removed the
NaN protection, leaving malformed compiled lights active. Ambient and sun
entity keys could also propagate non-finite values in ordinary builds.

The reader now validates binary32 bits, checking the original map bytes rather
than relying on decoded float values, preserving its existing 3.4e38 magnitude
bound and signed zeros.
Malformed compiled lights remain inactive. Invalid entity-key components keep
their last valid value; that is an explicit engine input policy. Valid lighting
arithmetic and renderer defaults are unchanged. MOHAA uses its own lighting
path and is not routed through this reader.

Three new production-header regressions and four existing controls pass in
the Win32 compiler environment, including strict and fast-math builds. The
retail controls read all 36 Alice maps (10,184 lights, 39 player starts) and
30 F.A.K.K.2 maps (10,267 lights, 36 starts). They compare the reader with an
independent lighting-law calculation. They establish parsing and arithmetic,
not retail visual parity. An initial existing-fixture run used an x64 compiler
environment and reached the intended architecture refusal; the correctly
configured Win32 rerun passes.

## Probe and documentation gaps

The Ritual client probe now requires output below the real project `.tmp`,
rejects installation/runtime overlap, linked ancestors, junctions, symlinks,
hard-linked files and unsupported file types before cleanup or writing. The
ancestor check uses `lstat` directly, including dangling Windows junctions,
and checks canonical containment too. Its regressions use authored fake
installations and verify protected data is preserved. No hard link is made
from a real installation. Final review also found that `--extra` could
override those validated paths with a later `+set`. All extra names are now
validated before any output validation, cleanup, writes or process launch;
filesystem/profile identity and the probe's renderer, window and input
settings cannot be overridden this way. Ordinary audio, rendering features
and diagnostic values still pass through, including empty values and values
containing `=`. All seven focused probe tests pass.

The general map probe silently omitted requested labels with different case
or missing names. This round's baseline requested `mohaa:DM/mohdm1`, while
the enumerator produced `mohaa:dm/mohdm1`; the baseline therefore contains
three cases per renderer, rather than four. Selection now resolves case and
whitespace consistently and rejects missing profiles, missing requested labels
and empty selections before output creation or launch. Four regressions pass.
The final run includes the deathmatch case. There is no before/after comparison
for that case; comparisons use only the three common cases per renderer.

The Audio Guide now includes MOHAA's aliases and channel ownership and states
the legacy fallback's actual limits: it retains one-shot volume but does not
apply alias distances or pitch, loop volume or MOHAA channel replacement/stop
semantics. These are existing implementation limitations, not newly removed
features. The guide also names MOHAA's supported `2015` map format for sidecar
generation. F.A.K.K.2's stale top-level ledger summary is reconciled with its
already recorded first-level reverse retail-server and bidirectional save
evidence; this round does not repeat or broaden that interop evidence.
Both ledger rows now correctly state that Alice and F.A.K.K.2 admit protocol
1, removing the claim that F.A.K.K.2 refuses Alice's protocol number.

## Build, full suite and runtime

The shared `meson/build/win32-dev` preset rebuild completes all 766 steps with
the three developer options enabled. It is a rebuild of the existing preset,
not an independent release clean-build claim. Final client SHA-256 is
`e9c75885c944dc71b0bae52772f409940c28af8d35824e09240ca27fdefb2680`; the
dedicated server is
`8f2cfeeb98fe632e25463a8ddbe51646c6bc1b8fd0cc50cc774471b5eb12b504`.
The companion pins all three renderer modules and changed source files.

The first complete run finishes 2,417 Python tests in 1,704 seconds with
120 skips, but its packaging checks fail: the Markdown record was created
before its JSON companion, so the test's staged documentation lacks the
companion. This causes one packaging error and eight proprietary-library
subtest failures, all with the same missing-document diagnostic. The other
179 Meson checks pass, and OpenAL loopback skips because `ALC_SOFT_loopback`
is unavailable. With the pair present, all 49 packaging tests pass (one
platform-dependent skip), in 278 seconds. An early rerun was deliberately
stopped after 226 seconds to include the dangling-junction probe correction.
The final complete run passes: 180 Meson checks, zero failures and one OpenAL
loopback skip. Python discovery runs 2,418 tests in 1,635.221 seconds with
120 skips and no failures. Engine source, binaries and tested source hashes
stay unchanged throughout that complete run.

The subsequent `--extra` guard changes only the Python probe and its test
file. All seven focused probe tests then pass in 0.308 seconds, with no
skips. The companion separately pins the complete-suite source and the final
source, lists these two later changes and retains their focused validation;
the complete suite was not rerun for that probe-only correction. No engine
rebuild or runtime binary changes follow the successful suite.

All installed maps load with GL and reach their first snapshots: Alice 36,
F.A.K.K.2 30 and MOHAA 54, for 120 normal exits, with no rejected loads,
crashes, faults or hangs. The sweep takes 573 seconds. Twelve selected
startups also pass across GL, Vulkan and RTX: `garden1`, `fakkhouse`,
`m1l1` and `dm/mohdm1`. Outcomes, first errors and world-surface counts are
unchanged for the nine common baseline/final cases; the three final
deathmatch cases have no baseline counterpart because of the selection bug.

Fifteen additional private-desktop probes produce inspected 1024 by 768
engine `screenshotPNG` captures and exit normally without forced stops.
Alice's initial cinematic captures differed in whether the character was
visible; three supported `skipcinematic` console checks show her third-person
view on all renderers, so the initial difference is not evidence of a missing
model. F.A.K.K.2 draws its opening scene. MOHAA's initial captures show the
Continue briefing; three further captures use the registered
`finishloadingscreen` console service and show the truck, soldiers, world and
HUD. No mouse, keyboard or engine input events are synthesized. These are
bounded views, not exhaustive renderer comparisons. RTX-module probes run
its raster path, with ray tracing disabled.

OpenAL diagnostics confirm the `inverse_clamped` distance model, geometry
occlusion, EFX reverb and engine audio zones: 512 on `garden1`, 343 on
`fakkhouse` and 68 on `m1l1`. This host loads `OpenAL32.dll`'s Generic Software
Compatibility Mode. It lacks OpenAL Soft extensions, HRTF, output-mode/device
clock extensions and highpass/bandpass filters; its lowpass path remains
available. Sound stays muted. Installation inventories and executable/renderer
hashes remain unchanged during the probes. All generated data stays under
the project's `.tmp/ubertools-round-2026-10-02/`.

The shared build retains FnQ3/FnQL presentation: model ambient/directed
lighting and the existing renderer feature options; the OpenAL distance
model, geometry occlusion, EFX reverb and engine-owned audio zones. Device
capabilities, rather than profile selection, limit HRTF and OpenAL Soft
extensions in these runs. No renderer or audio default is disabled for retail
presentation matching. Muted audio diagnostics do not establish audible
mix quality, and startup captures are not retail image comparisons.

## Remaining requirements and unrelated findings

All three profiles remain experimental. Alice still lacks retail peer, demo,
save and frame comparisons. F.A.K.K.2's prior limited interop does not establish
longer play or every reverse-direction level. MOHAA's engine cannot run here,
and its engine save header is incompatible with retail. Complete gameplay,
campaign progression, malformed-module recovery, audio listening and applicable
bidirectional interop remain open. Extreme finite lighting accumulation and
transactional MOHAA payload overflow are outside this slice.

The map warning audit identifies a further open MOHAA audio boundary: reliable
`fadesound` commands are reported as unknown and do not reach an engine fade.
This appears in the current `void` startup and in older valid-campaign
`m3l1a` and `m5l3` logs, including `fadesound 4000.00`. Current import 118
also ignores its `differentServer` argument. Both omissions are observed in
fnTech3's source; ignoring previous-map commands and consuming `fadesound`
through `S_FadeSound` are OpenMoHAA reconstruction behavior, not native retail
engine measurements. The audible effect and correct 1.00 timing/scale are
unmeasured. This new blocker is recorded; these adapters are unchanged by the
three fixes above.

Alice's ten `hedge2` collision-plane warnings match its existing map record,
and its 108 missing optional configuration messages match shipped files and
the recorded startup sequence. F.A.K.K.2 has no warnings in the quiet sweep.
MOHAA's nonfatal content/adapter warning categories are recorded separately
in the companion; reaching every first snapshot does not make those warnings
or audio omissions compatible.

Unrelated findings retained for follow-up:

- Older comments in `server_alice_game.hpp` still call implemented entity
  binding/linking incomplete and loading refresh a no-op.
- Pinned OpenMoHAA allocates its datatype storage with 4,096 bytes despite
  declaring an 8,192-byte capacity. fnTech3 already has the actual 8,192-byte
  array and is unaffected.
- The unchanged, inherited `TIKI_FindTikiAnim` passes an uninitialized
  `filename` array to its animation cache (`code/mohaa/tiki/tiki_cache.cpp:58`),
  which `MOHAA_ResourceCached` can reach. No runtime effect is established;
  this source defect is recorded for a separate correction.
- Q3Test is absent from the Audio Guide's client-scope sentence and the
  compatibility ledger's opening matrix-link list, although its detailed
  gate matrix is present.

Automatic approval review rejected cleanup of fifteen disposable writable
probe homes, the MOHAA raw disassembly, two verified empty scratch
directories and the final compiler temporary directory with the reason
`blocked by policy`.
They were retained and those rejected actions were not retried. The companion
records exact paths and any further cleanup outcome. Useful test logs,
manifests and engine captures remain in the designated project evidence area;
proprietary inputs are not committed.
