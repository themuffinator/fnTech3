# Elite Force Linux server and platform boundary

The Linux i686 dedicated server now builds and runs the installed Holomatch
QVMs. Ten local sessions reach `CS_ACTIVE` and `ClientBegin: 2` with two bots on
`hm_borg1`. Windows x86 fnTech3, Lilium Voyager 1.40 and the retail Holomatch
1.20 client all join it, record demos, capture frames through their engine
commands and exit cleanly.

| Installed content | Client | Protocol | Pure |
| --- | --- | ---: | ---: |
| Four retail paks, with and without the installed HD texture pak | fnTech3 | 24 | 1 |
| Same two content sets | fnTech3 | 26 | 1 |
| Same two content sets | Lilium Voyager | 24 | 0 |
| Same two content sets | Lilium Voyager | 26 | 1 |
| Four retail paks | Retail Holomatch | 24 | 0 and 1 |

The relay observes both packet directions, including fragmentation and pure
proofs. Protocol-26 sequence checksums have no mismatches. Protocol 24 has no
such checksum and is not represented as passing that check. The six recordings
made with the four retail paks play to completion in both fnTech3 and Lilium:
107, 75, 29, 29, 27 and 28 frames respectively, with each pair agreeing. These
are twelve playback runs. The extra grid visible just after joining is the
game's holodeck spawn transition; the later captures show the Borg environment.

Single player has a different boundary: its installed native game and UI
modules require Windows x86. Previously Linux accepted `stvef-sp`, mounted the
paks and then tried an empty native-module name. It now exits before filesystem
startup with the required platform and module names, and points to `stvef-mp`
for Holomatch. The same binary still rejects automatic mode selection for a
shared installation and does not mount Elite Force paks as Quake III assets.
A Windows x86 client still loads both retail SP modules, enters `tutorial`,
captures a frame and exits cleanly after the new guard.

The build and regression work found four portability defects in fixtures and
one shared engine build defect:

- Botlib used `std::memset` without including `<cstring>`; GCC required the
  direct include.
- Two Elite Force fixtures compared stored ABI floats with expressions that
  GCC i686 evaluates at greater x87 precision. They now compare the copied
  32-bit representations with the source records, without adding tolerances.
- The bounded MDR loader fixture now selects `-m32` on Linux, matching the
  engine's required target instead of the compiler's default architecture.
- The pure-proof fixture now defines its stub with the public declaration's
  linkage, avoiding Clang's conflicting `static` declaration.

The two corrected compiled fixtures pass with GCC's default x87 evaluation and
with MSVC. The MDR C/C++ and pure-proof executable fixtures pass on both hosts.
The complete Windows run passes 120 targets with two skips, including 1,843
Python tests with 72 conditional skips. Linux completes 94 targets with one
skip, but its aggregate Python target fails: 1,843 tests, 126 failure reports,
one error and 82 skips. Of those reports, 115 compile standalone fixtures for
the host's 64-bit architecture instead of the engine's required i686 target.
Another Elite Force fixture omits the math-library link. Other failures include
ET fixtures, unavailable POSIX permission semantics on DrvFS, and a packaging
test whose tracked-file snapshot preceded a document added during the run.
The manifest preserves the complete failed-run identity and counts; no
complete Linux suite pass is claimed. The aggregate finishes in about 51
minutes with a fourfold timeout allowance after an earlier 900-second timeout.
Read-only WSL Git queries translate the Windows worktree metadata path.
The [fixture follow-up](2026-09-21-stvef-linux-fixtures.md) resolves all 115
architecture reports and passes all Elite Force Python fixtures on both
hosts; unrelated Linux checks remain open.

The [manifest](evidence/2026-09-21-stvef-linux-server.json) records the source
states, binary and installed-pak hashes, configuration, commands, selected
events, capture/demo hashes and playback frame counts. Initial sessions used
the include-only build; retail-client sessions and startup checks used the
later platform-guard build. Their executable hashes remain distinct.

All clients run windowed with mouse and joystick disabled. Writable homes and
independent asset copies are under `E:\Games\fnTech3-tests`; no real
installation is modified. Local RCON passwords, client GUIDs, proprietary
assets, images and demos are excluded from the committed evidence. Private
scripts and logs remain in `.tmp/stvef-gaps/linux-*` and the test homes.

This record covers the Linux dedicated server. The separate
[client follow-up](2026-09-21-stvef-linux-client.md) adds the reverse network
direction and further cross-engine demos; neither is a non-x86 claim.
The development gate stays enabled. Broader installations, complete renderer
image parity and subjective listening/device coverage remain open.
