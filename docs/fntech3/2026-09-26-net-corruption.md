# Corrupted live traffic under Q3, QL and RTCW multiplayer

Commit `c71aa54` added `scripts/probe_net_corruption.py` on 2026-09-18 without a
record of a run. This record runs it under `q3`, `ql` and `rtcw-mp`, 20
sessions each: 60 clients whose traffic with an fnTech3 dedicated server was
corrupted once they were in the game. Every session ended in a clean drop,
none crashed, faulted or hung, and the server answered after every session and
ran to the end. The probe needed the changes listed below before its outcomes
described the engine. The
[machine-readable record](2026-09-26-net-corruption.json) holds every session's
outcome, traffic counts and drop reason.

## Method

[`scripts/probe_net_corruption.py`](../../scripts/probe_net_corruption.py)
runs one dedicated server for the whole probe, bound to `127.0.0.1`, and a fresh
client per session that connects through a loopback relay. Connectionless
packets, the handshake, pass unchanged. Once the client's first snapshot
arrives, the relay corrupts one sequenced datagram in eight, chosen by a seeded
generator, in both directions for 15 seconds: 1 to 8 random bytes, after the
sequence number in half of them and anywhere in the others. A session is
`connected` if the client is still in the game at the end with its console tick
loop running, `dropped` if it printed an engine error and was still running two
seconds later or if the server disconnected it, `faulted` if the error is the
exception filter's report of a caught access violation, `crashed` if the client
died, `hung` if none of these, and `never-active` if it never entered the game.
After each session the server must answer `getstatus`, and it must still be
running at the end. Every client is windowed on an unswitched private desktop
with sound, mouse and joystick input, Steam integration and the WebUI off, and
console commands only.

### Changes to the probe

Three earlier attempts on this build, kept under `.tmp/probe-records/`, showed
why the probe as committed could not describe the engine:

- A client that an error drops usually cannot tell the server it has left,
  because its `disconnect` crosses the same damaged channel, so it stays
  connected on the server until it times out. With three such clients lingering,
  `sv_maxclientsPerIP`, 3 by default, refused the next session from `127.0.0.1`
  with "Too many connections", and the server's four slots would have filled
  soon after. The server now has a slot per session, plus four, and admits that
  many connections from one address.
- The tick loop that proves a client is still running was queued before
  `connect` and never ticked again after the map loaded, so a session that
  survived the whole window would have read as `hung`. It now starts from
  `activeAction` once the client is in the game; two uncorrupted sessions then
  ended `connected`.
- A server that drops a client, here for a corrupted user command, sends a
  disconnect that the client handles without printing an error, and the probe
  called that `hung`. It now reads the server's broadcast of the reason and the
  cgame unload that follows as `dropped`. RTCW's cgame prefixes that broadcast
  with `[skipnotify]`; the probe accepts the prefix, and the one RTCW session
  it affected was re-read from its saved log, as the companion notes.

The probe also gained `faulted`, as `scripts/probe_bsp_faults.py` did in the
[damaged-map record](2026-09-26-bsp-faults.md).

## Results

| Profile | Map | Sessions | Connected | Dropped | Faulted | Crashed | Hung | Datagrams corrupted (to server / to client) |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| `q3` | `q3dm1` | 20 | 0 | 20 | 0 | 0 | 0 | 56 of 728 / 79 of 798 |
| `ql` | `campgrounds` `ffa` | 20 | 0 | 20 | 0 | 0 | 0 | 53 of 542 / 162 of 1,895 |
| `rtcw-mp` | `mp_beach` | 20 | 0 | 20 | 0 | 0 | 0 | 82 of 871 / 79 of 1,092 |

Fifty-six clients dropped within 2.2 to 4.6 seconds of the corruption
starting, each on one of its decoder's own checks: a reliable acknowledgement
out of range (15), an illegible server message (13), an area mask of an
impossible size (11), a truncated player-state delta (8), a player-state or
entity field count out of range (4), a truncated entity delta (3), a snapshot
older than the one before it, and entities running past the end of the
message. The other four stayed in the game for the full 15 seconds until the
server dropped them for an invalid or truncated user command: two under Q3 and
one each under QL and RTCW. No session stayed connected: at one damaged
datagram in eight, every client met a bad one within the window.

The server kept serving throughout. It answered `getstatus` after all 60
sessions, reported no caught exception and was still running when each probe
stopped it. Under RTCW its log shows how damaged client traffic is refused:
"dropping 16777216 commands" and repeated "Delta request from out of date
packet".

## What this does not show

- Every peer is fnTech3. Neither a retail client nor a retail server met
  corrupted traffic, and no other seed, rate or single-direction run was made.
- No session stayed connected, so the rate at which a client survives damage
  it can absorb is not measured; lower rates (`--every`) would show it.
- RTCW single player has no network peer to corrupt, and Enemy Territory, Elite
  Force and the other profiles are not part of the probe.
- Loopback only, Windows only, GLx renderer only.

## Identity

- Build: `3089df2` with the grid guard of `5ba3561`, the build of the
  [damaged-map record](2026-09-26-bsp-faults.md). Client
  `meson/build/win32-dev/fntech3.exe`, SHA-256
  `d3ff7129bccb2fdfa1c21540ec459181635fa4ada6c766bcbcddba23ca6b1bdc`; server
  `fntech3.ded.exe`, SHA-256
  `f30fcaa78ad2a94397187d3352d75eba5e7601cc7f4fdf9fd988667ac745398f`.
- Seed 20260918, session *n* seeded 20260918000 + *n*.
- Windows 11 x64 host, 21:50 to 21:55 UTC.
- Installations: `C:\Program Files (x86)\Steam\steamapps\common\Quake 3 Arena`,
  `E:\SteamLibrary\steamapps\common\Quake Live` and
  `E:\SteamLibrary\steamapps\common\Return to Castle Wolfenstein`, read only.
