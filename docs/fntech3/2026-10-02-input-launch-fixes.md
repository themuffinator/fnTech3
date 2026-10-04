# ET and Elite Force input fixes and VS Code launch coverage, 2026-10-02

This follow-up addresses ET's audible jump placeholder, Elite Force's input
contracts and saved launch settings, and missing VS Code launches. The
[JSON companion](2026-10-02-input-launch-fixes.json) records observations,
commands, source and executable identities, checks and limitations. The
experimental profiles remain gated; these corrections do not close gameplay
or live-interoperability gates.

## ET player sound placeholder

Original ET's `CG_CustomSound` returns zero for player aliases beginning with
`*`, and its jump event submits `*jump1.wav`. Its sound engine reserves handle
zero for a generated placeholder: eight silent samples at `s_defaultsound 0`,
or a 512-sample diagnostic ramp at 1. This is source observation, including
the fact that starting sound zero is permitted. fnTech3 had instead registered
Quake III's hit feedback sample or generated its generic fallback tone.

Both fnTech3 audio backends now reserve ET's placeholder independently of any
file, register its diagnostic option at 0, and retain the selected behavior
after registration and reload. Existing RTCW SP/MP behavior remains separate,
with Q3 and Elite Force controls. Production-function tests check exact PCM,
registration, reload, file independence and failed upload. ET's spatial sounds
retain FnQ3/FnQL's distance, occlusion, EFX, zones and HRTF; this change only
adapts the module's silent sound contract. No retail presentation mode is added.

## Elite Force input

The installed SP executable directly establishes eight buttons, Use on bit 5,
alternate attack on bit 7 and any-key on bit 8 (`0x100`). Registration callbacks
and the button-building instructions are hash-pinned in the companion. The
previous engine only registered Holomatch's Use/alternate aliases and applied
the Q3 any-key flag to SP. SP now registers its own aliases and limits its
button builder to its measured count; Holomatch keeps Use on bit 6, alternate
attack on bit 5 and any-key on bit 7. A production-input fixture checks both
profiles, ignored excess buttons, jump commands, release state and Q3/QL/RTCW
controls.

Read-only inspection of the existing VS Code Holomatch home found `in_mouse 0`.
That disables mouse input. Its Space binding is Use, with X and `/` for jump,
as the installation's default config specifies. Revised launches explicitly
choose raw mouse input and free-look, with prompts for other supported modes,
and preserve saved bindings. SP's saved `in_mouse -1` is supported Windows
message input, and is not evidence of disabled mouse input.

`inputstatus` is a read-only diagnostic for effective profile, state, key
catcher, mouse mode, pause flags and the latest generated command, view and
origin. The existing opt-in engine event probe is now available to both EF
profiles, disabled by default and command-line gated. No physical input backend
is replaced: FnQ3/FnQL's raw input, binding lifecycle, finite movement checks and
mouse processing still serve both modes. Live simulated or physical gameplay
input requires the user's narrowly scoped permission under `AGENTS.md`.

## VS Code coverage

There are 162 F5 configurations and 32 explicit `run:` tasks across all sixteen
profiles. All fifteen client profiles have menu and named-map launches for GLx,
Vulkan and RTX, plus a map/renderer picker and a matching SP/listen Run Task.
All sixteen have named and picker dedicated entries and a dedicated Run Task.
ET's six named maps each offer all three renderers. F.A.K.K.2, MOHAA and Q3Test
are included, with installation prompts and installed-map choices. The IHV
Test remains dedicated-only.

MOHAA also has explicit multiplayer starter launches on all three renderers,
a multiplayer picker and a listen Run Task beside its SP entries. Its 43 SP
maps and eleven multiplayer maps have separate pickers. SP pins game type 0
and one slot; multiplayer defaults to free-for-all and sixteen slots, offering
team, round and objective modes from the pinned OpenMoHAA game-type enumeration
(reference observation). Objective mode needs an `obj/` map. Multiplayer uses
a separate writable home so its settings do not change SP.

Jedi Academy and Outcast now have SP and listen-client entries labelled boundary
probes. Their client and SP module paths and wire contracts remain open; their
MP dedicated servers already run retail game modules. These entries do not
establish playable client support. Launches reuse the stable Win32 binaries,
force windowed mode, isolate writable homes and never build or test implicitly.
Run Tasks match the F5 picker arguments exactly. Tests verify coverage, input
references, explicit client mode, windowed state and the absence of implicit
build dependencies.

## Validation and limits

The stable Windows x86 development client and server were rebuilt with all
profile options and GLx/Vulkan/RTX. The companion pins this build's identity;
independent audio work later rebuilt the same stable paths during the long
Python regression run. Current source hashes and the probes' earlier executable
hashes are recorded separately, without claiming a frozen whole-tree build.
Passive windowed private-desktop probes loaded EF SP `borg1` and Holomatch
`hm_borg1`, reached active state with no
input catcher and no pause, generated advancing commands, wrote engine PNGs
and exited cleanly. These probes disabled devices and injected no input. They
establish startup and ownership, not movement, jump or mouse-look gameplay.
SP's engine screenshot still shows the map's scripted opening. A module's
camera or player lock can inhibit gameplay independently of the engine's key
catcher; an active client state is not proof that its player can move.

ET `battery` reaches its first snapshot in spectator mode and lists the new
silent OpenAL placeholder at handle zero. MOHAA's SP `m1l1` reaches the briefing
with its normal UI and pause ownership, while multiplayer `dm/mohdm1` reaches an
unpaused match. All six passive runs exit cleanly and leave their installation
inventories and tested binaries unchanged. The engine PNGs were inspected;
none is an operating-system capture.

Four mutation runs restore the individual old ET placeholder, ET file lookup,
SP alias registration and SP any-key behaviors in standalone production-body
fixtures. Each mutation fails an assertion; corrected fixtures pass. The final
18 launch tests cover MOHAA's additional SP/MP entries and observed map labels.

The complete Meson run finished with 178 passes, two failures and one skip.
Python discovery ran 2,419 tests, with two failures, one compile-timeout error
and 120 skips. One source fixture still expected three `+use` registrations;
it now explicitly checks the fourth, profile-gated SP binding. Independent
audio changes introduced `alSourceRewind`, missing from the speech test's fake
device; the fixture now models the initial state and resets processed buffers
and offsets on rewind. All 98 input source checks and four audio tests pass
on targeted rerun. The five force-feedback tests also pass with their original
timeout, so no timeout was relaxed. The final launch checks and all four
generated-document checks pass as well. README and installation readme were
regenerated from their templates.

The full suite was not repeated after these fixture corrections. Shared source
and binary changes during its initial run also limit whole-tree reproducibility;
the individual resolutions are recorded without claiming a clean complete-suite
pass. Its OpenAL loopback test skips because `ALC_SOFT_loopback` is unavailable.

Final automated-suite and any further authorized input results are recorded in
the companion. Broad in-game input and the audible jump event are not treated
as runtime-verified solely from source, PCM tests or passive startup. No retail
installation, proprietary module or asset is changed or incorporated.

An unrelated build-cache warning remains: Ninja reported a premature end of
file and recovered on both builds, which completed successfully. Its cause
was not investigated. Disposable probe caches and helpers were inventoried
below `.tmp/input-launch-fixes-2026-10-02`; automatic approval review rejected
their verified cleanup before execution with “blocked by policy”, giving no
further reason. They remain alongside the retained logs, results and engine
captures. No rejected cleanup was retried.
