# Freeze Q3 and QL renderer configuration contracts

Q3 cgame and UI previously copied internal `glconfig_t` directly into module
memory. The QL field adapter already handled its distinct layout, but its
local destination type still depended on internal enum types and string-size
constants. These paths now use separate fixed-width external PODs.

Both contracts occupy 0x2c44 bytes with four-byte alignment. The Q3 layout
comes from the pinned FnQuake3 renderer header; QL comes from the pinned FnQL
retail glconfig adapter (linked in CREDITS). QL inserts `multitextureAvailable`
before width/height and omits Q3's `smpActive` tail. Equal total size therefore
does not imply interchangeable layouts. Buffer capacities are fixed at three
1,024-byte strings and one 8,192-byte extension string; enum/boolean slots are
32-bit scalar fields.

A shared Q3 field adapter serves cgame's `cls.glconfig` and UI's `re.GetConfig()`.
It preserves every current Q3 byte, including complete string buffers and the
legacy smpActive field. Q3 VM bounds now use the frozen size. QL retains its
existing string copying, multitexture derivation, and public capture dimensions
when supersampling changes the private renderer dimensions. No renderer feature,
trap ordinal, or public dimension policy changes.

Layout tests pin every field offset, size, alignment and POD property. The
production-function harness compares Q3 against the former direct-copy result;
checks every QL value, destination guards, null handling, one/multiple texture
units, and ordinary/capture dimensions; and pins enum and boolean encodings.
Windows x86 and Linux i686 harnesses pass, as does the Linux i686 layout target.
These Linux results are test-target evidence, not a rebuilt Linux client run.

The incremental MSVC x86 build passes 110 configured tests, zero failures and
one unavailable OpenAL fixture skip. Its source aggregate passes 1,264 tests
with seven skips. The later enum assertions also pass the focused Windows run.
Both local Q3 QVM and QL native client/server probes pass pure admission,
snapshots, Vulkan restart and normal exits. Four engine-command screenshots
were inspected: Q3 gameplay/HUD and QL native join UI/world, before and after
restart. No QL gameplay join or retail peer is claimed.

Runtime probes used isolated writable homes, legitimate read-only assets,
windowed mode, disabled input capture and an unswitched private desktop. No
input was injected and no OS screen capture was used. The
[evidence manifest](2026-09-08-glconfig-boundary.json) records commands, binary
hashes and outcomes. This does not close broader renderer/module ABI, unknown
QL snapshot-tail, retail interoperability or clean-release gates. No new
unrelated issue was found; known optional-resource warnings remain.
