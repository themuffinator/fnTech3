# Elite Force Holomatch wire: protocol 24 messages and packet scramble

## Scope

This slice gives `stvef-mp` the retail Holomatch protocol-24 message layer:
raw least-significant-bit-first packing with Elite Force's byte alignment,
unfiltered strings, entity deltas coded through Raven's change-vector
dictionary, player-state and unkeyed user-command deltas, and the netchan
wire profile with its per-packet scramble. It also freezes the Elite Force
module records both modes will pass across their boundaries.

Server and client framing (where the reliable acknowledgement travels, the
gamestate terminator, the handshake), the QVM import tables, downloads and
pure admission are later slices. Both profiles remain experimental and gated
behind `-Dstvef-development=true`; nothing here claims gameplay or retail
interoperability. The [compatibility ledger](COMPATIBILITY.md#elite-force-gate-matrix)
tracks what remains.

## Evidence and provenance

The retail engine is `stvoyHM.exe` 1.20 (`ST:V HM v1.20 win-x86 Apr 17 2001`,
identified in the [foundation record](2026-09-18-stvef-contracts.md)). Facts
come from its machine code, read through a local decompilation and checked
against the instructions at these addresses:

| Routine | Address | Contract taken from it |
| --- | --- | --- |
| `MSG_WriteBits` / `MSG_ReadBits` | `0x422bb0` / `0x422cb0` | Every width from 1 to 32 bits is packed least significant first; each new byte starts cleared; there is no Huffman stage |
| `MSG_WriteData`, `MSG_WriteString`, `MSG_WriteBigString` | `0x422b10`, `0x422dd0`, `0x422fc0` | Inside a partly used byte, data and strings start one whole byte after it; strings are written unfiltered (1,024 and 8,192 limits) |
| `MSG_ReadString`, `ReadBigString`, `ReadStringLine`, `ReadData` | `0x423250`-`0x4233b0` | The same skip on reading; `%` becomes `.`; bytes above 127 are kept |
| `MSG_WriteDeltaEntity` / `MSG_ReadDeltaEntity` | `0x4237a0` / `0x4239f0` | 50-field table (`0x4c85f0`); a 5-bit dictionary index, or 31 and the raw 50-bit vector; changed values with no per-field flags; floats as 0 + 13-bit biased integer or 1 + 32 raw bits |
| Change-vector dictionary | `0x4c55f0` | 31 eight-byte vectors searched first match first; a 32nd record is never used |
| `MSG_WriteDeltaPlayerstate` / `MSG_ReadDeltaPlayerstate` | `0x423cd0` / `0x423ff0` | 48 fields each with a changed bit (`0x4c8848`), then stats, persistant, ammo (16-bit) and powerups (32-bit) blocks, always present |
| `MSG_WriteDeltaUsercmd` / `MSG_ReadDeltaUsercmd` | `0x423410` / `0x4235e0` | No key; one-byte buttons and weapon; a movement byte of -128 is kept |
| Packet scramble | `0x4246c0` | Seed `(first dword * 3) ^ (length * 123) ^ 0x87243987`; LCG `69069 * s + 1`; XOR from byte six, then swaps within a halved all-ones mask |
| `Netchan_Transmit` / `Netchan_Process` | `0x4247d0` / `0x4249c0` | Sequence, client qport, no checksum, 1,300-byte fragments; restore before parsing |

[Lilium Voyager](https://github.com/zturtleman/lilium-voyager/tree/0f7dcd89d142c8b635ce2756a80cb62411731848)
(GPL-2.0-or-later) implements the same protocol as its "compat" mode and was
used to cross-check `qcommon/msg.c` and `qcommon/net_chan.c`. Where the two
differ, retail is followed: Lilium clamps a -128 movement byte to -127 and
retail does not.

## Engine design

- `msg_t` gains `alignSkipsByte`, which `MSG_STVEFBitstream` sets with the raw
  bit mode. `MSG_WriteData`, every string writer, `MSG_ReadData` and every
  string reader apply the skip only in that mode, so Quake III's raw legacy
  demo reader keeps its own rule. The raw writer clears each new byte and the
  skip clears the byte it passes over, so output is reproducible; retail
  leaves the skipped byte's stale contents, which no reader examines.
- `NETCHAN_WIRE_STVEF_HM` enables the client qport, the scramble and raw
  messages, with no sequence checksum and no reliable XOR. Its contract keeps
  16,384-byte messages, 64 reliable commands, 32 packet backups and retail's
  2,048-byte download blocks in a window of 8.
- `code/compat/stvef_game_abi.h` freezes the Holomatch and single-player
  records (sizes and offsets in `tests/stvef_abi_layout_tests.cpp`), and
  `stvef_module_calls.def` records both modes' module numbering.
  `stvef_message_codec.hpp` codes the frozen Holomatch records; the engine
  never casts its canonical state onto them. `stvef_state_adapter.hpp`
  converts both ways: the third and fourth player events, the recharge, use
  and intro timers and the damage shield count live in the canonical
  state's Elite Force extension, and only Quake III's six trajectory types
  are accepted.
- The single-player wire profile (protocol 40) is loopback-only, and its
  delta bodies are fnTech3's own ([single-player host](2026-09-19-stvef-sp-host.md)).

## Retail demo decoding

The retail paks ship Holomatch recordings made by `stvoyHM.exe`. The probe
extracts the three a 1.20 search path opens (`pak2.pk3` replaces `pak1.pk3`'s
protocol-22 files of the same name), decodes every record with the engine's
message layer, and re-encodes every player and entity delta to compare its
bits with the recording. It reads the installation and writes only below its
output directory:

```text
meson compile -C meson/build/win32-dev fntech3_stvef_demo_probe
python scripts/probe_stvef_demos.py --installation "E:\Games\Star Trek Voyager Elite Force" --output .tmp/stvef/demo-probe --probe meson/build/win32-dev/fntech3_stvef_demo_probe.exe
```

| `pak2.pk3` demo | Recorded by | Records | Snapshots | Entity deltas | Server commands | Gamestate |
| --- | --- | ---: | ---: | ---: | ---: | --- |
| `demos/demo0000.efdemo` (1,721,835 bytes, CRC `aae6328a`) | v1.20 Apr 17 2001 | 22,087 | 22,086 | 79,100 | 878 | 29 configstrings, 12 baselines |
| `demos/demo0001.efdemo` (620,372 bytes, CRC `e5f7eef4`) | v1.20 Apr 17 2001 | 8,713 | 8,712 | 22,004 | 90 | 27 configstrings, 70 baselines |
| `demos/extreme.efdemo` (373,626 bytes, CRC `ea87588d`) | v1.20 Apr 13 2001 | 1,875 | 1,874 | 17,595 | 52 | 24 configstrings, 55 baselines |

All 32,675 records decode with no error, and every message ends with fewer
than eight unread bits. The recorded gamestate ends at its zero terminator
with no checksum feed; snapshots begin with the reliable acknowledgement; no
message carries an end-of-message opcode.

Re-encoding reproduces retail's bits except where retail flagged a field whose
change cannot survive decoding: a value outside the field's transmitted width,
or -0.0 against 0.0, differs in the sender's state but decodes identically.
The probe verifies that every such delta flags a superset of the fields that
really differ. The April 13 prerelease never used the dictionary, so its
deltas carry raw vectors that the release encoder, and fnTech3's, index:

| Demo | Player deltas | Entity deltas | Baselines |
| --- | --- | --- | --- |
| `demo0000` | 22 flag decode-invisible changes | 41 flag decode-invisible changes | all identical |
| `demo0001` | 6 flag decode-invisible changes | 7 flag decode-invisible changes | all identical |
| `extreme` | 1 flags a decode-invisible change | 6,930 raw-vector prerelease; 2 decode-invisible | 55 raw-vector prerelease |

No delta differs for any other reason. Both April 17 recordings use every
dictionary entry: `demo0000` never uses entry 23 and `demo0001` does 50 times.

## Tests

- `fnql_message_codec` pins hand-packed Holomatch bit patterns: the alignment
  skip and unfiltered strings, a dictionary delta, a raw-vector delta, removal
  and forced no-change encodings, a player delta carrying the Elite Force
  fields, unkeyed user commands with one-byte buttons, and the closed
  single-player wire.
- `fnql_netchan_codec` pins two scrambled packets and round-trips every
  datagram length from 0 to 1,400 bytes; longer packets are refused unchanged.
- `fntech3_stvef_abi_layouts` pins every frozen record's size and offsets and
  spot-checks the module numbering; `fnql_protocol_layout` pins the canonical
  state with its Elite Force extensions.

## Remaining

Server and client framing and the handshake; the Holomatch game, cgame and UI
import tables and server game data; shield bounding boxes; the retail bot
action flags and botlib differences; downloads; pure admission; `.efdemo`
playback; the renderer; and interoperability with retail clients and servers.
