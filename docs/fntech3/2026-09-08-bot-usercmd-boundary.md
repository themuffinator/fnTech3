# Correct incoming Q3/QL bot user-command layouts

The BOTLIB_USER_COMMAND trap passed module memory directly to SV_ClientThink
as an internal usercmd_t. A Q3 command is 24 bytes, while the internal command
is 28 bytes and inserts primary-weapon and FOV bytes before movement. As a
result, Q3 movement was shifted and the server read beyond the command prefix.
The trap also lacked a command-sized QVM bounds check.

A native Q3 regression reproduced this on the previous Windows x86 dedicated
binary. It sent a guarded command with time 12345, angles -1234/2345/-3456,
buttons 0x81, weapon 7 and movement -40/50/-60 to unused client slot 1, then
retrieved the command through G_GET_USERCMD. The before-fix comparison failed
in both normal and recovered-map cases. After rebuilding with the correction,
the same fixture passes byte-for-byte, including after map restart and recovery
from the separate deliberately invalid game-data binding.

The trap now checks the selected module command size and explicitly converts
to an engine-owned value before calling SV_ClientThink. Legacy Q3 uses the
existing Q3-to-internal adapter, which clears QL-only fields. Structured QL
uses a new inverse adapter for its frozen 28-byte POD, preserving all known
fields and discarding padding. The QL native import wrapper now names that
frozen type instead of the engine-internal type. This follows the same legacy
versus structured contract selection as G_GET_USERCMD. Invalid client indices
remain ignored; null commands and short QVM spans are rejected before client
processing.

Validation includes the actual dispatch branch with both module contracts,
exact bounds sizes, short spans, null pointers and invalid clients; it passes
on Windows and WSL Ubuntu. The adapter regression covers signed movement and
angle limits, QL primary weapon/FOV, guard preservation, poisoned source padding
and null-adapter behavior. The guarded native qagame round trip supplies runtime
before/after evidence for Q3. It uses an unused client slot, so it exercises the
command transfer without simulating player input or proving bot navigation/AI.

Windows x86 client/dedicated binaries and the existing Linux i686 dedicated
configuration rebuild successfully. The Linux build uses the documented dirty
scratch checkout based on b5776c9 with current affected files copied in; it is
incremental portability evidence, not a clean release build. The QL dispatch
and layout have executable regression evidence; this checkpoint does not claim
a new retail QL bot gameplay observation.

The [evidence record](2026-09-08-bot-usercmd-boundary.json) includes before/after
binary and fixture hashes, launch/config details, stock pak identities, markers
and test/build log hashes. Runtime probes use isolated writable homes, read-only
legitimate stock assets, private unswitched desktops and disabled input. No
keyboard/mouse control or screen capture is used. No external implementation
was incorporated.

The full Windows Meson suite passes 110 tests with one OpenAL skip; its Python
aggregate runs 1,275 tests with five skips. Q3 native client/pair/qagame and QL
native probes were enabled. All three Linux i686 ABI layout, adapter and server
game-data tests pass.

Full ABI, retail-interoperability and release gates remain open. No new
unrelated engine defect was found; the known optional config/font warnings
remain.
