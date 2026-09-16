# RTCW bot-library bootstrap and AAS8 reader

This checkpoint advances the shared engine's RTCW game-module boundary.
It does **not** provide playable SP/MP, live AI, route parity or savegames.
RTCW remains gated in default builds. All work is in the dirty worktree at
`c75573c7375e44c908256020ada2d7e12970cacf`; the commit alone does not identify it.

## Implementation and source facts

The game catalog owns bot-library storage defaults and AI initialization policy:

| Profile | `bot_enable` | Default bot clients | Default bot entities | AI systems initialized |
| --- | ---: | ---: | ---: | --- |
| Q3 | 1 | 64 | 1024 | Weapons, goals, chat, movement |
| QL | 1 | 64 | 1024 | Weapons, goals, chat, movement |
| RTCW SP | 1 | 128 | 2048 | Movement |
| RTCW MP | 0 | 128 | 1024 | None beyond navigation and elementary actions |

These storage defaults are separate from server player-slot defaults. The
installed RTCW game modules explicitly set `maxentities` to 1024 before setup.
Quake's external `MAX_CLIENTS` remains 64; the bot storage check uses the engine's
separate 128-client capacity. Setup rejects invalid/out-of-range allocation
counts before allocation. Repeated setup is idempotent, and failed initialization
can release partially created state, including after an engine drop. Recursive
shutdown is guarded. AI handles are released even when allocated independently
of their subsystem's startup policy. Empty elementary-action shutdown is safe.

RTCW imports 200–204 now dispatch through checked arguments: setup, shutdown,
library variable set/get and precompiler global definitions. The world and
bot-library boundaries share exact QVM extent/integer/string checks. Native
pointer checks reject null and address wrap; they cannot prove the readability
of an arbitrary native module allocation. Library variable strings and result
capacities are bounded to `MAX_STRING_CHARS`; zero-capacity reads do not write.
Aliased input/output, unaligned buffers and caller trailing bytes are handled.
Other bot/navigation/POD imports still fail explicitly.

The existing library variable parser could read past a trailing decimal point
and overflow its integer divisor on long fractions. It now handles those cases
without overflow, preserves unsigned decimal parsing for ordinary settings and
returns zero for nonnumeric/nonfinite results. The entity update boundary also
rejects the one-past-end entity number.

The independent `rtcw_aas_format.hpp` reader accepts EAAS version 8 into an
immutable view. It checks the XOR-encoded header and BSP checksum, all 14 lump
extents/strides/overlaps, finite geometry, bounds, signed indices, routing
references and acyclic node ownership before publishing a result. Failed
parses preserve the previous view. It handles unaligned input and reordered
lumps. `rtcw_aas_layout.def` freezes the original record sizes and offsets,
including the 32-byte area settings record and its ground-steepness float.

This reader is **not yet wired into `AAS_LoadMap`**. The subsequent
[navigation-world checkpoint](2026-09-15-rtcw-navigation-worlds.md) adds stable
storage and selection for two RTCW worlds. Remaining work includes live loading,
canonical conversion, distinct area/travel flags, ground-steepness routing,
entity linking, navigation/AI imports and original-engine runtime comparisons.
Accepting version 8 as if it were a Quake version 5 record would be incorrect.

## Observations and evidence

Both original retail modules now pass bot-library initialization and stop at
`BOTLIB_LOAD_MAP` (import 206), advancing from import 202. The initial bootstrap
probe used experimental Windows x86 client SHA-256
`c4665e2daaade1377d7bdc61a97ff11ee3fc1a62c64cb486eb0f02e53f40626f`;
`.tmp/rtcw-manager/bot-bootstrap-map/manifest.json` records its exact commands,
installed modules and failure logs. It ran SP `spdevmap escape1` and MP
`devmap mp_beach`, windowed on unswitched private desktops, with input and audio
disabled, isolated writable homes and unchanged installed assets. Those are
expected bounded failure observations, not successful map boots.

The installed archives contain 70 AAS8 files (64 in `pak0.pk3`, six overrides in
`sp_pak4.pk3`). All pass the reader and all four original/iortcw SP/MP header
layout fixtures on Windows x64, Windows x86 and Linux x86 with ASan/UBSan.
Each reference counts 14,973,558 records and
matches the existing engine point-area query against the original function in
784,321 cases. `scripts/probe_rtcw_aas.py` reads assets directly into memory;
its reports retain names, sizes, hashes and results, not proprietary geometry.

An initial validator incorrectly equated traversal stack capacity with tree
depth. The original and override `crypt1_b0`/`crypt1_b1` trees are valid, unique
and acyclic, with maximum depths 160 and 150. The corrected check admits them
and rejects shared child nodes/cycles; it does not impose that incorrect depth
limit. A synthetic 4096-node chain checks this distinction.

Six focused tests execute all four profiles' setup/error/shutdown paths, RTCW
native/QVM argument handling, real library variable allocation, AAS8 malformed
inputs and the existing server-world/cvar/campaign boundaries. The Windows x64
run passes in 4.454 seconds; Windows x86 passes in 13.421 seconds and Linux x86
with ASan/UBSan in 38.271 seconds. Before the MP default correction, the complete
suite passed 111 Meson checks (two skips) and 1,362 Python tests (11 skips).
The default Windows x86 client SHA-256 was
`17aa66e1834ffd7b211c9b0a0278e4369dd1c1acb16adb2c5bcaf4ecdf026c0c`.
Its Q3/QL GLX, Vulkan and RTX raster probes all passed; all six engine captures
were inspected. Evidence is in `bot-aas-full-testlog.txt` and
`bot-aas-live/manifest.json` under `.tmp/rtcw-manager/`. The reader/reference
reports are `aas8-reference-x64.json`, `aas8-reference-x86.json`,
`aas8-reference-sanitizers.json` (original references) and
`aas8-reference-iortcw-sanitizers.json` (iortcw references).

Further source inspection found that original MP defaults `bot_enable` to **0**
(`src/server/sv_bot.c`), whereas SP defaults to 1. The bootstrap MP probe had
inherited the shared engine's default of 1; it did not explicitly override it.
The catalog and both server fallback registrations now preserve each profile's
default and explicit overrides. Five focused tests passed on Windows x64/x86
and Linux x86 with sanitizers after this correction. The subsequent
`bot-defaults-map/manifest.json` probe, client SHA-256
`1d26e5d21588898c4da52feb50e0b40aaf7fb1e4452be03c17730527ca8cfb78`,
still stops SP at import 206; MP now passes game initialization and stops at
import 323, `BOTLIB_AAS_SETAASBLOCKINGENTITY`. Navigation remains needed for SP
and optional MP AI. Later blockers are recorded in the navigation checkpoint.

Original RTCW/iortcw files are GPLv3. Production uses independently authored
contracts/readers and changes to the existing GPL-2.0-or-later bot library.
Original query implementation text is compiled only in disposable fixtures
with its license notices; no retail files or GPLv3 implementation were staged.
