# Medal of Honor's frame time, 2026-10-25

Medal of Honor felt sluggish and unresponsive in the single-player campaign:
mouse look lagged, movement felt heavy, actions answered late and the frame
rate was low and uneven. The input, the clock and the network were not the
cause. The frames were slow: the start of `m1l1`, the convoy's truck ride,
took 13 to 17 ms a frame at 1280x720 on this machine's RTX 4060 laptop GPU,
and up to 30 ms with the 2026-10-14 build. Most of that went into two pieces
of CPU work that every pass of the frame repeated. The back end skinned each
Medal of Honor model surface again for every pass that drew it, and each
face of a dynamic light's shadow cube tessellated every caster again and
pushed each vertex off its surface, though all six faces get the same
vertices. Both are now done once: on Vulkan the truck ride takes 3.4 to 4.5
ms a frame, and its worst frame 15 ms where it had been 179 ms. Every frame
compared is the same as before, byte for byte. The
[JSON companion](2026-10-25-mohaa-frame-time.json) holds the identities,
commands and numbers. This is Source, Unit, Build and Live evidence. No
compatibility gate closes.

## What it was not

Every probe ran fnTech3's client on a private desktop, windowed at
1280x720, with the retail 1.00 installation read-only, an empty
`configs/unnamedsoldier.cfg` in an isolated home, and `com_speeds 1`
logging each frame. The user's own MoHAA home, from the VS Code launch
configurations, uses `cl_renderer vk`.

- **The clock and the network.** The local client's ping was 7 ms;
  `cl_showTimeDelta` moved one or two milliseconds about its mean with 20
  snapshots a second, as `sv_fps 20` gives; `cg_nopredict` and
  `g_synchronousClients` were 0 and the lagometer showed interpolation
  throughout.
- **A regression.** Frozen runtimes from 2026-10-14, 2026-10-17 and
  2026-10-23 ran `m2l1` uncapped at 100 to 105 frames a second, the
  2026-10-25 build at 106. Under synthetic play on `m5l1a` (firing,
  walking, turning), the 2026-10-14 runtime averaged 2.6 ms a frame and the
  current build 2.7 ms; a first run of the current build showed 60 to 160 ms
  hitches that its repeat did not, from other sessions' builds loading the
  machine. `m1l1` was slow before any of the recent work: the 2026-10-14
  runtime took 26 to 30 ms a frame there, the current one 15 to 17.
- **Sun shadows.** `r_csmShadows` is off by default in GL; turning it off
  explicitly changed nothing measurable.

## Where the time went

Freezing `m1l1` with `timescale 0` holds one frame still, so its settings can
be compared in one run. With `r_dynamiclight 0` the frame took 5 ms in GL,
with the level's nine dynamic lights 12 to 14 ms, and with `r_dlightShadows
0` (and a `vid_restart`) 6 ms: the two lights whose shadows were drawn, the
following truck's headlights, cost about 8 ms. Main-thread stacks sampled
with `cdb -pv` during that frame put 12 of 19 samples in the shadow pass,
`RB_RenderDlightShadowCasters`:

| Work in the shadow pass | Samples |
| --- | --- |
| `RB_SurfaceMOHAATiki` posing and skinning the trucks (`Skin`, `Rotate`) | 6 |
| `RB_ApplyDlightShadowCasterNormalBias` pushing each vertex | 4 |
| drawing | 2 |

The light's six faces each walked its caster chain, skinned the trucks again
and pushed every vertex again. Vulkan, which draws world casters through tess
too (GL takes them from the world VBO when `r_vbo` is 1), spent 23 of 52
samples in the push alone once the skinning was fixed.

## Skinned once a frame

[`tr_mohaa_skin_cache.h`](../../code/renderercommon/tr_mohaa_skin_cache.h)
keeps each Medal of Honor model surface skinned this frame. The pose was
already computed once a frame (`R_MOHAAPose`'s pool); a surface skinned by
that pose, at the same scale, level of detail and view-weapon rule, is the
same however many passes draw it. The first draw keeps a copy, later draws
copy it back, and a new pose generation, counted when the pool starts over,
drops every copy. A surface the store has no room for is skinned each time.
All three renderers' `RB_SurfaceMOHAATiki` use it. On the frozen frame the
dynamic lights' share fell from 5.6 to 7.4 ms to 2.8 to 3.6 ms in GL, and in
Vulkan from 10.0 to 12.4 ms to 5.8 to 7.9 ms.

## A light's casters built once for its six faces

[`tr_dlight_caster_reuse.h`](../../code/renderercommon/tr_dlight_caster_reuse.h)
holds the push, moved there unchanged from the three renderers' copies of
`RB_ApplyDlightShadowCasterNormalBias`, and a store for one light at a time.
The push reads only the light's origin and the entity's own orientation, which
are the same in every face (`RB_BuildDlightShadowView` gives each face the
light's origin; `R_RotateForEntity` the entity's axes). So the first face that
draws a caster pushes it in the caster pass itself and keeps its vertices and
indexes, and the light's other faces copy them. The stage iterator leaves a
batch of such casters alone (`rb_dlightCasterOffsetApplied`), and a batch
never holds both kinds. A caster that overflows tess begins a new batch before
it writes, which `RB_BeginSurface`'s counter tells the pass.

Only casters whose tessellation no face changes are kept: faces, triangle
soups (Medal of Honor's terrain and static models), MD3 models and the Alice,
F.A.K.K.2 and Medal of Honor TIKI models, which the shadow pass draws whole or
at the view's detail. A grid measures its detail along the face's own axis
(`LodErrorForVolume`), and RTCW's and Enemy Territory's skeletal meshes
project their radius on it, so each face may tessellate them differently; they
are built per face as before. `r_dlightShadowCasterReuse 0` builds everything
per face, for comparison.

## Results

The frozen frame, one run each, `r_dlightShadowCasterReuse` switched in the
run (frame time in ms, mean):

| Renderer | Before both changes | Skin store | Both | Both, no dynamic lights |
| --- | --- | --- | --- | --- |
| Vulkan | 12.3, 16.8 (two runs) | 9.2, 11.2 | 2.8 (reuse off 4.7) | 1.1 |
| GL | 10.8, 12.1 | 8.4, 9.3 | 5.3 (reuse off 7.7) | 3.2 |

The freeze lands on a slightly different moment in each run, which the
no-light column of each run shows.

Synthetic play on `m1l1` (no frame cap; the first four seconds are the truck
ride, then firing, walking and turning), two runs of each build, frame time in
ms:

| Renderer | Build | Truck ride, mean (p95) | Play, mean (p95, worst) |
| --- | --- | --- | --- |
| Vulkan | before | 13.0 (16), 7.3 (11) | 3.5 (10, 179), 2.6 (7, 19) |
| Vulkan | after | 4.5 (7), 3.4 (5) | 2.2 (4, 15), 2.2 (4, 17) |
| GL | before | 10.1 (13), 12.5 (18) | 7.4 (13, 31), 5.9 (10, 27) |
| GL | after | 7.4 (12), 10.3 (15) | 4.9 (8, 20), 7.8 (16, 25) |

GL gains less: with its default `r_vbo 0` it tessellates the world on the CPU
for the view every frame, which neither change touches. With `r_vbo 1` the
frozen frame took 8.7 ms where `r_vbo 0` took 11.2 in the same run. The
machine was building other sessions' work throughout, so single numbers vary
by a few milliseconds.

## The frames are the same

- **The push.** `tests/dlight_caster_reuse_tests.py` compiles the header
  beside the loop the renderers ran before and pushes 512 vertices of a
  turned, moved entity, one of them on the light and one with no normal, for
  a point light and for the sun's direction at three biases: the floats are
  identical. The store's keep, copy, index rebasing, light change, room and
  stray-index rules are checked there too, and
  `tests/mohaa_skin_cache_tests.py` checks the skin store's key, generation
  and room rules.
- **Quake III.** `demos/four.dm_68`, Quake III 1.32's own demo, replayed with
  `timedemo 1`: 591 of its 778 frames draw shadowed dynamic lights. Twenty
  captures from frame 60 to 1200 are byte-identical with reuse on, off and on
  again, and to the build before both changes, in Vulkan; on and off in RTX's
  raster path. In GL 19 of 20 are; at frame 420 all three runs, the build
  before both changes against itself with reuse off among them, differ from
  one another by up to 4 levels in a 50 by 20 pixel corner of the HUD, which
  replays there do not repeat.
- **Medal of Honor.** The frozen `m1l1` frame with reuse on, off and on
  again is byte-identical in Vulkan and GL. The two headlights light nothing
  that frame shows: with `r_dynamiclight 0` it is identical too, so this
  shows no change to the scene but does not show a lit caster.

## FnQ features

Nothing a player sees changes. The dynamic-light shadows, their filtering,
biases and atlas stay as they were (invariant 11); this is the same work done
once.

## Not done

- GL's `r_vbo` defaults to 0, as in the initial commit, so GL tessellates the
  whole world on the CPU every frame; Vulkan and RTX default to 1. Changing
  the default reaches every profile and was not checked here.
- Grids and RTCW's and Enemy Territory's skeletal meshes still build per
  face.
- Alice's and F.A.K.K.2's `RB_SurfaceAliceTiki` tessellates per pass too; their
  shadow faces now copy it, but their view and lit passes do not share it.
- On `m1l1` the convoy's headlights light nothing the view shows, yet cost a
  shadow cube each. Whether their radius (`r_dlightScale` 0.5 of the client
  game's intensity) or the truck's own body around the lamp is the reason was
  not investigated.
- The user's VS Code launch configurations start the client under the
  `cppvsdbg` debugger; its cost was not measured.
