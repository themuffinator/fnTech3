# Jedi Outcast SP: the retail 1.02a function-table bootstrap

This record adds the first executable single-player game boundary. It is an
experimental Windows x86 slice, with the release gate retained. It does not
establish campaign, savegame, client, frame, renderer, audio or protocol
compatibility.

## Observed retail contracts

The legitimate installation is `E:\Games\Jedi Outcast`, read without changes.
Its `version.inf` reports 1.02a. The measured binaries are:

| File | Bytes | SHA-256 |
| --- | ---: | --- |
| `jk2sp.exe` | 999,482 | `0d3c635ee5b20ac32fc957e00cf21e65ebc1584112b11f6ae026706dcc0d0e0c` |
| `jk2gamex86.dll` | 1,318,912 | `8cc1c15f462b15bcb9dc9840a2b9ccc18e57e17f7292d8080a77625a34a27e18` |

`jk2sp.exe` constructs its import table at `0x4368a0`, passes it to the game,
and compares the export version against **6** at `0x436d58`.
`jk2gamex86.dll` exports `GetGameAPI` at `0x200970a0`; it copies **103 words**
from the import pointer (`mov ecx, 0x67` followed by `rep movsd`), and returns
its own export table at `0x203abbd8`. Each pointer is four bytes: the import
table is **412 bytes**, and the export table is **76 bytes**.

The first 42 imports use the general Raven SP services, including the three
save callbacks at offsets 68, 72 and 76, the voice-volume pointer at 156, and
the allocator at 160. The allocator takes size, Raven memory tag and a
zero-initialize flag; retail callers push all three at `0x20098206` and
`0x200c4777`. The free callback at 164 has no required return value in the
observed call sites. Imports 42–102 remain opaque, with distinct refusal
callbacks that report the slot and byte offset.

The export table keeps `ConsoleCommand` at 52, `PrintEntClassname` at 56, an
unnamed function at 60, `gentities` at 64, `gentitySize` at 68, and
`num_entities` at 72. `GetGameAPI` writes **1,288** to the entity stride at
`0x20097182`. During game Init, the module clears 1,024 entities at that
stride, allocates a **3,188-byte** client (`0x20096f05`), and stores its
pointer at entity offset **252** (`0x20096f1a`). These are SP contracts;
the measured multiplayer strides of 828 and 1,756 are different.

The companion JSON records all 103 import constructor instructions and
retail addresses. They are disassembly observations, not runtime coverage.
The unnamed imports and unnamed export retain no inferred callable
signature.

## Implemented boundary

`code/compat/jk2_sp_game_api.h` freezes the measured table, validates its
version, required export functions, exact retail stride and entity count,
and keeps unknown records opaque. `code/server/server_jk2_sp_game.hpp`
loads the module through `VM_CreateTable`, supplies bootstrap console,
cvar, argument, filesystem, configstring, userinfo and visibility services,
attaches the bundled client's `dllEntry`, and dispatches game Init through
its nine-argument retail signature.

Every unimplemented save, world, Ghoul2 or client import fails explicitly
before reading a game record. No SP entity or player record is exposed to
the engine's existing data view. A successful return from Init would still
stop with the missing entity/frame adapter named; it does not enable
gameplay by silently skipping that boundary.

`code/compat/jk2_sp_memory.hpp` keeps Raven's memory tags as module metadata
and tracks independent allocations. It honours zero-initialization,
rejects negative sizes and failed allocations, refuses foreign and interior
pointers, and reclaims every outstanding block after a partial Init or
unload. Failed initialization does not run retail Shutdown against
unconstructed private state. The host publishes VM ownership before
`GetGameAPI`, closes module-owned file handles on refusal, and leaves the
normal server cleanup able to unload the module.

The completed groundwork also includes `code/compat/jk2_sp_game_abi.h`,
`jk2_sp_wire_layout.def` and `jk2_sp_state_adapter.hpp`. The first two freeze
every member of the 252-byte entity state and the 352-byte engine-visible
entity prefix against the retail delta table and world functions. The
adapter transfers the state into canonical fields and the engine-owned
`raven.sp` carrier, with all 252 bytes preserved on a round trip. It is
separate from Academy's 272-byte state and Outcast multiplayer's 296-byte
state. The shared carrier is storage, with no external ABI role.

Retail link/unlink functions read `inuse` at 256 and write `linked` at 260.
`SV_SetBrushModel` at `0x4364f0` fixes the brush flag at 268, bounds at 272
and 284, and contents at 296. Link code derives absolute bounds at 300 and
312 from origin at 324, using angles at 336. Snapshot checks read server
flags at 264: no-client bit 1 at `0x43a21e`, broadcast bit 32 at
`0x43a24e`, and portal bit 64 at `0x43a321`. Trace checks at `0x43ac40`
and `0x43ac8a` establish that owner at 348 is a native entity **pointer**.
The checked owner helper resolves that fixed-width address by arithmetic
against a published array and never dereferences it to obtain a number.
Ghoul2 starts at 352 and remains private to the module.

Trajectory meanings were measured separately in the SP module's evaluator
at `0x2000ccc0`, whose seven-way jump table is at `0x2000ce5c`:
stationary, interpolated, linear, linear-stop, Raven nonlinear-stop, sine,
and gravity. The adapter maps them to canonical meanings; SP ordinal 6 is
gravity and never the engine's Quake Live acceleration ordinal 6. This
state/prefix groundwork is tested but not yet connected to the server's
SP world data view.

## Verification

On 2026-10-02, the three `tests/jk2_sp_host_tests.py` tests passed with both
the default host compiler and a Windows x86 compiler environment. They
exercise version mismatch (including OpenJK API 10), every missing export
callback, multiplayer stride substitution, invalid counts, allocation
failure, zeroing and alignment, foreign/interior/repeated frees, repeated
cleanup, populated opaque refusal callbacks and failed Init cleanup. A
malformed native entity pointer is refused without dereferencing it.
The state test also asserts all 62 measured delta-field offsets, exercises
all seven trajectory meanings and byte-exact round trips, refuses foreign
trajectory contracts atomically, and verifies null, interior, foreign,
out-of-range and overflowing owner addresses.

The Windows x86 `tests/fixtures/jk2_sp_api_probe.cpp` probe loaded the
legitimate retail DLL and called its real `GetGameAPI`. It supplied an
empty fixture for the optional `ext_data/weapons.dat` read and then freed
that null fixture. The real module returned:

```text
GetGameAPI validation=0 api=6 imports=412 exports=76 entity_stride=1288 count=0
```

The retained log is `.tmp/jk2-sp-boundary/getgameapi.log`; the reusable
probe executable is `.tmp/runtime/jk2-sp-api/probe.exe`. No game window,
input action, engine Init, gameplay frame or save operation was used in
this native probe. Table discovery alone does not prove Init.

The integrated Windows x86 dedicated engine probe then mounted both retail
paks, loaded `maps/kejim_post.bsp` collision, and loaded the legitimate
native DLL through `VM_CreateTable`. Its bundled client entry point called
**client import 3**, which remains an explicit refusal:

```text
VM_CreateTable 'jk2gamex86.dll' ok
ERROR: Jedi Outcast SP client import 3 is not implemented
```

The server shutdown completed in the log. The harness terminated the
process after 15.891 seconds, so this is not a clean process-exit result.
It did not reach the game's Init banner. The dedicated executable SHA-256
was `4c66d195c4f0528997b081fa3f2bc5f8738038c8261203f741b56c09e082df29`
at repository commit `a1251ede2c382190b18c154b45357f6979d8171c` with local
changes. The command, installation identity and unchanged installation
check are recorded in `.tmp/jk2-compat-2026-10-02/runtime-sp/result.json`;
the log is beside it as `console.log`. This establishes the native load
and actual next boundary, not successful game Init.

## Reference use and remaining work

[OpenJK](https://github.com/JACoders/OpenJK/tree/1a6a643427aa347553e9073dac5570b33337c4d9),
particularly `codeJK2/game/g_public.h` and `g_main.cpp`, was inspected as
reference observation after verifying its GPLv2 licence against this
repository's GPLv2 licence. No implementation text was incorporated.
Its current JK2 table reports API **10**, adds and removes services, and
changes save handling; it cannot be substituted for this retail API **6**.
Retail disassembly and the native table result settle the constants above.

The next slice requires client import 3, client/player layout, checked
native game-data binding, SP trace marshaling, and the world imports used
by actual map spawning. Ghoul2 uses native C++ objects
through the SP table and is a separate ABI from multiplayer QVM handles.
The bundled client, protocol 40 state codec, campaign/save transitions,
rendering and FnQ3/FnQL audio presentation still require their own adapters
and runtime evidence. The integrated collision load also printed
`CM_AddFacetBevels... invalid bevel` warnings; their cause was not
investigated in this slice.
