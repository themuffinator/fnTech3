# Jedi Academy module numbers

## Scope

Jedi Academy's multiplayer modules and the engine speak to each other by
number, as Quake III's do. This record registers those numbers -- 796 of them
across the game, client game and menu modules, in both directions -- so that
later work has one frozen table to implement against. **Nothing dispatches them
yet**: no import is served, no export is called, and no argument or record
contract is described here. Registering a number is not compatibility.

Single player is not in this table. It hands the engine a `GetGameAPI` function
table instead of numbering its calls, which the
[profile foundation](2026-09-21-jka-contracts.md) records; its own table is a
later slice.

## Where the modules are, and what they export

A legitimate 1.01 installation settles both. Single player's `jagamex86.dll`
sits in the installation root beside `jasp.exe` and exports `GetGameAPI`,
`dllEntry` and `vmMain`: a function table for the game, and the client game's
entry points from the same library. Multiplayer's three modules are pak members
rather than loose files -- `assets2.pk3` carries `jampgamex86.dll`,
`cgamex86.dll` and `uix86.dll`, and `assets3.pk3` carries the patched copies --
and each exports `dllEntry` and `vmMain` and nothing else, which is Quake III's
native shape. Only the export tables were parsed; no module was loaded.

## How the numbers were read

Raven kept Quake III's habit of starting a block at a round number, so an
enumerator's position and its ordinal are different things: the game module's
imports run 0-78, then 100-199 for the memory and string calls, 200 for
navigation, 250 for the bot library's setup, 300 for its areas, 400 for its
elementary actions, 500 for its characters, and on to 634.

The table is read from Raven's released multiplayer sources as carried by
[OpenJK](https://github.com/JACoders/OpenJK/tree/1a6a643427aa347553e9073dac5570b33337c4d9)
at `1a6a643427aa347553e9073dac5570b33337c4d9`, from the six legacy
enumerations those modules and the retail engine share:

| Module | Direction | Source | Entries | Highest ordinal |
| --- | --- | --- | --- | --- |
| Game | Import | `codemp/game/g_public.h:378` | 329 | 634 |
| Game | Export | `codemp/game/g_public.h:717` | 40 | 39 |
| Client game | Import | `codemp/cgame/cg_public.h:203` | 233 | 350 |
| Client game | Export | `codemp/cgame/cg_public.h:441` | 32 | 31 |
| Menus | Import | `codemp/ui/ui_public.h:59` | 150 | 235 |
| Menus | Export | `codemp/ui/ui_public.h:214` | 12 | 11 |

Two independently written readings of those enumerations -- one splitting the
body on commas, one reading it line by line -- agree on every one of the 796
names and ordinals. Names are interoperability facts; no implementation text is
incorporated.

The API version each module negotiates with is recorded beside them: game 1,
client game 2, menus 3 (`codemp/game/g_public.h:32`,
`codemp/cgame/cg_public.h:26`, `codemp/ui/ui_public.h:26`), with single
player's game version 10 (`code/game/g_public.h:28`).

## What the table guarantees

`code/compat/jka_module_calls.def` holds the numbers and
`code/compat/jka_module_abi.h` turns them into a frozen enumeration and a
constant lookup table. The checks in `tests/jka_module_abi_tests.cpp`
(`meson test fntech3_jka_module_abi`) hold it to this:

- every module and direction has exactly the count and highest ordinal above;
- no two calls in one module and direction share a number, which is a
  compile-time assertion as well as a runtime check;
- every call resolves from its own number and back to that number again;
- the gaps Raven left -- 306 in the game's imports, 118 in the client game's
  and 86 in the menus' -- resolve to an explicit refusal, not to a neighbour;
- a number outside the table, an invalid module or direction, and every other
  profile, including Jedi Academy's own single player, resolve to nothing.

## Names shared with Quake III

Many names are Quake III's, and a few sit at Quake III's numbers, but the
tables are not Quake III's: Raven inserted its own calls throughout, so a name
match implies neither the same ordinal nor the same arguments. `CG_MEMSET` does
not even exist -- the client game calls it `CGAME_MEMSET`. Anything that
assumes a Quake III ordinal under a Jedi Academy profile is a defect.
