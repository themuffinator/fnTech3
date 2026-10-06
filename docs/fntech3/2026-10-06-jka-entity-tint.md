# Jedi Academy entity tint and forced alpha, 2026-10-06

The [attachments record](2026-10-06-jka-attachments.md) left fnTech3's
`jka-mp` client stopped in the client game's first frame after a map change,
at a plain model entity whose render flags carry 0x800, `Jedi Academy cgame
unsupported Raven renderer entity flags (type=0, flags=0x800)`, and later
0xc00. In Academy multiplayer 0x800 is `RF_RGB_TINT` and 0x400 is
`RF_FORCE_ENT_ALPHA`. This slice measures what retail `jamp.exe` 1.01's
renderer does with both, carries them across the client adapter as
engine-owned entity fields rather than Quake III flag aliases, and applies
them in all three Fn renderers. It also serves client-game import 201,
`CG_R_CLEARDECALS`. Live against retail `jamp.exe`, the client now passes
those frames; on `mp/ffa1` it completes `map_restart`, a reconnect and the
server quitting. The [JSON companion](2026-10-06-jka-entity-tint.json) holds
the identities, addresses, runs and results. No gate closes.

## Identities

| File | SHA-256 |
| --- | --- |
| `jamp.exe` 1.01 | `e8062d3948ff05917b48e47d8eec0ee43bcc6fb03924410712412404ddfee28b` |
| `base/assets3.pk3:cgamex86.dll` | `e5d5525829bfcaf3c0428d87fe804e5a46a75dcf0a141f8288061a3970ff140d` |
| `fntech3.exe` (runtime `jka-live-join`, rebuilt in place) | `69bec67629f480b21fb1d6bb776b792fa1df90e6e0a9ae0d1127c416e4f973c1` |
| `fnql_glx_x86.dll` | `f1a4d2b2fbad895f47f1af7bd565ffaf97ed657d73f68c4a8c1d710ef276ae56` |
| `fnql_vk_x86.dll` | `8ed401f60cba8c7b36e23b48995cdeaf271ef29c79c7597599a6d5f2532ba963` |
| `fnql_rtx_x86.dll` | `5fa42acc600da9fb058bfc9840dbbfa5358ba8015a8eca192840b4150e2ffb99` |

The runtime is HEAD `5603d244` plus this slice, built from a clean export
(MSVC 2022 x86, Meson `debugoptimized`, strict warnings, every development
gate on; all 1,499 targets, no warnings). The earlier binaries in that folder
matched the attachments record's hashes before they were replaced. Nothing
was written to the installation, and its inventory was unchanged after every
run.

## Observed in retail

Read with Capstone; no retail code is incorporated.

- **The stage iterator.** `RB_StageIteratorGeneric` (0x4af230, found by its
  log string at 0x55630c) runs the stage loop at 0x4aed70 over `tess`
  (0x1053760). For each stage (0x8c bytes, state bits at +0x78) the loop
  reads the current entity's render flags (entity at 0xfe2a64, flags at +4).
- **Tint.** Flag 0x800 makes the loop pass colour source 3, the entity's
  colour, to the colour function (0x4aef7e..0x4aef83, into 0x4ae280 at
  0x4aeff7), which then uses it in place of the stage's own (+0x58,
  0x4ae2c8). Its case 3 (0x4ae689) calls 0x4b0ba0, which writes the
  entity's four colour bytes (record +0x50) to every vertex. The alpha source
  stays the stage's own (+0x70, table 0x4ae9a8). An identity alpha
  (0x4ae70a) still writes 255, because it is skipped only for a source of
  identity (2) or of vertex (6) under an identity light of 1.0, and the
  forced source is neither. Colours are computed before the single- or
  multitexture branch (0x4af038), so both kinds of stage take the tint.
- **Forced alpha.** Only the single-texture branch reads 0x400 (0x4af172),
  after the distortion cases. It writes the entity's alpha (record +0x53) into
  every vertex's fourth byte (0x4aed40, with no test of the value) and draws
  with state 0x65, source-alpha blending with no depth write and no alpha
  test, in place of all the stage's own state; with 0x100000
  (`RF_ALPHA_DEPTH`) the state is 0x165, with depth write (0x4af18f). A
  multitexture stage (+0x24 set) goes to 0x4ac710 and keeps its state.
- **Draw order.** `RB_RenderDrawSurfList` (0x48aab0) holds back every
  surface of an entity whose flags carry 0x202400 (distortion 0x2000,
  forced post-render 0x200000, or 0x400), while fewer than 128 are held
  (0x48ab7b..0x48ac14). Once the rest of the list is drawn (0x48ae06) it
  draws them from the last held to the first, each in a batch of its own
  (0x48ae40..0x48b014). Forced-alpha surfaces write no depth, so this keeps
  later opaque surfaces from drawing over them.
- **Decal clear.** Client-game import 201 (0x412c71) calls only the
  renderer's decal clear (`call [0x8af074]`) and returns zero.

The same functions also read 0x20 (volumetric shading, 0x4ae2d8), 0x2000
(distortion), 0x20000 and 0x40000 (disintegration, 0x4aef72 and 0x4ae2ab),
0x100000 and 0x200000. They stay refused by name.

**The retail client game's items** (`cgamex86.dll` from `base/assets3.pk3`,
read from a read-only extraction): its item drawing hands import 202 an item
fading in after it spawns with 0x400 and an alpha rising from 1 to 255, then
the same item again with 0x800 and colour 0/200/85 (0x30033ff4..0x30034074);
a force-side-greyed item with 0xc00 and colour 150/150/150/200
(0x30033eb5..0x30033ed7); a greyed item's hologram with 0x800 (0x30033971);
and, with `cg_simpleItems` set, every item but the force boon as a white
sprite of radius 14 with 0x400 (0x300339c6..0x30033a5b).

**Reference** (OpenJK `1a6a643`, a maintained port): `codemp/rd-common/tr_types.h`
names the flags; `tr_shade.cpp`'s `RB_IterateStagesGeneric`, `ComputeColors`
and `ForceAlpha`, `tr_backend.cpp`'s post-render list and `cg_ents.c`'s
`CG_Item` agree with every reading above. Its `ComputeColors` also sets an
alpha shortcut that its alpha switch never reads; retail has no such step.
Only these facts are taken from it.

## What this slice serves

- **Engine-owned fields.** `refEntity_t` gains `jka.rgbTint` and
  `jka.forceAlpha`. The client adapter (`code/compat/jka_render_adapter.hpp`)
  sets them from 0x800 and 0x400 and gives neither a Quake III flag; every
  other unserved flag is still refused by name. Plain entities, Ghoul2
  instances and UI models all cross through the same adapter.
- **Colour source.** In each renderer's `R_ComputeColors`, a tinted entity's
  first bundle, which holds the stage's one colour in retail, takes the
  entity colour in place of its own, and an identity alpha then writes 255,
  as retail's does. The Vulkan and RTX renderers bake the colour source into
  pipelines, so a tinted stage whose pipeline would take a constant colour or
  none is drawn with the vertex-colour pipeline of the same kind (single
  texture, or the multiply or add of two).
- **Forced alpha.** A single-texture stage of such an entity takes the
  entity's alpha at every vertex and is drawn with state 0x65, through the
  same branch as Elite Force's forced alpha, whose state is the same.
- **Draw order.** `R_SortDrawSurfs` moves the surfaces of forced-alpha
  entities after the rest of each view's sorted list, so they draw last.
- **Static buffers.** A surface drawn for an entity with either override
  never takes the world's static vertex buffer, whose colours are fixed.
- **Path tracer.** The RTX path tracer's materials are per shader, so it
  leaves such entities to raster, which draws them with their overrides.
- **Import 201.** No Academy decal reaches Fn (import 205 stays refused by
  name), so its pool is always empty: the import returns zero and clears
  nothing.

`code/renderercommon/tr_jka_entity.h` holds the shared rules. Shadow-map
passes take neither override.

**Not changed, for other profiles.** Only Academy's adapter sets the new
fields, so every other profile's entities keep them zero and draw as before.
Elite Force's forced alpha now shares its branch with Academy's and keeps its
own rules: the alpha from 1 to 254, taken when the batch is flushed.

## Presentation (invariant 11)

The tint is the client game's colour for the entity, data the module hands
the engine, and replaces the stage's colour source as retail's does,
including its light-grid shading. Fn's own features still apply: Fn
materials and shaders, the per-pixel dynamic-light passes added over the
base, fog, shadows (an overridden entity still casts), and the canonical
FOV, HUD, capture and liquid hooks. Forced alpha turns off depth fade, GLx
streamed drawing and the depth-fragment pass for the stages it changes, as
Elite Force's does. Fn draws held-back surfaces in its own sort order, with
no cap; retail's reverse order and 128-surface limit are presentation and are
not reproduced. Under RTX path tracing these entities are drawn by raster, so
they cast no traced shadow. Enemy-highlight shells copy the entity, overrides
included, as they do for Elite Force. No retail-exact option is added.

## Live sessions

`scripts/probe_legacy_sessions.py --profiles jka-mp --directions join
--runtime .tmp/runtime/jka-live-join --sessions main kick`. The fnTech3
client runs against retail `jamp.exe` on loopback, windowed on a private
desktop, with pure admission and no sound or input.

| Run | Maps | Steps |
| --- | --- | --- |
| `sess-1` | `mp/ffa1`, `mp/ffa3` | join, client chat, server chat, **map change** pass; `map_restart`, reconnect, server quit fail; kick passes |
| `sess-2-ffa1` | `mp/ffa1`, `mp/ffa1` | join, both chats, map change, **`map_restart`, reconnect, server quit** and kick all pass |

The second run uses the same binaries and command through
`.tmp/jka-live/probe_variant.py --maps mp/ffa1,mp/ffa1`; nothing in it is a
diagnostic build. In the first, the flagged entities on `mp/ffa3`'s first
frame no longer stop the client. The same frame stops later, at the map's
present ambient set: `Jedi Academy cgame active ambient playback is not
served (name=city_outdoor)`, client-game import 48. In the second:

- **`map_restart`**: the client game's restart, which stopped at import 201
  in the attachments record's diagnostic run, now carries on.
- **Reconnect**: an explicit `connect` restarts the renderer, reloads the UI
  module and the client game, and takes a first snapshot.
- **Server quit**: the client prints `Server quit`, takes `disconnect`,
  unloads the client game and stays up at the menu.

Diagnostic captures, with the committed binaries, used a variant that loads
the second map with `devmap` so the spectator can be placed beside the
bowcaster with `setviewpos`, all through the client's own console. With
`cg_simpleItems 1` the client game hands the item over as its icon sprite
with 0x400, and GLx, Vulkan and RTX each draw it, read from each renderer's
own `screenshot`. Joining as a player (`team free`) stops at the first sound
the client game starts, `Unsupported jka-mp cgame import 37
(CG_S_STARTSOUND)`, so no capture of a tinted entity, such as the
placeholder a taken weapon leaves, was taken.

## Tests

- `tests/jka_entity_override_tests.py`:
  - the adapter: each flag to its own field, neither to a Quake III flag,
    sprites included, every other retail flag refused;
  - each renderer's real `R_ComputeColors`: the entity colour under a tint,
    the source never computed, an identity alpha of 255 even for a vertex
    stage under identity light, the stage's own entity, skip, vertex and
    constant alphas, the second bundle of a Vulkan stage untouched, and
    shadow passes and a missing entity untinted;
  - forced alpha on single-texture stages only, at any value from 0 to 255,
    never in shadow passes;
  - the draw-order move: stable, world and other entities in place, stale
    entity numbers ignored, nothing allocated left over;
  - the Vulkan and RTX tint remap over every real pipeline type (14 remapped,
    the rest unchanged, the stage's state, fog and mirror kept);
  - the path tracer leaving tinted and faded entities to raster;
  - where each renderer applies them, and import 201 returning zero.

  All 19 deliberate mutations of the served semantics fail it.
- `tests/jka_render_tests.cpp` and `tests/jka_cgame_render_tests.py` now
  expect both flags to be served, and still refuse the others.
- `tests/stvef_forced_alpha_tests.py` follows the shared branch.

On the export of HEAD `5603d244` with this slice, Meson's 249 project tests
other than the pytest wrapper give 244 passes and 2 skips for this machine,
with the two tests above failing on the flags they then refused; with them
updated both pass. `fntech3_jka_pose_host` exceeded Meson's 30-second limit
in the parallel run, and passes alone in 27.1 s on a loaded machine. On an
export of HEAD `2fcbcf3d` with this slice, the Python suite passes 2,550
tests with 130 skipped.

## What stays open

- Present ambient sets (import 48), the first sound a joined player's client
  game starts (import 37), decals (import 205), and the flags still refused:
  volumetric, distortion, disintegration, alpha depth and forced post-render.
- A capture of a tinted entity, and of an attached weapon.
- Retail's reverse post-render order and its 128-surface limit (presentation,
  not reproduced).
