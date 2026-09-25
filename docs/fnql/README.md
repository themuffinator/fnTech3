# FnQL baseline documents

[FnQL](https://github.com/themuffinator/FnQL) was the Quake Live engine that
fnTech3 merged with FnQuake3. The documents in this folder were written for
it, when Quake Live was the only game. fnTech3 keeps them for three reasons:
some still define how the `quakelive` profile works, some describe renderer
and audio systems that now serve every game, and the rest record FnQL's own
history.

Read them with these limits in mind:

- "FnQL" in these pages means the engine fnTech3 inherited; in shared
  subsystems it now means fnTech3.
- A Quake Live observation here is evidence for the Quake Live profile only.
  It never establishes another profile's behaviour.
- Status claims and release rules here are historical. The
  [compatibility ledger](../fntech3/COMPATIBILITY.md) and
  [RELEASING.md](../fntech3/RELEASING.md) are authoritative for fnTech3.
- Which games a shared feature applies to is stated in the
  [player guides](../README.md#player-guides).

Some filenames and identifiers keep the `fnql` spelling -- the renderer
modules `fnql_<renderer>_<arch>`, `fnql-web.pak`, the `fnql-audiozonesc`
tool, sound shader and console shader names -- because packages, configs and
tests depend on them.

## Quake Live profile references

These remain the reference for the engine side of the `quakelive` profile.

| Document | Subject |
| --- | --- |
| [QL_COMPATIBILITY_ROADMAP.md](QL_COMPATIBILITY_ROADMAP.md) | Inventory and order of the Quake Live engine work: native module API, WebUI, protocol 91 and demos, BSP 47 advertisements, ZMQ and Steamworks. |
| [WEBUI_BACKEND.md](WEBUI_BACKEND.md) | Running the retail WebUI through the installation's Awesomium runtime on Windows x86, and the native-menu fallback elsewhere. |
| [STEAM_PROVIDER.md](STEAM_PROVIDER.md) | The open loader boundary for a separately supplied Steam provider, and fnTech3's policy of not bundling it. [STEAM_PROVIDER_BINARY_NOTICE.txt](STEAM_PROVIDER_BINARY_NOTICE.txt) ships with release packages. |
| [FACTORY_COMPATIBILITY.md](FACTORY_COMPATIBILITY.md) | Factories, cvar overrides, map pools and rotation, dedicated startup. |
| [FONT_COMPATIBILITY.md](FONT_COMPATIBILITY.md) | Quake Live's TrueType faces and host text drawing. |
| [INPUT_COMPATIBILITY.md](INPUT_COMPATIBILITY.md) | Retail Quake Live mouse math, UTF-8 text input and joystick handling. The mouse model, `cl_mouseAccelStyle 2`, is the default in every profile. |
| [CLIENT_CVAR_COMPATIBILITY.md](CLIENT_CVAR_COMPATIBILITY.md), [SERVER_CVAR_COMPATIBILITY.md](SERVER_CVAR_COMPATIBILITY.md), [RENDERER_CVAR_COMPATIBILITY.md](RENDERER_CVAR_COMPATIBILITY.md) | The retail Quake Live client, server and renderer cvar surfaces and who owns each cvar. |

## Shared renderer and audio design notes

These describe systems every profile now uses. Their Quake Live examples are
examples, not limits.

| Document | Subject |
| --- | --- |
| [TECHNICAL.md](TECHNICAL.md) | FnQL's technical notes: collision capsules, release packaging, CI, shadow mapping and the audio backend. fnTech3's own overview is the [architecture overview](../fntech3/ARCHITECTURE.md). |
| [AUDIO_ENGINE.md](AUDIO_ENGINE.md) | The OpenAL and legacy audio backends, source layout and validation. |
| [RTX_RENDERER.md](RTX_RENDERER.md) | The RTX renderer's module contract, defaults, build gates and runtime checks; current work is in the [RTX feature ledger](../fntech3/RTX_Q2RTX_PARITY.md). |
| [GLOBAL_FOG.md](GLOBAL_FOG.md) | The optional per-map `.fog` sidecar format and its fallback rules. |
| [LIQUID_RENDERING.md](LIQUID_RENDERING.md), [UNDERWATER_VIEW.md](UNDERWATER_VIEW.md), [MENU_SOFT_FOCUS.md](MENU_SOFT_FOCUS.md) | The optional liquid surfaces, submerged view and in-game menu blur. |
| [DLIGHT_SHADOWMAP_ROADMAP.md](DLIGHT_SHADOWMAP_ROADMAP.md) | Dynamic-light, spot and sun shadow mapping on GLx and Vulkan. |
| [GLX_RENDERER.md](GLX_RENDERER.md), [GLX_FINAL_CONTRACT.md](GLX_FINAL_CONTRACT.md), [GLX_FEATURE_MATRIX.md](GLX_FEATURE_MATRIX.md), [GLX_COLORSPACE_AUDIT.md](GLX_COLORSPACE_AUDIT.md), [GLX_LEGACY_COUPLING.md](GLX_LEGACY_COUPLING.md) | GLx's architecture, target contract, feature closure, colour pipeline and remaining legacy sources. |
| [GLX_RC_GATES.md](GLX_RC_GATES.md), [GLX_PROOF_CORPUS.md](GLX_PROOF_CORPUS.md), [GLX_VISUAL_DOSSIER.md](GLX_VISUAL_DOSSIER.md), [VK_RC_GATES.md](VK_RC_GATES.md) | The GLx and Vulkan runtime gates, proof scene corpus and review dossier that `scripts/glx_runtime_sweep.py` and `scripts/vk_runtime_sweep.py` produce. |

## Repository tooling

[ISSUE_TRIAGE.md](ISSUE_TRIAGE.md) describes the GitHub issue-triage workflow,
which this repository still runs.

## Historical records

FnQL's release process and completed transitions. fnTech3 does not use them;
its changes go in the root [CHANGELOG.md](../../CHANGELOG.md).

| Document | Subject |
| --- | --- |
| [CHANGELOG.md](CHANGELOG.md), [RELEASE_COMPLETION.md](RELEASE_COMPLETION.md), [releases/](releases/README.md) | FnQL's release-note queue and curated notes. Nothing reads the queue now; the legacy `scripts/manual_release.py` snapshot tooling still looks for curated notes in `releases/`. |
| [GLX_PROMOTION.md](GLX_PROMOTION.md), [GLX_ROLLBACK_PACKAGE.md](GLX_ROLLBACK_PACKAGE.md) | The completed promotion of GLx over the retired `opengl` and `opengl2` renderers. |
