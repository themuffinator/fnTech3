# Alice's and F.A.K.K.2's models built once a frame, 2026-10-26

The [Medal of Honor frame-time record](2026-10-25-mohaa-frame-time.md) found
that the back end skinned each Medal of Honor model surface again for every
pass that drew it. Alice and F.A.K.K.2 did the same with their own models:
`RB_SurfaceAliceTiki` in all three renderers built each TIKI surface on the
CPU (`RA_TikiTessellateLod`) for the view, again for each dynamic light's lit
pass, for a crossfade's second skin and a custom shader's extra pass, for the
sun's cascades and for each dynamic light's shadow. Each surface is now built
once a frame for each level of detail it is drawn at, and the frame's other
passes copy it. The two games were not slow: the busiest views found here
take 2 to 9 ms a frame at 1280x720 on this machine's RTX 4060 laptop GPU,
and building their models took 0.3 to 0.43 ms of that. The store takes 0.08
to 0.19 ms a frame off those views, too little to show in `com_speeds`' whole
milliseconds against the machine's noise, and every frame compared is the
same with it on and off. The
[JSON companion](2026-10-26-ritual-model-frame-time.json) holds the
identities, commands and numbers. This is Source, Unit, Build and Live
evidence. No compatibility gate closes.

## How it was measured

Every probe ran fnTech3's client on a private desktop, windowed at
1280x720, with no input, the retail installations read-only (Alice at
`E:\Games\Alice`, F.A.K.K.2 laid down from its disc under
`.tmp/fakk2-install/game`), empty config files in an isolated home,
`com_maxfps 0` and `com_maxfpsUnfocused 0`, and `com_speeds 1` logging each
frame. Steps went in by `rcon` on loopback (`.tmp/alice-tiki/freeze.py`;
the runs, captures and logs are under `.tmp/alice-tiki/runs/`). Another
session's Medal of Honor probes and builds ran
on the machine throughout, so frame times vary by a few tenths of a
millisecond from case to case.

A frame is held still in F.A.K.K.2 with `timescale 0`, apart from the health
gauge in the top left corner, which the game animates in real time. Alice
does not hold under `timescale 0`: its third-person camera keeps moving in
real time, and `grounds1`'s opening script sets `timescale` back to 1 when its
cinematic ends. Alice's own `pause` holds every pixel still, its pause watch
included, and `rcon`, `com_speeds` and the store's switch go on working while
it does.

## Where the time went

Main-thread stacks sampled with `cdb -pv` (about 3 s a sample) while one
frame was held, the first look on the build before this change:

| View | Renderer | Samples | In `RA_TikiTessellateLod` |
| --- | --- | --- | --- |
| Alice `grounds1`, after its opening (`timescale 0`; the camera moved) | GL | 20 | 2 |
| Alice `grounds1`, after its opening (`timescale 0`; the camera moved) | Vulkan | 27 | 3 |
| F.A.K.K.2 `fakkhouse`, after its opening | GL | 16 | 0 |
| F.A.K.K.2 `fakkhouse`, after its opening | Vulkan | 22 | 2 |

and on the two busiest views, before and after, in Vulkan:

| View | Build | Samples | In `RB_SurfaceAliceTiki` | Of those, building |
| --- | --- | --- | --- | --- |
| Alice `grounds1`, paused after its opening | before | 40 | 8 | 8 |
| Alice `grounds1`, paused after its opening | after | 39 | 5 | 3 |
| F.A.K.K.2 `creeperpens`, `timescale 0` | before | 40 | 4 | 4 |
| F.A.K.K.2 `creeperpens`, `timescale 0` | after | 40 | 3 | 0 |

The rest went to drawing, the dynamic lights' shadow casters and their lit
passes. The four runs' frames, timed after their sampling holds, took 1.7
and 1.9 ms on Alice's view and 3.3 and 3.1 ms on `creeperpens`, before and
after: within the machine's noise, so the samples are read only for their
shares. (A first after-build pair resolved no renderer symbols, because the
DLLs name the build directory's PDB, since relinked; it was run again with
`cdb -y` at the runtime's own PDBs.)

A counting build (never committed; `.tmp/alice-tiki/diag_patch.py`) printed
how many TIKI surfaces the back end drew each frame and how many it had to
build with the store on, in Vulkan, unfrozen: through `grounds1`'s and
`fakkhouse`'s openings, and the other levels just after `skipcinematic`:

| View | Frame (ms) | Surfaces drawn | Built | Copied | Vertices built / copied |
| --- | --- | --- | --- | --- | --- |
| Alice `grounds1`, the griffin's fall in its opening | 2.6 | 99 | 35 | 64 | 4,095 / 14,382 |
| Alice `grounds1`, the Jabberwock's speech | 4.2 | 121 | 42 | 79 | 4,123 / 10,225 |
| Alice `grounds1`, the Jabberwock's fire on Alice | 4.4 | 128 | 42 | 86 | 4,123 / 12,040 |
| F.A.K.K.2 `fakkhouse`, its opening | 2.1 to 3.3 | 9 to 35 | 9 to 31 | 0 to 9 | about 1,000 to 2,400 / 0 to 900 |
| F.A.K.K.2 `towncenter_good` | 2.5 | 31 | 16 | 15 | 1,047 / 2,211 |
| F.A.K.K.2 `cemetery` | 4.2 to 4.4 | 26 | 21 | 5 | 1,617 / 612 |
| F.A.K.K.2 `homes1` | 4.3 | 133 | 77 | 56 | 4,712 / 5,485 |
| F.A.K.K.2 `creeperpens` | 5.9 to 6.7 | 202 to 205 | 78 | 124 to 127 | 2,518 / 6,284 to 6,377 |

The views with the most surfaces draw each two to three times a frame: the
lit passes of the levels' lights, the sun's cascades and the dynamic lights'
shadows, whose six faces already shared one build since
[the caster reuse](2026-10-25-mohaa-frame-time.md#a-lights-casters-built-once-for-its-six-faces).
Ritual's models are small -- the surfaces built in these views average 30
to 120 vertices -- so the busiest of these frames, the griffin's fall,
built about 18,500 vertices on the CPU before this change and 4,100 after.

## Built once a frame

[`tr_alice_tiki_cache.h`](../../code/renderercommon/tr_alice_tiki_cache.h)
keeps each surface built this frame. What `RA_TikiTessellateLod` writes
depends only on the surface, the entity's pose, its scale and the level of
detail. A skeletal model's pose is its skeleton, posed once a frame by
`RA_TikiPose` and good until `RA_TikiBeginFrame` starts the poses over; a
vertex-animated model's pose is the two frames and the lerp its entity
names. So the key is the surface, the skeleton's address, the animations,
frames and lerp, the entity's scale and the level of detail. A dynamic
light's shadow faces draw the whole surface (detail 1) and the views draw
the detail each chose (`ent->aliceLod`), so one surface may be kept twice in
a frame. `RA_TikiPoseGeneration` counts `RA_TikiBeginFrame`, and a new
generation drops every copy, so a skeleton's address is never trusted past
the frame it was posed in. A surface the store has no room for (1,024
surfaces, 65,536 vertices a frame) is built each time, as before. All three
renderers' `RB_SurfaceAliceTiki` use it; Medal of Honor's models keep their
own store, and no other profile draws these surfaces.

`r_ritualModelReuse` (default `1`, registered only for Alice and
F.A.K.K.2) builds every surface for every pass again when set to `0`, for
comparison. With it at `0` the function runs exactly the call the build
before this change made, so a frame with it on and off is the build before
and after, on one frame.

## What it saves

A second diagnostic build timed `RB_SurfaceAliceTiki` with
`ri.Microseconds()` and printed the mean over each 100 frames. Each run held
one frame and switched the store on, off, on, off, on for 6 s each; the
ranges are across a case's 100-frame means and across the cases of one
setting.

| View | Renderer | Surfaces drawn | Built, on / off | µs a frame, on | µs a frame, off |
| --- | --- | --- | --- | --- | --- |
| Alice `grounds1`, paused after its opening | GL | 128 | 42 / 128 | 244 to 303 | 398 to 427 |
| Alice `grounds1`, paused after its opening | Vulkan | 112 | 42 / 112 | 172 to 254 | 301 to 337 |
| Alice `grounds1`, paused in the opening's fire | Vulkan | 49 | 29 / 49 | 89 to 128 | 113 to 142 |
| F.A.K.K.2 `creeperpens` | GL | 204 | 78 / 204 | 229 to 254 | 424 to 429 |
| F.A.K.K.2 `creeperpens` | Vulkan | 204 | 73 / 204 | 213 to 236 | 399 to 406 |
| F.A.K.K.2 `homes1` | Vulkan | 133 | 77 / 133 | 238 to 287 | 335 to 350 |

Taking each setting's mean, the store saves 0.14 ms a frame on Alice's view
in GL and 0.11 ms in Vulkan, 0.02 ms on the lighter fire view, 0.19 and 0.18
ms on `creeperpens` and 0.08 ms on `homes1`. The time falls by less than the
work does: on `creeperpens` in Vulkan the store builds 2,441 vertices a frame
where the old path built 8,864, 72% fewer, and takes 45% less time. The
copies, the lookups and each draw's own work remain; how the rest divides
was not measured. The frames those runs timed with `com_speeds` took 1.4 to 5.5
ms, and the means of one setting's cases differed by up to 1.3 ms, so the
frame times do not show the saving; the clean build's own on and off cases
(below) moved the same way.

## The frames are the same

Each comparison held one frame and switched `r_ritualModelReuse` within the
run, on, off, on, off, on, with the engine's `screenshotPNG` after each
switch had settled. The runs used the clean build, not the diagnostic ones.

| View | Held by | Renderers | The five captures |
| --- | --- | --- | --- |
| F.A.K.K.2 `creeperpens` | `timescale 0` | GL, Vulkan, RTX raster (`rtx_rt_mode 0`) | byte-identical but for the health gauge, which differs as much between two captures of one setting |
| Alice `grounds1`, after its opening | `pause` | GL, Vulkan, RTX raster | byte-identical |
| Alice `grounds1`, the opening's fire | `pause` | Vulkan | byte-identical |

RTX's path tracer (`rtx_rt_mode 2`, its default) never draws the same frame
twice, so it shows nothing either way: two captures with the store on
differed as much as one on and one off. Alice held with `timescale 0` showed
nothing either, as its camera moved between captures. The frame times of
these runs, store on and off, overlap as the diagnostic build's did.

- **The store.** `tests/alice_tiki_cache_tests.py` compiles the header and
  checks its keep, copy, index rebasing, generation, room and full-probe
  rules, and that every part of the key matters. It also reads
  `RA_TikiTessellateLod` and checks that the only entity fields it reads are
  the key's (`anim`, `oldanim`, `frame`, `oldframe`, `backlerp`, `scale`),
  so a later change that reads another field fails the test, and that the
  three renderers copy before they build and keep what they build.
- **The tessellation.** `tests/alice_tiki_render_tests.cpp` builds a
  skeletal and a vertex-animated fixture through the real
  `RA_TikiTessellateLod`: an entity that differs in everything the key
  leaves out -- its torso and cross-blend animations, bone controllers,
  render flags, per-surface bytes and shader data -- builds the same
  triangles, bit for bit, from the same pose; a different frame, old frame,
  lerp or scale does not; two poses of one entity in one frame get two
  skeletons; and the generation moves on with `RA_TikiBeginFrame` and is
  never 0.
- **Builds.** The worktree's build with win32-dev's options (strict
  warnings, all three renderers) compiles it, and GCC i686 (`-Wall
  -Wextra`) compiles the three `tr_alice.c` and `tr_alice_tiki.cpp` without
  a warning.

## FnQ features

Nothing a player sees changes. The lighting, shadows and effects these
models are drawn with stay as they were (invariant 11); this is the same
work done once.

## Integration review

Commit `810eeb11` was fast-forwarded into the main checkout. A follow-up
review found that hashing an extreme module-supplied frame or animation
number could overflow signed arithmetic before conversion to unsigned.
The hash now converts each input before multiplying and adding, leaving
the key and tessellation unchanged. The cache fixture checks `INT_MIN`
and `INT_MAX` with signed-overflow trapping enabled; it trapped before
the correction and passes with it.

The strict-warning Windows x86 `meson/build/win32-dev` build compiled and
linked all three renderer modules and the native TIKI render fixture, which
passes. The 17 focused Python test files pass: 117 tests and 1,064 subtests.
The retained before/after runtime hashes match the companion, and
all 70 pairs of the seven recorded screenshot sets were checked again:
Alice's pixels match everywhere, and F.A.K.K.2's match outside its animated
health gauge. These are rechecks of the recorded captures, not new runtime
measurements of the hash correction.

## Not done

- No frame was compared across two builds: Alice's frames differ from run to
  run, and a pause lands on a different moment in each. The switch's `0`
  runs the old call, which is what makes the in-run comparison stand for
  one.
- Medal of Honor's store (`tr_mohaa_skin_cache.h`) and this one are two
  copies of one design with different keys; they could share one header.
- RTX's default path-traced mode ran F.A.K.K.2's `creeperpens` at 17 to 26
  frames a second where its raster path ran 136 to 159; that was not looked
  into.
