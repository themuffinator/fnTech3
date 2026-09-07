# 2026-09-07 build portability and native Linux probes

This checkpoint extends [Steam discovery](2026-09-07-steam-discovery.md) and
keeps full compatibility unproven. The source baseline is `main` commit
`a20ed530f91e57ac8ac43ff422f226bd570d021a`. Subsequent test-harness corrections
are recorded in the commit containing this report; they do not change engine
module, wire, rendering or service contracts.

## Published Linux and Vulkan build evidence

[Linux CI run 34161283293](https://github.com/themuffinator/fnTech3/actions/runs/34161283293)
passed at `a20ed53` after the Unix banner correction. It built:

- the i686 dedicated server through Make, dependency-isolated Meson, and CMake;
- all three i686 GLx/Vulkan/RTX renderer modules;
- the bundled-SDL3 i686 client; and
- the complete configured i686 Meson suite: **107 passed, 0 failed, 2 skipped**.

The skipped tests were OpenAL loopback and the unavailable retail factory
probe, both returning 77. CI confirms ELF 32-bit Intel 80386 output identities.
These are clean-checkout build/unit observations, not renderer runtime or QL
native-client support claims. The local copy of the CI log is
`.tmp/linux-a20ed53-ci.log`.

[Vulkan-family CI run 34161283358](https://github.com/themuffinator/fnTech3/actions/runs/34161283358)
also passed both renderer build jobs and the gate-plan job at that commit.
The opt-in runtime sweep was skipped. The earlier FreeType timeout/HTTP 502
was not reproduced in this run; no dependency hash or source was changed to
bypass it.

## Native Linux dedicated discovery

A separate local Meson cross configuration builds `fntech3.ded` as an i686 ELF
using the repository's `misc/meson/linux-x86.ini`, client/SDL/curl/Ogg disabled,
and isolated `.tmp/meson-linux-discovery` output. It runs on Ubuntu under WSL2,
Linux `6.6.87.2-microsoft-standard-WSL2`. A strict GCC `-m32 -std=c++17 -Wall
-Wextra -Werror` build and execution of the synthetic Steam-discovery suite
also pass. The local Meson discovery test passes 1/1.

The dedicated artifact has SHA-256
`a7bb07f2af53acf5e8be605730be955e4396f60c8bbad83a7be374f56be72ec3`.
Its `file` identity is ELF 32-bit LSB PIE, Intel 80386, interpreter
`/lib/ld-linux.so.2`, BuildID `3a74f7c4c2267069e6ad223a963d39cb98c97670`.

Native discovery is exercised with `STEAM_DIR=/mnt/c/Program Files (x86)/Steam`
and `scripts/probe_game_profiles.py --steam-discovery`. The script's expected
installations are the existing `Quake 3 Arena` and `Quake Live` directories
under that root. No `fs_basepath` is passed to either engine process. This
validates the native Unix `STEAM_DIR` route against legitimate mounted assets;
it is not evidence for default Unix HOME/XDG/Snap discovery.

| Profile | Observed native session | Outcome |
| --- | --- | --- |
| Q3 | QVM qagame, q3dm6, protocol 68, mounted Q3 base pak | PASS, exit 0, 16.550 s |
| QL | Native Linux qagame, campgrounds, protocol 91, mounted QL base pak | PASS, exit 0, 8.088 s |

Both use fresh Linux temporary homes and loopback sockets, `sv_pure=1`,
`r_fullscreen=0`, Steam integration disabled, Workshop skipped, and automatic
demo recording disabled. The console logs record game/AAS shutdown. Neither
run launches a client, controls input, captures the desktop, mutates an
installation, or copies proprietary files into the repository. These do not
prove live pure admission, authentication, or bidirectional interoperability.

Exact engine commands, installation/base-pak identities and outcomes are in
`.tmp/steam-profile-linux-runtime/manifest.json`; build and probe logs are
`.tmp/steam-profile-linux-build.log` and `.tmp/steam-profile-linux-runtime.log`.

## Windows test-harness portability

The initial release CI exposed harness failures separate from engine builds:

- Standalone Clang probes defaulted to x64 while an active MSVC x86 environment
  supplied x86 CRT libraries. A shared compiler-argument helper and the same
  existing target-selection convention now keep the affected config, demo,
  pure, FOV, UI, VM, server-command and banner probes on the selected x86 ABI.
- An i686-only MinGW compiler cannot test the diagnostic x64 acceptance path.
  That subcase now independently checks compiler capability and reports an
  explicit skip; the x86 rejection assertion still runs.
- The packed-vertex test compared a stored float to an x87 excess-precision
  literal. It now compares both copied UV float byte representations exactly,
  preserving the actual packing requirement without adding a tolerance.
- GCC's strict range-loop warning exposed an unnecessary temporary string in
  the new discovery fixture; the fixture now iterates literal pointers.

The complete Windows suite now also passes **inside `vcvars32`**: **110 passed,
0 failed, 1 OpenAL skip**, including **1243 Python tests with 7 skips**. This
resolves the earlier developer-environment limitation instead of requiring a
clean-shell workaround. Log: `.tmp/profile-msvc-environment-final-tests.log`.

The 35 focused lightmap/platform/VM/build-constraint tests pass using the real
`C:/msys64/mingw32/bin` GCC/G++ toolchain, with three explicit unsupported-x64
probe skips. Compiler selection was forced only for that local experiment;
other tool discovery was preserved. Log: `.tmp/profile-mingw32-verified-tests.log`.
This is focused MinGW evidence, not the complete MinGW release gate.

## Remaining gates and separate issues

The Windows release workflows must rerun against the test corrections;
older Windows CI failures are not retroactively relabelled successes. Native
Linux client runtime, wider discovery, pure/live peers, and complete release
packaging remain open. The inherited closed-source provider distribution
policy remains unresolved for fnTech3, as recorded in the compatibility ledger.
