# Elite Force Holomatch live sessions and Lilium Voyager interoperability

## Scope

This record covers live Holomatch sessions over loopback UDP: fnTech3's
client against fnTech3's dedicated server, and each against Lilium Voyager
1.40. It also covers recording a demo during a live session and playing that
demo in both engines. The retail `stvoyHM.exe` client and server, pure
admission and downloads are not covered. The profile stays behind
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

## Results

| Client | Server | Result |
| --- | --- | --- |
| fnTech3 | fnTech3 | The retail handshake completes (`challengeResponse`, then `connectResponse`). The client enters the game, is eliminated by a bot, sees the scoreboard with all three players and an 11 ms ping, and disconnects cleanly |
| Lilium Voyager | fnTech3 | Lilium reads protocol 24 from the challenge reply, switches to its legacy protocol, enters the game and plays with a normal HUD, and disconnects cleanly |
| fnTech3 | Lilium Voyager | Lilium's reply names protocol 26; the client connects with protocol 24, which Lilium admits. The client enters the game and plays, and disconnects cleanly |

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

## Tests

- `stvef_client_module_dispatch_tests` checks that the systeminfo is applied
  again after the cgame's initialisation and before loading ends, and that
  recording still requires `g_synchronousClients`.
- `wolfet_map_load_tests` covers the recording icon for ET and Elite Force.

## Remaining

- Sessions with the retail `stvoyHM.exe` client or server. Retail writes its
  configuration and logs into its installation directory, so a run needs a
  separate writable base path.
- Pure admission (`sv_pure 1`) and downloads between engines.
