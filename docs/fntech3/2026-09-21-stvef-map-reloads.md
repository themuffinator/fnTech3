# Elite Force map commands and client storage during reload

Elite Force SP now uses retail's full-map continuation, and no longer exposes
the multiplayer `map_restart` command. A separate shared client-storage fix
prevents a Windows stack overflow when Holomatch reloads with a changed client
limit. Both corrections preserve the engine's profile boundaries.

## SP's command contract

The user's `stvoy.exe` 1.20 registers twelve operator commands at
`0x4240e0..0x4241b1`. They include `map`, `devmap`, both transitions and the
savegame commands, but no `map_restart`, `spmap` or `spdevmap`. Its map-startup
block at `0x425a52..0x425a6d` sets `nextmap` to `map <current map>`.

An owned local probe maps that executable without starting the game. It
executes the complete command-registration function twice with a recording
`Cmd_AddCommand` callback: twelve registrations on the first call, none on
the second. It also executes the native nextmap block with four supplied map
names, substituting formatting and cvar-write callbacks and returning before
unrelated startup work. The [native fixture](../../tests/fixtures/stvef_sp_commands_retail.json)
contains the names, callback addresses, observed continuations and source
identities. No proprietary implementation text or executable bytes are
incorporated in the fixture or engine.

Previously the shared registration table exposed `map_restart` before the SP
branch returned, and shared startup set `nextmap` to `map_restart 0`. A
windowed SP probe saved and captured the tutorial, then hit the fatal
`VM_Call with NULL vm` after `map_restart 0`: the restart unloaded EF's
combined game/cgame DLL without reinitializing the local cgame. The probe
eventually terminated its own process after the fatal shutdown.

Registration now adds `map_restart` after the SP branch returns. SP startup
sets `nextmap` to retail's full `map` command. The other six profiles retain
their restart callback and default continuation. Modern utility commands are
not removed merely because they are absent from retail's command list.

`tests/stvef_sp_commands_tests.py` compiles the actual registration functions,
completion helpers and nextmap block. It checks all seven profiles, eight
compile-flag combinations and both standalone-cvar values: 112 registration
scenarios and 448 nextmap writes per platform. It checks callback ownership,
duplicate initialization, SP aliases, Workshop/factory and ban gates too.
The previous code fails all sixteen SP combinations. Windows x86 and Linux
i686 pass; Linux uses AddressSanitizer and UndefinedBehaviorSanitizer.

## The Holomatch reload crash

The control probe found a separate crash in both the new SP-command build
and the previously staged engine. After adding a bot, a full Holomatch reload
hit `STACK_OVERFLOW` in MSVC's `_chkstk`. The Windows x86 executable reserves
1,048,576 bytes of stack. Its internal `client_t` is 527,320 bytes, and the
optimized allocation and resize functions reserved 527,320 and 527,332 bytes
respectively for aggregate-assignment temporaries. The resize function calls
the allocation function, exhausting that stack.

Both empty-client assignments now zero their existing storage directly. A
compile-time trivial-type check guards this assumption. Live clients are
still copied unchanged, and the existing client-count bounds and allocation
ownership remain intact. The corrected resize prologue reserves twelve local
bytes instead of a complete client record. External module and wire layouts
are unchanged.

`tests/server_client_storage_tests.py` compiles the real allocation and resize
functions with the real internal client type and a 256 KiB stack. It checks
zeroing of dirty allocator output, sparse growth, preservation of connected
and primed clients, shrinking around an occupied high slot, unchanged-size
allocation, and negative/overflow rejection. The old MSVC code exits with
`0xc00000fd`; the corrected code passes MSVC x86 and sanitized Linux i686.
The probe uses `/EHs` because its owned C-declared error callback throws;
Microsoft's [exception-handling documentation](https://learn.microsoft.com/en-us/cpp/build/reference/eh-exception-handling-model?view=msvc-170)
explains why `/EHsc` would make that callback an invalid test substitute.
Production exception flags are unchanged.

## Runtime evidence and limits

Retail SP and fnTech3's GLx, Vulkan and RTX raster runs each complete a
four-save/four-capture sequence: initial play, immediate and delayed
`map_restart` attempts, and a full reload through `nextmap`. All saved chunks
parse to the end, time advances after the unsupported commands, and the full
reload reinitializes the map and client. Three further fnTech3 runs repeat
this sequence after the client-storage correction. Their engine screenshots
show the tutorial world and HUD; these are lifecycle checks, not synchronized
renderer comparisons.

Windows and Linux Holomatch clients each complete five phases with the local
player and Munro bot: initial play, growth from four to six slots, a fast
restart, a requested shrink to one bounded at two occupied slots, and a
`nextmap` restart. Each produces five engine screenshots and exits normally.
The unchanged restart command remains registered throughout.

An initial Linux script expected a delayed restart to execute ahead of its
own queued continuation. Its client exited normally with four captures and
three game initializations, but the harness expected four initializations.
That failed assertion is retained as a probe limitation. A separate Windows
script ends after requesting `map_restart 1`, allowing the engine's appended
restart command to run. It reaches the second local snapshot after a full
reload; the observer then deliberately terminates its own process. This
bounded timer check does not establish orderly shutdown, which the separate
five-phase sessions cover.

The initial retail SP run attempted its first save before the player was
alive. The longer startup wait used by the completed comparison sequence
produces all four saves. An initial build typo and an initial probe exception
flag error are retained in local logs; neither is counted as passing evidence.

Windows x86 and Linux i686 client/dedicated builds complete. The final Windows
suite passes 120 targets with two skips; its Python aggregate runs 1,861 tests
with 72 skips in 726.625 seconds. The SP-command-only checkpoint separately
passes 1,860 Python tests. Individual results, executable/installation hashes,
source commits, commands and all runtime trials are recorded in the
[manifest](evidence/2026-09-21-stvef-map-reloads.json).

The manual installation at `E:/Games/fnTech3-tests/stvef-manual` is refreshed
to source commit `25dcde9`. Its eight existing settings/save/launcher files
retain their hashes. Fresh SP save-load and Holomatch bot-match preflights
pass with the staged engine; the user's launchers are not run. In total,
eleven completed validation sessions produce forty engine captures, with one
additional capture from the separately bounded timer probe.

All game probes use independent writable homes under
`E:/Games/fnTech3-tests`, windowed mode, disabled mouse/joystick input and
inactive private Windows desktops or a private Linux display. Screenshots
come from the engine command. Original installations remain unchanged.

Campaign-wide play, physical movement/look/fire, manual menu save/load and
rebinding, listening, feedback hardware and broader renderer/device parity
remain open. Existing asset defects, the wider Linux-suite failures and the
Windows x86 RT-extension limitation are unchanged. Linux rebuilds retain the
existing ET/RTCW enum-comparison and unused completion-argument warnings.
Both Elite Force profiles remain development-gated.
