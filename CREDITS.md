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
- RTCW server defaults, campaign allocation and world-import arguments use
  behavior facts from the original [SP server](https://github.com/id-Software/RTCW-SP/tree/master/src/server)
  and [MP server](https://github.com/id-Software/RTCW-MP/tree/master/src/server).
  The profile policies and checked adapters are independently written; world
  queries retain fnTech3's existing GPL-2.0-or-later implementation. No GPLv3
  reference implementation text or game logic is incorporated. The
  [server checkpoint](docs/fntech3/2026-09-15-rtcw-server-world.md) records scope.

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

## Retail-content boundary

fnTech3 does not redistribute Quake III Arena or Quake Live data, retail game
modules, Steam SDK/runtime binaries, Awesomium, credentials, captured private
traffic, or other proprietary content. Runtime validation uses the user's
legitimate local installations and records only non-proprietary evidence.

## Project thanks

Thanks to the Quake, Quake III, and Quake Live communities: players, server
administrators, mod authors, mappers, tool authors, testers, reverse engineers,
and maintainers who keep these games understandable and playable.

## Trademark note

fnTech3 is independent and is not affiliated with, endorsed by, or sponsored by
id Software, Bethesda, ZeniMax Media, or Valve. Quake, Quake III Arena, and
Quake Live are trademarks of their respective owners.
