# IHV Test sessions under packet loss, and the client's level change, 2026-10-08

The IHV Test's sessions had run only on clean loopback, and sustained play was
open. These runs keep both clients in one game for three minutes while packets
are dropped, and change level under loss. They found one fault in fnTech3's
client, now fixed: after a level change it sent no move for about as long as
it had played the previous level.

The [JSON companion](2026-10-08-q3-ihv-loss-and-level-change.json) holds
identities, commands and replies.

## The level-change stall

**Observed.** On fnTech3's server at 25% loss, `rcon map km_portal` at 90 s
reached both clients. The original client began the new level at once.
fnTech3's client loaded the map at once, then sent no move. `status` still
showed it primed 60 s later, and the server took its `Begin` after more than a
minute. Against the original server, at 25% loss on the client only, the same
client loaded `km_portal` at once after the change at 70 s, and entered it only
40 to 70 s later; its `status` ping stayed at its last value meanwhile. The
stall followed the time spent on the level before, not the loss.

**Cause.** A move goes out once two commands are newer than the one an earlier
packet carried (`CL_SendCmd` `0x403360`). The original's `CL_ParseGamestate`
(`0x406c40`) calls `CL_ClearState` (`0x404ac0`), which zeroes
`0x430fc60`..`0x4369c70`. That range holds the command count (`0x4369b58`) and
the ring of sent packets (times at `0x4369b5c`, commands at `0x4369bdc`), so
both start again from zero.

fnTech3's client cleared its command history with the gamestate but kept its
packet ring (`ihvLive.out`). Every new command number was smaller than the old
ones, so no move was sent until the new count passed the old.

**Fix.** The gamestate now clears the ring and the recorded engine command
with the rest of the client state (`code/client/cl_q3ihv.cpp`).
`tests/q3_ihv_client_tests.py` checks that order.

## Runs

All runs were on loopback, windowed, on private desktops, with homes or
writable copies under `.tmp`. They were driven by console commands and
connectionless `rcon`, without input. `net_dropsim` drops that share of the
packets an fnTech3 process receives. The original has no such setting, so loss
toward the original client is not simulated.

| Run | Loss | Result |
| --- | --- | --- |
| fnTech3 server, original and fnTech3 clients, `tim_dm1`, 85 s | 10% at the server and at fnTech3's client | both in the game together; `kill` reaches the game through the reliable channel; fnTech3's lagometer marks the drops |
| the same, 205 s, `map km_portal` at 90 s | 25% at both | both rejoin; fnTech3's client stays primed for about a minute (before the fix) |
| the same, `map km_portal` sent four times from 90 s | 10% at the server, 25% at fnTech3's client | three level changes arrive; after each, both clients begin at once; scores rise as players spawn onto each other (after the fix) |
| original server, fnTech3 client, `map km_portal` at 70 s | 25% at the client | before the fix it enters 40 to 70 s after the change; after it, at once, ping 47 by 82 s |
| original server, original client, `map km_portal` at 50 s | none | reference: ping 999 at 6 and 12 s after the change, then 0 to 25 |

**Ping after a level change.** Both servers report 999 for the original client
for a while after a level change. The ping is a mean over 32 frame records,
which outlive the level while the server's time starts again. fnTech3's server
reads `SV_CalcPings` (`0x41a910`) the same way, so its 999 matches the
original's, and the value recovers once the client has acknowledged fresh
frames.

**Not changed.** The original's `status` lists `num score ping name lastmsg
address qport`. fnTech3's server keeps its own `status` layout. That is console
diagnostics, not a wire contract.

## Open

No player fell to another's weapon: the clients are given no input, and the
build's bots do not fight. Fought play, remote peers and loss toward the
original client remain open. The profile stays a Windows x86 developer path,
and release builds still refuse it.
