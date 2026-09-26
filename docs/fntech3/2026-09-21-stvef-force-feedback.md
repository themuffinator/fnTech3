# Elite Force SP force feedback

The four SP cgame imports at ordinals 34–37 now drive a force-feedback
controller instead of returning without an implementation. The controller
uses the selected installation's `FFC10.dll` through an independently declared
Windows x86 interface. It implements all 32 effect IDs, the retail slot rules,
and the optional joystick spring. No proprietary runtime or SDK code is
distributed.

This closes the implementation gap recorded in the earlier cgame and input
records. It does **not** establish physical device compatibility: the actual
runtime has not been initialized against a device during these tests. Physical
input, listening, broader campaign play and the other open compatibility gates
remain pending.

## Native observations

The reference is retail `stvoy.exe` 1.20, SHA-256
`8f7ccadb52b4230689c5b3275177f7ec7060a30bfaa3ade4e7b3fb0cda0dff74`.
Its installed `FFC10.dll` is version 1.0.11, Windows x86, 126,976 bytes,
SHA-256 `d4e88d9028e86894be5c61c62c7a0f648a032032d545a37834813389d61354a3`.
Read-only PE inspection confirms that it exports all 21 functions the bridge
resolves. The DLL itself was never loaded or executed by the automated probes.

An owned helper maps the retail executable without entering its startup code.
It substitutes recording functions for every reached allocation, constructor,
initializer, destructor, start, stop and status call. The device pointer is an
inert token; no device API is invoked. The native effect builder at `0x43e080`
produces 45 components across 32 effects. Four gain settings (0, 0.3, 1 and 3)
give 128 cases and 180 recorded component initializations. The
[numeric fixture](../../tests/fixtures/stvef_force_feedback_retail.json)
contains their raw argument words, GUID bytes and envelopes, plus effect names
from `0x496030`.

`js_ffmult`, registered at `0x42d58d`, defaults to 3.0. It scales component
magnitudes before integer truncation; fixed offsets and envelope levels remain
unchanged. In particular, gain zero is not a substitute for disabling feedback.
The implementation preserves the observed precision and signed offsets. It
gives negative or non-finite gains zero magnitude and saturates finite overflow
before conversion. These malformed-value rules are robustness choices, not
claims about native overflow behavior.

A further 35 native scheduler steps exercise `0x43dcb0` through `0x43df7c`,
including failed starts, stops and partial effect initialization:

- `Start` restarts an existing effect; `Ensure` starts it only if stopped.
- A stopped occupied slot is reused before an empty slot. If all four slots
  are playing, slot 1 (the second slot) is replaced.
- A failed stop preserves the old effect. A failed build releases its partial
  components and leaves the slot empty.
- `Stop` and `StopAll` retain cached effects. Clearing releases them and resets
  their IDs to the native empty sentinel, 33.

Device initialization and spring parameters are source observations at
`0x43d850`, `0x43da95`, `0x43df80` and `0x43e020`, not live device results.
The native object allocations are 40 bytes for CFeelMouse, 48 for CFeelDXDevice,
132 for CFeelConstant, 144 for CFeelPeriodic and 188 for CFeelSpring. The bridge
provides aligned opaque storage and calls the runtime's normal destructors;
it never invokes a deleting destructor on engine-owned storage. GUID and
envelope storage remains alive for each effect's full lifetime.

## Opt-in and lifetime

Only `stvef-sp` registers these cvars and commands. `use_ff` defaults to 0 and
does not load a runtime or open a device while disabled. When enabled, feedback
requires a focused, non-minimized game window with input grabbing permitted.
`in_nograb 1` also suppresses feedback. Both SDL and native Win32 input report
the window lifetime to the controller.

The default selection tries the FEELit mouse interface, then a DirectInput
force-feedback device. `ff_usemouse` and `ff_usejoy` request a fresh selection;
they do not enable `use_ff`. `ff_defaultTension` defaults to 1 and maps levels
0–3 to coefficients 0, 1000, 5000 and 10000. `ff_tension` sets that preference;
`ff_spring` temporarily sets a coefficient from 0 through 10000. `ff_play`
accepts an ID from 0 through 31 or `name` followed by the retail effect name.

All required exports resolve before construction begins. Missing libraries,
missing exports and failed devices report unavailability without claiming
success. A failed attempt is not retried every frame; focus restoration or a
selection/input restart permits another attempt. A device that cannot create
its spring can still play ordinary effects, and reports the spring limitation.

Disable, focus/input reset, disconnect, input shutdown and window restart
release effects and the device before unloading the library. Restoration
starts with empty slots; old effects are not replayed. Failed status queries
do not reuse stale status bits. Other architectures and non-Windows builds
report the native runtime boundary as unavailable. SP's retail modules already
require Windows x86; Holomatch and the other five profiles do not enter this
feedback controller.

## Validation and reproduction

The [evidence manifest](2026-09-21-stvef-force-feedback.json) records
source and binary identities, build/test results, native helper identity, exact
runtime commands, logs and captures. The five compiled regression groups run
the production table, scheduler, runtime bridge and controller with an owned
recording runtime. They pass on Windows x86 and Linux i686, including x86
register/stack calling conventions, all 128 native parameter cases, the 35-step
scheduler trace, every missing export, device fallback, spring failures,
partial cleanup, bounds, malformed gains, profile isolation, opt-in, focus,
disable, restart and shutdown. Linux uses undefined-behavior and float-cast
overflow sanitizers.

All three renderers build on Windows and Linux. The Windows SDL client builds
with warnings as errors; the alternative native Win32 input translation unit
also compiles with those production flags. The complete Windows suite passes
120 Meson entries with two skips and no failures. Its Python aggregate runs
1,851 tests with 72 skips and no failures.

Six Windows production probes load `_brig` in SP and `hm_borg1` in Holomatch,
once on each renderer. All exit normally, verify their selected renderer and
write a valid engine screenshot. Read-only module enumeration samples each
child 31–37 times and observes no `FFC10.dll` loaded with `use_ff 0`. SP's
`ff_play name Pistol` reports that feedback is disabled. A rebuilt Linux GLx
client also completes the existing 107-frame Holomatch demo and writes an
engine screenshot. These are startup and disabled-path checks, not physical
feedback or image-parity evidence.

After staging the actual FFC10 runtime in the independent asset installation,
one further SP GLx probe runs the staged engine with feedback disabled. It
loads the brig scene, writes an engine screenshot and exits normally. Its
38 module samples likewise observe no FFC10 DLL loaded, with the runtime
present and available on the installation path.

Runtime homes are isolated under `E:/Games/fnTech3-tests`. Windows processes use
a private desktop that is never activated; Linux uses Xvfb. Rendering is
windowed and mouse/joystick input is disabled. All images come from the engine
`screenshot` command. No physical input or device output was controlled.

The manual installation at `E:/Games/fnTech3-tests/stvef-manual` receives the
verified engine and instructions for optional user-operated feedback checks.
An independent copy of the installed FFC10 runtime is kept in that test asset
installation, outside the repository. Feedback remains disabled by default;
the interactive launchers are not run automatically. Device acceptance still
needs the device model, its driver, successful initialization, the observed
effects and a focus/disable check.

No new unrelated defect was found. The SP muted-start probes retain the
previously observed ambient warnings. The earlier bad `st_21.bik` ZIP CRC,
broader Linux-suite failures, Windows x86 RT capability limit and rejected
cleanup of earlier temporary directories remain documented in the
[runtime coverage record](2026-09-21-stvef-runtime-coverage.md).
