# Profile frame time and entity arenas, 2026-10-09

The non-Elite-Force profiles inherited Quake3e's scaled server tick, float
frame multiply, stopped clock at timescale zero and post-load clock restart.
Reading the installed engines shows that those are not a common retail
contract. QL does scale its tick; the other readable engines do not. This
change selects the observed rules by immutable startup profile, and admits
the complete entity arena for Q3, QL, JK2 MP and JKA MP.

The [JSON companion](2026-10-09-profile-frame-rules.json) pins the executable
paths, SHA-256 identities, instruction addresses and frame-baseline writes.
These are **Source** observations of retail machine code and **Unit** evidence
from production functions, not new Live or Interop evidence. No retail code,
assets or disassembly is incorporated into the repository.

## Observed frame contracts

`fixedtime` takes precedence. Otherwise zero timescale leaves elapsed time
unscaled, except that Q3, QL and Jedi MP multiply by zero in camera mode.
The table's floor is applied after either fixedtime or timescale. “Shared”
caps mean 5,000 ms on a dedicated server or remote client, 200 ms while
hosting a local game. The cinematic cap is 500 ms while `skippingCinematic`
is set and 200 otherwise, even without a running game.

| Profile / installed engine | Tick | Product | 1 ms floor | Cap |
| --- | --- | --- | --- | --- |
| Q3 1.32 | `1000 / integer fps` | x87, truncated | nonzero timescale | shared |
| QL build 1069, June 3 2016 | integer division above, then x87 timescale product | x87, truncated | nonzero timescale | shared |
| RTCW SP and MP | `1000 / integer fps` | x87, truncated | nonzero timescale | shared |
| ET installed Steam 2.60b executable (2021 PE timestamp) | `1000 / integer fps` | **float SSE**, truncated | nonzero timescale | shared |
| JK2 MP | `1000 / integer fps` | x87, truncated | nonzero timescale | shared |
| JKA MP | `1000 / integer fps` | integer elapsed converted to float first, then x87 product | nonzero timescale | shared |
| JK2 SP and JKA SP | `1000 / integer fps` | floor x87 product, carry float remainder | always | cinematic |
| Alice | `1000 / integer fps` | x87, truncated | always | shared |
| Q3 IHV | truncated `1000.0 / float fps` | x87, truncated | always | 200, including dedicated |
| Q3Test 1.02, 1.03, 1.05, 1.06, 1.07, 1.08 | `1000 / integer fps` | x87, truncated | always | 200, including dedicated |

All six installed public Q3Test releases were inspected independently. There
is no installed 1.04 release to claim. IHV reads its fixedtime cvar's float
field and truncates it, including a nonzero value below 1; fnTech3 now
selects that float field for IHV too.

For example, a 10 ms frame at timescale 0.7 has a product just below 7.
An x87 multiply followed by integer conversion yields 6; the installed ET
build's float multiply yields 7. Jedi SP stores that product as a float too,
then subtracts the floored integer: its remainder is 1 in this case. The
server adds remainders after its running/pause checks, subtracting one and
adding one millisecond when the total reaches 1. Both engines register
`cl_newClock` with default 1; fnTech3 adopts that default without introducing
the old-clock option. Its per-level fractional accumulator resets with the
server state. Floors and caps discard the current frame's fraction.

In every readable engine, the only direct writes to the prior-frame baseline
are in `Com_Frame`: one repairs a backwards clock, and one stores the sampled
frame start. None is in `SV_SpawnServer`. A map load therefore contributes
to the next frame, subject to the profile's cap. fnTech3 now preserves that
baseline for those profiles, as it already did for Elite Force.

## Entity records

Every inspected engine's `SV_SvEntityForGentity` rejects NULL and a record
number outside 0..1023; none checks the published entity count. IHV reads a
signed 16-bit number, the others a 32-bit number. Q3, QL, JK2 MP and JKA MP
were the remaining shared bindings limited to the published count, so they
now bind 1024 records. The existing RTCW, ET, Elite Force, IHV, Q3Test and
Jedi SP adapters already admit their full arenas (JK2 SP shares the JKA SP
binding boundary). Ritual and MOHAA retain their explicitly published
capacities. This does not change snapshot iteration over published entities.

fnTech3 still requires a pointer to an aligned record inside the declared,
validated memory span. Retail's acceptance of arbitrary addresses is not
reproduced. The tests check the world record beyond the high-water mark,
the arena endpoint, invalid capacities, short spans and untouched guards.

## Limits and inferences

- F.A.K.K.2's encrypted executable cannot establish these rules here. Alice
  supplies related-engine evidence only. Its timing policy is unchanged.
- MOHAA's SafeDisc executable likewise remains unreadable as engine code.
  The pinned [OpenMoHAA source](https://github.com/openmoh/openmohaa/tree/ab43a0def6c4feccedae36fdb58c2e007d7e4c35)
  has fixed ticks, zero left unscaled, a floor for nonzero timescales and the
  shared caps. It is a reconstruction, not a retail observation; MOHAA's
  timing remains unchanged. Its entity adapter already binds the module's
  published capacity. No implementation text was copied from this source.
- QL's retail tick can become zero at zero or tiny timescales. fnTech3 keeps
  its nonpositive-tick guard and 1 ms minimum for positive scales, avoiding
  the retail loop's nontermination. Ordinary positive scales now use retail's
  integer division and product precision.
- Existing fps limits, network scheduling, snapshot cadence, bot scheduling,
  diagnostics and FnQ3/FnQL presentation remain as before. This record covers
  the listed frame conversion, tick, spawn-clock and entity-bound rules,
  not every timing behavior of an engine or campaign.

## Validation

`tests/stvef_frame_time_tests.py` compiles the production frame conversion for
all sixteen profiles: zero, negative and tiny scales, rounding, camera mode,
fixedtime, floor and cap boundaries, cinematic skipping and fractional carry.
`tests/stvef_sp_cadence_tests.py` runs the production server loop with the
observed fixed and scaled ticks, including IHV/Q3Test and Jedi fractional
frames; Elite Force's 780 retail cadence cases remain intact.
`tests/world_owner_trace_tests.py` and `tests/server_game_data_tests.cpp`
exercise the widened arenas without widening snapshot counts.

The full planned-tree sweep covers 524 Python test files, each in its own
process with a 900-second cap, at most four at once: 517 pass under pytest;
the seven script-only entry points pass when run directly (pytest otherwise
collects nothing from them): 2,323 pytest cases pass and 16 skip. Platform
and opt-in runtime skips remain.
All changed production translation units compile with the existing Windows
x86 preset, and the complete entity-data C++ executable passes. Three
mutations (float product, stopped zero clock, scaled fixed-profile tick)
are rejected. Final test refinements and explicit ET float materialization
pass all 10 focused tests and production recompilation; the JSON companion
records the exact validation.
Retail files were only read; no game, input control or screen capture was
used for this pass. Full compatibility gates remain open.
