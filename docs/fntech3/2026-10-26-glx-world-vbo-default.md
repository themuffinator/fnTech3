# GL's world buffer on by default, 2026-10-26

GL (`cl_renderer glx`, the default renderer) registered `r_vbo` as `0`, as in
the initial commit, so it rebuilt every world surface on the CPU each frame
and its dynamic-light shadows drew the world's casters the same way; Vulkan
and RTX default to `1`. The [Medal of Honor frame-time
record](2026-10-25-mohaa-frame-time.md) found `m1l1`'s frozen frame at 11.2 ms
with `r_vbo 0` and 8.7 ms with `1`, and left the default open because it
reaches every profile. This record checks all sixteen. Every game draws the
same picture either way except where the two paths are known to round or
tessellate differently, none of it visible, no game is measurably slower, and
the heavy scenes are faster: GL now defaults to `r_vbo 1` in every profile. One
guard came with it: a brush model drawn with Elite Force's forced alpha now
takes the CPU path, as Jedi Academy's overridden entities already did. The
[JSON companion](2026-10-26-glx-world-vbo-default.json) holds identities,
commands and numbers. This is Source, Unit, Build and Live evidence. No
compatibility gate closes.

Integration note: these timings predate the separate
[view-culling correction](2026-10-26-mohaa-truck-ride-lights.md). They measure
the world buffer on the baseline named below, not an additional speedup on
the combined build. That later investigation also establishes that the
convoy's headlights are beams and coronas, not dynamic lights; the shadow
work discussed here belongs to the lantern and static sidecar lights.

## How it was measured

One runtime built from `6894fe47` with this change (`r_vbo`'s default, the
guard), configured like `meson/build/win32-dev` but GL only and client only, in
the worktree's `.tmp/vb`: `fntech3.exe` `012f76d5…`, `fnql_glx_x86.dll`
`979d8636…` (the committed renderer, `52852402…`, differs only in `r_vbo`'s
console description). Every probe set `r_vbo` explicitly, ran on a private
desktop, windowed at 1280x720 with sound, mouse, joystick and networking off,
an isolated home with empty configurations under every profile's names, and
the frame cap off (`com_maxfps 0`, `com_maxfpsUnfocused 0`); every image is the
engine's own `screenshotPNG`. The installations were read-only and their file
inventories were the same after every run. Other sessions' builds loaded the
machine throughout (about half its CPU), so single timings vary by 10 to 30%.

Three methods, as each profile allows:

- **Replays.** A demo replayed with `timedemo 1` shows the same game state at
  each frame count, so captures of two replays compare byte for byte. Each was
  replayed four times with captures (`r_vbo` 0, 1, 0, 1) and six times
  without, for timing (`timedemo`'s frames per second). Shipped demos: Quake
  III's `four`, Q3Test 1.08's `q3demo1`, the IHV Test's `sli`. For Quake Live,
  RTCW multiplayer, Enemy Territory, Elite Force Holomatch and Jedi Academy
  multiplayer the probe recorded a tour: the map's own spawn points, read from
  its entity lump, visited with `setviewpos` (no input). Medal of Honor's tour
  is `m1l1`'s scripted truck ride, recorded for 12 seconds.
- **Frozen views.** A level loaded, the clock stopped (`timescale 0` where the
  profile's frame rules reach zero, F.A.K.K.2 and Medal of Honor;
  `timescale 0.001` or `fixedtime 1`, one millisecond a frame, elsewhere),
  then four captures a few frames apart, each after `vid_restart` switching
  `r_vbo` 0, 1, 0, 1, and six timing windows alternating the two. Animated
  pixels change between any two captures, so the comparison counts pixels
  that differ away from (more than four pixels from) anything that also
  differs between the two captures of one setting.
- **Stage counts.** `glxinfo` before and after each window (or replay) gives
  how many shader stages GL drew through the world buffer and how many through
  the CPU path, so a view where the buffer is not used shows as such.

## What each game does

| Profile | Where | Stages a frame through the buffer | Picture | Time, `r_vbo` 0 / 1 |
| --- | --- | --- | --- | --- |
| `q3` | `four` demo; `q3dm7` tour and frozen | 21 of 98 (`four`), 31 of 59 (tour) | the same apart from curves (below) | `four` 198 / 191 fps, tour 206 / 230 fps |
| `ql` | `campgrounds` tour and frozen | 35 of 77 | curves; 5 frames with up to 4 levels on metal | tour 180 / 208 fps |
| `rtcw-mp` | `mp_beach` tour | 60 of 138 | 21 of 23 identical; 2 frames, 2 and 29 pixels at an edge | 129 / 138 fps |
| `rtcw-sp` | `escape1`, `village1`, `tram`, `castle` frozen | 0 (scene fog) | `village1` identical | 2.6-8.7 ms either way |
| `wolfet` | `oasis` tour | 115 of 135 | 2 levels inside the fog volume (below) | 276 / 260 fps |
| `stvef-sp` | `stasis1` frozen | 9 of 16 | identical | about 1 ms either way |
| `stvef-mp` | `hm_borg1` tour and frozen | 67 of 154 (tour), 78 of 187 (frozen) | curves only | 395 / 551 fps |
| `q3-ihv` | `tim_dm1` frozen | 8 of 65 (its fog keeps the rest) | the same away from animation | 3.5-3.8 ms either way |
| `q3test` | `q3demo1` demo; `q3test2` frozen | 29 of 86 (demo), 25 of 77 (frozen) | curves; 7 single pixels | 216 / 215 fps |
| `jka-mp` | `mp/ffa1` tour and frozen | 78 of 163 (tour), 55 of 88 (frozen) | frozen identical; 2 levels inside the fog volume | 297 / 331 fps |
| `jk2-mp` | `ffa_bespin` frozen | 25 of 29 | the same away from animation | 3.6-3.9 ms either way |
| `jka-sp`, `jk2-sp` | `t1_surprise`, `kejim_post` | none: the game stops before a level draws | -- | -- |
| `alice` | `garden1` frozen | 0 of 162 (12 surfaces built) | the same away from animation | about 7 ms either way |
| `fakk2` | `homes1` frozen | 80 of 234 | the same away from animation | about 7 ms either way |
| `mohaa` | `m1l1` ride; `m1l1`, `m2l1` frozen | about 13 of 356 on the ride; 0 on `m2l1` (fog) | curves (below) | ride 143 / 197 fps; `m1l1` frozen 6.8-7.3 / 4.8-6.0 ms |

Replays of one setting were byte-identical to each other everywhere but
Medal of Honor, whose compass and health bar differ between any two replays,
and the IHV Test, whose demo plays on wall-clock time. The stage figures are
per frame with `r_vbo 1`: through the buffer, of all stages drawn.
The frame rates are medians of three replays each; the times are the client's
own (`com_speeds`'s `cl`), means over a window. Quake Live held its frozen
probe at about 30 frames a second, which only the client time sees through.

Where the buffer draws nothing the setting changes nothing the player sees:
RTCW's and Enemy Territory's scene fog, Alice's and F.A.K.K.2's far fog, Medal
of Honor's fog and the IHV Test's fog all keep GL's world on the CPU path
(`RB_BeginSurface`), and so do Alice's own shader sources, Raven's light styles
and surface sprites, enhanced liquids and every animated stage
(`isStaticShader`). Medal of Honor's `m1l1` gains even so on the measured
baseline: its dynamic lights draw their shadows' world casters from the buffer
(`RB_RenderDlightShadowCasters`), where `r_vbo 0` built them on the CPU.

## What differs, and why

Observed in the replays, with the cause confirmed where a setting isolates it:

- **Curved surfaces.** The buffer stores each patch at full subdivision; the
  CPU path drops rows with distance (`LodErrorForVolume`, `r_lodCurveError`).
  Arches, rounded tops and distant mounds therefore differ at their edges and
  in their shading. Replayed with `r_lodCurveError 100000000`, which makes the
  CPU path keep every row too, the two settings match: Quake III's tour 23 of
  23 frames, Holomatch's 23 of 23, `four` 60 of 62 (two single pixels),
  `q3demo1` 47 of 54 (one or two pixels in each of the rest). Medal of
  Honor's ride keeps a few hundred to 2,600 differing pixels in most frames,
  down from 10,000 to 50,000, on the same distant mounds. Vulkan and RTX
  already draw patches this way (`USE_VBO_GRID`). This is accepted: the curves
  are drawn finer, never coarser.
- **Fog volumes.** Inside a Quake III-style fog volume the buffer fogs a stage
  with its own vertex and fragment programs, the CPU path with a separate fog
  pass. Enemy Territory's `oasis` lies inside one (`sd_siwafog`) and differs
  by 2 levels on about a third of its pixels, almost all in red and green,
  `r_vbo 1` a step nearer the sandy fog colour (a few hundred edge pixels
  differ by more); Jedi Academy's pit in `mp/ffa1` (`s_vjun_fog`) by 2 levels
  too. Neither changes with full-detail curves or `r_staticLights 0`, and
  RTCW's `mp_beach`, with no fog volume, matches but for two edge frames. The cause is inferred from where it appears; one step in the fog
  blend, doubled by overbright, is accepted.
- **Smaller.** Quake Live's girders and a lamp housing differ by up to 4 levels
  on under 2,000 pixels in five frames, inferred to be environment-mapped
  stages whose reflection coordinates the buffer's vertex program computes;
  RTCW's two frames differ in 2 and 29 pixels, the second a single pixel
  column at a polygon edge.

## Elite Force's forced alpha

Elite Force's render flag `0x800` blends each single-texture stage with the
entity's alpha as the batch draws (`tr_stvef_alpha.h`). The world buffer holds
each stage's colours precomputed, so a brush model drawn that way from the
buffer would have kept its full opacity. `RB_RenderDrawSurfList` now leaves
such an entity's batch to the CPU path, beside the same rule for Jedi
Academy's overrides. Nothing seen in this round used it on a brush model (the
Holomatch intro's fading room is an MD3 model, never in the buffer), so this
keeps `r_vbo 1` the same as `0` rather than fixing a seen fault.
`tests/stvef_forced_alpha_tests.py` checks the guard's place.

## Memory

With `r_vbo 1` the hunk holds the buffer's index lists: nothing more on
Alice's `garden1`, 0.1 to 0.6 MB more on `tim_dm1`, `q3test2`, `hm_borg1` and
`homes1`, 0.8 to 2.2 MB on the Quake III, Quake Live, RTCW and Jedi maps,
3.4 MB on `oasis`, 8.7 MB on `stasis1`, 10.7 MB on `m2l1` and 13.4 MB on
`m1l1`. The default hunk is 256 MB; the most any probe used was 95 MB.
`r_vbo`'s console description said 15 to 30 MB and now gives these numbers.

## FnQ features

Nothing a player sees changes beyond the rounding above; the renderer's
lighting, shadows, fog and liquids are the same features drawn from the
buffer (invariant 11). `r_vbo 0` stays, for comparison and for a driver that
draws the buffer wrongly; the [GLx guide](../GLX.md) says when to try it.

## Not done

- RTCW single-player demos cannot be replayed (its client game needs a running
  game module), so RTCW single player has frozen views only, all of them
  fogged; a capture right after `vid_restart` there catches the client game's
  fade and was left out.
- Medal of Honor and RTCW multiplayer come back from `vid_restart` to their
  briefing and limbo menus, and Enemy Territory's limbo panel covered its
  frozen view, so their frozen captures show menus; their replays stand in.
  Medal of Honor's map format has no spawn-point reader here, so its tour is
  the opening ride alone.
- The IHV Test's `sli` plays on wall-clock time, so its replays do not repeat;
  the frozen view stands in.
- Jedi Academy and Jedi Outcast single player stop before any level draws, so
  nothing could be compared there.
- Vulkan and RTX also draw brush models from their buffers without Elite
  Force's forced alpha in the vertex colours; not checked here.
- One GPU (RTX 4060 laptop, NVIDIA 591.74) and a busy machine.
