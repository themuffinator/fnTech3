# Jedi compatibility audit, second round — October 7, 2026

This follows the [first audit](2026-10-07-jedi-compatibility-audit.md).
Later combined-tree validation is in the
[October 9 integration follow-up](2026-10-09-compatibility-integration.md);
the identities and counts below remain those of this original round.
It advances bounded multiplayer rendering paths and corrects Academy's
single-player cvar metadata. Both titles remain experimental; no complete
compatibility gate closes. The [machine-readable companion](2026-10-07-jedi-compatibility-round2.json)
records executable identities, commands, installation identities and results.

Implementation is independently authored. Retail binaries and installed
assets are observation sources; no proprietary code or assets are incorporated.
OpenJK's pinned source supplies additional format observations, not a retail
runtime equivalence claim. The working tree is based on
`6a93b622e78e37949e89625484c56997f34fa8a5`, including the first audit's changes.

## Academy Line effects

The original `effects/mp/spawn.efx` contains two Lines followed by an Emitter.
Both Lines now compile through a bounded declaration adapter: one shader,
one immediate spawn, fixed lifetime, constant sampled colour/alpha, explicit
or traced endpoints, and constant or wave width. Unsupported flags, motion,
curves and primitive families still refuse by name. The entire declaration
is checked before any of its instances are published; accepting its Lines
does not silently discard its Emitter.

**Retail observations.** `jamp.exe`'s Line trace uses a 16,384-unit ray,
mask `0x1001`, skip entity `-1`, and cgame export 15. Its shared callback
record is 104 bytes: the 48-byte trace result precedes start, mins, maxs,
end, skip and mask at offsets 48, 60, 72, 84, 96 and 100. The adapter calls
the module's trace so entity collision remains part of the result.
The wave multiplier is binary32 `0.0031415901612490416`, derived from
retail's `3.14159`, with a float-rounded phase before cosine. Width can
overshoot its endpoint; lifetime includes the final millisecond.

An isolated x86 oracle executes the retail width routine and its statically
linked cosine, rebasing only their absolute data operands. All 96 scalar
evaluations pass. Separate native fixtures cover the adapter under normal
and fast-math compilation, malformed records, wave overshoot, degenerate
geometry, lifetime boundaries, pool exhaustion and transactional refusal.
No whole retail engine was run for these scalar observations.

**Ownership.** A callback can invalidate the borrowed effects state. A
lifetime revision, module/shared-buffer checks and a nested-effect guard
detect cleanup, reset, clock/view mutation and reentry before any borrowed
declaration is reused. Real error cleanup and subsequent valid calls are
tested. The canonical trace export maps only to the Academy MP contract.

**Presentation.** Lines use Fn's camera-facing polygon, shader, blending
and clipping path. Existing per-pixel/static world lighting and shadows
remain available; an additive effect material does not acquire opaque-model
lighting. No retail renderer or retail-exact presentation option is added.

**Live limit.** An unchanged installed spawn now stops at **Emitter**, where
the previous binary stopped at Line. Its moving physics/emission chain also
requires `env/beam_lights` (Light and FxRunner) and `env/beam_bolts`
(Electricity). These are explicit gaps. An independently authored Line-only
override in an isolated test home lets the retail client enter the match,
then exposes a separate Kyle bone-animation/bolt refusal. This diagnostic
is not evidence that the installed spawn works. Engine captures still show
the player setup overlay; visible Line appearance is not established.

## Outcast static menu rendering

Retail UI services 18/19/21–25/29/62 now enter Fn's model, skin, scene,
polygon, light, view, tag and bounds services. They marshal Outcast's own
records: a 212-byte entity, 368-byte view, 24-byte polygon vertex and
48-byte orientation. Academy's view is 384 bytes and is not substituted.
The retail renderer table and shipped `ui.qvm` independently establish the
sizes, field offsets and callback argument order. Failed transfers preserve
caller output and publish no renderer packet.

The stock menu uses a static MD3 logo. Twelve installed menu files place it
partly outside the viewport; the main menu's authored x is `-78`, producing
the observed view `(-77, 84, 248, 248)`. Negative origins are valid and now
remain intact. After menu scaling, the adapter checks both rectangle sums
and Fn's conversion to bottom-origin framebuffer coordinates using 64-bit
arithmetic. Unsupported procedural entities, Ghoul2 and Raven-only flags
still refuse explicitly.

Fn owns model geometry, materials, lighting and the menu viewport/aspect
settings. This adds no retail renderer and does not disable Fn features.
Tests cover native and QVM transfers, unaligned records, finite values,
signed viewports, callback output failures and unsupported contracts.

**Authored shaders and images.** Both retail executables enumerate
`shaders/*.shader`; the installation has 29 such files and none under
`scripts/`. All three Fn collectors now include that folder for both
Outcast modes, retaining engine shader supplements and their override
order. Previously, the menu could not find its material definitions.

The menu's noise stretch and random colour waveforms exposed two more
profile omissions. Both Outcast executables accept `random` as function 7
and evaluate noise with `(viewSeconds + phase) * frequency`, rounded to a
float at the noise-call boundary. Their table initializer seeds the same
15-bit sequence at 1001 as Elite Force's measured initializer. Outcast now
selects that existing, locally seeded table and its own noise clock path;
Q3/QL and other profiles retain their behavior. No sine or constant
substitute is used for these supported Outcast controls.

Outcast's top-down TGA font atlas now decodes with its authored orientation.
The SP and MP binaries independently establish row direction and rejection
of oriented RLE. The shared loader extends the existing Jedi policy to the
two Outcast profiles only. Both also select the installed `charsgrid_med`
console atlas, with half-cell horizontal UVs, instead of requesting a
missing Q3 `bigchars` image. Fn's text placement, scaling, colours and
shadows remain intact. Atlas orientation and text tests include all 16
profiles; shader collection tests exercise all three renderers.

**Live result.** The unchanged retail MP UI now draws its main menu,
including its two partly offscreen logo models and correctly oriented font,
and exits with code 0. A 640×480 GLX engine screenshot verifies the rendered
menu. The probe uses no injected input and does not establish menu interaction
or complete client gameplay. The first map attempt also exposed a client
module cleanup reentry that hid the original unsupported import. An
initialization flag now prevents Outcast's interrupted cgame from being
reentered during cleanup, while preserving normal shutdown and releasing
its files and language packages. The follow-up reports the actual first
blocker, **cgame import 300**, without recursive shutdown. Tests cover
partial/normal/error/repeated cleanup and preservation of the original
diagnostic through a real longjmp.

## Boundary corrections shared with rendering

Both Jedi view adapters now require a terminator within each 32-byte shader
text slot. Previously, a malformed slot could reach the renderer's
unbounded string reader. Each of the eight slots has tests for refusal
without publication and acceptance of 31 characters plus a terminator.

The shared UI coordinate rounding helper now refuses nonfinite and
out-of-range values before float-to-int conversion. Viewport adjustment
checks its derived sums and publishes all four fields only after validation.
Ordinary rounding, optional stretching and centred menu presentation retain
their existing behavior. Tests execute the actual shared helpers at several
aspect ratios, with normal and fast-math compilation. This correction also
protects the existing Q3/QL callers.

## Academy single-player cvar flags

The external SP record previously preserved requested flags but could omit
flags already present on an engine cvar, or retain stale ones after engine
updates. Retail `jasp.exe`'s `Cvar_Get` (`0x41bc70`) merges flags into the
existing live record at `0x41bd04` and returns that same pointer at
`0x41bd97`. The adapter now refreshes shared flags from the engine while
retaining only Academy's external save flag 256. Fn's internal TEMP bit
must not manufacture that save flag. Regression tests cover existing ROM
and LATCH flags, subsequent changes, stable pointer identity and save-bit
isolation. This is boundary evidence, not campaign/savegame validation.

## Validation and remaining work

Builds and runtime probes use the stable Windows x86 development paths.
Every game probe is windowed on an unswitched private desktop, with isolated
writable home data, zero sound gain and no mouse or keyboard control.
Rendering evidence comes from the engine's `screenshot` command. Retail
installation inventories are unchanged before and after each probe.

Runtime logs, oracle source/results and captures are retained under
`.tmp/jedi-audit-round2/`; disposable executables and writable test homes
are removed. The JSON companion records the completed checks and remaining
frontiers. Full campaigns, saves, complete world/effects rendering, demos,
input, gameplay and bidirectional retail interoperability remain open.

The x86 engines and all three renderer modules build. The final broad run
passes **265 of 267 targets**, with no failures; the OpenAL loopback and
optional Quake Live factory tests skip because their required runtime or
fixture is unavailable. The one-hour recursive source test is excluded.
Targeted waveform/image/text checks and 20 documentation/manual tests also
pass. No clean release/profile matrix or full-compatibility claim is made.
The system OpenAL runtime still lacks OpenAL Soft extensions, and some
symlink test cases remain unavailable on this host; these environment limits
are separate from the Jedi changes.
