# Demo drop errors cancel queued recovery commands

The current packaged Windows x86 client rejects invalid demo lengths, but queued
console scripts cannot establish recovery after that error. The production
`Com_Error` path calls `Cbuf_Init` before disconnecting and unwinding an `ERR_DROP`.
This discards pending commands, including a script intended to start a valid
recording. The pinned FnQuake3 baseline also contains this command-buffer reset.
No engine change is justified by these probe results.

The investigation used the unchanged e114ded package with isolated homes,
legitimate external installations and existing local Q3/QL demo fixtures. Four
fault lengths were appended in place of the original demo trailer: -2,
INT_MIN, 16385 and INT_MAX. The original payload records were retained.

The first harness scheduled recovery after 300 frames. All four Q3 runs reached
the rejection, stayed alive and required explicit harness termination at the
30-second bound. Its four QL runs started the valid recording before reaching
the malformed record and exited normally. Those QL runs are invalid as
post-error recovery evidence, despite their completion markers.

A revised harness executes a polling config and replaces its contents with the
recovery command only after observing the expected error. All four completed Q3
cases again rejected the length and never ran recovery. One QL -2 case confirmed
the same behavior. These five clients remained alive until the 30-second harness
bound and were explicitly terminated. An in-flight redundant QL run from the
initial synchronized batch was cancelled; it is excluded from completed results.

The synchronized controls use diagnostic front-buffer presentation and explicit
frame caps, separating command cancellation from background rendering delays.
All clients are windowed, input-disabled and run on private unswitched desktops
with sound disabled. Commands originate in startup/polling configs; no keyboard,
mouse, RCON or OS capture is used. All 304 package files remain unchanged.

These are failed recovery attempts, not passing recovery tests or evidence of a
crash. The harness assertions intentionally remained unsatisfied. The QL
single-case harness also retains its original eight-row aggregate assertion;
the per-case error observation is the evidence used here. Raw console logs and
retail-derived inputs remain untracked. The
[structured record](2026-09-08-demo-drop-recovery-limit.json) preserves commands,
fixture and binary identities, outcomes and log hashes.

Recovery through a fresh command submitted after error handling remains open.
It requires an appropriate authorized runtime interaction or separate test
instrumentation; weakening command cancellation or adding a production recovery
hook solely for automation would change the behavior being tested. The earlier
truncated-record nextdemo recovery evidence remains distinct: tolerated
truncation follows completion, whereas invalid lengths raise ERR_DROP.

No new unrelated engine issue was found. Full demo compatibility, seeking and
broader malformed-message coverage remain unproven.
