# UDP downloads under packet loss with retail peers, 2026-10-08

The [legacy-server record](2026-10-05-legacy-server-hosting.md) passed a UDP
pak download inside a mod with retail Quake III, RTCW and ET. Its pak was
32 KiB on clean loopback; larger files and loss were open. These runs
download a 1 MiB pak while fnTech3's side drops a fifth of the packets it
receives, in both directions, with each of those three retail engines and
with retail Elite Force. All pass. The [JSON companion](2026-10-08-legacy-downloads-under-loss.json) holds
identities, commands and results.

## Method

`scripts/probe_legacy_sessions.py` runs its `download` session. A server
whose mod folder holds a generated pak makes the client fetch the pak before
it may enter. The script gains two options, with its defaults unchanged:

- `--fntech3-set CVAR VALUE` sets a cvar on fnTech3's server and clients
  only;
- `--payload-bytes N` sizes the pak's one generated file.

`net_dropsim 20` makes an fnTech3 process drop 20% of the packets it receives
(`code/qcommon/net_ip.c`). The retail engines have no such setting. So loss
falls on the server's packets when fnTech3's client joins a retail server, and
on the client's when a retail client joins fnTech3's server. A 1 MiB payload
makes a 1,048,724-byte pak: 513 blocks of 2,048 bytes and an empty closing
block in each engine, 514 acknowledgements.

Each run is on loopback, windowed, with private homes under `.tmp`, and each
installation is unchanged afterwards.

## Results

| Profile | fnTech3 client, retail server | Retail client, fnTech3 server |
| --- | --- | --- |
| Quake III (`quake3.exe` 1.32) | pak exact, enters pure | pak exact, enters pure |
| RTCW MP (`WolfMP.exe` 1.4) | pak exact, enters pure; the retail server logged 29 "dropped gamestate, resending" | pak exact, enters pure |
| ET (`etded.exe` / `et.exe` 2.60b) | pak exact, enters pure; 6 gamestate resends against 1 without loss | pak exact, enters pure |
| Elite Force Holomatch (`stvoyHM.exe` 1.20) | pak exact, enters pure | pak exact, enters pure; fnTech3's server resent the gamestate 4 times |

"Exact" means the client's pak hashes the same as the server's. No partial
`.tmp` file remained, and no client was reported unpure. A control run for ET
without loss took about the same time (107 s against 108 s, and 112 s
against 113 s), because joining, not the transfer, dominates the step.

In the ET run with a retail client, fnTech3's server, itself dropping
packets, missed the harness's `rcon quit` and was stopped by force. That is a
harness effect, not a fault.

## Open

Real networks, files of tens of megabytes, broader maps and mods, and the
gamestate-overflow fallback remain open. No profile gate
closes.
