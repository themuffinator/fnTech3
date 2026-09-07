# Shared x86 renderer restart — 2026-09-07

At `04314c9`, the same Windows MSVC x86 client and dedicated server completed
six local pure sessions: Q3 and QL with each of GLx, Vulkan and RTX raster.
Every client executed one full `vid_restart` after connecting, then rendered
again and exited normally. No engine implementation change was needed.

| Profile | Renderer | Snapshots after restart command | Client/server exit |
| --- | --- | --- | --- |
| Q3 | GLx | 81 | 0 / 0 |
| QL | GLx | 152 | 0 / 0 |
| Q3 | Vulkan | 76 | 0 / 0 |
| QL | Vulkan | 149 | 0 / 0 |
| Q3 | RTX raster | 60 | 0 / 0 |
| QL | RTX raster | 130 | 0 / 0 |

Each server logged one active-state transition, the `vdr` reset and a second
`cp` pure proof. Each client logged one connection response, renderer teardown
and initialization between the restart markers, and subsequent snapshots.
Both before/after engine screenshots were written in every session. Visual
inspection of all six post-restart PNGs showed the Q3 world/HUD or QL
world/native join screen. These observations establish recovery, not exact
image parity across independently spawned viewpoints or elapsed game time.
RTX logs explicitly report ray tracing disabled before and after restart.

The [evidence record](evidence/2026-09-07-renderer-restart.json) includes
executable/renderer hashes, full command vectors, console configs, fixture
identity, log excerpts and screenshot hashes. Q3 used the isolated retail
pak0–pak8 fixture; QL used its legitimate installation. Each run had separate
writable homes and an unswitched private desktop. Windows stayed windowed;
mouse/joystick capture, audio, WebUI, Steam integration and Workshop were
disabled. No input injection or OS screenshots were used. Proprietary inputs
and render-target images remain outside version control.

One full restart per combination does not close sustained restart stress,
fast/keep-window restart, renderer switching, resize/device loss, WebUI or
audio lifecycle gates. The shared x86 native-RT gate also remains open.
Unrelated existing missing compiler-light-image, font fallback and mixed-image-
flag diagnostics remain visible; these probes do not resolve those asset issues.
