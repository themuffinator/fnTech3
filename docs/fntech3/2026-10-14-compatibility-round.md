# Compatibility gap resolutions, 2026-10-14

This round resolves three recorded gaps in Medal of Honor: Allied Assault and
Jedi Academy multiplayer, and corrects stale ledger text. Each change was built
and tested as HEAD `b0566581` plus this round's files alone, without another
session's uncommitted work in the shared tree. Its
[JSON companion](2026-10-14-compatibility-round.json) records the binaries,
commands, retail identities and outcomes. No compatibility gate closes.

## Medal of Honor sound fades

**Observed.** Retail 1.00's game library (`gamex86.dll`) asks for a sound
fade with the server command `fadesound %0.2f`: its fade routine at
`0x310e38e0` multiplies the seconds it is given by the float 1,000 at
`0x3119f4f0` and sends the result to every client, the intermission routine
at `0x310e3910` sends its own time the same way, and the one at `0x310e3a10`
sends `fadesound 0.00`. The retail client game (`cgamex86.dll`) contains no
`fadesound` string, so it has no handler. Under HEAD, fnTech3's client-game
import 118 passed every server command on, and earlier runs logged the result
on `void` (2026-10-02) and `m1l1` (2026-10-11): `Unknown client game command:
fadesound`, with no fade.

`MOHAA.exe` is encrypted and cannot be read, so how its engine takes the
command is inferred from two sources. OpenMoHAA's `CL_ProcessServerCommand`
consumes the command before the client game, drops one from a server the
client has left, and fades the whole mix in a straight line over that many
milliseconds of real time until `S_FadeSound(0)` at the next loaded level or
a cleared client state **[O]**. `alice.exe`, the parent engine, consumes the
same command at `0x004241f6` and holds the silence until the next level or
disconnect, as fnTech3's Alice profile already does.

**Change.** Import 118 now takes `fadesound` itself and returns false, so the
client game never sees it; a command marked as coming from the previous
server is dropped. `S_MOHAAFadeSound` starts the fade on the client's real
clock (`shared/MOHAAFade.h`); 0, a negative or a non-finite time cancels it,
and a new gamestate (`CL_ClearState`) or a client flush (`CL_ShutdownAll`)
ends it. The fade scales Fn's whole mix: the OpenAL listener gain, which
covers every source, music included, and in the software mixer the channel
volume and the streams copied into its paint buffer. Distances, rooms,
occlusion and the other FnQ3/FnQL audio features are unchanged; only the
level the game asks for is applied. `s_info` prints `fadesound gain` under
this profile. Other profiles neither start nor read the fade.

**Unit (Source).** `tests/mohaa_sound_fade_tests.py` runs the production
fade, engine entry, import 118 reader and both mixers' production paint and
update code: the linear law and cancels, the command taken before the client
game and a previous server's dropped, other commands passed on unchanged,
profile scoping, the OpenAL listener at the faded gain, and the software
mixer's channels, music ring and voice faded alike. The shared fixtures
`engine_input_capture_tests.py`, `rtcw_music_fixture.py` and
`alice_audio_backend_tests.py` gained the new function's stub.

**Live (subset).** A windowed `m1l1` listen server, driven by rcon over
loopback, had its player killed; the mix went to OpenAL Soft's wave writer,
so nothing reached a speaker. The retail game began its death intermission
and sent `fadesound 4000.00`; the engine took it, with no unknown-command
line. `s_info` read the gain as 1.00 until then, then 0.77, 0.52, 0.27, 0.02
and 0.00 a second apart. The restart's new gamestate ended the fade, and the
gain read 1.00 again. In the recorded mix, the level's sound (about 0.2 RMS)
fell to silence over about four seconds and came back after the restart.

The retail game begins that intermission only when the death animation ends,
which happened within the window in three of seven kill runs with the change
and in none of seven without it; the companion lists every attempt. All three
gave the same gains, and with a 45-second window the gain held at 0.00 until
the restart. The unchanged build's handling is therefore shown only by the
earlier logs above. A change-level trigger fired with the cheat `script`
command did not run the intermission. A deterministic retail trigger and the
timing of the original's fade remain unmeasured.

## Medal of Honor cached model definitions

`TIKI_FindTikiAnim` (`code/mohaa/tiki/tiki_cache.cpp`, taken from OpenMoHAA
`ab43a0de`) looked up a 1,024-byte buffer it never filled. Its only caller
is the loading screen's check of which resources the caches already hold
(OpenMoHAA's `UI_IsResourceLoaded`, here `MOHAA_ResourceCached`), which
excludes those from the bar's total. Model definitions and their parts were
therefore never found. The lookup now copies and canonicalises the name, as
`TIKI_FindTiki` and the registration that keys the cache already do.
`tests/mohaa_tiki_cache_tests.py` compiles the production lookups,
canonicaliser and resource check, fills the stack with a pattern before each
call, and fails on HEAD's lookup (Source, Unit). The change is listed in
`code/mohaa/README.md`. Its effect on a real second load's bar was not
measured.

## Jedi Academy saber glow and lines

The 2026-10-03 replay of fnTech3's own 440-record server demo stopped after
408 snapshots at client-game import 202 on Raven entity type 5,
`RT_SABER_GLOW` ([record](2026-10-02-jka-compatibility.md)).

**Observed, retail renderer.** `jamp.exe` 1.01's `RB_SurfaceEntity`
(`0x4bdcd0`) sends types 2 to 11 through a table at `0x4bddb8`:

| Type | Routine | What it draws |
| ---: | --- | --- |
| 5, saber glow | `0x4ba780` | Down the blade from `saberLength` (offset 180) along `axis[0]`, a view-facing sprite (`0x4ba600`) every 0.65 of the current radius, the radius growing 0.017 a sprite; then a hilt sprite of 5.5 plus 0.25 times `rand()/32767`. |
| 8, line | `0x4bb3c0` | One quad (`0x4ba860`) from `origin` to `oldorigin`, `radius` either side across the normalised cross product of the eye-to-end vectors; every corner the entity's whole colour; texture 0 to 1. |
| 9, oriented line | `0x4bb4c0` | The same quad (`0x4bb040`) across the normalised `axis[1]`, half the union's width (offset 148) either side, t running to its texture scale (offset 156). |

Its front end (`0x4a6442`, table `0x4a65c0`) gives types 2 to 6 and 8 to 10
one branch: no third-person entity in a primary view, the custom shader and
the sprite fog test. Type 7 draws nothing, and type 11 skips the
third-person test. Pinned OpenJK (reference observation, no text
incorporated) shows the multiplayer client game submitting types 5, 8 and 9
(`CG_DoSaber`, debug lines and oriented-line local entities).

**Observed, retail geometry.** Retail's own three routines were run
in-process on 22 synthetic entities and views (`tests/jka_surface_cases.h`):
a fixed-base stub held `jamp.exe`'s sections at its own address. `rand`,
`sin`, `cos` and `sqrt` were redirected and the batch flush trapped; the
x87 control word was 53-bit precision, which retail's maths routines expect
(`0x27f`). The 916 vertices and their indexes are
`tests/fixtures/jka_surface_retail.json`, generated from synthetic input
only.

**Change.** The canonical entity gains `RT_JKA_SABER_GLOW`, `RT_JKA_LINE`
and `RT_JKA_ORIENTEDLINE`, with the blade length, line width and texture
scale in `refEntity_t.jka`. The Academy adapter maps 5, 8 and 9 and refuses
a non-finite shape field of the type that draws it. The oriented quad (3),
electricity (6), cylinder (10) and mini-entity chain (11) stay refused by
type. `renderercommon/tr_jka_surface.h` tessellates the three in GLx,
Vulkan and RTX's raster path, sharing Elite Force's vertex helpers. The
types join each renderer's sprite branch, and GLx batches the lines as
beams. The renderers' shaders, fog, sorting, lighting and post-processing
apply unchanged.

Three engine choices differ from retail. Retail grows the entity's own
radius as it draws, so a second view of the same scene in one frame would
start larger; Fn starts each view from the module's radius. A blade stops
at 1,024 sprites, where retail has no bound and its own sabers need about
twenty. A line's vertices take the view-facing normal Elite Force's lines
write, where retail writes none. The blade's sprite count follows retail's
53-bit steps exactly.

**Unit and comparison.** `tests/jka_render_tests.cpp` covers the adapter's
types, shape fields and refusals. `tests/jka_surface_tests.py` builds each
renderer's own quad stamp with the shared headers and matches all 22 retail
cases. Sprite counts, indexes, texture coordinates and colour bytes match
exactly and positions within 0.00002 units, also after earlier vertices in
the batch. It also checks the batch flush, the sprite bound and the front
end's branch. Changing the glow spacing, the line's corner colour, the
oriented width or the radius growth each fails the comparison.

**Live (subset).** The server demo now plays to its end in GLx, Vulkan and
RTX: 439 snapshots including deltas, the end-of-demo marker and a normal
exit, with no refusal. The client-recorded demo of the same session (28
snapshots) also completes and draws the spectator world. No capture shows
a blade, for three separate reasons:

- After its opening player menu closes, the server demo draws no 3D view
  at all. `r_speeds` counts 863 world surfaces behind the menu, then none,
  from long before the saber appears; closing every menu or clearing
  `cl_paused` changes nothing. A non-invasive debugger read seven seconds
  in found the client's latest snapshot flagged `SNAPFLAG_NOT_ACTIVE`
  (snapFlags 6, server time 19,350), on which a client game draws its
  information screen instead of the world. The
  [snapshot-flag record](2026-10-14-jka-demo-snapshot-flags.md) settles
  this. The flag is on the 406 snapshots sent while the recorded client
  was loading, as retail's server writes it, and playback skips them as
  retail's does. The readings above were taken after the demo had ended.
  Its 1.6-second active span draws the world behind the menu.
- Joining a local `mp/ffa1` listen game stops, as before, at the spawn
  effect's Emitter primitive.
- The UI's saber preview first refuses its hilt: a model on the engine's
  internal skeleton, with UI bolt queries still unserved.

## Ledger corrections

The Elite Force and Jedi Outcast matrices still told readers to build with
`-Dstvef-development=true` or `-Djk2-development=true`, and the shared
release checklist spoke of each profile's development gate. Since the
[availability policy](2026-10-11-profile-availability.md) those options are
accepted no-ops; the three passages now say so.

## Validation

The Windows x86 export of HEAD plus this round's files built all 1,258
targets with strict warnings at four jobs. The first attempt at full
parallelism ran out of commit memory while another process held about
13.6 GB. The native Meson suite passed 263 tests with two skips: OpenAL
loopback, which this machine's library lacks, and the optional Quake Live
factory fixture. The complete Python suite passed 2,710 tests and 66,696
subtests, with 131 skips and six failed subtests. All six came from two IHV
renderer fixtures, which compile the actual `RB_SurfaceEntity` with stubs
and had none for the new `RB_SurfaceJKA`. With the stub added, both pass
(9 tests, 12 subtests). On the final content, the document,
release-workflow, IHV renderer and this round's tests pass: 158 tests and
1,684 subtests. Runtime probes used the fixed folders
`.tmp/runtime/round-1014-after` and `-before`, the latter differing only in
HEAD's import 118. Clients ran windowed with loopback-only networking, on a
private desktop for Academy and on screen for Medal of Honor's rcon probe.
No input was injected, and the installations were unchanged.

## Remaining and unrelated findings

- Academy's authored `glow` shader stage keyword is still reported as an
  unknown parameter. The stages draw, but nothing feeds Fn's glow and bloom
  from them.
- Jedi Outcast's render adapter still refuses every non-model entity; its
  sabers need the same types once its client game passes import 300.
- The software mixer gives Medal of Honor's music stream `s_musicvolume` but
  not `s_volume`, while OpenAL's listener applies both. This difference
  predates this round.
- Another session's uncommitted Alice effects work (record dated 2026-10-12)
  is in the shared tree and is not part of this round.
- Twelve September Jedi records -- the 2026-09-21 Academy contracts, ICARUS,
  maps, models, module ABI, records, ROFF and runtime records, the 2026-09-22
  Academy ICARUS runtime and trace records, and the 2026-09-22 Outcast
  contracts and records -- still lack the same-stem JSON companions
  AGENTS.md asks for.
- The Jedi Outcast identity row still says fnTech3's client stops at UI
  import 200, a frontier the later audits above its table supersede.
