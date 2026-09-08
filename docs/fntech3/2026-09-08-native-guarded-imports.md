# Guard native cgame import transfers in the running engine

The existing synthetic API-8 cgame probe treated a changed first byte as success
for glconfig and game-state imports. That could accept a partial write and did
not detect overruns. Its oversized game-state buffer also hid changes in the
external transfer size.

The fixture now surrounds the exact 0x2c44-byte glconfig and 20,100-byte game-state
buffers with eight guard bytes on each side. It checks renderer strings and
plausible QL dimensions at the frozen offsets, validates the game-state count,
all 1,024 string offsets and bounded string termination, and checks guards around
the 28-byte user command plus zero trailing alignment bytes. It still verifies
QL weapon, primary weapon and FOV values through the actual import table.
The fixture remains test-only code and implements no retail game behavior.

A regression executes the fixture's validator helpers against valid empty and
populated states, overwritten guards, zero/oversized counts, out-of-range offsets,
missing terminators and first-byte-only writes. It passes on Windows and Linux.
The native fixture compiles as a Windows x86 DLL with MSVC. The full Windows
suite passes 110 checks with zero failures and one unavailable OpenAL skip;
its aggregate passes 1,265 tests with seven skips.

The DLL was copied only into an isolated client home and loaded by the current
shared Windows x86 client. A local fnTech3 dedicated server supplied legitimate
QL qagame/maps; the real UI and assets remained in their read-only installation.
All twelve probe markers pass: float, cvar range, filesystem, parser, mute,
glconfig, game state, key API, user command, snapshot, active-frame and console
callback. The client and server both exit normally. The snapshot marker retains
its earlier limited validation; it does not resolve the unknown 0x58-byte tail.

The run uses a private unswitched desktop, windowed GLx, disabled input capture,
loopback networking, Steam/WebUI disabled and no input injection. No screenshots
were needed for this ABI test. Pure enforcement is explicitly disabled to load
the synthetic module; this is not retail pure-admission or peer-interoperability
evidence. The legacy opt-in unittest runner was not invoked: it still assumes a
loose retail cgame DLL and does not provide the private-desktop isolation used
here. Its modernization remains an identified test-tooling limitation.

The [evidence record](2026-09-08-native-guarded-imports.json) pins the synthetic
DLL, engine binaries, commands, generated configs and success markers. Broad ABI,
retail-peer and release gates remain open. Existing optional-resource warnings
persist; no new engine defect was observed in this run.
