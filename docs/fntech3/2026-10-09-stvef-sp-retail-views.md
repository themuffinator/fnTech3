# Elite Force single-player views beside retail, 2026-10-09

Eight static single-player scenes now have synchronized engine screenshots
from retail `stvoy.exe` 1.20 and fnTech3's GLx, Vulkan and RTX raster paths.
The two sweeps completed before the interrupted Claude session was resumed;
this record finishes that work from the saved manifests and captures.
The [JSON companion](2026-10-09-stvef-sp-retail-views.json) retains every
command, executable and capture hash, view position and image measurement.
This is **Live subset** evidence. No compatibility gate closes.

## Identity and method

The Windows x86 client is the frozen final runtime from the
[Holomatch clock slice](2026-10-09-stvef-holomatch-server-clock.md), built
from `465b5eb1` plus that slice (committed as `60d3a85a`): executable
SHA-256 `c6650f52474b3a9188f81d1d0e4df05a6b9a81ff01133071a928efc7b5905a1c`.
Its three renderer binaries were retained from the earlier frozen runtime;
this probe changes no renderer code. The companion records their hashes.
Retail's executable is
`8f7ccadb52b4230689c5b3275177f7ec7060a30bfaa3ade4e7b3fb0cda0dff74`.
Both read only the four retail paks and game libraries, excluding the
installation's third-party texture pack.

`scripts/probe_stvef_sp_views.py` runs every process windowed at 640 by 480
on a private desktop that is never switched to, with no injected input or
sound and isolated writable folders. It sets `fixedtime 50`, waits 30 game
seconds after loading, freezes NPCs and effects, hides the HUD and view
weapon, sets the view, and allows three seconds for the player to land.
Retail counts two `wait`s per frame and fnTech3 one, so retail's waits are
doubled. Each engine takes its own `screenshot` before `viewpos` prints;
otherwise the diagnostic text would contaminate the comparison.

Each sweep runs retail twice and fnTech3 once per renderer on each of
`voy2`, `stasis1`, `scav2`, `dn1`, `borg2`, `scav3`, `forge2` and `voy14`:
40 runs per sweep, 80 total. All exit normally with code 0, produce one
capture apiece and report identical final view positions within their scene.
Both manifests report the installation unchanged. The camera settles below
the requested height on `dn1` in every engine; the recorded final position,
not the requested height, establishes the comparison.

## Presentation controls

Retail's first start executes `perfect.cfg` on this CPU. It enables flares
and projection shadows, while its latched `r_picmip` remains 1 for that
session. The companion separates the binary observations, addresses and
shipped configuration from the live saved settings.

The first sweep keeps fnTech3's GLx/Vulkan presentation defaults. The second
uses `--matched`: flares 1, shadows 3, picmip 1,
`GL_LINEAR_MIPMAP_NEAREST`, anisotropy 16, subdivisions 2 and LOD bias 0.
**Both sweeps force RTX to raster SDR** (`rtx_rt_mode 0`, `r_hdr 0`). The
label "defaults" therefore does not mean RTX's normal HDR/RT presentation.
These are diagnostic settings, not new engine defaults or a retail-exact
mode imposed on players.

The images still use Fn's renderers, their geometry, shader, lighting,
postprocessing and screenshot paths. Spatial audio, HRTF, occlusion and
room effects are outside this silent image probe. Native ray tracing and
default RTX HDR are also outside it. This preserves invariant 11: retail's
image is comparison evidence, not a requirement to disable Fn features.

## Observed image differences

Each cell below is the mean absolute RGB difference averaged across the
three channels (0 to 255), followed by the percentage of pixels whose
largest channel difference exceeds 16. These are descriptive measurements,
not a pass threshold. The companion retains the individual channel values.

| Matched scene | GLx | Vulkan | RTX raster SDR |
| --- | --- | --- | --- |
| voy2 | 0.030 / 0.00% | 0.500 / 0.00% | 0.500 / 0.00% |
| stasis1 | 0.488 / 0.27% | 1.844 / 1.52% | 1.829 / 1.49% |
| scav2 | 0.310 / 0.10% | 1.663 / 0.76% | 1.672 / 0.76% |
| dn1 | 0.103 / 0.00% | 0.678 / 0.00% | 0.685 / 0.00% |
| borg2 | 1.381 / 2.28% | 2.524 / 3.42% | 2.494 / 3.39% |
| scav3 | 1.222 / 3.79% | 1.987 / 4.16% | 1.977 / 4.14% |
| forge2 | 0.500 / 1.32% | 1.482 / 2.94% | 1.482 / 2.95% |
| voy14 | 0.283 / 0.20% | 1.536 / 1.91% | 1.531 / 1.90% |

Matching the settings reduces GLx's mean difference in every sampled scene.
For example, `voy2` falls from about 1.55 to 0.03, and `stasis1` from 2.25
to 0.49. The retail repeats are identical on most scenes, but differ
slightly on `borg2`, `forge2` or `voy14` in at least one sweep; the largest
repeat channel mean is 0.089. Wall-clock-seeded game state means static
camera timing alone does not make every scene deterministic.

On `stasis1`, a repeat matched control reproduces the same GLx and Vulkan
metrics. Turning Vulkan HDR off changes its channel means from
0.970/2.145/2.416 to 0.957/2.128/2.402, matching the RTX SDR control.
Disabling Vulkan's offscreen buffer instead gives 0.611/1.730/2.028.
These controls show that HDR alone does not explain the remaining bright
strip differences. They do not establish a defect or isolate its cause.
No presentation code was changed to chase those pixels.

## Reproduction and limits

From the repository root, with the frozen runtime and stock asset layout
described above, use a fresh output directory for each command:

```powershell
python scripts/probe_stvef_sp_views.py --output .tmp/stvef-compat/sp-views-default --runtime .tmp/runtime/stvef-compat-after
python scripts/probe_stvef_sp_views.py --output .tmp/stvef-compat/sp-views-matched --runtime .tmp/runtime/stvef-compat-after --matched
```

The saved results and engine captures remain in those directories; the
additional controls are in `sp-views-vkcheck`, `sp-views-hdr` and
`sp-views-fbo` beside them. All 72 saved image comparisons across these five
manifests were recomputed from the engine images and reproduced exactly.
The probe now exits with failure for abnormal termination, missing or extra
captures, mismatched camera positions or image dimensions, or a changed
installation. `tests/stvef_sp_views_probe_tests.py` exercises those checks
without starting a game.

This work establishes sampled static views only.
It does not establish universal pixel parity, animated NPC/effect coverage,
physical input, listening, native RT, or full compatibility. Campaign
traversal is separate evidence in the
[campaign record](2026-10-09-stvef-sp-campaign.md).
