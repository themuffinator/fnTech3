# Marshal QL user commands at both module boundaries

The QL cgame and qagame user-command imports still copied engine-internal
`usercmd_t` directly into module memory. Its current layout matches the frozen
28-byte QL contract, but an internal layout change could silently break retail
modules and the copy could propagate internal alignment bytes. Q3 already used
its separate 24-byte adapter.

Both QL output paths now fetch into an internal temporary and marshal every
field into `fntech3_ql_usercmd_t`. Bounds checks and native wrapper signatures
use the frozen type. The adapter preserves QL weaponPrimary/FOV, signed movement,
angles, buttons and time, and zeroes the two trailing alignment bytes. No trap
ordinals, wire fields or Q3 behavior change. The existing frozen QL layout and
its FnQL provenance are unchanged; no external implementation was incorporated.

Adapter tests check every value, destination guards, deterministic alignment
bytes and null handling. Existing layout tests pin the 28-byte size and exact
field offsets. Executable wrapper tests verify successful conversion, no writes
for stale cgame requests, and no conversion after server index rejection. The
latter injects the fetch failure at the wrapper seam; existing history/index
checks remain responsible for deciding which requests fail.

Validation uses an incremental Windows MSVC x86 build and Linux i686 ABI test
targets. Windows passes all 110 configured checks (one unavailable OpenAL skip);
its source aggregate passes 1,262 tests with seven skips. The added wrapper
failure test was run separately afterwards, with both wrapper tests passing.
Linux layout and adapter executables pass (2/2); this is not new Linux client
runtime evidence.

The same Windows x86 client/dedicated pair passes Q3 QVM and QL native local pure
sessions, snapshots, Vulkan restart, engine screenshots and normal exits. All
four images were inspected: Q3 shows gameplay and QL its native join UI/world.
The runs used private unswitched desktops, windowed mode, disabled input capture,
isolated writable homes, and user-supplied assets. No user input was injected.
QL joining gameplay and bidirectional retail peers remain unverified here.

The [evidence record](2026-09-08-ql-usercmd-boundary.json) contains binary/source
hashes, commands and outcomes. Broader module-boundary auditing, the unknown QL
snapshot tail and full compatibility gates remain open. No new unrelated issue
was found; existing optional-resource warnings persist.
