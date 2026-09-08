# QL client joins a running FFA match

At clean commit `93beb7b`, the current Windows x86 fnTech3 client joins a local
campgrounds/ffa match served by the matching dedicated binary, using legitimate
retail QL modules and assets. Four bots participate. The engine screenshot
shows the first-person world, weapon, health/armor/ammo HUD, running match timer
and score display. Server status samples show `IN_PROGRESS`, all five players
and Visor's score increasing from zero to one. Both engine processes exit zero
without forced termination after 42.265 seconds. The
[record](2026-09-08-ql-active-match.json) retains identities, commands, sanitized
configuration, selected logs, status samples and all preliminary attempts.

Both executable hashes match the binaries validated by the
[key-mask correction](2026-09-08-retail-usercmd-key-mask.md) and
[post-correction runtime checkpoint](2026-09-08-post-codec-runtime.md).
The client makes one split-response HTTP request, downloads the exact expected
fixture archive and passes pure admission before entering active state. Its
startup `activeAction` issues `team f` through the engine command queue. The
server log records receipt of that command. No keyboard or mouse is controlled,
injected, captured or used to join the match.

## Probe corrections and unsuccessful attempts

The previous checkpoint stopped at QL's join panel. Extending that helper
exposed several investigation constraints before a successful match observation:

| Attempt | Observed result | Change for the next attempt |
| --- | --- | --- |
| Long server-side `wait` | `COUNT_DOWN`; screenshot shows Connection Interrupted | Remove the long queue hold and use private RCON for normal shutdown |
| Free-running startup | Retail configuration selects overkill; expected campgrounds readiness fails and client is not launched | Select campgrounds/ffa after startup settles |
| Immediate readiness polling | Bot RCON setup is not admitted; query rate-limit diagnostics appear; one-player match forfeits and rotates | Pace readiness polling and setup commands |
| Paced setup with one bot | Sarge is admitted, but the game forfeits before the client finishes downloading; screenshot shows match summary | Admit four bots before connecting the client |
| Paced setup with four bots | Client joins FFA; live player HUD, five participants, bot score progression and normal shutdown | Record the successful bounded observation |

The long wait was an unsuitable way to keep this server alive because it holds
queued game commands; the first observation alone does not isolate every cause
of the interrupted-client display. The readiness flood also competed with RCON
through the server's shared per-address rate limit. The final helper lets
startup drain, issues a post-startup map command, paces readiness/setup, admits
Sarge, Grunt, Visor and Major, and then connects the client. Its normal shutdown
uses `quit` over RCON to its own loopback server. No engine behavior was changed
to compensate for these probe mistakes.

## Scope and safety

The client runs windowed at 640 by 480 on a private desktop that is never
switched to the user's input desktop. Mouse/joystick input and audio are
disabled. Steam integration and WebUI are disabled; no online-service success
is simulated. The local game and HTTP endpoints use loopback, installations
remain read-only and writable homes are temporary. The random temporary RCON
password is redacted from committed configuration. The screenshot comes from
the engine's `screenshotPNG` command; its raw retail-asset image stays ignored.

This is a short active-match self-pair using the same shared engine distribution,
not bidirectional interoperability with an independent retail engine. It proves
more than reaching the join panel, but does not exercise keyboard/mouse movement,
aiming, firing, long-session stability or all match modes. Bot score progression
is observed; human-controlled combat is not claimed. The earlier unsuccessful
runs are not counted as gameplay passes.

This checkpoint changes evidence documentation only. Existing automated build
and codec-suite results remain applicable; no new full-suite run is claimed.
No unrelated engine defect was established. Required compatibility gates remain
open.
