# Jedi Academy's skinless-model query, client-game import 312, 2026-10-16

The [menu slice](2026-10-15-jka-ui-menus.md) took Academy's siege games past
the class menu. On `mp/siege_hoth` and `mp/siege_desert` in a siege game
(`g_gametype 7`) the client then stopped at "Unsupported jka-mp cgame import
312 (CG_G2_SKINLESSMODEL)", and `mp/siege_hoth` in a free-for-all stopped
there too after the [frame-index slice](2026-10-15-jka-setanimindex.md).
This slice serves import 312 from fnTech3's client Ghoul2 store, answering
as retail `jamp.exe` 1.01 answers. The
[JSON companion](2026-10-16-jka-skinless-model.json) records the binaries,
retail identities, addresses, census, runs and outcomes. No compatibility
gate closes.

Implementation is independently authored. Retail `jamp.exe` 1.01 and the
1.01 `cgamex86.dll` in `assets3.pk3` were read with capstone and pefile.
Pinned OpenJK was used only to name routines and to compare
(reference observation; no text incorporated).

## Retail

### Observed, `jamp.exe` 1.01

- The client game's dispatcher (`0x412170`) sends import 312 through index
  188 to `0x413c84`. It pushes the client game's two words and calls
  `0x4116e0`, then `0x4e0740` with the record that returns, and answers what
  `0x4e0740` answers.
- `0x4116e0` reads the first word as a pointer and loads the handle it
  holds (`0x4116e9`): a null word faults there. It asks the Ghoul2 info
  array (`0x4de4b0`, vtable `0x54e59c`, slot `+0x10` = `0x4de3d0`) for that
  handle's model list and returns the list's start plus the second word
  times `0xc8`, the size of one model record. Nothing bounds the second
  word.
- `0x4de3d0` answers an unknown handle (zero, or one whose slot holds
  another number) with a static empty list (`0x1300c30`), cleared through
  `0x4e16d0`, whose start is null. Place 0 of it is a null record.
- `0x4e0740` is the test. It sets up the record's model pointers
  (`0x4e0b10`); when that fails it answers 1. Setup fails for a null record,
  for an empty place (model index `-1` at `+0x30`), and when the mesh, its
  `.glm` header or its skeleton cannot be registered. Otherwise it walks the
  mesh's surface hierarchy from `ofsSurfHierarchy` (`+0x9c`) for
  `numSurfaces` (`+0x98`) records, each `0x90` bytes plus four per child
  (`+0x8c`). The first record whose shader name (`+0x44`) is not empty
  answers 0; none answers 1.
- The mesh loader (`R_LoadMDXM`, `0x49acd0`; hierarchy loop
  `0x49ae00..0x49aea8`) lowercases each surface name and trims a trailing
  `_off`, and only reads the shader name, to look it up. So the shader bytes
  the test reads are the file's own.
- Setup refuses a model whose file changed size since it was first set up
  ("Ghoul2 model was reloaded and has changed, map must be restarted.",
  `0x54e5b0`, `ERR_DROP`).

### Observed, `cgamex86.dll` 1.01

- One wrapper (`0x3005f530`) pushes 312 and calls the engine, from one site
  (`0x30032957`). Just before it, the client game makes a general entity's
  Ghoul2 instance with import 297 (`0x3005f230`), model index, skin, shader,
  flags and level of detail all zero, and asks only when the instance it
  got back is not null, always for place 0.
- On 1 it copies the model's path, cuts it after the last `/`, appends
  `model_default.skin` (`0x3008aa94`), registers that (import 55,
  `0x3005e6b0`) and sets it on place 0 as both skins (import 298,
  `0x3005f270`). A path with no `/` sets skin 0. On 0 it does neither.

### Installed meshes

Of the 127 `.glm` files in Academy's paks, 65 name no shader on any surface,
every one of them under `models/players/`, and every one has a
`model_default.skin` beside it. The other 62, the map objects among them,
name shaders. The census is in the companion.

### Inferred

The query asks whether a mesh relies on a skin for every surface. On the
maps measured it is asked of the map objects the client game draws as
general Ghoul2 entities, which answer 0; the player and vehicle meshes that
answer 1 go through the player path, not this one, in those runs.

OpenJK's renderer serves the import the same way, with its own client
adapter answering 0 for a null instance where retail faults.

## fnTech3 before

Import 312 fell to the client game's default branch and dropped with
"Unsupported jka-mp cgame import 312 (CG_G2_SKINLESSMODEL)".

## Implemented

- **The import.** `code/client/client_jka_cgame_ghoul2.hpp` serves
  `CG_G2_SKINLESSMODEL` after the native-module check every Ghoul2 import
  makes. A null identity is refused by name, where retail faults. Any other
  identity must be one this client's store owns: an unknown or stale one is
  refused, as for the other Ghoul2 imports, and asking never creates the
  store. A place below zero or past the instance's last is refused by name:
  retail reads whatever follows its record array there.
- **The test.** `CL_JKACgameGhoul2Skinless` answers 1 for an empty place, as
  retail's failed setup does. For a model it walks the kept `.glm` bytes
  exactly as `0x4e0740` does, in file order from `ofsSurfHierarchy`, and
  answers 0 at the first non-empty shader name. Every read is bounded: a
  model without kept bytes, a header whose surface count is not the parsed
  one, and a record or child list past the file are refused by name, though
  a mesh the store loaded has already passed the same walk in its parser.
  It reads; it loads, binds and moves nothing.
- **What follows a 1** is already served: import 55 registers the
  `model_default.skin` and import 298 sets it, through Fn's skin path.
- **Retail's setup failures** for a mesh or skeleton that cannot be
  registered do not arise here: fnTech3's store does not keep a place for a
  model it could not load, so the client game gets no instance to ask
  about.
- **Presentation.** None: the answer chooses which skin the client game
  registers, and the model draws through Fn's existing skeletal path. This
  slice adds no presentation feature and leaves none out.

## Tests

`tests/jka_cgame_skinless_model_tests.py` (new) compiles the production
header inside the cgame Ghoul2 fixture and runs import 312 against authored
meshes: the fixture mesh (a shader on its one surface), the same mesh with
that name cleared, and three three-surface hierarchies whose root has two
children, with a shader on the last surface, on none, and on the root. It
checks the answers on two passes, an appended model at place 1, the empty
place a copy past the end leaves and the copied model past it, places -1,
2, `INT_MIN` and `INT_MAX`, null, unknown, foreign and stale identities,
no store, a missing native entry point and structured exports, and the
helper on a model without kept bytes, with a changed surface count, and
with child lists that run past the file, the last record's included. It
also checks that the query opens no file and changes no skin. Fifteen
mutations of the header each fail it: the null refusal, the native gate,
either bound, an off-by-one bound, an inverted answer, an empty place
answering 0, a missing mesh answering 1, the surface name read as the
shader, only the first surface read, children not stepped over, the walk
started at the offset table, the header count unchecked, the child list
unbounded, and the import unserved.

## Live

Clients ran windowed at 800x600, on a private desktop, with no network
sockets, sound off and no input; console commands came from the probe's own
config (`.tmp/round-2026-10-15/jka-local-sets/probe_live.py`). Each ran on
its own home, and the installation was unchanged. Captures are the engine's
`screenshotPNG`. The pair was built from an export of `d17f578a`, without
and with this slice. HEAD then gained `c90b074b` and `2e2207e0`, the
[pass-entity slice](2026-10-16-jka-trace-pass-none.md); the export was
advanced to `2e2207e0` with this slice, path for path, and rebuilt as a
second "after" runtime. The identities are in the companion.

| Map, game type | Before | After |
| --- | --- | --- |
| `mp/siege_hoth`, siege | stops at client-game import 312 | plays to the probe's end; the class menu and objectives are up |
| `mp/siege_desert`, siege | stops at client-game import 312 | plays to the probe's end |
| `mp/siege_hoth`, free-for-all | stops at client-game import 312 | plays to the probe's end; the player configuration menu is up |

The second "after" runtime, on `2e2207e0`, gave the same three outcomes.

Diagnostic runtimes, each "after" build with one line printing each
answer, ran the same three and more:

- On every run the client game asked import 312 for
  `models/map_objects/mp/turret_chair.glm` at place 0, twice on
  `mp/siege_hoth` and four times on `mp/siege_desert`, and got 0: the
  chair's mesh names its shaders. No other mesh was asked about.
- In a free-for-all with `team free` and the menu closed the player plays:
  the capture shows the base, the crates, the HUD and the disruptor.
- In a siege game with `team red` the player joins the Imperials and
  waits. With `addbot Tavion 1 blue` as well, on `d17f578a` the round
  counts down and the server stops at "Jedi Academy trace cannot read pass
  entity -1 or the entities it touched", where `mp/siege_korriban` stopped
  in the menu slice.
- On `2e2207e0`, which serves that trace, the same commands on
  `mp/siege_hoth` and `mp/siege_desert` go further: both players enter the
  game, the round begins, and the client stops at "Unsupported jka-mp
  cgame import 311 (CG_G2_REMOVEGHOUL2MODEL)". That is the next refusal on
  both maps.
- A free-for-all with `team free` on `2e2207e0` plays to the probe's end as
  before.

Answer 1 has no live run: no measured map asks it of a skinless mesh.

## Validation

- **Builds.** The export, configured as the Windows x86 developer client
  with strict warnings, built `fntech3`, the GL renderer and the packages
  with no warning on `d17f578a`, `c90b074b` and `2e2207e0`, each with this
  slice.
- **Tests.** One pytest process per file, in the export: on `d17f578a` the
  85 files that read the changed header or the documents passed 616 tests
  and 1,935 subtests, with one skipped. On `c90b074b` the same files plus
  that commit's changed tests, 89 in all, passed 627 tests and 2,051
  subtests, with one skipped. On `2e2207e0` the 82 files that read the
  header, the documents or that commit's trace passed 615 tests and 2,043
  subtests, with one skipped.
- **The commit's parent.** HEAD then gained `35e01aa1`, which changes the
  client-game host this slice's fixture compiles from and three of its
  documents. The export was advanced to it with this slice and rebuilt
  with no warning, and 98 files, the 82 above, the client-game fixtures
  that compile that host and that commit's tests, passed 670 tests and
  2,069 subtests, with one skipped. Last, `cf68caf1` added import 315's
  case beside this slice's in the same header; on it with this slice the
  export rebuilt with no warning, and the 16 files that compile this
  header or check the documents and packages passed 120 tests and 915
  subtests, with one skipped.
- No native test compiles the header; the engine build above does.

## Remaining and unrelated findings

- **Siege's next stop** with a bot, now that the
  [pass-entity slice](2026-10-16-jka-trace-pass-none.md) serves the trace,
  is client-game import 311 (`CG_G2_REMOVEGHOUL2MODEL`) as the round begins
  on `mp/siege_hoth` and `mp/siege_desert`.
- **The gore imports** 313 to 315 stay with their own slice; this one
  leaves them alone.
- **A model that failed to load** gets no place in fnTech3's store, where
  retail keeps an empty one (model index `-1`). That belongs to import 297,
  not this query.
