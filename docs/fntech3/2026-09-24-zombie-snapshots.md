# Zombie snapshots: each profile's own empty player state

## Scope

A client that the server drops is a zombie until its slot is freed, a few
seconds later. `SV_FinalMessage` also makes every connected client a zombie
before a quitting server's last snapshot. Each message a zombie is sent
carries a snapshot with no changes: an empty player-state delta, then the end
of the entity list. The reliable commands in those messages are how the
client that was dropped learns why, and how every client hears a quitting
server's `print` and `disconnect`.

fnTech3 wrote Quake III's empty delta, a zero field count and one zero array
bit, in every profile whose messages are Huffman-coded. Elite Force Holomatch
already had its own. RTCW's and ET's readers take five more bits. Retail
`WolfMP.exe` and `et.exe` clients therefore misread every zombie snapshot and
failed with `CL_ParsePacketEntities: end of message`. The client that was
kicked failed before it could run its `disconnect`, and every client failed
when the server quit.

This record covers that fault in `rtcw-mp` and `wolfet`, where retail clients
showed it, and pins the empty delta of every profile the engine can send one
to. Quake III's and Quake Live's bytes do not change.

## Observed

### Retail clients on the unchanged server

`scripts/probe_drop_notice.py`, from `2026-09-24-rtcw-et-drop-notices.md`
(uncommitted when this was written), runs a fnTech3 dedicated server and two
retail clients over loopback, each on its own private desktop with an isolated
home, with no input injected. The client to be dropped joins first, then an
observer. With both in the game, rcon execs a config holding `clientkick 0`.
Three seconds after the observer has executed the notice, the probe quits the
server by rcon. Both clients log every server command they execute.

On `2d35919`, in RTCW MP on `mp_beach` and ET on `goldrush`, both clients
failed with `CL_ParsePacketEntities: end of message`. The dropped client ran
none of the notice, the configstring change or `disconnect`. The observer ran
the notice and the command clearing the dropped player's configstring
(`cs 576 ""`, `cs 689 ""`), and in ET the next score and entity-info
commands, and then failed.

A diagnostic wrapper around the same probe adds `cl_shownet 2` to one retail
client, which then prints how it parses every message. The runs are listed in
the [evidence file](2026-09-24-zombie-snapshots.json).

- The dropped client. The message holding the notice, the configstring change
  and `disconnect` opened with those three commands and a snapshot. RTCW MP
  read its player state as 2,274 bits and ET's as 2,055. Each then failed in
  the entity list. RTCW MP began that list at byte 234, while the changed
  server's message with the same three commands ends at byte 94. None of the
  three commands ran.
- The observer. It stayed in the game through the drop and failed on the
  server's last message. That message held two commands and a snapshot at
  bytes 6, 29 and 56, where the changed server's quit message holds its
  `print`, `disconnect` and snapshot (ET's `disconnect` at byte 30). RTCW MP
  read the snapshot's player state as 2,179 bits and ET as 753.
  `SV_FinalMessage` (`code/server/sv_init.cpp`) marks every client a zombie
  ("skip delta generation") before it sends that snapshot.

So the observers' failures were the same fault as the dropped client's. The
drop itself did not cause them: they fell on the server's quit message.
`2026-09-24-rtcw-et-drop-notices.md`'s runs fit that. Each fnTech3 server log
there ends at the rcon `quit`, and its retail-server runs' observers executed
`print "Server quit` and `disconnect` at that point.

### The readers

Each engine's reader, given a player state with nothing changed, makes these
reads. Each must give zero. An 8-bit read is one Huffman symbol in a
compressed message. Line numbers are those of the reference copies of the
GPL releases.

| Profile | Reader | Empty delta |
| --- | --- | --- |
| `q3`, protocols 66, 67, 68 and 71 | [Quake III 1.32](https://github.com/id-Software/Quake-III-Arena/blob/master/code/qcommon/msg.c) `MSG_ReadDeltaPlayerstate` (1315: count 1348, arrays 1394) | 8-bit count, array bit |
| `ql` | fnTech3's Quake Live layout reader, Quake III's shape with QL's fields | the same |
| `rtcw-mp` | [RTCW MP](https://github.com/id-Software/RTCW-MP/blob/master/src/qcommon/msg.c) `MSG_ReadDeltaPlayerstate` (1619: count 1652, arrays 1698, stored ammo 1775, ammo in clip 1790) | 8-bit count, array bit, ammo bit, four clip bits |
| `wolfet` | [ET](https://github.com/id-Software/Enemy-Territory/blob/40342a9e3690cb5b627a433d4d5cbf30e3c57698/src/qcommon/msg.c) `MSG_ReadDeltaPlayerstate` (1716: count 1749, arrays 1795, stored ammo 1872, ammo in clip 1887) | the same |
| `rtcw-sp` | [RTCW SP](https://github.com/id-Software/RTCW-SP/blob/master/src/qcommon/msg.c) `MSG_ReadDeltaPlayerstate` (1790: fields 1823, arrays 1860, stored ammo 1937, ammo in clip 1952) | a bit for each of 76 fields, array bit, ammo bit, four clip bits |
| `stvef-mp` | `stvoyHM.exe`, as `code/compat/stvef_message_codec.hpp` and the [Holomatch record](2026-09-18-stvef-holomatch.md) give it; raw bits | a bit for each of 48 fields, four array bits |
| `stvef-sp` | fnTech3's own loopback codec | 8-bit word count |

The RTCW and ET readers' alternative ammo encoding lies inside `#if 0` and is
not compiled.

Six more profiles existed only in other sessions' uncommitted work when this
was written. Alice's reader (`alice.exe` `0x004468e0`,
`2026-09-22-alice-wire.md`) takes a bit for each of 49 fields and six block
bits, and its snapshot then ends with a sound block. The IHV test's wire has
no message layout in this engine. The two Jedi Academy and two Jedi Outcast
profiles register no wire profile at all.

### Retail clients on the changed server

The same traces, on `2d35919` with this change:

- The dropped client, RTCW MP and ET alike, parsed the message holding the
  notice, the configstring change and `disconnect` to its end: three
  commands, a snapshot whose player state it reports as 16 bits, the entity
  list and `END OF MESSAGE`. It then ran all three and left with
  `Server Disconnected - was kicked`.
- The observer parsed the quitting server's message the same way and ran
  `print "Server quit` and `disconnect "Server quit"`.

### What retail servers send a zombie

- A traced client on retail `WolfMP.exe` received its zombie snapshots
  without delta compression, each carrying a whole player state, which it
  reports as 1,278 bits. Their `commandTime`s, 28,295 and 28,359, are older
  than its last snapshot in the game, 29,843: the slot's frame from an earlier
  message, never refreshed. It read them and ran the notice and
  `disconnect "player kicked"`.
- The RTCW MP, ET and Quake III 1.32 sources have no branch for a zombie in
  `SV_WriteSnapshotToClient`. `SV_BuildClientSnapshot` returns before it
  copies the player state for a zombie (RTCW MP `sv_snapshot.c` 559, ET 553,
  Quake III 462), and the writer then codes the frame's old state from
  nothing, because a zombie is not active.
- The branch that sends a zombie no changes is Quake3e's, whose
  `sv_snapshot.c` says "don't send any changes to zombies". FnQuake3 and FnQL
  inherited it.
- A traced ET client on retail `etded.exe` did not reach the game before the
  kick, because tracing slows it too much. ET's retail zombie snapshot is
  therefore known from its source. Untraced retail ET clients read it, in the
  runs below.

## Change

`SV_WriteSnapshotToClient` (`code/server/sv_snapshot.cpp`) writes a zombie's
player state through `MSG_WriteDeltaPlayerstateForWireProfile`, from and to
the same empty state, in every profile. Each profile's codec therefore flags
every field and array unchanged in its own encoding. A profile the engine has
no message layout for raises `ERR_DROP`, as the delta-coded path does, rather
than being sent a player state its client cannot read.

- `q3` and `ql` get Quake III's two fields, byte for byte as before.
- `stvef-mp` gets what it got before, through the same call.
- `rtcw-mp` and `wolfet` get the count and array bit, then the ammo bit and
  four clip bits.
- `rtcw-sp` and `stvef-sp` get their own forms, which fnTech3's own client
  reads. No retail client of either was run.

fnTech3 keeps Quake3e's rule of sending a zombie no changes, not the stale
frame retail sends. The retail clients above read the empty delta and ran
the commands that came with it.

The RTCW, ET, Quake III and Quake3e sources and the retail executables were
read as reference observations. No text from them is incorporated.

## Evidence

- `tests/server_zombie_snapshot_tests.py` compiles the production
  `SV_WriteSnapshotToClient` and `SV_EmitPacketEntities` with the engine's
  `msg.cpp` and static Huffman table. It runs every wire profile that
  `code/qcommon/netchan_profile.h` registers, and fails if one has no entry
  in its table of empty deltas. For each profile it writes a zombie's
  snapshot and reads it twice. The first reader is written in the test from
  the table above. The second is the engine's own. Both must find an empty
  player state, the entity end marker, the Alice sound block where there is
  one, and nothing after. An active client's snapshot must still carry its
  frame's player state, as a direct write of that state decodes. A profile
  without a message layout, and an unregistered one, must be refused. It
  also checks that `SV_FinalMessage` marks each client a zombie between
  queuing its `disconnect` and sending its snapshot.

  Nine mutations of the compiled production code each fail it: Quake III's
  empty delta in every profile; Quake III's everywhere but raw-bit framing,
  which is the code before this change; RTCW's in every profile; Quake
  Live's encoding whatever the client's; the slot's frame coded from nothing,
  as retail sends; a write the profile cannot make carrying on regardless;
  no entity end marker; every client sent a zombie's snapshot; and a zombie
  sent its frame's delta.
- Builds. The tree of `2d35919` with this change alone was exported and built
  as a dedicated server for Windows x86 with MSVC, `debugoptimized` and strict
  warnings, with the RTCW, ET and Elite Force development options:
  `fntech3.ded.exe`
  `4e2b7fd96b6520830c546593a74b5ef558297ce69f2151f73e8079172ba4f9a1`. It
  built without a warning. The baseline is `2d35919` alone, built the same
  way for `2026-09-24-rtcw-et-drop-notices.md`:
  `faf79479ffec484c84ec2796d3db73fc79d4b6d0c161a955bec829d44eb26112`.
- Suites. The same tree, built with the client too, built all 1,200 targets
  without a warning. Its 116 Meson C++ tests pass and two skip: OpenAL's
  `ALC_SOFT_loopback` was unavailable, and the retail factory probe found no
  Quake Live without `FNQL_RETAIL_QL_PATH`. The whole Python suite passes in
  the same export: 1,835 tests, with 76 skipped.
- Runtime, interop: the probe as above, unchanged, with retail `WolfMP.exe`
  and `et.exe` clients. RTCW MP played `mp_beach` in game type 5, ET
  `goldrush` in game type 2, both pure.

  | Server | The dropped client executed | The observer executed |
  | --- | --- | --- |
  | Retail `WolfMP.exe` | the notice, then `disconnect "player kicked"` | the notice; at the quit, `print "Server quit` and a bare `disconnect` |
  | fnTech3 `rtcw-mp`, baseline | nothing: `CL_ParsePacketEntities: end of message` | the notice and `cs 576 ""`; then the same error at the quit |
  | fnTech3 `rtcw-mp`, candidate | the notice, `cs 576 ""`, then `disconnect "was kicked"` | the notice and `cs 576 ""`; at the quit, `print "Server quit` and `disconnect "Server quit"` |
  | Retail `etded.exe` | the notice, then `disconnect "player kicked"` and a newline | the notice; at the quit, `print "Server quit` and a bare `disconnect` |
  | fnTech3 `wolfet`, baseline | nothing: the same error | the notice and `cs 689 ""`; then the same error at the quit |
  | fnTech3 `wolfet`, candidate | the notice, `cs 689 ""`, then `disconnect "was kicked"` | the notice and `cs 689 ""`; at the quit, `print "Server quit` and `disconnect "Server quit"` |

  Three more candidate runs for each profile ended the same way, with no
  error on either client. The candidate's notice is still Quake III's
  `print "%s^7 %s\n"`, and its reason `was kicked`: the retail forms are
  `2026-09-24-rtcw-et-drop-notices.md`'s change, not in `2d35919`. The probe
  therefore reports `clientsIntact` for every candidate run but not
  `passed`. Every run left the installation's inventory unchanged, and each
  server's hash matched at the start and the end.
- Runtime, the `pure-live` plan of `scripts/probe_restart_variable_change.py`
  (uncommitted when this was written). It turns `sv_pure` on under a
  connected retail client, restarts, and then quits the server by rcon. On
  `2d35919` the client in each profile came through both steps and then
  failed with `CL_ParsePacketEntities: end of message` at the quit, as the
  pure-live runs of `2026-09-24-wolfenstein-restart-variable-change.md`
  (also uncommitted) did. With this change the RTCW MP client ran
  `print "Server quit` and `disconnect "Server quit"`. The ET client printed
  `Server quit` and no error. `et.exe` logs no commands at that point,
  because it drops `developer` to 0 after the systeminfo update.

## Found, not changed

- **A quitting server's words.** Retail RTCW MP and ET send `print "%s"` and
  a bare `disconnect` from their final message (RTCW MP `sv_init.c`
  `SV_FinalMessage`, ET `SV_FinalCommand`, as in Quake III 1.32), and a retail
  client shows `Server disconnected`. fnTech3 sends `print "%s\n"` followed by
  a newline, and `disconnect "%s"`, so the client shows
  `Server Disconnected - Server quit`.
- **A quitting server's last snapshot.** Retail sends a client in the game an
  ordinary delta-coded snapshot at the quit. fnTech3 marks every client a
  zombie first and sends it no changes. Retail clients read both.
- **ET's `spawnserver`.** Retail `etded.exe` and `et.exe` hold the command
  `spawnserver` (`0x4c40f8`, pushed at `0x42cf3a`; `0x562a94`, pushed at
  `0x44680a`). ET's source sends it to every client from `SV_SpawnServer`
  before a level change, and its client game then shows "Server Restarting".
  fnTech3 never sends it.

## Open

- RTCW SP and Elite Force SP rest on the unit test and fnTech3's own readers.
  No retail single-player client took part.
- Quake III, Quake Live and Holomatch send the same bytes as before and were
  not run.
- Only `clientkick` drops and rcon quits were run, on `mp_beach` and
  `goldrush`. Timeouts and unpure clients are dropped through the same
  `SV_DropClient` and zombie snapshot, and were not run.
