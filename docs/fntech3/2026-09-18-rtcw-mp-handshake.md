# RTCW MP handshake, retail servers and only-visible clients

The [RTCW matrix](COMPATIBILITY.md#rtcw-gate-matrix) listed outbound MP peer
admission as open: an fnTech3 client joining a retail RTCW MP server. The first
attempt in this slice failed outright. This record fixes the handshake in both
directions, adds the RTCW MP `sv_onlyVisibleClients` contract the handshake
carries, and records outbound retail admission, pure and non-pure, with both
teams. The [machine-readable record](2026-09-18-rtcw-mp-handshake.json) holds
identities, commands and per-run results.

## The failure

Against retail `WolfMP.exe` running `+set dedicated 1 +map mp_beach`, the
fnTech3 client in the `rtcw-mp` profile received `challengeResponse <challenge>`
and then sent `connect` naming protocol **68**. The retail server logged
`rejected connect from version 68` for every attempt, and the client showed
`Protocol Mismatch Between Client and Server` until the probe stopped it.

The client read every server's reply with Quake III's token layout:
`challengeResponse <challenge> <nonce> <protocol>`, and a reply naming no
protocol meant "Quake III legacy server, use 68". RTCW does not use that layout:

| Server | `challengeResponse` tokens | `connectResponse` |
| --- | --- | --- |
| Original RTCW MP | `<challenge>`, plus `<sv_onlyVisibleClients>` when it is set | bare |
| iortcw MP | `<challenge> <onlyVisibleClients> <nonce> <protocol>` | `<challenge>` |
| Original RTCW SP | `<challenge>` | bare |
| iortcw SP | `<challenge> <nonce> <protocol>` (ioquake3's) | `<challenge>` |

Observed in [RTCW MP](https://github.com/id-Software/RTCW-MP) and
[RTCW SP](https://github.com/id-Software/RTCW-SP) `src/server/sv_client.c`
(`SV_GetChallenge`, `SV_DirectConnect`) and `src/client/cl_main.c`
(`CL_ConnectionlessPacket`), and in [iortcw](https://github.com/iortcw/iortcw)
`MP/SP code/server/sv_client.c` and `code/qcommon/q_shared.h`. The sources were
read only; no implementation text is incorporated.

Three further mismatches came out of the same reading:

- **The fnTech3 server used Quake III's layout for RTCW MP** when a client sent
  a nonce, so an iortcw MP client would take the nonce as `onlyVisibleClients`
  and fail its own nonce check on the protocol number.
- **The client named Quake III in `getchallenge`.** iortcw servers refuse any
  game name but their own (`Game mismatch: This is a wolfmp server`); original
  RTCW servers ignore the token.
- **RTCW connections asked Quake III's authorize server** about the CD key,
  because the authorization request was gated only on a protocol at or below
  68. Original RTCW MP used its own `wolfauthorize.idsoftware.com`, since
  decommissioned.

## Correction

A shared, independently written helper
([`code/qcommon/rtcw_handshake.hpp`](../../code/qcommon/rtcw_handshake.hpp)) parses
and produces RTCW's layouts:

- The client reads RTCW MP replies as `<challenge> [<onlyVisibleClients>
  [<nonce> <protocol>]]` and SP replies as `<challenge> [<nonce> <protocol>]`. A
  reply that echoes the client's nonce must echo the one sent; one that echoes
  none, as original servers send, is accepted only from the address the request
  went to. A reply that names no protocol means the profile's own (49 or 60);
  a different one is warned about and the profile's protocol is still offered,
  as iortcw does. Nothing in RTCW is ever relabelled Quake III legacy.
- The client accepts original RTCW's bare `connectResponse`, and an echoed
  challenge only if it matches; a named protocol must be the profile's own.
- The fnTech3 RTCW MP server answers a client that sent a nonce in iortcw's
  four-token layout, and one that sent none as the original server does: the
  challenge alone, or with `sv_onlyVisibleClients` when that is set. RTCW SP and
  every other profile keep their existing replies.
- `getchallenge` names `wolfmp` or `wolfsp` in the RTCW profiles; Quake III keeps
  `Quake3Arena`. The Quake III authorize request is Quake III's alone.

## Only-visible clients

An RTCW MP server with `sv_onlyVisibleClients` set (registered for RTCW MP,
default 0, as original) asks its clients to stop drawing players they cannot
see. Original RTCW MP and iortcw do this in the client parser: seven solid-only
lines from the eye (with lean) to points on each player, and a player unseen for
more than 600 ms gets `EF_NODRAW`. fnTech3 now does the same for RTCW MP only.
The result is written into the parsed state, so, as in the original, it also
becomes the next delta's base, clears a server-set `EF_NODRAW` on a player the
client can see, and the per-player times are never reset.

[`scripts/probe_rtcw_only_visible_clients.py`](../../scripts/probe_rtcw_only_visible_clients.py)
compiles the original `isEntVisible`, unchanged, in a disposable fixture next to
fnTech3's function, with the same shared math and a recording trace that
answers from a per-case script. Over 100,000 seeded cases (lean in a quarter,
crouched/prone movement in a quarter, degenerate directions included) both the
original and iortcw references agree with fnTech3 on every decision and on the
exact bit patterns, bounds, model, mask and capsule flag of all 436,777 traces,
on both the Windows x86 and x64 targets.

## Runtime evidence

All runs use the same Windows x86 development build, windowed on unswitched
private desktops with isolated homes, loopback UDP to 127.0.0.1 (so the full
challenge exchange runs), sound and mouse/joystick input disabled. The client
acts through console commands only; no mouse or keyboard input is injected and
no operating-system capture is taken. Installations are unchanged afterwards.
Run with [`scripts/probe_client_to_server.py`](../../scripts/probe_client_to_server.py)
and the existing [`scripts/probe_rtcw_retail_client.py`](../../scripts/probe_rtcw_retail_client.py).

| Client → server | Replies the client received | Result |
| --- | --- | --- |
| fnTech3 → retail `WolfMP.exe`, non-pure | `challengeResponse <challenge>`, bare `connectResponse` | retail `CS_ACTIVE` and `ClientBegin`; spectator capture; client exit 0 |
| fnTech3 → retail, `sv_pure 1`, team check | same | client prints `Connected to a pure server.`; no unpure drop; joins Axis then Allies in retail's log; two captures, the second first-person with the Allied weapon |
| fnTech3 → retail, `sv_onlyVisibleClients 1` | `challengeResponse <challenge> 1` | client reports the server's request and keeps it; admitted; exit 0 |
| fnTech3 → fnTech3 dedicated, `sv_pure 1`, team check | `challengeResponse <challenge> 0 <nonce> 60 FnQL`, `connectResponse <challenge>` | pure admission, both teams, two captures |
| fnTech3 → fnTech3 dedicated, `sv_onlyVisibleClients 1` | `challengeResponse <challenge> 1 <nonce> 60 FnQL` | request taken; admitted |
| fnTech3 → fnTech3 dedicated, Q3 `q3dm17`, `sv_pure 1` | `challengeResponse <challenge> <nonce> 68 FnQL`, `connectResponse <challenge>` | unchanged Q3 layout; pure admission; exit 0 |
| retail `WolfMP.exe` → fnTech3 dedicated, `sv_pure 1`, team check | bare, as before | 15 paks on the client's pure list; `CS_ACTIVE`; both teams; both exit 0 |

Before the correction the same outbound run failed as described above. The
retail server's own log is the admission evidence; every client capture was
inspected and shows the `mp_beach` warmup scene as a spectator or on a team.

## Tests

[`tests/rtcw_handshake_tests.py`](../../tests/rtcw_handshake_tests.py) runs the
helper against every layout (original and iortcw, both modes, bad and missing
echoes, redirected replies, bare and echoed connect replies, the server's
layouts and a round trip), executes the real culling policy from
`cl_parse.cpp` (profile and cvar gating, bounds, first clear line wins, the
600 ms rule, per-player times, the solid-only mask) and pins the client and
server call sites. All pass on the x86 and x64 targets.

## Remaining gates

- One map, loopback only, short sessions. Other stock maps, remote networks and
  sustained play remain open.
- No iortcw MP client or server binary was run; the iortcw layouts are source
  and unit evidence only.
- The culling itself is compared against the original function, not observed in
  a live game with players out of sight.
- A server's `onlyVisibleClients` value is not reapplied after the client
  records a demo; recorded demos keep the culled flags, where original demos
  keep the server's raw ones.

## Unrelated observations

- The retail dedicated server runs its recommended-settings config and a
  `vid_restart` broadcast on startup, then restarts the game once before
  settling; probes wait for its registered items after that restart.
