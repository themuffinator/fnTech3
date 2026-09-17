# RTCW prediction over installed navigation

This checkpoint adds an offline installed-content comparison to the
[movement implementation](2026-09-16-rtcw-movement-prediction.md). Production
algorithms and the previously validated engine binaries are unchanged.
Dynamic entity collision and live campaign prediction remain open.

## Fixture and provenance

`scripts/probe_rtcw_prediction_maps.py` loads actual AAS geometry through the
engine's checked loader. A separate fixture library builds the shipping BSP
collision sources and supplies actual `CM_PointContents` results to both
prediction implementations. It verifies that the computed BSP checksum matches
the AAS header. Assets arrive through an owned input pipe and stay in memory;
temporary libraries, executables and sources are removed after the run.

Archive overlays resolve 64 distinct AAS members from `pak0.pk3` and
`sp_pak4.pk3`. Earlier 70-pair archive comparisons included overridden versions;
these counts describe different corpora. Each member is tested in both AAS
world slots, with 512 predictions per slot. Cases vary area-center offsets,
velocities, commands, presence types, frame counts, durations and stop events.
The actual BSP service exercises water/slime/lava classification rather than
assuming empty contents.

Original [SP](https://github.com/id-Software/RTCW-SP/blob/master/src/botlib/be_aas_move.c)
and [MP](https://github.com/id-Software/RTCW-MP/blob/master/src/botlib/be_aas_move.c)
functions and [iortcw](https://github.com/iortcw/iortcw) remain confined to
disposable GPLv3 comparison programs with their notices. The engine retains
its GPL-2.0-or-later implementation. Reports identify reference sources,
generated fixtures, collision-library binaries, compiler commands and each
installed AAS/BSP pair by hash. Proprietary inputs are not tracked.

The fixture marks the loaded geometry ready for isolated prediction; it does
not repeat full routing-cache initialization. Both implementations use the
engine's same BSP contents service, so this is not original-engine BSP parity.
No dynamic entities are supplied. MP policy over installed SP navigation is a
synthetic MP corpus, not an installed MP bot compatibility claim.

## Observations

Each original reference receives 65,536 predictions per configuration. As in
the synthetic checkpoint, original start-solid paths can read an uninitialized
direction. The probe records their raw observations and excludes 28,765 such
cases from strict agreement. All remaining **36,771 predictions per reference**
agree in return value and every byte of the 80-byte external result on:

- Windows x86 and x64 with Clang strict floating-point arithmetic.
- Windows x86 with the release's MSVC compiler, `/O2` and `/fp:precise`.
- Linux x86 with explicit SSE2 arithmetic, AddressSanitizer,
  UndefinedBehaviorSanitizer and leak detection.

The Linux SSE2 comparison does not alter production compiler settings or close
the earlier default-x87 numeric differences. Contents-service call counts vary
between compilers, including excluded paths; this checkpoint does not assert
cross-compiler execution identity.

The Windows x86 fast-math diagnostic differs on 7,370 defined records per
original reference and excludes 28,764 undefined-path cases. One difference
changes a decision: `baseout_b0`, world slot 1, corpus query 58, source area
2,507, stop mask 32 and a 16-frame limit. The engine completes 16 frames while
the original compiled reference returns failure with a cleared record.
The report preserves both records and the reproducible query parameters.
Consequently, the earlier synthetic corpus's unchanged-decision observation
must not be generalized to installed geometry. Production arithmetic settings
remain unchanged, and fast-math parity remains open.

iortcw is diagnostic because its friction and normalization policies differ.
Under strict arithmetic its SP comparison has 32,845 differing records and
3,457 changed decisions; MP has 32,842 and 3,458 respectively. These differences
are not treated as original-game compatibility failures or hidden as agreement.

## Evidence and remaining scope

Final reports are `.tmp/rtcw-manager/prediction-maps-final-{x86,x64,msvc-x86,linux32}.json`
and `prediction-maps-final-fast-x86.json`; the checkpoint identity record is
`prediction-maps-source-identities.json`. Earlier reports without the final
prefix retain investigation history, including a Windows macro compile error
and an x64 link attempt made with x86 runtime-library paths. Both fixture setup
issues were corrected before final comparison. All final programs exit normally
and their disposable directories are removed.

The production source and binary hashes still match the prior prediction
checkpoint, whose full suite passed 111 Meson checks and 1,424 Python tests,
with two and 11 expected skips. Its eight inspected engine captures remain
briefing/renderer smoke evidence, not live AI evidence. No additional game or
input session was launched for this offline checkpoint.

Dynamic entity collision, installed-map tactical behavior, cold-cache retail
observations, cache generation, campaign AI, save/load, transitions, demos and
bidirectional interoperability remain open. Default RTCW startup stays gated.
The unrelated Q3/QL routing-cache reader bounds audit remains open; no further
unrelated issue was found.
