# Jedi Academy and Jedi Outcast compatibility audit, 2026-10-07

Later combined-tree validation is recorded in the
[October 9 integration follow-up](2026-10-09-compatibility-integration.md).
The build and runtime identities below remain those of this original audit.

This audit covers `jka-sp`, `jka-mp`, `jk2-sp` and `jk2-mp`. Both titles
remain experimental. The changes below repair implemented boundaries and
advance developer probes; they do not complete either campaign, either
client, or the bidirectional retail interoperability gates. The
[JSON companion](2026-10-07-jedi-compatibility-audit.json) records validation
and executable identities.

## Academy's shared skeletal clock

**Observed in retail.** `jamp.exe` 1.01 has one pair of Ghoul2 clocks for
its game, client game and UI, at `0x10bcb74` and `0x10bcb78`. Its setter
at `0x4de0d0` clears the client clock when it exceeds the server clock plus
200, using signed comparison after wrapping addition. Bolt queries use the
nonzero client clock in preference to the server clock. The client frame
sets the server clock only when no local server exists, then sets the
client clock. Module teardown does not reset the pair. These facts were
measured in the [server clock record](2026-10-07-jka-server-pose-clock.md);
this audit re-read the setter in the same retail executable.

The no-rotation selector at `0x10bcb6c` is also common to the game and
client game. A failed no-rotation query leaves it for the next successful
query, which clears it. Its lifetime also crosses module teardown.

**Correction.** The three adapters now share engine-owned clocks and that
selector. Their model stores remain separately owned. Cgame teardown
revokes frame readiness without clearing retail's global state. Local
cgame bolt, bone-frame and scene queries can therefore use a published
clock instead of refusing every listen server. Dedicated-server clock
publication and other profiles are unchanged.

**Unit evidence.** Four linked translation units exercise the production
server, cgame and UI boundaries, including their cleanup, foreign-profile
isolation, invalid selectors, signed wrap and the inclusive 200 ms limit.
The server pose tests compare a listen client's selected time with the
actual posed result. Cgame bolt and scene fixtures now exercise local
publication as well as remote publication.

**Live evidence.** The old executable's local client entered `mp/ffa1`
then brought its server down at the explicit local-clock refusal. The
candidate held the same local session for 50 seconds and answered `status`
with ProbeHost on loopback. An engine `screenshot` captured the player
configuration menu. `map_restart 0` then exposed a separate pose-boundary
refusal, now repaired as described below. This is admission and menu evidence,
not played combat or complete listen-server compatibility.

**Angle timing correction.** That restart supplied PREMULT/POSTMULT angles
with a nonzero blend duration. Retail's dispatcher at `0x497c34` reads the
duration only for REPLACE (flag 4); the exact flag-1 and flag-2 paths at
`0x4980ac` and `0x497d94` ignore it. Academy MP's existing opt-in pose stores
now accept that metadata for those two modes. Nonzero REPLACE blending and
combined flags retain their explicit refusals. An independently authored
native harness executed the retail PREMULT/POSTMULT paths in memory with
synthetic matrices: all 2,160 combinations of time, start, duration,
root/child and render/normal matrix selection matched zero-duration output.
No retail program was launched by that harness. Synthetic production pose
tests cover the same allowance and unchanged refusal paths.

The corrected local client answers `status` after `map_restart 0`, with both
legacy and OpenAL audio enabled in separate probes. A console `team free`
command reaches match admission, then honestly refuses the unsupported
`Line` primitive in `effects/mp/spawn.efx`. This is the current observed
play frontier; no keyboard or mouse input was used.

## Academy positional sounds and looping policy

**Observed in retail.** The import table at `0x4142bc` routes cgame 37
through `0x412910` to the sound core at `0x46ac60`. It takes a nullable
origin, entity number, channel and sound handle. The local level follows
the last audio listener (`0x46ae6f`), not a newer snapshot's player number.
Imports 40 and 42 reach the same ordinary looping core at `0x46b180`.

**Correction.** Import 37 now copies and validates its origin and sends
the sound through Fn's spatial path, with Raven's channel translation and
the existing measured listener-volume rule. Import 42 uses Fn's ordinary
looping policy, including Doppler, where it previously selected persistent
looping. This does not claim that old loops survived frame clearing:
Academy's clear already cleared both kinds.

**Unit evidence.** Production adapter/audio tests cover all fourteen
channels, listener changes, nullable and unaligned origins, malformed
vectors and scalars, entity bounds, absent backends and foreign profiles.
Ordinary and fast-math builds run; the wider host build also checks scalar
truncation guards. Actual legacy loop bodies verify the Doppler distinction.
Audible playback is not established by these tests.

## Academy voice animation metadata

**Observed in retail.** Import 35 reads a 1,024-entry entity table at
`0xc4d228`. Voice starts write -1; idle entities read 0; measured speech
produces -1 or 1 through 4. Retail legacy mixing (`0x46bb00`), PCM envelope
construction (`0x472b80`), MPEG envelope construction (`0x46d560`) and
OpenAL updates (`0x46d620`) supply distinct timing paths. The default
thresholds are 0.5, 4, 7 and 8, with strict comparisons against waveform
peak. Even the counterintuitive all-silent, zero-peak result of 4 is
preserved. Synthetic waveforms executed against the isolated retail PCM
and MPEG routines establish threshold equality and partial-bin behavior.

The MPEG subtype requires the `sound/chars/` prefix and the measured
compressed-versus-decoded size rule, including `s_mp3overhead` (31,756).
An MP3 extension alone is insufficient. Its optional trailing ID3
`#MAXVOL` field supplies peak metadata; ordinary PCM instead measures its
resampled waveform. Analysis resampling preserves the observed binary32
intermediates and fixed-point cursor. These are contract observations,
not incorporated implementation text.

**Correction.** Both Fn backends now populate real entity values from
active speech. OpenAL retains a bounded-by-decoded-length envelope and
checks actual source state; legacy uses its mixer sample clock. Stop,
reset and shutdown clear the table. Invalid values and unsupported
multichannel speech remain explicit failures. No RTCW or Elite Force
return scale is substituted. Playback retains Fn's spatial and output
systems; this metadata controls the module's facial animation only.

**Evidence limits.** The integrated client now runs and restarts with each
backend enabled, at zero output gain. Legacy reports 22,050 Hz. The system
OpenAL compatibility runtime reports 44,100 Hz and lacks OpenAL Soft
extensions, so this run does not establish HRTF or the complete spatial
filter set. Active voice animation, audible quality, decoder starvation
and differences between decoded MP3 waveforms still need live evidence.

## Outcast localization

**Observed in retail.** UI 200 registers a StripEd package with owner 4
(`0x410636`); cgame 272 registers with owner 1 (`0x402ebe`). The path is
`strip/<name>.sp`. UI 201 always returns true after copying text; cgame 271
returns false and `??` for missing or empty text (`0x410656`, `0x402e5b`).
The bounded, uppercase reference lookup is at `0x438090`. The installed
files and pinned OpenJK `code/qcommon/strip.cpp` provide additional format
observations, distinguished from retail dispatcher facts.

**Correction.** An independently written bounded reader and host adapter
serve these imports, preserving each owner's lifetime across UI/cgame
shutdown and retaining language-specific strings. It refuses malformed
packages, conflicting registrations, excessive memory and the Asian font
lanes the existing font adapter cannot serve.

**Unit and asset evidence.** All 30 installed packages parse in memory,
with 2,039 entries and no extracted retail text retained. Tests cover
malformed input, transactional publication, language fallback, bounded VM
copies, the distinct missing-text results, shared ownership and reload.
The integrated client passes its old UI-200 frontier. The next observed
key-binding refusal at UI 34 was also repaired.

**Key services and cleanup.** Retail UI 33/34 (`0x40ffc5`, `0x40ffe7`)
use Q3's key numbers for names and bindings; setters, down-state,
overstrike, clear-state and catcher imports use that same boundary.
Outcast now uses the existing bounded Q3 translators, including outgoing
key events. These tests exercise synthetic key data, not user input.
A separate initialized flag prevents error recovery from calling into a
partly initialized menu. Package ownership and module files are released
on both successful and failed shutdown.

**Definition files and music services.** The next live stop was import 68.
Retail dispatches 63/68/69 to add, load and clear botlib global definitions
(`0x410460`, `0x410502`, `0x410520`). Outcast now uses the already bounded,
transactional Fn parser with copied inputs and checked VM offsets. Its
installed `ui/jk2mp/menudef.h` parses in memory. Tests also cover malformed
files, callback availability, source lifetimes and clearing definitions.
Record-free music imports 70/71 use Fn's stop/start services, matching
`0x410530` and `0x41053a`. This is dispatcher evidence, not audible music
validation. Stock Outcast's menu uses `vm/ui.qvm`; the adapter supports
that boundary as well as legacy native modules.

Imports 66/67/72 now marshal token, diagnostic and time records instead
of exposing engine structures. Retail `0x4b5a60` establishes the token's
four scalars at offsets 0/4/8/12 and 1,024-byte text at offset 16. Two
diagnostic procedures in the shipped `ui.qvm` (instructions 38438 and
38486) reserve a 128-byte filename buffer and call import 67 at 38470 and
38518. Retail time at `0x428760` writes nine 32-bit fields and accepts a
null destination. The new PODs freeze those 1,040-byte and 36-byte records;
tests exercise native and QVM destinations, bounds, unaligned writes,
callback failures and preservation of outputs on failure.
Failed token reads deliberately leave caller storage unchanged instead of
publishing parser scratch; this is a hardened failure boundary, not a claim
of byte-for-byte scratch behavior at retail EOF.

Retail's lexer also treats unquoted `@` references as names (entry
`0x4d40f5`, type write `0x4d39ab`). Outcast's UI source loader selects the
existing Raven opt-in lexer used by Academy. Ordinary Q3 parsing keeps its
existing behavior. All 35 shipped multiplayer menu files parse in memory
through both native and QVM adapters (70 passes and 2,694 observed `@`
names); included names and macro replacements are also tested. The
integrated client now passes these parser imports
and stops at UI 18, renderer model registration; a working menu/client is
not established.

**Error ownership.** Explicit registration failures return through the
owning parse scope before raising the engine's longjmp error. Allocation
counting with real longjmp exercises malformed input, conflicting packages,
capacity and memory limits. The audio loader similarly defers invalid
threshold failures until decoded PCM owners have unwound.

The review found the same pre-existing error pattern in Academy's StringEd
discovery, language selection and table loading. Those paths now unwind
owned containers before reporting errors. Discovery publishes no partial
state when English packages are missing, and both games guard string
reference arithmetic before checking VM bounds. Real longjmp regressions
verify allocations, rollback and subsequent valid requests.

## Single-player cvars

**Observed in retail.** JK2 1.02a's save writer tests flag **2** at
`0x438556` and `0x438585`, before writing `CVCN`, `CVAR` and `VALU`
chunks. Its cvars occupy 40 bytes, with `next` at offset 36
(`0x4163c5`, `0x416416`). `g_gravity` registers flags `0x42` in the
retail game library. Academy's separate SP library registers `0x140`:
its save flag is 256. Modern OpenJK's SP flag numbers must not replace
Outcast 1.02a's observed contract.

**Correction.** Outcast's save flag is retained in an owned external
record rather than becoming Fn's network `CVAR_USERINFO`. Its external
linked list contains only external records; offset 36 no longer exposes
the internal validator. Stable identity and current values, flags, latch
and modification metadata are synchronized at the module boundary.
Cleanup releases the mirrors. The regression fails against the old code.

Both SP adapters also refreshed records with `Cvar_Get`, which consumes
pending restart-latched values, and restored `modified` after the module
acknowledged it. A read-only lookup now preserves the latch; modification
state is republished only when the engine's modification count changes.
Missing engine records refuse explicitly. Tests emulate the mutating
registration lookup so these regressions cannot be hidden by the fixture.

This does not implement saving: JK2 SP still stops at cgame import 3 before
integrated Init, and its world/frame/save callbacks remain unserved.

## Remaining gates and presentation

Academy SP still refuses client admission and lacks integrated campaign,
player-state and savegame support. Outcast SP lacks its client bridge,
world/frame binding and saves. Multiplayer still requires complete
rendering, effects, audio/input, demos/downloads and both directions of
retail interoperability. The audit does not turn an unsupported callback
into a fabricated successful result.

Presentation follows invariant 11: skeletal poses use Fn's geometry and
renderer path, and sounds retain Fn's spatial and output paths, with
occlusion, EFX and audio zones available where the selected backend and
runtime support them. The probe validates only the capabilities recorded
above. Outcast text uses the existing Fn font
adapter. No retail renderer or sound backend is substituted, no default
feature is disabled and no retail-exact option is introduced. Input has
not been driven. Every runtime probe is windowed on an unswitched private
desktop, with isolated writable homes and unchanged retail installations.

Implementation text is independently authored. Retail disassembly, assets
and the already pinned OpenJK tree are reference observations only.

## Validation scope

The Windows x86 development client and dedicated server build with the
existing strict MSVC preset. The broad registered Meson run passes 261
tests, with zero failures and two skips: OpenAL loopback requires the
unavailable `ALC_SOFT_loopback` extension, and the optional Quake Live
factory probe did not discover an installation. The separately registered
application-icon test also skips its symlink cases because this host does
not permit creating symlinks. The
one-hour recursive Python source suite was not run; this is not a complete
automated-suite or clean profile-matrix claim. Final localized corrections
are rerun against their affected tests as recorded in the JSON companion.
Documentation generation and player-manual tests pass (20 tests and 831
subtests). Task-local logs, engine captures and independently authored
retail-oracle sources remain under `.tmp/jedi-audit/`; temporary executable
copies and writable test homes are removed.
