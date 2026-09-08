# GLx/Vulkan switch and restart runtime checkpoint

The artifacts from the [Vulkan descriptor correction](2026-09-08-vulkan-descriptor-restart.md)
now pass bounded renderer-switch sequences with legitimate retail modules and
assets. This checkpoint changes no engine code and does not rerun the automated
suite; it adds runtime evidence beyond individual renderer startup/restart.

| Platform/profile | Sequence after pure admission | Observed result |
| --- | --- | --- |
| Linux i686 X11, Q3 | GLx → Vulkan fast → GLx fast → Vulkan full → GLx full | Five engine screenshots, continued snapshots, normal client/server exits |
| Windows x86, Q3 | GLx → Vulkan fast → Vulkan fast → GLx full → GLx fast | Five engine screenshots, continued snapshots, normal client/server exits |
| Windows x86, QL | Same Windows sequence | Five engine screenshots at the native join UI/world, continued snapshots, normal client/server exits |

Every screenshot was inspected. The Q3 world and HUD and QL join UI/world remain
readable across the transitions. The QL probe does not enter a match; active
network state and native join UI rendering are the evidence claimed here.

Requested mode and actual shutdown scope differ. `CL_ShutdownRef` promotes any
changed renderer module to `REF_UNLOAD_DLL` (3); Linux and Windows logs confirm
this even for cross-renderer `fast` requests. The Windows same-renderer `fast`
steps report `REF_KEEP_WINDOW` (1). That path is designed to retain the window,
but this checkpoint does not measure native window-handle identity. Cross-module
window preservation is not claimed.

All clients force windowed mode and disable input grabbing. Linux runs under
private Xvfb with Mesa software GL/Vulkan; Windows uses private desktops that
are never switched onto the user's display. Writable homes are temporary and
isolated. Screenshots come from the engine, and neither retail inputs nor
screenshots are committed. The [evidence record](evidence/2026-09-08-renderer-switch.json)
pins commands, configurations, profiles, artifact hashes, fixture identities,
transition traces and log/screenshot hashes. The modified Linux checkout's
source lineage remains explicit; it is not a fresh clean build.

This expands GLx/Vulkan lifecycle evidence. RTX transitions, SDL cross-renderer
switches, device loss, long-session stress, HDR, audio interaction, full visual
parity and other merger gates remain open. Existing image/font warnings and
hosted CI's account billing/spending blocker remain unresolved; this round found
no additional unrelated defect.
