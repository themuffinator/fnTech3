# fnTech3 Changelog

Reviewed, user-visible changes to fnTech3. Add changes under **Unreleased** as
they land; `python scripts/release_workflow.py prepare --version X.Y.Z` freezes
them into a dated entry before tagging. Released entries are retained.

fnTech3 starts at **0.1.0**. FnQuake3 and FnQL versions and release notes are
historical upstream records, not fnTech3 release history. See the
[release guide](docs/fntech3/RELEASING.md) for versioning and publication rules.

## [Unreleased]

### Highlights

- Introduce fnTech3's shared engine distribution and startup game profiles for
  Quake III Arena and Quake Live, based on the documented FnQuake3 and FnQL
  baselines. Supply a legitimate installation of the selected game.

### Compatibility

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

### Builds and Packaging

- Start independent fnTech3 versioning at 0.1.0, with reviewed changelog entries
  and manually requested GitHub draft prereleases built from an existing tag.
- Package Windows x86 builds from MSVC and MinGW, and a Linux x86 build, with
  SHA-256 checksums, source commit identity, notices and compatibility evidence.
- Distribute the engine and project-owned content only; retail game modules,
  proprietary runtimes and game assets must come from the user's installation.

### Documentation and Tooling

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

### Known limitations

- Full unified compatibility is not established. Q3 and QL remain subject to
  the open gates in the [compatibility ledger](docs/fntech3/COMPATIBILITY.md).
- RTCW single-player and multiplayer profiles remain experimental and disabled
  in default release builds. Development probes require `-Drtcw-development=true`.
- macOS and additional CPU architectures are outside this release matrix.
