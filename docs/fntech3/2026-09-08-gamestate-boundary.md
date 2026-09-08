# Freeze game-state transfers for both cgame profiles

`CG_GETGAMESTATE` previously assigned the engine's `gameState_t` directly into
module memory and bounded that write using the internal structure size. The
current Q3 and QL layouts agree, but neither module contract should change when
engine-internal state evolves.

Separate Q3 and QL PODs now freeze the configstring-transfer layout: 1,024 signed
32-bit offsets at byte 0, 16,000 string bytes at byte 4,096, and a signed 32-bit
data count at byte 20,096. Each structure occupies 20,100 bytes with four-byte
alignment. These are source facts from the pinned GPL Q3/QL references named
in the boundary headers and CREDITS; they are not a new retail reverse-engineering
claim. Both profile adapters copy fields explicitly, and the cgame trap selects
the corresponding frozen size and adapter. The native QL wrapper uses its frozen
type. No trap ordinal or configstring interpretation changes.

Compile-time capacity checks require an explicit audit before internal capacity
changes can affect either profile. Adapters preserve the complete string buffer,
including bytes beyond the active count, matching existing transfer behavior.
Validation of server-supplied configstrings remains in the existing parser;
these adapters do not reinterpret or sanitize that input.

Tests pin every external field offset, size, alignment and POD property. Adapter
checks cover all offsets, all string bytes, full/empty replacement, destination
guards and null handling for both profiles. Linux i686 layout/adapter targets
pass (2/2). Windows client/dedicated incremental builds and the full configured
suite pass: 110 configured checks, zero failures and one unavailable OpenAL
fixture skip. The source aggregate passes 1,263 tests with seven skips.

Windowed local Q3 QVM and QL native sessions use the same Windows x86 binaries,
isolated writable homes, read-only legitimate assets and an unswitched private
desktop. They exercise configstring delivery during map load and again across
Vulkan renderer restart. The engine's screenshot command supplies the visual
evidence; no input injection or operating-system capture is used.

The [evidence manifest](2026-09-08-gamestate-boundary.json) records source/binary
hashes, commands and outcomes. This is a bounded module-boundary improvement,
not full ABI or interoperability closure. The unknown QL snapshot tail, broader
native contracts, clean-release and retail-peer gates remain open. The full suite also found and corrected an unrelated test-harness target
selection defect: the preceding QL user-command wrapper test compiled x64 inside
an x86 MSVC environment. It now uses the shared compiler-target helper. Existing
optional-resource warnings remain.
