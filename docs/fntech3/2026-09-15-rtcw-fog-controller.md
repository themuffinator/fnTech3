# RTCW fog controller — 2026-09-15

SP and MP now have a shared, independently authored fog controller with explicit
mode rules. The cgame fog import validates its three integer arguments and
dispatches seven arguments through renderer API 23. GLX, Vulkan and RTX use
the same controller and copy its state into their queued views.

This is a controller and boundary implementation. Active scene fog still raises
an explicit renderer error because stage and sky GPU application is unfinished.
It does not establish visible fog, shader parsing, sky composition or full RTCW
rendering support. The default RTCW release gate remains closed.

## Mode behavior

The original SP and MP interfaces use the same slot ordinals but have materially
different semantics. SP's map selection disables fog, density `1` selects linear
fog, and a zero-duration change immediately selects the target and marks it
dirty. With no current fog, an SP transition starts from the selected record.
MP keeps map fog as a registered target, treats density `1` as exponential fog,
starts from map fog when needed, and retains the source on the exact timestamp
of a zero-duration transition. Their unregistered-target and dirty-state rules
also differ. SP's snooper view extends a linear fog clip by 1,000 units.

Both modes preserve interrupted transitions, same-mode distance/color blends,
immediate changes between exponential and linear fog, and the original
clear-screen state during a fade. A command failure leaves every controller
slot unchanged. Out-of-range slots, non-finite consumed colors/density, negative
density and overflowing finish times are rejected. Unregister and switch
commands ignore their unused float arguments. Clock rewinds freeze at the source
instead of extrapolating, and timestamp differences use wider arithmetic.

Each queued view owns copies of its fog records and clip value. Later commands
cannot alter those copies or the module's source refdef. Menu views skip world
fog advancement. Q3 and QL retain their own renderer paths. Active RTCW fog is
rejected explicitly until the GPU implementation is connected; an unregistered
target remains a harmless command, as in the references.

## Evidence

`scripts/probe_rtcw_fog.py` compiles original SP/MP and iortcw SP/MP controller
functions only in disposable fixtures with their notices. Each reference passes
80,643 complete state comparisons on Windows x64, Windows x86 and Linux x86
with ASan/UBSan. The corpus covers slot aliases, exponential/linear thresholds,
positive, negative and zero durations, interrupted fades, target removal,
negative/zero/positive clock origins and normal/snooper clip decisions. These
fixtures exercise the original C boolean/selected-enum storage as integers;
they do not emulate OpenGL or establish pixel parity. Source hashes, compiler
commands and counts are in
`.tmp/rtcw-manager/fog-reference-{x64,x86,sanitizers}.json`.

The controller tests compile as both C and C++, with optimization and the legacy
renderer boundary's explicit `-fno-fast-math` override. Float arguments and
overflow require IEEE arithmetic: Clang 20 on Windows can discard even an
integer-bit finite check when the argument has finite-only assumptions. The
existing `tr_scene.c` build override prevents that; a regression pins this rule.
Consumed non-finite values are checked through their IEEE bit pattern.
They additionally exercise
malformed commands, atomic rejection, unused NaN arguments, timestamp bounds,
large finite colors and the deliberate rewind policy. The renewed controller,
view and spatial-audio run passes ten tests on Windows x64, Windows x86 and
Linux x86 ASan/UBSan (`spatial-audio-{x64,x86,sanitizers}.log`).
The renderer/import tests exercise exact seven-argument calls, view ownership,
profile isolation, menu views, portal selection, invalid inputs and the explicit
GPU guard. The boundary run passes 62 tests on Windows x64 and x86, plus the
targeted Linux x86 sanitizer checks (`fog-boundary-{x64,x86,sanitizers}.log`).

Windows x86 client/server builds pass with API 23 and all three renderers. The
first development probe (`fog-map/manifest.json`, client SHA-256
`828e2561a0953a9c0d2cee96af0e9bafe7d183af0919ac17c0436c494318fd6c`)
passes MP's fog command and reaches entity-position import 36. SP remains at
navigation import 206. No RTCW map image is produced, and this does not prove
an active fogged scene can render. All six Q3/QL GLX, Vulkan and RTX raster
captures from the default API-23 build were inspected and pass
(`fog-live/manifest.json`, client SHA-256
`70c000dc76ebaa49b74f71b32f968db8ce7c13e47f73fc5bb30f7cdcb24829c3`).

The subsequent [spatial-audio checkpoint](2026-09-15-rtcw-spatial-audio.md)
records the renewed shared regression and runtime evidence after the float
validation and audio boundary changes. No retail modules or assets are
incorporated. Production code is
independently authored under GPL-2.0-or-later; GPLv3 reference functions remain
confined to the comparison fixtures.
