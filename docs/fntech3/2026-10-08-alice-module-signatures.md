# Alice module signatures

## Scope

The [module ABI record](2026-09-22-alice-module-abi.md) measured the length of
Alice's four module tables and named every slot, but claimed no signature:
"An argument count is not a signature." This record gives every slot of all
four tables a fixed-width C type, read out of the retail binaries, and freezes
those types in code so that serving a slot, or calling an export, through the
wrong shape is a compile error rather than a corrupted stack. It also records
what the reading found fnTech3 doing differently, and what changed.

The machine-readable companion is
[2026-10-08-alice-module-signatures.json](2026-10-08-alice-module-signatures.json).
The rows themselves are `code/compat/alice_game_import_sigs.def`,
`alice_game_export_sigs.def`, `alice_cgame_import_sigs.def` and
`alice_cgame_export_sigs.def`.

## Observed: how each type was read

Nothing was run. The three binaries were read with capstone and pefile:

| Binary | Bytes | SHA-256 |
| --- | --- | --- |
| `alice.exe` | 2,209,835 | `8aa22917b7707f0e1a7c4dbe9a3c38332b1873ae2059174ac5329de26921fc18` |
| `base/fgamex86.dll` | 2,084,930 | `ae5a572728fbb2cbf8935d633ef9e23f023edad986d7dd9ee506d589eb261aea` |
| `base/cgamex86.dll` | 409,666 | `fa70feb1be9883511cf46e1f5556e5c9a2bc52b6b6eecd31a28fa228fa3b443c` |

Each slot was read from up to three sides, and each row carries letters for
the sides that fix it:

- **E, the engine.** The function `alice.exe`'s table builders write into the
  slot (game imports `0x004544c0`, client-game imports `0x00424400`, the
  renderer's slots through its export block at `0x0079da28`), followed
  instruction by instruction with the stack tracked: an argument it
  dereferences is a pointer, one it loads with `fld` a float, one it compares
  or indexes an integer, and a value left in `st(0)` at `ret` a float return.
  For exports, the engine's own calls through the tables it keeps.
- **M, the module.** Every call through the tables the modules copy
  (`fgamex86.dll` `0x101e8d10`, `cgamex86.dll` `0x3029eb60`), each argument
  classified by what is pushed, and whether `eax` or `st(0)` is used after the
  call. For exports, the export's own body.
- **S, Ritual's F.A.K.K.2 v1.02 SDK** -- the member at that position in
  `fgame/g_public.h` and `cgame/cg_public.h`, read for reference only. It is
  the hypothesis E and M confirm or refute; no text was taken from it, and
  no row rests on it alone.

Every slot target and export ends in a bare `ret` (three tail-jump into one
that does), so every function is `__cdecl`, which is what `QDECL` is on Win32.
A record returned by value is written as MSVC passes it: a hidden first
pointer, returned in `eax`. Types are fixed-width: `int` (handles, `qboolean`
and `long` included), `float`, pointers, and `unsigned char` or
`unsigned short` where retail reads or returns only that width.

| Table | Slots | Functions | Values | Empty in retail |
| --- | --- | --- | --- | --- |
| Game imports | 111 | 108 | 2 (101 `DebugLines`, 102 `numDebugLines`) | 1 (100) |
| Game exports | 26 | 17 | 6 (0, 21 to 25) | 3 (11 `BotBegin`, 12 `BotThink`, 13 `PrepFrame`) |
| Client-game imports | 125 | 122 | 0 | 3 (0, 88, 116) |
| Client-game exports | 7 | 7 | 0 | 0 |

No slot is left unsettled: wherever the module or the engine exercises a slot,
both sides fix its shape, and the slots nobody calls still have engine bodies
whose use of their arguments fixes it. The two entry points match fnTech3's
declarations: `GetGameAPI` (`0x100a48a0`) takes the import table and returns
the export table (`0x101e8ca8`); `GetCGameAPI` (`0x300241b0`) takes nothing and
returns `0x30272420`.

## Observed: where Alice departs from the SDK

These are where a prototype taken from the SDK would be wrong for Alice.
fnTech3 already followed Alice in each, but for the two narrow widths.

- **The tag queries take one bone-controller pointer**, not the SDK's two
  arrays: game imports 78 and 79 and client-game imports 115 and 117
  (`0x00453e20`, `0x00453ec0`, `0x0045f440`, `0x0045fbc0`). Game import 79 takes
  seventeen stack arguments, the hidden return pointer included.
- **A dialog line's played flag is a byte.** Game imports 86 and 87 read only
  the low byte (`0x00423238`, `0x004232cf`); the module pushes a word whose
  upper three bytes are whatever its stack held (`0x100a4052`, `0x100a40b6`).
- **`CalcCRC` returns sixteen bits** in `ax` and leaves the rest of `eax`
  undefined (`0x0043a4fb`, `0x0043a503`, `0x0043a50e`); the module masks it
  (`0x100b2020`).
- **`pointbrushnum`'s second argument is a clip handle** (`0x00435eee`); both
  module sites pass zero. A name only: it is an `int` either way.
- **Client-game import 0 is left empty**, where the SDK declares an API
  version; the module never reads it.
- **Game export 0 is the SDK's own member**, its `apiversion`, which the
  module writes as the constant 4: the 26 slots are the SDK's 26 members in
  order. `alice_game_exports.def` had called it an Alice addition to a
  25-member table; it now records it as written by an immediate.

## What changed

**The freeze.** `code/compat/alice_module_abi.h` turns each row into a type
and compares a function with it by ABI class -- void, a 32-bit integer, a 16-
or 8-bit one, a float, a pointer -- parameter by parameter, with the count
and whether it is variadic. Classes are compared rather than C++ types
because this engine hands records over as its own types where a row says
`void *`. The game's `SERVE` and the client game's `SERVE_CGAME` now
`static_assert` that each served function fits its slot, the two data slots
are checked as values, and every type an export is called through is checked
against its row, the client game's never-called `EntityOrientation` included.

**Five game imports had the wrong shape.** None misbehaved: two are slots the
module never calls, and the other three were compensated downstream. Each
would now fail to compile, so each was corrected:

| Slot | Was | Now |
| --- | --- | --- |
| 18 `FS_PrepFileWrite` | a shared no-op, `void ()` | `const char *( const char * )`: the path the file would be written to, its directories made, as retail's `0x0043cb80` does in a static buffer. Retail's path is its game directory's; fnTech3's is the home path's, where every write goes, and a name no write may take answers an empty path (`FS_PrepareWritePath`, `code/qcommon/files.c`) |
| 27 `DebugGraph` | the same no-op | `void ( float, int )`, dropping the value as this engine's other servers do: retail's single executable feeds its client's graph ring (`0x0042e2a0`); a server here has none |
| 86 `Alias_UpdateDialog`, 87 `Alias_AddActorDialog` | the played flag an `int`, truncated later | `unsigned char`, the width retail reads |
| 99 `CalcCRC` | `int` | `unsigned short`, the width retail returns |

**Behaviour the reading turned up, now retail's:**

- `FS_ReadFile`'s third argument (game 13, client game 14) is retail's
  `quiet`: passed on to the open (`0x0043c9c8`), it keeps back the developer
  line `Can't find %s` for a missing file (`0x0043c1fa..0x0043c204`). fnTech3
  had called it unestablished and logged every read; a read it cannot find
  now prints that line unless the module asked for quiet.
- `FS_Flush` (game 24, `0x0043b5d0`) is Quake III's `FS_ForceFlush`: the file
  goes unbuffered for good (`0x0043b5da..0x0043b5e1`). fnTech3 had flushed it
  once; it now flushes it and makes it unbuffered.
- `SendServerCommand` (game 28) broadcasts for -1 alone and drops any other
  number outside the clients (`alice.exe` `0x00453467`,
  `0x00453489..0x00453494`; `fakk2.exe` 1.02 `0x004444a7`,
  `0x004444c9..0x004444d4`, the same rule). fnTech3 had broadcast for every
  negative number.
- Client-game `Tag_NamedOrientation` (117) poses the legs with every
  crossblend and torso argument zero and both lerps 0.0
  (`0x0045fbd1..0x0045fbe7`), where fnTech3 passed -1. The module never calls
  the slot.

**Tables.** `alice_game_import_calls.def` recorded game import 79 at three
arguments: both its sites (`0x1010b3be`, `0x101679a6`) build their seventeen
around two nested method calls, each pushing one of its own, and the scan
counted only what followed the last. The row now reads seventeen, the one
hand correction in that generated file, with its header saying so.

## Left as they were

- **`DebugPrintf`** (game 2, client game 3, `0x00438a90`) writes to the
  debugger in retail (`OutputDebugStringA`, `0x0046b765`) when the developer
  cvar is set; fnTech3 writes it to its own console, which is where this
  engine's diagnostics go.
- **Error levels.** Retail's `Com_Error` (`0x00438ae0`) treats 0 as fatal,
  1 as a drop, 2 as a disconnect and 3 as a missing CD (`0x00438b78`,
  `0x00438b9e`, `0x00438beb`); fnTech3 drops for every level a module raises,
  as `SV_AliceError` documents, so a module's fatal error returns to the menu
  rather than ending the process.
- **Empty slots.** Game import 100 and client-game imports 0, 88 and 116 hold
  this engine's unserved thunks, which drop by name, where retail holds 0. The
  module never reads any of them.
- **F.A.K.K.2's tables** are served from the same functions with four
  overrides of its own (`fakk2Import`, `fakk2CGameImport`). They are not typed
  here; a parallel set of rows keyed by its own slot numbers would let the
  same check guard them.

## Weakest evidence

Every row has engine or module evidence for its shape. These individual types
rest on the SDK alone, because neither side tells them apart; each is a
four-byte value only stored or passed on, so its type has no ABI effect:

- game import 54 `NumSkins`'s parameter (its body is `xor eax, eax; ret`);
- game import 27's `color`, stored (`0x0042e2bf`) but never read back;
- the `parameters` string of game import 89 and client-game imports 82 and
  119, only stored and forwarded (`0x00422cd0`);
- client-game import 59's light `type`, a flag word passed through.

## Tests

- `tests/alice_module_abi_tests.cpp` holds the four signature tables to the
  name tables slot by slot; holds every count the module's own call sites fix
  (`AGREED` and `PUSHES` rows) to its row's parameter count, exactly or, for a
  variadic row, at least its fixed part; requires every `VARIADIC` call row to
  be a variadic signature; ties the empty and value rows to the slots the
  other tables say are empty or data; and asserts at compile time that the
  check refuses the five old shapes, a float passed as an int, a non-variadic
  `Printf` and a data slot served as a function, while accepting the frozen
  ones.
- The server's and client's own table fills are the rest of the test: they
  compile only if every served function and every export type fits.

## Not established

- The rows are a static reading. No retail engine or module was run, and no
  call was observed at run time to confirm a type.
- Slots 18 and 27 have no caller in the module, so their new bodies are
  reached by nothing shipped.
- F.A.K.K.2's slots are not typed (above).
