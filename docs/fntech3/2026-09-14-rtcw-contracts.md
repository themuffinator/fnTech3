# Game manager and RTCW contracts

## Scope and current status

RTCW single player and multiplayer target the **same `fntech3.exe`** and shared
engine. They are two profiles of one title, not separate engine distributions.
The catalog exposes `rtcw-sp` and `rtcw-mp`; `rtcw` or an installation containing
both modes requires an explicit mode choice before compatibility-sensitive
startup. Process-wide profile selection remains immutable.

**RTCW gameplay is not implemented yet.** Its catalog entries have explicit
runtime blockers, and both detection and activation reject them before the
filesystem, protocol codec or module can accidentally use a Q3/QL contract.
The remaining runtime, renderer, savegame and interoperability work is recorded
in the [compatibility ledger](COMPATIBILITY.md).

The [game manager API](../../code/qcommon/game_manager.h) owns read-only title
and profile enumeration, canonical keys and aliases, configuration filenames,
archive policies, native-module search and activation eligibility. Its implementation remains in the
existing early [profile translation unit](../../code/qcommon/game_profile.cpp),
so every build path and standalone early-startup probe uses the same catalog.
The `gameList` console command reports the registered profiles and their actual
startup availability. Registration does not imply compatibility sign-off.

## Switching SP and MP through the same executable

`gameMode <profile>` and RTCW's original `startSingleplayer` / `startMultiplayer`
menu commands now share a mode-switch controller. It validates a different mode
of the current title, startup availability, installation markers and the launch
arguments before saving configuration or shutting anything down. Both RTCW
profiles remain gated, so this controller cannot bypass their runtime blockers.

The manager formats a fresh startup command with the canonical target profile,
current `fs_basepath`, `fs_homepath` and `r_fullscreen`, and an empty `fs_game`.
This preserves the installation and writable home while allowing the target
mode to load its own settings and modules. A mode-specific mod is not carried
across the boundary. Quoted paths preserve spaces, semicolons, plus signs,
trailing backslashes and Unicode; quote/control characters and commands over
the 1,023-byte startup limit are rejected without truncation.

The independently authored [platform handoff](../../code/platform/process_relaunch.cpp)
copies the command and resolves this process's executable. Preparation starts
no child. The controller saves the current mode's configuration, shuts down the
server/client, modules, renderer and filesystem, and releases network sockets
before the platform quit path launches the new mode. Recoverable errors, fatal
errors and recursive shutdown cancel a prepared handoff. A launch failure after
shutdown is reported with an unsuccessful process exit; it cannot arm a later
quit to retry unexpectedly.

Windows uses an explicit image path with
[`CreateProcessW`](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-createprocessw)
and disables handle inheritance. Startup reads `GetCommandLineW` and converts
directly to UTF-8 instead of passing through WinMain's ANSI argument. Linux and
macOS replace the process with `execv` after terminal cleanup and marking
non-standard descriptors close-on-exec. Linux executable discovery uses
`/proc/self/exe`; macOS uses dyld. Other Unix ports return an explicit unsupported
relaunch diagnostic. Windows and Linux have runtime fixture coverage; the macOS
branch still needs a supported-platform build and runtime check. No shell,
PATH search, separate WolfSP/WolfMP executable or in-process profile mutation is
used.

`process_relaunch_tests.py` relaunches a windowless synthetic executable in both
directions, through the actual command formatter, OS process API, startup
argument reconstruction and engine tokenizers. It verifies image identity,
Unicode executable/home paths, command values and descriptor isolation.
`game_mode_transition_tests.py` executes the real controller and quit sequence
against synthetic services, including blocked/missing/mixed installations,
cross-title requests, repeated mode requests and preparation/config-write
failures. These establish handoff behavior independently of RTCW gameplay.

Windows filesystem boundaries now retain those UTF-8 paths through file and
directory access, enumeration, atomic replacement, removal, statistics, native
DLL loading, home discovery and Steam registry/environment discovery. Bounded
wide-character conversions reject malformed or oversized input without
truncating it or changing the process locale. The synthetic Windows fixture
uses real Unicode files and a tiny test DLL; registry fault cases use mocked
APIs and make no registry changes.

The first real Unicode-home startup probe exposed inherited ANSI file calls
writing console logs into a garbled sibling directory. After the wide-path
correction, all four Q3/QL automatic/explicit startup probes pass with the
intended Unicode home. The original failed manifest and recovered logs remain
separate from the successful rerun. Automatic approval review rejected removal
of that disposable garbled directory with "blocked by policy"; it remains
contained under `.tmp/rtcw-manager` (eight files, 483,956 bytes, no links).

## Map format selection

The manager now classifies BSP versions for the selected title before map
translation or loading. Q3 and QL retain their existing v43/v46/v47 asset set.
RTCW SP and MP accept the original v47 contract: a 144-byte, 17-lump header.
Only the Quake Live format reads the extra advertisement descriptor at byte
144. The collision loader and all three renderers use this decision, so RTCW
entity payload is no longer interpreted as a Quake Live lump descriptor.

The renderer receives a narrow `GameBspFormat` import bound to the immutable
engine selection. Renderer API 15 rejects missing callbacks and older import
tables before copying them or publishing GLx imports. The file loaders also
reject a missing selection/callback and preserve the existing lump bounds and
identifier checks. The legacy standalone Q3 BSP-compiler branch retains its
fixed Q3/QL format policy; fnTech3 does not build that historical utility.

The [disk-layout facts](../../code/compat/rtcw_bsp_layout.def) check all 57
fields of 12 common records, their sizes/alignment, lump indices and surface
kinds. Ninety-six new assertions pass against original SP, original MP and
both iortcw modes on Windows x86/x64. The shared engine disk records satisfy
the same layout checks. With the four collision-handle/content assertions
below, the complete reference probe contains 1,850 SP and 1,845 MP assertions,
including the earlier module contracts.

Read-only metadata inspection of ten maps from the legitimate Steam
installation found v47, valid ranges for all 17 lumps, and first payload at
144 in each sample. `.tmp/rtcw-manager/bsp-observations.json` records archive
and map names, sizes and SHA256 values; no map payload was extracted or staged.
This is file-layout evidence, not a successful RTCW collision/rendering run.

`bsp_profile_loader_tests.py` executes the actual collision and three renderer
loader controllers with synthetic files, the real manager classifier and mock
lump services. It checks SP/MP headers, Q3/QL advertisement routing, legacy
translation routing, all truncated prefixes, invalid versions/identifiers,
every negative lump range, missing files and missing game contracts. A separate
fixture supplies only the old renderer-import prefix to verify rejection
before the new field can be read. Full RTCW material, collision-flag, model,
fog and animation behavior remains open.

## Collision models and module traces

The catalog owns a 512-value collision-handle range for both RTCW modes,
compared with 256 for Q3/QL. RTCW reserves 510 for a temporary capsule and 511
for a temporary box; Q3/QL retain 254/255. Model resolution, position tests,
sweeps and transformed queries now use the selected contract. A real RTCW
inline model numbered 254 or 255 therefore retains its own geometry.

The loader rejects inline-model lists that overlap either reserved handle,
before allocation or publication. This also corrects an inherited Q3/QL edge
case: the old count limit admitted models at temporary-hull indices. The
unambiguous model-count limits are 254 for Q3/QL and 510 for RTCW, including
world model zero. Synthetic tests exercise actual loading/resolution around
both limits, all four catalog profiles, high RTCW indices, negative handles,
and `INT_MAX`. The invalid-handle diagnostic no longer adds to an unchecked
signed handle. The existing capsule geometry tests also exercise both RTCW
modes without Quake Live's cylinder-radius adjustment.

Cgame map loading, inline-model lookup, temporary box/capsule creation and
point-content imports route to the selected collision implementation. Four
cgame trace imports and the two game trace imports now write through the
frozen 56-byte RTCW record. The engine trace remains separate, and results
publish only after the backend and conversion succeed. Required null pointers,
backend failures and invalid boolean encodings leave the caller's record
unchanged; optional null bounds remain supported. SP/MP dispatcher fixtures
check argument order, box/capsule selection, transformed queries, aliased
buffers, short argument arrays, guard bytes and preservation of raw map flags.

The [offline collision probe](../../scripts/probe_rtcw_collision.py) compiles
the shipping collision sources and math/checksum helpers with synthetic
filesystem, allocator and cvar services. Only its startup-selection callbacks
are substituted to exercise registered RTCW contracts while normal engine
startup remains gated. It reads legitimate `escape1` from `pak0.pk3` and
`mp_beach` from `mp_pak0.pk3` through an owned pipe, loads all collision lumps,
checks every inline model's bounds, runs 128 deterministic world sweeps and
point queries, and tests a temporary capsule contact. No game module,
renderer, input system or writable installation access is involved.

Those two maps pass on Windows x86/x64 and Linux x86 with ASan/UBSan and leak
detection. SP has 230 models and 15,321 surfaces; MP has 39 models and 6,123
surfaces. Each run completes 129 traces, with 119 SP and 121 MP contacts. The
reports and exact source/map/executable hashes are under
`.tmp/rtcw-manager/collision-retail-x86`, `collision-retail-x64` and
`collision-retail-linux-x86`. These establish subsystem loading and query
behavior, not original-engine trace parity, live prediction or gameplay.
Read-only header metadata for 50 installed map records found at most 232
models; the larger handle range is also exercised with synthetic maps.

## Rendering records

Q3 and QL now have separate frozen `refEntity_t` (140 bytes) and `refdef_t`
(368 bytes) declarations. The pinned FnQuake3 renderer header blob is
`14c262d9d72a93401a607b75c21aa3a0f2e32b06`; the pinned FnQL blob is
`8dfbeb741d8f990dc353c4867d1d3d1203fc8e5d`. Both carry GPL-2.0-or-later
headers. These layouts are source facts, not a new retail-module observation.
`probe_render_contracts.py` checks 43 record/field/type assertions per profile
against those exact Git revisions, preserving the reference worktrees.
`legacy-render-x86.json` passes both profiles; `q3-render-x64.json` passes Q3.
The pinned FnQL platform header intentionally rejects x64, as recorded in the
earlier `legacy-render-x64.json`; it is not bypassed to claim a QL x64 client.

The independently authored
[`render_record_adapter.hpp`](../../code/compat/render_record_adapter.hpp)
copies fields into engine-owned records. Both normal and integer-shader-time
cgame/UI entity calls check the frozen record size before reading it. QL's
native scene wrappers also copy only the frozen view before framebuffer
scaling. The shader-time word is preserved without a float/integer conversion.
Q3/QL captures, highlighting, FOV policy and QL viewport scaling retain their
existing behavior on the resulting local copies.

The internal entity now has a separate RTCW animation/effect tail: torso axes
and frame interpolation, SP scale (MP receives unit scale), fire direction,
fade times, highlight intensity, effect flags and entity identity. RTCW's ten
entity-type ordinals are translated explicitly; Q3/QL still accept only their
original eight. RTCW's highlight/shadow-plane and blink/wrap-frame aliases
retain both meanings. Unknown original-module renderfx bits do not activate
engine flags; in particular, modern iortcw's added crosshair flag requires a
separate module-contract decision.

The internal view keeps RTCW portal/sky/underwater/snooper state and fog in its
own tail. RTCW's skybox bit cannot become Q3/QL's `RDF_NOFIRSTPERSON` flag.
SP's underwater/draw-skybox alias is preserved; MP does not gain that SP flag.
Both fog layouts are marshalled, including SP's additional dirty word. Invalid
entity ordinals and boolean representations reject the entire transfer without
publishing partial state. Unknown RTCW view bits cannot enable Q3 FOV/capture
flags. All Q3/QL transfers clear the full RTCW tail.

Renderer API **18** covers these enlarged internal records, the later
canonical 2D quad export and indexed entity-tag export; all three renderers retain the translated view tail
in their scene state. It does not
change the frozen game-module structures or their import ordinals. The
offline regression executes the actual Q3/QL cgame/UI scene cases and QL
native wrappers, exact-size/unaligned inputs, short VM ranges, failure guards,
all entity types, flag combinations, source immutability and float/integer
shader-time bit patterns. The expanded RTCW source probe passes **1,886 SP and
1,876 MP assertions** against original and iortcw headers on Windows x86/x64,
including 36 SP and 31 MP renderer flag/type facts.

The shared RTCW renderer-import handler now connects 22 original SP/MP UI and
cgame imports: world loading, model/skin/shader registration, scene clearing,
entity/view submission, color, stretch pictures, model bounds and shader
remapping. Entity and view inputs pass through the frozen record adapters;
RTCW keeps its authored viewport and original float shader time. Model-bounds
outputs are staged and both ranges checked before the backend runs. Null color
resets and optional shader-remap time offsets retain their original meanings.
The handler checks the active profile before reading arguments, so Q3/QL keep
their established dispatch. Source reports also hash the original/iortcw
client dispatch files as observations; no implementation text is incorporated.

Four further UI/cgame imports submit single polygons or batches through a
separate 24-byte RTCW vertex record. Counts and byte products are checked before
reading, and unaligned vertices are copied field by field into canonical
storage. Small batches use stack storage; large batches use temporary engine
hunk memory. Zero counts and RTCW shader zero consume no vertex memory. All
three renderers reject null/empty/negative submissions and use subtraction to
check remaining vertex capacity without signed overflow. Valid Q3/QL shader
zero, ordering, fog selection and capacity truncation retain their behavior.
The source probes now pass 1,894 SP and 1,884 MP assertions, including the
vertex's size, alignment and three field offsets/sizes (`abi-polygons-*.json`).

Calls with different arguments require separate implementations. RTCW lights
take an additional overdraw argument, and tag lookup takes an entity plus
a starting tag index (implemented below for MD3/MDC). Its decal call takes an orientation angle, always reads
four input points, and writes five floats per point; negative fragment counts
select the old mapping, and negative output point counts flag supplied texture
coordinates. Routing those calls through the Q3 signatures would be incorrect.
RTCW's 2D gradient applies the current color to the top two corners and a
separate RGBA color to the bottom two; the supplied gradient-type integer is
ignored by the original backend. MP's rotated-picture argument is measured in
turns and produces four points around the rectangle's center at its half-
diagonal radius, including for nonsquare input rectangles. It is not a generic
rectangle rotation. Both calls now have independently authored adapters in
[`rtcw_draw_adapter.hpp`](../../code/compat/rtcw_draw_adapter.hpp). They produce
an engine-owned quad with four positions, texture coordinates and explicit
corner colors. Missing color bits sample the current 2D color when the renderer
executes the command. Invalid geometry/nonfinite color inputs fail before
publication; finite out-of-range gradient colors clamp to the byte range.

All three renderers queue the quad through the same command producer/backend.
The backend retains each renderer's existing stretch-picture setup, shader
batching, overflow handling, bloom and UI bookkeeping, then replaces the four
reserved vertices before submission. Existing stretch-picture commands retain
their previous size and behavior. Renderer API 17 adds only an internal export;
no new game-module ordinal or retail structure is introduced. SP cannot route
MP's rotated-picture import.

[`probe_rtcw_draw.py`](../../scripts/probe_rtcw_draw.py) compiles the original
and iortcw command producers and CPU drawing functions in disposable fixtures,
preserving their source notices. It compares geometry, texture coordinates,
triangle order and colors with the independent adapters. There are 128
gradient cases per SP reference and 256 gradient/rotation cases per MP
reference: **768 comparisons per platform**. Original/iortcw references pass
on Windows x86/x64 and Linux x86 with ASan/UBSan and leak detection in
`draw-reference-x86.json`, `draw-reference-x64.json` and
`draw-reference-linux-x86.json`. Reports pin source and adapter hashes. These
compare positions within 0.0001 units and texture coordinates, indices and
colors exactly. They are CPU reference comparisons, not RTCW GPU rendering
or gameplay evidence.
MDS, special surfaces, fog/effect rendering, original-engine parity and
full runtime rendering remain open; the startup gate stays in place.

### MDC compressed models

The independently written [`tr_mdc_format.h`](../../code/renderercommon/tr_mdc_format.h)
reads IDPC version 2 through checked little-endian byte operations. It keeps
disk records separate from engine objects, accepts unaligned storage, validates
all frame maps, triangle indices, names and section extents, and publishes a
view only after the whole model passes. Vertex/frame products are checked before
multiplication on x86. Borrowed views require immutable backing storage; renderer
registration makes an owned hunk copy before releasing the filesystem buffer.

The installed Steam data supplied **711 MDC records in 1,269 surfaces**, including
override records. All observed surface discriminator words are 7, not IDPC.
There are tag-only models with no surfaces, and valid models with up to 41
surfaces; the historical 32-surface constant cannot serve as a file validator.
Finite inverted frame bounds occur in 1,064 frames, including 21 frames with
visible geometry. The reader preserves these exporter sentinels. Renderer
metadata also retains RTCW's named `mg42`/`sherman` culling-envelope convention.
Malformed ranges and nonfinite frame metadata remain rejected.

[`tr_mdc_decode.h`](../../code/renderercommon/tr_mdc_decode.h) keeps base positions
and compressed offsets separate through interpolation. Offsets retain their
0.05-unit encoding instead of being quantized into MD3's 1/64-unit coordinates.
Compressed normals use a geometrically constructed 256-direction dictionary,
including the encoding's six-decimal rounding residues and signed zeroes.
Base normals use RTCW's 1,023-period sine table; the Q3/QL renderer waveform
table has a different period and bias and is left unchanged.

[`probe_rtcw_mdc.py`](../../scripts/probe_rtcw_mdc.py) compares the decoder with
original SP/MP and iortcw CPU interpolation functions compiled in disposable
fixtures. It reads every installed MDC directly from the installation's paks
into memory; no retail geometry or modules are staged. Each reference passes
**3,961,341 vertex comparisons and 16,231 tag comparisons**, with zero observed
position or normal error. The complete direction dictionary and sine table
also compare byte for byte. Windows x86/x64 and Linux x86 ASan/UBSan runs pass
in `mdc-reference-x86.json`, `mdc-reference-x64.json` and
`mdc-reference-linux-x86.json` under `.tmp/rtcw-manager/`. The source layout
probe adds 55 MDC assertions, reaching **1,949 SP / 1,939 MP assertions** per
original/iortcw source family on both Windows targets.

All three renderers now have a native MDC model representation, immutable
compressed geometry, LOD registration, frame/tag metadata and a shared mesh
view for culling, skins, fog and shadow submission. Format discovery appends
MDC after the existing IQM/MDR/MD3 fallback order. Missing LODs use an available
coarser level; inconsistent frame counts are rejected before an LOD is accepted.
The raster backends and RTX geometry transport sample the same animation
decoder. None of this changes the frozen module layouts or renderer API 17.
That checkpoint's MDC tag path serves the existing boolean engine tag
interface; the subsequent indexed RTCW boundary is described below.

The CPU/installed-file comparisons establish decoding evidence, not retail
RTCW rendering or gameplay. The renderer validation checkpoint below records
the separate integration and synthetic runtime evidence.

### Indexed mesh attachments

SP/MP cgame `CG_R_LERPTAG` and UI `UI_CM_LERPTAG` now use the distinct
four-argument RTCW contract: output orientation, frozen entity, tag name and
inclusive starting index. A frozen orientation holds 48 bytes: three origin
floats and a 3-by-3 axis. The module adapter validates the complete output and
mode-specific entity ranges, captures inputs, calls the renderer using local
records and publishes the output field by field only after the call returns.
Zero is a valid first tag index; failure remains `-1`. Short/unaligned records,
null inputs, backend failures, integer narrowing and aliased inputs/outputs
have executable regression coverage. Q3/QL retain their existing dispatch and
boolean tag interface.

All three renderer modules share the independently authored indexed MD3/MDC
lookup in `tr_mesh_tags.h`. Old and current frames are searched independently;
repeated names are enumerated from the supplied index. High frames clamp to
the last frame, and negative frames safely clamp to zero. RTCW's two rounded
blend fractions, per-axis normalization and tag-name truncation are preserved.
Failed/missing/unsupported lookups return a zero origin and identity axes;
nonfinite or overflowing interpolation cannot publish a partial orientation.
The new internal export was appended in renderer API 18. The subsequent MDS
path is recorded below; modern iortcw IQM/MDR indexed extensions remain open.

The original SP/MP engines return the current-frame tag index. iortcw returns
the old-frame index when an MD3's tag order differs between frames. The
implementation follows original RTCW, and a synthetic reference case verifies
this intentional distinction. The installed corpus has no observed index
differences. `scripts/probe_rtcw_tags.py` compares **804 installed models**
(711 MDC, 93 MD3) in memory against all four references. Every reference passes
**427,880 calls** across all tag frames, eight blend/extrapolation values,
inclusive/negative/over-limit starts, frame clamps and missing names. Windows
x86/x64 numerical errors are zero; Linux x86 ASan/UBSan maximum errors are
`9.53674316e-7` for origins and `5.96046448e-8` for axes, within the recorded
`1e-4`/`2e-6` comparison tolerances. No installed geometry is staged or reported.
The source record probes now pass **1,955 SP / 1,945 MP assertions** for each
original/iortcw source family on both Windows architectures.

### MDS skeletal file boundary

The preceding indexed-attachment checkpoint is followed by an independent
MDSW v4 file boundary in `tr_mds_format.h`. It validates signed header links,
file/surface/variable-weight ranges, global bone and triangle indices, collapse
maps, finite consumed floats, names and bounded acyclic skeletons before
publishing an immutable view. It accepts forward ancestry and multiple roots;
the 128-bone contract bounds ancestry work. Weight values are preserved without
renormalization. Surface identifiers, shader indices, the fourth compressed
angle and unused fixed-parent exporter words do not become engine behavior.
This initially standalone reader now backs the owned skeletal models described
in the subsequent MDS integration checkpoint below.

The read-only installed corpus contains **34 MDS models, 150 surfaces and
76,674 frames**, with 48–75 bones per model, 12–21 tags, 15–862 vertices per
surface and one to five weights per vertex. Some skeletons have two roots.
All weight indices name global bones, despite the old header's local-index
comment; 18,102 weights exceed their surface's reference count while still
being valid global references. Surface bone lists need not be sorted. All
observed collapse links point to earlier vertices (vertex zero references
itself), and all 150 surface header links point back to their owning model.

`scripts/probe_rtcw_mds.py` passes against original SP/MP and iortcw SP/MP on
Windows x86/x64 and Linux x86 ASan/UBSan. Each reference compares all **5,361,545
compressed bone records, 2,353 bone descriptions, 587 tags, 31,054 vertices,
39,227 weights and 42,876 triangles** with exact scalar/byte matches. Reports
`mds-format-reference-{x86,x64,linux-x86}.json` retain hashes and counts only.
The C11/C++17 boundary fixture passes on those platforms, including every
truncation, byte mutations, full-depth 128-bone cycles, unaligned records,
malformed names/indices/counts and transactional failures. Logs are
`mds-format-{x86,x64}.log` and `mds-format-sanitizers-final.log` under
`.tmp/rtcw-manager/`. Seventy-four new layout facts increase the source probe
to **2,029 SP / 2,019 MP assertions** for each source family on x86/x64. This
format-only addition postdates the indexed-attachment suite. The subsequent
pose, LOD and native-renderer evidence is recorded below.

### MDS poses, detail selection and renderer integration

`tr_mds_pose.h` evaluates separate leg and torso animation channels, bounded
bone ancestry, shortest-angle interpolation, normalized offset directions,
torso rotation about the authored pivot and weighted positions. It preserves
RTCW's first-weight-only normal transform and the different matrix operation
for attachment bones. All work is local and staged before publication; there
is no shared bone cache across entities or tag calls. Nonfinite/overflowing
animation state fails without publishing a partial pose.

`scripts/probe_rtcw_mds_pose.py` compares the actual surface bone lists and
fresh tag ancestry against original SP/MP and iortcw SP/MP. Each reference
passes **76,674 frames, 5,727,837 requested bone poses, 1,114,279 weighted
vertices and 21,545 attachments** across all 34 installed models. The probe
mixes independent leg/torso frames, torso rotations, interpolation and finite
extrapolation. Windows x86/x64 errors are zero. Linux x86 ASan/UBSan has a
maximum position error of `3.05175781e-5` units and zero direction error,
within the preselected `0.001`/`0.00002` tolerances. Reports are
`mds-pose-surfaces-reference-{x86,x64,linux-x86}.json` under
`.tmp/rtcw-manager/`; they contain counts and source/asset hashes, not models.

An initial artificial evaluation of every serialized bone exposed a reference
state leak: a full-torso tag can leave the global `fullTorso` choice active for
an unrelated partial-torso mesh bone. This occurs with `femzombie/body.mds`
when unused tag bone 4 is evaluated before mesh bone 5. Its actual surface
lists omit that tag at that point. The corrected probe follows authored
surface ordering and checks fresh tag ancestry; both match. fnTech3 derives
the choice separately for each bone, and a regression test pins that behavior.
This is a reference-code observation, not evidence of a retail gameplay bug
or parity for arbitrary interleavings of the reference cache.

`tr_mds_model.h` validates the whole file and renderer capacities before
allocating one owned block for immutable disk bytes, frame metadata, surfaces
and native vertex descriptors. Authored surface names are preserved. Stale
representations from rejected format fallbacks cannot select another model's
path. All three renderers register `.mds`, traverse it through the shared mesh
frontend, cull using its leg-frame metadata and submit skeletal geometry through
`SF_MDS`. The common backend deforms retained vertices and applies the authored
collapse ancestry, discarding only triangles made degenerate by reduction.

Renderer API **19** also carries the game manager's immutable selected game ID.
SP's detail flag 8 forces full detail; MP's flag 8 halves detail, and flag 32
permits dead models to pass below the surface minimum, with RTCW's 35% floor.
The backend uses its current view for projected radius. SP hides `h_blink`
surfaces unless blinking; MP looks up the surface's `_b` skin entry, falling
back to the regular entry. Q3/QL entities retain their own flags and can use
MDS assets through their single animation channel. The existing renderer
lighting and shadow infrastructure is preserved.

`scripts/probe_rtcw_mds_lod.py` compares the source detail calculation and
actual source triangle-reduction blocks. Each of four references passes
**134,400 detail fractions and vertex counts, 31,054 reduced meshes and
19,909,176 output indices** on Windows x86/x64 and Linux x86 ASan/UBSan, with
exact equality. Every retained count from one vertex through full detail is
checked across the installed 150 surfaces. Reports are
`mds-lod-reference-{x86,x64,linux-x86}.json`; malformed/zero-count behavior and
nonfinite inputs are additionally covered by independent C11/C++17 fixtures.

Indexed MDS tags use the same pose decoder, preserve the unnormalized authored
basis, compare the complete tag name, support inclusive duplicate-tag indices
and return an all-zero orientation on failure, as the original engines do.
The legacy boolean interface also accepts MDS with one animation channel.
RTX's bounded CPU transport receives full-detail skeletal geometry and uses
the same material rules. Its ray path has CPU evidence only at this checkpoint.

The focused Windows x86/x64 renderer tests cover owned pointers, variable
weights, profile differences, overflow/failure propagation, frame validation,
aliased tag inputs, duplicate/long names, continuous reduction, shader fallback
and exact surface dispatch. Linux x86 ASan/UBSan and leak checks pass the
format, pose, raster, tag, LOD and ray fixtures. An initial ray assertion
compared a stored float with x87's wider expression value; the test now
explicitly stores the expected float before exact comparison. This required
no change to the renderer's geometry calculation.

The Windows x86 client, dedicated server and all three renderer builds pass.
`mds-live/manifest.json` records three windowed 640x480 Q3 `testgun` runs using
the independently authored skeletal fixture in `mds-fixture.pk3` and client
SHA256 `7ea9896ce928155041e8dc364f7cb31d7366452c64788a5a928ca7018b33533f`.
All three engine screenshots were inspected: the cyan mesh has the expected
deformed top edge over a readable Q3 scene and HUD. The runs use frame 1,
old frame 0 and backlerp 0.5, observe `.md3` to `.mds` fallback and exit normally.
GLx/Vulkan/RTX draw more than 47,000 fixture pixels each; RTX runs in raster
mode. Input and sound are disabled on unswitched private desktops. Writable
homes are removed after containment/link checks. This proves native synthetic
MDS drawing through a legitimate Q3 module, not retail RTCW gameplay, complete
RTCW shader semantics or GPU ray tracing. RTCW startup remains gated.

## Shared catalog limits and discovery

The manager also owns configstring/client limits and the player-configstring
base. Internal storage holds 2,048 configstrings and 128 possible client
records; Q3/QL still enforce 1,024 and 64. MP uses player configstrings starting
at 576; Q3, QL and SP retain 544. Module export adapters preserve each frozen
gamestate layout and never copy the enlarged engine record wholesale.

Windows and Unix Steam discovery now keep one cache entry per profile ID.
Multiple modes can resolve the same installation without overwriting another
title's path. Steam AppID discovery remains separate from Steamworks services:
RTCW uses AppID `9010` for discovery and enables neither Steam authentication
nor Workshop integration.

## Observed contracts

The supplied source references are:

- [id Software RTCW SP](https://github.com/id-Software/RTCW-SP), local
  `E:\_SOURCE\_CODE\RTCW-SP-master`.
- [id Software RTCW MP](https://github.com/id-Software/RTCW-MP), local
  `E:\_SOURCE\_CODE\RTCW-MP-master`.
- [iortcw](https://github.com/iortcw/iortcw), local
  `E:\_SOURCE\_CODE\iortcw-master`, used to cross-check archive precedence and
  its newer protocol identities.

These source releases carry GPL-3.0-or-later notices, including id Software's
additional terms. Their headers and license files were inspected. This slice
uses compatibility facts and independently authored boundary records, policies
and message codecs; it incorporates no reference engine/game implementation or retail
module behavior. Future implementation imports require their own license and
attribution review. The repository's existing GPL notices are preserved.

| Contract | RTCW SP | RTCW MP |
| --- | --- | --- |
| Canonical profile | `rtcw-sp` | `rtcw-mp` |
| Other explicit aliases | `rtcwsp`, `wolfsp` | `rtcwmp`, `wolfmp` |
| Base directory | `main` | `main` |
| Retail protocol / demo default | 49 | 60 |
| iortcw protocol identity | 50 | 61 |
| Client configuration | `wolfconfig.cfg` | `wolfconfig_mp.cfg` |
| Dedicated configuration | `wolfconfig_server.cfg` | `wolfconfig_server.cfg` |
| Shared media | `pak0.pk3` | `pak0.pk3` |
| Mode overrides | `sp_*` | `mp_*` |
| Excluded archives | `mp_*` | `sp_*` |
| Retail Windows module names | `cgamex86.dll`, `qagamex86.dll`, `uix86.dll` | `cgame_mp_x86.dll`, `qagame_mp_x86.dll`, `ui_mp_x86.dll` |

Archive sorting models the reference engine's virtual `zz_` prefix for mode
overrides without modifying names on disk or in memory. A user archive sorting
after that key retains its precedence. Equal virtual keys use the real name as
a deterministic tie-break; this tie-break is an explicit fnTech3 policy, not a
claim about the reference engine's unspecified `qsort` tie order. Prefix checks
are case-insensitive, consistent with fnTech3 virtual paths.

SP and MP local configuration loading, automatic persistence and filesystem
recovery now select their catalog filename. RTCW retains its reference startup
behavior of executing `autoexec.cfg` in safe mode while omitting the managed
config; Q3 retains its existing safe-mode omission of both. QL retains its
profile-root hardware/replicated files and legacy migration behavior. Managed
configuration filenames from every registered profile are excluded from pak
lookup.

## Native module loading and calls

The shared VM loader gets ordered filename/location candidates from the game
manager. Windows SP searches `*_sp_x86.dll` and then retail `*x86.dll`; MP uses
`*_mp_x86.dll`. Unix names use `.sp.`/`.mp.` separators and SP retains its
unsuffixed retail fallback. RTCW Windows x64 names use `x64`; the engine's
internal architecture spelling is `x86_64`. Existing Q3/QL names and the
game-only Linux `qagamex64.so` fallback retain their order.

Both SP filenames are searched through mod/pak paths before either filename
is considered in the selected installation root. Only known native-module
leaves can use that root fallback. Canonical containment rejects a symlink
escaping the installation; unrelated root files are not exposed to the VFS.
Root images enter the same temporary cache and byte-pinning path as pak images,
so a later pure restart can reuse the image loaded before admission. These are
module lookup and ownership checks, not evidence of RTCW pure interoperability.

The [call registry](../../code/compat/rtcw_module_calls.def) records all original
UI, cgame and game imports/exports separately for SP and MP. Its
[ABI interface](../../code/compat/rtcw_module_abi.h) provides bounded constant-time
lookup and explicit absent/unknown results. For example, UI ordinal 15 is SP's
file seek but MP's file write; cgame memory-set is SP 110 and MP 100. No shared
handler may infer argument compatibility merely from a matching name.

The cgame boundary now transfers RTCW gamestates, user commands and snapshots
through the frozen adapters. Snapshot validation covers sequence and entity
history, clamps output to 256 entities, clears unused output, and publishes
only after every player/entity conversion succeeds. A malformed final entity
cannot leave a partially written module snapshot. SP/MP dispatch also maps 35
compatible primitive imports to shared handlers. UI and server dispatch now
resolve their own SP/MP ordinals, with 34 UI and 24 game primitive imports
mapped to existing handlers. Server user commands use the frozen SP/MP adapter.
Unsupported calls fail with their mode, ordinal and name before reading arguments.
The remaining trap/export, renderer/audio/input contracts and RTCW native
execution stay open; the startup gate is unchanged.

Engine-to-module calls now translate 11 common game exports, 12 UI exports
and nine cgame exports. The RTCW UI inserts a get-active-menu export at 8,
so engine console-command and connection-screen requests must become 9 and 10.
All 20 SP and 19 MP menu values translate separately in both directions;
menus absent from a mode are rejected. The engine's get-active-menu request
uses a separate internal token so Q3/QL export numbers remain unchanged. Cgame init
requires two meaningful SP arguments and three MP arguments; frame stereo and
event-handling values are mapped explicitly. Missing arguments and unsupported
exports fail before module entry or syscall-counter changes. RTCW accepts
legacy native entry points only. Release builds now validate VM argument counts
too: the inherited guard was debug-only and did not reject negative counts.
Offline tests also exercise unchanged Q3/QL legacy, structured and QVM dispatch.

Cgame popup imports now route to the correct UI menu. SP permits briefings
before active play; both modes gate ordinary popups during loading and demos,
and retain their own book, pregame, class/team and limbo choices. Closing a
popup follows SP's single dismissal or MP's two limbo dismissals after checking
the active menu. Unknown menu results are rejected after balancing the VM call
level. Synthetic callbacks test the complete cgame-to-UI route without sending
input to a running application. Retail `startMultiplayer`/`startSingleplayer`
commands use the shared mode-switch controller described above.

The [key-number table](../../code/compat/rtcw_keycodes.def) freezes 110 named
key values. Bidirectional translation preserves ordinary byte keys and maps
wheel, joystick and auxiliary buttons by meaning despite the shared engine's
additional mouse buttons. Engine-only keys are ignored at RTCW exports without
changing module counters. Character-event flags preserve their byte payload.
UI/cgame binding and key-state imports use the same translation, and binding
lookup searches only keys the original module can represent. Text queries
respect output capacity, including zero-length buffers. RTCW catcher queries
expose its four flags; updates preserve engine browser ownership without
interpreting unknown RTCW bits as QL flags. These are synthetic state tests,
not live input or international text-rendering evidence.

RTCW cvar registration and updates use a frozen 272-byte record, with no QL
flags word before its 256-byte string. Registration rejects private engine
cvars without publishing an incomplete record. Update handles unchanged,
cleared and private cvars transactionally, including module strings without
a terminator. The original flag bits are translated explicitly; MP's retail
`WOLFINFO` bit `0x0800` becomes a separate internal flag, never QL's protected
bit. MP map spawn and frame updates publish that group in configstring 36.
Q3, QL and SP do not publish this MP channel.

iortcw changed MP `WOLFINFO` to `0x4000` and used `0x0800` for server-created
metadata. The source probe checks each variant's actual value separately.
The implemented cvar boundary targets original RTCW modules; selecting the
modern filename does not establish compatibility with iortcw's changed flags.
A separate module-contract decision remains necessary before enabling those
modules. Unknown original-module flag bits are discarded, never interpreted
as QL metadata.

The cgame command-value import also has distinct argument counts: SP supplies
weapon, holdable, sensitivity and its 16-bit damage value; MP supplies two
separate setup/identification bytes instead. The generated command preserves
the input button/kick fields, clears QL-only command values, and sends the mode
tail through the existing history and frozen record adapters. Synthetic tests
exercise this path without generating or reading real input events.

The [client record adapters](../../code/compat/rtcw_client_adapter.hpp) add
RTCW's 3,084-byte UI state, 7,268-byte renderer information, 1,040-byte parser
token and 20,548-byte font. UI and cgame dispatch check each record's own
capacity. Renderer extensions truncate to RTCW's 4,096-byte field; strings
remain terminated even when an internal source array lacks a terminator.
Invalid connection/renderer enums or booleans are rejected without modifying
output. Parser EOF/errors and partial font-backend failures also preserve the
caller's record. All 256 glyphs cross explicitly. Calendar imports in all
three modules use a frozen 36-byte record and preserve output if localtime
fails, without reading an uninitialized output buffer.

UI/cgame parser-location imports stage the backend's full 1,024-byte filename
before publishing at most 127 characters and a terminator to RTCW. Original
`ui_shared.c` diagnostic callers in both source trees allocate 128 bytes.
The backend capacity is compile-time checked. Missing handles, partial backend
failure and an unterminated result preserve both caller outputs; null outputs
are rejected before entering the parser. This also prevents the shared
parser's unbounded legacy copy from writing into the smaller RTCW buffer.

The shared renderer does not yet report anisotropy capabilities through this
contract; the RTCW record leaves them unavailable, along with unsupported
legacy NVIDIA fog and ATI Truform capabilities. iortcw renamed the multitexture
count while preserving its offset, and changed `TC_EXT_COMP_S3TC` from 2 to 3.
The probe records that distinction; original RTCW adapters continue to emit 2.
These record transfers do not establish RTCW scene rendering or parser-language
parity. Renderer operations, capabilities, assets and complete parser integration
remain open.

The expanded source probe passes **1,750 SP and 1,745 MP assertions** against
both original source trees and iortcw, on Windows x86 and x64. It verifies
every frozen field plus original import/export ordinals, API versions, cvar
encodings, menu/key/event values and 13 server-flag encodings. The server entity boundary translates those flags
by meaning: RTCW capsule, client visibility and server-info flags must not
reuse colliding Q3/QL bit values. Engine updates preserve unknown module bits.
RTCW visibility-dummy, cast-AI and footstep flags have separate canonical bits;
their complete engine behavior remains part of the open runtime work.
iortcw renames the unused embedded `entityShared_t.s` pad to `unused` while
preserving its layout. Its legacy Windows header requires two probe-only
compiler spelling accommodations; no reference source is edited. Additional
iortcw imports beyond the original contracts are not implemented by this table.
Reports and source hashes are retained as `abi-modules-*.json`,
`abi-iortcw-*.json`, `abi-flags-*.json`, `abi-cvars-*.json` and
`abi-client-pods-*.json`, `abi-key-exports-*.json` and `abi-popups-*.json`
in `.tmp/rtcw-manager/`.

Manager, loader, root-file ownership and cgame-transfer tests pass, including
unknown ordinals, absent arguments, short buffers, Unicode installation paths,
short reads, unexpected extra bytes, failed closes, stale/wrapped histories
and unchanged destinations on failure. The Linux x86 ASan/UBSan pass is recorded
in `module-sanitizers.log`. It also exposed an invalid-enum read in the existing
profile rejection tests; game/title IDs now have explicit four-byte unsigned
C++ representations, preserving the C ABI and allowing invalid values to be
rejected without undefined behavior.

## Retail identity observations

The legitimate local installation is
`E:\SteamLibrary\steamapps\common\Return to Castle Wolfenstein`.
The Steam manifest identifies AppID `9010`, build `3746`. No account identifier,
credential, game module or asset payload is included in this record.

Early identity probes check bounded ZIP central-directory metadata. These are
identity hints, not payload authentication or a replacement for full pak checks.

| Pak | Metadata entry | Uncompressed bytes | CRC32 |
| --- | --- | ---: | --- |
| `main/pak0.pk3` | `default.cfg` | 2237 | `844222c3` |
| `main/pak0.pk3` | `maps/escape1.bsp` | 12366352 | `5f08b87c` |
| `main/mp_pak0.pk3` | `default.cfg` | 1372 | `91dfaca0` |
| `main/mp_pak0.pk3` | `maps/mp_beach.bsp` | 7343512 | `77533d56` |

The existing Q3/QL fingerprints and malformed-directory bounds are retained.
Unknown or mixed-title evidence still rejects automatic selection. Explicitly
selected RTCW modes can recognize their shared installation without treating
the other mode's marker as a different game installation.

## Frozen module layouts

[RTCW boundary PODs](../../code/compat/rtcw_game_abi.h) are separate from engine
state and from Q3/QL PODs. Their common prefixes are grouped only where the
reference offsets agree. Equal overall sizes do not imply equal semantics:
the final two command bytes contain SP damage (`cld`) versus MP setup and
client identification (`mpSetup`, `identClient`).

| External record | SP bytes | MP bytes |
| --- | ---: | ---: |
| `usercmd_t` | 28 | 28 |
| `playerState_t` | 1472 | 1464 |
| `entityState_t` | 288 | 288 |
| `sharedEntity_t` | 680 | 684 |
| `snapshot_t` | 75256 | 75248 |
| `gameState_t` | 24196 | 24196 |
| `trace_t` | 56 | 56 |
| `refEntity_t` | 232 | 220 |
| `refdef_t` | 432 | 428 |

Notable boundaries include 2048 configstrings, 64 weapon/ammo slots, four
player events, a second entity-state record inside `sharedEntity_t.r`, and
the `linked` field at byte 576. None can be represented by blindly exposing the
current Q3/QL internal layout.

[probe_rtcw_contracts.py](../../scripts/probe_rtcw_contracts.py) compiles the
frozen declarations directly against the supplied reference headers. It checks
every represented field's offset and size as well as each record's size and
alignment: **942 assertions for SP and 934 for MP**, passing with Clang targeting
both `i686-pc-windows-msvc` and `x86_64-pc-windows-msvc`. It performs no module
execution. Ordinary [ABI layout tests](../../tests/game_abi_layout_tests.cpp)
also pass under strict C++17 x86 and x64 compilation.

The [server game-data view](../../code/server/server_game_data.cpp) accepts
these exact SP/MP prefixes with independently validated entity/client strides
and spans. Frozen state overloads retain every RTCW field; shared field access
uses the correct second-entity-state offsets. SP's 128-record client capacity
and MP's 64-record capacity are checked separately. Tests cover complete state
transfers, ping writes, private-tail preservation, short records, expired
references and cross-mode rejection. Canonical overloads now marshal through
the [RTCW state adapter](../../code/compat/rtcw_state_adapter.hpp). They preserve
all SP/MP fields without exposing engine-internal layouts to modules.
This is module-memory evidence, not a claim that SP gameplay supports 128 users.

Engine-owned extensions retain RTCW's extra ammo slots, clip and heat arrays,
events, AI state, cursor traces and mode-specific command/player tails. Existing
Q3/QL member offsets are unchanged; the frozen external Q3/QL records are
unchanged. RTCW trajectories map explicitly to canonical values because RTCW's
enum diverges from Q3 after linear-stop. Invalid trajectory values and trace
booleans fail without publishing partial state. Tests round-trip every field
and array element, reject invalid records, and check guards and native padding.
Q3/QL adapters and packet readers clear RTCW-only state.

Example reproduction from the project root:

```powershell
python scripts/probe_rtcw_contracts.py `
  --sp-source E:/_SOURCE/_CODE/RTCW-SP-master `
  --mp-source E:/_SOURCE/_CODE/RTCW-MP-master `
  --target i686-pc-windows-msvc `
  --report .tmp/rtcw-manager/abi-x86.json
```

Source SHA256 anchors:

| Reference | File | SHA256 |
| --- | --- | --- |
| SP | `src/game/q_shared.h` | `39dbf9c785b72c48fdc569b11fbefb376b2a95271fa8d8ebb79ec5178cf85e77` |
| MP | `src/game/q_shared.h` | `e458a1456ca8b7dfc7e330e854c2d4a61aa64c9e63bd06771c1f6153989d3cb1` |
| SP | `src/qcommon/qcommon.h` | `3df9bf6a73ca65e70e1b36d6e9f09be27a8e94aefc5252b911c6423eb49e1d16` |
| MP | `src/qcommon/qcommon.h` | `84e3eb7b02968326a3e42776b63a52019035471c589be92c9fcc7fd3a35bccb1` |
| SP | `src/cgame/tr_types.h` | `e17a8db656ce630358da1fc1faed2f6149a747e023cb61ac96017cdf0fffc63e` |
| MP | `src/cgame/tr_types.h` | `7c0f0c4d1a72a478e9048dfe2569e52ed0f2bffb6509a6bab66a10cb23ae7ff1` |

## Protocol 49/60 message codecs

The shared [message implementation](../../code/qcommon/msg.cpp) now includes
both independently written [RTCW body codecs](../../code/compat/rtcw_message_codec.hpp).
Their frozen entry points and canonical profile dispatch are built into the
shared client and dedicated executable. The same reference packet fixtures
pass through both entry points, including in-place and truncated-input cases.
Live module, snapshot and connection integration remains open; protocols 50/61
are identities, not verified codecs.

The transport catalog now distinguishes protocol 49 from protocol 60. Original
SP disables reliable XOR (`DO_NET_ENCODE = 0`) and sends an uncompressed connect
request; MP enables XOR and compresses connect requests. Neither adds ioquake3's
sequence checksum. Both source contracts use 32,768-byte messages, 256 reliable
commands and eight 2,048-byte download blocks. Reliable histories and network
storage now accommodate those capacities, while each connection retains its
protocol's window and message-size checks. Tests exercise 64/256-command
history wrap, full 16/32-KiB reassembly, oversized payloads, both packet
directions, and unknown wire profiles. Incoming reassembly now accounts for
the four-byte sequence header when checking the caller's destination size.

Original SP/MP string writers replace high bytes but retain percent signs.
Ordinary readers sanitize both, while big-string readers retain high bytes.
The usercmd key uses the signed-byte `Com_HashKey` formula, independently of
netchan's command-byte substitutions. Profile string/hash regressions pin these
distinctions without changing Q3/QL behavior.

The field order and widths are checked against the original engines. SP uses
a 71-field entity change mask with a 32-symbol dictionary, while MP uses the
last changed field count and individual change flags. The SP dictionary is
recorded as protocol bit-mask facts, without copying the reference lookup
implementation. The original SP writer searches only symbols 0 through 30;
both decoders must also accept symbol 31. SP player state sends 76 field flags;
MP uses a last-field count over 77 fields. Both carry four events, 64 ammo
slots and 64 clip slots, with signed array values and mode-specific command
tails. Binary32 values retain the reference compact encoding where applicable.

[probe_rtcw_messages.py](../../scripts/probe_rtcw_messages.py) separately compiles
the user-supplied engines' `msg.c`/`huffman.c` with an offline synthetic driver.
It compares 1,143 SP and 1,117 MP native encodings byte for byte, including
decoded state and consumed bit count, and exercises decoding in both source
directions. An additional crafted SP packet verifies dictionary symbol 31.
Cases cover every scalar field, every transmitted array slot, dictionary
patterns, mixed baselines, removal/unchanged records and all bit alignments.
Reference sources are read-only, probe executables are temporary, and neither
game modules nor assets are involved.

The 2,260 native-encoding comparisons and the symbol-31 fixture passed on
Windows x86 and x64 and Linux x86. The reports
identify the compiler, source hashes and exact case counts for each run.
Representative [reference packets](../../tests/fixtures/rtcw_message_vectors.inc)
are retained as ordinary automated regressions, independent of access to the
source trees. Tests remove every packet suffix at every bit alignment, exercise
both separate and in-place destinations, and check guard bytes on short writes.
They also cover malformed counts, 1,536 deterministic arbitrary packets,
non-finite floats, negative zero and command clock wrap/rewind.

The decoder rejects malformed counts instead of reproducing the original MP
decoder's silent clamp. Reads publish a record only after successful decoding.
The writer uses a full timestamp on a clock rewind instead of truncating a
negative delta; the resulting packet is valid for the original decoder.
Out-of-range and non-finite floats never reach an undefined float-to-int cast.

The sanitizer pass also exposed an existing issue in malformed Q3/QL profile
tests: an unfixed C++ wire-profile enum made the intentionally unknown value
undefined before the rejecting switch could inspect it. Its underlying type
is now explicitly unsigned, retaining the four-byte engine representation and
all existing values. This is independent of RTCW's packet layout.
The complete message tests (including Q3/QL), canonical ABI adapters and server module-record tests
pass under Linux x86 AddressSanitizer and UndefinedBehaviorSanitizer, with
halt-on-error and leak detection enabled.

Example reproduction:

```powershell
python scripts/probe_rtcw_messages.py `
  --sp-source E:/_SOURCE/_CODE/RTCW-SP-master `
  --mp-source E:/_SOURCE/_CODE/RTCW-MP-master `
  --report .tmp/rtcw-manager/message-x86.json `
  --fixtures tests/fixtures/rtcw_message_vectors.inc
```

## Build and non-regression evidence

The working base is `c75573c` with uncommitted work. The concurrent task
**Improve QL snapshot and map loading** was also editing loading/audio/archive
code. Its edits were preserved, and these builds include the shared working
tree rather than claiming an isolated committed source snapshot.

The existing Windows x86 release preset built the client, dedicated executable
and all three renderers under `.tmp/rtcw-manager/build`. The dedicated executable
with SHA256 `25a87625df1f9cef71f64adccd1c126f26bea9d78880c74e25f4ed7740e36a8c`
passed automatic and explicit Q3/QL startup: legitimate modules/maps, local
pure configuration, protocol 68/91 query responses, installation identity and
normal exit. Exact command lines, fixture identities and replies are in
`.tmp/rtcw-manager/runtime/manifest.json` and its adjacent logs.

Catalog/alias/activation, profile config/restart/recovery, malformed-pak and
Steam-discovery tests pass. A complete Windows x86 Meson run after the message
codec implementation passed **111 targets, with two skips and no failures**.
The earlier failed checks are resolved: the recovery harness uses the new config
API, the launch test accepts each preset's explicit profile, and the README's
canonical template contains the RTCW paragraph. The other task also corrected
its in-flight ZIP uppercase lookup failure. Server-binding verification also
passed 111 targets with two skips, recorded in `binding-full-suite.log`.
Fresh Q3/QL automatic and explicit map-startup probes for that build passed in
`.tmp/rtcw-manager/runtime-codec/manifest.json`, dedicated SHA256
`8d01d78b1781476c79d78b523a4da9e71e1cb5a317d634a25bf44dac4542479e`.
These runtime probes predate the canonical RTCW state extensions.
After those extensions, `state-full-suite.log` records another 111 passes and
two skips. `.tmp/rtcw-manager/runtime-state/manifest.json` records four fresh
Q3/QL map-startup passes for dedicated SHA256
`c9cded6d601c3b9b21f3855d45df47f441745d8112e41486f3bf1c0f7629e13b`.
The subsequent capacity build passed all 111 active targets across the full run
and corrected aggregate rerun, with the same two skips. The aggregate includes
1,306 Python tests (11 environment/optional skips); its earlier failures were
harness assumptions about the old limits. `capacity-sanitizers.log` records the
Linux x86 ASan/UBSan configstring, fragment and ABI tests. Four new Q3/QL
map-startup probes passed in `.tmp/rtcw-manager/runtime-capacity/manifest.json`,
dedicated SHA256 `42cbbdc5ef2cd57463d5566df3a896e7681e7da41961007e1f35278af400429a`.

After module search and cgame-transfer integration, `module-full-suite.log`
records **111 passes, two skips and no failures**. Four fresh Q3/QL automatic
and explicit map-startup probes passed in
`.tmp/rtcw-manager/runtime-modules/manifest.json`, dedicated SHA256
`c74adab039c8744926b67d1d496e4cca0ccfbebcfaea873fe0751066b7f08754`.
These runs validate the shared engine's existing profiles; they do not execute
RTCW's still-gated modules.

After server-flag translation and RTCW UI API-4 acceptance, the fresh
`ui-api-full-suite.log` records **111 passes, two skips and no failures**.
It resolves a stale, already-loaded UI test fixture from the preceding run.
Four Q3/QL map-startup probes after server-flag translation passed in
`.tmp/rtcw-manager/runtime-server-flags/manifest.json`, dedicated SHA256
`8fe489d529c2215ba8bea2675df0e6afa1decf9b2af9103d1133e95c53474b3b`.

After cvar/command-value integration, `cvar-full-suite.log` records another
**111 passes, two skips and no failures**. All six module-boundary fixtures
pass under Linux x86 ASan/UBSan in `cvar-sanitizers.log`. Four fresh Q3/QL
map-startup probes pass in `.tmp/rtcw-manager/runtime-cvars/manifest.json`,
dedicated SHA256 `ef8bf22b6faf94c95b49e7e5899dd3f7c4b3f4601dc96add8a49a98a14b1dd92`.

After the client POD/calendar adapters, `client-pods-full-suite.log` records
**111 passes, two skips and no failures**. All seven module-boundary fixtures
pass under Linux x86 ASan/UBSan in `client-pods-sanitizers.log`. Four fresh
Q3/QL map-startup probes pass in
`.tmp/rtcw-manager/runtime-client-pods/manifest.json`, dedicated SHA256
`6760c28f10b102afb3387b88abd6f9c88e808342931e8e925ff97beea968854c`.

After menu/key/export routing, `key-exports-full-suite.log` records
**111 passes, two skips and no failures**, including 1,313 Python regressions
(11 optional/environment skips). All eight module-boundary fixtures pass
Linux x86 ASan/UBSan in `key-exports-sanitizers.log`. The four fresh Q3/QL
startup probes pass in `.tmp/rtcw-manager/runtime-key-exports/manifest.json`,
dedicated SHA256 `3571678d8c16f48ba975b7aa18e46ebe5fefb0de39b884e0a08c9abb6bbf5f81`.

The subsequent parser-location adapter builds with the same Windows x86
client/dedicated/all-renderer preset (`parser-location-build.log`). Four focused
Windows x86 module fixtures pass in `parser-location-x86-tests.log`; all eight
Linux x86 module fixtures pass ASan/UBSan in `parser-location-sanitizers.log`.
The earlier full-suite/runtime results above predate this RTCW-only addition.

After complete menu translation and popup routing, `popup-full-suite.log`
records **111 passes, two skips and no failures**, including 1,313 Python
regressions (11 optional/environment skips). All eight Linux x86 module
fixtures pass ASan/UBSan in `popup-sanitizers.log`. Four Q3/QL automatic and
explicit startup probes pass in `.tmp/rtcw-manager/runtime-popups/manifest.json`,
dedicated SHA256 `e35a77f00a6ffa917c165a43261a0bb6e0d1ea6b5843ae344bee79a6b3419a9a`.

After same-executable mode transitions and the Windows UTF-8 filesystem
correction, `utf8-files-full-suite.log` records **111 passes, two skips and no
failures**, including 1,318 Python regressions (11 optional/environment skips).
All 12 Linux x86 module/process fixtures pass ASan/UBSan with leak detection in
`utf8-files-sanitizers.log`. Four Q3/QL automatic/explicit startup probes using
Unicode writable home paths pass in
`.tmp/rtcw-manager/runtime-unicode-fixed/manifest.json`, dedicated SHA256
`4b3e1472d565c81abe66c0b04702b288bfc5d72d0f9709d8dcbe72a563cc1114`.
These results predate the BSP profile-format change.

After BSP profile selection and renderer API validation, `bsp-profile-full-suite.log`
records **111 passes, two skips and no failures**, including 1,321 Python tests
(11 optional/environment skips). All 15 Linux x86 fixtures pass ASan/UBSan in
`bsp-profile-sanitizers.log`. The four Q3/QL automatic/explicit map-startup
probes pass with Unicode homes in
`.tmp/rtcw-manager/runtime-bsp-profiles/manifest.json`, dedicated SHA256
`85aa2b534b0cae6160fe7d2d161166606c68f7d4c95fc99ec4e9c480a5328e46`.

After collision handles, module trace translation and the reserved-hull guard,
`collision-full-suite.log` records **111 passes, two skips and no failures**.
The saved `collision-full-testlog.txt` includes **1,322 Python tests**, with
11 optional/environment skips. All 16 Linux x86 fixtures pass ASan/UBSan with
leak detection in `collision-sanitizers.log`. Both real-asset collision probes
pass on Windows x86/x64 and Linux x86 with sanitizers; the final manifests are
in `collision-retail-x86`, `collision-retail-x64` and `collision-retail-linux-x86`.
Four fresh Q3/QL automatic/explicit map-startup probes with Unicode homes pass
in `.tmp/rtcw-manager/runtime-collision/manifest.json`, dedicated SHA256
`ca3c2f18970c68351b45412ae582713a9195b7089eadbfc48df44c0e76c338ee`.
The source ABI probes now check 1,850 SP and 1,845 MP assertions against both
original and iortcw headers on Windows x86/x64 (`abi-collision-*.json`).

After freezing Q3/QL scene records and extending the internal renderer records,
the Windows x86 client, dedicated executable and all three renderer DLLs build.
The full run plus corrected aggregate rerun pass **111 targets with two skips**;
`render-records-aggregate-testlog.txt` records **1,324 Python tests**, with 11
optional/environment skips. The initial aggregate failure was a test extracting
extension handlers by a removed comment; it now selects the actual cases.
The sanitizer fixture also needed its deliberately invalid API number changed
from literal 16 to values relative to `REF_API_VERSION`. Its API-14 allocation
had incorrectly been presented as the current version. No renderer validation
change was needed. Across `render-records-sanitizers.log` and the corrected
`render-records-sanitizer-corrections.log`, all **19 distinct Linux x86 fixtures**
pass ASan/UBSan with leak detection.

The six windowed retail scene runs in `render-records-live-fixed/manifest.json`
pass with the same Windows x86 client, SHA256
`6b5bc78e1540915287cc40518e02eb04b0da92d1bc7eecd7395e7153969740e5`.
Each profile loads its real UI/cgame/qagame contracts (Q3 QVMs; QL APIs 8/8/10),
uses GLx, Vulkan or RTX raster as requested, produces a 640x480 engine PNG,
and exits normally. All six PNGs were inspected: Q3 world/weapon/items/HUD and
QL native join UI/world are readable. These are smoke observations, not image
parity: Q3 spawn positions differ and QL was not joined through input. RTX ran
in raster mode; no ray-tracing or unavailable GPU-debug-marker claim is made.
The private desktops were never switched, input and sound were disabled, and
no OS capture was used. Temporary homes and native caches were removed after
checking containment and links. Only logs, configs, manifests and engine PNGs
remain. The first smoke runner exceeded the engine's 32-startup-command limit;
moving remaining settings into a config and correcting the QL `pak00.pk3`
fixture identity resolved that harness failure.

Four Q3/QL automatic/explicit dedicated startup probes also pass with Unicode
homes in `runtime-render-records/manifest.json`, dedicated SHA256
`87df97b22d18c491fb53ccf2e5e29690f2d216aab419d004c2fd1bbff3297e23`.
These records validate Q3/QL non-regression; they do not execute the gated
RTCW scene imports or establish RTCW rendering.

After the basic RTCW renderer imports, the Windows x86 client, dedicated server
and three renderer DLLs build. `render-imports-full-suite.log` records **111
passes, two skips and no failures**; `render-imports-full-testlog.txt` includes
**1,325 Python tests**, with 11 optional/environment skips. The five affected
Linux x86 import/record/state/cvar fixtures pass ASan/UBSan with leak detection
in `render-imports-sanitizers.log`. The fixture drives actual SP/MP UI/cgame
dispatch prefixes against a synthetic renderer, including exact argument
allocations, unaligned records, bounds failures, null/reset contracts, scalar
order, staged output failures and unchanged Q3/QL dispatch. Updated source
reports `abi-render-imports-x86.json` and `abi-render-imports-x64.json` retain
1,886 SP and 1,876 MP passing assertions for both original and iortcw sources.
This checkpoint is offline import evidence; the six real Q3/QL render scenes
above predate it, and RTCW rendering remains gated.

After polygon submission and renderer capacity guards, Windows x86 client,
dedicated server and all three renderer DLLs build. The complete configured
suite again passes **111 targets, with two skips and no failures**
(`polygon-imports-full-suite.log`). The saved full test log contains **1,326
Python tests**, with 11 optional/environment skips. Four affected fixture
methods pass both Windows x64 and Linux x86 ASan/UBSan with leak detection
(`polygon-imports-focused-x64.log`, `polygon-imports-sanitizers.log`). They
exercise exact-length and unaligned vertex storage, float-bit/color transfers,
empty/oversized/overflowing arguments, allocator and backend failures, and the
actual queue functions in all three renderers. Queue checks cover capacity
truncation, cumulative submissions, fog selection, shader-zero preservation
and rejecting malformed submissions before memory access. This remains
offline RTCW evidence, not a live RTCW scene.

After gradient/rotation adapters and renderer API 17, the Windows x86 release
client, dedicated server and all three renderer DLLs build. The complete
configured suite passes **111 targets, with two skips and no failures** in
`draw-quad-full-suite.log`; `draw-quad-full-testlog.txt` records **1,327 Python
tests**, with 11 optional/environment skips. The eight affected Linux x86
import/record/queue/API fixture methods pass ASan/UBSan with leak detection in
`draw-quad-sanitizers.log`. Actual command producers and the existing 2D
backend paths are exercised for all renderers, including late color sampling,
all corner-color masks, producer ownership, shader changes, flush/capacity
handling, bloom/UI bookkeeping and unchanged legacy command allocation.

Six fresh Q3/QL scene runs pass in `draw-quad-live/manifest.json`, all using
the same Windows x86 client, SHA256
`01278db02badcc2982594f3086fbb56741e2cfaa05371d58eec83fb724d7aef3`.
The expected Q3 QVM and QL native APIs 8/8/10, requested GLx/Vulkan/RTX raster
renderer, windowed 640x480 mode, engine PNG and normal exit were verified for
every run. Each PNG was inspected: Q3 world/weapon/HUD and QL native join UI
over the world are readable. There is no image-parity or RTX ray-tracing claim.
Input and sound were disabled, private desktops were never switched, and no
OS capture was used. Temporary homes/native caches were removed after path
and link checks. This establishes Q3/QL non-regression; it does not exercise
the gated RTCW client on a GPU. Initial helper declaration ordering and a
probe-only architecture flag were corrected before these passing checks.

Useful local evidence is retained in `.tmp/rtcw-manager/`: build and full-suite
logs, runtime manifests, `abi-x86.json`, `abi-x64.json`, `message-*.json`,
`codec-binding-sanitizers.log`, `state-sanitizers.log`, and reference hashes.
Early foundation checks used offline fixtures and dedicated probes. The later
render-record checkpoint adds the six Q3/QL client scenes described above,
without input control or proprietary-file redistribution. RTCW runtime and all
full-compatibility gates stay open.

### MDC renderer validation checkpoint

The Windows x86 client/dedicated/all-renderer release build and complete suite
pass in `mdc-final-suite.log` and `mdc-final-testlog.txt`: **111 passing Meson
targets, two configured skips, 1,331 Python tests with 11 skips** (260.723 seconds
for the Python suite). The seven focused Windows x64 methods pass in
`mdc-focused-x64.log`; four MDC methods pass with Linux x86 ASan/UBSan and leak
detection in `mdc-sanitizers-fixed.log`. The fixtures exercise C/C++ byte readers,
normal encodings, malformed/truncated/unaligned files, ownership, all LOD-presence
combinations, tags, frame interpolation, dispatch/queue guards, RTX material and
geometry transport, and allocation/submission failures. The shared mesh view
also bounds negative MD3 skin indices to the first shader instead of indexing
before the shader array.

Three synthetic MDC engine scenes pass in `mdc-live-fixed/manifest.json`. A
528-byte independently generated quad has one base frame and one compressed
frame. Retail Q3's `testgun` command requests its `.md3` name, exercises the MDC
fallback and draws the two frames blended at 0.5. The engine PNGs visibly show
the cyan quad over the world/HUD in GLx, Vulkan and RTX raster mode. Each image
has more than 5,000 fixture-colored pixels. This establishes native MDC drawing
through the existing Q3 module interface; it is not a retail RTCW runtime or
ray-tracing claim. The original helper used incorrect `cg_gun_x/y/z` cvar names;
its results are explicitly marked failed in `mdc-live/manifest.json`. The passing
run uses `cg_gunX/Y/Z` and recognizes each renderer's fallback diagnostic.

Six additional retail Q3/QL scene probes pass in
`mdc-regression-live/manifest.json`. All nine passing runs use the same client
SHA256 `01278db02badcc2982594f3086fbb56741e2cfaa05371d58eec83fb724d7aef3`
and renderer API 17. The renderer DLL hashes are
`4a37b05e4e8e01beb5cef51480fb4a862e4b573feb08d21f0479abf6f40e5867` (GLx),
`5caae88a4bb98fe637e3c461f47da85dc232e2faef42aabdae9ef21ca1c6923b` (Vulkan), and
`3df544abe21afda92231e0117a30c97842753f349393ba98ee28671cadcd09b0` (RTX).
The expected Q3 QVM and QL native module APIs 8/8/10, requested renderer,
windowed 640x480 output and normal exit are verified. All nine engine PNGs were
inspected; the QL native join screen remains readable, and no match/input
interaction was attempted. All temporary writable homes/native caches were
removed after containment and link checks. Installed content remains unchanged.

Before the passing checkpoint, MSVC caught a shadowed local name in the new RTX
transport, UBSan caught an incompatible surface callback signature, and three
existing source assertions needed the new mesh-view expression. These were
corrected and the complete suite rerun. Successful MDC fallback also clears
stale pointers left by rejected format loaders, so tag dispatch cannot select
a rejected MD3 representation. The remaining RTCW startup and compatibility
gates are unchanged.

### Opt-in developer boot probes

Meson's `rtcw-development` option is false by default. Enabling it adds
`FNTECH3_RTCW_DEVELOPMENT=1` only to the client/dedicated engine compilations,
allowing the incomplete RTCW catalog entries to be activated for isolated
probes. The same `fntech3.exe` selects SP or MP before filesystem/module startup;
profile contracts and lifetime immutability are unchanged. It cannot be combined
with the Q3-only diagnostic target. The catalog retains its blocker descriptions
and startup prints an explicit experimental-build warning. Standard builds and
their profile regression tests keep RTCW gated.

`rtcw_development_profile_tests.py` compiles the actual manager with the default
gate, developer gate and Q3-only restriction. Separate processes exercise both
modes, immutable activation, explicit/automatic selection, ambiguous evidence,
forged descriptors and protocol/Steam identity. Windows x64 and Linux x86
ASan/UBSan with leak detection pass in `development-profile-x64.log` and
`development-profile-sanitizers.log`.

`scripts/probe_rtcw_boot.py` launches windowed, input-disabled UI or map probes
on unswitched private desktops, with isolated writable homes and the engine's
`screenshotPNG` command. It records executable/renderer hashes, launch commands,
diagnostics and installation metadata checks. It removes only contained,
link-checked temporary homes and never copies proprietary inputs into evidence.

The clean Windows x86 developer build (client, dedicated server, GLx/Vulkan/RTX)
passes in `development-build.log`. The first SP/MP UI runs in
`rtcw-ui-first/manifest.json` use the same client SHA256
`3212fe13efc740a76b9c0c4fd9c8ae595b7bbd77481aabead836e53eccf1a004` and renderer
API 21. Both fail before a capture: the observed shutdown diagnostics report
unimplemented `UI_LAN_SAVECACHEDSERVERS` (SP ordinal 82, MP ordinal 79), followed
by recursive client shutdown. The harness stops each process after 45 seconds.
This proves a native UI import was reached, not successful UI initialization.
Installation file sizes/timestamps are unchanged, both writable homes are removed,
and no user input or OS capture was used. Browser import/cache adapters and
initialization/shutdown recovery remain open.

The first default regression run overlapped the clean developer build and timed
out at the Python target's 300-second limit. It reported 110 passing targets,
two skips, no individual failures and one timeout. Its evidence is retained as
`development-gate-full-suite.log` and `development-gate-timeout-testlog.txt`.
A second run also reached that limit with no individual failure. A bounded
600-second rerun completed successfully: `development-gate-extended-suite.log`
and `development-gate-extended-testlog.txt` record **111 passing targets, two
configured skips, and 1,341 Python tests with 11 skips**, in 316.79 seconds.
The focused Windows x86 developer gate also passes in
`development-profile-x86.log`. The aggregate's default timeout is now 600
seconds to accommodate the expanded native fixture suite; individual fixture
timeouts remain unchanged.

### Indexed attachment validation checkpoint

The Windows x86 client, dedicated server and GLx/Vulkan/RTX renderer builds
pass with renderer API 18. `tags-full-suite.log` and `tags-full-testlog.txt`
record **111 passing targets, two configured skips, 1,332 Python tests with
11 skips**. Four affected fixture methods pass Linux x86 ASan/UBSan with leak
detection in `tags-sanitizers.log`; the indexed attachment fixture compiles
both C11 and C++17 against all three renderer model definitions and executes
their existing boolean tag functions as well. The standalone imports and
mesh-tag checks also pass Windows x64. Source and installed-asset comparisons
are saved as `abi-tags-{x86,x64}.json` and
`tags-reference-{x86,x64,linux-x86}.json` under `.tmp/rtcw-manager/`.

All six fresh Q3/QL scene probes pass in `tags-live/manifest.json`, using the
same x86 client SHA256
`d447513b25b6c2686b7e3ae6dae681852d64e27341506862a64e9a00653aa8bd`.
Every engine PNG was inspected: Q3 world/weapon/HUD and the native QL join UI
over the world remain readable. Logs verify the requested renderer, Q3 QVM
modules, QL native APIs 8/8/10, windowed 640x480 mode and normal exit. Input and
sound were disabled; private desktops were never switched and no OS capture
was used. Isolated homes/native caches were removed after containment and link
checks. This is Q3/QL non-regression evidence, not retail RTCW gameplay or RTX
ray-tracing evidence. RTCW startup and remaining compatibility gates stay open.

### MDS integration validation checkpoint

`mds-full-suite.log` and `mds-full-testlog.txt` record the Windows x86 client,
dedicated server and all three renderer builds, **111 passing targets, two
configured skips and 1,337 Python tests with 11 skips**. The Python aggregate
completed in 270.638 seconds. The build uses renderer API 19 and the same
client SHA256 as the MDS synthetic drawing probe above. `mds-integration-focused-x86.log`,
`mds-renderer-x64.log`, `mds-integration-sanitizers.log`,
`mds-ray-lod-sanitizers.log` and `mds-renderer-sanitizers-final.log` retain the
focused checks, including the initial test-only x87 assertion and its passing
correction. The final duplicate-tag/name regressions are included in the full
Windows suite and final Linux renderer fixture.

`mds-regression-live/manifest.json` records six fresh Q3/QL scenes. Every engine
PNG was inspected and each run verifies the requested renderer, windowed
640x480 mode, Q3 QVM modules or native QL APIs 8/8/10, and normal exit. Q3
world/weapon/HUD and the native QL join menu remain readable. RTX uses raster
mode and its existing visual differences are not treated as image parity.
The private desktops were not switched, no input was supplied and the isolated
homes/native caches were removed after containment/link checks. No new Q3/QL
runtime regression was observed; the broader release and RTCW gates remain open.

### Skin parsing and model lookup

The three renderer implementations now share the inherited Q3 skin loader in
`tr_skin.h`. The selected immutable profile enables RTCW's `md3_` model keys
and uniform `playerscale` metadata; Q3/QL retain their surface interpretation.
`CG_R_GETSKINMODEL` uses a staged, bounded 64-byte output, preserving the
destination on a miss, invalid input or backend failure. Lookups are case
insensitive and the first duplicate key wins. Renderer API 20 adds this internal
export without changing a game-module structure or ordinal.

Missing RTCW files remain retryable. Surface/model entries have independent
256-entry bounds, nonfinite scales are rejected, and failed registrations use
the default skin. The tag-only loper upper-body exception remains valid when
requested repeatedly; the original cache returned zero on a second request.
An inherited shared-loader bug was also fixed: shader names shorter than five
characters no longer make the `.skin` extension check read before the string.

`scripts/probe_rtcw_skins.py` compares the unchanged original SP/MP and iortcw
loaders and lookup functions with the shared implementation. All **412** installed
skin entries match each reference: **1,718 surfaces, 586 model references and
2,408 lookup queries** per reference, all accepted. The corpus contains at most
three model references and nine surfaces per entry, fitting the unchanged stock
arrays. A preliminary line-based inventory had counted comments as metadata;
its approximate six-entry result is superseded by the parser probe. The
five-model original limit is unchecked, whereas iortcw stops parsing there;
fnTech3's larger bounded storage is covered separately with synthetic inputs.

`skin-reference-{x86,x64,linux-x86}.json` records exact comparisons on Windows
x86/x64 and Linux x86 ASan/UBSan with leak detection. Reference code and assets
are used only in memory/disposable fixtures; retained reports contain hashes,
counts and diagnostics. `skin-focused-{x86,x64}.log` and `skin-sanitizers.log`
cover each renderer's actual skin records/parser and both module import paths,
including cache behavior, comments/quotes, invalid handles, missing files,
duplicate/long keys, bounded overflow and aliased query/output buffers.

Six fresh Q3/QL runs in `skin-live/manifest.json` use renderer API 20 and client
SHA256 `e0ff82576a7b0b5e17c82642050a39bfb77d3ffddd32e1aa6cc76a6eb0df657f`.
Every engine PNG was inspected and logs confirm the requested renderer, Q3 QVM
or QL native module APIs, windowed 640x480 output and normal exit. The Q3
world/weapon/HUD and native QL join menu remain readable. Input and sound were
disabled, private desktops were not switched, and isolated homes/native caches
were removed after containment/link checks. Evidence was relocated from the
initial project-local `skin-live` folder into `.tmp/rtcw-manager/skin-live`;
the manifest retains that relocation and the original launch commands. This
does not establish retail RTCW rendering or gameplay; the remaining gates stay
open.

`skin-full-suite.log` and `skin-full-testlog.txt` record the matching Windows x86
client, dedicated server and all three renderer builds: **111 passing targets,
two configured skips and 1,338 Python tests with 11 skips**. The aggregate
completed in 272.23 seconds. No Q3/QL regression was found at this checkpoint.

### Brush-surface shader lookup

`CG_R_GETMODELSHADER` now reaches a common brush-surface lookup in all three
renderers. Both original SP and MP use three integer arguments. Negative and
out-of-range surface indices select the first surface; all four source engines
ignore `withLightmap`, despite their interface comment. Materials with a
nonnegative lightmap index request a diffuse-lit `LIGHTMAP_NONE` version. Image
matching is case sensitive, preserves an existing image's mipmap setting, and
defaults to mipmaps when no image matches. Other lightmap encodings return the
surface's existing shader handle. Invalid model handles, non-brush models,
empty brushes and missing surface shaders safely return zero.

Renderer API 21 adds the internal export. Module arguments are checked for
32-bit representation before dispatch. The engine uses a distinct cached
diffuse variant, leaving ordinary materials and font/image registration intact.
Its first color generator and normal requirement are set after white/lightmap
simplification and before multitexture and GPU pipeline selection. This avoids
the reference implementation's mutation of an already compiled shared shader.
The variant distinction is an implementation choice: reference shader-cache
side effects, remap combinations, GPU material parity and retail debris
rendering still need runtime evidence.

`scripts/probe_rtcw_model_shaders.py` compiles each unchanged reference lookup
function against synthetic image/model/shader services in a disposable fixture.
Each source matches **1,925** selection/lookup cases, including **875** diffuse
requests, on Windows x86/x64 and Linux x86 ASan/UBSan with leak detection.
`model-shader-reference-{x86,x64,linux-x86}.json` retains source/implementation
hashes and results. The C++ fixture supplies a scoped enum-conversion operator
for iortcw's C bitwise assignment; its 0/1 mipmap values are unchanged.

`model_shader_tests.py` executes each renderer's actual model/brush records,
model-handle validation, shared lookup, shader-cache functions and compiler
adjustment. It covers default/missing shaders, bounds, deliberate hash collisions,
case matching, mipmaps, backend failures, cache isolation and empty stages.
`rtcw_render_import_tests.py` exercises both cgame dispatch paths, exact argument
counts, integer limits and rejection outside RTCW. These pass Windows x86/x64
and Linux x86 sanitizers in `model-shader-focused-{x86,x64}.log` and
`model-shader-sanitizers.log`. Shader parsing and GPU pipeline creation are stubbed
in the cache fixture; it does not establish live shader parity.

Six fresh Q3/QL scene probes pass in `model-shader-live/manifest.json` using
renderer API 21 and the same client SHA256
`2463c824951840d7d6076cb352da9ddabb19ca47c26c2aa4800f0cac69ffbafd`.
Every engine PNG was inspected. Requested renderer, Q3 QVM or QL native APIs,
windowed 640x480 output and normal exit are verified. World/weapon/HUD and
native QL join UI remain readable. RTX uses raster mode; no image-parity claim
is made. Isolated writable homes/native caches were removed after containment
and link checks, private desktops were not switched, and no input or OS capture
was used. These are non-regression probes, not retail RTCW gameplay evidence.

`model-shader-full-suite.log` and `model-shader-full-testlog.txt` record the
matching Windows x86 client/dedicated/three-renderer build and **111 passing
targets, two configured skips, and 1,340 Python tests with 11 skips**. The Python
aggregate target completed in 238.34 seconds. The remaining RTCW compatibility
gates are unchanged.

### Native UI cache and module-context corrections

The manager catalog now owns the private server-cache filename. Q3 and QL
retain `servercache.dat`; RTCW MP uses `fntech3-rtcw-mp-servercache.dat` under
the selected writable game home. RTCW SP performs no cache I/O and does not
reset the lists: both original SP and iortcw SP intentionally leave MP's cache
and favorites alone. The private MP file contains fnTech3 records, not either
reference engine's binary cache layout. Importing a pre-existing WolfMP or
iortcw favorites cache remains separate work.

The inherited shared cache loader previously trusted disk counts and ignored
short reads. It now checks exact length, both capacities, payload size, string
termination and enum representations, then publishes staged arrays only after
every read succeeds. Invalid files leave empty counts without publishing partial
records. Saves reject invalid records/counts and report short writes; a global
query's `-1` pending sentinel saves as an empty list. The Q3/QL byte layout and
filename are unchanged. `server_cache_tests.py` executes the actual functions
and manager against controlled I/O, covering all profiles, truncation, oversized
counts, malformed active/unused records, partial I/O, exceptions and closure.
`cache-focused-{x86,x64}.log` and `cache-sanitizers.log` pass, including Linux
x86 ASan/UBSan with leak detection.

The first cache-enabled boots in `rtcw-ui-cache/manifest.json` exposed an
earlier renderer-pointer error that shutdown had hidden. External inline
renderer helpers referenced translation-unit-local `VM_ArgPtr` functions. The
linker could select the cgame resolver for a UI import when cgame was absent.
`VM_ExplicitArgPtr` now resolves through the supplied VM, and every shared
renderer buffer/name conversion uses it. `vm_explicit_pointer_tests.py` links
separate UI/cgame callers with conflicting implicit contexts and exercises
QVM records, native records, null contexts and both native ABI styles. The
linked test, full render-record fixture and RTCW renderer import fixture pass
Windows x64 and Linux x86 sanitizers in `vm-pointer-focused-x64.log` and
`vm-pointer-sanitizers.log`.

`rtcw-ui-pointers/manifest.json` confirms that both native UIs proceed to menu
parsing, then encounter sound registration. Both original RTCW UIs pass only
one sound-name argument and force the compression hint to false. That import,
local sound submission and background-track stop are now routed accordingly;
SP's distinct music/fade calls are not mapped to Q3's shorter signature.
Minimal argument arrays, integer boundaries and backend errors are exercised
in `rtcw_module_cvar_tests.py`; `ui-boot-focused-x64.log` passes six methods and
`ui-audio-sanitizers.log` passes the updated import fixture.

The latest boots, `rtcw-ui-audio/manifest.json`, use the same developer client
SHA256 `a6707b2822c4635cebc7a324937f85fdd1fcc8116fcb816c7ee0b3d0535467ba`
for SP and MP, with renderer API 21. SP reaches startup configuration/main-menu
setup and stops at `UI_S_STARTBACKGROUNDTRACK` (73); MP stops at
`UI_CL_TRANSLATE_STRING` (97). Neither produces a screenshot or establishes
successful UI operation. All installations remain unchanged by size/timestamp
inventory; isolated homes are removed after containment/link checks. The probe
now uses append/flush logging and stops its owned process when the engine log
reports an error, preserving earlier diagnostics without user input or OS capture.

Source inspection for the next slice confirms that original SP's third music
argument includes fades and negative queue commands, whereas iortcw's current
SP sound interface drops that argument. Original MP translation uses four
language columns, case-insensitive English keys, optional offsets and a
32,000-byte UI/cgame output buffer. These are source observations only; music,
translation, remaining browser imports and RTCW material/parser behavior stay
open pending implementation and runtime evidence.

`ui-boot-full-suite.log` and `ui-boot-full-testlog.txt` record the matching
Windows x86 client, dedicated server and three-renderer builds: **111 passing
targets, two configured skips, and 1,343 Python tests with 11 skips**. The
aggregate completes in 351.42 seconds under the updated bounded timeout.
`ui-boot-focused-x86.log` passes the six cache, pointer, renderer-record and
module import checks against the x86 toolchain.

Six fresh Q3/QL runs in `ui-boot-live/manifest.json` pass with renderer API 21
and client SHA256
`9e7159bd3e47ccdc9105065f0372c320277e0bf0035918e64562349bb9f447d9`.
All engine PNGs were inspected: Q3 world/weapon/HUD and the native QL join menu
remain readable. Logs verify each requested renderer, Q3 QVM or QL native
module APIs, windowed 640x480 mode and normal exit. RTX uses raster mode and
its existing visual differences are not treated as image parity. Isolated
homes/native caches were removed after containment/link checks. No input or
OS capture was used. These are Q3/QL non-regression results; RTCW UI operation,
audio, gameplay and the remaining release gates are still open.

### MP localization and startup service contracts

The independently written `rtcw_translation.hpp` catalog now backs original
MP UI import 97 and cgame import 119. It loads `scripts/translation.cfg`, then
`translations/*.cfg` in filesystem order. Four target languages preserve the
original bytes, case-insensitive English keys, literal backslashes, newline
lookup/fallback rules and debug brackets. Duplicate text replaces all language
columns while retaining prior offsets unless a new offset is supplied. Invalid
language values fall back to English, including negative values that the
reference implementation does not bound safely.

Files are staged atomically, capped at 4 MiB, with at most 16,384 keys and
1,023-byte tokens. Missing, incomplete, malformed or oversized files do not
publish partial entries. Formatting checks retain the reference's percent
sequence rule and also check complete conversions, rejecting `%n` and
positional argument substitutions. Module output is staged within the exact
32,000-byte native contract, permitting input/output aliasing without partial
writes. These bounds deliberately harden the reference's unchecked token,
format scratch-buffer and partial-file behavior. Optional authoring operations
(missing-string logging, automatic dictionary saving and version editing) are
not implemented; debug brackets are supported and no dictionaries are written.

`scripts/probe_rtcw_translation.py` compiles unchanged original MP and iortcw MP
parser/lookup/translation functions as C in disposable fixtures with their
GPLv3 notices. Installed assets stay in memory. The eight installed dictionaries
contain **1,201 records**. Individual-file, forward/reverse overlay and synthetic
cases pass **11,347 entry comparisons and 818,856 translation comparisons per
reference**, with exact text and offset agreement. The reports
`translation-reference-x86.json`, `translation-reference-x64.json` and
`translation-reference-sanitized.json` record source/implementation/asset hashes
and counts for Windows x86/x64 and Linux x86 ASan/UBSan/leak checks. Separate
malformed-input tests exercise only fnTech3, avoiding the original parser's
unsafe cases. Four focused tests pass on those three targets in
`translation-final-x86.log`, `translation-final-x64.log` and
`translation-final-sanitizers.log`, including actual loader I/O, ordered reload,
short reads, failures, handle/list ownership and module dispatch.

MP's original two-name background-track import now uses the common sound
service. SP's three-argument fade/queue contract remains separate and open.
MP's void legacy update-check/install imports explicitly report unavailable
service: they perform no network/update installation and do not synthesize
an available update, successful check or an up-to-date result for WolfMP.
The exact minimal argument arrays and these notices are tested alongside
the existing cvar/UI imports.

The latest developer MP boot in `l10n-ui/manifest.json`, with French selected,
uses client SHA256
`9b9aa63a2d393d4db550f3efd046fe82621eaab6c4ce32c20309c38e3c01d117` and API 21.
It passes translation, MP music and the updater, then stops at
`UI_R_ADDLIGHTTOSCENE` (26). `l10n-mp` and `l10n-music` retain the preceding
music/update failures. No RTCW screenshot or localized-rendering success is
claimed. The probe now binds both IPv4 and IPv6 to loopback with ephemeral
ports; its command and log verify `[::1]:0`. It uses an unswitched private
desktop, windowed rendering, no input control and isolated writable homes,
which are removed; the installation inventory remains unchanged.

The final localization checkpoint builds the Windows x86 client, dedicated
server and GLX/Vulkan/RTX renderers. `translation-full-suite.log` and
`translation-full-testlog.txt` record **111 passing targets, two configured
skips, and 1,345 Python tests with 11 skips**, with no failures. The aggregate
completes in 399.32 seconds. Six fresh Q3/QL runs in
`translation-live/manifest.json` pass using client SHA256
`93e038ff06a76ec01a1c8605deeed1e06a8e19fd97fa1fa1b1197eac311d9d4f` and API 21.
All six engine PNGs were inspected: the Q3 world/weapon/HUD and native QL join
menu remain readable in every renderer. Logs verify the requested backend,
Q3 QVM or QL native modules, 640x480 windowed output and normal exit. RTX is
in raster mode; existing visual differences do not establish image parity.
These private-desktop probes use no input or OS capture and remove their
isolated homes after containment/link checks. RTCW rendering, audio/gameplay
and the remaining compatibility/release gates remain open.

### RTCW lights and the first native MP menu captures

Renderer API 22 adds an explicit six-argument RTCW light boundary, shared by
the SP/MP UI and cgame adapters. GLX, Vulkan and RTX select the original
projected-light path for RTCW while retaining Q3/QL's existing lighting-mode
selection. Submission preserves the original 32-light limit, force/junior
flag handling, SP radius scaling, MP unscaled radius, repeated overdraw and
negative/flame material selection. Special materials have separate cache
entries and keep their texture/blend passes. Original SP's vertex-light
collapse remains available; original MP's light materials skip that collapse.
Modern map/test light injection does not add unintended RTCW lights.

The shared projection code preserves XY coordinates, height attenuation,
triangle clipping, authored texture/blend selection and depth-equal repeated
passes. Model lighting uses RTCW's highlight/minimum-light and snooper rules,
excluding special-material lights from diffuse illumination. Q3/QL's existing
minimum-light behavior remains intact. Non-finite parameters, unusable radii,
invalid indices and excessive overdraw cannot reach unbounded work or unsafe
numeric conversions. Ordinary overdraw is capped at 64; stock effects use
zero, one or two. Forced-light selection deliberately avoids the references'
read of a stale destination slot when `r_dynamiclight` is 2.

Tracing the legitimate MP UI found a deliberate zero-sized `modelselection`
item in its installed main menu. The original UI divides by the resulting
zero field of view and submits infinite light positions before rendering an
empty viewport. fnTech3 culls these positions without publishing them. Its
existing empty-view guard then discards the scene. The renderer configuration,
parser token ABI and numeric values were correct; no asset or module patch is
needed. `lighting-installed-contracts.json` records hashes and geometry/pass
observations without retaining installed source contents. It also records the
one-pass negative material and three-pass flame material.

The source comparison probe, `scripts/probe_rtcw_lights.py`, compiles unchanged
GPLv3 reference functions only in disposable licensed fixtures. Per reference,
it checks **36,000 submission cases**, **20,000 projected scenes** containing
**45,496 draw calls / 545,952 vertex comparisons**, and **7,200 model-lighting
cases per renderer**. GPU services and lightgrid sampling are controlled;
model comparisons cover ambient/direct vectors and direction, excluding packed
byte shading. Projection compares texture coordinates, colors, triangle
selection, blend state, pass counts and counters. It does not establish
original-engine framebuffer parity or projected world/fog lighting.

Original Win32's x87 color conversion follows the floating-point rounding mode;
the portable original path truncates. fnTech3 preserves this platform
distinction. The original conversion function passes **262,148 values across
four rounding modes per SP/MP reference** on Windows x86. All source projection
fixtures use the original platform's conversion; iortcw's callback is controlled
accordingly. iortcw's own newer Win32 SSE conversion truncates. Its optional
greyscale and backface filters use their defaults (0 and 1 respectively) in
these comparisons.

The first successful native MP menu runs are retained in
`light-empty/manifest.json`, `light-vk/manifest.json` and
`light-rtx/manifest.json`. All three produce inspected, windowed 640x480 engine
PNGs with readable French menu text, backgrounds and animated models, then
exit normally. They use the same experimental `fntech3.exe`, isolated writable
homes and an unswitched private desktop; no input or OS capture is used and
the installation inventory is unchanged. These captures precede the final
color-rounding/material validation. SP music, remaining renderer/material/fog
contracts, projected world-light GPU parity, gameplay and live interop remain
open. The default RTCW activation gate is unchanged.

The final lighting checkpoint passes eight focused methods on Windows x64
(12.457 seconds), Windows x86 (14.297 seconds), and Linux x86 with
ASan/UBSan/leak detection (81.399 seconds). Source reports
`lighting-reference-{x64,x86,sanitizers}.json` pass all 22 rows per target.
Linux projected-coordinate fixtures use SSE2 binary32 arithmetic to avoid x87
excess precision; the diagnostic one-ULP difference remains recorded in
`lighting-float-diagnostic.log`. Legacy renderer recipes disable fast-math for
the compatibility-sensitive scene/shade translation units.

Fresh, inspected MP captures in `light-final-{glx,vk,rtx}/manifest.json` include
the final material/color changes. Six additional Q3/QL captures in
`lighting-live/manifest.json` preserve readable world/weapon/HUD or native join
UI across all renderers. These are menu/scene non-regression observations, not
original projected-light framebuffer parity. The first complete suite found
six stale source assertions for API 21/export lists and the old collapse
predicate; `lighting-full-failed.log` retains that result. The assertions were
updated for API 22 and the explicit RTCW material predicate, and 75 targeted
source checks passed. The later [music/menu checkpoint](2026-09-15-rtcw-music-menus.md)
records the subsequent SP music implementation and both-mode menu captures.
