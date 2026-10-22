# fnTech3 Documentation

fnTech3 is a unified, engine-only source port for id Tech 3 games and the
Ritual engine that grew from it. One distribution -- one client, one dedicated
server and three renderer modules -- aims to run Quake III Arena, Quake Live,
Return to Castle Wolfenstein, Wolfenstein: Enemy Territory, Star Trek Voyager:
Elite Force, Star Wars Jedi Knight: Jedi Academy, Star Wars Jedi Knight II:
Jedi Outcast, American McGee's Alice, Heavy Metal: F.A.K.K.2 and Medal of
Honor: Allied Assault. Each process selects one of sixteen game profiles at
startup and keeps it until it exits.

Every build includes all sixteen profiles. Games beyond Quake III Arena and
Quake Live remain experimental. **No profile has full compatibility
sign-off.** The [compatibility ledger](fntech3/COMPATIBILITY.md) records what
has been shown to work and what has not.

Start with the [README](../README.md) for an overview, then
[Getting Started](GETTING_STARTED.md) and the [Games Guide](GAMES.md) for your
game.

## Player guides

The player guides are written for players, in plain language: what a feature
does, how to turn it on and what to try when something goes wrong. Technical
evidence belongs in the ledger and the records below, not in these guides.
Release packages include them as web pages in their `manual` folder, built by
[`scripts/player_manual.py`](../scripts/player_manual.py) with the theme in
[`docs/manual`](manual/README.md).

- [Getting Started](GETTING_STARTED.md) -- installing fnTech3, starting your
  first game, the console, settings files and where your files are saved.
- [Launcher](LAUNCHER.md) -- the window that finds your games and starts them:
  the game list, adding folders, mods and options, and its settings.
- [Games](GAMES.md) -- every game and mode, what you need, choosing a game,
  playing online, and what works in each game today.
- [Graphics](DISPLAY.md) -- renderers, window and fullscreen, smoother edges,
  brightness, HDR, lighting, shadows, water, fog, glow and motion blur.
- [Widescreen](ASPECT_CORRECTION.md) -- field of view, the HUD, menus and
  movies on wide screens, and HUD layout rules for mod makers.
- [Player highlights](VISUALS.md) -- glows and outlines that make other
  players stand out.
- [Sound](AUDIO.md) -- headphones and surround, 3D sound, sound zones, each
  game's sound, devices and troubleshooting.
- [Console](CONSOLE.md) -- commands, suggestions, copy and paste, on-screen
  messages and chat, and the console's look.
- [Screenshots](SCREENSHOTS.md) -- formats, file names, camera notes,
  watermarks, map previews and cube maps.
- [GLx Renderer](GLX.md) -- the default renderer, fixing picture problems and
  what to include in a report.
- [RTX Renderer](RTX.md) -- ray tracing: what you need, turning it on, tuning
  it and troubleshooting.
- [Entity Editor](ENTITY_EDITOR.md) -- the in-game editor of a map's
  entities, and the `maps/<map>.ent` overrides it saves and every game loads.

## Building, releasing and maintaining

- [Build Guide](../BUILD.md) -- toolchains, Meson options, the developer
  options for each game, Visual Studio Code tasks and launches, Linux builds.
- [Architecture overview](fntech3/ARCHITECTURE.md) -- how profiles, adapters,
  the filesystem, modules, networking, renderers and audio fit together, and
  how evidence is recorded.
- [Compatibility ledger](fntech3/COMPATIBILITY.md) -- the unified goal, every
  profile's gate matrix, evidence levels and release sign-off.
- [Engine package](fntech3/PACKAGES.md) -- `fntech3-pkg.fnz`, its per-profile
  namespaces, lookup rules and shipped sidecars.
- [Releasing](fntech3/RELEASING.md) -- versioning, the changelog and draft
  prereleases.
- [Changelog](../CHANGELOG.md) -- reviewed, user-visible changes.
- [Project mission and contributor rules](../AGENTS.md) -- the invariants every
  change must keep, and the evidence and non-regression rules.
- [Credits](../CREDITS.md) and [third-party notices](../THIRD_PARTY_NOTICES.md).

### Game-specific ledgers and records

- [Medal of Honor shader sweep](fntech3/2026-10-15-mohaa-shader-sweep.md)
  -- every map loaded in developer mode, each shader warning traced to the
  engine or to 1.00's data, and the engine's own fixed.
- [October 14 compatibility round](fntech3/2026-10-14-compatibility-round.md)
  -- Medal of Honor's sound fades and cached model definitions, and Jedi
  Academy's saber glow and lines checked against retail's own vertices.
- [Jedi Academy spawn effect](fntech3/2026-10-15-jka-spawn-effect.md)
  -- the spawn effect's emitter, lights and bolts, measured in retail and
  drawn with fnTech3's lights and polygons.
- [Jedi Academy local play blockers](fntech3/2026-10-15-jka-play-blockers.md)
  -- animation clips that end, and the spawn shield's distortion copy,
  answered as the original engine answers them.
- [Jedi Academy particle dynamics](fntech3/2026-10-15-jka-particle-dynamics.md)
  -- the torch fire on `mp/ffa2`: shader lists, particle motion, curves,
  delays and the `glow` shader keyword, measured in retail.
- [Jedi Academy tails and particle physics](fntech3/2026-10-15-jka-tails-and-physics.md)
  -- sparks' tails, bouncing particles, impact and death effects and spawn
  shapes, measured in retail.
- [Jedi Academy effect sounds](fntech3/2026-10-15-jka-effect-sound.md)
  -- the sounds of the game's effects, including muzzle flashes, measured
  in retail and played through fnTech3's spatial audio.
- [Jedi Academy weapon fire](fntech3/2026-10-15-jka-weapon-fire.md)
  -- Escape, alternate attack, the button word and the sound mute a local
  player needs to fire, measured in retail Jedi Academy and Jedi Outcast.
- [Jedi Academy effect camera shake](fntech3/2026-10-15-jka-camera-shake.md)
  -- the view shake of muzzle flashes and explosions, measured in retail
  and handed to the game's own camera.
- [Jedi Academy effects on an entity](fntech3/2026-10-15-jka-entity-effect.md)
  -- the first-person muzzle flash, played at the weapon's own axis as
  retail plays it.
- [Jedi Academy Ghoul2 trace](fntech3/2026-10-15-jka-g2-trace.md)
  -- shots checked against a body's own triangles and the box that stands
  for it, measured in retail's server and compared trace for trace.
- [Jedi Academy effect decals](fntech3/2026-10-15-jka-decal.md)
  -- the scorch marks an effect leaves on the world, kept and faded as
  retail's renderer keeps them and cut by fnTech3's own mark clipper.
- [View shake scale](fntech3/2026-10-15-view-shake.md)
  -- `cl_viewShake`, the engine's shake strength option, and which games'
  shakes it can reach.
- [Jedi Academy one-shot animations](fntech3/2026-10-15-jka-oneshot-animmap.md)
  -- explosion pictures that play once and stop, measured in retail and
  read by every renderer.
- [Jedi Academy frame-indexed shaders](fntech3/2026-10-15-jka-setanimindex.md)
  -- door lights whose frame the game sets (`RF_SETANIMINDEX`), measured in
  retail and drawn by every renderer.
- [Jedi Academy entity refraction](fntech3/2026-10-15-jka-entity-refraction.md)
  -- the spawn shield's see-through shimmer, measured in retail and drawn
  with fnTech3's liquid refraction in every renderer.
- [October 15 compatibility round](fntech3/2026-10-15-compatibility-round.md)
  -- Jedi Academy's sound sets heard around a point, checked against
  retail's own routines, and Enemy Territory's close of a shut area portal.
- [Jedi Academy siege and sky portals](fntech3/2026-10-15-jka-siege.md)
  -- siege's round record kept from one level to the next, and the scene a
  sky portal draws, each checked against retail.
- [Jedi Academy menus the client game opens](fntech3/2026-10-15-jka-ui-menus.md)
  -- siege's class menu and the client game's other menus, handed to the
  game's own menus by number as retail hands them.
- [The Jedi single-player menus](fntech3/2026-10-16-jedi-sp-menus.md)
  -- Jedi Academy's and Jedi Outcast's single-player menus, which their
  engines run themselves, read and run by fnTech3's engine from each game's
  own files.
- [The Jedi menus, clicked and typed](fntech3/2026-10-17-jedi-menu-input.md)
  -- both Jedi games' single-player menus and multiplayer menu modules
  driven with the engine's synthetic input, and the language list and
  binding prompt that run put right.
- [Jedi Outcast's client Ghoul2 and HUD](fntech3/2026-10-16-jk2-cgame-ghoul2.md)
  -- Outcast multiplayer's client Ghoul2 imports and models, its engine
  string packages and its HUD placed on wide screens.
- [Jedi Academy's skinless-model query](fntech3/2026-10-16-jka-skinless-model.md)
  -- whether a general entity's model needs its folder's default skin,
  answered from the client's model store as retail answers it.
- [Jedi Academy's trace with pass entity -1](fntech3/2026-10-16-jka-trace-pass-none.md)
  -- what retail reads for the game's "ignore nothing" pass entity, and
  why fnTech3 reads it as no pass entity, checked live on a siege round.
- [Jedi multiplayer broadcasts and level time](fntech3/2026-10-15-jedi-broadcast-level-time.md)
  -- which entities the Jedi servers send each player, permanent ones and
  broadcast masks included, and the level time a map change keeps, checked
  against retail's servers.
- [The level clock across a map change](fntech3/2026-10-15-level-clock.md)
  -- whether a new level carries on from the last one's time in Quake III,
  Quake Live, RTCW, Enemy Territory and Holomatch, checked against retail's
  servers.
- [Portals seen through portals](fntech3/2026-10-15-snapshot-portal-recursion.md)
  -- what a server sends through a portal seen through another portal in
  every game, and RTCW's scripted cameras, checked against retail's servers.
- [The Jedi single-player snapshot walk](fntech3/2026-10-16-jedi-sp-snapshot-walk.md)
  -- which entities Jedi Academy's and Jedi Outcast's single-player servers
  send, Force Sight's included, read from the original engines.
- [Small gaps from the October 15 census](fntech3/2026-10-15-census-small-gaps.md)
  -- Enemy Territory's tactical search queues and update records, and the
  pure gamestate budget of Enemy Territory and Jedi Outcast, read from the
  retail executables.
- [Enemy Territory hide search](fntech3/2026-10-15-wolfet-hide-search.md)
  -- how a bot finds cover from an enemy, read from the retail server and
  rebuilt step for step.
- [Pure gamestate budget of Medal of Honor, the IHV Test and Q3Test](fntech3/2026-10-15-pure-budget-own-wires.md)
  -- Medal of Honor's pak-list budget measured on its own gamestate, sounds
  included, and why the IHV Test and Q3Test have none.
- [Jedi HUD and menus on any screen shape](fntech3/2026-10-15-jedi-widescreen-hud.md)
  -- Raven's virtual 640x480 screen, read in both retail executables, and
  Jedi Academy's widescreen HUD layout.
- [Raven's sun shader keyword](fntech3/2026-10-15-raven-sun-keyword.md)
  -- the `sun` line of both Jedi games' sky shaders, read as their engines
  read it, which ends the grey skies of maps such as `mp/duel9`.
- [Jedi Outcast's one-shot and clamped animations](fntech3/2026-10-15-jk2-animmap-variants.md)
  -- Raven's `oneshotanimMap` and `clampanimMap`, read in both Outcast
  engines, which its explosions and campaign doors use.
- [Raven's surface sprites](fntech3/2026-10-15-jedi-surface-sprites.md)
  -- the `surfaceSprites` stages of both Jedi games, read as their engines
  read them and drawn as grass and splashes over the ground.
- [Raven's alphaFunc GE192](fntech3/2026-10-16-jedi-alphafunc-ge192.md)
  -- the fourth alpha test of both Jedi games' shaders, read as their
  engines read it, which cuts the grass blades' edges cleanly.
- [Raven's map shader flags](fntech3/2026-10-15-raven-bsp-shader-flags.md)
  -- the surface and content bits in both Jedi games' maps, read with
  Raven's values, so RTX finds each map's real sky and its sun.
- [Map shader flags of Ritual's and 2015's maps](fntech3/2026-10-15-map-shader-flag-dialects.md)
  -- the surface flags each engine's map compiler writes, compared, and
  Alice's, F.A.K.K.2's and Medal of Honor's read with their own values.
- [Medal of Honor's worldspawn sun](fntech3/2026-10-16-mohaa-worldspawn-sun.md)
  -- what 2015's sun and sky keys mean, read from the game and its maps'
  lightmaps, and RTX lighting Medal of Honor's maps with them.
- [Raven's light styles](fntech3/2026-10-15-raven-light-styles.md)
  -- how both Jedi games add a map's switchable and flickering lights, and
  how every installed map now loads with them.
- [Raven's light styles, animated](fntech3/2026-10-16-raven-light-styles-animated.md)
  -- the client game's flickering and switched lights reaching all three
  renderers, and where the four Jedi engines keep their colours.
- [Jedi Academy's selection bars and loading screen, live](fntech3/2026-10-15-jka-hud-live-check.md)
  -- the bars and the loading screen checked in captures, each bar's name
  kept with its bar on tall screens, and why the zoom masks are out of reach.
- [Jedi Academy's zoom](fntech3/2026-10-15-jka-zoom.md)
  -- the turned pictures and ranged fog the zooms need, measured in
  `jamp.exe` and served through Fn's HUD and fog, with both zooms captured.
- [Jedi Academy's skin-gore clear on respawn](fntech3/2026-10-16-jka-clear-skin-gore.md)
  -- client-game import 315 measured in `jamp.exe` and served, so a
  respawn no longer stops the client, with free-for-all and siege runs.
- [Jedi Academy's surface switch](fntech3/2026-10-16-jka-surface-on-off.md)
  -- client-game import 321 measured in `jamp.exe`, the call site that runs
  in a siege match found, and the import served through Fn's skin binding.
- [Jedi Academy's Ghoul2 collision and surface names](fntech3/2026-10-22-jka-g2-collision.md)
  -- the game's and client game's collision tests and the struck surface's
  name, measured in `jamp.exe` and compared call for call, and the path
  tracer's frozen Ghoul2 poses.
- [Widescreen field of view in every game](fntech3/2026-10-22-fov-correction-all-profiles.md)
  -- `r_fovCorrection` in all sixteen profiles, Jedi Outcast's stretched
  view and the 1999 tests included, keeping the game's underwater sway.
- [FnQ shadows for every model format](fntech3/2026-10-22-fnq-shadow-formats.md)
  -- dynamic-light and sun shadows for Enemy Territory's and the TIKI games'
  models, and the sun Alice, F.A.K.K.2 and Medal of Honor name in their
  levels, with before and after captures in each game.
- [October 9 compatibility integration](fntech3/2026-10-09-compatibility-integration.md)
  -- resumed Alice and Elite Force work, shared-tree validation, and probe
  evidence and limits for the combined compatibility slices.
- [October 9 Elite Force single-player retail views](fntech3/2026-10-09-stvef-sp-retail-views.md)
  -- eighty synchronized runs across eight scenes, with presentation controls
  and measured limits for GLx, Vulkan and RTX raster SDR.
- [October 8 Medal of Honor GameSpy LAN search](fntech3/2026-10-08-mohaa-gamespy-lan-search.md)
  -- the Join LAN Game list's GameSpy search beside its `getinfo` scan, and
  the query sockets on Linux.
- [October 6 Medal of Honor GameSpy LAN queries](fntech3/2026-10-06-mohaa-gamespy-lan-queries.md)
  -- the GameSpy query port beside the game's, and listen servers' team matches.
- [October 8 MP3 last frame](fntech3/2026-10-08-mp3-last-frame.md)
  -- the last frame before stray bytes, decoded as Raven's engines and Miles
  decode it, and every MP3 of the six titles that use them, before and after.
- [October 6 Jedi listen servers and game types](fntech3/2026-10-06-jedi-listen-server-game-types.md)
  -- each Jedi game's single-player number, `spmap` and `map`, and which
  game types keep a listen server from remote players.
- [October 6 Quake Live listen servers and their server type](fntech3/2026-10-06-ql-listen-server-type.md)
  -- retail's offline, LAN and Internet admission, which replaces the game
  type and the single-player flag for a match started from the client.
- [October 6 Jedi Academy ambient sets](fntech3/2026-10-06-jka-ambient-sets.md)
  -- retail's ambient-set player, its fades, draws and loop mixing, through
  Fn's loop voices and one-shots, and the full session against retail
  `jamp.exe`.
- [October 6 Jedi Academy entity tint and forced alpha](fntech3/2026-10-06-jka-entity-tint.md)
  -- retail's stage colour, alpha and draw order for the client game's tinted
  and fading entities in Fn's renderers, the decal clear, and `map_restart`,
  reconnect and quit against retail `jamp.exe`.
- [October 6 Jedi Academy Ghoul2 attachments](fntech3/2026-10-06-jka-attachments.md)
  -- models posed from another model's bolt in the client game, retail's tag
  frame, and the next boundary against retail `jamp.exe`.
- [October 6 Jedi server Ghoul2 attachments](fntech3/2026-10-06-jedi-server-attachments.md)
  -- Jedi Academy's server answering bolt queries on held sabers and weapons
  as retail's server does, and what Jedi Outcast's engine does instead.
- [October 7 Jedi Academy server pose clock](fntech3/2026-10-07-jka-server-pose-clock.md)
  -- the clock Jedi Academy's server poses bolt queries at, animation
  transitions on held sabers and weapons, and the fallback a failed query
  writes, measured in retail's server and served.
- [October 7 Jedi Academy server bone animation query](fntech3/2026-10-07-jka-server-bone-anim.md)
  -- what Jedi Academy's server answers when the game asks which animation a
  bone is playing, measured in retail's server and served.
- [October 9 Jedi Academy German, French and Spanish voices](fntech3/2026-10-09-jka-foreign-voices.md)
  -- `s_language` choosing Academy's German, French and Spanish voice
  recordings, with the English one where a language has none, as both of its
  engines choose.
- [October 8 menu sounds as interface audio](fntech3/2026-10-08-ui-sound-policy.md)
  -- the channels every game's menus play their sounds on, and Fn playing
  them all as interface audio, outside the level, whatever the channel.
- [October 8 Jedi Academy UI local sound](fntech3/2026-10-08-jka-ui-local-sound.md)
  -- menu sounds (UI import 33) through the client game's channel adapter
  and Fn's local-sound path, reached without input from the UI's own
  `ui_openmenu`.
- [October 8 Jedi Outcast UI local sound](fntech3/2026-10-08-jk2-ui-local-sound.md)
  -- Outcast's menu sounds (UI import 32) as interface audio on Quake III's
  channel numbers, reached without input, and its MP3 sounds that fnTech3
  does not decode yet.
- [October 8 Jedi Outcast MP3 sounds](fntech3/2026-10-08-jk2-mp3-sounds.md)
  -- Outcast's sounds and music, stored as MP3, loading and playing, each
  sound's `.wav` read before its `.mp3` as both of its engines read them.
- [October 8 Jedi Outcast German and French voices](fntech3/2026-10-08-jk2-foreign-voices.md)
  -- `s_language` choosing Outcast's German and French voice recordings, with
  the English one where a language has none, as both of its engines choose.
- [October 6 Jedi Academy live join](fntech3/2026-10-06-jka-live-join.md)
  -- client-game local sounds, the map-change export and the next boundary
  against retail `jamp.exe`.
- [October 5 legacy-server hosting](fntech3/2026-10-05-legacy-server-hosting.md)
  -- masters, ports, info replies, listen servers, ET web downloads and live
  sessions with each multiplayer game's retail peers.
- [October 2 all-game compatibility round](fntech3/2026-10-02-all-games-round.md)
  -- all-profile file-list boundaries, stricter identity probes and shared-binary startup checks.
- [October 2 input and launch fixes](fntech3/2026-10-02-input-launch-fixes.md)
  -- ET's jump-sound placeholder, Elite Force input contracts and all-profile VS Code coverage.
- [October 2 UberTools compatibility round](fntech3/2026-10-02-ubertools-round.md)
  -- Alice, F.A.K.K.2 and MOHAA module, wire, lighting and runtime checks.
- [Quake III point releases, Q3Test and demos](fntech3/Q3_POINT_RELEASES.md)
- [Quake III IHV Test](fntech3/Q3_IHV.md)
- [Quake III Q3Test 1.02 to 1.08](fntech3/Q3TEST.md)
- [RTX renderer and Q2RTX comparison](fntech3/RTX_Q2RTX_PARITY.md)
- The dated records in `docs/fntech3/` hold each change's evidence: probes,
  retail observations, fixtures and results. The ledger links each one from
  the gate it affects. The foundation records for the later games are the
  [RTCW contracts](fntech3/2026-09-14-rtcw-contracts.md),
  [ET contracts](fntech3/2026-09-17-wolfet-contracts.md),
  [Elite Force contracts](fntech3/2026-09-18-stvef-contracts.md),
  [Jedi Academy contracts](fntech3/2026-09-21-jka-contracts.md),
  [Academy protocol and SP bootstrap](fntech3/2026-10-02-jka-compatibility.md),
  [Jedi Outcast contracts](fntech3/2026-09-22-jk2-contracts.md),
  [Outcast protocol 15](fntech3/2026-10-02-jk2-protocol-15.md),
  [Outcast client and fonts](fntech3/2026-10-02-jk2-client-boundary.md),
  [Outcast SP bootstrap](fntech3/2026-10-02-jk2-sp-bootstrap.md),
  [Jedi compatibility audit](fntech3/2026-10-07-jedi-compatibility-audit.md),
  [Jedi audit, second round](fntech3/2026-10-07-jedi-compatibility-round2.md),
  [Alice contracts](fntech3/2026-09-22-alice-contracts.md),
  [F.A.K.K.2 contracts](fntech3/2026-09-24-fakk2-contracts.md) and
  [Medal of Honor contracts](fntech3/2026-09-24-mohaa-contracts.md).

### Tools and data formats

- [Audio-zone compiler](../code/tools/audiozones/README.md) --
  `fnql-audiozonesc`, which builds the `.azb` sidecars from Quake III, Quake
  Live, RTCW, Enemy Territory, Alice and F.A.K.K.2 maps.
- [Global fog sidecars](fnql/GLOBAL_FOG.md) -- the optional per-map `.fog`
  format.
- [Userinfo filters](filter.txt) -- the `sv_filter` file format for server
  administrators.
- [Server firewall examples](firewall/fw-q3host.sh) -- rate-limiting rules for
  a server port, with an [OpenWrt variant](firewall/fw-openwrt.txt).
- [Quake III URL handler](q3url_add.cmd) -- registers `q3a://` links that
  launch fnTech3's Quake III profile on Windows, reading the Quake III
  installation named as its argument and writing to a home folder of their
  own; [`q3url_rem.cmd`](q3url_rem.cmd) removes it.

## Inherited documentation

fnTech3 merges two engine lines, FnQuake3 and FnQL, which descend from
Quake3e, ioquake3 and id Software's release. Their documentation is kept for
reference. It describes the lineage, not fnTech3's current scope, and it never
establishes compatibility for a profile.

- [FnQL baseline documents](fnql/README.md) -- the Quake Live engine
  contracts (WebUI, Steam, factories, input, fonts, cvars) that remain the
  reference for the `quakelive` profile, the shared renderer and audio design
  notes, and FnQL's historical release records.
- [Upstream archive](archive/README.md) -- the Quake3e engine reference and
  change history, id Software's Q3Test and Linux readmes, and other historical
  material.
- [FnQL design reviews and plans](fnql/plans/README.md) -- the reviews and
  plans from FnQL's renderer and audio work, kept as history.
