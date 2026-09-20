# Credits

- [zlib](https://zlib.net/), by Jean-loup Gailly and Mark Adler, supplies the
  maintained DEFLATE decoder used to load pak assets and PNG images. The
  inherited minizip archive frontend retains Gilles Vollant's attribution
  in `unzip.c`.

## Project lineage

fnTech3 combines two GPL id Tech 3 engine lines into one profile-driven
codebase:

- [FnQuake3](https://github.com/themuffinator/FnQ3) supplies the Q3-oriented
  modernization lineage. The merger reference is pinned at
  [`7d8c9629cdc7979f09697266470359101ffd9eda`](https://github.com/themuffinator/FnQ3/commit/7d8c9629cdc7979f09697266470359101ffd9eda).
- [FnQL](https://github.com/themuffinator/FnQL) supplies the QL-oriented engine
  compatibility baseline. The imported reference is pinned at
  [`fce44fe714564621e6ceb79e3da7c53089646af2`](https://github.com/themuffinator/FnQL/commit/fce44fe714564621e6ceb79e3da7c53089646af2).

The FnQuake3 worktree had uncommitted renderer, documentation, build, and test
changes when the merger began. fnTech3 records only the pinned committed
baseline lineage; that dirty work was not blindly imported. Individual ideas
or changes may be ported later after review, attribution, and profile-specific
regression coverage.

The lineage above continues through:

- id Software's [Quake III Arena source
  release](https://github.com/id-Software/Quake-III-Arena), the original GPL
  engine foundation.
- [ioquake3](https://github.com/ioquake/ioq3), the community-maintained Quake
  III continuation.
- [Quake3e](https://github.com/ec-/Quake3e), the modern engine foundation used
  by FnQuake3.

These engine sources use GPL-2.0-or-later headers and are compatible with the
repository's GPL license. Their original file headers and provenance comments
remain authoritative.

## Compatibility references and adapted work

- [FnQuake3's HUD implementation](https://github.com/themuffinator/FnQ3/blob/7d8c9629cdc7979f09697266470359101ffd9eda/code/client/cl_hud.cpp)
  supplies the restored layout rules, 3D HUD viewport placement, shader-name
  matching, and grouped draw diagnostics under GPL-2.0-or-later, compatible
  with this repository. fnTech3 adapts that implementation for its immutable
  Q3/RTCW profiles, bounded JSON loading, profile-scoped package lookup, and
  RTCW gradient/rotated draws. QL is excluded. The existing independently
  implemented FnQuake3 FOV behavior is extended to both RTCW profiles.

- RTCW's separate SP/MP HUD presets are independently authored from local
  retail draw metadata, stock menu layout observations, and the published
  [SP cgame](https://github.com/id-Software/RTCW-SP/tree/master/src/cgame) and
  [MP cgame](https://github.com/id-Software/RTCW-MP/tree/master/src/cgame)
  coordinate/shader contracts. This is reference-only use: no GPLv3 source
  implementation or retail menu text is incorporated in the JSON presets.

- The reviewed [FnQuake3 package assets](https://github.com/themuffinator/FnQ3/tree/7d8c9629cdc7979f09697266470359101ffd9eda/pkg)
  supply Q3 and Team Arena audio-zone, fog, and sound-tuning data, plus the
  retained legacy HUD preset, under `pkg/games/quake3/`. The package subtree
  matches the pinned GPL-2.0-or-later merger baseline. Asset contents and
  provenance comments are preserved; two sound-shader filenames are mapped
  to fnTech3's existing consumer path. The [92-file inventory and hashes](docs/fntech3/2026-09-17-fnq3-package-assets.json)
  distinguish this data import from engine implementation work. No retail
  samples, maps, or modules are included.

- id Software's [RTCW SP](https://github.com/id-Software/RTCW-SP) and
  [RTCW MP](https://github.com/id-Software/RTCW-MP) source releases, with
  [iortcw](https://github.com/iortcw/iortcw) as a cross-check, supply ABI,
  protocol identity, message field/dictionary, module name/call-number, cvar and
  client-record and menu/key/event encodings, mode-command names, BSP disk layouts and collision handles,
  configuration and archive-order
  facts for fnTech3's independently authored RTCW game-manager, boundary and
  message-codec work. These GPLv3
  references remain reference material; no retail game logic or reference
  engine implementation is incorporated by the
  [contract foundation](docs/fntech3/2026-09-14-rtcw-contracts.md).

- Xiph's [Vorbis output-channel documentation](https://xiph.org/vorbis/doc/vorbisfile/ov_read.html)
  supplies channel-order facts for the independently implemented Ogg-to-speaker
  permutation. No Xiph implementation text was incorporated for that mapping.

- RTCW MP's native-module pak references and pure-proof contract use observed
  behavior from the original [MP filesystem](https://github.com/id-Software/RTCW-MP/blob/master/src/qcommon/files.c),
  [MP server](https://github.com/id-Software/RTCW-MP/blob/master/src/server/sv_client.c)
  and [iortcw](https://github.com/iortcw/iortcw). The profile policy and pinned
  module validation are independently authored. Original functions run only
  in disposable fixtures with their GPLv3 notices; no reference implementation
  text is incorporated. The [pure checkpoint](docs/fntech3/2026-09-15-rtcw-mp-pure.md)
  records deliberate parser differences and the fixture's architecture scope.

- The shared RTCW fog controller uses observed slot, transition and clip behavior
  from the original [SP renderer](https://github.com/id-Software/RTCW-SP/blob/master/src/renderer/tr_main.c),
  [MP renderer](https://github.com/id-Software/RTCW-MP/blob/master/src/renderer/tr_main.c)
  and [iortcw](https://github.com/iortcw/iortcw). SP and MP differences remain
  explicit. Production code is independently authored; reference functions and
  their GPLv3 notices are used only in disposable comparison fixtures. The
  [controller checkpoint](docs/fntech3/2026-09-15-rtcw-fog-controller.md) records
  its evidence and the unfinished GPU boundary.

- RTCW entity/listener sound imports are independently adapted from the original
  [SP client](https://github.com/id-Software/RTCW-SP/blob/master/src/client/cl_cgame.c)
  and [MP client](https://github.com/id-Software/RTCW-MP/blob/master/src/client/cl_cgame.c)
  contracts. Their sound-entity limits and copying behavior inform the checked
  boundary; no reference implementation text is incorporated. The
  [spatial-audio checkpoint](docs/fntech3/2026-09-15-rtcw-spatial-audio.md)
  distinguishes import evidence from playback parity.

- RTCW MP looping-sound submissions and range/volume spatialization are
  independently authored from the original
  [MP sound system](https://github.com/id-Software/RTCW-MP/blob/master/src/client/snd_dma.c).
  The shared policy extends the inherited legacy and OpenAL backends. Its
  comparison fixture preserves the original functions and GPLv3 notices in
  disposable output only. The [loop checkpoint](docs/fntech3/2026-09-15-rtcw-mp-loop-audio.md)
  records the difference from iortcw's per-entity loop storage and the remaining
  SP fade and output-device requirements. MP's interpolated-viewpoint import
  independently follows its client module contract.

- RTCW SP sound-reset modes, submission-time loop fades and mixer-volume
  quantization independently follow the original
  [SP sound system](https://github.com/id-Software/RTCW-SP/blob/master/src/client/snd_dma.c)
  and [SP mixer](https://github.com/id-Software/RTCW-SP/blob/master/src/client/snd_mix.c).
  The [SP loop checkpoint](docs/fntech3/2026-09-16-rtcw-sp-loop-audio.md) records
  source comparisons and both-backend checks. The game manager's pure-mode
  defaults/flags follow the original
  [SP server](https://github.com/id-Software/RTCW-SP/blob/master/src/server/sv_init.c)
  and [MP server](https://github.com/id-Software/RTCW-MP/blob/master/src/server/sv_init.c),
  corroborated by [iortcw](https://github.com/iortcw/iortcw). These are behavior
  observations; no GPLv3 implementation text is incorporated into the engine.

- RTCW SP/MP one-shot sound flags, channel reuse, free-list ordering and linear
  spatialization independently follow observed contracts in the original
  [SP sound system](https://github.com/id-Software/RTCW-SP/blob/master/src/client/snd_dma.c)
  and [MP sound system](https://github.com/id-Software/RTCW-MP/blob/master/src/client/snd_dma.c).
  The [one-shot checkpoint](docs/fntech3/2026-09-16-rtcw-one-shot-audio.md)
  records reference comparisons, safe allocation differences and remaining
  output/streaming requirements. Original GPLv3 functions run only in temporary
  comparison fixtures with their notices; production policy and adapters are
  independently authored under GPL-2.0-or-later.

- RTCW MP limbo history and SP/MP cinematic import adapters independently use
  contracts observed in the original
  [MP client](https://github.com/id-Software/RTCW-MP/tree/master/src/client),
  [SP client](https://github.com/id-Software/RTCW-SP/tree/master/src/client)
  and [iortcw](https://github.com/iortcw/iortcw). Reference history functions
  run only in disposable GPLv3 comparison fixtures. The
  [limbo/cinematic checkpoint](docs/fntech3/2026-09-15-rtcw-limbo-cinematics.md)
  records colour-predicate differences and the unfinished cinematic gates.
  The [startup/movie checkpoint](docs/fntech3/2026-09-17-rtcw-cinematic-presentation.md)
  adds independently implemented catalog startup sequences, SP letterboxing and
  its header frame rate, using those client sources and the original common
  startup code as behavioral evidence. Shared clock and missing-movie recovery
  changes are independently authored; no reference movie decoder is copied.
  The [RoQ bounds checkpoint](docs/fntech3/2026-09-17-roq-bounds.md) independently
  corrects framing, codebook counts and recovery. Installed movie metadata and
  [FFmpeg's RoQ decoder](https://github.com/FFmpeg/FFmpeg/blob/master/libavcodec/roqvideodec.c)
  establish the length-dependent zero-count interpretation; FFmpeg implementation
  text is not incorporated. Original SP/MP sources also establish the preserved
  JPEG-chunk no-op policy.

- RTCW material compression directives and image exemptions independently use
  behaviour observed in the original
  [MP renderer](https://github.com/id-Software/RTCW-MP/tree/master/src/renderer)
  and [SP renderer](https://github.com/id-Software/RTCW-SP/tree/master/src/renderer).
  No reference implementation text is incorporated. The
  [material checkpoint](docs/fntech3/2026-09-15-rtcw-material-compression.md)
  records per-material isolation and the remaining fog and image-parity gates.

- RTCW bot-library initialization and AAS8 layout/encoding use behavior facts
  from the original [SP bot library](https://github.com/id-Software/RTCW-SP/tree/master/src/botlib),
  [MP bot library](https://github.com/id-Software/RTCW-MP/tree/master/src/botlib)
  and [iortcw](https://github.com/iortcw/iortcw). The profile policy, checked
  bootstrap boundary, immutable AAS8 reader, live record conversion and
  two-world loading/blocking contracts
  are independently authored. The blocking comparison fixture preserves the
  original SP/MP versus iortcw SP reset-predicate distinction.
  Original point-query functions run only in disposable reference fixtures
  with their GPLv3 notices; no GPLv3 implementation text is incorporated here.
  The [live-loading checkpoint](docs/fntech3/2026-09-15-rtcw-aas-loading.md)
  distinguishes geometry loading from the unfinished AI/routing integration.
  The [BSP-query checkpoint](docs/fntech3/2026-09-16-rtcw-bsp-queries.md) records
  independently implemented string padding, numeric parsing and checked module
  imports, with original/iortcw functions confined to comparison fixtures.
  The [routing-cache checkpoint](docs/fntech3/2026-09-16-rtcw-routing-caches.md)
  records the independently authored RCD12/15 decoder, visibility expansion,
  map validation and per-world native conversion. Original/iortcw cache and
  visibility readers are likewise used only in disposable GPLv3 fixtures.
  The [routing-initialization checkpoint](docs/fntech3/2026-09-16-rtcw-routing-initialization.md)
  records independent frame scheduling, per-profile slope/rounding rules and
  bounded graph/cost tables, with original/iortcw functions confined to
  disposable comparisons and their floating-point evaluation limits stated.
  The [entity checkpoint](docs/fntech3/2026-09-16-rtcw-ai-entities.md) adds
  independently authored record conversion, shared entity ownership and
  per-profile AI client-slot policy. Entity update behavior is compared with
  original SP/MP and iortcw functions only in disposable GPLv3 fixtures; the
  inherited GPLv2-or-later implementation remains the engine's code base.
  The [AI state checkpoint](docs/fntech3/2026-09-16-rtcw-ai-state.md) adds
  profile-sized goal/movement pools, frozen input conversion and distinct
  avoidance reset/initialization semantics. Original/iortcw comparisons stay
  in disposable fixtures; the original last-avoidance reset's out-of-bounds
  read is explicitly excluded, with iortcw's bounded behavior verified instead.
  The [cast/action checkpoint](docs/fntech3/2026-09-16-rtcw-cast-actions.md)
  adds independently authored cast-message policy and action/AI-command
  boundaries. Its comparisons preserve original and iortcw action functions
  only in disposable GPLv3 fixtures, including RTCW's aliased action flags.
  The [AAS/tag checkpoint](docs/fntech3/2026-09-16-rtcw-aas-tags.md) adds checked
  basic query imports and an independently written server-to-cgame tag bridge.
  Query comparisons retain original/iortcw functions only in disposable
  GPLv3 fixtures; production uses the inherited engine algorithms.
  The [cast lifecycle checkpoint](docs/fntech3/2026-09-16-rtcw-cast-lifecycle.md)
  records independently implemented SP snapshot, timeout and disconnect
  policies observed in the original and iortcw servers.
  The [area-trace checkpoint](docs/fntech3/2026-09-16-rtcw-aas-traces.md)
  adds checked trace buffers around the inherited traversal. Original/iortcw
  functions remain confined to disposable GPLv3 comparison fixtures.
  The [route-query checkpoint](docs/fntech3/2026-09-16-rtcw-route-queries.md)
  adds independently written RTCW cache-update, expiry and route-selection
  policies around the inherited routing engine, with a checked module import.
  Original/iortcw functions execute only in disposable GPLv3 fixtures with
  independent mutable caches and queues; installed navigation stays in memory.
  The [visibility checkpoint](docs/fntech3/2026-09-16-rtcw-visibility.md)
  adds independently authored cached visibility and SP route-position queries,
  and distinct SP/MP neighborhood settings in the inherited fuzzy-area search.
  Original SP/MP and iortcw bodies remain in disposable GPLv3 fixtures only;
  the production code retains its GPLv2-or-later license.
  The [cast-query callback checkpoint](docs/fntech3/2026-09-16-rtcw-cast-query-callbacks.md)
  independently implements the original SP/MP server-to-game visibility and
  attack callback contracts. Source bodies are observations, not imported code;
  native-dispatch fixtures use a synthetic game entry point.
  The [tactical navigation checkpoint](docs/fntech3/2026-09-16-rtcw-tactical-navigation.md)
  independently implements original SP/MP hide/attack policies with separate
  search storage and checked imports. GPLv3 reference functions run only in
  disposable comparisons; source occupancy/math differences and cold-cache
  divergences are recorded explicitly.
  The [movement-prediction checkpoint](docs/fntech3/2026-09-16-rtcw-movement-prediction.md)
  uses original SP/MP physics, event-order and layout observations to extend
  the inherited GPLv2-or-later movement engine and independently implement its
  checked result adapter. GPLv3 reference bodies remain in disposable fixtures;
  original undefined trace paths, iortcw policy differences and compiler
  arithmetic differences are recorded separately.
  The [installed-prediction comparison](docs/fntech3/2026-09-16-rtcw-installed-prediction.md)
  combines those disposable references with the engine's own AAS loader and
  BSP contents service; installed inputs remain in memory and are not shipped.
  The [movement-area checkpoint](docs/fntech3/2026-09-16-rtcw-movement-areas.md)
  adds a checked reachability lookup and independently implements original
  SP disabled-area and RTCW model-travel policies. Its GPLv3 reference bodies
  remain confined to disposable comparisons over synthetic geometry.
  The [directional-movement checkpoint](docs/fntech3/2026-09-16-rtcw-directional-movement.md)
  independently extends inherited movement code using original grounded-flag,
  slime, normalization and setup-setting observations. Original/iortcw GPLv3
  bodies execute only in disposable fixtures sharing engine AAS and action
  services; failed-prediction safety and compiler differences are explicit.
  The [movement-view checkpoint](docs/fntech3/2026-09-16-rtcw-movement-views.md)
  adds independently authored SP/MP next-link policies and checked goal/view
  boundaries to inherited movement code. Reference GPLv3 bodies stay in
  disposable comparisons with separate engine routing worlds; iortcw's changed
  SP selector signature and normalization remain diagnostic differences.
  The [travel-execution checkpoint](docs/fntech3/2026-09-16-rtcw-travel-execution.md)
  independently adds SP/MP travel, ladder, mover and hook policies. Disposable
  original/iortcw comparisons share engine services and explicitly exclude
  undefined original result fields; no GPLv3 implementation text is imported.
  The [movement-to-goal checkpoint](docs/fntech3/2026-09-16-rtcw-movement-to-goal.md)
  independently extends shared route orchestration, previous-area ladder
  detection and SP/MP look-ahead, with explicit goal/result bit mappings.
  Disposable reference programs preserve caller-owned optional fields while
  excluding undefined helper-local fields from comparisons.
  The [jump run-up checkpoint](docs/fntech3/2026-09-16-rtcw-jump-runstart.md)
  adds independently authored RTCW slime/normalization policy and failure
  propagation. Original/iortcw helper bodies remain in disposable fixtures;
  failed predictions and nonfinite reference results are explicitly excluded.
- RTCW server defaults, campaign allocation and world-import arguments use
  behavior facts from the original [SP server](https://github.com/id-Software/RTCW-SP/tree/master/src/server)
  and [MP server](https://github.com/id-Software/RTCW-MP/tree/master/src/server).
  The profile policies and checked adapters are independently written; world
  queries retain fnTech3's existing GPL-2.0-or-later implementation. No GPLv3
  reference implementation text or game logic is incorporated. The
  [server checkpoint](docs/fntech3/2026-09-15-rtcw-server-world.md) records scope.
  The [entity-collision checkpoint](docs/fntech3/2026-09-16-rtcw-entity-collision.md)
  independently adds RTCW's entity-flag hull policy. Original and iortcw
  selectors remain confined to disposable GPLv3 comparisons; the production
  collision engine and Quake profile policies are retained.

- The independently authored synthetic Q3 UI fixture uses public ordinal/layout
  facts from the GPL-2.0-or-later pinned
  [FnQuake3 UI header](https://github.com/themuffinator/FnQ3/blob/7d8c9629cdc7979f09697266470359101ffd9eda/code/ui/ui_public.h).
  It implements no retail menu behavior and uses no proprietary inputs.

- The synthetic Q3 native cgame probe independently exercises the legacy ABI
  using ordinal and layout facts from the GPL-2.0-or-later pinned
  [FnQuake3 cgame header](https://github.com/themuffinator/FnQ3/blob/7d8c9629cdc7979f09697266470359101ffd9eda/code/cgame/cg_public.h).
  It contains no reconstructed or incorporated retail game behavior.

- Frozen renderer config layouts use the GPL-2.0-or-later
  [FnQuake3 renderer header](https://github.com/themuffinator/FnQ3/blob/7d8c9629cdc7979f09697266470359101ffd9eda/code/renderercommon/tr_types.h)
  and [FnQL retail config adapter](https://github.com/themuffinator/FnQL/blob/fce44fe714564621e6ceb79e3da7c53089646af2/code/client/cl_cgame.cpp).
  Their layouts are compatible with this repository license; the new Q3 field
  adapter is independently authored and the existing QL translation is retained.

- Frozen Q3/QL scene and entity records use layout facts from the pinned
  [FnQuake3 renderer header](https://github.com/themuffinator/FnQ3/blob/7d8c9629cdc7979f09697266470359101ffd9eda/code/renderercommon/tr_types.h)
  and [FnQL renderer header](https://github.com/themuffinator/FnQL/blob/fce44fe714564621e6ceb79e3da7c53089646af2/code/renderercommon/tr_types.h).
  Both references use GPL-2.0-or-later. Their field adapters are independently
  authored, as are the RTCW scene/animation/fog adapters and basic renderer
  import routing and 2D quad adapters informed by the original SP/MP renderer
  contracts linked above. The local 2D comparison probe compiles reference
  functions only in disposable fixtures with their notices intact. No RTCW rendering
  implementation is incorporated by this boundary work.

- The independent MDC reader and animation decoder use format facts from the
  original [RTCW SP](https://github.com/id-Software/RTCW-SP) and
  [RTCW MP](https://github.com/id-Software/RTCW-MP) releases and
  [iortcw](https://github.com/iortcw/iortcw): byte layouts, frame maps, coordinate
  scales, direction encodings and named culling conventions. The normal
  dictionary is constructed geometrically with recorded encoding-rounding
  residues. The GPL-3.0-or-later reference implementations are compiled only
  in disposable local comparison fixtures, retaining their notices; their
  implementation text is not incorporated in the engine. The shared mesh
  integration extends fnTech3's existing GPL-2.0-or-later renderer code.

- The independently authored indexed MD3/MDC attachment path uses RTCW's
  entity-based call, inclusive tag-index and interpolation contract facts from
  the original SP/MP releases linked above. Its comparison probe compiles the
  original and iortcw tag functions only in disposable local fixtures, with
  their notices intact. The existing fnTech3 math and legacy tag implementation
  remain GPL-2.0-or-later; no GPLv3 implementation text is incorporated.

- The independent MDS reader, pose decoder, owned renderer models and
  continuous detail selection use skeletal format and behavior facts from the
  original [RTCW SP](https://github.com/id-Software/RTCW-SP),
  [RTCW MP](https://github.com/id-Software/RTCW-MP) and
  [iortcw](https://github.com/iortcw/iortcw) headers and renderers. Local probes
  compare decoded records, poses, weighted vertices, skeletal attachments and
  reduced triangle lists against the references and legitimate installed data
  in memory, including partially initialized foot-tag queries. GPL-3.0-or-later
  reference functions are compiled only in disposable
  fixtures with their notices intact. fnTech3's implementation and synthetic
  fixtures are independently written; no reference implementation text or
  retail game assets are incorporated.

- The bounded RTCW cgame-to-game animation-speed bridge uses import/export
  contract observations from the original
  [SP](https://github.com/id-Software/RTCW-SP) and
  [MP](https://github.com/id-Software/RTCW-MP) releases. MP loop-clear and
  stop-loop dispatch also follow observed source contracts. The bridge and
  synthetic tests are independently authored; no GPLv3 game implementation
  text is incorporated.

- RTCW startup appearance defaults, selected-world queries, empty item updates
  and the opaque SP animation-model handoff use source-contract observations
  from original [SP](https://github.com/id-Software/RTCW-SP),
  [MP](https://github.com/id-Software/RTCW-MP) and [iortcw](https://github.com/iortcw/iortcw).
  The original SP zombie-hit import is deliberately disabled; iortcw's enabled
  version is a distinct behavior. The [startup checkpoint](docs/fntech3/2026-09-16-rtcw-startup-contracts.md)
  records that distinction, independently authored adapters and tests, and
  source/runtime evidence. No external implementation text is incorporated.

- RTCW's silent null/empty model-registration result follows behavioral
  observations of the original
  [SP renderer](https://github.com/id-Software/RTCW-SP/blob/master/src/renderer/tr_model.c)
  and [MP renderer](https://github.com/id-Software/RTCW-MP/blob/master/src/renderer/tr_model.c).
  The bridge correction and regression checks are independently authored;
  no GPLv3 implementation text is incorporated. See the
  [empty-model checkpoint](docs/fntech3/2026-09-17-rtcw-empty-model-names.md).

- The frozen Q3/QL configstring transfer contracts use layout facts from the pinned
  [FnQuake3 q_shared.h](https://github.com/themuffinator/FnQ3/blob/7d8c9629cdc7979f09697266470359101ffd9eda/code/qcommon/q_shared.h)
  and [FnQL q_shared.h](https://github.com/themuffinator/FnQL/blob/fce44fe714564621e6ceb79e3da7c53089646af2/code/qcommon/q_shared.h).
  These GPL-2.0-or-later references are compatible with this repository; their
  provenance remains in the boundary headers. The field adapters are independently authored.

- The [PKWARE ZIP specification](https://pkware.cachefly.net/webdocs/casestudies/APPNOTE.TXT),
  version 6.3.10, supplies central-directory layout information for the independently
  authored early pak identity probe. No external implementation or specification
  text was incorporated; identity constants come from local retail metadata observations.

- The WebUI request notifications, bounded bulk request transport, avatar retry
  queue, and associated tests/probe are adapted from the GPL-2.0-or-later
  [FnQL WebUI work](https://github.com/themuffinator/FnQL). These were reviewed
  uncommitted updates on 2026-09-06, after the pinned merger baseline, and are
  incorporated implementation rather than behavioral reference only. The
  [port record](docs/fntech3/2026-09-06-webui-request-port.md) pins the source
  snapshot, preserves the Q3/QL profile boundary, and records fnTech3 validation.
- The Q3 viewport-FOV policy is independently implemented from the pinned
  [FnQuake3 renderer behavior](https://github.com/themuffinator/FnQ3/blob/7d8c9629cdc7979f09697266470359101ffd9eda/code/renderer/tr_main.c).
  It applies only to the Q3 profile; QL retains its module-authored projection.
- The Q3 cgame extension ordinals `112..116` and discovery names were
  cross-checked against the pinned GPL-2.0-or-later FnQuake3
  [`cg_public.h`](https://github.com/themuffinator/FnQ3/blob/7d8c9629cdc7979f09697266470359101ffd9eda/code/cgame/cg_public.h)
  and [`cl_cgame.cpp`](https://github.com/themuffinator/FnQ3/blob/7d8c9629cdc7979f09697266470359101ffd9eda/code/client/cl_cgame.cpp).
  fnTech3's profile remapping is independently written; the header records the
  source of these boundary constants.
- [Quake Live Source Reconstruction Project
  (QLSRP)](https://github.com/themuffinator/QL-SRP) provides behavioral, ABI,
  protocol, and format evidence for independently written QL engine work; its
  implementation text is not treated as an import.
- [CPMADevs CNQ3](https://bitbucket.org/CPMADevs/cnq3) is credited for the
  rainbow text-color escape lineage.
- [Spearmint](https://github.com/zturtleman/spearmint) is credited for the
  `bsp_q3ihv.c`-based Quake 3 IHV / `IBSP v43` map-loading reference.
- [WolfcamQL](https://github.com/brugal/wolfcamql) is used to cross-check legacy
  demos and QL-adjacent compatibility behavior.
- Luigi Auriemma's [QuakeLive beta files decoder
  (`quakelivedec`/qldec)](https://aluigi.altervista.org/papers.htm#qldec) is the
  GPL-2.0-or-later source of the published beta `.pk3` XOR table retained in
  `code/qcommon/quakelive_pk3_xor.h`. See the [third-party
  notice](THIRD_PARTY_NOTICES.md).
- [DarkMatter-Q2](https://github.com/Paril/DarkMatter-Q2) is used to cross-check
  Quake II `.pak` and `.wal` support.
- Mikko Mononen's [FontStash](https://github.com/memononen/fontstash) and Sean
  Barrett's `stb_truetype` are credited for the pinned historical rasterizer
  used by the host-text path; their source headers carry the applicable terms.

## RTX rendering references

The RTX renderer's independently written scene-transport work uses
[NVIDIA Q2RTX](https://github.com/NVIDIA/Q2RTX/tree/f2526e9a165949f66e91e82f0d63aa7bb2567b4d)
as a feature and behavior reference. No Q2RTX implementation text or assets
were incorporated in this round. Standard scattering and sampling equations
were checked against *Physically Based Rendering*'s
[reflection sampling](https://www.pbr-book.org/3ed-2018/Light_Transport_I_Surface_Reflection/Sampling_Reflection_Functions)
and [dielectric reflection/transmission](https://www.pbr-book.org/4ed/Reflection_Models/Specular_Reflection_and_Transmission)
chapters; the GLSL implementation and synthetic tests are independently written.

- The shared skin loader extends the inherited GPL-2.0-or-later Q3 loader,
  retaining its notice. RTCW model-key and player-scale behavior is independently
  implemented from observations of the original [SP](https://github.com/id-Software/RTCW-SP),
  [MP](https://github.com/id-Software/RTCW-MP) and [iortcw](https://github.com/iortcw/iortcw)
  sources. The skin comparison probe compiles their unchanged functions and
  records only in disposable local fixtures with their notices; no GPLv3
  implementation text is incorporated in the engine.

- RTCW brush-surface shader lookup is independently implemented from the same
  original SP/MP and iortcw renderer behavior. Its source probe compares surface
  selection, mipmap choice and diffuse-material requests in disposable fixtures.
  The separately cached diffuse variants extend fnTech3's inherited GPLv2-or-later
  shader compiler; the original engines' post-compilation shader mutation is not
  incorporated.

- RTCW SP's intentional no-op server-cache policy is independently implemented
  from the original [SP](https://github.com/id-Software/RTCW-SP) and
  [iortcw](https://github.com/iortcw/iortcw) references. The shared cache reader and
  writer retain fnTech3's inherited GPLv2-or-later implementation and disk layout,
  with transactional validation and a distinct engine-owned MP cache filename.
  The UI sound-registration contract is likewise independently implemented
  from the original SP/MP one-argument imports, preserving their fixed
  uncompressed hint without copying reference implementation text.

- MP localization is independently authored from dictionary and module-contract
  observations of [original RTCW MP](https://github.com/id-Software/RTCW-MP) and
  [iortcw MP](https://github.com/iortcw/iortcw/tree/master/MP). The comparison
  probe compiles their unchanged GPLv3 parser/translation functions only in
  disposable fixtures with original notices; installed dictionaries remain in
  memory. No GPLv3 implementation text is incorporated into the engine. The
  MP background-track import uses the existing sound service, and the legacy
  WolfMP updater is explicitly unavailable in the shared fnTech3 executable.

- RTCW dynamic-light submission, projected passes and model-lighting policy are
  independently implemented from observations of the original
  [SP](https://github.com/id-Software/RTCW-SP),
  [MP](https://github.com/id-Software/RTCW-MP) and
  [iortcw](https://github.com/iortcw/iortcw) renderers. The comparison probe
  compiles unchanged reference functions, including original Win32 color
  conversion, only in disposable fixtures with their GPLv3 notices. The engine
  retains its GPLv2-or-later implementation; no GPLv3 source is incorporated.

- RTCW SP music queue, filename, fade and saved-game music behavior is
  independently implemented from observations of the original
  [RTCW SP sound system](https://github.com/id-Software/RTCW-SP/blob/master/src/client/snd_dma.c).
  The shared controller serves fnTech3's existing OpenAL and legacy backends.
  The [iortcw SP audio layer](https://github.com/iortcw/iortcw/tree/master/SP/code/client)
  was inspected as supporting evidence; its fade/streaming stubs were not
  used as implementations. The engine retains its GPLv2-or-later license.
  The music comparison probe compiles unchanged original GPLv3 functions only
  in disposable fixtures with their notices; it does not incorporate them into
  the engine.

- RTCW shader-file recovery is independently implemented from observations of
  the original [SP shader indexer](https://github.com/id-Software/RTCW-SP/blob/master/src/renderer/tr_shader.c)
  and legitimate installed files. The inherited GPLv2-or-later tokenizer is
  retained. Installed shader text, menu geometry and material pass counts are
  read-only evidence, not incorporated assets or implementation text.

- RTCW's sixteen-frame shader animation limit follows the original
  [SP renderer contract](https://github.com/id-Software/RTCW-SP/blob/master/src/renderer/tr_local.h)
  and [MP renderer contract](https://github.com/id-Software/RTCW-MP/blob/master/src/renderer/tr_local.h),
  corroborated by installed fire materials. This is reference-only use of a
  format limit; no GPLv3 implementation or retail shader text is incorporated.

- RTCW entity-stream allocation, stop and fade behavior is independently
  implemented from the original [SP sound system](https://github.com/id-Software/RTCW-SP/blob/master/src/client/snd_dma.c)
  and [MP sound system](https://github.com/id-Software/RTCW-MP/blob/master/src/client/snd_dma.c).
  The controller and bounded decoder serve both existing audio backends.
  The comparison probe compiles unchanged GPLv3 functions only in disposable
  fixtures with their notices; production retains its GPLv2-or-later license.
  The released SP entity-loop/music-channel bug is corrected explicitly; see
  the [evidence and limits](docs/fntech3/2026-09-16-rtcw-entity-streams.md).

- RTCW game/cgame file-write return values and save-file promotion are
  independently implemented from observations of the original
  [SP filesystem traps](https://github.com/id-Software/RTCW-SP/blob/master/src/server/sv_game.c)
  and [MP filesystem traps](https://github.com/id-Software/RTCW-MP/blob/master/src/server/sv_game.c).
  The inherited GPLv2-or-later filesystem remains the implementation;
  [runtime findings and limits](docs/fntech3/2026-09-16-rtcw-autosave-write.md)
  distinguish the observed autosave failure from subsequent verification.

- RTCW scene-fog selection, shader directives, GL application and decal
  projection are independently implemented from observations of the original
  [SP renderer](https://github.com/id-Software/RTCW-SP/tree/master/src/renderer)
  and [MP renderer](https://github.com/id-Software/RTCW-MP/tree/master/src/renderer).
  Decals reuse this repository's inherited GPLv2-or-later Q3 polygon clipper;
  no RTCW implementation text is incorporated. Separate engine/module records
  preserve the XYZST output and signed fragment-count contract. See the
  [fog](docs/fntech3/2026-09-16-rtcw-gl-fog.md) and
  [decal evidence](docs/fntech3/2026-09-16-rtcw-marks.md).
- RTCW speech measurements and the silent default sound are independently
  implemented from the original
  [SP mixer](https://github.com/id-Software/RTCW-SP/blob/master/src/client/snd_mix.c),
  [SP sound system](https://github.com/id-Software/RTCW-SP/blob/master/src/client/snd_dma.c),
  [MP sound system](https://github.com/id-Software/RTCW-MP/blob/master/src/client/snd_dma.c)
  and [iortcw](https://github.com/iortcw/iortcw) observations. Both existing
  backends measure real queued speech; original MP's disabled talking-animation
  query remains zero. Reference functions and their GPLv3-or-later notices
  appear only in disposable comparison probes. See the
  [speech checkpoint](docs/fntech3/2026-09-17-rtcw-speech.md) for clock differences,
  runtime observations and remaining playback-parity limits.
- RTCW corona accumulation, SP flags/spotlights, MP visibility and persistent
  fades are independently implemented from the original
  [SP flare renderer](https://github.com/id-Software/RTCW-SP/blob/master/src/renderer/tr_flares.c)
  and [MP flare renderer](https://github.com/id-Software/RTCW-MP/blob/master/src/renderer/tr_flares.c)
  observations. The existing fnTech3 backend quad paths provide GPU submission;
  [evidence and limits](docs/fntech3/2026-09-16-rtcw-coronas.md) distinguish
  checked implementation from the still-open mission and image-parity gates.

- RTCW camera parsing, sampled paths and profile-specific timing are
  independently implemented from the original
  [SP camera system](https://github.com/id-Software/RTCW-SP/tree/master/src/splines),
  [MP camera system](https://github.com/id-Software/RTCW-MP/tree/master/src/splines)
  and [iortcw](https://github.com/iortcw/iortcw) observations. The source probe
  compiles their unchanged GPLv3-or-later implementations only in disposable
  executables; production remains independently written GPLv2-or-later code.
  [Camera evidence and limits](docs/fntech3/2026-09-16-rtcw-cameras.md) distinguish
  original MP timing, iortcw's SP timing, undefined paths and live gates.

- RTCW browser-query dispatch and list-source translation are independently
  implemented from the original
  [SP contract](https://github.com/id-Software/RTCW-SP/blob/master/src/game/q_shared.h)
  and [MP contract](https://github.com/id-Software/RTCW-MP/blob/master/src/game/q_shared.h)
  observations. Their source IDs differ; the retired MPlayer source is not
  substituted with another service. See the
  [browser evidence and limits](docs/fntech3/2026-09-17-rtcw-ui-browser.md).

- RTCW character-mip, conditional texture and normal-based alpha behavior is
  independently implemented from observations of the original
  [SP renderer](https://github.com/id-Software/RTCW-SP/tree/master/src/renderer),
  [MP renderer](https://github.com/id-Software/RTCW-MP/tree/master/src/renderer)
  and [iortcw](https://github.com/iortcw/iortcw). Reference switch bodies and
  their GPLv3-or-later notices appear only in disposable comparison fixtures;
  no reference implementation or retail shader text is incorporated. The
  [character-material checkpoint](docs/fntech3/2026-09-17-rtcw-character-materials.md)
  records alpha-byte comparisons, inspected startup captures and remaining
  GPU/gameplay limits.

- RTCW save selection/restart, cinematic-save protection, episode preservation,
  trajectory ordinals, MP match-state restarts, and world sun/light-grid metadata are independently
  implemented from behavior facts observed in the original
  [SP server and save system](https://github.com/id-Software/RTCW-SP),
  [MP contracts and renderer](https://github.com/id-Software/RTCW-MP),
  [iortcw](https://github.com/iortcw/iortcw), and the user's legitimate retail
  installation. No GPLv3 implementation text or proprietary save serializer is
  incorporated. The [campaign/save checkpoint](docs/fntech3/2026-09-17-rtcw-campaign-savegames.md)
  distinguishes source comparisons, engine fixes, retail defects and bounded
  runtime coverage.

- RTCW activation/lean button encodings, pickup userinfo ownership, and mesh
  blink selection are independently implemented from behavioral observations
  of the original [SP client/game/renderer](https://github.com/id-Software/RTCW-SP/tree/master/src)
  and [MP client/renderer](https://github.com/id-Software/RTCW-MP/tree/master/src).
  No GPLv3 implementation text is incorporated. The
  [interaction and eye checkpoint](docs/fntech3/2026-09-17-rtcw-player-interactions.md)
  records failing-before regressions, installed observations, and limits.
  The [eye-rendering follow-up](docs/fntech3/2026-09-17-rtcw-eye-rendering.md)
  independently corrects Vulkan state recovery and reuses this project's
  observed RTCW packed-normal contract for MD3 heads as well as MDC frames.
  The independently authored [transparent-fog correction](docs/fntech3/2026-09-17-rtcw-transparent-fog.md)
  follows framebuffer blend equations and flame-material observations from the
  user's legitimate installation; no proprietary material text is incorporated.

- The [RTCW engine command audit](docs/fntech3/2026-09-17-rtcw-engine-commands.md)
  uses command names, input encodings, menu/chat ownership and music queue facts
  from the original [SP engine](https://github.com/id-Software/RTCW-SP/tree/master/src)
  and [MP engine](https://github.com/id-Software/RTCW-MP/tree/master/src).
  Input bindings, command dispatch, regression fixtures and the inventory checker
  are independently authored; no GPLv3 implementation text is incorporated.

- The Enemy Territory profile uses interoperability facts from the original
  [id Software ET source release](https://github.com/id-Software/Enemy-Territory/tree/40342a9e3690cb5b627a433d4d5cbf30e3c57698)
  and the user's legitimate installed game. The GPLv3 reference was inspected
  for structure offsets, API ordinals, format encodings and observable behavior;
  no implementation text is incorporated. Adapters, codecs, tag validation and
  synthetic fixtures are independently authored. See the
  [ET evidence record](docs/fntech3/2026-09-17-wolfet-contracts.md) and
  [snapshot callback/visibility follow-up](docs/fntech3/2026-09-17-wolfet-snapshots.md)
  and [viewpoint/capacity record](docs/fntech3/2026-09-17-wolfet-viewpoints.md).

- The Star Trek Voyager: Elite Force profiles use interoperability facts from
  the user's legitimate installation, observations of its retail `stvoy.exe`,
  and three references kept distinct from implementation:
  [Lilium Voyager](https://github.com/zturtleman/lilium-voyager/tree/0f7dcd89d142c8b635ce2756a80cb62411731848)
  (GPL-2.0-or-later) for the Holomatch engine contract; Raven's
  [Elite Force SP mod SDK](https://github.com/UberGames/SP-Mod-Source-Code/tree/04e0e618d1ee57a2919f1a852a688c03b1aa155d),
  whose non-free "STEF Game Source License" limits its use to names, ordinals
  and record layouts; and the GPLv2 engine side of
  [VoyagerSP-Android](https://github.com/imjustadudegamer/VoyagerSP-Android/tree/0d8942e86a8469859da086f590875bcb66b4f4df)
  as a second single-player host. No implementation text from any of them is
  incorporated. See the [Elite Force foundation record](docs/fntech3/2026-09-18-stvef-contracts.md).
  Protocol 26 also uses independently generated synthetic fixtures from
  Lilium's message encoder, with [bidirectional runtime evidence](docs/fntech3/2026-09-20-stvef-ioef.md).

- Elite Force's MP3 sounds and music are decoded by
  [minimp3](https://github.com/lieff/minimp3/tree/ea99364f61c14656440e8d77e9c233ccf3124633)
  by Lion (lieff), released under CC0 1.0 and consumed through
  `subprojects/minimp3.wrap`. The frame walker that reproduces retail
  unpacking sizes is independently authored.

- Elite Force single-player movies use the user's installed Bink 1 runtime
  through an independently declared interface observed in that installation.
  No SDK header, decoder implementation or runtime binary is incorporated.
  The [movie record](docs/fntech3/2026-09-20-stvef-bink.md) separates the native
  runtime's role from the engine's file validation, playback and rendering.

## Retail-content boundary

The RTCW audio-zone authoring mode uses material-flag facts observed in the
original [SP surface flags](https://github.com/id-Software/RTCW-SP/blob/master/src/game/surfaceflags.h)
and [MP surface flags](https://github.com/id-Software/RTCW-MP/blob/master/src/game/surfaceflags.h),
cross-checked against [iortcw](https://github.com/iortcw/iortcw).
The acoustic classification is independently authored; no GPLv3 implementation
text is incorporated. The 47 stock sidecars are generated acoustic metadata
from a legitimate local installation, with source/output identities and limits
in the [generation record](docs/fntech3/2026-09-17-rtcw-audio-zones.md).

fnTech3 does not redistribute Quake III Arena, Quake Live, Return to Castle
Wolfenstein, Enemy Territory or Elite Force data, retail game modules, Bink or
other proprietary runtimes, Steam SDK/runtime binaries, Awesomium,
credentials, captured private traffic, or other proprietary content. Runtime
validation uses the user's
legitimate local installations and records only non-proprietary evidence.

## Project thanks

Thanks to the Quake, Quake III, Quake Live, Return to Castle Wolfenstein and
Star Trek Voyager: Elite Force communities: players, server administrators, mod authors, mappers, tool authors,
testers, reverse engineers, and maintainers who keep these games understandable
and playable.

## Trademark note

fnTech3 is independent and is not affiliated with, endorsed by, or sponsored by
id Software, Bethesda, ZeniMax Media, Valve, Raven Software, Activision or
Paramount. Quake, Quake III Arena, Quake Live, Return to Castle Wolfenstein,
Star Trek and Elite Force are trademarks of their respective owners.
