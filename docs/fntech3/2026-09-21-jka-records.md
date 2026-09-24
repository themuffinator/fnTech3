# Jedi Academy's shared records, frozen

## Scope

Everything Jedi Academy's modules and its protocol exchange travels in a
handful of records: an entity's state, a player's whole state, the entity the
engine and the game share, an input frame, a trajectory and the block of Force
powers inside a player. Both the module host and the protocol-26 codec have to
agree with the retail modules on every byte of them, so this slice freezes
those layouts and proves the freeze.

Nothing uses them yet. Registering a layout is not a module host and not a
codec; it is the thing both of those must be built against.

## What is frozen

`code/compat/jka_game_abi.h` declares seven records with fnTech3's own names
and explicit-width members:

| Record | Bytes | Members |
| --- | --- | --- |
| `fntech3_jka_trajectory_t` | 36 | 5 |
| `fntech3_jka_usercmd_t` | 28 | 10 |
| `fntech3_jka_forcedata_t` | 464 | 43 |
| `fntech3_jka_entityState_t` | 532 | 94 |
| `fntech3_jka_playerState_t` | 1,552 | 168 |
| `fntech3_jka_entityShared_t` | 112 | 15 |
| `fntech3_jka_sharedEntity_t` | 864 | 23 |

A shared entity is the two halves in Raven's own order: the entity state at 0,
the game's own fields after it, and the engine's shared half at 560.

These are boundary records, not engine types. Nothing outside this title's
adapters may take their shapes, and none of Quake III's records may stand in
for them: a Jedi Academy player state is 1,552 bytes where Quake III's is 476,
and an entity state 532 where Quake III's is 208.

## How the layout was established

The layouts were measured, not guessed. A probe compiled Raven's released
headers, as carried by OpenJK at
`1a6a643427aa347553e9073dac5570b33337c4d9`, for Win32 x86 and printed
`offsetof` and `sizeof` for every member of every record. That output is
`code/compat/jka_wire_layout.def`, 363 members in declaration order.

`tests/jka_abi_layout_tests.cpp` then asserts, at compile time, that fnTech3's
own records agree with every line of it: each record's size, and each member's
offset, size and kind. The kind matters as much as the width: 293 of the
members are integers, 56 are floating point and 14 are a nested record or a
pointer, and a float declared as an integer of the same width would read as
nonsense while passing every size check. It also counts the table, so a record cannot quietly lose a
member, and checks that the measured members leave no hole -- each record's
members run end to end with no gap, and only the input frame has a byte of tail
padding, after its three movement bytes.

Member names and order are interoperability facts; no implementation text is
incorporated. Pointer members inside the shared entity -- the vehicle, the
Ghoul2 instance, the script parameters -- are declared opaque, because their
width is what crosses the boundary and their contents belong to the module.

## What this does not settle

- Which members the protocol actually sends, in what order and at what width:
  that is the delta table, and the [foundation record](2026-09-21-jka-contracts.md)
  records why the installation's own `netf_overrides.txt` cannot be used for it.
- How the engine hands these records to a module: the game's entity array, its
  stride and the import that locates it belong to the module host slice, whose
  numbers are in the [module record](2026-09-21-jka-module-abi.md).
- Anything about single player, whose records are its own and are unwritten.
