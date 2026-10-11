# Jedi Academy live join: local sounds and the map change, 2026-10-06

fnTech3's `jka-mp` client joined retail `jamp.exe` 1.01's dedicated server
with pure admission and entered, but the server's first chat line ended the
session: its sound reached client-game import 38, `CG_S_STARTLOCALSOUND`,
which fnTech3 refused by name ([legacy-server record](2026-10-05-legacy-server-hosting.md)).
This slice serves import 38 through fnTech3's own local-sound path, then
follows the same live session to the next two stops: a map change the engine
refused as an Enemy Territory call, now fixed, and a Ghoul2 attachment in the
client game's first frame on the new map, recorded here as the next
boundary. The [JSON companion](2026-10-06-jka-live-join.json) holds source
and binary identities, the command line and every run's step results. No
gate closes.

## Identities

| File | SHA-256 |
| --- | --- |
| `jamp.exe` 1.01 | `e8062d3948ff05917b48e47d8eec0ee43bcc6fb03924410712412404ddfee28b` |
| `base/assets3.pk3:cgamex86.dll` | `e5d5525829bfcaf3c0428d87fe804e5a46a75dcf0a141f8288061a3970ff140d` |
| `fntech3.exe` (runtime `jka-live-join`) | `1483d738b765284425ee473a3a87a8f73f0d8101737face716f7ab12ba55db92` |

The runtime is HEAD `9f85e4ab` plus this slice's engine changes, built from a
clean export (MSVC 2022 x86, Meson `debugoptimized`, strict warnings, every
development gate on). Its engine source is this slice's commit. The
installation was read only and its inventory was unchanged after every run.

## Import 38

**Observed** (retail, read with Capstone; no retail code is incorporated):

- `jamp.exe`'s client-game dispatcher bounds the import at 0x15e and jumps
  through a two-level table (0x412183). Import 38 lands at 0x412934: it passes
  `args[1]`, the handle, in a register and pushes `args[2]`, the channel, to
  `S_StartLocalSound` (0x46aed0), then returns zero.
- `S_StartLocalSound` returns at once while sound is not started or is muted
  (0xd83160, 0xd8315c). Otherwise a handle outside the registered range
  stops the session with `S_StartLocalSound: handle %i out of range` (an
  `ERR_DROP`, 0x46aeee), and the sound starts on the listener's own entity
  (0xd83140) with no origin through `S_StartSound` (0x46ac60).
- `S_StartSound` reads Academy's own channel numbers: it compares with 7 (the
  ambient channel) before playing the listener's own sounds at 0.75 of full
  volume (0x46ae4d to 0x46ae88; 0.75 at 0x54d4f0 times 255.0 at 0x5660e0), and
  marks a voice on 3, 4 and 12 (0x46ae8b to 0x46ae98). `S_PickChannel` picks a
  free channel for 10 as for 0 (0x46a15b).
- The retail client game's wrapper for import 38 (0x3005e4c0) forwards both
  words unchanged. Its sixteen callers send channel 9 nine times, 8 three
  times, 1 twice and 0 twice; the `chat` command's handler (0x3005d4d2,
  after comparing with the string at 0x300914dc) sends 8, and the
  disruptor's zoom sounds 0.

**Reference** (OpenJK `1a6a643`, a maintained port): `codemp/cgame/cg_public.h`
names import 38 with the same two arguments, and
`codemp/qcommon/q_shared.h`'s `soundChannel_t` names Academy's 14 channels,
agreeing with every number retail reads above: auto 0, local 1, weapon 2,
voice 3, attenuated voice 4, item 5, body 6, ambient 7, local sound 8,
announcer 9, less-attenuated 10, menu 11, global voice 12, music 13. Only these
format facts are taken from it.

**Served.** Academy inserts channels among Quake III's, so its raw numbers
would take on the wrong engine rules: its 7 (ambient) is the engine's
announcer. `client_jka_sound.hpp` gives each Academy channel that means what
an engine channel means the engine's number (auto, local, weapon, voice,
item, body, local sound, announcer), and every other number one of its own
outside the engine's channels, as Elite Force single player's are. Import 38 hands the
handle, the translated channel and retail's level (0.75 below the ambient
channel, otherwise full) to `S_StartLocalSoundVolume`, fnTech3's own local
sound entry. A word outside the module's `int` is refused. The canonical
dispatcher, which would pass Academy's raw channel through, never sees it.

**Presentation (invariant 11).** The sound plays through fnTech3's mixers as
Quake III's local sounds do: the local and announcer channels are heard at
the listener, the announcer and the listener's own sounds are the last a
new voice evicts, and each channel keeps its tone class, under the OpenAL backend's HRTF and output
settings or the legacy mixer. Retail supplies only the channel's meaning and
the level; no retail mixer is reproduced and no option replaces Fn's.

**Differences recorded, not reproduced.** Retail drops the session on an
out-of-range handle while sound runs; fnTech3's backends print a warning and
play nothing. The handle is fnTech3's own registration number, which the
client game only receives from fnTech3's registration imports. With sound
disabled both engines do nothing.

## The map change

After chat both ways passed, the server's `map mp/ffa3` ended the session
with `ET snapshot callback requires an ET game module`. The engine numbers
its semantic game exports and its semantic client-game exports in two lists
from 0x10000, so Enemy Territory's `GAME_SNAPSHOT_CALLBACK` and Academy's
`CG_RAVEN_MAP_CHANGE` (the client game's export 27, sent on Academy's
`svc_mapchange`) are both 0x10004. `VM_Call` refused the number before asking
which module was called. It now refuses the snapshot callback only for a game
module outside ET, as the RTCW guards beside it already ask, and refuses the
map change for every client game but Academy multiplayer's, so the shared
number reaches each module only under its own profile. Every profile, both
modules and both build kinds are checked against production `VM_Call`; the
new check fails against HEAD's `vm.c`.

## Live sessions

`scripts/probe_legacy_sessions.py --profiles jka-mp --directions join`, the
fnTech3 client against retail `jamp.exe` on loopback, windowed on a private
desktop, without sound or input:

| Run | Client | Steps |
| --- | --- | --- |
| `sess-1` | import 38 served | join, client chat, **server chat** pass; map change fails (ET snapshot guard); kick passes |
| `sess-final` | import 38 and the map change | join, client chat, server chat, **map change** pass; `map_restart`, reconnect and server quit fail (client stopped); kick passes |

On the new map the client game's first frame submits a Ghoul2 instance
holding two models, `models/players/kyle/model.glm` and the Bryar pistol's
world model `models/weapons2/blaster_pistol/blaster_pistol_w.glm` in its
second slot. fnTech3's client-game renderer serves only single-model
instances ("attachments remain open"), so the client stops there and
disconnects; the server's `map_restart` then runs without it. The refusal
now names the instance's model count and models, which is how this was read.

Two diagnostic variants, the same runtime, change nothing in the probe:

- `cg_thirdPerson 0`: the same instance, the same stop, so it is not drawn
  only for a third-person view of the client's own body.
- changing to `mp/ffa1` again instead of `mp/ffa3`: the same instance, the
  same stop. On its first entry (`mp/ffa1`) the client is a spectator
  (`nfr 6 1 3`), the cgame opens the player-configuration menu and no such
  instance is drawn; after a map change (`nfr 6 0 3`, no menu) one is.
  Which entity it belongs to is not established.

So a map change now completes at the protocol level, but no `map_restart`,
reconnect or server quit can be observed from fnTech3's client until Ghoul2
attachments render. That is the next refusal; it is a renderer slice of its
own (bolt matrices from the parent's pose, the attached model's own skeleton
and placement through Fn's skeletal path), not served here.

## Re-check at `6a93b622`, 2026-10-08

The slice was checked again at `6a93b622`, after the attachment, entity tint,
ambient set and server Ghoul2 slices that followed it. A clean export of that
commit was built as above, and its binaries replaced this record's runtime in
`.tmp/runtime/jka-live-join` (`fntech3.exe` `0f698f02...`). Its import-38
and map-change unit tests (below) pass in that export.

With sound on, a client at `6a93b622` still stops at the voice-volume query
(import 35) before any chat line, so the sound runs use a diagnostic build
that differs from `6a93b622` there alone: import 35 answers 0 with a backend
running, as the [ambient sets record](2026-10-06-jka-ambient-sets.md)'s
diagnostic did (`fntech3.exe` `a394abed...` in
`.tmp/runtime/jka-ambient-diag`, not committed). Every client ran windowed
on the probe's private desktop, so it was offscreen-muted and nothing was
heard. The installation was unchanged after every run.

| Run | Runtime | Sound | Steps |
| --- | --- | --- | --- |
| `sess-head` | `6a93b622` | off | every main and kick step passes: join, both chat lines, map change, `map_restart`, reconnect, server quit, kick |
| `chat-audio-1` | diagnostic | OpenAL | join and both chat lines pass, the OpenAL world dumped every three frames around each line |
| `sound-on-openal` | diagnostic | OpenAL | every main and kick step passes |
| `sound-on-legacy` | diagnostic | legacy mixer | every main and kick step passes |

**Observed** (fnTech3, OpenAL backend, `s_alDebugDump`): each chat line --
the server's game echoing the client's own `say` back as `chat`, and the
server's `svsay` -- starts `sound/player/talk.wav` in the first dump after
the command arrives (the installation holds `talk.mp3`, which fnTech3's
sound fallback loads), and the voice holds for 23 dumps each time. It is a
one-shot on the listener's own entity, 0, where retail's `S_StartLocalSound`
plays it: Fn's `local` source class, listener-relative at pan 0, gain 1.00
(retail's full level for channel 8, above the ambient channel), dry 1.00 and
no reverb send. The dump's `dist=36.0` is the view height between the
player's origin and the listener's eye; a relative voice plays at the
listener. The legacy mixer's voices were not observed: its per-voice print,
`s_show`, is a cheat variable, which a server with `sv_cheats 0` resets, so
that session shows only that the mixer takes import 38 through the whole
session without a refusal.

**Presentation (invariant 11).** In the same dumps the map's own loops carry
Fn's occlusion, band-pass tones and the reverb of the listener's `hall`
environment, while the chat sound keeps the local class's dry, unsent
routing. This machine's system OpenAL runtime has no OpenAL Soft extensions:
its filters are low-pass only, so the local class's high-pass tone and the
band-pass tones degrade to low-pass, and HRTF is unavailable here.

So the standard session reaches no refusal at `6a93b622`. The boundaries
beyond it are those recorded after this slice: the voice-volume query with
sound on, and the first sound a client that joins the game starts, import 37
([ambient sets record](2026-10-06-jka-ambient-sets.md)). The UI's own local
sound (UI import 33) is unchanged.

## Tests

- `tests/jka_cgame_local_sound_tests.py` compiles the production import-38
  case against a recording engine entry: every Academy channel's
  translation, that no other number takes on an engine channel's rules and
  none is shared, the level on both sides of channel 7, the four channels the
  retail client game sends, unregistered handles and over-wide words.
- `tests/jka_map_change_export_tests.py` compiles production `VM_Call` in
  client and dedicated builds: Academy's map change reaches its client game as
  export 27, every other profile's client game refuses it, and the game's
  snapshot callback still reaches only ET's game module.
- `tests/jka_sp_vm_export_tests.py`'s hand-written prelude gains the client
  game's map-change name the new guard uses.

## What stays open

- **Ghoul2 attachments** in the client game, the next refusal above, and
  with them `map_restart`, reconnect and server quit in this direction.
  Served since ([attachments record](2026-10-06-jka-attachments.md)); the
  2026-10-08 re-check passes all three.
- **Audible play.** These sessions run without sound. With audio enabled,
  Academy's voice-volume query (import 35) still refuses an enabled backend,
  so import 38's mixing is established by unit tests only. The 2026-10-08
  re-check's diagnostic build, which answers that query, shows the chat
  sound starting as an Fn OpenAL voice; the mix itself is not captured.
- The UI's own local sound (UI import 33) remained a named refusal; no probe
  without input reached it. Served since through import 38's path, and
  reached without input from the UI's own `ui_openmenu` command
  ([UI local sound record](2026-10-08-jka-ui-local-sound.md)).
- The retail client needs disc 1, so the other directions remain unrun; the
  download session's retail server still cannot load its game library from
  a generated mod folder.
