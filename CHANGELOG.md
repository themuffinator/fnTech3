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
- Lay the logo that retail Return to Castle Wolfenstein single player and
  Enemy Territory lay over their console. Once the console is half open,
  `WolfSP.exe` draws `console2` over its `console` background, and `et.exe`
  draws `console2-16bit` over `console-16bit`, where `WolfMP.exe` draws its
  own logo. This client drew only RTCW multiplayer's. Single player and Enemy
  Territory now register theirs, and the textured style,
  `con_backgroundStyle 0`, draws them as it draws multiplayer's, keeping the
  logo's shape on any screen
  ([record](docs/fntech3/2026-09-23-wolfenstein-console-logo.md)).
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
- Name keys as both retail key tables name them. The keypad star is `KP_STAR`
  again, so a retail config line that binds it loads and is written back
  instead of being refused and lost, and keys 130 and 157-159 are written as
  `0x82` and `0x9d`-`0x9f` rather than `POWER` and `F13`-`F15`, so a config
  this engine writes loads in retail Quake III and Quake Live. Those four
  names are still read, so configs written by earlier builds keep working. The
  keypad star itself still reports `*`, as retail Quake III, Return to Castle
  Wolfenstein and Enemy Territory all report it on Windows and X11, so a
  `KP_STAR` binding does not fire and an existing `bind *` keeps working.

### Builds and Packaging

- Start independent fnTech3 versioning at 0.1.0, with reviewed changelog entries
  and manually requested GitHub draft prereleases built from an existing tag.
- Package Windows x86 builds from MSVC and MinGW, and a Linux x86 build, with
  SHA-256 checksums, source commit identity, notices and compatibility evidence.
- Distribute the engine and project-owned content only; retail game modules,
  proprietary runtimes and game assets must come from the user's installation.

### Documentation and Tooling

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
