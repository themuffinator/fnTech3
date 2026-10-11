# Alice's module tables and their measured shapes

## Scope

This record establishes the shape of the boundary between Alice's engine and
its two game libraries: which entry points exist, how many slots each table
has, and the two values the game library writes into its own table as
constants. It establishes nothing about what any slot means. No module is
loaded, twenty of the 111 game imports are served, and the only exports called are Init and Shutdown. See the
[foundation record](2026-09-22-alice-contracts.md) for the profile and the
[compatibility ledger](COMPATIBILITY.md#alice-gate-matrix) for what remains
open.

*Update, 2026-09-24:* that scope describes the record as first written. The
retail game and client-game modules now load and run: 110 of the 111 game
imports are served (slot 100 is null in retail too) and 119 of the 125
client-game imports, the six left being slots the client game never calls.
The ledger holds the current evidence.

## Why this boundary is not Quake III's

Quake III's native modules export `dllEntry` and `vmMain`: the engine hands
over one function pointer and afterwards speaks to the module through a single
numbered call. Neither of Alice's libraries exports either symbol. Each library exports exactly
fifty-three symbols, and fifty-two of them are Ritual's `str` string class:
forty-nine mangled C++ methods and the three format literals they use, shared
across the library boundary so that engine and module use one string
implementation. The fifty-third is the entry point:

| Library | Entry point | Bytes | SHA-256 |
| --- | --- | --- | --- |
| `base/fgamex86.dll` | `GetGameAPI` | 2,084,930 | `ae5a572728fbb2cbf8935d633ef9e23f023edad986d7dd9ee506d589eb261aea` |
| `base/cgamex86.dll` | `GetCGameAPI` | 409,666 | `fa70feb1be9883511cf46e1f5556e5c9a2bc52b6b6eecd31a28fa228fa3b443c` |

The engine's own error strings name both: `Sys_GetGameAPI without
Sys_UnloadingGame` and `GetProcAddress failed on GetCGameAPI`. There is no QVM
form of either library in any retail pak, and no third module at all, because
Alice's menus are engine-side `.urc` resources rather than a UI module.

The client game keeps Quake III's `cgamex86.dll` filename while keeping none of
its ABI, which is the one trap on this boundary worth naming: a loader that
recognizes the filename will load a library that cannot answer `vmMain`.

## How the lengths were measured

Both libraries are ordinary unprotected PEs, so their entry points can be
disassembled directly. Each table's length is the count in the `rep movsd`
that copies it.

### The game's import table: 111 slots

`GetGameAPI` is at RVA `0xa48a0`, VA `0x100a48a0`. Its first instructions are:

```
push    esi
mov     esi, [esp+8]          ; the import table the engine passes
push    edi
mov     ecx, 0x6f             ; 111
mov     edi, 0x101e8d10       ; the module's own copy
mov     eax, 0x101e8ca8       ; the module's export table
rep movsd                     ; 111 dwords
```

So the engine hands `GetGameAPI` a table of **111 pointer-sized slots**, and
the module keeps its own copy of all of them.

The engine agrees, in its own code and without reference to the module. The
sequence that clears that table runs from `0x004544cb`, with the count at
`0x004544d6`:

```
xor     eax, eax
push    esi
push    edi
mov     edi, 0x00c300a0
rep stosd
mov     ecx, 0x6f             ; 111
lea     edi, [esp+8]          ; the table it is about to fill
rep stosd                     ; every slot starts null
```

It then writes function addresses into that block from `0x004544f1` to
`0x00454956` and calls `Sys_GetGameAPI` at `0x0046be50`, which is the routine
the `Sys_GetGameAPI without Sys_UnloadingGame` string and the `gamedll` cvar
registration sit beside. Two independent measurements, one on each side of the
boundary, give the same 111.

Reading that fill needs care, and an earlier reading of it was wrong. The
table lives on the engine's own stack, so every slot is written as
`mov [esp+N], ...`, and `N` moves whenever `esp` does. There is an unmatched
`push eax` at `0x00454821`, in the middle of the run. Ignoring it attributes
every later store one slot too high, and the result does not hold together:
two slots come out written twice and four come out never written. Tracking
`esp` across the run partitions the table exactly -- 110 slots, each written
once, with 110 distinct values and a single hole. That is the reading below.

Those 111 slots divide three ways:

- **108 are code pointers**, into a band of `.text` from `0x0040b590` to
  `0x0046dfc0`.
- **Two are data pointers**: slots 101 and 102 hold `0x006a0870` and
  `0x006a0874`, adjacent dwords in `.data`. The table is therefore not a pure
  function table, and an adapter that assumes it is will hand the module two
  callable addresses where it expects to read values.
- **One is left null** by that `rep stosd` and never written: slot 100. A
  module that called it would call zero, and this module never does.

The engine functions behind those 110 slots are now named, and the arithmetic
closes more simply than the earlier reading allowed.

**Correction (2026-09-23).** This record first decomposed the table as the
reference table's 105 members plus an Alice insertion at slot 48 plus five
appended slots, and named slots 45 to 47 `itemindex`, `soundindex` and
`modelindex`, left 50 and 86 unnamed and put `Alias_UpdateDialog` at 85. The
index family was read one slot off. Serving the slots to the running module
settled it: 45 is `imageindex` (configstring base `0x320`, 64 entries), 46
`itemindex` (`0x400`, 64), 47 `soundindex` (`0x220`, 256) and 48 `modelindex`
(`0x20`, 512, and it loads the TIKI definition the name gives) -- the
`tik`-suffix test that made 48 look like an Alice addition is the model
index's own. 50 is `GameDir`, 85 `Alias_GetList` (named from its position)
and 86 `Alias_UpdateDialog` (the five arguments its call site pushes). So there is no insertion: the table is the
reference table's members in order, with the one null at 100 and Alice's
appended slots after them, which is what `code/compat/alice_game_imports.def`
now records and `tests/alice_module_abi_tests.cpp` checks. The rows below are
corrected; the prose that follows them keeps its original reading of 45 to 48
where it quotes the measurement, with the names fixed.

A name belongs to an engine function *address*, because an address is what was
fingerprinted to establish it. So correcting the slot numbering moved thirty
names onto the slots that actually hold their functions, and several of those
moves are independently confirmed by the bodies themselves: the table-driven
CRC-16 at `0x0043a4c0` is `CalcCRC` and sits at slot 99, and the function at
`0x00453790` that reads five stack arguments straight into five adjacent
globals is `LocateGameData` and sits at slot 103. Neither is where the earlier
reading put it.

The map was not made by pattern-matching two lists. Each slot was fingerprinted
in the engine binary -- the stack arguments its target reads, the literals it
uses, the immediates in its body -- and named only where that fingerprint
identifies it. The evidence is recorded per slot, because a slot named from its
neighbours is a weaker claim than one named by its own code:

- **literal** (22 slots): the target's own string names it, as slot 32's
  `SV_GetUserinfo: bufferSize == %i` names `getUserinfo`.
- **behaviour** (17 slots): a distinctive constant or operation, as slots 29
  and 30 both bound an index by `0x530` -- the same 1,328 configstrings
  measured independently in the engine's own guards -- and slot 99 masks by
  `0xffff` for `CalcCRC`.
- **arity** (48 slots): the argument displacements the target reads match one
  declared member and no other nearby one, as slot 103 reads exactly five
  arguments for `LocateGameData` and slot 71 reads six for `Frame_Bounds`.
- **order** (16 slots): position between two slots named another way. These
  are the weakest rows and are marked as such.
- **Alice's own** (4 slots): 106 to 109. The fifth append, slot 110, names
  itself and is a literal. Nothing is claimed about the four, except that slot
  106 does the same 4KB stack probe and reaches the same formatting routine as
  `Printf` and `DPrintf`, so it is a sixth print-family call. This reading
  first counted slot 48 here too, for its name test against `tik`; that test
  is the reference table's `modelindex`, which loads the model definition
  the name gives (correction above).
- **unnamed** (none since the correction): this reading first left 50 and 86
  unnamed; 50 is `GameDir` and 86 `Alias_UpdateDialog`.

The counts above are the first reading's; the correction moved 48 out of
Alice's own and named 50 and 86, and `code/compat/alice_game_imports.def`
carries each slot's evidence as it now stands.
- **absent** (1 slot): 100, which the engine's `rep stosd` leaves null and
  never writes. The module never calls it, which is the agreement that makes
  the null safe.

Four chains of independent agreement are worth naming, because each one would
have broken a wrong map:

1. **The data pair.** The reference table declares `DebugLines` and
   `numDebugLines` adjacent, the only two non-function members in it. The
   measurement found exactly two non-code pointers in the whole table, adjacent,
   at `0x006a0870` and `0x006a0874`. Their slot numbers put the rest of the
   tail in place.
2. **The file calls.** Slot 16 uses `wb` and slot 17 uses `ab`, which is
   `FS_FOpenFileWrite` and `FS_FOpenFileAppend` in that order and no other.
3. **The index block.** Slots 45, 46 and 47 all reach `SV_FindIndex: overflow`
   and each carries a different configstring base -- `0x320`, `0x400` and
   `0x220` -- which is three index calls; with 48's `0x20` they are
   `imageindex`, `itemindex`, `soundindex` and `modelindex` (corrected
   above). The 256 entries at `0x220` are the sounds; the models are the
   512 at `0x20`. Slot 49 sits beside them and is not an index call at all: it writes a
   configstring at base `0x360` directly, which is `SetLightStyle`.
4. **The TIKI block's arities.** Slots 53 to 87 reproduce the declared
   argument counts of the model, animation, frame, surface, tag and alias
   calls in order, including the two large `Tag_Orientation` bodies and the
   six-argument `Alias_AddActorDialog`.

`code/compat/alice_game_imports.def` carries the whole map and
`tests/alice_module_abi_tests.cpp` holds it to its own shape: one row per slot
in order, no duplicate names, the one absent slot exactly where the engine
leaves its null, and more slots named from their own bodies than from position.

| Slot | Name | Evidence | Sites | Args |
| --- | --- | --- | --- | --- |
| 0 | `Printf` | behaviour | 181 | variadic |
| 1 | `DPrintf` | behaviour | 225 | variadic |
| 2 | `DebugPrintf` | order | 10 | variadic |
| 3 | `Error` | literal | 1497 | variadic |
| 4 | `Milliseconds` | arity | 12 | 0 |
| 5 | `Malloc` | arity | 5 | -- |
| 6 | `Free` | literal | 5 | 1 |
| 7 | `cvar` | literal | 132 | -- |
| 8 | `cvar_set` | arity | 86 | 2 |
| 9 | `argc` | order | 15 | -- |
| 10 | `argv` | order | 22 | 1 |
| 11 | `args` | order | 2 | 0 |
| 12 | `AddCommand` | order | 35 | -- |
| 13 | `FS_ReadFile` | arity | 10 | 3 |
| 14 | `FS_FreeFile` | literal | 6 | 1 |
| 15 | `FS_WriteFile` | literal | 3 | 3 |
| 16 | `FS_FOpenFileWrite` | behaviour | 2 | 1 |
| 17 | `FS_FOpenFileAppend` | behaviour | -- | -- |
| 18 | `FS_PrepFileWrite` | literal | -- | -- |
| 19 | `FS_Write` | arity | 23 | 3 |
| 20 | `FS_Read` | arity | -- | -- |
| 21 | `FS_FCloseFile` | arity | 2 | 1 |
| 22 | `FS_FTell` | arity | 5 | 1 |
| 23 | `FS_FSeek` | order | 3 | 3 |
| 24 | `FS_Flush` | literal | 1 | -- |
| 25 | `GetArchiveFileName` | literal | 1 | -- |
| 26 | `SendConsoleCommand` | arity | 5 | 1 |
| 27 | `DebugGraph` | arity | -- | -- |
| 28 | `SendServerCommand` | behaviour | 40 | variadic |
| 29 | `setConfigstring` | behaviour | 13 | 2 |
| 30 | `getConfigstring` | behaviour | 2 | 1 |
| 31 | `setUserinfo` | behaviour | -- | -- |
| 32 | `getUserinfo` | literal | 1 | -- |
| 33 | `SetBrushModel` | literal | -- | -- |
| 34 | `trace` | order | 3 | -- |
| 35 | `pointcontents` | order | 23 | 2 |
| 36 | `pointbrushnum` | literal | 2 | 2 |
| 37 | `inPVS` | literal | 3 | 2 |
| 38 | `inPVSIgnorePortals` | literal | -- | -- |
| 39 | `AdjustAreaPortalState` | literal | 5 | 2 |
| 40 | `AreasConnected` | order | 1 | 2 |
| 41 | `linkentity` | literal | 4 | 1 |
| 42 | `unlinkentity` | literal | 3 | 1 |
| 43 | `AreaEntities` | arity | 8 | 4 |
| 44 | `ClipToEntity` | order | 1 | -- |
| 45 | `imageindex` | behaviour | 6 | 1 |
| 46 | `itemindex` | behaviour | 2 | 1 |
| 47 | `soundindex` | behaviour | 4 | 1 |
| 48 | `modelindex` | behaviour | 3 | 1 |
| 49 | `SetLightStyle` | behaviour | 2 | 2 |
| 50 | `GameDir` | behaviour | -- | -- |
| 51 | `IsModel` | behaviour | 13 | 1 |
| 52 | `setmodel` | literal | 1 | -- |
| 53 | `NumAnims` | arity | 7 | 1 |
| 54 | `NumSkins` | behaviour | -- | -- |
| 55 | `NumSurfaces` | arity | 1 | -- |
| 56 | `NumTags` | arity | 1 | 1 |
| 57 | `InitCommands` | arity | 2 | 2 |
| 58 | `CalculateBounds` | arity | 5 | 4 |
| 59 | `Anim_NameForNum` | arity | 6 | 2 |
| 60 | `Anim_NumForName` | arity | 7 | 2 |
| 61 | `Anim_Random` | arity | 4 | 2 |
| 62 | `Anim_NumFrames` | arity | 2 | 2 |
| 63 | `Anim_Time` | arity | 3 | 2 |
| 64 | `Anim_Delta` | arity | 3 | -- |
| 65 | `Anim_AbsoluteDelta` | arity | 1 | -- |
| 66 | `Anim_Flags` | arity | 3 | 2 |
| 67 | `Anim_HasCommands` | arity | 1 | -- |
| 68 | `Frame_Commands` | arity | 5 | 4 |
| 69 | `Frame_Delta` | arity | 1 | -- |
| 70 | `Frame_Time` | arity | 1 | -- |
| 71 | `Frame_Bounds` | arity | 1 | -- |
| 72 | `Surface_NameToNum` | arity | 1 | -- |
| 73 | `Surface_NumToName` | arity | 3 | 2 |
| 74 | `Surface_Flags` | arity | -- | -- |
| 75 | `Surface_NumSkins` | arity | -- | -- |
| 76 | `Tag_NumForName` | arity | 99 | 2 |
| 77 | `Tag_NameForNum` | arity | 3 | 2 |
| 78 | `Tag_Orientation` | order | 5 | 7 |
| 79 | `Tag_OrientationEx` | order | 2 | 3 |
| 80 | `Alias_Add` | arity | 1 | 4 |
| 81 | `Alias_FindRandom` | arity | 1 | -- |
| 82 | `Alias_Dump` | arity | -- | -- |
| 83 | `Alias_Clear` | arity | -- | -- |
| 84 | `Alias_FindDialog` | arity | 1 | -- |
| 85 | `Alias_GetList` | order | 1 | -- |
| 86 | `Alias_UpdateDialog` | arity | 1 | 5 |
| 87 | `Alias_AddActorDialog` | arity | 1 | 6 |
| 88 | `NameForNum` | arity | 1 | -- |
| 89 | `GlobalAlias_Add` | literal | 1 | -- |
| 90 | `GlobalAlias_FindRandom` | literal | 7 | 1 |
| 91 | `GlobalAlias_Dump` | literal | -- | -- |
| 92 | `GlobalAlias_Clear` | literal | -- | -- |
| 93 | `centerprintf` | order | 8 | 2 |
| 94 | `locationprintf` | order | -- | -- |
| 95 | `Sound` | arity | 2 | 6 |
| 96 | `StopSound` | order | 1 | 2 |
| 97 | `SoundLength` | arity | 4 | 1 |
| 98 | `SoundAmplitudes` | arity | 1 | -- |
| 99 | `CalcCRC` | behaviour | 1 | -- |
| 100 | -- | absent | -- | -- |
| 101 | `DebugLines` | behaviour | -- | -- |
| 102 | `numDebugLines` | behaviour | -- | -- |
| 103 | `LocateGameData` | arity | 3 | 5 |
| 104 | `SetFarPlane` | arity | 1 | -- |
| 105 | `SetSkyPortal` | arity | 1 | -- |
| 106 | -- | Alice's own | 10 | variadic |
| 107 | -- | Alice's own | 5 | 0 |
| 108 | -- | Alice's own | 3 | 1 |
| 109 | -- | Alice's own | 3 | 0 |
| 110 | `Cbuf_ExecuteText` | literal | 1 | 2 |

The names come from Ritual's published F.A.K.K.2 v1.02 SDK, under its own
non-free licence, used for member names and ordering only, as this tree already
uses Raven's Elite Force SDK. No implementation text is incorporated, and every
name above is carried by a measurement in Alice's own binary rather than by the
SDK alone.

### What the module does with those slots

The map above says what each slot *is*. The module says what it is *for*, and
it says so 2,654 times: that is how many times `base/fgamex86.dll` calls
through the table it copied to `0x101e8d10`. Counting those calls answers two
questions the engine side cannot.

**Which slots matter.** The module calls 92 of the 111. Nineteen it never
calls at all, and slot 100 -- the one the engine leaves null -- is one of the
nineteen. That agreement is the whole reason the null is safe, and it is
asserted in `tests/alice_module_abi_tests.cpp`: an absent slot that the module
called would be a build failure, not a runtime crash.

**How many arguments each takes.** At a call site two things can be read, and
each fails differently. Counting back over the pushes that set the call up is
reliable when the arity is non-zero and wrong when it is zero, because with
nothing pushed the walk runs on into whatever pushes came before. An
`add esp, N` immediately after a cdecl call gives the arity exactly -- when it
belongs to that call, which it often does not, because the compiler batches
several calls' cleanup into one adjustment.

Neither settles a slot alone. Together they do: a site whose stated cleanup
equals its own push count is a site where both readings describe the same
call. Where every such site agrees, the count is settled; where they disagree
with each other, the member is variadic. That settles 59 of the 92, and marks
exactly six as variadic -- `Printf`, `DPrintf`, `DebugPrintf`, `Error`,
`SendServerCommand` and Alice's own slot 106, which is the set that should be.

The counts were measured after the names were fixed, so where they agree it is
two independent readings of the same slot. They agree on `ClipToEntity` taking
seven, `Tag_Orientation` seven, `AreaEntities`, `CalculateBounds`,
`Frame_Commands` and `Alias_Add` four each, `FS_FSeek` and `FS_ReadFile`
three, `inPVS`, `AreasConnected` and `AdjustAreaPortalState` two, and
`linkentity`, `unlinkentity` and `Free` one. The correction to the slot
numbering improved this agreement rather than damaging it: under the earlier
numbering `Sound` took two arguments, which is wrong for it; under the
corrected one it takes six, `StopSound` two and `SoundLength` one.

Two further readings fall out of the counts. The two data members are the only
slots the engine writes that the module never calls, which is what tells a
data pointer from a function pointer from the module's side alone. And a
reachability walk from each export shows there is no small set of imports to
serve first: `Init` alone reaches 2,235 functions and 71 of the 92 called
slots. That set is a ceiling rather than a floor, because the walk follows
every branch out of `Init` including ones a run never takes: serving twenty
imports is in fact enough for `GAME_INIT` to complete. What the number does
rule out is any expectation that a handful of imports gets the module through
a frame.

`code/compat/alice_game_import_calls.def` carries the per-slot counts, and the
Sites and Args columns above are from it.

### Serving the imports

The table above is a map, and a map is not a boundary. What makes it one is
serving the slots, and the way to find out which slots matter is to serve what
can be served honestly, fill every other slot with a thunk that names itself,
and let the module say what it needs next. Under cdecl the caller cleans the
stack, so one thunk declaring no parameters stands in for all 111 slots
whatever their arity -- which is what makes "fill every slot" possible at all.

Thirty-three slots are served. Each one is served because something measured
says what it is, not because a reference declaration has a member of that
name:

- **0, 1, 2 and 3**, the printf family and the error call, which the call
  sites read as variadic.
- **4**, measured at no arguments across twelve sites.
- **5 and 6**, the allocator pair, served through one allocator so what one
  hands out the other takes back.
- **7**, the only import returning a record. The module's own use of the
  returned pointer names the layout: `+0x00` compared, `+0x04` read twenty-five
  times, `+0x08` read, `+0x1c` read five times with `fld`, `+0x20` read as an
  integer. The `fld` fixes a float at 28 and an integer at 32, and nothing
  beyond `+0x20` is touched, so fnTech3's own record can be handed over whole.
- **8**, at two arguments across eighty-five sites.
- **9, 10 and 11**, the command accessors.
- **12**, at one argument across thirty-three of thirty-five sites. One
  argument means it registers a name and no handler, so the engine has to route
  the command back through the module's `ConsoleCommand` export -- which is
  measured at no arguments, because it reads the command through argc and argv.
- **13, 14 and 15**, the file calls.
- **101 and 102**, which are not functions. The module writes *through* them:
  `mov eax, [slot 101]` then `mov [eax], ecx`. Filling them with a thunk like
  every other slot has the module store into code, and that access violation is
  how the data pair confirmed itself at run time. Each slot holds the address
  of a variable the engine owns and the module assigns to.
- **103**, `LocateGameData`, whose body identified it by reading five arguments
  into five adjacent globals.
- **107, 108 and 109**, three of Alice's own additions, whose engine bodies
  are short enough to read whole. 107 returns the address of a fixed 60-byte
  block, and that size is measured rather than guessed: the routine that builds
  the import table opens by clearing fifteen dwords there. 108 stores one value
  in a global. 109 takes nothing and returns on the branch that matters.
- **110**, which the map now names rather than numbering. Its engine body
  reports `Cbuf_ExecuteText: bad exec_when` for any first argument outside
  nought, one and two, so it is this engine's own command buffer call with this
  engine's own three values -- a literal naming a slot that had only a
  position.
- **29 and 30**, the configstring pair, whose indices the module bounds by
  `0x530` in its own body: 1,328, which is this profile's count and not Quake
  III's 1,024.
- **45, 46 and 47**, the index family. Each is a two-instruction wrapper in the
  retail engine around one shared find-or-add routine, and each passes a
  different configstring base and length -- `0x220` over 256 entries, `0x320`
  over 64, `0x400` over 64 -- which is what tells them apart. **49** sits
  beside them and is not an index call: it writes a configstring at base
  `0x360` directly.
- **104 and 105**, two-instruction recorders of renderer state this engine has
  no renderer for. They are recorded rather than refused, because a module that
  cannot set them is a module that stops.
- **41 and 42**, linking and unlinking, which is where the entity record starts
  to matter; the section below says how far that goes.

### What the module does with them

On a developer dedicated server, `base/fgamex86.dll` now completes `GAME_INIT`:

```
Alice game module loaded: API version 4, 732-byte entity record, table at 101E8CA8
  20 of 111 imports are served; the rest drop by name when called.
Alice GAME_INIT: levelTime 0, randomSeed 805
==== InitGame ====
Warning: cvar "sv_maxclients" given initial values: "8" and "1"
Event system initialized: 221 classes 1027 events 907868 total memory in response list
Alice located game data: 8 entities of 732 bytes at 05F13F00, clients of 2112 bytes at 05FCAF18
```

Everything from `==== InitGame ====` down is the retail module's own output,
reaching the console through the `Printf` this file serves. It registers its
event system -- 221 classes and 1,027 events -- and then publishes where its
entities live.

This is not one map's luck. A sweep that starts a separate dedicated server
for each of the thirty-six retail maps completes `GAME_INIT` on **all
thirty-six**, and every one reports the same pair of record sizes.

Two things in that last line are worth keeping. The record size it reports,
**732 bytes**, is the same number the module writes into export slot 22 as an
immediate, arrived at by an unrelated route: a static read of its own table
against a value it computes and hands over while running. And the client record
is **2,112 bytes**, which is a measurement nothing static had produced -- only
running the module gives it up.

The engine's frame is dispatched too, and the level is spawned before it. The
boundary now stops at the first import a spawned level needs that fnTech3 does
not serve: `linkentity`, which cannot be served until Alice's 732-byte entity
record is mapped. The section below says what the spawn and the frame rest on.

### The frame, and why it is still refused

`GAME_INIT` completing makes the next call the engine's frame. That one is
refused, and the reasons are measurements rather than caution.

**The frame's own signature is settled.** `RunFrame` reads two arguments where
Quake III passes one, and what it does with them says what they are: it stores
the first as an integer, then converts both to float and multiplies each by the
same constant, `0x3a83126f` -- 0.001, milliseconds to seconds. So the first is
the level time in milliseconds and the second is the frame's own length. The
engine does not hand a frame length to the game, but it is recoverable from the
level time, and `SV_AliceFrameTime` does that.

**The engine's own call sites say where the exports are called from.**
`alice.exe` keeps the game's export table in the global `0xac0e08` and checks
`[eax] == 4` before using it, which confirms the version slot from the engine's
side. One of those sites settles `Init`: the engine pushes two values and
cleans eight bytes, so `Init` takes the level time and the seed it has just
produced, which is what fnTech3 passes it.

Counting pushes at a call site is weaker evidence than it looks, and `Shutdown`
shows why: the site pushes `edi` before calling it, but that is a register save
restored by a matching `pop`, and the export takes no arguments at all. So the
argument counts here stay the ones read from the export bodies; the call sites
agree with them without independently establishing them.

One detail only the engine side shows: after a call it reads export slot 25
and, if that slot is not null, passes it to its own error routine and clears
it. Slot 25 is a deferred error message the module leaves for the engine to
raise, which means an engine that never reads it swallows those failures.

**The step before the frame is the spawn.** Alice spawns a level separately
from starting the game, and Quake III has no ordinal for that, so nothing asks
for it. Running a frame without it faults inside the module on a null world, at
the first `mov ecx, [eax+0x20]` off a global the spawn fills.

Guessing that call does not work, and the failure is worth recording. Export
slot 4 reads three arguments and hands them to the same object `RunFrame` calls
into, so entity text, level time and random seed is the obvious reading.
Calling it that way smashes the module's stack: under a debugger the module
returns into its own data, with the bytes `61707322 6c666e77 22736761` --
`"spawnflags"` -- sitting where a return address belongs.

The retail engine's own call site is what settles it, at `0x004552e7`:

```
mov  eax, [0xac0e08]      ; the export table
push edi                  ; third argument: zero
lea  esi, [eax + 0x10]    ; slot 4
call 0x430dc0             ; the engine's copy of the entity text
mov  ecx, [0xb59b58]      ; its server state
push eax                  ; second argument: that text
mov  edx, [ecx + 4]
push edx                  ; first argument: a short name
call [esi]
add  esp, 0xc             ; three arguments
```

So the first argument is a **name** and the second is the text, which is the
opposite of the obvious reading and exactly why the obvious reading overran a
buffer: the module hands its first argument to a dialog loader that builds
`dialog/<name>.TLK` in a 128-byte stack frame. The third is zero.

The second argument is a copy rather than the engine's own lump. The routine
behind `0x430dc0` returns a global that is written in one place, by code that
allocates a buffer the size of a lump, records the length beside it, and
`rep movsd`s the lump in. The module parses that text in place, so it is given
something it may write to.

Called that way it works. `SpawnEntities("garden1", 76348 bytes, 0)` returns,
and what the module does next confirms every part of the reading: it loads
`dialog/standard.TLK`, then `dialog/garden1.TLK` -- the map's own dialog, which
only a correct first argument produces -- and then `maps/garden1.pth`, the
map's navigation paths. It then asks for `linkentity`, which is a module that
has spawned entities and wants them in the world.

### The next gate: what linking an entity needs

With the level spawned, the module asks for `linkentity`, the first import
fnTech3 cannot serve from an engine service alone: it takes a pointer to one of
Alice's 732-byte entity records, so serving it means agreeing with the module
about what is inside that record.

Bounding such a pointer is itself a measurement. `LocateGameData`'s own count
is not the array's length -- it reports the count at the moment it is called,
and the module calls it again on every entity it creates. Watching those calls
is the clearest evidence there is that the spawn is real: the count climbs from
eight, a server's worth of client slots, to a hundred and four as `garden1`
fills, with the array address never moving. The module's export slots 21, 23
and 24 carry the array, the count in use and the capacity, and those are what
bound a pointer.

### What it spawns

With the model calls served, the module builds the level's own entities and
says what they are:

```
Classname Characters_WhiteRabbit used, but TIKI was not valid, using Object.
Classname Enemies_Snark used, but TIKI was not valid, using Object.
Classname Item_MetaEssence_Medium used, but TIKI was not valid, using Object.
```

Those are Alice's own classnames out of the map's entity text, each one
becoming a record in the module's array. The complaint beside them is this
engine being honest rather than failing: `IsModel` is served, and it answers
that no model is loaded, because none is. The engine registers a name for a
`.tik` and holds nothing behind it, so the truthful answer to "is there a
model" is no, and the module falls back to a plain object and carries on.

Answering yes would have been the one lie this boundary cannot afford. The
module would have gone on to use geometry that does not exist, and the failure
would have surfaced somewhere with no connection to the cause. Answering no
costs the visual and keeps every other measurement meaningful.

The record is not mapped, but the part the engine itself reads is, because the
engine's own `linkentity` is import slot 41's target at `0x00457a40` and it
dereferences its argument in the open:

| Offset | What the engine does with it |
| --- | --- |
| `+0xb8` | the model index, written by both `setmodel` and `SetBrushModel` |
| `+0x1b4` | written with the handle `CM_TempBoxModel` returns for the bounds below, or with zero |
| `+0x1dc` | a rotation, handed to a bounds transform that walks a box's eight corners -- what turns a box, not where it is |
| `+0x204` | compared against zero; zero means do not link, and no engine code writes it, so the module owns it |
| `+0x208` | set to 1 as the linker's last act at `0x00457ef7`, cleared first thing by `unlinkentity` at `0x004579eb`, and required by the snapshot walk: the linked flag |
| `+0x214` | set to 1 by `SetBrushModel` and cleared by `setmodel`: a brush model flag |
| `+0x218` | the low bound, three floats |
| `+0x224` | the high bound, three floats |
| `+0x230` | contents, compared against `0x100000` and `0x4000000`, and defaulted to -1 |
| `+0x234` | the absolute low bound, copied from `+0x218` and then offset |
| `+0x240` | the absolute high bound, copied from `+0x224` and then offset |
| `+0x260` | the world origin: `absmin[i] = mins[i] + origin[i]` at `0x00457c01` and the same for `absmax` at `0x00457c39` |
| `+0x26c` | the angles, all three tested against zero before the simple bounds path |
| `+0x27c` | the solid type, compared against 1, 2 and 3 |

`+0x204` and `+0x208` are a pair, and only the second is `linked`. Reading
them as one left the flag clear after every successful link, which told the
module the opposite of what had just happened.

`+0x1b4` is the delta table's `solid`, at offset 436 -- the field that goes
on the wire. Ritual fills it with a collision handle rather than Quake III's
packed bounds, and with `0xffffff` for a brush model, which is the value
Quake III reserves for the same meaning.

## The entity record

The server's snapshot walk at `0x004571e0` reaches into the same record and
adds three more offsets, none of which `linkentity` touches:

| Offset | What the engine does with it |
| --- | --- |
| `+0` | the entity's own number: `SV_SvEntityForGentity` at `0x004533d0` bounds it by 1,024 and indexes a 604-byte side array with it, and the walk repairs it with `FIXING ENT->S.NUMBER!!!` when it disagrees with the slot |
| `+0x1c8` | compared against 1,023, the reserved "no entity" number, so it names another entity -- and 456 is exactly where the delta field table says the networked half of the record ends, so this is the first engine-owned member |
| `+0x210` | a flag word. The engine sets bit 15 on every entity it walks; bit 0 hides the entity from clients, bit 1 forces its client's ping to zero, bits 2 and 14 send it past the visibility test, bit 3 runs a second visibility pass from its own `s.origin2`, and bit 4 lets it past the requirement to carry a model or a looping sound. No instruction this reading found tests any other bit |

`+0x1c8` is the first independent check the delta table's length has had: the
table says the networked half of the record ends at 456, and this is the first
offset the engine reads as one of its own.

With those, the record divides completely enough to bind. The server's shared
entity view now has an Alice layout -- `code/compat/alice_game_abi.h` for the
networked half, generated from the delta table so every member sits where the
table puts it, and the `R_` offsets above for the engine's half -- and
`LocateGameData` binds the module's arrays to it. Three members the view
expects are not in the record at all, and each is marked absent rather than
pointed at a plausible offset:

| Member | Why it is absent |
| --- | --- |
| `linkcount` | neither the linker nor the unlinker touches one, and no engine instruction reads one; the view keeps its own, as it does for Elite Force's single player |
| `singleClient` | the snapshot walk has no test that restricts an entity to one client, and every flag bit it does test is accounted for |
| a ping inside the player state | the engine writes the ping at `0x0045660b`, into offset 1,044 of the client record -- the first word past the networked part of the player state, which is the third time 1,044 has arrived by a different route |

The flag word at `+0x210` is translated rather than passed through, because
Alice's bit positions are not this engine's. Bit 0 hides an entity from
clients, bit 1 forces a ping of zero, bit 2 sends past the visibility test and
bit 3 runs a second visibility pass from the entity's own `s.origin2` -- a
portal. Bit 4 lets an entity past the walk's requirement to carry a model or a
looping sound, and bit 14 reaches the same unconditional-send arm as bit 2;
neither is mapped, so both drop on the way in and survive untouched on the way
out. Bit 15 is the engine's own mark, set on every entity it walks.

Only thirty of the seventy-nine networked members cross into this engine's
entity state, because only thirty have a counterpart by name. The other
forty-nine are Ritual's and have nowhere to go, so every write reads the
module's record first and puts the thirty back into it: a member fnTech3 does
not understand comes out of a round trip exactly as the module left it.
`tests/server_game_data_tests.cpp` asserts that, member by member.

Two correspondences that would be easy to make are not made. Alice's `anim` is
a whole-body animation with `torso_anim` overriding the upper half, where this
engine's pair is legs and torso, so calling `anim` the legs animation would be
an interpretation rather than a measurement; and `skinNum` is not
`modelindex2`. Both stay in the module's record, where they are correct.

**Correction.** `+0x1dc` was read as the origin and used as one, which put
every linked entity in the wrong place. The linker settles it: its ordinary
branch at `0x00457c01` builds `absmin` and `absmax` by adding `+0x260`,
`+0x264` and `+0x268` to the local bounds, and `+0x260` is immediately
followed by the angles at `+0x26c` -- the origin-then-angles pairing every
engine of this lineage keeps. `+0x1dc` goes instead to a routine that clears a
bounds accumulator and walks the box's eight corners through a transform, so
it is a rotation. The rotated-brush branch offsets by a second origin inside
the entity state, at `+0x60`, which sits in the twelve bytes the delta table
leaves unnetworked right after `netorigin`; the two are expected to agree and
that is not yet checked.

The absolute pair is computed here rather than left to the module, because the
engine computes it: it copies the local bounds across and offsets them by the
origin, and only when all three angles are zero. A rotated box does not keep
its extents when it turns, so the rotated case takes the largest extent in any
direction and squares it off -- larger than the true bounds and never smaller,
which is what keeps a query from missing something.

Two of those are firm for a different reason than the rest: `+0x218` and
`+0x224` are handed straight to the engine's bounding-box packer (`0x0044a000`)
and temporary box-model routine as a mins/maxs pair, so their type and order
are fixed by the call rather than by resemblance. The solid type at `+0x27c` being compared against three small
integers is what a `solid_t` enum looks like, but which value means what is not
established here.

This is a start on the record, not the record. Nothing about the other 700-odd
bytes is claimed, no field is named after a reference declaration, and no
entity is marshalled anywhere in fnTech3.

### The server runs

With 86 of the 111 imports served, a developer dedicated server loads a retail
level and runs it. `garden1` reports what it built:

```
685 entities spawned
19 entities inhibited
Loading nodes...
Alice ReadFile: "maps/garden1.pth" -> 17218 bytes
Alice ReadFile: "maps/garden1.scr" -> 7245 bytes
Alice ReadFile: "scripts/utils.scr" -> 3984 bytes
Alice ReadFile: "scripts/sound.scr" -> 1111 bytes
Alice ReadFile: "scripts/cinematics.scr" -> 3066 bytes
Alice ReadFile: "maps/cinematics/garden1_cinematics.scr" -> 9363 bytes
soundtrack switched to sound/music/garden1.mus.
```

That is the module doing its own level startup: spawning six hundred and
eighty-five entities, inhibiting nineteen, loading the map's AI path nodes, and
then loading and running five script files including the map's cinematic. The
script interpreter running there is Alice's own, inside the module -- this
engine only had to serve what it asked for.

**The frame loop turns.** A server started with `+map garden1 +wait 200 +quit`
exits by itself, cleanly, in eleven seconds. `+wait` counts down one frame per
server frame, so it only reaches zero if two hundred frames actually ran, and
the engine's own `sv.state = SS_GAME` separator is the last thing printed
before it. That is a running Alice server, not a loaded one. (*2026-09-24:*
`wait` has since become Alice's own, milliseconds counted only by the
client's pass of the frame (`0x0043a183`), so on a dedicated server
`+wait 200` now waits forever, as retail's would; the observation stands for
the engine it was made on.)

**Every retail map runs.** The same `+wait 200 +quit` against all thirty-six
loads and runs every one of them, each exiting on its own with a clean code.

Getting there went through three rounds of the same loop, and the counts are
worth keeping because they show where the work actually was:

| Served | Maps running | What the rest were waiting for |
| --- | --- | --- |
| 86 | 19 | 14 at `trace`, 2 faulting, 1 at `AdjustAreaPortalState` |
| 95 | 30 | 4 faulting, 2 at `AdjustAreaPortalState` |
| 96 | 36 | -- |

The four that faulted all faulted at one address, which is the subject of the
next section.

So the trace is the gate, not a gate. Its shape is measured even though it is
not served, and measuring it took two readings rather than one. The engine's
trace opens by zeroing thirty dwords, which is tempting to read as the result's
size and is not: that is a scratch region. What the caller gets is the copy at
the end,

```
mov edi, [esp+0x8c]      ; the results pointer, the first argument
mov ecx, 0xf             ; fifteen dwords
lea esi, [esp+0x40]      ; a sub-range of that scratch
rep movsd
```

so **Alice's trace result is 60 bytes**. It takes **eight arguments**, from the
argument slots its body reads and the eight pushes its call sites make, against
Quake III's seven into a smaller record.

Two of the fifteen dwords are named, and they are named by what is done to
them rather than by where they sit.

The engine writes the constant 1022 into the result at `+0x34`, and 1023 in its
place on one branch. Those are the two reserved entity numbers of a
1024-entity world -- none, and the world itself -- so `+0x34` is the entity the
trace struck. And `+0x8` is read with `fld`, so it is a float.

Both land exactly where Quake III's own trace record keeps the same two things:
a fraction at `+0x8` and an entity number at `+0x34`. That is a real
correspondence and it is also the trap this section exists to avoid. Alice's
record is 60 bytes where Quake III's is 56, so the two cannot simply be the
same, and two fields agreeing says nothing about the thirteen that have not
been read. Quake III's layout is a hypothesis to test against the collision
routine that fills this record, not a map to adopt.

The rest of the fifteen are filled inside that routine rather than in the
import's own body, and that routine is `0x00437130`. It opens by reserving
0x124 bytes and clearing sixty-five dwords of working structure, so the record
the caller receives is a sub-range of something considerably larger, the way
the import's own sixty were a sub-range of its thirty-dword clear. Mapping it
means following that structure to the copy that leaves it, which is the next
piece of work on this boundary and the one that unblocks fourteen of the
seventeen maps that do not currently run.

### Testing the trace rather than assuming it

Three fields agreeing is a reason to test a layout, not to adopt one. What
makes it worth testing is that this particular guess cannot fail quietly: the
module steers on the trace, so a wrong record has entities fall through floors
or stick in the air, and the maps that stopped at this import either run
afterwards or they do not.

So the first 56 bytes are filled as that record -- a real trace against the
world through this engine's own collision, with the remaining four bytes left
zero -- and the maps that stopped at it were run again. They run. Maps that had
refused at the trace now load, turn two hundred frames and exit cleanly, which
is the layout being confirmed by the only test that could confirm it.

Two things remain true alongside that. The trace is against world geometry
only, because nothing is linked into a world yet, so the entity number it
reports is the world or nothing rather than a contact with an entity behind it.
And eleven of the fifteen dwords are still unread: they are zero here, and
nothing has yet shown whether the module wants something else in them.



What runs in those frames is bounded by what is served. Entities exist and
carry their measured fields, but nothing is linked into a world, so an area
query finds nothing and a trace hits nothing. No model is loaded, so the
script's actors report that they cannot process the events a model would have
given them -- `rabbit_actor`, `turtle_actor` and the rest, named from the map
and resolved to entity numbers, with their events declined rather than faked.
No client has connected and nothing is rendered.

### A family that does not answer uniformly

Serving the model query family with one shared answer was right for
twenty-five of its twenty-seven members and wrong for two, and the way it was
wrong is worth recording.

Every one of those calls opens by resolving a model index and branching away
when the result is negative. Twenty-five of those branches are the same two
instructions, `xor eax, eax; ret`, so one function returning zero serves them
all. Slots 78 and 79 are not like that. The module does this with what they
return:

```
call dword ptr [0x101e8e48]   ; slot 78
mov  esi, eax                 ; the returned pointer
mov  ecx, 0xc
rep movsd                     ; twelve dwords, out of it
```

It copies forty-eight bytes *from* the value, so a zero is dereferenced. The
engine does not return zero there either: its no-model path clears a
forty-eight-byte block, builds an axis in it from a zero vector, and returns
that block's address -- an orientation at the origin with an identity
rotation, which is what this engine returns now.

Five maps found this, all faulting at the same instruction in the module. The
lesson generalises past these two slots: a family that answers uniformly inside
the engine can still need different answers here, and the only way to know
which is to read each member's own branch rather than the family's shape.

### The entities are in a world

For most of this boundary's life an entity existed without being anywhere: it
carried its measured fields, but nothing could find it by position and nothing
could hit it. Three imports were limited by that rather than by anything
unmeasured -- the area query returned nothing, the entity trace was refused,
and the world trace hit only world geometry.

They are not limited by it now, and the measurements that unlocked them are the
entity's own. Linking computes the absolute bounds the way the engine does and
records the entity number as linked; unlinking forgets it. The world this
engine keeps for the title is that set and nothing else:

- **The area query** walks the linked set and returns every entity whose
  absolute bounds meet the box it was given.
- **`ClipToEntity`** traces against one entity through the clip model built
  as it clips, as retail's `SV_ClipHandleForEntity` does (`0x004577b0`,
  called at `0x00453709` and `0x0045821b`): a brush model against its own
  inline model at its own origin and angles, anything else against the
  temporary box rebuilt from its own bounds and contents -- Ritual's
  `CM_TempBoxModel` takes the contents (`0x00430f50`). Until 2026-09-24 this
  used a handle kept from linking, which named whichever entity's box was
  built last, and linking kept none for a brush model: bullets and blows went
  through actors, and traces through doors and platforms.
- **The trace** does the world first and then every linked entity the moving
  box could reach, as retail's entity pass does (`0x00458130..0x004582e5`):
  nothing that is not solid, not the passing entity, its owner, its own
  missiles or its owner's other missiles, and nothing whose contents the mask
  does not take. It keeps the nearest hit, and any solid start, and reports
  that entity's number rather than the world's.
- **The state's solid** (`+0x1b4`), which the client game unpacks to clip
  against the entity, is written by the link as retail writes it
  (`0x00457a78..0x00457bfc`): a bounding box packed by `0x0044a000`, the
  brush-model marker `0xffffff`, or zero. It had held the temporary box's
  handle, which unpacks as a box 511 units wide and none high.

A sector tree would be faster than walking the set, and a level's several
hundred entities do not need one. What matters more is that the structure is
this engine's own and holds nothing the module cannot see: the linked set is
derived entirely from fields the module writes and the engine reads.

The entity number the trace reports is now a real entity where one is hit, and
the world or nothing otherwise, which is the whole of what the reserved 1022
and 1023 are for.

### What is still unserved

Four slots remain, and the module's own call counts say how much that means:

| Slot | Name | Sites | Why it is not served |
| --- | --- | --- | --- |
| 25 | `GetArchiveFileName` | 1 | names a path into an archive this engine does not write |
| 50 | unnamed | 0 | returns a pointer to engine state whose size is not established |
| 99 | `CalcCRC` | 1 | a table-driven CRC whose table is retail data |
| 100 | -- | 0 | the slot the engine leaves null |

Two of the four the module never calls at all, so their thunks can never fire
and serving them would be decoration. The other two are both on the savegame
path -- one names the archive file, the other checksums it -- and neither is
reached in a frame. Nothing observable depends on any of the four today.

*Update, 2026-09-24:* the savegame work serves 25 and 99 as retail does, each
called with two arguments at its one site, and 50 is `GameDir`; 110 of the 111
slots are served, and only 100, which the engine itself leaves null, is not.
See [2026-09-23-alice-savegames.md](2026-09-23-alice-savegames.md). The call
table in `alice_game_import_calls.def` still reads 25 and 99 as `UNSETTLED`:
that is its mechanical rule's verdict on a single site whose cleanup is
batched, not a disagreement about their arity.

`pointbrushnum` came off this list by being answerable rather than by being
implemented: the engine returns -1 from it when it has no world to search, and
-1 is a real answer here too, because this engine's collision hands out no
brush numbers. That is different from inventing one.

### The game's export table: 26 slots

Directly after that copy, the same function writes its export table as
immediates. Twenty of the twenty-six slots are written here, and the rest are
written elsewhere in the library:

| Slot | Written as | Where |
| --- | --- | --- |
| 0 | `4` | immediate, `0x100a48b8` |
| 1-10 | function pointers | immediates, `0x100a48c2` onward |
| 11-13 | never written | -- |
| 14-20 | function pointers | immediates |
| 21 | from a register | `0x100a3801` |
| 22 | `0x2dc` = 732 | immediate, `0x100a496c` |
| 23 | from `eax` | `0x100a3909`, `0x100d01e9`, `0x100d0f27` |
| 24 | from `eax` | `0x100a3809` |
| 25 | a pointer, and `0` | `0x100a3587`, `0x100a4976` |

Scanning the whole library for any instruction that writes into this region
finds nothing above slot 25, which fixes the table at **26 slots**. A second
measurement agrees: the module's import copy begins at `0x101e8d10` and its
export table at `0x101e8ca8`, and the gap between them is `0x68` -- exactly 26
four-byte slots.

This table is the reference `game_export_t` with one addition: the reference
declares 25 members and Alice's has 26, because slot 0 carries an API version
the reference does not have. Every other slot is the reference member one place
below it, and the shape agrees at every point that could disagree:

| Slot | Name | What the module writes |
| --- | --- | --- |
| 0 | `apiversion` | the immediate `4`; Alice's own addition |
| 1-10 | `Init`, `Shutdown`, `Cleanup`, `SpawnEntities`, `ClientConnect`, `ClientBegin`, `ClientUserinfoChanged`, `ClientDisconnect`, `ClientCommand`, `ClientThink` | function addresses |
| 11-13 | `BotBegin`, `BotThink`, `PrepFrame` | nothing, ever |
| 14-20 | `RunFrame`, `ConsoleCommand`, `WritePersistant`, `ReadPersistant`, `WriteLevel`, `ReadLevel`, `LevelArchiveValid` | function addresses |
| 21 | `gentities` | a register, at run time |
| 22 | `gentitySize` | the immediate `0x2dc` = 732 |
| 23 | `num_entities` | a register, at three sites |
| 24 | `max_entities` | a register |
| 25 | `error_message` | a pointer, and zero |

Three of those rows carry their own proof. **The three slots the module never
writes are exactly `BotBegin`, `BotThink` and `PrepFrame`** -- and Alice's two
bot slots being null agrees with what the profile slice established from a
completely different direction, that this engine has no bot library, no
`bot_enable`, no `.aas` navigation and no botfiles in any pak. **`gentitySize`
is the one data member written as a constant**, which is why 732 is a claim
here and the other four data members are not. And **`num_entities` is written
from a register at three separate sites** while `max_entities` is written at
one, which is how a count that changes as entities spawn and free differs from
a capacity set once.

`code/compat/alice_game_exports.def` carries the map, marking each slot as a
function or a value. That distinction is the one an adapter has to respect:
six of the twenty-six slots hold data, so calling slot 23 would jump to an
entity count.

### The client game's tables: 125 imports, 7 exports

`GetCGameAPI` at VA `0x300241b0` takes no argument at all. It writes seven
function pointers into `0x30272420` and returns that address:

```
mov     dword [0x30272420], 0x300240e0
mov     dword [0x30272428], 0x3002c710
mov     dword [0x30272424], 0x30024150
mov     dword [0x3027242c], 0x30021520
mov     dword [0x30272430], 0x30023f60
mov     dword [0x30272434], 0x300217f0
mov     dword [0x30272438], 0x30024160
mov     eax, 0x30272420
ret
```

Scanning the library finds no write above slot 6, so the client game's export
table is **7 slots**, with no version slot of its own.

Because `GetCGameAPI` takes no import table, the imports arrive through the
first export instead. That function, at `0x300240e0`, begins:

```
mov     edx, [esp+4]          ; the import table
push    esi
push    edi
mov     ecx, 0x7d             ; 125
mov     esi, edx
mov     edi, 0x3029eb60       ; the module's own copy
rep movsd                     ; 125 dwords
mov     ecx, 0x10928
xor     eax, eax
mov     edi, 0x302b21c0
rep stosd                     ; clears its own state
mov     ecx, 0x4d09
mov     edi, 0x3029ed60
rep stosd
call    [edx+0x1f0]           ; import slot 124, called immediately
```

So export slot 0 is the client game's initializer, it receives **125
pointer-sized import slots**, and the very first thing it does after clearing
its state is call the last of them. Whatever import slot 124 is, it is on the
critical path before the client game does anything else.

The engine side of this one is now located too. `0x0046c020` reads the
`cgamedll` cvar, prints `LoadLibrary (%s)`, loads `cgamex86.dll` and resolves
its entry point, and the caller that reports `CL_InitCGameImport failed` is at
`0x00424400`. The builder is that same function, immediately after the failure
check -- which is why looking for a run of stack stores or global stores never
found it. This table is not built in either place. Its address arrives as an
argument and it is filled through a register base:

```
mov     edx, [esp+4]          ; the table the caller supplies
push    edi
mov     ecx, 0x7d             ; 125
xor     eax, eax
mov     edi, edx
rep stosd                     ; every slot starts null
mov     dword [edx+4],  0x438850
mov     dword [edx+8],  0x438a40
mov     dword [edx+0xc],0x438a90
...
```

That is the engine's own confirmation of **125**, from the opposite side of the
boundary to the module's `rep movsd`, and it settles three things the module's
use sites could not:

- **Three slots are never filled**: 0, 88 and 116. Slot 0 is the first one,
  left null by the `rep stosd` and never written, so this table begins with a
  hole rather than a member.
- **Ninety-five slots take an immediate**, an engine function address written
  at compile time. **Twenty-seven take a register**, and the run at slots 50
  and up reads from a tight cluster of globals at `0xdc8168` to `0xdc81a0` --
  pointers this engine resolves while it runs rather than ones the linker
  knows, which is what the renderer's own entry points would look like.
- **Twenty-one slots hold an engine function the game import table also
  holds**, so the game map names them by address, which is how those names
  were established in the first place: slots 1, 2, 3 and 6 are `Printf`,
  `DPrintf`, `DebugPrintf` and `Error`; 8 and 9 are `cvar` and `cvar_set`; 10
  to 12 are `argc`, `argv` and `args`; 14 to 16 are the three file calls; 18
  is `SendConsoleCommand`; and 82 to 85 are the four `GlobalAlias` calls.

The import the client game calls before it does anything else is now known as
well. Slot 124 is `0x0042c490`, whose whole body is `mov eax, 0xc300a0; ret`.
That address is not arbitrary: `0xc300a0` is the block the *game* table build
zeroes at `0x004544cf`, before it fills anything. So the client game's first
act is to ask the engine for a pointer to the same shared block the server
side clears on startup, and the game's own slot 107 returns it too.

With every filled slot carrying an engine address, the rest can be
fingerprinted the same way the game table's were -- by what the function
itself says. Twenty-two of the seventy-four slots the game map does not name
say something, and eleven of those name themselves outright:

| Slot | The literal its target reaches | What the slot carries |
| --- | --- | --- |
| 20 | `Client command overflow` | the reliable client command call |
| 21 | `CM_LoadMap: NULL name`, `cm_noAreas`, `cm_noCurves` | loading the collision map |
| 22 | `CM_InlineModel: bad number` | `CM_InlineModel` |
| 26 | `CM_ClipHandleToModel: bad handle %i` | a clip handle call |
| 30 | `S_MILES_StartSound: handle %i out of range` | starting a sound |
| 34 | `S_AddLoopingSound: handle %i out of range` | `AddLoopingSound` |
| 39 | `S_UpdateEntityPosition: bad entitynum %i` | `UpdateEntityPosition` |
| 75 | `CL_GetSnapshot: snapshotNumber > cl.snap.messageNum` | `GetSnapshot` |
| 78 | `CL_GetParseEntityState: %i >= %i` | `GetParseEntityState` |
| 80 | `CL_GetUserCmd: %i >= %i` | `GetUserCmd` |
| 81 | `CL_GetServerCommand: a reliable command was cycled out` | `GetServerCommand` |

The other eleven -- 5, 7, 17, 31, 38, 42, 43, 48, 91, 98 and 99 -- say what
kind of call they are without naming one: 5 and 38 reach the `Z_Free` checks,
7 the journal file errors, 17 `qconsole.log`, 31 the same Miles sound error as
30, 48 `courier-14`, and 91, 98 and 99 `TIKI_Error.`. Slots 42 and 43 reach
only the string `none`, which says they take a name and have a default.

The sound block at 30 to 44 and the client-state block at 75 to 81 are
therefore both located, which is the part of this table a client adapter would
need first.

What can also be said is how far the tables have drifted from the reference:
the reference declares a 123-member client import table and a 6-member export
table, against Alice's measured 125 and 7. The export table's first member is
declared as `CG_Init( clientGameImport_t *, int, int )`, which is exactly the
shape measured above -- an initializer that receives the import table -- so the
two agree. Two extra imports and one extra export are a smaller drift than the
game table's six. The client game module's own call sites then check all of this from a third
direction. It calls 89 of its 125 slots, 627 sites in all, and the same
agreement rule settles 51 of them. Every name above survives that check:
slots 1, 2, 3 and 6 read variadic, which is what `Printf`, `DPrintf`,
`DebugPrintf` and `Error` have to be; slot 8 takes three arguments and slot 9
two, for `cvar` and `cvar_set`; `argv` takes one; `FS_FreeFile` one;
`CM_InlineModel` one; `AddLoopingSound` five; `GetSnapshot` two; `GetUserCmd`
two; and `GetServerCommand` one. Slot 124 takes none, which is exactly right
for a function whose whole body returns an address.

That last one is worth stating plainly, because it closes a loop across three
independent measurements: the engine puts a zero-argument getter in slot 124,
the module's initializer calls slot 124 before anything else, and the module's
call sites show it called with no arguments. None of the three was derived
from the others.

Between the twenty-one named by a shared address and the twenty-two that
speak for themselves, about a third of this table now has the kind of evidence
the game table's rows carry. That is not yet a map, so no client-game slot is
named in `code/compat/`: these are recorded here as measurements. Freezing
them should wait until the ordering is pinned the way the game table's was,
because a partial map invites the same mistake the game table's slot numbering
already made once.

## What is frozen

`code/compat/alice_module_abi.h` carries the four lengths, the API version, the
entity size, and the two entry-point names, with the byte sizes a Win32 x86
retail build gives them. It also carries the two measured tables:
`alice_game_imports.def` for what each import slot is, and
`alice_game_import_calls.def` for what the module does with it.
`tests/alice_module_abi_tests.cpp` asserts them, holds the two tables to each
other -- the slot the engine leaves null has to be one the module never calls
-- and checks ten members whose argument count the reference table also fixes
against the count measured from the call sites. It checks
that the profile catalogue names the same two libraries and refuses every other
architecture and extension, checks that no other profile admits Alice's module
API, and checks that Alice claims no pure proof, because nothing has admitted
one of its modules yet.

## The module confirms it

Everything above is read out of the binaries without running them. It was then
checked against the module itself, because a measurement that a real library
contradicts is worth nothing.

`GetGameAPI` can be called safely with a table of nulls. Its whole body is the
`rep movsd` that copies the import table and the immediate writes that publish
its exports: it calls nothing and dereferences nothing from what it is handed.
So a probe loaded the retail `base/fgamex86.dll`, resolved `GetGameAPI`,
handed it 111 zeroed slots, and read back what it published. Every prediction
held:

| Prediction | Runtime |
| --- | --- |
| the export table is at `0x101e8ca8` | published at `0x101e8ca8` |
| slot 0 is the API version, 4 | 4 |
| slot 22 is a record size, 732 | 732 |
| seventeen slots are function pointers | 17, every one inside the module's own image |
| six slots carry values, not code | 6 |
| three slots are never written | 3 null |

That is the module agreeing, in a running process, with a map derived from
reading its bytes. It removes the possibility that the table shape above is a
misreading.

### The engine loads it

`code/server/server_alice_game.hpp` does the same thing inside the engine.
Alice's game is a function table, so it loads through `VM_CreateTable` -- the
path Elite Force's single player and the IHV build already use -- rather than
through the `dllEntry`/`vmMain` loader, which is what used to refuse it. Having
taken the table, the engine checks it rather than trusting it: the API version,
the entity record size, and every slot the frozen map says this build never
writes, which must still be null.

A developer dedicated server now does this on every one of the thirty-six
retail maps. It mounts the installation, loads the map's collision, loads
`base/fgamex86.dll`, and prints what the module published:

```
VM_CreateTable 'fgamex86.dll' ok
Alice game module loaded: API version 4, 732-byte entity record, table at 101E8CA8
  20 of 111 imports are served; the rest drop by name when called.
```

It then runs `GAME_INIT` and stops at the next export, by name:

```
ERROR: Alice's game module is loaded and its table is published, but no export
is served: the engine asked for GAME_INIT (0). None of its 111 imports has an
established signature, so the module holds a table of nulls and must not be
called.
```

That refusal is the point, not a limitation to be worked around. The module
holds a table of nulls, so calling an export would not fail in the engine but
inside the module, on a null import, with nothing to say which one. Refusing
says exactly what is missing. `GAME_SHUTDOWN` is the one call answered rather
than refused, because nothing was started and erroring there would fire again
while the first refusal was being handled.

One detail worth keeping: the ordinal the engine asks for is Quake III's, not
Alice's. Alice's export table is numbered differently -- its slot 0 is a
version, not a call -- so an incoming ordinal must not be looked up in the map
above. Doing so named the refusal `apiversion` instead of `GAME_INIT`.

## What this does not claim

*Update, 2026-10-08:* every slot of all four tables now has a fixed-width
type read out of the retail binaries, and the engine's table fills and export
calls are checked against those types at compile time
([signatures record](2026-10-08-alice-module-signatures.md)). The paragraph
below is this record's original scope.

No slot's signature, calling convention or return value is claimed, for any
of the four tables. An argument count is not a signature: 59 of the game's
import slots have one measured from the module's own call sites, and not one
of them has a parameter type. The game's 111 imports and 26 exports carry
names; the client game's 125 imports and 7 exports do not. Of the game's
exports only `Init` and `Shutdown` are called, with argument counts measured
from their own bodies rather than declared anywhere. The entity record's members are unknown; only
its size is. Nothing here loads a library, and the engine still refuses both of
them by name, which is what a developer server does today on every retail map.

A cross-check worth recording: the independent
[OpenAlice](https://github.com/themuffinator/OpenAlice) reconstruction also
puts the game import table at 111 slots and the API version at 4, arrived at
separately. Its export table is given as 24 slots where this measurement finds
26; the two extra are slots 24 and 25, both written from registers, and the
measurement above is what this tree uses.
