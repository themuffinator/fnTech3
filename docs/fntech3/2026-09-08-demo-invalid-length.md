# Invalid demo-length runtime rejection

The same Windows x86 client at clean source commit a5a74f6 rejects four invalid
demo-envelope lengths for each profile: -2, INT32_MIN, 16385 and INT32_MAX.
The production message capacity is MAX_MSGLEN = 16384, so 16385 tests the first
oversized value. Each input retains a valid locally recorded dm_68 or dm_91
prefix and replaces its terminal trailer with the malformed envelope header.
No payload is supplied for that header. The [evidence record](2026-09-08-demo-invalid-length.json)
identifies source, executable, input hashes, exact commands and raw-log hashes.

All eight runs produce exactly one matching invalid-length error, without a
truncation warning or normal-demo-completion marker. The error path returns to
UI initialization and each process remains alive for at least two seconds after
the error appears. The harness then explicitly terminates its own isolated
process, producing exit code 1. These are bounded rejection observations, not
normal-exit or successful post-error playback claims. Source inspection shows
ERR_DROP clears queued commands and unloads/reinitializes client resources;
cl_quitOnDemoCompleted does not apply to this rejected-input path.

The probes use legitimate installations read-only, temporary writable homes,
windowed clients on private unswitched desktops, disabled mouse/joystick input,
and disabled optional services. No user input control or OS capture occurs.
Proprietary recording bytes and raw runtime output remain ignored. No engine
change or external implementation was needed. The prior full automated suite
remains scoped to the [completion handoff correction](2026-09-08-demo-completion-handoff.md);
it was not repeated for this evidence-only addition.

Longer post-error stability, interactive recovery, every claimed legacy format,
seeking and independent retail-engine demo exchange remain open. Existing font
fallback and QL temporary-hunk diagnostics remain outside this probe. No gate is
promoted to full compatibility.
