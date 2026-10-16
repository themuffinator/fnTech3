# Credits

- [zlib](https://zlib.net/), by Jean-loup Gailly and Mark Adler, supplies the
  maintained DEFLATE decoder used to load pak assets and PNG images. The
  inherited minizip archive frontend retains Gilles Vollant's attribution
  in `unzip.c`.
- [SDL3](https://github.com/libsdl-org/SDL/tree/release-3.4.0), by Sam
  Lantinga and the SDL contributors, under the zlib License and consumed
  through `subprojects/sdl3.wrap` or a system package, supplies the window,
  input and audio backend of client builds that use it, and the launcher's
  window, input, folder dialogs and drawing. The launcher's interface is
  built with the same pinned [Dear ImGui](https://github.com/ocornut/imgui)
  core as the entity editor; its ImGui glue, game discovery and look are
  independently authored, and ImGui's own SDL3 backends were consulted as
  reference only.

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

- The IHV audit uses the user-supplied February 1999 reference in
  [Quake3-IHV](https://github.com/themuffinator/Quake3-IHV/tree/d5ce877155904c57f8b28fe0fd0c8aa2e9473337).
  New BSP validation, model-membership, checksum, legacy material, fog, sky,
  native-boundary, identity, link/trace and protocol-codec components are
  independently authored from asset/binary observations. The
  optional fog and sky probes call a hash-pinned user-supplied executable to record
  numeric facts. The optional native game probe loads the hash-pinned supplied
  game DLL to observe initialization, shutdown and failure cleanup. Isolated
  follow-up hosts observe original client-allocation, password/ClientConnect
  callbacks and the server's client-limit clamp; they do not run a game session.
  The optional wire probe compares original state codecs using synthetic
  inputs; retained fixtures contain numeric observations only.
  The optional collision probe compares original link bounds and solid packing
  with the BSP leaf query replaced by an empty result; it does not establish
  original collision-trace parity. The subsequent trace probe compares original
  server sweep/point decisions with synthetic contacts and CM replies; it does
  not establish BSP collision or gameplay parity. A guarded cgame probe observes
  its original snapshot-ring copy with the following packet-graph helper stubbed;
  it does not initialize cgame or rendering. A separate original configstring
  parser probe compares complete native string records using synthetic message
  reads and a notification callback; no native client session is started.
  Client data follow-ups compare original history getters and complete snapshot
  records, then pass synthetic packets and the supplied recording through the
  new session component to original cgame data callbacks. These probes use
  synthetic timing, bypass cgame initialization, stub packet-graph history and record
  server commands without executing them. Native game integration remains open.
  A separate Init-prefix probe observes original cvar requests and map import
  order, stopping at registration boundaries and checking injected failures;
  it does not complete cgame initialization or load assets.
  The subsequent model-metadata implementation compares tag interpolation
  directly with the original executable and supplies separate native model,
  frame and tag records. A later cgame host completes original Init and frame/draw
  callbacks with read-only model metadata, synthetic collision/resources and
  captured drawing calls. It starts no engine/display/audio/input/network
  session. Earlier references to a stubbed "prediction helper" describe packet
  graph history instead; the newer host leaves that helper unmodified.
  The independently authored scene boundary copies native submissions into
  owned arrays and validates resource kinds. Original numeric area-visibility
  and entity-color routines establish the mask polarity and RGBA fields using
  generated inputs; these probes start no graphics session and do not establish
  real BSP/PVS or rendered native playback.
  The independent IHV skin parser, registration and renderer selection follow
  original numeric cache/parsing observations and the original surface-ordinal
  shader lookup. Supplied skin files are read-only comparison inputs; their
  contents and the original implementation are not incorporated.
  The particle path is independently authored from original geometry, generated
  image, shader-record and GL-state observations. Its optional reference probe
  supplies generated points and replaces image/GL services without creating a
  graphics context. No original particle implementation or image bytes are
  incorporated; the procedural texture is generated from the observed formula.
  Entity visibility, origin interpolation and beam geometry are independently
  authored from generated original-executable observations. The optional entity
  probe replaces renderer/GL services, checks special model registration and
  captures numeric transform, lighting and beam behavior without starting a
  graphics session. Lighting observations remain distinct from implemented
  behavior; no original code or model/texture bytes are incorporated.
  Native MD3 sine samples, interpolation, normal arithmetic and culling are
  independently authored from numeric observations of that same executable.
  The optional animation host reads user-supplied models and compares every
  frame without starting the engine; no model or original implementation bytes
  are incorporated. Generated vectors and digests are retained as tests.
  Model-light sampling, BSP traversal, dynamic-light arithmetic, fixed light
  direction, diffuse colors and the animation toggle are independently authored
  from selected original-executable observations. The optional lighting host
  compares the actual translator/renderer light loader against all four supplied
  maps, with image/shader/allocation services replaced by CPU stubs. It preserves
  raw inputs read-only and incorporates no original code or asset bytes.
  The native dedicated-game adapter is independently authored from observed
  API 4 lifecycle, cvar flags, import behavior and original module execution.
  Its optional engine probe loads user-supplied maps and the original DLL,
  retains game allocations across levels, and exercises error cleanup. These
  observations do not establish native clients, bots or network compatibility;
  no original implementation, DLL, navigation or map bytes are incorporated.
  The metadata reader follows Microsoft's
  [PE format specification](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format).
  No reference implementation, module or
  asset is incorporated. The inherited Spearmint translator attribution remains
  below; the [IHV ledger](docs/fntech3/Q3_IHV.md) records the limited runtime scope.

- The Quake III point-release audit uses official binaries and recording
  fixtures preserved by Jason Brownlee's
  [Quake3OfficialArchive](https://github.com/Jason2Brownlee/Quake3OfficialArchive/tree/960182ae296be8cbcd6b59db78b31aaa9d28ed9f).
  [Lilium Arena Classic](https://github.com/clover-moe/lilium-arena-classic/tree/2fc883cbd244fa58046542a63518e51a7e01479f)
  (GPL-2.0-or-later) was inspected to distinguish the older QVM and protocol-43
  contracts. The new negotiation, validation and pak policies are independently
  authored from observations; no implementation text was incorporated from
  that reference. See the [release ledger](docs/fntech3/Q3_POINT_RELEASES.md).

- RTCW routing-cache recovery is independently implemented from the disk and
  generation contracts observed in the original
  [SP routing code](https://github.com/id-Software/RTCW-SP/blob/master/src/botlib/be_aas_route.c),
  [MP routing code](https://github.com/id-Software/RTCW-MP/blob/master/src/botlib/be_aas_route.c),
  [iortcw](https://github.com/iortcw/iortcw), and retail `WolfSP.exe` cache
  generation. These GPLv3 references supply behavior observations only;
  their functions and notices appear solely in disposable comparison fixtures.
  No reference implementation text or retail assets are incorporated.
  The [recovery record](docs/fntech3/2026-09-20-rtcw-cache-recovery.md) separates
  cache interoperability from remaining collision and route-cost differences.

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

- Heavy Metal: F.A.K.K.2's HUD preset is independently authored from the
  sizes, alignments and art names of its four retail HUD menus, read from a
  legitimate installation's `ui/*.urc`, and from the engine's own HUD draw
  dump. This is reference-only use: no retail menu text or assets are
  incorporated.

- The Enemy Territory, Elite Force and Alice HUD presets are independently
  authored from the engine's own HUD draw dumps and from layout facts read in
  the [Enemy Territory GPL source](https://github.com/id-Software/Enemy-Territory),
  Raven's Elite Force single-player SDK and the
  [Elite-Reinforce](https://github.com/kugelrund/Elite-Reinforce) port.
  Giving Elite Force single player's client game the 4:3 frame follows
  [Lilium Voyager](https://github.com/zturtleman/lilium-voyager)'s handling
  of its Holomatch interface. All of this is reference-only use: no source
  text is incorporated.

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

- RTCW SP's [sound entry point](https://github.com/id-Software/RTCW-SP/blob/master/src/client/snd_dma.c)
  supplies the null-effect request rule, and Enemy Territory's
  [input entry point](https://github.com/id-Software/Enemy-Territory/blob/master/src/client/cl_input.c)
  supplies its command-button facts. ET's
  [client handshake](https://github.com/id-Software/Enemy-Territory/blob/master/src/client/cl_main.c),
  [server handshake](https://github.com/id-Software/Enemy-Territory/blob/master/src/server/sv_client.c)
  and [client parser](https://github.com/id-Software/Enemy-Territory/blob/master/src/client/cl_parse.c)
  supply its reply positions and visibility rule for the independently authored
  [reported-fault corrections](docs/fntech3/2026-10-01-observed-compatibility-fixes.md).
  These GPLv3 sources were used as reference observations only; no implementation
  text was incorporated.

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

The path tracer (`code/rendererrtx/rt`, `code/rendererrtx/shaders/rt_*`) is
independently written. Its methods follow published work, used as method
references only:

- light selection by resampled importance sampling: Talbot, Cline and Egbert,
  [*Importance Resampling for Global Illumination*](https://diglib.eg.org/handle/10.2312/EGWR.EGSR05.139-146)
  (EGSR 2005);
- reflections sharing surface lights with light sampling by the balance
  heuristic: Veach and Guibas,
  [*Optimally Combining Sampling Techniques for Monte Carlo Rendering*](https://graphics.stanford.edu/papers/combine/)
  (SIGGRAPH 1995);
- the denoiser's variance-guided temporal and a-trous filtering: Schied et al.,
  [*Spatiotemporal Variance-Guided Filtering*](https://research.nvidia.com/publication/2017-07_spatiotemporal-variance-guided-filtering-real-time-reconstruction-path-traced)
  (HPG 2017), and Dammertz et al.,
  [*Edge-Avoiding A-Trous Wavelet Transform for fast Global Illumination Filtering*](https://jo.dreggn.org/home/2010_atrous.pdf)
  (HPG 2010);
- the specular lobe: Walter et al.,
  [*Microfacet Models for Refraction through Rough Surfaces*](https://www.graphics.cornell.edu/~bjw/microfacetbsdf.pdf)
  (EGSR 2007), and Duff et al.,
  [*Building an Orthonormal Basis, Revisited*](https://jcgt.org/published/0006/01/01/)
  (JCGT 2017);
- texture filtering from ray differentials: Igehy,
  [*Tracing Ray Differentials*](https://graphics.stanford.edu/papers/trd/)
  (SIGGRAPH 1999);
- the compute backend's hierarchies and intersections: Wald,
  *On fast Construction of SAH-based Bounding Volume Hierarchies* (IEEE
  Symposium on Interactive Ray Tracing 2007), and Möller and Trumbore,
  *Fast, Minimum Storage Ray-Triangle Intersection* (Journal of Graphics
  Tools 1997);
- the normal maps' tangent frame: Lengyel,
  [*Computing Tangent Space Basis Vectors for an Arbitrary Mesh*](http://www.terathon.com/code/tangent.html)
  (2001).

Its map lighting reproduces the light models of the map compilers as
behaviour: id Software's q3map in the
[Quake III Arena source release](https://github.com/id-Software/Quake-III-Arena)
(GPL-2.0) and q3map2 in
[NetRadiant](https://gitlab.com/xonotic/netradiant) (GPL-2.0), consulted for
their point, spot, linear, area, sun and ambient rules, their choice and
normalisation of the sun, their backsplash, `alphashadow` and `lightfilter`
rules, which surfaces cast shadows, q3map2's `q3map_sunExt` spread and
`q3map_skyLight` dome of suns, the worldspawn keys only q3map2 reads, and
q3map2's Wolfenstein light model for RTCW and Enemy Territory maps. No
compiler source text is incorporated. The
material maps a texture pack may supply take the companion image names,
channel layouts and normal-map axes of the rend2 renderer in
[OpenJK](https://github.com/JACoders/OpenJK/tree/1a6a643427aa347553e9073dac5570b33337c4d9)
(GPL-2.0-or-later, `codemp/rd-rend2`), as reference observation only; no
implementation text is incorporated. The renderer's waveform and colour-generator behaviour
is matched against fnTech3's own inherited renderer code.

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
  The [area-routing correction](docs/fntech3/2026-10-01-wolfet-area-routing.md)
  follows numeric observations of the installed executable's cache-update and
  contents functions, checked against that same source as a reference. Its
  penalty helper, generated worlds and bounded x86 comparison harness are
  independently authored; no original implementation text is incorporated.

  The [ET AAS8 loading follow-up](docs/fntech3/2026-10-01-wolfet-aas8-loading.md)
  checks the original format header's numeric layout and reader's checksum
  marker, then selects the engine's existing independent field decoder. These
  are reference observations; no GPLv3 implementation text is incorporated.

  The [ET travel-cost correction](docs/fntech3/2026-10-01-wolfet-travel-costs.md)
  measures the installed client and dedicated binary's scalar math and type
  lookup, and observes their endpoint-entry exception. Its cost helper, generated fixture and
  bounded emulator are independently authored; the source release is used
  only to corroborate contract facts, with no GPLv3 text incorporated.

  The [2026-10-02 compatibility round](docs/fntech3/2026-10-02-compatibility-round.md)
  observes ET's exclusion of world-model entities, routing-cache settings,
  update limits, eviction rules and download/binary-message framing in the
  original source release. The
  corrections and generated regressions are independently authored;
  the GPLv3 source supplies contract observations only.

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
  The independently written RDM5 reader and renderer corrections are checked
  against the installed retail mathematical routines and model corpus; see
  the [model and lighting record](docs/fntech3/2026-09-20-stvef-mdr.md).
  Synthetic effect geometry is compared with the installed Holomatch routines
  in the [effect record](docs/fntech3/2026-09-20-stvef-effects.md).
  The [effect capture method](docs/fntech3/2026-09-21-stvef-effect-captures.md)
  uses independently observed retail screenshot timing and rendered frames;
  no retail implementation or shader text is incorporated.
  The [SP cadence record](docs/fntech3/2026-09-21-stvef-sp-cadence.md)
  pins simulation and local snapshot scheduling to independently observed
  retail function outputs; no proprietary implementation is incorporated.
  The [startup/capture follow-up](docs/fntech3/2026-09-21-stvef-sp-startup.md)
  adds independently observed settling-frame clocks and SP screenshot timing.
  The [map-reload record](docs/fntech3/2026-09-21-stvef-map-reloads.md) pins
  SP command registration and full-map continuations to native observations,
  and documents the independently corrected shared client-storage crash.
  The [player-snapshot record](docs/fntech3/2026-09-21-stvef-player-snapshots.md)
  pins SP's visible local-player inclusion to the native executable. The
  [Elite-Reinforce reference](https://github.com/kugelrund/Elite-Reinforce/tree/526e911c532817b3021aaa564b6b0cf087879827)
  informed the investigation of shared animation and weapon behavior;
  its non-GPL implementation text is not incorporated.
  The [command-ordinal record](docs/fntech3/2026-09-21-stvef-command-ordinals.md)
  pins SP command recovery, mutable game commands and snapshot acknowledgements
  to 72 owned native-probe cases and runtime observations. The source reference
  informed the timestamp-clamp investigation; no implementation is incorporated.
  The [texture-compression record](docs/fntech3/2026-09-21-stvef-texture-compression.md)
  pins the menu/configuration control to native registration and runtime facts,
  and separates texture-format differences from renderer comparisons.
  The [alpha-format record](docs/fntech3/2026-09-21-stvef-alpha-compression.md)
  checks independently authored format selection against both native engines.
  The [method-selection record](docs/fntech3/2026-09-21-stvef-compression-method.md)
  adds native compression preferences, floating-point enable behavior and
  live video-restart validation of the independent implementation.
  The [lightmap-control record](docs/fntech3/2026-09-21-stvef-lightmap-controls.md)
  pins the native compression/precision choices and validates merged and
  separate uploads across the three renderer backends.
  Native projection and stencil extrusion observations are documented in the
  [shadow record](docs/fntech3/2026-09-20-stvef-shadows.md).
  The [movie audio record](docs/fntech3/2026-09-20-stvef-movie-audio.md)
  documents independently observed sound-state and PCM delivery contracts.

- The Star Wars Jedi Knight: Jedi Academy profiles use interoperability facts
  from the user's legitimate retail 1.01 installation -- its pak identity,
  module locations and export tables, and every map and model it ships -- and
  from [OpenJK](https://github.com/JACoders/OpenJK/tree/1a6a643427aa347553e9073dac5570b33337c4d9)
  at `1a6a643427aa347553e9073dac5570b33337c4d9` (GPL-2.0-or-later), which
  carries Raven Software's released single-player and multiplayer sources. It
  is reference material only: protocol numbers, module names and entry points,
  configstring layouts, limits, the RBSP identity and the ICARUS script
  numbers are cited by file and line, and no implementation text is
  incorporated. The ICARUS numbers were derived from the installation's own
  scripts first and read from those sources afterwards; the two agree. No Jedi Academy module or asset
  is redistributed or shipped, and none is needed to build fnTech3: the
  installed evidence comes from the user's own copy. See the
  [Jedi Academy foundation record](docs/fntech3/2026-09-21-jka-contracts.md)
  and [protocol and SP bootstrap record](docs/fntech3/2026-10-02-jka-compatibility.md).
  The latter records independent protocol encoders and the measured retail
  API-8 table, ROFF timing and Ghoul2 animation seed behavior. Its
  single-player menu sources (`code/ui`, which also build Jedi Outcast's) were
  read for how the two engines run their menus -- the order of a menu's
  scripts, focus, mouse and key handling, and each item type's drawing --
  and the retail `jasp.exe` and `jk2sp.exe` settle every keyword, named value
  and text style where the two differ; fnTech3's menu reader and runtime are
  written independently ([menus record](docs/fntech3/2026-10-16-jedi-sp-menus.md)).

- The American McGee's Alice profile uses facts from the user's legitimate
  retail installation -- its pak identity, its module export tables, and every
  map it ships -- and from the retail `alice.exe` itself, whose cvar
  registrations and bounds checks supply every constant the profile claims.
  Two references are kept distinct from implementation. Ritual
  Entertainment's Heavy Metal: F.A.K.K.2 v1.02 SDK supplies the `FAKK` lump
  order and record layout, the `TAN ` model records, and the game module's
  import and export table member names and ordering, under its own non-free
  SDK licence, so it is used for names and layouts only. Every one of those
  was re-measured against Alice's own binaries and assets before being
  written here -- the maps against all thirty-six, the models against all
  694, and each module slot against the engine's own table build -- and no
  implementation text is incorporated.
  [OpenAlice](https://github.com/themuffinator/OpenAlice), an independent
  reconstruction of this engine, supplies a Ghidra corpus for the retail
  binaries; it is GPLv3 while fnTech3 is GPLv2, so nothing from it may be
  incorporated in any case, and the retail binary outranks it wherever they
  disagree. No Alice module or asset is redistributed or shipped, and none is
  needed to build fnTech3. See the
  [Alice foundation record](docs/fntech3/2026-09-22-alice-contracts.md),
  [map record](docs/fntech3/2026-09-22-alice-maps.md),
  [module record](docs/fntech3/2026-09-22-alice-module-abi.md),
  [model record](docs/fntech3/2026-09-22-alice-models.md) and
  [wire record](docs/fntech3/2026-09-22-alice-wire.md).

  The [October 2 UberTools round](docs/fntech3/2026-10-02-ubertools-round.md)
  directly pins configstring bounds to Alice's executable and the official
  F.A.K.K.2 1.02 patch. OpenAlice's corpus only helped locate Alice's accessors;
  the implementation corrections and malformed-lighting checks are independently
  authored, with no external implementation text incorporated.

- The Medal of Honor: Allied Assault profile uses facts from the user's
  legitimate retail 1.00 installation -- its pak identity, every map it ships,
  and the two retail game libraries, plain PEs whose tables, records and call
  sites are measured directly. Its `MOHAA.exe` is SafeDisc-encrypted and was
  never read or run.
  [OpenMoHAA](https://github.com/openmoh/openmohaa/tree/ab43a0def6c4feccedae36fdb58c2e007d7e4c35)
  at `ab43a0def6c4feccedae36fdb58c2e007d7e4c35` (GPL-2.0-or-later), an
  independent reconstruction of 2015's engine, is used in two ways that are
  kept apart. Its implementation text is incorporated, each file keeping its
  original licence header, in the TIKI, skeletor, script-container and alias
  code under `code/mohaa/` (see its [README](code/mohaa/README.md)), the
  protocol-6 message tables in `code/qcommon/mohaa_msg.cpp`, the protocol-6
  client parse in `code/client/cl_mohaa.cpp`, the snapshot walk and module
  queues in `code/server/server_mohaa_net.hpp`, the world and hit-location
  traces in `code/server/server_mohaa_world.hpp`, and the terrain collision in
  `code/qcommon/cm_mohaa_terrain.c` with the reading of the terrain's variance
  tree it shares with the renderers in `code/qcommon/mohaa_terrain_squares.h`,
  whose source is copyright 2008 Leszek Godlewski. Everywhere else -- the
  engine menus, HUD, music, savegames, loading and sound rules, the permanent
  decals with their editor, the decal cutter for terrain and brush models, the
  lights its client game's shadows ask for, and the level-of-detail tool -- it
  is reference observation, cited by file, and
  none of its text is used. The retail libraries outrank it wherever they
  disagree. No Medal of Honor module or asset is redistributed or shipped;
  `system86.dll`, the modules' allocator, is loaded from the user's own
  installation. See the
  [contracts record](docs/fntech3/2026-09-24-mohaa-contracts.md) and
  [map record](docs/fntech3/2026-09-24-mohaa-maps.md).

  The [October 2 framing correction](docs/fntech3/2026-10-02-ubertools-round.md)
  pins continuation-bit parsing to the retail 1.00 client module and separates
  that observation from OpenMoHAA's queue-capacity reconstruction. The change
  is independently written in the already attributed adapter.

- Elite Force's MP3 sounds and music are decoded by
  [minimp3](https://github.com/lieff/minimp3/tree/ea99364f61c14656440e8d77e9c233ccf3124633)
  by Lion (lieff), released under CC0 1.0 and consumed through
  `subprojects/minimp3.wrap`. The frame walker that reproduces retail
  unpacking sizes is independently authored.

- The entity editor's interface is
  [Dear ImGui](https://github.com/ocornut/imgui/tree/f1cc2ae15e53a861a874c3034aae6798fde194ab)
  by Omar Cornut and its contributors, released under the MIT License and
  consumed through `subprojects/imgui.wrap`; only its core and its
  `std::string` helpers are built. Its embedded ProggyClean and ProggyForever
  fonts are Tristan Grimmer's and Disco Hello's, under the MIT License. The
  bridge that draws ImGui's triangles through the engine's renderers, and the
  editor's panels, are independently authored.

- Elite Force single-player movies use the user's installed Bink 1 runtime
  through an independently declared interface observed in that installation.
  No SDK header, decoder implementation or runtime binary is incorporated.
  The [movie record](docs/fntech3/2026-09-20-stvef-bink.md) separates the native
  runtime's role from the engine's file validation, playback and rendering.
  The [presentation record](docs/fntech3/2026-09-20-stvef-cinematic-presentation.md)
  documents independently observed retail fade and crop behaviour.

- Elite Force single-player force feedback uses interoperability observations
  from the user's installed executable and FFC10 runtime. The effect table,
  scheduler, C++ ABI declarations and lifecycle controller are independently
  authored; no CFeel SDK or proprietary implementation is incorporated. The
  [feedback record](docs/fntech3/2026-09-21-stvef-force-feedback.md) identifies
  the reference binaries, native observations and device-test limits.

## Retail-content boundary

The [ET/Elite Force input follow-up](docs/fntech3/2026-10-02-input-launch-fixes.md)
uses ET's player-alias and placeholder facts observed in the original
[client game](https://github.com/id-Software/Enemy-Territory/blob/40342a9e3690cb5b627a433d4d5cbf30e3c57698/src/cgame/cg_players.c)
and [sound engine](https://github.com/id-Software/Enemy-Territory/blob/40342a9e3690cb5b627a433d4d5cbf30e3c57698/src/client/snd_dma.c),
and independently observed Elite Force retail input registration and button
constants. These are reference observations only; no GPLv3 source text,
proprietary executable code or retail asset is incorporated. The placeholder
adaptation extends fnTech3's existing independently authored RTCW PCM helper.

The RTCW audio-zone authoring mode uses material-flag facts observed in the
original [SP surface flags](https://github.com/id-Software/RTCW-SP/blob/master/src/game/surfaceflags.h)
and [MP surface flags](https://github.com/id-Software/RTCW-MP/blob/master/src/game/surfaceflags.h),
cross-checked against [iortcw](https://github.com/iortcw/iortcw).
The acoustic classification is independently authored; no GPLv3 implementation
text is incorporated. The 47 stock sidecars are generated acoustic metadata
from a legitimate local installation, with source/output identities and limits
in the [generation record](docs/fntech3/2026-09-17-rtcw-audio-zones.md).

The 47 RTCW static light files are generated from the maps' own `light`
entities. The entity keys and spawnflags were observed in the original
`QUAKED light` comments ([SP](https://github.com/id-Software/RTCW-SP/blob/master/src/game/g_misc.c),
[MP](https://github.com/id-Software/RTCW-MP/blob/master/src/game/g_misc.c)) and
the `dlight` entity's radius in the [SP cgame](https://github.com/id-Software/RTCW-SP/blob/master/src/cgame/cg_ents.c);
the falloff constants in the Wolf lighting model of q3map2
([NetRadiant](https://gitlab.com/xonotic/netradiant), `tools/quake3/q3map2`,
GPL-2.0-or-later), checked against the maps' baked lightmaps. All three were
read as reference observation only; no implementation text is incorporated.
Identities, rules and limits are in the
[generation record](docs/fntech3/2026-09-24-rtcw-map-lights.md).

fnTech3 does not redistribute Quake III Arena, Quake Live, Return to Castle
Wolfenstein, Enemy Territory, Elite Force or Jedi Academy data, retail game
modules, Bink or
other proprietary runtimes, Steam SDK/runtime binaries, Awesomium,
credentials, captured private traffic, or other proprietary content. Runtime
validation uses the user's
legitimate local installations and records only non-proprietary evidence.

## Game icon artwork

Thirteen [game application icons](assets/icons/games/README.md) are imagegen
remakes of the original games' executable icons; standalone MOHAA artwork is
also remade with imagegen. RTCW single-player retains
the previous artwork pending its blocked remake. Retail icons, Alice's menu portrait
and the F.A.K.K.2 installer splash were visual references; extracted reference
artwork is not included. The [generation record](assets/icons/games/PROMPTS.md)
documents the prompts. Game names, characters, logos and symbols remain the
work of their respective creators and owners.

The [social banner and README silhouette strip](assets/logos/README.md) use
these generated remakes as visual references for new imagegen raster artwork,
with one icon per title. Their [generation record](assets/logos/PROMPTS.md)
documents the ten-icon rows.

## Project thanks

Thanks to the Quake, Quake III, Quake Live, Return to Castle Wolfenstein,
Star Trek Voyager: Elite Force and Jedi Knight communities: players, server administrators, mod authors, mappers, tool authors,
testers, reverse engineers, and maintainers who keep these games understandable
and playable.

## Trademark note

fnTech3 is independent and is not affiliated with, endorsed by, or sponsored by
id Software, Bethesda, ZeniMax Media, Valve, Raven Software, Activision or
Paramount. Quake, Quake III Arena, Quake Live, Return to Castle Wolfenstein,
Star Trek and Elite Force are trademarks of their respective owners.
