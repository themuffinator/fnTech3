# Reject incomplete reliable commands before storing them

The client previously hashed, translated and published a server command before
the outer message parser noticed that its reader had passed the payload end.
For a newer sequence this could replace reliable-command history with partial
text and set the command event bit; command-specific disconnect handling could
also run before rejection. The shared live/demo parser now checks the read bound
immediately after reading the sequence and command string, before those effects.

The same function used signed subtraction to compare incoming and stored
sequence numbers. It now uses the existing full-width `IsNewerCounter` helper,
avoiding signed overflow when crossing INT_MAX/INT_MIN. Equal and older counters
remain ignored. The half-range ambiguous value is also ignored, consistently
with that helper; synthetic boundary tests establish this policy, not a retail
observation at that boundary.

An executable regression extracts the actual production function and instruments
its readers and side effects. Incomplete newer, duplicate and stale commands must
raise ERR_DROP without changing history, hashes, ignore flags, sequence, event
state or disconnect count. Valid, duplicate, stale, signed-wrap and zero-wrap
cases run under both wire-profile selections. The QL raw hash remains based on
the unsanitized wire string while stored percent characters remain sanitized;
the Q3 reader's existing sanitized-input convention is preserved.

The regression fails on the predecessor because the incomplete command is not
rejected locally. It passes after correction on Windows and Linux, including
Linux AddressSanitizer and UndefinedBehaviorSanitizer. The Windows x86 build and
complete suite pass: 110 targets, zero failures and one skip, with 1,308 Python
tests and five skips. Reader faults are instrumented in the extraction test;
this is not a claim of malformed live-packet reproduction.

The corrected Windows x86 client also passes two short retail-server sessions:
Q3 changes q3dm6 to q3dm7 against retail quake3.exe, and QL changes Campgrounds to
Overkill against the retail Linux dedicated server. Both clients and servers
exit zero without forced termination. Engine screenshots show destination world,
weapon and HUD. QL remains in warmup awaiting players. These controls exercise
ordinary reliable traffic across map renewal; they do not establish sustained
counter-wrap interoperability or exhaustive command compatibility.

Runtime probes use isolated homes, legitimate external assets, windowed clients
on private unswitched desktops, disabled input/audio and an unprivileged Linux
retail server. No OS capture, mouse/keyboard control or RCON is used. Raw logs,
screenshots and proprietary inputs remain untracked. The
[structured evidence](2026-09-08-server-command-transaction.json) pins source,
binary, command, fixture and validation identities.

No external implementation was incorporated and no new unrelated issue was
found. Broader malformed strings/messages, fresh-command recovery after a drop,
long sessions and full compatibility remain open.
