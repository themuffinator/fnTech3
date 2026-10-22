# RTX shadows behind a portal sky, 2026-10-22

RTX's raster path (`rtx_rt_mode 0`) drew no cascaded sun shadows
(`r_csmShadows 1`) on Return to Castle Wolfenstein's `forest`, and none of
its point-light shadows either, where GLx and Vulkan draw both. The
shadow-format investigation (commits `e1502a80` and `c845803b`) found the gap
while capturing every game and showed it predates that work. The cause is
the map's portal sky. RTX's shadow atlases now start from either of the two
passes a view can be drawn in, and its sun-shadow receiver counts match GLx
and Vulkan. The [JSON companion](2026-10-22-rtx-portal-sky-shadows.json) has
the identities, commands, counters and capture hashes. No compatibility gate
closes: this is presentation.

The original comparisons below were captured on the shadow-format branch,
which also changes how baked surfaces receive sun shadows. The main-branch
integration takes only the portal-sky pass fix and its regression check
(`3369bee7`), plus this record and the player guidance (`abacf0f0`); those
separate shading changes are not a prerequisite. Historical capture metrics
remain tied to their recorded builds.

## Observed

- With `r_csmDebug 1`, RTX on `forest` (third person, `give all; weapon 3`)
  reported `caster:0 cache h/m/u:1/0/1 recv world:4 ent:0 baked:4` on every
  one of 100 frames. GLx and Vulkan, built from the same commit, reported
  `caster:335 cache h/m/u:0/1/1 recv world:104 ent:576 baked:104`. The
  point-light shadows (`r_dlightShadowDebug 1`) planned 2 lights in all three,
  but RTX rendered none (`render lights:0 ... surfs:0`) where Vulkan rendered
  2 lights and 763 caster surfaces.
- `forest` has a portal sky (`props_skyportal`), as do 19 of the 32 maps in
  the single-player paks and 5 of the 15 multiplayer ones. Enemy Territory's
  six retail maps, `oasis` among them, have none, which is why `oasis` took
  its sun shadows under RTX. Jedi Academy has one on 12 of its 61 maps.
- With `+set r_depthFade 0` (soft particles off), the frozen RTX build of the
  shadow formats slice (`runtime/csm-after`, `fnql_rtx_x86.dll`
  `67be3219...`) reported GLx's numbers exactly (`caster:335 ... recv
  world:104 ent:576 baked:104`, 2 shadowed lights, 763 surfaces). With
  depth fade at its default of 1, it reported the 4 and 0 again.

## Cause

This part is read from the source, and the `r_depthFade` run above confirms
it:

- The client game draws the portal sky as a full-screen scene of its own,
  ahead of the main view. That scene is a primary full view, so its first
  surface sorted after opaque takes the frame's depth-fade snapshot
  (`RB_RenderDrawSurfList`, `vk_copy_depth_fade`).
- RTX's snapshot resumes the scene in the post-bloom pass. Vulkan's resumes
  it in the main pass (`vk_begin_main_render_pass_load`). So under RTX the
  main view behind the portal sky begins in the post-bloom pass. RTX's
  path-trace gate and its depth clear already allowed for that
  (`vk_rt_primary_view_eligible`, `vk_clear_depth_internal`).
- The sun, point-light and spot shadow atlases leave the scene pass for
  their own and come back. Their begin functions accepted only the main pass,
  so behind a portal sky each one refused to start. The main view's sun
  atlas counted as uncacheable and then drew nothing (the `u:1` with
  `caster:0`). Nothing published it, so its receivers were skipped. The only
  receivers left were the portal sky scene's 4, which drew from a cached atlas
  (the `h:1`).

## Fix

`vk_begin_dlight_shadow_render_pass`, `vk_begin_spot_shadow_render_pass` and
`vk_begin_csm_shadow_render_pass` (`code/rendererrtx/vk.c`) now accept the
main or the post-bloom pass, note which one they left, and resume that one
when the atlas ends, as RTX's motion blur already did. A view drawn in the
main pass leaves and resumes it as before. GLx and Vulkan are not changed.
The path tracer's handling of these passes is not changed either.

FnQ3/FnQL features: this restores FnQuake3's cascaded sun shadows and its
shadowed dynamic lights to RTX's raster path behind a portal sky. Nothing is
left out, and there is no retail-exact option, since the original renderers
had neither feature.

## Evidence

Each run is a windowed 800x600 client on a private desktop, with no input
and no network, an isolated home with empty configs, `r_csmShadows 1`,
engine `screenshotPNG` captures, and the steps of the shadow formats
record's batch (`.tmp/rtcw-rtx-csm/tools/batch.py`). Four captures are taken:
everything on, sun shadows at strength 0, point-light shadows at strength
0, and on again. Before is a `git archive` build of `e1502a80`; after is the
same build with only `fnql_rtx_x86.dll` relinked with this change. The
engine, package and other two renderers are the same files. "Sun shadow"
is the share of all frame pixels more than 12 luma darker with the sun shadows
on than off and within 4 luma of the repeat capture. Every run exited cleanly, with
the installations and binaries unchanged.

| Run | Casters | Receivers: world, model, baked | Shadowed lights, surfaces | Sun shadow | Mean luma on, off |
| --- | --- | --- | --- | --- | --- |
| `forest`, RTX before | 0 | 4, 0, 4 | 0, 0 | 0.4 % | 42.4, 42.2 |
| `forest`, RTX after | 335 | 104, 576, 104 | 2, 763 | 72.0 % | 21.7, 42.2 |
| `forest`, GLx | 335 | 104, 576, 104 | 2, 700 | 72.7 % | 21.3, 41.5 |
| `forest`, Vulkan | 335 | 104, 576, 104 | 2, 763 | 72.1 % | 21.7, 42.2 |
| `mp_village` (RTCW MP, spectator), RTX before | 0 | 0, 0, 0 | 0, 0 | 0.2 % | 54.8, 54.2 |
| `mp_village`, RTX after | 0 (cached) | 1,655, 0, 1,655 | 0, 0 | 0.2 % | 54.8, 54.2 |
| `mp_village`, GLx | 0 (cached) | 1,655, 0, 1,655 | 0, 0 | 0.0 % | 54.5, 54.1 |
| `mp/duel9` (Jedi Academy), RTX before | 693 | 611, 80, 607 | none planned | 0.2 % | 163.3, 163.4 |
| `mp/duel9`, RTX after | 655 | 574, 80, 568 | none planned | 0.2 % | 159.9, 160.0 |
| `mp/duel9`, GLx | 655 | 574, 80, 568 | none planned | 0.2 % | 147.6, 147.7 |

- **`forest`.** After the fix RTX's shadow pattern matches GLx and Vulkan: the
  trees' shadows lie across the floor and over the player (mean luma 19.4
  against 19.3 and 19.4 on the repeat capture). Before, it was the
  shadows-off frame (41.1 against 42.2). The parent record's 0.7 % before
  was taken with the same steps on its own build.
  `.tmp/rtcw-rtx-csm/forest-renderers.png` puts the five captures side by
  side.
- **`mp_village`.** The spectator's view has no models, and a baked surface
  takes only models' sun shadows, so the frame does not change. The
  counters show the receivers now draw: 0 before, GLx's 1,655 after.
- **Jedi Academy's `mp/duel9`** (portal sky, `sun 0.8 0.8 0.95` in its sky
  shader) took its sun shadows under RTX before and after. Its sky scene did
  not leave the main pass in this view. Each launch spawns at random, so the
  before and after runs saw different views. After matches GLx surface for
  surface. `mp/duel6` has a portal sky but no sun ("skip no-sky-sun" in
  both).
- **Path traced** (`rtx_rt_mode 2`), `forest` before and after: each run has
  four `RTX RT timing` samples and no reported view left to raster. The two
  traced captures differ by more than 12 luma on 73 of 480,000 pixels
  (0.0152 %); 30 darken and 43 brighten. Any RGB change affects 16,111
  pixels (3.3565 %), with mean absolute luma difference 0.0213 on the
  0--255 scale. The original 0.01 % figure measured only pixels darkened
  by more than 12 luma, not all differences. The tracer owns the world and
  models there, so the main view's sun atlas is not needed, before or
  after. The point-light atlas now renders for the surfaces raster still
  draws (2 lights, 763 surfaces, against 0).
- **Rebased.** On `c845803b` (the sky-sun units change) with this commit on
  top, RTX's `forest` again reported `caster:335 ... recv world:104 ent:576
  baked:104` and 72.2 % against GLx's 72.7 %.
- **Tests.** `tests/rtx_vulkan_safety_source_tests.py` gains a check that
  the three atlases leave and resume the open scene pass, and that the
  depth-fade snapshot still resumes in the post-bloom pass. Run in the
  Visual Studio x86 shell: the 35 test files that read RTX's `vk.c` or cover
  the shadows, the liquid, fog and bloom passes passed (309 passed, 1
  skipped: F.A.K.K.2's map sweep, whose laid-down installation is not in
  this worktree). The script-style `tests/csm_shadow_cache_source_tests.py`
  passed. The after renderer built with the strict-warnings options.
- **Completion audit.** The documentation and packaging run completed after
  the session stopped: 146 passed, 2 skipped, 135 subtests passed. The skips
  were the absent F.A.K.K.2 installation and a symlink test requiring a
  Windows privilege unavailable to this process. All 18 saved runs and 64
  capture hashes were checked against the logs and the evidence copy in the
  main checkout; the sun-shadow measurements were recomputed. The rebased
  renderer matches the built DLL, and the exported `vk.c` matches the
  committed source after newline normalization. The renderer safety,
  changelog and generated-document checks were rerun: 20 passed, 30 subtests
  passed; the script-style CSM cache check passed again. After the player
  guides were updated, the manual, generated-document and changelog checks
  passed: 25 tests and 838 subtests. Logs and the audit summary are retained
  under `.tmp/rtcw-rtx-csm/` in both checkouts.

## Main-branch integration

The focused integration was built on `3e1371b0`, with source fix `863969c6`
and documentation `308cef72`. It carries only the portal-sky pass fix from
`3369bee7` and the record and guidance from `abacf0f0`. It preserves main's
existing caster eligibility and shading; the broader shadow-format work
remains separate. In particular, main does not yet include that branch's
MDS/MDC sun-shadow eligibility or baked-receiver rules, so the historical
335 casters / 576 model receivers above are not this build's expected counts.

The existing Windows x86 `meson/build/win32-dev` preset rebuilt the RTX
renderer with strict warnings. The engine package was rebuilt with Meson's
`fntech3-pkg.fnz` target and passed `scripts/verify_release_layout.py`.
The 36 applicable renderer, shadow and documentation test files passed:
315 tests and 64,089 subtests, with no skips. The script-style CSM cache
contract check also passed.

Fresh `forest` runs used the same stable executable, windowed 800x600 on a
private desktop, no input or network, isolated homes and engine
`screenshotPNG` captures. Debug logging was enabled after the four lighting
captures. All three runs exited normally, with no reported errors and with
the installation and binaries unchanged; all 15 capture hashes were checked.

| Renderer | Casters | Receivers: world, model | Shadowed lights, surfaces | Sun shadow | Mean luma on, off |
| --- | --- | --- | --- | --- | --- |
| RTX raster | 251 | 104, 492 | 2, 763 | 65.18 % | 24.34, 39.87 |
| GLx | 251 | 104, 492 | 2, 700 | 64.33 % | 24.18, 39.46 |
| Vulkan | 251 | 104, 492 | 2, 763 | 65.06 % | 24.35, 39.87 |

The sun-shadow counters agree across all three renderers on all 130 debug
frames per run. This confirms the pass fix independently of the other
branch's shading changes. Build/test logs and `runtime-evidence.json` are
in `.tmp/rtcw-rtx-csm/merge-main/`; the captures and console logs are in
`.tmp/rtcw-rtx-csm/runs/merge-main-{rtx,glx,vk}/`. The JSON companion's
`mainIntegration` section records the exact launch arguments and identities.

## Not covered, and open

- Spot shadows take the same path, but no view here plans one.
- RTCW's single-player portal-sky maps other than `forest`, and Jedi
  Academy's `mp/siege_hoth` and single-player maps, were not captured.
  Jedi Outcast's portal sky is not flagged as a scene of its own in RTX
  (`skyboxPortalScene` covers Enemy Territory, Jedi Academy multiplayer and
  RTCW's lighting profiles), so it cannot reach this path that way.
- Under RTX, behind a portal sky, the global-fog pass is still drawn once a
  frame on the first full-screen world view. That view is the portal sky
  (`vk_draw_global_fog`'s `lastFogFrame`), so the main view behind it gets
  none. `r_globalFog` is off by default, and this was read from the source,
  not captured.
- The dynamic-light counts differ between renderers independently of this
  fix: `forest` has 700 caster surfaces under GLx and 763 under Vulkan/RTX;
  `mp_village` plans one light out of 16 under RTX and none under GLx.
  These differences were observed before and after and were not investigated.
