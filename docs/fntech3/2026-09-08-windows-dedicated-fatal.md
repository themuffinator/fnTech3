# Windows dedicated fatal errors terminate without dismissal

The preceding pak-identity probe exposed an unattended-server failure: Windows
`Sys_Error` entered the GUI console message loop even in a dedicated build.
An invalid startup installation therefore left the server process alive until
the probe's timeout, preventing a supervisor from observing failure or restarting
it. This also affected fatal errors after game initialization.

Dedicated builds now print the formatted error to stderr, flush it, release the
timer request, clear the exception filter, destroy the console and exit with
status 1. The client retains its existing error console and dismissal loop.
This independently authored platform change does not alter profile contracts
or the successful quit path. Early errors do not require initialized filesystems
or game modules. Game/server cleanup for initialized fatal errors continues
through the existing `Com_Error` path before `Sys_Error` terminates the process.

The executable regression extracts the production function, verifies formatted
diagnostics and cleanup, and exercises dedicated and client branches. It fails
against the prior dedicated implementation when that branch enters `GetMessage`;
the changed function passes on Windows and Linux test hosts. The Linux run is a
portable harness check of Windows logic, not a native Windows API runtime claim.

The rebuilt Windows x86 dedicated binary was probed with isolated writable homes,
read-only legitimate installations, loopback networking and windowed settings.
No input injection or screen capture was used:

- A one-byte base pak and an unknown profile each terminate with status 1 and
  their specific stderr diagnostic, before filesystem startup.
- Q3 qagame QVM/q3dm6 and QL native API-10 qagame/campgrounds initialize their
  maps and AAS, then execute the registered developer `error` command. Both
  log game/AAS shutdown and terminate with status 1 and `Testing fatal error`.
- Automatic and explicit Q3/QL startup still pass module/map and protocol 68/91
  response checks and exit normally (four runs).

The complete configured Windows suite passes 110 checks with zero failures and
one unavailable OpenAL fixture skip; its source aggregate passes 1,262 tests
with seven skips.

The [evidence manifest](2026-09-08-windows-dedicated-fatal.json) records source
and executable hashes, commands, installation identity and outcomes. This is an
incremental MSVC x86 build. It closes the observed dedicated error-console wait;
it does not establish long-running server reliability, crash-handler correctness,
retail peer interoperability or release compatibility. The client's interactive
fatal behavior is intentionally retained and covered by the harness.
