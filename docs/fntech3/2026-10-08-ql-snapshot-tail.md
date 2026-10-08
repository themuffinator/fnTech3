# Quake Live's player-state tail, 2026-10-08

Retail Quake Live hands its client game a 592-byte player state in every
snapshot, and fnTech3 names only its first 504 bytes. The last 88, offsets
504 to 591, had stayed an open question since the
[snapshot marshalling correction](2026-09-07-ql-snapshot-marshalling.md):
what they are, and whether fnTech3's zeroes match what retail puts there.
Three earlier checkpoints narrowed it. The [cgame layout](2026-09-08-retail-snapshot-layout.md)
showed the client game copies all 592 bytes. The
[producer and delta reader](2026-09-08-retail-snapshot-producer.md) showed
the reader inherits the tail from its baseline in the cases tried. And
[live sampling](2026-09-08-retail-snapshot-tail-live.md) read zeroes in 490
retail history records. What none of them settled was whether anything else
in the engine writes there.

This record settles it by reading every instruction in
`quakelive_steam.exe` that can reach those bytes, on the client and on the
server. **The tail is player-state storage private to the game module.** The
retail engine copies it on the server but never sends it, and on the client
it is zero in every snapshot, live or from a demo. fnTech3 zero-fills it on
the client and never reads it on the server, which is what retail does.
fnTech3's behaviour does not change. The
[JSON companion](2026-10-08-ql-snapshot-tail.json) lists every address.

## Identities

| File | SHA-256 |
| --- | --- |
| `quakelive_steam.exe` (2,107,904 bytes) | `c926fe9f6c851e00b3b9332e88903ad01f28fdd60454873891c0158f5ded1299` |
| `baseq3/cgamex86.dll` (561,152 bytes; loose and in `bin.pk3`) | `310542161ae03cc09a2edf7c5933a3cb5e2f13d791f59d5a33a62721bbf39953` |
| `bin.pk3!qagamex86.dll` (640,512 bytes) | `9bfad1b5df4cbbb3fcfb20781024fc5c0abe73ba8389c2f20be8c0a55552e83d` |

These are the same executable and client game as the three earlier records.
They were read with pefile 2024.8.26 and Capstone 5.0.6, from the Steam
installation at `E:\SteamLibrary\steamapps\common\Quake Live`, which was only
read. All addresses below are virtual addresses (image base `0x400000` for
the executable, `0x10000000` for the modules). No retail code is
incorporated, and no retail program ran for this record.

## Observed: the client

The client keeps a 32-slot history of 664-byte snapshot records at
`0x1472874`, each holding its player state at offset `0x3c`, and a current
snapshot `cl.snap` at `0x146cd24` inside the client state `cl` at
`0x146cd20`. A disassembly of the whole code section found every
instruction whose operand is an address in the history or in `cl.snap`.
A raw byte search of every section for those addresses then found nothing
more: its three further matches are bytes straddling unrelated instructions
(`0x4b511f`, a `call`; `0x4ceda7`, a branch; `0x4ee33e`, a byte load). Each
decodes the same from every starting offset tried.

The instructions are these, and only these:

- **The snapshot producer** (`0x4af570`, the client game's import 87) reads a
  slot at `0x4af5a9` and copies its player state to the client game.
- **The snapshot parser** (`0x4bd350`) holds its new snapshot in a 664-byte
  local that it zeroes first (`0x4bd376`). It takes the delta's base slot at
  `0x4bd437` and passes it, read-only, to the player-state delta reader
  (`0x4d66c0`) and the entity parser (`0x4bd000`). Invalid slots get only
  their `valid` word cleared (`0x4bd54e`). It copies the local into `cl.snap`
  (`0x4bd568`) and `cl.snap` into the slot (`0x4bd5d6`), 166 doublewords each
  time, and writes `cl.snap`'s ping (offset `0x14`).
- **Three clears of all of `cl`**, 0xbbe54 bytes zeroed at `0x4b892c`,
  `0x4b8a10` and `0x4bc6af`.
- **Reads** of `cl.snap`'s header words, from timing and lag code.

Inside the parser, the local snapshot is written only by:

- the delta reader, which zeroes a local baseline when the delta has no base
  (`0x4d66fa`), copies 592 bytes of that or the base slot's player state into
  the local (`0x4d670b`), and then stores the 58 fields of its table at
  `0x5424d8`;
- the entity parser, which writes the local's entity count and first entity
  (offsets `0x28c` and `0x290`) and nothing else in it;
- the area-mask read at `0x4bd497`, at most 32 bytes at offset `0x18`
  (bounded by the check at `0x4bd404`).

The delta reader has no fixed displacement at or beyond 480 bytes (it ends at
`0x4d6ba4`), and its field table ends at byte 478 (`upmove`). It stores every
field, the three one-byte movement fields included, as a four-byte word
(`0x4d67c4`, `0x4d682e`, `0x4d6868`, `0x4d68c2`). A word stored at
`upmove`'s offset therefore covers bytes 478 to 481. Its upper three bytes are
zero, or copies of the baseline's, which are zero by the same argument.

**So every client player state starts from zero, and only the delta reader's
baseline copy ever writes bytes 504 to 591.** It copies them from zero or from
an earlier slot. By induction, every slot's tail, `cl.snap`'s, and therefore
every snapshot the client game receives, holds 88 zero bytes. The same holds
for `ping` and the bytes after it up to 503, which the table never names. This
agrees with the 490 live samples and the emulated reader. A demo plays through
the same parser, so it holds there too.

## Observed: the server

The server reaches the game's client records only through `SV_GameClientNum`
(`0x4e10d0`): base `0x1333774` plus index times the game's stated client size
`0x1333778`, both set once, at `0x4e1353` and `0x4e1358`, when the game
locates its data.
Nothing else in the executable names either variable. Its three callers:

- the `status` listing reads a score word (`0x4dece5`);
- `SV_CalcPings` writes the client's ping at offset 480 (`0x4e47b4`), the
  offset fnTech3 already uses (`tests/game_abi_layout_tests.cpp`);
- `SV_BuildClientSnapshot` copies 592 bytes into the frame (`0x4e5719`).

The player-state delta writer (`0x4d5d50` to `0x4d66b9`) has no fixed
displacement at or beyond 480 either, and walks the same 58-field table. The
tail rides along in every server frame and is never encoded.

## Observed: the game module

In `qagamex86.dll`, following the player pointer each entity holds at offset
`0x23c` (1,200 loads) leads to reads and writes in the tail. Offset `0x1f8` is
a flag set to 1 or cleared (`0x1004bd3b`, `0x1004d168`) that guards a client
number latched from another player's `clientNum` into `0x1fc`
(`0x1004d15c`). Offset `0x204` is added to with the same amount the
`persistant` score at `0x100` takes (`0x1006457c`, after `0x1006456a`), and
again at `0x100466cb`. The words from `0x208` to `0x21c` are read where the
game decides its medals, each beside the name it awards: `COMBOKILL`
(`0x100479f5`), `RAMPAGE` (`0x100483de`), `MIDAIR` and `HEADSHOT`
(`0x10047ba9`, `0x10047be5`), `PERFORATED` (`0x10047c0e`) and `REVENGE`
(`0x10047968`). The client game reads none of these through the snapshot: the
[layout record](2026-09-08-retail-snapshot-layout.md) found no direct use of
the copied tail.

## Inference

The tail is the end of the game module's own player state: medal and scoring
bookkeeping the game keeps per client, which the engine carries between the
game's memory and its frames but which no retail engine path puts on the wire.
On the client those 88 bytes are always zero. fnTech3's client builds the
native snapshot with a zeroed tail (`FnTech3_QLSnapshotPlayerStateFromInternal`,
tested in `tests/game_abi_adapter_tests.cpp` with guards on both sides). Its
`ping`, `pmove_framecount`, `jumppad_frame`, `entityEventSequence`,
`externalEventTime` and `armorTier` are never written on the client either,
so they are zero as well, matching retail's bytes 480 to 503. Its server reads
the game's records through the 504-byte prefix at the game's own stride,
writes `ping` at retail's offset, and encodes the same 58 fields. The
remaining difference, already recorded in the
[wire checkpoint](2026-09-08-retail-playerstate-wire.md), is the encoder's:
retail compares the movement bytes as four-byte words, so a change to `ping`
alone also re-sends `upmove`. Both encodings decode to the same state.

The meanings given above to individual words are inferences from the
instructions that use them; the engine contract does not depend on them.

## Scope

This settles the snapshot tail for the module ABI gate. That gate stays
partial for Quake Live, because the rest of its boundary audit and native
module coverage remain open. No engine source or behaviour changed, so no
build or test run is claimed for this record. Two code comments that called
the tail unexplained now point here.
