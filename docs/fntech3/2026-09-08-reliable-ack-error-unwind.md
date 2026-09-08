# Validate reliable acknowledgements before changing connection state

The [interrupted-download investigation](2026-09-08-download-retry-and-zombie-reconnect.md)
observed a recursive client error after a malformed reliable acknowledgement.
The parser assigned the incoming acknowledgement to clc before checking whether
it was ahead of the locally generated reliable sequence. On rejection, error
cleanup attempted a disconnect using that invalid window. CL_AddReliableCommand
declined to enqueue during error handling, but CL_WritePacket then rejected the
same window again, escalating the original recoverable error.

The validation helper now takes the incoming acknowledgement as a candidate and
commits it only after validation. Live future acknowledgements still cause the
same ERR_DROP, while the last valid window remains available to cleanup. Accepted
values, wrap-safe counters, stale-window clamping and demo acknowledgement
behavior remain intact. This is shared client logic for both profiles.

An executable regression extracts the production parser header, validation,
CL_AddReliableCommand and CL_WritePacket's reliable-window check. With the
committed pre-change parser, a rejected future acknowledgement causes a second
error in the cleanup sequence. With the change, exactly one error occurs;
disconnect is queued in the expected slot and its window passes serialization
validation. Additional cases check accepted acknowledgement, exact ring capacity,
stale-ring clamping, both demo policies and signed-counter wrap. The regression
passes on Windows and WSL Ubuntu.

The test replaces error unwinding with an exception so it can inspect state and
exercise the relevant cleanup operations. It does not claim to run every full
Com_Error path or every renderer/module shutdown. The successful native runtime
probes in the full suite establish non-regression of their existing contracts,
not a new full-client malformed-packet recovery transcript.

Windows Sys_Error intentionally shows the fatal client console and waits for
dismissal. That policy remains unchanged; an unswitched private desktop made the
console invisible in the earlier probe. Dedicated fatal errors continue to exit
nonzero without dismissal. The correction avoids this specific unnecessary
escalation rather than removing normal client fatal-error presentation.

The [evidence record](2026-09-08-reliable-ack-error-unwind.json) records source,
binary and test-log identities and the before/after executable regression.
Windows x86 client rebuild passes. The full Windows suite passes 110 checks with
one skip and no failures; its Python aggregate runs 1,284 tests with five skips.
No external implementation was incorporated.
No new input control, game screenshot or proprietary fixture was required.
Broader error-unwind/runtime-malformed-message coverage remains open. The prior
temporary-hunk warnings during recursive shutdown are not independently fixed
or explained here; no general renderer-shutdown correction is claimed.
