# Preserve the game context across SDL console rendering

The native Linux SDL texture corruption reproduces with both embedded and
dynamically loaded GLx, ruling out static linkage as its cause. A temporary
texture-upload probe then observes `GL_UNPACK_ROW_LENGTH=140` and alignment 1
while uploading tightly packed engine textures, including the 16-by-16 default
image. The expected row length is zero (use each upload's width).

The Unix console uses SDL's accelerated renderer. Its rendering path changes
the current OpenGL context without restoring the caller's context. Console
output during engine initialization therefore redirects subsequent GL work
into the console context, which carries SDL's texture upload state. Changing
the engine's unpack settings alone would not correct context ownership.

An independently authored scope guard now restores the previous SDL window and
GL context after console creation, drawing and destruction, including early
returns and callers with no current GL context. Destruction discards a saved
context belonging to the console window being destroyed. A restoration failure
is fatal rather than allowing engine GL work to continue in the wrong context.
Console acceleration is preserved.

With this correction, the diagnostic uploads report row length zero and
alignment 4. The inspected engine screenshot has readable world textures and
HUD elements. A subsequent embedded GLx build removes the temporary upload
instrumentation and exercises Q3 pure admission, snapshots, an engine-command
`vid_restart`, screenshots before/after restart and normal shutdown. The
[evidence record](2026-09-08-sdl-console-context.json) pins the artifacts,
source changes, commands, fixture identity, observations and log hashes.

The regression test executes production `SysCon_Render` against controlled SDL
dependencies. The pre-fix rendering body leaves the console context current;
the corrected body preserves the caller across repeated renders and early
returns. Additional guard cases cover creation, destruction, no-context callers
and failed restoration. Focused tests pass on Windows and Linux, along with
the 21 existing Windows platform-safety tests. The full configured Linux SDL
suite passes 84 checks with zero failures and one unavailable retail-factory
fixture skip; its aggregate source suite passes 1,255 tests with 15 skips.

These are modified builds based on the earlier `b5776c9` checkout, with the
recorded Unix exit/X11 lifetime fixes and this console correction. They are not
new clean-checkout builds. Probes use private Xvfb, windowed mode, isolated
writable homes, disabled mouse/joystick grabbing and engine screenshots. Retail
assets and screenshots remain local and uncommitted.

This resolves the reproduced Linux SDL texture corruption; it does not establish
full visual parity, native Vulkan runtime, every driver or long-session
behavior. Native Linux QL client modules are absent from the legitimate local
installation, so runtime coverage here is Q3; the console correction is shared
platform code. Broader compatibility gates, existing image/font warnings and
hosted CI's account billing/spending blocker remain open.
