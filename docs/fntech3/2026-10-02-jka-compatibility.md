# Jedi Academy protocol and client / single-player boundaries

Both Academy profiles remain experimental and gated. This slice adds the
multiplayer protocol-26 messages, client record and font adapters, and begins
the retail single-player game boundary. Retail MP menu startup and a narrow
animated UI model scene now have runtime evidence. Campaign, complete rendering,
savegame and bidirectional retail interoperability gates remain open.

## Evidence and provenance

The implementation is independently written from contract observations of the
legitimate 1.01 installation at `E:\Games\Jedi Academy` and the GPL-2.0-or-later
[OpenJK reference](https://github.com/JACoders/OpenJK/tree/1a6a643427aa347553e9073dac5570b33337c4d9),
pinned at `1a6a643427aa347553e9073dac5570b33337c4d9`. No retail module or asset
is committed. Runtime probes use unchanged installed paks, stable project-local
executables and isolated writable homes; games are windowed on unswitched
private desktops, without mouse or keyboard control. Screenshots, when reached,
are requested through the engine's registered command.

| Retail file | SHA-256 |
| --- | --- |
| `jamp.exe` | `e8062d3948ff05917b48e47d8eec0ee43bcc6fb03924410712412404ddfee28b` |
| `jasp.exe` | `9351a8de230fcd577e2b474889d08f38982450306767a6823dd659482566d6ce` |
| `jagamex86.dll` | `6b87b804137f56f3bb8901235788c735e6c36cf1bea42cab48dad7f2d15b6ee3` |

The source checkout HEAD is `a1251ede2c382190b18c154b45357f6979d8171c` with
uncommitted changes. Executable hashes and full command lines accompany each
runtime result under `.tmp/jka-compat/`; HEAD alone does not identify its build.
The [machine-readable companion](2026-10-02-jka-compatibility.json) distinguishes
source, unit, live and attempted interoperability evidence.

## Multiplayer messages

Protocol 26 selects Academy's own immutable wire contract. The same number also
belongs to ioEF; lookup now uses the selected profile rather than assuming a
protocol number uniquely identifies a game. Academy keeps qport and reliable
command XOR, a 128-command reliable ring and a 49,152-byte message limit.

The frozen record codecs carry 132 entity fields, 137 normal-player fields,
140 pilot fields and 69 vehicle fields, their sparse arrays, keyed user
commands and the second vehicle player-state body. Mod-supplied
`ext_data/MP/netf_overrides.txt` and `psf_overrides.txt` are parsed transactionally
and reset at filesystem transitions. Installed files contain comments only;
default widths remain active. JKA preserves high string bytes; its checksum and
XOR filtering retain their distinct percent-character behavior.

The generated oracle corpus matches the pinned reference encoder for all
25,192 Huffman-compressed packets on Windows x86 and x64. It covers every field,
six boundary values, eight bit alignments, player arrays and input fields.
The engine's actual netchan reassembles bodies through the signed-short boundary
and up to 49,152 bytes in both directions, including a zero-length terminal
fragment, and rejects truncated, out-of-order and oversized input. Existing
message-codec checks cover Q3, QL, RTCW and Elite Force alongside Academy.
These are unit and source evidence, not packets observed from a retail peer.

The forward maintained-peer attempt exposed a handshake error: accepting a
legacy challenge had selected Quake III's protocol 68. Academy now sends its
own challenge request and keeps protocol 26 through challenge and connect
responses. The retail dispatcher sends `challengeResponse %i` without an echoed
nonce at `0x4571a5`/`0x457347`, and uses a bare `connectResponse` at
`0x457c30`. The client accepts those shapes from the expected endpoint, as
well as a correctly echoed nonce from the maintained peer; integer overflow,
extra tokens, wrong nonce or protocol and foreign bare replies refuse before
connection state changes. Connect replies require the exact endpoint. Tests
execute the production branches and preserve other profiles' protocol choices.
The retry receives gamestate and loads retail cgame. The next owned-model
slice passes initialization and bolt metadata, then reaches the named ambient
precache import `CG_AS_ADDPRECACHEENTRY (50)` in maintained forward run eight.
The next slice parses the two selected ambient definitions in run nine and
advances to `CG_FX_REGISTER_EFFECT (263)`.
Active admission and complete cgame rendering remain unestablished.

The shared compact-float decision now uses binary32 integer bits, so the
engine's fast-math options cannot collapse exceptional values or misclassify
the signed compact range. Complete message tests pass with and without
fast-math; the Academy oracle packet digests remain unchanged.

Snapshot admission uses Raven's separate `broadcastClients` mask, including
viewer 31, self visibility and its precedence over global broadcast and PVS.
It does not interpret JKA's flag 0x2 as Quake III's `singleClient` mask.

Retail message framing is independently confirmed: `jamp.exe`'s decoder
compares with **10** at `0x42268c`, its transport writer emits 10 at
`0x4616f1`, and its gamestate writer emits 10 at `0x4580f7`. The diagnostic
table at `0x5707c8` identifies setgame as **8** and mapchange as **9**.
Both gamestate and outer message terminators now select the profile's value.
The server publishes a validated game directory before gamestate baselines,
so mod field-width overrides are active before decoding. The client reads
a bounded byte string, refuses truncated or oversized input before mutation,
ignores invalid paths, and remounts only when the validated mod changes.
This changes the profile's mod search path without changing its identity.

The first OpenJK-to-fnTech3 admission attempt receives gamestate and loads
retail cgame, then reads past the message end. The missing field is Academy's
16-bit RMG trailer after checksum feed. Retail calls its reader at `0x4222e6`
and writes a zero static-map length at `0x458760`. Live, demo and size-budget
writers now emit that zero; the client consumes it and explicitly refuses a
nonzero terrain payload before parsing another opcode. Production framing
tests preserve other profiles' tails. The next maintained-peer retry joins the
retail game module, reaches the active client state and receives 386 distinct
snapshots, including deltas. Its engine-written capture comes from OpenJK's
renderer and shows the player-configuration overlay; it does not establish
fnTech3 rendering or controlled gameplay. Disconnect then reaches the named
missing game import `G_G2_CLEANMODELS (605)`, so the strict run remains failed.

## Single-player bootstrap

Retail `GetGameAPI` copies 131 import slots and publishes API **8**, not 10.
`jagamex86.dll` writes 8 at VA `0x20073472`; `jasp.exe` independently checks 8
at `0x439891`. The module publishes a 1,252-byte entity stride at
`0x20073512`. Its 19-member export table is 76 bytes; four bytes between the
export and import storage are alignment, not another export. OpenJK's modern
savegame interface changes the three retail save callbacks into one pointer,
so that modern table is not copied into this boundary.

The bridge validates the table, publishes ownership before module calls and
tracks file handles and allocated file buffers through partial initialization.
It refuses unsupported callbacks by their SP names and avoids calling a module's
shutdown after incomplete initialization. Entity state (272 bytes), shared
prefix (376 bytes), command (28 bytes) and trace (1,080 bytes) have separate
frozen records. Retail player state is separately frozen at 4,916 bytes, with
50 measured scalar delta fields and the five sparse arrays. Its private suffix
remains opaque and no live player-state adapter is enabled. Retail's SP cgame
snapshot is 144,248 bytes and includes ping at offset 4, which the maintained
reference's current snapshot omits. Neither record is cast to multiplayer or
Quake III storage; save data remains unresolved.

The first private-desktop probe loads retail `t1_sour` collision and invokes the
real module's API-8 initialization, reaching the named missing callback
`GetConfigstring (25)`. It does not crash or establish a playable level.
Configstring, userinfo and serverinfo callbacks were subsequently implemented.
The second live probe advances to `G2API_PrecacheGhoul2Model (45)`, with no
crash, fatal error or hang. That callback and its model-name consumers were
subsequently implemented. The third probe advances to
`RE_GetAnimationCFG (122)` without a crash, fatal error or hang. Its measured
bounded text cache was then implemented. The fourth probe passes that callback
and reaches single-player cgame import 9, file opening, without a crash, fatal
error or hang. The fifth probe passes that early cgame filesystem slice and
reaches `G2API_InitGhoul2Model (46)`. It has no crash, fatal error or hang;
that result predates the owned metadata slice below. The sixth probe passes
model initialization and reaches the named Ghoul2 trace-geometry refusal.
Its error cleanup then fails recursively during retail DLL destruction, exits
with status 1 and is recorded as a failed run. A late array Delete callback
after the engine invalidates handles was corrected with a closed-lifetime
handle set. The seventh probe reaches the same trace refusal with selector 2,
LOD 0, passEntity 88 and contentMask `0x1381`, then recovers without a second
error. The harness stops that recovered process; this is error-unload recovery,
not graceful process-exit evidence. No playable level is established.

The eighth probe passes the prior trace refusal and reaches `WE_IsShaking
(128)` without a recursive or fatal error. World obstruction now shortens the
entity candidate interval, with checked fraction remapping and temporary SP
body contents restored after each query. Nonempty Ghoul2 geometry candidates
remain explicitly refused. The process is recovered and stopped by the
harness; that run did not directly trace Init returning. The tenth probe now
prints the engine's phase marker after retail API-8 Init returns, its export
table validates and its entity arena binds. Its first frame reaches the same
weather refusal without a recursive or fatal error. Init return is observed;
client admission, campaign and graceful exit remain open.

The subsequent collision correction uses SP `SOLID|BODY` (`0x101`) when
publishing a nonbrush entity's packed solidity. Point queries use native state
origin/angles and a leased `0x100` temporary BODY hull; normal completion and
error cleanup restore the shared hull. Map-wide contents starts with `0x100`
only for SP and preserves every loaded brush/patch flag. Actual production
tests check all profile seeds, genuine `0x02000000` asset flags, invalid
broadphase counts and slots, finite geometry, exclusions and partial-query
cleanup. These BODY corrections have source/unit evidence; campaign collision
and nonempty Ghoul2 triangle queries still require runtime evidence.

The subsequent world slice serves 46 callbacks and one VoiceVolume data slot,
with 84 named refusals. A fixed checked 1,252-by-1,024 entity arena feeds the
engine's world queries through the SP adapter. Tests retain entity private-tail
and outer canaries, reject foreign/misaligned pointers and missing player data,
and round-trip every entity-state byte. Ordinary collision traces and the
measured selector's world/candidate filtering are served; actual Ghoul2
geometry collision remains refused.

The SP Ghoul2 array exposes the retail MSVC vtable in the measured order:
New, Delete, IsValid, const Get, mutable Get. The retail resize path calls
slot 4 at `jagamex86.dll:0x20132806`. Empty handles use a checked 512-entry
pool; mutable vector access stops before crossing the unresolved retail C++
allocator boundary in the earlier implementation. Tests call the raw vtable
offset as well as named fields. The current dedicated metadata slice owns
stable 204-byte model records and their canonical vector buffers independently
of guest-visible headers. Init validates the GLM and matching GLA before
publishing a record; invalid assets leave no new handle or slot. Mutable Get
can expose those owned records, while modified begin/end/capacity headers are
refused and cleanup frees canonical storage without following guest pointers.
The retail DLL's resize helpers are reached only through its distinct cgame
exports; all SP cgame exports remain explicitly refused before module entry,
including outer/nested vector resizing. Engine-private model pointers and
validity stay empty, so this is model metadata initialization rather than a
pose or renderer implementation. The sixth live probe exercises this metadata
initialization. Closing a lifetime frees canonical buffers and retains only
acquired identity tombstones for the retail DLL's late Delete callbacks.
Exact late aliases delete idempotently, while New/Get/Init stay closed and
arbitrary identities refuse. The next module load opens a fresh lifetime.
Simulated DLL-detach tests cover normal scalar changes and corrupted guest
vector headers; the seventh live probe confirms recursive error recovery.
The first thirteen SP client pre-initialization imports have a separate adapter;
their 272-byte cvar record has no flags member and places its string at 16.
Argument joining preserves retail's trailing whitespace, and cgame file
read/write/close calls retain their distinct zero-return contract. Tracked
handles close exactly once when a read abandons initialization.
Primary disassembly identifies separate private heaps in the retail module and
engine's nonempty Ghoul2 allocation paths. Shared allocator ownership remains
unproven. Owned records may be inspected and their scalar metadata changed;
retail-driven vector resizing and private engine buffers remain refused.

Model precaching uses an owned, bounded handle registry and the engine's GLM
and GLA readers. It loads a mesh's declared skeleton in reservation order,
caches failed names, and releases partial reads on errors. The generated
`*default.gla` is independently described as a one-bone, one-frame identity
skeleton; no proprietary serialized blob is copied. Model animation-name
queries read the registry's own stable metadata. Retail-owned nested vectors
and pose consumers still refuse before the unresolved allocator boundary.
Animation configuration uses an independently owned text cache, with bounded
reads, full-length query results, terminated/truncated output, successful-name
cache semantics and tracked cleanup when a read abandons initialization.

## Multiplayer UI records and fonts

The legacy UI reports API 7; modern OpenJK's separate API 3 does not replace
that retail version. Browser sources are translated around the engine's retired
MPlayer slot, cvar flags preserve their Raven meanings, and frozen client state,
parser token, time and cvar outputs are marshalled field by field.

Retail UI import 44 copies 19 dwords at `0x42470f`: a 76-byte glconfig with four
32-bit pointers to engine-owned strings. Its width and height are at 56 and 60.
This differs from Jedi Outcast's large inline-string record. The converter
keeps stable renderer-owned strings and supplies backend texture capabilities
through a new engine renderer export; renderer API **33** also appends the
owned skeletal-model bridge and requires matching
client and renderer modules. Existing external Q3/QL record sizes remain frozen.

All seven installed 7,180-byte font records read through the bounded shared
Raven glyph reader. Three are byte-identical to Outcast's fonts. Academy's
`arialnb` has zero height/ascender/descender metadata; its point size is 14.
The engine derives usable layout metrics for that authored font and submits its
glyph atlas through fnTech3's active renderer and HUD/menu coordinate transform.
This preserves the existing renderer features. Float guards remain effective
under the engine's fast-math build options by inspecting encoded exponents.
Asian multipage fonts and language-specific font substitution remain open.

StringEd reads installed packages through fnTech3's filesystem. All 215 retail
`.str` records parse, yielding 13,931 package/reference entries. Western
language discovery, selection, bounded output and byte-wise character reads
are served. UI's missing-key result remains true with an empty string; cgame
returns false with `??`. Language caches reset at module and mod transitions.
Optional `.ste` overrides and non-Western codepages remain open.

The first live UI probe loads the retail API-7 module and reaches its raw key
binding lookup, import 35. A measured read-only adapter now maps Academy's
different key slots to existing engine binding metadata, preserving uppercase
letter storage and empty unmapped slots. It neither reads nor injects physical
input. Cleanup tracks whether UI initialization returned, skips reentry into
an abandoned native module, and defers reload until error recovery completes.
The second live probe passes that query and reaches global parser definitions,
import 71. It no longer reenters the failing UI during error cleanup. After that
adapter is added, the third probe reaches an unquoted localized menu name at
`ui/jamp/main.menu:186`, then source diagnostics (70). Academy's source loader
now accepts its leading `@` names without changing other profiles' lexer rules;
the installed menu parses 955 tokens, including 16 such names, without errors
or retained allocations. Source loading and bounded diagnostics (67/70), global
define loading/removal (71/72), and guarded key-catcher metadata (41/42) have
production-parser and host tests. These metadata calls do not capture or
inject physical input. The fourth live probe passes those parser calls and
reaches `UI_G2_INITGHOUL2MODEL (208)`. The same boundary stops the maintained
fnTech3-to-OpenJK admission attempt before connection.

The next slice owns a separate UI Ghoul2 metadata store. It validates a GLM
and matching GLA, manages model slots and bounded animation/surface names, and
clears resources after successful or abandoned module initialization. Pointer
slots are four-byte retail records even when unaligned. Nonzero renderer,
skin, flag and LOD modes remain explicitly refused. The fifth live UI probe
passes initialization and advances to `UI_G2_PLAYANIM (213)`. The
following animation adapter preserves the ten measured argument words,
including float speed and fractional seed, in the owned timing store.
Posing still refuses its unsupported modes. The owned geometry bridge below
subsequently enables a bounded UI scene.

Retail cvar registration at `0x43a350` creates the variable before checking
its nullable output. UI import 51 now honors that ordering, so the installed
menu's null-output registration succeeds. The 272-byte external record is
staged through aligned storage; denied private registrations and unchanged or
invalid updates preserve caller bytes. Tests use the real cvar implementation,
unaligned records and surrounding canaries. UI point-light import 25 is also
served through the existing Fn light pipeline, with finite, bounded inputs;
retail dispatch at `0x4244ee` confirms its four float words and zero return.
The seventh retail UI startup probe exits normally and writes an engine PNG
showing the MP menu. A Vulkan repeat also displays the menu. These are startup
and presentation evidence; no navigation or physical input is exercised.

Retail's 68 shader files live under `shaders/`, with none under `scripts/`.
All three renderers now collect that profile-specific directory while retaining
engine and user `scripts/*.shader` supplements and fnTech3's `.shaderx` paths.
The existing tokenizer, bounds, hash precedence and cleanup have executable
tests. Academy's `noTC` directive maps to the active renderer's existing
no-compression image flag. Additional Raven shader directives, including glow,
remain open; collecting their files alone does not establish correct rendering.

The third UI probe also exposed top-origin glyph atlases. The shared TGA loader
now honors Academy's measured vertical origin for uncompressed images; 17
installed atlases/images use that flag. Other profiles retain their existing
orientation policy. Oriented RLE input is refused before pixel allocation, as
in the retail decoder. Decoder tests cover row order, color formats, malformed
input, cleanup and profile isolation. This correction uses the existing image
and font rendering pipeline, now exercised by the retail menu captures.

Extensionless Academy `videoMap` paths now receive `.roq` before lookup or
cache reuse. Retail `jamp.exe` applies that extension at `0x41671e` before its
cache scan at `0x416738`; the installed `video/ja01.roq` is a 30-fps RoQ movie.
Bounded pathname tests preserve explicit extensions, cache identity, other
profiles and Elite Force's separate Bink path. The fourth UI probe no longer
reports a missing `video/ja01`.

Academy's supported world views now use the existing FnQuake3 FOV correction
in all three backends. Retail MP's `0x30061ac2..0x30061bad` and SP's
`0x20145e00..0x20145e41` derive vertical FOV from an unchanged authored
horizontal angle and viewport dimensions; their retail modules do not contain
the maintained port's `cg_fovAspectAdjust` option. The Fn presentation policy
preserves 4:3 vertical framing on other viewport shapes, while menu views keep
their authored projection. `r_fovCorrection 0` retains the module's FOV.
Production projection tests cover both Academy modes and existing profiles;
live Academy aspect behavior remains unproven.

## Multiplayer client-game records

Retail `cgamex86.dll` (`assets3.pk3`) has SHA-256
`e5d5525829bfcaf3c0428d87fe804e5a46a75dcf0a141f8288061a3970ff140d`.
Its `vmMain` at `0x300425a0` consumes 3, 0, 0, 3, 0, 0, 2, 2, 1 arguments
for exports 0 to 8. A C whitelist keeps modern QL exports from reaching Raven
callbacks by numerical coincidence. Mapchange uses engine semantic export
`CG_RAVEN_MAP_CHANGE`, mapped to Raven's ordinal 27 with no arguments.

The bounded host converts game state, snapshots including vehicle state,
default entity state, user commands, cvars, parser tokens, time and packed
collision traces. Snapshot stride is 532 and its total record is 139,352 bytes;
native outputs use `memcpy` and preserve surrounding canaries. Sound names,
loop clearing, model/shader registration and font glyph submission reach the
existing fnTech3 services. Static entity and view records are validated and
converted into the engine's renderer, including the existing UI viewport and
HUD transforms. Ghoul2 pointers, unsupported Raven render flags and types,
effects, ambient playback and input overrides stop by their own names.
These adapters have unit evidence; they do not establish a rendered frame or
the complete spatial audio/input contracts. SP's client admission remains refused.

The cgame Ghoul2 metadata adapter now owns a separate bounded model store,
independent of UI and server lifetimes. Imports 293, 297, 301, 306 and 310
load validated GLM/GLA pairs, query real model presence, publish the asset's
animation name and release owned identities. Primary retail dispatch confirms
that Has (310), like UI Has (220), receives an address of a four-byte identity
slot; Have receives the identity itself. A missing GLA name leaves the caller
buffer unchanged, also corrected in UI. Failed loading and stale/foreign
identities preserve caller slots. This first slice refuses nonzero Init
selectors; the later skin slice below serves its custom-skin handle. Tests cover unaligned slots, canaries, short
reads, containment, model/instance caps and the actual client cleanup hook.

Imports 317 and 319 now record a real named bone/surface bolt and its opaque
attachment descriptor. Retail dispatch maps only the bolt name, returns the
lookup result, and explicitly returns zero for the void descriptor setter.
The adapter retains that distinction and bounds every string and model index.
This metadata introduces no renderer path; cgame Ghoul2 scene submission and
attachment rendering remain refused. Maintained forward runs seven and eight
advance through model initialization, descriptor storage and bolt lookup to
the ambient precache boundary (50). The following metadata slice passes that
boundary and reaches effects registration (263). No active cgame frame is established.

The earlier maintained forward retry reaches cgame's missing Ghoul2 import 297
during Init. Shutdown now distinguishes completed initialization from an abandoned
native callback, keeps owned resources alive during a successful shutdown and
skips native reentry during error recovery. The next retry reports only the
named missing import, with no recursive shutdown error or crash. It still
fails admission and is stopped by the harness.

## Ambient definitions and MPEG audio

Cgame imports 49, 50 and 52 now own selected sound-set names, parse
`sound/sound.txt` and return registered mover-stage sound handles. Retail's
dispatcher calls them at `0x412a8a`, `0x412a98` and `0x412ae0`.
`AddSet` at `0x467270` establishes the 10–25 second interval, volume 255 and
radius 250. The parser preserves the three category passes and the last
duplicate-name lookup; authoring directory headers never become runtime paths.
Names, file size, definitions, subwaves, numeric ranges and contained wave
paths are bounded. Failed parsing preserves the previous table, and cgame
unload closes owned files and clears registrations through partial failures.

The installed 41,044-byte definition file has SHA-256
`8ec32a43e7d233e0ead8c8e47ddddfdc7f304657d3ed0e78e13a9132c2c95e30`.
All 234 definitions, 231 unique names and 551 wave references parse in the
asset probe. Eighty-two unique references resolve to installed MP3 files;
all decode completely through the existing Fn frame walker and minimp3
configuration. Academy's two profiles now register that codec. Production
tests preserve the other profiles' codec lists, WAV priority and stream
fallback, including builds with MP3 disabled.

Retail parses these definitions even with sound disabled. The fresh maintained
forward probe observes two selected names and two parsed sets, then refuses
effects registration (263). It uses the stable Windows x86 client, pure server,
windowed private desktop and unchanged installation; its homes are removed.
It has no active admission or snapshots. Source and unit tests also cover
invalid stage queries, real zero handles when audio is disabled, malformed
spans, missing/short files and error cleanup.

Registration uses Fn's sound services and decoder. Ambient playback imports
48 and 51 remain refused, so scheduling, crossfades and mixed output are not
established. Distance, occlusion, EFX, audio zones and HRTF remain the intended
Fn playback features and are not exercised by this silent metadata probe.
No retail presentation default or comparison option is introduced.

## Effect declarations and registration

Cgame import 263 now reads contained `.efx` files into an engine-owned graph
and registers their shaders, models, sounds and effect dependencies through
Fn services. Retail dispatch at `0x4133e0` calls `0x4064d0`; its cache strips
at the first period and lowercases the key in place. Literal `effects` prefixes
remain part of the key. Missing/empty/oversized files return zero; malformed
graphs stop with a named diagnostic. Pending identities preserve dependency
cycles, and partial errors or normal unload close files and clear ownership.

Retail's allocator starts at identity one, stops before 256 and sets repeat
delay to 300 at `0x406d53`. Its 13 recognized primitive kinds match all 1,503
recognized primitives in 376 installed effect files. All files parse with
1,731 resource references and 10,950 explicit numeric parameters; 25
`forcefeedback` groups are ignored, as in retail's primitive-name dispatch.
The maximum installed declaration has 15 primitives; the bound is 24.
File bytes, names, tokens, tree depth/nodes, lists, paths, numeric arity and
finite values are bounded. Numeric validation uses an IEEE bit check under
fast-math. The parser keeps ordered parameters and curve data for the future
Fn effect playback path; it does not substitute a retail renderer.

The production registration and parser checks pass, including dependency
cycles, cache aliases, actual zero backend handles, every resource error,
filesystem failures and engine `longjmp` cleanup. The fast-math asset probe
passes every file. A separate unsanitized 10,000-mutation run completes;
optional x86 AddressSanitizer validation is unavailable in this LLVM install.
The fresh maintained forward probe registers 6 effect declarations
and advances to `CG_FX_INIT_SYSTEM (271)`. It remains a failed admission probe;
effects playback and an active rendered cgame frame are not established.

Registration uses Fn shader/model/audio services and the existing decoder.
Per-pixel/static lights, shadows, effect scheduling and spatial-audio output
are not exercised by this silent metadata probe. No retail-exact presentation
option or default is introduced. No external implementation text or retail
assets are incorporated; format facts from
[pinned OpenJK](https://github.com/JACoders/OpenJK/tree/1a6a643427aa347553e9073dac5570b33337c4d9)
remain separate from primary retail observations.

## Effect context lifecycle

Imports 271, 272 and 273 now initialize the engine's instance pool and
clock, bind a bounded nullable borrowed view and free owned definitions.
Retail init at `0x40f210` clears the 1,800-instance pool on its first call,
resets the free cursor on every call, registers `fx_debug=0`,
`fx_countScale=1` and `fx_nearCull=16`, resets the helper clock and stores the
view without reading its contents. The default values and flags here are
primary observations rather than maintained-port defaults. Retail view
binding at `0x41354d` accepts null. Free at `0x40f1c0` clears instances and
templates, keeps the initialized flag and helper view/clock, and returns one.

The engine owns its instance records separately from Raven's structures.
Repeated-init, nullable-view, record-bounds, definition cleanup, control
registration failures and `longjmp` recovery pass the production checks.
This initializes the graph service; scheduled playback/rendering remains
named and unimplemented. Fn resource services and controls are used; effect
lighting/shadows, spatial-audio output and input are not exercised.

A first comparison attempt ends during OpenJK server readiness before the
fnTech3 client starts. The next two private/windowed attempts initialize the
context and register 107 effect declarations with 444 primitives, then reach
`CG_G2_INITGHOUL2MODEL (297)`. Its arguments are measured as model index zero,
skin one, shader zero, flags zero and LOD zero. Those earlier attempts stop
at the nonzero skin selector. All tested binaries and the installation remain
unchanged during the runs, and their homes are removed. These probes do not
establish client admission, active snapshots or a rendered cgame frame.

## Cgame skins, animation and registration controls

The next native slice preserves Init's custom-skin word in the selected owned
model. Retail stores it at offset 56 in its 200-byte model record. Cgame skin
registration (55) uses actual Fn renderer handles and retains independently
parsed declarations. All 305 installed skin files pass the production parser:
13,828 surface bindings, including 6,610 `*off` bindings. SetSkin (298) updates
the custom handle and the selected model's bounded surface visibility. A zero
render-skin word preserves its existing visibility; a nonzero selection clears
old overrides and applies the measured `OFF`/`NODESCENDANTS` mask. Default-off
surfaces stay off unless the declarations require an override. Composite skin
names and unknown renderer identities remain named refusals.

PlayAnim (303) preserves the two encoded float words, retail frame/seed clamps
and fractional start-clock behavior. AngleOverride (302) copies the bounded
twelve-byte vector and its axis, mode and blend/clock selectors into the same
owned store. The retail core does not consume its model-list word. Authored
production fixtures reach real Fn deformation matrices for supported modes;
unsupported modes and angle blending still fail before posed submission.
Actual cgame geometry, skin-material consumption and bolt attachments remain
open. This uses Fn's existing skeletal and renderer-resource paths; it adds
no retail presentation option or per-pixel/shadow rendering evidence.

Sound registration control (45) changes optional missing-sample diagnostics.
Its retail handler stores one scalar and returns zero. The pinned OpenJK
reference separately identifies the diagnostic use; Fn applies that use to
its codec and legacy registration warnings, preserving handles, fallback
attempts, streaming diagnostics and playback. Other profiles keep their
diagnostics. Normal/partial cgame teardown and sound shutdown reset the flag.
Cache imports 339–341 reproduce the retail handler's zero return without
reading another argument. These listen-server cache optimizations are distinct
from actual bolt attachments; Fn retains separate owned module poses.

Focused native/host and cross-profile codec checks pass. Files, pending names
and declarations survive the tested failure boundaries through owner cleanup,
including actual engine longjmp recovery. The production forward probes pass
skin, animation, angles, warning control and cache cleanup, then stop at
`CG_S_STARTBACKGROUNDTRACK (47)` with its preload-only word set. They establish
no active snapshots or rendered cgame frame. Attempt 22 records an independent
OpenJK server access violation before fnTech3 launches; fresh attempt 23 with
the same binaries reaches the preload boundary. All installations/binaries
remain unchanged during each probe and the isolated homes are removed. A
sixth complete regression suite now covers this source with no failures.

## Explicit music preload and effects clock

Cgame music-start import 47 now retains the preload-only word. The sound-disabled
facade performs no file or decoder work. With sound enabled, explicit tracks
use bounded 64-byte names and at most two actual Fn codec stream owners. The
preload stops background playback, initializes those decoders and keeps them
without queuing audio. A later ordinary stream open transfers the existing
decoder to the Fn music player. Empty/disabled starts, cgame error/unload and
codec teardown release cache ownership; streams already transferred belong to
the normal player. Preload reads are bounded to 64 MiB per file. Symbolic
dynamic-music sets still refuse by name.

Retail's optional wireframe-automap initializer (225) returns false when
`r_autoMapDisable` is nonzero. Its measured registration defaults to one.
That disabled mode is served without generating a cache; explicitly enabling
it refuses by name until the Fn renderers own an Academy wireframe-map path.
This does not establish automap rendering or turn off Fn lighting and shadows.

The effects clock (274) publishes absolute time, previous time, signed wrapping
32-bit frame delta and float seconds in the owned context. A nonpositive input
preserves the clocks and clears the step. The retail helper rounds the integer
delta to float before multiplying by its measured `0.001f` constant; the Fn
adapter preserves that order without signed-overflow undefined behavior. Init
resets the clock and free preserves it. Scheduling and effect playback remain
named boundaries.

Native and production-function fixtures cover names, aliases, decoder ownership,
normal-player handoff, missing/oversized files, other-profile refusal, paused and
backward clocks, word wrap, precision, partial initialization and engine longjmp
recovery. The Windows x86 production build and focused checks pass. In muted
candidate and production probes, the default Fn OpenAL backend initializes and
preloads one decoder for the installed `music/mp/MP_action4.mp3`; playback and
spatial output are not established. Production attempt 26 receives one snapshot
before the clock boundary. Attempt 28 records the independent OpenJK startup
access violation; fresh attempt 29 with the same binaries receives three
snapshots and stops at `CG_R_SET_LIGHT_STYLE (220)`. All runs remain failed
compatibility probes, with no rendered cgame frame, complete admission or clean
disconnect established. Installations and binaries remain unchanged and the
isolated homes are removed. The seventh complete suite and source/runtime audit
pass for this music/clock source, before the light-style changes below.

## Bounded light-style metadata and empty client ROFF update

Cgame light-style imports 219 and 220 now own 64 RGBA colors, initially white.
The setter decodes the packed little-endian scalar and the getter writes four
bytes to the caller's buffer, including unaligned destinations. Invalid indices
and wider native words refuse. Retail writes the caller buffer; the pinned
OpenJK getter instead casts the address of its local pointer as the destination.
That source observation is kept separate from retail evidence and does not
establish an OpenJK runtime failure.

The current RBSP projection accepts only normal style-zero worlds. Style zero
therefore accepts neutral RGB with its actual alpha, and colored modulation
refuses without changing the stored color. Other indices retain their actual
metadata but cannot be referenced by a successfully loaded world. The five
extra-style maps still refuse. Neutral lighting retains Fn's existing static,
per-pixel and shadow paths; this slice establishes neither colored radiance
modulation nor live world rendering. Unload resets the owned colors.

Client ROFF update (284) now serves the measured empty client lane. Retail's
dispatch reads no additional native argument and both loops do no work when
that lane is empty. Client cache and play remain named refusals, so the adapter
cannot admit a trajectory. The existing server ROFF player remains independent.
No client motion playback or trajectory advance is claimed.

The production build and eight focused checks pass, including caller canaries,
packed colors, index bounds, reset, empty native calls and separate server
ownership. Candidate attempts 30 and 31 advance past these boundaries, and
production attempt 32 confirms `CG_R_GETDISTANCECULL (222)` as the next refusal.
It receives no validated snapshot and establishes no active admission, cgame
frame or complete clean disconnect. The private test stays windowed, injects
no input, leaves the installation and binaries unchanged and removes its homes.
The eighth complete suite and source/runtime audit pass for this source,
before the later renderer-owned distance and token-iterator changes.

## Renderer distance and entity-token iteration

MP import 222 now copies the actual loaded Fn world's distance-cull float into
the caller's bounded four-byte span. Retail's getter at `0x4132cf` reads the
renderer global; initialization at `0x490c11` establishes **6000**, and the
worldspawn parser accepts an authored `distanceCull` override. Read-only
inspection finds 57 installed maps: 46 omit the key and 11 author values from
1800 to 10240. This is contract data, not a replacement rendering pipeline.
All three Fn renderer modules own and validate that value, publish it only
after complete world loading and cap their existing radial far plane at the
distance times sqrt(3), with the existing near-plane bound. Fn PVS, FOV, fog,
viewport and lighting remain in use. The internal renderer API is now 34;
existing export offsets and external game-module records are unchanged.
SP's visibility policy and other profiles are unchanged.

The first candidate reaches the subsequent entity-token reset and fails with
`Q_strncpyz: NULL dest`. Retail helper `0x490f30` handles size **-1** before
touching the buffer: it resets the world entity cursor and returns one. Normal
iteration returns zero when the cursor or token is empty, without automatic
rewind. Import 261 and each renderer now serve that MP contract, including a
null reset buffer, bounded normal output and repeatable end-of-stream behavior.
The other profiles retain their own iterator semantics.

The production preset build completes 187 targets. Eleven focused groups and
25 additional source tests pass. Authored fixtures execute the actual parser,
renderer getter/iterator and far-plane functions in C and C++, including
optimized fast-math builds, malformed values, partial world ownership,
unaligned caller buffers, bounds, reset/EOF and the other 15 profiles. These
fixtures and a successful build do not establish a complete rendered world.
The preceding eighth combined suite predates this change; a fresh combined
checkpoint is required.

Candidates 35, 36 and 37 reach `CG_G2_DUPLICATEGHOUL2INSTANCE (309)` on OpenGL,
Vulkan and the RTX module, respectively, with 94, 52 and 59 valid snapshots.
Production attempt 38 reaches the same import with 74 snapshots and logs the
actual no-override value of 6000. Each receives an unpure drop: its proof has
one module checksum before `@`, missing the UI checksum after pinned reload.
At attempt 38, the source omitted Academy from pak re-verification on that
path. The later causal repair and maintained pure-admission subset are
recorded below. No attempt
establishes active admission, a delta snapshot, a cgame frame or a complete
clean disconnect. Attempt 34 has a peer startup access violation before the
client launches. Authored-map attempt 39 fails peer readiness before client
launch, so it establishes no runtime override. All attempts retain their
failed results, leave installed files and tested binaries unchanged and remove
their writable homes. Launched clients remain windowed with input disabled.
The JSON companion carries the primary observations, source hashes, exact
commands, module identities, per-attempt outcomes and candidate build records.


## Native pak proofs, Ghoul2 copies and cgame menu relay

Academy's pinned native UI and cgame reload now verify the actual selected
allowed pak member before loading. The verifier compares its bytes, length
and CRC, restores file-reference flags after the read, and refuses a
mismatched highest-precedence allowed entry. Existing RTCW and ET checks use
the same verifier; unrelated proof policies and game-module loading retain
their contracts. Candidate 40 restores the missing UI checksum, reaches
active admission and receives 57 snapshots, including deltas, without an
unpure drop. Its cgame still stops at duplicate import 309.

Primary retail dispatch and helper observations establish imports 309 and 308
as void calls with different pointer shapes. Duplication publishes an owned
copy through a checked four-byte caller slot, supports unaligned storage and
preserves a populated destination. Specific-model copy receives two opaque
identities and two scalar indices; it supports replacement and exact append.
Both use the actual Fn Ghoul2 Store, retain independent skin and visibility
bindings, and share only immutable loaded asset bytes. Source cleanup leaves
the new instance valid. The instance budget is 1,024 and each model list is
bounded at 32; sparse destination gaps remain named refusals. Candidate 42
advances past duplicate to copy with 77 snapshots, and candidate 43 advances
past copy to menu import 238 with 59. Complete skin/material and pose drawing
remain open.

Menu import 238 relays selectors 0 through 5 through the existing VM/UI
dispatcher and its export whitelist, argument counts and nested-call
lifetime tracking. Selector 3 is Academy's player configuration menu, distinct
from Quake III's existing menu 3. A new canonical ID is appended without
renumbering any existing one. Requests require the initialized x86 legacy
native UI; unsupported selectors refuse by name. The UI retains Fn's
HUD/menu transforms and renderer services. Candidate 45 requests selector 3
and reaches read-only routing getter 243 with 55 snapshots. That getter now
uses the existing engine scalar routing service; no key event, button state,
physical input capture or injection is tested or claimed.

The final candidate 48 and production attempt 50 advance to
`CG_SETUSERCMDVALUE (235)` with 59 and 73 snapshots respectively, including
deltas. Both reach server active state and ClientBegin with accepted pure
proofs and no unpure drop. Neither completes a cgame frame, an engine capture
or a clean disconnect/drop sequence, so the full probes remain failed.
Attempts 41, 44, 46, 47 and 49 instead fail maintained-peer startup with
`0xc0000005` before launching fnTech3. Shortening a test path permits one
successful start but another short-path start fails; the cause remains open.
No authored-distance runtime proof follows from the failed map starts.
Installations and tested binaries remain unchanged, writable homes are
removed, and launched clients stay windowed with physical input disabled.

The actual production preset builds 64 targets. Eight focused groups and
four additional source-fixture tests pass. The repaired BSP controller
fixture supplies the real distance policy declarations and checks that
other-profile and failed world loads clear stale Academy values. The ninth
complete run's failure remains preserved; a fresh combined-source suite is
required for these later repairs. The machine companion records exact
commands, executable/module hashes, primary observations and each failed or
partial runtime outcome. No external implementation text is incorporated.


## Cgame command values and polygon submission

Retail import 235 carries eight native words: weapon, sensitivity, pitch/yaw
and sensitivity overrides, force selection, inventory selection and fighter
control. Its dispatcher at `0x412fe4`, helper at `0x411930` and module wrapper
establish the argument shape. The retail command builder publishes the low
bytes of the two selection integers at usercmd offsets 21 and 22. Fn now
owns those selections separately from external and client PODs and publishes
them through the real command finisher only for Academy MP. The existing Fn
weapon/sensitivity service remains in use. Generic commands, movement,
buttons and the other fifteen profiles retain their data. Non-finite values
and negative sensitivity refuse before state mutation; nonneutral angle or
sensitivity overrides and fighter controls remain named refusals. No physical
input is read or injected by these fixtures or probes.

Single-polygon import 203 receives shader, vertex count and caller pointer;
the retail dispatcher supplies the renderer's separate polygon-count word as
one. The actual renderer at `0x4ab090` copies and walks vertices at a 24-byte
stride. The separately pinned OpenJK format corroborates XYZ at offset zero,
texture coordinates at 12 and RGBA bytes at 20. An independent frozen POD
with size/offset assertions is converted from potentially unaligned storage,
with all five floats checked even under fast-math. Conversion completes
before publication, so a malformed final vertex cannot submit a partial
polygon. The existing 32,768-vertex Fn allocation baseline bounds the input.
The actual owned queues preserve Fn's shader, fog and scene paths in GLX,
Vulkan and RTX modules; multi-polygon import 204 remains separate. Queue-body
fixtures exercise all three backends. This is submission evidence; a complete
rendered cgame frame and active RTX rendering remain unverified.

Candidate 51 passes import 235 and stops at polygon import 203 with 60
snapshots. Candidate 52 passes both and stops at voice-volume query 35 with
72. Production attempt 53 reaches the same refusal with 55
snapshots. All three receive deltas, reach maintained server active state and
ClientBegin, and have accepted pure proofs without an unpure drop. Their
whole probes still fail before a completed cgame frame, engine capture and
clean disconnect. Writable homes are removed and installations/binaries
retain their hashes. The production preset builds 13 targets;
eight focused groups and three additional Python fixtures pass. The exhaustive
RTCW fixture now includes Academy's unsupported menu ID in both modes and
passes its repaired check. Checkpoint ten's failure remains preserved. A
fresh combined-source regression suite remains required after these changes.


## Cgame voice absence, clocks and bolt queries

Import 35 reads an entity-indexed retail integer table. Its initial 1,024
entries are zero; active speech tracking is not measured by that observation.
The new native facade validates the Academy MP profile and entity bounds,
then returns zero only when the actual Fn audio interface is absent. Enabled
audio refuses by name until voice tracking is adapted into Fn's backend.
This preserves an honest disabled-audio probe without claiming spatial audio,
occlusion, EFX, zones, HRTF or mouth animation. Candidate 54 receives 90
snapshots including deltas and reaches bolt query 294.

Import 294 carries nine words and publishes a 48-byte three-by-four float
matrix. The complete primary core at `0x4dfa00..0x4dfc3a` reads its selected
clock, never consumes caller time word 7 or model-list word 8, and uses a
world placement matrix built from caller angles and position. On a missing
model or bolt, `0x4dfc1d..0x4dfc3a` publishes that world matrix multiplied by
the fixed MP axes at `0x54e69c` and returns false. Those axes match the
existing Fn root basis. The adapter now serves owned remote-client poses
through Fn's existing CPU hierarchy and animation kernel, checks all consumed
vectors by their float bits, and stages publication for unaligned callers.
Missing model/bolt output is real measured failure output. Stale, foreign and
null opaque identities, unsupported hierarchy/animation and invalid records
refuse explicitly. No-reconstruct variants 295 and 296 remain unserved.

The exact retail frame function at `0x4148b0..0x41490d` updates clock zero
when no local server is running and always updates clock one before cgame's
draw-frame export. The primary setter wraps addition at 32 bits and clears
clock one when it exceeds clock zero plus 200 under signed comparison. The
getter prefers nonzero clock one. Imports 325 and 326 now expose a per-cgame
Fn owner; the actual `CL_CGameRendering` hook changes only Academy MP and
clear resets it. A valid pose requires a ready remote-client frame. Local
server clock sharing remains a named refusal because server/UI owners have
not yet been reconciled with this shared retail contract.

Candidate 55 passes voice and bolt queries and reaches
`CG_G2_GETBONEFRAME` (305) with 62 snapshots including deltas. Production
attempt 56 reaches the same refusal with 105 valid snapshots; that attempt
contains no delta snapshot. Both reach maintained active state and
ClientBegin with accepted pure proof, and neither completes a rendered
cgame frame, engine capture or clean drop. These are failed whole probes,
separate from original retail interoperability. Both use windowed private
desktops, disabled physical input and isolated writable homes; homes are
removed and installation/binary hashes are unchanged.

The preset rebuild completes 80 actions. Eight focused groups and twelve
additional Python fixtures pass, including plain and optimized/fast-math
boundary checks, authored poses, all sixteen frame selectors, all three
polygon queues and repaired RTCW/menu/lifecycle fixtures. Checkpoint eleven's
four missing-header failures remain preserved. The twelfth frozen inventory and source/runtime audit pass for this promoted
source, before any later bone-frame import implementation. Complete cgame geometry, Ghoul2 world
drawing, active voice audio and the profile's gameplay gates remain open.
The existing server bolt adapter also needs a follow-up: it preserves failed
output and uses caller time, while this newly measured shared primary core
publishes fallback output and selects its clock slots. This slice records
that discrepancy without claiming to have repaired the server boundary.


## Cgame bone-frame metadata

Import 305 carries six native words: opaque model owner, bone name, caller
time, float output, model list and model index. The retail wrapper
`0x4df400..0x4df456` replaces caller time with the selected clock, and the
complete query/index bodies at `0x4e3750..0x4e37c3` and
`0x4e3640..0x4e3743` never consume the model-list pointer. A missing bone
returns false without writing the caller output. A known bone without active
animation publishes float zero and returns false. Active timing publishes
its integer frame plus fractional lerp. The adapter now implements those
distinct results using the owned Fn model state and existing timing kernel,
with native lifetime and unaligned-output checks and staged publication.

The initial narrow query refuses candidate 57 with 66 delta-bearing
snapshots. Diagnostic attempt 58 receives 70 and identifies a valid forward
clip: flags `0xc8`, frames 8 to 10 of a 21,376-frame skeleton and speed 0.05.
The complete timing core `0x497290..0x4976c1` reads loop/freeze control,
start/pause time, range and speed, but no blend duration or prior clip.
Current-clip metadata therefore permits the blend flag under mask `0xd8`.
Actual blended pose evaluation remains a separate, unserved renderer
boundary. Forward in-file ranges, looping and frozen completion are served;
reverse ranges, extra controls, overflowing/non-finite progression, clock
subtraction wrap and unfrozen terminal state changes refuse explicitly.
Embedded skeletons and local-server clock sharing remain unserved.

Attempt 59's maintained server crashes during startup before launching our
client; it provides no query evidence. Candidate 60 passes the query and
reaches the client Ghoul2 scene-submission refusal, import 202, with 75
snapshots including deltas. Production attempt 61 reaches the same
refusal with 77 valid snapshots including deltas. Both reach maintained
active state and ClientBegin with accepted pure proof. Both whole probes
fail before a complete cgame frame, engine capture and clean drop. They use
windowed private desktops and disabled physical input; installation/binary
hashes are unchanged and their isolated homes are removed.

The promoted source passes five focused groups and two additional Python
fixtures, alongside the four staged fixtures and shared pose checks in
plain and optimized/fast-math configurations. The preset rebuild completes
3 actions. The thirteenth frozen source and runtime audit pass for this promoted
bone-frame metadata implementation, before the later staged renderer work. Complete Ghoul2 world drawing, pose blending
and rendering/gameplay remain open.

## Owned cgame geometry and static tags

Native scene import 202 now stages a copied 212-byte record for one owned,
remote-client GLM model with a paired GLA. At scene flush, the existing Fn
CPU pose evaluator supplies matrices to the Fn skeletal renderer. Actual
registered skins and surface overrides select visible geometry; flag-zero
surfaces draw, bolt/off surfaces do not, and the no-descendants flag stops
traversal. These format and visibility meanings are separately observed in
the [pinned OpenJK source](https://github.com/JACoders/OpenJK/tree/1a6a643427aa347553e9073dac5570b33337c4d9).
The installed Kyle asset supplies primary metadata, not a rendering pass.
The bridge retains Fn materials, lighting, shadows and the canonical
FOV/HUD/capture/liquid/highlight hooks. It uses LOD0 and default model scale;
attachments, embedded skeletons and local-server clocks refuse explicitly.

Retail's complete indexed animation setter, `0x4e32f0..0x4e359f`, queries
the prior active clip. Its inactive path clears the incoming blend flag.
The cgame boundary now does this for known inactive or missing prior clips,
so an initial requested blend does not create a nonexistent transition.
Active-prior requests remain intact and blended pose evaluation remains
unserved. Complete prior-clip terminal mutation is not claimed.

UI and cgame runtime models share one process-lifetime owner allocator.
Cleanup and renderer restarts cannot recycle an identity into another
model's pose-update authority; exhaustion refuses. Native-error cleanup
releases pending records, geometry and working allocations.

Tag import 213 forwards six words and publishes an independently frozen
48-byte orientation. Retail's complete query `0x4a9860..0x4a9a3f` writes
zero origin and identity axes on missing model/tag failure, and interpolates
then normalizes axes without clamping the fraction. The adapter calls the
existing Fn tag query, validates finite words/results, and publishes with
an unaligned-safe copy. Negative frames and malformed spans/names refuse.
Authored fixtures execute the actual MD3 query bodies from all three Fn
backends, including failure, extrapolation, frame clamps and fast-math
finite checks. Broader Fn model support is not retail MDR-tag evidence.

Candidate attempt 66 passes scene queueing and the tag query, then stops at
`CG_FX_PLAY_EFFECT_ID` (266), with 77 valid snapshots including deltas.
Production attempt 67 reaches the same frontier with 74. Both reach active
state and ClientBegin with accepted maintained pure proof. Neither reaches
a complete scene flush, engine capture or clean drop. Windowed private
desktops disable physical input; installation and runtime hashes remain
unchanged and writable homes are removed. These are maintained-peer
observations, not original retail interoperability or a visual pass.

Seven staged fixtures, 14 focused production groups and three additional
shared-record/RTCW fixtures pass. The preset rebuild completes 12 actions.
The first focused run exposed a missing scene dependency in the extracted
static-renderer fixture; its failure is retained, and the repaired fixtures
preserve the static and cross-profile assertions. The fourteenth combined suite and source/runtime audit pass for this
promoted geometry/tag source, before any later staged effects work. Effects playback, complete world rendering,
gameplay, spatial audio, SP campaign/save and original retail interop remain
open; no retail-exact presentation default is introduced.

## Gameplay corrections

Bot actions now carry Academy's alt-fire (`0x200000`) and force-power
(`0x100000`) flags through the existing bot input pipeline. The jump-history
marker is translated separately. Dispatcher tests exercise 4,096 combinations,
client bounds, pointer bounds and reset behavior against the real action store.

ROFF v2's header interval is milliseconds: 50 means 20 Hz, not 50 Hz. Retail
`jamp.exe` divides 1,000 by that value at `0x426341` and schedules the next
sample from the current server time at `0x426cda`. The server applies measured
trajectory velocities, rotation offsets, note callbacks and the delayed final
freeze, with cache generations protecting reentrant callbacks.

The Ghoul2 animation seed is a float, including its `-1.0f` sentinel. Retail
`0x4e3523` through `0x4e353e` shifts the start clock by
`(setFrame - startFrame) * 50 / animSpeed`, then truncates it. Fractional seeds,
invalid encodings, zero speed and elapsed-time overflow have unit coverage.

Multiplayer Ghoul2 cleanup now receives the measured pointer-to-handle slot,
releases only a model owned by that store and clears the successful slot.
Null, stale and foreign handles preserve their slot and do not affect sibling
models or cached asset bytes. Stores use globally unique atomic opaque identities
instead of allocator addresses, preventing stale handles from aliasing later
models after release or reset. Production-dispatcher tests exercise unaligned
slots, attachments, native/QVM pointer translation, concurrent allocation,
exhaustion and shared Jedi Outcast store behavior. This leaves the distinct SP
retail C++ allocator boundary unchanged. The third maintained reverse run
passes clean client exit, retail `ClientDisconnect: 0` and the engine's
subsequent zombie-state transition, confirming the callback returned. It
receives 324 distinct snapshots including deltas, with unchanged binaries,
modules and installation, and leaves no writable test home behind.

Academy's pure checksum proof omits Quake III's serverId prefix. Retail's
verifier reads the cgame checksum at argv 1 (`0x45915e`), UI at argv 2
(`0x459195`), `@` at argv 3 (`0x4591c5`) and referenced paks from argv 4.
Both sender and verifier now use that JKA-only framing while retaining module
pak checks, bounds, membership, duplicate rejection and the checksum XOR.
Production-code tests cover the other profiles' proof formats. The fifth
maintained reverse run passes pure admission, retail ClientBegin, active state,
424 distinct snapshots including deltas and clean disconnect. Installed
assets and all tested binaries remain unchanged. This is maintained-peer
evidence; original retail admission and downloads remain open.

A new synthetic game fixture queries the legitimate retail dedicated engine
and fnTech3 against the same installed Kyle mesh and humanoid skeleton. Both
engines initialize and clean the model and return all 18 requested bolt
matrices successfully, but none matches exactly. The comparison spans six
bones at initial state, root animation and a later thoracic angle override,
with phases separated by server frames. Retail's override changes the
thoracic bone and its descendants; fnTech3 returns their prior matrices.
Its initial root also carries the skeleton's 0.64 bind scale and zero height,
where retail returns unit axes and height -24. This is direct counterevidence
to the old inference that independently posed bones and rest fallback establish
correct server posing. Hierarchy, inherited animation, angle application and
coordinate/scale handling now form an explicit open gameplay and rendering
blocker. The fixture implements no retail gameplay, performs no input or
render capture, and both engines exit cleanly with unchanged installations.
Full commands, binary and fixture identities, raw matrix words and comparison
results are in the companion and `.tmp/jka-compat/pose-comparison.json`.
An extended retail-only probe records 90 successful queries, adding the three
bolt-transform operations, nonuniform scale and entity placement, three
angle flags and three axis permutations. Its measured words extend the
independent kernel's validation corpus. The rebuilt candidate repeats those
90 queries successfully, with a maximum absolute matrix difference of
`0.0000114441` (tolerance `0.00005`); both engines exit normally and retain
unchanged installations and binaries. The corrected kernel composes parent
deformations, inherits animation timing, applies measured angle modes and
separates model scaling from entity placement. This validates the measured
server bolt subset; blending, unmeasured modes, SP posing and drawn geometry
remain open. Raw words and build identities are retained in the companion.

## Owned geometry and renderer bridge

The owned GLM geometry layer validates all 127 installed models and their 317
LODs, preserving positions, normals, UVs, weights, triangle winding and surface
hierarchy. Its installed-memory probe deforms 252 LODs against their paired
GLA files. The 41 inline `*default` models are parsed without inventing poses.
The probe exposed and corrected double accumulation of vertex weights and
signed aggregate overflow. These are asset and kernel checks, not drawn-model
evidence.

The normal-style RBSP renderer path retains the original map and all 18 lumps
alongside its validated projection into Fn geometry, shaders, static lighting
and light grids. Its CPU census accepts 56 of 61 installed Academy maps;
`kor1`, `kor2`, `t1_fatal`, `t3_bounty` and `t3_stamp` require additional light
styles and remain named refusals. Inactive NaN channels remain untouched in
the retained records; consumed numeric values are validated. A black ambient
value is valid, and style 255 identifies an empty light-grid sample.

A narrow UI skeletal bridge is implemented in source. It stages owned records
until scene submission, evaluates the shared deformation palette using the
retail-selected UI clock, and constructs an
engine-owned MDR for the existing Fn skeletal rendering path. It initially
requires one unbolted paired-GLA model, default surface visibility and default
scale. Default bolt and off surfaces are excluded. Custom skins, attachments,
inline poses and extra Raven effects remain named boundaries. Animation and
bone-angle metadata use measured UI imports 213 and 212. Retail UI refresh
sets clocks 0 and 1 through import 228 before its other work; import 227 picks
nonzero clock 1, otherwise clock 0. The measured signed cutoff resets clock 1
when it exceeds clock 0 plus 200, including the x86 wrapping addition. A production-host
test checks clock selection at scene submission, ownership, canonical submission, stale
identities and failure cleanup. All three renderer modules build with API 33.
An independently authored native UI fixture loads the installed Kyle GLM and
humanoid GLA, animates the installed `BOTH_RUN1` range 12305..12330 and submits
two engine screenshots from the same scene. OpenGL, Vulkan and the RTX module's
raster path show distinct running poses and exit normally. No retail module
or asset is included in the fixture. This establishes animated geometry through
the default Fn skeletal path; the incomplete skin/material binding is visible
in the captures and remains an open boundary.

The first Vulkan and RTX captures were black. A bounded diagnostic confirms
the model reached draw calls with the expected camera matrix. Pose updates had
drained queued commands using a routine that also submitted the current Vulkan
frame, leaving later scene draws without an open command buffer. A separate
runtime-model drain now consumes the previous CPU pose while preserving the
frame and RTX graph. Existing full drains still submit and wait as before.
Production dispatcher tests cover multiple poses in one frame, empty drains,
normal submission, unregistered/minimized/throttled paths and backend skipping.
Fresh default captures pass nonblack geometry and substantial pose-change
checks; the formerly failing Vulkan wireframe probe also exits normally.

This path uses the existing Fn model, shader, entity-lighting, dynamic-light,
lighting/shadow and output systems without introducing a retail presentation
default. The isolated scene has no world or static lights, so world lighting,
CSM and audio are not exercised. RTX ray tracing is disabled in these probes;
the module's raster path is the observed path. Composite skins, attachments,
blend modes and full retail cgame rendering still require their own evidence.

## Regression evidence

A prior complete Meson run against a frozen source inventory finishes with 216
passes, two failures and two skips. Its Python suite runs 2,482 tests, with
two failures and 126 skips. Both failures are renderer-host test fixtures
missing the Academy profile predicate introduced by the Fn FOV change;
the shared fixture dependency is now repaired and both two-test modules pass.
The frozen inventory and logs identify the tested source separately from
later concurrent slices. A full run of the final combined source remains
required; repaired targeted checks do not turn the earlier run into a pass.
The second frozen inventory contains 4,078 files and remains byte-identical
through the complete run, including the original source. Its 237 Meson groups
finish with 233 passes, two failures and two skips. The Python aggregate runs
2,509 tests with 14 failed checks and 126 skips. Failures identify the missing
UI light fixture member, IHV runtime-model fixture dependencies, missing SP
profile predicates in RTCW fixtures, an unguarded SP cgame check in dedicated
builds, and an RTX assertion expecting shading normals/tangents to be ignored.
The RTX hash correctly includes those lanes so pose-only lighting changes
restart accumulation; its oracle now checks that behavior.

The dedicated guard and fixtures are corrected. The focused 25-test run passes
the client/SP, RTCW and RTX checks while exposing the IHV fixture's further
missing bounded string comparator. The subsequent IHV suite passes all 34
checks, the extended cgame model/bolt check passes, and all four native SP
bridge/metadata groups pass. The current incremental build passes. These
targeted repairs do not turn either earlier frozen run into a pass; a fresh
complete run is still required.

The third frozen inventory contains 4,080 files and remains byte-identical in
both its independent copy and the original source through the complete run.
Its 238 Meson groups finish with 235 passes, one failure and two skips; the
Python aggregate runs 2,510 tests with one failure and 126 skips. The remaining
collision fixture omits the new SP profile predicate required by the real
map-contents function. That fixture now passes all three checks, and the six
ambient/client/codec production groups pass. The ambient slice's incremental
build passes. A fresh complete suite is required for this combined source;
the previous failed results remain recorded.

The fourth frozen inventory contains 4,085 files. Its complete run passes
239 Meson groups with two expected skips and no failures; the Python aggregate
runs 2,512 tests with 126 skips and no failures. Both the frozen
copy and original source remain byte-identical through the audit. This closes
the ambient/MP3/collision regression checkpoint.

The fifth frozen inventory contains 4,089 files and adds effect registration,
context lifecycle and selector diagnostics. Its complete run passes 241 Meson
groups with two expected skips and no failures; the Python aggregate runs
2,513 tests with 126 skips and no failures. The independent frozen copy and
original source remain byte-identical through the audit. This closes the
effects regression checkpoint. Skin and animation changes made after that
audit are covered by the sixth inventory below.

The sixth frozen inventory contains 4,092 files and adds cgame skins, animation,
bone angles, optional registration-warning control and native attachment-cache
cleanup. Its complete run passes 242 Meson groups with two expected skips and
no failures; the Python aggregate runs 2,514 tests with 126 skips and no failures.
The frozen copy, original source and all 160 reserved binaries/package inputs
match their pre-suite hashes through the audit. This closes that regression
checkpoint before the later music, disabled-automap and effects-clock changes.

The seventh frozen inventory contains 4,095 files and adds music preloading,
default-disabled automap initialization and the effects clock. Its complete run
passes 244 Meson groups with two expected skips and no failures; the Python
aggregate runs 2,515 tests with 126 skips and no failures. The frozen copy,
original source and all 161 reserved binaries/package inputs match their
pre-suite hashes through the audit. This closes that checkpoint before the
light-style and empty client ROFF changes.


The eighth frozen inventory contains 4,097 files and adds neutral light-style
metadata and the empty client ROFF update. Its complete run passes 245 Meson
groups with two expected skips and no failures; the Python aggregate runs
2,515 tests with 126 skips and no failures. The frozen copy, original source
and all 162 reserved binaries/package inputs match their pre-suite hashes
through the audit. The later distance-cull and token-iterator changes require
their own combined-source checkpoint.


The ninth frozen inventory contains 4,099 files and includes renderer-owned
distance-cull data and MP entity-token iteration. All 248 Meson groups finish:
245 pass, one fails and two skip. The Python aggregate runs 2,517 tests with
one failure and 126 skips. The failure is the extracted BSP loader fixture's
missing distance globals and Printf service declaration. Both source copies
and all 162 reserved runtime/package inputs remain unchanged through audit.
The fixture repair and later native-pak/model/menu slice pass their focused
checks; this does not convert the failed run into a pass. A new combined
checkpoint remains required.


The tenth frozen inventory contains 4,099 files and includes the BSP fixture
repair, actual native-pak verification/reference restoration, owned Ghoul2
copy/duplication, initialized menu relay and read-only routing getter. All
248 Meson groups finish with 245 passes, one failure and two expected skips.
The Python aggregate runs 2,517 tests with one failure and 126 skips. The
failure is the exhaustive RTCW export fixture's missing appended Academy
menu expectation: its two 27-entry arrays no longer match the canonical
count of 28. The independent frozen source, original source and all 162
reserved runtime/package inputs retain their pre-suite hashes through audit.
The subsequent fixture repair preserves all assertions and expects the new
menu to be rejected in both RTCW modes. A new combined suite is required;
this failed checkpoint remains preserved.


The eleventh frozen inventory contains 4,101 files. Its complete run finishes
all 248 Meson groups with 242 passes, four failures and two expected skips;
the Python aggregate runs 2,518 tests with four failures and 126 skips.
Three Academy lifecycle fixtures extract `CL_JKACgameClear`, and the RTCW
client-state fixture extracts `CL_FinishMove`; each omits the new actual
Academy command-state header. All frozen and original source files and all
162 reserved runtime/package inputs retain their recorded hashes through
audit. The repairs include the real header and preserve the existing checks,
adding clear-state and cross-profile assertions. This failed checkpoint
remains preserved until a subsequent combined suite passes.


The twelfth frozen inventory contains 4,104 files and includes the owned
remote-client bolt query and frame clocks, guarded voice absence and all
four extraction-fixture repairs. All 248 Meson groups complete with 246
passes and two expected skips, without failures. The Python aggregate runs
2,520 tests with 126 skips and no failures. Frozen and original source and
all 162 reserved runtime/package inputs retain their pre-suite hashes through
audit. Earlier failed checkpoints remain preserved. This is combined suite
evidence for the stated source; it does not establish clean-build, complete
rendering/gameplay, SP campaign/save or original retail interoperability.


The thirteenth frozen inventory contains 4,104 files and includes the
promoted current-clip bone-frame query. All 248 Meson groups complete with
246 passes and two expected skips. The Python aggregate runs 2,520 tests
with 126 skips and no failures. The source/runtime audit confirms unchanged
original and frozen source and all 162 reserved runtime/package inputs.
This combined checkpoint excludes later staged geometry, inactive-prior
blend normalization and shared renderer-owner allocation. Complete
rendering/gameplay, SP campaign/save and original retail interop remain open.


The fourteenth frozen inventory contains 4,109 files and
includes the promoted owned scene/tag source and extraction-fixture repairs.
All 250 Meson groups complete with 248 passes and two expected skips. The
Python aggregate runs 2,522 tests with 126 skips and no failures. Frozen and
original source and all 162 reserved runtime/package inputs
retain their pre-suite hashes. This checkpoint excludes later staged
effects playback. Complete frames/gameplay, active blending, SP campaign/save
and original retail interoperability remain open.

## Shader-only effects playback

Native imports 266 and 270 now own a bounded shader-only playback subset.
Particle, oriented-particle and cylinder declarations with one shader, one
immediate spawn, fixed integer life and no motion or active curve controls
are compiled into engine-owned instances. The default life is 50 ms, measured
in retail's complete constructor. Both native vectors are copied and checked;
each complete effect is staged before any instance enters the 1,800-slot pool.
Unflagged curves retain their start values, alpha dims RGB, and the default
shader clock remains zero. Paused frames create no new instances; the normal
lane expires after, rather than at, the end time. These facts are separately
pinned to complete primary curve, lifetime and creation bodies. Unsupported
families, fields, controls and pool exhaustion refuse by name.

The adapter submits canonical Fn sprites and polygons. It retains Fn
materials, fog and scene handling, and owns its random sampling, color
quantization and 16-segment cylinder quality. Light and sound primitives,
physics, attachments and active curves remain unserved; this subset provides
no spatial-audio evidence and introduces no retail renderer default. The
[pinned OpenJK source](https://github.com/JACoders/OpenJK/tree/1a6a643427aa347553e9073dac5570b33337c4d9)
supplies field/range and data-control observations; no implementation text is
incorporated. Four composed fixtures pass in ordinary and fast-math modes,
including actual native imports, lifetime boundaries, renderer packets,
malformed input, pool exhaustion and error cleanup. The production preset
build and all 15 focused groups pass. The fifteenth combined suite and
source/runtime audit pass for this promoted effects source, before later
staged default-skeleton support.

The corrected candidate passes effect playback, receives 57 valid snapshots
including deltas, and stops at scene import 202. Its detailed refusal names
`models/weapons2/blaster_r/blaster_w.glm`, skeleton selector `*default`, and
no retained mesh or skeleton bytes, with boltInfo zero. The production repeat
reaches the same state with 69 valid snapshots and no observed delta in that
run. Both peers reach active/pure admission and ClientBegin. Neither probe
reaches scheduled effect drawing, a complete scene flush, capture or clean
drop. One intervening maintained server fails before client startup with
`0xc0000005`; it contributes no client compatibility evidence.

Primary internal data at `jamp.exe` address `0x547b98` identifies `*default`
as the engine-provided one-bone, one-frame skeleton with identity rest,
inverse and local frame transforms. A GLM requesting it does not embed its
own bones. At that checkpoint, serving the selector and retaining its GLM bytes
remain the next boundary; no builtin blob or proprietary asset has been copied into source.
The full compatibility gate remains open.


The fifteenth frozen inventory contains 4,111 files and
includes the promoted shader-effect lifetime source and detailed model refusal.
All 251 Meson groups complete with 249 passes and two expected skips. The
Python aggregate runs 2,523 tests with 126 skips and no failures. Frozen and
original source and all 162 reserved runtime/package inputs
retain their pre-suite hashes. This checkpoint excludes later staged
default-skeleton support. Complete frames/gameplay, active blending, SP
campaign/save and original retail interoperability remain open.

## Engine-default skeleton and retained static models

The Academy MP cgame store now owns the measured `*default` skeleton selector.
An independently generated one-bone, one-frame GLA carries the measured bone
name and identity rest, inverse and local pose transforms. It is 298 bytes,
with engine-owned padding rather than a copy of retail's 294-byte builtin.
The normal bounded reader and Fn CPU pose kernel consume it; no star-named
file is opened. The store retains the requesting GLM and generated skeleton
and uses the existing Fn geometry/runtime-MDR path. UI, server, SP and Outcast
stores keep their prior default-off behavior. Every shared Store consumer in
the comparison client is rebuilt together.

All 41 installed highest-pak GLMs requesting `*default`, including all 65
LODs, pass actual Store, pose and weighted-geometry checks in ordinary and
fast-math modes. Independent fixed-basis position/normal checks preserve UVs
and indices. No proprietary asset is extracted or incorporated. Seven
composed fixtures and six standalone shared-consumer runs pass, covering
native queries, bone/bolt transforms, Fn registration/updates, contradictory
bone counts and error cleanup. The production build and 23 focused groups
pass. The sixteenth combined inventory subsequently passes its complete suite and source/runtime audit.

Candidate 74 passes the previous `blaster_w.glm` scene-submission refusal,
receives 85 valid snapshots including deltas and reaches ambient update 48.
Production attempt 80 reaches the same boundary with
83 snapshots including deltas.
Both reach active/pure admission and ClientBegin. A diagnostic-only repeat
reports name `default`, two loaded ambient definitions and no matching record;
the callback still refuses. Two preceding maintained servers crash during
startup with `0xc0000005` and contribute no client evidence.

The supported data uses Fn pose, material, lighting, fog and shadow systems.
Active blending, attachments and nontrivial scale remain explicit boundaries.
No complete scene flush, rendered cgame frame, capture, clean forward drop,
active audio or physical input is established. Original retail interoperability
and the full compatibility gate remain open.

## Absent ambient-set update

Native import 48 now returns zero for a missing, null or empty ambient-set
name. Retail's complete lookup/update bodies establish that this path does
not read the listener origin or emit audio. The owned loaded catalog resolves
names; a nonempty request without a catalog, or any present definition needing
active playback, remains an explicit refusal. The adapter does not synthesize
ambient loops, crossfades or subwaves. Existing definitions and media handles
still use Fn audio registration; this absence path provides no spatial-audio
or audible-output evidence.

Four composed fixtures and all six focused production groups pass. The native
fixture tests invalid listener spans left untouched, null/missing/empty and
case-sensitive names, present-record/no-catalog refusals, retained registration
behavior and error cleanup in ordinary and fast-math modes. The actual cgame
object, client and package rebuild pass. An initial package-only build retained
a header timestamp older than its object; attempt 81 consequently repeated
the earlier import-48 refusal. Its 62 snapshots provide no ambient-change
evidence. The timestamps were refreshed after source hash verification and the
client rebuilt before the valid production repeat.

Candidate 77 receives 77 snapshots including deltas, and production attempt 84
receives 59 snapshots including deltas.
Both pass the missing `default` ambient request, reach active/pure admission
and ClientBegin, then stop at scene import 209. Attempts 82 and 83 fail in the
maintained server before client startup and contribute no client evidence.
A separately recorded
diagnostic measures view flags `0x10` (`RDF_DRAWSKYBOX`), an 800 by 600 viewport
and six queued owned entities. No complete scene flush, world frame, capture
or clean forward drop is established by those attempts. The sixteenth combined inventory subsequently passes its complete suite and source/runtime audit. Original retail and full
compatibility gates remain open.


The sixteenth frozen inventory contains 4,113 files and
includes the promoted default skeleton, retained static models and absent
ambient-name return. All 252 Meson groups finish with 250 passes and two
expected skips. The Python aggregate runs 2,524 tests with 126 skips and no
failures. Frozen and original source and all 162 reserved
runtime/package inputs retain their pre-suite hashes. This checkpoint excludes
later normal-sky and active animation-blend stages; complete frames/gameplay,
SP campaign/save and original retail interoperability remain open.


## Normal sky, forward-animation transitions and no-rotation bolts

The MP adapter accepts the measured ordinary-sky hint, `RDF_DRAWSKYBOX` (`0x10`),
through Fn's regular sky, cloud, fog, PVS, FOV and lighting paths. The Raven bit
is consumed at the boundary; unsupported portal/automap/no-fog/force-sight
requests still refuse. All earlier view translation checks remain intact.

Owned remote cgame models now blend a frozen prior forward playhead into the
advancing clip through the existing Fn CPU pose, weighted geometry and runtime
MDR paths, retaining Fn materials, entity lighting, fog and shadows. Retail
setter and evaluator bodies establish reversed prior-sample weights, a separate
blend clock, inherited child timing, same-clock replacement and interrupted
replacement from the prior active clip. An independently authored module makes
440 legitimate Kyle-model queries. A first analysis using supplied query times
fails and is retained; read-only observation of the actual shared pose clock
settles the discrepancy. Actual Fn C++ results then match all 440 queries in
ordinary and optimized fast-math builds (880 comparisons), within `0.00005`.
No asset payload is written or external implementation incorporated. UI, SP,
JK2 and server stores retain their default-off blend capability. Reverse clips,
angle blends, unfrozen completion and local-server clock sharing remain open.

Native import 296 sets only the no-rotation selector despite its combined
`NOREC_NOROT` name. Complete retail dispatch and shared-core bodies establish
that success clears the selector while a failed query publishes the fixed
world basis and preserves it for the next successful bolt call. The owned
cgame adapter follows that sequence and preserves the existing pose, span,
clock and numeric guards. Native295 reconstruction remains unserved.

Eleven composed Python fixtures and six ordinary/fast-math shared-consumer
checks pass. All 23 focused production groups pass. The existing Windows x86
client, dedicated server and package build completes 21 actions, including
all four shared cgame/UI/server consumer objects; this is an incremental
preset build, not clean-build evidence. The seventeenth combined source/runtime
audit subsequently passes all 252 groups (250 passes and two expected skips),
including 2,525 Python regressions with 126 skips and no failures.

Candidate90 receives 350 snapshots including deltas; production92 receives
297, both with maintained pure admission and ClientBegin. Both write an
800 by 600 engine screenshot showing the player-configuration menu over the
world. The new refusal is `CG_ROFF_CLEAN` (283) during client shutdown after
capture, and the forward clean drop remains incomplete. This is rendered
menu-frame evidence, not complete gameplay or original retail interoperability.
Maintained peer91 crashes before client launch and contributes no client
evidence. The unchanged ET infinite-gradient rejection also fails under Clang
x86 fast-math in both baseline and candidate; its ordinary fixture passes.



The seventeenth frozen inventory contains 4,115 files.
All frozen and original source and all 162 reserved runtime/package
inputs retain their pre-suite hashes. Its complete 252-group suite includes
normal sky, active forward-animation transitions and native296 selector
behavior, with 2,525 Python tests, 126 skips and no failures. Later empty
ROFF/weather cleanup stages are excluded. Gameplay, all-backend world drawing,
SP campaign/save and original retail interoperability remain open.


## Empty cleanup and completed maintained forward admission

The owned empty client ROFF cleaner returns 1. Client cache/play/purge remain
refused and the independent server store is untouched. The installed retail
cgame's literal `die` request reaches world-effect import 349. Complete primary
dispatch, command and cleaner bodies establish its return 0. Fn accepts null,
empty and exact case-insensitive `die` while the owned weather lane remains
empty; every creation/control command and unimplemented parser form refuses.
Existing Fn world, pose, fog, lighting and audio resources stay owned by their
normal systems. Active MP weather and SP weather import 128 remain open.

The pinned maintained server clears userinfo/name after its disconnect callback
returns, then prints a blank-name `CS_ZOMBIE` transition. The probe now requires
this probe's named disconnect command, slot 0 callback and following empty-name
transition in order. Partial, reversed, wrong-client/slot and unrelated traces
refuse. Nineteen staged fixtures, ordinary/fast-math host checks and all six
focused production groups pass. The actual cgame object, client and package
rebuild pass. The frozen inventory 18 suite passes all 252 groups (250 passes,
two expected skips), including 2,526 Python regressions with 126 skips and no
failures. Its source and native test executables remain unchanged. Concurrent
RTX source/DLL edits prevent the planned complete current-production audit;
those edits are preserved. Later light/charset source is excluded from this suite.

Candidate 98 and production 99 pass maintained forward pure admission,
ClientBegin, normal client exit and completed server disconnect. They receive
223 and 341 snapshots respectively, including deltas, and write valid 800 by
600 engine menu captures. Attempt 97 already exits normally with 349 snapshots
and no unsupported import, but its old name-dependent probe falsely reports
completion missing; that failed report is retained. The original retail peer
still requires legitimate Disc 1, and this maintained subset does not close
original-engine interoperability or complete gameplay.

Before the light/charset slice below, the fixed Fn candidate replays the
previously generated maintained-client
`.dm_26` control through 28 snapshots and deltas, writes an engine screenshot,
reaches its explicit EOF marker and exits normally. The capture shows a
spectator world view. The diagnostic `gfx/2d/bigchars` image is missing and its
upper-left placeholder remains a rendering gap. The longer native-server demo
reaches 408 snapshots and deltas, then refuses dynamic-light import 207 before
capture/EOF. This is one bounded Fn reader control pass; native-server playback,
original retail reading, other renderers and the complete demo gate stay open.

## Ordinary/additive scene lights and the Academy charset

Retail MP imports 207 and 208 take an origin and four float words and return
zero. Complete primary dispatch bodies (`jamp.exe` 0x412d42 and 0x412d6d), both
renderer wrappers (0x4ab4a0 and 0x4ab4d0), their shared core (0x4ab420) and the
renderer export setter establish the normal/additive selection. The x86 native
cgame boundary now copies unaligned vectors and preserves exact finite radius
and RGB words into `re.AddLightToScene` or `re.AddAdditiveLightToScene`. It rejects
malformed spans, NaN/infinity, foreign owners and unavailable selected services
before publishing a light. The Fn renderer retains its dynamic-light colors,
budgets and shadow controls. The retail core's fixed light-list size is evidence
for retail presentation, and is not introduced as a Fn default. Effect-definition
light primitives and active weather remain separate, unserved boundaries.

The complete retail MP art initializer at 0x41f350 registers
`gfx/2d/charsgrid_med` without mipmaps. The installed 256 by 256 RGBA atlas has
16 by 16 cells; pinned OpenJK draw helpers sample the left 8 by 16 glyph region.
Academy's SP/MP profile selection now supplies that atlas and half-cell UV width
to Fn diagnostic, console and HUD text. Fn placement, colors, shadows and the
other profiles' charset choices remain intact. SP selection has unit evidence;
the runtime captures here are MP.

All 13 staged and 13 promoted production fixtures pass, including ordinary and
optimized fast-math native host checks, malformed light inputs, all-profile text
selection/placement and cgame/render lifecycle checks. All seven focused
production groups pass. All 49 affected production client objects compile.
The linking attempt stops with LNK1104 because a concurrent renderer sweep uses the shared client
executable. Its running probes are preserved. The actual candidate consumers
compile and link successfully; production relink/runtime checks remain pending
until the shared executable is available. This is not a clean-build signoff.

Candidate control replays each receive 28 snapshots including deltas, reach
natural EOF and exit normally in GLx, Vulkan and the RTX module's raster path.
Their 800 by 600 engine render-target captures show the spectator world and
readable diagnostic text. Current production playback has not been tested by
this slice: its relink/runtime checks remain pending.
Production 99's earlier maintained forward admission/clean-disconnect evidence
covers the preceding empty-cleanup source, and is not rerun evidence for this
light/charset slice. Installed content and tested binaries stay unchanged
during these windowed, input-disabled
probes; their writable homes are removed. No operating-system capture is used.

The longer native-server demo now passes import 207, then stops after 408
snapshots and deltas at `CG_R_ADDREFENTITYTOSCENE` (202), reporting **type 5,
flags 0**. The pinned enum identifies this as `RT_SABER_GLOW`. That failed run
has no completed frame/capture or EOF. It establishes boundary advancement,
not visible dynamic-light output or complete demo playback. Authored `glow`
shader stages and the Raven entity remain open for the next slice.

Concurrent production RTX edits invalidate the complete current-production
source/runtime audit. Candidate RTX captures use the independently preserved
pre-drift DLL; they do not validate the newer shared RTX implementation. The
252-group frozen source18 pass excludes this later light/charset slice. Inventory
17 remains the last complete production unchanged-input audit. Full gameplay,
all maps/materials/effects, active tracing/audio/input, original retail peers and
SP campaign/save gates remain open; default Academy startup stays gated.

## Open gates

The first retail-client admission attempts reached a challenged protocol-26
server identity, but the retail UI loader failed before connection. Extracting
the installation's pak-borne DLLs into the isolated client home alone did not
resolve its native search path. Copying the DLLs beside the stable isolated
retail executable subsequently loads UI. The retail peer then reports
**Disk 1 not in drive** before sending a connect packet. Primary disassembly
places that media check inside `CL_Connect_f` before address parsing. Its menu
background movie is a consequence of returning to the UI, rather than proof
that startup commands were lost. These results are failed harness attempts,
not evidence that client admission works or that the codec disagrees.

The pinned OpenJK MP client, dedicated server and vanilla renderer have been
built independently for Windows x86 under the fixed comparison runtime path
`.tmp/runtime/jka-openjk-build/`. Its checkout remains clean. Maintained-peer
tests can establish additional implementation evidence while the original
retail peer requires legitimate Disc 1; they do not close the retail gate.

Still required: original retail peers in both directions, their pure admission and downloads,
complete demo capture/playback, remaining native client-game and UI records, complete RBSP
world rendering and extra light styles, complete Ghoul2 rendering through
fnTech3's renderer features, spatial audio and input,
SP campaign and save/load, and profile-appropriate clean builds and complete
suite evidence. The existing collision translation drops RBSP lighting data;
it supplies collision evidence only. No retail-exact presentation default is
introduced by this slice.
