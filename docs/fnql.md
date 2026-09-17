# fnTech3 Documentation

fnTech3 is a unified, engine-only source port for Quake III Arena, Quake Live,
and Return to Castle Wolfenstein single player and multiplayer. The goal is one
distribution with shared client and dedicated-server binaries and four
profile-specific compatibility contracts: `q3`, `ql`, `rtcw-sp`, and `rtcw-mp`.

Start with the [README](../README.md#games-and-profiles) for game availability
and startup examples, and the [compatibility ledger](fntech3/COMPATIBILITY.md)
for the unified goal and evidence. Q3/QL are enabled; RTCW requires an explicit
development build option and is gated by default. No profile has full
compatibility sign-off.

This index retains its legacy filename. Documents under `docs/fnql/` preserve
QL subsystem contracts and inherited FnQL history; their observations do not
establish compatibility for another profile or supersede the fnTech3 ledger.

## Player Guides

- [Display](DISPLAY.md) — renderers, video modes, HDR, render scaling,
  anti-aliasing, lighting, bloom, fog, liquids, and post-processing.
- [RTX Renderer](RTX.md) — requirements, selection, fallback policy, and
  troubleshooting for the Vulkan ray-tracing renderer.
- [Visuals](VISUALS.md) — player highlighting, colours, cel shading, and
  outlines.
- [Aspect Handling](ASPECT_CORRECTION.md) — retail WebUI, menus, and
  cinematics.
- [Audio](AUDIO.md) — OpenAL, HRTF, surround sound, spatial effects, device
  selection, and fallback behavior.
- [Console](CONSOLE.md) — scalable text, scrolling, completion, clipboard, and
  mouse interaction.
- [Screenshots](SCREENSHOTS.md) — capture formats, filenames, metadata,
  levelshots, watermarks, and cubemaps.

## Building And Maintainer Documentation

- [Build Guide](../BUILD.md) — supported build systems, toolchains, options,
  platform notes, and the RTCW development option.
- [Project mission and contributor rules](../AGENTS.md) — shared-engine
  invariants and non-regression requirements for all profiles.
- [Compatibility ledger](fntech3/COMPATIBILITY.md) — current gates for Q3,
  QL, RTCW SP, and RTCW MP, plus shared-release sign-off requirements.
- [RTCW contracts](fntech3/2026-09-14-rtcw-contracts.md) — game-manager
  foundation, profile boundaries, and same-executable mode switching.
- [FnQL baseline technical notes](fnql/TECHNICAL.md) — inherited repository
  structure, subsystem contracts, and historical workflow.
- [FnQL compatibility roadmap](fnql/QL_COMPATIBILITY_ROADMAP.md) — QL baseline
  subsystem history and retail validation boundaries.

fnTech3 does not ship retail assets or reconstructed `qagame`, `cgame`, or UI
game code for any game. Supply the legitimate installation for each selected
profile; retail modules, assets, and proprietary runtimes remain there.
