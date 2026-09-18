# Elite Force Holomatch client: handshake, framing, downloads and demos

## Scope

This slice gives the `stvef-mp` client the retail protocol-24 connection: the
connectionless handshake, the framing of client and server messages, pure
checksums, downloads, `.efdemo` recording and playback, and the one-byte
button field. The cgame and UI import tables, the renderer's Elite Force
entity types and a session with a retail peer are later slices; until the
cgame slice lands, a connection stops when the client loads the cgame module.
The profile stays behind `-Dstvef-development=true`.

## Evidence and provenance

Retail facts come from `stvoyHM.exe` 1.20 (identified in the
[foundation record](2026-09-18-stvef-contracts.md)), read through a local
decompilation and checked against its instructions. [Lilium Voyager](https://github.com/zturtleman/lilium-voyager/tree/0f7dcd89d142c8b635ce2756a80cb62411731848)
was used as a cross-check; where it differs from retail, retail is followed.

| Contract | Retail source | fnTech3 |
| --- | --- | --- |
| Challenge | Client sends a bare `getchallenge` and takes a reply from any address (`0x40ed89`); server `SV_GetChallenge` `0x42ee80` reads no arguments and answers `challengeResponse %i`, after up to 20 s while it waits for the authorize server | The client adds its nonce and Lilium's game name `EliteForce`, which retail ignores. A bare reply is taken only from the address asked; an echoed nonce must match. A server naming protocol 26 (ioEF, Lilium) is accepted, since such servers fall back to 24 |
| Connect | Plain-text `connect "<userinfo>"` with `protocol 24` (`0x40e41f`); the server answers a bare `connectResponse`, which the client matches by keyword (`0x40edfe`) | Plain text, no Huffman stage; a bare reply or one echoing the challenge (and, if named, protocol 24) is accepted |
| Server messages | `CL_ParseServerMessage` `0x4116d0`: raw bits, no leading acknowledgement, the message ends where its data ends; opcodes 1, 2, 5, 6, 7 only | Same; the acknowledgement is read inside `svc_snapshot` (`CL_ParseSnapshot` `0x410f80`) |
| Readers | `MSG_ReadByte` `0x4231c0`, `ReadShort` `0x4231f0`, `ReadLong` `0x423220` test `readcount + n <= cursize` before reading and return -1 without moving | Same for Elite Force messages only; other profiles keep their sticky past-end state |
| Gamestate | `CL_ParseGamestate` `0x411320`: stops at a byte `<= 0`, reads no client number, then reads the checksum feed | Same. A demo's gamestate has no feed, which then reads as -1 as in retail; the client number comes from each snapshot's player state |
| Client messages | `CL_WritePacket` `0x40a660`: raw bits, no `clc_EOF`, unkeyed commands | Same |
| Pure checksums | `CL_SendPureChecksums` `0x40df30`: `cp` with no server id | `cp <cgame> <ui> @ <refs...> <mixed>` |
| Downloads | `CL_ParseDownload` `0x4114e0`: a size of -1 is followed by an empty chunk; the client prints `File not found.` and moves on; an empty chunk has no data | Same; an empty chunk is never read as data, which would skip an alignment byte |
| Demos | `CL_Record_f` `0x40c1e0`: `demos/<name>.efdemo`, refused unless `g_synchronousClients` is set; its gamestate record is `2, seq, configstrings, baselines, 0` with nothing after it | Same name, refusal and layout. Live messages are stored as received, after the sequence; re-recorded snapshots carry the acknowledgement inside `svc_snapshot` and no end marker. Playback accepts `.efdemo` only, and only in `stvef-mp` |
| Buttons | `CL_CmdButtons` (about `0x40a3d5`): seven buttons and 0x80 for any key; `+altattack` and `+use` share the `+button5` and `+button6` handlers (`0x409890`, `0x409930`) | Same; buttons 7 and above never reach an Elite Force command |

## Tests

- `fntech3_stvef_client_wire` checks retail, Lilium and fnTech3 challenge
  and connect replies against the nonce, source address and protocol rules.
- `fnql_message_codec` checks that Elite Force readers return -1 without
  moving when a value is missing, and that other raw-bit profiles do not.
- Python probes run the production code: `cp` without a server id
  (`pure_profile_contract_tests`), the missing-file download
  (`download_chunk_tests`), `.efdemo` naming, refusal and playback selection
  (`demo_record_profile_tests`, `demo_profile_selection_tests`), and the
  button byte and bindings (`rtcw_player_interaction_tests`).

## Remaining

- The cgame and UI modules, and the renderer's Elite Force entity types.
- A session between this client and a retail or Lilium server, and playback
  of the retail demos through the client.
- CD-key authorization is not attempted: the Raven authorize service is gone,
  and retail servers admit clients after their 20-second timeout.
