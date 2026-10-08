# The IHV Test's original client joins an fnTech3 server, 2026-10-08

The [live client record](2026-10-08-q3-ihv-live-client.md) connected
fnTech3's client to the original `quake3.exe` server. This record covers the
other direction: fnTech3's dedicated server now takes protocol-33 clients. The
original IHV client connects, enters `tim_dm1`, plays while the server's game
runs and leaves cleanly. Together with fnTech3's own client, both survive a
level change and a kick. The [JSON companion](2026-10-08-q3-ihv-server-sessions.json)
holds identities, commands and results.

## Observed in the original

Addresses are virtual addresses in `quake3.exe` (SHA-256 `e9fcaa0a...5135d37`),
read with pefile and Capstone. No original code is incorporated.

**Connectionless.** The dispatcher (`0x41a560`) compares the first word
exactly. It handles `getstatus`, `getinfo`, `getchallenge`, `connect` and
`rcon`, ignores `disconnect`, and prints `bad connectionless packet` for
anything else. That includes the client's own broadcast `info 33`.

- `getchallenge` (`0x4180e0`) keeps 1024 entries matched by address alone. It
  never refreshes them; an unknown address takes the oldest entry and a new
  `(rand() << 16) ^ rand()`. The reply is `challenge <n>`.
- `getinfo` (`0x41a260`) answers `info\n33 "<hostname>" "<mapname>" <maxclients> <n>`.
  The format has five numbers but the call passes four, so the last one is
  whatever word was on the stack: the original server answered `91536724`.
- `getstatus` (`0x41a0c0`) answers `status\n<argument>\n<serverinfo>\n`. Then
  comes one `score ping "name"` line per connected client, the score being the
  signed word at player-state byte `0x84`, while the reply stays under 1400
  bytes.
- `rcon` (`0x41a340`) checks the `rconpassword` cvar.

**Connect** (`SV_DirectConnect`, `0x4181d0`).

- The request is `connect 33 <qport> <challenge> "<userinfo>"`. Any other
  version is answered `print\nServer is version 4.01.\n`.
- The rcon password, when set and given as `password`, opens the
  `privateclients` slots.
- A non-local address needs its challenge (`No challenge for address.` or
  `Bad challenge.`), and its `ip` key is set.
- A client from the same address, with the same qport or port, takes its slot
  again with no disconnect, once `sv_reconnectlimit` has passed.
- The game's `ClientConnect` sees the userinfo before the slot is set up. A
  refusal is answered `print\n<reason>\n` and leaves the slot free. Otherwise
  the slot copies the userinfo and reads name, rate and snaps from it, the
  reply is `client_connect`, the channel is set up with the client's qport,
  and the client is connected.

**Client messages.**

- A connected client's first message brings it the gamestate (`0x418750`).
  That is every set configstring and every baseline with a visible field, then
  a zero byte, and the client counts as holding every configstring change so
  far.
- `SV_ExecuteClientMessage` (`0x418ef0`) then reads nops, moves and at most
  seven commands. Every reader returns -1 for a value the message does not
  hold, without consuming anything, so a truncated message ends quietly.
- A move (`0x418cc0`) is read whole before anything is tested:
  - The delta frame and acknowledged configstring sequence are stored first.
  - A count outside 2..32 ends the move with a console line.
  - A move naming another level runs nothing.
  - The first move naming this level enters the world (`0x418840`): the entity
    takes its number and `ClientBegin` runs.
  - The commands before the provisional last one run through `ClientThink`. A
    gap is filled first with up to five runs of the move's first command.
  - Unless `sv_nopredict` is set, the provisional command goes to the export
    that runs after the commands.
  - The entity's state time then takes the server time.
- Client commands are `userinfo` (the game sees it before the engine),
  `disconnect`, `download` and `nextdl`; anything else goes to the game while
  the level runs.

**Sending.**

- Server commands are the byte 5 and the text, copied onto the reliable message
  (`0x419e90`): to one client whatever its state, or to every client from primed
  on. A dedicated server echoes broadcast prints.
- Each datagram (`0x41b520`) repeats every configstring whose change stamp is
  newer than the client's acknowledged sequence (`0x41af60`), then carries the
  snapshot. Each change gets its stamp from the new configstring sequence
  (`0x419655`).
- The next snapshot time comes from the datagram (`0x41b5e8..0x41b690`):
  - a loopback client gets one every server frame;
  - a client silent for over a second gets one a second;
  - otherwise the rate's delay, `(bytes + 48) * 1000 / rate`, or the snapshot
    interval when that is longer. The rate-delayed flag in the snapshot header
    is set when the rate decided.
- Every other slot, zombies included, gets an empty message whenever reliable
  data waits, or once a second (`0x41b6a0`).
- An overflowed reliable message drops its client.
- A bot's snapshot is built and nothing is sent. Its next snapshot time is
  never advanced, so the original rebuilds it every server frame.
- Ping (`0x41a910`) is the mean of every acknowledgement time minus send time
  that is not negative, over all 32 frames, unused ones included. Every
  accepted packet restamps the frame it acknowledges.

**Message writes.** Strings and raw data are copied at the byte count without
touching the bit position (`0x410d00`, `0x410aa0`, `0x410a30`). A bit write is
dropped, and the message marked overflowed, once fewer than four bytes remain
(`0x410b40`).

**Levels and shutdown.** A new level clears the configstring sequence and its
stamps and resets every slot's times. It offers every client from connected
on, bots included, to `ClientConnect` again (`0x419966`): refused ones are
dropped, the rest are connected. A drop (`0x4186e0`) announces
`"<name> <reason>"` to everyone primed, calls `ClientDisconnect`, queues
`disconnect` and leaves a zombie. Shutdown (`0x419cd0`) sends every connected
client the print and the disconnect, twice.

## The fnTech3 server

`code/server/server_q3_ihv_net.hpp` follows each of these. The rules that need
no engine state are in `code/compat/q3_ihv_server_session.hpp`: the original's
message writes and reads, the move reader, which commands run, the replies, the
schedule and the ping. It uses the existing protocol-33 channel and the
snapshot builder and writer, which earlier records had already compared with
the original.

- **Two records per client.** Each network client has the engine's `client_t`
  slot, for `status`, `kick` and timeouts, and the native client record the
  snapshot code reads. The engine's packet, server command, drop,
  configstring, ping, level and shutdown paths hand the IHV profile to the
  session.
- **The gamestate and status.** The gamestate is written with the original's
  own primitives. `status` reads the IHV player state's score.
- **Bot snapshots.** As the original does, the snapshot pass now rebuilds a
  bot's snapshot every server frame. It had advanced bots by their snapshot
  interval.

**fnTech3 boundaries.**

- `getinfo`'s last field is the number of connected clients, where the original
  sends an unwritten stack word.
- A download is refused with the original's refusal (length -1); none is served.
- A datagram counts as overflowed once it passes 1396 bytes, the point after
  which the original's bit writes stop. It is then sent empty with the
  original's warning.
- A slot is not dropped twice. The original has no such guard.
- Session times are kept in level time as the original's are. The engine's own
  timeout check reads its server time.

## Runs

The server ran from a build of HEAD (`6cc18d02`) plus this change, with an
isolated home. The installation was only read:
`fntech3.ded.exe +set com_gameProfile q3-ihv ... +set net_ip 127.0.0.1
+set net_port 27996 +map tim_dm1`. The original client ran from the writable
copy under `.tmp/q3-ihv/original-server` with `connect 127.0.0.1:27996`.
Everything ran on private desktops, windowed, and no input was driven.

| Run | Clients | Result |
| --- | --- | --- |
| Original alone | original `quake3.exe` | connected, entered the game, disconnected; exit 0 after 30 s |
| Both | original and fnTech3 (GLx) | both entered `tim_dm1`, then `km_portal` after `rcon map`, and each saw the other; `clientkick 0` reached the original as a clean disconnect; both exit 0 |
| fnTech3 alone | fnTech3 on GLx, on Vulkan | entered, played, disconnected; exit 0 |
| Queries | none | `getinfo`, `getstatus`, `getchallenge`, a version-32 connect, a bad challenge and `info 33`, answered as above |

**The original client.**

- In the final run alone, its two captures read health 119 and then 106 as the
  spawn bonus counted down, so the server's game ran and its snapshots arrived.
- Its console logs the server commands and the kick: `entered the game`, a
  telefrag when both clients spawned on one point after the level change,
  `Original was kicked`, `disconnect`.
- Its own screenshots are taken after the buffer swap and are not pixel
  references.
- Its `wait` counts uncapped main-loop passes, about 10,000 a second while
  loading. The harness chains cvars through `vstr`, since the build has no
  `alias`.

**Replies and ping.**

- The original server's replies to the same queries have the same formats. Its
  serverinfo keys are its own (`hostname`, `privateclients`); fnTech3's carry
  the engine's.
- Ping read 0 on loopback. In one run the original client's read 999 after the
  level change. This is inferred, not compared live: per-frame acknowledgement
  stamps from the previous level outweigh the new level's send times until
  fresh acknowledgements cover all 32 frames, as the original's arithmetic
  allows. In the final run it read 0.

**Tests.** `tests/q3_ihv_server_session_tests.cpp` covers:

- the write and read rules, including the misaligned copy;
- moves and which commands run;
- the replies, schedule and ping;
- a gamestate written with the original's primitives, byte-equal to the shared
  writer's and accepted by the client's decoder.

The bridge test now checks the per-frame bot snapshot and the network slot's
exemption. Eleven source-slicing fixtures gained the new hooks' stubs. The
touched server sources build with MSVC strict warnings, and pass GCC and clang
i686 syntax checks.

## Open

- longer, impaired and remote sessions;
- moved and fought play, which needs input;
- downloads;
- the original's serverinfo key set;
- comparing the original client against the original server for the same
  scenes.

The profile stays a Windows x86 developer path, and release builds still refuse
it.
