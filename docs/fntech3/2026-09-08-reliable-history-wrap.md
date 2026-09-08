# Preserve reliable history across signed counter wrap

The client reliable-command audit found two additional signed-arithmetic paths
after the parser correction. The demo writer could skip pending commands when
the counter crossed INT_MAX/INT_MIN, and its inclusive signed loop could
overflow at INT_MAX. Module history lookup also subtracted signed counters when
classifying retained, overwritten and future requests.

The writer now obtains the forward distance with the existing full-width counter
helpers, caps it at MAX_RELIABLE_COMMANDS and iterates a bounded unsigned count.
Each output sequence is derived from the current counter. It preserves oldest
to newest ordering and writes at most 64 retained commands, including across
signed and zero wrap. Equal, stale and half-range ambiguous positions emit no
commands; the writer cursor still catches up to the current sequence afterward.

Module lookup uses the existing PendingCounterCount helper before indexing the
ring. Ages 0 through 63 remain available, while age 64 is overwritten. Older
requests retain the existing live-error/demo-false distinction. Future and
half-range ambiguous requests fail before reading a slot. Module ABI layouts,
trap ordinals, string encoding and reliable-ring capacity are unchanged.

An executable test extracts the production writer and module lookup checks.
Both profile selections cover five current-counter boundaries, pending counts
through 100, the 64-command cap, exact output ordering, stale/future cursors,
live/demo lookup policy and half-range ambiguity. The predecessor fails by
skipping expected commands across wrap. The correction passes Windows execution
and Linux AddressSanitizer/UndefinedBehaviorSanitizer execution.

The Windows x86 build and full suite pass: 110 targets, zero failures and one
skip, including 1,309 Python tests with five skips. The corrected shared client
also re-records existing local Q3 and retail-derived QL demos during playback,
then replays those new recordings to completion. The new Q3 recording contains
43 messages and 3,851 bytes; QL contains 179 messages and 8,375 bytes. Both retain
valid trailers. All four client processes exit zero without forced termination,
parser errors or truncation warnings. No screenshot/render-quality claim is
made for this playback control.

Runtime probes use legitimate external installations, isolated writable homes,
windowed clients on private unswitched desktops and disabled input/audio. No
keyboard/mouse control, RCON or OS capture is used. Raw demos and console logs
remain untracked. The [structured evidence](2026-09-08-reliable-history-wrap.json)
records source, binary, fixture, command and validation identities.

Synthetic boundary tests establish arithmetic behavior; the short runtime
controls do not prove live interoperability at counter wrap. Broader demo
formats, module semantics, long sessions and complete compatibility remain open.
No external implementation was incorporated and no new unrelated issue was found.
