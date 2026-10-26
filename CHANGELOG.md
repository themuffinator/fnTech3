# fnTech3 Changelog

Reviewed, user-visible changes to fnTech3. Add changes under **Unreleased** as
they land; `python scripts/release_workflow.py prepare --version X.Y.Z` freezes
them into a dated entry before tagging. Released entries are retained.

fnTech3 starts at **0.1.0**. FnQuake3 and FnQL versions and release notes are
historical upstream records, not fnTech3 release history. See the
[release guide](docs/fntech3/RELEASING.md) for versioning and publication rules.

## [Unreleased]

- Make all sixteen game profiles accessible in every build. Legacy development
  switches no longer disable games, and experimental warnings are informational.
  Native-module platform requirements and documented compatibility limits remain
  ([record](docs/fntech3/2026-10-11-profile-availability.md)).
- Publish ordinary releases only: release tooling and package manifests explicitly
  disable prerelease status while retaining runtime evidence requirements.
- Resolve installation icon paths through Windows file handles before checking
  containment, preventing MinGW from accepting an external junction target
  ([record](docs/fntech3/2026-10-11-icon-containment.md)).
- Fade Medal of Honor's sound when a level ends or restarts, as the game asks.
  The command had reached the client game, which reported it as unknown, and
  the loading bar now recognises model definitions that are already loaded
  ([record](docs/fntech3/2026-10-14-compatibility-round.md)).
- Draw Jedi Academy's lightsaber glow and core lines as the original renderer
  builds them, checked against its own vertices; a recorded match no longer
  stops at the first lit saber
  ([record](docs/fntech3/2026-10-14-compatibility-round.md)).
- Let Jedi Academy multiplayer's shots hit players: every trace missed a
  player's box, which carried Quake III's body bit instead of Academy's, and
  the server's Ghoul2 trace now checks a shot against the body's own
  triangles as the original does, so a concussion rifle shot no longer stops
  the match on reaching the server
  ([record](docs/fntech3/2026-10-15-jka-g2-trace.md)).
- Play Jedi Academy maps whose speakers and effects carry sound sets, such
  as `mp/ctf3`: the client no longer stops on their first frame, and each
  set's loop and occasional sounds play where they are, checked against the
  original engine's own routines
  ([record](docs/fntech3/2026-10-15-compatibility-round.md)).
- Let an Enemy Territory level close an area portal that is already shut,
  as the original executables do, instead of ending the level
  ([record](docs/fntech3/2026-10-15-compatibility-round.md)).
- Start Jedi Academy siege games: the server keeps a round's time to beat for
  the next round, as the original does, though a siege game still stops when
  the class menu opens. Maps whose sky shows a distant view of its own, such
  as `mp/duel6`, now draw that view as the original renderer does
  ([record](docs/fntech3/2026-10-15-jka-siege.md)).
- Send Jedi Academy and Jedi Outcast players what the original servers
  send. A speaker or other fixture that Academy's game marks permanent is no
  longer sent twice, so its sounds are no longer counted twice; an entity
  meant for a list of players also reaches every player who can see it; and
  in Outcast a player shown through walls by Force Sight, or as the Jedi
  Master, now reaches the players the game names. A new level also keeps
  the game's clock running, as both original servers do
  ([record](docs/fntech3/2026-10-15-jedi-broadcast-level-time.md)).
- Keep the game's clock running from one level to the next in Quake III,
  Return to Castle Wolfenstein, Enemy Territory and Elite Force Holomatch
  servers, as the original servers do, even with no one connected. Quake Live
  servers start every level's clock again, as the original does, even with
  players connected ([record](docs/fntech3/2026-10-15-level-clock.md)).
- Start every Jedi Academy and Jedi Outcast single-player level's clock at
  one second, as the original engines and Elite Force's do, instead of at
  zero or at the last level's time
  ([record](docs/fntech3/2026-10-16-jedi-sp-level-clock.md)).
- Open Jedi Academy's siege class menu and the game's other menus as the
  original does: the menu the game asks for now reaches its own menus by
  the game's number, so a siege game no longer stops when its class menu
  opens ([record](docs/fntech3/2026-10-15-jka-ui-menus.md)).
- Let Jedi Academy's `mp/siege_hoth` and `mp/siege_desert` play past the
  siege class menu, and `mp/siege_hoth` as a free-for-all: the game's
  question whether a model needs its folder's default skin is now answered
  as the original answers it, instead of stopping the game
  ([record](docs/fntech3/2026-10-16-jka-skinless-model.md)).
- Let Jedi Academy's siege rounds start with a bot on the other side: the
  bots' ground checks, which ask the server to ignore no one, no longer stop
  the server ([record](docs/fntech3/2026-10-16-jka-trace-pass-none.md)).
- Give Enemy Territory's bot navigation one search record per area, as the
  original server does: when a mod's bots look for cover, a retreat or nearby
  areas, the server no longer writes past the end of those records, and the
  danger and retreat searches no longer lose areas to an earlier search
  ([record](docs/fntech3/2026-10-15-census-small-gaps.md)).
- Measure the pure server's room for its pak list with the game's own
  messages: a big Enemy Territory map keeps checking players' files instead of
  falling back, and a busy Jedi Outcast server no longer publishes a list its
  players cannot receive ([record](docs/fntech3/2026-10-15-census-small-gaps.md)).
- Measure a Medal of Honor pure server's room for its pak list in its own
  messages, counting the sounds a multiplayer game adds once the level has
  loaded: a big map keeps checking players' files instead of falling back,
  and the list is dropped only when it really would not fit. The IHV Test
  and Q3Test, which never checked players' files, now say that `sv_pure` has
  no effect ([record](docs/fntech3/2026-10-15-pure-budget-own-wires.md)).
- Add `cl_viewShake`, from `0` (no shake) to `1` (the game's own shake, the
  default), to turn down the view shake the engine hands a game. Jedi
  Academy multiplayer's muzzle flashes and explosions honour it; the other
  games make their shakes inside their own code, out of its reach
  ([record](docs/fntech3/2026-10-15-view-shake.md)).
- Let Enemy Territory bots find cover as the original server does: a mod's
  bot hiding from an enemy now skips ladders, keeps clear of the enemy, avoids
  occupied spots and asks the game whether each spot is really out of sight,
  within the original's work limit for each frame
  ([record](docs/fntech3/2026-10-15-wolfet-hide-search.md)).
- Scale Jedi Academy's HUD and both Jedi games' menus to the window. They
  draw on a 640x480 screen the originals stretched over the window, and had
  filled only its top-left corner; on a widescreen display Academy's HUD now
  keeps its shape at the screen's corners, and `cl_menuAspect 1` centres the
  menus ([record](docs/fntech3/2026-10-15-jedi-widescreen-hud.md)).
- Draw the skies of Jedi Academy maps such as `mp/duel9`, which showed
  plain grey. Both Jedi games' sky shaders name their sun with a `sun` line,
  which the shader parser now reads as the original engines do, along with
  Raven's `lightColor` and Jedi Outcast's `noTC`
  ([record](docs/fntech3/2026-10-15-raven-sun-keyword.md)).
- Grow the grass, reeds and splashes of both Jedi games' outdoor ground,
  such as the grass field on Jedi Academy's `mp/ctf3`, which drew as a black
  and white grid. Their shaders' `surfaceSprites` stages are now read as the
  original engines read them and drawn as sprites over the ground in all
  three renderers; `r_surfaceSprites 0` turns them off
  ([record](docs/fntech3/2026-10-15-jedi-surface-sprites.md)).
- Cut the grass and reeds of both Jedi games cleanly at their edges, as
  the original engines do. Their shaders' `alphaFunc GE192` was not
  understood, so each blade drew with soft fringes and its see-through
  parts hid the grass behind it
  ([record](docs/fntech3/2026-10-16-jedi-alphafunc-ge192.md)).
- Light Jedi Academy's and Jedi Outcast's maps under RTX with their real
  sky and its sun: the path tracer read the maps' surface flags with Quake
  III's values, so ground and walls whose material shares a bit with Quake
  III's sky were taken for sky, and `mp/ffa3` lost its sun. All three
  renderers now also leave out the hidden patches the original engines skip
  ([record](docs/fntech3/2026-10-15-raven-bsp-shader-flags.md)).
- Stop RTX from letting light through the stone, metal and wood of Medal of
  Honor's, F.A.K.K.2's and Alice's maps: their map flags use other values
  for Quake III's light-filter bits, and each title's own alpha-shadow
  surfaces now filter the path tracer's light instead
  ([record](docs/fntech3/2026-10-15-map-shader-flag-dialects.md)).
- Light Medal of Honor's outdoor maps with their sun under RTX. The maps name
  the sun and the sky's light in their level settings, not in a sky shader,
  so the path tracer had lit them with no sun; their streets and rooftops
  now take the sun, the sky and the shadows the game's own lighting shows
  ([record](docs/fntech3/2026-10-16-mohaa-worldspawn-sun.md)).
- Let Jedi Academy's flickering and switched lights flicker and switch: the
  client game's light colours now reach GLx, Vulkan and RTX's raster path,
  which relight just the parts of the map each light touches. Jedi Outcast's
  multiplayer client hands its colours over too. `r_debugStyle`, the
  original single-player renderers' cheat, shows one light at a time
  ([record](docs/fntech3/2026-10-16-raven-light-styles-animated.md)).
- Load every Jedi Academy and Jedi Outcast map: 45 maps with switchable or
  flickering lights, among them Outcast's `ffa_ns_streets`, stopped with
  "light styles are not served yet". Their extra lights are now added in,
  each at full brightness for now
  ([record](docs/fntech3/2026-10-15-raven-light-styles.md)).
- Load Jedi Outcast's explosion shaders and the one-shot doors, panels and
  screens of its campaign, which were drawn with the default image: both
  Outcast profiles now read Raven's `oneshotanimMap` and `clampanimMap`, as
  the original engines do
  ([record](docs/fntech3/2026-10-15-jk2-animmap-variants.md)).
- Keep each of Jedi Academy's weapon, Force power and item bars together
  with its name on screens taller than 4:3, where the name was drawn over
  the bar's middle icon. The bars and the loading screen are now checked in
  captures ([record](docs/fntech3/2026-10-15-jka-hud-live-check.md)).
- Let Jedi Academy's binoculars and disruptor rifle zoom in, where they
  stopped the game. Each sight stretches over the screen with its readouts
  in place, the disruptor's turning rings follow it, and a zoom sees
  further into a foggy map, as the original's does
  ([record](docs/fntech3/2026-10-15-jka-zoom.md)).
- Let Jedi Academy multiplayer players respawn. The game clears the
  marks hits left on a player's model when they come back, which stopped
  the match at the first respawn; fnTech3 keeps no such marks yet, so it
  now checks the player's model and carries on
  ([record](docs/fntech3/2026-10-16-jka-clear-skin-gore.md)).
- Let Jedi Academy multiplayer games hide and show parts of a model. The
  game closes a dead player's eyes, and turns off parts of vehicles and
  other characters, through a request fnTech3 refused, which stopped a
  siege match soon after a bot's body appeared; it is now served as the
  original serves it
  ([record](docs/fntech3/2026-10-16-jka-surface-on-off.md)).
- Keep the marks hits leave on Jedi Academy multiplayer models. The game
  can now add, count and clear them as the original engine does, and they
  expire on its clock. They are not drawn yet, and a fight still stops
  before the first mark, where the game checks what a hit touched
  ([record](docs/fntech3/2026-10-17-jka-skin-gore.md)).
- Take Jedi Outcast's multiplayer client game through its start: where it
  stopped before any map loaded, it now loads the level and registers its
  effects, as the original engine serves them. It still stops before the
  first picture, at the player models, so maps are not drawn yet
  ([record](docs/fntech3/2026-10-16-jk2-cgame-services.md)).
- Draw Jedi Outcast's multiplayer maps: the level, your weapon, the HUD and
  the scoreboard now draw, the game's messages read its own text where they
  showed "??", and the HUD keeps its shape in the corners of a wide window,
  with the scoreboard in the middle, as Jedi Academy's does. Other players
  and sabers have not been seen in a game yet
  ([record](docs/fntech3/2026-10-16-jk2-cgame-ghoul2.md)).
- Open Jedi Academy's and Jedi Outcast's single-player menus. Neither game
  ships a menu program -- their engines run the menus themselves -- so
  fnTech3 now does too, from each game's own menu files: the main menu, a new
  game, loading, controls, setup and quitting all work, and a failed game
  start shows its reason in the game's own error box. Character creation's
  lists and models, saved games and the in-game menus are still to come
  ([record](docs/fntech3/2026-10-16-jedi-sp-menus.md)).
- Show the text language in Jedi Academy's single-player setup menu, which
  was blank: the menu now lists the installed languages, as the original
  does. Both Jedi games' menus have now been driven with clicks and keys
  through the engine's own test input
  ([record](docs/fntech3/2026-10-17-jedi-menu-input.md)).
- Widen the view in every game with `r_fovCorrection 1`. Jedi Outcast's
  multiplayer world was drawn stretched sideways on wide screens, and the 1999
  IHV Test and Q3Test versions ignored the setting. Under water the view now
  sways as the game sways it, and Jedi Academy's grass keeps its distances
  when you zoom on very wide screens
  ([record](docs/fntech3/2026-10-22-fov-correction-all-profiles.md)).
- Let Jedi Academy multiplayer check a hit against the body itself on the
  server and in the client: stormtroopers spawned from the console no longer
  stop the client, and a hit names the body part it struck, as the original
  does. Jedi characters now move in the RTX path tracer's view and shadows
  instead of keeping their first pose
  ([record](docs/fntech3/2026-10-22-jka-g2-collision.md)).
- Let every game's models cast fnTech3's shadows. Enemy Territory's soldiers
  and the characters of Alice, F.A.K.K.2 and Medal of Honor now cast shadows
  from dynamic lights, and with `r_csmShadows 1` those and Return to Castle
  Wolfenstein's characters cast and take the sun's. Alice, F.A.K.K.2 and
  Medal of Honor, whose levels name their sun themselves, now have one for
  those shadows ([record](docs/fntech3/2026-10-22-fnq-shadow-formats.md)).
- Light Alice's and F.A.K.K.2's levels with their own sun in RTX's path
  tracer, which traced them without one, and stop a local game from
  sometimes hanging at "connecting" after a long map load: the server
  dropped its own player as silent. Medal of Honor's light grid now reads its
  sun keys only as the original renderer spells them
  ([record](docs/fntech3/2026-10-22-fnq-shadow-formats.md)).
- Stop the sun's shadows (`r_csmShadows 1`) from shading walls and floors a
  second time. A level's own sun shadows are already in its lighting, so
  walls and floors now take only the shadows of models and doors, only in
  the sun, and only as deep as the sun lights them there; models still take
  every shadow. A lamp-lit room under a roof, as at the start of
  F.A.K.K.2's `shield`, no longer darkens
  ([record](docs/fntech3/2026-10-22-fnq-shadow-formats.md)).
- Light Return to Castle Wolfenstein's levels in RTX's path tracer with a
  sun as strong as their own lighting had it; it was traced about a third
  too dim. The sun shadows read each map's sun the same way, so a Quake III
  map built with q3map2 is shaded by its sun's real strength
  ([record](docs/fntech3/2026-10-22-fnq-shadow-formats.md#sky-suns-in-the-path-tracer)).

- Draw the sun's shadows (`r_csmShadows 1`) and the dynamic lights' shadows
  in RTX on levels with a distant sky view, such as Return to Castle
  Wolfenstein's `forest`, where RTX drew neither while OpenGL and Vulkan
  drew both ([record](docs/fntech3/2026-10-22-rtx-portal-sky-shadows.md)).
- Apply RTX's optional global fog to the world behind a distant sky view,
  and make Vulkan/RTX reject planar light receivers and shadow casters
  outside the light's reach, as GLx already does. The recorded forest
  caster counts now agree across renderers
  ([record](docs/fntech3/2026-10-22-portal-fog-caster-parity.md)).

- Let Jedi Academy multiplayer hide and show parts of a body on the server,
  as a lost limb or a character's skin does, so hits pass where a part is
  hidden, and take models off a body on the server and in the client, as
  the game does when a weapon or saber changes hands. A siege round on
  `mp/siege_hoth` or `mp/siege_desert` with a bot no longer stops as the
  players enter it
  ([record](docs/fntech3/2026-10-22-jka-g2-surfaces.md)).

- Register Elite Force's console commands and settings as the original
  engines do. The turn speed the controls menus set is saved and can be
  changed from the console, single player walks
  unless Shift or Always Run is used, `+careful` can be bound, `sv_cheats 1`
  turns cheats on in the middle of a single-player level, and Holomatch has
  `rserverinfo`. Both modes send the original's player settings, so a
  single-player save lists the settings an original save does
  ([record](docs/fntech3/2026-10-23-stvef-retail-registrations.md)).
- Filter textures in Vulkan and RTX as the GL renderers do: the anisotropy
  setting now applies with the bilinear texture mode too, and a light flare
  sitting just behind its surface shows as it does in GLx
  ([record](docs/fntech3/2026-10-23-stvef-renderer-parity.md)).
- Fade Elite Force Holomatch's holodeck room under RTX's ray tracing as the
  other renderers do: anything the game fades with its own alpha flag is
  now drawn the normal way while it fades, where the trace had drawn the
  room solid and hidden the level behind it
  ([record](docs/fntech3/2026-10-24-stvef-playability.md)).
- Find Elite Force Holomatch games on the internet again. The original
  server list has shut down, so the browser also asks the community list
  Lilium Voyager uses, `efmaster.tjps.eu` (INTERNET2 in the menu), and it now
  reads Elite Force server lists as the original game does, where it had
  shown none ([record](docs/fntech3/2026-10-24-stvef-playability.md)).
- Draw Heavy Metal: F.A.K.K.2's canal water and caustic walls the lime green
  the original shows, and Alice's and F.A.K.K.2's fog volumes as thin as the
  original draws them, in all three renderers; the damaged-demo probe now
  runs under both games
  ([record](docs/fntech3/2026-10-23-fakk2-gap-round.md)).
- Light a model in Alice and F.A.K.K.2 from a light it holds as the originals
  do, and let an additive light add its own colour to the walls and floor
  round it, in either dynamic-light mode and under RTX's ray tracing, so
  the glowing dung pile on F.A.K.K.2's `creeperpens` is green and casts its
  green glow, as in the original
  ([record](docs/fntech3/2026-10-24-fakk2-playability-round.md)).
- Light a map's spot lights only inside their cones, in every renderer, for
  Return to Castle Wolfenstein, Alice, F.A.K.K.2 and Medal of Honor. A spot
  lit everything round it, so a lamp brightened the walls behind and beside
  it and a dark passage on F.A.K.K.2's `cemetery`; it now lights what it
  points at
  ([record](docs/fntech3/2026-10-24-static-spot-cones.md)).
- Read joysticks and gamepads on Windows in every game, as most of the
  originals did, once `in_joystick 1` or the game's controls menu switches
  them on: the
  Windows download through WinMM with Quake III's key mapping, SDL3 builds
  through SDL3's gamepad support. Both look for a controller plugged in
  after the game starts; the Windows download does so when Windows reports
  a new device, never every frame. Windows builds had compiled joystick
  input out since the code base began
  ([record](docs/fntech3/2026-10-24-windows-joystick.md)).
- Honour Elite Force's "X axis as button" and "Y axis as button" settings
  with a joystick in the Windows download: with one off, that direction of
  the stick turns or moves the player smoothly, as in the original, and a
  full stick turns at the original's 1.27 times the key speed in both modes
  ([record](docs/fntech3/2026-10-24-windows-joystick.md#elite-forces-axis-switches-and-turn-scale)).
- Read joysticks and gamepads in SDL3 builds on Linux and macOS too, and
  give a recognised gamepad keys the games' own controls menus can bind:
  `JOY1` onwards and the arrow keys, as their original engines reported a
  joystick. Quake Live keeps its named `PAD0_*` gamepad keys. SDL3 builds
  no longer start looking for controllers until joystick input is switched
  on ([record](docs/fntech3/2026-10-25-sdl3-joystick.md)).
- Make Medal of Honor's campaign quick to respond. The truck ride that opens
  `m1l1` ran at 60 to 75 frames a second at 1280x720, because each model was
  skinned again for every pass that drew it and every face of a dynamic
  light's shadow rebuilt its casters. Models are now skinned once a frame,
  and every game's dynamic-light shadows build a caster once for all six
  directions: the ride runs about three times as fast in Vulkan, with the
  same picture ([record](docs/fntech3/2026-10-25-mohaa-frame-time.md)).
- Build each of Alice's and F.A.K.K.2's model surfaces once a frame. The
  renderers built a model's shape again for every pass that drew it -- the
  view, each dynamic light on it, the sun's cascades and the lights'
  shadows; now the frame's other passes copy the first build. In the
  busiest views found, 110 to 200 model surfaces a frame, that saves 0.08
  to 0.19 ms a frame of the 0.3 to 0.43 ms the models took, with the same
  picture; `r_ritualModelReuse 0` builds them every time, for comparison
  ([record](docs/fntech3/2026-10-26-ritual-model-frame-time.md)).
- Stop spending shadow work on dynamic lights the view cannot see. A light
  behind the player still lit the models in view that it touched, so it
  was given a shadow; on `m1l1`'s truck ride the lantern hanging behind
  the player cost about 4 ms a frame for nothing on screen. Every game draws the
  same picture, faster where such lights occur
  ([record](docs/fntech3/2026-10-26-mohaa-truck-ride-lights.md)).

## [0.1.0] - 2026-10-11

### Highlights

- Correct cross-platform UI validation, Jedi and IHV numeric rounding,
  Outcast SP allocation alignment, RTCW prediction/skin validation, RTX
  model-light rounding, Windows installation/module path containment and
  MSYS2 release tag resolution found during release validation
  ([record](docs/fntech3/2026-10-11-release-portability.md)).

- Restore Medal of Honor's terrain and static map scenery in RTX path
  tracing, fixing missing ground on `m1l1` and the same omission on other
  maps ([record](docs/fntech3/2026-10-11-mohaa-rtx-world.md)).

- Restore Medal of Honor's barrel explosions, smoke, debris and other
  effects from hidden emitters across GL, Vulkan and RTX. The engine had
  discarded visible particles carrying their emitter's hidden flag
  ([record](docs/fntech3/2026-10-11-mohaa-effects.md)).

- Introduce fnTech3's shared engine distribution and startup game profiles,
  based on the documented FnQuake3 and FnQL baselines. Quake III Arena and
  Quake Live start in every build; Return to Castle Wolfenstein, Enemy
  Territory, Elite Force, Jedi Academy, Jedi Outcast, American McGee's Alice,
  Heavy Metal: F.A.K.K.2, Medal of Honor: Allied Assault, the Quake III
  IHV Test and the Q3Test releases are experimental profiles that start only
  in developer builds. Supply a legitimate installation of the selected game
  ([Games Guide](docs/GAMES.md)).
- Find and start your games with `fntech3-launcher`. It looks in every Steam
  library (Flatpak and Snap too), GOG Galaxy and GOG installers, Heroic and
  Lutris, the registrations disc installers leave, Wine prefixes and the usual
  game folders, and lists each installation the engine itself recognizes with
  its game icon, its store and its folder; the player can add any other
  folder, or drop it on the window. Play starts the client beside the
  launcher with the installation's profile and folder named explicitly, in
  the chosen single-player or multiplayer mode and with a mod and extra
  command line if set. Games this build cannot start are listed apart with the
  reason. Built with SDL3 and Dear ImGui (Meson option `launcher`, on by
  default; CMake `BUILD_LAUNCHER`) ([guide](docs/LAUNCHER.md)).
- Path trace the main view with the RTX renderer's `rtx_rt_mode 2`. The world,
  posed models and the first-person weapon are traced with their shader
  stages as authored and lit by the level's own lights in place of its
  lightmaps, with no per-map setting: the light entities its map keeps
  (Quake III's and RTCW's maps keep the lights their designers placed;
  q3map2 left none in Quake Live's and Enemy Territory's), its sky and
  `q3map_sun`, and the surfaces that give off light -- `q3map_surfacelight`
  shaders, glow stages, additive flames, and the bright texels of shaders
  drawn fullbright, which the scene now lights as it lights any surface --
  with traced bounces in place of the compilers' ambient and `_minlight`
  and nothing baked -- no lightmap, vertex light or light grid is read --
  shown as traced, in the units the map compiler gave its lights, with no
  exposure (`rtx_rt_camera 1` sees it in linear light through Q2RTX's tone
  mapper: a curve fitted to each view's histogram, as Eilertsen, Mantiuk and
  Unger fit theirs, to the light a surface receives so that textures keep
  their detail, blended with Reinhard's curve behind an exposure metered on
  the view's 70th to 90th brightness percentiles; `rtx_rt_lighting 3` lights
  from the sky and emitters alone, as Q2RTX lights Quake II, and `1` and `2`
  from the lightmaps and the map compilers' baked light models, for
  comparison; `alphashadow` and `lightfilter` surfaces filter the light as
  the compilers read them), and by the
  game's dynamic lights, shadowed with a penumbra from a body a twentieth of
  their reach and reaching what the raster light pass
  reaches (RTCW's darkening and flame light materials multiply what they
  reach, as its renderer drew them), plus up to four
  diffuse bounces (`rtx_rt_bounces`) and glossy reflections that share the
  map's surface lights with light sampling (`rtx_rt_mis`), then denoised
  along each moving model's own motion; blended surfaces, effects, fog, a
  portal sky and the HUD keep drawing with them as in raster, and RTCW's and
  Enemy Territory's global fog fogs traced surfaces as it fogs raster ones.
  Drivers without the Vulkan ray-tracing pipeline, such as the 32-bit one the
  shared Windows client runs on the development machine, trace with compute
  shaders instead (`rtx_rt_backend`). A texture pack may add normal,
  specular, roughness and metalness maps under OpenJK rend2's names
  (`rtx_rt_material_maps`). It replaces the earlier hybrid tracer and its
  experimental controls, among them `rtx_rt_dynamic_blas`,
  `rtx_rt_material_heuristics`, `rtx_rt_raster_reference` and the exposure,
  temporal and quality-preset cvars ([guide](docs/RTX.md)).
- Edit a map's entities inside the game, in every game fnTech3 runs.
  `entityeditor` opens a free camera over the running map with every entity
  drawn where it stands -- the nearest as boxes coloured by kind, the rest as
  dots, with classnames, facing arrows, brush-model bounds and lines to the
  entities each targets -- under a [Dear ImGui](https://github.com/ocornut/imgui)
  interface drawn by the game's own renderer: a menu bar, a filterable entity
  list and a panel of the selected entity's keys. Entities are selected with
  the mouse, several at once with Ctrl or Shift or by dragging a box over
  them, then dragged or moved along axis handles, nudged, turned, dropped to
  the floor, made, duplicated, deleted and re-keyed together, and copied and
  pasted as text between maps, with undo and redo, and every action is also
  a console command. Saving writes the map's entity override,
  `maps/<map>.ent`; Apply saves and restarts the level so the game spawns the
  edits, loading the map again when the entities to spawn changed. The
  server now loads `maps/<map>.ent` in place of a
  BSP's entities in every profile (`sv_entityOverrides`, on by default), after
  checking it as the games read entity text and falling back to the BSP's with
  the reason when a game would refuse it; the game module, its bots and, on a
  listen server, the client game all read the same text. Quake Live's
  `sv_altEntDir` still comes first. `sv_saveentfile` writes the running
  level's entities as a starting file ([guide](docs/ENTITY_EDITOR.md),
  [record](docs/fntech3/2026-10-05-entity-editor.md)).

### Compatibility

- Keep American McGee's Alice's game module when a save of the running level
  loads or the level restarts, as `alice.exe` does: its models,
  configstrings and players stay, the module only cleans up, and the client
  takes the new server count as retail's does, stopping every sound but the
  music. Heavy Metal: F.A.K.K.2 does the same, and a FAKK map's checksum is
  now the one its header carries
  ([record](docs/fntech3/2026-10-08-alice-compatibility-round.md)).
- Run Alice's and F.A.K.K.2's pure servers as their engines do: the client
  keeps to the server's pak list, from its own server too, reads no loose
  file but configurations and saves, and neither side asks for or sends a
  pure proof. Both servers publish only their engines' own server and system
  info keys, so a retail client sees none of fnTech3's, and the client takes
  its cheat state from `cheats`
  ([record](docs/fntech3/2026-10-08-alice-compatibility-round.md)).
- Complete Alice's developer menus: the file pickers behind `soundpicker`,
  `viewspawnlist` and `maplist`, `loadmenu`, and the inventory commands;
  scrollbars repeat while an arrow is held; the controls menu's Alice turns
  her head after the cursor; and `vid_restart` leaves the menus and the
  console as they were
  ([record](docs/fntech3/2026-10-08-alice-compatibility-round.md)).
- Freeze Alice's module signatures. Every slot of its four module tables now
  has a type read from the retail binaries, and the engine's table fills and
  export calls must match those types to compile. Five game imports served
  at the wrong shape are corrected; a module's file read keeps quiet when it
  asks to, a flush leaves the file unbuffered, and a server command reaches
  every client only for -1, as in retail
  ([record](docs/fntech3/2026-10-08-alice-module-signatures.md)).
- Host and join each game's original servers as its own engine does. A
  public server reports to the game's own master with its own heartbeat --
  Quake III's, RTCW's, Enemy Territory's, Elite Force's, Jedi Academy's and
  Jedi Outcast's -- and the server browser asks the same master; Jedi Academy
  and Jedi Outcast use their own ports, 29070 and 28070. Retail RTCW's browser
  now lists fnTech3's servers, which it had dropped for a missing game name,
  and fnTech3's Elite Force browser lists Holomatch servers. A game started
  from the client's menu now admits remote players, joining a server that
  runs a mod keeps the console log, Enemy Territory fetches a server's files
  from its web address (`cl_wwwDownload`), Elite Force and Jedi Knight
  servers take `svsay`, their own console chat, a client that connects again
  to a pure server is no longer dropped for a pak only it has, a server's own
  `sv_maxclients` sizes its first client array, so that RTCW's and Enemy
  Territory's first warm-up no longer reloads the map for every player, and
  `sv_minPing` and `sv_maxPing` work as in the original games
  ([record](docs/fntech3/2026-10-05-legacy-server-hosting.md)).
- Keep Jedi Academy's multiplayer client on a retail server through chat and
  a map change. The client game's local sounds, each chat line's among them,
  play through fnTech3's own sound system with Academy's sound channels
  mapped to the engine's, where the client had stopped at the first one, and
  a server's map change no longer ends the session: the engine had refused
  Academy's map-change call as Enemy Territory's snapshot query, which shares
  its number. On the new map the client still stops where its game draws a
  player carrying a weapon ([record](docs/fntech3/2026-10-06-jka-live-join.md)).
- Draw Jedi Academy's multiplayer models that hang from another, such as the
  weapon in a player's hand. A model the client game links to another's bolt
  is posed from that bolt with its own skeleton, in the original engine's
  order and with its frame for tag surfaces, and drawn through fnTech3's own
  skeletal path with its materials, lighting and shadows; the client game's
  bolt queries, such as a weapon's muzzle, answer from the same pose. On a
  retail server the client now passes the weapon and stops next at items
  fading in with a colour tint ([record](docs/fntech3/2026-10-06-jka-attachments.md)).
- Draw the items Jedi Academy's multiplayer client game tints or fades: its
  colour (`RF_RGB_TINT`) replaces a stage's own, and its alpha
  (`RF_FORCE_ENT_ALPHA`) blends single-texture stages, which then draw after
  the rest of the view, as the original renderer does, in all three of
  fnTech3's renderers; items fading in, greyed force items and simple-item
  sprites are among them. The client game's decal clear is answered. On a
  retail server the client now passes `map_restart`, a reconnect and the
  server quitting on `mp/ffa1`, and stops on `mp/ffa3` at its ambient sound
  set ([record](docs/fntech3/2026-10-06-jka-entity-tint.md)).
- Play Jedi Academy multiplayer's ambient sound sets, the background sounds
  a map and its areas name: the set the client game names fades in over a
  second as the one before fades out, and its loop and its occasional
  sounds play at the volumes and intervals the original engine draws,
  through fnTech3's own loop voices and one-shots, with their room reverb.
  On a retail server the client now passes `mp/ffa3` and completes every
  step of the standard session on both maps
  ([record](docs/fntech3/2026-10-06-jka-ambient-sets.md)).
- Play Jedi Academy's German, French and Spanish voices. As in the original
  game, `s_language` chooses them, `deutsch`, `francais` or `espanol` as its
  setup menus write it, and a line with no recording in that language plays
  in English; `snd_restart`, which the menus' Set Language buttons run,
  reloads the voices. The multiplayer client plays them; single player, which
  has no client yet, takes them the same way
  ([record](docs/fntech3/2026-10-09-jka-foreign-voices.md)).
- Answer GameSpy's LAN queries from Medal of Honor servers. A multiplayer
  server, dedicated or started from Start New Server, answers GameSpy's
  status queries on UDP 12300 or the next free port (`net_gamespy_port`,
  `sv_gamespy`, the menu's GameSpy box) with its name, map, game type,
  players and rules, so GameSpy's browsers and the tools that speak its
  protocol find it on a LAN; GameSpy's masters are gone, and none is told
  of it. A team match started from the client no longer passes for single
  player, which had hidden it from the Join LAN Game list and turned away
  every remote player
  ([record](docs/fntech3/2026-10-06-mohaa-gamespy-lan-queries.md)).
- Find Medal of Honor servers on any game port in Join LAN Game. Refresh
  also makes GameSpy's LAN search, asking UDP 12300 to 12316 as GameSpy's
  own browsers did, so a server the usual scan of game ports 12203 to 12206
  misses is listed too. Only servers of version 1.00, the one fnTech3 joins,
  are shown, and a server that answers both ways is listed once
  ([record](docs/fntech3/2026-10-08-mohaa-gamespy-lan-search.md)).
- Load Jedi Outcast's sounds and music, which it stores as MP3 files. Both
  Outcast profiles now decode them, so the multiplayer menus play their own
  sounds, and, as the original engines do, a sound's `.wav` file is used
  before an `.mp3` of the same name, whichever one the game asks for
  ([record](docs/fntech3/2026-10-08-jk2-mp3-sounds.md)).
- Play Jedi Outcast's German and French voices. As in the original game,
  `s_language` chooses them, `deutsch` or `francais` as its setup menu writes
  it, and a line with no recording in that language plays in English;
  `snd_restart`, which the menu's Set Language button runs, reloads the
  voices. The multiplayer client plays them; single player, which has no
  client yet, takes them the same way
  ([record](docs/fntech3/2026-10-08-jk2-foreign-voices.md)).
- Give Jedi Academy and Jedi Outcast multiplayer their own single-player game
  types. `spmap` starts Jedi Academy's single player, 5, and Jedi Outcast's,
  4, where it had started Jedi Master; `map` keeps every Jedi Academy game
  type, and in Jedi Outcast turns only single player into a free-for-all, where
  it had turned Jedi Master into one. A server started from the client hides
  from LAN browsers and remote players only in Jedi Outcast's single player,
  and never by game type in Jedi Academy, as each original engine does; a
  Jedi Master game had been hidden in both
  ([record](docs/fntech3/2026-10-06-jedi-listen-server-game-types.md)).
- Let other players into a Quake Live match started from the game, by its
  server type as the original does. Offline (`sv_serverType 0`, the default)
  keeps the match to its host; LAN (1) lets in players on the local network
  and turns others away with "Server is for LAN clients only."; Internet (2)
  lets in anyone. Such a match had turned every other player away, whatever
  its server type, and a Race match was hidden as if it were single player
  ([record](docs/fntech3/2026-10-06-ql-listen-server-type.md)).
- Stop a long run of strings in a menu or bot file from crashing the game.
  Strings that a `#define` expands to, written side by side, are joined into
  one; the engine joined each by calling itself again, so about a thousand in
  a row ran it out of stack. They are now joined one after another, and every
  installed menu and bot file reads exactly as before
  ([record](docs/fntech3/2026-09-29-precompiler-string-runs.md)).
- Route Elite Force Holomatch bots as the retail engine does. Its routes
  count the time a bot needs to cross the area it is heading for, which
  fnTech3 left out as Quake III 1.32 does, so almost every route's travel
  time and, where two ways were close, the way a bot took differed from
  retail's on every map. On maps whose bot navigation Quake III's later
  compiler built, team-only and do-not-enter liquid areas are no longer
  avoided either, as retail does not avoid them
  ([record](docs/fntech3/2026-09-29-stvef-route-seed.md)).
- Tell Elite Force players why they left as the original servers do. A
  kicked Holomatch player is told `kicked`, and a player leaving or kicked
  gets Holomatch's own disconnect message, which the original client shows
  as it is; a quitting server says `server shut down`. Holomatch's `devmap`
  removes the bots, its `map` out of a single-player game keeps them, and a
  server running a single-player game answers no browser or connection, as
  the original server does. Single player announces a player leaving in its
  own words ([record](docs/fntech3/2026-10-08-stvef-server-messages.md)).
- List Elite Force single-player saves in the save and load menus with their
  time, date and level beside their descriptions, as the original does. The
  menus had shown stray characters there, because each save carried leftover
  bytes from the engine's memory in place of that line; a save now holds
  exactly what the original writes. Older saves still load
  ([record](docs/fntech3/2026-10-08-stvef-input-menus.md)).
- Keep Elite Force's game speed as the original engines do. A Holomatch
  server's game now advances in the original's 50 ms steps when `timescale`
  speeds it up or slows it down, where it had lengthened each step, which
  changed how bots fought; `timescale 0` leaves either mode at normal speed
  instead of stopping it; skipping a single-player cutscene runs as fast as
  in the original, two and a half times faster than before; and a level
  starts by catching up on the time its loading took, as the original does
  ([record](docs/fntech3/2026-10-08-stvef-frame-time.md)).
- List the same details for an Elite Force Holomatch server as the original
  does: server browsers and the game's log now see whether it is pure, and no
  longer see fnTech3's own download and rate settings. Its `sv_pure` setting
  is saved with the configuration, as the original's is
  ([record](docs/fntech3/2026-10-08-stvef-serverinfo.md)).
- Keep Elite Force Holomatch Elimination games running. Their bots
  sometimes ask the server to take the level's world out of play, which the
  original server accepts and fnTech3's had treated as an error, stopping
  the server with `SV_SvEntityForGentity: bad gEnt`
  ([record](docs/fntech3/2026-10-09-stvef-world-record.md)).
- Decode the last frame of an MP3 sound that stray bytes follow, as the
  original engines do: each frame is decoded from its own header, without
  waiting for the next. The Force Drain sound of Jedi Outcast and Jedi Academy
  had ended a frame, 26 ms, early, and so had 675 of F.A.K.K.2's sounds and
  music tracks, MP3 audio inside RIFF files, which the game's own Miles
  decoder plays whole. Every other MP3 file of Elite Force, the Jedi games,
  Alice, F.A.K.K.2 and Medal of Honor decodes exactly as before
  ([record](docs/fntech3/2026-10-08-mp3-last-frame.md)).
- Stop a server faulting when `bot_saveroutingcache 1` is set while its bots'
  navigation is still being prepared, as it is for a while after a map loads
  with `bot_forcereachability 1`. There are no routes to save yet: the server
  now says so and plays on, and saves them when asked again later
  ([record](docs/fntech3/2026-09-29-route-cache-before-routing.md)).
- Announce a Quake Live rage quit as the retail engine does. A player who
  leaves with `ragequit` was announced as `<name> disconnected`; everyone now
  sees `<name> rage quits`, with "rage" in red, as on retail servers. The
  retail game marks the player through an engine import that fnTech3 had
  taken for the pure check's own mark
  ([record](docs/fntech3/2026-09-29-ql-rage-quit.md)).
- Read back the bot route caches a server saves. With `bot_saveroutingcache 1`
  (a cheat in Quake III's game) the bot library writes the routes it has
  worked out to `maps/NAME.rcd`, and reads them when the map next loads, but
  the reader took each cache's size from its type byte and the server
  faulted on the next allocation. The dumps Quake III, Team Arena and Quake
  Live servers write now load and stay in use, and every cache is first
  checked against the map: a damaged dump, in the home or a pak, is refused
  whole and routing builds its caches as it goes
  ([record](docs/fntech3/2026-09-28-route-cache-checks.md)).
- Scale the light grid's ambient on models by each game's own
  `r_ambientScale`. fnTech3 used Quake III's `0.6` for every profile.
  American McGee's Alice and Heavy Metal: F.A.K.K.2 register `0.5`, so the
  props and items they light from the grid drew 2% to 14% brighter than
  retail at the same light. Alice now takes `0.5`, and F.A.K.K.2's profile
  takes it too when it lands; its props then draw within about 3% of
  `fakk2.exe`'s own frames. Their characters and weapons take the maps'
  compiled lights and are unchanged. Quake Live raised it to `10` and made
  it a player option: the `ql` profile now defaults to `10`, takes 1 to 100
  as retail does, keeps it in `repconfig.cfg`, and shows the Video page's
  Ambient Light Scale row, which had been hidden as a cheat control. A
  player model on `campgrounds` now draws about 2.4 times as bright as
  before, at retail's default. Quake III keeps `0.6`, as `quake3.exe` 1.32
  registers. RTCW, Enemy Territory, Elite Force and the Jedi single-player
  engines register `0.5` too and are left for now
  ([record](docs/fntech3/2026-09-25-ambient-scale.md)).
- **Play the Q3Test releases of 1999.** A `q3test` profile
  (`-Dq3test-development=true`, Windows x86) pins the release its
  installation holds -- 1.02 and 1.03's servers, 1.05 to 1.08's clients --
  and runs its own modules, protocol (35 to 40) and maps (IBSP 44 and 45,
  translated in memory with Q3Test's own curve collision). The dedicated
  server hosts each release's retail clients, up to the 128 every release
  allowed; the client joins fnTech3's and
  each retail server, runs 1.05 to 1.08's client games and 1.06 to 1.08's
  menus (1.05's engine menus are fnTech3's own, driving the same cvars and
  commands), browses LAN and master lists, and records and plays `.dm3`
  demos, the six shipped ones included. The releases' shader dialect
  (`fogParms` with five values, `fogonly`, `clampTexCoords`), their
  alpha-blended 2D pictures and a broken shader in 1.06 to 1.08's
  `gfx.shader` are read as each release read them, and serverinfo and
  systeminfo carry each release's keys. `scripts/probe_q3test.py` runs every
  pairing, including a headless protocol client for 1.02 and 1.03, which have
  no retail client, and checks the sound a demo replay mixes
  ([Q3Test ledger](docs/fntech3/Q3TEST.md)).
- **Play the Quake III IHV Test of February 1999.** The `q3-ihv` profile
  (`-Dq3-ihv-development=true`, Windows x86) now has a developer client
  beside its dedicated server. It runs the build's own client game, plays
  its `sli.dm3` on GLx, Vulkan and RTX, joins the original IHV dedicated
  server over protocol 33, and records `.dm3` demos the original plays too.
  fnTech3's IHV server takes the original client and fnTech3's together,
  publishes the original's serverinfo keys, and both keep playing through
  level changes and lost packets. The build's 2D art takes the colours its
  client game sets, its stageless scripts and its portal draw as its own
  renderer drew them, and its damage splat draws in its colour. Its menus
  are not drawn yet ([IHV ledger](docs/fntech3/Q3_IHV.md)).

- Keep the Win32 client's path tracer compiling. The 32-bit NVIDIA driver
  compiles the compute backend's trace inside the client's 2 GB address
  space, and the trace had grown to the edge of what it can compile there:
  a few more instructions ended its pipeline creation with "LLVM ERROR: out
  of memory", which aborted the client. Direct shading now samples the
  lights from one call site, so the sampler is compiled once instead of
  twice, and the inlined trace is about 15% smaller at the same speed.
- Path trace by default with the RTX renderer: `cl_renderer rtx` now starts
  in `rtx_rt_mode 2`, the path tracer, instead of its raster path, which
  `rtx_rt_mode 0` keeps. A GPU that can trace neither way still falls back
  to the raster frame (`rtx_rt_require 0`). A configuration saved before
  can hold `seta rtx_rt_mode "0"`, which keeps raster until changed.
- Show the path tracer's light alone: `rtx_rt_debug_visualizer 10` draws
  what the view's light does to plain grey surfaces, through the same
  camera, so its shadows, light pools and bounce light are easy to see.
- Trace Quake Live's advertisements. The renderer draws a map's
  advertisement brush models as world surfaces, which the path tracer left
  out, so a traced view showed the wall behind each billboard instead of its
  poster; they are now traced in place, lit by the scene, their shaders'
  environment-mapped sheen given to traced reflections.
- Trace Enemy Territory's terrain with its own normals. The renderer smooths
  a face whose points share no plane into four floats a point, and the path
  tracer read them three apart, so on `goldrush` the blended ground traced as
  flat grey facets lit as if they stood on edge.
- Say "Wrote" only for a screenshot written. The RTX renderer printed
  `Wrote <file>` after a frame it could not read back, which it had already
  reported unwritten.
- Compile the path tracer's pipelines when the RTX renderer starts, before a
  level's images and world load. On a 32-bit Windows client the driver
  compiles the compute backend's trace in the client's own address space,
  and after Quake Live's `campgrounds` had loaded the compile failed about
  half the time, leaving the view to raster (or failing `rtx_rt_require 1`).
- Play Enemy Territory demos. The `wolfet` profile now opens `.dm_84`
  demos, as `et.exe` names them, and reads them with ET's own wire; no other
  profile opens one. fnTech3 plays a demo retail `et.exe` 2.60b recorded and
  `et.exe` plays one fnTech3 recorded, each to the same number of timedemo
  frames as the other engine and with the same view at every sampled frame;
  the retail client game draws its demo controls in both.
- Keep a 32-bit Windows client's native game module reloadable across a
  renderer restart. The engine now holds the address range an unloaded
  module occupied until that module's slot loads again. Enemy Territory's
  57 MB client game module used to fail to reload ("VM_Create on cgame
  failed") after a traced `vid_restart` on its larger maps, because the
  restart's allocations and the UI module split the only hole left large
  enough; it now loads back at its preferred base every time.
- Name the demo that `demo` could not open. Given a name without an
  extension, a profile that plays no demos printed `couldn't open` followed
  by an uninitialised buffer. It now says that the profile's demo playback
  is not implemented and names `demos/<name>`. Other
  profiles still name the last file they looked for, such as Quake III's
  `.dm3` fallback.
- Render Elite Force single player's sounds with the engine's own audio
  pipeline instead of retail's gain and pan. Each channel's retail distances
  -- full volume to 256 units and silent by 1,506, a voice's 1,024 to 2,274,
  an attenuated voice's 38.4 to 288.4 -- go to the OpenAL distance model with
  its fade toward the silent end and its air absorption, and every voice gets
  traced occlusion, zone or traced room reverb, tone classes by channel and
  HRTF. The ambient sets' loops are placed at their own points instead of
  panned head-relative, and the room tone plays at the listener, in the
  room. `s_separation` above its default spreads placed voices through
  OpenAL Soft's source radius, `s_alDistanceModel linear_clamped` plays
  retail's straight-line law exactly, and the legacy mixer keeps retail's
  gain and pan. A voice started beyond Quake III's 1,530-unit cull distance
  but within its own range now plays, and `s_alDebugDump` prints each
  voice's dry and wet gains
  ([record](docs/fntech3/2026-09-20-stvef-sp-sound-distances.md#the-engines-own-features)).
- Render Return to Castle Wolfenstein's and Enemy Territory's sounds with the
  engine's own audio pipeline instead of retail's gain and pan.
  - Retail's law goes to the OpenAL distance model, with its fade toward the
    silent end and its air absorption. A sound is at full volume to 6.4% of
    its range and silent the range beyond it: 80 to 1,330 units for every
    one-shot.
  - Every voice gets traced occlusion, tone classes by channel, HRTF, and
    reverb from the room its map's audio zones or the traced room give. The
    47 RTCW and 6 ET zone sidecars had loaded but were never heard.
  - Retail's slot allocation, loop levels and phase, single player's effects
    fade and RTCW's disabled loop Doppler are kept.
  - `s_alDistanceModel linear_clamped` plays retail's straight-line law
    exactly, and the legacy mixer keeps retail's gain and pan
    ([record](docs/fntech3/2026-09-24-wolfenstein-openal-voices.md)).
- Give RTCW single player and multiplayer the engine's own scene lights: a
  map's static light file (`maps/<map>.lights.json`), surface-light proxies
  and `r_dlightTest`, which every other profile already had. RTCW's modules
  keep the 32 lights a frame retail accepts: the engine's lights take only
  slots those modules can no longer claim -- RTCW draws its sky portal as a
  scene of its own before the main view submits any light -- and a portal
  view's copies no longer count against them. The engine's lights are drawn
  beneath the modules' own, so RTCW's darkening light still darkens them as
  it darkens the lightmap. They are added only in the per-pixel modes
  (`r_dlightMode 1` and `2`): mode 0 stays the original projected lights, and
  `r_dynamiclight 0` or `2`, which keep only forced module lights, and the
  snooper scope's heat view add none
  ([record](docs/fntech3/2026-09-24-rtcw-engine-lights.md)).
- Keep Enemy Territory's 32 module lights a frame free of the engine's own
  scene lights, as RTCW's are: static map lights, surface-light proxies and
  `r_dlightTest` take only slots ET's cgame and UI can no longer claim, so a
  sky portal's or a portal view's engine lights no longer refuse the main
  view's own. ET's forced lights, such as a thrown charge's glow, now stay
  drawn at `r_dynamiclight 0`, as in ET, where every light used to be
  cleared, and the engine adds none of its own there. Directed and
  grid-only lights, still not drawn, take one of the 32 as ET's own do
  ([record](docs/fntech3/2026-09-24-wolfet-engine-lights.md)).
- Ship a static light file for each of RTCW's 32 official single-player and
  15 multiplayer maps, 4,316 lights converted from the maps' own `light`
  entities under the light compiler's conventions. Each light's radius and
  brightness are fitted to the light retail baked into the map, for its
  mode's `r_dlightScale`; grid-only, non-dynamic, negative lights and those
  at a fixture the cgame already lights are left out, and 128 are kept a map
  across its areas. The package validator now also refuses a packaged light
  file that a renderer would reject or cut short
  ([record](docs/fntech3/2026-09-24-rtcw-map-lights.md)).
- Draw American McGee's Alice with no overbright bits by default, as
  `alice.exe` does: it registers `r_overBrightBits` and
  `r_mapOverBrightBits` as `0` and draws each map's light as the map holds
  it. Under this engine's usual one bit over two, `garden1`, `utemple` and
  `centipede1` drew two to nearly three times as bright, and Alice herself
  about three times. A lightmap stage that names no colour now takes
  retail's identity; every stage Alice ships already did. Characters and
  weapons take retail's compiled lights (next entry). Other profiles are
  unchanged ([record](docs/fntech3/2026-09-25-alice-overbright.md)).
- Light Alice's characters and weapons from the map's compiled lights, as
  `alice.exe` does. Its game module flags every sentient and weapon
  `RF_EXTRALIGHT`, and retail lights such a model from the map's own light
  list rather than the light grid: the lights each leaf around it lists and
  its view can see, each falling off with the square of its distance.
  fnTech3 now reads the list from the map and that law from the executable,
  and fits the light to the ambient and directed light all three renderers
  shade models with; dynamic and static lights, shadows and bloom stay the
  engine's own. Alice's dress is 1.7 to 5.9 times as bright on the opening
  views, and on `utemple`, where the grid had drawn her black, she is lit.
  `r_ritualModelLights 0` lights every model from the grid again, and the
  same code reads F.A.K.K.2's maps with `fakk2.exe`'s own constants. Other
  profiles are unchanged
  ([record](docs/fntech3/2026-09-25-ritual-model-lights.md)).
- Let an Enemy Territory dedicated server make bots, as `etded.exe` does.
  `spawnbot` with `bot_enable 1` failed in the bot library, which looked
  below `botfiles` for the `botfiles/chars.h` that ET's character includes,
  and the retail game then faulted printing its own failure. ET opens a
  character below `botfiles` but reads its includes from the game root, and
  the `wolfet` profile now does too. ET's bot library also sets up Quake
  III's weapon, item, chat and movement configuration, which fnTech3
  skipped, so no bot's item weights could load. Its engine registers
  `bot_thinktime` as 50 rather than Quake III's 100, at which the retail
  game let bots think with no navigation and acknowledge their commands. A
  goldrush bot now joins, stays out of broadcasts and is dropped once
  addressed commands fill its window, as on `etded.exe`, though after 64
  commands rather than 256. Quake III, Quake Live, RTCW and Elite Force bots
  load as before ([record](docs/fntech3/2026-09-24-wolfet-bot-characters.md)).
- Announce a client's drop as RTCW multiplayer and Enemy Territory do.
  `WolfMP.exe` broadcasts `print "[lof]%s^7 [lon]%s\n"`, so RTCW MP's client
  game translates the reason and leaves the name as it is. fnTech3 sent
  Quake III's `print "%s^7 %s\n"`, whose reason a client in another language
  could not translate. ET 2.60b pops the notice up with `cpm "%s^7 %s\n"`
  instead, and announces no bot leaving a single-player game type. A retail
  observer on a fnTech3 server now receives both in retail's form. The
  `disconnect` command sent to the client that left, and the notice in
  every other profile, are unchanged. The record lists the other drop-path
  differences found in the retail engines
  ([record](docs/fntech3/2026-09-24-rtcw-et-drop-notices.md)).
- Send a client that is dropped, and every client of a server that quits, a
  last snapshot its own engine can read. That snapshot carries no changes,
  and brings the drop notice, the `disconnect` command and a quitting
  server's last words. fnTech3 wrote Quake III's empty player state into it
  in every profile but Elite Force Holomatch, and RTCW's and ET's readers
  take five more bits. Retail `WolfMP.exe` and `et.exe` clients therefore
  failed with `CL_ParsePacketEntities: end of message`: the client that was
  kicked never learned why, and every client failed when the server quit.
  Each profile now gets its own empty player state, and retail RTCW MP and
  ET clients run those commands. Quake III and Quake Live clients get the
  same bytes as before. RTCW and Elite Force single player, which only
  fnTech3's client reads, get their own forms too
  ([record](docs/fntech3/2026-09-24-zombie-snapshots.md)).
- Leave Enemy Territory bots out of broadcast server commands, as `et.exe`
  and `etded.exe` do: retail passes over any client whose entity carries
  `SVF_BOT`, and fnTech3 queued every broadcast for a bot too. Only the
  game's bot AI acknowledges a bot's commands, and on a stock dedicated
  server it never thinks, so the broadcasts only filled the bot's window. A
  bot in a fresh slot would have been dropped with "Server command overflow"
  on the 65th. Commands addressed to a bot, RTCW's cast-AI rule and every
  other profile are unchanged. No ET bot survives `spawnbot` under fnTech3
  yet: the bot library cannot find the `botfiles/chars.h` that ET's character
  files include, and the retail game then faults. The change is therefore
  shown by unit tests and by a retail `etded.exe` run
  ([record](docs/fntech3/2026-09-24-wolfet-bot-broadcasts.md)).
- Give every bot its profile's reliable-command window, as each retail engine
  gives every client one fixed window. A bot sets up no channel, so fnTech3
  took its window from whatever wire profile its slot last held: Quake III's
  64 in a slot no player had used. There an RTCW or ET bot that nothing
  acknowledged was dropped with "Server command overflow" on its 65th
  command, where `WolfSP.exe`, `WolfMP.exe`, `et.exe` and `etded.exe` allow 256,
  and a Jedi Academy or Jedi Outcast multiplayer bot on its 65th, where
  `jamp.exe` and `jk2mp.exe` allow 128. Now RTCW and ET bots hold 256 in every
  slot and Jedi bots 128; an ET bot on a fnTech3 server was dropped on its
  257th, as retail's is. A bot that takes a slot from a player of another
  window no longer reads that player's unacknowledged commands from the wrong
  indices: they are dropped. Quake III, Quake Live and Elite Force bots keep
  64, network clients are unchanged, and the IHV test, which rings no
  commands, seats no bot in a client slot
  ([record](docs/fntech3/2026-09-24-bot-reliable-windows.md)).
- Settle Enemy Territory's restarted level as `et.exe` and `etded.exe` do. An
  immediate `map_restart` now runs six frames before it brings the clients
  back, not three. Each runs at the current time before the time advances,
  and so does its last frame. The game's first frame now runs at the time its
  restart began, and the level ends 700 ms on, not 400. On goldrush the retail
  game's map script timeline for the first 2.5 s after a restart now matches
  retail `etded.exe`'s line for line. A single-player or co-op restart also
  leaves bots out, as retail does. Retail's entry of clients that are still
  connecting or loading at the restart is not reproduced, and they still
  enter on their first command. Quake III, Quake Live, both RTCW modes and
  the other profiles are unchanged
  ([record](docs/fntech3/2026-09-24-wolfet-restart-settle.md)).
- Settle Enemy Territory's newly loaded level as `et.exe` and `etded.exe` do.
  A map load now runs six frames, with its bots, before it brings the clients
  back, not three. Each runs at the current time before the time advances,
  and so does its last frame. The game's first frame now runs at the time the
  level started, and a load leaves the level 700 ms on, not 400. On goldrush
  the retail game's map script timeline for the first 2.5 s after a load now
  matches retail `etded.exe`'s line for line. A load also drops the bots of a
  single-player or co-op game type, and gives every bot it drops an empty
  reason, as retail does, where fnTech3 gave Quake3e's `was kicked`. The
  retail game refuses both game types, so that part rests on the disassembly
  and unit tests. Every other profile keeps its bots and its reason, and this
  change leaves its frames as they were
  ([record](docs/fntech3/2026-09-24-wolfet-map-load-settle.md)).
- Settle every other profile's level in the order its retail engine uses, as
  Enemy Territory's now is. A map load and an immediate `map_restart` run each
  settling frame, and the last one, at the current time and advance the time
  afterwards, not before as in Quake3e. So the game's first frame runs at the
  time the level started, and a restart begins its players 200 ms after that,
  not 300. The level still ends 400 ms on. Retail Quake III 1.32, Quake Live,
  both RTCW modes, both Elite Force modes and both Jedi titles were read, and
  every one runs its frames this way; the IHV test already did. On `mp_rocket`
  and single player's `rocket`, the retail game's map script timeline for the
  first 2.5 s after a load or restart now matches retail `WolfMP.exe`'s and
  `WolfSP.exe`'s line for line, and an RTCW SP save's restored time is now the
  time of its first frame, as in retail. The other titles' games print nothing
  a live run could compare, so they rest on the disassembly and unit tests.
  Frame counts, clients and bots are unchanged
  ([record](docs/fntech3/2026-09-24-settle-frame-order.md)).
- Let a running server change `sv_maxclients` without crashing. The next
  level spawn stopped with `STACK_OVERFLOW`, in code every profile shares:
  clearing a client slot with `client = {}` made MSVC build a whole
  `client_t`, over 500 KB, on the stack, and `SV_ChangeMaxClients` did so
  while calling `SV_AllocClients`, which did so again, past the 1 MB
  main-thread stack.
  Slots are now cleared in place, including a connecting client's, whose
  frame in an unoptimized build was larger than the whole stack. Quake III,
  RTCW multiplayer and Enemy Territory dedicated servers now spawn the level
  again with the new count, and retail `WolfMP.exe` and `et.exe` clients stay
  connected across the `map_restart` that applies it
  ([record](docs/fntech3/2026-09-24-client-slot-stack-overflow.md)).
- Darken stencil shadows (`cg_shadows 2`) by the same 0.6 in the Vulkan and
  RTX renderers as in GL, in every profile. The Vulkan finish pass drew with
  the last surface's settings, so under RTCW's global fog it multiplied by a
  fogged colour. On `escape1` shadowed pixels came out at 0.32 of their
  unshadowed colour instead of GL's 0.60, and tinted. Both now measure 0.60
  on the same scene, and GL is unchanged.

- Let Enemy Territory's `map_restart` run while a delayed restart is pending,
  as `et.exe` and `etded.exe` do. fnTech3 refused every `map_restart` until a
  pending delay fired, as Quake III and `WolfMP.exe` do. Retail ET replaces
  the pending delay with a new one, earlier or later, and publishes the new
  time in `CS_WARMUP`. Without a delay it restarts at once, and the pending
  restart still fires when it falls due. On `goldrush` one console sequence
  now makes the same restarts, and leaves the same match state, as on a retail
  `etded.exe`, where the previous build refused every restart once a one-hour
  delay was pending. Quake III, Quake Live, both RTCW modes and the other
  profiles still refuse
  ([record](docs/fntech3/2026-09-24-wolfet-pending-map-restart.md)).
- Echo broadcast messages on Return to Castle Wolfenstein multiplayer and
  Enemy Territory dedicated servers without their `[lon]` and `[lof]`
  localization tokens, as `WolfMP.exe` and ET 2.60b do. The games use them to
  tell a client which parts of a message to translate, and fnTech3's console
  printed them raw: `broadcast: print "Server:[lof] timelimit [lon]changed to[lof] 10\n"`
  where retail logs `broadcast: print "Server: timelimit changed to 10\n"`.
  Only the console line changes. Clients still receive the tokens, and Quake
  III, Quake Live, RTCW single player and the other profiles are unchanged
  ([record](docs/fntech3/2026-09-24-rtcw-et-broadcast-echo.md)).
- Trace a box entity in Return to Castle Wolfenstein multiplayer and Enemy
  Territory as what it is. For each sweep of an entity that is not a brush
  model, `WolfMP.exe` and `et.exe` give the temporary box the entity's own
  contents, then put `CONTENTS_BODY` back. fnTech3 tested and reported every
  such box as `CONTENTS_BODY`. A trace mask now reaches a corpse, a trigger
  or a solid box through its own contents, even without `CONTENTS_BODY`, and
  the trace reports those contents, such as a corpse's `CONTENTS_CORPSE`. In
  the retail games this is what stops a dead player at a solid box entity.
  Capsules, brush models, single-entity traces, point contents and the
  client's own traces keep `CONTENTS_BODY`, as retail does. Quake III, Quake
  Live, RTCW single player and the other profiles are unchanged
  ([record](docs/fntech3/2026-09-23-entity-box-contents.md)).
- Keep Return to Castle Wolfenstein multiplayer's `map_restart` to a game type
  the map allows, as `WolfMP.exe` does. A map can forbid objective, stopwatch
  or checkpoint play, and the game copies those flags into the world's entity
  record. Retail reads them before any delay and replaces a forbidden
  `g_gametype`, including one just typed at the console, with objective, or
  with checkpoint when the map forbids objective too. fnTech3 restarted in the
  forbidden type, and the game then broadcast `Invalid gametype was specified,
  Restarting` and restarted again ten seconds later. On `mp_destruction` and
  `mp_beach` the same console sequence now leaves the same game type as retail
  at every step. Enemy Territory's `map_restart` has no such check and is
  unchanged ([record](docs/fntech3/2026-09-23-rtcw-mp-restart-game-type.md)).
- Restart a Return to Castle Wolfenstein multiplayer or Enemy Territory level
  in place after a new game type or pure mode, as `WolfMP.exe` and ET 2.60b
  do. Their `map_restart` reloads the level only when `sv_maxclients` has
  changed, and prints `sv_maxclients variable change -- restarting.`. fnTech3
  also reloaded it after a `g_gametype` or `sv_pure` change, printing
  `variable change -- restarting.`, so a connected retail client loaded a new
  gamestate. The retail servers instead send it a `map_restart` command and
  the new game type in a serverinfo update, and fnTech3 now does the same. A
  new `sv_pure` now waits for the next level load in these two games. The
  retail servers also ask connected clients for a new pak proof as soon as
  pure mode comes on, which fnTech3 still does not. Quake III, Quake Live,
  RTCW single player, Elite Force and the other profiles keep their own check
  ([record](docs/fntech3/2026-09-24-wolfenstein-restart-variable-change.md)).
- Lay the logo that retail Return to Castle Wolfenstein single player and
  Enemy Territory lay over their console. Once the console is half open,
  `WolfSP.exe` draws `console2` over its `console` background, and `et.exe`
  draws `console2-16bit` over `console-16bit`, where `WolfMP.exe` draws its
  own logo. This client drew only RTCW multiplayer's. Single player and Enemy
  Territory now register theirs, and the textured style,
  `con_backgroundStyle 0`, draws them as it draws multiplayer's, keeping the
  logo's shape on any screen
  ([record](docs/fntech3/2026-09-23-wolfenstein-console-logo.md)).
- Keep Return to Castle Wolfenstein multiplayer and Enemy Territory running
  when thrown dynamite comes to rest. The game makes the world, entity 1022,
  the owner of a charge that stops moving, and the charge's next trace passes
  that owner as the entity to ignore. The server then reads the world's own
  record, which lies past the entity count the game publishes, and fnTech3
  dropped the server with `SV_ClipMoveToEntities: invalid pass entity 1022`.
  ET's landmines, satchels, tripmines and smoke do the same. Both games keep a
  fixed 1024-entity arena, as RTCW single player's does, and the server now
  reads the whole arena for all three while snapshots still stop at the
  published count. A probe that throws one charge on `mp_beach` and on
  `oasis` reproduces the crash on the previous build. With this change the
  charge comes to rest on both listen and dedicated servers
  ([record](docs/fntech3/2026-09-23-world-owned-explosives.md)).
- Draw each profile's notify lines and chat line as its own engine does. The
  console drew Quake Live's chat overlay for every profile: a strip 413 lines
  down, Quake Live's prompt, a 73-character field, and no recent console
  lines. Every profile but Quake Live now shows its latest lines at the top of
  the screen for `con_notifytime` seconds, registered with its engine's
  default: 3, or 7 in RTCW multiplayer and Enemy Territory, or 5 in Elite
  Force Holomatch. Its engine's chat line follows under them: Quake III's big
  `say:` and `say_team:`, the translated prompts of RTCW multiplayer and Enemy
  Territory, Enemy Territory's fireteam prompt, or Holomatch's small one.
  Menus hide them only in the engines whose menus did, RTCW multiplayer's
  limbo menu hides both, and Enemy Territory draws them only with the cheat
  `con_drawnotify` set. Each profile gets its engine's chat commands and
  field widths, so Enemy Territory has `clMessageMode` to `clMessageMode3`,
  and Elite Force single player's `messagemode` opens nothing, as in
  `stvoy.exe`. As in the retail engines, three lines show after a line feed,
  with an empty row under them, and `[skipnotify]` no longer hides the line
  before it. Quake Live's overlay is unchanged
  ([record](docs/fntech3/2026-09-23-profile-notify-lines.md)).
- Give Return to Castle Wolfenstein multiplayer's console the art retail
  `WolfMP.exe` uses. Retail writes console text in the HUD font,
  `gfx/2d/hudchars`, over `console-16bit` smoke. Once the console is half
  open it also lays a `console2-16bit` logo over the background. This client
  used Quake III's `gfx/2d/bigchars` and `console`, and the `console` shaders
  RTCW MP can see name images it does not ship. The logo keeps its shape on
  any screen and belongs to the textured style, `con_backgroundStyle 0`. RTCW
  MP's cgame registers `gfx/2d/hudchars` too, so the two share one shader. A
  HUD aspect rule now matches a shared shader by either the client's name,
  `charset`, or the cgame's, and the shipped `round_timer` rule keeps its
  place ([record](docs/fntech3/2026-09-23-rtcw-mp-console.md)).
- Draw Return to Castle Wolfenstein's and Enemy Territory's water ripples
  instead of ending the session. A bullet or explosion landing in water makes
  a ripple entity. The client already translated it, but none of the three
  renderers accepted the type, so on `mp_beach` the session stopped with
  `R_AddEntitySurfaces: Bad reType`. Retail `WolfMP.exe`, `WolfSP.exe` and
  ET 2.60b's `et.exe` accept it and the tapered rail core as generated
  surfaces. They draw the ripple as a quad lying flat on the water, mirrored
  in mirrors, and the tapered core as the debug axis. All three renderers now
  do the same. A live probe that fires a Thompson into `mp_beach`'s surf
  reproduces the old crash on the previous build and draws the ripples in
  all three renderers now
  ([record](docs/fntech3/2026-09-23-rtcw-water-ripples.md)).
- Load Quake Live's host text fonts under Quake Live only. Every other
  profile tried to read QL's four TTF faces at renderer start and warned
  about each missing one. On Windows it then drew its console and large
  client text in `segoeui.ttf`, the system face QL uses as a fallback. The
  console and that text now use the profile's own bitmap charset, as the
  original engines do and as non-Windows builds already did. Quake Live is
  unchanged.
- Skip a screenshot or video frame the RTX renderer has no GPU memory left
  to read back, with a warning, instead of ending the game. Another program
  holding the device's memory made the readback's allocation fail, which was
  fatal.
- Keep the RTX renderer display-referred at its default `r_hdr 1`, as GLx and
  Vulkan are, in every profile. RTX treated `r_hdr 1` alone as scene-linear
  colour, so it decoded every texture from sRGB and blended in linear light.
  Translucent effects and the edges of 2D text drew brighter than in the other
  renderers: RTCW's water spray averaged 3.5-4.8 levels per pixel from GLx at
  the same demo frame, against Vulkan's 0.8. Scene-linear colour now needs
  `r_srgbTextures` and a non-legacy tone mapper, colour grading or an HDR
  display, as `docs/DISPLAY.md` describes and GLx implements. RTX now matches
  Vulkan's difference from GLx. The fog and underwater tints follow the same
  choice.
- Size each profile's console on its own character cell. Every profile used
  Quake Live's retail cell, 12x24 at half size on a 768-row reference, which
  is 6x11 at 1280x720. Quake III, RTCW, Enemy Territory and Elite Force now
  use their original 8x16 small character, scaled with the 640x480 virtual
  screen as FnQuake3 does: 12x24 at 1280x720, or native 8x16 with
  `con_scaleUniform 0`. `con_scale` still multiplies either cell. Quake Live
  is unchanged.
- Name a new player as each Wolfenstein engine does. RTCW multiplayer, RTCW
  single player and Enemy Territory registered Quake III's `UnnamedPlayer`,
  so a fresh home joined an RTCW MP game under a name retail never uses. They
  now register `WolfPlayer`, `Player` and `ETPlayer`, the names `WolfMP.exe`,
  `WolfSP.exe` and `et.exe` register. An archived `name` is unaffected.
- Show Enemy Territory's demo-recording notice. ET's cgame draws it itself,
  from three read-only cvars the engine keeps: `cl_demorecording` and
  `cl_demofilename`, set as a recording starts and stops, and
  `cl_demooffset`, the size written so far, set every frame. Retail `et.exe`
  registers and sets all three. This engine had none of them, so ET showed
  no notice while recording. The client now registers them under Enemy
  Territory alone and sets them where `et.exe` does, with the name cut to
  the 63 characters `et.exe` keeps. No other profile gains a cvar. In a
  developer client, the retail cgame now draws `RECORDING demo probe: 9k`
  while recording and nothing once recording stops
  ([record](docs/fntech3/2026-09-23-wolfet-demo-recording-cvars.md)).
- Draw the client's large text in every profile but Quake Live as the
  original engines draw it. The demo-recording notice, the spatial-audio
  overlay and big edit fields went through Quake Live's host text. Outside
  Quake Live that meant a Windows system face, or a stand-in of narrow glyphs
  above a baseline, with no shadow, and the recording notice sat 420 lines
  down. They now use Quake III's large string: square cells of the profile's
  own charset, from the top-left, over a black shadow two pixels right and
  down. Elite Force's half-width glyphs are included, and a big field's
  cursor is the charset's own. The recording notice goes where each retail
  engine puts it. That is centred at the top in Quake III, RTCW single player
  and Elite Force Holomatch, and in the bottom-left corner in RTCW
  multiplayer. Enemy Territory's engine draws none, since its cgame draws its
  own, and Elite Force single player cannot record. Quake Live is unchanged
  ([record](docs/fntech3/2026-09-23-profile-large-text.md)).
- Measure what a Ghoul2 compressed bone pose holds. A `.gla` skeleton keeps
  its animation as a pool of fixed-size poses that the frames index into, and
  this engine has bounds-checked that pool without decoding it. The pool
  stride is fourteen bytes: for each of eighteen shipped skeletons across both
  Jedi titles, the highest index any frame reaches, plus one, times fourteen,
  equals the pool exactly, with nothing left over. Seven divides those pools
  too, and would leave the upper half of every one of them unreachable.
  Frame indices are three bytes each, the block rounded up to a multiple of
  four.

  Of the fourteen bytes, the first four of seven sixteen-bit words are a
  rotation. Biased about `0x8000` and scaled by `1/16384` their length is one:
  the mean raw length is 16,380.97 across Academy's 599,570 distinct poses and
  16,382.40 across Outcast's 445,610, and across the whole of both pools it
  stays between 0.998 and 1.003. No other four-word window is within 15% of
  constant.

  The last three words are how far the bone has moved from where it was
  modelled, in the model's own space, at a scale of `1/64`. That they are a
  deviation and not a position is settled by a model that does not move:
  Academy's `turret_cannon.gla` has four bones, three identical frames and real
  geometry -- one bone 6.68 units from its parent, another 28.86 from its --
  and every translation in it is zero, which no position could be.

  Three independent measurements give the scale. Across all 87 skeletons of
  both titles `model_root` deviates on the z axis alone and takes seven values,
  every non-zero one an exact multiple of 64, reading at `1/64` as a whole-unit
  drop to the ground of 12, 19, 24, 64, 120 or 154 units; six values landing on
  a multiple of 64 by chance is about one in 10^11. At `1/64` every bone's
  travel is what that bone really does -- the face bones, which never animate
  in multiplayer, are exactly zero; a head moves ten units, a hand about fifty.
  And sweeping the scale with the composition rule held fixed puts a sharp
  error floor at `1/64`: 0.076 units, against 26 at `1/32`, 6.5 at `1/128` and
  19 with the displacement dropped.

  A pose composes as `W = D . base` -- the decoded transform applied to the
  bone's own rest matrix, **with no walk up the hierarchy at all**, because the
  pose already carries where the bone ended up. Six candidate rules were tried
  against the fact that bones do not stretch, and this one holds Academy's
  humanoid to a median bone-length error of 0.034 units, about 0.38% of a bone,
  over 16,968 measurements; every other candidate is wrong by 99% or more. So
  the reader gained `PoseMatrix`, `ComposeMatrix`, `BoneRestMatrix`, `BonePose`
  and `BoneMatrix`, the last answering where a named bone is in a given frame
  ([record](docs/fntech3/2026-09-22-jka-pose.md)).

  Read the same constants out of the retail engine afterwards, which settled
  them exactly and **found an error in the above**. Its decompressor divides
  then subtracts: a rotation component is `word / 16383 - 2` and a displacement
  is `word / 64 - 512`. The displacement matched. The rotation did not -- the
  mid-point is 32,766 and the divisor 16,383, not 32,768 and 16,384 -- and the
  data had been saying so, since a unit quaternion measured at 0.9998 rather
  than 1 and that residual was written off as animation noise. With the
  engine's constants it measures 1.0000 and the rigidity error falls to 0.0749
  units. The two halves of a pose really do disagree about where zero is; that
  is Raven's and is now stated rather than tidied away. Reading the engine also
  confirmed positively what had only been inferred, including the `and ecx,
  0xffffff` / `imul ecx, ecx, 0xe` pair that is the three-byte index and the
  fourteen-byte stride in one place, and every mesh constant the skinning path
  uses.

- Measure when a Ghoul2 bone is, which had been called unmeasurable. It is not
  in the files, but it is in the engine: `G2_TimingModel` gives
  `frame = startFrame + ((now - startTime) / 50) * animSpeed`, clamped so time
  never runs backwards, with fifty milliseconds to a frame and so twenty frames
  a second at a speed of one. It splits that into a current frame, the frame
  after it and the fraction between, loops on flag bit `0x10` and otherwise
  holds the last frame. The server-side store gained `PlayheadAt`, with tests
  covering the rate, the blend weight, both ends of the range, looping, a
  range the file cannot hold and a backwards one.

- **Serve `G_G2_GETBOLT`**, and with it `G_G2_GETBOLT_NOREC` and
  `G_G2_GETBOLT_NOREC_NOROT`, which asks where a named place on a model is. It
  was the last import a Jedi Academy bot match reached and the only one it was
  refused, and it is answered now.

  A bolt that names a bone is that bone, posed for the moment asked about. A
  bolt that names a surface is a tag -- three corners carried by the bones they
  are weighted to -- and the bolts a game actually asks for are these: a sabre
  hangs off `*r_hand`, which is surface 60 of an Academy player mesh and not a
  bone at all. Each corner is skinned the ordinary way, first undoing where its
  bone rests and then applying where it is now, because a vertex is modelled in
  the whole model's space rather than relative to the bone that carries it.
  Getting that wrong is invisible by eye and was got wrong first here: against
  a retail model it put the right hand's tag 26.7 units from the hand. With the
  undo in place it lands 0.644 units from `rhang_tag_bone`, which is where a
  tag on a hand belongs. The three corners then give an origin and a set of
  axes, written as columns because that is how the retail engine writes them,
  and the result is turned by the entity's angles, moved to its position and
  scaled where a scale was given.

  A bolt this engine cannot answer writes nothing and reports failure, which is
  what the game checks; handing back an identity matrix would put the thing
  somewhere definite and wrong. The three call numbers are served alike,
  because retail separates them only by how much of a skeleton it is willing to
  rebuild on the way, and this engine rebuilds nothing.

  **A Jedi Academy bot now plays a retail multiplayer level with no import
  refused anywhere.** A dedicated server on `mp/ffa1` has it connect, be
  announced, begin, and spend its time taking things -- `weapon_disruptor`,
  `weapon_thermal` -- with a sabre loaded and the bolt branch live. The
  installation was read and never written, and the run used an isolated
  `fs_homepath`.

- Add American McGee's Alice as an eighth title and the `alice` profile,
  gated behind `-Dalice-development=true`. Alice is the first supported game
  outside the Quake III line: it runs Ritual Entertainment's Heavy Metal:
  F.A.K.K.2 engine, speaks protocol 1, writes `config.cfg`, records demos as
  a fixed `.dm3`, loads `base/fgamex86.dll` and `base/cgamex86.dll` through
  `GetGameAPI` and `GetCGameAPI` rather than Quake III's `vmMain`, and has no
  menu module, no bot library and no `fs_homepath`. Every constant is read
  out of the retail binary rather than inferred
  ([record](docs/fntech3/2026-09-22-alice-contracts.md)).
- Read Alice's maps. All thirty-six retail maps are ident `FAKK` version 42,
  with a header checksum Quake III's does not have and twenty lumps in
  F.A.K.K.2's own order; eleven of the thirteen fixed-size records are
  Quake III's byte for byte, and only shaders and surfaces differ, each by
  one trailing subdivision field. A new translator presents such a map to the
  existing collision loader, and a developer dedicated server loads all
  thirty-six maps' collision against a legitimate installation before
  refusing the module boundary by name. The layout was re-measured against
  every retail map rather than taken from Ritual's SDK alone, and the
  translation deliberately drops the light grid, the entity lights and the
  light definitions, so it is collision evidence only
  ([record](docs/fntech3/2026-09-22-alice-maps.md)).
- Measure Alice's module boundary. Neither retail library speaks Quake III's
  ABI: `fgamex86.dll` exports `GetGameAPI` and takes 111 import slots,
  answering with 26 export slots whose first is API version 4 and whose
  twenty-third is a 732-byte entity record; `cgamex86.dll` exports
  `GetCGameAPI`, which takes no argument at all and answers with 7 export
  slots, the first of which receives 125 import slots and immediately calls
  the last of them. Both also export Ritual's `str` class across the library
  boundary. `alice.exe`'s own table build confirms the 111 from the other
  side and divides it into 108 code pointers, two data pointers and one
  null, so the table is not the pure function table an adapter would
  assume. The four lengths are frozen with the two constants the game
  library writes for itself; at the time of that entry no import was served
  ([record](docs/fntech3/2026-09-22-alice-module-abi.md)).
- Read Alice's `TAN ` models. All 694 of the installation's vertex-animation
  models are ident `TAN ` version 2, and a bounded reader parses every one:
  8,782 frames, 1,059 surfaces, 91,016 vertices, 110,875 triangles and 457
  tags, with each surface's blocks held inside the surface that declares
  them and the surface chain tiling the file exactly. Running that check
  corrected the format: the surface's `minLod` is how many vertices survive
  at the lowest detail, not a level number, and bounding it by the
  reference's four levels refused 624 of the 694 files.
- Read Alice's `SKAN` skeletal animations, completing this title's model
  pipeline. These files declare no length and carry no offsets, so what
  admits their layout is that one fixed stride per version reproduces every
  file's size to the byte: a 112-byte header with 28-byte poses at version
  2, and a 108-byte header with 16-byte poses at version 3, which compressed
  the pose and changed nothing else. All 1,402 read, 43,640 frames and
  3,254,001 poses, and 3,253,939 of those poses begin with a unit
  quaternion, which corroborates the stride independently
  ([record](docs/fntech3/2026-09-22-alice-models.md)).
- Read Alice's `.tik` model definitions, the index that maps every surface
  to a shader and every animation name to its file. All 801 parse: 1,089
  surface mappings, 3,447 animation entries and 109 macros. The reader
  carries what the shipped assets actually contain -- a `#define` preamble
  before the magic, `$define` macros at file scope, one definition that
  never closes its last block, and references to animations the
  installation does not ship -- because a reader that refuses any of those
  refuses assets the game ships. No command is interpreted
  ([record](docs/fntech3/2026-09-22-alice-models.md)).
- Read Alice's `SKL ` skeletons. The format has no published declaration, so
  every field was derived from the 79 installed `.skb` files and admitted
  only because all 79 agree: a 92-byte header, 72-byte bones whose parent is
  always an earlier bone, and surfaces whose vertex block has no stride and
  has to be walked, 24 bytes a vertex and 20 a weight. All 79 parse, giving
  3,910 bones with 165 roots and a deepest chain of 21, 278 surfaces, 46,965
  vertices and 59,145 weights, with every bone's walk to a root proved to
  terminate. Alice's 1,402 `SKAN` animations are not read, though four of
  their header fields are now established: the frame count, bone count,
  total time and frame time, fixed by the first equalling the product of
  the last two on every file and by 1,399 of them naming a bone count an
  installed skeleton has
  ([record](docs/fntech3/2026-09-22-alice-models.md)).
- Map every one of Alice's 111 game import slots. Its table is Ritual's
  F.A.K.K.2 `game_import_t` with four local edits -- one call inserted among
  the model calls, one member not taken, two further insertions left null,
  and four members appended -- and the count closes exactly. Each slot was
  fingerprinted in the engine binary and named only where that fingerprint
  identifies it: 102 slots carry a name, 86 of them from the target's own
  literals, constants or argument count, and the 16 named from position
  alone are marked as the weaker claim they are. Its 26 export slots are
  mapped as well, six of them data rather than code, and the three the
  module never writes are exactly `BotBegin`, `BotThink` and `PrepFrame` --
  which agrees, from the module's side, with the profile's finding that this
  engine has no bot library. No slot's signature is frozen and nothing is
  called. The retail library then confirmed the map in a running process: handed
  111 zeroed import slots, which its entry point copies without dereferencing,
  it publishes an export table at the predicted address carrying the predicted
  API version, record size, function-pointer count and untouched slots.
- Frame Alice's packets. Protocol 1 was a registered number with nothing
  behind it, so the client refused its own server's `connectResponse`. Retail
  `alice.exe` now supplies the framing: its `netchan_t` is Quake III's field
  for field and its fragmenting the same, the client writes a qport the server
  reads back and neither side checksums or XORs anything, every sequenced
  packet is scrambled from byte six, and `MSG_WriteBits` has no Huffman stage
  at all -- its `msg_t` is twenty-eight bytes, without the `oob` flag a
  Huffman path exists to bypass. The scramble turns out to be the same routine
  Elite Force Holomatch uses, step for step: same seed, same
  `x = x * 69069 + 1` key stream, same mask, same swap. The one textual
  difference is a `LittleLong` in the seed, which `Swap_Init` installs as
  `return arg` on a little-endian target. Its limits are its own and larger:
  48 KB messages against Quake III's 16, 128 reliable commands of 1,024 bytes,
  32 packet backups and 32 usercmds of 20 bytes each. The engine's
  cross-profile message storage grows to match, with the fit now asserted at
  compile time rather than left to a silent packet drop. Six of Quake III's
  out-of-band names have no counterpart and are recorded as absent rather than
  carried: there is no download path, because the client routes `svc_download`
  to the same arm as an unknown opcode, and no pure exchange, because the
  server answers exactly two client commands. The delta layout was declared
  here and served a few entries below; until it was, every codec entry point
  refused it rather than falling through to Quake III's field tables
  ([record](docs/fntech3/2026-09-22-alice-wire.md)). With the contract
  registered a loopback client completes the handshake: the server accepts
  it, sends the gamestate and moves it from `CS_CONNECTED` to `CS_PRIMED`.
- Read Alice's delta contracts. The engine's own field tables survive in the
  binary with their names intact, so walking them gives 79 entity fields and
  49 player fields with their offsets and bit widths, frozen by a generator
  rather than transcribed. Each table is internally consistent -- every offset
  four-aligned, none repeated, the last field bounding the record -- and the
  1,044-byte player state it implies sits inside the 2,112-byte client record
  the module publishes at run time, which is the first independent check
  either number has had. Under half of either table is Quake III's: 49 of the
  79 entity fields and 27 of the 49 player fields have no counterpart there.
  The additions are the engine's character -- five bone modifiers, an
  attachment that parents an entity to another's tag, a separate torso
  animation with crossblend times, per-entity alpha and scale; and on the
  player, music mood, reverb, camera and damage-angle state a Quake III client
  game would own. Nothing encodes or decodes a field
  ([record](docs/fntech3/2026-09-22-alice-wire.md)).
- Link Alice's entities where they are. The offset used as an entity's world
  origin was a rotation -- what turns a box, not where the box is -- so every
  linked entity got its absolute bounds around the wrong point, and every
  trace against one clipped in the wrong place. The retail linker settles it:
  it builds those bounds by adding `+0x260`, `+0x264` and `+0x268` to the
  local pair, and `+0x260` is immediately followed by the angles the same
  routine tests, which is the origin-then-angles pairing every engine of this
  lineage keeps. The offset that was being used goes instead to a routine that
  walks a box's eight corners through a transform. Two fields were also
  being read as one: `+0x204` gates linking and the module owns it, while
  `+0x208` is the linked flag the retail linker sets and the unlinker
  clears. fnTech3 cleared it on unlink but never set it on link, so the
  module could read the opposite of what had just happened; it is now set.
  All thirty-six maps still load and run two hundred frames.
- Bind Alice's entity record to the server's shared view. The record has no
  declaration anywhere, so its networked half is generated from the delta
  field table the retail engine carries -- every member at the offset the
  table gives it, tiling 456 bytes exactly -- and its engine-owned half comes
  from the offsets the linker, unlinker and snapshot walk touch. The module's
  arrays bind when it publishes them, at the capacity it publishes separately
  rather than the count in use, because that count is a high-water mark it
  raises as it spawns. Three members the view expects are not in the record
  and are marked absent rather than pointed at a plausible offset: no link
  count, which the view keeps itself; no single-client member, measured by
  absence from a snapshot walk whose every flag test is accounted for; and no
  ping inside the player state, because the engine writes it at 1,044, the
  first word past the networked part -- the third time that number has arrived
  by a different route. The flag word is translated rather than passed
  through, since Alice's bit positions are not this engine's, and the two bits
  the engine tests but this engine has no meaning for drop on the way in and
  survive untouched on the way out. Only the thirty members with a counterpart
  by name cross; every write reads the module's record first and puts those
  thirty back, so the forty-nine Ritual members come out of a round trip
  exactly as the module left them. A client now builds its gamestate and
  baselines and stops on the one thing this engine still does not have for
  the title -- a codec for its message bodies
  ([record](docs/fntech3/2026-09-22-alice-module-abi.md)).
- Encode Alice's message bodies. The delta tables said what a record holds;
  reading the engine's own six codec routines says what a message does with
  them, and it is Ritual's shape rather than id's. An entity delta carries a
  five-bit symbol: thirty-one means the seventy-nine bit changed-field vector
  follows raw, and anything less is an index into a thirty-one entry
  dictionary of change vectors the binary carries, each with a use counter
  ahead of it that ships at zero. After the changed values comes a thirty-two
  byte block sent one byte at a time under a mask of its own, and an entity
  whose only change is in that block still sends a delta -- the quiet case
  needs both to be empty. A player delta has no vector at all, one bit a
  field for forty-nine of them, then six thirty-two entry arrays under a mask
  each at sixteen bits a value. A changed float goes as a biased thirteen-bit
  integer when it is integral and within 4,096, else raw, with none of
  Quake III's separate zero shortcut. A user command is not keyed.
  `tests/alice_message_codec_tests.cpp` holds all of it to the measured
  shape, down to what each case costs in bits
  ([record](docs/fntech3/2026-09-22-alice-wire.md)).
- Record and play American McGee's Alice demos as `alice.exe` does. Its
  `record`, `stoprecord` and `demo` write and read Quake III's container
  under one fixed extension, `demos/<name>.dm3`, where fnTech3 had named
  them `.efdemo` because Alice shares Elite Force's framing, and refused to
  play them at all. `record` refuses unless `g_syncronousClients` is set --
  retail's own spelling, and a refusal rather than Quake III 1.32's warning
  -- keeps a typed name as typed, and with no name takes the first free
  `demo0000` to `demo9999`. `demo` opens the name as given when it contains
  `.dm3`, else adds it. No `RECORDING` notice is drawn, since `alice.exe` has
  none, and a demo recorded during playback keeps only its gamestate, as in
  retail. The gamestate's baselines were deltas from an all-zero entity;
  `alice.exe`'s server, client and recorder all delta from a null entity of
  its own -- alpha and scale 1, parent 1,023, a white constant light,
  `renderfx` 16 and bone tags -1 -- and fnTech3's now do too, which any demo
  exchanged with retail depends on. A garden1 gamestate shrinks from 31,511
  to 23,538 bytes, and a demo recorded there replays, and times, in a
  windowed client. No demo has yet crossed between fnTech3 and `alice.exe` in
  either direction ([record](docs/fntech3/2026-09-22-alice-wire.md#demos)).
- Run Alice's client game. With the wire contract, the entity binding and
  the message codec in place, a loopback client completes the handshake,
  parses a gamestate the server built in Alice's own delta format, loads the
  level's camera paths and scripts, and brings up the retail
  `base/cgamex86.dll` through the function-table path its entry point needs.
  Sixteen of the client game's 125 imports are served -- the printf family,
  the error call, the allocator pair, the cvar pair, the command accessors,
  the file calls, the console command and the shared block it asks for before
  anything else -- and the module runs until it needs one of the rest, which
  it then names. Four more were served walking that loop, two of them records
  that had to be established first: the game state is 21,316 bytes, which is
  1,328 configstring offsets, 16,000 bytes of string data and a count, and
  the graphics configuration is 5,188, Quake III's own members with
  1,024-byte strings and a 2,048-byte extension list, anchored by the
  renderer name the engine puts in a userinfo key, the width and height it
  pushes into `glViewport` and the flag its screen update tests for stereo.
  Twenty of the 125 are now served. What remains is a different shape and a
  wide one: the table's slots from 48 up are not wrappers but the renderer's
  own exported functions, so Alice's client game calls the renderer directly
  rather than through an import API of its own, and serving them means
  presenting this engine's renderer behind that table one entry at a time.
- Play American McGee's Alice's opening levels in a developer client. The
  retail client game now runs with 119 of its 125 imports served and the game
  module with 110 of its 111, and every slot left unserved is one the module
  never calls. The server builds each client's snapshot by retail's walk --
  the player's own entity first, attached and bound children judged by their
  parents, portal and sky-portal walks, the far plane, and the portal, sky
  and dual flags rewritten as retail rewrites them -- and the client game's
  `Could not find parent entity` warnings on `garden1` go from 2,998 to none
  in one binary. `garden1` draws Alice in third person with her HUD's health
  and mana gauges and weapon and powerup foldouts, alike on the GL, Vulkan and
  RTX renderers, with Ritual's shader dialect, FTX textures, the light grid
  and TIKI models. Saves are written as retail's four files into the home
  path, from the console or from the load/save menu, and read back, and a
  load of the running level resumes play with its models intact. The engine's
  own `.urc` menus are served with every widget class they use, the intro's
  logos lead to them, and New Game plays `opening.roq` into `gvillage`.
  `sv_maxclients` now defaults to 1, as retail's `SV_Init` registers it. No
  retail client, server, save or demo has been exchanged with fnTech3 and no
  frame compared with retail's, so every Alice gate stays partial
  ([ledger](docs/fntech3/COMPATIBILITY.md#alice-gate-matrix),
  [snapshot](docs/fntech3/2026-09-22-alice-wire.md#the-snapshot),
  [savegames](docs/fntech3/2026-09-23-alice-savegames.md),
  [HUD](docs/fntech3/2026-09-24-alice-hud.md),
  [menus](docs/fntech3/2026-09-24-alice-menus.md)).
- Carry what Alice's records hold and this engine has no member for. Its
  entity networks seventy-nine members and its player forty-nine; thirty-six
  and twenty-two of them are this engine's own, and the rest -- five bone
  modifiers, an attachment to another entity's tag, a torso animation with
  crossblend times, per-entity scale and alpha, and on the player the music,
  reverb and camera state a Quake III client game would have owned -- now
  ride in extensions beside the RTCW, Elite Force and Raven ones. Which
  member goes where is generated from the delta tables together with the
  mapping, and the generator refuses to emit anything unless the mapped and
  carried sets cover a table exactly, so a member cannot be lost by
  forgetting a line. Two correspondences that would be easy to make are not
  made: Alice's `anim` is a whole-body animation with a separate torso
  override rather than this engine's legs animation, and its `skinNum` is not
  `modelindex2`.
- Correct two record sizes. A delta table says what goes on the wire, not how
  large a record is, and reading its end as the record's end was wrong in
  both cases. Alice's entity state is 512 bytes, not the 456 its table
  reaches -- its reader clears and copies that many and the client's parse
  ring strides by it -- and its player state is 1,048 against a table
  reaching 1,044. Each record carries a tail its own engine never sends, and
  the members in those tails that some other part of the engine reads are now
  named: an owner number the snapshot walk compares against 1,023, a rotation
  the linker hands to its bounds transform, and the ping.
- Ask the feature, not the profile, about message framing. The client decided
  whether a message was raw bits with no leading acknowledgement by naming
  the Elite Force Holomatch wire profile, where the server had always asked
  the wire feature that makes it true. Alice has that feature too, so the two
  halves of a loopback session wrote and read different shapes -- the server
  sent raw bits with no leading long, the client read Huffman with one, and
  the result was a reliable-command sequence of 791,621,384. Both sides now
  ask the feature.
- Say why a record cannot be reached. Two refusals were each reporting the
  wrong thing. The shared entity view said "bad entity number" both for an
  index outside the array and for a profile it had no layout for, where no
  index would have worked; the delta writers said "invalid wire profile" both
  for a number nothing had registered and for one that is registered but whose
  message bodies are not. Each now says which of the two it is, so the next
  reader does not go and check a number that was never wrong.
- Start an Alice client. A client build brings the renderer up for this
  profile -- `R_Init` completes -- and then stops by name on the contract no
  other profile has: Alice ships no menu module, because its menus are the
  engine's own `.urc` resources. The loader would otherwise have reported a
  missing file, which says the wrong thing about why it stops. Two further
  gates are visible before that point: the retail `cgamex86.dll` is refused
  because it exports `GetCGameAPI` rather than `dllEntry` or `vmMain`, which is
  the same function-table shape the game module needed its own loader for, and
  the renderer asks for `gfx/2d/bigchars`, `white` and `console`, which are
  Quake III's asset names and are not in an Alice installation.
- Put Alice's entities in a world. Linking computes each entity's absolute
  bounds the way the engine does -- offset by the origin, or squared off to the
  largest extent when the entity is rotated -- and records its number as
  linked, which is the whole of the world this engine keeps for the title. On
  that, three imports stop being limited by fnTech3: the area query returns
  every linked entity whose bounds meet the box, `ClipToEntity` traces against
  one entity through the clip handle linking wrote, and the trace now clips
  against every linked entity a moving box can reach instead of against world
  geometry alone, reporting that entity's number where it hits one. 107 of the
  111 imports are served and all thirty-six maps still run. Of the four that
  are not, two the module never calls and two are on the savegame path, so
  nothing observable depends on any of them.
- Run every American McGee's Alice map. With 105 of the 111 game imports
  served, a developer dedicated server loads and runs all thirty-six retail
  maps: `+map <name> +wait 200 +quit` exits by itself with a clean code on
  every one, and `+wait` only reaches zero if two hundred server frames ran.
  Getting there took three rounds of the same loop -- serve what is measured,
  run, and let the module name what it needs next -- and the counts show where
  the work was: 86 imports served ran 19 maps, 95 ran 30, 96 ran all 36.
  One of those rounds corrected a mistake worth recording. The model query
  family was served with a single shared answer, because twenty-five of its
  twenty-seven members take the same `xor eax, eax; ret` branch when there is
  no model. Two do not: the module copies forty-eight bytes *out of* what slots
  78 and 79 return, so a zero is dereferenced, and five maps faulted at one
  instruction in the module until those two returned what the engine returns --
  the address of a cleared orientation with an identity axis. A family that
  answers uniformly inside the engine can still need different answers here.
- Run an American McGee's Alice server. With 86 of the 111 game imports
  served, a developer dedicated server loads a retail level and turns its
  frame loop on nineteen of the thirty-six retail maps: started with
  `+map <name> +wait 200 +quit` each exits by itself, cleanly, in about eleven
  seconds, and `+wait` only reaches zero if two hundred server frames actually
  ran. Of the seventeen that stop, fourteen stop at the same import, the trace,
  one at `AdjustAreaPortalState` and two at an exception inside the module. The
  trace is now served. Its record is 60 bytes -- the engine copies fifteen
  dwords back to the caller in two separate routines, while the thirty dwords
  it clears first are scratch, a reading that has to come from the copy rather
  than the clear -- and it takes eight arguments against Quake III's seven.
  Two of the fifteen dwords are named by what the engine does to them: `+0x8`
  is set to 1.0f where nothing is hit and read back with `fld`, and `+0x34`
  takes 1022 or 1023, the reserved entity numbers of a 1024-entity world. Both
  sit where Quake III's own trace record keeps a fraction and an entity number,
  and the module reads `+0x20`, which in that layout is the impact plane's
  vertical normal. The first 56 bytes are therefore filled as that record from
  a real trace against world collision, which was a hypothesis until the maps
  that had stopped at this import ran two hundred frames and exited cleanly. The module does its own level startup inside it
  -- 685 entities spawned and 19 inhibited on `garden1`, the map's AI path
  nodes loaded, and five script files loaded and running, including the map's
  own cinematic. The script interpreter doing that is Alice's, inside the
  module; this engine only serves what it asks for.
  Most of the last batch of imports were served together because the engine
  answers them together: every call in the model query family, and every one
  in the per-model alias block, opens by resolving a model index through one
  shared routine and taking the same negative branch, which returns zero and
  writes nothing. That branch was checked slot by slot rather than assumed, and
  it is the honest answer here because this engine loads no model. What runs in
  those frames is bounded by that: entities exist and carry their measured
  fields, but nothing is linked into a world, no model backs them, no client
  has connected and nothing is rendered.
- Spawn Alice's own entities. With the model, index, configstring and linking
  imports served, the retail module builds the level's entities from the map's
  entity text and names them as it goes -- `Characters_WhiteRabbit`,
  `Enemies_Snark`, `Item_MetaEssence_Medium` -- with the entity count climbing
  from a server's eight client slots to 104 on `garden1` and the array never
  moving. The configstring blocks those calls index are measured from the
  retail wrappers that pass them: base 0x20 over 512 entries for TIKI models,
  0x220 over 256, 0x320 over 64, 0x360 for light styles and 0x400 over 64, all
  inside this profile's 1,328. `IsModel` is served and answers that no model is
  loaded, because this engine loads none; the module falls back to a plain
  object and carries on, which is what makes answering honestly better than
  claiming a model that is not there.
- Spawn Alice's levels and run its frame. Alice spawns a level separately from
  starting the game and Quake III has no ordinal for that step, so fnTech3 has
  to drive it. The argument order is read from the retail engine's own call
  site: a short name first, the entity text second, zero third -- the opposite
  of the obvious reading, and trying it the obvious way overruns a 128-byte
  stack buffer in the module and has it return into its own entity text. The
  engine hands over a copy of the text rather than its lump, because the module
  parses it in place, and fnTech3 does the same. `SpawnEntities("garden1",
  76348 bytes, 0)` now returns, and the module confirms the reading by loading
  `dialog/standard.TLK`, then the map's own `dialog/garden1.TLK`, then
  `maps/garden1.pth`. `GAME_RUN_FRAME` is dispatched with the level time and a
  frame length recovered from it, both in milliseconds, which is what the
  module's own 0.001 scaling says those two arguments are. The boundary now
  stops at `linkentity`, the first import a spawned level needs.
- Read `alice.exe`'s own calls through the game export table, which shows one
  thing only the engine side does: after each call it reads export slot 25 and
  raises it as an error if it is not null, so that slot is a deferred error
  message rather than a return value.
- Serve twenty of Alice's game imports, and run its retail game module. Every
  one of the 111 slots is now filled: twenty with an engine service and the
  rest with a thunk that drops by name, so the module runs until it needs
  something that is not there and then says what. `base/fgamex86.dll` now
  completes `GAME_INIT` on a developer dedicated server -- it prints its own
  banner through the served `Printf`, registers its event system of 221 classes
  and 1,027 events, and publishes where its entities live. Two slots had to be
  served as data rather than as functions, which the module itself proved by
  storing *through* them and faulting when they held code; they are the two the
  corrected slot numbering puts at 101 and 102. The module reports a 732-byte
  entity record while running, matching the immediate it writes into export
  slot 22, and a 2,112-byte client record, which no static reading had given
  up. The engine's next call, `GAME_RUN_FRAME`, is refused rather than guessed:
  `RunFrame` reads two arguments where Quake III passes one.
- Establish Alice's light grid cell, which had been the one unestablished
  part of its map format. Searching the data for it had failed -- no cell size
  from 16 to 512 units reproduced even one axis -- so it was read out of the
  retail renderer instead: the light grid loader at `0x0049acb0` writes 192,
  192, 320 and their reciprocals into adjacent globals before it touches the
  lump, which is Quake III's `lightGridSize` pair with Alice's own cell in
  place of 64x64x128. With that cell, Quake III's own derivation against the
  world model's bounds reproduces the light grid point count exactly on all
  thirty-six retail maps. The cell and the derivation are frozen in
  `code/qcommon/bsp_fakk.h`; the translator still drops the lump, which now
  needs only the entity text to carry a matching `gridsize` key.
- Locate the engine side of Alice's client-game import table, which had been
  the open half of that boundary. It is not built on the stack like the game's
  and not a static array either, which is why earlier scans missed it: its
  address arrives as an argument and it is filled through a register base, in
  the same function that reports `CL_InitCGameImport failed`. The builder
  clears 125 slots -- the engine confirming from its own side the count the
  module's `rep movsd` gives -- and fills 122, never writing slots 0, 88 and
  116. Twenty-one slots hold an engine function the game import table also
  holds, so the game map names them by address, and twenty-two more speak for
  themselves once their addresses are known: slots 75 to 81 reach
  `CL_GetSnapshot`, `CL_GetParseEntityState`, `CL_GetUserCmd` and
  `CL_GetServerCommand`, and the block at 30 to 44 reaches the Miles sound
  errors. That locates both the client-state and sound runs of this table.
  None is frozen in `code/compat/` yet, because a third of a table is not a
  map. The import the client game
  calls before anything else is slot 124, whose body returns the address of
  the same shared block the game table build zeroes on startup.
- Measure what Alice's game module does with each of its 111 import slots,
  by disassembling the 2,654 places it calls through them. It calls 92 and
  never calls 19. Where a call site's own stack cleanup agrees with its own
  push count the argument count is settled, which fixes 59 of the 92 and marks
  six as variadic. The counts were taken after the slot names were, so where
  they agree they are two independent readings: `ClipToEntity` seven,
  `CalculateBounds` and `Alias_Add` four, `FS_FSeek` three, `inPVS` two.
- Correct the measurement of Alice's engine-side import table build. The
  engine fills that table on its own stack, and an unmatched `push` partway
  through the run shifts every later slot. Reading it without tracking `esp`
  wrote two slots twice and left four unwritten; tracking it partitions the
  table exactly, into 110 slots written once each and a single null at slot
  100. Thirty import names move onto the slots that actually hold their
  functions, confirmed by the bodies: the CRC-16 routine is `CalcCRC` at slot
  99, and the call that reads five arguments into five adjacent globals is
  `LocateGameData` at slot 103. The table now decomposes as the reference
  table's 105 members, one Alice insertion, and five Alice slots appended.
- Load Alice's retail game module on a developer server. Its game is a
  function table rather than a `vmMain`, so it loads through the same
  `VM_CreateTable` path Elite Force's single player uses, and the engine
  checks the published table against the frozen map before accepting it.
  On all thirty-six retail maps the server now mounts the installation,
  loads the map's collision, loads `base/fgamex86.dll`, prints its API
  version and 732-byte entity record, and refuses the first export by name.
  That refusal is the boundary: with no import signature established the
  module holds a table of nulls, so calling an export would fail inside the
  module rather than in the engine.
- Name the profiles a build can actually start when profile detection fails.
  The message carried a fixed list that had gone stale: a developer build
  with only the RTCW, ET and Elite Force options offered `q3-ihv`, `jka-sp`
  and `jka-mp`, which that same build refuses, and offered no profile a
  newer option enables. It is now built from the catalogue, so it lists
  exactly what the running executable admits.
- Run Raven's retail multiplayer game module through its own initialization.
  `jampgamex86.dll` loads out of the pak on a developer server, registers its
  cvars, prints its own `gamedate: Nov  6 2003`, opens `base/games.log` through
  the engine's filesystem and writes its first line into it. Twenty-seven
  imports are served: the console and error calls, the clock, the five cvar
  calls, the two argument calls, the console command, Raven's two arithmetic
  calls, the five filesystem calls, the configstring pair, the serverinfo and
  entity-token readers, the real-time clock, vector snapping, the scratch
  buffer the module lends the engine, the entity and client binding, and the
  Ghoul2 attachment clean. Quake III's own fourteen shared arithmetic and
  memory calls are handed through because Jedi Academy kept their numbers, and
  every other import stops the server with its own name and number rather than
  a made-up answer.

  Two of those are answered completely rather than partly, and a test keeps the
  difference honest: holding the lent buffer is the whole of
  `G_SET_SHARED_BUFFER`, and there is genuinely no attachment for
  `G_G2_CLEANENTATTACHMENTS` to clear while every import that could make one is
  refused. The module now stops at `G_ICARUS_INIT`, so Raven's script
  interpreter is on this title's critical path before a single entity exists,
  with server-side Ghoul2 immediately behind it.

- **Run a retail multiplayer level in both Jedi titles, refusing nothing.** A
  developer dedicated server loads Jedi Academy's `mp/ffa1` and Jedi Outcast's
  `ffa_bespin` against legitimate retail installations, runs each title's own
  retail game module through initialization and level spawning, and holds the
  level open through its frame loop with no unserved import at all. Outcast
  registers its items and parses its 29 bots and 15 arenas on the way.

  That is a boundary result, not a compatibility claim: no client connects, no
  snapshot is built, no packet is encoded and no frame is drawn. What it shows
  is that every import either module reaches during a map's life is answered,
  and that none of the answers is a guess.

  Every number Outcast needed was read out of its own engine's dispatcher
  rather than taken from Academy's table, and three of them show why that had
  to be done twice:

  - **Outcast's world block is Quake III's numbering; Academy's is not.**
    Its 30 and 31 call the same two routines the engine's own link offsets were
    read out of.
  - **Outcast's trace is Quake III's 56-byte record; Academy's is 48.** The
    retail engine reads the fraction eight bytes into the record it is handed
    and writes the entity number at fifty-two -- where Quake III keeps them,
    and where Academy does not.
  - **Outcast's recorded motion paths sit five numbers below Academy's**,
    identified by the `ROFF System Error` message the retail engine reports
    from the very routine the import calls.

  Academy's last word is the engine's own honesty coming back through the
  game: it reports being unable to save navigation data, because `G_NAV_SAVE`
  answers that it wrote nothing rather than write a file into an installation
  it was asked only to read. The checksum the module prints with that message
  is byte for byte the one the retail `.nav` file carries, which identifies
  that field of the container from two directions at once.

- **Two bots play a retail Jedi Outcast multiplayer match.** Ninety seconds of
  `ffa_bespin` with `Reborn` and `Tavion` in it: both clients connect, change
  their userinfo, enter, begin and collect seventeen items between them, with
  no unserved import and no error. It got there by its own route, with almost
  all of the engine work shared with Jedi Academy.
  Two measurements finished it. The **four arrays in the player state** were
  named by the engine itself: its delta reader prints `PS_PERSISTANT` before
  the loop at 280, `PS_AMMO` before the one at 408 and `PS_POWERUPS` before the
  one at 344, each running sixteen times, with the powerups loop calling a
  different reader -- long words where the others read shorts, which is what a
  powerup is. The first loop, at 216, is the one left over: stats. That
  accounts for 680 of the record's 1,368 bytes; the other 688 have no name in
  any table this engine has read, so the record declares them as unnamed spans
  and **there is deliberately no way to write one**, because a write would put
  zeroes over half a record it cannot name.

  The second was a trace whose pass entity was -1, which the engine refused.
  That is not a fault in the module: retail compares the pass entity only
  against 1023, so a -1 indexes its entity array at -1 and reads whatever lies
  in front of it. The only defined reading of -1 is that there is no pass
  entity, so this engine says that deliberately rather than reproduce it by
  reading out of bounds, and serves the trace on its own for the purpose.

  The blocker was this title's input frame, and the engine is no help with it:
  the record is 28 bytes and the engine copies it whole -- its reader is a
  seven-dword `rep movsd`, and every other place it touches one does the same
  -- so it never reads a field. The client that *builds* a command does. At
  `0x4076c0` it writes the time at 0, ORs the buttons together at 16, and
  stores four bytes at 20, 21, 22 and 23 each from its own global, **clearing
  the fourth's global immediately after** -- which is what identifies that
  field as a command that fires once rather than merely showing that one is
  there. Three more bytes at 24-26 are the moves, and with the first byte field
  at 20 only 4, 8 and 12 remain for the view angles.

  It comes out the same shape as Academy's, which was not assumed from the
  shared size: Academy's was frozen from its own measurement, and this was read
  out of Outcast's own client before the two were compared. Quake III's is 24
  bytes and is neither.

  With that, its elementary-action block and remaining bot-library calls were
  served -- all sixteen actions reach the same export table the setup call
  reaches, at slots consecutive in that library's declaration order, and import
  211 computes `&clients[n]` by the client stride, which is this engine's own
  client think.

  It stops at `SV_BuildClientSnapshot`, with no player-state adapter. Academy's
  was generated from a table naming all 168 of its fields; Outcast's delta
  table names 105 and leaves gaps where arrays live, the largest from 192 to
  468, so the same generation cannot be done until those are measured.

- **A Jedi Academy bot connects, enters and plays.** With the player state
  written, the bot stopped being a client that exists and started being one
  that plays: a developer dedicated server left running showed it moving about
  the map and taking things -- `Item: 0 weapon_disruptor`, `weapon_thermal`,
  `ammo_powercell` -- with no import refused anywhere in the path.

  That state held only while every lightsaber in the game was quietly failing
  to load, which kept the game off the branch that asks where a bolt on a saber
  is. Repairing the loader (below) is plainly right, and the consequence is
  that the game now reaches that branch as soon as the bot has a saber: it
  connects, enters, begins, and is refused a bolt matrix. Running longer on a
  model that failed to load was not the better state.

  Academy's player state is the larger of its two records: 168 fields, of which
  43 are Quake III's at Quake III's widths and stay in the canonical record and
  124 are Raven's and are carried in an engine-owned extension. Ammunition is
  the one field that is neither -- this title has nineteen slots where the
  canonical record holds sixteen -- so the first sixteen keep their canonical
  meaning and the last three are carried. None of the 168 members was typed out
  by hand: they were generated from the measured table, and the same
  byte-for-byte test the entity state has covers this one, so a field left
  unmapped comes back zero and fails. The force block is carried whole and
  unread, because the engine has no business knowing what a force power is.

- Stop looking for a skeleton that was never a file. The server-side Ghoul2
  store had been reporting, of every lightsaber in both games, that *the
  skeleton it names is not there*. It was right that it had failed and wrong
  about why: the mesh names `*default`, and a name beginning with a star is not
  a path -- the mesh carries its own bones and there is nothing to open.

  Measured across both installations, `*default` is the **only** such name
  there is: 41 of Jedi Academy's 127 meshes and 23 of Jedi Outcast's 71 carry
  it, and every name that does not begin with a star resolves to a `.gla` that
  is present, in both. So the star is the rule, not an exception. Fixing it
  moved the module boundary forward immediately, which is the best evidence
  the negative was real -- with its sabers loading, the game went on to ask for
  four more Ghoul2 calls it had never been able to reach, each of which is now
  answered from the retail engine's own handler.

- Confirm Jedi Academy's frozen import table against the retail engine's own
  dispatcher, and have it come out exactly. `jamp.exe` bounds the game import
  number at 634 and jumps straight through a 635-entry table; the slots that do
  not fall to `Bad game system trap` number **329**, and fnTech3's table names
  **329**. The two sets are identical -- every number the retail engine handles
  is named here and every number named here is handled there, with none left
  over on either side. The table was built by an unrelated method, so this
  confirms all 329 rows at once.

  The same table also says what the next frontier takes. The call that stops a
  bot spawning reads ten arguments and converts exactly one of them from a
  module address, which places the bone's name as the only pointer in the list;
  its neighbours are recorded the same way so that when they are served, the
  shape comes from the engine rather than from a guess.

- Connect a bot to a running Jedi Academy server. `addbot` walks the whole
  client path without a network client, and it gets a long way: the module
  reads Raven's own `botfiles/bots.txt`, finds the bot, is given a client slot,
  writes its userinfo, connects it, allocates its goal, move and weapon states
  out of the bot library, and builds its body -- a Ghoul2 instance duplicated
  for it, attached to its entity number, its skeleton read back, and its bolts
  registered by name.

  Four things were built for that, and each is real rather than an empty
  answer. An instance can be copied, and the copy owns its models. Attachments
  are held by entity number, releasing an instance takes its attachments with
  it, and **`G_G2_CLEANENTATTACHMENTS` now really clears what the engine
  holds** -- it used to do nothing, which was honest only while nothing could
  make an attachment, and the test that guarded that has been replaced by one
  checking the clearing is real. Models carry the surface and bone names their
  files already proved were terminated, read out once at load. And a bolt is
  looked up among those names, a surface first and then a bone, because the
  two are numbered separately; a name the model does not carry is reported as
  no bolt rather than invented.

  **The bot now enters the game**, and no import is refused anywhere in the
  path: the module broadcasts its arrival and calls `ClientBegin` for client 0.
  Getting there needed a bone able to be set playing and a bone able to be
  bent -- both argument lists read out of the retail engine's own handlers, the
  first converting exactly one of ten arguments and the second three of eleven
  -- a client input frame that converts both ways, seventeen elementary actions
  served by name because Raven inserted two into the middle of Quake III's
  block, and the input frame read back, whose record was measured rather than
  assumed from the one place the module reads its action flags.

  Two of those carry stated gaps rather than hidden ones. The engine holds what
  a bone was set playing and does not work out which frame it is on, because
  that arithmetic is not measured. And the two actions Raven inserted have no
  counterpart in the bot library this engine carries, so they are answered
  without doing anything: a bot here moves, turns, jumps, crouches, fires and
  changes weapon, and does not alt-fire or use a force power.

  It now stops at the engine's own boundary rather than the module's.
  `SV_BuildClientSnapshot` refuses to read the bot's player state, because this
  title has no player-state adapter and the checked view will not read a Quake
  III one out of a Raven record. That names the next contract exactly.

- Fix a guard of this engine's own that was refusing valid input. The check
  that keeps a vector that is not finite out of the collision model also
  refused a *missing* box, and a missing box is how this lineage has always
  asked for a point trace -- the server's own trace substitutes the origin for
  one. The guard now allows the two a trace may leave out, refuses a missing
  vector anywhere else, and names the call and the numbers when it refuses: its
  old message printed zeroes for a null pointer, which read as though the game
  had handed in bad data.

- Freeze Jedi Outcast multiplayer's entity state whole, and name every one of
  its 74 members. Raven released no multiplayer source for this title, so the
  record was read out of the retail engine's own delta table: `jk2mp.exe` holds
  73 triples of name, offset and bit width, and sorted by offset they cover
  **every four-byte slot from 4 to 292 with no gap at all**. `number` at 0
  completes it -- no engine in this line delta-codes an entity's own number --
  and the record ends at 296, which is the size already measured by an
  unrelated means from the engine's brush-model writer. Two methods, one
  answer.

  The table also types every member without guesswork: a member sent at zero
  bits is a float, and the members that applies to are exactly the ones a float
  belongs in. The result is held to the table at compile time.

  `ping` is at 484. It is in no delta table, because no engine in this line
  sends a ping that way, so it was read instead out of the one place the engine
  writes it: the ping pass clamps to 999, calls its own client-state accessor
  -- a base plus index times stride, and nothing else -- and stores through
  `mov [eax + 0x1e4], edx`. The engine's checked view now writes Outcast's ping
  there instead of refusing to write one.

- Bound Jedi Outcast's module boundary, and serve its Ghoul2 model
  initializer. Its module is a QVM, which reaches the engine by calling a
  negative address, so counting those calls says exactly what it can ever ask
  for: **97 imports, and no others**. Comparing those against Jedi Academy's
  150 -- measured the same way from its DLL's call wrappers -- says which parts
  of the two titles' numbering are the same and which are not, by argument
  count rather than by resemblance. The elementary-action block agrees on all
  sixteen numbers both call, and the AI-state block on all eleven; the block at
  0 and the Ghoul2 block do not agree at all. So import 588 was named by
  reading its own handler in the retail engine, which converts two module
  addresses and passes them with five more numbers to a routine that allocates
  an instance when the module's pointer is null and loads a named model into
  it. That is the call Academy makes at 590, and the same server-side store now
  answers both.

  Outcast's bots were declared from the same kind of evidence as Academy's: its
  installation ships Raven's own `botfiles/*.jkb` and `bots.txt`, none of Quake
  III's AI configuration sources, and no `.aas` file for any of its 41 maps.

- Spawn a retail Jedi Academy multiplayer level. The module's entity state now
  converts both ways: twenty-nine of its ninety-four fields are Quake III's and
  keep Quake III's meanings, and the other sixty-five are Academy's and live in
  an engine-owned extension. The accounting is checked rather than claimed -- a
  record with a distinct value in every one of its 532 bytes goes out to the
  engine's state and comes back byte for byte, so a field left unmapped fails
  the test instead of reading as zero.

  With that, the world answers: linking and unlinking, brush models, point
  contents, both visibility tests, area portals, area connection, the entities
  in a box, entity contact and the world trace. Linking and unlinking are
  served by name and never handed to the shared dispatcher, because Quake III's
  34 and 35 are the two bot-client calls -- handing Raven's numbers through
  would have freed a bot client instead of unlinking an entity.

  Two of Raven's renumberings were measured out of the retail module rather
  than inherited from Quake III, and either taken on resemblance would have
  been silently wrong everywhere it was used:

  - **Its sine and gravity trajectories are 5 and 6, not Quake III's 4 and 5.**
    Raven inserted a type of its own at 4. Every falling object in the game
    would have oscillated.
  - **Its trace is 48 bytes, not Quake III's 56**, with the two solid flags and
    the entity number packed into the first four, so the fraction sits where
    Quake III keeps a flag and everything after it is four bytes early. This
    was read off 203 of the module's own 233 call sites: offsets 0 and 1 only
    ever as single bytes, offset 2 only ever as a sixteen-bit word, offset 4
    only ever as a float.

- Hold the navigation graph Raven's bots walk on. It is the engine's to keep
  and the game's to fill -- nodes with positions and radii, edges with costs,
  shortest paths between every pair, nodes an entity failed at and edges
  something is blocking. It is not Quake III's area awareness and shares
  nothing with it, and that is measured, not assumed: **the retail module
  carries a call wrapper for 150 of the 796 import numbers and not one of them
  is in the area-awareness block**, and the installation ships no `.aas` file
  for any map. The same count bounds the whole of this title's module boundary
  rather than estimating it.

  The bot profile followed the evidence. It claimed Quake III's four configured
  AI subsystems; the installation ships none of `items.c`, `weapons.c`,
  `syn.c`, `rnd.c` or `match.c`, only Raven's own `botfiles/*.jkb` and
  `bots.txt`, and the module never loads a character or allocates a chat state.
  It now says what was measured, which stopped the engine truthfully reporting
  a missing `weapons.c` for a file this game was never going to have.

- Hold Raven's recorded motion paths server-side and cache them by name, with
  the steps of a path readable one at a time and its note track readable by
  number. Starting a path stays refused by name: how this engine should move an
  entity along one -- whether it sets the origin outright or lays a trajectory
  between steps -- is not measured, and a guess would move every scripted lift
  and camera in the game the wrong way.

- Carry Jedi Academy multiplayer's entity and client arrays in the engine's
  checked game-data view, at Raven's own strides: an 864-byte shared entity
  whose engine-owned half starts 560 bytes in, past the game's playerstate,
  vehicle and Ghoul2 pointers, and a 1,552-byte player state. Raven's own
  `mIsRoffing` flag and 64-bit broadcast mask sit inside that engine half and
  are left to the game. No entity state is converted: the view refuses rather
  than read a Quake III state out of a Jedi Academy record.

- Start a Jedi Academy developer dedicated server on a real installation. It
  activates either profile, mounts the four retail paks in Raven's own order,
  executes Raven's `default.cfg` out of them, writes each mode's own
  configuration into an isolated home path, and loads a retail map's collision
  through the RBSP translation. It then refuses the game module by name instead
  of handing a retail module an import table nothing serves, which is what used
  to end in an access violation. Nothing is drawn and no packet is sent. See
  the [runtime record](docs/fntech3/2026-09-21-jka-runtime.md).

- Read Jedi Academy's own maps. `RBSP` version 1 is registered as the only map
  format either mode accepts, and a bounded translator presents one to the
  shared collision loader as the layout it already reads, keeping the map's own
  checksum. All 57 maps in a legitimate installation translate. Raven's four
  lightmap sets, their styles and the light array are deliberately dropped, so
  this is collision evidence only and a renderer still needs its own reader.
  Retail pak naming is now profile-owned, so Jedi
  Academy's `assets0`-`assets8` are never offered for download while Quake's
  numbered names stay free for a mod. See the
  [map record](docs/fntech3/2026-09-21-jka-maps.md).

- Read Jedi Academy's Ghoul2 models. Bounded readers walk a `.glm` mesh's
  surface hierarchy, levels of detail, triangles and weighted vertices, and a
  `.gla` skeleton's bones, frame indices and compressed pose pool, following the
  retail renderer's own traversal and refusing every malformed case rather than
  trusting a stored offset. All 127 meshes and 66 skeletons in a legitimate
  installation read, each mesh against the skeleton it names. Nothing is posed,
  skinned or drawn. See the
  [model record](docs/fntech3/2026-09-21-jka-models.md).

- Read Jedi Academy's compiled ICARUS scripts, the `.ibi` files its designers'
  behaviour graphs compile to. A bounded reader walks the container -- an
  identifier, a version Raven wrote as a float, then blocks of numbered members
  whose payloads are sized but never padded -- refusing a count that no
  remaining byte could serve, a payload that reaches past the file, and trailing
  bytes that belong to no block. It offers a block that has no members rather
  than passing over it, and hands a member out as text only when it ends in one
  terminator and holds none before it. All 1,018 scripts a retail installation
  stores parse exactly, with nothing left over in any of them; the 1,011 the
  engine would open sweep through the reader itself with no refusals. Nothing is
  interpreted: no block number is given a meaning and the engine still refuses
  every ICARUS import.

  A second reader over that container groups each block's members into the
  values the block was given, because a member's payload is not always its
  value: some introduce the ones after them -- three floats, a type and a name,
  a pair of bounds. Six rules cover every one of the 26,512 blocks in a retail
  installation, and the reader refuses anything they do not: a member number
  the corpus never used, a group that runs off the end of its block, a type
  that names no kind of value, a comparison anywhere but last. All 1,011
  scripts the engine would open read through it with no refusals -- 25,987
  statements and 39,854 values -- and every one of the 169 ranges has its first
  bound below its second.

  What the corpus fixes about itself beyond that is recorded rather than built
  in: five block numbers open a body that one closes and all 1,011 scripts nest
  exactly, and the names divide into a map's, a single script's and a level's.
  A structure established over one installation is evidence, not a
  specification to enforce.

  Raven's own enumerations, read after all of that was derived, name the same
  numbers and agree on every one: the five that open a body are its affect,
  loop, if, else and task, the memberless one that closes them is its block
  end, the only block carrying a comparison is its conditional, and the one
  whose string always names an installed script is its run. Those names are
  frozen with their numbers and cited by file and line, so a script can be
  described rather than only counted; nothing runs one, and every ICARUS import
  is still refused. This matters earlier than scripting usually would, because
  a Jedi Academy game module asks the engine to initialize ICARUS before a
  single entity exists. See the
  [script record](docs/fntech3/2026-09-21-jka-icarus.md).

- Read Jedi Academy's ROFF motion paths, the recorded offsets and rotations its
  cameras, lifts and cut-scene props follow, with their note tracks. All 92
  paths a legitimate installation ships read; a path with a count it cannot
  keep, a note range that leaves its track or bytes left over is refused.
  Nothing moves along one yet. See the
  [motion record](docs/fntech3/2026-09-21-jka-roff.md).

- Freeze the eight records Jedi Academy multiplayer's modules and protocol
  exchange -- its entity state at 532 bytes, its player state at 1,552, the
  shared entity at 864, and the cvar record, input frame, trajectory and Force
  block. All 363 members' offsets and sizes are measured from Raven's own
  headers and asserted at compile time, so a member that moves or changes width
  cannot reach a module or a packet. Nothing marshals them yet. See the
  [record layout](docs/fntech3/2026-09-21-jka-records.md).

- Freeze Jedi Academy multiplayer's 796 module numbers across its game, client
  game and menu modules, with its API versions, its gaps refused explicitly and
  no other profile resolving them. Nothing dispatches them yet. See the
  [module record](docs/fntech3/2026-09-21-jka-module-abi.md).

- Register Star Wars Jedi Knight: Jedi Academy as a sixth title with two mode
  profiles over one `base/` installation, `jka-sp` and `jka-mp`, behind
  `-Djka-development=true`. Both carry their own protocol numbers, module
  names and locations, configuration names, configstring layouts, client
  limits, RBSP map identity and userinfo defaults, and both stay blocked in
  ordinary builds. A legitimate 1.01 installation is recognized from
  `base/assets2.pk3`, which holds both configurations, and reports that a mode
  must be chosen; single player's module is read from the installation root
  while multiplayer's three ship inside the paks. No Steam identity is guessed.
  Protocol versions may now name more than one family, since Jedi Academy
  shares 26 and 40 with Elite Force's families. No module, wire, savegame or
  renderer boundary exists yet; see the
  [foundation record](docs/fntech3/2026-09-21-jka-contracts.md).

- Give the gated IHV dedicated server its native bot clients and their
  snapshots. Bot allocation, release and userinfo, the bot scheduler's position
  in the simulation step, both snapshot-history getters and the builder that
  selects what a client can see now run on native client records, checked
  against the original functions. IHV keeps its own Quake II-style cluster
  record; the other games' linking is unchanged. The dedicated server also
  writes each frame as a protocol-33 snapshot message, byte-identical to the
  original writer, with its delta-base rules, Quake II-style baselines and
  entity limit.

- Give Quake III IHV its own network channel. Protocol 33 no longer borrows a
  Quake III wire contract: it carries two sequence words with reliable bits, a
  single retransmitted reliable slot, Quake III-style fragmenting and a
  scrambled payload, matching the original packet for packet. No connection
  layer carries these packets yet.

- Run the original IHV API 4 game DLL in gated Windows x86 dedicated probes.
  Preserve game allocations across maps, marshal cvars and entity imports,
  connect real collision queries, and clean up partial startup safely. All four
  supplied maps, level restart, shutdown/reload and two rejection paths have
  runtime evidence. Native clients, snapshot building and protocol-33 transport
  remain incomplete.

- Restore IHV model lightmap sampling, dynamic-light attenuation and diffuse
  colors in GLx, Vulkan and RTX raster rendering. Preserve raw lightmaps and
  native numeric behavior, checked against all four supplied maps. Connect
  `r_lerpmodels` to native raster/ray animation and ray-cache invalidation.
  Native scene integration and rendered playback remain gated.

- Match native IHV MD3 vertex/normal interpolation and origin-expanded culling
  in GLx, Vulkan and RTX, including ray geometry. Use a private native sine
  table and stage surfaces before publishing vertices. Original-function
  comparisons include all 111 supplied models; native scene integration and
  rendered playback remain open.

- Add owned IHV entity batches, separate native flags, primary/portal visibility,
  camera-facing axes and origin interpolation to the renderer paths. Register
  native `*beam` models and render their verified geometry/material in GLx,
  Vulkan and RTX, including ray geometry. Renderer API 28 requires matching
  DLLs. Retained transform/lighting flag `0x80` is explicitly rejected; native
  scene integration, lighting and rendered playback remain open.

- Add IHV particle submission and per-view triangle expansion to GLx, Vulkan
  and RTX, including RTX ray geometry. Match original geometry, procedural
  texture and blend/depth observations. Enable the existing IHV asset dialect
  for the dedicated `q3-ihv` profile as well as retail Q3 asset loading.
  Native startup and rendered IHV demo playback remain gated.

- Correct IHV BSP 43 map loading: admit valid surface identifiers, preserve
  installed map checksums and exact inline brush membership, and validate
  malformed geometry before conversion. All four supplied maps load in the
  three renderers with retail Q3 modules. Full IHV game, protocol-33, demo and
  material support remains open in the [IHV ledger](docs/fntech3/Q3_IHV.md).

- Parse IHV sky/cloud, entity-color and depth-write commands in all three renderers,
  and repair the supplied `common.shader` closing brace in memory without
  changing installed assets. Portal and other IHV rendering contracts remain open.

- Render IHV gradient and animated fog in GLx, Vulkan and RTX using density
  checked against the original executable. Preserve transparent-stage fading
  and use classic dynamic-light compositing on IHV fog maps. This adds asset
  rendering support; native IHV gameplay and complete visual parity remain open.

- Restore IHV cloud colors and lightning timing, checked against the original
  executable. Preserve each cloud stage through GLx, Vulkan and RTX shader
  optimizations; the supplied `tim_dm1` sky no longer uses a fallback texture.

- Establish separate IHV native game records, bounded entity/cvar storage and
  tagged allocation ownership, checked by original-DLL initialization, shutdown
  and failure probes. Engine integration and native IHV gameplay remain open.

- Add separate IHV protocol-33 state codecs, message readers/writers and
  length-only demo containers, checked against original routines and the
  supplied `sli.dm3`. Malformed input leaves previous state intact. Engine
  playback/recording integration and live interoperability remain open.

- Register a separate `q3-ihv` profile and recognize the supplied installation
  from bounded DLL-export and BSP43 metadata checks without executing modules.
  Keep mixed retail/IHV installations ambiguous and IHV startup gated.
  Add IHV's 16-bit solid packing and original link-bound calculations to the
  server link path, checked against 2,740 original-function cases.
  Preserve its 256-client engine limit separately from the default of eight
  and the native game's entity-capacity rule. Reject IHV at the retail bot/AAS
  boundary and keep its limit out of the fixed retail bot pools.

- Connect IHV entity and raw player records to the native server game-data
  adapter, including word-sized entity numbers, owner/client pointers, checked
  allocation ownership and the original ping offset. Preserve native padding
  and private fields during engine updates; validate the actual adapter against
  original DLL callbacks and the server link path. Native startup remains gated.

- Restore IHV server trace-result replacement, owner filtering, state/current
  transforms and sweep-bound rounding against original numerical observations.
  Keep its valid entity 1023 distinct from a null owner. Preserve later profile
  trace behavior; full BSP collision and native gameplay remain open.

- Decode and write IHV configstring updates after the initial gamestate,
  preserving message event order and the original native string-table packing.
  Check complete records against the original parser and reject malformed or
  overflowing updates without publishing partial state. Native client startup
  and live protocol integration remain gated.

- Add IHV native client histories, complete snapshot records and ordered packet
  delivery with failure cleanup. Compare supplied demo states with the original
  parser and cgame data callbacks. Native initialization, prediction, rendered
  playback and live sessions remain open.

- Add validated IHV MD3 frame/tag metadata and stable native model records,
  including the frame pointers used to position HUD models. Tag interpolation
  matches the original executable across all supplied models. The isolated
  cgame host now completes initialization and demo frame/draw callbacks;
  engine integration and rendered native playback remain open.

- Add owned IHV scene records with checked array bounds, resource identities
  and atomic publication. Confirm the opposite IHV/retail area-mask polarity
  and entity RGBA against original numeric routines. All captured native demo
  scenes pass the boundary; actual rendering and native startup remain gated.

- Implement IHV skin registration and surface-order shader selection across
  GLx, Vulkan and RTX, including ray geometry. Preserve native case-sensitive
  caching and missing-file fallback; bound the old parser's unchecked arrays.
  Correct existing RTCW nonfinite skin-scale rejection under fast-math and RTX
  negative MD3 skin indexing found during validation. Native playback remains open.

- Add Q3 protocols 66/67 for 1.29 through 1.31, including automatic server
  negotiation, the earlier pure-proof format, exact historical pak6 variants
  and demo recording/interchange. Recognize raw 43/45/46/48 demo extensions
  and the official protocol-46 recordings named `.dm_48`; validate the actual
  gamestate before selecting a decoder. Q3Test and earlier live protocols
  remain open in the [point-release ledger](docs/fntech3/Q3_POINT_RELEASES.md).

- Regenerate missing or damaged RTCW navigation caches instead of refusing
  map startup. SP and MP keep their own visibility rules and cache versions;
  generated files are saved in the writable home and can be read again.
  Retail SP cache interchange passes on three maps. RTCW remains experimental.
- Refuse Elite Force single player before filesystem startup on platforms that
  cannot host its Windows x86 modules, with an actionable Holomatch alternative.

- Report unavailable Elite Force movie audio when Bink initializes its sound
  factory but cannot open the movie's sound buffer; video playback continues.

- Restore Elite Force's full-length stencil shadow extrusion in GLx, Vulkan
  and RTX, with native geometry fixtures for both SP and Holomatch.

- Bound Elite Force cylinder detail and electricity lengths before integer
  conversion, with native retail geometry fixtures for all eight Holomatch
  effect primitives in GLx, Vulkan and RTX.

- Restore Elite Force single-player movie fades and the central crop selected
  by `cl_VideoQuality 2`, including decoder pause and sound muting during
  transitions in all three renderers.

- Validate Elite Force RDM5 skeletal models before renderer allocation, restore
  retail LOD and held-frame behaviour, and match its lighting defaults and
  colour rounding across GLx, Vulkan and RTX.

- Support Elite Force Holomatch protocol 26 with Lilium Voyager/ioEF, including
  compressed messages, sequence checksums, pure servers, downloads and `.dm_26`
  demos. Retail protocol 24 and `.efdemo` remain available in the same build.

- Play Elite Force single-player Bink movies using the user's installed Windows
  x86 runtime, including in-game pause/resume, German track selection and
  low-resolution fallback. No proprietary runtime or movie data is included.

- Carry Elite Force single player's scissor rectangle through all three renderers,
  including fractional pixel conversion, reset, and clipping bounds.

- Name keys as both retail key tables name them. The keypad star is `KP_STAR`
  again, so a retail config line that binds it loads and is written back
  instead of being refused and lost, and keys 130 and 157-159 are written as
  `0x82` and `0x9d`-`0x9f` rather than `POWER` and `F13`-`F15`, so a config
  this engine writes loads in retail Quake III and Quake Live. Those four
  names are still read, so configs written by earlier builds keep working. The
  keypad star itself still reports `*`, as retail Quake III, Return to Castle
  Wolfenstein and Enemy Territory all report it on Windows and X11, so a
  `KP_STAR` binding does not fire and an existing `bind *` keeps working.

- Wait out another program's hold on a file being replaced, on Windows. A
  virus scanner or the search indexer opens a file moments after the engine
  writes it, and Windows will not replace a destination somebody else holds
  open, so a replacement with nothing wrong with it failed. Unpacking a game's
  native module into the local cache reported that as a hard drop, which is
  how launching a title with a native module could fail for a reason that was
  gone a millisecond later. The three denials another process can cause are
  now retried for up to a second; a missing source, an unusable path and every
  other error still fail on the first attempt. No other platform is affected,
  since only Windows denies a replacement over an open file.

- Add Heavy Metal: F.A.K.K.2 as a ninth title and the `fakk2` profile, gated
  behind `-Dfakk2-development=true`. F.A.K.K.2 runs Ritual's own engine, the
  parent of Alice's, and is served through Alice's adapters: its 105-slot game
  and 124-slot client-game tables map onto Alice's slot for slot, and its
  shorter entity state, render entity, view, snapshot, game state and command
  lists cross into Alice's records at the module boundary. Its `FAKK` 12 maps
  load in collision and all three renderers, every map spawns through the
  retail game module, and a developer client draws the retail main menu and
  `fakkhouse`, saves and loads back, and records and plays back a demo. Its
  `fakk2.exe` is encrypted on disk, so its engine behaviour is taken from the
  retail modules and data and, where they do not settle it, from Alice's
  engine ([record](docs/fntech3/2026-09-24-fakk2-contracts.md)). A copy of the
  retail disc is refused by name, since its game libraries are still inside the
  installer's cabinets.
- Give each inline model its own model index in Alice and F.A.K.K.2, as their
  engines do when the world is cleared (`alice.exe` 0x00457960). Model
  definitions could take a brush model's index, and the game module then ran
  that definition's init commands on the brush entity: F.A.K.K.2's
  `fakkhouse` lost its sky camera that way and drew a black sky.
- Draw a Ritual shader's `nextBundle` stages. F.A.K.K.2's scripts put a second
  texture bundle on 58 stages -- its menu art, HUD gauges, water and blood --
  which every renderer refused, and those shaders fell back to the default.
  They now draw both bundles with `GL_MODULATE`, as Ritual's engine does.
- Give a Ritual picture the colour it was drawn in. Alice's and F.A.K.K.2's
  `rgbGen global` shaders read the colour the last `SetColor` set, which
  Ritual's engine applies at once; fnTech3 batched pictures and read the colour
  when the batch was drawn, after the menu code had reset it to white, so
  F.A.K.K.2's menu clouds drew at full strength over the whole menu.
- Give F.A.K.K.2 its HUD, inventory and weapon groups. Its HUD is four of its
  menus made of stat bars with their flash, numbers and the weapons' models,
  shown by which weapons are in hand and whether a boss lives; its inventory
  opens from the weapon keys and the mouse wheel with the held items' models;
  `saveweaps` keeps the weapons in hand as a `weapgroup` alias, which the
  configuration now keeps between sessions with its quoted names intact.
- Read F.A.K.K.2's own player-state slots for the cinematic flag, the fade and
  the letterbox. The client read Alice's, so it drew letterbox bars from the
  left weapon's maximum ammunition and could skip a cinematic that was not
  playing instead of opening the menu.
- Light a model drawn without a world in F.A.K.K.2, as Quake III does. Its HUD
  and inventory draw lit weapon models in such views and add no light, and
  Alice's rule of starting them unlit drew black silhouettes.
- Serve F.A.K.K.2's menu widgets: its resolution and texture pulldowns, the
  graphics menu's report box, linked labels shown as words ("Default",
  "Trilinear"), whole-menu fades, the boss bar's slide, its own slider thumbs
  and scrollbars, and the Apply buttons' restart check. Linked labels in both
  Ritual titles now show a setting's pending value, as their engines do.
- Ship an audio zone file and a static light file for each of F.A.K.K.2's
  thirty maps, generated from the retail maps as Alice's are, so its levels get
  fnTech3's room acoustics and static map lights. The lights' colours are
  balanced so that the per-pixel lights no longer wash levels out. The zone
  compiler now reads F.A.K.K.2's own map version under `--profile fakk2`.
- Load a Ritual level's audio zones after a level change that names a spawn
  target. F.A.K.K.2's exits change level with `gamemap "training$spawn_fakkhouse"`,
  and the zone lookup asked for `training$spawn_fakkhouse.azb`; it now reads
  the level's own `training.azb`. Fifteen of F.A.K.K.2's 27 exits name a spawn
  target. None of Alice's do, and other profiles are unchanged.
- Start F.A.K.K.2's menu widgets in `verdana-12`, as its retail menu loader
  does, rather than `verdana-14`: their text had been a size too large. The
  load/save list now shows each save's level title, newest first, the chosen
  one black on white, as retail's does; retail's own load menu lists and
  pictures saves fnTech3 wrote.
- Draw and drive F.A.K.K.2's controls-menu bind list as its retail menu
  does: its Action, Primary and Alternate columns of pictures, each key's own
  picture or name, and a second key per action that can be bound, cleared and
  shown. Enter and a second click on a key cell wait for a key, and any mouse
  button binds while one waits. It had been drawn as Alice's two-column text
  list, with no alternate key.
- Register the Ritual clients' `com_blood` (default 1, sent in the userinfo
  and serverinfo), `s_milesdriver`, `s_speaker_type`, `s_khz` and
  `joy_threshold` (0.15), as both retail clients do. F.A.K.K.2's game module
  reads `com_blood` for its gore, and the options menus show the others. They
  had drawn blank, EXCESSIVE VIOLENCE unticked, and the joystick threshold at
  zero on Windows, whose input has no joystick layer.
- Draw F.A.K.K.2's scrollbars and save list as its retail menus do: light
  grey, black-edged scrollbars whose arrows grey out while there is nothing to
  scroll, always shown beside the save list; dark column headers with white
  titles; and the `no_saved_games` picture when no save is chosen.
- Draw F.A.K.K.2's sliders as its retail menus do: an arrow 15 units wide at
  each end, lit while held, and a thumb 10 units wide, lit while dragged. They
  had no arrows, and a thumb 8 units wide that sat off retail's. `thumbshader`,
  `leftshader` and `rightshader` now replace their pictures. The arrows step
  as retail's do.
- Cut each F.A.K.K.2 menu widget's drawing to its own frame within its menu,
  as retail's per-widget scissor does. The graphics report's last line had
  run half out of its box and over the frame below.
- Add `ui_menuDissolve` for F.A.K.K.2, off by default: set to 1, its
  full-screen menus fade over what the screen last showed, as its retail
  engine's uncleared screen leaves them. A closing menu hazes white over its
  own picture and the next one comes up through the haze. By default they
  still fade over black.
- Load a save by double-clicking it in F.A.K.K.2's Load/Save list, as
  retail's list control does. Page Up and Page Down now go to the shown
  page's first or last save before paging on, Left and Right step like Up and
  Down, and the mouse wheel scrolls the list.
- Stop saving F.A.K.K.2's `ui_hud` in its configuration: retail keeps the HUD
  switch only for the session. Alice's `ui_subtitles`, `ui_skipDepthClear`,
  `ui_crosshair` and `ui_jumpreticle` are no longer registered for F.A.K.K.2,
  whose engine has none of them.
- Ring 64 reliable commands on F.A.K.K.2's wire, as retail `fakk2.exe` 1.02
  does, where fnTech3 rang Alice's 128: the two titles share protocol 1, so a
  client on it now rings its profile's window. A server drops a client at
  its 65th unacknowledged command, and a retail client on an fnTech3 server
  now gets that drop where it used to fail on its own.
- Bound an F.A.K.K.2 map at 384 inline models, as `fakk2.exe` 1.02 does,
  rather than Alice's 512. No retail map has more than 201.
- Read and write Alice's and F.A.K.K.2's configuration in the file their
  `config` cvar names, as both retail engines do: `+set config julie.cfg`
  keeps its own settings and saves. fnTech3 had followed the cvar for the
  save names alone. Retail kept the name in the registry for later launches;
  fnTech3 keeps it for the launch that names it.
- Indent F.A.K.K.2's menu checkboxes as its retail menus do: the box 16 units
  in and its title 48, where fnTech3 had drawn them by Alice's rule at the
  widget's edge.
- Register `r_useglfog` (default 1) for Alice and F.A.K.K.2, as both retail
  renderers do, so F.A.K.K.2's advanced options menu shows "Use GL Fog" as
  retail's does. fnTech3's renderers draw distance fog their own way and do
  not read it.
- Light the static models Alice and F.A.K.K.2 compile into their maps with
  the colours the map baked on them, in all three renderers. Their shaders
  light by entity (`rgbGen lightingDiffuse`), and the world gives none, so
  F.A.K.K.2's banners, ferns, leaves, lamps and cloth, and Alice's kelp on
  `utemple`, drew black or vanished into the dark behind them.
- Fog Alice's and F.A.K.K.2's far-plane fog as their engines do: an opaque
  surface's stages draw unfogged, then one pass of the fog colour over them,
  thicker with distance. Each stage had been fogged on its own, a texture over
  a lightmap fading toward white, so distant walls on six F.A.K.K.2 maps and
  two of Alice's grew brighter with the fog on than off. F.A.K.K.2's `fog`
  now matches retail's view region for region.
- Keep up to 64 frames of an animated shader stage, as Alice's and
  F.A.K.K.2's engines do. F.A.K.K.2's water caustics have 47, and the last
  23 had been dropped, so they cycled in half the time.
- Draw no fog volume on an Alice or F.A.K.K.2 surface while the far-plane fog
  is on, as their engines do. F.A.K.K.2's `over` and `under` start the camera
  inside a black fog volume, which had halved the tunnel's brightness and
  left Julie nearly black.
- Give an F.A.K.K.2 lightmap stage that names no colour retail's default,
  identity rather than Quake III's identity lighting, so it and the texture
  over it draw as one pass, as retail's do.
- Announce a client leaving an Alice or F.A.K.K.2 server as their engines do,
  `print "%s %s\n"` with no colour code after the name, and send the client
  a bare `disconnect`, where fnTech3 had sent Quake III's forms.
- Use an F.A.K.K.2 inventory item in the hand chosen, as retail does. A
  one-handed weapon used from a hand it does not fit is refused rather than
  going to the other hand, and the commands name the hand as retail's do
  (`use "GreenSword" righthand`).
- Print F.A.K.K.2's centre and located messages and its frame counter in
  `facfont-20`, the font its retail view is built with, rather than the menus'
  `verdana-14`. They now match retail's in glyphs and place.
- Draw Heavy Metal: F.A.K.K.2 with no overbright bits by default, as its
  retail engine does. Most of its shaders' lightmap stages had taken Quake
  III's identity lighting, which each overbright bit halves. Its grass and
  floors drew at seven tenths of retail's brightness, and blood on the ground
  looked darker with them. Its frames now match retail's region for region on
  the GL renderer. Alice draws with none as well; other profiles keep one bit.
- Light the F.A.K.K.2 menu button that has the keys, as retail does. Its
  hover picture stays on the button the mouse last came onto, or the arrow
  keys chose, and each menu opens with the keys on its first entry, or the one
  it last had: the main menu opens with NEW GAME pulsing. fnTech3 had lit a
  button only while the mouse was over it. Alice's buttons are unchanged.
- Draw F.A.K.K.2's inventory as retail `fakk2.exe` 1.02 draws it. Each cell's
  ammunition gauge fills its copper groove from the bottom, under the cell
  art rather than over it; a picked item shows its own selected picture; and
  the column lights its current type whenever the cursor is over it. Weapon
  models no longer vanish where the world behind their cell is nearer, which
  had hidden the light sword: the view kept Alice's `ui_skipDepthClear`,
  which F.A.K.K.2 does not have.
- Send and read F.A.K.K.2's own message bodies, as the official 1.02 patch's
  plain `fakk2.exe` reads them. Its entity table has 73 fields and its own
  31-entry dictionary, and its player state sends `pm_flags` and `gravity` in
  16 bits. fnTech3 had sent Alice's shape, and retail `fakk2.exe` refused
  fnTech3's F.A.K.K.2 demos. It now plays them through.
- Carry the server's time residual in every Ritual snapshot, the byte
  `alice.exe` and `fakk2.exe` both write after the server time and read before
  the delta byte. Without it, a retail reader took the delta byte for the
  residual.
- Show F.A.K.K.2's HUD by its own engine's rule. The one-handed weapon HUD
  shows its rings even with empty hands. The HUD comes up as soon as the
  letterbox goes, without Alice's cinematic check or 550 ms delay.
- Keep F.A.K.K.2's HUD in shape on widescreen displays. Its HUD menus now take
  the HUD correction Quake III Arena and RTCW use: the engine package's
  `fntech3-hud.json` holds the health gauge to the upper left, the weapon
  rings and their models to the upper right and the boss's bar to the left,
  each at its 4:3 proportions, where they had stretched with the screen.
  `cl_hudAspect 0` stretches them again, and `cl_hudDump` and `hud_reload`
  work in F.A.K.K.2 as in those games
  ([Aspect Handling Guide](docs/ASPECT_CORRECTION.md)).
- Keep the HUD and the view of every game written for 4:3 screens in shape on
  widescreen displays. Enemy Territory, both Elite Force modes, Alice and
  F.A.K.K.2 now take the HUD correction Quake III Arena and RTCW use, each
  with stock rules in the engine package, and all of them and Medal of Honor
  widen the world view to keep its 4:3 vertical framing (`r_fovCorrection`).
  Alice's gauges and cards no longer grow on wider screens. Enemy Territory's
  scoreboard and expanded map draw as centered 4:3 screens, and Elite Force
  single player's target brackets stay on their targets. A new `extend` rule
  mode carries a scope's or a letterbox's masks to the screen's edges
  ([Aspect Handling Guide](docs/ASPECT_CORRECTION.md)). Medal of Honor's
  first-person arms, whose unposed bounds stand behind the eye, are no longer
  culled from the widened view, and its model detail and Enemy Territory's
  foliage distance keep the field of view the game gave. Quake Live's own Hor+
  fits only 4:3, 5:4, 16:10 and 16:9 screens and stretched the world sideways
  on any other, by a third at 21:9; `r_fovCorrection` now completes it,
  keeping its vertical view, and its markers follow.
- Keep every part of Quake III Arena's HUD in its place on widescreen and
  tall displays. Item pickup names, votes, team chat, the attacker's name and
  centred lines such as the warmup, ammunition warnings and scoreboard titles
  no longer split or drift from what they belong to; the no-ammo cross stays
  on its weapon, the team overlay and lag meter reach the screen's corners, and
  the status bar and team chat sit on its bottom edge. The scoreboards and the
  loading screen draw as centred 4:3 screens, and the loading screen's
  lettering keeps its shape. HUD rules can now place text by the line and
  match by an edge or a height, and a rule can start a centred screen
  ([Aspect Handling Guide](docs/ASPECT_CORRECTION.md)).
- Draw the sprites of hidden F.A.K.K.2 emitters, as retail does: the smoke
  of `fakkhouse`'s chimneys, and blood splats.
- Let traces hit Ritual entities as retail `alice.exe` does. Each clip now
  builds the entity's clip model at that moment: a brush model's own model,
  or a box from the entity's bounds and contents. Before, a stale shared box
  let F.A.K.K.2's bullets and sword blows pass through monsters, and traces
  pass through doors and platforms. The trace's entity pass also skips what
  retail skips (non-solid entities, the tracer's owner and missiles, contents
  the trace ignores). The link publishes each entity's `solid` packed as
  retail packs it, which clients use to clip. Alice gets the same fixes.
- List the saves in F.A.K.K.2's Load/Save menu. Its save list is drawn as
  retail's list control draws it: headers, then a line a save with its map,
  level time and date, the chosen one highlighted. The keys and a click choose
  the save the buttons load or delete, and its picture shows beside the list.
  The menu had shown an empty box. Alice's menu keeps its list hidden.
- Send a fragment's offset as a long on the Ritual wire, as retail `alice.exe`
  does. A short one cut off every message past 32 KB, so a F.A.K.K.2 client
  could not enter `oracle`, whose gamestate is 40 KB, and a retail Alice peer
  would have misread every fragment. Other profiles keep Quake III's short.
- Connect to Ritual servers, and accept their clients, with their own
  handshake. Under Alice and F.A.K.K.2 the connect is plain text with
  protocol 1, and the server answers with a bare `connectResponse`, as
  `fakk2.exe` 1.02 does. fnTech3's server had dropped a retail client's plain
  connect in a failed Huffman decode, and its own client had asked for
  protocol 68. Retail F.A.K.K.2 now joins an fnTech3 server and plays, and
  fnTech3's client joins over the network too.
- Make F.A.K.K.2's save pictures as its own engine does: in colour, each
  channel averaged. They had been made as Alice's are, grey and brightened,
  which turned F.A.K.K.2's rooms almost white.
- Show F.A.K.K.2's version as `1.02` in its menus, the `shortversion` its
  1.02 engine registers. It had shown Alice's `1.00`.
- Play F.A.K.K.2's own intro: its soundtrack, then the Ritual Entertainment
  and Gathering of Developers plates, each faded in, held and faded out, then
  the main menu. It had gone straight to the menu. BACK TO GAME is offered
  only in a game, as its engine does after the intro and on Escape, and
  Escape no longer waits for the intro to end.

- Add Medal of Honor: Allied Assault as a tenth title and the `mohaa` profile,
  gated behind `-Dmohaa-development=true`. It runs 2015's engine, which
  descends from Ritual's and Quake III's, and is served through adapters of
  its own: the retail 1.00 game and client-game libraries run from the user's
  installation with its own `system86.dll`, their 167 game and 168
  client-game imports mapped from the libraries themselves. All fifty-four
  maps spawn on the dedicated server and load in the client; each of the
  forty-three single-player maps, briefings and credits included, plays its
  opening with its scripts and AI running, and saves, loads and level exits
  work in play. The retail menus, HUD, scoreboard, loading bar and message
  boxes are served by the engine's own UI, and the GL, Vulkan and RTX
  renderers draw the worlds, terrain, TIKI models, far-plane fog and the sun's
  lens flare. Two clients find a dedicated server on the LAN list, join it
  over protocol 6 and play free-for-all, team, round-based and objective
  matches. The retail `MOHAA.exe` is SafeDisc-encrypted and cannot run on
  current Windows, so the engine's rules come from the two plain libraries,
  the game's data and [OpenMoHAA](https://github.com/openmoh/openmohaa)'s
  reconstruction, parts of which are incorporated under its GPL licence, and
  no frame, packet or save has been compared with retail's
  ([record](docs/fntech3/2026-09-24-mohaa-contracts.md)).
- Light Medal of Honor's terrain with its own lightmaps. Every map's terrain
  had drawn unlit, at full brightness: on a night level such as `m1l2a` the
  sand glared white instead of lying dark around the lamps.
- Stop Medal of Honor's screen flashes and fades from dropping the game. Each
  frame they drew registered a new blank picture, and a long battle such as
  the Omaha Beach landing used up the renderer's picture slots within a few
  minutes. The picture is now registered once.
- Serve Medal of Honor's developer file pickers, `viewspawnlist`,
  `lod_spawnlist` and `soundpicker`, which its model viewer, LOD tool and
  sound menus open.
- Admit Medal of Honor clients to pure servers. With `sv_pure 1` the client
  now proves the paks it read from as 2015's engine does, naming no server id
  or module, since 1.00's modules are not in a pak. A pure server had dropped
  every client. `sv_pure` stays off by default, as in the original game, and
  a client can download an archive the server uses and then join.
- Alpha-test multitexture stages in the Vulkan and RTX renderers. Their
  two- and three-bundle shaders were compiled without the alpha test, so a
  stage that alpha-tests one texture and multiplies another in the same pass
  drew its whole quad. Medal of Honor's foliage and fences are such stages,
  and its tree lines showed the far-plane clear where the sky belonged. They
  now match the GL renderer. A stage without an alpha test is unchanged.
- Turn Medal of Honor's view with the mouse. Its client game has no import
  to hand the engine a mouse scale, so the scale stayed at nought and a
  moving mouse turned nothing. The engine now asks the client game for it on
  every move, as the original engine does, and a move counts as activity for
  the server's idle kick.
- Stop filling Medal of Honor's console with `EXEC_NOW ui_removehud
  hud_stopwatch` under `developer 1`. Its client game runs that line every
  frame; the engine no longer echoes such lines for Medal of Honor, Alice or
  F.A.K.K.2, whose own engines never did.
- Forward a command the server does not know, sent by rcon to a listen
  server, whole. It had reached the game short by the length of
  `rcon <password> `.
- Start each Medal of Honor level with only its own sound aliases, on a
  server and on a client of a remote one. Every level's aliases had stayed
  for the rest of the session, so a later level found sounds its data does
  not give it, and its client game's parse printed about 1,900
  duplicate-alias lines under `developer 1`. A `restart`, or a
  load of the running level's save, keeps them, as the original engine does.
  A sound a level plays while it spawns, such as a vehicle's idle, still
  finds no alias: the original game registers them only after the spawn too.
- Draw Medal of Honor's permanent decals, the posters, signs, cracks and
  stains its designers laid on eleven maps, in every renderer. The advanced
  options' "Static Decals" box shows and hides them.
- Light Medal of Honor's bullet holes, scorches and other lit marks. Their
  light was handed to the client game on the wrong scale, and every lit mark
  drew black.
- Draw Medal of Honor's character and prop models at the detail their
  distance calls for, by the level-of-detail records the game ships with
  them. The advanced options' model detail choice now takes effect; the
  renderers had refused to let it set `r_lodscale`.
- Serve Medal of Honor's decal editor and level-of-detail tool, which its
  developer menus open (F6 and `l` in `development.cfg`): with
  `dcl_editmode 1` a level's decals can be laid, changed and saved, and
  `lod_spawn` shows a model at any level of detail, whose record
  `lod_save 1` writes. Both write the original game's own file formats.
- Stop drawing the Medal of Honor terrain its maps take out. A map's terrain
  can leave out triangles, which players walk through and the original game
  never drew; fnTech3 drew them all, up to four in five of a map's terrain
  triangles, and now leaves out the same ones its collision does.
- Leave Medal of Honor's decals on terrain and on doors and other moving
  brushes. Scorches and the decal editor's decals now reach terrain, and marks
  on a door swing with it; the decal editor had re-cut a level's terrain
  decals to nothing. Bullet holes still leave terrain alone, as the original
  game's renderer does; `ter_minMarkRadius 0` lets them mark it too.
- Cast Medal of Honor's "Complex" shadows from the level's lights. The game
  asks which lights reach a character and was told none, so the advanced
  options' Complex shadows fell back to the Simple ones.
- Let Medal of Honor players chat in multiplayer. The game's `say` and
  `sayteam` keys (`y` and `t`) opened nothing, and the chat line that did open
  sent a command the game never reads; they now open the chat line, whose
  words reach everyone, the team or one player as the original game sends
  them.
- Leave Return to Castle Wolfenstein single player's level changes to its
  game, as `WolfSP.exe` does. A `map`, `spmap` or `devmap` typed while a
  briefing is up, while a level's exit fades out or while a failed mission
  reloads now does nothing, where it had started a level under the game's
  own change, and disconnecting clears the game's loading state. Nine level
  changes the game makes itself now have runtime evidence: four exits carry
  the player's weapons, ammunition and armour into the next level, and five
  cinematic levels hand over to the next episode. A death reloads the level's
  last save in place, as the original game does
  ([record](docs/fntech3/2026-10-08-rtcw-sp-level-exits.md)).
- Keep a Q3Test trace's contents when it starts and ends inside a brush, as
  every Q3Test release does; fnTech3 wrote the brush's, as Quake III does.
  With that, 64,800 world traces asked of every release's retail server and
  of fnTech3's agree in every hit, flag and contents
  ([record](docs/fntech3/2026-10-08-q3test-traces.md)).
- Leave a Q3Test entity unlinked when its box reaches only solid leafs, or
  when the game has not marked it in use, as every Q3Test release does;
  fnTech3 linked both. A brush model moved into a wall had answered box
  queries and stopped traces that a retail server never saw
  ([record](docs/fntech3/2026-10-08-q3test-traces.md)).
- Collide in Return to Castle Wolfenstein as its own engines do. Multiplayer
  cuts curves into pieces as `WolfMP.exe` does, so a curve far from the
  middle of a level no longer stops a shot or a player the original server
  lets through, and it finds a player inside a curve as `WolfMP.exe` does.
  Single player treats its players as boxes, as `WolfSP.exe` does, where
  fnTech3 had rounded them into capsules. A trace that starts and ends inside
  a wall or curve keeps the contents it had in both modes, and a shot traced
  at a multiplayer entity flagged as a capsule now stops on it where it had
  passed through. Asked the same questions, the retail servers and fnTech3's
  now agree on every hit, flag and contents of 202,860 traces on fourteen
  maps but for nine that differ only in floating-point precision
  ([record](docs/fntech3/2026-10-08-rtcw-traces.md)).
- Let a Return to Castle Wolfenstein level close a door's area portal that is
  already shut, as both its engines do. fnTech3 stopped the level with
  "negative reference count" where the original game went on
  ([record](docs/fntech3/2026-10-08-rtcw-area-portals.md)).
- Place Return to Castle Wolfenstein entities as its engines do when the
  server asks what is at a point: where the game last sent them, and turned
  as it sent them, as `WolfSP.exe` and `WolfMP.exe` do, where fnTech3 used
  the entity's current position. In single player a sweep from a player no
  longer stops on the object that player carries, as in the original game
  ([record](docs/fntech3/2026-10-08-rtcw-entity-queries.md)).
- Cut Q3Test's curves into pieces as every Q3Test release does, measuring
  each piece within a square reaching 8192 units out from its plane's point
  nearest the level's origin, where fnTech3 used Quake III's 65535. A curve
  far from the origin no longer stops a shot or a player that a retail
  server lets through, and a long one the square clips is kept where retail
  keeps it. No shipped map has such a curve: each release's own curve code,
  run beside fnTech3's, keeps the same pieces on every shipped curve and on
  curves placed where the size decides
  ([record](docs/fntech3/2026-10-09-q3test-facet-windings.md)).

### Builds and Packaging

- Repair release validation across platforms: the Make FreeType probe parses
  correctly, Linux source tests install their 32-bit compiler dependencies,
  and Linux packages include the zlib and FreeType development dependencies.
  Native fixtures compile with GCC, Clang and MinGW, tolerate Windows short
  paths, and account for x87 intermediate precision without changing their
  recorded numeric expectations. Allow the complete source suite to finish
  within the release job's time limit.
  Install the OpenGL headers used by the production shader fixtures as well.
  Detect MinGW's actual 32-bit target when compiling standalone fixtures.
  Large release histories retain their complete packaged changelog while
  GitHub notes show highlights, limitations and working documentation links.

- Keep Enemy Territory's corpse detail floor consistent on Linux i686 and
  Windows: round the vertex-count product to float before truncating it, so a
  100-vertex surface retains 35 vertices at the 35% floor instead of 34.

- Fix a missing botlib header exposed by the Linux i686 build and make four
  compatibility fixtures portable across GCC, Clang and MSVC. Record Linux
  Holomatch server sessions with fnTech3, Lilium Voyager and the retail client.

- Start independent fnTech3 versioning at 0.1.0, with reviewed changelog entries
  and manually requested GitHub draft prereleases built from an existing tag.
- Package Windows x86 builds from MSVC and MinGW, and a Linux x86 build, with
  SHA-256 checksums, source commit identity, notices and compatibility evidence.
- Distribute the engine and project-owned content only; retail game modules,
  proprietary runtimes and game assets must come from the user's installation.
- Show each game's own icon, remade at high resolution, on every platform.
  The client carries an original vector remake of every supported game's
  application icon, from Quake III Arena's red emblem to Alice's portrait,
  and by default uses the active game's as its window, taskbar and Dock icon.
  Most retail executables' icons stop at 32 px (RTCW single player's reaches
  128 and Quake Live's 256) and could only be borrowed on Windows from an
  installed executable; the remakes need nothing installed, and Windows binds
  exact frames for the window's DPI. `cl_gameIcon 2` still prefers the
  installation's own icon and `0` keeps fnTech3's. `fntech3.exe` exposes every game's icon to
  shortcuts, Linux gets hicolor icons and a launcher template per game whose
  application id the window now reports, and each game has a macOS `.icns`
  ([game icons](assets/icons/games/README.md)). The legacy Make build's SDL
  console no longer names an icon that no longer exists.
- Build on Linux with GCC again. The bot library's AAS loader, the
  navigation graph and the Ghoul2 server declarations had used constructs
  MSVC accepts and GCC refuses; the i686 client (meson) and dedicated
  server (make and CMake) now build from an unmodified tree.
- Find game files on Linux file systems with 64-bit inode numbers, such as
  WSL's drvfs mounts, XFS, btrfs and NFS. The Makefile, which builds the
  Linux release, and CMake now enable large-file support as Meson always
  has; without it the 32-bit engine's directory listing stopped with
  `EOVERFLOW`, and a dedicated server reading the Steam Quake III
  installation through `/mnt/c` found no paks
  ([record](docs/fntech3/2026-10-05-make-large-files.md)).
- Build every CMake target for Linux i686, not only the dedicated server
  that CI built. Under CMake the Vulkan and RTX renderers no longer compile
  code written for OpenGL, the bundled libjpeg-turbo takes the build's
  `-m32` instead of coming out 64-bit and stopping the client's link, and a
  Jedi Academy test finds its header; a test that compared a float with a
  literal GCC keeps at x87 precision now passes
  ([record](docs/fntech3/2026-10-05-cmake-i686-full-build.md)).
- Build every CMake target in Linux CI and run its tests. The Linux
  workflow's CMake step built the dedicated server alone; it now builds the
  client, the server, the three renderer modules and the tests, checks that
  each binary is 32-bit x86, and runs `ctest`. Both of the workflow's jobs,
  Meson and Make, also install the i386 zlib development package, without
  which each had stopped at its first build
  ([record](docs/fntech3/2026-10-05-cmake-i686-full-build.md)).

### Documentation and Tooling

- Rewrite the documentation for fnTech3 instead of FnQL. A new
  [documentation index](docs/README.md) replaces `docs/fnql.md`; the
  [Games Guide](docs/GAMES.md) covers choosing a profile, installations,
  writable homes and each game's status; the
  [architecture overview](docs/fntech3/ARCHITECTURE.md) maps profiles, adapters
  and subsystems for maintainers; and the Build Guide lists every game's
  development option. The player guides now say which games each feature
  applies to -- static map lights, fog, coronas, player highlighting, FOV and
  HUD correction, cube-map capture -- and the Audio Guide describes how each
  game's sound is rendered and its retail-exact option. The FnQL documents are
  indexed as the Quake Live profile's reference, upstream history moved to
  `docs/archive/`, and the issue-triage script reads documents that exist.

- Write the player guides for players. A new
  [Getting Started](docs/GETTING_STARTED.md) guide covers installing,
  starting a first game, the console and settings files, and every guide now
  leads with what a feature does and how to use it, explains each setting in
  everyday words, and keeps its technical detail in the technical notes and
  the compatibility ledger. Release packages now include the guides as web
  pages, `manual/index.html`, in the fnTech3 flame's orange and black, with
  search, light and dark themes and copy buttons on every example; they open
  offline in any browser, and `README.html` is redesigned to match and links
  into them. `scripts/player_manual.py` builds the pages from the same
  Markdown, and packaging stops if a guide links to a page or section that
  does not exist ([theme notes](docs/manual/README.md)).

- Generate F.A.K.K.2's delta tables and dictionary from the official 1.02
  patch's `fakk2.exe`, with `scripts/generate_fakk2_wire_tables.py`. Its
  `--check` mode, run by `tests/fakk2_wire_tests.py` when that executable is
  present, compares them with the executable.
- Check a Ritual-engine client's sound from the engine's own capture instead
  of by ear, with `scripts/probe_ritual_audio.py`. It records a demo, replays
  it into `video` with and without the soundtrack, and looks for each effect
  the playback started in the recorded mix, using that effect's own WAV from
  the installation. `docs/fntech3/2026-09-24-fakk2-contracts.md` records what
  it found on F.A.K.K.2's `fakkhouse`.
- Drive a Ritual-engine client's menus and play in automated probes without
  touching the keyboard or mouse. `in_synthesize` queues keys, typed
  characters, mouse motion and console lines in the engine's own event queue.
  It exists only under Alice, F.A.K.K.2 and Medal of Honor, and only a client
  started with `+set in_synthetic 1` accepts it.
  `scripts/probe_ritual_client.py --actions` turns a short script (point,
  click, press, down, up, type, motion, shot) into chained steps. Each key or
  button's command runs before the probe goes on.
- Record the retail entity-type switches of `WolfMP.exe`, `WolfSP.exe` and
  `et.exe`, the host-font gate and the RTX colour-space finding in
  `docs/fntech3/2026-09-23-rtcw-water-ripples.md`. Regression tests run each
  renderer's own ripple code, the renderer's own font start-up for every
  profile, and RTX's scene-linear gate against GLx's.
  `scripts/probe_rtcw_ripple.py` fires into `mp_beach`'s surf through engine
  commands alone on a private desktop. It can run retail `WolfMP.exe` the same
  way with `--retail`.
- Record the key names in `docs/fntech3/2026-09-20-key-names.md`, with the
  retail evidence for each corrected row, and share Enemy Territory's
  key-number table with Return to Castle Wolfenstein's instead of keeping a
  second copy of the same 110 numbers.
- Record why the keypad star stays a character key in
  `docs/fntech3/2026-09-20-keypad-star.md`, with the retail producers it
  matches, a reproducible reading of the Windows keyboard layout from
  `scripts/probe_win32_keypad_star.py`, and the cost of the alternative.
  Two commented-out fragments that read as unfinished work now carry that
  reason, and regression tests hold all three producers to it.
- Keep a probe's record when it has to stop its client. Windows refuses
  `TerminateProcess` for a process whose exit has begun but signals the
  process only once its teardown ends, so the private-desktop helper
  (`tests/windows_private_process.py`) raised `PermissionError` whenever it
  met a client already exiting. `scripts/probe_ritual_client.py` met one
  every time it stopped a client at `--timeout`, since it stopped it twice,
  and lost that run's `manifest.json`. The helper now waits out the teardown
  and fails only if the client is still running, and the probe writes its
  manifest however the client ends, with `stopError` if it could not be
  stopped.

### Known limitations

- Full unified compatibility is not established for any profile. Q3 and QL
  remain subject to the open gates in the
  [compatibility ledger](docs/fntech3/COMPATIBILITY.md).
- Every other profile -- RTCW single player and multiplayer, Enemy Territory,
  Elite Force single player and Holomatch, Jedi Academy and Jedi Outcast in
  both modes, Alice, F.A.K.K.2, Medal of Honor and the IHV Test -- remains
  experimental and disabled in release builds. Development probes need that game's option, such
  as `-Drtcw-development=true`; see the [Build Guide](BUILD.md#game-development-options).
- macOS and additional CPU architectures are outside this release matrix.
