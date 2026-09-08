# Connected retail QL map restart

On 2026-09-08, one authenticated Campgrounds warmup map_restart passed in each
retail interoperability direction: Windows x86 fnTech3 client to the unmodified
Linux x86 retail dedicated server under WSL, and installed Windows retail QL
client to fnTech3's Windows x86 dedicated server. Both clients rendered the
world, weapon and HUD after the restart and exited normally. All four processes
exited 0 without forced termination.

The [machine-readable record](2026-09-08-retail-ql-map-restart.json) pins source
baseline `b6306fd`, binaries, configs, process outcomes and image/log hashes.
The installations and provider identities are the previously established
[retail admission](2026-09-08-ql-retail-auth-interop.md) and
[dedicated-server](2026-09-08-ql-dedicated-retail-interop.md) fixtures.

## Synchronization and observations

An initial forward probe scheduled restart using a fixed wait after server
startup. The client connected only after that restart and did not capture its
second image before the 45-second deadline. The server exited normally and the
remaining client was harness-terminated. This is retained as a failed probe
setup, not evidence of a connected restart failure or success.

The corrected server executes a short polling config from its isolated home.
After the harness observes the accepted `team f` client command, it replaces
that local config with a delayed sequence: restart-begin marker, map_restart 0,
restart-end marker, a further wait, and quit. This synchronizes the restart with
actual admission while allowing the client's startup screenshot sequence to run.
It uses neither RCON nor keyboard/mouse injection.

In the forward run, fnTech3 receives the server's map_restart command and has
one connectResponse across the session. The retail server logs two transitions
to CS_ACTIVE, one BeginAuthSession accepted with result 0, and one EndAuthSession
on final disconnect. The post-restart engine screenshot shows the new warmup
spawn with first-person weapon and HUD.

In the reverse run, fnTech3 logs two game initializations and both restart
markers. The retail client has one connectResponse; the server has one platform
authentication validation. The retail post-restart engine screenshot likewise
shows the new spawn. The pair completes normally in 36.562 seconds. This
supports continuity of the existing authenticated connection; it does not claim
that restart requires a fresh ticket or that every authentication callback path
has been exercised.

## Safety, validation and remaining gates

Clients were windowed on private unswitched desktops with input and audio
disabled. Servers and clients used isolated writable homes; legitimate assets
and runtimes remained external inputs. The Linux server ran unprivileged, bound
to its private WSL address. Screenshots came from engine render targets; retail
TGA conversion was only for inspection. No OS capture, input control or messages
to other people were used. Raw logs, account identities, tickets, assets and
screenshots remain untracked.

No engine code changed. The existing
[Windows suite and focused Windows/Linux regression](2026-09-08-paused-command-history.md)
remain the source validation record. This documentation update passed release
document staging and link validation. These two short warmup restarts do not
close different-map changes, scored match transitions, packet-loss cases,
long/repeated restart stress or broader platform coverage.

Unrelated diagnostics remain: the dedicated server reports a failed user-stat
request (response 2), and the retail client reports its website-loading failure.
Neither prevented the observed game restart; neither is reported as successful
optional-service behavior. Audio remained disabled.
