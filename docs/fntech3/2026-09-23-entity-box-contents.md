# Box entities traced as their own contents: RTCW MP and ET

## Scope

A server trace tests every entity that is not a brush model against a
temporary box brush. In fnTech3 that brush's contents were always
`CONTENTS_BODY`, as they are in Quake III. Retail RTCW MP and ET give the brush
the traced entity's own `r.contents` for the sweep that tests it, and put
`CONTENTS_BODY` back afterwards. The brush's contents decide two things in the
shared trace code: whether the trace mask reaches the entity at all
(`b->contents & tw->contents` in `CM_TraceThroughLeaf` and `CM_TestInLeaf`),
and what `trace.contents` reports when it does. This record covers the
`rtcw-mp` and `wolfet` profiles. Every other profile keeps `CONTENTS_BODY`.

## Observed

### Sources

- The [RTCW MP](https://github.com/id-Software/RTCW-MP/blob/master/src/server/sv_world.c)
  and [ET](https://github.com/id-Software/Enemy-Territory/blob/40342a9e3690cb5b627a433d4d5cbf30e3c57698/src/server/sv_world.c)
  `SV_ClipMoveToEntities` call `CM_SetTempBoxModelContents( touch->r.contents )`
  when the entity's clip handle is `BOX_MODEL_HANDLE`, 511, before
  `CM_TransformedBoxTrace`. After merging that entity's result they call it
  again with `CONTENTS_BODY`. The setter (`qcommon/cm_load.c`) writes
  `box_brush->contents`. The capsule handle, 510, is never set.
- Neither engine's `SV_ClipToEntity` or `SV_PointContents` calls it.
- The [RTCW SP](https://github.com/id-Software/RTCW-SP/blob/master/src/server/sv_world.c)
  source has no such call. iortcw keeps the call in MP and has none in SP.
  None was found in OpenJK (Jedi Academy and Jedi Outcast), the Elite Force
  references (Elite Reinforce, Lilium Voyager) or the Quake III source.

### Retail executables

The executables were read with capstone. Addresses are virtual addresses in
each image.

| | `WolfMP.exe` | ET 2.60b `et.exe` | ET 2.60b `etded.exe` | `WolfSP.exe` |
| --- | --- | --- | --- | --- |
| SHA-256 | `7cf99a938cf18ed2f5aecd0be542cbae8868959e0cce24b4b6eea08991e321f5` | `4d1326c5936b61c5f878fc017e4aa0abddf13e008139e31fce2f13d57b26243e` | `7eb606913a56d6901e358a598fb0079120b2730e113a10fc963f90b777580cdf` | `07d206f61da5c80562ff60d68c645bca349b31adb5d7e0e7f186d61ad46683c7` |
| Contents setter | `0x4164a0`: loads `box_brush` (`0xedd408`) and stores its argument at `+4` | `0x420f00`: the same through `0x121da50` | `0x406270`: the same through `0xc94390` | none |
| Its calls | exactly two, both in `SV_ClipMoveToEntities` behind `cmp ebp, 0x1ff`: `0x43d348` pushes `[esi+0x26c]`, the entity's `r.contents`, and `0x43d3f0` pushes `0x2000000` | exactly two, behind `cmp ..., 0x1ff`: `0x449e6e` pushes `[edi+0x14c]`, ET's `r.contents`, and `0x449f1c` pushes `0x2000000` | exactly two: `0x4307af` pushes `[edi+0x14c]` and `0x430881` pushes `0x2000000` | -- |
| Order in the sweep | clip handle from `0x43c9a0` at `0x43d32f`; set; trace (`0x41d4c0`) at `0x43d38b`; merge; restore at `0x43d3e3`..`0x43d3f0` | handle at `0x449e4e`; set; trace (`0x429420`) at `0x449ead`; merge; restore at `0x449f0e`..`0x449f1c` | set at `0x4307a2`; trace at `0x4307f4`; restore at `0x430870` | handle from `0x439580` at `0x439f1d`, trace at `0x439f60`, no set or restore |
| `box_brush` elsewhere | `CM_InitBoxHull` writes `0x2000000` at `+4` (`0x41628f`); `CM_TempBoxModel` writes only the bounds, `+8`..`+0x1c` | `CM_InitBoxHull` at `0x420a62`; `CM_TempBoxModel` only the bounds | not traced | `CM_InitBoxHull` writes `0x2000000` (`0x41367f`); `CM_TempBoxModel` (`0x413740`) only the bounds |

The entity filters that come first in each loop (pass entity, owner, and a
mask that shares nothing with `r.contents`) jump past both the set and the
restore. A raw scan for the four bytes of each engine's `box_brush` address
finds the same references the disassembly decodes: 10 in `WolfMP.exe`, 9 in
`WolfSP.exe`.

These are inferences from the RTCW MP and ET game sources, not runtime
observations:

- A mask reaches a box entity through the entity's own contents. A dead
  player moves with `MASK_PLAYERSOLID & ~CONTENTS_BODY` in both games, as
  does a spectator in RTCW MP (`g_active.c`; ET's spectators trace no
  entities). In retail a box entity whose contents are `CONTENTS_SOLID` stops
  them. fnTech3 let them through, since the box brush said `CONTENTS_BODY`,
  which that mask excludes.
- A trace reports what it struck. Neither game gives players `EF_CAPSULE`
  (it is commented out in `g_client.c`), so players and their corpses are
  boxes. A shot (`MASK_SHOT`) into a corpse, which `player_die` gives
  `CONTENTS_CORPSE`, reports `CONTENTS_CORPSE`, and RTCW MP's head box
  (`G_BuildHead`, `CONTENTS_SOLID`) reports `CONTENTS_SOLID`. fnTech3
  reported `CONTENTS_BODY` for both.
- Entities that are traced whatever the brush says, because the mask holds
  `CONTENTS_BODY` and their own contents, are hit exactly as before.

## Change

- The profile catalog gains `entityBoxContentsPolicy`, next to
  `entityHullPolicy`. `rtcw-mp` and `wolfet` have
  `COM_ENTITY_BOX_CONTENTS_ENTITY`; every other profile, RTCW SP included, has
  `COM_ENTITY_BOX_CONTENTS_BODY`, the zero value.
- `CM_SetTempBoxModelContents` (`code/qcommon/cm_load.c`) sets the box brush's
  contents, as the retail setter does. `CM_TempBoxModel` still leaves them
  alone.
- `SV_ClipMoveToEntities` asks `UseEntityBoxContents`
  (`code/server/sv_collision_model.hpp`) once it has the entity's clip handle.
  Under the entity policy, and only when the handle is the temporary box, it
  sets the entity's contents before the sweep. A `SV_MakeScopeExit` guard
  puts `CONTENTS_BODY` back when that entity's iteration ends: after its
  result is merged, as in retail, and also on the Quake III IHV path's early
  `continue`, an all-solid return or a thrown exception. A `Com_Error`
  longjmp may skip the guard, but it also stops the server, and the next
  `CM_LoadMap` builds the box again as `CONTENTS_BODY`.
- `SV_ClipToEntity`, which the bot library's entity traces use,
  `SV_PointContents` and the client game's own traces still see
  `CONTENTS_BODY`, as in retail.

The RTCW, ET and iortcw sources and the retail executables were read as
reference observations. No text from them is incorporated; the setter keeps
the original engine's name so the two can be compared.

## Evidence

- `tests/collision_model_parity_tests.cpp` checks `UseEntityBoxContents`, and
  that a zeroed profile keeps `CONTENTS_BODY`. `tests/game_profile_tests.cpp`
  checks that exactly `rtcw-mp` and `wolfet` carry the entity policy.
- `tests/entity_box_contents_tests.py` has three tests.
  - Real traces. It compiles the production `SV_Trace`,
    `SV_ClipMoveToEntities`, `SV_ClipToEntity`, `SV_PointContents` and
    `SV_ClipHandleForEntity` with the game-data view, over the Quake III,
    Quake Live, RTCW SP, RTCW MP and ET entity layouts. They run against
    `cm_load.c`'s own box hull functions and the real `cm_trace.c` and
    `cm_test.c`, in an empty world. Five box entities lie along one line: a
    corpse, a trigger, a solid box, a body, and a corpse flagged as a capsule.

    | Sweep | `rtcw-mp`, `wolfet` | `q3`, `ql`, `rtcw-sp` |
    | --- | --- | --- |
    | `MASK_SHOT` | corpse, `CONTENTS_CORPSE` | corpse, `CONTENTS_BODY` |
    | `CONTENTS_CORPSE` | corpse, `CONTENTS_CORPSE` | nothing |
    | `CONTENTS_TRIGGER` | trigger, `CONTENTS_TRIGGER` | nothing |
    | `CONTENTS_SOLID` | solid box, `CONTENTS_SOLID` | nothing |
    | `CONTENTS_BODY` | body, `CONTENTS_BODY` | body, `CONTENTS_BODY` |
    | `MASK_SHOT` at the capsule | capsule, `CONTENTS_BODY` | the same |
    | `CONTENTS_CORPSE` at the capsule | nothing | nothing |
    | Starting inside the corpse, `CONTENTS_CORPSE` | all solid, `CONTENTS_CORPSE` | nothing |

    After every sweep the box brush is `CONTENTS_BODY` again. In every
    profile, `SV_ClipToEntity`, `SV_PointContents` and a client-style
    `CM_TempBoxModel` trace test the corpse as `CONTENTS_BODY`, and the
    module's records are unchanged.
  - Every way out. With the collision calls recorded, every catalog profile
    runs with its own policy and again with the entity policy forced on. The
    forced runs take each profile's own path through the loop. The box
    handle alone is set, and only around its own sweep; the capsule and
    inline-model handles never are. The box is `CONTENTS_BODY` again after a
    normal sweep, after an all-solid return and after an exception thrown
    from inside the sweep. `SV_ClipToEntity` and `SV_PointContents` never set
    it.
  - Source. Only `SV_ClipMoveToEntities` calls the setter, it sets before
    the sweep under a scope guard, and `CM_TempBoxModel` leaves the contents
    alone.
- Six in-memory mutations of `sv_world.cpp` and `cm_load.c` each fail at
  least one of the two behavioral tests: never setting, setting for every
  profile, setting for capsules too, never restoring, a setter that ignores
  its argument, and restoring by a plain statement after the sweep. The last
  fails only the thrown-sweep check, which is what the scope guard is for.
- Two fixtures from other work, uncommitted when this was written, also
  compile the production `SV_ClipMoveToEntities`:
  `tests/world_owner_trace_tests.py` and `tests/q3_ihv_trace_tests.py`. They
  were given the two headers it now needs and a no-op setter, which also
  compile without this change, and pass. They are not part of this change.
- This change alone on `75561b9` was exported and built for Windows x86 with
  MSVC, `debugoptimized` and strict warnings. The RTCW, Elite Force and ET
  development options were on, as in `meson/build/win32-dev`. It built
  without a compiler warning. `fntech3.exe` had SHA-256
  `03c27ede47240013dda50cb9f918f5e4d974146120735a544b8ef61268c40603` and
  `fntech3.ded.exe`
  `a3a332f7913bc70d62eb890036c83bf47129dac920c9fc10a6747f409c7ae2f5`. Its 116
  Meson C++ tests pass and two skip. The full Python suite passes, 1,835 tests
  with 76 skipped, run in a throwaway repository of that tree, because the
  release-packaging check names its source commit through `git`.

## Open

- No retail session was compared with fnTech3 for a trace whose mask lacks
  `CONTENTS_BODY`. The gameplay effects above are inferred from the game
  sources.
- Three related differences were found and left alone, for their own
  changes; RTCW's two are followed since the
  [entity-query record](2026-10-08-rtcw-entity-queries.md):
  - ET's `SV_ClipMoveToEntities` and `SV_PointContents` skip an entity whose
    clip handle is 0, the world (`et.exe` `0x449e59` and `0x44a63d`). fnTech3
    traces it.
  - RTCW SP's `SV_ClipMoveToEntities` skips an `ET_PROP` whose
    `s.otherEntityNum` is the pass entity plus one, a carried chair
    (`WolfSP.exe` `0x439f07`..`0x439f16`). fnTech3 now does too.
  - `WolfMP.exe`'s `SV_PointContents` passes the entity's `s.origin` and
    `s.angles` to `CM_TransformedPointContents` (`0x43d60a`..`0x43d614`), as
    the Quake III source does, and so does `WolfSP.exe` (`0x43a1ba`..
    `0x43a1c4`). fnTech3 now does too for both RTCW modes. `et.exe` passes
    `r.currentOrigin` and `r.currentAngles`.
- Jedi Outcast's and Jedi Academy's `CONTENTS_BODY` is `0x100`, not Quake
  III's `0x2000000`, which Raven leaves unused. Their retail engines build the
  box brush with `0x100`: `jk2mp.exe` at `0x420e92`, `jk2sp.exe` at
  `0x40d092`, `jamp.exe` at `0x428abd` and `jasp.exe` at `0x40da6b`. fnTech3
  builds it with `0x2000000` for every profile, so under `jk2-*` and `jka-*`
  a mask that names their `CONTENTS_BODY` but not the unused bit reaches no
  box entity. That resting value would have to be profile-owned too,
  including where this change's restore writes it, which only `rtcw-mp` and
  `wolfet` reach.
