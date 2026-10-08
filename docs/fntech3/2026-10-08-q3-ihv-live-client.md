# The IHV Test's client joins its original server, 2026-10-08

The [client record](2026-10-08-q3-ihv-client.md) ran the IHV Test's own client
game in the developer client, but only on its demo. That client now makes a
protocol-33 connection. It joins the original `quake3.exe` dedicated server on
loopback, enters `tim_dm1`, plays there while the server's game runs and
sends it frames, and leaves cleanly. The reverse direction, the original client
joining an fnTech3 server, is not done: fnTech3's IHV server still drops every
packet. The [JSON companion](2026-10-08-q3-ihv-live-client.json) holds
identities, commands and results.

## Observed in the original

Addresses are virtual addresses in `quake3.exe` (SHA-256 `e9fcaa0a...5135d37`,
as in the client record), read with pefile and Capstone. No original code is
incorporated.

**Handshake.** It is Quake II's. Every three seconds `CL_CheckForResend`
(`0x405280`) sends `getchallenge` while connecting. While challenging it sends
`connect 33 <qport> <challenge> "<userinfo>"`, the qport from the `qport`
cvar; a local server needs no challenge. `CL_ConnectionlessPacket`
(`0x405420`) answers five words. `challenge <n>` stores the number and moves
to challenging. `client_connect` is taken only while challenging and only
from the server's address; it sets up the channel and makes the client
connected. `disconnect`, `echo` and `info` (the browser's) are the others.
There is no `print`, which the build's own server sends with a refusal.

**Sending.** `CL_SendCmd` (`0x403360`) sends a connected client's empty packet
each second, or at once when reliable data waits. A client that is loading,
primed or active builds the frame's command into the 64-slot history, then a
provisional copy into the next slot, which the next frame's command replaces.
The client game reads both through imports 59 and 60. Except over loopback,
packets are held to `cl_maxpackets`. A move goes once two commands are new:
opcode 2, the server id, the client's time, the frame to delta from (or -1),
the configstring sequence of the newest frame, the last finished command, a
count of at most 32, then each command as a delta from the one before it
(`0x411910`). Client commands, such as `userinfo`, are opcode 3 and a string
in the channel's reliable block. The 14-byte command is milliseconds (over
250 becomes 100), buttons (0x10 walking, 0x80 any key), the client game's
weapon (import 61), short angles and signed moves (`CL_FinishMove`
`0x402e40`). This is Q3Test 1.02's layout, an inheritance its records had
already shown.

**Receiving.** The channel's message is parsed from just after its header
(`0x4057f1`), and an empty one ends at once (`0x4070cd`). A frame's ping
(`CL_ParseFrame` `0x406ab2`) is the time since the newest of the last 32
packets whose command number is the one the server names as its last.

## The fnTech3 client

`code/client/cl_q3ihv.cpp` follows each of these. It uses the existing
protocol-33 channel (`q3_ihv_netchan.hpp`), which already matched the original
packet for packet, and the existing message codecs. The engine's own
reliable-command queue drains into the channel's reliable block, as Q3Test's
legacy path does. The session's ping now comes from the decoded frame
(`ClientSession::FeedWith`), and the command history can rewrite its newest
slot (`ClientData::ReplaceNewestCommand`). An empty message is skipped; the
first live run had stopped on one, an acknowledgement-only packet, with
"Illegible server message". A `print` is shown on the console, where the
original showed nothing. A developer option, `cl_ihvRawLog <file>`, records
every message of a connection in `.dm3` record form for offline replay.

## Runs

The original server ran from the writable copy under
`.tmp/q3-ihv/original-server`: `quake3.exe +set dedicated 1 +set net_port 27995
+map tim_dm1`. The client ran `connect 127.0.0.1:27995`, took engine
screenshots 300 and 600 frames in, and quit. Both were on private desktops,
windowed, with no input. The installation was only read.

| Renderer | Client exit | Seconds | Server log |
| --- | --- | --- | --- |
| GLx | 0 | 16.0 | `fnTech3 connected`, `entered the game`, `disconnected` |
| Vulkan | 0 | 17.0 | the same |

The captures show `tim_dm1` in the map's own fog, with the weapon and the HUD.
Across an earlier run's two captures, the spawn bonus counted health down from
121 to 116, so the server's game ran and its frames arrived. Replayed offline
through the decoder, the GLx session's log holds 187 messages: one gamestate,
186 frames over 9.25 s of server time, 12 configstrings and the server's
`entered the game`, with no decoding failure (Vulkan: 179 and 178).

No movement or firing was driven. That needs input on the client, and no
input was injected, so the server's handling of moves is shown only by its
admission of the client into the game.

## Open

The original client joining an fnTech3 server, longer and impaired sessions,
a remote peer, moved and fought play, downloads, and recording an IHV demo
remain open. The client stays a developer, Windows x86 path, and release
builds still refuse the profile.
