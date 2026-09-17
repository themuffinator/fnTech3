# RTCW dynamic lighting and shadow maps

RTCW SP and MP now honor `r_dlightMode` in GLx, Vulkan and RTX raster. Ordinary
RTCW lights enter the existing Q3/QL per-pixel light pass and point-shadow atlas:

| Setting | RTCW behavior |
| --- | --- |
| `r_dlightMode 0` | Original projected-light rendering. |
| `r_dlightMode 1` | Shared per-pixel world lighting. |
| `r_dlightMode 2` | Shared per-pixel world and model lighting. |
| `r_dlightShadows 1` | Eligible lights use the existing shadow budget, atlas, bias, filtering and strength controls. This setting is latched. |

The enhanced path applies `r_dlightScale`, `r_dlightIntensity` and the shared
color controls, with the existing falloff and specular calculation in each
renderer. RTCW overdraw becomes the equivalent light-color gain. Its original
force/disable flags, mode-zero radius rules, snooper behavior, and authored
negative/flame light materials retain their separate contracts. Those special
materials keep their projected texture/blend passes and do not become ordinary
positive shadow lights.

## Torch observations and implementation

The local retail `escape1.bsp` entity lump contains 18 `dlight` entities, including
orange lights with animated style strings. The original
[SP cgame lightstyle path](https://github.com/id-Software/RTCW-SP/blob/master/src/cgame/cg_ents.c)
interpolates their radius at 100 ms intervals and submits them through
`trap_R_AddLightToScene`. The visible flame material and the light submission
are separate. Original RTCW source was inspected for behavior only; no GPLv3
implementation text was incorporated into this GPLv2-or-later project.

The submitted radius and color remain animated. A bounded history of exact
light origins retains the largest quantized shadow range observed while a
stationary light remains present. This prevents a flicker cycle crossing a
projection band from repeatedly shrinking and expanding the depth projection.
Entries expire after 120 absent renderer frames, and a restarted frame clock
invalidates old matches. A larger observed radius expands the range; changing
light intensity alone does not require rebuilding stationary world depth.

Both backend filters now accept RTCW MDS and MDC casters, matching the existing
frontend collector. Previously the frontend collected these models but the
backend rejected their surface/model types. MDS shadow views use full detail
so the six cube faces share one silhouette. Animated model casters invalidate
the existing world-only cache; they are not frozen along with torch depth.
Their backend face culling is conservative when no format-specific bounds are
available. Existing entity flags and material exclusions still apply.

Shadow receiving retains the existing Q3 implementation's world-surface scope.
Models receive mode-two per-pixel light and cast onto the world. Alpha-tested,
blended and shader-deformed casters remain outside the current shadow path.
Baked lightmaps and light-grid illumination are not removed by dynamic shadows.

## Evidence

Baseline commit: `0b6af9ba7a17d73bf27c5c016c00772a1a93899a`, with the existing
working-tree changes retained. Evidence is under `.tmp/rtcw-dlight-shadows/`;
`evidence.json` records source and binary hashes. Runtime manifests retain
exact commands, configuration, executable/renderer identities and capture hashes.

- Windows x86 Meson development and release builds pass for the shared client
  and all three raster renderer modules. RTCW remains development-gated.
- Seven executable lighting tests cover both RTCW profiles, Q3/QL mode
  isolation, legacy and per-pixel builds, malformed inputs, force/disable
  flags, authored material passes, radius/color scaling, projection history,
  snooper model lighting, and both backend caster filters.
- The 50 discovered shadow tests pass. The affected render-import, MDS/MDC,
  fog, GLx shadow, renderer-contract and portability regressions also pass.
  These focused runs are not a complete compatibility-suite sign-off.
- `installed-final/` contains four registered `screenshotPNG` captures per
  backend from the legitimate SP `escape1` cinematic: shadow strength 0.6,
  strength zero, mode zero, and restored mode two/strength 0.6. All processes
  exit successfully with no engine errors.
- `caster-verified/` is a separate controlled probe: a private renderer copy
  positions an ordinary light beside the existing infantry MDS model. Its
  on/off captures establish visible model occlusion in all three backends.
  This is diagnostic light placement, not an unchanged retail torch scene.
  The stock scene's visible occlusion differs substantially among renderers;
  it is not claimed as cross-renderer image parity.

All probes are windowed, use isolated writable homes, leave the installation
unchanged, and capture through the engine. They use an unswitched private
desktop with input devices disabled. The only input is one briefing Ready
click per process under the standing project authorization. Private hooks
schedule captures and freeze the scene; retail modules and assets are unchanged.

Broader SP/MP runtime coverage, original-engine image parity, native ray tracing,
and campaign/interoperability gates remain open. An existing fog/brightness
difference is also visible between raster backends, especially RTX raster;
these light changes do not establish fog parity.

Disposable probe-build cleanup was attempted after confirming task-directory
containment, no filesystem links and no running probe executable. Automatic
approval review rejected both the guarded cleanup and the explicit-path
cleanup with only `blocked by policy`. Those temporary files remain in the
task evidence directory; no installation cleanup was attempted.
