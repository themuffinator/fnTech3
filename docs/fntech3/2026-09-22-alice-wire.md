# Alice's netchan framing and protocol contract

## Scope

This record establishes how American McGee's Alice frames a packet, what it
says out of band, and how large its messages may be, and registers all of it
as a wire profile and a protocol contract. Everything here is read out of
retail `alice.exe` -- its netchan, its bit writer, its connectionless
dispatchers and its command tables -- rather than inherited from Quake III
because the engine descends from it. Nothing here encodes or decodes a game
message: the entity, player-state and usercmd layouts are a separate gate and
this slice deliberately closes the door on them. See the
[foundation record](2026-09-22-alice-contracts.md) for the profile, the
[module record](2026-09-22-alice-module-abi.md) for the library boundary, and
the [compatibility ledger](COMPATIBILITY.md#alice-gate-matrix) for what remains
open.

## Why it needed measuring at all

Alice announces protocol 1, and until now no contract answered that number, so
the client refused its own server's `connectResponse`:

```
Going from CS_FREE to CS_CONNECTED for Player
CL packet loopback: connectResponse 0
ConnectResponse selected protocol 1, which is not supported by the active
American McGee's Alice profile. Ignored.
```

The number was registered; the framing behind it was not. A contract is not a
label -- it decides whether a qport is written, whether the payload is
scrambled, whether bits go through a Huffman stage, and how large a message
may be. Getting any of those from family resemblance would have produced a
channel that looks connected and drops every packet.

## Netchan

Alice's `netchan_t` is Quake III's, field for field, which the transmitter's
own accesses pin:

| Offset | Field | Evidence |
| --- | --- | --- |
| `+0x00` | `sock` | indexes a two-entry `{"client","server"}` table at `0x005185f4` |
| `+0x04` | `dropped` | not read by the transmitter |
| `+0x08` | `remoteAddress` | 20 bytes copied by `rep movsd` of five dwords into the send call |
| `+0x1c` | `qport` | unread; the transmitter uses the `net_qport` cvar |
| `+0x20` | `incomingSequence` | printed as `ack=` |
| `+0x24` | `outgoingSequence` | written as the first long, then incremented |

`Netchan_Transmit(chan, length, data)` lives at `0x00447080` and
`Netchan_Process(chan, msg)` at `0x004473c0`. Both carry Quake III's
`showpackets` strings verbatim, including the fragmented forms.

Constants measured from the transmitter:

| Constant | Value | Evidence |
| --- | --- | --- |
| `MAX_PACKETLEN` | 1400 | `MSG_Init(&send, buf, 0x578)` at `0x004470a3`, and the scramble errors with the literal `MAX_PACKETLEN` past `0x578` |
| `FRAGMENT_SIZE` | 1300 | `cmp esi, 0x514` at `0x00447090` chooses the fragmenting path |
| fragment bit | `0x80000000` | `or edx, 0x80000000` at `0x004470c1` |
| fragment offset | a long, 32 bits | written by the 32-bit writer `0x00445740` at `0x0044710e`, and read back by the 32-bit reader `0x00445930` at `0x00447423`; Quake III's is a short |
| fragment length | a short, 16 bits | the 16-bit writer `0x00445720` at `0x00447119`, and the reader `0x00445900` |

The long offset is what lets a message pass 32 KB. fnTech3 wrote and read a
short there until 2026-09-24, so every offset past 32,767 arrived negative and
the fragment was dropped. A F.A.K.K.2 client could not enter `oracle`, whose
gamestate is 40,244 bytes, and a retail peer would have misread every fragment
(`NETCHAN_FEATURE_LONG_FRAGMENT_START`, `tests/netchan_profile_tests.cpp`).

The qport is one-sided in both directions. The transmitter writes it only when
`chan->sock` is zero (`test eax, eax` / `jne` at `0x004471fd`), and
`Netchan_Process` reads and discards a short only when `chan->sock` is one
(`cmp dword ptr [ebx], 1` at `0x0044740e`). There is no sequence checksum and
no reliable-command XOR anywhere between them.

## The packet scramble

Every sequenced datagram is obscured from byte six onward, by `0x004472a0` on
the way out and restored by `0x004476f0` on the way in. Both are called
unconditionally: no cvar gates them.

The routine is **the same one Elite Force Holomatch uses**, which this tree
already implements in `code/qcommon/stvef_netchan.hpp`. That is a measured
equivalence, not a family resemblance -- every step matches:

| Step | Alice | fnTech3's existing implementation |
| --- | --- | --- |
| seed | `(LittleLong(first) * 3) ^ (size * 123) ^ 0x87243987` | `(first * 3) ^ (size * 123) ^ 0x87243987` |
| key stream | `x = x * 69069 + 1`, one dword per byte | the same |
| XOR | `data[i] ^= (byte)key[i]` for `i` in `[6, size)` | the same |
| mask | smallest all-ones value covering `size - 6`, halved | the same |
| swap | `swap(data[i], data[6 + (key[i] & mask)])`, `i` ascending | the same |
| bounds | nothing below seven bytes, error above 1400 | the same |

The one textual difference is the `LittleLong` in the seed, an indirect call
through `0x00b9b64c`. `Swap_Init` at `0x0044b520` writes `{1, 0}` as bytes and
reads it back as a short; on the little-endian branch it installs `0x0044b4d0`,
whose whole body is `return arg`. On the x86 target the call is the identity,
so the two seeds are the same value.

## Raw messages

Alice has no Huffman stage. `MSG_WriteBits` at `0x00445520` writes bits LSB
first straight into the buffer, and its `msg_t` is 28 bytes rather than
Quake III 1.27's 32:

| Offset | Field |
| --- | --- |
| `+0x00` | `allowoverflow` |
| `+0x04` | `overflowed` |
| `+0x08` | `data` |
| `+0x0c` | `maxsize` |
| `+0x10` | `cursize` |
| `+0x14` | `readcount` |
| `+0x18` | `bit` |

The writer and the readers name six of those seven: `MSG_WriteBits` takes
`maxsize` and `cursize` to decide it has room, sets `overflowed` when it does
not, and walks `data` by `bit`; `MSG_BeginReading` clears `readcount` and
`bit`. The seventh, at `+0x00`, is unwritten by any of them and is named here
by elimination against Quake III's order.

The missing field is `oob`, which exists in Quake III only to bypass the
Huffman path. A record without it is a record with no Huffman path to bypass.
`MSG_Init` at `0x00445410` clears exactly seven dwords, which fixes the size
independently of the field walk.

## Limits

| Limit | Value | Evidence |
| --- | --- | --- |
| `MAX_MSGLEN` | 49,152 | every game-message `MSG_Init` passes `0xc000`, in a frame of `0xc030` -- the buffer and its locals, with no slack |
| `MAX_RELIABLE_COMMANDS` | 128 | client adder `0x0042a220` errors past 128 outstanding and indexes `and 0x7f`; server adder `0x00455a10` does the same |
| reliable command bytes | 1,024 | both adders `shl eax, 0xa` into the ring and copy `0x400` bytes |
| `PACKET_BACKUP` | 32 | `and eax, 0x1f` at `0x00428bba` (client) and `0x004532aa` (server) |
| `CMD_BACKUP` | 64 | `and eax, 0x3f` at `0x00428c6b` |
| `MAX_PACKET_USERCMDS` | 32 | client clamps to `0x20` with `MAX_PACKET_USERCMDS`; server refuses past `0x20` with the same string |
| usercmd bytes | 20 | the client's ring strides by 20 and the server's read loop by `0x14` |

48 KB is three times Quake III's message limit and larger than any other
profile fnTech3 carries, so `MAX_GAME_MSGLEN` moves from 32,768 to 49,152.
That constant is the engine's own storage maximum across profiles, and
`Netchan_Process` refuses a channel whose message limit does not fit it --
silently, by dropping the packet -- so `code/qcommon/net_chan.cpp` now asserts
the fit at compile time where adding a profile is what could break it.

## Out-of-band vocabulary

The server's connectionless dispatcher at `0x00456250` answers seven commands
and the client's at `0x0042bc90` answers eight: `challengeResponse`,
`connectResponse`, `infoResponse`, `print`, `echo`, `keyAuthorize`, `motd` and
`getserversResponse`. Six of Quake III's names have no counterpart in this
engine at all, and the contract leaves them empty rather than carrying a name
nothing would answer:

| Quake III name | Alice | Evidence |
| --- | --- | --- |
| `getchallenge`, `connect`, `getinfo`, `getstatus`, `rcon`, `disconnect` | present | the server's dispatcher, which also answers `ipAuthorize` |
| `challengeResponse`, `connectResponse`, `infoResponse`, `statusResponse`, `print`, `echo` | present | the client's dispatcher and the server's senders |
| `userinfo` | present | the server's client-command table |
| `download`, `nextdl`, `stopdl`, `donedl` | **absent** | see below |
| `cp`, `vdr` | **absent** | see below |

`connectResponse` carries no payload, as Quake III's does not: `0x0045293f`
sends the bare string.

**The connect is plain text.** Alice has no Huffman stage anywhere, and
Heavy Metal: F.A.K.K.2 1.02 shows the handshake in full
(`code/qcommon/ritual_handshake.hpp`; the F.A.K.K.2 record has the
addresses). The client asks with a bare `getchallenge` and reads only the
challenge from the reply. It then prints `connect "<userinfo>"` with
`protocol 1`, and the server reads it as a line and answers the bare
`connectResponse`. fnTech3 had only ever run Ritual sessions locally. Over a
network its server Huffman-decoded the plain `connect`, which failed and
dropped it unlogged. Its client compressed the `connect` and asked for
protocol 68. Both now follow retail under both Ritual profiles, and a retail
F.A.K.K.2 client joins an fnTech3 server and plays. Alice's own
`connectResponse` sender (`0x0045293f`) agrees with the bare reply; its
client's side has not been read.

**There is no download path.** Alice has the `svc_download` opcode, but its
client's server-message jump table at `0x0042e278` routes opcode six to the
same address as an unknown opcode -- the arm that reports an illegible server
message. Nothing can arrive over it, so the contract's download window and
block size are zero rather than a size copied from a neighbouring engine.

**There is no pure exchange.** The server's client-command table at
`0x0051a398` holds exactly two entries, `userinfo` and `disconnect`, followed
by a null pair; everything else a client says is forwarded to the game module.
That agrees with the profile already recording that no pure proof is available
for this title.

The systeminfo predates Quake III 1.27's pak-name triplet: `sv_paks` and
`sv_serverid` are set, and `sv_pakNames`, `sv_referencedPaks` and
`sv_referencedPakNames` appear nowhere in the image.

## Server messages

Measured, and recorded here because it is what the next gate has to serve --
not because anything reads or writes it yet. The opcode names come from the
engine's own `svc_strings` table at `0x0051012c`:

| Opcode | Name | Client's arm |
| --- | --- | --- |
| 0 | `svc_bad` | illegible |
| 1 | `svc_nop` | nothing |
| 2 | `svc_gamestate` | `0x0042df70` |
| 3 | `svc_configstring` | illegible outside a gamestate |
| 4 | `svc_baseline` | illegible outside a gamestate |
| 5 | `svc_serverCommand` | `0x0042e120` |
| 6 | `svc_download` | illegible |
| 7 | `svc_snapshot` | `0x0042dc30` |
| 8 | `svc_centerprint` | `0x0042dbd0` |
| 9 | `svc_locprint` | `0x0042dbf0` |

Two of those are Ritual's own additions, and there is **no `svc_EOF`**: the
parse loop ends when `MSG_ReadByte` returns -1 past the end of the message and
reports `END OF MESSAGE`. The client writes `clc_move` as 2, `clc_moveNoDelta`
as 3 and `clc_clientCommand` as 4, which are Quake III's values.

## The delta field tables

The engine keeps its delta contracts as arrays of
`{ const char *name, int offset, int bits }`, and Alice's two survive in the
binary with their names intact. Walking them until the triples stop being
triples gives **79 entity fields** at `0x00517640` and **49 player fields** at
`0x005179f8`, frozen in `code/compat/alice_wire_fields.def` by a generator
rather than transcribed. The bit count follows the convention these tables
share: zero is a float, a positive count is that many unsigned bits, a
negative count that many signed. That is not assumed from Quake III either:
Alice's own `MSG_WriteBits` negates a negative count at `0x0045559d` after
range-checking the value as signed, and the delta writer takes the float path
where the count is zero.

Both tables are internally consistent in a way that corroborates the reading:
every offset is four-aligned, no offset appears twice, and in each table the
last field's offset plus four is the record's bound -- 456 bytes for the
entity state, 1,044 for the player state. Neither is Quake III's, whose
tables are 51 and 48 fields long.

Forty-nine of the 79 entity fields have no Quake III counterpart by name;
thirty are shared, and twenty-one of Quake III's fifty-one are missing here.
What Alice added says what this engine is for. Five bone modifiers of five
fields each (`bonemod[0..4].tag`, `.scale`, `.angles[0..2]`) drive the
skeletal models the [model record](2026-09-22-alice-models.md) reads. An
attachment block (`parent`, `tag_num`, `attach_use_angles`,
`attach_offset[0..2]`) binds an entity to a tag on another, which is the
parenting the client's own snapshot
code complains about (`CL_GetSnapshot: ... could not find parent`). A separate
`torso_anim`, `torso_frame` and `torso_crossblend_time` splits the upper body
from the legs, and `crossblend_time` on each blends between animations rather
than snapping. `alpha`, `scale` and a second `renderfx2` word are per-entity
render state. Going the other way, Quake III fields this record simply does
not have include `weapon`, `legsAnim`, `powerups`, `event`, `eventParm`,
`otherEntityNum` and `generic1`.

The player state diverges further: twenty-seven of its forty-nine fields have
no Quake III counterpart and twenty-six of Quake III's forty-eight are absent,
so under half of either table is shared. Alongside the movement fields it
carries the music, reverb and camera state a Quake III client game would own
-- `current_music_mood`, `fallback_music_mood`, `music_volume`, `reverb_type`,
`reverb_level`, `camera_origin`, `camera_angles`, `camera_offset`,
`camera_time`, `camera_flags`, `damage_angles`, a four-component `blend` and
an `fov` -- and it has no `weapon`, `weaponstate`, `ammo`, `stats`,
`persistant` or `powerups` at all. Its 1,044 wire bytes sit inside the
2,112-byte client record the module publishes at run time, which is the first
independent check either number has had.

### The server's own code agrees with the table

The tables were read out of one part of the binary; the server's entity code
is another, and it reaches into the entity record at offsets the table
independently predicts:

| Access | Table says | Agreement |
| --- | --- | --- |
| `SV_SvEntityForGentity` bounds `[ent + 0]` by 1024 | offset 0 precedes `eType` at 4 | the record opens with its entity number, as Quake III's does, and `MAX_GENTITIES` is 1,024 |
| `cmp [ent + 0xa0], 0x3ff` | `parent` at 160 | compared against 1,023, the reserved "no entity" number, so it holds one |
| `cmp [ent + 0x9c], 0x46ea6000` | `loopSoundMinDist` at 156, bits 0 | that immediate is `30000.0f`, so the field really is a float |
| `[ent + 0x94]` non-zero gates sound | `loopSound` at 148 | |
| `[ent + 0xb8]` non-zero gates visibility | `modelindex` at 184 | |
| `test byte ptr [ent + 0x1c0], 0x80` | `renderfx` at 448 | bit tests on a 32-bit render word |
| `cmp [ent + 0x1c8], 0x3ff` | the table's last field ends at 456 | **456 is the first word past the table**, compared against 1,023. It is Ritual's bind parent, in the entity state's unsent tail: `fgamex86.dll`'s `Entity::bind` stores the master's number there (`0x1009c35e`) and its unbind `0x3ff` (`0x1009c502`); see [the snapshot](#the-snapshot) |

The last row is the one that matters. The field table's length says the entity
state is 456 bytes; the server's own access to the engine-owned half of the
record starts at exactly 456. Neither number was derived from the other.

These are declarations, not a codec. `tests/alice_module_abi_tests.cpp`
asserts the shape -- every row named, every offset aligned, unique and inside
its record, every bit width one the convention allows, and the anchors above
-- and nothing reads or writes a field.

## The snapshot

Which entities a client's snapshot carries is decided by Ritual's walk, not
Quake III's, and the walk writes on the entities it passes. All of it is read
from retail `alice.exe`.

`SV_BuildClientSnapshot` (`0x00456f20`) bumps the snapshot counter first
(`0x00456f2b`), clears the frame's area bits, and returns for a client with
no entity or a zombie. It copies the player state, and sees from
`camera_origin` (`+0x3dc`) when the player state's `pm_flags` has `0x100`,
otherwise from the origin raised by `viewheight` (`0x00456fbb..0x00457002`).
It adds the client's own entity -- found by the client's slot, not by the
player state -- before any test (`0x00457024..0x00457037`), then walks from
the view (`0x00457049`), sorts the numbers with a comparison that drops on a
duplicate (`SV_QsortEntityStates: duplicated entity`, `0x00457110`), inverts
the area bits, and only then copies each entity's 512-byte state into the
client's frame (`0x0045709d..0x004570f9`).

`SV_AddEntToSnapshot` (`0x00457150`) takes the server entity, the entity, the
list and two flags, a portal and a sky. An entity already in this snapshot has
`RF_DUALENTITY` (`0x800000`) set and `RF_PORTALENTITY` (`0x400000`) cleared on
its own `renderfx` (`+0x1c0`) and is not appended. Otherwise its counter is set,
nothing more happens when the list holds `0x400` numbers, and else
`RF_PORTALENTITY`, `RF_DUALENTITY` and `RF_SKYENTITY` (`0x4000`) are cleared and
one set -- `RF_PORTALENTITY` for a portal's view, `RF_SKYENTITY` for the sky's
-- before the number is appended.

`SV_AddEntitiesVisibleFromPoint` (`0x004571e0`) takes a view origin, the
frame, the list, the portal's own server entity (null in the main view) and
the sky flag. For every entity number below the live count:

| Step | Retail |
| --- | --- |
| skipped unless its module-owned word `+0x204` is set | `0x00457255` |
| its number repaired (`FIXING ENT->S.NUMBER!!!`) | `0x00457263` |
| `svFlags` bit 15 set on it, whether linked or not | `0x00457282` |
| skipped unless linked (`+0x208`), or when no-client | `0x0045728f`, `0x00457295` |
| its parent is `s.parent` (`+0xa0`), or when that is `0x3ff` the bind parent (`+0x1c8`) | `0x0045729d..0x004572bc` |
| skipped when the parent is no-client, or its `renderfx` has `RF_DONTDRAW` (`0x80`) | `0x004572ca`, `0x004572d7` |
| an `RF_SKYORIGIN` (`0x2000`) entity: skipped unless the level's sky portal is on and this is the main view; a second one drops (`duplicate sky origin`); otherwise added with neither flag and remembered | `0x004572f9..0x00457347` |
| broadcast (`0x4`) or `svFlags` bit 14 (`0x4000`): added | `0x0045734c`, `0x0045735a` |
| a parent already in the snapshot: the child added | `0x00457363..0x0045737e` |
| otherwise the parent's server entity decides areas and clusters | `0x00457384` |
| no model, no looping sound and no `svFlags` bit 4 (`0x10`): skipped | `0x00457394..0x004573aa` |
| a looping sound whose minimum distance is exactly `30000.0f`: added | `0x004573b0..0x004573c4` |
| neither area of the deciding entity connected to the view's: skipped | `0x004573ca..0x004573f8` |
| with a far plane set: the squared distance from the view to the centroid (`+0x250`) less the squared radius (`+0x24c`), of the parent when there is one, beyond the plane: skipped | `0x004573fe..0x00457486` |
| the deciding entity's clusters against the view's PVS; the overflow scan runs to the entity's own `lastCluster` and fails at the deciding one's, as Quake III's does | `0x0045748c..0x00457521` |
| already in the snapshot: `RF_DUALENTITY` set, `RF_PORTALENTITY` cleared | `0x00457527..0x0045754c` |
| added with the call's flags; a portal (`0x8`) that is not the view's own portal opens a walk from its `s.origin2` (`+0x6c`) with itself as the portal | `0x0045755f..0x0045759e` |

When the main view is done and it met a sky origin, the walk runs again from
that entity's `s.origin` (`+0x60`) with the sky flag set (`0x004575cf..0x00457605`).
The far plane is what the `SetFarPlane` import (`0x004537d0`) stored: zero,
or the distance plus 32, squared, in 32 bits; the sky portal switch is what
`SetSkyPortal` (`0x00453800`) stored. Both live in the server state the full
spawn clears (`0x00455190`).

What the client game does with a child whose parent it did not draw: its
model code asks the renderer for the parent's scene entity (`cgamex86.dll`
`0x30025f57`, `0x30022bbb`) and prints `CG_ModelAnim` or `CG_GetOrigin: Could
not find parent entity` when there is none; `CL_GetSnapshot` (`0x00423c40`)
drops a child whose parent is not in the snapshot. On `garden1` fnTech3's
Quake III walk sent three weapons (entities 470, 471, 479) whose ant-soldier
parents (111, 308) were no-client, and two children of parents drawn with
`RF_DONTDRAW` (113, 302); retail's parent rule sends none of them.

### fnTech3

`code/server/server_alice_snapshot.hpp` is the walk and `SV_AddEntToSnapshot`
above, as a template over the world it reads, on the module's 732-byte records
through byte offsets. `sv_snapshot.cpp`'s `SV_BuildClientSnapshot` hands an
Alice client to `SV_BuildAliceClientSnapshot` right after the entity check,
before the common frame the other profiles share is built; their path is
unchanged. The Alice path follows retail's order, then stores a frame of its
own holding exactly the chosen states, read from the game after the walk so
they carry the flags it wrote. `sv.aliceFarPlane` and `sv.aliceSkyPortal` are
the two server-state words.

Where it is not retail's:

- **One frame per client.** Retail copies each client's states after that
  client's walk; the other profiles here share one frame per server frame.
  Alice's clients each get a frame from the same ring, which is what retail's
  copy amounts to. The flags stay on the game's entities as retail leaves them,
  so the game sees the last walk's. The profile runs one client and a loaded
  game refuses more; a second is served this way rather than refused.
- **Bounds retail does not check.** A parent number past the module's arena
  drops the server instead of reading outside it, and a null PVS row sees
  nothing.
- **The entity limit.** Retail's `0x400` replaces the 256 fnTech3 used for
  Alice. The client game still receives at most 256 (`CL_GetSnapshot`
  truncates, as retail's does at `0x00423d42`), and fnTech3's parse ring is
  8,192 entries where retail's is 2,048.

### The sound block on every path, and no download

Retail's `SV_WriteSnapshotToClient` (`0x00456ba0`) has no shortcut for a
zombie: every snapshot it writes ends with the sound block (`0x00456cff`,
through `0x00446d00`) and a pending print, and Alice's client reads the block
after the entities of every snapshot. fnTech3's zombie branch now ends with
the same tail. The engine-only snapshot that opens a download message ends with
an empty block, a single zero bit, for Alice; without it the client read the
first bit of the following `svc_download` byte as the block's presence bit.

That message has no retail counterpart at all: retail's engine knows two
client commands, `userinfo` and `disconnect` (its table at `0x0051a398`), and
has no `download`, `nextdl`, `stopdl` or `donedl`. Under Alice those four now
reach the game module, as any other name does in retail, and the client
starts no download (`CL_InitLegacyDownloads` completes at once).

### The time residual

After the server time, a snapshot carries one more byte:
`min(sv.timeResidual, 255)`, the milliseconds the server has run past its
last frame. `SV_WriteSnapshotToClient` writes it at
`0x00456c63..0x00456c7b`, and `CL_ParseSnapshot` reads it next, before the
delta byte (`0x0042dc6e`), and keeps it with the snapshot. fnTech3 sent and
read neither, which agreed with itself and with no retail reader: retail
would take the delta byte for the residual and the flags for the delta. Both
of fnTech3's snapshot writers now send it, and its client reads and drops it.
Retail's `CL_SetCGameTime` takes only the flags and the time. The omission
surfaced when retail `fakk2.exe` 1.02, whose engine writes the byte at
`0x00447b13` and reads it at `0x0042385e`, overran the first snapshot of an
fnTech3 demo ([F.A.K.K.2 contracts](2026-09-24-fakk2-contracts.md)). No
retail Alice client has read the corrected snapshot: `alice.exe` is
SafeDisc-wrapped, and its `secdrv.sys` does not load on this machine's
Windows 11.

### Evidence

Client, `+devmap garden1`, windowed, isolated home, no input, `skipcinematic`
after 9 s, 1,000 frames, same build (`meson/build/win32-alice-snap`; the final
one's `fntech3.exe` SHA-256 begins `078a90c52df70169`, `fntech3.ded.exe`
`b6a84b8022cc0711`, `fnql_glx_x86.dll` `de436faa96d7d69e`):

| | `Could not find parent entity` | `Not all children could find their parents` |
| --- | --- | --- |
| before (the Quake III walk with the client's entity first) | 3,004 | 230 |
| before, rerun through a temporary switch in the same binary | 2,998 | 231 |
| after | 0 | 0 |
| after, final build (`.tmp/alice-snap/final-garden1`) | 0 | 0 |

Screenshots: `.tmp/alice-snap/before-garden1/alice-probe.png`,
`.tmp/alice-snap/after-garden1/alice-probe.png` and, from the final build,
`.tmp/alice-snap/final-garden1/alice-probe.png`: the same view, Alice drawn in
each. The HUD differs between them because another change to it was being
made at the same time; that difference is not this one's.

A later pair of builds, 2026-09-24: the same `+devmap garden1` probe
(windowed 800x600, isolated home, no input, `skipcinematic` 9 s and again
13.5 s after the client became active, screenshots at 12 s and 18.5 s) ran on
two `meson/build/win32-alice-client` builds. At 07:37, before the walk
(`.tmp/alice-lead/models2`, `fntech3.exe` SHA-256 beginning
`22dbd02a0a9e3195`), its console log holds 3,604 `Could not find parent
entity` lines and 315 `Not all children could find their parents`. At 08:35,
after it (`.tmp/alice-unrel/parents1`, beginning `ddd57893cc42dc97`), it holds
none of either, and its screenshot shows Alice in third person with the HUD's
gauges. Other changes landed between those two builds as well, so the pair
corroborates the same-binary comparison above rather than isolating the walk
on its own. Both runs exited with code 0, and their manifests record the
installation unchanged.

Sky portals, counted from each snapshot by a temporary server print on the
same probes: `hatter1` sends nine sky entities (`sky_hatter`, `sky_gear01`,
`sky_watch`, `sky_metalfleck01`, `sky_nut`), `skool2` two (`skool_sky`,
`bookstacks`), `qlair` one (`sky_facade`, with a far plane of 100,641,024,
which is 10,032 squared), each flagged `RF_SKYENTITY` and nothing else so
flagged; `hedge2`, `centipede2` and `keep` send their sky origin and no sky
entity from the start. `fortress1`'s portal camera sends four
`RF_PORTALENTITY` entities. The GL renderer draws an `RF_SKYENTITY` entity
only in the sky portal's pass (`R_AliceEntityInView`). No start position shows
the sky entities themselves on screen; a raised third-person camera on
`hatter1` shows the sky (`.tmp/alice-snap/sky-hatter1-retail/home/base/screenshots/sky.png`)
without them in frame, so their drawing in the sky pass is not established by
a picture.

Tests: `tests/alice_snapshot_tests.cpp` runs the walk over a fake world --
the flags on a first and second add and on a full list, the gates, the
relevance rules, areas and the overflow scan, the parent rules, the far plane,
nested portals, the sky portal and its duplicate, the limit, the view origin
and the duplicate sort. `tests/alice_snapshot_source_tests.py` pins the
branch, the retail order, the server-state words, the sound block on the
zombie and download paths, the download commands' absence, and that no other
profile's snapshot function mentions Alice; `tests/wolfet_viewpoint_tests.py` still
drives Quake III, Quake Live, both RTCW modes and ET through
`SV_BuildClientSnapshot`.

## The message bodies

The tables above say what a record holds. What a message does with them was
read out of the same four routines, and it is Ritual's, not id's:

```
MSG_WriteDeltaEntity      0x00445e10   MSG_ReadDeltaEntity      0x004460c0
MSG_WriteDeltaPlayerstate 0x004464e0   MSG_ReadDeltaPlayerstate 0x004468e0
MSG_WriteDeltaUsercmd     0x00445bb0   MSG_ReadDeltaUsercmd     0x00445ca0
```

**An entity delta** opens with its number in ten bits and one bit saying
whether the entity was removed. If it was not, a second bit says whether a
delta follows. The delta is a five-bit symbol: thirty-one means the
seventy-nine bit changed-field vector follows raw, as nine bytes and seven
bits, and anything less is an index into a **thirty-one entry dictionary of
changed-field vectors** the engine carries at `0x00513640`, each with a
four-byte use counter ahead of it that ships at zero. The changed values
follow with no per-field flag. Then a thirty-two byte block at offset 396
goes out under a mask of its own: one bit saying whether any of it changed, a
thirty-two bit mask, and one byte for each bit set.

The quiet case needs both to be empty. An entity whose only change is in that
block still sends a delta, with an all-zero field vector -- which the
dictionary does not contain, so it escapes to the raw form. When both are
empty, an unforced entity writes nothing at all, and a forced one only its
number, not-removed and no-delta (`0x00445ee1..0x00445f2d`; the force flag is
the fourth argument, read at `0x00445ef5`). Until 2026-09-24 fnTech3's
encoder had those two inverted -- twelve bits for every unchanged entity in
every snapshot, and ninety-seven for a forced one -- which still decoded, but
was not retail's wire.

**A player delta** has no vector at all: one bit a field, and the value after
it when the bit is set, for all forty-nine. Then six thirty-two entry arrays
between offsets 176 and 943, each under its own one-bit flag and thirty-two
bit mask, with sixteen bits to a value, read back sign-extended
(`MSG_ReadShort` `0x00445900`, `movsx`), so an empty slot's -1 arrives as -1.

**A changed float** is sent as a biased thirteen-bit integer when it is
integral and within 4,096, else as thirty-two raw bits -- one selector bit
either way, with none of Quake III's separate zero shortcut. A changed
integer is sent at its field's width, sign-extended when that width is
negative.

**A user command** is not keyed. Its time goes as an eight-bit step from the
previous command when the step fits, else in full; then the three view
angles, the three movement bytes, the buttons and the byte after them, each
flagged one bit at a time.

Elite Force Holomatch frames its own bodies with the same five-bit dictionary
and the same float rule, which is a family resemblance and not a source: the
dictionary's thirty-one vectors, both field tables, the masked blocks and
every width here are Alice's own and were read out of Alice's own binary.

`code/compat/alice_message_codec.hpp` implements all of it, over the external
records rather than over engine state, and `tests/alice_message_codec_tests.cpp`
holds it to the measured shape: a removal costs its number and one bit, an
unchanged entity nothing unless it is forced and then its number and two, a
dictionary hit five bits where the raw
vector costs seventy-nine, a block-only change is still a delta, an integral
float costs fourteen bits and a NaN thirty-three, and every record survives a
round trip byte for byte. A truncated message is refused rather than
published half-decoded.

### Where the members go

Alice's entity networks seventy-nine members and its player forty-nine. This
engine's own records have a member for thirty-six and twenty-two of them; the
rest are carried in the extensions in `code/qcommon/alice_state.h`, so a
snapshot keeps everything the module put in it. Which member goes where is
not written by hand: `code/compat/alice_state_carry.def` is generated from the
delta tables together with the mapping, and its generator refuses to emit it
unless the mapped and carried sets cover a table exactly. A member cannot be
dropped by forgetting to write a line.

The client game is handed more than the networked members. Retail's reader
(`MSG_ReadDeltaEntity` `0x004460c0`) ends its delta branch by deriving four
more (`0x004463e5..0x0044644a`): `origin` (+96) from `netorigin`, the angles
(+132) from `netangles`, the entity's rotation as a quaternion (+460) from
those angles through `EulerToQuat` (`0x0044a810`: the identity for three zero
words, else `AnglesToAxis` `0x0044aa40` and `MatToQuat` `0x0044a3a0`), and each
tagged bone modifier's quaternion from its own angles. The retail client game
interpolates that origin and slerps those quaternions (`cgamex86`
`0x30022655`, `0x300226cd`, `0x30022739`). Until 2026-09-24 fnTech3 handed
them over as zeros, so the client game read a zero origin and rotation for
every entity whose trajectory is `TR_LERP`. `fntech3::alice::state::ForClientGame` now builds
each record the client game reads (slots 75 and 78) that way, from zero, since
the delta branch writes nothing else into its ring slot. Slot 78 also follows
retail's bounds (`0x00423bc0`): a number not yet parsed drops the session and
one older than retail's ring of 2,048 is refused.

Two correspondences that would be easy to make are not made. Alice's `anim`
is a whole-body animation with `torso_anim` overriding the upper half, where
this engine's pair is legs and torso, so calling `anim` the legs animation
would be an interpretation; and `skinNum` is not `modelindex2`. Both are
carried under their own names, which is exact.

## What is registered

`NETCHAN_WIRE_ALICE` in `code/qcommon/netchan_profile.h` and
`fnql::protocol::Alice` in `code/qcommon/protocol_contract.hpp`, with the
capabilities, commands, info keys and limits above.
`tests/netchan_profile_tests.cpp` and `tests/protocol_contract_tests.cpp`
assert every measured number.

`GameMessageLayout::Alice` is served: all six codec entry points in
`code/qcommon/msg.cpp` dispatch to the codec above, through the same
external-record conversion the RTCW and Elite Force paths use. The check that
refuses an unserved layout stays, because a layout added without a codec would
otherwise fall through to Quake III's field tables and decode a snapshot into
plausible-looking nonsense rather than fail where it can be seen.

One thing had to change on the client to match. `CL_LegacyFraming` -- which
decides whether a message is raw bits with no leading acknowledgement and no
end-of-message opcode -- named the Holomatch wire profile rather than the
feature that makes it true, where the server's `SV_LegacyFraming` had always
asked the feature. Alice has the feature too, so the two halves of a loopback
session were writing and reading different shapes: the server sent raw bits
with no leading long and the client read Huffman with one, which came out as
a reliable-command sequence of 791,621,384. Both sides now ask the feature.

## Demos

`alice.exe` registers `record` (`CL_Record_f`, `0x0042a410`), `demo`
(`CL_PlayDemo_f`, `0x0042a890`) and `stoprecord` (`CL_StopRecord_f`,
`0x0042a2e0`) at `0x0042cbaa`, `0x0042cbb9` and `0x0042cbd7`. The demo
state is Quake III's, in `clc`: `demoName` (64 bytes) at `0x00c178a4`,
`demorecording` `0x00c178e4`, `demoplaying` `0x00c178e8`, `demowaiting`
`0x00c178ec`, `firstDemoFrameSkipped` `0x00c178f0`, `demofile` `0x00c178f4`,
`timeDemoFrames` `0x00c178f8`, `timeDemoStart` `0x00c178fc`.

Observed in the code:

| Rule | Evidence |
| --- | --- |
| The file is Quake III's container: each record a little-endian sequence, a little-endian length and that many message bytes. | `CL_WriteDemoMessage` `0x0042a270`: `LittleLong` of `clc.serverMessageSequence`, then of `cursize - headerBytes`, then the bytes past the netchan header |
| The recorder starts with a gamestate it builds, at sequence `serverMessageSequence - 1`. | `0x0042a683`: `mov eax, [0x00bf7790]; dec eax` |
| That gamestate is raw bits with no acknowledgement ahead of it: `svc_gamestate`, the command sequence, each configstring as `svc_configstring`, a short and a string, each used baseline as `svc_baseline` and a forced delta from the null entity, then a zero byte, with nothing after it -- no client number and no checksum feed. | `0x0042a59a..0x0042a67e` |
| Baselines are deltas from a null entity that is not zero: clear 512 bytes, then `constantLight` `0x00ffffff`, `parent` 1,023, `scale` and `alpha` 1.0, `renderfx` `0x10`, and each bone modifier's tag -1. The server's gamestate, the client's parse and the recorder all call it. | `0x00446460`; callers `0x00452b6f`, `0x0042e0ab`, `0x0042a60b` |
| Before encoding a baseline the recorder copies `origin` to `netorigin` and `angles` to `netangles`, the inverse of the copy the entity reader makes after decoding, so a baseline goes back out as it came in. | `0x0042a622..0x0042a649`; reader `0x004463e5..0x00446419` |
| Once started, a message is copied only after the first uncompressed snapshot (`demowaiting`), which the client asks for by sending `clc_moveNoDelta` while it waits. | set `0x0042a590`; cleared `0x0042dcc1`; `CL_WritePacket` `0x00428c1a`; copied in `CL_PacketEvent` `0x0042c0f2` |
| `stoprecord` writes -1 twice and closes; with nothing recording it says so and stops. | `0x0042a306`, `0x0042a321` |
| The name is `demos/%s.dm3`. A typed name is copied into 64 bytes and used as typed -- `record x.dm3` writes `demos/x.dm3.dm3`. With no name, the first of `demo0000` to `demo9999` whose file reads back as no longer than nothing; with all taken, `demo9999`. | format `0x0050f314`; copy `0x0042a4be`; `CL_DemoFilename` `0x0042a360` (`demo%i%i%i%i`, and `demo9999.tga` out of range); search `0x0042a4e1..0x0042a525` |
| `record` needs a level (`cls.state` 8) and refuses unless `g_syncronousClients` -- retail's spelling, which its client game registers with `"0"` (`cgamex86.dll` `0x30023a7c`) -- is non-zero. Quake III 1.32 only warns. | `0x0042a458`, `0x0042a476` |
| `demo` opens `demos/%s` when the argument contains `.dm3` anywhere, else `demos/%s.dm3`; the test is `strstr`, so case sensitive. It kills the local server, disconnects, reads messages until the gamestate is parsed, and skips the first snapshot's frame. | `0x0042a8d7`; `sv_killserver` `0x0042a8ba`; loop `0x0042a9a2` |
| Playback reads a message whenever the client's time passes the snapshot's; `timedemo` advances 50 ms a frame and prints frames, seconds and frames per second at the end, which then disconnects and runs `nextdemo`. A length of -1, a short read or a truncated body ends the demo; a length past 48 KB is an error. | `CL_SetCGameTime` `0x00424b10`; `CL_DemoCompleted` `0x0042a6f0`; `CL_NextDemo` `0x0042a9e0`; `CL_ReadDemoMessage` `0x0042a760` |
| Nothing is copied to a recording during playback, and no recording notice is drawn: the executable has no `RECORDING` string. | `CL_ReadDemoMessage` `0x0042a760` |

fnTech3 follows it. `record`, `stoprecord` and `demo` use the rules above
under the Alice profile, held in `code/client/client_alice_demo.hpp`; the
gamestate is the one the engine already wrote for Elite Force, which is the
same shape. `MSG_NullEntityStateForWireProfile` builds the null entity from
`fntech3::alice::message::InitNullEntity`, and the server's gamestate, the
client's parse and the recorder take it from there, so all three changed
together and every other profile keeps its zero null. A playback demo's wire
profile is Alice's, so Alice's snapshot sound block and its centre and located
prints replay through the same parse as a live message.

The live gamestate ends the same way. Retail's server writes the closing zero
byte and sends the message (`0x00452bbd`: `push 0`, `MSG_WriteByte`, then the
send at `0x00452bcf`), and its client's parse reads nothing after the loop
(`0x0042e0e2`). fnTech3's server wrote Elite Force's trailing checksum feed
there, which a retail client would have met as more message data; it now
writes nothing after the zero byte for an Alice client, and fnTech3's client
reads nothing either, keeping the checksum feed its filesystem already has
rather than restarting for one that never came. The pure-server gamestate
estimate (`SV_RemainingGameState`) measures Alice's own encoding from its own
null entity, where it had used Quake Live's tables. Observed on 2026-09-24,
`devmap garden1`, windowed 800x600, isolated home under
`.tmp/alice-lead/feed2/`: the gamestate is acknowledged, the filesystem starts
once, and the engine's `screenshotPNG` shows the level's opening letterboxed
scene with its subtitle.

Divergences, each deliberate: the file is written as `<name>.tmp` and renamed
on `stoprecord`, as the engine does for every profile, so an interrupted
recording leaves a `.tmp`; the console lines keep the engine's wording
("Stopped demo recording.", "demo <demoname>", and two decimals for a timedemo
shorter than ten seconds); and a demo recorded during playback keeps only its
gamestate, where the engine would otherwise re-encode snapshots it has no
Alice form for.

Measured on 2026-09-23, `-Dalice-development=true` Windows x86 build, windowed
800x600, isolated home under `.tmp/alice-demo/`: `devmap garden1`,
`set g_syncronousClients 1`, `record test1`, ten seconds, `stoprecord` wrote
`demos/test1.dm3`, 102,653 bytes in 158 records -- a 23,538-byte gamestate
at sequence 173 with 133 configstrings, 156 snapshots from sequence 175
(174 was skipped waiting for an uncompressed one), and the -1 trailer. A second
process's `demo test1` loaded garden1 and the retail client game from the file
alone and its engine screenshot shows the level and the HUD; `timedemo 1`
played it in 198 frames and ran `nextdemo`. Recording and playing in one
process (`record` with no name, then `demo demo0000`) killed the local server
and replayed `demo0000.dm3`. The same recording with a zero null gave a
31,511-byte gamestate. `tests/alice_demo_tests.py` holds the name rules, the
null entity and a gamestate round trip through the engine's envelope reader,
and pins every constant above to `alice.exe` when it is present.

Not established: no demo has crossed between fnTech3 and `alice.exe` in
either direction, and no retail-recorded demo is available to play.

## What this slice does not claim

No retail client is invited to join and none
could: Alice ships no multiplayer. No packet has been exchanged with any peer;
the only channel exercised is the loopback one a single-player session uses.
Demos are covered [above](#demos); the client's snapshot code binds entities
to parents, which Quake III's does not.

## Where it reaches

With the contract registered, a client build carries a loopback connection
past the handshake: the server accepts the client, `connectResponse` is no
longer refused, the gamestate is sent and the client moves `CS_CONNECTED` to
`CS_PRIMED`. It stops at the next thing it needs, by name:

```
SV_SendClientGameState() for Player
Going from CS_CONNECTED to CS_PRIMED for Player
ERROR: SV_WriteBaselineMessage: the American McGee's Alice profile's message
layout has no delta field tables in this engine, so its entities and player
states cannot be encoded. Its wire framing is registered; its message bodies
are not.
```

With the codec in place that gate is gone. The server builds a gamestate in
Alice's own format, the client parses it, loads the level's camera paths and
scripts, and brings up the retail `base/cgamex86.dll` through the
function-table path its entry point needs:

```
Going from CS_CONNECTED to CS_PRIMED for Player
VM_CreateTable 'cgamex86.dll' ok
Alice client game loaded: table at 30272420
ERROR: Alice's client game called import 77, which fnTech3 does not serve.
```

That is the next surface, and it is a wide one rather than a last step: the
client game has 125 imports and this engine serves twenty.

Four more were served walking that loop, and each was its own measurement.
Slot 13 registers a console command with a null handler, which is how this
lineage says the module will answer it. Slot 21 loads a level's collision.
Slot 74 hands over the game state -- 21,316 bytes, which is 1,328 offsets,
16,000 bytes of string data and a count, and the 1,328 is the configstring
bound the profile already carried, so the three members add up on their own.
Slot 77 hands over the graphics configuration: 5,188 bytes, Quake III's
`glconfig_t` with 1,024-byte strings and a 2,048-byte extension list, anchored
three ways -- the renderer name leads because the engine hands the record's
first byte to `Info_SetValueForKey` under that key, `+5160` and `+5164` are
pushed into `glViewport` as a width and a height, and `+5180` is the flag the
screen update tests to draw a left and a right field instead of one.

What lies past them is a different shape again, and worth naming before
anyone plans around it. The table's slots from 48 up are not wrappers at all:
the engine fills them straight from its renderer's exported function table --
the same pointers it copies into its own dispatch block at `0x0040e4c7` --
so **Alice's client game calls the renderer's functions directly rather than
through an import API of its own**. Quake III's client game asks for a shader
through `trap_R_RegisterShader`; Alice's calls the renderer's registration
function through a pointer the engine handed it. Serving those slots means
presenting this engine's renderer behind that table, one entry at a time, and
that is where the remaining work is. The entity
record's own binding is in
[the module record](2026-09-22-alice-module-abi.md#the-entity-record).

Two refusals were rewritten on the way, because each was reporting the wrong
thing. The shared entity view said "bad entity number" both for an index
outside the array and for a profile it had no layout for, where no index would
have worked; the delta writers said "invalid wire profile" both for a number
nothing had registered and for Alice, whose number is registered and whose
message bodies are not. Each now says which of the two it is, so the next
reader does not go and check a number that was never wrong.
