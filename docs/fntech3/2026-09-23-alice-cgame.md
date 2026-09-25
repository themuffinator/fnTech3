# Alice's client game: the import map, the exports and the render path

## Scope

This record names every slot of American McGee's Alice's client-game import
table and export table, measured against the retail November 2000
`alice.exe` and `base/cgamex86.dll`, and records the renderer-side contracts
the client game's calls reach: the FAKK 42 world as the renderer loads it,
FTX textures, and the light grid. It builds on the
[module record](2026-09-22-alice-module-abi.md), which measured the table
lengths, the engine's own table build at `0x00424400` and twenty-one slot
names, but stopped short of a map.

Observed facts and inferences are kept apart below. Nothing from Ritual's
F.A.K.K.2 SDK or from any reconstruction is incorporated as text; the SDK's
declarations are used as a hypothesis that each slot's own evidence then
confirms or refutes.

## The import map

The engine's builder fills 122 of the 125 slots. For each filled slot it
writes either an engine function address as an immediate (95 slots) or a
pointer it loads out of its own renderer export block at `0x00dc8160` (27
slots). Walking each engine function it writes gives the evidence:

- **Self-identifying slots land in the SDK's order.** Every slot whose
  target names itself -- `SCR_UpdateScreen: recursively called` at 19,
  `Client command overflow` at 20, `CM_LoadMap: NULL name` at 21,
  `CM_InlineModel: bad number` at 22, `S_MILES_StartSound` at 30,
  `S_AddLoopingSound` at 34, `Couldn't load sound` at 37,
  `S_UpdateEntityPosition` at 39, `%s.mus` at 42, `MUSIC: changing from` at
  43, `.lip` at 45 and 46, the `CL_Get*` family at 75 to 81, and the
  `TIKI_*` family from 91 to 118 -- is the member the SDK's client-game
  import table declares at that position.
- **The three slots the engine never writes are members the module never
  calls**: 0 (`apiversion`), 88 (`NumSkins`) and 116 (`Tag_OrientationEx`).
- **One slot's own string misleads.** At 111 the SDK declares a surface-flags
  query, and Alice's engine writes a function whose error string is
  `TIKI_Surface_DamageMultiplier: Surface %d out of range for %s.` The body
  says otherwise: it returns the surface record's flags word at `+0x154`,
  where the real damage query at `0x0045df80` -- handed to neither module --
  reads the float at `+0x158`. It also bounds-checks the surface index and
  then never scales by it, so retail always answers with the first surface's
  flags. The slot is `Surface_Flags`, defect included.
- **Slot 124 is Alice's own**, appended after the SDK's 124 members: the
  shared-block getter the module calls first.
- **The renderer block is ordered like Quake III's `refexport_t` with
  Ritual's additions.** The builder copies slot 53 (`R_RegisterModel`) from
  `+0x08`, 54 from `+0x0c`, 55 from `+0x10`, 56 from `+0x14`, 49 from `+0x20`,
  52 (`R_LoadWorldMap`) from `+0x28`, 50 from `+0x2c`, 57 from `+0x30`, 51
  (`R_RenderScene`) from `+0x40`, and so on; slot 48 is not a renderer
  pointer but an engine routine that calls `+0x04` with the graphics
  configuration and then re-registers the menu system's resources, which is
  what `BeginRegistration` is.

Where the SDK's names are corroborated by argument counts rather than by
literals, the counts come from walking the target with `esp` tracked across
its own pushes: `CM_TransformedBoxTrace` reads ten arguments,
`CM_MarkFragments` seven, `CM_BoxTrace` eight, `CalculateBounds` four,
`TIKI_Alias_FindDialog` four -- each the declared member's count.

The module's own call sites were then read with the rule the game table
used: a site whose `add esp` cleanup equals its push count settles the
arity, and sites that agree individually but differ between themselves are
variadic. 55 slots are AGREED, 13 PUSHES, 4 VARIADIC, 17 UNSETTLED and 36
never called. Those counts agree with the SDK's declarations everywhere but
one, and that one is a real difference:

**`Tag_Orientation` takes six arguments, not seven.** Both of the module's
call sites push seven dwords and clean seven, and one of the seven is the
hidden pointer a struct returned by value costs the caller. The engine's
body at `0x0045f440` agrees: it reads the return pointer at entry `+4` and
six arguments at `+8` to `+28` -- model, animation, frame, tag, a scale
loaded with `fld`, and one pointer where the SDK declares two bone-controller
arrays.

`code/compat/alice_cgame_imports.def` freezes the map with each slot's
evidence and call-site measurement, and `tests/alice_module_abi_tests.cpp`
holds it to the engine measurements made before any slot had a name: the
first slot called is the appended one, the three unwritten slots are exactly
the three ABSENT rows and are never called, and every SHARED slot carries the
name the game table gives the same engine address.

## The exports

`GetCGameAPI` writes seven function pointers. Six are the SDK's
client-game exports in order, and the bodies confirm the two that say what
they do:

| Slot | Name | Arguments read | Evidence |
| --- | --- | --- | --- |
| 0 | `Init` | 3 | copies the 125 imports; stores its second and third arguments |
| 1 | `Shutdown` | 0 | |
| 2 | `DrawActiveFrame` | 3 | |
| 3 | `ConsoleCommand` | 0 | |
| 4 | `GetRendererConfig` | 0 | calls import 77, the graphics configuration, and scales the result |
| 5 | `Draw2D` | 0 | |
| 6 | Alice's own | 3 | copies two vectors out of the module's per-entity array, stride `0x5a0` |

The retail engine's calls through the table pointer it keeps at `0x00532a2c`
fix how each is used:

- `CL_InitCGame` at `0x004248d0` calls `S_BeginRegistration`, builds the
  import table on its own stack, and calls export 0 with the table,
  `clc.serverMessageSequence` and the last executed server command. It sets
  the connection state to loading before the call and primed after it.
  **fnTech3 had been calling export 0 with the table alone**, leaving the
  other two arguments to whatever the stack held; the module stores both.
- Export 2 is called at `0x004249a7` with the server time, the stereo field
  and the demo flag.
- Export 3 is tail-called at `0x0042498a`.
- Export 5 is called from the menu system's in-game view at `0x0041fe4e`,
  after the 3D frame, unless a full-screen state is up.
- Export 4 is called at `0x0042c68f`, straight after the renderer is
  restarted.
- Export 6 is called from the engine's own client-side collision at
  `0x00431261` with an entity number and two output vectors, while it walks
  the entities a box touches -- the engine asking the client game where an
  entity is drawn this frame.
- Export 1 is called at `0x004243e3` as the client game is torn down.

`code/client/client_alice_cgame.hpp` routes the engine's client-game calls to
those exports accordingly.

## The world as the renderer loads it

The collision translation drops the light grid because its record had not
been established. It is now: the retail renderer's loader at `0x0049acb0` is
Quake III's `R_LoadLightGrid` -- it writes the 192x192x320 cell, derives the
point count from the world bounds, refuses a lump that is not eight bytes a
point with `WARNING: light grid mismatch`, and colour-shifts bytes 0-2 and
3-5 of each point. So the record is Quake III's eight bytes.

Correction (2026-09-25): the shift is not exactly Quake III's. `alice.exe`
shifts by the overbright bits alone (`0x0049adf7..0x0049ae15`), where Quake
III shifts by the map's bits less the overbright bits. At retail's default of
no overbright bits a point's colours stay as the map holds them. fnTech3 now
gets the same result from its own shift with both of Alice's settings at `0`
([overbright record](2026-09-25-alice-overbright.md)).

`FAKK_TranslateToV46Ex` in `code/qcommon/bsp_fakk.h` now takes a flag that
carries lump 16 into Quake III's light grid slot. The three renderers'
`RE_LoadWorldMap` translate a FAKK 42 map with that flag and then pin
Alice's cell, because Alice reads no `gridsize` key. The collision
translation is unchanged. The entity lights, their visibility and the light
definitions are still dropped; what the retail renderer does with them is
not established here.

## FTX textures

The retail image loader at `0x00483e80` treats any name ending `.tga`
specially: it replaces the extension with `.ftx`, reads a twelve-byte header
-- width, height and a flag word it hands back but does not use to decode --
and then width x height x 4 bytes of RGBA. Only if that file is missing does
it fall back to its ordinary loaders. All 5,893 FTX files in the
installation are exactly that size.

`code/renderercommon/tr_image_ftx.c` reads the format, and the three
renderers' image loaders try it first for a `.tga` name under this profile,
as retail does.

## Pak order

Retail sorts `base/*.pk3` with a case-sensitive `strcmp` (the comparator at
`0x0043ea00`) and pushes each onto the head of the search path, so the last
name sorts highest. fnTech3 sorts case-insensitively, which ranks `Pak2` and
`Pak3` above `pak0` where retail ranks them below -- but no file in either
exists in any other pak, so the difference has no effect. The paks that do
overlap -- `pak1_large` and `pak1_small` (2,898 files, including `.tik`
definitions as well as textures), the five language paks (43 files), and
three files `pak0` shares with `pak1_small` -- sort the same way under both
rules. An installation carrying both `pak1` variants therefore mounts the
small one over the large one, in retail and here alike; that is what a full
copy of the retail media does, and it is recorded rather than changed.

## What this does not claim

No record layout the client game hands the renderer is frozen here. No
client-game import beyond the twenty listed in the gate matrix is served,
and the world load above has not yet been reached at run time, because the
client game stops at the scene-clear call before it asks for the map.

*Update, 2026-09-24:* that paragraph describes this record's own slice. 119
of the 125 slots are now served (`CL_AliceFillCGameImports` in
`code/client/client_alice_cgame.hpp`). The six that are not -- 0, 48, 49, 73,
88 and 116 -- are all `UNCALLED` in `code/compat/alice_cgame_imports.def`,
and 0, 88 and 116 are the three `ABSENT` slots the engine never writes. The
world load is reached at run time: a `garden1` client
(`.tmp/alice-unrel/parents1/console.log`) prints `119 of 125 client-game
imports are served`, then `CM_LoadMap( 'maps/garden1.bsp', 1 )` and
`...loaded 3774 faces, 20 meshes, 8 trisurfs, 0 flares`, and goes on to
`CS_ACTIVE`. See the [compatibility ledger](COMPATIBILITY.md#alice-gate-matrix)
for what the client does from there.
