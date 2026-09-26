# RTCW bot elementary actions against the published functions

`scripts/probe_rtcw_actions.py` arrived with the shared RTCW foundation
(`a80f2409`, 2026-09-16) without a record of a run, and by 2026-09-26 it no
longer built: the Enemy Territory work (`b0d8e52b`) had renamed the two botlib
hooks it calls, `EA_RTCWExtraInputFlags` and `EA_RestoreRTCWInput`, to
`EA_ExtraInputFlags` and `EA_RestoreInput`, with the same signatures. This
record updates the probe to the current names and runs it against both RTCW
modes of the original id Software source release and of iortcw. No engine code
changed. The [machine-readable record](2026-09-26-rtcw-bot-actions.json) holds
every run's target, flags, reference hashes and outcome.

## Method

The probe compiles fnTech3's botlib action layer (`code/botlib/be_ea.cpp`), the
RTCW record adapter (`code/compat/rtcw_bot_actions.hpp`) and the server's RTCW
action imports (`code/server/server_rtcw_actions.hpp`), as
`tests/rtcw_actions_tests.py` assembles them, together with the reference
tree's unchanged `EA_` functions and its own `bot_input_t` declaration, placed
in a separate namespace, into one disposable fixture. Both sides act on the same client and every result is
compared byte for byte in the reference's own record layout:

- all 16,384 combinations of the low action flags, each with hashed high bits:
  the restored input as read back, each of sixteen elementary actions
  (gesture, attack, reload, talk, use, respawn, jump, delayed jump, crouch,
  walk and the six movement directions) applied to it, a reset, a jump, a
  delayed jump and a second reset;
- `EA_Move` at seven speeds from -999 to 999, including both signed zeros;
  `EA_View`; `EA_SelectWeapon` at `INT_MIN`, -1, 0, 63 and `INT_MAX`;
  `EA_EndRegular`;
- the seven text actions (`say`, `say_team`, the raw command, and the item
  and inventory commands), each with an empty string, `item name; test` and
  1,023 characters, comparing the command text and the client it reaches.

That is 344,099 comparisons per run. The fixture ends by shutting the layer
down and checking that no allocation is left. Reference text is compiled only
inside the fixture under `.tmp/`; none of it is committed.

## Results

| Mode | Reference | Target | Sanitizers | Comparisons | Outcome |
| --- | --- | --- | --- | ---: | --- |
| SP | original (`RTCW-SP-master`) | i686-pc-windows-msvc | none | 344,099 | pass |
| MP | original (`RTCW-MP-master`) | i686-pc-windows-msvc | none | 344,099 | pass |
| SP | iortcw (`SP/`) | i686-pc-windows-msvc | none | 344,099 | pass |
| MP | iortcw (`MP/`) | i686-pc-windows-msvc | none | 344,099 | pass |
| SP | original | i386-pc-linux-gnu | address, undefined | 344,099 | pass |
| MP | original | i386-pc-linux-gnu | address, undefined | 344,099 | pass |
| SP | iortcw | i386-pc-linux-gnu | address, undefined | 344,099 | pass |
| MP | iortcw | i386-pc-linux-gnu | address, undefined | 344,099 | pass |

The Windows runs used LLVM `clang++` targeting 32-bit MSVC from the Visual
Studio x86 developer shell; the probe now picks that target the way the test
suite does (`tests/compiler_support.py`), where it used to build x86-64 with
`FNTECH3_Q3_ONLY`. A sanitized Windows build did not link: this toolchain has
no AddressSanitizer or UndefinedBehaviorSanitizer runtime for
i686-pc-windows-msvc. The sanitized runs used Ubuntu clang 18.1.3 with `-m32`
under WSL instead, reading the same checkout.

Every action, flag alias, reset, read-back and command text matched the
published functions of both modes and both references, and the sanitizers
reported nothing.

## What this does not show

- It compares the action layer alone. Whether a retail game module drives it
  as the original engine does, and anything a bot then does in a game, is
  outside it; the [bot-entity](2026-09-16-rtcw-ai-entities.md) and
  [AI-state](2026-09-16-rtcw-ai-state.md) records cover neighbouring layers.
- The references are published source releases, not the retail executables;
  where the two differ, retail outranks both.
- Enemy Territory shares the renamed hooks but not this comparison.

## Identity

- Commit `3089df2e0758a287ee2142316cf3b3945e53cc71` plus the probe's rename,
  committed with this record.
- `code/botlib/be_ea.cpp` and `code/compat/rtcw_bot_actions.hpp` at that
  commit; the companion gives their hashes and each reference file's.
- Windows 11 x64 host, LLVM clang for i686-pc-windows-msvc with MSVC 19.44's
  libraries; Ubuntu 24.04 under WSL 2 with clang 18.1.3. Runs between 20:43
  and 20:45 UTC.
