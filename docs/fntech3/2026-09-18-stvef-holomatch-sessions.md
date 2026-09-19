# Elite Force Holomatch live sessions and interoperability

## Scope

This record covers live Holomatch sessions over loopback UDP between
fnTech3, Lilium Voyager 1.40 and the retail `stvoyHM.exe` 1.20, as client and
as server, with and without pure admission. It also covers recording a demo
during a live session and playing that demo in fnTech3 and Lilium Voyager,
and downloading a missing pak from each server. The profile stays behind
`-Dstvef-development=true`.

## Setup

A Windows x86 developer build ran against the retail installation. Every
process was windowed where it had a window, had an isolated home directory,
bound to 127.0.0.1 only, and announced to no master server; sound was
silent and mouse input off. Servers ran `hm_borg1` in free-for-all with the
bots Munro and Telsia and `sv_pure 0`. Clients connected with `connect`,
took `screenshotJPEG` frames and quit. The installation was left unchanged.

Lilium Voyager 1.40's 32-bit dedicated server stops with an access violation
on this machine before it writes any output, with or without arguments; its
64-bit dedicated server was used.

Retail was run with `fs_cdpath` pointing at a copy of the four retail paks
and `fs_basepath` at a scratch folder, both given as short paths, and wrote
only there. Three facts shaped that setup:

- Retail formats search paths into 128-byte buffers. With longer paths it
  truncates them, finds no paks, falls back to its restricted demo mode and
  stops on `Couldn't load default.cfg`.
- The installation also holds a third-party texture pack,
  `zSTVEFtex.pk3`. With it in the search path the retail server overflows its
  stack about 20 seconds after starting, with no client connected. fnTech3
  and Lilium load it without trouble.
- Retail does not treat 127.0.0.1 as a LAN address. It first asks Raven's
  authorize server about the client, then answers the challenge when its
  authorize wait runs out, about 20 seconds later.

## Results

| Client | Server | Result |
| --- | --- | --- |
| fnTech3 | fnTech3 | The retail handshake completes (`challengeResponse`, then `connectResponse`). The client enters the game, is eliminated by a bot, sees the scoreboard with all three players and an 11 ms ping, and disconnects cleanly |
| Lilium Voyager | fnTech3 | Lilium reads protocol 24 from the challenge reply, switches to its legacy protocol, enters the game and plays with a normal HUD, and disconnects cleanly |
| fnTech3 | Lilium Voyager | Lilium's reply names protocol 26; the client connects with protocol 24, which Lilium admits. The client enters the game and plays, and disconnects cleanly |
| fnTech3 | Retail | Retail answers with a bare `challengeResponse` and a bare `connectResponse`. The client enters the game, plays with a normal HUD, and disconnects cleanly |
| Retail | fnTech3 | The retail client, windowed at 640x480, enters the game on fnTech3's server, draws its own HUD and takes its own screenshots, and quits cleanly |

With `sv_pure 1` every server admits every client tried: fnTech3's and
Lilium's clients on fnTech3's server, fnTech3's client on Lilium's and on
retail's, and retail's client on fnTech3's. Each client reports a pure server
and enters the game; none is dropped as unpure.

## Demos recorded live

Retail refuses to record unless `g_synchronousClients` is set
(`CL_Record_f` `0x40c253`), and it keeps the value a server's systeminfo gives
that cvar. This engine's cvar registration instead resets a value created by
the server's systeminfo to the module's default, and the Holomatch cgame
registers `g_synchronousClients`, so the server's 1 became 0 and recording was
refused on a server that allowed it. It also changes play: with the cvar set,
the retail cgame stops predicting. For Holomatch the client now applies the
server's systeminfo again once the cgame has registered its cvars, and
recording works.

A 20-second recording from a live session plays to the end in both fnTech3
and Lilium Voyager, 382 frames in each, and the two frames captured from
each engine show the same scene.

The client also no longer registers the `icons/record` image, which Elite
Force does not ship.

## Downloads between engines

Each server had one extra pak that no client had: `zzdltest.pk3`, holding the
retail `hm_noon` map as `maps/dltest.bsp` (487,016 bytes), built in the
scratch folder and never placed in the installation. The server ran `dltest`
with `sv_allowDownload 1`; the client connected with `cl_allowDownload 1`.

| Client | Server | Result, with `sv_pure 0` and with `sv_pure 1` |
| --- | --- | --- |
| fnTech3 | fnTech3 | The client downloads the pak and enters the map |
| Lilium Voyager | fnTech3 | The same |
| Retail | fnTech3 | The same; the server logs the download's start and completion, then `ClientBegin` |
| fnTech3 | Lilium Voyager | The same |
| fnTech3 | Retail | The same |

Every downloaded pak was byte-identical to the server's, and every pure
server admitted the client that had just downloaded from it.

Retail's server sends one 2 KB block per message and paces its messages by
the client's `rate` (it does not treat 127.0.0.1 as a LAN address). Sampling
the partial file every half second, the pak reached fnTech3's client, which
sends `rate 25000`, at about 14 KB/s: 35 seconds. The retail client's
default `rate 3000` receives it at about 2.4 KB/s, as the retail client
showed against the retail server. fnTech3's and Lilium's servers send
downloads at their own configured rate.

## Tests

- `stvef_client_module_dispatch_tests` checks that the systeminfo is applied
  again after the cgame's initialisation and before loading ends, and that
  recording still requires `g_synchronousClients`.
- `wolfet_map_load_tests` covers the recording icon for ET and Elite Force.

## Remaining

- Lilium's client on the retail server, which did not connect within 40
  seconds; it covers no fnTech3 boundary.
