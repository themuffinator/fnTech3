# Native Linux client runtime: open defects

The clean `b5776c9` Linux client binaries were exercised against the corresponding
native dedicated server using legitimate Q3 assets. Ubuntu Xvfb was installed
for a private virtual display; `xvfb-run -a` used an 800x600x24 virtual screen,
no TCP listener and software OpenGL. Each engine forced windowed mode, disabled
mouse/joystick capture and grabbing, disabled audio/Steam/Workshop/WebUI, and
used a fresh writable home. Screenshots were produced by `screenshotPNG`,
never an operating-system capture. No user input was controlled.

All three probes reached active state after pure admission and continued
receiving snapshots. That does not establish a visual or lifecycle pass:

- **Bundled SDL with embedded GLx:** world and HUD textures are visibly
  corrupted. Client/server exits are normal. The context reports Mesa llvmpipe
  OpenGL 4.5 and the GL41 executor.
- **Same SDL binary with `r_glxStreamMode orphan`:** the corruption remains.
  This setting does not resolve the defect; it does not identify a root cause.
- **Non-SDL X11 client with dynamically loaded GLx:** the engine screenshot
  shows a readable world and HUD, but shutdown reports signal 11 followed by
  recursive client shutdown. Lighting parity has not been established.

The X11 process reports exit code zero despite the fatal signal. Source
inspection finds that `fatal_signal_handler` in `code/unix/linux_signals.cpp`
calls `Sys_Exit(0)` after cleanup. Therefore the zero status is not accepted as
a successful lifecycle result. The underlying shutdown fault and fatal exit
status behavior both require correction. The SDL and X11 probes differ in
platform backend and renderer linkage, so they do not isolate which difference
causes the texture corruption.

Exact commands, artifacts, log hashes, screenshots hashes and observed results
are in the [evidence record](2026-09-08-linux-client-runtime.json).
The proprietary scene screenshots stay in ignored local probe output.

The available retail QL `bin.pk3` contains Windows cgame/UI modules and Linux
qagame modules, but no Linux cgame/UI modules. Native Linux QL client runtime
cannot be claimed from this installation. The Windows x86 shared-client
contract and the observed Linux QL dedicated-server support remain distinct.

These are new runtime defects/limitations, not successful closure of the
earlier build-only gates. Existing image/font warnings and the hosted CI
account billing/spending blocker remain open.
