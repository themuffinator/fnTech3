# Report Unix fatal shutdown as failure

The [native Linux runtime probe](2026-09-08-linux-client-runtime.md) found a
GLX shutdown signal fault that nevertheless returned process status zero.
`fatal_signal_handler` explicitly called `Sys_Exit(0)`. Separately, the debug
implementation of `Sys_Exit` asserted that its code was zero, so simply passing
a failure code could raise another fatal signal during cleanup.

Fatal signal handling now passes status 1. Debug error exits use `_exit(code)`
after console cleanup, preserving the failure without asserting or running
potentially unsafe exit handlers. Normal debug exits still call `exit(0)`;
the release `_exit` path and requested SIGTERM/SIGINT shutdown behavior are
unchanged. This independently authored correction fixes reporting, not the
underlying GLX crash or signal-handler async-safety.

`tests/unix_signal_exit_tests.py` executes the production fatal/requested
handlers and exit function with controlled cleanup in fresh processes. Before
the fix it reproduced status zero for a fatal handler. Afterward, Windows and
Linux pass all debug/release and client/dedicated combinations for fatal,
double-fault, requested, idle and explicit error paths. The 21 existing platform
port-safety tests also pass on Windows.

A native i686 X11 client rebuild applies only these two source corrections
to the earlier `b5776c9` checkout; it is no longer claimed as that clean build.
The repeated windowed private-Xvfb probe still encounters the Mesa GLX shutdown
fault. Its failure status is checked separately from successful pure admission
and screenshot output. The [evidence record](evidence/2026-09-08-unix-fatal-exit.json)
pins the modified source/artifact hashes and runtime logs.

A debugger run of the original fault stops inside
`/lib/i386-linux-gnu/libGLX_mesa.so.0`; its backtrace cannot unwind past that
frame. This is insufficient to assign the fault to a particular engine call
or to the driver. SDL texture corruption, the GLX shutdown root cause, existing
image/font warnings and hosted CI's billing/spending blocker remain open.
