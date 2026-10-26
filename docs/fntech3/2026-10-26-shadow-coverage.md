# Projected size and point-light shadows, 2026-10-26

**Decision: keep the production shadow planner without a projected-size
floor.** A temporary, profile-neutral cutoff was built into GL, Vulkan and
RTX's raster planner and measured. It does not remove either cube in the
Medal of Honor truck view that motivated the work, or in the RTCW static-light
view. In Quake III it removes only 10 of 981 planned cubes at the largest
tested small-light threshold, changes one targeted capture, and gives no
repeatable frame-time improvement across the renderers. The diagnostic
code and switch have been removed from the source and rebuilt out of the
normal binaries.

The [JSON companion](2026-10-26-shadow-coverage.json) records executable and
renderer hashes, fixture identities, commands, measurements and every
capture comparison. This is Source, Unit, Build and bounded Live evidence;
no compatibility gate closes.

## Correcting the premise

The [truck-light record](2026-10-26-mohaa-truck-ride-lights.md) incorrectly
described two shadowed static lights on the moving ride as roughly 5,000
units away, lighting a few distant pixels. Its saved GL census instead
identifies the eligible lights with receivers at **(-1330, -3619, 280)** and
**(-1324, -3040, 319)**. Their distance from the truck's lantern in that
census is 466 to 637 units. That is a distance to the logged lantern,
not an eye-distance measurement. The far points in the initial light list
have no receivers there; they are different lights.

In the new run, the first of the shadowed lights has radius 153.1, forward
eye-space depth 274.78 and a conservative projected bound of **594.09 by
519.48 pixels** at 1280x720. The other sphere crosses the eye plane and is
retained without projecting it. These are not small projected lights.
Both the prose and JSON of the earlier record now identify the correction.

## Experiment

Base commit: `1745635816824ba5dea7b473c48e34b28546591f`, which brings the
earlier off-view fix onto main after the Ritual model-cache fix. Windows
x86, MSVC 2022, existing `meson/build/win32-dev` debugoptimized preset with
strict warnings. All runtime launches use that stable executable path,
private desktops, windowed 1280x720, isolated homes under
`.tmp/shadow-coverage/`, no mouse or keyboard input, and engine screenshots.
Retail installations are read-only inputs. RTX runs use `rtx_rt_mode 0`;
this experiment does not establish path-traced behavior.

The prototype surrounds the light's actual lighting radius with an
eye-space cube, projects its corners, clips the resulting rectangle to the
viewport and skips the point-shadow candidate only when **both** rectangle
dimensions are at or below the threshold. The light still illuminates its
receivers. Spheres crossing the eye plane, stereo views and shadow
correctness mode retain their cubes. The same helper is called at the same
point in each renderer, after receiver and input validation. It uses the
lighting radius, not the quantized shadow projection distance. Its bounds
are an upper bound on coverage, not a measurement of actual lit fragments.

Thresholds 16, 32 and 64 pixels are compared against zero (disabled);
the truck view is also tried at 128 and 256. The temporary
`fn_probeShadowPixels` switch, helper, patch and probe scripts are retained
only with the local evidence. No new setting is shipped.

## Medal of Honor: m1l1

The early frozen view is a useful negative control: zero planned cubes,
identical captures at all three thresholds. A later view is located by
waiting for two planned cubes before `timescale 0`. Each setting is
captured and then timed for four seconds with `com_speeds`, with shadow
debug output disabled during timing. Values are mean client milliseconds;
the repeat baseline exposes unrelated timing variation.

| Cutoff | Baseline | 16 | 32 | 64 | Baseline again |
| --- | ---: | ---: | ---: | ---: | ---: |
| Later truck view, Vulkan | 3.47 | 4.01 | 3.16 | 4.07 | 3.47 |

Every case plans two cubes and 12 faces, with 94 shadow draws and 207
surfaces. All five captures are byte-identical. A separate diagnostic
view keeps the same work at 128 and 256 pixels too; its timings are 4.86,
4.72, 4.77 and 3.30 ms for baseline, 128, 256 and baseline again, and all
four captures match. None of these timing differences is a shadow-work
saving. The broad baseline drift prevents a small performance claim.

## Quake III: four.dm_68

The retail demo runs to completion with `timedemo 1`: 1,260 rendered
frames. All three raster renderers first take 20 fixed captures, at
command-wait counts 60 through 1200. Every candidate capture matches its
renderer baseline, as does the repeated baseline. That sampling alone
misses the short frames where small cubes disappear.

| Renderer | Cubes at zero | At 16 | At 32 | At 64 | Faces at zero / 64 |
| --- | ---: | ---: | ---: | ---: | ---: |
| GL | 980 | 979 | 975 | 970 | 5744 / 5697 |
| Vulkan | 981 | 980 | 976 | 971 | 5750 / 5703 |
| RTX raster | 981 | 980 | 976 | 971 | 5750 / 5703 |

The Vulkan diagnostic lists 16 candidate occurrences with bounds no larger
than 64 pixels. A second replay samples 56 frames surrounding **all** those
occurrences, including cases already excluded by budget or priority.
The capture commands live in a config, avoiding the cvar string limit.
At 32 pixels, all 56 captures match. At 64, **one capture changes**:
command-wait count 1109, `shot51.png`, ten pixels in inclusive rectangle
(647, 283)-(651, 286), with maximum channel difference 63/255 and nine
pixels differing by more than four levels. The repeated baseline matches
all 56 original captures, including those ten pixels. This is an observed
shadow change, not replay noise. The other 55 candidate captures match.

Separate full-demo timings have screenshots and shadow diagnostics off.
Each row alternates baseline/cutoff/baseline/cutoff/baseline. These are
seconds for all 1,260 frames, not screenshot-run timings.

| Renderer | Baseline | 64 | Baseline | 64 | Baseline |
| --- | ---: | ---: | ---: | ---: | ---: |
| GL | 4.79 | 4.85 | 4.66 | 4.92 | 4.78 |
| Vulkan | 2.59 | 2.33 | 2.19 | 2.27 | 2.43 |
| RTX raster | 2.37 | 2.16 | 2.46 | 2.50 | 2.14 |

GL does not improve; Vulkan's candidate times sit within the baseline
spread; RTX alternates an apparent gain and loss. Ten fewer cubes and 47
fewer faces per demo do not establish a useful performance gain here.
Other work was running on the host, so these are bounded observations,
not isolated benchmark claims.

## RTCW multiplayer: mp_ice

The client joins Allies using console commands on a private desktop.
`setviewpos -3700 -800 40 180`, `cg_thirdPerson 0`, disabled warmup and
`timescale 0` provide a view of static sidecar lighting inside the map.
No physical input is controlled. Baseline, 32, 64 and baseline again all
keep **two cubes, 12 faces, 42 draws and 272 surfaces**; the spheres are
not small projection candidates. Mean client times are 5.81, 4.87, 5.95
and 5.73 ms, with no shadow-work reduction.

The captures do not repeat even at unchanged settings. Against the first
baseline, the 32-pixel case changes 62,733 pixels, the 64-pixel case 55,191,
and the second baseline 62,029; all three reach a maximum channel
difference of 255. The changed areas include the weapon and HUD. Their
full bounds, counts and stable-baseline pixel subsets are in the companion;
none is claimed as a cutoff-induced change, because the planner skips no
light. An earlier spawn-view control has zero cubes and also fails to
repeat its pictures; its five captures are retained and explicitly marked
as an unsuitable image-comparison control.

## Decision and limits

A projected bound can cheaply identify some small light spheres. It does
not identify the expensive truck lights as small, and the measured cases
do not justify shipping a universal threshold. Making the threshold large
enough to reject the truck light would allow changes across a substantial
part of the screen. Counting receiver surfaces cannot substitute for
counting receiver pixels: a single surface can span the view, and bounds
can include hidden or unlit fragments. A GPU receiver-coverage measurement
was not implemented or benchmarked in this slice.

The experiment also does not prove that an abrupt cutoff would avoid
visible transitions at other resolutions, fields of view, portal views,
or in the other thirteen profiles. The targeted ten-pixel Q3 change is
small but real. Without a demonstrated gain, keeping the existing behavior
is the supported decision, not a new compatibility blocker.

FnQ/FnQL's per-pixel lighting, static sidecars, shadow atlas, shadow
filtering, biases, caster reuse and all other presentation features remain
enabled as before. No retail-exact option is introduced. The previously
validated off-view light rejection is now integrated on main; the
in-view coverage experiment adds no production rendering change.

## Validation and local evidence

- The client and all three renderer DLLs build with strict warnings after
  removing the prototype.
- The 44 targeted dynamic-light tests and 64 shadow tests pass (overlapping
  test sets), as do four generated-documentation tests. All 547 recorded
  capture references and hashes, JSON and relative documentation links
  were validated. This is not a full-suite or full-compatibility claim.
- Raw commands, timing windows, comparison metrics, fixture hashes and
  captures are under `.tmp/shadow-coverage/`. The companion contains the
  machine-readable record and identifies the local evidence. Captures are
  deduplicated to 129 files by SHA-256; logs are compressed. The task removed
  850 disposable or duplicate files and 147 empty probe directories.
- Startup-only probe failures were caused by exceeding the engine's
  documented 32-command limit. Moving non-startup values to a config fixed
  the harness; they provide no renderer evidence.

Two existing worktrees look stale and are left for approval before removal:

- `E:/Repositories/fnTech3/.claude/worktrees/happy-fermat-3dd42d`, branch
  `claude/happy-fermat-3dd42d`: clean, with no unique commits; its tip is
  already an ancestor of main.
- `E:/Repositories/fnTech3/.claude/worktrees/elated-bassi-85fe6f`, branch
  `claude/elated-bassi-85fe6f`: clean, with its off-view fix now integrated
  on main as `17456358`. Preserve any needed older local probe evidence
  before removing it. The GL-default branch still has separate work and
  is not included in this recommendation.
