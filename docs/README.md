# fnTech3 Documentation

fnTech3 is a unified, engine-only source port for id Tech 3 games and the
Ritual engine that grew from it. One distribution -- one client, one dedicated
server and three renderer modules -- aims to run Quake III Arena, Quake Live,
Return to Castle Wolfenstein, Wolfenstein: Enemy Territory, Star Trek Voyager:
Elite Force, Star Wars Jedi Knight: Jedi Academy, Star Wars Jedi Knight II:
Jedi Outcast, American McGee's Alice, Heavy Metal: F.A.K.K.2 and Medal of
Honor: Allied Assault. Each process selects one of sixteen game profiles at
startup and keeps it until it exits.

Quake III Arena and Quake Live start in every build; the other games need a
developer build and remain experimental. **No profile has full compatibility
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

- [October 6 Medal of Honor GameSpy LAN queries](fntech3/2026-10-06-mohaa-gamespy-lan-queries.md)
  -- the GameSpy query port beside the game's, and listen servers' team matches.
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
