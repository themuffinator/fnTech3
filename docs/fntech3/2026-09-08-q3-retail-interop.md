# Retail Q3 bidirectional interoperability

On 2026-09-08, Windows x86 fnTech3 binaries at `2a9b0f8` passed short,
pure-enabled protocol-68 loopback sessions against the installed Steam retail
Quake III Arena executable in both directions. This is independent retail-engine
evidence beyond earlier FnQuake3 peer sessions. The
[machine-readable record](2026-09-08-q3-retail-interop.json) identifies binaries,
stock pak hashes, commands, configs, console hashes and outcomes.

## Method and observations

The legitimate retail executable ran directly from its installed location,
with separate temporary working directories and writable homes. Fixtures linked
nine stock paks; installations were left unchanged. Steam was running. Dedicated
startup succeeded with both the normal inherited environment and explicit public
Steam app identity/path context; peer sessions used the normal environment.
The earlier copied-executable `Failed to find Steam` result remains valid for
that fixture but is no longer the current blocker. Its precise cause was not
isolated.

Clients ran windowed on private, unswitched Windows desktops with mouse,
joystick and audio disabled. Startup configs supplied commands; no keyboard or
mouse control, OS capture, or RCON was used. Screenshots came from engine render
targets. Retail assets, executable bytes, screenshots and raw logs remain outside
tracked files.

| Direction | Result | Pair elapsed time |
| --- | --- | --- |
| fnTech3 client to retail dedicated server | Pure admission, CS_ACTIVE, ClientBegin, engine PNG with world, weapon and HUD; both exit 0 normally | 14.719 s |
| Retail client to fnTech3 dedicated server | Pure admission, CS_ACTIVE, ClientBegin, engine TGA with world, weapon and HUD; both exit 0 normally | 35.312 s |

The first forward probe used `wait 700; quit` on the server and loaded the
gamestate without establishing active play before shutdown. A diagnostic repeat
reached CS_ACTIVE and exchanged snapshots, correcting the initial admission-failure
interpretation, but lacked the requested screenshot before server shutdown.
Both incomplete runs required harness termination of the remaining client at
45 seconds. They are not passes or engine crashes.

A forward repeat with `wait 2400; quit` and a 150-second harness deadline
completed normally with the requested screenshot. No engine change was made.
Wait counts are engine frames, not seconds. This supports a probe lifetime
issue but does not establish an invariant timing guarantee.

## Limits and diagnostics

This establishes short, local retail Q3 interoperability with stock q3dm6 and
one connected client. Remote-network, packet-loss, long-session, mod, download,
broader renderer and QL retail/Steam-authentication gates remain open.
No engine source changed, so the full suite was not repeated; the preceding
[baseline checkpoint](2026-09-08-glibc-baseline-build.md) records the build and
suite results for these sources. Release-document staging and evidence links
were validated for this documentation update.

Unrelated diagnostics remain: missing optional shader light images, mixed image
flags, and repeated retail audio `not started or muted` messages with audio
intentionally disabled. Neither screenshot establishes audio functionality or
pixel parity.
