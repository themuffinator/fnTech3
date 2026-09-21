# Elite Force: matching effect captures to rendered frames

The large portable-shield mismatch in the [shader-wave comparison](2026-09-21-stvef-shader-waves.md)
was caused by unmatched capture timing. Correcting the probe produces close
retail comparisons for both successive frames at nine demo states on GLx,
Vulkan and RTX. This changes the test method and evidence; the production
engine and renderer binaries are unchanged.

## Why a frozen snapshot was insufficient

An isolated, instrumented GLx renderer identifies the large shield as
`gfx/misc/blue_portashield`. Its two retail material stages use sine waves;
it is distinct from `blue_dmgshield`, which has the random wave corrected
earlier. At held view times 753850 and 762850, the portable shield's material
clock, four positions, texture coordinates, stage colors and blend state
stay fixed. Nevertheless, each of 31 successive rendered frames contains
one more copy of the same quad: each stage draws 1, 2, ..., 31 times. The
camera matrix also changes at the first state. Holding the game clock does
not stop cgame from submitting these effects on each new frame.

The earlier scripts matched the advancing demo's time, snapshot and command
readouts, but then used `wait 10` in both engines. In the tested retail 1.20
client, a rendered frame consumes two wait counts; fnTech3 consumes one.
The shield therefore accumulated for different numbers of frames. Scaling
the held waits reduces the later shield's full-frame RGB mean absolute
difference from 14.8900 to 1.7984 on GLx, without changing the engine.

There is a second offset. Retail's screenshot command at `stvoyHM.exe`
`0x453430` calls the pixel reader at `0x4530f0` directly. The reader calls
the dynamically resolved `glReadPixels` at `0x453164`; its function pointer
at `0xac3ef8` is assigned during OpenGL initialization at `0x458911`.
fnTech3's `R_ScreenShot_f` schedules the capture at the end of the current
frame. A command issued at the same point in both command streams therefore
captures different rendered frames. Read-only inspection of the installed
executable establishes this distinction; no retail code is incorporated.

## Corrected capture method

The reusable [capture helper](../../tests/stvef_demo_capture.py) writes the
exact command streams used by all twelve final runs. It does not launch
the game or control input, and refuses to overwrite an existing script.
For example, with a fresh test home's `baseEF` directory already created:

```powershell
python tests/stvef_demo_capture.py --engine fntech3 --output E:/Games/fnTech3-tests/new-capture/baseEF/capture.cfg
```

Use `--engine retail` for the retail control. Start the installed demo with
`cl_freezeDemo 1`, `timedemo 0`, and `activeAction "exec capture.cfg"`.
The script advances 80, 200 and 180 timedemo frames to cumulative samples
80, 280 and 460. At each sample it captures two successive rendered frames,
then resumes. Retail renders each held frame before its screenshot command;
fnTech3 renders it afterwards. Both render exactly two held frames per
sample, preserving the number of subsequent cgame calls as well as matching
the capture itself. The helper's optional sample list must increase strictly.

The run manifest retains every startup argument and configuration. The
Windows x86 tests use independently copied retail assets, isolated writable
homes, a private desktop that is never activated, `r_fullscreen 0`,
`in_mouse 0`, `in_joystick 0` and `in_nograb 1`. Sound is disabled. All images
come from the engine's registered screenshot command. No input injection,
OS capture, installation edits or proprietary files in Git are involved.

The comparison uses 640x480, picmip 1, gamma 1, overbright 0/map-overbright 1,
legacy dynamic lights, no bloom/HDR/texture compression, and trilinear texture
filtering. Each renderer is selected with `cl_renderer` and verified from
the log. RTX uses its SDR raster path on the tested Windows driver. These
are controlled legacy-rendering settings, not evidence of native Windows RT.

## Results and limits

All twelve uninstrumented runs exit normally and write six images each:
three retail controls plus three demos on each of the three fnTech3
renderers. Both captures at all nine sample states are compared, giving
54 fnTech3-to-retail comparisons. The complete RGB images are used without
alignment, exclusion masks, or choosing a best matching frame.

| Demo | GLx RGB MAE range | Vulkan RGB MAE range | RTX RGB MAE range |
| --- | --- | --- | --- |
| `demo0000` | 0.3892–0.7258 | 0.3898–0.7277 | 0.3898–0.7277 |
| `demo0001` | 0.6446–2.2037 | 0.6422–2.1962 | 0.6422–2.1962 |
| `extreme` | 0.4413–0.6507 | 0.5127–0.6507 | 0.5127–0.6507 |

Values are mean absolute differences in RGB channel units out of 255.
The inspected images agree in world and weapon placement, portable-shield
coverage and brightness, phaser/beam placement, and the selected hit/burst
effects. The two successive shield images change in both engines, as
expected from their additional cgame frame. Matching snapshot readouts are
also checked in all 72 images. This adds bounded runtime image evidence;
the results are not pixel-identical. Individual channel differences reach
246, and the `demo0001` burst at sample 460 has the largest average residual.
Those residuals and their complete metrics are retained without attributing
them to an unproven engine defect.

The checked-in helper's output is compared byte-for-byte with all twelve
run scripts. Its command-line checks cover both engine choices, invalid or
unordered sample lists, and refusal to overwrite an existing file. The
existing production builds and their full Windows suite results remain
those in the [force-feedback record](2026-09-21-stvef-force-feedback.md);
this slice changes no production C/C++ source and does not rerun that suite.

The [evidence manifest](evidence/2026-09-21-stvef-effect-captures.json) pins
the production and instrumented binaries separately, the retail inputs,
scripts, logs, screenshots, shader observations, diagnostic counts and all
image metrics. Retained local evidence is under `.tmp/stvef-gaps`, with
independent test homes under `E:/Games/fnTech3-tests`.

Automatic approval review rejected removal of disposable trace objects,
linker outputs and two generated configuration files with the reason
"blocked by policy." The action did not execute. These files remain under
`.tmp/stvef-gaps`; no alternative cleanup was attempted.

Broader SP and effect-scene parity, campaign play, physical input,
listening/force-feedback devices and Windows native RT remain open. Both
profiles remain development-gated. No new unrelated defect was found; the
previously recorded installation, platform and cleanup limitations remain
in the [runtime record](2026-09-21-stvef-runtime-coverage.md).
