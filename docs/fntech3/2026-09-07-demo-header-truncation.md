# Partial demo header diagnostics — 2026-09-07

`CL_ReadDemoMessage` previously treated a partial eight-byte record header
exactly like a trailer or EOF between records. A recording cut within its
header therefore completed silently, whereas a partial payload printed
`Demo file was truncated.` The shared reader now prints that same diagnostic
when one through seven header bytes are available. It preserves completion,
the historical tolerance for EOF between records, and the length-only `-1`
trailer convention. No wire or module contract changes.

Validation at `ae91465` plus this patch:

- An executable harness runs the production `CL_ReadDemoMessage` with the
  production envelope reader. Every partial header length, partial payload,
  negative/oversized length, trailer, missing handle, boundary EOF and valid
  envelope is covered. Invalid/incomplete records never reach message parsing
  or update sequence/time state. The harness passes on Windows and Linux.
- All nine focused Python demo tests and the C++ demo-stream tests pass.
- The Windows MSVC x86 client rebuild passes.
- Six windowed runtime probes use the same client: partial header, partial
  payload and boundary EOF under each profile. Both truncation cases print
  the warning; boundary EOF does not. All six reach their completion callback
  and exit normally. These are synthetic envelope-only inputs, not gameplay
  or malformed message-body coverage.

The [runtime record](2026-09-07-demo-header-truncation.json) contains
the executable hash, complete commands/configs, exact synthetic bytes and
outcomes. Legitimate installations supply startup resources; each run uses a
separate writable home outside the repository, removed after logs are saved.
Mouse/joystick capture, Steam integration and WebUI are disabled. No user input
was injected or proprietary data committed. No new unrelated issue was found.

Broader malformed message-body, demo format and interoperability gates remain
open. Completion callbacks are still emitted for truncation, so a callback
alone must not be counted as successful complete-demo playback.
