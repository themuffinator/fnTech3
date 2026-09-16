# RTCW navigation worlds and MP client map initialization

This checkpoint adds two-world ownership and blocking semantics to the shared
engine. It does **not** implement live AAS8 loading, playable RTCW, AI route
parity or savegames. RTCW remains gated in default builds. Evidence refers to
the dirty worktree at `c75573c7375e44c908256020ada2d7e12970cacf`, not that commit
alone.

## Contracts and implementation

The game catalog owns the navigation-world count: one for Q3/QL, two for RTCW
SP/MP. World objects have stable addresses; switching selects storage instead
of copying objects containing owning pointers. Setup selects world zero;
shutdown releases every world and restores zero. The shared physical BSP data
is released once. Link accounting, route-cache accounting, alternative-route
scratch and hide-area travel times now belong to their world. Hide-area scratch
previously survived map transitions and could be too small for the next map.
Failed replacement allocations no longer leave freed scratch pointers behind.

RTCW game import 307 selects a world. Blocking imports 324 (SP) and 323 (MP)
visit both worlds, skip unloaded worlds, query up to 128 intersecting areas in
each loaded world, and update routing enable flags. They leave world one
selected, including when both worlds are unloaded. This last case is exercised
by stock MP even with `bot_enable 0`. The implementation changes real area state
and invalidates that world's routing caches; it is not an unconditional success
stub. Cache invalidation also works before routing tables are initialized.

The raw signed blocking argument is preserved. Equal bounds reset all areas
when the argument is negative in original SP, or zero in original MP. iortcw
SP instead uses MP's zero predicate. Original MP's savegame source still passes
`-1`, contradicting its engine reset predicate; this source discrepancy remains
open for retail/savegame evidence. The current SP contract follows original SP.

The boundary checks exact QVM extents, integer ranges, finite vectors and
ordered bounds before changing world state. Native pointer checks reject null
and address wrap, without claiming to validate arbitrary native allocations.

MP map startup exposed three additional client contracts:

- Cgame sound registration, like RTCW UI registration, supplies only a name.
  A shared sound-name reader passes `qfalse` for compression, bounds strings to
  `MAX_QPATH`, and checks exact QVM extents without masking addresses. Long names
  return the original failure handle; no nonexistent second argument is read.
- Import 51 reads map entity tokens through the renderer's existing parser.
  Checked local output preserves return status, truncation, terminators and
  caller trailing bytes, including when a backend fails.
- Null model and skin names return zero, as in both original RTCW renderers.
  The stock weapon table uses null for absent alternate models.

## Evidence

`tests/rtcw_navigation_tests.py` executes real area-box queries, routing state
changes, cache invalidation, world selection and lifecycle functions. It covers
different geometry in each world, unloaded worlds, negative SP reset arguments,
failed allocations, repeated shutdown, QVM bounds, unaligned vectors, canaries,
nonfinite values and inverted bounds. Related client fixtures execute sound
registration and entity-token dispatch, including minimal argument arrays.

`scripts/probe_rtcw_navigation.py` compares 480 cases per original/iortcw SP/MP
reference, using actual engine area queries and a signed trap-argument fixture.
Original SP, original MP and iortcw MP match all 480 cases. iortcw SP matches 392
and has 88 explicitly expected reset-predicate differences. All four reference
runs pass on Windows x64/x86 and Linux x86 with ASan/UBSan. This establishes
source-decision agreement, not the behavior of a compiled retail enum ABI.
Reports are `navigation-reference-{x64,x86,sanitizers}.json` under
`.tmp/rtcw-manager/`.

The final focused Windows x86 run passes eight tests in 8.841 seconds
(`navigation-final-x86.log`). Four final navigation/client tests pass Linux
x86 ASan/UBSan in 31.769 seconds (`navigation-final-sanitizers.log`). The full
suite passes 111 Meson checks with two skips and no failures; its Python suite
passes 1,363 tests in 258.739 seconds, with 11 skips. Logs are
`navigation-full-tests.log` and `navigation-full-testlog.txt`.

The default Windows x86 client SHA-256 is
`0be9e636d1a78df588c31b09ed5a43de59cad010ef7dc52a2b450dd28b3629e7`.
Q3 and QL scene probes pass in GLX, Vulkan and RTX raster, with all six engine
captures inspected. `navigation-live/manifest.json` and its audit retain the
windowed/no-input commands, module observations and captures. QL shows its
native join UI over the world; these tests did not join a match.

The original installed MP module now passes server initialization, collision
and sound loading, map entity parsing, weapon models and server models. The
latest bounded map probe stops during client model initialization with
`Invalid module render entity`; SP still stops at import 206 (`BOTLIB_LOAD_MAP`).
`navigation-final-map/manifest.json` records commands, module identities,
executable hash and logs. Its experimental Windows x86 client SHA-256 is
`024dbdf9a6b5a69a6fe8ab2771a1ea08c6bb56f8662590dc530b23a401bf20aa`.
The probes used windowed unswitched private desktops,
disabled input/audio, isolated writable homes and unchanged installed assets.
They produced expected bounded failures, not successful map boots.

Source inspection identifies a likely next blocker: original
`CG_CalcMoveSpeeds` supplies a partially initialized entity to `R_LerpTag`,
setting only model and leg frames for foot-tag queries. The current tag adapter
validates unrelated scene-entity fields, and MDS tag calculation currently
decodes the whole skeleton. This inference needs focused/runtime validation;
unused channels must not be treated as required scene input.
The subsequent [client-animation checkpoint](2026-09-15-rtcw-client-animation.md)
confirms and fixes this blocker, advancing MP through cgame initialization.

Original/iortcw sources are GPLv3. Only independently authored production
changes and modifications of the inherited GPL-2.0-or-later engine are used.
Original functions are compiled solely in disposable reference fixtures with
their notices; no proprietary assets or GPLv3 implementation are staged.
