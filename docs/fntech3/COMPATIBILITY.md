# fnTech3 compatibility ledger

All sixteen profiles are available in every build as of 2026-10-11. Experimental
status describes incomplete compatibility, not a startup restriction. Legacy
development options no longer affect availability. Earlier dated observations
below retain the build commands and gating policy used when they were recorded;
those historical restrictions are superseded by the
[availability policy](2026-10-11-profile-availability.md). Native-module platform
requirements and every incomplete compatibility gate remain in force.

## Unified goal and supported-game scope

fnTech3's goal is one modern id Tech 3 engine distribution and shared executable
set for ten games and sixteen immutable profiles. Shared renderer, audio, input,
platform, and tooling improvements must preserve each game's own contracts.
Full compatibility is **not yet established for any profile**.

The [0.1.0 portability follow-up](2026-10-11-release-portability.md) records
unit fixes for shared UI bounds, Jedi and IHV rounding, SP allocation
alignment, RTCW prediction/skin validation, RTX light-byte parity and MinGW
installation/module path containment found by cross-platform release checks.
It closes no runtime gate.

| Game / mode | Startup profile | Current availability |
| --- | --- | --- |
| Quake III Arena | `q3` / `quake3` | Enabled; partial build, fixture, live, and interoperability evidence; required gates remain open. |
| Quake III Arena — IHV Test | `q3-ihv` | Experimental; available in every build. Recorded Windows x86 dedicated native-game probes, and a developer client that plays the supplied demo through the original client game on all three renderers and joins the original dedicated server over protocol 33; its dedicated server takes the original client and fnTech3's own over protocol 33. |
| Quake III Arena — Q3Test 1.02 to 1.08 | `q3test` | Experimental; available in every build. Windows x86 probes run the client and server with user-supplied retail modules against retail peers in both directions; see the [Q3Test gate matrix](#q3test-gate-matrix). Required gates remain open. |
| Quake Live | `ql` / `quakelive` | Enabled; retail client boundary is Windows x86; required gates remain open. |
| Return to Castle Wolfenstein — single player | `rtcw-sp` | Experimental; available in every build; campaign and broader compatibility gates remain open. |
| Return to Castle Wolfenstein — multiplayer | `rtcw-mp` | Experimental; same executable as SP; multiplayer and broader compatibility gates remain open. |
| Wolfenstein: Enemy Territory | `wolfet` / `et` | Experimental; the client and server are available in every build; compatibility gates remain open. |
| Star Trek Voyager: Elite Force — single player | `stvef-sp` | Experimental; available in every build; the retail game module runs every single-player map on a developer server, and the retail menus and cgame run in a developer client, which plays the levels with their ambient sound at retail's own distances, saves them and moves between them through the whole campaign to its closing movie and credits; synchronized image parity and the listening and device checks remain open. |
| Star Trek Voyager: Elite Force — Holomatch | `stvef-mp` | Experimental; same executable as SP; the retail QVMs run, the retail demos play to completion with bounded retail image comparisons, and fnTech3 plays with and downloads from Lilium Voyager and the retail game as client and server in a developer build; part of the renderer gate remains open. |
| Star Wars Jedi Knight: Jedi Academy — single player | `jka-sp` | Experimental; available in every build, with Windows x86 retail modules. The measured retail API-8 bootstrap returns from module Init and stops in the first frame at weather import 128; separate entity/world adapters have unit evidence. Client, campaign, wire, rendering and savegame gates remain open. |
| Star Wars Jedi Knight: Jedi Academy — multiplayer | `jka-mp` | Experimental; same executable as SP. Earlier developer-server evidence runs the retail module and bots. Protocol 26 now has separate codecs, profile-aware transport, reference packet parity and fragmentation tests. Retail client admission is under investigation; demos, rendering, audio/input and bidirectional interoperability remain open. |
| Star Wars Jedi Knight II: Jedi Outcast — single player | `jk2-sp` | Experimental; available in every build, subject to native-module platform requirements. The retail API 6 `GetGameAPI` table now loads through a checked bootstrap; an engine map probe stops at unsupported SP client import 3 before gameplay. Campaign, savegame, wire, client and renderer gates remain open. |
| Star Wars Jedi Knight II: Jedi Outcast — multiplayer | `jk2-mp` | Experimental; same executable as single player. Earlier developer-server evidence runs the retail `vm/jk2mpgame.qvm` with two bots. Protocol 15 now has separate message codecs, transport/handshake selection and tested frozen client transfers. Client/UI import and authored-font adapters are partial; live interoperability, demos, rendering, audio and input remain open. |
| American McGee's Alice | `alice` | Experimental; available in every build, subject to native-module platform requirements. This is a Ritual F.A.K.K.2 engine rather than a Quake III one. In a developer client the retail game module and client game run with 110 of 111 and 119 of 125 imports served, every unserved slot being one the module never calls. The intro's logos lead to the engine's own `.urc` menus, New Game plays `opening.roq` into `gvillage`, and `garden1` draws Alice in third person with her HUD -- on the GL, Vulkan and RTX renderers alike -- saves and loads back, and records and plays back a demo. No retail client, server, save or demo has been exchanged with fnTech3 in either direction and no frame compared with retail's, so every gate stays partial and complete compatibility remains unestablished. |
| Heavy Metal: F.A.K.K.2 | `fakk2` | Experimental; available in every build, subject to native-module platform requirements. Ritual's own engine, which Alice's descends from, served through Alice's adapters with F.A.K.K.2's records crossing at the module boundary. The disc's `fakk2.exe` is encrypted. The official 1.02 patch's is plain: it was read, and it runs here as a client; its server requires the CD check, satisfied in the recorded first-level probe with an image of the user's disc. The evidence is that executable, the plain retail modules, `alice.exe` and the data. Retail `fakk2.exe` plays fnTech3's demos and draws the same frames. The comparison found three differences, all now corrected: the message bodies and a snapshot byte (Alice's too), the HUD's rule, and emitter sprites. Once the handshake was corrected, retail's client joined an fnTech3 server over loopback UDP. Its movement, turns, fire buttons and commands played there, and fnTech3 plays the demo it recorded there exactly as retail does. A developer server spawns all thirty maps through the retail game module with 105 of 105 imports served. A developer client runs the retail client game with 120 of the 123 imports it could call served (the other three have no call site) and enters all thirty maps. It draws the retail menus, HUD and inventory, and takes New Game's path and the level exits with the player's state carried. It saves, lists and loads games, and records and plays back a demo. English and German installations both run. Driven by engine-queued keys and mouse, fnTech3's menus, their pulldown, text selection, save list, hover and click sounds, the inventory's cursor mode, and play with movement and fire all work. With that disc image mounted for retail's CD check, fnTech3's client joined retail's dedicated server on `fakkhouse`, and opening and sword-equipped saves loaded in both engines in both directions ([contracts record](2026-09-24-fakk2-contracts.md#open), 2026-09-25). These are bounded first-level sessions; later-level reverse sessions and save exchange remain open. |
| Medal of Honor: Allied Assault | `mohaa` | Experimental; available in every build, subject to native-module platform requirements. 2015's engine, served by adapters of its own. The retail `MOHAA.exe` is SafeDisc-encrypted and cannot run on this machine, so the evidence is the two plain retail libraries, the data and OpenMoHAA's reconstruction. A developer server spawns all fifty-four maps through the retail game module with 167 of 167 imports served; a developer client runs the retail client game with 149 of 168 served, none of the rest having been called, and enters every map. It draws the engine's 179 retail menus and the HUD, plays every single-player map, briefing and the credits through their scripts with their music, dialogue and loading bar, carries the player through a level exit, saves, lists, deletes and loads games from the Load/Save menu, records and plays back demos, and plays free-for-all, team, round-based and objective matches over UDP, two clients fighting and scoring, on pure servers too, downloading a missing archive over UDP or HTTP. Only fnTech3's own client and server have met; no retail peer, save or demo exists to compare with, so every gate stays partial and complete compatibility remains unestablished. |

RTCW is one title with two mode profiles. A mode change relaunches the same
executable; it does not mutate the running process's profile. Catalog
registration, a development option, or a partial runtime result does not
establish supported release compatibility. The [Q3/QL matrix](#compatibility-and-gate-matrix)
and [RTCW matrix](#rtcw-gate-matrix), plus the [ET matrix](#wolfet-gate-matrix), the
[Elite Force matrix](#elite-force-gate-matrix), the
[Jedi Academy matrix](#jedi-academy-gate-matrix), the
[Jedi Outcast matrix](#jedi-outcast-gate-matrix), the
[Alice matrix](#alice-gate-matrix), the
[F.A.K.K.2 matrix](#fakk2-gate-matrix) and the
[Medal of Honor matrix](#medal-of-honor-gate-matrix), define the current
unified scope.
Elite Force is one title with two mode profiles, as RTCW is, and so are Jedi
Academy and Jedi Outcast.
Dedicated-server and online-service gates apply according to each profile's
contract; RTCW SP additionally requires campaign, AI, and save/load evidence.

## Status

The [pure budget record](2026-10-15-pure-budget-own-wires.md) settles the last
profiles whose pure gamestate budget measured Quake Live's message. Medal
of Honor's measures its own 48 KiB gamestate: its own baselines, and the 364
to 381 sounds its multiplayer game registers once the level has spawned,
which make every stock map's gamestate bigger than Quake Live's whole
message; the pak list is measured again once they are in. No IHV Test or
Q3Test engine has a pure mode, and their clients' systeminfo never carries
a pak list, so `sv_pure` publishes nothing there and says so. No
compatibility gate closes.

The [October 14 compatibility round](2026-10-14-compatibility-round.md)
makes Medal of Honor's engine take the game's `fadesound` and fade the whole
mix as the game asks, and makes its loading bar find cached model
definitions. It serves Jedi Academy's saber glow, line and oriented line as
retail `jamp.exe` draws them, matching retail's own vertices, and Academy's
native-server demo now plays to its end. It also corrects stale
development-option text. No compatibility gate closes.

The [October 11 RTX entity-shadow record](2026-10-11-rtx-entity-shadows.md)
separates direct-shadow history, keeps BODY poses current, adds geometry refits
and indirect-light budgets, and moves optional GPU pacing before input polling.
Q3 engine captures show continuous consecutive-frame BODY shadow movement on
compute and hardware, with bounded timing and fallback evidence. Other profiles
have shared source/affected-test coverage; no compatibility gate closes.

The [October 11 RTX liquid record](2026-10-11-rtx-liquids.md) replaces
screen-space liquids with traced optical interfaces in native RT. Quake III
`q3dm12` supplies hardware and Win32 compute observations at the requested
camera, with raster fallback and underwater checks. Other profiles have
shared source coverage only; this does not close their renderer gates.

The [October 11 MOHAA RTX world record](2026-10-11-mohaa-rtx-world.md)
restores generated terrain and static scenery to path tracing, including
maps with scenery but no terrain. Engine captures and a diagnostic ray
confirm the repaired ground on `m1l1`, with additional scene probes on
`m1l3a` and `m1l3c`. A separate 32-bit BVH allocation limit at maximum curve
detail remains recorded; the renderer gate stays partial.

The [October 11 MOHAA effects record](2026-10-11-mohaa-effects.md) corrects
the client boundary's rejection of visible particles and debris that
inherit their hidden carrier's render flags. It adds regression coverage
and a retail-module fixture for `m1l1`'s two gas barrels, related explosions
and continuous emitters. This is bounded visual evidence, not full retail
effect parity, and the renderer gate remains partial.

The [October 9 integration record](2026-10-09-compatibility-integration.md)
collects validation of the recovered Alice, Jedi, audio and MOHAA slices,
finishes the Elite Force static-view evidence, and records the exact limits
of the fresh probes. These checks do not close a full compatibility gate.

The [2026-10-05 entity editor record](2026-10-05-entity-editor.md) adds
`maps/<map>.ent` entity overrides to every profile (`sv_entityOverrides`, on
by default) and an in-game entity editor that writes them. Each consumer --
the game module through each profile's own path, the botlib, and a listen
server's client game -- reads the chosen text; Quake Live's retail
`sv_altEntDir` still comes first, unchecked. Without a `.ent` file every
profile hands its module the BSP's text exactly as before, and
`sv_entityOverrides 0` restores that whatever the files. The reader matches
the engine's `COM_ParseExt` over seeded random text (Unit), and reads all
607 installed maps of twelve titles, writing each lump back byte for byte
(Fixture). Live, on local listen servers: the override loaded on both of a
run's restarts and the editor, drawn with Dear ImGui, ran end to end on Q3
(GLx, Vulkan, RTX), QL, ET, RTCW MP and SP, Elite Force Holomatch and single
player, Alice, F.A.K.K.2, Medal of Honor and Q3Test 1.08, Q3's game
spawned an item only the override holds, ET's client game drew snow only an
override's worldspawn carried, and RTCW MP's registered a sound only one
named. The same Q3 flow passed on a Linux i686 client under Xvfb; the
panels were driven once by the engine's own synthetic input, with the user's
permission for that run; and Apply reaches another server over rcon, whose
reply names the file it loaded, as a Q3 and a Jedi Academy MP client showed
against fnTech3 dedicated servers. A `map_restart` after the entity list
changed hung RTCW SP's retail cgame in an animation loop with a zeroed
`frameLerp` (3 of 6 runs), so Apply in the Quake III line loads the map again
whenever the entities to spawn change (14 of 14 repeated runs since). On
dedicated servers the IHV Test's, Jedi Outcast MP's and both Jedi Academy
modes' own game modules spawned an override's marker entity, and did not
with `sv_entityOverrides 0`, as did Q3's QVM on a Linux i686 dedicated
server; Jedi Outcast SP stops before its module initialises and has Source
and Unit evidence only. No profile gate changes.

The [2026-10-05 legacy-server record](2026-10-05-legacy-server-hosting.md) checks, for every
multiplayer profile, hosting the original game's clients and joining its
servers. Each profile now publishes to its retail engine's default master
with that engine's heartbeat and shutdown, Jedi Academy and Jedi Outcast
listen on their own ports (29070, 28070) and masters (29060, 28060), info
replies carry each retail engine's extra keys -- without RTCW's `gamename`,
retail `WolfMP.exe` had listed no fnTech3 server -- and fnTech3's Elite Force
browser reads Holomatch's one-line info replies from Raven's master port,
where it had dropped every reply and asked Quake III's. A server started
from the client no longer refuses remote players; a server's own
`sv_maxclients` sizes its first client array, so RTCW's and ET's first
warm-up no longer reloads the map; joining a mod server keeps the console
log; the ET client follows a server's web download redirect; Raven's servers
take their `svsay`; a client that connects again to a pure server no longer
names in its proof a pak only it has; and `sv_minPing`/`sv_maxPing` work as
retail's (Unit). Live on loopback (Interop subsets): discovery against fake
masters for HEAD, the fixed build and every retail server that runs here;
scripted sessions -- join, chat both ways, map change, `map_restart`,
reconnect, server quit, kick, a UDP download inside a mod, and a retail and
an fnTech3 client on one server -- pass with retail Quake III, RTCW, ET and
Elite Force in both directions, on fnTech3 listen servers and as retail
controls, and fnTech3's client joins the retail Jedi Academy server until an
unfinished client-game import; ET web downloads pass both ways with retail,
including a 404's fallback to UDP and a client leaving to fetch. Real
networks, sustained play, Quake Live, Medal of Honor (no runnable retail
peer; its GameSpy queries are taken up below), Jedi Outcast's
client and the retail Jedi clients remain open. No profile gate closes.

The [2026-10-08 downloads under loss record](2026-10-08-legacy-downloads-under-loss.md)
repeats that UDP download with a 1 MiB pak while fnTech3's side drops a fifth
of the packets it receives (`net_dropsim 20`). With retail Quake III, RTCW, ET
and Elite Force, in both directions, the client's pak matches the server's
byte for byte and the client enters pure, though RTCW's retail server resent
the gamestate 29 times. Real networks and much larger files remain open. No
profile gate closes.

The [2026-10-08 sustained sessions record](2026-10-08-legacy-sustained-sessions.md)
keeps a retail and an fnTech3 client on one server for ten minutes, with chat
every 20 seconds and a map change halfway, while fnTech3's side drops a
tenth of the packets it receives. With retail Quake III, RTCW, ET and Elite
Force, in both directions, every one of the 30 rounds is heard both ways and
both clients follow the map change. Q3Test 1.08 holds its two clients for
five minutes in both directions through a map change. The retail client
hears every line of fnTech3's, but retail Q3Test reads its config files only
at startup, so it cannot be made to speak. The short sessions -- join, chat,
map change, `map_restart`, reconnect, server quit and kick -- also pass at
that loss with the four retail engines in both directions. No profile gate
closes.

The [2026-10-06 GameSpy record](2026-10-06-mohaa-gamespy-lan-queries.md)
makes a Medal of Honor multiplayer server, dedicated or listen, answer
GameSpy's LAN status queries on the first free port from 12300 with the keys
OpenMoHAA's server reports **[O]**, less those 1.00's game lacks; no master is
told of it. It also corrects the legacy-server record: 1.00's own Join LAN
Game list asks the game port with `getinfo` (its `ui/joinlangame.urc`), not
GameSpy's ports, and a listen server hosting a team match had answered
neither, the engine taking Medal of Honor's game type 2 for Quake III's
single player (Source, Unit; Live on loopback against fnTech3's own servers
and client). No retail program has read the answers. No profile gate closes.
The [2026-10-08 GameSpy search record](2026-10-08-mohaa-gamespy-lan-search.md)
makes fnTech3's own Join LAN Game list make GameSpy's LAN search beside its
`getinfo` scan, as the SDK's browser made it **[O]**: `\status\` broadcast to
12300 to 12316 from a socket of its own, the answers taken for three seconds,
judged as OpenMoHAA's browser judges one **[O]** and listed only when 1.00
can join them; a server that answers `getinfo` is listed from that answer.
A server whose game port the `getinfo` scan cannot reach is now found, and
the query and search sockets carry datagrams on Windows and on i686 Linux
(Source, Unit on both; Live on loopback against fnTech3's servers and the
probe's own stand-in answers). No retail program has sent or answered these
queries. No profile gate closes.

The [2026-10-06 Jedi Academy live-join record](2026-10-06-jka-live-join.md)
serves that import, client-game import 38 (`CG_S_STARTLOCALSOUND`), through
fnTech3's own local-sound path with Academy's channel numbers translated to
the engine's, and stops `VM_Call` refusing Academy's client-game map change
as Enemy Territory's game snapshot callback, the two semantic exports sharing
the number 0x10004 (Source, Unit). Live against retail `jamp.exe`, fnTech3's
client now takes the server's chat and follows a map change, then stops in
its client game's first frame on the new map at a Ghoul2 instance with an
attached weapon model, the next boundary; `map_restart`, reconnect and server
quit stay unobserved in that direction. The record's 2026-10-08 re-check at
`6a93b622` passes every step of the standard session with sound off, and,
in a diagnostic build answering the voice-volume query (import 35), with
sound on through both backends; there each chat line starts the chat sound
as an Fn OpenAL voice in the local class at the listener, at retail's full
level (Live subset). No profile gate closes.

The [2026-10-08 Jedi Academy UI local sound record](2026-10-08-jka-ui-local-sound.md)
serves the UI module's own local sound, import 33 (`UI_S_STARTLOCALSOUND`),
through the same channel translation, level and Fn local-sound path as
client-game import 38: retail `jamp.exe` hands both imports to
`S_StartLocalSound` with the same code, and the retail UI sends channel 0
from its `play` command, 1 from its move previews and 8 for focus sounds
(Source, Unit). The UI's own `ui_openmenu` console command reaches the import
without input, through a menu whose `onOpen` runs `setfocus` and by closing
one whose `onClose` runs `play`: the build before the slice refuses it by
name, and with the slice both menu sounds start as Fn OpenAL voices, the
focus sound in the dry `local` class at full level and the `play` sound in
the `self` class at 0.75, while the legacy mixer plays them at 127 and 95
(Live subset, a client at its main menu). No profile gate closes.

The [2026-10-08 menu sound record](2026-10-08-ui-sound-policy.md) settles
the class that record left open, for every profile: a sound a game's menu
module starts through its local-sound import is now interface audio in Fn's
tone policy whatever channel it names, the announcer channel keeping its own
class, while the channel, every channel rule, the level and the client games'
sounds are unchanged (Source, Unit). Its survey of every retail menu module
finds them all on local, local-sound or announcer channels except Academy's
`play` (the automatic channel) and seven Elite Force Holomatch call sites on 8,
among them the holodeck intro's computer voice, which plays as the main menu
first opens; Jedi Outcast's `play` is on 0 too, though its UI import 32 is
not served yet. Live with no input, Academy's `play` click and Holomatch's
intro voice move from the `self` class, with a send of up to 0.17 into the
menu's small-room reverb, to the dry `local` class at the same levels; the
legacy mixer plays both as before (Live subset, clients at their main
menus). No profile gate closes.

The [2026-10-06 Jedi Academy attachments record](2026-10-06-jka-attachments.md)
serves that boundary: in the multiplayer client game, a Ghoul2 model linked
to another is posed from its parent's bolt with its own skeleton and drawn
as its own Fn skeletal entity, and the client game's bolt queries answer
from the same posing. Retail `jamp.exe`'s model link, model order,
parent-bolt root and surface tag frame are measured, and the store
reproduces 54 of 60 bolt answers taken from retail itself to within 0.00011
units; the other six, on a model retail never poses, are refused by name
(Source, Unit). Live against retail `jamp.exe`, the attached weapon no longer
stops the client; it stops next at item entities carrying Academy's
`RF_RGB_TINT` and `RF_FORCE_ENT_ALPHA` render flags. Diagnostic builds past
them flush the attached weapon on its animated hand and reach `map_restart`.
The server's stores keep this engine's earlier tag frame and do not pose
through links, which retail's answers contradict. No profile gate closes.

The [2026-10-06 Jedi Academy entity tint record](2026-10-06-jka-entity-tint.md)
serves those flags. Retail `jamp.exe`'s stage iterator, colour function and
surface list are measured with Capstone: under `RF_RGB_TINT` (0x800) every
stage takes the entity's colour in place of its own source, under
`RF_FORCE_ENT_ALPHA` (0x400) single-texture stages take the entity's alpha
with source-alpha blending and no depth write, and such entities draw after
the rest of the view. The client adapter carries both as engine-owned entity
fields, not Quake III flags, and all three renderers apply them; the RTX path
tracer leaves those entities to raster. Client-game import 201
(`CG_R_CLEARDECALS`) returns zero as retail's does, no Academy decal being
served (Source, Unit). Live against retail `jamp.exe`, the flagged entities
no longer stop the client: on `mp/ffa3` it stops next at the map's present
ambient set (import 48), while on `mp/ffa1` `map_restart`, a reconnect and
the server quitting all complete, and engine captures in GLx, Vulkan and RTX
show an item drawn with forced alpha. A player who joins the game stops at
the first sound its client game starts (import 37). No profile gate closes.

The [2026-10-06 Jedi Academy ambient sets record](2026-10-06-jka-ambient-sets.md)
plays that set. Retail `jamp.exe`'s ambient-set player is measured with
Capstone: the set the client game names each frame (import 48) becomes
current and fades in over 1,000 ms of the client's real time as the one it
replaces fades out, each slot plays its set's loop at the set's volume and,
at intervals and volumes drawn from the engine's own generator, its
subwaves, and the loops of one sound mix as one. An engine-owned player
written from those readings matches an independent model of them frame by
frame, and hands its loops to a new Academy loop entry both backends render
as their own loop voices and its subwaves to Fn one-shots on Academy's
ambient channel (Source, Unit). Live against retail `jamp.exe`, the client
now passes `mp/ffa3`'s `city_outdoor` and completes every step of the
standard session, `map_restart`, reconnect and server quit included, on
both maps. With sound on, the client stops earlier, at the voice-volume
query (import 35); a diagnostic build answering it showed the set's loop as
an OpenAL voice at the listener whose gain rose from 0.04 to 1.00 over about
a second. No profile gate closes.

The [2026-10-06 Jedi server attachments record](2026-10-06-jedi-server-attachments.md)
gives Jedi Academy's server a Ghoul2 store of its own, serving the engine
`*default` skeleton, attached models and retail's tag frame, because retail
`jamp.exe`'s game imports reach the same Ghoul2 cores as its client game's.
One synthetic game module asks retail's dedicated server and fnTech3's 72
bolt queries on Kyle holding three pistols and a saber: the 66 retail's
model order reaches match within 0.00011 units, and the other six are
refused with nothing written; before, every pistol and saber query was
refused and body tags were 0.59 units off (Source, Unit, Live against
retail's server). Jedi Outcast's engine was read with Capstone: its links,
model order, attached roots, tag frame and engine skeleton are Academy's,
but it keeps each bolt per build, and its server needs the disc, so that
slice left its store as it was. Server blends, query time and failure output stay open.
No profile gate closes.

The same record's second slice (2026-10-07) gives Jedi Outcast's server that
store: one build per time judged by model 0, bolts answered from what the
build kept, 587 answered without building, a bend of model 0 seen at once,
and Academy's skeleton, links, tag frame and bends with Academy's placement
less its row normalization, all read from `jk2mp.exe` 1.02a (Source, Unit).
Its dedicated server never smooths bones: the renderer that registers
`r_ghoul2animsmooth` never starts there. Imports 586, 587, 591 and 595 are
served, but the retail module did not reach them then: 584 read its instance
words as addresses and answered "no models", and correcting it led to the
refused 592, 593, 596 and 599. No profile gate closes.

The [2026-10-07 Jedi Outcast server clips record](2026-10-07-jk2-server-clips.md)
serves those four and corrects 584, all read from `jk2mp.exe` 1.02a with
Capstone: 592's clip setter and Outcast's own clip timing, read from its bone
transform (an age of milliseconds times 0.02, loops wrapped by `fmod`,
reverse clips, and a clip ended without loop or freeze leaving its bone on
its parent's frames), 593's skeleton name, 596's duplicate, which leaves a
field already holding an instance alone, and 599's clean, which frees only
through a field 588 or 596 made an instance for. A QVM's instance field is
now read and written as a 32-bit word, where 588 had written a pointer's
width (Source, Unit). On fnTech3's developer dedicated server the retail
QVM's client setup now completes for two bots, and in 75 to 300 seconds of
`ffa_bespin` and 180 of `duel_pit` every 586 (the saber's `*flash`), 591 and
592 was served, and every 587 but each bot's first ten, which ask for a bolt
no build has reached yet (Live subset). The bots never fought, so the saber
hit-location queries were not reached. No profile gate closes.

The [2026-10-07 Jedi Academy server pose clock record](2026-10-07-jka-server-pose-clock.md)
serves the three things that record left open on Academy's server. Retail
`jamp.exe`'s bolt query never reads the time word a module passes: it poses
at the engine's Ghoul2 clock, which the server sets during its spawn and
after each server frame's game frames, so a game frame's queries pose one or
two frames behind its time; `map_restart` sets none. fnTech3's Academy
server now keeps that clock, serves the client game's transitions between
clips, writes retail's fallback for a model or bolt the instance lacks, and
keeps the no-rotation selector until a query succeeds. Under `fixedtime 75`
a synthetic module asks retail's dedicated server and fnTech3's 1,468
queries retail answers on Kyle holding a pistol and a saber, 231 of them
mid-transition: 1,466 match within 0.0000134 and 0.000126 units, the 25
failures write retail's fallback within 0.00000006, and the other two are
retail reusing a pose at one clock, recorded and not reproduced. Before, 853
were refused and 240 were up to 43 units off (Source, Unit, Live against
retail's server). The October 3 blend measurement had used this server path
too. No profile gate closes.

The [2026-10-07 Jedi Academy server bone animation record](2026-10-07-jka-server-bone-anim.md)
serves import 597, which the retail game calls once and fnTech3's server
refused. Retail `jamp.exe` answers it at the Ghoul2 clock, ignoring the time
word, with a running clip's frame, range and speed and its bone record's own
flags word, which carries the 0x8000 the clip setter marks it with, a blend's
0x80 until the next clip, and any angle mode; a bone with no clip gets 0..1
defaults and a name the skeleton lacks nothing, matched without case. The
store now keeps that word by both setters' rules. Through one synthetic
module under `fixedtime 75`, all 678 answers retail's dedicated server and
fnTech3's both give match bit for bit, 124 of them fractional frames, and the
retail game's own hand-off of an answer to two arm bones poses within
0.0000115 units; before, the first call stopped the server (Source, Unit,
Live against retail's server). Clips the store does not time are refused by
name. No profile gate closes.

The [2026-10-09 Jedi Academy foreign voices record](2026-10-09-jka-foreign-voices.md)
registers `s_language` for both Academy profiles as both retail engines do,
`english`, archived and kept by `cvar_restart`, and loads each voice as they
do: both lowercase a sound's name, and when `s_language` is `deutsch`,
`francais` or `espanol` the first `chars` in it becomes `chr_d`, `chr_f` or
`chr_e`, read before the English recording (Source, Unit). Without input, a
multiplayer client at its main menu loads Kyle's lines and three other voices
from the German, French and Spanish folders, the English recording where a
language has none and a taunt that has no English recording, through both
mixers, and changes language through the setup menu's own
`ui_load ; snd_restart`; the build before the change loaded English
throughout (Live subset). Single player's voices have unit evidence only. No
profile gate closes.

The [2026-10-06 Jedi listen-server record](2026-10-06-jedi-listen-server-game-types.md)
gives `jka-mp` and `jk2-mp` their own single-player game types, read from
each retail engine's `SV_Map_f`: 5 in `jamp.exe` 1.01 and 4 in `jk2mp.exe`
1.02a, where fnTech3 used Quake III's 2, both games' Jedi Master. `spmap`
now selects them; `map` keeps every Academy game type and turns Outcast's 4,
not 2, into a free-for-all. A client-hosted server's `getinfo`, `getstatus`
and `getchallenge` stay closed in Outcast's single player alone, as
`jk2mp.exe`'s are, and in no Academy game type, since `jamp.exe` never reads
the game type there; every other profile is unchanged (Source, Unit over all
sixteen profiles; Live on loopback against fnTech3's own servers and
clients). Academy's retail game refuses Jedi Master outright, and its host's
own client game still stops at the open local-server bolt clock, so the
queries went to the client executable's server in dedicated mode. No retail
program ran. No profile gate closes.

The [2026-10-06 Quake Live server-type record](2026-10-06-ql-listen-server-type.md)
makes a Quake Live server started from the client admit players by its
`sv_serverType`, as retail `quakelive_steam.exe` does, where it had refused
every remote challenge whatever its server type: the single-player flag its
local match sets and Race, game type 2, taken for single player, neither of
which retail's server reads. Offline (0, the default) drops every packet from
outside in client builds (`SV_PacketEvent` 0x4e4500), LAN (1) refuses a
challenge from outside the LAN with retail's text (`SV_GetChallenge`
0x4df430), Internet (2) admits anyone (Source, Unit over all sixteen
profiles in both builds; Live on loopback against fnTech3's own client
servers). Retail's dedicated server was not read, so dedicated builds keep
their earlier rule. No retail program ran. No profile gate closes.

The [2026-10-08 Quake Live snapshot tail record](2026-10-08-ql-snapshot-tail.md)
settles the last 88 of the 592 player-state bytes Quake Live hands its
client game, which fnTech3 had zero-filled without knowing what retail puts
there. Every instruction in `quakelive_steam.exe` that can reach them was
read, by disassembly of the whole code section and a raw search for the
addresses. On the client, only the delta reader's baseline copy writes them,
always from zeroed storage, so every retail snapshot holds zeroes there, as
fnTech3's do. On the server they are copied into each frame from the game's
own client record and never encoded. The game module uses them for its
medal and scoring bookkeeping. fnTech3's behaviour does not change (Source).
The Quake Live module ABI gate stays partial for its other open items.

The [2026-10-08 IHV client record](2026-10-08-q3-ihv-client.md) lifts the
IHV Test's client gate in developer builds. The original `cgamex86.dll` runs
in the engine with its 62 imports served by engine collision, renderers,
audio and 2D, read from `quake3.exe`'s own table. It is pushed its data as
the original engine pushes it, and asked each frame whether it can draw.
`sli.dm3` plays to its end on GLx, Vulkan and RTX, and with OpenAL sound,
with the installation unchanged (Unit, Live). Under cdb the original reads
and renders as the driver does. Its own screenshots are not pixel references,
so no frame is compared. Network sessions, recording and the build's menus
remain open, and no profile gate closes.

The [2026-10-08 IHV live client record](2026-10-08-q3-ihv-live-client.md)
connects that client to the original `quake3.exe` dedicated server over
protocol 33, read from the original's own handshake, command packets and
frame ping. On loopback it enters `tim_dm1`, receives the server's frames
and quits cleanly, on GLx and Vulkan, and the original server logs it
connected, entering the game and disconnecting (Live). Replayed offline, a
session's 187 messages decode without failure. No input was driven, so moved
or fought play is unshown. No profile gate closes.

The [2026-10-08 IHV server sessions record](2026-10-08-q3-ihv-server-sessions.md)
adds the other direction. fnTech3's dedicated server takes protocol-33
clients as the original server does, read from its own code:
- the challenge and connect, with private slots and reconnects;
- the gamestate and reliable server commands;
- moves, gap replays and the provisional command;
- configstrings repeated in each datagram until acknowledged;
- the rate schedule and ping;
- level changes and drops.

On loopback the original `quake3.exe` client joins `tim_dm1`, plays and leaves
cleanly. With fnTech3's own client beside it, both see each other, re-enter
after an `rcon map km_portal`, and a kick reaches the original client as a
clean disconnect (Live). `getinfo` and `getstatus` answer in the original's
formats. Bot snapshots are now rebuilt every server frame, as the original
rebuilds them. No input was driven, downloads are refused, and no profile
gate closes.

The [2026-10-08 IHV demo recording record](2026-10-08-q3-ihv-demo-recording.md)
adds `record`. It writes the build's own length-only `.dm3`, following
`quake3.exe`'s `CL_Record_f`: a gamestate of the client's configstrings and
model-carrying baselines, then each message from the next full frame, then
one -1. Recordings made on fnTech3's server and on the original server play to
their end in both clients. The original client's own recording plays in
fnTech3, and its gamestate record re-encodes byte-identically through the
shared writer (Live/Fixture). Recordings that span a level change, by either
client, play through both levels in fnTech3. The original client stops after
a demo's second gamestate, in its own recordings too.

The [2026-10-08 IHV commands and 2D record](2026-10-08-q3-ihv-commands-and-2d.md)
closes three client gaps.

- Console commands forwarded to a server, such as `kill` and `give all`, had
  all failed with "Client command overflow", because the IHV wire gave the
  reliable command ring no room. They now reach the game on both servers.
- The build's 2D art takes the colour its client game sets, as the original's
  `Draw_StretchPic` (`0x43fd60`) gives it. Its `nomipmaps` stages take the 2D
  colour in all three renderers, and handle 0 draws white. The HUD's counters
  are now green and the scoreboard's bar blue with its text, as in the
  original's capture.
- The overlay square (`R_BlendBlobs` `0x442700`) is drawn after the scene in
  eye space, but no observed scene used it.

No profile gate closes.

The [2026-10-08 IHV stageless and portals record](2026-10-08-q3-ihv-stageless-and-portals.md)
closes two rendering gaps on `km_portal`, the map of the build's `sli.dm3`.
Captures of the original client and fnTech3's at the same spawn showed both.

- A shader script with no stage that is neither sky nor fog now draws the
  image of its name. The original's `R_FindShader` (`0x439fd0`) does this; Q3
  1.32 gives such a script the default shader. `km_portal`'s walls
  (`textures/sfx/blackness`) now draw black, as in the original, not as
  fnTech3's grid.
- Portals have no range and their cameras always roll, as in the original's
  portal pass and orientation (`0x42c160`, `0x42bde0`). The IHV dialect has
  no `alphaGen portal`, so Q3 1.32's range test had never drawn the view.
  `km_portal`'s portal now shows its turning view in GLx, Vulkan and RTX.

No profile gate closes.

The [2026-10-08 IHV loss and level change record](2026-10-08-q3-ihv-loss-and-level-change.md)
keeps the original client and fnTech3's in one game on fnTech3's server for
three minutes. fnTech3's processes drop 10 to 25% of the packets they receive,
and the level changes under loss. Both clients play through it, and console
commands reach the game. The runs found one fault in fnTech3's client, now
fixed. After a level change it sent no move for about as long as it had
played the level before, on either server: its ring of sent packets outlived
the gamestate, which the original's `CL_ClearState` (`0x404ac0`) clears. Both
servers report a ping of 999 for a while after a level change, as
`SV_CalcPings` (`0x41a910`) averages frame records that outlive the level.
No profile gate closes.

The [2026-10-08 IHV serverinfo record](2026-10-08-q3-ihv-serverinfo.md)
publishes the original's serverinfo keys, in its order: those its `SV_Init`
(`0x419a40`) marks, such as `hostname` and `privateclients`, and the game's.
fnTech3's own serverinfo cvars, such as `sv_hostname` and
`sv_allowDownload`, no longer appear. `getstatus` carries the same string, so
a server browser reads the `hostname` an operator sets. Only the version stays
fnTech3's. No profile gate closes.

The [2026-10-08 IHV overlay and particles record](2026-10-08-q3-ihv-overlay-and-particles.md)
sets the client game's particles beside the original's, through its own
`cg_testparticles`. The 4,096-particle grids match, and 99.2% of the
original's particle pixels are white in fnTech3's capture too, so the large
white discs of `sli.dm3`'s opening are the original's own look. The overlay
square is the client game's damage blood splat (`0x10010880`). It now draws
its shader's first image in the overlay colour, as `R_BlendBlobs`
(`0x442700`) does, not as an opaque black blot. The original issues that
draw, as cdb shows, but its screenshots show no splat, for a reason not yet
settled. No profile gate closes.

The original client's parser treats the download message as illegible
(`0x407188`), so no IHV peer can carry a download, and downloads are no
longer counted as an IHV gap. No profile gate closes.

Desktop system-console branding has Source/Unit/Build evidence for all sixteen
profiles: title palettes, active-profile remade icons in Windows client and
dedicated consoles, and the corrected Elite Force artwork. The
[presentation record](2026-10-02-system-console-branding.md) records contrast,
icon-resource and build checks; it changes no gameplay or live gate status.

The [2026-10-02 all-game round](2026-10-02-all-games-round.md) adds executed
file-list and packed mod-list boundaries for all sixteen real profiles. Null
output and non-positive capacity now return an empty result without touching
the buffer or enumerating files; a null list-count destination is refused.
Normal listings, profile base-game exclusions and the root renderer shader
remain covered. Windows x86 guard mutations and Linux x86 sanitizers pass.
Q3/QL identity probes now require a fresh challenge, unambiguous complete
fields and the child server's observed port. One unchanged client/server pair
also covers 41 bounded startup or expected-refusal cases, including 36 inspected
GLx/Vulkan/RTX engine captures. These are startup and API-boundary subsets;
gameplay input, listening, campaign and retail interoperability gates remain open.

The [2026-10-02 UberTools round](2026-10-02-ubertools-round.md) bounds Alice
and F.A.K.K.2's game configstring imports by their own native limits, retains
MOHAA's client-game message terminator at full queue capacity, and validates
Ritual lighting inputs under fast-math. Production regressions distinguish
retail observations from input policy and reconstruction evidence. Probe
output containment and explicit map selection are strengthened, and MOHAA's
legacy audio limitations are documented. All three profiles remain experimental;
no full profile gate closes.
The same shared client reaches a first snapshot and exits normally on all
120 installed maps with GL (Alice 36, F.A.K.K.2 30, MOHAA 54), and on twelve
selected map/renderer cases. Fifteen additional GL/Vulkan/RTX raster probes
write inspected engine captures with OpenAL initialized and muted. The host
runtime lacks HRTF; campaign, listening and retail comparisons are not added
by these startup checks.

The [2026-10-02 compatibility round](2026-10-02-compatibility-round.md)
adds ET's source-observed routing-cache budget, per-map frame limits and
two-cache eviction, and excludes duplicate world-model entities from ET's
multi-entity sweeps and point queries. Generated production checks cover
profile controls; stock ET has no navigation for a live cache comparison.
The client refuses ET URL redirects before they can be treated as file data;
its download capability remains UDP only. A generated 32,916-byte pak now
transfers over UDP in both retail interoperability directions with pure
admission and normal exits. The reverse run initially failed in retail's
client after one chunk: the download writer and network sender both added
an EOF, leaving a false cgame binary message. ET now leaves termination to
the sender; the reproduction passes after the correction. HTTP redirects,
larger/lossy transfers and broader mods remain open. Release probes now require both
challenged info/status replies from the selected endpoint, protocol and map,
plus the child server's observed bound port. No full profile gate closes.

The [2026-10-01 reported-fault follow-up](2026-10-01-observed-compatibility-fixes.md)
repairs RTCW SP null effects consuming weapon channels, gives ET its own
command-button bits, and includes
ET's native client-module checksums in its pure proof. The reported Elite Force
maps load with Vulkan. The [wider Elite Force memory sweep](2026-10-01-stvef-hunk-map-sweep.md)
sets the SP minimum to 160 MiB, reserves a 96 MiB initial SP zone, and retains
Holomatch's 128 MiB floor. All 99 installed maps load at 1024 by 768 with Vulkan
and OpenAL initialized and muted. ET rejoins
a pure listen server through windowed →
fullscreen → windowed renderer restarts. ET also joins both teams on all six
stock maps under pure admission to original retail servers after the
protocol-84 handshake correction. RTCW's `escape1` combat trace has matching
weapon starts, but a listening comparison and the user's intermittent case
remain open. Alice is deferred at
the user's request. Full profile sign-off remains open.

The [Elite Force foundation](2026-09-18-stvef-contracts.md) registers
`stvef-sp` and `stvef-mp` behind `-Dstvef-development=true`, identifies the
retail installation, and adds MP3 decoding that reproduces every one of the
8,074 retail `#UNCOMP` sizes. The [Holomatch wire slice](2026-09-18-stvef-holomatch.md)
adds retail protocol-24 message coding and the packet scramble; all 32,675
records of the retail Holomatch demos decode through the engine. The
[Holomatch server slice](2026-09-18-stvef-holomatch-server.md) hosts the retail
game module with its bots on a developer dedicated server: free-for-all and a
complete capture-the-flag match run without errors. The
[Holomatch client slice](2026-09-18-stvef-holomatch-client.md) adds the retail
handshake, client framing, downloads, pure checksums and `.efdemo` recording
and playback. The [Holomatch cgame and UI slice](2026-09-18-stvef-holomatch-cgame-ui.md)
runs the retail cgame and UI with Raven's effect primitives in all three
renderers: the retail main menu draws, and all three retail demos play to the
end. The [Holomatch sessions record](2026-09-18-stvef-holomatch-sessions.md)
plays live with Lilium Voyager and the retail game, as client and server and
with pure servers, and records demos live that fnTech3 and Lilium play. The
[languages record](2026-09-19-stvef-languages.md) plays the German voices,
gives the UI retail's translated key names and its own key numbers, and
registers retail's three language cvars. A missing pak downloads between
fnTech3, Lilium and retail in every direction.

The [ET contract and dedicated-server slice](2026-09-17-wolfet-contracts.md)
adds a distinct immutable profile, protocol-84 codecs, frozen module records,
checked server-memory transfers, native game imports, and attachment-tag loading.
ET is not a supported client release. UI/cgame, demos (partial: see the
demo playback record below),
pure admission, gameplay, and bidirectional interoperability remain open.
ET's skeletal meshes now have a model type, a surface and a draw call in all
three renderers, verified by arithmetic over every installed model. A
developer-build ET client now runs against a retail installation, draws ET's
own menus and limbo panel through the retail modules, and quits cleanly; see
the [client run record](2026-09-18-wolfet-client-run.md). A
retail ET client reaches an active non-pure spectator session on the experimental
server and both processes exit normally. The later
[reported-fault follow-up](2026-10-01-observed-compatibility-fixes.md) adds pure
outbound admission and both teams on all six stock maps; sustained sessions
remain open.
The [ET snapshot follow-up](2026-09-17-wolfet-snapshots.md) adds checked per-client
callbacks and single/multiple visibility proxies with executable regressions.
Retail Axis/Allies admission and same-binary Q3/QL startup still pass; live
landmine/proxy behavior remains open. The
[viewpoint and capacity follow-up](2026-09-17-wolfet-viewpoints.md) adds ET
origin-only brush visibility, self/exclusive cameras and lean, and removes the
incorrect 512-entity server cap while preserving the frozen cgame limit.
4,936 finite camera/lean cases match the original-source calculation; live
visibility and rendered-camera parity remain open. The
[game-import follow-up](2026-09-17-wolfet-game-imports.md) raises handled ET
game imports from 52 to 189 of 212, covering every import the retail game
module can issue. It adds the binary message channel, temporary bans and their
connection refusal, ET's three-argument drop-client import, preferred-slot bot
allocation, the dedicated sound stubs, and the elementary-action, AI-state,
bot-library and navigation surfaces. Eight of the 23 unhandled ordinals
reproduce refusals retail ET makes itself; the other 15 need bot-library
exports or record conversions without evidence. No live ET bot session is
claimed. The [pure follow-up](2026-09-17-wolfet-pure.md) then opens ET pure
admission: it restores retail's `sv_pure 1` default, admits ET wherever the
RTCW native module pair was handled, and publishes the loaded-pak names retail
ET clients require but fnTech3 had left empty. A retail ET client now passes
pure admission, reaches CS_ACTIVE and joins both teams on all six stock maps;
one client and one installation do not close the broader pure, download or
interoperability gates. The
[gameplay follow-up](2026-09-17-wolfet-gameplay.md) adds two more bounded
inbound runs under pure admission: a retail client spawns on Axis and moves
about 250 units through three distinct view origins, and a goldrush-to-battery
map change keeps that client connected and re-entering the world. Movement
parity, combat, objectives and dense exchanges remain open. The
[official-pak correction](2026-09-17-wolfet-downloads.md) then replaces the
compile-time download test that could only match Quake's directory, so ET and
RTCW stop offering their own retail paks; no ET download had been transferred
end to end at that checkpoint. The [navigation follow-up](2026-09-17-wolfet-navigation.md) raises
handled ET game imports to 198 of 212, corrects a blocking-entity rule the
retail game module reaches, and adds ET's chat-match record. The six still
refused each need navigation data or trace results this engine does not
produce. The [redirected-download record](2026-09-17-wolfet-web-downloads.md)
adds ET's server-side www download: the redirect message, the fallback URL and
the client progress protocol, kept unknown to the other four profiles. No ET
download had been transferred at that checkpoint. The
[2026-10-02 round](2026-10-02-compatibility-round.md) adds a generated
UDP transfer in both retail directions after correcting a duplicate
download EOF; its HTTP redirect capability remains unavailable. The
[visibility follow-up](2026-09-17-wolfet-visibility.md) derives ET's per-area
waypoints and visibility from the loaded navigation world and adds its five
reachability floods, reaching 202 of 212 game imports. Retail ET ships no
navigation files, so none of that surface is reachable from stock content and
no live behaviour is claimed. The
[prediction follow-up](2026-09-17-wolfet-prediction.md) then writes ET's own
movement prediction and the world-trace result it returns, completing the
declared ET game import surface at 204 of 212: the eight that remain are the
ordinals retail ET's own engine refuses. The
[model record](2026-09-18-wolfet-models.md) adds bounded MDM and MDX readers
and decodes all 21 skeletal files in a legitimate installation; decoding is not
rendering and the client stays gated. The
[cgame boundary record](2026-09-18-wolfet-client-boundary.md) then opens ET's
client-side records, its renderer imports, its key vocabulary and the dispatcher
that routes them, reaching 106 of the 147 cgame imports, and the
[UI boundary record](2026-09-18-wolfet-ui-boundary.md) reaches 104 of the 121 UI
imports, six of which retail ET refuses itself. The
[sound record](2026-09-18-wolfet-audio.md) then carries ET's own sound values --
a volume with every one-shot, a start time with every loop and a stop-sounds
flag on fades -- and the [light record](2026-09-18-wolfet-light.md) carries its
dynamic light. The
[client services record](2026-09-18-wolfet-client-services.md) then wires its
binary message channel in both directions, its screen-space polygons, its saved
view and its in-game popups, and the
[text and files record](2026-09-18-wolfet-text-and-files.md) adds its
localisation lookup and its own delete, taking those two surfaces to 136 and
114, and the [camera record](2026-09-18-wolfet-camera.md) adds its spline
paths and the
[scene geometry record](2026-09-18-wolfet-scene-geometry.md) its batched
geometry and runtime shaders, and the
[texture identity record](2026-09-18-wolfet-texture-id.md) its image lookup,
taking the cgame surface to 143. Of the seven UI imports still open, six
are ordinals retail ET refuses itself and the seventh is a deliberate refusal to
open a module-supplied URL; all seven still open on the cgame side are renderer
features. Nothing in any of
them has run inside a client.

The [RoQ bounds and recovery checkpoint](2026-09-17-roq-bounds.md) adds checked
movie framing, dimensions, codebooks, VQ/motion reads and partial shutdown. It
fixes synchronous first-frame hangs and terminal-frame accounting, verifies
27 distinct installed movies, and passes malformed-movie recovery under all four
profiles through the same Windows x86 client. Original RTCW also ignores JPEG
chunks, so that preserved policy is no longer listed as an unimplemented
original-engine contract. Full cinematic/audio parity remains open. The
[RoQ fuzz record](2026-09-18-roq-fuzz.md) then passes 7,300 seeded corruptions
of ten installed movies through that decoder under AddressSanitizer, and 60 per
profile through one running client under `q3`, `rtcw-sp` and `rtcw-mp`.

The [RTCW dynamic-light update](2026-09-17-rtcw-dlight-shadows.md) enables the
shared Q3/QL `r_dlightMode` lighting and point-shadow path for ordinary SP/MP
lights, stabilizes flickering torch projections, and admits MDS/MDC model
casters in every raster backend. Executable tests, x86 builds and bounded
`escape1` engine captures add lighting evidence; image parity and broader
renderer/gameplay gates remain open.

The [RTCW engine scene lights record](2026-09-24-rtcw-engine-lights.md) gives
both RTCW modes the static map lights, surface-light proxies and `r_dlightTest`
lights every other profile had. They join only the per-pixel modes and never
the snooper view or `r_dynamiclight` 0 or 2; RTCW's module lights keep the 32
slots a frame retail gives them, the engine's taking only slots beyond those,
and are drawn after the engine's so the darkening and flame materials apply
over them. Frame simulations, per-renderer executed slices and seven caught
regressions hold those rules. `mp_ice` replays in GL, Vulkan and RTX draw the
lights, match the build without the change byte for byte with them off, and
Quake III's replays are byte-identical before and after. No RTCW light file
ships yet; generating one from the maps' `light` entities is open.

The [ET engine scene lights record](2026-09-24-wolfet-engine-lights.md)
gives Enemy Territory the rules RTCW's scene lights got. ET's modules keep the
first 32 lights a frame the original stores, counted apart from the engine's
static, proxy and test lights, which take only slots the modules can no longer
claim and join every shared mode but not `r_dynamiclight 0`. A forced module
light now stays drawn at `r_dynamiclight 0`, as in the original, where the
renderer used to clear it; directed and grid-only lights, still not drawn,
take one of the 32 as the original's do. Executable tests, a 20,000-frame
simulation and nine caught regressions hold the rules. Live `oasis` clients in
GL, Vulkan and RTX draw the engine's lights, and a thrown charge's glow stays
drawn at `r_dynamiclight 0` in GL; Quake III, Quake Live and RTCW MP replays
are byte-identical before and after. No ET scene with a sky portal, multiview
or portal view has been run. ET demos now replay
(`2026-09-25-wolfet-demo-playback.md`): fnTech3 and retail `et.exe` each play
the other's `.dm_84` with the same parse, message for message, and the same
view at every sampled timedemo frame, on one map with spectators only.

Since the [RTCW map lights record](2026-09-24-rtcw-map-lights.md), the package
ships that file for all 32 official SP and 15 official MP maps, 4,316 lights in all. Each
map's `light` entities are read with the keys and flags RTCW's own `QUAKED`
comment documents and q3map2's Wolf falloff, whose `fade` reading and
inverse-square law the maps' baked lightmaps confirm; each light's radius and
brightness are fitted to retail's baked curve at its mode's `r_dlightScale`.
Grid-only, non-dynamic, negative and cgame-drawn fixture lights are left out,
and 128 are kept a map across its PVS clusters. The package validator now
reads every packaged light file as the renderers would. `mp_castle` replays
in GL, Vulkan and RTX load all 128 from the package and change about 86,000
pixels; in mode 0, and with a package lacking the files, frames with
`r_staticLights` on match frames with it off byte for byte. A live `escape1`
client shows them in a lamp-lit corridor. They are engine-owned estimates added to the baked lighting; no
frame is compared with retail and no shadow they cast was seen.

The [FOV/HUD restoration](2026-09-17-aspect-correction.md) restores FnQuake3's
HUD rule/dump system for Q3 and both RTCW profiles, extends the shared FOV
policy to RTCW, and leaves retail QL presentation untouched. Separate stock
SP/MP HUD presets ship in their profile namespaces. Executable layout/import
tests and windowed engine captures add presentation evidence; they do not
close the broader renderer, mod-layout, campaign, or multiplayer gates.

The [Quake III HUD alignment record](2026-10-06-q3-hud-alignment.md) places
each part of baseq3's HUD by its own layout on wide and tall screens: text is
matched by the line, so no line splits, and the scoreboards and loading screen
are centred 4:3 screens. Unit fixtures replay cg_draw.c's geometry, and
windowed engine captures at four screen shapes, two clients voting and demo
playback show every part placed; it is presentation evidence only.

The [profile-scoped package update](2026-09-17-profile-packages.md) separates
engine assets under `games/<profile>/<gamedir>/` in `fntech3-pkg.fnz` and
incorporates the complete reviewed FnQuake3 package, including Team Arena.
Executable resolver tests cover all four profiles; same-binary dedicated
reads verify Q3/QL isolation and Q3 missionpack precedence. This adds package
lookup/parser evidence without closing the broader filesystem or asset gates.

The [RTCW audio-zone generation](2026-09-17-rtcw-audio-zones.md) adds sidecars
for all 32 official SP and 15 official MP maps, including SP patches and the
official MP map packs. Profile-specific material tests preserve Q3/QL behavior;
all 47 files pass parser/lookup audits. Coverage warnings remain on 24 maps,
and no listening or live OpenAL parity gate is closed by these generated assets.
Since the [Wolfenstein OpenAL voices record](2026-09-24-wolfenstein-openal-voices.md),
RTCW's and ET's voices are sent to these zones' rooms. Before it, the sidecars
loaded and every voice's send was zero.

The following record describes the Q3/QL merger work. References to "both
profiles" in that history mean Q3 and QL. Later RTCW evidence and remaining
blockers are recorded in the [RTCW section](#rtcw-sp-and-mp-shared-executable-implementation-in-progress);
earlier Q3/QL results must not be promoted to RTCW evidence.

fnTech3 does **not** yet claim full Quake III Arena or Quake Live
compatibility. Immutable profile selection, Q3/QL module adapters, server-memory
views, and profile-specific live delta codecs are integrated and tested. The
same Windows x86 client now loads legitimate Q3 QVMs and QL native modules,
passes local pure admission, reaches `CS_ACTIVE`, and writes engine screenshots
under both profiles. Dedicated probes load maps and advertise protocols 68/91
under automatic and explicit selection. These observations do not close the
complete renderer, demo, authentication, or bidirectional interoperability gates.
The [2026-09-06 checkpoint](2026-09-06-merger-checkpoint.md) records exact
executable identities, intermediate failures, test totals, and limitations.
The subsequent [RTX work round](2026-09-06-rtx-round.md) fixes the recorded Q3
black-polygon defect and establishes separate native-RT Q3 x64 evidence. The
shared x86 GPU/driver path still lacks native-RT capability on this machine.
The subsequent [WebUI request port](2026-09-06-webui-request-port.md) records
FnQL's request/avatar retry corrections, the preserved QL-only boundary, and
new fnTech3 build and offscreen retail-adapter evidence. Gameplay performance
and the broader WebUI lifecycle gates remain open.
The [2026-09-07 Steam discovery checkpoint](2026-09-07-steam-discovery.md) adds
profile-owned Q3/QL discovery, bounded metadata parsing, and same-binary
dedicated startup without explicit installation paths.
The [build-portability checkpoint](2026-09-07-build-portability.md) adds clean
Linux Make/Meson/CMake and renderer-build evidence, native Linux dedicated
startup for both profiles, and corrections to Windows compiler probes.
The [Q3 reference-peer checkpoint](2026-09-07-q3-reference-interop.md) adds
bidirectional protocol-68 pure admission and active sessions against existing
FnQuake3 binaries. Their source revision is unknown, so retail interoperability
and the broader network gate remain open.
The [engine-only packaging correction](2026-09-07-engine-only-packaging.md)
removes inherited proprietary provider staging and rejects known provider and
Steam runtime libraries in release trees and archives.
The [console-profile correction](2026-09-07-console-profiles.md) restores Q3's
stock console shader and verifies console rendering for both profiles with the
same rebuilt Windows x86 client; other asset warnings remain open.
The [protocol-71 checkpoint](2026-09-07-q3-protocol71.md) adds bidirectional
reference-peer sessions, a complete local suite pass after the snapshot fixes,
and clean CI evidence for the earlier engine-only packaging correction.
The [shipped Q3 demo checkpoint](2026-09-07-q3-shipped-demos.md) adds successful
playback of retail-provided `.dm_66`, `.dm_68` and two `.dm3` fixtures. Visual
parity, other formats and malformed-input coverage remain open.
The [QL truncated-demo probe](2026-09-07-ql-truncated-demo.md) adds bounded
recovery evidence for an independently confirmed incomplete `.dm_91` payload;
it is not counted as complete external-demo playback.
The [demo isolation correction](2026-09-07-demo-profile-isolation.md) prevents
format selection and automatic fallback from crossing the active Q3/QL
profile boundary, with executable search tests and runtime rejection probes.
Both profiles also preserve active local sessions after explicit cross-profile
demo rejection, continuing snapshots and rendering without reconnecting.
The [partial-header correction](2026-09-07-demo-header-truncation.md) reports
truncated demo headers consistently with truncated payloads while preserving
historical EOF handling, with shared reader tests and both-profile probes.
The [QL reference-peer checkpoint](2026-09-07-ql-reference-interop.md) adds
bidirectional protocol-91 pure/native-module sessions with existing FnQL
binaries. Retail executable and Steam-authenticated interoperability remain open.
The [shared x86 renderer-restart checkpoint](2026-09-07-renderer-restart.md)
adds one successful full restart for each profile with GLx, Vulkan and RTX
raster, including renewed pure proofs, snapshots and engine screenshots.
Repeated stress, renderer switching and WebUI/audio lifecycle remain open.
The same checkpoint now also records one successful GLx → Vulkan → RTX raster
→ GLx cycle per profile, preserving the connection and renewing pure proofs.
All six directed renderer transitions now pass three repetitions per profile
in the checkpoint's follow-up. Long-duration stability and the remaining
WebUI/audio/native-RT lifecycle gates remain open.
The [config migration correction](2026-09-07-config-migration.md) prevents
legacy Q3 QVM selectors from breaking first-launch QL native-module startup.
Alternating client launches now retain separate settings in a shared home.
The [Q3 native-module checkpoint](2026-09-07-q3-native-modules.md) adds a
legacy-native non-pure session and restores Q3's required QVM selection at
pure admission. QL pinned-native admission still passes with the same client.
The [shader light-RGB correction](2026-09-07-shader-light-rgb.md) accepts the
bare triples observed in a Q3 add-on while preserving parenthesized syntax,
with malformed-input tests and both-profile playback in all three renderers.
The [2026-09-08 regression checkpoint](2026-09-08-regression-checkpoint.md)
records the complete local suite after these corrections and clean CI for
the earlier config-migration revision, keeping their evidence scopes separate.
The [profile path-conversion correction](2026-09-08-profile-path-conversion.md)
contains unrepresentable candidate paths before filesystem marker queries,
with Windows/Linux tests and both-profile automatic dedicated startup.
The [filesystem recovery correction](2026-09-08-filesystem-recovery-config.md)
keeps QL profile-root configs out of Q3's recovery queue, with executable
client/dedicated tests; end-to-end invalid-filesystem recovery remains open.
The [packaged-profile checkpoint](2026-09-08-packaged-profiles.md) verifies
byte-identical archives after timestamp changes and both profiles running
from one unchanged extraction. Independent-build reproducibility remains open.
The [audio restart checkpoint](2026-09-08-audio-restart.md) adds muted legacy
and OpenAL initialization/restart sessions for both profiles. Audible output
remains unverified. Its pinned x86 OpenAL Soft follow-up passes 41 synthetic
PCM assertions and both profiles' muted HRTF-enabled restart probes; complete
game-audio parity and device-loss recovery remain open. The same checkpoint
also records both profiles passing the connected-device recovery guard and
forced live reopening of a working default fallback without sound restart.
The [audio system audit](2026-10-02-audio-system-audit.md) adds production-stream
loopback and fault tests for deferred starts, underruns, music endings and full
buffer pools; spectral tests for band-limited rate conversion, high-band tone
preservation and Ritual's non-native score rates; runtime-selected sinc conversion
for static sources; MOHAA score loops, EOF and playback positions; raw idle-drain
phase continuity; native PCM byte-order checks; and ALSA partial-write coverage. Resampler allocations fall by about
89--90% in the recorded chunk benchmark. This is shared subsystem evidence;
physical-device acceptance and each profile's complete audio/input gate remain
open.
The [pak download correction](2026-09-08-pak-download-policy.md) scopes QL's
`pak00` retail exclusion to QL, restoring Q3 add-on download eligibility and
covering both profiles' missing-pak planning with executable regressions.
The [download chunk guard](2026-09-08-download-chunk-bounds.md) rejects truncated
bodies before file writes or acknowledgements; its executable handler tests
cover both profiles without claiming remote fault-injection closure.
The [UDP total-size guard](2026-09-08-download-total-size.md) rejects excess
bytes and premature EOF before writing or finalizing a file; valid downloads
from both reference servers still reach pure admission and active state.
The [server read-failure checkpoint](2026-09-08-server-download-read-failure.md)
bounds reads to the declared total and verifies explicit disconnect delivery to
both reference clients after a generated server pak is truncated mid-transfer.
The [two-client isolation checkpoint](2026-09-08-download-failure-isolation.md)
shows continued reliable-command delivery and rendering by an already active
client after a second download fails, for Q3/QL reference clients and self-pairs.
The [download write correction](2026-09-08-download-write-errors.md) prevents
UDP acknowledgement after a failed write and propagates short writes through
both HTTP callbacks, with controlled failure regressions.
Its Q3 HTTP follow-up also verifies automatic disconnect/reconnect, pure
admission and gameplay after removing pending test-script actions that delayed
the queued reconnect command.
The [QL download checkpoint](2026-09-08-ql-downloads.md) extends UDP and automatic
HTTP reconnect evidence to QL with matching add-on hashes and native pure
admission. It also records a newly observed hosted CI billing/spending block.
The [HTTP error and cURL cleanup checkpoint](2026-09-08-http-errors-and-curl-cleanup.md)
records 404/truncated-response rejection and fixes handle release before
connection reset. Successful HTTP reconnect still passes in both profiles;
error-probe shutdown remains incomplete and required forced termination.
The [manual cancellation checkpoint](2026-09-08-download-cancellation.md)
verifies removal of an observed partial file and same-process retry while the
first HTTP response is still open, for both profiles with normal shutdown.
The [download initialization cleanup](2026-09-08-download-begin-cleanup.md)
releases partial transfer state after an extension-only name is rejected,
allowing an immediately following valid command in both profiles.
The [general HTTP downloader checkpoint](2026-09-08-manual-http-downloads.md)
fixes trailing-slash URL joining and verifies manual Q3/QL split-header downloads
and same-process recovery from an empty response with temporary-file cleanup.
The [HTTP split-header correction](2026-09-08-http-split-headers.md) accepts
arbitrary body callback boundaries in both download paths, rejects incomplete
headers before rename, and passes delayed-header Q3/QL runtime probes.
The [cURL multi-stack error correction](2026-09-08-curl-multi-errors.md) stops
both HTTP polling paths after a transport-stack failure, with controlled
Windows/Linux regressions and Q3/QL HTTP success non-regressions.
The [Linux i686 HTTP checkpoint](2026-09-08-linux-http-runtime.md) verifies
runtime-loaded cURL, a matching generated Q3 add-on with UDP fallback disabled,
automatic reconnect, pure admission, rendering and normal shutdown.
The [HTTP interruption checkpoint](2026-09-08-http-interruption.md) verifies
that short responses leave no installed add-on and that both profiles can
download successfully in a new process reusing the same isolated home.
The [rename fallback correction](2026-09-08-rename-fallback.md) preserves source
files when fallback copying fails, including download finalization, with
Windows/Linux executable failure regressions.
The [clean local Linux checkpoint](2026-09-08-clean-linux.md) builds the i686
dedicated configuration at `b5776c9`, passes its configured suite (84 pass,
one skip), and runs both profiles' native Steam-discovery/map-startup probes.
It does not replace the hosted client/renderer release matrix.
The [local Linux client/renderer extension](2026-09-08-linux-renderers.md) also
builds the non-SDL i686 client and all three renderer modules from that clean
checkout, with 107 configured tests passing and two skips. Linux client
runtime remains a separate gate. The [bundled-SDL build checkpoint](2026-09-08-linux-sdl.md)
also builds the i686 client with embedded GLx and passes its configured suite
(84 pass, one skip), without claiming SDL runtime validation.
The subsequent [native Linux client probe](2026-09-08-linux-client-runtime.md)
reaches Q3 pure admission but finds SDL texture corruption and an X11 shutdown
signal fault masked by exit status zero. Linux client runtime remains open.
The [Unix exit correction](2026-09-08-unix-fatal-exit.md) makes fatal errors
report failure without recursive debug assertions; it does not close the
rendering or GLX shutdown defect.
The [X11 callback lifetime correction](2026-09-08-x11-extension-lifetime.md)
then identifies premature DGA library unloading and fixes the reproduced
shutdown fault: native i686 Q3 pure admission, snapshots and normal client/server
exit pass. SDL texture corruption and broader Linux runtime gates remain open.
The [SDL console context correction](2026-09-08-sdl-console-context.md) then
resolves the reproduced SDL texture corruption by restoring the game's GL
context after console work. Q3 runtime and a scripted renderer restart pass;
full visual parity and broader platform/profile runtime gates remain open.
The [Vulkan descriptor correction](2026-09-08-vulkan-descriptor-restart.md)
fixes a native Linux crash during the pure-join resource rebuild by reallocating
all attachment descriptor sets before updating them. Linux Q3 rendering and a
renderer restart gain runtime evidence without closing broader parity gates.
The [renderer-switch checkpoint](2026-09-08-renderer-switch.md) adds four-step
GLx/Vulkan transition sequences on Linux Q3 and Windows Q3/QL, including Windows
same-renderer fast restarts. All three probes render and exit normally; changed
modules still take a full unload, so cross-module window retention is not claimed.
The [Linux RTX checkpoint](2026-09-08-linux-rtx.md) adds i686 Q3 raster and
software native-RT dispatch/restart evidence. Native RT output remains noisy,
Khronos validation layers were unavailable, and this does not close the shared
Windows x86 Q3/QL native-RT gate.

The target is one shared, drop-in binary set. It must select `q3` or `ql` before
compatibility-sensitive initialization and then use that profile's complete,
internally coherent contracts for the rest of the process.

The [map-loading checkpoint](2026-09-14-map-load-times.md) records faster
`devmap mcsarges ffa` loads, explicit asset/snapshot timing, and same-binary
Q3/QL pure sessions with Vulkan and GLx. It adds bounded decoder/cache tests
and Windows/Linux x86 asset-reader evidence. The measured reload improvements
do not close remote network, complete renderer parity or interoperability gates.

The [client-slot stack correction](2026-09-24-client-slot-stack-overflow.md)
clears server client slots in place. Changing `sv_maxclients` on a running
server overflowed the 1 MB main-thread stack, in code every profile shares,
because `SV_ChangeMaxClients` and the `SV_AllocClients` it calls each built a
whole `client_t` temporary of over 500 KB. Q3, RTCW MP and ET dedicated servers now
spawn the level again with the new count, and retail RTCW MP and ET clients
stay connected across the `map_restart` that applies it (Fixture, and Live
inbound). Other profiles, listen servers and unoptimized builds were not run.

## Frame-time and entity rules checked against installed engines

The [2026-10-09 frame/entity audit](2026-10-09-profile-frame-rules.md) adds
retail machine-code observations and production-function unit evidence.
These rows do not close live gameplay or interoperability gates. Elite Force
keeps its separately established rules.

| Profile | Frame-time evidence and correction | Entity arena |
| --- | --- | --- |
| `q3` | Source/Unit: fixed ticks, x87 product, zero unscaled except camera mode, nonzero-scale floor, load retained | Source/Unit: 1024 records, published snapshot count retained |
| `ql` | Source/Unit: retain scaled ticks; integer division then x87 product, zero unscaled except camera mode, nonzero-scale floor, load retained; zero-tick safety guard retained | Source/Unit: 1024 records, published snapshot count retained |
| `rtcw-sp` | Source/Unit: fixed ticks, x87 product, zero unscaled, nonzero-scale floor, load retained | Full arena already bound; retail number limit reconfirmed |
| `rtcw-mp` | Source/Unit: fixed ticks, x87 product, zero unscaled, nonzero-scale floor, load retained | Full arena already bound; retail number limit reconfirmed |
| `wolfet` | Source/Unit: fixed ticks, installed Steam executable's float product, zero unscaled, nonzero-scale floor, load retained | Full arena already bound; retail number limit reconfirmed |
| `jka-sp` | Source/Unit: fixed ticks, x87 product with fractional carry, zero unscaled, unconditional floor, cinematic 200/500 ms cap, load retained | Full arena already bound; retail number limit reconfirmed |
| `jka-mp` | Source/Unit: fixed ticks, float input then x87 product, zero unscaled except camera mode, nonzero-scale floor, load retained | Source/Unit: 1024 records, published snapshot count retained |
| `jk2-sp` | Source/Unit: fixed ticks, x87 product with fractional carry, zero unscaled, unconditional floor, cinematic 200/500 ms cap, load retained | Shared SP arena already bound; retail number limit reconfirmed |
| `jk2-mp` | Source/Unit: fixed ticks, x87 product, zero unscaled except camera mode, nonzero-scale floor, load retained | Source/Unit: 1024 records, published snapshot count retained |
| `alice` | Source/Unit: fixed ticks, x87 product, zero unscaled, unconditional floor, load retained | Published-capacity arena retained; retail number limit reconfirmed |
| `q3-ihv` | Source/Unit: fixed tick from float fps, x87 product, zero unscaled, unconditional floor and 200 ms cap, load retained | Full arena already bound; retail signed 16-bit number limit reconfirmed |
| `q3test` | Source/Unit: all six installed releases independently checked; fixed ticks, x87 product, zero unscaled, unconditional floor and 200 ms cap, load retained | Full arena already bound; retail number limit reconfirmed |
| `fakk2` | **Open:** encrypted engine; related Alice behavior is inference only; timing unchanged | Existing published-capacity binding retained; engine acceptance remains inference |
| `mohaa` | **Open:** SafeDisc engine; OpenMoHAA supports fixed ticks and zero unscaled, but is reconstruction evidence only; timing unchanged | Existing published-capacity binding retained; engine acceptance remains reconstruction evidence |

The [Jedi broadcast and level-time record](2026-10-15-jedi-broadcast-level-time.md)
adds one clock rule, `FRAME_KEEP_LEVEL_TIME`: in `jka-mp` and `jk2-mp` a map
change keeps the level time, as `jamp.exe` 1.01 and `jk2mp.exe` 1.02a run
their game on `svs.time`, which only a shutdown clears. The
[level clock record](2026-10-15-level-clock.md) settles the other
multiplayer profiles. `quake3.exe` 1.32, `WolfSP.exe`, `WolfMP.exe`,
`etded.exe` 2.60b and `stvoyHM.exe` 1.20 also hand `GAME_INIT` a clock that
only their shutdown clears, so `q3`, `rtcw-sp`, `rtcw-mp`, `wolfet` and
`stvef-mp` keep it too. A synthetic module's level times through a map
change, a `map_restart` and a `killserver` now match each retail server's
marker for marker, where fnTech3 restarted the clock at 0 at an empty
server's map change (Source, Unit, retail comparison). `quakelive_steam.exe`
instead clears its level clock with every level, whoever is connected, and
gives connected clients the old time as an offset (`0x4e35f6`, `0x4e363a`).
`ql` now does the same through `FRAME_RESET_LEVEL_TIME`, and its
`map_restart` clears that offset for every connected client (Source, Unit;
no retail Quake Live server runs here). `jka-sp` and `jk2-sp` remain open:
their retail engines clear the clock at every spawn, and fnTech3 keeps a
connected player's time.

The [portal recursion record](2026-10-15-snapshot-portal-recursion.md)
reads the snapshot walk of every retail engine that shares fnTech3's: each
walks a portal seen through another portal, bounded only by its
already-added test, as fnTech3 already did. It adds RTCW's camera rule in
`rtcw-sp` and `rtcw-mp` (`WolfSP.exe` `0x4391de`, `WolfMP.exe` `0x43c49e`):
a viewer watching a scripted camera is sent only the portals and what their
cameras see. It reads Quake3e's second view (`0x20000`) for `q3` and `ql`
only, where `jka-mp`, `jk2-mp`, `jka-sp` and `jk2-sp` read it too though no
retail walk does, and shrinks the walk's recursive frame from 5,948 bytes to
60 so a chain as deep as retail's fits the stack (Source/Unit; Live in
`jka-mp` against `jamp.exe`). On 2026-10-16 a synthetic module's bot read
the camera rule's entity sets back from retail `WolfMP.exe` and `WolfSP.exe`
dedicated servers in four stages; fnTech3 now sends the same sets, where
the previous build ignored the flag (Live in `rtcw-mp` and `rtcw-sp`; no
retail camera scene with a client was played).

The [Jedi single-player snapshot walk record](2026-10-16-jedi-sp-snapshot-walk.md)
reads the whole walk, add routine and builder of `jasp.exe` 1.01
(`0x43c710`) and `jk2sp.exe` 1.02a (`0x43a170`) and serves `jka-sp` and
`jk2-sp` by them. Both skip an entity not in use and repair the number of
an unlinked one; read no client flag but `SVF_NOCLIENT`, where fnTech3 had
left out every inactive entity and NPC spawner as Quake III's client-mask
rules read those bits; always send the player's entity 0, which fnTech3's
builder had marked sent first; and add up to 1,024 entities, one fewer than
their 512- or 256-state ring on an odd snapshot. Academy also skips a
permanent entity (`0x43c7a9`), sends its sky-portal entities (`0x43c80e`),
and in the eye's pass under Force Sight adds, unwalked, what the power shows
from the client game's camera (`0x43c762`, `0x43c600`), whose position is
also its viewpoint (Source, Unit). Neither profile builds a client snapshot
yet: `jka-sp` serves no client or player state, and `jk2-sp` has no entity
layout at all, so `ReadEntityState` fails for it. No gate closes.

## Evidence vocabulary

| Level | Meaning |
| --- | --- |
| Source | A pinned implementation/reference comparison establishes a contract or identifies a gap. |
| Unit | Deterministic tests exercise a contract without launching the game or using proprietary inputs. |
| Build | A supported configuration compiles and links the shared targets. |
| Fixture | The built engine is exercised against legitimate local modules/assets or recorded non-proprietary data. |
| Live | A real client/server or renderer session exercises the contract. |
| Interop | Both directions work with the intended retail/reference peer. |

Evidence does not automatically promote upward. In particular, Source + Unit +
Build is not a live-compatibility claim. Every closed gate should link or record
the commit, executable, platform, profile, command, inputs, and result.

## Profile contract summary

### Quake III Arena and Quake Live

| Contract | Quake III Arena (`q3`) | Quake Live (`ql`) |
| --- | --- | --- |
| Installation identity | `baseq3/pak0.pk3`; Q3 installation/Steam identity including AppID 2200 where applicable | `baseq3/pak00.pk3`, `bin.pk3`, and `web.pak` as required; retail Steam AppID 282440 |
| Preferred network/demo | Protocol 68, with the supported Q3/ioquake3 legacy set kept explicit; Q3 demo extensions selected by their protocol | Retail protocol 91 and `.dm_91` |
| Module boundary | Q3 QVM and legacy native trap/syscall contracts: GAME API 8, CGAME import API 4, UI API 6 with the known API-4 compatibility path | Exact retail structured qagame/cgame/UI tables and x86 layouts: native GAME API 10, CGAME API 8, and UI API 8; legacy fallback only where explicitly supported |
| Key numbers | Original Quake III's, converted at the UI and cgame boundary: `MWHEELDOWN` 183, `JOY1` 185, `AUX16` 232, `K_CHAR_FLAG` 1024 ([record](2026-09-19-q3-key-numbers.md)) | Retail Quake Live's, which are this engine's own: `MOUSE6`-`MOUSE9` 183-186, `MWHEELDOWN` 187, `AUX16` 236 |
| Key names | The names the console, `bindlist` and each config use, shared by every profile. They agree with both retail tables: 176 is `KP_STAR`, and 130 and 157-159 are written as `0x82` and `0x9d`-`0x9f`, unnamed as retail leaves them. Four earlier names are still read so older configs load, and are never written ([record](2026-09-20-key-names.md)) | The same table; quakelive_steam.exe's names are quake3.exe's plus `MOUSE6`-`MOUSE9` ([record](2026-09-20-key-names.md)) |
| Keypad star | All three key producers report the character `*`, as quake3.exe's Win32 and X11 producers do; RTCW SP, RTCW MP and ET report it the same way and only the Mac clients report `K_KP_STAR`. The `KP_STAR` name loads and round-trips while no producer reports the key, which is retail's own arrangement on Windows. Source/Unit; no runtime input was observed ([record](2026-09-20-keypad-star.md)) | Unobserved. quakelive_steam.exe names 176 `KP_STAR`, but what its producer reports for that key was not established ([record](2026-09-20-keypad-star.md)) |
| Shared data layout | Frozen Q3 `vmCvar_t`/trajectory/usercmd/entity/player-state boundary PODs (32-bit ABI): 272/36/24/208/468 bytes | Separate frozen QL PODs (32-bit ABI): 276/40/28/236/504 known bytes, including QL-only fields; the retail native snapshot reserves `0x250` player-state bytes, whose trailing `0x58` is the game module's own state, never transmitted and always zero on the client ([tail record](2026-10-08-ql-snapshot-tail.md)) |
| Maps/assets | Q3 pak/pure rules, BSP 46 plus explicitly supported legacy formats, shaders/models/images/audio/cinematics | BSP 47, QL advertisements/material behavior, retail pak/WebUI/native-module assets |
| Platform constraint | Shared x86 binary is the initial all-in-one proof target; broader Q3 platforms remain desirable | Retail client compatibility requires Windows x86; dedicated-server claims are recorded separately |

Supported legacy protocols and demo formats must remain enumerated in code and
tests rather than inferred from the preferred value. The
[Q3 point-release ledger](Q3_POINT_RELEASES.md) now covers Q3Test, Demo Test,
the standalone demo and retail patches separately. Protocols 66/67 have
bidirectional original-engine, pure/non-pure and recording-interchange
evidence for 1.29f/g/h, 1.30 and 1.31; the existing 68/71 gates still apply.
Raw 43/45/46/48 demo formats are explicitly selected from their gamestate,
including id's protocol-46 `.dm_48` files. **Full historical Q3 support remains
open:** Q3Test 35/36/38/39/40 codecs and BSP/module contracts, old QVM APIs and
pre-1.29 live protocols are not implemented by these demo and 66/67 changes.

The [IHV ledger](Q3_IHV.md) covers the supplied February 1999 build separately.
The [dedicated game adapter record](2026-09-21-q3-ihv-game-bridge.json)
adds actual original-DLL startup on all four supplied maps, retained allocations
across map changes/restart, and shutdown/reload in one engine process. Both
normal shutdowns release all native allocations; allocation-budget and client
capacity failures exercise partial-startup cleanup. The adapter marshals native
cvars, entity imports and by-value traces, publishes IHV systeminfo before
spawning, and preserves the other profiles' lifecycle paths. The developer
option enables Windows x86 probes only. The [client record](2026-10-08-q3-ihv-client.md)
adds native client-game hosting and rendered demo playback, and the
[live client record](2026-10-08-q3-ihv-live-client.md) a protocol-33 session
with the original server, and the [server sessions record](2026-10-08-q3-ihv-server-sessions.md)
the original client on an fnTech3 server. Gameplay parity remains open.
The [bot client record](2026-09-21-q3-ihv-bot-clients.json) completes the
remaining five imports on native server client records that are separate from
the retail client array and player projection. Bot allocation, release and
userinfo, the bot scheduler's position and callbacks, the engine's own name,
rate and snaps update and both snapshot getters match the original functions
across 11 allocations, 33 scheduler steps, 12 userinfo cases and 6,528 getter
results. The [snapshot record](2026-09-21-q3-ihv-snapshots.json) then adds the
builder that fills that history: IHV's Quake II-style cluster record, stored for
this profile alone, and the visibility walk that selects what a client sees.
2,048 generated scenarios match the original builder on every selected entity,
area byte and repaired number. The [snapshot message record](2026-09-21-q3-ihv-snapshot-messages.json)
then adds the writer: the delta base a client asks for, its two staleness
rules, Quake II-style entity baselines and the 1,350-byte entity limit. 1,536
generated scenarios produce byte-identical protocol-33 messages, including 366
refused delta requests and 415 truncated entity lists. The
[transport record](2026-09-21-q3-ihv-transport.json) then adds the envelope:
IHV's netchan keeps Quake II's two sequence words and single reliable slot,
fragments Quake III's way and scrambles its payload, and matches the original
packet for packet across 5,202 of them. It is registered as its own wire
profile, so protocol 33 can never fall back to a Quake III contract. The
connectionless handshake and client sessions now carry these packets
([server sessions record](2026-10-08-q3-ihv-server-sessions.md)); original
BSP visibility parity remains open.
Its original server advertises protocol 33, and its native function tables and
length-only demos differ from public Q3Test and retail Q3. BSP 43 translation
now admits all four supplied maps, preserves original checksums and exact inline
brush membership, and validates malformed geometry. Twelve windowed map-load
and screenshot probes pass across GLx/Vulkan/RTX using retail Q3 QVMs.
The material follow-up adds legacy sky/cloud, entity-color and depth-write
commands and an exact-file in-memory `common.shader` repair, with another 12
asset probes and three portal follow-ups passing across the same renderers.
The [fog follow-up](2026-09-20-q3-ihv-fog.json) matches 306 original-function
results, adds flat/animated vertex-density fog to all three renderers and passes
another 12 map probes. IHV fog maps use classic dynamic lights before the fog
overlay; original lighting parity remains open.
Its complete regression run passes 121 Meson targets with two skips and no
failures, including 1,833 Python cases with 72 skips.
The [sky follow-up](2026-09-20-q3-ihv-sky.json) matches 730 original iterator
frames, preserves per-stage lightning colors in GLx/Vulkan/RTX, and passes
12 more map probes plus three capture sequences. The `tim_dm1` fallback sky
is replaced with clouds; the earlier captures retain their original scope.
The sky regression run passes 121 Meson targets with two skips and no failures,
including 1,835 Python cases with 72 skips.
The [native game boundary probe](2026-09-21-q3-ihv-native-game.json) runs the
original game DLL's Init/Shutdown against separate IHV records and bounded
host-owned allocations. Normal startup, missing bot configuration, client
count adjustment and three allocation-failure cases pass, including x86
stack/runtime checks. Exact layout and malformed-boundary tests pass in x86
and an x64 memory-checking build. These components await engine integration
and do not establish native map/gameplay or cgame behavior.
The [protocol-33 codec evidence](2026-09-21-q3-ihv-wire.json) matches 36,192
original-function entity/player/command cases, and adds separate transactional
message and length-only demo readers/writers. The supplied `sli.dm3` parses
and regenerates all 57 messages with identical decoded states; 55 messages
are byte-identical. Two full frames omit unchanged absent-baseline records.
The incomplete `berj.dm3` is rejected cleanly. Seven focused x86 targets pass,
and the two new codecs pass x64 AddressSanitizer/fast-math tests. These are
isolated components, not engine demo playback, recording or live sessions.
The [identity/link follow-up](2026-09-21-q3-ihv-identity-collision.json) registers
a separate, still-gated `q3-ihv` profile and recognizes the supplied loose
installation without executing its DLLs. Retail/IHV mixed markers are ambiguous.
IHV solid packing and link bounds match 2,740 original-function numeric cases
and reach the engine's profile-specific link branch. The reference leaf query
is stubbed empty; original PVS, collision traces and native entity integration
remain separate open gates.
The catalog separates the original engine's 256-client ceiling from its default
of eight and the native game's `maxentities / 8` allocation rule. Original-DLL
allocation and ClientConnect probes pass; retail bot/AAS entry points reject
IHV without allocating. The full regression run passes 126 Meson targets, with
two skips; its Python batch passes 1,837 cases with 72 skips. These are boundary
and regression results, not a native IHV gameplay or interoperability gate.
The subsequent [native game-data adapter](2026-09-21-q3-ihv-game-data.json)
binds the original shared entity prefix and exact player records, validates
published client pointers and preserves private module bytes. Original DLL
Init/ClientConnect/Shutdown callbacks exercise the actual engine adapter, and
the engine link path passes its geometry vectors through native record storage.
Native lifecycle dispatch, shared player projections and live sessions remain
open; these fixtures do not enable the profile.
The [server trace follow-up](2026-09-21-q3-ihv-server-trace.json) adds IHV's
world-zero result, complete startsolid/allsolid replacement, owner filtering,
state/current transforms and original sweep-bound rounding. Actual server
functions match 4,737 original sweeps and 641 point queries with synthetic
contact/CM replies. Native null owners use internal `-1` so entity 1023 remains
valid. Native record guards and all seven later profile policies are tested.
Original BSP collision, PVS/link membership and native import dispatch still
require separate evidence.
The [native configstring follow-up](2026-09-21-q3-ihv-configstrings.json) adds
protocol-33 configstring updates and preserves ordered message events. Its
20,100-byte native string record matches 512 original-parser cases, including
unchanged values, repacking, deletions and change notifications. Malformed and
overflowing messages retain prior state; native cgame dispatch remains open.
The [native client data follow-up](2026-09-21-q3-ihv-client-data.json) connects
decoded packets to exact client records and ordered callback delivery. History
getters match 33,152 original cases; complete snapshots and entity reads match
40 synthetic frames and all 56 frames of `sli.dm3`. The same packets pass the
original cgame's data callbacks with Init bypassed and packet-graph history stubbed.
Malformed packets cause no callbacks, and callback failure clears the session.
This closes isolated client data transfers, not native startup or playback.
An additional 61 original Init-prefix probes check cvar/data adapters and
failure cleanup through the collision/renderer map-import boundary, with
synthetic service replies; completed native initialization remains open.
The [model/cgame follow-up](2026-09-21-q3-ihv-cgame-runtime.json) adds validated
MD3 frame/tag storage and the native collision-model mirror that cgame reads
directly. All 243,600 pose components from 79,625 queries match the original
interpolator in the recorded x86 run, including all 111 supplied models.
An isolated host now completes original cgame Init, 56 ready frame updates and
draw callbacks, capturing 124 scene submissions with actual model metadata
and synthetic collision/resource services. Tag/scene callback failures clear
the client session. The older helper called "prediction" is packet-graph
history; the new host runs it unpatched. These observations supersede the
initialization-prefix limitation, but do not establish engine startup,
rendered playback, live prediction, actual collision or audio delivery.
The [owned scene boundary](2026-09-21-q3-ihv-scenes.json) now checks and copies
all 124 cgame submissions into separate engine-owned arrays, resolves resources
by kind and retains native effect fields. Original numeric routines confirm
262,144 area-visibility decisions and 147,456 entity-color components; IHV's
visible-area bits must be complemented for retail's blocked-area convention.
These tests use generated leaves/PVS and captured scenes. Native material,
effect, particle and backend rendering adapters, actual BSP/PVS and rendered
demo comparison remain open.
The [skin follow-up](2026-09-21-q3-ihv-skins.json) implements IHV's case-sensitive
registration, cached default-skin fallback, bounded ordered shader parsing and
surface-ordinal material selection in GLx, Vulkan and RTX, including ray geometry.
Original numeric comparisons cover 54 registrations and all three supplied skin
files; other-profile and sanitizer checks cover the shared paths. This adds
Unit/Build/Fixture evidence, not native rendered playback or a completed material
gate. It also fixes existing RTCW nonfinite-scale rejection under fast-math and
RTX negative MD3 skin indexing found during those checks.
The [particle follow-up](2026-09-21-q3-ihv-particles.json) adds owned particle
submission and per-view triangles to all three renderers, including RTX ray
geometry and camera-dependent cache invalidation. Original numeric comparisons
match 1,130,688 vertex components, the 1,024-byte procedural texture and original
blend/depth callbacks. Shader, fog and sky compatibility paths now also admit
the separate `q3-ihv` profile; other profiles keep their prior behavior. Renderer
API 27 introduced that entry point. This is Unit/Build/Fixture
evidence; native scene-adapter dispatch and rendered comparisons remain open.
The [entity follow-up](2026-09-21-q3-ihv-entities.json) adds atomic, owned entity
batches through renderer API 28. Separate native flags drive primary/portal
visibility and primary-camera axes; the origin interpolation helper and native
beam vertices match original numeric observations. All three renderers register
the case-sensitive `*beam` name, submit an unfogged red additive material and
use the entity transform; RTX ray geometry uses the same beam vertices and
tracks exact native pose changes. The scene reader now accepts null models
while still rejecting unknown nonzero resource tokens. Flag `0x80` is explicitly
rejected until sorted transform and lighting retention work across raster/ray
paths. Actual model lighting, animation controls, portal behavior and
native scene-adapter integration remain open. This remains Unit/Build/Fixture
evidence, with no rendered native playback claim.
The [animation follow-up](2026-09-21-q3-ihv-animation.json) replaces retail MD3
math for native entities in GLx/Vulkan/RTX and ray geometry. A private sine table,
combined vertex/origin interpolation, normal arithmetic and expanded cull bounds
match selected original functions: 451,584 generated animation components,
5,408,046 components from every frame of all 111 models, and 165,888 culling
cases. Surfaces stage before vertex publication; finite/span/overflow checks and
eight fast-math sanitizer fixtures pass. Other profiles retain their renderer
paths. This is Unit/Build/Fixture evidence; native scene integration and rendered
parity remain open.
The [model-light follow-up](2026-09-21-q3-ihv-model-light.json) implements native
raw lightmap sampling, BSP traversal, dynamic lights, fixed direction and diffuse
colors in all three raster renderers. Actual translated-map light loading matches
32,768 original traces across four maps; another 32,768 generated tree traces,
24,576 dynamic-light cases and 34,816 diffuse vertices match original functions.
The native `r_lerpmodels` float toggle now controls raster/ray interpolation and
invalidates affected ray geometry. These are CPU/fixture results; native scene
dispatch, sorted flag-`0x80` retention and rendered lighting parity remain open.
**Full IHV support remains OPEN:** sustained and fought play and legacy
material/collision parity are not established. The
client game now plays the supplied demo ([client record](2026-10-08-q3-ihv-client.md))
and joins the original server ([live client record](2026-10-08-q3-ihv-live-client.md)),
and the original client joins an fnTech3 server
([server sessions record](2026-10-08-q3-ihv-server-sessions.md)). Its demos are
recorded and play in both engines ([recording record](2026-10-08-q3-ihv-demo-recording.md)).
Original portal, clamp and lighting behavior
still need validation.

### Return to Castle Wolfenstein

| Contract | Single player (`rtcw-sp`) | Multiplayer (`rtcw-mp`) |
| --- | --- | --- |
| Installation identity | `main/pak0.pk3` with SP marker evidence; shared RTCW Steam AppID 9010 where applicable | `main/mp_pak0.pk3` with MP marker evidence; same title/AppID |
| Configuration and module selection | `wolfconfig.cfg`; `sp_` archives selected and `mp_` archives excluded from the mode policy | `wolfconfig_mp.cfg`; `mp_` archives selected and `sp_` archives excluded from the mode policy |
| Preferred network/demo contract | Protocol 49; sustained wire/demo validation remains open | Protocol 60; retail peer and demo validation remain open |
| Module and state boundary | Separate SP legacy-native imports/exports and frozen SP records; campaign AI and savegame lifecycle | Separate MP legacy-native imports/exports and frozen MP records; native-module pure proof and multiplayer state |
| Maps/assets | RTCW BSP 47, MDC/MDS and RTCW materials, cinematics, audio, and navigation data | RTCW BSP 47 with mode-specific module/asset behavior and multiplayer objectives |
| Runtime availability | Available in every build; recorded installed probes use the shared Windows x86 client | Available in every build; switching from SP starts a new process profile |

Contract facts come from the [game-manager foundation](2026-09-14-rtcw-contracts.md)
and subsequent checkpoints below. An installation containing both modes requires
an explicit profile choice. The [RTCW gate matrix](#rtcw-gate-matrix) records
which contracts have runtime evidence and which still need closure. The
[pure-name investigation](2026-09-17-rtcw-mp-pure-names.md) establishes that the
retail MP client, unlike retail ET, needs no `sv_pakNames`; that check is the
only RTCW behaviour the ET pure work left open, and it required no change.

## Compatibility and gate matrix

Status values are `PASS`, `PARTIAL`, `OPEN`, or `BLOCKED`. A `PASS` applies only
to the evidence level named in the same cell. This matrix covers Q3 and QL;
the [RTCW gate matrix](#rtcw-gate-matrix) covers the other two profiles and is
equally required for unified sign-off.

| Surface | Q3 status/evidence | QL status/evidence | Required promotion gate |
| --- | --- | --- | --- |
| Immutable early profile selection | **PARTIAL — Unit/Build/Fixture.** Explicit aliases, marker precedence, ambiguity rejection, and immutability have tests; automatic and explicit dedicated startup pass against a legitimate installation. Canonical profile mirrors survive startup-variable replay. [Startup command overflow](2026-09-08-startup-command-bounds.md) now fails explicitly before a trailing profile override can be discarded. | **PARTIAL — Unit/Build/Fixture.** Same seam and successful automatic/explicit dedicated startup against retail assets. | Bounded known-retail pak metadata recognition now rejects filename-only placeholders and swapped archives; see the [identity checkpoint](2026-09-08-pak-identity.md). Payload integrity, broader archive variants, mixed trees, and restart behavior remain open. Q3/QL Steam discovery now has Unit/Build/Fixture evidence; see the [discovery checkpoint](2026-09-07-steam-discovery.md). |
| Project identity and packaging | **PARTIAL — Unit/Build/Fixture.** Canonical metadata and primary output names are migrated. The engine-only x86 stage passes dependency/root-archive checks and profile probes. | **PARTIAL — Unit/Build/Fixture.** The same artifacts use the profile-scoped engine package and retain bound renderer/WebUI sidecar names; the proprietary provider is excluded. | Provider staging is removed and known proprietary libraries are rejected; see the [packaging correction](2026-09-07-engine-only-packaging.md). Validate clean CI, reproducible artifacts and remaining packaging paths without renaming external contracts. |
| Filesystem, configs, pak order, and pure checks | **PARTIAL — Unit/Live.** Q3 config order and two-module pure proof are restored. Local QVM clients pass pure admission; malformed and oversized proofs have executable regressions. | **PARTIAL — Unit/Live.** Single-native-module pure proof and profile-specific config/service paths are preserved; local native clients pass pure admission. | [UDP fixture-pak downloads](2026-09-08-udp-pak-download.md) now complete byte-for-byte with pure admission in both reference directions for both profiles, baseline and impaired transport. The [interrupted UDP retry checkpoint](2026-09-08-download-retry-and-zombie-reconnect.md) verifies replacement of a retained partial file and corrects same-endpoint zombie-slot reconnect behavior in both profiles. The [whole-file pak integrity correction](2026-09-08-whole-file-pak-integrity.md) rejects short reads and CRC-mismatched payloads before exposing buffers, with both-profile corrupt/valid config and legitimate client runtime checks. The [allocation-bounds follow-up](2026-09-08-file-allocation-bounds.md) rejects unrepresentable whole-file lengths and prevents temporary-allocator alignment/header/capacity arithmetic overflow, with synthetic oversized-pak rejection in both profiles. Writing a latched cvar's current value cancels its pending value again ([latch record](2026-09-18-cvar-latch-cancel.md)), and range checks now cover values a config set before the cvar was registered, such as Quake III's own `com_hunkMegs 56` ([range record](2026-09-18-cvar-range-checks.md)); both apply to every profile. When a server's pak list arrives, the client now forgets references held by paks the list leaves out, which cannot be read while it is in force: a reference the menus made between two connections had reached the pure proof of a reconnect that needed no filesystem restart, and retail Holomatch dropped fnTech3's client as unpure for naming a non-retail texture pak the server lacked. A client with no such pak sends the same proof as before, in every profile ([legacy-server record](2026-10-05-legacy-server-hosting.md)). Golden search-path/pure transcripts and clean-install probes, broader home/config/mod paths, HTTP/failure downloads, case/path behavior, streaming integrity and server-referenced paks remain required. |
| Module ABI and VM/native loading | **PARTIAL — Unit/Live.** Retail qagame, cgame, and UI QVMs load and run a local session. Guarded synthetic [cgame](2026-09-08-q3-native-cgame.md), [UI](2026-09-08-q3-native-ui.md), and [qagame](2026-09-08-q3-native-qagame.md) DLLs also pass actual legacy vmMain/syscall transfers and callbacks; qagame covers shared-entity/collision access, restart and invalid-binding recovery. The [native loopback pair](2026-09-08-q3-native-snapshot-roundtrip.md) also checks initial/updated player and entity fields in client snapshots plus a live usercmd return. Frozen POD adapters, transactional DLL loading, QVM bounds, cgame traps/extensions, UI policy, and cvar flags have executable regressions. UI and cgame key events and key imports use Quake III's own key numbers, checked against quake3.exe's key table; they had been this engine's, whose mouse wheel q3_ui's menus read as joystick buttons that press the item under the cursor ([record](2026-09-19-q3-key-numbers.md)). | **PARTIAL — Unit/Live.** Retail qagame API 10 and cgame/UI API 8 load and run through the same client. Guarded synthetic native imports now exercise config/usercmd transfers through the running client; see the [native import checkpoint](2026-09-08-native-guarded-imports.md). Its [opt-in runner](2026-09-08-native-runner.md) now supports packed installs and private-desktop normal-exit verification. Structured contracts remain separate. The native snapshot's last `0x58` player-state bytes are the retail game module's own medal and scoring state: `quakelive_steam.exe` copies them into server frames but never encodes them, and every snapshot its client hands the client game holds them as zeroes, as fnTech3's does ([tail record](2026-10-08-ql-snapshot-tail.md)). Modules keep this engine's key numbers, which match all 111 that quakelive_steam.exe names ([record](2026-09-19-q3-key-numbers.md)). Game import 192 marks the client whose retail game ran `ragequit`, and its drop is announced as `print "%s ^1rage^7quits\n"`, as `quakelive_steam.exe` does; fnTech3 had bound the import as the pure check's `cp` mark and announced `<name>^7 disconnected`. A fnTech3 observer received retail's form from a fnTech3 server running the retail game ([rage quit record](2026-09-29-ql-rage-quit.md)). | Renderer configuration PODs and the Q3 cgame/UI adapter are frozen; see the [glconfig checkpoint](2026-09-08-glconfig-boundary.md). Both profiles marshal frozen configstring-transfer layouts; see the [game-state checkpoint](2026-09-08-gamestate-boundary.md). Incoming bot commands now marshal Q3/QL PODs with bounds checks; see the [bot-command correction](2026-09-08-bot-usercmd-boundary.md). QL user-command outputs use the frozen 28-byte adapter at cgame/qagame boundaries; see the [user-command checkpoint](2026-09-08-ql-usercmd-boundary.md). Complete the boundary audit, and broaden Q3 native and packaged-client module coverage. |
| Live network codec and handshake | **PARTIAL — Unit/Live.** Local protocol-68 sessions and both directions against a freshly built clean FnQuake3 peer at the pinned baseline reach active state after pure admission (71 with fnTech3 as client, 68 with fnTech3 as server); see the [pinned-peer checkpoint](2026-09-08-pinned-q3-interop.md). Both directions also pass [controlled loopback loss, delay and duplication](2026-09-08-q3-impaired-interop.md) and [initial gamestate fragment-loss recovery](2026-09-08-q3-fragment-recovery.md). The [second-fragment checkpoint](2026-09-08-second-fragment-recovery.md) also passes and corrects earlier protocol-68-only claims. [Terminal-fragment loss](2026-09-08-terminal-fragment-recovery.md) recovers in both peer directions. Shared lineage still limits peer independence. Field parity, truncated messages, and float encodings are tested for protocols 68/71. Live traffic with one sequenced datagram in eight corrupted in both directions drops each of 20 sessions cleanly, on the client's own decoder checks or the server's user-command check, with no crash, caught fault or hang, and the server answers throughout ([corrupted-traffic record](2026-09-26-net-corruption.md)). Scripted loopback sessions with retail `quake3.exe` 1.32 pass in both directions and on an fnTech3 listen server -- join, chat both ways, a map change, `map_restart`, reconnect, server quit, a kick, a UDP pak download inside a mod with pure admission, and a retail and an fnTech3 client on one server -- as a retail-only control does; a public server heartbeats `master.quake3arena.com`'s port as retail does, and both engines' browsers list both servers ([legacy-server record](2026-10-05-legacy-server-hosting.md)). | **PARTIAL — Unit/Live.** Local protocol-91 sessions reach active state. FnQL's four connectionless checks -- protocol 91 in `getinfo`, the FnQL nonce echo, retail's binary SteamID/ticket challenge and silence for a malformed ticket -- pass against the current server ([handshake record](2026-09-26-ql-protocol91-handshake.md)). Both directions against the existing FnQL peer also pass [periodic loss/delay/duplication and initial gamestate-fragment loss](2026-09-08-ql-impaired-interop.md), with pure admission and native join UI/world rendering; [second-fragment loss](2026-09-08-second-fragment-recovery.md) and [terminal-fragment loss](2026-09-08-terminal-fragment-recovery.md) also recover. Reference source revision and active team gameplay remain unverified. Field parity and malformed-input tests pass; invalid usercmd batches drop only their sender without partial execution. Corrupted live traffic drops each of 20 sessions the same clean way ([corrupted-traffic record](2026-09-26-net-corruption.md)). A server started from the client admits players by `sv_serverType` as retail's does: offline drops every packet from outside, LAN refuses a challenge from outside the LAN, Internet admits anyone; neither its game type nor the local match's single-player flag closes its challenge any more, and live a second fnTech3 client joined a Race match on LAN and Internet and not offline ([server-type record](2026-10-06-ql-listen-server-type.md)). | A [client-fragment checkpoint](2026-09-08-client-fragment-recovery.md) verifies exact, ordered delivery of six reliable commands after first-client-fragment loss in all four reference configurations. [UDP fixture-pak downloads](2026-09-08-udp-pak-download.md) also pass baseline and impaired reference transport. The [reliable acknowledgement unwind fix](2026-09-08-reliable-ack-error-unwind.md) prevents a rejected header from corrupting disconnect cleanup state. Golden packet transcripts, malformed/fuzz corpus, broader fragmentation/reliable/pure/download coverage, then bidirectional retail interop for each profile remain required. |
| Demos | **PARTIAL — Unit/Live.** New local `.dm_68` recordings play to normal completion. The [shipped-demo pacing check](2026-09-08-shipped-demo-pacing.md) also verifies normal-speed `.dm_66` playback and an engine world/HUD screenshot. Shipped four.dm_66/four.dm_68 bytes are identical, limiting their independence as wire samples. Executable tests preserve protocol-71 fallback and extension behavior. | **PARTIAL — Unit/Live.** New local `.dm_91` recordings play to normal completion after fixing an incorrect `.dm_68` suffix for QL captures without voice messages. The [retail demo checkpoint](2026-09-08-ql-retail-demo-interop.md) adds normal/timedemo cross-engine playback in both directions with complete short warmup recordings and world/HUD screenshots. | The [completion handoff checkpoint](2026-09-08-demo-completion-handoff.md) fixes premature exit when nextdemo starts another recording and verifies Q3/QL truncated-header/body completion plus intact recovery playback with the same Windows x86 client. The [invalid-length runtime checkpoint](2026-09-08-demo-invalid-length.md) verifies negative and oversized envelope rejection in both profiles, with bounded post-error liveness and explicit harness termination. The [pause/resume checkpoint](2026-09-08-demo-pause-resume.md) observes fixed QL frames and Q3 world rendering during cl_freezeDemo, followed by normal resumption; The [command-history correction](2026-09-08-paused-command-history.md) removes Q3's spurious paused connection warning, with Windows/Linux executable regressions and both-profile runtime evidence; the Q3 lagometer strip that still moves while paused moves identically in retail `quake3.exe` on the same shipped demo, so it is Q3 cgame behaviour, not an engine defect; see the [paused-lagometer record](2026-09-18-q3-paused-lagometer.md). The [retail controls comparison](2026-09-08-retail-demo-controls.md) verifies that QL restores timescale to retail's 0.1 minimum, holds cl_freezeDemo frames identically, and exercises the existing advance-and-hold console sequence under both profiles. Its paused-restart follow-up verifies retained pause, fixed first-frame rendering and eventual completion under both profiles, matching retail QL. The [drop-recovery investigation](2026-09-08-demo-drop-recovery-limit.md) confirms that ERR_DROP clears queued recovery scripts under both profiles; failed recovery attempts and premature QL control completions are recorded without claiming post-error recovery. The [RTCW demo record](2026-09-18-rtcw-demos.md) makes demo protocols profile-owned: Q3 keeps 66/67/68/71 and `.dm3` in its historical order, QL keeps 91, and neither admits RTCW's 49/60; fresh local Q3 `.dm_68` and QL `.dm_91` record, replay and timedemo runs pass with the changed client. The [damaged-demo record](2026-09-18-demo-faults.md) plays fixed envelope faults and 40 seeded body corruptions of two Q3 demos and one QL demo: no run crashes or hangs, where retail `quake3.exe` 1.32 crashes on 40 of the same 94 Q3 variants. Record and play every claimed format; broaden truncation, post-error recovery, seeking, timedemo, protocol switching, and unchanged termination behavior. |
| BSP, collision, and renderer asset decode | **PARTIAL — Unit/Live.** q3dm6 loads with GLx/Vulkan/RTX raster; the recorded RTX black-polygon defect is fixed and verified on the frozen demo. Separate Q3 x64 native probes cover baked lightmap and vertex-lit world materials. See the [RTX round](2026-09-06-rtx-round.md). Models scale the light grid's ambient by `quake3.exe` 1.32's `r_ambientScale`, `0.6` (`0x0045152a`; [ambient scale record](2026-09-25-ambient-scale.md)). The replacement path tracer (`2026-09-24-rtx-path-tracer.md`) traces eight retail maps' worlds, models and weapon with their shader materials and the maps' own q3map lights, with no Vulkan validation messages; its traced direct light is within 16% of the baked lightmaps' frame brightness on all eight. Its compute backend draws the same frozen frames as the ray-tracing pipeline on six maps, and the shared x86 client traces ten retail Q3 maps with it at 0.87-1.27 of their raster frames' brightness. Stencil shadows (`cg_shadows 2`) darken by GL's 0.6 in the Vulkan and RTX renderers too ([stencil shadow record](2026-09-24-stencil-shadow-finish.md)). Of 101 damaged copies of `q3dm1`, none crashes, hangs or faults since the patch-grid guard and the loaders' index checks, which refuse the seven that faulted on unchecked node, leaf, leaf-surface and brush indices ([damaged-map record](2026-09-26-bsp-faults.md), [index-check record](2026-09-27-bsp-index-checks.md)); the checks change how none of the 69 installed maps loads, in any renderer. The loaders also walk each node tree once before anything follows it, and refuse a child that leads back to its own node or one above it, a subtree two nodes share and a path through more than 2048 nodes: of three more damaged copies of `q3dm1` built that way, the two with a cycle crashed without the walk and the one with a share at every node down its deepest path loaded in 10.8 s rather than about 4, and all three are refused now; the walk refuses no installed Quake III tree ([node-tree record](2026-09-27-bsp-node-trees.md)). Built without the walk, the two cycles no longer end the process: the stack overflow their traces cause is reported as an `ERR_DROP` naming `STACK_OVERFLOW` and the client carries on, where the report itself used to fault ([stack overflow record](2026-09-28-stack-overflow-reports.md)). Retail `quake3.exe` 1.32's dedicated server and fnTech3's answer one synthetic native module's 75,240 traces on six stock maps -- world and linked brush models, box and capsule hulls -- with every point contents, link flag and box query equal and all but ten traces within 0.01 units; those ten follow the double-precision collision arithmetic fnTech3 inherits from Quake3e, at curve bevels, tied brushes and very long traces, left as a decision ([trace record](2026-10-08-q3-traces.md)). | **PARTIAL — Unit/Live.** campgrounds BSP 47 loads with all three raster renderers and produces engine screenshots; the final shared RTX raster probe passes. Models scale the light grid's ambient by retail's `r_ambientScale`, `10`, a player option bounded to 1 through 100 and kept in `repconfig.cfg` (`quakelive_steam.exe` `0x00449013`); fnTech3 had used Quake III's `0.6` cheat, which drew a player model on `campgrounds` at 41% of retail's brightness ([ambient scale record](2026-09-25-ambient-scale.md); `tests/ambient_scale_defaults_tests.py`). The shared x86 client's path tracer (compute backend) traces twelve retail maps without error; their live views are the join-match menu, so they are not compared. No retail QL map keeps a light entity (q3map2 leaves them out), so the tracer lights QL's worlds from their lightmaps and its models from the light grid by default; a frozen demo frame reads 0.963 of raster's (`2026-09-24-rtx-path-tracer.md`). Stencil shadows (`cg_shadows 2`) darken by GL's 0.6 in the Vulkan and RTX renderers too ([stencil shadow record](2026-09-24-stencil-shadow-finish.md)). Of 105 damaged copies of `campgrounds`, none crashes, hangs or faults: the index checks refuse the five that faulted ([damaged-map record](2026-09-26-bsp-faults.md), [index-check record](2026-09-27-bsp-index-checks.md)), and change how none of the 149 installed maps loads, in any renderer. The loaders' node-tree walk refuses three more copies, whose trees loop back on themselves or share a subtree at every node down the deepest path: without it the two cycles crashed and the share loaded. It refuses no installed Quake Live tree ([node-tree record](2026-09-27-bsp-node-trees.md)). Built without the walk, the two cycles now end in a reported stack overflow, in the renderer's world load, rather than a crash ([stack overflow record](2026-09-28-stack-overflow-reports.md)). | Broaden maps and collision paths, compare equivalent screenshot scenes, and prove live advertisement presentation/fallback. |
| Other assets (models, shaders, images, audio, cinematics, UI resources) | **PARTIAL — Unit/Live.** WAV decoder fixtures and a measured synthetic engine audio path pass; seeded corruptions of five installed Q3 movies pass the shared RoQ decoder under AddressSanitizer and the running client ([RoQ fuzz record](2026-09-18-roq-fuzz.md)); the remaining asset inventory is not signed off. The script precompiler that reads menus and bot files joins the strings defines expand to in a loop, where a run of 1,000 had overflowed its stack and 20,000 in a bot character had faulted the dedicated server; every installed Quake III, Quake Live, RTCW, ET, Elite Force and Jedi menu and bot file reads to the same tokens as before ([precompiler record](2026-09-29-precompiler-string-runs.md)). | **PARTIAL — Unit/Live.** The same WAV checks and measured engine audio path pass; broader assets and WebUI resource regression remain incomplete. | The [WAV payload correction](2026-09-08-wav-payload-integrity.md) rejects incomplete whole loads and corrects short-read stream counts, with executable Windows/Linux regressions and exact decoded-byte comparison for 631 Q3 and 538 QL retail WAV paths. The [container-bounds follow-up](2026-09-08-wav-container-bounds.md) rejects truncated/out-of-container headers and checks packed/loose seek results, while preserving terminal odd-data acceptance observed in stock Q3 assets. It repeats the identical corpus output and clarifies that the installed Q3 corpus includes 18 add-on paths. The [extensible identity correction](2026-09-08-wav-extensible-identity.md) validates full PCM/float GUIDs and extension lengths, with malformed GUID fixtures and eight exact synthetic decodes. Valid-bit/channel-layout semantics and audible/backend validation remain open. The [engine audio-output checkpoint](2026-09-08-engine-audio-output.md) measures silence controls, 997 Hz output and half-volume gain response through the same client under both profiles, using file output without speakers. The [streaming-music checkpoint](2026-09-08-engine-music-streaming.md) adds PCM16/24/float32 intro-loop handoff, channel routing, stop-to-silence and malformed-track recovery through both running profiles. The [rate-conversion investigation](2026-09-08-stream-resampling-gap.md) exposed per-chunk frame/phase drift. The [persistent resampling correction](2026-09-08-persistent-stream-resampling.md) now passes exact partition comparisons, queue/fallback checks, both-profile rate conversion and mixed-rate stop/replay probes; broader raw/device lifecycle and performance coverage remain open. The [PCM8 WAV correction](2026-09-08-wav-pcm8-stereo.md) normalizes unsigned multichannel WAV bytes at the codec boundary, preserving raw-module conventions and correcting the measured stereo silence/tone defect in both profiles. The [multichannel endian correction](2026-09-08-wav-multichannel-endian.md) swaps every complete PCM16 scalar sample across channels and partial frames, with failing-before simulated-endian regressions and Linux sanitizer coverage; full big-endian runtime remains unverified. The [extensible precision correction](2026-09-08-wav-extensible-precision.md) rejects precision larger than its container, preserves reduced-precision PCM scaling and verifies malformed-track recovery followed by measured intro/loop output in both profiles; the speaker-mask correction is recorded below. The [speaker-mask investigation](2026-09-08-wav-speaker-mask-gap.md) recorded center-to-right misrouting. The [speaker-routing correction](2026-09-08-wav-speaker-routing.md) preserves internal mask metadata, mixes non-native layouts by speaker position in both backends, aligns music reads to whole frames, and verifies both-profile center/control, static, layout-transition, six- and three-channel file output; hardware surround and legacy DMA output remain unverified. The [loaded-speaker bounds follow-up](2026-09-08-speaker-buffer-bounds.md) validates source ranges before routing and avoids metadata-size overflow after rejected conversion, with a failing-before executable regression and repeated both-profile loaded-center output. The [SDL legacy-output checkpoint](2026-09-08-sdl-legacy-output.md) verifies both-profile streamed/static speaker routing and seven-channel file output on a separately built SDL x86 engine, while recording persistent silence gaps as an open continuity issue; native hardware output remains unverified. The [legacy timing trace](2026-09-08-legacy-timing-trace.md) links the recurring 50 ms gap component to the cursor offset and records near-half-second update intervals despite at most 5 ms of pre-paint work; timing defaults remain unchanged and continuity remains open. The [client-stage trace](2026-09-08-client-screen-timing.md) places measured long stalls inside screen updates under both profiles; UI/render/presentation attribution and continuity remain open. The [renderer-stall investigation](2026-09-08-renderer-stall-localization.md) measures delays at OpenGL synchronization points and removes them in a front-buffer control on the private desktop. The [locked-session follow-up](2026-09-25-locked-session-presentation-stall.md) finds those delays only in runs made while the Windows session was locked, and none on an unlocked private desktop, so continuity with normal presentation can be measured there; it has not been. The [converted WAV short-read correction](2026-09-08-wav-short-read-alignment.md) preserves scalar alignment across positive filesystem short reads, with failing-before partition regressions, sanitizers and unchanged both-profile corpus output. The [native WAV follow-up](2026-09-08-native-wav-short-reads.md) accumulates positive short reads for native PCM as well, with simulated-endian regressions and exact streaming reconstruction of all 1,169 installed WAV paths under 509-byte filesystem reads. The [Ogg payload correction](2026-09-08-ogg-payload-bounds.md) validates decoded-size metadata before narrowing and rejects incomplete whole loads, with executable fault/cleanup regressions and both-profile retail-fixture smoke output; callback and broader Ogg corpus gates remain open. The [Ogg callback correction](2026-09-08-ogg-callback-bounds.md) bounds read/seek arithmetic and verifies real positions, with exact decoded comparison across 559 retail Ogg paths and both-profile compressed-pak playback smoke evidence; broader malformed-container gates remain open. The [Vorbis speaker-order correction](2026-09-08-vorbis-speaker-order.md) fixes observed center-to-right routing, preserves encoded-field tags and passes both-profile three/six-channel loaded/music file-output probes under OpenAL and SDL legacy; wider hardware and layout coverage remains open. The [older OpenAL fallback correction](2026-09-08-openal-unmatched-speakers.md) preserves unmatched speakers when direct remix is unavailable, with failing-before Linux center-channel output, corrected loaded/music measurements and both-profile Windows regression evidence. Per-format fixtures plus windowed runtime scenes for both profiles; confirm error handling and cross-renderer/audio consistency without redistributing assets. |
| WebUI native requests and pending avatars | **PARTIAL — Source.** QL-only activation and frame guards are preserved; this port does not enable WebUI for Q3. | **PARTIAL — Unit/Build/Fixture.** Notification scheduling, bounded Unicode transport, avatar backoff/batching, and reload cleanup are tested; fnTech3's production adapter passes an offscreen retail-runtime probe. See the [port record](2026-09-06-webui-request-port.md). The retail Video page shows its Ambient Light Scale row again, and the configuration snapshot publishes `r_ambientScale`, now a player option ([ambient scale record](2026-09-25-ambient-scale.md)). | The web document survives `vid_restart`, a resize and switches between GLx, Vulkan and RTX with the same capture, and a dead browser child leaves the native interface in its place ([lifecycle stress record](2026-10-08-ql-webui-lifecycle-stress.md)). Confirm affected Linux/Proton gameplay performance, live Steam-avatar callbacks through the engine, and device loss. The source-reported DataPak document-query limitation remains open. |
| Renderer API/lifecycle | **PARTIAL — Unit/Live.** All three modules initialize, render a local session, and shut down. Q3 x64 native RT survives renderer restart and windowed resize with Vulkan validation; that evidence applies to the diagnostic Q3-only executable. The path tracer repeats restart, resize, map change and raster/RT switches with no validation messages (`2026-09-24-rtx-path-tracer.md`). | **PARTIAL — Unit/Live.** All three modules initialize, render the native join UI/world, and shut down with WebUI disabled. This machine's x86 driver lacks the ray-tracing pipeline; the shared x86 client path traces with its compute backend in `rtx_rt_require 1` runs, then restarts to raster, but no 32-bit Vulkan validation layer is installed to check it. | The shared x86 client's hardware backend on a driver that offers it, x86 validation-layer runs, broader renderer switch/restart and screenshot parity; HDR, performance, and WebUI device loss. RTX raster startup does not prove ray tracing. |
| Steam and optional services | **PARTIAL — Unit/Build/Fixture.** AppID 2200 discovery supplies the Q3 base path without enabling QL services or adding a second Q3 Steam search tree. | **PARTIAL — Source/Unit/Live.** The [retail QL checkpoint](2026-09-08-ql-retail-auth-interop.md) observes platform authentication validation, pure admission and native join UI/world with the retail client on fnTech3. The [dedicated-server follow-up](2026-09-08-ql-dedicated-retail-interop.md) adds fnTech3 client admission and first-person warmup against the Linux x86 retail server. Broader authentication gates remain open. | Broader Q3/QL Steam-path validation; QL logged-in/offline/failure behavior; session-ticket admission both ways; callback/shutdown fault tests; live match reports, unattended Workshop download, and sustained GameServer stress. |
| Dedicated server | **PARTIAL — Build/Unit/Fixture.** [Retail bot combat](2026-09-08-retail-bot-runtime.md) now passes with the corrected incoming command boundary. Its bot navigation files are checked before the botlib follows them -- every number one table gives into another, the node tree walked once from node 1, lumps inside the file and the ranges Quake III's routing lays out -- and a damaged one is refused while the map runs on without bots: on `q3dm1` a node naming itself had hung the server and a child or plane out of range had faulted it. All 92 installed Quake III and Team Arena files pass, and each of the 61 maps with navigation loads it, routes and takes a bot as before ([navigation record](2026-09-28-aas-index-checks.md)). A route cache the botlib saved (`bot_saveroutingcache 1`, `maps/NAME.rcd`) is checked cache by cache against the map before the botlib reads it: the dumps the server wrote for `q3dm7`, `q3dm1`, `q3dm17`, `q3ctf2` and Team Arena's `mpteam1` now load and stay in use, where the old reader, taking each cache's size from its type, faulted the server on every one, and a damaged dump is refused whole while the level plays on and routing builds its caches as it goes ([route cache record](2026-09-28-route-cache-checks.md)). A dump asked for before routing exists, while `bot_forcereachability` has the botlib compute `q3dm7`'s reachability again, is no longer written: the server had faulted in `AAS_WriteRouteCache`, and now warns and plays on ([record](2026-09-29-route-cache-before-routing.md)). q3dm6, retail qagame QVM, botlib, pure=1 info response with protocol 68, and clean shutdown pass for automatic and explicit profile selection. A running server's `sv_maxclients` change now spawns the level again instead of overflowing the stack ([client-slot stack correction](2026-09-24-client-slot-stack-overflow.md)). A map load and an immediate `map_restart` now run each settling frame, and the last one, at the current time before advancing it, as `quake3.exe` 1.32 does, so the game's first frame runs at its start time; the game prints no millisecond time that a live run could compare, so this rests on disassembly and fixtures ([settle order record](2026-09-24-settle-frame-order.md)). A stack overflow in the server's frame is reported as an `ERR_DROP` naming `STACK_OVERFLOW`: one dedicated server sent three overflows and an access violation by rcon, with and without a map, reported each in its rcon reply, kept answering and quit with status 0 ([stack overflow record](2026-09-28-stack-overflow-reports.md)). Routing to an area entered by more than 128 links now starts from a row as long as its links, where `quake3.exe` 1.32 read its stack past a 128-entry array; Team Arena's `mpq3tourney6` has such an area and loads, routes and takes a bot as before ([goal-link record](2026-09-28-aas-goal-links.md)). | **PARTIAL — Build/Unit/Fixture.** campgrounds/ffa, retail API-10 qagame, botlib, pure=1 info response with protocol 91, and clean shutdown pass using the same binary. The same navigation checks refuse a damaged file on `campgrounds`, where the same damage had hung or faulted the server; all 149 installed files pass, and all 149 maps load their navigation, route and take a bot as before (`qztraining` refuses `addbot` in both builds) ([navigation record](2026-09-28-aas-index-checks.md)). The same checked reader loads the route caches the server wrote for `falloutbunker` and `campgrounds`, where the old one faulted it ([route cache record](2026-09-28-route-cache-checks.md)). Asking for a dump while `campgrounds`' reachability is computed again warns instead of faulting the server ([record](2026-09-29-route-cache-before-routing.md)). A map load and an immediate `map_restart` now run each settling frame, and the last one, at the current time before advancing it, as `quakelive_steam.exe` does; disassembly and fixtures ([settle order record](2026-09-24-settle-frame-order.md)). The same four faults by rcon are reported and survived, and the server quits with status 0 ([stack overflow record](2026-09-28-stack-overflow-reports.md)). The same row serves `blackcathedral` and `reflux`, whose areas entered by up to 198 links `quakelive_steam.exe` routed through stack words past its 128-entry array, the first two its per-process `/GS` cookie; both load, route and take a bot as before ([goal-link record](2026-09-28-aas-goal-links.md)). | Windows x86 and native Linux i686 discovery/map probes now pass for both profiles; see the [portability checkpoint](2026-09-07-build-portability.md). Windows dedicated early and initialized fatal errors now exit with status 1 without console dismissal; see the [fatal-exit checkpoint](2026-09-08-windows-dedicated-fatal.md). The [retail bot checkpoint](2026-09-08-retail-bot-runtime.md) admits four QL bots. The [follow-up](2026-09-08-bot-trace-and-probe-corrections.md) corrects command-queue interference, localizes timestamp-only logging to the retail DLL and fixes botlib no-entity traces; the subsequent [world-link restart fix](2026-09-08-world-link-restart.md) passes a shrinking native entity-binding regression and a free-running retail QL restart with four bots, IN_PROGRESS state and a scored frag. A subsequent [retail transition series](2026-09-08-retail-transition-series.md) passes three explicit restarts and two map changes per profile with four bots at all six checkpoints; QL reports IN_PROGRESS throughout those samples. The [bot-profile default checkpoint](2026-09-08-bot-profile-defaults.md) corrects QL bot_grapple to the retail engine default 1 while retaining Q3 default 0 and explicit overrides. The [retail QL restart checkpoint](2026-09-08-retail-ql-map-restart.md) verifies one connected warmup restart per retail direction, preserving authentication and rendering after restart. The [full retail map-change checkpoint](2026-09-08-retail-map-changes.md) verifies one destination-map reload and active admission per retail direction under both profiles. Longer transition stress remains open. Connect retail clients, exercise pure admission/live messages, and broaden long/faulted sessions. A connectionless pure=1 response does not prove pure negotiation. Bots ring 64 reliable commands in every slot, as `quake3.exe` 1.32 and `quakelive_steam.exe` do; a fnTech3 Quake III bot sent a burst of addressed commands held 64 and was dropped on the 65th, before and after the window became the profile's ([bot window record](2026-09-24-bot-reliable-windows.md)). |
| Cross-profile isolation | **PARTIAL — Unit/Fixture/Live.** Active profile is immutable; config read and write select `q3config.cfg`. Both profiles start sequentially from the same staged distribution with isolated homes. With one writable home shared by `q3`, `ql`, `rtcw-sp` and `rtcw-mp`, Q3 restarts with only its own archived values and protocol 68, runs its QVMs and persists only to `baseq3/q3config.cfg`; see the [shared-home record](2026-09-18-profile-isolation.md). One pair of developer binaries also runs `q3`, `ql`, `rtcw-sp` and `rtcw-mp` in turn with separate homes: each client reaches its stock map and writes an engine screenshot, and each multiplayer server answers `getstatus` ([release-profile record](2026-09-26-release-profiles.md)). | **PARTIAL — Unit/Fixture/Live.** Active profile is immutable and QL's `qzconfig.cfg`/`repconfig.cfg` save path is preserved. In the shared home QL restarts with its own values and protocol 91 and loads native modules from its own cache; it imports Q3's `q3config.cfg` once, only when no `qzconfig.cfg` exists, as the documented one-way migration. The same pair of binaries runs QL's client and server between the other three profiles ([release-profile record](2026-09-26-release-profiles.md)). | Broaden download, Steam-state, dedicated-server and concurrent-process isolation checks. In a shared home Q3 and QL keep one `baseq3/servercache.dat`, the preserved legacy layout. |
| Release/build matrix | **PARTIAL — Build/Fixture.** [Clean Windows x86 MSVC build at 4383039](2026-09-08-clean-windows-4383039.md) completes all 1,152 steps and passes the full suite (110 checks, one skip), including native-module runtime opt-ins. Staged client/dedicated/renderer probes pass their bounded startup checks. A separate diagnostic Q3-only x64 configuration builds and exercises native RT. | **PARTIAL — Build/Fixture.** The same clean x86 artifacts also pass QL HTTP download/reconnect, pure admission and native join UI/world checks. Earlier startup/demo/RTX raster evidence remains scoped to its recorded artifacts. The Q3-only x64 executable rejects QL and cannot establish this release contract. | The [Linux filesystem/package refresh](2026-09-08-linux-filesystem-package.md) clean-builds d5c7fbd, passes 109 targets with two skips, and verifies extracted Q3 client/QL dedicated plus corrupt/oversized pak rejection. Its six ELF files require GLIBC_2.38 and fail the configured GLIBC_2.31 release baseline; local runtime and archive-layout success do not close that portability gate. The [Ubuntu 20.04 follow-up](2026-09-08-glibc-baseline-build.md) produces new i686 binaries requiring at most GLIBC_2.29, passes the baseline checker and complete suite, and verifies extracted Q3 client/QL dedicated runtime inside the older userspace. It also corrects statistics decimal rounding and test-harness compiler compatibility. The [post-audio Linux baseline refresh](2026-09-08-linux-audio-build-refresh.md) clean-builds 2c33a2b with no source changes, passes 108 targets with one skip, verifies all six binaries against GLIBC_2.31 and repeats Q3 pure-join rendering plus QL dedicated startup from an unchanged deterministic 268-file package. The [post-codec Linux refresh](2026-09-08-linux-post-codec.md) clean-builds commit 218ab89 in 928 steps, passes 109 targets with two skips and no failures, and repeats Q3 pure-join rendering plus QL dedicated startup using the new binaries. The [current Linux i686 checkpoint](2026-09-08-current-linux-i686.md) builds client/server and all renderer modules, passes 109 checks (2 skips) after making a release-notes test archive-portable, and verifies Q3 GLx client plus QL dedicated runtime. Earlier Make/CMake and bundled-SDL evidence remains scoped to its recorded commits. Windows MSVC/MSYS2 x86 release validation also passes; see the [reference-peer checkpoint](2026-09-07-q3-reference-interop.md). The [current Windows package checkpoint](2026-09-08-current-windows-package.md) stages the latest filesystem/allocation corrections, validates all evidence links and six PE dependencies, produces identical repeated archives, and passes both profiles from the same unchanged 224-file extraction. The [local Windows package checkpoint](2026-09-08-windows-package-runtime.md) validates extracted same-binary Q3/QL runtime and deterministic ZIP creation from fixed inputs; the [packaged evidence correction](2026-09-08-packaged-evidence-documents.md) now includes fnTech3 Markdown/JSON evidence and validates its internal file links. Make and CMake builds for Linux and the BSDs now define `_FILE_OFFSET_BITS=64`, as Meson does: without it an i686 Make-built dedicated server of 8cf605c7 found no paks in the Steam Quake III installation through WSL's drvfs mount, whose inode numbers exceed 32 bits, and with it the Make and CMake servers load `q3dm17` from all 15 of its paks ([large-file record](2026-10-05-make-large-files.md)). A CMake build of every target for Linux i686 with CI's flags now completes -- client, dedicated server and all three renderer modules -- and passes 81 of its 82 tests (the retail-factory probe skips), where 8cf605c7 stopped at the Vulkan and RTX modules, a 64-bit libjpeg-turbo in the client link and a test's include path, and one test aborted on a literal GCC keeps at x87 precision ([CMake i686 record](2026-10-05-cmake-i686-full-build.md)). Reproducible compiler artifacts, broader packaged runtime gates and visual/service failures remain open; see the [portability checkpoint](2026-09-07-build-portability.md). |

## Current blockers

### RTCW SP and MP: shared-executable implementation in progress

RTCW SP and MP target the same `fntech3.exe`, with two immutable mode profiles
under one title. The [game-manager/contract foundation](2026-09-14-rtcw-contracts.md)
adds catalog and Steam discovery, mode-specific configuration/archive policies,
verified external PODs, canonical state adapters, larger profile-owned storage
limits, native-module search/call tables, transactional cgame state transfers
and source-compared protocol 49/60 message bodies. A validated controller now
handles RTCW's menu mode commands by relaunching the same executable; synthetic
Windows/Linux process tests exercise both directions without game input.
**Both RTCW profiles are available in every build; full gameplay and compatibility
sign-off remain open.** Startup prints an informational experimental warning.
Availability does not close any compatibility gate.
Native SP/MP UI probes now pass cache imports, explicit VM pointer conversion
and basic sound registration. MP translation and its two-argument background
music import are integrated; the legacy updater reports its service unavailable.
The [SP music/menu checkpoint](2026-09-15-rtcw-music-menus.md) adds a shared
OpenAL/legacy music controller and exact SP music/fade imports. Both modes now
boot their legitimate native main menus through the same `fntech3.exe`, with
inspected textured English SP/French MP captures in GLX, Vulkan and RTX raster.
Both initialize muted audio; OpenAL queues decoded music. Dynamic-light
submission, projected passes and model-lighting policy have source/fixture
evidence. World-light/fog rendering, measured audio parity and gameplay remain
open.

The [server-world checkpoint](2026-09-15-rtcw-server-world.md) adds checked
world/entity imports and profile-owned server defaults, including SP campaign
AI slot allocation. MP's installed map passes initial entity spawning. Both
installed map probes advanced past import 202 through the
[bot-library bootstrap checkpoint](2026-09-15-rtcw-botlib-bootstrap.md). At that
checkpoint SP stopped at import 206 (`BOTLIB_LOAD_MAP`). The
[navigation-world checkpoint](2026-09-15-rtcw-navigation-worlds.md) adds two-world
ownership and area blocking, corrects MP's bot default, and advances MP through
server initialization, sound/entity parsing and weapon/model loading. The
[client-animation checkpoint](2026-09-15-rtcw-client-animation.md) fixes partial
foot-tag queries and the animation-speed handoff. The
[MP pure-admission checkpoint](2026-09-15-rtcw-mp-pure.md) gives the game manager
an explicit pure-proof policy and verifies both native module paks. MP now
reaches `CS_ACTIVE` and receives its first local map snapshot. The
[fog-controller checkpoint](2026-09-15-rtcw-fog-controller.md) adds shared
SP/MP state transitions and renderer API 23; active GPU fog remains guarded.
The [spatial-audio checkpoint](2026-09-15-rtcw-spatial-audio.md) adds checked
entity/listener imports. The [MP loop checkpoint](2026-09-15-rtcw-mp-loop-audio.md)
adds range/volume loops to both audio backends and the interpolated viewpoint.
The [limbo/cinematic checkpoint](2026-09-15-rtcw-limbo-cinematics.md) adds chat
history and cinematic imports, fixes movie-file shutdown ownership, and passes
MP local boot through the deployment-menu briefing movie with normal exit in
all three renderers. Its initial visual defects are corrected by the
[material-directive checkpoint](2026-09-15-rtcw-material-compression.md), whose
three inspected captures show textured deployment menus and briefing movies.
The menu obscures the world, so this does not establish playable-map support.
The [live AAS8 checkpoint](2026-09-15-rtcw-aas-loading.md) connects the validated
reader to owned navigation arrays and two-world map loading. The installed SP
module loads both `escape1` hulls. All 70 installed AAS files pass record conversion
checks for both profiles/worlds. The [BSP-query checkpoint](2026-09-16-rtcw-bsp-queries.md)
adds checked entity/property queries with source-compared string padding and
numeric parsing. At that checkpoint SP stopped at import 205 (`BOTLIB_START_FRAME`).
The [routing-cache checkpoint](2026-09-16-rtcw-routing-caches.md) adds bounded
RCD12/15 decoding, topology validation and per-world native cache/visibility
ownership. All 70 installed SP caches pass reference-reader and engine-loader
comparisons; MP version selection uses synthetic headers. The
[routing-initialization checkpoint](2026-09-16-rtcw-routing-initialization.md)
connects import 205 to both worlds, profile-specific slope costs and bounded
routing tables. All 70 installed navigation pairs pass binary32 source
comparisons; extended x87 reference arithmetic has a documented difference.
The [startup-contract checkpoint](2026-09-16-rtcw-startup-contracts.md) adds
selected-world queries, empty item updates, profile-owned appearance defaults
and SP's native animation-model handoff. Original SP's disabled zombie-hit
import is honored; iortcw's enabled effect remains a distinct open contract.
At that checkpoint SP stopped at cgame import 34 (`CG_S_CLEARLOOPINGSOUNDS`).
The [SP loop-audio checkpoint](2026-09-16-rtcw-sp-loop-audio.md) adds both-backend
reset modes, music preservation and source-compared loop/mixer fade stages.
The game manager now owns pure-mode defaults and flags: original SP defaults
off, MP on, with explicit settings preserved. SP pure verification remains
explicitly unavailable. With pure mode off, installed SP initializes both
`escape1` worlds, loads animation models and receives its first local snapshot;
observed boundaries are cgame import 31 (`CG_S_STARTSOUND`) and game import
207 (`BOTLIB_UPDATENTITY`) in separate runs.
The [one-shot-audio checkpoint](2026-09-16-rtcw-one-shot-audio.md) adds checked
SP/MP plain, extended-flag and local starts; both backends share source-compared
cutoff/reuse/FIFO rules and linear spatial gains with bounded exhaustion.
Legacy dynamic positions survive sound clears and update independently of
loop submissions. Both final SP audio runs receive an `escape1` snapshot and
stop at game import 207 (`BOTLIB_UPDATENTITY`); MP briefing captures remain valid
with both backends. The [AI entity checkpoint](2026-09-16-rtcw-ai-entities.md)
adds checked entity updates, world-zero shared records with explicit area-link
ownership, and profile-owned AI client-slot allocation/free. Original and
iortcw SP/MP each pass 153,334 update/record/callback comparisons on Windows
x86/x64 and Linux x86 sanitizers. Installed SP now passes entity updates and
client allocation, reaching game import 546 (`BOTLIB_AI_ALLOC_GOAL_STATE`).
The subsequent [AI state checkpoint](2026-09-16-rtcw-ai-state.md) implements
checked goal/movement allocation, initialization, reset and free, with SP's
128-state capacity and MP/Q3/QL's 64-state capacity. Defined original/iortcw
behavior passes 524,288 initialization and 12,288 reset comparisons per mode,
reference and target; iortcw's bounded last-avoidance reset is separately
verified. SP now passes import 546 and exits cleanly after a mission-briefing
capture. A longer probe still reports `cg_norender=1`; initial cast/save-load
readiness is unresolved. MP briefing and all six Q3/QL renderer smoke checks
pass with inspected engine captures. No SP playable-map capture is claimed.
The subsequent [cast/action checkpoint](2026-09-16-rtcw-cast-actions.md)
identifies repeated AI reliable-command overflows in those earlier briefing
logs; their boot-pass labels did not establish cast readiness. The harness
now rejects that diagnostic. Checked cast classification restores RTCW's
broadcast/configstring exclusions, and all 28 elementary-action imports plus
AI command delivery are implemented. Original/iortcw SP/MP each pass 344,099
action comparisons per target. SP now advances to unsupported import 308
(`BOTLIB_AAS_POINT_AREA_NUM`) without those overflows; MP's inspected deployment
capture still passes. Campaign readiness and play remain open.
The subsequent [AAS/tag checkpoint](2026-09-16-rtcw-aas-tags.md) implements
five checked basic navigation queries and the server-to-cgame tag callback.
Each original/iortcw SP/MP reference passes 527,849 query comparisons per target.
The installed SP game now spawns all 29 soldiers, sends `rockandroll` and
displays its normal ready arrow. It then pauses in the pregame menu; no input
or playable-world capture is claimed. MP and all six Q3/QL renderer checks
still pass with inspected engine captures.
The [cast lifecycle checkpoint](2026-09-16-rtcw-cast-lifecycle.md) adds SP-only
snapshot and packet-timeout exemptions plus silent cast disconnect notices.
Executable tests cover all four layouts, callback flag changes, cleanup,
authentication, admission deadlines and zombie reclamation. Sustained cast
behavior in a playable campaign remains unverified.
The [area-trace checkpoint](2026-09-16-rtcw-aas-traces.md) adds checked import
309 with owned endpoints, bounded staged output arrays and nonlocal-error
cleanup. Original/iortcw SP/MP traces each pass 786,560 comparisons per target;
shared Q3/QL positive-capacity behavior and safe empty traversal are covered.
The [route-query checkpoint](2026-09-16-rtcw-route-queries.md) adds checked
import 317, RTCW goal costs and per-link update limits, portal first-step
selection, and RTCW cache expiry. Original/iortcw SP/MP each agree on 66,176
synthetic route queries and 17,920 queries over the 70 installed navigation
pairs on Windows x86/x64 and Linux x86 with sanitizers. The MP corpus uses
synthetic RCD12 headers over SP data.
These are source/fixture comparisons, not live AI or retail peer evidence.
The [visibility checkpoint](2026-09-16-rtcw-visibility.md) connects cached
RCD visibility and SP import 323, with distinct SP and MP fuzzy-area searches.
Original/iortcw references agree on 10,816 synthetic visibility queries,
87,552 fuzzy queries and, for SP, 32,768 route-position queries. Each also
passes visibility and fuzzy checks over all 70 installed navigation pairs;
SP adds 17,493 route-position comparisons with routing endpoints. The 427
non-routing endpoint cases are explicitly excluded from route parity because
the engine's existing safety guards reject them earlier. Windows and Linux
are each compared with their own reference binaries; cross-platform numeric
identity and sustained live tactical behavior remain open.
The [cast-query callback checkpoint](2026-09-16-rtcw-cast-query-callbacks.md)
adds the server-to-game callbacks for hide and attack queries. Both modes use
their exact five-argument exports 11 and 12, with owned position vectors and
explicit native-module availability. Synthetic native dispatch, nested calls
and profile isolation are verified.
The [tactical navigation checkpoint](2026-09-16-rtcw-tactical-navigation.md)
implements imports 321/322 with original SP/MP frame budgets, flags, occupancy
and entity-area policies. Per reference, 4,096 hide and 4,096 warm-cache attack
queries agree with full callback arguments. Original-game normalization and
iortcw MP's disabled occupancy are explicit comparison conditions. Cold-cache
diagnostics differ in 215 of 4,096 attacks and retain subsequent route
differences; retail investigation and live callback use remain open. Search
storage is independent of route-cache work lists and survives nonlocal errors
through map-owned cleanup. Complete tests, both builds, SP/MP briefing probes
and six Q3/QL renderer checks pass; this does not establish live campaign AI.
The [movement-prediction checkpoint](2026-09-16-rtcw-movement-prediction.md)
adds imports 319/320, the exact 80-byte prediction record and original SP/MP
physics/event policies. Both game ABIs use 10-bit entity numbers; cast/tactical
guards now use that limit instead of SP's larger internal botlib allocation.
Each original reference agrees on 7,267 defined synthetic predictions under
strict SSE arithmetic. Original start-solid undefined behavior is excluded;
default Linux x87 strict arithmetic still fails exact numeric agreement, and
fast-math diagnostics show drift without changed decisions in this corpus.
iortcw deliberately differs in friction and normalization.
The [installed-prediction checkpoint](2026-09-16-rtcw-installed-prediction.md)
compares 64 distinct installed AAS members with their matching BSP contents.
Each original mode agrees on 36,771 defined predictions with strict Clang
Windows x86/x64, MSVC x86 and Linux x86 SSE2 sanitizer builds. Original undefined
trace paths remain excluded. The broader fast-math diagnostic has one changed
decision per original reference, so unchanged decisions are not a general
guarantee. Dynamic entities, original BSP/retail numeric parity and live AI
remain open; MP policy uses SP geometry in this fixture.
The [entity-collision checkpoint](2026-09-16-rtcw-entity-collision.md) corrects
RTCW target-capsule selection for box and point queries through an immutable
profile policy. Each original/iortcw selector agrees in 1,024 cases across
Windows x86/x64 and sanitized Linux x86. Actual server/AAS fixtures verify
external entity layouts, nearest hits, pass filtering and both navigation
worlds, with synthetic collision backends. Physical collision and live
moving-entity prediction remain open. Both builds, the full suite and eight
inspected startup/renderer captures pass; Q3/QL shape selection is unchanged.
The [entity box contents record](2026-09-23-entity-box-contents.md) gives RTCW
MP the temporary-box contents retail `WolfMP.exe` uses: a server sweep of a box
entity tests and reports the entity's own contents, and `CONTENTS_BODY` comes
back on every way out of the sweep. RTCW SP keeps `CONTENTS_BODY`, as
`WolfSP.exe` does. Real-trace fixtures cover all five frozen entity layouts and
a recorded-call fixture every catalog profile; no retail session was compared.
Two related differences, `WolfSP.exe`'s sweep skipping a carried prop and
both RTCW engines' point contents placing an entity at its `s.origin`, are
followed since the [entity-query record](2026-10-08-rtcw-entity-queries.md),
which shows each against the retail servers (Live).
The [RTCW trace record](2026-10-08-rtcw-traces.md) compares physical
collision with the retail `WolfSP.exe` and `WolfMP.exe` servers through a
synthetic native game module: 202,860 traces, 21,000 point contents, 434
links and 2,800 box queries on fourteen maps (Live). It applies seven RTCW
collision rules read from the executables -- contents kept inside a solid,
MP's curve facets and position test, SP's capsules traced as boxes -- and
fixes an inherited bounds test that let a point pass through a capsule
entity. Nine traces still differ, in floating-point precision only, the
decision the [Quake III trace record](2026-10-08-q3-traces.md) leaves open.
Client-side traces and the other maps are not compared.
The [area-portal record](2026-10-08-rtcw-area-portals.md) lets an RTCW
level close a door's portal that is already shut, as both engines do;
fnTech3 had dropped `escape1` where `WolfSP.exe` went on (Live).
The [movement-area checkpoint](2026-09-16-rtcw-movement-areas.md) admits checked
import 553, preserves SP-only disabled-area behavior and RTCW model-travel
selection, and rejects invalid entity/model accesses or incomplete worlds.
Each original/iortcw reference agrees on 32,256 movement queries plus direct
area/model queries across Windows x86/x64 and sanitized Linux x86. Synthetic
BSP ground responses isolate lookup policy; active campaign movement remains
unverified.
The [directional-movement checkpoint](2026-09-16-rtcw-directional-movement.md)
admits checked import 550 and preserves RTCW grounded-flag, slime, normalization
and setup-setting behavior. Each original/iortcw reference agrees in 51,200
queries under strict Windows x86/x64 and sanitized Linux x86 SSE2 arithmetic,
using shared engine AAS/prediction/action services. Fast math changes 25,744
combined records but no success decisions in this corpus; this does not
supersede the installed prediction diagnostic's decision difference. Failed
prediction is rejected before subsequent actions. Builds, the full suite and
eight inspected startup/renderer captures pass; active AI remains unverified.
The [movement-view checkpoint](2026-09-16-rtcw-movement-views.md) adds checked
imports 554/573, a frozen goal layout and distinct original SP/MP next-link
selection. It preserves partial targets on failure, original distance/travel
rules and visible-position semantics, with bounded traversal and checked
storage. Original SP/MP agree in 71,712 synthetic selector/view cases each
under strict Windows x86/x64 and sanitized Linux x86 SSE2 arithmetic; iortcw signature/normalization and
fast-math differences remain explicit diagnostics. Module boundaries, focused
tests, both builds, the full suite (111 Meson tests and 1,436 Python tests, with
two and 11 skips respectively), and eight inspected engine captures pass.
Live actor movement remains unverified.
The [travel-execution checkpoint](2026-09-16-rtcw-travel-execution.md) adds
original SP/MP policies for 22 travel routines. Each original agrees in
126,720 synthetic cases under strict Windows x86/x64 and sanitized Linux x86
SSE2 arithmetic, including actions, state changes and defined result fields.
Undefined original output words are excluded explicitly; shared engine
services and controlled BSP responses limit the evidence. Compiler/iortcw
numeric differences remain diagnostics, with no compared decision changes.
Both builds, 14 focused tests on three targets, the full suite (111 Meson and
1,440 Python tests, with two and 11 skips), and eight inspected captures pass.
Active actor travel remains unverified.
The [movement-to-goal checkpoint](2026-09-16-rtcw-movement-to-goal.md) adds
checked import 549 with a frozen 52-byte result, explicit goal/result bit
translation, owned buffers and validation. Shared orchestration retains
original route lifetime, jump-pad recovery and distinct SP/MP future views;
ladder fallback and traversal bounds handle partial or malformed state.
Original SP/MP agree in 18,432 synthetic cases each under strict Windows
x86/x64 and sanitized Linux x86 SSE2 arithmetic. iortcw has explicit decision
differences; original fast-math differences are numeric in this corpus.
Both builds, 18 focused tests on three targets, the complete suite (111 Meson
and 1,446 Python tests, with two and 11 skips), and eight inspected captures
pass. Active actor movement remains open.
The [jump run-up checkpoint](2026-09-16-rtcw-jump-runstart.md) adds RTCW
normalization/slime policy and stops jump actions on failed prediction.
All four original/iortcw references agree in 9,476 defined helper cases each
under strict Windows x86/x64 and sanitized Linux x86 SSE2 arithmetic, with
960 failed predictions and four nonfinite reference velocities excluded
explicitly. Fast-math differences and its six excluded velocities are
recorded separately. Both builds, 13 focused tests on three targets and eight
inspected captures pass. The complete suite passes 111 Meson tests (two
skips), including 1,449 Python tests (eleven skips).
The [entity-stream checkpoint](2026-09-16-rtcw-entity-streams.md) adds checked
start/stop imports, a shared pool/decoder, both-backend queued replacement and
SP entity stops/fades. Original-source allocation, stop and fade comparisons
agree on Windows x86/x64 and sanitized Linux x86; 26 focused audio tests pass
on all three. Both builds, four muted SP/MP stream probes and six Q3/QL renderer
probes pass, with all ten engine captures inspected. The complete suite passes
111 Meson tests (two skips), including 1,455 Python tests (eleven skips).
SP entity loops intentionally stay on their entity lane, correcting the
released source's music-channel bug. PCM and live-dialogue parity remain open.

At the entity-stream checkpoint, voice amplitudes and live dialogue,
missing-cache generation and AI, letterboxed movies, active GPU fog and
broader gameplay/import coverage remained open. Later startup, material and
speech evidence is recorded below; campaign play and save/load remain unverified.

The [first approved SP ready-click probe](2026-09-16-rtcw-autosave-write.md)
reached the game-owned player-start action, then failed its initial autosave:
the engine discarded the write byte count and returned zero. The adapter now
preserves RTCW game/cgame write results and admits bounded, non-truncating
save-file replacement. Focused tests and both builds pass. The first approved
post-fix retest wrote matching 218,188-byte autosave and current-save files,
then hit the explicit missing scene-fog GPU guard. Its engine screenshot shows
that error dialog; playable SP and save restoration remain unverified. The
full suite passes (111 Meson tests/two skips, 1,457 Python tests/eleven skips).
At that checkpoint, ordinary RTCW menu mouse-coordinate adaptation
also remained open; that test delivered its one approved click directly to
the native UI using RTCW's relative-delta contract. The subsequent
[profile-specific menu mouse correction](2026-09-17-menu-mouse.md) restores
relative UI/cgame motion for Q3 and RTCW across SDL, Win32 and X11, while
preserving QL absolute input. Source, executable unit fixtures and Windows
builds cover that correction; live menu hover/click validation remains open.

Subsequent [approved startup retests](2026-09-16-rtcw-coronas.md) passed the new
GL/GLSL scene-fog path, silent-backend voice response, RTCW XYZST decal boundary
and corona imports in sequence. Retest 5 then stopped at `CG_LOADCAMERA` (98).
All five approved startup clicks are consumed, and their engine screenshots
remain error dialogs. At that checkpoint, SP had no capture after the intro.
Active voice amplitude and save restoration remain open; Vulkan/RTX retain
the active scene-fog guard. The decal POD probes now pass 2,215 assertions per
SP reference and 2,205 per MP reference on Windows x86/x64. Current default and
development client/server/renderer builds and seven focused Linux x86
sanitizer checks pass; all six post-change Q3/QL renderer captures were inspected
and audited. The full suite passes 111 Meson tests with two skips, including
1,465 Python tests with eleven skips (529.459 seconds).

The [camera implementation](2026-09-16-rtcw-cameras.md) adds an owned 64-slot
manager, bounded file parsing, fixed/interpolated/spline evaluation, target and
FOV events, and checked camera imports. Original SP and MP timing policies are
selected by profile; iortcw MP's use of SP timing is recorded separately.
Final source comparisons cover 320,880 sample pairs per Windows x86/x64
architecture, with exact original-SP and finite original-MP results; undefined
MP outputs and iortcw numerical differences are recorded separately. A new,
separately authorized 90-second windowed `escape1` test progressed through ten
camera commands and reached the first-person knife/health-HUD view with a valid
snapshot, camera mode cleared and no engine error. All four engine screenshots
were inspected. Character/attachment defects and missing skin-shader warnings
remain; the test used disabled audio and no movement/combat input. The sole
camera-test click is consumed. Full SP/MP gameplay compatibility remains open.
The final camera suite passes 111 Meson tests with two skips, including 1,470
Python tests with eleven skips (511.371 seconds). Nine focused Windows x86/x64
and Linux x86 sanitizer tests pass, and all six final Q3/QL renderer captures
were inspected and audited against the final ordinary executable.

The [RTCW UI browser query boundary](2026-09-17-rtcw-ui-browser.md) now dispatches
the supported LAN query imports and translates SP/MP list IDs explicitly. The
installed MP Join Server menu opens without its prior unsupported-import-75
error. One focused RTCW boundary test and 32 shared UI tests pass, and the
Windows x86 client rebuild passes. The 1600x1200 no-CSM screenshot set includes
SP after the intro and MP first-person local warmup; all selected runs report
`r_picmip 0`. This adds menu and rendering observations, without closing remote
browser/join, favorites editing, sustained gameplay or interoperability gates.

The [character-material correction](2026-09-17-rtcw-character-materials.md)
adds SP character mip selection, RTCW conditional texture alternatives, MP's
`nomipmap` alias and normal-based vertex alpha across all three renderers.
The alpha evaluator agrees on 819,000 cases per original/iortcw SP/MP reference
on Windows x86/x64 and sanitized Linux x86. Installed SP/MP briefing probes
report zero unknown shader directives. The SP opening cinematic now shows
assembled character heads/bodies and reaches first-person startup; `r_picmip`
and `r_picmip2` remain zero. Six final Q3/QL raster-renderer regression captures
were inspected. The complete Windows x86 development suite passes 111 Meson
tests with two skips, including 1,471 Python tests with eleven skips.
Broader character/material GPU parity, campaign gameplay and
interop remain open.

The [activation/pickup/eyes correction](2026-09-17-rtcw-player-interactions.md)
adds RTCW activation bindings and lean/any-key encodings, publishes the pickup
preference to the server, and applies SP/MP blink rules to MD3/MDC attachments
as well as MDS models. Failing-before regressions cover all three renderers;
windowed installed probes show eyes in GL/Vulkan/RTX raster and verify default
and explicitly disabled automatic-pickup userinfo. Live door/weapon interaction,
native ray-tracing GPU output and broader gameplay
remain open.

The [RTCW eye-rendering follow-up](2026-09-17-rtcw-eye-rendering.md) corrects
Vulkan descriptor state lost across bloom, which could remove a repeated head
draw while leaving its eyelids visible. It also aligns RTCW MD3 packed normals
with the already-correct MDC decoder in all raster backends and native RTX
geometry, preserving Q3/QL calculations. Failing-before regressions, Windows
and Linux tests, installed SP/MP head captures and an SP cinematic eye
transition add evidence; all-character original-engine and native RT GPU
parity remain open.

The [engine command audit](2026-09-17-rtcw-engine-commands.md) accounts for 183
original SP/MP command names and adds the remaining named action bindings,
notebook/help, UI/translation reload, limbo chat, recommended settings and SP
music queue dispatch. Executable regressions cover profile ownership, button
values, input recovery and QL-only chat export isolation. Same-client installed
SP/MP startup probes verify registrations and safe utility dispatch. Optional
authoring tools and legacy integrations are explicitly classified; MP master
match reporting returns an honest unavailable diagnostic. Actual gameplay and
broader compatibility gates remain open.

The [transparent-fog correction](2026-09-17-rtcw-transparent-fog.md) makes RTCW
additive and filter passes fade toward their blend-neutral color. Compiled
compositing regressions fail before and pass after in all three backends; nine
fog tests pass on Windows/Linux x86. Inspected installed-SP engine captures
show the torch rectangles removed in GL, Vulkan, and RTX raster. RTX's overall
fog brightness, original-engine visual parity, and native RT runtime evidence
remain separate open items.

The 2026-09-24 path tracer record (`2026-09-24-rtx-path-tracer.md`) adds the
first native RT runtime observations for both modes. In the shared Windows
x86 developer client, whose driver here lacks the ray-tracing pipeline, the
RTX path tracer's compute backend traces 25 of the 26 retail SP maps probed and
all 15 MP maps with their light entities under RTCW's compiler rules (linear
and angle-free by default, `fade`, grid-only `lightJunior`, and no light from
a light whose target names no entity, as the retail lightmaps show), the
maps' global fog, and the main view behind a portal sky, at 0.81-1.33 (SP) and
0.97-1.50 (MP) of their raster frames' brightness, with no errors and the
installation unchanged; `norway` stops in its game script, in raster too. The
earlier record's `escape1` figure compared the raster main view with itself:
its portal sky took the trace. The torch `dlight` entities add 0.95 of the
light raster's dynamic light pass adds on `crypt1`, the `negdlightshader` and
`flamedlightshader` materials multiply as the original renderer drew them,
and a retail MP demo replays traced at 1.54 of raster's brightness (vertex-lit
terrain at night). No 32-bit Vulkan validation layer is installed here.
Original-engine visual parity and campaign-length native RT runs remain open.
`norway` stopped because the probe loads a map with `devmap`, which RTCW SP
runs as free-for-all: the retail game then runs each spawn script as its
entity spawns, and `floating_ice2`'s names a `path_corner` 614 entities
later. Loaded from a campaign save, `norway` traces without error. Since the
record's fifth round the default lighting is traced in linear light and
tone mapped as Q2RTX maps its views; the brightness ratios above were
measured under the compilers' light models, now `rtx_rt_lighting 2`. Since
its sixth round the default traces the light entities RTCW's maps keep
(`escape1` 665, `crypt1` 468) with their emitters, sky and sun, so a room the
designer lit with light entities and no visible fixture is lit as its
lightmap was; the emitters alone, as Q2RTX lights Quake II, are
`rtx_rt_lighting 3`.

The [speech checkpoint](2026-09-17-rtcw-speech.md) removes the active-SP
voice-amplitude guard with real legacy/OpenAL sample measurements and preserves
MP's disabled talking-animation result. The measurement probe agrees with
299,264 cases per original/iortcw SP reference on Windows x86/x64 and sanitized
Linux x86. RTCW also owns its silent default sound, fixing a legacy intro
failure caused by the missing Q3 fallback asset. Both audio backends complete
the installed SP intro with nonzero speech results and inspected first-person
captures; output was focus-muted, so audible synchronization remains open.
Six ordinary-binary Q3/QL/MP audio startup checks and both Windows x86 builds
pass. The full development suite passes 111 Meson tests with two skips and
zero failures, including 1,475 Python tests with eleven skips. The 28 focused
audio checks also pass on Windows x64 and sanitized Linux x86. Original
device/PCM timing, facial synchronization, sustained playback and full gameplay
remain open.

The [startup/movie checkpoint](2026-09-17-rtcw-cinematic-presentation.md) moves
default movie selection into the immutable profile catalog, admits SP's
letterbox flag/console mode and header frame rate, and repairs the shared
first-frame clock transition. Windowed engine captures show the SP film through
48 seconds on GL, Vulkan and RTX raster. Installed MP's absent startup movies
fall through to its menu without changing archive isolation. Movie audio,
complete codec validation and original-engine parity remain open. The later
[RoQ checkpoint](2026-09-17-roq-bounds.md) establishes that original SP/MP also
ignore JPEG chunks; it preserves that policy and adds bounded decoder,
installed-corpus and same-process recovery evidence.

The [campaign/save checkpoint](2026-09-17-rtcw-campaign-savegames.md) launches
all 47 official maps separately, initializes both AAS worlds on all 32 SP maps,
and exercises same-process plus fresh-process saves on all 25 playable SP maps.
It repairs save staging/timing, bot scheduling after restart, saved entity
capacity, fog/episode metadata, cinematic-save protection, all twelve trajectory
ordinals, and authored sun/light-grid directives. Inspected MP world captures
cover all 15 maps with the advertised objective/checkpoint mode. Two specifically
authorized movement/attack probes were run: SP passed; MP passed a separately
authorized retest after repairing match-state restarts and the probe's command
scheduling. All 15 MP maps also pass native warmup restarts with advancing
snapshot/usercmd clocks. Seventy installed AAS/RCD pairs pass
loader/conversion/cost comparisons. This does not close complete campaign,
custom-cache generation, sustained navigation/gameplay or peer-interoperability
gates; RTCW remains an explicitly enabled development profile.

The [Vulkan/RTX fog and menu-resize correction](2026-09-17-rtcw-vulkan-fog-resize.md)
supersedes the historical Vulkan/RTX active-fog guard above. Both raster
backends now apply per-material scene/sky fog; installed `escape1` probes reach
the opening 3D scene, retain the paused briefing across resolution changes,
and capture controlled fog-on/off results. An actual SDL window-resize event
also refreshes and restores the briefing. Native ray fog is implemented and
compiled, but the x86 driver rejects required ray-tracing extensions, so native
RT runtime and original-engine visual parity remain open. Q3's resized main
menu was inspected; isolated QL evidence covers UI reload/state/dimensions and
its splash only, with WebUI disabled.

The [empty-model registration correction](2026-09-17-rtcw-empty-model-names.md)
extends the RTCW bridge's silent handle-0 result to empty names as well as null
pointers, matching original SP/MP behavior. The import regression fails before
the correction and passes afterward; focused Windows x86 checks and seven
installed runtime probes pass. SP `escape1` cinematic probes show zero
`RE_RegisterModel: NULL name` warnings in GL/Vulkan/RTX raster. This is a
bounded model-registration correction; broader compatibility gates remain open.

The [shader animation limit correction](2026-09-17-rtcw-animmap.md) restores
RTCW SP/MP's sixteen-frame ordinary `animMap` limit in GL/Vulkan/RTX. Actual
parser regressions cover all profiles, bounds and the existing shaderx
extension; fourteen focused Windows x86 checks and the strict development
build pass. Six installed SP/MP map-startup probes report zero excess-animation
warnings. These establish registration behavior, not fire-effect visual parity.

The [demo record](2026-09-18-rtcw-demos.md) makes demo protocols profile-owned.
RTCW recordings had been renamed `.dm_68`, and RTCW profiles searched Q3 demos
and `.dm3` while refusing their own `.dm_49`/`.dm_60`. RTCW MP now records,
replays and timedemos `.dm_60`, and exchanges demos with retail `WolfMP.exe` in
both directions, including a retail recording made under pure admission. SP
records only with `g_synchronousClients` set, as original SP requires, and
refuses replay: retail `WolfSP.exe` fails fatally on its own SP demo because the
SP cgame needs a running game module. Q3 and QL record/replay pass unchanged.
Short spectator recordings on one map per profile do not close active-play,
malformed-demo or seeking gates.

The [handshake record](2026-09-18-rtcw-mp-handshake.md) fixes outbound RTCW MP:
against retail `WolfMP.exe` the client had read the reply with Q3's token
layout and connected as protocol 68, which retail rejected. Client and server
now use RTCW's own original and iortcw layouts, including MP's
`sv_onlyVisibleClients` and the client culling it requests, name `wolfmp`/`wolfsp`
in `getchallenge`, and no longer ask Q3's authorize server about RTCW keys. An
fnTech3 client passes pure admission on the retail server and joins both teams;
retail clients still join fnTech3; Q3's handshake is unchanged. One map and
loopback UDP only; no iortcw binary was run.

The [navigation-cache recovery record](2026-09-20-rtcw-cache-recovery.md) adds
missing/damaged-cache generation and portable RCD15/RCD12 writing. Three SP maps
regenerate both worlds, then exchange the resulting caches with retail WolfSP
in both directions. Original/iortcw visibility-generation comparisons and
allocation/write-failure tests pass on Windows and sanitized Linux. A separate
retail comparison retains small waypoint/route-cost differences as an open
parity gate; MP cache generation still has fixture evidence only.

#### RTCW gate matrix

| RTCW gate | SP | MP | Required closure |
| --- | --- | --- | --- |
| Official maps and saves | Source/Unit/Live; 32 individual map launches with both AAS worlds, 25 gameplay save/reload/fresh-process cycles, repeat/cross-map/autosave and malformed-header handling; two-map retail save interchange; cinematic saves refused; the final cutscene now ends the campaign in the endgame credits menu instead of an unsupported-import drop; see the [endgame record](2026-09-18-rtcw-sp-endgame.md); Live for nine level changes the retail game makes itself, with no input but each briefing's Ready click: four persistent exits (escape1, village1, dig, castle) carry the player's weapons, ammunition and armour into the next level through `save/current.psw`, and five cinematic levels hand over to the next episode; a death reloads the level's save in place, at the save's game time ([level exit record](2026-10-08-rtcw-sp-level-exits.md)) | Source/Unit/Live; 15 individual map launches, advertised modes and native warmup restarts with advancing match/usercmd clocks; separately authorized soldier movement/firing retest; `map_restart` keeps `g_gametype` to a type the map's worldspawn allows before any delay, as `WolfMP.exe` does, and one console sequence on `mp_destruction` and `mp_beach` leaves the same types on retail and fnTech3 dedicated servers, where the previous build restarted into types the game then corrected ([game-type record](2026-09-23-rtcw-mp-restart-game-type.md)) | The other 22 campaign level changes, behind the use key, shooting or fights, and sustained AI/combat; broader historical/damaged save payloads; MP peers, objectives and other classes/weapons |
| Title/profile selection | Source/Unit/Build/Fixture; registered and gated; installed Windows x86 SP-to-MP menu handoff using the same executable, correct modules and inspected captures; synthetic Windows/Linux handoff; the client binary that runs Q3, QL and MP reaches the `escape1` briefing with SP's own modules ([release-profile record](2026-09-26-release-profiles.md)) | Source/Unit/Build/Fixture; installed Windows x86 MP-to-SP menu and active-briefing handoffs using the same executable, correct modules and inspected destination captures; synthetic Windows/Linux handoff; the client and server binaries that run Q3 and QL reach `mp_beach` with MP's own modules and answer `getstatus` ([release-profile record](2026-09-26-release-profiles.md)) | Broader live menu/session transitions; macOS handoff build/runtime |
| Filesystem/config/pure | Source/Unit/Live; archive/config policy, ordered module/root lookup and intentional no-cache policy integrated; original SP pure default/flags; explicit pure-proof policy unavailable; with one writable home shared with Q3, QL and MP, SP restarts with only its own `main/wolfconfig.cfg` values and protocol 49 and loads only its own modules ([shared-home record](2026-09-18-profile-isolation.md)) | Source/Unit/Build/Fixture/Live, inbound Interop subset; in the same shared home MP restarts with only its own `main/wolfconfig_mp.cfg` values and protocol 60 and loads only `_mp_` modules from its own native cache; a first MP start without its own server cache imports retail WolfMP's favorites once, checked against a byte-identical retail `WolfMP.exe` rewrite of the cache ([favorites record](2026-09-18-rtcw-mp-favorites.md)); profile-owned two-native-module proof, pinned-module pak validation and local pure admission with retail modules; original/iortcw proof and verifier comparisons; a retail MP client passes inbound pure admission on one map and reaches the world while the server withholds the loaded-pak names, so RTCW MP needs no `sv_pakNames` and keeps ET's publication scoped away from it; see the [pure-name record](2026-09-17-rtcw-mp-pure-names.md); an fnTech3 client also passes outbound pure admission to retail `WolfMP.exe` and joins both teams, after the [handshake record](2026-09-18-rtcw-mp-handshake.md) replaced a Q3 protocol-68 connect the retail server rejected; both directions then pass pure admission on all 15 stock MP maps, after the [pure-map record](2026-09-18-rtcw-mp-pure-maps.md) stopped `mp_sub` falling back to degraded pure under a Q3-sized gamestate estimate | SP pure contract; mods and architecture policy; retail mount observations, malformed and partial-install probes; iortcw MP favorites import |
| Module ABI | Source/Unit/Build/Fixture; 2,215 original/iortcw assertions on x86/x64; bounded bot-entity/movement-input/goal/movement-result/prediction/state/parser/trace/render/vertex/orientation records, 10-bit game entity limit, mode imports/exports, popup/menu/key translation, command values and native x86 game-owned animation pointer handoff; elementary bot actions match the original and iortcw `EA_` functions in 344,099 comparisons each, sanitized on Linux ([bot action record](2026-09-26-rtcw-bot-actions.md)) | Source/Unit; 2,205 original/iortcw assertions on x86/x64; bounded bot-entity/movement-input/goal/movement-result/prediction/state/parser/trace/render/vertex/orientation records, 10-bit game entity limit, mode imports/exports, popup/menu/key translation, command values and WOLFINFO; elementary bot actions match the original and iortcw `EA_` functions in 344,099 comparisons each, sanitized on Linux ([bot action record](2026-09-26-rtcw-bot-actions.md)) | An audit of all 435 SP/MP import ordinals finds 420 dispatched after the [endgame record](2026-09-18-rtcw-sp-endgame.md) added SP's `G_ENDGAME`; the fifteen refused by name are unhandled in the original engines too, deliberately refused (MP `UI_OPENURL`), or reachable only from a Mac SP build (`G_FS_COPY_FILE`). Remaining: retail UI/cgame/qagame execution coverage and live game-data binding; distinguish original/iortcw module flag/renderer contracts |
| Live wire and demos | Source/Unit/Fixture/Live; 1,143 matched native message encodings plus a decoder-only dictionary fixture; first local snapshot with installed modules and pure mode off; `.dm_49` recording under original SP's synchronous-client precondition, and SP replay refused because retail SP cannot replay SP demos (`VM_Call with NULL vm` on its own recording); see the [demo record](2026-09-18-rtcw-demos.md). A zombie's snapshot carries SP's own empty player state, in unit tests ([zombie snapshot record](2026-09-24-zombie-snapshots.md)) | Source/Unit/Fixture/Live, demo Interop subset; 1,117 matched native message encodings; local pure admission and first map snapshot with installed modules; `.dm_60` record, replay and timedemo; retail `WolfMP.exe` replays an fnTech3 demo, and fnTech3 replays demos retail recorded against the fnTech3 server, pure and non-pure; see the [demo record](2026-09-18-rtcw-demos.md); original and iortcw handshake layouts in both directions, `sv_onlyVisibleClients` with its client culling matching the original function on 100,000 cases, and outbound retail admission; see the [handshake record](2026-09-18-rtcw-mp-handshake.md). A zombie's snapshot carries MP's own empty player state, so a retail `WolfMP.exe` client that fnTech3 drops now reads it and runs the notice and `disconnect`, and every retail client reads a quitting server's last message, where both used to fail with `CL_ParsePacketEntities: end of message` ([zombie snapshot record](2026-09-24-zombie-snapshots.md)). Fixed envelope faults and seeded body corruptions of two MP demos are completed or rejected with no crash or hang; one run stopped after 16 of its 47 variants ([damaged-demo record](2026-09-18-demo-faults.md)). Live traffic corrupted one sequenced datagram in eight drops each of 20 `mp_beach` sessions cleanly, with no crash, caught fault or hang, and the server answers throughout ([corrupted-traffic record](2026-09-26-net-corruption.md)); scripted loopback sessions with retail `WolfMP.exe` 1.4 pass in both directions and on an fnTech3 listen server -- join, chat both ways, a map change, `map_restart`, reconnect, server quit, a kick, a UDP pak download inside a mod and a retail and an fnTech3 client on one server -- and retail's browser lists fnTech3's server, which it had dropped for want of `gamename wolfmp`; a public server heartbeats `Wolfenstein-1` and ends with one `WolfFlatline-1`, as retail does, and sends its masters the `gameCompleteStatus` match report, which retail announces but, once it has sent a heartbeat, addresses to a master it never resolved, so none arrived from it; a server whose cfg sets `sv_maxclients` now takes it before allocating clients, as `WolfMP.exe` 1.4 does (0x438e98), so its first warm-up restarts in place where fnTech3's had reloaded the map ([legacy-server record](2026-10-05-legacy-server-hosting.md)); all 94 damaged variants of the two RTCW MP fault-corpus demos end in completion or rejection, where retail `WolfMP.exe` crashes on two and stalls on two more that fnTech3 rejects ([demo-fault record](2026-10-08-rtcw-demo-faults.md)) | MP: ten-minute mixed sessions with retail `WolfMP.exe` 1.4 at 10% loss pass in both directions, as do the short sessions ([sustained sessions record](2026-10-08-legacy-sustained-sessions.md)). Sustained live state integration beyond them, netchan rules, iortcw peer runs, retail transcripts, broader demo coverage (other maps, active play, seeking within damaged demos) and bidirectional interoperability |
| Assets/renderers/audio | Source/Unit/Build/Fixture; RTCW v47 selection, stock `escape1` collision probe, frozen render records, basic scene/polygon/gradient imports, CPU 2D comparisons, MDC decoding/reference comparisons and native drawing through a synthetic Q3 fixture; indexed MD3/MDC/MDS tags, MDS poses and continuous LOD pass installed-asset CPU comparisons; native synthetic MDS draws through all three renderers; skin metadata/model lookup matches all 412 installed skins against four references; brush-surface shader lookup matches 1,925 cases per reference; explicit VM pointer/basic UI sound imports and source-compared dynamic/model lights integrated; entity types 2-8 reach the generated-surface path as in retail `WolfSP.exe`, the water ripple flat and the tapered rail core as the debug axis, in all three renderers ([ripple record](2026-09-23-rtcw-water-ripples.md)); the engine's static map lights, surface-light proxies and `r_dlightTest` in the per-pixel modes, with the modules keeping their 32 lights a frame, shown on, off and on again in a live `escape1` client in all three renderers ([engine scene lights record](2026-09-24-rtcw-engine-lights.md)); a static light file for each of the 32 official maps, fitted from its `light` entities and served from the package, drawn in a live `escape1` client in all three renderers ([map lights record](2026-09-24-rtcw-map-lights.md)); of 102 damaged copies of `escape1`, none crashes, hangs or faults: the loaders' index checks refuse the six that faulted on unchecked indices ([damaged-map record](2026-09-26-bsp-faults.md), [index-check record](2026-09-27-bsp-index-checks.md)), and change how none of the 32 installed single-player maps loads, in any renderer; the loaders' node-tree walk refuses three more copies, whose trees loop back on themselves or share a subtree at every node down the deepest path and which crashed (two) or hung (one) without it, and no installed RTCW tree ([node-tree record](2026-09-27-bsp-node-trees.md)); built without the walk, those two cycles now end in a reported stack overflow rather than a crash ([stack overflow record](2026-09-28-stack-overflow-reports.md)). Live: world and entity collision answers 116,640 traces on eight maps, 12,000 point contents, 256 links and 1,600 box queries as retail `WolfSP.exe` does, but for five traces that differ in floating-point precision; players flagged as capsules are traced as boxes, as `WolfSP.exe` traces them ([trace record](2026-10-08-rtcw-traces.md)); a close of an area portal already shut goes on, as in retail, where fnTech3 had dropped `escape1` ([area-portal record](2026-10-08-rtcw-area-portals.md)) | Source/Unit/Build/Fixture/Live; same contracts plus rotated pictures, stock `mp_beach` collision probe and inspected French native menu captures in all three renderers; the same entity types checked against retail `WolfMP.exe`; a Thompson fired into `mp_beach`'s surf draws flat ripples in all three renderers, where the previous build stopped with `Bad reType`; `mp_beach` loads in all three renderers with the console in RTCW's own charset rather than Quake Live's fonts ([ripple record](2026-09-23-rtcw-water-ripples.md)); the same engine scene lights, drawn in `mp_ice` replays in all three renderers and byte-identical to the build without them when switched off, in mode 0 or with `r_dynamiclight 0` ([engine scene lights record](2026-09-24-rtcw-engine-lights.md)); a static light file for each of the 15 official maps, fitted for MP's `r_dlightScale`; `mp_castle` and `mp_base` replays in all three renderers load them from the package, and `mp_castle`'s frames with them on match its frames with them off byte for byte in mode 0 and with a package lacking them ([map lights record](2026-09-24-rtcw-map-lights.md)); stencil shadows darken by GL's 0.6 in the Vulkan and RTX renderers too ([stencil shadow record](2026-09-24-stencil-shadow-finish.md)); of 101 damaged copies of `mp_beach`, none crashes, hangs or faults: the index checks refuse the six that faulted ([damaged-map record](2026-09-26-bsp-faults.md), [index-check record](2026-09-27-bsp-index-checks.md)), and change how none of the 15 installed multiplayer maps loads, in any renderer; the node-tree walk refuses three more copies of `mp_beach` built that way, which crashed (two) or hung (one) without it ([node-tree record](2026-09-27-bsp-node-trees.md)); built without the walk, those two cycles now end in a reported stack overflow rather than a crash ([stack overflow record](2026-09-28-stack-overflow-reports.md)). Live: world and entity collision answers 86,220 traces on six maps, 9,000 point contents, 178 links and 1,200 box queries as retail `WolfMP.exe` does, curves cut and tested as its own, but for four traces that differ in floating-point precision ([trace record](2026-10-08-rtcw-traces.md)); a close of an area portal already shut goes on, as in `WolfMP.exe` (Source/Unit; [area-portal record](2026-10-08-rtcw-area-portals.md)) | A same-frame ripple comparison with retail, which runs the same probe to the same place but paces its frames differently; client-side and remaining-map collision against the original engines, and the open float precision decision, full RTCW material flags, live corona/decal and GL fog parity, Vulkan/RTX scene-fog original-engine parity and native RT runtime validation, projected world-light GPU parity, retail MDS scene/tag and skin/model assembly validation, shader remap/GPU parity, remaining shader/fog/animation behavior, music/fade/cinematic contracts and further retail RTCW engine-render-target screenshots; MDC/MDS ray transport has CPU evidence only |
| Console and HUD text | Source/Unit/Build/Live; retail `WolfSP.exe`'s `gfx/2d/bigchars` charset, `console` background and `console2` logo layer are registered, and the logo is drawn in retail's place, in engine screenshots beside retail's own at 640x480 and at 1280x720 ([console logo record](2026-09-23-wolfenstein-console-logo.md)). Source/Unit/Live; notify lines, their `con_notifytime` default of 3 and the chat line as `WolfSP.exe` draws them, including while disconnected and over a cinematic, in unit tests and a live FFA `escape1` frame ([notify record](2026-09-23-profile-notify-lines.md)) | Source/Unit/Build/Live; retail `WolfMP.exe`'s `gfx/2d/hudchars` charset, `console-16bit` background and `console2-16bit` logo layer are registered and drawn, and match a retail engine screenshot at 640x480; the shipped HUD rules keep matching the charset handle the cgame shares, in unit tests and a live `mp_beach` frame ([console record](2026-09-23-rtcw-mp-console.md)). Notify lines with `WolfMP.exe`'s default of 7, its translated chat prompts, and nothing drawn in limbo, in unit tests and live `mp_beach` frames ([notify record](2026-09-23-profile-notify-lines.md)) | Retail's separator bar and 8x16 text cell; whether RTCW's console should default to the textured style; SP's console hidden while connecting unless `con_debug` is set |
| Navigation | Source/Unit/Build/Fixture/Live, RCD15 Interop subset; live AAS8/RCD15 loading; missing/damaged-cache generation and writing, both worlds on three installed SP maps, fresh-process reload and bidirectional retail cache interchange ([recovery record](2026-09-20-rtcw-cache-recovery.md)); checked 128-state goal/movement lifecycle; shared entity updates with owned area links, per-world blocking, checked BSP imports, frame/state/time adapters and empty-config item updates; basic/trace/route/visibility, hide/attack, movement prediction and release ShowRoute imports; SP corner search; 70-pair graph/cost, route/cache and visibility comparisons with original/iortcw; tactical/prediction synthetic comparisons and 64-member installed prediction with stated arithmetic limits; the loader now also applies the shared navigation checks, which refuse what the AAS8 reader let through -- a face index of 0, area 0's face range, portal 0's clusters and numbers, a portal off its cluster's border -- where a copy of `escape1` whose portal 0 names a cluster past the table had faulted in `AAS_RemoveRoutingCacheInCluster`; all 70 installed files pass, and the 32 maps end as before ([navigation record](2026-09-28-aas-index-checks.md)) | Source/Unit/Build/Fixture; checked 64-state goal/movement lifecycle; same shared entity/ownership, BSP, frame/state/time, query, prediction and empty-item contracts; MP truncating costs versus SP ceiling, grid search and tactical occupancy verified; installed prediction uses SP geometry; RCD12 loading, missing/damaged-cache generation and writing use executable fixtures; stock MP keeps bots disabled; no installed MP map ships navigation, and the same checks apply ([navigation record](2026-09-28-aas-index-checks.md)) | Dynamic-entity prediction and original BSP parity; cold-cache tactical divergences, installed-map/live tactical observations; installed MP-cache observations, generated waypoint/route-cost differences from retail, x87/fast-math numeric and decision differences, original-engine runtime route and gameplay evidence |
| Gameplay and lifecycle | Open campaign; Source/Unit/Fixture for AI state lifecycle, camera evaluation, clean mission-briefing shutdown, shared movie-owner cleanup, main-menu shutdown and partial navigation-load cleanup; Windows x86 GL startup progresses through the opening cinematic to inspected first-person knife/HUD captures with either active audio backend and nonzero SP speech queries; Source/Unit for each cast AI's reliable window, 256 commands in every slot as `WolfSP.exe` rings them, where a slot no player had used rang Quake III's 64 ([bot window record](2026-09-24-bot-reliable-windows.md)); Source/Unit/Live for the settling frames of a map load and `map_restart`, each run at the current time before advancing it as `WolfSP.exe` does, from a restored save's time too: the retail game's `rocket` script timeline for 2.5 s after a load or restart matches `WolfSP.exe`'s line for line ([settle order record](2026-09-24-settle-frame-order.md)); Source/Unit/Live for map commands while the game changes level: ignored while `g_reloading` holds anything but 2, as `WolfSP.exe`'s map command does, where a map typed at a briefing had started under the game's own level change, and the disconnect command clears both loading states first ([level exit record](2026-10-08-rtcw-sp-level-exits.md)) | Source/Unit/Fixture for local map initialization, profile-specific camera timing, briefing-movie playback/shutdown and active-briefing-to-SP handoff; Source/Unit for a bot's reliable window, 256 commands in every slot as `WolfMP.exe` rings them, where a slot no player had used rang Quake III's 64 ([bot window record](2026-09-24-bot-reliable-windows.md)); Source/Unit/Live for a thrown engineer's charge coming to rest on `mp_beach` on listen and dedicated servers, where the previous build dropped at the world-owned charge's trace ([explosives record](2026-09-23-world-owned-explosives.md)); Source/Unit/Live and inbound Interop subset for `map_restart` after a variable change: only a new `sv_maxclients` reloads the level, as `WolfMP.exe` does, and a retail client connected across a new game type or `sv_pure` gets a `map_restart` command where the previous build sent it a new gamestate; live pure-proof checking is still unlike retail's ([restart record](2026-09-24-wolfenstein-restart-variable-change.md)); Source/Unit/Live for the settling frames of a map load and `map_restart`, each run at the current time before advancing it as `WolfMP.exe` does: the retail game's `mp_rocket` script timeline for 2.5 s after a load or restart matches `WolfMP.exe`'s line for line ([settle order record](2026-09-24-settle-frame-order.md)); gameplay remains open | SP movement/combat, broader character parity, audible speech/facial timing, campaign/AI/save-load/transitions; MP classes/objectives/maps; broader partial initialization, sustained play and shutdown |
| Music and fades | Source/Unit/Build/Fixture; original-source queue/fade and SP loop/mixer comparisons; both-backend clear modes with preserved music and bounded PCM/gain routing; native menu music buffers on OpenAL; legacy initialized while muted | Source/Unit/Build/Fixture; exact two-argument import; native menu music buffers on OpenAL; legacy initialized while muted | Measured output and original playback parity; game-script triggers, save-load restoration and backend/device lifecycle |
| One-shot audio | Source/Unit/Build/Fixture; checked native/QVM imports, original-source channel-policy and fixed-range gain comparisons, legacy/OpenAL lifetime and failure handling; SP client-channel replacement and auto-channel separation. Source/Unit/Live: the OpenAL backend renders SP's one-shots and loops through the engine's own features, not retail's pan. They are positional, carried by the device's distance model across retail's span with a horizon fade, traced for occlusion, sent to the zone or traced room and given a tone class by channel. On `escape1` the loops are traced to occlusion 0.84 to 1.00 and sent to the 397-zone sidecar's room, and the death cry at the listener takes the `voice` class. Retail's exact gain and pan stay in the legacy mixer ([OpenAL voices record](2026-09-24-wolfenstein-openal-voices.md)) | Source/Unit/Build/Fixture; same boundaries/backends, original MP auto-channel reuse and FIFO policy; inspected briefing startup on both backends. Source/Unit/Live: the same OpenAL rendering; on `mp_beach` the in-range loops are positional `world` sounds at retail's 89/127 with sends into the 246-zone sidecar, where the previous build played every one head-relative with no send ([OpenAL voices record](2026-09-24-wolfenstein-openal-voices.md)) | Queued/threaded timing; output-device/PCM parity; sustained game audio and resource pressure; documented safe exhaustion differences; a listening test; an in-range remote one-shot observed at runtime; HRTF on a runtime that offers it |
| Entity streams | Source/Unit/Build/Fixture; checked start/stop imports, shared decoder, eleven entity slots, queued replacement, immediate entity stop, bounded fades, both-backend synthetic stream runtime and installed introductory speech with nonzero amplitude | Source/Unit/Build/Fixture; checked start import, twenty-three entity slots, original intro-repeat loop policy, both-backend synthetic silent stream runtime | Original PCM/device timing; dialogue and mouth synchronization parity; save/load restoration; source-different SP loop correction and fade-slot admission; device recovery/resource pressure |
| Speech amplitude/default sound | Source/Unit/Build/Fixture; signed one-shot/peak-stream measurements on both backends; 299,264 matching cases per original/iortcw SP reference on three targets; installed intro completes with nonzero queries; eight-frame silent default | Source/Unit/Build/Fixture; original zero speech response; 512-frame silent default; both-backend installed briefing startup | Original clock/output/facial parity; full dialogue/codec coverage; device recovery and resource pressure |
| MP localization | Not an SP import; SP module key names now follow `cl_language` as original SP's do (5,472 original-function comparisons across both modes); see the [key-name record](2026-09-18-rtcw-key-names.md) | Source/Unit/Fixture/Live; all eight installed dictionaries (1,201 records), 818,856 translations per original/iortcw reference, Windows x86/x64 and Linux x86 sanitizers; inspected French native main menu in all three renderers; module key names in the player's language, pinned to retail `WolfMP.exe`, and a live `cl_language` change switching the spectator hint's text and key names exactly as retail does; see the [key-name record](2026-09-18-rtcw-key-names.md). A dedicated server's console echoes broadcast prints without the `[lon]` and `[lof]` tokens, as `WolfMP.exe` does, while clients still receive them, in unit tests and a live `mp_beach` log that matches retail's ([broadcast echo record](2026-09-24-rtcw-et-broadcast-echo.md)). A client's drop is announced as `print "[lof]%s^7 [lon]%s\n"`, as `WolfMP.exe` does, so a client translates the reason and leaves the name as it is; a retail observer received it from a fnTech3 server ([drop notice record](2026-09-24-rtcw-et-drop-notices.md)) | Remaining UI/cgame rendering, byte/font agreement, and menu files after a live change (`ui_restart`); optional translation-authoring diagnostics remain unimplemented |
| Cinematic startup/presentation | Source/Unit/Build/Fixture/Live; catalog-owned repeat policy and wolfintro selection, letterbox import/command, 24 fps header timing, clock handoff/rewind/wrap and inspected movie frames on all three raster renderers; [bounded decoding and same-process malformed-movie recovery](2026-09-17-roq-bounds.md), installed movie corpus and final-frame checks; seeded corruptions of `menu.roq`, `menu2.roq` and `wolfintro.RoQ` pass the decoder under AddressSanitizer, and 60 in one running SP client recover to the unmodified movie ([RoQ fuzz record](2026-09-18-roq-fuzz.md)) | Source/Unit/Build/Fixture/Live; catalog logo/intro policy, original 30 fps behavior, missing-startup-movie fallback and briefing movie startup; same bounded decoder, installed movie corpus and malformed-movie recovery; seeded corruptions of `mp_beach.roq` and `mp_rocket.roq` pass under AddressSanitizer, and 60 in one running MP client recover | JPEG no-op matches original SP/MP source. Original-engine behaviour on the corrupted movies, original visual/timing parity, audible A/V synchronization, game-script fades and full campaign movie transitions remain required |
| Shared release | Open | Open | Q3/QL non-regression, clean supported-platform builds, complete suite and same-artifact runtime evidence |

The existing Q3/QL blockers below remain in force. Their evidence does not
transfer to either RTCW mode.

1. **Boundary ABI verification.** Q3 adapters and profile-specific server-memory
   access are integrated, with Q3 QVM and QL native client/server observations.
   Complete client and extension audits, broader Q3 native module coverage, and
   runtime probes are still required.
2. **Live wire evidence.** Live usercmd/entity/playerstate call sites select
   explicit wire profiles. Field-table parity and transactional malformed-input
   regressions pass. Short bidirectional retail sessions, authenticated QL
   admission and full map changes now pass for both profiles, including the
   [clean Windows x86 package](2026-09-08-clean-windows-retail-package.md).
   Complete packet-flow coverage, Internet impairment and sustained sessions
   remain open; the observed private-host/WSL tests do not close those gates.
3. **QL snapshot tail (resolved).** The known QL player-state fields occupy
   `0x1f8` bytes, while the retail native snapshot reserves `0x250`. The
   [tail record](2026-10-08-ql-snapshot-tail.md) reads every instruction in
   `quakelive_steam.exe` that can reach the other `0x58`: they are the game
   module's own medal and scoring state, copied into server frames and never
   encoded, and every snapshot the retail client hands its client game holds
   them as zeroes, live or from a demo. fnTech3 zero-fills them on the client
   and never reads them on the server, so its behaviour matches and does not
   change. The steps that led there follow.
   The [snapshot marshalling correction](2026-09-07-ql-snapshot-marshalling.md)
   now converts the frozen known prefix explicitly and tests zeroed unknown
   bytes and destination guards. The [guarded native snapshot probe](2026-09-08-native-snapshot-containment.md)
   exercises exact output bounds, rejected-request preservation and zero-tail
   containment through the running client; the tail's meaning remains unverified.
   The [retail consumer layout inspection](2026-09-08-retail-snapshot-layout.md)
   independently pins the 592-byte copy, snapshot-buffer separation and entity
   offsets/stride to the installed module. It confirms that the tail is copied,
   without establishing its producer values or downstream semantics.
   The [retail producer and delta-reader checkpoint](2026-09-08-retail-snapshot-producer.md)
   traces the full history copy and, in controlled CPU emulation, establishes
   zero tail bytes from a null baseline and preservation of a supplied nonzero
   tail across field updates. Other writers and tail semantics remain open.
   [Live retail tail sampling](2026-09-08-retail-snapshot-tail-live.md) observes
   490 valid records across all 32 slots with zero tails during a connected
   map-change session. Finite sampling does not resolve those remaining gaps.
   The [compressed player-state payload checkpoint](2026-09-08-retail-playerstate-wire.md)
   runs retail Huffman initialization and real bit IO in CPU emulation, checks
   six states in both encoding directions, and pins two accepted retail
   movement-byte variants in Windows/Linux codec regressions. It is not live
   peer interoperability or exhaustive packet coverage.
   Those retail-derived vectors also pass all eight starting bit offsets and
   reject every shorter framed byte prefix without modifying separate or
   aliased output state (392 combinations, Windows x86 and Linux i686).
   The [retail field corpus](2026-09-08-retail-playerstate-field-corpus.md)
   expands differential checks to 478 synthetic transitions in both directions,
   including every scalar field and array slot, and retains 58 retail-generated
   scalar boundary fixtures. Windows x86 and Linux i686 decode all retail
   outputs correctly; that corpus alone does not prove full-process interoperability.
   The [retail entity-wire checkpoint](2026-09-08-retail-entity-wire.md)
   similarly checks 402 entity writer cases (all byte-identical), 400 decoded
   payloads, and two intentional no-output cases. It pins 58 observed field
   descriptors and adds retail-generated fixtures passing Windows/Linux codec
   regressions, without claiming a live snapshot exchange.
   The [retail keyed-command checkpoint](2026-09-08-retail-usercmd-wire.md)
   corrects QL movement-byte preservation while retaining Q3's clamp. All 24
   writer payloads match retail. The subsequent
   [key-mask checkpoint](2026-09-08-retail-usercmd-key-mask.md) traces the extra
   key bit through retail batch acceptance and GetUsercmd, and matches that
   QL behavior without changing Q3. Its controlled branch and finite corpus
   do not establish complete live command interoperability.
   A [post-correction runtime check](2026-09-08-post-codec-runtime.md) passes
   both Q3/QL self-pairs with the same Windows x86 binaries, exact split-response
   HTTP downloads, pure-to-active admission, engine screenshots and normal
   shutdown. QL reaches its join panel; active match gameplay is not claimed.
   The later [QL active-match checkpoint](2026-09-08-ql-active-match.md)
   joins campgrounds/ffa through an engine command, with a first-person player
   HUD, five participants, `IN_PROGRESS` samples and bot score progression.
   It remains a short self-pair; keyboard/mouse combat and independent retail
   engine interoperability are not exercised.
   The [active QL demo checkpoint](2026-09-08-ql-active-demo.md) records a joined
   FFA session and replays all 564 messages in normal/timedemo modes with native
   HUD screenshots, EOF completion and normal shutdown. Seeking and independent
   broader retail-engine demo coverage remain unverified. Short cross-engine normal
   and timedemo playback now passes both directions in the
   [retail demo checkpoint](2026-09-08-ql-retail-demo-interop.md).
   [Snapshot history validation](2026-09-07-snapshot-history.md) also rejects
   malformed/stale module requests before writes. The subsequent
   [entity-history correction](2026-09-07-entity-history-wrap.md) covers parser
   increments and client/demo indexing, with executable wrap tests and both
   profiles' record/playback probes; sustained network-wrap evidence is open.
4. **Detection is not yet authoritative.** Known retail central-directory
   metadata now replaces filename-only evidence; it remains an identity hint,
   not payload authentication or proof of a complete installation. Profile-owned Q3/QL Steam discovery
   now passes bounded Unit/Build/Fixture checks; archive identity and complete
   profile-specific asset validation remain open.
5. **Release packaging remains open.** Canonical metadata and primary artifacts
   now use fnTech3 identity. Retained external sidecar names, unsupported legacy
   packaging paths and complete release evidence remain open. The
   [fresh Windows package](2026-09-08-clean-windows-retail-package.md) now passes
   a clean build/full suite, deterministic archiving, both-profile retail map
   changes and unchanged extraction checks. This does not close independent
   compiler reproducibility or the complete multi-platform release workflow. The
   [clean Linux media refresh](2026-09-08-linux-media-build-refresh.md) builds the
   later WAV/Ogg corrections in Ubuntu 20.04, passes the full configured suite
   and glibc ceiling, and repeats deterministic packaging plus Q3 pure-client
   and QL API 10 dedicated smoke checks.
   The [clean Windows media refresh](2026-09-08-windows-media-build-refresh.md)
   packages e114ded from a fresh export, repeats the full suite and all four
   retail map-change directions, and verifies both-profile file audio from
   identical packaged binaries. Hosted CI remains blocked by the account issue.
   The [engine-only correction](2026-09-07-engine-only-packaging.md) removes
   inherited closed-source provider staging. Subsequent clean CI now validates
   that packaging path; see the [regression checkpoint](2026-09-08-regression-checkpoint.md).
   Reproducibility and packaged runtime gates remain open.
6. **Q3 coverage remains incomplete.** Local QVM startup, pure admission, world
   rendering, and HUD are observed. The RTX raster black geometry defect is fixed
   and verified on the frozen demo; broader formats, native modules, remote peers,
   and packaged runtime paths remain open.
7. **QL coverage remains incomplete.** Unified native-module startup, local pure
   admission, and join UI/world rendering are observed. Short authenticated retail
   sessions now pass both directions, including first-person warmup on the retail
   dedicated server; see the
   [dedicated checkpoint](2026-09-08-ql-dedicated-retail-interop.md). Authentication faults,
   WebUI presentation, remote peers, broader gameplay, and lifecycle stress still
   require new fnTech3 evidence.
8. **Retail Q3 interop now has short bidirectional live evidence.** Launching
   the installed retail executable with isolated writable homes passes pure
   admission, active play and engine screenshots in both directions against
   Windows x86 fnTech3. The
   [retail checkpoint](2026-09-08-q3-retail-interop.md) retains earlier startup
   and short-lifetime probe failures separately. Remote, long-session and
   broader protocol coverage remain open.

The [reliable-command transaction correction](2026-09-08-server-command-transaction.md)
rejects truncated commands before history/hash publication and removes signed
overflow from duplicate detection. Both-profile executable boundary tests, Linux
sanitizers and short retail-server map changes pass; malformed live packets and
sustained counter-wrap interoperability remain open.
The [reliable-history follow-up](2026-09-08-reliable-history-wrap.md) bounds demo
command emission and module history lookup across signed counter wrap, with
executable boundary tests, Linux sanitizers and both-profile re-record/replay
controls. Live counter-wrap interoperability remains unverified.

## Open live gates

Hosted CI currently cannot start new jobs because GitHub reports an account
payment or spending-limit issue; see the [QL checkpoint](2026-09-08-ql-downloads.md).
This is an external release-gate blocker, not a reported build/test regression.
Local compatibility work can continue while the account owner resolves it.

### Quake III Arena

- Windowed clean-install startup and engine-screenshot smoke scenes using
  legitimate Q3 assets.
- Q3 QVM and supported native module loading with exact trap/API behavior.
- Protocol 68/71 golden and live sessions, including pure/download flows.
- Broaden the [bidirectional FnQuake3 peer result](2026-09-07-q3-reference-interop.md)
  and the loopback retail sessions of the [legacy-server record](2026-10-05-legacy-server-hosting.md) to
  reproducible pinned peers, remote network cases and a live master listing.
  Ten-minute mixed sessions with retail `quake3.exe` 1.32 at 10% loss pass in
  both directions ([sustained sessions record](2026-10-08-legacy-sustained-sessions.md)).
- Record/playback for every claimed Q3 demo format and representative malformed
  inputs.
- Renderer/collision/asset coverage across the claimed BSP and media formats.

### Quake Live

- Re-run legitimate retail qagame/cgame/UI module probes through the fnTech3
  `ql` profile and packaged binaries.
- Broaden retail protocol-91 packet captures/server coverage and prove a retail
  QL client can join the fnTech3 dedicated server.
- Complete `.dm_91` seek/timedemo and malformed message-body stress coverage.
- Exercise live advertisements and deterministic missing-service fallback in
  every enabled renderer.
- Stress WebUI device loss and recovery without a client restart. Resize,
  renderer restart and switch, and a crashed child that leaves the native UI
  pass ([lifecycle stress record](2026-10-08-ql-webui-lifecycle-stress.md)).
- Complete live match-report conversion, logged-in unattended Workshop
  subscription/download, provider loss/fault injection, and sustained
  GameServer callback tests.

### Return to Castle Wolfenstein

- Complete the [RTCW gate matrix](#rtcw-gate-matrix) for both modes using
  legitimate installations and the same Windows x86 engine binaries.
- Extend the SP level-exit probe from its nine level changes to the whole
  campaign, with sustained AI/combat and broader save/load compatibility.
- Extend local MP map probes to sustained classes, weapons, objectives,
  pure admission, and bidirectional retail/reference peer interoperability.
- Validate claimed demos, renderer/material/fog behavior, audio/input, and
  partial initialization/recovery/shutdown for each mode.
- Broaden real SP/MP menu handoffs while preserving mode-specific configs,
  modules, filesystem rules, and process-lifetime profile isolation.

### Shared release

- Run the full automated suite and supported build matrix on a clean checkout.
- Run every applicable client and server path of all sixteen profiles from the
  same packaged executable set with isolated homepaths and no file replacement
  between launches. Explicitly retain the unwritten Jedi
  single-player/client/wire boundaries until implemented.
  Each experimental profile's open compatibility gates must be resolved
  through evidence before full release compatibility can be claimed.
- Validate only windowed sessions unless fullscreen permission is explicit.
  Rendering evidence must come from the engine `screenshot` command, never an
  operating-system capture. Automated probes must not control user input
  without narrowly scoped permission.
- Archive non-proprietary logs, manifests, packet/demo fixtures, checksums, and
  test summaries. Do not archive retail assets, modules, runtimes, tickets, or
  credentials.

## Evidence snapshot: merger foundation

The following evidence establishes the starting point, not compatibility:

- FnQuake3 source baseline pinned at
  `7d8c9629cdc7979f09697266470359101ffd9eda`: its existing Python suite passed
  624 tests with 9 skips and 440 subtests during the import audit. The dirty
  local worktree was catalogued separately and was not treated as committed
  baseline evidence.
- FnQL source baseline pinned at
  `fce44fe714564621e6ceb79e3da7c53089646af2`: its existing Python suite passed
  1,192 tests with 7 skips and 24,957 subtests during the import audit.
- On 2026-08-21, a fresh Windows x86 MSVC Meson configuration of the merged
  baseline compiled all 1,142 build steps, including the client, dedicated
  server, renderers, and the focused fnTech3 game-profile test executable.
- The game-profile test executable separately passed under a strict C++17
  compile. No fnTech3 game runtime was launched for this foundation snapshot.
- The frozen Q3/QL ABI headers passed standalone C11 checking and strict C++17
  layout tests, including i686 size/offset parity against their pinned source
  structures. This proves the declarations, not their complete runtime use.

Later evidence must be appended or linked with its commit and environment; do
not silently reinterpret these baseline results as proof for subsequent code.

## Full-compatibility sign-off

The unified goal is complete only when every required row in every applicable
game/mode matrix passes at its required evidence level for all sixteen profiles
in the [registered scope](#unified-goal-and-supported-game-scope), all applicable
blockers and open live gates are closed, and the same release artifacts pass:

1. automatic and explicit profile selection;
2. exact module API/ABI and asset loading;
3. client and dedicated-server networking in both interoperability directions
   where applicable;
4. all claimed demo formats and malformed-input gates;
5. representative gameplay/render/audio/input sessions using legitimate data,
   including RTCW SP campaign/AI/save-load and RTCW MP objectives;
6. cross-profile isolation and deterministic packaging; and
7. complete automated, build, and documented windowed runtime gates.

Sign-off must identify the exact release artifacts and evidence bundle. A
partial profile, one-way client connection, inherited FnQL result, or source
claim cannot satisfy the goal. A release may make a narrower, explicitly
documented profile/platform claim, but that does not complete the unified goal.

## WolfET gate matrix

The [2026-10-15 compatibility round](2026-10-15-compatibility-round.md#enemy-territory-area-portals)
gives Enemy Territory RTCW's rule for an area portal closed while it is
shut. Both 2.60b executables skip such a close and flood the areas as after
any close (`etded.exe` `0x40c44c..0x40c45d`, `et.exe` `0x426bdc..0x426bed`),
where fnTech3 had kept Quake III's drop (Source, Unit). No ET level was run
to such a close. No gate closes.

The [detail-floor follow-up](2026-09-18-wolfet-lod.md#verification) fixes a
Linux i686 x87 rounding difference at ET's corpse detail floor. Synthetic
arithmetic checks preserve the existing Windows vertex count; renderer and
interoperability gates keep their existing status.

The [October 2 input/launch follow-up](2026-10-02-input-launch-fixes.md)
corrects ET's sound-zero placeholder for player aliases, including jump, on
both audio backends. Exact PCM, registration and reload checks establish
Source/Unit evidence; hearing a live jump event remains open.

The [reported-fault follow-up](2026-10-01-observed-compatibility-fixes.md) adds
production command-builder regressions for ET action/any-key bits and native
UI/cgame pure verification across renderer restarts. An approved runtime test
enters 2560×1440 fullscreen and returns to its 800×600 window; all three cgame
starts reach their first snapshot with pure admission intact. The rebuilt client
also passes pure retail-server admission and joins both teams on all six stock
maps with `sv_onlyVisibleClients 1`; a separate UDP session checks fnTech3's
extended reply. This adds outbound admission evidence; sustained gameplay,
live multi-player culling and broader interoperability remain open. The
[demo follow-up](2026-10-01-wolfet-demo-regressions.md) adds current-build
record/playback/timedemo and retail exchanges with team/spawn player states,
plus malformed inputs; the demo gate remains partial.

The [area-routing correction](2026-10-01-wolfet-area-routing.md) gives ET
its own goal-row seed, avoidance penalties, contents rules and eligible-link
count. Production matches 42 original-executable cache observations and 768
contents combinations over generated records. This is Source/Unit/Fixture
evidence: the stock installation has no navigation files. The
[AAS8 loading correction](2026-10-01-wolfet-aas8-loading.md) supplies ET's
own area records. The [travel-cost follow-up](2026-10-01-wolfet-travel-costs.md)
matches 265 original client/server scalar costs and 42 travel-type results
on generated inputs and adds ET's endpoint exception for large-area entry
restrictions. The [cache-limits follow-up](2026-10-02-wolfet-routing-limits.md)
adds the 16 MiB budget, configured frame-limit miss deferral, retained hits
and forced portal dependencies, and ET's two-cache/table-order eviction.
These are Source/Unit checks. Retail cache comparisons, loaded retail
navigation and live bot paths remain open; generated checks do not establish
live navigation. The same follow-up recorded a source-observed stale
tactical `inlist` gap in avoidance/retreat searches; the
[census-gaps record](2026-10-15-census-small-gaps.md) settles it against
retail `etded.exe`: the danger and retreat searches queue without reading the
mark, the range and hide searches keep the check, a full range search leaves no
mark, and ET has one update record per area, as `etded.exe` allocates, where it
had Quake III's per-cluster count and its tactical searches wrote past it.
The [hide-search record](2026-10-15-wolfet-hide-search.md) then rebuilds the
hide search from retail `AAS_NearestHideArea`: its ladder, distance, enemy,
occupancy and visibility tests, one game question an area, and its step count,
which past 3,000 stops queuing but still empties the queue. Fifteen scenarios
on a generated corridor pass and nineteen mutations fail them; stock ET has no
navigation to run the search on, and its game never calls it.

Enemy Territory is the fifth immutable profile and fourth title. Its initial
[implementation record](2026-09-17-wolfet-contracts.md) separates original-source
checks, generated packet fixtures and installed dedicated observations.

| Gate | Evidence | Remaining requirement |
| --- | --- | --- |
| Profile / identity / discovery | Source, Unit, Live, discovery Interop subset | Default/client gates preserved. A public server heartbeats `etmaster.idsoftware.com`'s port with `EnemyTerritory-1` and ends with one `ETFlatline-1`, as `etded.exe` 2.60b does (0x42eec0), sends its masters the `gameCompleteStatus` report its game asks for at a match's end (retail announces it but sent none that arrived, as in RTCW), and both engines' browsers list both servers ([legacy-server record](2026-10-05-legacy-server-hosting.md)); broader installs and platforms remain open. |
| Module ABI / server memory | Source, Unit, Live subset | 1,326 layout/constant/ordinal checks, native qagame startup, and every declared game import handled: 204 of 212, the other eight being ordinals retail ET refuses itself. See the [import](2026-09-17-wolfet-game-imports.md), [navigation](2026-09-17-wolfet-navigation.md), [visibility](2026-09-17-wolfet-visibility.md) and [prediction](2026-09-17-wolfet-prediction.md) records. The cgame surface handles 146 of 147 imports and the UI surface 114 of 121, each refusing the rest by name; see the [cgame](2026-09-18-wolfet-client-boundary.md), [UI](2026-09-18-wolfet-ui-boundary.md), [sound](2026-09-18-wolfet-audio.md), [light](2026-09-18-wolfet-light.md), [client services](2026-09-18-wolfet-client-services.md), [text and files](2026-09-18-wolfet-text-and-files.md), [camera](2026-09-18-wolfet-camera.md) [scene geometry](2026-09-18-wolfet-scene-geometry.md) and [global fog](2026-09-18-wolfet-global-fog.md) records. Six of the open UI ordinals are ones retail ET refuses itself. The navigation imports have no retail content to run against. ET's area-cache update and contents flags now match original-function observations on generated records, including goal costs, avoidance and route selection ([area-routing record](2026-10-01-wolfet-area-routing.md)); its AAS8 disk adapter now passes generated loads and failure cleanup ([loading record](2026-10-01-wolfet-aas8-loading.md)), and measured geometry costs match the original client/server on 265 generated inputs, with endpoint entry restrictions corrected ([travel-cost record](2026-10-01-wolfet-travel-costs.md)); ET's routing budget, configured miss-deferral limit and table-order two-cache eviction now have production Source/Unit evidence ([cache limits](2026-10-02-wolfet-routing-limits.md)); the tactical searches now queue and leave marks as `etded.exe` does, over one update record per area, as it allocates ([census-gaps record](2026-10-15-census-small-gaps.md)); the hide search now follows `etded.exe`'s on generated worlds ([hide-search record](2026-10-15-wolfet-hide-search.md)); retail cache-query/eviction comparisons, loaded navigation and live bot paths remain open. The cgame and UI surfaces have both been reached from a developer client: the retail UI drew ET's first-run profile dialog through its font, drawing and precompiler imports, and the retail cgame called its trace, per-map fog, decal, corona and skin imports, among others, while it loaded oasis and then all six stock maps on all three renderers; see the [client run](2026-09-18-wolfet-client-run.md) and [map load](2026-09-18-wolfet-map-load.md) records. Neither record lists every import those runs reached, so the cgame and UI counts remain source and unit evidence. |
| Protocol-84 message bodies | Source, Unit, bidirectional admission Interop subset | 1,169 original-engine comparisons and inbound retail sessions. The client now keeps protocol 84 through an original ET server's bare challenge/connect replies and honors its optional visibility value; pure outbound admission to both teams passes on all six stock maps ([reported-fault follow-up](2026-10-01-observed-compatibility-fixes.md)). Full transport, sustained exchanges and malformed peer behavior remain open. |
| Filesystem / pure / downloads | Unit, Live, bidirectional admission/download Interop subsets | Separate etmain/home and native names. A retail client passes the proof, reaches CS_ACTIVE and joins both teams on all six stock maps; see the [pure record](2026-09-17-wolfet-pure.md). The fnTech3 client now passes pure admission to original ET servers on those same maps after native UI/cgame pak verification and reference flags were carried through filesystem restarts ([reported-fault follow-up](2026-10-01-observed-compatibility-fixes.md)). The official-pak refusal is profile-owned and covers etmain and mp_bin; see the [download correction](2026-09-17-wolfet-downloads.md), and the www redirect has source and unit evidence; see the [redirect record](2026-09-17-wolfet-web-downloads.md). A generated 32,916-byte pak now transfers over UDP in both retail directions, with exact bytes, pure admission and normal exits; a duplicate EOF that caused retail to call an unloaded cgame after the first chunk is corrected. Unsupported URL/fallback redirects were refused safely before file-data parsing ([2026-10-02 round](2026-10-02-compatibility-round.md)); the client now follows a web redirect as `et.exe` 2.60b does -- `cl_wwwDownload 1`, `wwwdl ack`, an HTTP fetch while it stays connected, `wwwdl done` -- from retail `etded.exe` and from fnTech3's server, also when the server lets clients leave to fetch; retail `et.exe` fetches from fnTech3's redirect, leaving and coming back when allowed; a 404's `wwwdl fail` falls back to UDP blocks in both directions; only a fallback page is still refused, naming its URL ([legacy-server record](2026-10-05-legacy-server-hosting.md)). A 1 MiB pak downloads exactly in both directions while fnTech3's side drops 20% of the packets it receives ([downloads under loss record](2026-10-08-legacy-downloads-under-loss.md)). The pure gamestate budget measures ET's own 32 KiB message and tables, where it measured Quake Live's inside 16 KiB and could drop a big map to degraded pure; `etded.exe` publishes `sv_paks` without a budget ([census-gaps record](2026-10-15-census-small-gaps.md)). Files of tens of megabytes, broader maps/mods and a live gamestate-overflow fallback remain open. |
| Assets / collision | Source, Unit, Live subset | Six stock BSPs and installed TAGs load, and all 21 installed MDM/MDX skeletal files decode through bounded readers, and through the renderer's own byte reader with every count agreeing; see the [model](2026-09-18-wolfet-models.md) and [renderer model](2026-09-18-wolfet-renderer-models.md) records. The shipped assets settle three things ET's own source does not: its surfaces and tags are chained rather than strided, a vertex weight names a global bone rather than the surface's bone references, and every one of the 59,400 shipped skeleton frames writes the greater corner of its box first. Every mesh paired with every skeleton -- 110 pairs, 64,284 poses and 4.1 million skinned vertices -- poses, skins, reduces and places its tags inside the bounds each frame declares; see the [drawing record](2026-09-18-wolfet-drawing.md). No image has been produced, and the remaining render assets stay open. Server sweeps test and report a box entity as its own contents, as `et.exe` and `etded.exe` do, in unit and real-trace fixtures; see the [entity box contents record](2026-09-23-entity-box-contents.md). ET now excludes world-handle entities from its multi-entity sweep and point-content queries, preserving the actual world's collision identity. Production fixtures cover all sixteen profiles and retained explicit single-entity traces; no affected retail gameplay scenario was compared ([2026-10-02 round](2026-10-02-compatibility-round.md)). |
| Dedicated gameplay / snapshots | Source, Unit, Live, inbound Interop subset | All six maps initialize under explicit and auto selection; a retail client reaches CS_ACTIVE, joins Axis/Allies, respawns and receives replies to two objective-camera requests. [Callbacks/proxies](2026-09-17-wolfet-snapshots.md) and [viewpoints/server capacity](2026-09-17-wolfet-viewpoints.md) pass executable regressions. A retail client also spawns, moves about 250 units and follows a goldrush-to-battery map change under pure admission; see the [gameplay record](2026-09-17-wolfet-gameplay.md). A thrown engineer's charge now comes to rest on oasis on listen and dedicated servers; the world owns settled dynamite, landmines, satchels, tripmines and smoke, and the previous build dropped at their traces ([explosives record](2026-09-23-world-owned-explosives.md)). The dedicated console echoes broadcast prints without the `[lon]` and `[lof]` tokens, as `et.exe` and `etded.exe` do, while clients still receive them; a live goldrush log matches `etded.exe`'s ([broadcast echo record](2026-09-24-rtcw-et-broadcast-echo.md)). A client's drop is announced with `cpm "%s^7 %s\n"`, as `et.exe` and `etded.exe` do, and not at all when a bot leaves a single-player game type; a retail observer received it from a fnTech3 server. fnTech3's engine `kick` and `clientkick` also shadow the game's own, which drop with `player kicked` and ban the address ([drop notice record](2026-09-24-rtcw-et-drop-notices.md)). A zombie's snapshot carries ET's own empty player state, so a dropped retail client now reads it and runs the notice and `disconnect`, and every retail client reads a quitting server's last message, where both used to fail with `CL_ParsePacketEntities: end of message` ([zombie snapshot record](2026-09-24-zombie-snapshots.md)). `map_restart` runs while a delayed restart is pending, as `et.exe` and `etded.exe` do: a new delay replaces the pending one and its `CS_WARMUP` time, and an immediate restart leaves it to fire later. One rcon-paced sequence on goldrush makes the same restarts and match states on retail `etded.exe` and fnTech3, where the previous build refused every restart once a delay was pending ([pending restart record](2026-09-24-wolfet-pending-map-restart.md)). `map_restart` reloads the level only for a new `sv_maxclients`, as `et.exe` and `etded.exe` do, and a retail client connected across a new game type or `sv_pure` now gets a `map_restart` command rather than a new gamestate; live pure-proof checking is still unlike retail's ([restart record](2026-09-24-wolfenstein-restart-variable-change.md)). An immediate `map_restart` settles in six frames and runs its last frame, each at the current time before advancing it, and leaves bots out of a single-player or co-op restart, as `et.exe` and `etded.exe` do. The retail game's goldrush script timeline for 2.5 s after a restart matches `etded.exe`'s line for line. Retail also enters connecting and loading clients at the restart, where fnTech3 still enters them on their first command ([restart settle record](2026-09-24-wolfet-restart-settle.md)). Broadcast commands now pass over a client whose entity carries `SVF_BOT`, as `et.exe` and `etded.exe` do, so a bot gets only commands addressed to it; on a stock retail dedicated server nothing acknowledges those, and `etded.exe` drops the bot on its 257th unacknowledged command ([bot broadcast record](2026-09-24-wolfet-bot-broadcasts.md)). A bot now rings 256 unacknowledged commands in every slot, as `et.exe` and `etded.exe` do, where a slot no player had used rang Quake III's 64. On a build of the shared tree whose uncommitted bot-library work lets `spawnbot` make one, a fnTech3 ET bot sent a burst of addressed commands held 256 and was dropped on the 257th, where the same build without this change dropped it on the 65th ([bot window record](2026-09-24-bot-reliable-windows.md)). `spawnbot` now makes a bot on a `bot_enable 1` dedicated server, as `etded.exe` does: the bot library reads a character's includes from the game root and sets up Quake III's weapon, item, chat and movement configuration, and the engine registers `bot_thinktime` as 50. On goldrush the bot joins, gets no broadcast and, never thinking without navigation, is dropped once addressed commands fill its window, whose window that earlier build sized at 64 commands to retail's 256; the later bot-window record above corrects that size. The previous build's retail game faulted instead ([bot character record](2026-09-24-wolfet-bot-characters.md)). A map load settles in six frames and runs its last frame, each with its bots at the current time before advancing it, and drops the bots of a single-player or co-op game type with no reason, as `et.exe` and `etded.exe` do. The retail game's goldrush script timeline for 2.5 s after a load matches `etded.exe`'s line for line ([map load settle record](2026-09-24-wolfet-map-load-settle.md)). Live landmine/proxy/brush/lean visibility, rendered cameras, dense retail exchanges, movement parity, combat, objectives, bots with navigation remain open. The later area-routing and travel-cost records above correct the earlier Q3-style goal row and avoidance behavior; the [cache-limits follow-up](2026-10-02-wolfet-routing-limits.md) adds ET's budget, frame limits and eviction. Tactical search marks and the hide search now follow `etded.exe` ([census-gaps](2026-10-15-census-small-gaps.md) and [hide-search](2026-10-15-wolfet-hide-search.md) records). Retail navigation, live hide searches and live cache comparisons remain open. |
| Client / renderer / audio / input | Live subset | **The ET client has run** in a developer build: it draws ET's first-run profile dialog with ET's own fonts through the retail UI module, loads oasis through the retail cgame and draws ET's limbo panel, and quits cleanly with the installation unchanged; see the [client run record](2026-09-18-wolfet-client-run.md). That run found and fixed eight defects no unit test could see, among them every cgame trace running along the wrong line and a string leaking out of a skipped preprocessor branch for every profile. No gameplay is claimed. **All six official maps now load** in the developer client with ET's own configuration on all three renderers, drawing their lightmaps, foliage, decals, coronas and skies and loading their audio zones; that load found and fixed sixteen defects, among them every map drawn without its lightmaps and most of ET's shaders refused; see the [map load record](2026-09-18-wolfet-map-load.md). ET's cgame and UI records, renderer transfers, key numbers, browser numbering and 2D geometry pass regressions, and both dispatchers are wired; see the [cgame](2026-09-18-wolfet-client-boundary.md) and [UI](2026-09-18-wolfet-ui-boundary.md) boundary records. ET's sound surface is carried on both boundaries, with its own volume, loop start time and stop-sounds flag; see the [sound record](2026-09-18-wolfet-audio.md). The OpenAL backend renders ET's voices through the engine's own features: positional, retail's span as the device's distance model, traced occlusion, tone classes, and sends into the six stock maps' audio zones. On `oasis` the loops play positionally with sends into the 512-zone sidecar's hall, where the previous build sent them nothing ([OpenAL voices record](2026-09-24-wolfenstein-openal-voices.md)). Retail's loop Doppler and loop merging are not carried. No listening test has been performed. ET's dynamic light reaches all three renderers with its own radius, brightness and force flag; its global directional and grid-only lights are skipped and say so, and still take one of the 32 lights a frame its modules get, as in ET. Those 32 are the modules' alone: the engine's own scene lights take only slots the modules can no longer claim, and a forced light stays drawn at `r_dynamiclight 0` as it does in ET, shown in live `oasis` clients ([engine scene lights record](2026-09-24-wolfet-engine-lights.md)). ET's binary message channel is wired in both directions, though no payload has crossed. ET's spline camera paths are carried, though a legitimate installation ships none to follow. ET's batched scene geometry, runtime shader definitions and image lookup are carried. The renderer now carries both of ET's skeletal model types, its own surface and its own draw call in all three renderers, including the entity's torso rotation. See the [drawing record](2026-09-18-wolfet-drawing.md). ET's world-covering fog volume now loads instead of refusing the map -- every fog volume retail ET ships is one -- and a module can retune it with a fade; see the [global fog record](2026-09-18-wolfet-global-fog.md). Fog density is not claimed to match retail. ET's render-to-texture import is refused rather than approximated, for a recorded reason; retail ET calls it only from a developer example its shipped build does not define. ET's water ripple and tapered rail core reach the renderer as ET's source draws them, instead of stopping the session with `Bad reType` (Source/Unit; [ripple record](2026-09-23-rtcw-water-ripples.md)). No absent online service is reported as present. The textured console style lays `et.exe`'s `console2-16bit` logo over its `console-16bit` background once the console is half open, in the place `et.exe` draws it, in engine screenshots beside retail's own at 640x480 and at 1280x720; retail's grey separator line and 8x16 text cell are not drawn yet ([console logo record](2026-09-23-wolfenstein-console-logo.md)). |
| Demos / bidirectional interoperability | Bidirectional admission and demo Interop subsets; malformed-input Live subset; full gates open | Retail spectator/team admission connects to fnTech3, including pure admission on all six stock maps. The reverse direction passes pure admission to original ET servers and joins both teams on those maps ([reported-fault follow-up](2026-10-01-observed-compatibility-fixes.md)). Recording keeps the three read-only cvars ET's cgame draws its recording notice from; see the [recording cvars record](2026-09-23-wolfet-demo-recording-cvars.md). Protocol-84 demos replay through ET's own wire codec ([playback record](2026-09-25-wolfet-demo-playback.md)). Current-build self recordings, retail recordings against a pure fnTech3 server, normal/timedemo playback and retail playback of fnTech3 recordings pass on `goldrush`, including Axis/Allied player states. Damaged inputs complete or are rejected with the main loop still alive ([demo follow-up](2026-10-01-wolfet-demo-regressions.md)). Scripted loopback sessions with retail `etded.exe` and `et.exe` 2.60b pass in both directions and on an fnTech3 listen server -- join, chat both ways, a map change, `map_restart`, reconnect, server quit, a kick, a UDP pak download inside a mod and a retail and an fnTech3 client on one server; a server whose cfg sets `sv_maxclients` takes it before allocating clients, as `etded.exe` 2.60b does (0x42d398), so its first warm-up restarts in place where fnTech3's had reloaded the map ([legacy-server record](2026-10-05-legacy-server-hosting.md)). A retail and an fnTech3 client share a server for ten minutes at 10% loss in both directions, chat heard every round both ways through a map change, and the short sessions pass at that loss too ([sustained sessions record](2026-10-08-legacy-sustained-sessions.md)). Combat/objective/match demos, seeking, `.dm_83`, mods and broader malformed coverage remain open. |
| Optional services | Unavailable | No PunkBuster, authentication or Workshop success is synthesized. |

Every build includes the ET client and server. Use isolated writable homes for
probes. Availability closes no compatibility gate; Q3/QL/RTCW evidence is not
ET evidence.

The 2026-09-24 path tracer record (`2026-09-24-rtx-path-tracer.md`) traces
all six retail maps with the RTX path tracer's compute backend, into ET's
main view, which ET insets by one pixel, at 0.93-0.99 of their raster
frames' brightness, with no errors and the installation unchanged. q3map2
left none of their light entities in the BSPs, so the tracer lights their
worlds from the lightmaps and their models from the light grid by default,
and traced pixels take ET's global fog. Five of the six ran in a
large-address-aware copy of the x86 developer client: after the probe's
renderer restarts the shared 32-bit client could run out of address space
reloading ET's 57 MB client game module. The engine now keeps an unloaded
native module's address range reserved until its slot loads again, and all
six pass in the shared client, the module reloading at its preferred base.
Since the record's fifth round the tracer lights ET's worlds from the sky
and the surfaces that give off light by default, not from the lightmaps. It
is renderer evidence only; no ET renderer gate closes on it.

## Elite Force gate matrix

The [October 2 input/launch follow-up](2026-10-02-input-launch-fixes.md)
pins SP's Use, alternate-attack and any-key bits to its installed executable,
keeps Holomatch's distinct mapping, and gives all client launches explicit
mouse and free-look settings. Passive retail-module probes reach active,
unpaused gameplay with no input catcher; they do not prove live movement or
mouse-look. The affected input/gameplay gates remain partial.

The [reported-fault follow-up](2026-10-01-observed-compatibility-fixes.md) loads
`borg1` and `hm_borg1` with Vulkan after enforcing a 128 MiB client hunk minimum.
Existing low retail settings are raised, larger settings retained, and dedicated
minimums unchanged. Broader renderer and campaign gates remain open.

Star Trek Voyager: Elite Force is the fifth title, with two mode profiles:
`stvef-sp` hosts Raven's single-player `efgamex86.dll`/`efuix86.dll`, and
`stvef-mp` runs the Holomatch QVMs. The [foundation record](2026-09-18-stvef-contracts.md)
pins the installation identity and startup contracts.

The [SP clock correction](2026-09-21-stvef-sp-clock.md) repairs missing
camera reference tags and adds 24 production scene reruns, three longer
bridge sequences, save-clock checks and staged manual preflights. The
[extended runtime record](2026-09-21-stvef-runtime-coverage.md) adds full
Holomatch playback on all three renderers and the observed x86 RT limit.

The [SP cadence correction](2026-09-21-stvef-sp-cadence.md) keeps simulation
ticks independent of `timescale` and sends local snapshots after new ticks,
removing the growing fixed-step snapshot lag. Native fixtures, seven-profile
compiled controls, production scene reruns and another retail save round trip
cover the change. Those captures used different frame phases and are not
synchronized retail renderer parity evidence; the later startup/capture
follow-up below aligns displayed times. Full-resolution
comparisons retract an initial contact-sheet impression of a Vulkan/RTX
corridor defect: those corridor images agree with GLx within one channel value.

The [rectangular mipmap correction](2026-09-21-stvef-image-mips.md) fixes
incomplete Vulkan/RTX texture chains and one-dimensional weighted filtering
in all renderers. Exact compiled upload checks cover all seven profiles on
Windows x86 and Linux i686; a visible, independently created texture fixture
reduces the measured Vulkan/RTX difference from GLx from 11.824 to 0.040
channel values. This shared renderer defect is separate from the retracted
corridor assessment and does not close broader retail image parity.

The [SP startup/capture follow-up](2026-09-21-stvef-sp-startup.md) corrects
the four map-settling calls to retail's current-clock ordering, with native
observations, seven-profile compiled controls, all 66 maps in the current
SP inventory and another retail save round trip. An adjusted capture script
aligns displayed snapshot times at three rates. Nine SP image comparisons
then use verified matching filters; their mean RGB differences are 1.15–2.25
out of 255. These bounded observations do not close campaign or image parity.

The [map-reload correction](2026-09-21-stvef-map-reloads.md) removes SP's
accidental multiplayer restart command and restores retail's full-map
continuation. It also fixes a shared Windows stack overflow during Holomatch
client-array resizing. Native observations, seven-profile command checks,
bounded-stack storage tests and Windows/Linux bot-retaining reloads cover
these changes. Campaign and manual acceptance remain open.

The [player-snapshot correction](2026-09-21-stvef-player-snapshots.md) includes
SP's visible local player in packet entities, restoring its third-person
model and shared animation updates used by scripted weapon changes. Native
observations, 168 compiled cases across seven profiles, the full Windows
suite and targeted runtime checks on all three renderers cover the change.
All 66 supplied SP/tour maps complete startup in retail and each baseline
renderer; the corrected GLx client repeats the complete inventory.
The [command-ordinal correction](2026-09-21-stvef-command-ordinals.md) then
resolves the observed startup event/weapon timing difference in 69 queried
snapshots. It replaces timestamp filtering and acknowledgement inference
with retail's command ordinal and bounded recovery contract. Native
fixtures, compiled Windows/Linux boundaries, save/hub and cross-engine
reloads, another complete GLx map sweep, three-renderer scene checks and
Holomatch restart controls cover the change. Campaign traversal and manual
acceptance remain open.

The [texture-compression follow-up](2026-09-21-stvef-texture-compression.md)
connects both retail menus' `r_ext_compress_textures` setting to the renderer,
with native defaults and flags, seven-profile compiled checks and live video
restarts on Windows/Linux. It also finds that the preceding SP captures had
different compression settings. With compression disabled in both engines,
the same tutorial samples have mean RGB differences of 0.074–0.145 out of 255.
GLx also honors the enabled request; Vulkan/RTX retain their uncompressed
capability. The [alpha-format correction](2026-09-21-stvef-alpha-compression.md)
then selects native DXT5 for GLx alpha images, with 72 native observations,
16,128 compiled selections per platform and Windows/Linux restart evidence.
Disabled-compression and Vulkan/RTX control captures remain pixel-identical.
The [method-selection correction](2026-09-21-stvef-compression-method.md)
also honors the native preference and fractional enable values, with 448
native observations, seven-profile Windows/Linux checks and seven complete
restart cycles. The [lightmap-control correction](2026-09-21-stvef-lightmap-controls.md)
honors native compression and precision, including merged/separate uploads
and Vulkan precision fallback, with 648 native observations and Windows/Linux
checks on all three backends. Eleven completed sessions produce 88 verified
engine captures. The full Windows suite passes 120 targets (two skips),
including 1,859 Python checks. Localized image differences, campaign play
and broader input/audio/device acceptance remain open.

The [renderer selection audit](2026-09-21-stvef-renderer-verification.md)
corrects the September 20 live probes: `r_renderer` did not select a
backend, so those runs were GLx only. Source and compiled fixtures remain
valid. New runs select `cl_renderer`, verify the loaded backend in the log,
and retain measured image differences instead of claiming image parity.

| Gate | Evidence | Remaining requirement |
| --- | --- | --- |
| Profile / identity / discovery | Source, Unit, Live subset | Both profiles, their aliases and the shared-pak mode choice pass unit checks; the Holomatch profile mounts the retail paks in a developer dedicated server. Windows x86 SP remains the native-module boundary and unsupported platforms now refuse it before filesystem startup. Linux i686 Holomatch server startup, explicit mode choice and isolation from Q3 assets are observed ([platform record](2026-09-21-stvef-linux-server.md)). The Linux i686 client also starts and completes a Holomatch demo with all three renderer modules on a private Mesa software display ([renderer audit](2026-09-21-stvef-renderer-verification.md)); six Linux client sessions also pass against Windows retail, Lilium and fnTech3 servers ([client record](2026-09-21-stvef-linux-client.md)); with only the retail paks and libraries, the single-player campaign plays through and all 99 maps load on Vulkan ([campaign record](2026-10-09-stvef-sp-campaign.md)); broader installations and physical input/device coverage remain open. |
| Audio codecs | Source, Unit, Fixture, Live subset | Every one of the 8,129 retail MP3 files decodes, and every one of the 8,074 Raven `#UNCOMP` sizes is reproduced by the retail frame rule. Each frame is decoded from its own header without looking past it, as all five Raven engines decode one, so a last frame that stray bytes follow is not dropped; no Elite Force file has one ([last-frame record](2026-10-08-mp3-last-frame.md)). Single player's ambient sound sets are read and played ([record](2026-09-20-stvef-sp-ambient.md)), and every single-player sound carries retail's own distances, with a voice heard to 1,024 units, an attenuated voice silent by about 290, the sounds the player themselves make held back, and the `s_separation` spread both executables register ([record](2026-09-20-stvef-sp-sound-distances.md)). **Single player's voices are rendered by the engine's own audio pipeline**, not by retail's gain and pan: each channel's full-volume and silent distances go to the OpenAL distance model with its fade and air absorption, and every voice gets traced occlusion, zone or traced room reverb, tone classes by channel (the attenuated voice as a voice) and HRTF. The ambient loops are placed at their own points, the room tone takes the room at the listener, and a separation above the default spreads placed voices through the source's radius. `s_alDistanceModel linear_clamped` plays retail's straight-line law exactly, and the legacy mixer keeps retail's gain and pan ([features](2026-09-20-stvef-sp-sound-distances.md#the-engines-own-features)); no listening test has been made. Single player opens a speaker's mouth as retail does, measuring each voice against Raven's own `#MAXVOL` ([record](2026-09-20-stvef-sp-lipsync.md)). Bink movies play through the installed Windows x86 runtime, with track selection and live lifecycle evidence. Native-file PCM comparisons, both language tracks through the real device, and honest partial sound-initialization failure reporting are now observed ([audio record](2026-09-20-stvef-movie-audio.md), [verified backend reruns](2026-09-21-stvef-renderer-verification.md)); Linux i686 Holomatch also renders a measured tone, half-volume response, both language MP3 paths and sound restart through a private OpenAL Wave Writer ([Linux audio record](2026-09-21-stvef-linux-audio.md)). Broader listening/device parity remains unverified ([record](2026-09-20-stvef-bink.md)). |
| Holomatch wire codec | Source, Unit, Fixture | Protocol-24 bit packing, alignment, strings, entity/player/command deltas, frozen records and the packet scramble. All 32,675 retail demo records decode, and re-encoding reproduces retail's bits except where retail flagged decode-invisible changes ([record](2026-09-18-stvef-holomatch.md)). Protocol 26 now has its own Huffman message layout, checksum framing and unkeyed commands, with independent reference vectors, bounded decoding and live interoperability ([record](2026-09-20-stvef-ioef.md)). |
| Holomatch game module / server | Source, Unit, Live subset | The retail `qagame.qvm` runs with bots on a developer dedicated server: game imports, records, server flags, shield boxes, round-to-nearest float conversion, the 1.17 bot contract and retail's botlib rules for weapons and item choice, Expansion Pack modes, and server-side framing ([record](2026-09-18-stvef-holomatch-server.md)). fnTech3's, Lilium Voyager's and the retail client play on it, and pass its pure admission ([record](2026-09-18-stvef-holomatch-sessions.md)). `sv_maxclients` stays within the module's 64 client slots where retail allows 128. Bots ring 64 reliable commands in every slot, as `stvoyHM.exe` does ([bot window record](2026-09-24-bot-reliable-windows.md)). A client that leaves is announced, then made a zombie before the game hears of it, and sent `disconnect %s`, unquoted; `kick` says `kicked`; a quitting server sends `disconnect server shut down`; `devmap` removes the bots and `map` from a single-player game keeps them; and a server running a single-player game answers no query, dedicated or not, as `stvoyHM.exe` does. A retail client receives the same commands from fnTech3's server as from retail's, and a twelve-step bot sequence matches the retail server at every step ([server messages record](2026-10-08-stvef-server-messages.md)). A map load and an immediate `map_restart` run each settling frame, and the last one, at the current time before advancing it, as `stvoyHM.exe` does; disassembly and fixtures ([settle order record](2026-09-24-settle-frame-order.md)). The Linux i686 server also hosts all three Windows clients, with protocols 24/26 and pure admission ([record](2026-09-21-stvef-linux-server.md)). Its bot navigation files are checked as Quake III's are: on `hm_borg1` a node naming itself had hung the server and a child or plane out of range had faulted it, and all 33 installed files pass while every Holomatch map loads its navigation, routes and takes a bot as before ([navigation record](2026-09-28-aas-index-checks.md)). The Holomatch game never saves a route cache (it ignores `bot_saveroutingcache`); of two dumps made for `hm_borg1` from its navigation header, the checked reader reads the one without caches and refuses the one with a cache for area 0 while the level plays on, where the old reader faulted the server ([route cache record](2026-09-28-route-cache-checks.md)). Routing to an area entered by more than 128 links, 355 on `hm_cam`, no longer reads the stack past Quake III 1.32's 128 zeros. `stvoyHM.exe` never zeroed that row: it starts every area cache from the goal area's own first row of travel times, as ET and RTCW do ([goal-link record](2026-09-28-aas-goal-links.md)). This profile now starts there too, follows a link into an area whatever its `AREA_DISABLED` flag, and takes `stvoyHM.exe`'s contents flags, as its routing update does. Of 4,559,687 routes to sampled goals over all 33 maps, 4,520,492 change their travel time and 34,723 their first link, and every map still loads its navigation, routes and takes a bot ([route seed record](2026-09-29-stvef-route-seed.md)). The server keeps its 50 ms tick under `timescale`, lets a zero timescale run at normal speed and counts a level's load into its first frame, as `stvoyHM.exe` does; bots then play capture the flag at `timescale 3` as they do on the retail server ([frame-time record](2026-10-08-stvef-frame-time.md)). Its serverinfo carries `sv_pure`, archived and unlatched, and none of fnTech3's own keys, as `stvoyHM.exe`'s does ([serverinfo record](2026-10-08-stvef-serverinfo.md)). An Elimination game's bots unlink the world's own record, past the entities the game publishes; `stvoyHM.exe` takes any record numbered below 1024, and fnTech3's server had stopped with `bad gEnt`, so it now binds the game's whole entity array, as for RTCW and ET ([world record](2026-10-09-stvef-world-record.md)). Every game type -- free-for-all, tournament, team and capture the flag -- and each Expansion Pack mode plays with bots on both servers, with the same modes left on after the engine's conflict rules and no error; how a game ends varies with the bots' play on both ([modes record](2026-10-09-stvef-holomatch-modes.md)). |
| Holomatch client framing / demos | Source, Unit, Live subset | Retail handshake with nonce and protocol checks, raw-bit framing with the acknowledgement in snapshots, retail read-past-end semantics, `cp` without a server id, the missing-file download, the one-byte button field, and `.efdemo` naming, refusal and playback selection ([record](2026-09-18-stvef-holomatch-client.md)). All three retail demos play to the end in a developer client ([record](2026-09-18-stvef-holomatch-cgame-ui.md)). Live sessions with fnTech3's, Lilium Voyager's and the retail server, pure and not, and live demo recording that fnTech3 and Lilium play back ([record](2026-09-18-stvef-holomatch-sessions.md)). Protocol-26 negotiation, `.dm_26` recording and playback are also observed; both engines complete each recording with the same frame count ([record](2026-09-20-stvef-ioef.md)). |
| Holomatch cgame / UI | Source, Unit, Live subset | The retail cgame and UI run in a developer client: both import tables with retail's accepted and refused numbers, their records, the quiet cvar set, 3D shader registration, press-only UI key events, both server lists and the absent CD-key service ([record](2026-09-18-stvef-holomatch-cgame-ui.md)). The UI draws the main menu; the cgame plays the retail demos and live sessions ([record](2026-09-18-stvef-holomatch-sessions.md)). The UI's key events and key imports use Elite Force's own key numbers ([record](2026-09-19-stvef-languages.md)). The UI's sounds, seven call sites of them on channel 8 with the holodeck intro's computer voice among them, play as Fn interface audio on any channel ([menu sound record](2026-10-08-ui-sound-policy.md)). |
| Languages | Source, Unit, Fixture, Live subset | German voices chosen by `s_language`, loaded from `sound/vox_d` at runtime; translated key names chosen by `g_language`, all 7,904 compared equal with both retail executables' tables; `s_language`, `g_language` and `k_language` registered as retail registers them ([record](2026-09-19-stvef-languages.md)). Single-player movies choose the second track for German, falling back to the first when absent; the selected PCM matches native file playback, and both languages reach the real device in verified GLx, Vulkan and RTX runs ([original audio record](2026-09-20-stvef-movie-audio.md), [corrected backend checks](2026-09-21-stvef-renderer-verification.md)). Subjective listening and other devices remain unverified. |
| SP module host / savegames | Source, Unit, Live subset | The game half of the retail `efgamex86.dll` runs on fnTech3's server through its function table: the 42 imports in retail's order, the cgame half attached before `Init`, retail's records through the server's gentity view, and every one of the 63 single-player maps initialised and run; the loopback wire carries every single-player record field exactly, compactly enough for the largest levels' gamestates ([record](2026-09-19-stvef-sp-host.md)). Savegames are retail's chunks with the engine's own around the game's: levels save and load, walk and fall back between each other, and keep their autosaves and pictures; the retail engine loads a save fnTech3 wrote and fnTech3 loads one retail wrote ([record](2026-09-19-stvef-sp-savegames.md)). New levels now start at retail's 1,000 ms; a zero start had prevented delayed reference tags from registering and broke scripted cameras. Production reruns and fresh save/hub-clock checks cover the correction ([record](2026-09-21-stvef-sp-clock.md)). Checked writes and final close now preserve previous saves on failure; real file obstruction/recovery probes verify honest completion messages and hub-transition refusal, with a fresh retail save round trip ([save-integrity record](2026-09-21-stvef-save-integrity.md)). A client that leaves is a zombie before the game hears of it, and is then announced as `print "%s %s\n"` and sent a bare `disconnect`, as `stvoy.exe` does; a quitting server sends a bare `disconnect` too ([server messages record](2026-10-08-stvef-server-messages.md)). A map load runs each settling frame, and the last one, at the current time before advancing it, as `stvoy.exe` does ([settle order record](2026-09-24-settle-frame-order.md)). Retail has no `map_restart`, and fnTech3 registers none for single player either: its `nextmap` reloads the level in full, as retail's does ([map reload record](2026-09-21-stvef-map-reloads.md)). A save's comment chunk is cleared and carries retail's second line at byte 64, the time, date and level the menus list beside a save, where fnTech3 had left stack bytes ([input and menus record](2026-10-08-stvef-input-menus.md)). A level begins with a full 200 ms frame, a skipped cutscene runs at retail's 500 ms a frame, and a zero timescale runs at normal speed, as in `stvoy.exe` ([frame-time record](2026-10-08-stvef-frame-time.md)). The whole campaign plays through on the retail modules, 45 levels by their own exits to the closing movie and credits with every autosave, as do the holodeck programmes, the brig and the Virtual Voyager tour's hub saves, with or without the installation's extra pak ([campaign record](2026-10-09-stvef-sp-campaign.md)). |
| SP menus / cgame | Source, Unit, Live subset | The retail `efuix86.dll` runs in a developer client through its function tables: the 59 imports in retail's order with the scissor slot left empty, menus opened by name with their holodeck variants, retail's check whether a menu may open, press-only keys, the connection screen's text and retail's order of drawing and refreshing; the main menu draws as retail's ([record](2026-09-19-stvef-sp-menus.md)). The cgame half of `efgamex86.dll` runs in the client through the game library the server loads: all 71 imports, single player's records, the calls the game half makes while a level spawns, the cgame shut down before the library unloads, the command number it predicts from, and snapshots seen from a camera scene or the leaning eye; the retail levels play, with their scenes, subtitles, HUD and view weapon ([record](2026-09-19-stvef-sp-cgame.md)). The savegame commands and the picture a save keeps are in the [savegame record](2026-09-19-stvef-sp-savegames.md), and the ambient sound sets, which give a room its tone and a mover its sounds, in the [ambient record](2026-09-20-stvef-sp-ambient.md). How far each of single player's sounds carries is in the [sound distance record](2026-09-20-stvef-sp-sound-distances.md). Lip-sync is measured from the playing voices ([record](2026-09-20-stvef-sp-lipsync.md)). The visible local player now reaches the packet-entity list, restoring third-person rendering and shared weapon animation; the subsequently observed startup event/weapon timing difference is resolved in 69 queried snapshots by explicit command ordinals and native recovery behavior ([player-snapshot record](2026-09-21-stvef-player-snapshots.md), [command-ordinal record](2026-09-21-stvef-command-ordinals.md)). The images the menus and cgame hand the engine now carry the brightness retail applies at its `glColor3f` boundary, and each image submitted in a client frame keeps its own pixels through the queued 2D draw ([raw-image record](2026-09-21-stvef-raw-image.md)). Both profiles carry HUD aspect presets ([widescreen record](2026-09-25-widescreen-hud-and-fov.md)), checked against the draws their retail modules issue, and every official map has a generated audio zone sidecar ([presentation-asset record](2026-09-22-stvef-presentation-assets.md)). |
| Renderer / cinematics | Source, Unit, Live subset | Holomatch's entity record and eight effect primitives, lighting flags, entity alpha forcing, environment mapping, the `random` wave and `depthFunc disable`, overbright default and console font are implemented in all three renderers; demo frames were compared with Lilium and retail ([record](2026-09-18-stvef-holomatch-cgame-ui.md)). Both modes' `noise` waves, and SP entity records, held frames, rotated pictures and lighting queries have evidence ([record](2026-09-19-stvef-sp-cgame.md)). All 201 installed RDM5 models pass the bounded reader; 2,081,643 bones decode bit-identically to retail, and skinning, LOD and lighting have native comparisons and executable regressions in all three implementations ([MDR record](2026-09-20-stvef-mdr.md)). Native vectors and compiled fixtures also cover scissor command ordering, movie fade/crop calculations, all eight effect primitives and full-length legacy shadow extrusion ([scissor](2026-09-20-stvef-scissor.md), [presentation](2026-09-20-stvef-cinematic-presentation.md), [effects](2026-09-20-stvef-effects.md), [shadows](2026-09-20-stvef-shadows.md)). The September 20 live runs used GLx regardless of their requested labels. Twenty-one correctly selected Windows reruns now verify scissor pixels on all three backends and Vulkan/RTX shadow scenes, effects, movie fades, stable held frames, level pause/resume, both audio tracks and continued video after sound failure. Three Linux i686 software-rendered demo runs complete with 107 frames each ([renderer audit](2026-09-21-stvef-renderer-verification.md)). The RTX default linear-light HDR path differs from legacy images; two SDR control runs now reproduce Vulkan held movies exactly and its shadow scenes within one channel value. Eight broader SP scenes now complete on each verified backend after the camera-tag fix, and all three effective retail Holomatch demos complete on each backend with retail's frame counts. Those earlier scene captures were not synchronized retail pixel comparisons. Two new eight-scene sweeps now supply 80 synchronized single-player runs: retail twice and each Fn renderer per scene, with Fn settings and a matched-settings control. All runs exit normally at matching view positions; GLx's matched RGB mean error ranges from 0.030 to 1.381, while Vulkan/RTX raster differences remain. RTX is an explicit SDR raster control, not native RT or its default HDR ([retail views record](2026-10-09-stvef-sp-retail-views.md)). A 66-map startup sweep adds retail and all three baseline renderers, followed by all 66 maps on corrected GLx and targeted first/third-person checks on all three corrected renderers; it covers initial scenes, with an HD overlay, and does not establish campaign traversal or universal pixel parity ([player-snapshot record](2026-09-21-stvef-player-snapshots.md)). The tested x86 driver lacks required RT extensions: raster fallback and strict refusal are observed, while native RT remains unavailable ([clock/scene record](2026-09-21-stvef-sp-clock.md), [full demos and RT boundary](2026-09-21-stvef-runtime-coverage.md)). All eight campaign movies play to their end in the campaign's order, and every campaign level gives its engine screenshot ([campaign record](2026-10-09-stvef-sp-campaign.md)). Broader image parity and listening/device coverage remain open. |
| Interoperability | Live, Interop | fnTech3 plays with Lilium Voyager 1.40 and the retail `stvoyHM.exe` 1.20 as client and as server, with and without pure admission; demos recorded live play in fnTech3 and Lilium; and a missing pak downloads byte-identical between every pairing, pure or not ([record](2026-09-18-stvef-holomatch-sessions.md)). Protocol 26 passes both fnTech3/Lilium directions with pure enabled and disabled, matching downloads, cross-engine demos and valid packet checksums; protocol 24 remains independently exercised ([record](2026-09-20-stvef-ioef.md)). Ten Linux-hosted sessions and twelve cross-engine demo playbacks add dedicated-server platform evidence, including the retail client ([server record](2026-09-21-stvef-linux-server.md)). Six Linux client sessions against Windows retail/Lilium/fnTech3 servers and twelve further cross-engine demo playbacks establish the reverse platform direction ([client record](2026-09-21-stvef-linux-client.md)). Scripted loopback sessions with retail `stvoyHM.exe` 1.20 pass in both directions, on an fnTech3 listen server and retail on retail -- join, chat both ways (`svsay`, which fnTech3's server now takes as retail's does), a map change, `map_restart`, reconnect, server quit, a kick, a UDP pak download inside a mod and a retail and an fnTech3 client on one server. Before the pure-proof change the retail server dropped fnTech3's client as unpure on the reconnect, for a non-retail texture pak only the client had. fnTech3's Holomatch browser now reads Holomatch's one-line info replies and asks Raven's master port, 27953, where HEAD's dropped every reply and asked 27950, and a public server ends with retail's `\heartstop\` ([legacy-server record](2026-10-05-legacy-server-hosting.md)). A retail and an fnTech3 client share a server for ten minutes at 10% loss in both directions, chat heard every round, and the short sessions pass at that loss ([sustained sessions record](2026-10-08-legacy-sustained-sessions.md)). |
| Input / physical devices | Source, Unit, Live (engine synthetic input); physical devices not run | Elite Force key numbers, translated names and press-only UI events have the recorded language/input checks. Through the engine's own synthetic input, which the user approved for these probe windows, both modes move, look and fire with their default bindings, run a console binding, and rebind a key through their controls menus, which their configs keep; single player also saves, lists, loads and respawns through its own menus ([input and menus record](2026-10-08-stvef-input-menus.md)). The user chose not to run the checks with a physical keyboard, mouse or joystick, or by ear, so the platform layer's capture and listening are not covered. SP force-feedback imports now use the installation's FFC10 runtime through an opt-in Windows x86 bridge: all 32 effects, four-slot playback rules and spring controls have native parameter and compiled boundary checks. Six production starts verify that disabled feedback does not load the DLL. Feedback device initialization and output are untested: no force-feedback device is available ([feedback record](2026-09-21-stvef-force-feedback.md), [manual acceptance](2026-09-21-stvef-sp-clock.md)). |
| Optional services | Unavailable | No authorize/master success is synthesized. |

The [shader-wave follow-up](2026-09-21-stvef-shader-waves.md) corrects the
random wave's view clock and both profiles' deterministic noise tables, with
native SP/MP vectors, compiled checks on both platforms and production runs
on all three renderers. Two held Holomatch views gain bounded retail image
comparisons. Its later shield and explosion captures varied within retail
itself; the following investigation separates this capture-timing limit
from renderer behavior.

The [effect-capture follow-up](2026-09-21-stvef-effect-captures.md) resolves
the large portable-shield comparison mismatch: frozen cgame frames kept
adding shield quads, retail and fnTech3 consumed different wait counts, and
their screenshot commands captured at different points in the frame.
Matching those points gives 54 full-frame comparisons across nine states
in all three retail demos on every renderer, with RGB mean absolute
differences of 0.39–2.20 out of 255. Both successive captures are retained.
This establishes bounded image evidence, not pixel identity or completion
of the broader renderer gate.

The [electricity investigation](2026-09-21-stvef-electricity.md) separates
randomized ribbons from the largest remaining burst-image difference.
Instrumented retail and fnTech3 copies with the same jitter input reduce
that scene's difference to 0.65–0.67 on all three renderers; sixteen control
images remain pixel-identical to their uninstrumented runs. Native counts
confirm 45 random values per ribbon and are now checked in all renderers on
Windows x86 and Linux i686. This is separate diagnostic evidence, without
changing production randomness or closing the broader renderer gate.

The [runtime follow-up](2026-09-21-stvef-runtime-followup.md) adds a complete
fnTech3-to-retail-to-fnTech3 SP save round trip after a longer retail startup
wait, plus a 107-frame Linux Holomatch run with the ray-tracing pipeline
active on software Vulkan. These are bounded fixture/platform observations;
manual menu saves, campaign-wide acceptance and Windows x86 RT remain open.

Both Elite Force profiles are available in every build under the
[availability policy](2026-10-11-profile-availability.md); the legacy
`-Dstvef-development` option is accepted as a no-op and closes no release gate.

## Jedi Academy gate matrix

The [2026-10-14 compatibility round](2026-10-14-compatibility-round.md#jedi-academy-saber-glow-and-lines)
serves Raven entity types 5, 8 and 9 -- the saber glow, line and oriented line
the multiplayer client game submits -- as retail `jamp.exe`'s
`RB_SurfaceEntity` (`0x4bdcd0`) draws them. GLx, Vulkan and RTX each match
the 916 vertices retail's own routines drew from 22 synthetic cases, run
in-process (Source, Unit, retail comparison). The native-server demo that
stopped at type 5 after 408 snapshots now plays to its end in all three
renderers, 439 snapshots with deltas (Live subset). That demo draws no 3D view
once its opening menu closes (no world surfaces in `r_speeds`): its snapshots
then reach the client flagged `SNAPFLAG_NOT_ACTIVE`, a separate open gap, so no
capture shows a blade. Types 3, 6, 10 and 11 stay refused. No gate closes.

The [October 7 audit](2026-10-07-jedi-compatibility-audit.md) corrects the
shared game/cgame/UI skeletal clock and no-rotation selector, angle timing,
positional sound, looping policy and voice-animation metadata in both audio
backends. A windowed local client reaches its player menu and survives a
map restart with either backend enabled. That audit's join stopped at a
`Line` primitive in `effects/mp/spawn.efx`. SP cvar refresh now preserves
restart latches and module acknowledgements. Unit, isolated retail-routine
and live evidence advances these paths without closing any gate. The record
supersedes the earlier blanket local-clock, positional-sound and enabled-audio
voice-query refusals below.

The [second round](2026-10-07-jedi-compatibility-round2.md) adds bounded Line
effects, the retail cgame trace callback and measured wave timing through
Fn polygons. The unchanged installed spawn now refuses its Emitter; its
Light/FxRunner/Electricity dependencies remain open. A synthetic Line-only
diagnostic reaches play before a separate bone-animation/bolt refusal, not
complete installed-effect support. SP flag mirrors now reflect current
engine flags while isolating Academy's save bit. Renderer text and shared
UI viewport validation are also hardened. No gate closes.

The [spawn-effect slice](2026-10-15-jka-spawn-effect.md) (2026-10-15)
serves the installed spawn effect's Emitter and its Light, FxRunner and
Electricity dependencies, measured in retail `jamp.exe` 1.01 (CreateEffect
`0x407c40`, the emitter's motion and emission walk `0x403c30`/`0x4038f0`,
the count rule `0x4072b1`). Lights enter Fn's dynamic lights and bolts are
Fn-authored polygon strips. A full effects pool now evicts, as retail's
never refuses. A local `mp/ffa1` join reaches play in GLx, Vulkan and RTX;
with `timescale 0.1` each runs to its scripted end with the emitter
climbing, shedding lights and throwing traced bolts, and the teal light is
visible in the captures (Source, Unit, Live subset). With
`cg_renderToTextureFX` at its retail default the invulnerability sphere's
distortion copy (renderfx `0x2000`) still refuses, and at normal speed play
stops about 12.5 s in on Kyle's Ghoul2 bolt pose. No capture isolates a
Line's or bolt's appearance. No gate closes.

The [play-blockers slice](2026-10-15-jka-play-blockers.md) (2026-10-15)
resolves both. A clip past its end with neither loop nor freeze is cleared
as retail's timing clears it (`jamp.exe` `0x4975a6`): its bone keeps the
frames it inherits, and queries answer it as inactive once a build has
cleared it, refusing before then, where retail would read uninitialised
memory. Distortion copies (renderfx `0x2000`), which retail's renderer
uses only to refract the background (`0x48af26`), are validated and not
drawn; Fn has no entity refraction pass. With retail defaults, a local
`mp/ffa1` session now plays its scripted 35 seconds to the end in GLx,
Vulkan and RTX (Source, Unit, Live subset). No gate closes.

The [particle-dynamics slice](2026-10-15-jka-particle-dynamics.md)
(2026-10-15) serves what `mp/ffa2`'s torch effect needs, measured in retail
`jamp.exe` 1.01. Each spawn picks one shader from its primitive's list
(`0x405d50`). Particles move under velocity, acceleration and gravity, and
rotate with a decaying step (`0x401730`). Their size, colour and alpha run
retail's linear, nonlinear, wave and clamp curves (`0x401e50`, `0x402040`,
`0x402390`), with `useAlpha`. Delays, `evenDistribution` and late spawns
follow the scheduler (`0x4073c6`, `0x407810`), and
`rgbComponentInterpolation` follows `0x406de0`. `cullrange` is accepted and
inert, as in retail. Academy's `glow` stage keyword (`0x4b3fd4`, `jasp.exe`
`0x47a188`) is now read by all three renderers, so glow shaders no longer
default; Fn's bloom stands in for retail's glow buffer. Of the 376 installed
effect files, 68 now compile completely, up from 15, and none regresses. A
local `mp/ffa2` join now plays to its scripted end in GLx, Vulkan and RTX,
with the torches' flames drawn (Source, Unit, Live subset). Random curves,
particle physics and the families and fields the record lists remain named
refusals. No gate closes.

The [tails-and-physics slice](2026-10-15-jka-tails-and-physics.md)
(2026-10-15) serves the next refusals of the per-file sweep, measured in
retail `jamp.exe` 1.01. Tails move like particles and draw from their origin
back along their motion by a length curve (`0x402e30`, `0x4031c0`,
`0x4033b0`); Fn draws them as polygons. Traced particles bounce, play impact
effects and die on impact (`0x401a50`), and death effects play as particles
end (`0x401500`). A particle without `expensivePhysics` collides only while
the `RMG` cvar is set, as in retail (`0x401afc`). Spawn points on spheres
and cylinders, cheap origins and absolute motion follow CreateEffect
(`0x407cea..0x408e29`). Lines, oriented particles, tails and bolts run the
particle curve law, with the `random` word. Unknown keys, and groups that
are not primitives such as `forcefeedback`, are read past as retail reads
them. Of the 376 installed effect files, 190 now compile completely, up from
68, and none regresses. A local `mp/ffa2` join with five added effect
runners plays to its scripted end in GLx, Vulkan and RTX, with sparks,
spray, dust and traced bolts drawn (Source, Unit, Live subset). Six
explosion shaders still default on Academy's `oneShotAnimMap` stage
keyword, and weapon fire stays unchecked behind the Sound and Decal
families. No gate closes.

The [effect-sound slice](2026-10-15-jka-effect-sound.md) (2026-10-15)
serves the Sound primitive, measured in retail `jamp.exe` 1.01. CreateEffect's
Sound case (`0x40b0dc`) picks one handle from the primitive's list
(`0x405d50`) and hands S_StartSound (`0x46ac60`) the spawn origin,
ENTITYNUM_NONE and CHAN_AUTO, with no volume or radius. Fn starts the sound
through the client game's own spatial-sound entry, so Fn's distance model,
occlusion, EFX and HRTF apply. Flag words retail does not know, such as
`lessAttenuation`, are read past as retail's parsers read them. Of the 376
installed effect files, 247 now compile completely, up from 190, and none
regresses; every Sound primitive compiles. Live on `mp/ffa2`, effect runners
added through an entity override start the concussion rifle's and thermal
detonator's muzzle-flash sounds, a saber clash, a punch impact and sparks as
OpenAL voices at their own places, with Fn's tone classes and reverb send, in
GLx and Vulkan joins (Source, Unit, Live subset). Scripted weapon fire is
still unchecked: after a join the menu module keeps the key catcher, its key
event (UI export 3) is unserved, and `+altattack` is not bound for the
profile. Most weapons' muzzle flashes now stop on CameraShake and wall
impacts on Decal. No gate closes.

The [weapon-fire slice](2026-10-15-jka-weapon-fire.md) (2026-10-15) makes
weapon fire reachable for a local `jka-mp` player, measured in retail
`jamp.exe` 1.01. Retail's engine never clears the menu module's key catcher
on a join (every write to `0x8af104` was read). The client game opens the
Player Configuration menu on the server's `nfr` command, and the module
releases the catcher itself when Escape closes its menus. The module's key
event (UI export 3) is now served as retail's CL_KeyEvent (`0x41c290`) and
CL_CharEvent (`0x41c640`) call it: `(key, down)` with the engine's time
left behind, in the numbers of retail's key-name table (`0x570bc8`).
`+altattack` is button 7 for `jka-mp` and `jk2-mp`, as both executables
register it (`0x41aae7`, `0x4080eb`). Both Jedi profiles now read fifteen
buttons and send Raven's any-key bit `0x100` (`0x419e85`, `0x4076b0`), not
Quake III's 2048, which is their force drain. Ghoul2 model copies past an
instance's end grow it with empty places, as `0x4e04b0` does, and the client
game's sound mute (import 36) ends that entity's sound on that channel in both
of Fn's mixers. Live on `mp/ffa2` with no input, `togglemenu` closes the join
menu (catcher `0x0002` to `0x0000`), `give all` completes, and the concussion
rifle's primary and alternate shots play their muzzle-flash effects and
sounds (Source, Unit, Live subset). The listen server then stops on the
game's Ghoul2 trace (game import 28). The Bryar pistol stops on CameraShake
and its charged shot on the client game's sprite import (280). Retail's
client stops at `NEED CD` without the disc, so its side is static evidence
only. No gate closes.

The [camera-shake slice](2026-10-15-jka-camera-shake.md) (2026-10-15)
serves the CameraShake primitive, measured in retail `jamp.exe` 1.01 and its
client game. CreateEffect's CameraShake case (`0x40b2ca`) draws the life,
radius and intensity, truncates the first two, and writes them with the spawn
origin to the client game's shared buffer for its export 31 (`0x40c560`).
Retail `cgamex86.dll` then does nothing beyond the radius, scales the
intensity by distance, holds it at 16 and shakes its own view origin and
angles, fading over the life (`0x30062c70`, `0x30062990`). Fn hands the same
record to export 31 through a new engine export that other profiles refuse;
the shaken view reaches Fn's renderer and listener through the module's own
imports, and fnTech3 has no engine view-shake option to apply. Of the 376
installed effect files, 290 now compile completely, up from 247, and none
regresses; all 73 CameraShake primitives compile, including the muzzle
flashes of the Bryar pistol, blaster, bowcaster, DEMP2, disruptor, flechette,
repeater and rocket launcher. Live on `mp/ffa2`, effect runners within the
shake radius jitter the client game's own view origin and the engine
screenshots in GLx and Vulkan, while the same runners beyond it, and a muzzle
flash without a shake, leave both still (Source, Unit, Live subset). Fired
in third person, the Bryar pistol's muzzle flash now plays in full, its sound
and shake record included, before the listen server stops on the game's
Ghoul2 trace (game import 28); in first person the view weapon's flash stops
on client-game import 268 first. No gate closes.

The [entity effect slice](2026-10-15-jka-entity-effect.md) (2026-10-15)
serves client-game import 268, `CG_FX_PLAY_ENTITY_EFFECT_ID`, measured in
retail `jamp.exe` 1.01 and its client game. Import 268 (`0x41346b`,
`0x401050`) calls PlayEffect (`0x407110`) with the caller's own axis, its bolt
word, no Ghoul2 instance and no fxParm; the entity number is never read, and
the volume and radius go to template words no served family reads. All four
retail call sites (`cgamex86.dll` `0x3005efa0` callers: the first-person
muzzle flash and three force effects) pass the unbolted word -1. Fn plays
the effect through the import 266 path at that axis, unnormalized, and
refuses any other bolt word by name, since retail would bolt the spawns to a
model through Ghoul2. Live on `mp/ffa2`, a first-person Bryar pistol shot now
plays `bryar/muzzle_flash` through import 268 with its sound, light and shake
record in GLx and Vulkan; engine screenshots show the flash at the view
weapon's muzzle, and the client game's view jitters after the shot. With
the Ghoul2 trace served, the bolt then flies until its wall impact stops
on the effects Decal primitive (Source, Unit, Live subset). No gate closes.

The [view-shake slice](2026-10-15-view-shake.md) (2026-10-15) adds
`cl_viewShake`, an engine-owned 0..1 scale (default 1, retail's shake) for
view shakes the engine itself hands a game module. The Academy effects host
multiplies the intensity it writes into export 31's record by it and still
makes the call at every scale, so the retail client game keeps its own
shake sequence and only the movement shrinks. A survey of all sixteen
profiles found no other engine crossing: Quake III, Quake Live, RTCW, ET,
Elite Force and the Jedi single-player modes make their shakes inside the
client game; Outcast multiplayer's effects boundary is still open; Alice,
F.A.K.K.2 and Medal of Honor carry theirs in the player state's
`damage_angles`, shared with the damage flinch, which the engine copies
unchanged. Source and Unit evidence, and a live `mp/ffa2` comparison (Live
subset). No gate closes.

The [Ghoul2 trace slice](2026-10-15-jka-g2-trace.md) (2026-10-15) serves
game import 28 (G_G2TRACE) as retail `jamp.exe` 1.01 serves it, together with
import 27, which is the same server trace (`0x462920`) with Ghoul2 flags 0.
Retail's entity loop (`0x462490`) keeps Raven's owner rules (owner-not-shared
`0x1000`, the two shot masks, missiles of the pass entity's owner) and skips
CONTENTS_NOSHOT for shots, and a start inside an entity names it. With flag 1,
the entity the box sweep named gets the Ghoul2 step: its models are posed at
the level time, traced in its yaw and origin by segment or by a radius of
half the box's width (at least 1 with flag 8), and a miss puts the trace back
as it was before that entity, while a hit moves the end and normal onto the
mesh and, with flag 4, puts the surface index in the surface flags. Dead
bodies are left to their box unless flag 2 asks. Collision detect walks
models and surfaces in retail's order, keeps sixteen records, stops when they
are full and sorts them with a port of the runtime's own qsort. Retail's
box brush carries Academy's body bit `0x100` (`0x428abd`), and fnTech3 gave
`jka-mp` Quake III's `0x2000000`, so until now no trace in this profile struck
a body's box and players' `s.solid` was never packed; `jka-mp` now uses
`0x100`, as `jk2-mp` does. One synthetic game module linking two Kyle bodies
on `mp/ffa1` asks both engines 736 traces: 735 agree within 0.0037 units
(558 bit for bit), all 181 Ghoul2 hits and 387 pass-throughs included; the
other is a box-only trace meeting the box exactly on its edge, where the
shared collision model picks the other face's plane. Live on `mp/ffa2`, the
concussion rifle's alternate beam plays its fifteen rings and its missile its
trail, and both stop at the impact on the effects Flash primitive; with an
NPC spawned, the client game asks for its own collision test (import 299)
first (Source, Unit, Live subset). No gate closes.

The [effect decal slice](2026-10-15-jka-decal.md) (2026-10-15) serves the
effects Decal primitive, measured in retail `jamp.exe` 1.01. CreateEffect's
Decal case (`0x40ac7e`) draws the size, alpha and rotation, picks a shader
and hands them, with the spawn origin, the axis's first row and the RGB
start, to the renderer's AddDecalToScene (`0x4ab6c0`), the entry client-game
import 205 also reaches. That keeps a pool of `r_markcount` (default 100)
records of each of two kinds (`0x4ab5b0`): a mark pushed out by newer ones,
with the others made in its scene, fades over a second (`0x4ab530`), and
every world scene draws the pool (`0x4abb20`). Fn keeps the same pool in its
client, cuts each square with its own mark clipper and draws the fragments
through its polygon path before each world scene; import 201 clears it. The
`ghoul2Decals` flag, which also asks client-game export 17 to mark a Ghoul2
model, refuses by name. 309 of the 376 installed effect files now compile,
up from 290, and 40 of the 41 Decal primitives. Live on `mp/ffa2`, a
first-person Bryar pistol shot into a wall plays its wall impact to the end,
and full-speed engine screenshots with and without marks differ only where
the scorch mark lies, in GLx and Vulkan (Source, Unit, Live subset). No gate
closes.

The [one-shot animation slice](2026-10-15-jka-oneshot-animmap.md)
(2026-10-15) makes all three renderers read Academy's `oneShotAnimMap` and
`clampAnimMap` stage keywords, for `jka-sp` and `jka-mp` only. Retail
`jamp.exe` 1.01 and `jasp.exe` read them in ParseStage's `animMap` branch
(`0x4b426c`, `0x47a42e`). That branch writes the stage's one-shot byte, loads
`clampAnimMap`'s frames clamped and keeps up to 32 frames. R_BindAnimatedImage
(`0x4ac3d0`, `0x472fd0`) holds a one-shot bundle on its last frame instead of
wrapping. The six `gfx/exp` explosion shaders no longer default. Live on
`mp/ffa2`, an effect runner's `chunks/sparkexplode` draws its explosion where
the default square drew, then goes dark on the black last frame while its
particle still lives, in GLx, Vulkan and RTX (Source, Unit, Live subset).
Retail's `RF_SETANIMINDEX` frame (renderfx `0x80000`, used by the `*_onoff`
door movers) stayed refused by the render adapter until the frame-index
slice below. No gate closes.

The [frame-index slice](2026-10-15-jka-setanimindex.md) (2026-10-15) serves
retail's `RF_SETANIMINDEX`. The client game sets renderfx `0x80000` on
`EF_SHADER_ANIM` movers, with the frame to show in `skinNum`.
R_BindAnimatedImage then takes that frame instead of the stage's timed one
(`jamp.exe` `0x4ac42f`; `jasp.exe` `0x473026` tests `0x20000`). The adapter
carries the flag as `refEntity_t.jka.setAnimIndex`, and all three renderers
read it for `jka-sp` and `jka-mp` only. A negative frame shows frame 0,
where retail reads before its frames. The test map is `mp/siege_hoth`, with
an entity override that leaves out the sky portal and the Ghoul2, NPC and
ion-cannon entities the map stopped on at the time. There the session
no longer stops at the first locked door. Locked doors show their red light
and unlocked ones their green, in GLx, Vulkan and RTX. A control renderer
that ignores the flag shows the locked doors green (Source, Unit, Live
subset). `jka-sp` has no client-module boundary to carry its `0x20000` yet.
No gate closes.

The [entity-refraction slice](2026-10-15-jka-entity-refraction.md)
(2026-10-15) draws the distortion copies the play-blockers slice left out.
Retail `jamp.exe` 1.01 posts them after the view (`0x48aba6`), copies a
radius-sized square of the screen for each (`0x48af26`) and draws the copy's
own stages over that copy. The client game's shield copy uses
`effects/refract_2` at entity colour `(0, 255, 0, 100)` (`cgamex86.dll`
`0x3004db31`), so retail filters the background green as well as
magnifying it. The earlier reading of `0x4b6e63` as an ARB program is
corrected: it names a marker shader with no stages. Fn carries `0x2000` as
an engine-owned entity field and draws the copy with its liquid refraction
underlay in GLx, Vulkan and RTX. The entity's alpha scales the bend, its
colour tints the bent scene, and a new `r_entityRefraction` (default 1)
scales the bend; only `jka-mp` allocates the snapshot for it. Live
before-and-after captures of the shield show the green filter and a mean
bend of 18 to 30 pixels at 768 lines in all three (Source, Unit, Live
subset). No gate closes.

The [2026-10-15 compatibility round](2026-10-15-compatibility-round.md#jedi-academy-sets-heard-around-a-point)
serves client-game import 51, the sound sets the client game names around
its entities: every entity in its frame that carries one and is not a
mover, among them effect runners and speakers on `mp/ffa2`, `mp/ctf3`,
`mp/duel6`, `mp/duel7` and both siege maps. Retail `jamp.exe` 1.01's
`AS_AddLocalSet` (`0x4684f0`) and `AS_PlayLocalSet` (`0x4680d0`) ran
in-process on 118 synthetic cases, and fnTech3 matches every loop, subwave,
answer and generator state they left (Source, Unit, retail comparison). A
set's loop is heard at its entity through Fn's loop path, at full volume
within half its radius and falling to nothing at the radius. Its subwaves
come from the entity at the intervals `sound.txt` gives, paced by a time the
engine keeps for each entity. Retail's multiplayer reckoning compares its
real time with the client game's own clock, so there they come never or on
almost every frame. Before, the first frame of `mp/ffa2` and `mp/ctf3`
dropped with `Unsupported jka-mp cgame import 51`. Now `mp/ctf3` plays as a
spectator to the end of each probe: a speaker's loop follows the law, and
its birds come about 20 seconds apart (Live subset). `mp/ffa2` then stopped
at its torches' fire, which the particle-dynamics slice above serves; its
`mp/ffa2` runs carried this round's import-51 files as an overlay. No gate
closes.

The [siege and sky-portal slice](2026-10-15-jka-siege.md) (2026-10-15)
serves the two Academy refusals the round above left. Game imports 42 and 43
keep siege's round record -- whether a time is to be beaten, the team that
set it and the time -- as retail `jamp.exe` 1.01 keeps its three words:
beside the game's VM pointer, copied each way and cleared by nothing
(`0x45ae89`, `0x45aeb3`, `0x12a4d34..0x12a4d3f`). A synthetic game module
reads and writes the record across a map change, a `map_restart` and a
`killserver`, on retail and on fnTech3's dedicated server, and all eleven of
its answers match (Source, Unit, retail comparison). `mp/siege_hoth`,
`mp/siege_desert` and `mp/siege_korriban` now start a siege game and stop on
the client at UI menu 11, the class menu. The scene a level's sky portal
draws (Raven's view flag `0x8`, on `mp/duel6`, `mp/duel9` and
`mp/siege_hoth`) now crosses as an engine-owned field. Once it has been
drawn, the level's other views leave their sky to it in GLx, Vulkan and RTX,
as retail's renderer does (`0x4abe18`, `0x4af5c9`, `0x4b96ad`), and the
server sends the entities the game marks as seen from the portal
(`0x46105c`). `mp/duel6` and `mp/duel9` now play to the end of their probes
(Live subset). `mp/siege_hoth` in a free-for-all then stops at Raven's
`RF_SETANIMINDEX` (`0x80000`). `mp/duel9`'s sky draws grey: Fn's shader
parser rejects the general `sun` line of 59 installed shaders, 58 of them
skies, and defaults them. No gate closes.

The [broadcast and level-time slice](2026-10-15-jedi-broadcast-level-time.md)
(2026-10-15) settles the two server differences the siege slice found, in
both Jedi multiplayer profiles. Retail `jamp.exe` 1.01's snapshot walk never
tests `SVF_BROADCASTCLIENTS` (`0x2`): its `broadcastClients` mask only adds
an entity (`0x46104f`), where fnTech3, after OpenJK, left a flagged entity
out for every client the mask did not name. The same walk never sends a
permanent entity (`EF_PERMANENT`, `0x460f90`), which the client game draws
from its baseline; fnTech3 sent it too, so the client game added it twice a
frame. And a level after `map` now carries on from the last level's time,
as retail's game runs on `svs.time`, which only a shutdown clears
(`0x45dd50`), where fnTech3 started an empty server's map at 0. A synthetic
module's bot snapshots and level times match retail's through a map change,
a `map_restart` and a `killserver`, at the wall clock and at `fixedtime 75`
(Source, Unit, retail comparison). On `mp/ctf3` a permanent speaker's loop is
now named once a frame, at 0.82 of full level 147 units away, where it was
doubled to 1.00 (Live subset). No gate closes.

The [sun keyword slice](2026-10-15-raven-sun-keyword.md) (2026-10-15) ends
the grey skies the siege slice found. All four Jedi engines read Raven's
general `sun` as `q3map_sun` under a second name (`jamp.exe` 1.01
`0x4b51a1`, `jasp.exe` 1.01 `0x47b312`): six words -- colour, intensity,
degrees, elevation -- a missing one as 0, and the rest of the line left to
the keyword loop. Both Academy profiles now read it so, into fnTech3's own
sky sun, and skip Raven's `lightColor` line as both engines do (`0x4b4e28`,
`0x47afcc`). Through the production parser, all 59 installed definitions
with `sun` (58 shaders) and the four with `lightColor` alone now load in
GLx, Vulkan and RTX, and no other definition changes (Source, Unit). On
`mp/duel9` the sky now draws in all three renderers, where it was flat grey,
or black under RTX, and GLx's cascaded shadows and RTX's path tracer take
its sun. `mp/ctf3`, `mp/duel1`, `mp/duel4`, `mp/ffa2` and `mp/ffa3` list
their skies as loaded where they were defaulted (Live subset). The other
profiles still refuse `sun`, as their engines do. No gate closes.

The [map shader flags slice](2026-10-15-raven-bsp-shader-flags.md)
(2026-10-15) reads the flag words of a Raven map's shader records with
Raven's own bits. The infoparm tables of both Academy engines put sky at
`0x2000` and nodraw at `0x200000` (`jamp.exe` 1.01 `0x56cfc8`, `jasp.exe`
1.01 `0x4f3440`), and the low five surface bits are a material number.
Both engines' `ParseMesh` skips a nodraw patch (`0x48d610`, `0x457644`).
fnTech3 read the records with Quake III's bits: RTX took every material
with bit `0x4` as the compiler's sky, 15,371 surfaces in 42 installed
Academy maps, and all three renderers drew 619 nodraw patches. The RBSP
reader now rewrites both words into Quake III's bits for every renderer
(Source, Unit). Under RTX, `mp/ffa3` takes `h_tatsky`'s sun, which a hull
surface standing first as "sky" had withheld, and `mp/duel9` traces 8,344
of its 8,364 world triangles where 6,669 were left to the rasterizer as sky
(Live subset). No gate closes.

The [light styles slice](2026-10-15-raven-light-styles.md) (2026-10-15)
serves the Raven light styles that made the renderers refuse a map. Retail
adds each later lightmap channel as its own stage tinted by its style's
colour, sums a vertex-lit surface's colour channels, and sums the light
grid's channels; every style starts white and stays white until a game names
it (OpenJK's renderer and client game, reference observation). The renderers'
RBSP reader now composes the later channels into the one Fn draws, at those
white colours. Each channel is found at its first channel's texels moved by
whole texels, as in every installed map (Source, Unit). All 98 installed Jedi
maps load in the renderers, where 45 were refused. Academy's `kor1` draws in
GLx, Vulkan and RTX, and `t1_fatal` and `t3_bounty` in GLx (Live subset).
Animated and switched styles do not reach the renderer yet. No gate closes.

The [surface sprites slice](2026-10-15-jedi-surface-sprites.md)
(2026-10-15) reads Raven's `surfaceSprites` stage word and its `ss`
companions as both Academy engines do (`jamp.exe` 1.01 `0x4b3fef`,
`jasp.exe` 1.01 `0x47a1a6`): the rest of the line goes to the sprite parser,
a bad or unknown word only warns, and only `jasp.exe` knows `flattened`.
The sprite stages leave the surface's passes and draw after it from the
second stage on (`0x4aef88`, `0x4af48f`), as sprites built from its
triangles through each renderer's own pipeline. Through the production
parser, all 39 installed sprite definitions now load in GLx, Vulkan and
RTX, and no other definition changes (Source, Unit). On `mp/ctf3` the grass
field, which drew as the default image's grid, now grows its grass in all
three renderers (Live subset). Raven's `alphaFunc GE192`, which the grass
stage also uses, is still unknown to Fn's parser. No gate closes.

The [alphaFunc GE192 slice](2026-10-16-jedi-alphafunc-ge192.md)
(2026-10-16) reads Raven's fourth alpha test as both Academy engines do:
`NameToAFunc` returns `0x80000000` for `GE192` (`jamp.exe` 1.01
`0x4b24da`, `jasp.exe` 1.01 `0x4785f0`) and `GL_State` applies it as
`glAlphaFunc( GL_GEQUAL, 0.75f )` (`0x48a4c2`, `0x454bf2`). `jka-sp` and
`jka-mp` carry it in Fn's comparison-and-reference alpha test (greater or
equal, 192 / 256), which the GL renderer draws on its CPU path. Through the
production parser, the GE192 warning leaves all 32 installed definitions
that use it in GLx, Vulkan and RTX, and nothing else changes (Source,
Unit). On `mp/ctf3` the grass blades are now cut cleanly, and the blades'
transparent parts no longer hide the grass behind them, in all three
renderers (Live subset). No gate closes.

The [menu slice](2026-10-15-jka-ui-menus.md) (2026-10-15) serves the client
game's menu requests, which the siege slice above left refused from 6 to
11. Retail `jamp.exe` 1.01 serves client-game import 238 at `0x413032` as
`VM_Call(uivm, 7, args[1])` and answers 0: the number is the UI module's
own, and the engine neither translates nor bounds it. `uix86.dll` 1.01's
`_UI_SetActiveMenu` (`0x40010de0`) opens menus 0 to 11 and ignores any
other number, as it ignores every number while no menu is loaded
(`0x40010dfa`, `0x40010de6`). The 1.01 client game asks for 3, 7, 8, 10 and
11 from seven call sites; the engine itself sends 0, 1, 2 and 9, the last
from its `voicechat` command, which fnTech3 does not register. fnTech3 now
hands the client game's number to export 7 unchanged through an
engine-only request, `UI_SET_MODULE_MENU`. The engine's own requests keep
their vocabulary, and a UI that is not ready or not the legacy native
module is still refused before any call (Source, Unit). Live, all three
siege maps open the class menu. `mp/siege_korriban` plays on with it, and
`team red` from the console joins as the team's first class and waits for
the other team. With a bot on that team the server stops at a trace with
pass entity -1, which retail compares only with 1023 (`0x4624f8`).
`mp/siege_hoth` and `mp/siege_desert` stop at client-game import 312
(`CG_G2_SKINLESSMODEL`). Diagnostic runtimes past the trace stop reached
imports 215 and 315, and the client game's `briefing` opened menus 10 and
8 (Live subset). No gate closes.

The [pass-entity slice](2026-10-16-jka-trace-pass-none.md) (2026-10-16)
serves the trace that stopped `mp/siege_korriban` with a bot. Retail
`jamp.exe` 1.01 compares the pass entity only with 1023 (`0x4624f8`) and
reads any other number's owner (`+0x294`) and flags (`+0x238`) from the
game's entity array without a bound. The retail game passes -1 from seven
`MASK_SOLID` sites in both its 1.00 and 1.01 builds, the bot's melee and
saber ground checks and the seeker drone's line of fire, so retail reads
the record in front of `g_entities`: the module's own cvars, the owner the
float bits of `g_forceBasedTeams` and the flags a tail of `g_podiumDist`'s
string. At the defaults that skips solid things owned by client 0 or by -1;
it is the module's memory layout, not a contract. fnTech3 reads -1 as no
pass entity, as its Outcast trace does, and still refuses every other
number outside the game's records (Source, Unit). The client game's trace
imports carry no pass entity. Live, the round now begins and plays about
40 seconds; when the bot dies the server's `ircg` reaches the client game,
which stops at client-game import 315 (`CG_G2_CLEARSKINGORE`) (Live
subset). No gate closes.

Star Wars Jedi Knight: Jedi Academy is the sixth title, with two mode profiles
over one `base/` installation: `jka-sp` for Raven's single-player campaign and
`jka-mp` for multiplayer. The [foundation record](2026-09-21-jka-contracts.md)
pins the installation identity and startup contracts against a legitimate
retail 1.01 installation, whose paks, modules and assets have been read.
The retail multiplayer game module runs a retail level and bots on a developer
server. Its protocol-26 codec now has source and unit evidence, including
reference packet parity and full-size fragmentation. Retail `jamp.exe`'s
dedicated server admits fnTech3's client, pure; the retail client needs
disc 1 ([legacy-server record](2026-10-05-legacy-server-hosting.md)).
Single player has a measured retail API-8 bootstrap
and separate entity/world adapters; campaign, client, rendering and savegame
gates remain open. See the [protocol and SP bootstrap record](2026-10-02-jka-compatibility.md).

| Gate | Evidence | Remaining requirement |
| --- | --- | --- |
| Profile / identity / discovery | Source, Unit, Live subset | Both profiles, their aliases, limits and the shared-installation mode choice pass unit checks, and a legitimate 1.01 installation is recognized from `base/assets2.pk3`'s pinned identity, reports that a mode must be chosen, and resolves single player's module in its root while refusing to find multiplayer's there. A developer dedicated server starts under both profiles against that installation, writes each mode's own configuration into an isolated home path, and loads a retail map's collision in each mode before refusing the module boundary ([record](2026-09-21-jka-runtime.md)). The multiplayer profile listens on 29070 and sends to masters on 29060, as `jamp.exe` 1.01 does (0x4526db, 0x45f77a), where it had used Quake III's 27960 and 27950, and its info reply carries retail's `needpass`, `truejedi`, `wdisable` and `fdisable`; each engine's browser lists the other's server, and the retail server heartbeats as fnTech3's does ([legacy-server record](2026-10-05-legacy-server-hosting.md)). Its single player is game type 5 and `spmap` selects it, `map` keeps every game type, and a client-hosted server answers `getinfo`, `getstatus` and `getchallenge` in every game type, as `jamp.exe` does (`SV_Map_f` 0x4560b0, `SVC_Info` 0x45fa10, `SV_GetChallenge` 0x457080, `SVC_Status` 0x45f830), where fnTech3 had taken 2, Jedi Master, for single player; live, the client executable's server answered all three and admitted a second fnTech3 client in a free-for-all and in single player, and the retail 1.01 game refused Jedi Master ([Jedi listen-server record](2026-10-06-jedi-listen-server-game-types.md)). Other editions, a Steam installation and its AppID and folder, and every platform but this one remain open. |
| Module ABI | Source, Unit, Live subset | Multiplayer's 796 module numbers are frozen, unique, reversible and refused outside their own profile, with Raven's gaps refused explicitly, and the installed modules' export tables confirm both shapes: `GetGameAPI`/`dllEntry`/`vmMain` for single player's root module, `dllEntry`/`vmMain` for the three multiplayer modules that ship inside the paks ([record](2026-09-21-jka-module-abi.md)). **The retail engine's own dispatcher confirms the game half of that table exactly**: `jamp.exe` bounds the import number at 634 and jumps through a 635-entry table, and the 329 slots that do not fall to `Bad game system trap` are the *same set* as the 329 game imports fnTech3 names -- no extras on either side, from a table built by an unrelated method. **The retail module can only ever ask for 150 of those 329**: every import it can reach has a call wrapper in its own code, and counting them bounds the whole of this title's boundary rather than estimating it ([record](2026-09-22-jka-trace.md)). Eleven records are frozen to the byte -- entity state 532, player state 1,552, shared entity 864, the 48-byte trace and siege's 12-byte round record -- with all 379 members' offsets and sizes asserted against a measured table ([record](2026-09-21-jka-records.md), [siege record](2026-10-15-jka-siege.md)). Ninety-eight of multiplayer's imports were served by that record, and 159 are now, as `tests/jka_module_refusal_tests.py` counts them ([bone animation record](2026-10-07-jka-server-bone-anim.md), [siege record](2026-10-15-jka-siege.md)), and Quake III's fourteen shared arithmetic and memory calls are handed through unchanged; linking and unlinking are served by name and never handed through, because Quake III's 34 and 35 are the bot client calls. Academy's entity state converts both ways, field by field: 29 of its fields are Quake III's and 65 are its own, and a record with a distinct value in every one of its 532 bytes returns byte for byte, so a field left unmapped fails the test rather than reading as zero. Two renumberings were measured rather than inherited: **this title's sine and gravity trajectories are 5 and 6, not Quake III's 4 and 5**, and **its trace is 48 bytes with the two solid flags and the entity number packed into the first four**, so its fraction sits where Quake III keeps a flag. Either taken on resemblance would have been silently wrong in every trace and every falling object ([record](2026-09-22-jka-trace.md)). Single player now freezes retail API 8 with 131 import slots and a 76-byte export table, preserving its three save callbacks; the bridge owns partial-initialization cleanup and names every unsupported callback ([protocol and SP bootstrap record](2026-10-02-jka-compatibility.md)). Retail SP player state (4,916 bytes) and cgame snapshot (144,248 bytes, including ping) now have separate frozen records; player-state integration and savegame records remain open. |
| Wire codec / demos | Source, Unit, Live subset | Protocol 26 selects Academy's own contract, distinct from ioEF despite the shared number. Frozen tables carry 132 entity, 137 normal-player, 140 pilot and 69 vehicle fields, keyed commands and sparse arrays. Filesystem transitions reset and transactionally reload mod field-width overrides. All 25,192 generated Huffman packets match the pinned reference on x86 and x64. Real netchan tests reassemble 49,152-byte messages in both directions and cover truncated, oversized and reordered fragments; Q3/QL/RTCW/Elite Force message checks remain covered. Snapshot build and parse carry the second vehicle body and Raven's broadcast-client mask. Profile-aware demo selection and vehicle snapshot recording pass production-code tests. The measured 16-bit static-map RMG trailer is now served across live and demo gamestates; OpenJK-to-fnTech3 now passes active admission, 324 distinct snapshots including deltas and clean disconnect. The original retail peer still requires Disc 1, and fnTech3-to-OpenJK now preserves protocol 26 through the legacy handshake and receives gamestate, then passes bounded cgame model/bolt and skin/animation metadata, ambient/effect definitions, explicit music preloading and the effects clock. An earlier production attempt receives three snapshots at the light-style boundary. Neutral light-style metadata, the empty client ROFF update, renderer-owned distance culling and entity-token reset/EOF now pass their measured boundaries. Pinned native pak reload now restores both actual module proofs. Production attempt 50 reaches maintained active admission and ClientBegin, with 73 valid snapshots including deltas and no unpure drop, then stops at CG_SETUSERCMDVALUE (235) after owned Ghoul2 copy/duplication, initialized UI menu relay and the read-only routing getter. Later production attempt 53 passes neutral command values and single-polygon submission, reaches the same maintained active/pure state with 55 delta-bearing snapshots and stops at CG_S_GETVOICEVOLUME (35). Production attempt 56 passes the measured audio-absent voice query and owned remote-client bolt query, reaches CG_G2_GETBONEFRAME (305) with 105 valid snapshots and accepted maintained pure admission; this attempt contains no deltas, while candidate 55 has 62 delta-bearing snapshots. Owned bone-frame metadata then advances production attempt 61 to the client Ghoul2 scene-submission refusal (202), with 77 snapshots including deltas; blend-marked metadata is served; later measured forward transitions gain owned Fn pose evidence. Owned cgame scene staging and six-word static tag queries then advance production attempt 67 to effects playback import 266 with 74 delta-bearing snapshots and accepted maintained pure proof. Actual Fn geometry/skin visibility and shared UI/cgame owner IDs have unit evidence; no complete scene flush, capture or clean drop is established. The shader-only single-spawn lifetime subset advances production attempt 73 to the engine-default skeleton model at scene import 202, with 69 valid snapshots and no observed delta in that run; corrected candidate 72 has 57 snapshots including deltas. The actual `blaster_w.glm` requests `*default`, whose mesh/skeleton bytes are not retained. No scheduled effect drawing or complete frame is established. The generated engine-default GLA and retained static meshes advance production attempt 80 to ambient update import 48 with 83 snapshots including deltas; candidate 74 has 85 delta-bearing snapshots. All 41 installed default models and their 65 LODs pass pose/geometry probes, with no asset extraction. Complete scene flush and frames remain unproven. The measured absent ambient update advances candidate 77 and production 84 to view import 209; production has 59 snapshots including deltas. A separate diagnostic identifies normal-view `RDF_DRAWSKYBOX` (0x10) and six queued entities. Full scene rendering remains open; present ambient sets are now played ([ambient sets record](2026-10-06-jka-ambient-sets.md)). Complete client admission, cgame rendering and clean disconnect in this direction remain open. fnTech3's client now joins retail `jamp.exe` 1.01's dedicated server on loopback with pure admission, enters, sends chat the server receives and is kicked with retail's `@@@WAS_KICKED` ([legacy-server record](2026-10-05-legacy-server-hosting.md)). The server's chat line, whose sound reaches client-game import 38 (`CG_S_STARTLOCALSOUND`), now plays through fnTech3's local-sound path, and a map change completes since `VM_Call` no longer refuses Academy's map-change export as ET's snapshot callback, which shares its number; the client game's first frame on the new map passes its two-model Ghoul2 instance ([attachments record](2026-10-06-jka-attachments.md)) and its items carrying Academy's `RF_RGB_TINT` (0x800) and `RF_FORCE_ENT_ALPHA` (0x400) ([live-join record](2026-10-06-jka-live-join.md)); on `mp/ffa1` `map_restart`, a reconnect and the server quitting complete in this direction, the client staying up at its menu after the quit ([entity tint record](2026-10-06-jka-entity-tint.md)), and with `mp/ffa3`'s present ambient set (import 48) now played, every step of the main session completes with `mp/ffa3` as the second map, as does the kick session ([ambient sets record](2026-10-06-jka-ambient-sets.md)). The retail client needs disc 1, so the reverse direction, downloads and live demo recording/playback remain unproven ([protocol and SP bootstrap record](2026-10-02-jka-compatibility.md)). |
| Filesystem / pure / downloads | Source, Unit, Live subset | A developer dedicated server mounts the installation's four paks in their own order -- `assets3` over `assets2` over `assets1` over `assets0`, 23,744 files -- and executes Raven's own `default.cfg` out of them ([record](2026-09-21-jka-runtime.md)). Raven's `assets0`-`assets8` naming is refused for download in `base` and `missionpack`, and a referenced pak is marked for the native `cgamex86.dll`/`uix86.dll` pair as well as Quake's QVM names ([record](2026-09-21-jka-maps.md)), which this title needs because its multiplayer modules are themselves pak members. A maintained pure-admission attempt exposed Academy's omission of serverId from its checksum proof; retail disassembly confirms the format and both sender/verifier now have production-code tests. The fifth maintained reverse run passes pure admission, active state, 424 distinct snapshots including deltas and clean disconnect. Actual byte/length/CRC verification of pinned native UI and cgame now restores their pak references after restart; forward production attempt 50 has accepted proofs, active state and 73 snapshots including deltas, before its next named cgame refusal. Failed highest-precedence pak entries do not fall back. Original retail pure admission, mod directories and downloads remain open. |
| Assets / collision | Source, Unit, Fixture, Live subset | Every one of the installation's 342 maps, models and motion paths reads: all 57 maps carry `RBSP` version 1 and translate for collision, 376,744 surfaces in all; all 127 Ghoul2 meshes and 66 skeletons read, 529,664 vertices and 131,782 frames, with each mesh checked against the bone count of the skeleton it names; and all 92 ROFF motion paths read with their note tracks ([map record](2026-09-21-jka-maps.md), [model record](2026-09-21-jka-models.md), [motion record](2026-09-21-jka-roff.md)). Every one of the 1,011 compiled ICARUS scripts the engine would open reads through a bounded container reader as well, 25,987 blocks and 51,567 members with no refusals, which matters early because the game module asks for ICARUS inside `GAME_INIT`. A second reader groups each block's members into its values under six rules that cover all 26,512 blocks, refusing anything they do not, and reads the same 1,011 scripts into 25,987 statements and 39,854 values with no refusals. The rest of what the corpus fixes about itself is recorded rather than built in: five block numbers open a body that one closes and all 1,011 scripts nest exactly, and the names divide into a map's, a single script's and a level's. That original record describes the container reader; subsequent server ICARUS implementation has unit coverage and still requires campaign evidence ([script record](2026-09-21-jka-icarus.md)). The collision projection drops lighting data. A separate normal-style renderer reader retains the original RBSP and all 18 lumps, adapts geometry, static lighting and the indexed light grid into Fn paths, and accepts 56 of 61 installed maps in CPU probes; five extra-style maps remain named refusals. No live world-rendering proof is established. Server Ghoul2 posing and ROFF playback have separate unit evidence. Client skinning, full script gameplay and Raven's shader extensions remain open. All 12,902 MP3 files decode through Fn's MP3 decoder, which reproduces every one of the 12,510 sizes Raven's tags store, decoding each frame from its own header as Raven's engines do ([last-frame record](2026-10-08-mp3-last-frame.md)). |
| Server / gameplay | Source, Unit, Live subset | Earlier retail multiplayer probes run `mp/ffa1` and bots through navigation, collision and server Ghoul2 calls ([runtime record](2026-09-21-jka-runtime.md), [pose record](2026-09-22-jka-pose.md)). Alt-fire and force action flags now pass through the real bot action store, with 4,096 combination checks. ROFF playback uses the measured millisecond interval, trajectory velocities and note callbacks; cache generations protect callback reentry. Ghoul2 animation seeds retain fractional floats and retail start-clock truncation. SP has separate 272-byte entity state and 376-byte shared prefix, a measured 1,252-byte stride and checked world callbacks; its sixth module Init probe passes owned Ghoul2 metadata initialization, then reaches the named Ghoul2 trace-geometry refusal and fails recursively during retail DLL teardown. The checked late-Delete lifetime correction then passes the seventh error-unload probe: the same named trace refusal recovers without a recursive fatal error, before the harness stops the process. The eighth Init probe passes the prior trace frontier after world obstruction and candidate filtering are corrected, then reaches WE_IsShaking (128) without recursive or fatal error; the harness stops the recovered process. A fresh phase marker observes retail API-8 Init returning and its entity arena binding before the first frame reaches the same weather refusal. SP BODY linking, native point placement and the map-contents seed now have production unit evidence. Actual Ghoul2 geometry collision and campaign collision remain open. The bridge serves 46 function callbacks and one data slot, with 84 named refusals. A new direct retail-engine comparison finds all 18 bolt matrices differ for the same model and queries; the corrected kernel then matches 90 direct retail queries within 0.00005, with maximum difference 0.0000114441. This covers inherited animation, measured angle modes and bolt placement; geometry, blending, unmeasured modes and SP posing remain open. The server has a Ghoul2 store of its own, which serves what retail's server does: retail `jamp.exe`'s game imports 587 to 589, 594 and 600 reach the same Ghoul2 cores as its client game's, so the engine `*default` skeleton every weapon and saber names, models attached to another posed from their parent's bolt, and retail's tag frame are served as the client game serves them ([attachments record](2026-10-06-jka-attachments.md)). One synthetic game module asks retail's dedicated server and fnTech3's 72 bolt queries on Kyle holding three pistols and a saber: all 66 that retail's model order reaches match within 0.0000075 and 0.000107 units, the 6 on a self-linked instance are refused with nothing written. Before, the 48 on the pistols and the saber were refused outright, `G2_ADDBOLT` answering -1 for their tags, and body tags were 0.59 units off. The retail 1.01 game asks through 52 call sites and reads none of the results ([server attachments record](2026-10-06-jedi-server-attachments.md)). The server poses those bolts at its own Ghoul2 clock, set where retail's server sets it, so a game frame's queries answer one or two server frames behind the time the frame was given and the module's time word is ignored; transitions between clips are served there too, and a failed query writes retail's fallback, the entity's placement times the fixed axes. A second module asks both dedicated servers 1,468 queries retail answers, 231 mid-transition, across a `map_restart` that sets no clock: 1,466 match within 0.0000134 and 0.000126 units, the other two being retail reusing a pose at one clock; before, every mid-transition query was refused and 240 others were up to 43 units off ([pose clock record](2026-10-07-jka-server-pose-clock.md)). Pose reuse at one clock and a listen server's shared clock remain open. Import 597 (`G2_GETBONEANIM`), which the retail game calls once to hand a bone's clip on to its arm bones, answers at that clock as retail's does: the clip's frame, range and speed with its bone record's own flags word -- the 0x8000 the clip setter marks, a blend's 0x80, any angle mode -- or 0..1 defaults for a bone with no clip, and nothing for a name the skeleton lacks, matched without case; the clip setter drops a mark handed back. A third module's 678 answers retail and fnTech3 both give match bit for bit, 124 of them fractional frames; clips the store does not time are refused by name, and the mark stays where retail's collision cache would clear it, since import 607 is still refused ([bone animation record](2026-10-07-jka-server-bone-anim.md)). Both setters and AddBolt now match Academy bone/surface names without ASCII case, including replacement clips, bends and duplicate bolt indices; the client game opts into the same shared retail-core contract and Outcast retains its prior matching. A focused synthetic server module repeats in retail and fnTech3: all 195 animation answers, 33 distinct setter results and 31 distinct AddBolt results match exactly, and all 91 bolt matrices match within 0.0000063 in their axes and 0.0000105 units in position ([bone names record](2026-10-07-jka-bone-names.md)). Siege's round record (imports 42 and 43) is kept for the life of the process, as retail keeps it, and a synthetic module's eleven answers across a map change, a `map_restart` and a `killserver` match retail's ([siege record](2026-10-15-jka-siege.md)). Game imports 27 and 28 are retail's one server trace (`0x462920`): Raven's owner and no-shot rules, a start inside an entity naming it, and for import 28 the Ghoul2 step, in which an entity's models are posed at the level time and traced as segments or swept radii, a miss restoring the trace and a hit moving its end onto the mesh; the box that stands for a body now carries Academy's body bit `0x100`, without which no trace in this profile struck a body's box. A synthetic module sweeping two linked Kyle bodies with 736 traces in retail and fnTech3 agrees on 735 within 0.0037 units, the other a box-edge plane tie ([Ghoul2 trace record](2026-10-15-jka-g2-trace.md)). Other gameplay corrections require live validation. SP player-state integration, client admission, campaign and saves remain open ([protocol and SP bootstrap record](2026-10-02-jka-compatibility.md)). |
| Client / renderer / audio / input | Source, Unit, Live subset | Multiplayer loads retail UI API 7 and cgame through export whitelists and marshals its own records, including vehicle snapshots and the 76-byte glconfig. All seven fonts and 215 StringEd packages parse; glyphs use the active Fn renderer and, from Raven's virtual 640x480 screen, its HUD and menu transforms ([widescreen record](2026-10-15-jedi-widescreen-hud.md)). Live captures show the weapon, force-power and inventory bars whole and centred at 21:9, 16:9, 4:3 and 5:4, each bar's name now going down with its bar on screens taller than 4:3, and the loading screen's stretched levelshot (or `unknownmap_mp`) under a centred 4:3 composite; either zoom then stopped the client at cgame import 216 (`CG_R_SETRANGEFOG`) before its mask was drawn ([HUD live check](2026-10-15-jka-hud-live-check.md)). Imports 214 and 215 now turn their pictures through the HUD rules with retail `jamp.exe`'s pivots, and 216 sets a ramp on Fn's fog volume pass for the world-covering fog, starting where retail's GL linear fog (`0x4aed70`) starts; both zooms draw live with every piece stretched with its mask at 21:9 and 5:4 ([zoom record](2026-10-15-jka-zoom.md)). Retail MP menu startup now exits normally with engine captures on OpenGL and Vulkan after nullable UI cvar registration is corrected. An independently authored UI fixture renders two distinct installed Kyle running poses through OpenGL, Vulkan and the RTX module's raster path. Owned geometry, selected UI clocks and a runtime MDR use Fn's existing skeletal, shader, entity-lighting and point-light systems. A pose drain now preserves the open Vulkan frame and RTX graph; production tests cover old-pose consumption and final submission. Skin/material binding, extra flags/types, effects and full cgame rendering remain incomplete; bounded cgame model/bolt, skin visibility, animation/angle, ambient-definition and effect-registration/context metadata, actual explicit music preloading, the effects clock, neutral light-style metadata and the empty client ROFF update, actual renderer-owned distance-cull data and entity-token reset/EOF pass their earlier boundaries on all three renderer modules. Owned Ghoul2 copy/duplication, initialized player-configuration menu relay and the existing read-only routing getter now advance production attempt 50 to CG_SETUSERCMDVALUE (235) with 73 snapshots including deltas, maintained active admission and no unpure drop. Neutral command values and frozen 24-byte polygon records now advance production attempt 53 to CG_S_GETVOICEVOLUME (35) with 55 snapshots including deltas. All three owned polygon queue bodies pass their fixtures. Owned remote bolt queries now use the existing Fn CPU pose kernel and the measured frame clocks, publishing retail failure matrices. Active audio, local-server clock sharing and complete cgame geometry remain unserved; the server bolt failure-output/time discrepancy remains open. The later OpenGL probe captures the player-configuration menu over the world; complete gameplay remains open; sparse model-copy gaps, nonneutral command overrides and fighter controls remain named refusals. Fn world views use the measured map distance while retaining Fn PVS, FOV, fog and lighting. Colored style-zero modulation and client ROFF cache/play remain named refusals. Muted OpenAL probes preload an actual installed MP3; dynamic music and audible output remain open. Wireframe automap initialization serves the measured default-disabled return and refuses an enabled request. All 305 installed skin declarations parse; owned cgame skin/visibility and single-model Fn skeletal scene staging now pass unit fixtures. Scene queueing and static tag queries advance production to effects import 266; complete scene flush and skin-material rendering remain unproven. All 376 installed effect files parse; registration owns declarations and real Fn media handles. Context lifecycle prepares its own bounded pool, clock and nullable view; a private forward probe registers 107 effects. Shader-only single-spawn no-motion particle/oriented/cylinder lifetimes now enter Fn sprite/polygon packets, with measured constant-curve alpha, shader-clock, paused-creation and expiry behavior. Focused production checks pass. Candidate 72 and production 73 pass playback and stop on the engine-provided `*default` model at scene import 202; scheduled drawing and complete frames remain unproven. Remaining playback controls and primitive families refuse explicitly. Owned default skeletons now serve static GLMs through the same Fn CPU pose and runtime-MDR path. Missing/null/empty ambient names now return zero before touching the listener vector; present sets are now played (below). Later ordinary-sky, owned forward-blend and native296 adapters pass that view and pose boundary. Production92 receives 297 snapshots including deltas and writes an 800 by 600 engine menu capture before refusing ROFF cleanup283 during shutdown. Candidate90 has 350 delta-bearing snapshots. All 440 retail transition queries match the actual Fn C++ kernel in ordinary and fast-math builds. Angle blends, local-server clocks and complete gameplay remain open. Candidate and production advance to ambient update 48; complete scene drawing remains open. Partial cgame initialization skips native shutdown reentry and recovers without the earlier recursive error. All renderers collect shaders/*.shader plus engine supplements, adapt noTC and support the normal-style RBSP projection; five extra-style maps and live world rendering remain open. Supported world views retain Fn FOV correction and its explicit disabled option. Supported sound calls and ambient definitions/mover-stage queries use Fn audio services; client-game local sounds (import 38) carry Academy's 14 channel numbers as the engine channels of the same meaning, the rest as numbers of their own, and retail's 0.75 level for the listener's own sounds below the ambient channel, into Fn's local-sound path; the UI module's own local sounds (import 33) take the same path, and its `ui_openmenu` command reaches them without input ([UI local sound record](2026-10-08-jka-ui-local-sound.md)), as interface audio on any channel, so `play`'s automatic-channel clicks take no room ([menu sound record](2026-10-08-ui-sound-policy.md)); live, the client game's first frame after a map change no longer stops at the Ghoul2 instance holding Kyle's model with the Bryar pistol's world model attached ([live-join record](2026-10-06-jka-live-join.md)): a model linked to another is posed from its parent's bolt with retail's link, model order and tag frame and drawn as its own Fn skeletal entity with Fn materials, lighting and shadows, and the client game's bolt queries answer attached models from the same posing; no capture shows an attached weapon yet ([attachments record](2026-10-06-jka-attachments.md)); items' `RF_RGB_TINT` (0x800) and `RF_FORCE_ENT_ALPHA` (0x400), measured in retail's stage iterator, colour function and surface list, cross as engine-owned entity fields into all three renderers' colour, blend and draw-order paths, the RTX path tracer leaving those entities to raster, and engine captures in GLx, Vulkan and RTX show an item sprite drawn with forced alpha; a joined player then stopped at the first client-game sound (import 37) ([entity tint record](2026-10-06-jka-entity-tint.md)), which the [October 7 audit](2026-10-07-jedi-compatibility-audit.md#academy-positional-sounds-and-looping-policy) serves; Academy now registers the existing MP3 decoder, which completely decodes all 82 referenced installed MPEG files. With `s_language` `deutsch`, `francais` or `espanol`, both profiles play the German, French or Spanish voices, falling back to English, as both retail engines do; live, Kyle's lines and other voices load in each language at the multiplayer menu ([foreign voices record](2026-10-09-jka-foreign-voices.md)). The silent forward probe establishes definition parsing only. Present ambient sets (import 48) now play through an engine-owned player measured in retail `jamp.exe`: a new set fades in over 1,000 ms of real time as the one it replaces fades out, each slot's subwaves come at intervals and volumes drawn from the engine's own generator, and the parsed sets of every map keep their own numbers, so the set of the map before fades out on the next one. The player matches an independent model of those readings over 10,000 generated frames; its loops reach both backends as their own loop voices, merged by sound and clamped at 255 as retail mixes them, with Fn's room reverb, tone classes and output, and its subwaves are Fn one-shots on Academy's ambient channel from the world entity. With sound on, a diagnostic build answering voice volume (import 35) showed `mp/ffa3`'s `city_outdoor` loop live as an OpenAL voice at the listener rising from 0.04 to 1.00 over about a second ([ambient sets record](2026-10-06-jka-ambient-sets.md)). Voice volume (35) is served with the facial values retail measures from the playing voices ([October 7 audit](2026-10-07-jedi-compatibility-audit.md#academy-voice-animation-metadata)), and local sets (51) are played ([2026-10-15 round](2026-10-15-compatibility-round.md#jedi-academy-sets-heard-around-a-point)); a listening test, complete spatial-audio adaptation and physical input remain unproven. SP menus/client admission and active RTX tracing remain open ([boundary record](2026-10-02-jka-compatibility.md)). Entity types 5, 8 and 9 -- the saber glow, line and oriented line -- enter all three renderers' sprite branch and are tessellated as retail's `0x4ba780`, `0x4bb3c0` and `0x4bb4c0` tessellate them, matching retail's own vertices on 22 synthetic cases; types 3, 6, 10 and 11 remain named refusals ([round record](2026-10-14-compatibility-round.md#jedi-academy-saber-glow-and-lines)). The authored `glow` stage keyword is now read as both retail executables read it, and effect particles serve shader lists, motion, curves and delays; a local `mp/ffa2` join plays to its scripted end with its torches burning on all three renderers ([particle record](2026-10-15-jka-particle-dynamics.md)). Tails, particle physics, impact and death effects and spawn shapes are served too; effect runners added to `mp/ffa2` play sparks, spray, dust and traced bolts on all three renderers ([tails record](2026-10-15-jka-tails-and-physics.md)). The `oneShotAnimMap` and `clampAnimMap` stage keywords are read as both retail executables read them, so the six `gfx/exp` explosion shaders load and play their frames once on all three renderers ([one-shot record](2026-10-15-jka-oneshot-animmap.md)). A level's sky portal (Raven's view flag `0x8`) is drawn as retail draws it: its scene first, and the level's other views then leave their sky to it, in all three renderers; `mp/duel6` shows the portal's sky in GLx, Vulkan and RTX captures ([siege record](2026-10-15-jka-siege.md)). Effect Sound primitives start their sounds at the spawn origin through the client game's spatial-sound entry and Fn's audio; muzzle-flash, clash and impact sounds played from effect runners show as positioned OpenAL voices ([effect sound record](2026-10-15-jka-effect-sound.md)). Effect CameraShake primitives hand their record to the client game's export 31, whose own view shake Fn renders; runners within the shake radius jitter the view live in GLx and Vulkan, and `cl_viewShake` scales the intensity handed over ([camera shake record](2026-10-15-jka-camera-shake.md), [view-shake record](2026-10-15-view-shake.md)). Weapon fire is reachable for a local player: Escape reaches the menu module as retail sends it, `+altattack` is button 7, the any-key bit is `0x100`, and a concussion rifle shot plays its muzzle flash and sound ([weapon fire record](2026-10-15-jka-weapon-fire.md)); its Ghoul2 trace is served, so both modes reach their impact, where `concussion/explosion.efx` stops on the effects Flash primitive ([Ghoul2 trace record](2026-10-15-jka-g2-trace.md)). Distortion copies (renderfx `0x2000`) cross as an engine-owned entity field into an entity refraction pass built on Fn's liquid refraction in all three renderers; live, the spawn shield's copy bends and filters the scene behind it in GLx, Vulkan and RTX captures, and retail's mirrored, texture-mapped screen copy is not reproduced ([entity refraction record](2026-10-15-jka-entity-refraction.md)). In first person the view weapon's muzzle flash plays through client-game import 268 at the caller's own axis, with its sound, light and shake, and a bolted entity effect refuses by name ([entity effect record](2026-10-15-jka-entity-effect.md)). Effect Decal primitives press their marks into a pool kept as retail's renderer keeps it, cut by Fn's mark clipper and drawn as Fn polygons; a first-person Bryar shot leaves its scorch mark on the wall in GLx and Vulkan captures ([decal record](2026-10-15-jka-decal.md)). The client game's and menus' 2D is Raven's virtual 640x480 screen, which retail stretches over the window (`jamp.exe` `0x48b09c..0x48b0e5`); Fn drew it unscaled in the window's top-left 640x480 pixels until the [widescreen record](2026-10-15-jedi-widescreen-hud.md). The HUD now takes Fn's widescreen layout with Academy's own rules, the client game reads the HUD's 4:3 frame as its screen, and the menus follow `cl_menuAspect`; engine captures at 21:9, 16:9, 4:3 and 5:4 in GLx and at 21:9 in Vulkan and RTX show the placement (Live subset). |
| Savegames | None | Single player's save format is unimplemented. |
| Optional services | Unavailable | No service success is synthesized. |


The normal-sky, owned forward-animation and native296 slice has an incremental
production build, 23 focused passing groups and a visually inspected engine
menu capture. Its seventeenth combined source/runtime audit passes all 252
groups, with 250 passes, two expected skips and 2,525 Python regressions
(126 skips, no failures). Later empty-cleanup stages are excluded. Original retail pure
admission and bidirectional interoperability, demos, complete gameplay,
all-backend rendering, active audio/input and SP campaign/save gates remain open.

Empty ROFF/weather cleanup now lets production 99 complete the maintained
forward pair with 341 snapshots including deltas, pure admission, an engine
menu capture and clean disconnect. The maintained reverse pair has earlier
clean-disconnect evidence; original retail peers still require legitimate
Disc 1. Measured MP scene-light imports 207/208 now marshal into Fn's normal/
additive light, color and shadow services. Academy's installed half-cell atlas
now serves Fn diagnostics/console/HUD text. The bounded generated-client control
reaches EOF with 28 snapshots and readable spectator-world captures in candidate
GLx, Vulkan and RTX raster. All 13 promoted fixtures, ordinary/fast-math host
checks and seven focused groups pass. All 49 affected production client objects
compile; a concurrent renderer sweep blocks linking into the shared executable,
so this slice's production relink/runtime checks remain pending. Its running
probes are preserved. The longer native-server demo passes light 207, then refuses
Raven `RT_SABER_GLOW` entity type 5 (flags 0) after 408 snapshots, before a
completed frame or EOF. The full renderer/demo/retail, gameplay, weather, active
audio/input and SP campaign/save gates stay open.

Frozen source18, including empty cleanup, passes all 252 groups: 250 passes,
two expected skips and 2,526 Python regressions (126 skips, no failures), with
unchanged frozen source and native test executables. Concurrent RTX source/DLL
edits prevent a complete current-production unchanged-input audit and are
preserved. Candidate RTX evidence uses the reserved pre-drift DLL. The frozen
suite excludes later light/charset source; its incremental build and focused
checks do not replace a fresh complete current-production audit.

## Jedi Outcast gate matrix

The [October 7 audit](2026-10-07-jedi-compatibility-audit.md) adds bounded
StripEd localization, the retail key-number boundary, global definitions
and checked parser/time records to the multiplayer menu, and isolates
single player's flag-2 save metadata and 40-byte cvar
records from engine cvars. SP refresh preserves restart latches and module
acknowledgements. All 30 installed string packages parse in memory.
That audit's integrated menu reached renderer model registration (UI 18).
These corrections supersede the earlier UI-200 frontier below; they do not
establish a working client or campaign, and no gate closes.

The [second round](2026-10-07-jedi-compatibility-round2.md) serves static
menu model/skin/scene/polygon/light/view/tag/bounds imports with Outcast's
own frozen renderer records. Valid offscreen logo viewports retain their
signed origins with checked framebuffer arithmetic. All three renderers
now discover Outcast's authored `shaders/` files while retaining Fn shader
supplements. Its measured noise/random waveforms, TGA orientation and console
atlas enter Fn's material/image/text paths. Ghoul2, procedural entities, remaining Raven flags and complete
client/world/audio paths remain open; no gate closes.
The unchanged MP UI draws its main menu in a verified GLX engine capture
and exits cleanly. Local map startup reaches cgame import 300; cleanup now
preserves that refusal instead of recursively reentering the partial module.

The [UI local sound record](2026-10-08-jk2-ui-local-sound.md) serves UI
import 32. Outcast's retail client game plays chat on channel 6 and its
announcer on 7, Quake III's numbering, and its engine starts every one-shot
at one level, so a menu sound reaches Fn's interface entry with the module's
own channel at the full level. With no input, the menus' `play` reaches it on
channel 0, and their focus sound on 6 in a diagnostic build. fnTech3 did not then
register its MP3 codec for either Outcast profile, so 7,987 of the
installation's 8,296 sounds, the menus' included, failed to load, an audio
blocker the MP3 record below resolves. No gate closes.

The [2026-10-08 Outcast MP3 sounds record](2026-10-08-jk2-mp3-sounds.md)
registers the MP3 codec for both Outcast profiles and loads each sound as
both retail engines do: the extension the game names is dropped, and
`<name>.wav` is read before `<name>.mp3`, so a `.wav` beside an `.mp3` wins
whichever the name says (Source, Unit). All 8,126 of the installation's MP3
files decode through Fn's decoder; this slice reproduced 7,970 of Raven's 7,971
stored sizes (Asset subset); the [last-frame correction](2026-10-08-mp3-last-frame.md)
then reproduced all 7,971. Without input, the multiplayer menus' focus and
`play` sounds load from their `.mp3` and play through both mixers, an `.mp3`
name whose only file is a `.wav` loads that `.wav`, and an MP3 music track
opens and decodes; the build before the change loaded none of them (Live
subset, a client at its main menu). The foreign-voice slice below serves
German and French; single player's live sounds remain open. No gate closes.

The [2026-10-08 Outcast foreign voices record](2026-10-08-jk2-foreign-voices.md)
registers `s_language` for both Outcast profiles as both retail engines do,
`english`, archived and kept by `cvar_restart`, and loads each voice as they
do: when it is `deutsch` or `francais`, the first `chars` in a sound's name
becomes `chr_d` or `chr_f`, read `.wav` then `.mp3`, before the English
recording (Source, Unit). Without input, a multiplayer client at its main
menu loads Mon Mothma's announcer line and three other voices from the German
and French folders and the English recording where a language has none,
through both mixers, and changes language through the setup menu's own
`ui_load ; snd_restart`; the build before the change loaded English
throughout (Live subset). Single player's voices have unit evidence only, and
its cinematic voice track and German dismemberment setting remain open. No
gate closes.

The [broadcast and level-time slice](2026-10-15-jedi-broadcast-level-time.md)
(2026-10-15) gives Outcast's server the rule retail `jk2mp.exe` 1.02a walks
(`0x44fcf0`): no test of `0x2`, which fnTech3 had read as Quake III's client
mask, and a two-word mask at `+104` in the engine's half that sends an
entity past every visibility test (`0x44fe1a`). The retail module fills it
for Force Sight and the Jedi Master, so those players now reach the clients
it names when out of their view. The engine half's readable prefix grows to
112 bytes, a 408-byte record; the retail QVM's records are 828 bytes and
still bind, running Jedi Master with two bots on the new server (Live
subset). A map change also keeps the level time, as `jk2mp.exe` runs its
game on `svs.time` (`0x44cf54`). Outcast's retail server stops at its disc
check, so there is no retail runtime comparison (Source, Unit). No gate
closes.

The [sun keyword slice](2026-10-15-raven-sun-keyword.md) (2026-10-15) has
both Outcast profiles read Raven's general `sun` as `jk2mp.exe` and
`jk2sp.exe` do (`0x48ccbc`, `0x47cab4`): six words into the sky sun, with
the rest of the line left to the keyword loop. With it they read `noTC` as
Academy's profiles do (`0x48c80b`, `0x47c7a9`), and Raven's `lightColor`,
which `jk2sp.exe` skips (`0x47c701`) and `jk2mp.exe` reads as a bracketed
vector, failing the shader when it is malformed (`0x48c697`). Through the
production parser, all 11 installed `sun` skies and 10 more definitions
with `noTC` now load in GLx, Vulkan and RTX, and no other definition
changes (Source, Unit). No Outcast sky has been drawn live: the
multiplayer client still stops at client-game import 300 before a map is
drawn. No gate closes.

The [map shader flags slice](2026-10-15-raven-bsp-shader-flags.md)
(2026-10-15) reads Outcast's map flags with Raven's bits too. `jk2mp.exe`
and `jk2sp.exe` give sky, nodraw and translucent Academy's values
(infoparm tables `0x502200`, `0x4cf5a8`) and skip nodraw patches
(`0x471253`, `0x45e630`). Read with Quake III's bits, RTX had taken 635
surfaces in 9 installed Outcast maps as sky and 10,690 as light filters.
The shared RBSP reader now translates both words (Source, Unit). No
Outcast map has been drawn live: the multiplayer client still stops at
client-game import 300. No gate closes.

The [light styles slice](2026-10-15-raven-light-styles.md) (2026-10-15)
serves Outcast's light styles too, composed into the one light channel Fn
draws at the white colours retail starts with (Source, Unit). Of Outcast's
41 installed maps, 40 had been refused by the renderers' reader. 39 were
refused for light styles, among them the multiplayer `ffa_ns_streets`, and
one, `yavin_temple`, for 11 flares in a fog the map lacks, now drawn
unfogged. All 41 now project. No Outcast map has been drawn live: the
multiplayer client still stops at client-game import 300. No gate closes.

The [animMap variants record](2026-10-15-jk2-animmap-variants.md)
(2026-10-15) has both Outcast profiles read Raven's `oneshotanimMap` and
`clampanimMap` stage keywords, which fnTech3 read for Academy alone.
`jk2mp.exe` and `jk2sp.exe` send both to their `animMap` branch
(`0x48d97b`, `0x47d7ab`): `clampanimMap` loads its frames clamped,
`oneshotanimMap` sets a bundle byte on which the animated-image bind holds
the last frame instead of wrapping (`0x488389`, `0x478225`), and both keep
Quake III's 8 frames where Academy's keep 32 (`0x48d9ed`, `0x47d81d`).
Through the production parser, all 22 installed definitions that use them,
12 campaign doors, panels and screens and 10 explosions, now load in GLx,
Vulkan and RTX, and no other definition changes (Source, Unit).
`jk2sp.exe`'s `RF_SETANIMINDEX` frame override (`0x4781ee`) is not yet
served for `jk2-sp`. Nothing has been seen live: the multiplayer client
still stops at client-game import 300 before a map draws. No gate closes.

The [surface sprites slice](2026-10-15-jedi-surface-sprites.md)
(2026-10-15) has both Outcast profiles read `surfaceSprites` and its `ss`
words as `jk2mp.exe` and `jk2sp.exe` do (`0x48d712`, `0x47d542`), with the
same checks and defaults as Academy's engines and the record kept inside
the stage (`+0xf0`). The sprites draw from the second stage on
(`0x48867a`), and the surface asks for normals as retail marks it
(`0x48e7a8`). Through the production parser, all 22 installed sprite
definitions now load in GLx, Vulkan and RTX, and no other definition
changes (Source, Unit). No sprite has been drawn live under Outcast: the
multiplayer client still stops at client-game import 300, and the maps
that use them are single player's. No gate closes.

The [alphaFunc GE192 slice](2026-10-16-jedi-alphafunc-ge192.md)
(2026-10-16) has both Outcast profiles read `alphaFunc GE192` as
`jk2mp.exe` and `jk2sp.exe` do: state bit `0x80000000` (`0x48dc72`,
`0x47daa2`) and `glAlphaFunc( GL_GEQUAL, 0.75f )` (`0x46d3d8`,
`0x45c3f8`), carried in Fn's comparison-and-reference alpha test. Through
the production parser, the warning leaves all 15 installed definitions
that use it in GLx, Vulkan and RTX, and nothing else changes (Source,
Unit). Nothing has been seen live under Outcast. No gate closes.

Star Wars Jedi Knight II: Jedi Outcast is the seventh title, with two mode
profiles over one `base/` installation: `jk2-sp` and `jk2-mp`. Every contract
below is pinned against a legitimate retail 1.02a installation and its own two
engine binaries ([foundation record](2026-09-22-jk2-contracts.md), [records
record](2026-09-22-jk2-records.md)). The retail multiplayer module now runs a
retail level **refusing nothing**, holding `ffa_bespin` open through its frame
loop in the earlier bot probe. The October 2 slices add the
[protocol 15 codecs](2026-10-02-jk2-protocol-15.md),
[client and font boundary](2026-10-02-jk2-client-boundary.md), and
[SP bootstrap](2026-10-02-jk2-sp-bootstrap.md). These partial boundaries do
not establish a working client, campaign or live network interoperability.

| Gate | Evidence | Remaining requirement |
| --- | --- | --- |
| Profile / identity / discovery | Source, Unit, Live subset | Both profiles carry protocol numbers, configuration names, limits and module locations read from the retail binaries that enforce them: single player's loopback 40 and multiplayer's 15 from their own cvar registrations, 1,024 and 1,400 configstrings and 512 and 256 submodels from their own bounds checks, and 32 clients from multiplayer's own clamp. A legitimate 1.02a installation is recognized from `base/assets0.pk3`, which holds both configurations. The multiplayer profile listens on 28070 and sends to masters on 28060, as `jk2mp.exe` 1.02a does (0x44414a, 0x44e52c), where it had used Quake III's 27960 and 27950, and its info reply carries retail's `sv_allowAnonymous`; the retail client lists fnTech3's server. fnTech3's own client then stopped at UI import 200; it now draws the multiplayer menus and starts a local map until client-game import 300 ([second round](2026-10-07-jedi-compatibility-round2.md)), and the retail server needs the game disc, so no session with a retail peer has run ([legacy-server record](2026-10-05-legacy-server-hosting.md)). Its single player is game type 4: `spmap` selects it, `map` turns it into a free-for-all, and a client-hosted server answers no `getinfo`, `getstatus` or `getchallenge` while it runs, as `jk2mp.exe` does (`SV_Map_f` 0x447910, `SVC_Info` 0x44e7b0, `SV_GetChallenge` 0x448750, `SVC_Status` 0x44e5f0), where fnTech3 had taken 2, Jedi Master, for single player; live, the client executable's server answered in Jedi Master on `ffa_bespin` and kept quiet in single player ([Jedi listen-server record](2026-10-06-jedi-listen-server-game-types.md)). Other editions, a Steam installation and every platform but this one remain open. |
| Module ABI | Source, Unit, Live subset | Retail `jk2mp.exe` dispatches imports 0 to 599 with 221 handled, through a byte index table at `0x44cc50` and a jump table at `0x44c8d8`, in runs at 0, 100, 200, 300, 400 and 500. The first two runs are Quake III's own numbers, established by what each handler does rather than by resemblance. **The retail module can only ever ask for 97 of the 221**: a QVM reaches the engine by calling a negative address, so counting the negative calls in `vm/jk2mpgame.qvm` bounds this title's whole boundary ([record](2026-09-22-jk2-records.md)). Its entity state is now **frozen whole**: `jk2mp.exe`'s own delta table names every four-byte slot from 4 to 292 with no gap, and gives the width each is sent at, which types every member -- a member sent at zero bits is a float. `number` at 0 completes it, and the record ends at 296, the size already measured from the engine's brush-model writer by an unrelated means. `ping` is at 484, read out of the one place the engine writes it. Forty-four imports are served: Quake III's twelve arithmetic and memory calls, the thirty of the first run that carry no record of this title's, the array publisher, and the Ghoul2 model initializer at 588, whose handler was read on its own rather than taken from Academy's names -- **this title's Ghoul2 run is measurably not Academy's**, while its elementary-action and AI-state blocks agree with Academy's on every argument count. Its bolt queries 586 and 587, bone bend 591 and model copy 595 are now served as well, each read from its own handler and core ([server attachments record](2026-10-06-jedi-server-attachments.md)), and so are its clip setter 592, skeleton name 593, duplicate 596 and clean 599; the instance word the module passes back is read unconverted, as retail's handlers read it, 584's included, and its instance field at a QVM's 32-bit width ([server clips record](2026-10-07-jk2-server-clips.md)). Single player's API 6 table is now frozen at 103 imports (412 bytes), 76 export bytes and a 1,288-byte entity stride; the real retail DLL passes the standalone table probe. The integrated engine probe stops at unsupported SP client import 3 before Init/frame evidence ([bootstrap record](2026-10-02-jk2-sp-bootstrap.md)).
| Wire codec / demos | Source, Unit, retail binary-body observation | Protocol 15 has separate 73-field entity, 105-field player and keyed command codecs. Nine independent retail x86 emulator fixtures match the raw bodies; production raw/Huffman tests cover all bit alignments, truncation, malformed counts and transactional rollback. The profile selects its own qport/reliable-XOR transport, 16,384-byte message limit, 128-command ring, EOF 9, eight 2,048-byte download blocks and bare challenge/connect replies ([protocol record](2026-10-02-jk2-protocol-15.md)). These are body and source observations, not live packet exchange. Bidirectional retail interoperability, reliable commands, download completion and `.dm_15` playback remain open; SP loopback protocol 40 remains separate and unimplemented. |
| Filesystem / pure | Source, Unit, Live subset | A developer server mounts the installation's two paks, 14,685 files, and starts a retail map. Its pure gamestate budget measures Outcast's own entity encoding, which spends about two bytes more per item or mover baseline than the Quake Live measure it used, so a pak list the gamestate cannot hold is no longer published; `jk2mp.exe` publishes `sv_paks` without a budget ([census-gaps record](2026-10-15-census-small-gaps.md)). Pure admission, mod directories and downloads remain open. |
| Assets / collision | Source, Unit, Fixture, Live subset | All 1,557 of the installation's maps, models, skeletons, motion paths and scripts read through fnTech3's readers with no refusals: 41 `RBSP` version 1 maps, 71 Ghoul2 meshes, 21 skeletons, 68 ROFF paths and 1,356 ICARUS scripts, every format shared with Jedi Academy. Nothing is drawn, posed or moved. All 8,126 MP3 sounds and music tracks decode through Fn's MP3 decoder, which reproduces every one of the 7,971 sizes Raven's tags store, decoding each frame from its own header as Raven's engines do: `sound/weapons/force/drain.mp3`'s last frame, before a stray byte, had been dropped ([last-frame record](2026-10-08-mp3-last-frame.md)). |
| Server / gameplay | Source, Unit, Live subset | **Earlier evidence established bot spawning and item collection, not combat**: ninety seconds of `ffa_bespin` with two bots in it, both clients connecting, changing userinfo, entering, beginning and collecting seventeen items between them, with no unserved import and no error. The retail QVM loads out of the pak, initializes, publishes its arrays into the engine's checked view at its measured strides, registers its items and parses its 29 bots and 15 arenas. Its entity state, input frame and player state all convert through its own adapters, which reach none of Academy's records. Every number was read from its own engine's dispatcher rather than taken from Academy's table, and four things show why that mattered: **its world block is Quake III's numbering, which Academy's is not; its trace is Quake III's 56-byte record, which Academy's 48-byte one is not; its motion paths sit five numbers lower than Academy's; and its Ghoul2 run is different again** -- each established from the retail engine's own handlers, error strings and debug names. Its player state's four arrays were named by the engine as it walks them, with powerups read as long words because they are times. **688 of that record's 1,368 bytes have no name in any table read so far**: they remain unnamed spans. The October 2 adapter now retains all 1,368 bytes in engine-owned backing storage and overlays only named fields when writing; tests preserve unknown bytes through round trips ([protocol record](2026-10-02-jk2-protocol-15.md)). A trace whose pass entity is -1 is read as having none, which is the only defined reading; retail indexes its entity array at -1 instead. Its bots are declared from evidence too: the installation ships Raven's own `botfiles/*.jkb` and `bots.txt`, none of Quake III's AI configuration sources and no `.aas` file for any of its 41 maps. Starting a motion path stays refused for the same reason it does in Academy. A bot rings 128 reliable commands, as `jk2mp.exe` does, where it rang Quake III's 64 ([bot window record](2026-09-24-bot-reliable-windows.md)). No live network-client or retail snapshot exchange is established. A map load and an immediate `map_restart` run each settling frame, and the last one, at the current time before advancing it, as `jk2mp.exe` does, and so would a single-player map load, as `jk2sp.exe`'s does; disassembly and fixtures ([settle order record](2026-09-24-settle-frame-order.md)). `jk2mp.exe`'s Ghoul2 links, model order, attached roots, tag frame, bends, model copy and built-in `*default` skeleton were read and are Academy's, but it builds once per time, judged by model 0, answers each bolt with what that build kept, answers 587 from the last build without building, and places a bolt without normalizing its rows; its dedicated server never starts the renderer that registers its bone smoothing, and it needs the disc, so no retail answer was observed. The server's own store now does the same, with the `*default` skeleton, attachments and imports 586, 587, 591 and 595, and refuses with nothing written what it does not pose: a bolt no build has reached and REPLACE bends (Source, Unit; [server attachments record](2026-10-06-jedi-server-attachments.md)). The two-bot run above set up no client model, because fnTech3 read the module's Ghoul2 instance words as module addresses and 584 answered "no models" for the precached Kyle. **The retail module's client setup now completes**: 584 reads the word as retail does, and 592, 593, 596 and 599 are served with Outcast's own clip timing, read from its clip setter and bone transform, so each of two bots gets its own instance, hand and bone bolts, a looping clip, two bends and its saber. In 75 to 300 seconds of `ffa_bespin` and 180 of `duel_pit`, every per-frame 586 (the saber's `*flash`), 591 and 592 was served, and every 587 (`Motion`) but each bot's first ten, which ask for a bolt no build has reached and which retail answers from an uninitialized record (Live subset). The bots never fought, so the saber hit-location queries were not reached, and no retail answer was observed ([server clips record](2026-10-07-jk2-server-clips.md)). **Bot combat now has separate live evidence**: the QVM rejects a zero `s.solid`; JK2 BODY is `0x100`, not Q3's `0x02000000`. Correcting linking and temporary hull contents, and the owner offset from 392 to retail's 396, lets the bots acquire enemies and inflict damage. Retail-measured alternate attack 407 and force power 408 now set their action bits; snapshot 209 and AAS are not on this QVM's enemy-search path. An uninstrumented Windows x86 developer server logs three saber kills in 120 seconds on `duel_pit`, game type 3. A separate game-type-0 run with `g_dismember=100` logs three kills and three successful model-0 queries at QVM 129659 (`rtibia`, `lradius`, `cranium`). That call is on the saber death/dismemberment path; default `g_dismember=0` does not exercise it. Retail pose equality, other location bones, broad gameplay and retail sweep special cases remain open ([bot combat record](2026-10-07-jk2-bot-combat.md)).
| Client / renderer / audio / input | Source, Unit, Asset subset, Live subset | MP cgame/UI QVMs have separate ordinal dispatch and frozen gamestate, snapshot, user-command and renderer-config transfers. Cgame font imports 47–51 and UI 56–60 adapt all three shipped `.fontdat` atlases through FnQ3/FnQL shader and HUD/menu drawing; bounded parser and transfer tests pass ([client record](2026-10-02-jk2-client-boundary.md)). No game map has been visually verified. The menus' pictures, fonts and 3D views are Raven's virtual 640x480 screen, as `jk2mp.exe` stretches it (`0x46da51..0x46da9a`); a 1680x720 engine capture shows the main menu filling the window, stretched by default and centred with `cl_menuAspect 1` ([widescreen record](2026-10-15-jedi-widescreen-hud.md)). Remaining imports refuse explicitly. UI import 32 plays the menus' sounds as Fn interface audio on the module's own channel at the profile's one-shot level: Outcast numbers its channels as Quake III does, chat on 6 and the announcer on 7 in its retail client game; live without input, the menus' `play` reaches it ([UI local sound record](2026-10-08-jk2-ui-local-sound.md)). Both profiles decode the installation's MP3 sounds and music, reading a sound's `.wav` before its `.mp3` whatever extension the game names, as both retail engines do; live, the menus' sounds load from their `.mp3` and a music track decodes, and all 8,126 MP3 files decode ([MP3 record](2026-10-08-jk2-mp3-sounds.md)). With `s_language` `deutsch` or `francais`, both profiles play the German or French voices, falling back to English, as both retail engines do; live, Mon Mothma's announcer line and other voices load in each language ([foreign voices record](2026-10-08-jk2-foreign-voices.md)). `+altattack` is button 7 and the any-key bit is Raven's `0x100`, as `jk2mp.exe` 1.02a registers and sets them ([weapon fire record](2026-10-15-jka-weapon-fire.md)). The client game's collision, sound, registration, scene, 2D, input-value, parser, cinematic and ROFF-lane imports follow their `jk2mp.exe` handlers, its traces hand over retail's 1080-byte record, and its effects play through the effects engine built for Academy with Outcast's own traces, decals and clock; live, a local `ffa_bespin` start loads the world, registers 22 effect files and stops at client-game import 285, Ghoul2, before a frame is drawn ([client services record](2026-10-16-jk2-cgame-services.md)). Localization, menu/key services, Raven RBSP world lighting, render entities other than plain models, Ghoul2, the direct effect primitives and ambient definitions remain open; the world/audio paths must adopt the engine feature set. SP client/menu paths remain separate. |
| Savegames | None | Single player's `.sav` format is unimplemented. |
| Optional services | Unavailable | No service success is synthesized. |

Both Outcast profiles are available in every build under the
[availability policy](2026-10-11-profile-availability.md); the legacy
`-Djk2-development` option is accepted as a no-op and closes no release gate.



## Alice gate matrix

The [2026-10-02 UberTools round](2026-10-02-ubertools-round.md) adds native
observations and production regressions for the 1,328-configstring module
boundary and overflow-safe light styles, and malformed model-light checks in
strict and fast-math builds. Valid-data presentation uses the shared engine
features; this does not establish retail gameplay or visual parity.

The [map shader flag dialects slice](2026-10-15-map-shader-flag-dialects.md)
(2026-10-15) reads Alice's map flags with `alice.exe`'s own values. Its
infoparm table (`0x5294d0`) keeps sky, nodraw and translucency in Quake III's
places, but puts `alphashadow` at `0x800` and `wood` at Quake III's
alphashadow bit, `0x10000`. RTX read the light filters with Quake III's
values; each renderer now translates the map's surface flags right after
loading them (Source, Unit). No installed Alice map has a `0x10000` record,
and its 37 `alphashadow` records now filter the path tracer's light. No
Alice map was run live for this. No gate closes.

American McGee's Alice is the eighth title and the thirteenth profile,
`alice`, over one `base/` installation. It is the only supported title outside
the Quake III line: Alice runs Ritual Entertainment's Heavy Metal: F.A.K.K.2
engine, so almost none of the shared contracts apply to it unchanged. The
[foundation record](2026-09-22-alice-contracts.md) pins the installation
identity and the startup constants against a legitimate retail November 2000
installation. Every contract below was read out of the retail `alice.exe`,
`fgamex86.dll` or `cgamex86.dll` and is cited by address in the record linked
beside it.

**Observed.** In developer builds (`-Dalice-development=true`, Windows x86)
the retail game module and client game run together in one client. `garden1`
loads, plays its opening scene and then draws Alice in third person with her
HUD's gauges and foldouts; a save made in play loads back and play resumes;
the intro's logos lead to the engine's own menus, and New Game's command plays
`opening.roq` and then loads `gvillage`; demos record and play back; and the
GL, Vulkan and RTX renderers draw `garden1` alike. Every client run was
windowed at 800x600 with an isolated home path and no mouse or keyboard
input, its frames
were taken with the engine's own screenshot command, and the installation was
left unchanged: the probes' manifests record it, and on 2026-09-24 no file or
directory in the installation had been modified this month. The rows below
say where each was observed.

**Not established.** No retail peer has taken part in any of it. No session
has run between retail `alice.exe` and fnTech3 in either direction, no
retail-written save or retail-recorded demo has been read, and no fnTech3
frame has been set beside one retail drew. Alice ships no multiplayer, so the
only channel exercised is loopback. Every gate below therefore stays partial,
and the release gate stays closed.

| Gate | Evidence | Remaining requirement |
| --- | --- | --- |
| Profile / identity / discovery | PARTIAL -- Source, Unit, Live subset | The profile, its three aliases, its limits and its single-mode title pass unit checks, and `base/pak0.pk3`'s pinned identity -- `default.cfg` at 1,322 bytes with CRC `3781148b` and `maps/garden1.bsp` at 6,323,368 bytes with CRC `cb85fdae` -- distinguishes a retail installation from every other supported title's. Protocol 1 is admitted by Alice and F.A.K.K.2 and refused by the other profiles. Developer dedicated servers and clients start under the profile against a legitimate retail installation, write only into an isolated home path and shut down cleanly ([foundation record](2026-09-22-alice-contracts.md), [map record](2026-09-22-alice-maps.md)). `sv_maxclients` defaults to 1, which is what retail's `SV_Init` registers (`"1"`, serverinfo and latched, `0x0045566b..0x0045567c`); the 8 read earlier at `0x00454e0c` is inferred to apply only to a cvar not yet registered (`code/qcommon/game_profile.cpp`, and the correction in the [foundation record](2026-09-22-alice-contracts.md)). Other editions, the Mac release, the later digital re-releases and every platform but Windows x86 remain open. |
| Module ABI | PARTIAL -- Source, Unit, Live subset | The four table lengths are measured in the retail libraries and frozen -- 111 game imports, 26 game exports with API version 4 and a 732-byte entity record, 125 client-game imports and 7 client-game exports -- and every slot of all four tables is mapped from the engine's and the modules' own code: the game's in `code/compat/alice_game_imports.def` and `alice_game_exports.def`, the client game's in `alice_cgame_imports.def` and `alice_cgame_exports.def`, held to each other and to the engine's own table builds by `tests/alice_module_abi_tests.cpp` ([module record](2026-09-22-alice-module-abi.md), [client-game record](2026-09-23-alice-cgame.md)). Both libraries load through the function-table path (`VM_CreateTable`), and the engine checks what each publishes before calling it. **110 of the 111 game imports are served**; the other, slot 100, is the one retail's own table build leaves null and the module never calls (`code/server/server_alice_game.hpp`). **119 of the 125 client-game imports are served** (`code/client/client_alice_cgame.hpp`); the six that are not -- 0, 48, 49, 73, 88 and 116 -- are all slots the module never calls, and 0, 88 and 116 are the three retail's builder never writes. Both counts are printed at load ([client-game record](2026-09-23-alice-cgame.md)). Every unserved slot holds a thunk that drops by name, so no null is ever handed over. The client game's exports are called where retail calls them, its initializer with the import table, the server message sequence and the last executed server command. **Every slot's signature is frozen**: all 269 slots carry a fixed-width type read from the three retail binaries (`code/compat/alice_*_sigs.def`), and every function served into a slot and every type an export is called through must fit its slot's type to compile; the check found and corrected five game imports served at the wrong shape, none of which had misbehaved ([signatures record](2026-10-08-alice-module-signatures.md)). `CalcCRC` (game slot 99) is served and unit-tested; under a debugger no fresh level, level change or save load of the shipped game reaches the module's one routine that calls it ([round record](2026-10-08-alice-compatibility-round.md#calccrc)). |
| Wire codec / demos | PARTIAL -- Source, Unit, Live subset | **The netchan is retail's** ([wire record](2026-09-22-alice-wire.md)): Quake III's `netchan_t` and fragmenting at 1,400-byte packets and 1,300-byte fragments, but with each fragment's offset a long, so messages can pass 32 KB (`0x0044710e`, `0x00447423`), a one-sided qport, no sequence checksum or reliable-command XOR, the packet scramble Elite Force Holomatch uses step for step, and no Huffman stage. Its limits are Alice's own: 48 KB messages, 128 reliable commands of 1,024 bytes, 32 packet backups, 64 command backups and 32 usercmds of 20 bytes. There are ten server opcodes, no `svc_EOF`, no download path and no pure exchange. **The message bodies are encoded and decoded in Ritual's shape**: 79 entity and 49 player fields walked out of the engine's own delta tables and frozen by a generator, a thirty-one entry dictionary of change vectors, a masked 32-byte entity block, six 32-entry player arrays read as signed shorts ([HUD record](2026-09-24-alice-hud.md)), Ritual's float rule and an unkeyed user command, all held to their measured costs by `tests/alice_message_codec_tests.cpp`; every baseline deltas from Alice's own null entity (`0x00446460`). **The snapshot walk is retail's** (`code/server/server_alice_snapshot.hpp`, [snapshot](2026-09-22-alice-wire.md#the-snapshot)): the client's own entity first, a child judged by its attachment or bind parent, a portal's second walk, a sky origin's walk after the main view, the far-plane cull, and the portal, sky and dual flags rewritten on each add, with a frame for each client. On `garden1` the client game's `Could not find parent entity` warnings go from 2,998, with a temporary switch back to the Quake III walk, to none in the same binary, and from 3,604 in an earlier build to none in a later one. **Demos are recorded and played as `alice.exe` does** ([demos](2026-09-22-alice-wire.md#demos)): Quake III's container under `demos/<name>.dm3`, `record` refused without `g_syncronousClients`, and no recording notice; a 158-record `garden1` recording replays in a separate windowed client, and `timedemo 1` plays it in 198 frames. **A snapshot carries the server's time residual** after its time, which `alice.exe` writes (`0x00456c63..0x00456c7b`) and reads (`0x0042dc6e`). fnTech3 had sent and read neither, consistent only with itself. Retail F.A.K.K.2's reader, of the same shape, exposed the gap. Both sides now carry it ([wire record](2026-09-22-alice-wire.md#the-time-residual)). **The connect is plain text**, protocol 1, answered by a bare `connectResponse`, as Ritual's engine has it (`alice.exe` has no Huffman stage; `fakk2.exe` 1.02 shows the whole exchange). fnTech3's server had Huffman-decoded it and its client had asked for protocol 68. Both now follow retail under the two Ritual profiles, and a local `garden1` game still starts. A client that leaves is announced with `print "%s %s\n"` and sent a bare `disconnect`, as `alice.exe` does (`0x004529fb`, `0x00452a6b`), where fnTech3 had sent Quake III's forms ([drop notice record](2026-09-24-rtcw-et-drop-notices.md)); a client on the Ritual wire rings the profile's reliable window, which is Alice's 128. **The infostrings are retail's**: under both Ritual profiles the systeminfo holds `cheats`, `sv_serverid`, `sv_pure`, `sv_paks` and `timescale`, the serverinfo none of this engine's own keys, and the client takes the cheat state from `cheats` as retail's does; a retail F.A.K.K.2 1.02 client joins an fnTech3 server with none of fnTech3's keys reaching it ([round record](2026-10-08-alice-compatibility-round.md#infostrings)). No Alice packet has been exchanged with any peer but fnTech3's own loopback server, no demo has crossed between fnTech3 and `alice.exe` in either direction, and no retail-recorded demo is available to play. |
| Filesystem / pure / downloads | PARTIAL -- Source, Live subset | Developer servers and clients mount all ten of the installation's paks, 14,818 files, and execute Ritual's own `default.cfg` out of them. Retail sorts `base/*.pk3` with a case-sensitive `strcmp` (`0x0043ea00`) and fnTech3 case-insensitively; on a retail installation both orders mount the same files, because no file in `Pak2` or `Pak3` is in another pak and the paks that do overlap sort alike under both rules ([client-game record](2026-09-23-alice-cgame.md#pak-order)). A `.tga` name is looked for as `.ftx` first, as retail's image loader does (`0x00483e80`). Saves are written into `fs_homepath`, where retail writes into its installation ([savegame record](2026-09-23-alice-savegames.md)). Alice registers `fs_basepath`, `fs_cdpath`, `fs_filedir` and `mapdir` and no `fs_homepath`. The download refusal now covers all ten retail paks: `pak0`, `Pak2` and `Pak3` through Quake's numbered-stem rule, and the seven suffixed ones -- `pak1_small`, `pak1_large`, the four `pak4_*` language sets and `pak5_dutch` -- through a rule of Alice's own, a numbered pak with a suffix after an underscore (`FS_OfficialPak`; Unit, `tests/profile_pak_download_tests.py`). No transfer has been attempted. **Pure servers are retail's**: `sv_pure` is registered in the systeminfo, a pure server lists its paks' checksums in `sv_paks`, the client keeps to the first 64 of them even from its own server and reads no loose file but configuration, savegames and their pictures, and neither side asks for or sends a pure proof, which neither retail engine has; a loose map loads with `sv_pure 0` and is refused with `sv_pure 1` ([round record](2026-10-08-alice-compatibility-round.md#pure-servers)). |
| Assets / collision | PARTIAL -- Source, Unit, Live subset | All thirty-six maps are ident `FAKK` version 42 and translate for collision -- 3,969 shaders, 342,548 planes, 62,793 brushes and 183,673 surfaces of which 6,666 are patches -- with eleven of the thirteen fixed-size records Quake III's byte for byte ([map record](2026-09-22-alice-maps.md)). The renderers' world load carries the light grid at Alice's 192x192x320 cell, with which Quake III's derivation reproduces the point count exactly on all thirty-six maps ([client-game record](2026-09-23-alice-cgame.md)); the renderers' translation drops the entity lights, their visibility and the light definitions (`code/qcommon/bsp_fakk.h`), and the renderers read the first two from the map themselves for model light ([model light record](2026-09-25-ritual-model-lights.md)); retail's world load never reads the light definitions at all, and a map's checksum is the one its header carries, as retail's `CM_LoadMap` returns it ([round record](2026-10-08-alice-compatibility-round.md#light-definitions)). The entity lights are read offline instead: their 60-byte record is the one `alice.exe`'s loader copies (`0x0049a360`), checked field by field against the source entities on all thirty-six maps, and each map's are converted into the static light file the package now ships beside its audio zones ([sidecar record](2026-09-24-alice-map-sidecars.md)). All four model formats read -- 694 `TAN `, 79 `SKL `, 1,402 `SKAN` and 801 `.tik` ([model record](2026-09-22-alice-models.md)) -- and the 797 `.cik` files are compiled definitions that the retail loader prefers to the text. One TIKI runtime per process loads them, compiles the text where there is none, and answers the queries both modules make, with retail's pose, skinning and tag arithmetic ([TIKI record](2026-09-23-alice-tiki.md)). Ritual's shader dialect goes through all three renderers' parsers: of the 2,162 shaders in the installation's 52 scripts, only the 23 stageless compiler shaders that Quake III's own rule also rejects fail, in each parser (`tests/alice_shader_parse_tests.py`). The twelve `.urc` menus read, 401 widgets (`tests/alice_urc_tests.cpp`), and so do the fourteen `.ritualfont` fonts (`tests/alice_ritualfont_tests.cpp`). The level scripts, camera paths, path nodes and dialog files are read by the retail game module itself, through the file imports. |
| Server / gameplay | PARTIAL -- Live subset | A developer server runs the retail game module on every one of the thirty-six retail maps: it spawns the level's own entities (685 spawned and 19 inhibited on `garden1`), runs the map's scripts in its own interpreter, and its entities are linked into a world that the area query, `ClipToEntity` and the trace reach ([module record](2026-09-22-alice-module-abi.md)). That sweep ran two hundred server frames a map on a build whose `wait` still counted frames; Alice's `wait` now counts milliseconds in the client's pass alone, as retail's does, so a dedicated repeat needs one-frame waits (`tests/alice_command_wait_tests.py`). Linked entities record their areas and clusters, so they reach snapshots -- 195 of 274 on `garden1`, where none had -- and slot 39 opens the portal between an entity's two areas as `0x004536d0` does. A close that takes a portal's count below zero is told to a developer and the level goes on, the count kept below zero, as `alice.exe`'s `CM_AdjustAreaPortalState` has it (`0x004361a0`). Quake III's drop there had stopped `wforest` entered at its second start, as the campaign enters it from `jlair2`, and `wchess1` with its intro skipped early; a developer client now loads all thirty-six maps ([area portal record](2026-09-27-alice-area-portal-counts.md)). The level flow is retail's: `map`, `devmap`, `gamemap`, `restart`, `pause`, `savegame`, `autosavegame`, `loadgame` and `loadbrokengame` replace the Quake III set, with retail's spawn order and three settle frames ([savegame record](2026-09-23-alice-savegames.md)). In a developer client on `garden1` the module's own `skipcinematic` ends the opening scene, and weapons and powerups given from the console are wielded and shown on the HUD ([HUD record](2026-09-24-alice-hud.md)). `sv_maxclients` defaults to 1 and clamps at 128, `MAX_CONFIGSTRINGS` is 1,328 and `MAX_SUBMODELS` 512. There is no bot library: the maps carry 5,365 designer-placed `info_pathnode` entities for the module's own AI and no `.aas` file ([map record](2026-09-22-alice-maps.md)). **A load or a restart of the running level keeps the game module**, as retail's reuse path does: its model table, configstrings and clients stay, the module only cleans up, and the client takes the new server count as retail's does; a full spawn clears the per-actor dialog history as every retail spawn does. **Death, combat and a level exit play without input**: a forced attack, `kill` and a load back, a mallet wielded and swung, the `garden1` exit reached by placing Alice in it, and a save made on `garden1` loaded from `garden2` through the load menu's own command. The players start at configstring 896. Alice's remaining area-portal closes are traced, three of them played ([round record](2026-10-08-alice-compatibility-round.md)). Movement and firing by the player's own buttons have not been played: they need synthesized input. |
| Client / renderer / audio / input | PARTIAL -- Live subset | **The client plays the opening levels.** With the retail client game running, `garden1` loads, plays its letterboxed opening, and after `skipcinematic` draws Alice in third person with AliceHud's eight children -- the health and mana gauges and the weapon and powerup foldouts, with their fold, card swap, dice count and powerup timer ([HUD record](2026-09-24-alice-hud.md), [snapshot evidence](2026-09-22-alice-wire.md#the-snapshot)). **The menus are the engine's own**, hosted as the client's UI module: the twelve `.urc` menus load and every widget class they use is served -- Label, Button, CheckBox, Slider, Field, List, FakkBindList, FakkLoadGameClass, AutoScroll and Alice3D -- with retail's menu cvar rules, pause and Escape order, and the UI's commands registered once so that they outlive the UI's restarts. The main, settings, key binding, credits, quit, load/save, `zound` and `GameCamera` menus were drawn at the main menu and in play ([menu record](2026-09-24-alice-menus.md)). **The intro and cinematics run**: in one windowed client (`.tmp/alice-cin/run4`, built in `meson/build/win32-alice-cin`, executable hash not recorded), whose probe chained its steps through `nextmap`, the intro played `ealogo.roq` and `roguelogo.roq` and brought up the main menu; the New Game button's own command, entered from the console, played `opening.roq`, whose end loaded `gvillage` and drew its opening camera. `tests/alice_cinematic_tests.py` holds those steps to retail's `cinematic`, `CIN_Play`, `CIN_Finish`, `CIN_Stop` and `_intro`. An in-game `cinematic` shuts the local server down and keeps the local client, as retail's `SV_Shutdown` (`0x00455910`) does: on `garden1` the game's own ending steps played `ending.roq` and brought up the credits over the main menu, and a `nextmap` naming a level loaded it after an in-game movie (`2026-09-24-engine-input-capture-fixes.md`). **All three renderers draw Alice's levels**, with the light grid, FTX textures, TIKI models and Ritual's shader dialect: five `garden1` captures from one probe script, run on GL and seven minutes later on Vulkan and RTX from `meson/build/win32-alice-vk` (`.tmp/alice-vkshade/rx-glx`, `rx2-vk` and `rx2-rtx`, JPEG, executable hashes not recorded), differ from GL's by a mean of 2.97/255 to 3.05/255 a channel on four of them and 4.3/255 on the lens-flare capture, and Vulkan's from RTX's by 0.45/255 to 1.28/255; and `tests/alice_renderer_parity_tests.py` keeps the Vulkan and RTX front ends' Alice functions identical to GL's. **No overbright bits**, the `0` `alice.exe` registers for both `r_overBrightBits` and `r_mapOverBrightBits` (`0x00475136`, `0x0047529e`): retail shifts lightmaps, vertex colours and the light grid by its overbright bits alone (`0x0049ba40`), so each map's light draws as the map holds it. fnTech3's usual one bit over two had drawn `garden1`, `utemple` and `centipede1` 1.96, 2.22 and 2.68 times as bright on GL and on Vulkan ([overbright record](2026-09-25-alice-overbright.md); `tests/alice_overbright_tests.py`). A lightmap stage that names no `rgbGen` takes identity, as `alice.exe`'s parser gives it (`0x0048c648`); every stage Alice ships was identity already. Characters and weapons, which the game module flags `RF_EXTRALIGHT`, are lit from the map's compiled lights by `alice.exe`'s law, fitted to the renderers' own model light, and other models from the grid as the map holds it; on `utemple` the grid had drawn Alice black ([model light record](2026-09-25-ritual-model-lights.md); `tests/ritual_model_lights_tests.py`). The grid's ambient takes the `0.5` `alice.exe` registers for `r_ambientScale` (`0x004754ed`), not Quake III's `0.6`, which had drawn the props it lights 2% to 14% brighter at the same light on six opening views ([ambient scale record](2026-09-25-ambient-scale.md); `tests/ambient_scale_defaults_tests.py`). **Sound and music are served**: the client game's sound imports (30 to 41) and music imports (42 to 44) reach fnTech3's own Alice sound and soundtrack engines ([audio record](2026-09-23-alice-audio.md); `CL_AliceFillCGameImports` in `code/client/client_alice_cgame.hpp`), and in the intro run above each logo printed `Playing mp3`, the main menu loaded `sound/music/intro.mus`, and `gvillage` loaded its soundtrack and set its moods (`.tmp/alice-cin/run4/base/qconsole.log`). No listening test has been made, that run's audio was muted by the unfocused window, and every other run above had sound off. **Map sidecars**: each official map ships a generated audio zone file and static light file, served from the package; the zone compiler reads FAKK 42 with Ritual's flags from `alice.exe`'s own `surfaceparm` table (`0x005294d0`). The light files load in all three renderers and add up to eight dynamic lights a scene by default, keeping their balance with no overbright bits, and the zones give the OpenAL backend its room ([sidecar record](2026-09-24-alice-map-sidecars.md)). **Alice's voices are rendered by the engine's own audio pipeline**, not a copy of Miles: the OpenAL distance model with each sound's own min distance, air absorption, traced occlusion, zone or traced room reverb, tone classes by Ritual's channels, and HRTF, while the engine keeps retail's voice bookkeeping; a room the client game names replaces the zones while it is named, and the legacy mixer keeps retail's gain and pan. Static models compiled into the world whose shaders say `rgbGen lightingDiffuse` (745 surfaces on six maps, 670 of them on `utemple`) had drawn black, as the world entity gives them no light; all three renderers now light them with the colours their map baked on the vertices, and `utemple`'s kelp draws lit (found on F.A.K.K.2, [record](2026-09-24-fakk2-contracts.md); `tests/ritual_world_diffuse_tests.py`). The far-plane fog of `centipede1` and `utemple` now fogs an opaque surface once, as `alice.exe` does: its stages unfogged, then one pass of the fog colour with alpha min(|v - eye| / distance, 1) (0x0049ebb0, RB_BeginSurface 0x0049286e), where fogging each stage with RTCW's white neutral had brightened distant walls; while the far fog is on, no fog volume is drawn either (0x004927d3..0x004927ff; `tests/ritual_far_fog_tests.py`). Their first views moved by 0.5 of 255 on average. On `garden1` the level's sounds carry reverb sends and occlusion ([audio record](2026-09-23-alice-audio.md#the-engines-own-features)); no listening test has been made. **Input**: the user command follows retail's rules in unit tests (`tests/alice_input_tests.cpp`), but no run has been driven with mouse or keyboard input. One cinematic run started on the interactive desktop (`.tmp/alice-cin/run3`) received three presses routed to Escape that its probe never sent, which a window there can get from the desktop's own keyboard; the engine invents none, and the same flow on a private desktop played to its end (`2026-09-24-engine-input-capture-fixes.md`). No fnTech3 frame has been set beside retail's. The developer pickers (`soundpicker`, `viewspawnlist`, `maplist`), the scrollbars' arrow repeat, the controls menu's head turn, `loadmenu` and, under Alice, the inventory commands are served; a video restart leaves the menus and the console as they were; a new server count stops every sound but the music and cancels the fade; and the HUD draws in its builder's order on the GL, Vulkan and RTX renderers ([round record](2026-10-08-alice-compatibility-round.md#menus-and-the-engine-ui)). |
| Savegames | PARTIAL -- Source, Unit, Live subset | **Saves are written and read back on developer servers and clients** ([savegame record](2026-09-23-alice-savegames.md)). Retail's four files per save, `save/<config>_<name>.sav`, `.spv`, `.ssv` and `.tga`, go into the home path, never the installation. The engine writes retail's 200-byte header, which has no magic, and the area portal block behind it; the game module writes its level and persistent archives through exports 16 to 19 at the shapes retail's call sites push; and the grey 256 by 256 picture equals retail's rule applied to the engine's `screenshot` of the same frame in 65,536 of 65,536 pixels. A load puts the portal block back whole, a count below zero included, as `alice.exe`'s `CM_ReadPortalState` does (`0x004362e0`), where fnTech3 had refused such a save ([area portal record](2026-09-27-alice-area-portal-counts.md)). The level flow follows retail's order: on `garden2` a level archived at 3,250 ms reads back at 3,250 ms and runs on, `gamemap` carries the persistent file into the next level and its queued autosave writes `garden20000` as `Starting Herbaceous Border`, and that save loads in a process with no map running. A load or a restart of the running level keeps the game module, as retail's reuse path does: on `garden1` a save made 12 s into play and loaded 3 s later reads back at level time 12,300 and play resumes ([round record](2026-10-08-alice-compatibility-round.md#a-load-or-a-restart-of-the-running-level)). `map`, `loadgame` and `loadbrokengame` stop a playing cinematic where retail does (`0x00451118`, `0x00451b09`, `0x00451c53`). The load/save menu lists saves from their headers without `quicksave`, saves from the list and closes, and asks before deleting ([menu record](2026-09-24-alice-menus.md)); its own command loads a save made on another level and reads it back. **Open:** no retail-written save has been read and no fnTech3 save has been loaded by retail; a load from a fresh process, or of another map, reads model indexes back as numbers, and on `garden1` the client game then reports `Can't find animation 1` every frame, which by the record's reading retail's full path shares but no retail run confirms; and nothing shipped reaches the module's one `CalcCRC` call. |
| Optional services | Unavailable | No service success is synthesized. Ritual's `fakkmaster.ritual.com` master server has been gone for years; no master query is made. |

Every build includes this profile; use isolated writable homes for probes.
Availability closes no compatibility gate, and no other title's evidence
is Alice evidence.


## F.A.K.K.2 gate matrix

The [2026-10-02 UberTools round](2026-10-02-ubertools-round.md) adds direct
1.02 executable observations and production regressions for this profile's
1,200-configstring boundary, plus malformed lighting checks shared with Alice.
Its prior first-level reverse retail-server and save evidence remains limited
to the scope recorded below; this round does not repeat retail interop.

The [map shader flag dialects slice](2026-10-15-map-shader-flag-dialects.md)
(2026-10-15) reads F.A.K.K.2's map flags with Alice's values (`alice.exe`
`0x5294d0`; Ritual's SDK agrees, **[I]** for the encrypted `fakk2.exe`):
`0x10000` is `wood`, not Quake III's `alphashadow`. RTX had taken 766
installed wood records for light filters. Each renderer now translates the
map's surface flags right after loading them (Source, Unit). Under RTX,
`swamp3` filters the path tracer's light through no triangle where 78,482
did, and traces 126,005 triangles as opaque where it traced 47,523; its
captured view, by the water, is unchanged (Live subset). No gate closes.

Heavy Metal: F.A.K.K.2 is the ninth title and the fourteenth profile, `fakk2`,
over one `fakk/` installation. It runs Ritual's own engine, the parent of
Alice's, and fnTech3 serves it through the Alice adapters with F.A.K.K.2's
records crossing at the module boundary. The
[contracts record](2026-09-24-fakk2-contracts.md) pins the installation and the
evidence. The disc's `fakk2.exe` is LaserLock-encrypted. The official 1.02
patch's is plain, and the same build as the retail modules. It was read and
runs here as a client. Its server starts only with the CD: with an image of the
user's disc copy mounted and the machine key it reads set, both with the
user's approval, it hosted fnTech3's client. So
the engine's behaviour is read from that executable where it has been checked,
and otherwise taken from `alice.exe`, with every such inference marked in the
record.

**Observed.** In developer builds (`-Dfakk2-development=true`, Windows x86,
`meson/build/win32-fakk2`) against an installation laid down from the user's
retail disc: a dedicated server spawns all thirty maps through the retail game
module and quits cleanly, and a client enters all thirty; a client draws the main menu from the retail `.urc`
files, plays `fakkhouse`'s opening under its sky portal, saves and loads back,
and records a demo and plays it back. In play its HUD menus draw the health,
water, ammunition and boss bars and the weapons' models, its inventory opens
from `warpinv` with the held items' models, and `saveweaps` keeps a weapon group
that survives a restart. A level exit fired from the game script carries the
player to the next level with the weapon in hand and autosaves there. Every
client run was windowed at 800x600 on a private desktop with an isolated home
path. Its frames were taken with the engine's own `screenshotPNG`, and each
probe's manifest records the installation unchanged. The user's keyboard,
mouse and desktop were never used. The runs that play by input queued their
keys, characters and motion inside the engine (`in_synthesize`, enabled only
by `+set in_synthetic 1` on the command line, through
`scripts/probe_ritual_client.py --actions`). What they show:

- **Menus.** Hover sounds play, and clicks open the menus. A pulldown's list
  opens, is dragged to "Low" and sets `r_picmip 2`. The graphics report's text
  is selected by a drag and by Shift with the arrows.
- **Save list.** Menu saves are made, rows are chosen by click and by
  Up/Home/End, a save is removed by Delete, and Enter loads it.
- **Inventory.** The cursor mode opens on I, types and rows are picked by
  hover and click, and an item goes into Julie's left hand.
- **Play.** On `fakkhouse`, W, A, S, Space and mouse turns move Julie, MOUSE1
  fires the handgun and CTRL swings the sword.

**Compared with retail.** Retail `fakk2.exe` 1.02 plays an fnTech3 demo of
`fakkhouse` end to end. Frame by frame (`cl_avidemo`), it draws what fnTech3
draws. The comparison found three differences and each was corrected from the
executable: the message bodies and a snapshot byte, the HUD's rule, and the
sprites of hidden emitters. Its main menu has the same content, drawn 1:1
where fnTech3 scales it. Its `shortversion` is "1.02", which fnTech3 now
registers too. Its save pictures are in colour, and fnTech3's now are as
well. **A live session:** retail's client joins an fnTech3 dedicated server
over loopback UDP and plays. The server sees its movement, turns, fire
buttons and commands. That needed Ritual's handshake, a plain-text `connect`
with protocol 1 and a bare `connectResponse`, which fnTech3 had never run
over a network. With `g_syncronousClients 1`, retail records a demo there,
and fnTech3 plays it back as retail does, down to the same 302 warnings.

**The other way.** On 2026-09-25, with the user's approval, an image of the
user's disc copy was mounted as a CD-ROM drive, and the machine key retail's
server start reads (`Software\Ritual\FAKK2`, `PATH`) was pointed at it.
Retail then ran `fakkhouse` itself and as a dedicated server:
- **Network.** fnTech3's client joined that server over loopback UDP, played
  the opening cinematic, and played. Holding W moved Julie to the point
  fnTech3's own server puts her, within 0.01 units by retail's `whatis`, and
  its buttons armed and swung the sword.
- **Saves.** Saves crossed both ways, at the opening and armed with the sword
  in the right hand: fnTech3 loaded retail's to the same view, and retail
  loaded fnTech3's.

The key was removed and the image unmounted afterwards.

**Not established.** Every gate below stays partial, and the release gate
stays closed.

| Gate | Evidence | Remaining requirement |
| --- | --- | --- |
| Profile / identity / discovery | PARTIAL -- Source, Unit, Live subset | The profile, its three aliases and its limits pass unit checks. `default.cfg` ships loose rather than in a pak, so the installation is named by `fakk/pak0.pk3`'s `menu.cfg` (413 bytes, CRC `609b5da3`) and `maps/fakkhouse.bsp` (4,700,748 bytes, CRC `b72ef3a6`). Protocol 1 is admitted by Alice and F.A.K.K.2 and refused by the other profiles. A copy of the retail disc is refused with a message naming the missing `fakk/gamex86.dll` and the installer. A German installation, from the same disc's `deu` pak, is the same profile: its localized menus draw, `localized.cfg` runs and its voices play. The German pak's stray `fakk/` lip-sync folder holds English leftovers that retail cannot reach either: `fakk2.exe` 1.02 asks for a sound's lip-sync at the sound's own path (0x00432d70), and its filesystem did not find a test pak's `fakk/strayprobe.cfg` by its bare name. The 1.02 patch changes only the executable and `pak2.pk3`, since its game libraries are the disc's. With that pak mounted, fnTech3 draws the patched advanced options menu with every control retail's shows, in the same state, but for the two documented presentation defaults. The disc's two libraries are Win32 x86 PE files, and the profile names them on Windows x86 alone (`Com_GameManagerNativeModuleFilename`), so another platform has no module to load. No other platform has been run. |
| Module ABI | PARTIAL -- Source, Unit, Live subset | The four table lengths are measured in the retail libraries and frozen: 105 game imports, 26 game exports with API version 4, a 708-byte entity record and 2,112-byte client record; 124 client-game imports and 6 exports. Every slot is the member Alice's table keeps at the mapped position (`code/compat/fakk2_module_abi.h`); the tag queries take F.A.K.K.2's two bone arrays and client-game slot 73 draws boxes. The records crossing the boundary -- entity state, render entity, view, snapshot, game state, command lists and mark fragments -- round-trip in `tests/fakk2_module_abi_tests.cpp`. **105 of 105 game imports and 120 of the 123 client-game imports after the version slot are served**; the three that are not have no call site in the retail client game. |
| Wire codec / demos | PARTIAL -- Live subset, retail demo playback | The framing is Alice's protocol 1, as `fakk2.exe` 1.02's readers show: no acknowledgement, and a gamestate ending at a zero byte. The bodies are F.A.K.K.2's own. Its readers walk a 73-field entity table with its own 31-entry dictionary, and a player table with `pm_flags` and `gravity` in 16 bits. These are generated from the executable (`scripts/generate_fakk2_wire_tables.py`) and coded with Alice's algorithm over F.A.K.K.2's records. Every Ritual snapshot carries the server's time residual after its time. fnTech3 had sent Alice's bodies and no residual byte, and retail refused its demos. Now retail `fakk2.exe` plays an fnTech3 demo through to its end, 207 snapshots (`tests/fakk2_message_codec_tests.cpp`, `tests/fakk2_wire_tests.py`). The 48 KB messages need Alice's long fragment offset; `oracle`'s gamestate is 40,244 bytes. The reliable window is `fakk2.exe` 1.02's 64 commands each way (0x004468dd, 0x0041ffaf), where Alice's engine rings 128. fnTech3 had rung Alice's, because protocol 1 is one wire for both. A client on that wire now rings the profile's window. On an fnTech3 dedicated server, retail `fakk2.exe` and fnTech3's client each take a 60-command burst whole. At 70, each is dropped at the 65th with `Server command overflow`, as retail's server drops. `fakk2.exe` also bounds a map at 384 inline models (0x0042575f), not Alice's 512, and the profile now holds that. On every one of the thirty maps, retail `fakk2.exe` joins fnTech3's dedicated server and enters the game: connect, gamestate, active, the module's `ClientBegin`, and an answer to its `whatis 0` (`.tmp/fakk2/sweep/sweep.py`). A client that leaves is announced with `print "%s %s\n"` and sent a bare `disconnect`, as `fakk2.exe` does (0x00443abb, 0x00443b2b), and a retail client printed another's departure in that form ([drop notice record](2026-09-24-rtcw-et-drop-notices.md)). **Live with retail:** the handshake is Quake III 1.x's (`code/qcommon/ritual_handshake.hpp`, `tests/ritual_handshake_tests.py`). There is a bare `getchallenge`, then `connect "<userinfo>"` printed as plain text with protocol 1 (0x004215ef) and read with no Huffman stage (0x00447100), and a bare `connectResponse` (0x004439fd). fnTech3's server had dropped the plain `connect` in a failed Huffman decode, and its client had asked for protocol 68. With both corrected, retail `fakk2.exe` joins an fnTech3 dedicated server over loopback UDP, and its usercmds, fire buttons and client commands drive Julie there. fnTech3's client joins the same way. A demo retail recorded on that server plays in fnTech3 as in retail: its recorder skips entity 0's baseline (0x0042039e), and both engines drop Julie and her weapon on playback, 302 times each. fnTech3's client joins retail's dedicated server the same way, with an image of the user's disc mounted for retail's CD check: it plays the opening cinematic, and its movement lands Julie where fnTech3's server does, to 0.01 units, with its attack buttons swinging the sword. The server's infostrings now hold only `fakk2.exe` 1.02's own keys (systeminfo `cheats`, `sv_serverid`, `sv_pure`, `sv_paks` and `timescale`; `SV_Init` `0x00446471..0x004465ae`), and its client takes the cheat state from `cheats` as retail's does (`0x00423ad8`); a retail client joined afterwards with none of fnTech3's keys reaching it ([Alice round record](2026-10-08-alice-compatibility-round.md#infostrings)). |
| Filesystem / pure / downloads | PARTIAL -- Live subset | Developer servers and clients mount both paks, 12,142 files, and execute the loose `default.cfg`. Saves are written into the home path. A pure server lists its paks in `sv_paks` and its clients keep to the first 64 (`0x00432516`) and read no loose file but configuration, savegames and their pictures (`0x00430614..0x0043072e`); there is no pure proof, as in Alice: on `fakkhouse` with `sv_pure 1` the server lists its two paks and a save loads back ([Alice round record](2026-10-08-alice-compatibility-round.md#pure-servers)). No download claim is made. |
| Assets / collision | PARTIAL -- Source, Unit, Fixture, Live subset | All thirty maps are `FAKK` version 12 with Alice's records and light grid, admitted by this profile alone (`tests/bsp_profile_loader_tests.py`). Of the 1,347 shaders in the scripts, only the 15 stageless compiler shaders and two that retail also refuses fail in each of the three parsers, and the 58 live `nextBundle` stages parse alike in all three (`tests/alice_shader_parse_tests.py`). The 29 `.urc` menus load. 675 of the 1,340 `.mp3` files are MPEG audio inside RIFF files whose audio a `LIST` chunk follows; Fn's MP3 decoder now decodes their last frame as Miles 6.0b, the decoder F.A.K.K.2 ships, decodes it, and as their own `fact` chunks count, and every other file decodes as before ([last-frame record](2026-10-08-mp3-last-frame.md)). |
| Server / gameplay | PARTIAL -- Live subset | A developer server spawns every map's entities through the retail module (931 on `fakkhouse`) and runs its scripts, camera paths and path nodes. A close that takes a portal's count below zero is told to a developer and play goes on, as `fakk2.exe` 1.02 has it (`0x0042abb0`), not dropped as in Quake III ([area portal record](2026-09-27-alice-area-portal-counts.md)). Inline models now own their model indices as `alice.exe`'s world clear gives them (0x00457960), which keeps the module from running a model definition's init commands on a brush entity; that had removed `fakkhouse`'s sky camera. A level exit fired from the game script (`fakkhouse` to `training`) runs through the retail module's own intermission and `gamemap "<map>$<spawn target>"`. The persistent data is archived and read back, the weapon in hand carries over, and the new level autosaves. The audio zones found the new level only once their lookup dropped the spawn target, as the BSP name does. New Game's intro hands over to `fakkhouse` the same way. After a death, the module's respawn brings up the Load/Save menu, as retail's does, and a save loads back in a fresh process with the weapon in hand. Movement and combat run through the module's own developer commands: Julie jumps 630 units across `fakkhouse`, a spawned creeper kills her, and her handgun and sword wound it, 10 and about 13 a hit. That needed the entity pass of the engine's trace rebuilt as retail's: each clip model is now built as it clips, where a stale shared box had let bullets and blows through actors, and traces through doors. Played by engine-queued keys and mouse, W carries Julie 148 units along her heading, and 400 counts of mouse motion turn her 44°. A strafes her to the wall, and Space jumps her. MOUSE1 fires the handgun and CTRL swings the sword, their sounds starting. Retail's client drives her the same way on an fnTech3 dedicated server (Wire). On `creeperpens`, once the only client has left, the retail game module faults in its own frame: it posts `stopwatchingactor` to the player it no longer has (gamex86 0x10038437, faulting at 0x100a2799). Retail's `SV_Frame` also runs the game with nobody connected (0x004477af), so its own dedicated server is inferred to fault the same way **[I]**; that needs the disc to check. |
| Client / renderer / audio / input | PARTIAL -- Live subset | The retail client game runs; `fakkhouse` draws its world, actors and sky portal, alike on the GL, Vulkan and RTX renderers. The RTX renderer's ray-traced mode did not activate here, so only its raster path ran. The main menu draws from the retail files with Julie's `nextBundle` highlight and the cloud layer, whose `rgbGen global` passes read the colour each picture was drawn in, as `alice.exe`'s immediate `R_SetColor` has them (0x00498b80). Sound uses fnTech3's OpenAL pipeline and the soundtrack engine Alice uses; `fakkhouse`'s soundtrack loads and changes mood. The engine reads the cinematic, fade and letterbox stats from F.A.K.K.2's own slots (14, 15, 16, where its retail game module stores them). The HUD is UIFakkLabel's stat bars with their flash, numbers and item models over the four HUD menus. They show by `fakk2.exe` 1.02's own rule (0x0040b29b..0x0040b326): `ui_hud` set, no letterbox or menu, and no delay; the health bars always; the one-handed menu even with empty hands, unless a two-handed weapon is held; the boss bar while stat 13 is above zero; models in a view without a world take Quake III's flat light, as `fakk2.exe` 1.02 gives them (0x0048f143); `alice.exe`'s rule would leave F.A.K.K.2's lit weapon models black. The inventory (`global/inventory.scr`, `inventory`, `warpinv`, `nextinv`, `previnv`, `inv_restart`) and the weapon groups (`saveweaps`, aliases kept in the configuration) are served, an item going to the hand used by `fakk2.exe` 1.02's rule (0x00409fc9, 0x0040a209); the Alt/Ctrl tables run from `default.cfg`; the options menus' `PulldownMenuContainer`, `MultiLineEdit`, `linkstring`, menu `fadein`, `ui_checkrestart` and `r_gfxinfo` are served, the controls menu's bind list is `fakk2.exe` 1.02's three columns of pictures with an alternate key per action (Draw 0x004c1360, keys 0x004c0de0), scrollbars are its `UIVertScroll` (0x004b3ab0), sliders its `UISlider` with 15-unit arrows and a 10-unit thumb in the colour the widget's draw left (Draw 0x004b64e0), each widget is cut to its frame within its menu as its 2D window's scissor cuts it (0x0049ee80, 0x0049ec21), the menus' cvars include `joy_threshold` (0x0045768a) and `r_useglfog` (0x004616fc), a checkbox's box sits 16 units in and its title 48, as the constructor's indent puts them (0x004bae69..0x004bae7a) where Alice's is 0, and the Load/Save menu lists the saves as retail's list control draws them (`0x004ce3a0`) (`tests/fakk2_hud_tests.cpp`, `tests/fakk2_ui_source_tests.py`). Every official map ships a generated audio zone file and a static light file from its compiled 56-byte lights ([sidecar record](2026-09-24-fakk2-map-sidecars.md)); `fakkhouse` loads 343 zones and 128 lights at run time. Each light's radius follows `fakk2.exe` 1.02's model-light law at its defaults, 7500 a light (0x0048dd80, the constant at 0x004d6050) times the factor of 4 its last step gives the sum, `1 << (r_mapOverBrightBits - tr.overbrightBits)` (0x0048e090): `alice.exe`'s `|I| x 30000 / d^2`, as the files had inferred. Their colours are balanced for the default per-pixel dynamic lights: unbalanced, `towncenter_evil`'s opening averaged 123 with them and 22 without, and it averages 46 balanced. Beside retail's first views, on all three renderers, the lights leave `fakkhouse`'s outdoor opening as it is but brighten `over`'s tunnel walls to 1.20 to 1.41 of retail's and `towncenter_evil`'s room to 1.20 to 1.59, where those walls draw at 0.94 to 1.12 and 0.97 to 1.03 without them. The balance allows that, letting a light add up to half of full brightness at its origin; half the radius would spare the walls away from the lamps but give models a quarter of retail's light, so it is left open. The inventory's column, cells, gauges and models are drawn as `fakk2.exe` 1.02 draws them (`FakkInventory::Draw` 0x004088c0, `FakkItemList::Draw` 0x00409950, `DrawTilePicOffset` 0x00481dd0). Retail's client on fnTech3's server draws the same inventory from the same state. The comparison found the gauge had been drawn over the cell art instead of under it, the item's own selected picture swapped for the type's, and a model hidden where the world behind its cell was nearer: fnTech3 had kept Alice's `ui_skipDepthClear`, which F.A.K.K.2 does not have. The mix is checked from the engine's own capture, a replayed `fakkhouse` demo recorded by `video` (`scripts/probe_ritual_audio.py`). The creeper's slashes and attacks and Julie's pain cry are each found in the recorded track at the time the log gives, at their own pitch. The track never clips, and the soundtrack accounts for 3.3 dB of it. On the OpenAL backend the same replay's sources carry the engine's occlusion, reverb sends and zones. Set beside retail's frames of the same demo, the view, models, effects and HUD match, 12/255 apart on average. Emitters' sprites are now drawn as retail draws them, whatever their flags, so the chimneys smoke and blood splats show. Retail draws its menus and HUD 1:1 where fnTech3 scales them, a presentation choice. The version label reads retail's `1.02` (0x0042e4fd). The intro is `fakk2.exe` 1.02's (`_intro` 0x0040e0b0): the soundtrack, then Ritual's and Gathering's plates, each faded in, held and faded out, then the main menu. It and Escape disable BACK TO GAME outside a game (0x0040e295, 0x0040c032); fnTech3 had gone straight to the menu and always offered it. A widget's picture takes its whole foreground colour (0x0049f078), and a button's hover picture stays on the button that has the keys (0x00498c10); each change of menus gives the keys to the menu's current order (0x004a40d0), so the main menu opens with NEW GAME lit, as retail's does. fnTech3 had lit a button only under the mouse, which a retail capture of the main menu showed as a halo it lacked. Its full-screen menus fade over black, the frame this engine clears under them; `ui_menuDissolve 1` is the retail-exact option of fading them over the image retail's uncleared screen keeps (0x0040b0cf..0x0040b296), where the old menu hazes white and the next comes through. `ui_hud` is F.A.K.K.2's unsaved flag (0x004df7b8), and Alice's `ui_subtitles`, `ui_skipDepthClear`, `ui_crosshair` and `ui_jumpreticle` are Alice's alone. **Input:** a probe's engine-queued keys and mouse (`in_synthesize`, command-line gated, Ritual only; `tests/ritual_synthetic_input_tests.py`) drive every menu path. Hover sounds (`sound/dialog/menu/*.mp3`) and click sounds start. A pulldown's list opens and chooses. The graphics report's text is selected by drag and by Shift with the arrows. The save list takes clicks, Up, Home, End, Delete and Enter, and a double click loads the save, as retail's list control's does (0x004ad497). The inventory's cursor mode picks types and rows by hover and click and uses an item. The view's centre and located prints and frame counter are in `facfont-20`, the font retail builds its view with (0x0040ecb2), and the loading picture is centred at its own size on black (0x0040b14c). F.A.K.K.2 draws with no overbright bits, as `fakk2.exe` 1.02 registers it (0x00460d74): 338 of its 365 lightmap stages had taken Quake III's identity lighting, which one bit halved; they now take retail's identity (0x00476e8c), so each draws with the texture over it as one pass, as retail's does (`tests/alice_shader_parse_tests.py`). Its grass had drawn at 0.69 of retail's. The same demo's frames now match retail's region for region on the GL renderer (`tests/fakk2_overbright_tests.py`). On all thirty maps, the first view from retail's client and fnTech3's on fnTech3's server match in world, HUD and models, but for errors of fnTech3's, now corrected (the world's static models, the far-plane fog, fog volumes under it, animation frames, and models' light, now taken from the map's compiled lights), and the canal water's caustics on `over` and `under`, not traced. The first correction: static models compiled into the world, whose shaders say `rgbGen lightingDiffuse` (`training`'s banners, `towncenter_good`'s ferns, `oracle`'s cloth; 55 surfaces on `fakkhouse`, 745 on six Alice maps), drew black. Retail skips their lighting call for the world entity and keeps the colours last written (0x0047d45a); all three renderers now light them with the colours their map baked on the vertices, as Medal of Honor's static models are lit (`tests/ritual_world_diffuse_tests.py`). The far-plane fog of six maps had brightened distant lightmapped walls, because each stage was fogged on its own and one modulating the lightmap took RTCW's white neutral colour. Retail draws an opaque surface's stages unfogged and then one pass of the fog colour, alpha min(|v - eye| / distance, 1) at each vertex (FinishShader 0x004792bc..0x0047937b, 0x004948a0; `alice.exe` alike), and so now does fnTech3 for both Ritual profiles in all three renderers; a shader with a blended first stage keeps the per-stage fog (`tests/ritual_far_fog_tests.py`). On the GL renderer `fog`'s view then matches retail's region for region within 1% (the pool 4% with the fog off), and `swamp1`'s within 2%; on the Vulkan and RTX renderers the rock, ivy and cave walls match as closely, and the ground by the glowing items is 1.4 times retail's, 1.07 on the Vulkan renderer with its default bloom off. While the far fog is on, retail also draws no fog volume (RB_BeginSurface 0x0047c553..0x0047c583): `over` and `under` start the camera inside a black one, which had halved `over`'s tunnel and blackened Julie; both are lit as retail's now. An animated stage keeps retail's 64 frames (0x00476e00), where fnTech3 had kept 24 of the water caustics' 47. The canal water there is still bluer than retail's lime green: its caustics add blue in fnTech3 and none in retail, measured with overlay textures and not traced. Characters and weapons are lit from the map's compiled lights by `fakk2.exe`'s own law (7500, shifted by the map's two overbright bits, `0x0048e090`), fitted to the renderers' own model light, and other models from the grid. On `water`, where the grid is black at the dedicated server's start, Julie was a third of retail's brightness on her silhouette and is now within 7 to 9% of it; on `over` she is 17% above it ([model light record](2026-09-25-ritual-model-lights.md)). The Vulkan renderer's brighter walls are its bloom, on by default for every profile: with `r_bloom 0` it matches the GL renderer region for region. The user listened to the engine's mix of the `fakkhouse` demo and reported that it sounds right (2026-09-25); that mix is the legacy mixer's, which `video` records, not the default OpenAL path's with its EFX environments and zones. **Open:** other levels' audio, and the OpenAL path's on any level, have not been listened to; a private-desktop probe mutes its listener, so no probe here can record them. |
| Savegames | PARTIAL -- Live subset | A save made in play on `fakkhouse` writes the `.sav`, `.ssv` and `.tga` files into the home path and loads back, with the retail module archiving and restoring its level. Saves made, chosen, removed and loaded from the Load/Save menu by click and key behave as its list control's. The picture is `fakk2.exe` 1.02's colour reduction (0x0045ffa0), not Alice's grey one, which had made F.A.K.K.2's shots near white (`tests/fakk2_saveshot_tests.py`). The engine's side of the format is read from `fakk2.exe` 1.02 and is Alice's: `save/<config>_<name>.<ext>` (0x0042ebe5), the `.ssv`'s 200-byte header and area portals (0x00442a00), which a load puts back whole, a count below zero included (0x0042acf0), the persistent `.spv` and level `.sav` through the module's archive exports (0x004428f0, 0x00442940), and `gamemap`'s queued autosave (0x00441f3c); the module writes the archives' contents in both engines. A load or a restart of the running level keeps the game module, as `fakk2.exe` 1.02's spawn does (`0x00445f41..0x00445fb3`; [Alice round record](2026-10-08-alice-compatibility-round.md#a-load-or-a-restart-of-the-running-level)). Retail's load menu lists and pictures saves fnTech3 wrote, from their `.ssv` headers and `.tga` shots, as fnTech3's own list now does (newest first, the level's title). With an image of the user's disc mounted for retail's CD check, saves crossed both ways on `fakkhouse`, at the opening and armed with the sword in the right hand: fnTech3 loaded retail's saves to the view retail restores, and retail loaded fnTech3's to the same state. |
| Optional services | Unavailable | No service success is synthesized; Ritual's master server is gone and no query is made. |

Every build includes this profile; use isolated writable homes for probes.
Availability closes no compatibility gate, and
Alice's evidence is not F.A.K.K.2 evidence except where the record says an
inference rests on it.


## Medal of Honor gate matrix

The [2026-10-02 UberTools round](2026-10-02-ubertools-round.md) fixes a lost
client-game message continuation bit when the accepted payload fills its
4,096-byte queue. The retail 1.00 client module pins the closing-bit rule;
capacity comes from reconstruction evidence. Production Huffman/codec checks
cover terminal capacities, width validation and the following opcode. Module
payload rollback and retail-engine interoperability remain open.

The same round's warning audit identifies reliable `fadesound` commands being
reported as unknown, in the current `void` startup and prior `m3l1a`/`m5l3`
campaign logs. Import 118 also ignores `differentServer`. MOHAA's server-driven
audio fades and previous-map command handling therefore remained open, until
the round below, even
when the default OpenAL backend and all map startups succeed. OpenMoHAA's
handling is reconstruction evidence; no retail-engine comparison is claimed.

The [2026-10-14 compatibility round](2026-10-14-compatibility-round.md#medal-of-honor-sound-fades)
resolves that gap: import 118 now takes `fadesound` before the client game,
whose retail library has no handler for it, drops one from a server the client
has left, and fades Fn's whole mix in a straight line until the next gamestate,
in the OpenAL and software backends alike (Source, Unit). On `m1l1` the retail
game's death intermission sent `fadesound 4000.00`, the gain fell from 1 to 0
over four seconds in the recorded mix, and the restart's gamestate restored
it (Live subset). The original engine's own timing is inferred from OpenMoHAA
and `alice.exe` **[I]**. The same round makes the loading bar's cache check
find model definitions (`TIKI_FindTikiAnim`, Source, Unit). No gate closes.

The [2026-10-15 shader sweep](2026-10-15-mohaa-shader-sweep.md) loads all 54
maps with `developer 1` in the GL and Vulkan renderers and traces every shader
warning their logs print. The parser met no keyword it lacks. The client no
longer looks up Quake III's console, recording icon, cursor and connect-screen
art, nor the renderers Quake III's `sun`, none of which 1.00 ships, and the
`cull front` that seven `*_backculled` hedgerows spell out is front-sided
culling without a warning (Source, Unit, Live subset). The warnings left are
1.00's own data: images and shaders its scripts, models and client game name
and no pak holds. What 1.00 drew for the DM loading screens' failed `white`
mat is open **[I]**. No gate closes.

The [map shader flag dialects slice](2026-10-15-map-shader-flag-dialects.md)
(2026-10-15) reads Medal of Honor's map flags with 2015's values. Taken from
OpenMoHAA because `MOHAA.exe` is encrypted, and borne out by the maps
**[I]**: `alphashadow` is `0x200`, while `0x8000` and `0x10000`, Quake
III's `lightfilter` and `alphashadow`, are its metal and rock. RTX had read
3,272 metal and 8,413 rock records as light filters. Each renderer now
translates the map's surface flags right after loading them (Source, Unit).
Under RTX, `DM/mohdm6` filters the path tracer's light through 386
triangles, its canopies and fences, where 6,474 did (Live subset). RTX
still found no sun for it, which the worldspawn sun record below settles. No
gate closes.

The [worldspawn sun record](2026-10-16-mohaa-worldspawn-sun.md)
(2026-10-16) gives RTX's path tracer Medal of Honor's sun. No 1.00 shader
has a `q3map_sun`; 46 of the 54 maps name the sun in worldspawn. Read from
`gamex86.dll`'s event strings, OpenMoHAA (reference) and the maps'
lightmaps: `sundirection` is the angles toward the sun, and `suncolor`, or
`sunlight` under another name, is stored as that colour times the cosine
(median 0.98 over 405 opposite-wall pairs on 37 maps). `sundiffuse` (1 when
absent, no more stored above 1) of `sundiffusecolor` is the sky's share
**[I]**. Under the `mohaa` profile the path tracer now takes that sun and a
dome of suns that gives an open floor the diffuse sun, in place of
`RT_FindSun`; every other profile is unchanged (Source, Unit). Under RTX
`DM/mohdm6` and `m5l1a` print "worldspawn sun" and are sunlit, with the
lightmaps' shadows, at levels within a quarter of the baked ones; with no
sun, their sunlit patches were near black (Live subset). No gate closes.

Medal of Honor: Allied Assault is the tenth title and the fifteenth profile,
`mohaa`, over one `main/` installation. It runs 2015's engine, a descendant of
Ritual's and of Quake III's, and fnTech3 serves it through adapters of its own
rather than through the Ritual profiles'. The
[contracts record](2026-09-24-mohaa-contracts.md) pins the installation and the
evidence, and the [map record](2026-09-24-mohaa-maps.md) the maps. The retail
`MOHAA.exe` is SafeDisc-encrypted, and SafeDisc does not run on this machine,
so the engine can neither be read nor run. Every engine rule is measured in
the two plain retail libraries, read from the data, or taken from OpenMoHAA's
reconstruction, and the record marks which.

**Observed.** In developer builds (`-Dmohaa-development=true`, Windows x86,
`meson/build/win32-mohaa`) against the user's 1.00 US installation, every
client windowed at 1280 by 720 with an isolated home path, its frames taken
with the engine's own `screenshot`, and every probe driven by console commands
alone:

- **Server.** All fifty-four maps spawn through the retail game module and
  run twelve seconds each on the dedicated server with no drop.
- **Client.** All fifty-four maps load in the client and exit cleanly. The
  main menu and its submenus draw from the 179 retail menus; `m1l1`'s
  briefing waits on its Continue button; `m1l1`'s scripted opening runs, the
  truck's crew drawn once customised actors reached the renderer; the HUD
  shows health, the weapon's clip, the compass, the weapons bar and the
  crosshair. On a listen deathmatch server the team menu, chat and game
  messages appear in their boxes. Over UDP on the loopback address, the Join
  LAN Game list finds a dedicated server and the client joins it from there.
  Two clients on one dedicated server play a free-for-all (`DM/mohdm1`,
  `g_gametype 1`, `join_team freeforall`), a team match (`DM/mohdm2`,
  `g_gametype 2`), a round-based match (`DM/mohdm2`, `g_gametype 3`) and an
  objective match (`obj/obj_team1`, `g_gametype 4`): each joins its team
  (`join_team`), spawns with its team's weapon, sees the other's join
  messages and lists both teams on its scoreboard, and the round-based and
  objective matches run their clocks; the server's `status` lists both. That
  `status` had dropped the server, reading a Quake III game client where 1.00
  keeps its own record; it and `getstatus` now read 1.00's, a client's score
  being its kills stat.
- **Level flow and saves.** A level exit (`gamemap m1l1` to `m1l2a`) carries
  the player's state and autosaves. Saves made in play and from the Load/Save
  menu list with their pictures; one is deleted through the delete question;
  games load from the list in play and from the main menu; `loadlastgame`
  follows a death; triggered music resumes where it was saved.
- **Demos.** A 3-second `m1l1` recording plays back with its HUD, and
  `timedemo 1` plays it to its end. So does a network client's 8-second
  recording of a dedicated `DM/mohdm1` free-for-all (151 frames).
- **Scripted waits.** Driven by rcon over loopback, so that no probe `wait`
  holds back what a level stuffs, the first briefing shows each slide on its
  narration's timing and hands on to `m1l1`; `training`'s instructor lines
  follow one another; `m1l1`'s convoy reaches the checkpoint. Until the client
  told the game when a sound it waits on starts (export 34, with import 135
  answered from the client), every script stopped at its first such wait --
  527 of them in 35 level scripts.
- **Single-player soak.** `m3l1a`, `m4l1`, `m5l1a` and `m6l1a` each ran three
  minutes with the player under `god` and the level's AI and scripts running.
  `m3l1a` had dropped the game after about two and a half minutes: its
  screen flashes drew with a picture registered anew each frame, which used
  up the renderer's 2,048 image slots. Registered once, the same soak runs
  its three minutes with its image count steady (350, then 359). Sixteen
  more levels -- `m1l2a` to `m6l3a` -- each ran two and a half minutes on the
  Vulkan renderer with no drop, their image counts flat but for models and
  menus first shown during the run. Their other `ERROR:` lines name sound
  aliases a level plays before any are defined -- in single player 1.00's
  client game reads `ubersound/`, after the server has spawned the level
  **[O]** -- or that nothing defines (`heavy_drop`; `m6l3a`'s own aliases sit
  inside a comment of its script). Two clients on a dedicated `DM/mohdm6`
  team match stayed connected for four minutes, with no drop on either side.
- **Single-player sweep.** Each of the 43 single-player maps, briefings and
  credits included, loads, passes its loading screen by `finishloadingscreen`
  and plays 45 seconds with no drop, script error or unserved import; the
  only `ERROR:` lines are the retail game's own AI notices (leashes and far
  planes reducing ranges) and `m1l2a`'s and `m5l1b`'s "Bad voice type m".
- **Loading bar, items bar and sun.** `m1l1`'s loading bar follows its load
  record to the end, with the game's resources counted during the listen
  server's spawn, and shows red to about three fifths in a frame taken
  mid-load. With `ui_itemsbar 1` and `ui_weaponsbar 3`, `give all` (under
  `+set cheats 1 +set thereisnomonkey 1`) puts the papers and binoculars
  models in the items bar. Facing `m3l2`'s sun with the game's `face` cheat,
  the GL, Vulkan and RTX renderers draw its lens flare.
- **Hidden effect carriers.** `m1l1`'s two gas barrels now show fire, smoke
  and fragments. Imports 88 and 89 had rejected every draw submission with
  inherited `RF_DONTDRAW`, even though the client game had already hidden
  the carrier itself. The correction applies to every submitted effect,
  with a fixture covering other barrel debris, grenade/bazooka network
  events and continuous emitters; [record and limits](2026-10-11-mohaa-effects.md).
- **RTX world geometry.** Path tracing includes the appended terrain and
  static scenery range without baking inline doors into the world. The
  repaired `m1l1` ground is verified at `(-3610 -309 486)`, with additional
  terrain and static-only map probes. Compute tracing ran; hardware tracing
  and the separate maximum-detail allocation limit remain open
  ([record and limits](2026-10-11-mohaa-rtx-world.md)).

**Not established.** No retail peer can run, so no packet, demo or save has
crossed between fnTech3 and retail in either direction, and no frame has been
compared with retail's. Every gate below stays partial, and the release gate
stays closed.

| Gate | Evidence | Remaining requirement |
| --- | --- | --- |
| Profile / identity / discovery | PARTIAL -- Source, Unit, Live subset | The profile, its three aliases, its marker, protocol 6's family, the 1,024-submodel ceiling and version 19 maps pass unit checks (`tests/game_profile_tests.cpp`, `tests/profile_marker_filesystem_tests.py`). The installation is recognised by `main/Pak0.pk3`; its six paks' names are capitalised; `system86.dll` is loaded from the installation. On i686 Linux (GCC 13, 2026-09-25) the dedicated server builds and links with the profile's runtime through the legacy Makefile and CMake, and the client through meson (X11, GL renderer, `-Dmohaa-development=true`), from the tree as it stands, since three shared files GCC had refused were made portable (`be_aas_file.cpp`'s `<cstring>`, `nav_graph.hpp`'s cast, `ghoul2_server.hpp`'s record names); neither can run there, as 1.00 ships its game, client game and allocator only as Windows x86 libraries (`gamex86.dll`, `cgamex86.dll`, `system86.dll`), so this profile runs only in a Windows x86 build. Other language editions and the later patches are unexamined. |
| Module ABI | PARTIAL -- Source, Unit, Live subset | The tables and records are measured in the retail libraries and frozen: 167 game imports, a 0xa4-byte export record with API version 11, an 852-byte entity and 2,272-byte client; 168 client-game imports and 30 exports; 596-byte entity and 660-byte player states, a 614,096-byte snapshot, a 412-byte render entity, a 42,948-byte game state (`code/compat/mohaa_game_abi.h`, `mohaa_cgame_abi.h`, `tests/mohaa_module_abi_tests.cpp`). The alias record both modules read keeps 1.00's layout, its subtitle at +0xd4 where the later games put a flag. **167 of 167 game imports and 149 of 168 client-game imports are served**; the other 19 client-game slots are read by no instruction in the retail module, so every import either module can call is served, and an unserved one would drop the game by name. |
| Wire codec / demos | PARTIAL -- Source, Live subset | Protocol 6's framing and record tables are OpenMoHAA's, acting on the 1.00 records: Huffman-coded messages, a client qport, the reliable-command XOR, the out-of-band direction byte, the three extra server opcodes and the end byte 11, sounds in the snapshot, and the eye record with each move. **Over UDP**, both processes bound to the loopback address: the client's `localservers` broadcast carries the direction byte, the dedicated server answers, and the Join LAN Game list shows its name, map, players, game type and ping; the client joins from the list and plays `DM/mohdm1`. That needed the handshake read as 2015's: the client had taken protocol 6 for an old Quake III one and connected with 68, which the server refused. The client now accepts a bare `challengeResponse <challenge>` (OpenMoHAA **[O]**) or one that echoes its nonce, and connects with 6. A listen server hosting a team match had ignored `getinfo`, `getstatus` and `getchallenge` from every remote client, the engine taking Medal of Honor's game type 2 (Team-Match; single player is 0, `gamex86.dll` 0x310e5a50) for Quake III's single player; it now answers them, and a second client joins it over UDP ([GameSpy record](2026-10-06-mohaa-gamespy-lan-queries.md)). Only fnTech3's own client and server have exchanged messages. Demos are `demos/<name>.dm_6`, recorded and played by fnTech3 alone. Retail's own acceptance of any of it is unknown. |
| Filesystem / pure / downloads | PARTIAL -- Source, Unit, Live subset | Clients and servers mount the six paks and write configurations and saves into the home path. `sv_pure` is off by default, as 2015's server registers it **[O]**; with it on, the client proves its general pak references alone, with no server id, module checksum or `@`, as OpenMoHAA reads 2015's `cp` **[O]** (`tests/pure_profile_contract_tests.py`); the profile had had no proof, so a pure server dropped every client. Two fnTech3 clients played on a pure dedicated `DM/mohdm1`, and a client downloaded an extra archive the server loaded its map from (1,384,444 bytes, over UDP and again over HTTP from `sv_dlURL`, SHA-256 matching each time), then passed the pure check with it and played ([contracts record](2026-09-24-mohaa-contracts.md#wire)). The pure gamestate budget measures 1.00's own gamestate -- the 48 KiB message, the game's own baselines and the sounds a multiplayer game registers once the level has spawned, measured again once they are in -- where it had measured Quake Live's inside 16 KiB; every stock map's gamestate, 19,585 to 24,796 bytes, is bigger than that ([pure budget record](2026-10-15-pure-budget-own-wires.md)). Only fnTech3's client and server have taken part. |
| Assets / collision | PARTIAL -- Source, Live subset | All fifty-four maps are "2015" version 19, translated for collision and the renderers with their terrain, static models, light grid and fence brushes ([map record](2026-09-24-mohaa-maps.md)). TIKI models, skeletal animation and sound aliases run through OpenMoHAA's model runtime (`code/mohaa`). The patch collision builder's reports are traced to their patches: `m4l3`'s 48 degenerate-plane reports are twelve flat arches with cells of no area, which OpenMoHAA's builder, compiled from its source over the same patches, reports alike **[O]**; its skipped-bevel reports, now printed only at developer 2 as 2015's are **[O]**, differ from OpenMoHAA's float builder on a few thin cylinders ([map record](2026-09-24-mohaa-maps.md#patch-collision-reports)). |
| Server / gameplay | PARTIAL -- Live subset | The retail module spawns and runs every map; single-player scripts pass `level waittill spawn` once the local client's first snapshot arrives, as 2015's engine starts them, and their waits on sounds end when the local client's sound does; AI, doors, triggers and the level exit run. A close that takes a portal's count below zero is told to a developer and play goes on, as OpenMoHAA reads 2015's engine **[O]**; no retail script or door was found to make one ([area portal record](2026-09-27-alice-area-portal-counts.md)). Deathmatch runs on a listen server. Two clients join a dedicated server over UDP and play a free-for-all, a team match, a round-based match and an objective match together. On a `devmap` server with `thereisnomonkey 1`, the game's own `tele` and `face` cheats stand them 250 units apart on `DM/mohdm1` facing each other, and one fires its Thompson by console command: the other falls dead, both read "ProbeAllied was perforated by ProbeAxis's SMG in the upper torso", the victim's view goes to the death camera, and the server's `status` credits the kill. Chat can be typed: the game reads only its own `dmmessage`, which the client game's `say` and `sayteam` keys now reach through fnTech3's chat line, where Quake III's line had sent a `say` the game never read ([contracts record](2026-09-24-mohaa-contracts.md#client), `tests/mohaa_chat_tests.py`); a player's `dmmessage` comes back to the listen client as chat. |
| Client / renderer / audio / input | PARTIAL -- Live subset | The retail client game runs and the GL renderer draws the worlds, terrain, static models, TIKI actors, sprites, far-plane fog and sky portals; the Vulkan and RTX renderers (the latter with its ray tracing off) draw the worlds, TIKI models and the sun's flare. The engine UI serves the retail menus, HUD, scoreboard, loading screens, message boxes, bind list, saved-game list, LAN game list and `dialog`; the briefing slides a level shows, the single-player objectives overlay, hints naming the player's own keys, the levels' fades and the dialogue subtitles appear. Presentation follows invariant 11: the HUD scales with the screen (`ui_hudScale`, 0 for retail's pixel sizes); sound runs through fnTech3's OpenAL pipeline with 2015's alias distances and pitch, and a room a level's reverb trigger names (none ships) takes the nearest EFX preset. Every official map ships an audio zone file and a static light file for the renderers' static map lights, beside it in its pak's folder; the renderers now look for a map's light file there before its base name, which no other profile's maps change ([sidecar record](2026-09-25-mohaa-map-sidecars.md)). `DM/mohdm1` loads 166 zones and 119 lights, and its first view promotes eight. A level's aliases exist in 2015's order: each map starts with none, on a server and on a client of a remote one, and a single-player level gets its own from the client game's parse, so a sound its spawn plays finds none, as retail 1.11's logs show; they had carried from each level into the next ([contracts record](2026-09-24-mohaa-contracts.md#sound-aliases), `tests/mohaa_alias_registration_tests.py`); music is the shared soundtrack manager plus triggered music. The team menus draw the Allied and Axis player models in 3D, and the map and player-model pickers, Start New Server (as a listen server) and the briefing room's difficulty commands are served. The audio backend plays the levels' voices where they are (a probe's `s_alDebugDump` in `m1l1` finds the captain's briefing line playing in 3D from his truck), and the dedicated server and two clients agree on who is playing. The loading screen's bar follows each map's load record (`maps/<map>.min`, `tests/mohaa_load_record_tests.cpp`) and counts the listen server's spawn; the items bar draws the held items' models (`invmodelhand`) under `ui_itemsbar 1` and `ui_weaponsbar 3`, which retail's menus never set. A level's sun flares from `global/lensflaredefs.txt` through the Alice lens flares in all three renderers, its lights by their lens-flare or additive type (`tests/mohaa_sun_flare_tests.py`); swipes are drawn as fading scene polygons in every renderer (`tests/mohaa_swipe_tests.py`), though no retail content uses one; beams are the client game's own scene polygons. Decals reach terrain and brush models: one cutter in the three renderers serves client-game imports 49 and 50, naming each terrain fragment by its patch as the client game reads it and handing a brush model's back in the model's own space, so bullet holes swing open with `DM/mohdm7`'s doors and the decal editor re-cuts `DM/mohdm3`'s terrain decals to 296 fragments where the retail file holds 312, on the same patches; a decal no larger than `ter_minMarkRadius` cuts no terrain, so no bullet hole marks it, as OpenMoHAA's renderer rules **[O]** ([contracts record](2026-09-24-mohaa-contracts.md#decals-and-level-of-detail), `tests/mohaa_terrain_marks_tests.py`). Import 54's lights, which the client game casts a character's foot shadows from under "Complex" shadows, are the map's sphere lights and sun as the collision model keeps them, chosen as OpenMoHAA's renderer chooses them **[O]**, where none had been given (`tests/mohaa_light_sources_tests.py`): in `m3l1a`'s landing craft a soldier's boot now casts a long foot shadow under Complex that Simple does not draw, not compared with retail's. The terrain a patch's variance tree takes out, which the collision model walks through and 2015's renderer never drew **[O]**, had been drawn; it no longer is, the collision model and the renderers reading the bits through one header (21,475 of `DM/mohdm4`'s 30,976 terrain triangles; [map record](2026-09-24-mohaa-maps.md)). The RTX renderer, with its ray tracing off, draws `m3l2`, its HUD and the sun's flare; asked for ray queries (`rtx_rt_mode 1`) it falls back to rasterising, as the driver offers this 32-bit process no acceleration-structure or ray-query extensions (an RTX 4060 laptop GPU). Its path tracer (`rtx_rt_mode 2`) runs anyway, on its compute backend: `m1l2a` traces a 274,974-triangle world with 105 of the map's lights. It reads those lights as the Quake III compilers' and has not been matched to 2015's, and the level's lamps light little. Terrain is lit by its patches' lightmaps in all three renderers; it had drawn unlit, glaring white on night levels such as `m1l2a` ([map record](2026-09-24-mohaa-maps.md), `tests/mohaa_terrain_lightmap_tests.py`). The Vulkan and RTX renderers had drawn the level's alpha-tested foliage cards as whole quads, as their multitexture shaders were compiled without the alpha test; they are now compiled with it, and `m3l2`'s tree line and hedges match GL's to within the run-to-run residual of the foliage's animation, fog included (`tests/multitexture_alpha_test_tests.py`). Mouse look turns the view by the client game's own scale, export 9's `fov_y / 75`, which the engine asks for on every move as 2015's does **[O]**; left at nought, the scale had turned every mouse move into none. Engine-queued motion (`in_synthesize`, on a private desktop) turned `m5l3`'s player 11.86 degrees for 400 counts at sensitivity 2 and scale 0.674, and pitched it 5.93 for 200 (`tests/mohaa_mouse_look_tests.py`); no probe may use a real mouse, so none has driven it. A level's permanent decals (`maps/<name>.dcl`) are built into its world in all three renderers, each fragment in the open leafs in front of it, the file's own leaf lists being stale in eight of its eleven files; on the three whose lists fit, 49 of 50 fragments land in exactly the listed leafs and the fiftieth adds only a solid one; `r_drawstaticdecals`, the options' "Static Decals" box, shows and hides them, and GL, Vulkan and RTX draw `m2l3`'s alike ([map record](2026-09-24-mohaa-maps.md)). The retail decal editor (`dcl_editmode`, F6's menu, `dclc_*`) and level-of-detail tool (`lod_*`, `l`'s menu) are served for every renderer by the engine through the client game's exports 16 to 18, whose arguments and buffers were measured; skeletal models lose detail with their size on screen by their shipped `.lod` records (22,078 triangles fall to 3,220 at the tool's far end), and the options' model-detail choice now reaches `r_lodscale`, which the renderers had held cheat-protected. Import 52's decal light was handed on a 0..1 scale where every caller stores it as a byte, drawing every lit mark black; it is now on the byte scale, as OpenMoHAA's is **[O]** ([contracts record](2026-09-24-mohaa-contracts.md#decals-and-level-of-detail), `tests/mohaa_permanent_decal_tests.py`). The server's `fadesound` fades the whole mix, music included, in both backends, and the next gamestate ends it ([round record](2026-10-14-compatibility-round.md#medal-of-honor-sound-fades), `tests/mohaa_sound_fade_tests.py`). **Open:** beams are unexamined against retail, and decal cutting against anything but 1.00's decal files, `ter_minMarkRadius` being OpenMoHAA's **[I]**; how 1.00's own editor and tool presented themselves is inferred from their menus and OpenMoHAA **[I]**; 1.00's own fade timing is unmeasured **[I]**; and nobody has listened to the output, though a capture of `m1l1`'s opening mix through OpenAL Soft's wave writer holds the level's triggered music and the captain's first line, found by cross-correlation with the retail files. |
| Savegames | PARTIAL -- Live subset | A save is the game's `.sav`, fnTech3's own `.ssv` layout (1.00's cannot be read; `code/compat/mohaa_savegame.hpp`, `tests/mohaa_savegame_tests.cpp`) and a colour picture of the view. A load puts the portal counts back as saved, a count below zero included, as OpenMoHAA's reader does **[O]**; fnTech3 had raised such a count to zero ([area portal record](2026-09-27-alice-area-portal-counts.md)). Saves, autosaves, quick saves, deletion, the Load/Save list, `loadlastgame` and the triggered music's resumption work in play; a save made in the middle of Omaha Beach's landing (`m3l1a`, 275 KB) loads back into the boat, and the landing carries on. No retail save exists to read, and fnTech3's cannot be offered to retail. |
| Optional services | PARTIAL -- Source, Unit, Live subset (LAN queries and search); masters Unavailable | 1.00's GameSpy masters are gone; no heartbeat is sent, no `\secure\` challenge is answered and no service success is synthesized. While a multiplayer game runs and `sv_gamespy` is set, the server answers GameSpy's LAN status queries -- `\status\`, `\basic\`, `\info\`, `\rules\`, `\players\`, `\echo\` -- on the first free of a hundred ports from `net_gamespy_port` (12300), as OpenMoHAA's server and the GameSpy SDK it carries do **[O]**, with OpenMoHAA's keys less `sprinton` and `realism` (cvars 1.00's game lacks), `allowlean` and its bot keys; `gamever` is 1.00 **[I]** (`code/compat/gamespy_query.hpp`, `tests/mohaa_gamespy_query_tests.py`). Start New Server's GameSpy box sets `sv_gamespy`. **Live** on loopback: a broadcast LAN search over 12300 to 12316 finds two dedicated servers at 12300 and 12301 and a listen team match at 12300, each answer well formed with the expected values, the listen server's player list counting its players and a death; HEAD answered nothing there ([GameSpy record](2026-10-06-mohaa-gamespy-lan-queries.md)). 1.00's own LAN list does not use these ports: it asks the game port with `getinfo` (`ui/joinlangame.urc`), which fnTech3's server answers. fnTech3's Join LAN Game list asks both: beside its `getinfo` scan of 12203 to 12206 it makes GameSpy's LAN search as the SDK's browser made it **[O]** -- `\status\` broadcast once to 12300 to 12316 from a search socket of its own, the answers taken for three seconds -- and lists an answer as OpenMoHAA's browser judges it **[O]** only when 1.00 can join it (`mohaa`, a `gamever` within a tenth of 1.00, not a demo's, at its `hostport`); a server that answers `getinfo` is listed from that answer. **Live** on loopback, a dedicated server on game port 12210, beyond the `getinfo` scan, is listed from its GameSpy answer and another on 12203 once, from `getinfo`; the probe's own stand-in answers for 1.11, Spearhead's `mohaas` and a demo are refused and a 1.00 answer in two packets is listed; HEAD listed the 12203 server alone. The query and search sockets carry datagrams in a socket test of `net_ip.c` on Windows and on i686 Linux ([search record](2026-10-08-mohaa-gamespy-lan-search.md)). No retail client, server, GameSpy Arcade or other GameSpy browser has sent or read these queries and answers. |

Every build includes this profile; use isolated writable homes for probes.
No other title's evidence is Medal of Honor's.

## Q3Test gate matrix

The public Quake III test releases of 1999, Q3Test 1.02 to 1.08, are the
sixteenth profile, `q3test`, under the Quake III title. Each release has its
own protocol (35 to 40), map version (IBSP 44 or 45) and module tables, so the
profile pins one release at startup from the installation's own modules and
pak and never mixes two. 1.02 and 1.03 shipped as dedicated servers alone;
1.05 added the client, with menus compiled into its engine; 1.06 to 1.08 carry
menu modules. The [Q3Test ledger](Q3TEST.md) records the contracts, and the
[client and interoperability record](2026-09-25-q3test-client-interop.md) the
runs.

**Observed.** In developer builds (`-Dq3test-development=true`, Windows x86,
`meson/build/win32-q3test`), every process windowed on a private desktop with
an isolated home, frames taken with the engine's own `screenshot`, and every
probe driven by console commands alone:

- **Server.** Every release's retail game module hosts every map it ships.
  Each client release's retail client (1.05 to 1.08) connects to fnTech3's
  server and enters the game; for 1.02 and 1.03, which have no retail client,
  a headless protocol client built from the same codecs joins the retail
  server and fnTech3's alike, walks and leaves. A server takes 128 clients,
  as every release does: the protocol client in slot 120 in every release,
  and a retail 1.06 client there too. The serverinfo and systeminfo keys
  match each retail server's, but that retail 1.05 leaves its game's keys
  out.
- **Client.** fnTech3's client runs 1.05 to 1.08's retail client games,
  joins its own server, each retail server and its own listen server, and
  draws 1.06 to 1.08's retail menus and 1.05's engine menus in GLx, Vulkan
  and the RTX renderer, raster and path-traced.
- **Demos and sound.** The six shipped recordings time-demo, 1.06 to 1.08's
  to retail's frame counts; recordings made by fnTech3's client replay in
  fnTech3, and an independent decoder reads every record. Each client
  release's replay mixes its recording's effects at their logged times in
  the legacy mixer's capture. The retail engines run in a restricted demo
  mode that reads only their pinned paks, so a retail client cannot load
  fnTech3's recordings.

| Gate | Status | Evidence and remaining work |
| --- | --- | --- |
| Profile / identity | PARTIAL -- Unit, Live | Release recognition and pinning (`tests/q3test_identity_tests.cpp`); every release activates from its installation. |
| Filesystem | PARTIAL -- Unit, Live | Each release's paks mount; retail's restricted-mode pak pins are not enforced. No release has a pure mode or sends a pak list, so `sv_pure` has no effect and says so ([pure budget record](2026-10-15-pure-budget-own-wires.md)). |
| Map format / collision | PARTIAL -- Unit, Live | IBSP 44 and 45 translate for collision and every renderer; Q3Test's curve rules are unit-tested, and each release's own patch generator, run in-process, keeps the same facets as fnTech3's on all 1,948 shipped patches and on 36 synthetic ones placed where its 8192-unit squares and +-99999 bounds decide ([facet record](2026-10-09-q3test-facet-windings.md)); 97,740 traces through the world and around linked brush models, 22,500 point contents, 186 links and 3,000 box queries on all fifteen shipped maps of the six releases agree with each retail server in every hit, contents, entity, link and box, and within 0.0026 units, since a trace left inside a brush keeps its contents and a link that finds no area, or an entity not in use, stays unlinked, as in every release ([trace record](2026-10-08-q3test-traces.md), rerun with q3tourney in the facet record); two traces on 1.06 and 1.07's q3tourney differ in surface flags alone; position tests and client entities are not compared. |
| Module ABI | PARTIAL -- Live | Game modules of every release; client games 1.05 to 1.08; menus 1.06 to 1.08. Bots are not claimed: no release shipped a bot library. |
| Wire | PARTIAL -- Interop subset | Short one-client loopback sessions in both directions with every retail peer that exists, up to slot 120 of 128. A retail 1.08 client and fnTech3's share fnTech3's server, and both share a retail 1.08 server, for five minutes at 10% loss through a map change, with chat from fnTech3's client heard by the retail one ([sustained sessions record](2026-10-08-legacy-sustained-sessions.md)). No session with a remote peer. |
| Demos | PARTIAL -- Live | Shipped recordings time-demo (1.06 to 1.08 to retail's counts); fnTech3's recordings replay and decode; retail cannot read them; 1.05's machine-dependent time-demo count is not reproduced. |
| Renderer / assets | PARTIAL -- Live | GLx, Vulkan and RTX (raster and path-traced) captures of both map formats, menus and HUD; the release shader dialect and 2D pictures read as retail read them. |
| Audio / input | PARTIAL -- Live (audio) | Replays' effects found at their logged times in the legacy mixer's capture; the OpenAL mix is not captured; no input test, because simulated buttons need the user's permission per run. |
| Gameplay | PARTIAL -- Interop subset | The protocol client moves on every retail and fnTech3 server; combat and pickups seen only in recordings; no live play. |
| Optional services | Unavailable | The 1999 master and update servers are gone; `globalservers` sends its query and reports nothing it did not receive. |

Every build includes this profile; use isolated writable homes for probes.
No other profile's evidence is Q3Test's.
