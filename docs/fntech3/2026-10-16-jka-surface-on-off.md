# Jedi Academy's surface switch, 2026-10-16

The Jedi Academy multiplayer (`jka-mp`) client game hides and shows named
parts of a Ghoul2 model through client-game import 321,
`CG_G2_SETSURFACEONOFF`: a corpse's eyes, a vehicle's breakable panels, an
NPC's optional parts, Boba Fett's jets. fnTech3 refused the import, so a
siege match with a bot on `mp/siege_korriban` stopped soon after
the bot's second corpse appeared (the
[skin-gore slice](2026-10-16-jka-clear-skin-gore.md) reached it). This slice
serves it as retail `jamp.exe` 1.01 does. The
[JSON companion](2026-10-16-jka-surface-on-off.json) records the binaries,
retail identities, commands and outcomes. No compatibility gate closes.

Implementation is independently authored. Retail `jamp.exe` 1.01 and the
1.01 `cgamex86.dll` in `assets3.pk3` were read with capstone and pefile.
Pinned OpenJK was used only to name routines (reference observation; no text
incorporated).

## Retail

### Observed, `jamp.exe` 1.01

- The client game's dispatcher (`0x412170`) takes import 321 to index 197
  (`byte [0x414638 + 321]`) and its target `0x413d7e`
  (`dword [0x4142bc + 197*4]`).
- `0x413d7e` maps `args[2]` (the name) through the module's address mapping
  (`0x44adf0`, which passes a null address through as null), pushes
  `args[3]` (the flags word) and calls `0x4deb70` with `args[1]`, the client
  game's Ghoul2 word, in `eax`. It answers whatever `0x4deb70` answers. The
  import takes no model index.
- `0x4deb70` answers 0 for a null word (`0x4deb76`) and for an instance
  with no models (`0x496650`, which also answers 0 for an invalid handle).
  Otherwise it takes the first model record (`[info + 4]`, the start of the
  0xc8-byte records): the switch only ever works on **model 0**. It answers
  0 when that model's setup (`0x4e0b10`) fails, as it does for an empty
  place (`mModelindex` -1, `+0x30`) or a model with no mesh. After a
  successful setup it zeroes the model's mesh frame word (`+0x9c`), which
  makes the renderer rebuild its surface list, and calls `0x4ee700`.
- `0x4ee700` answers 0 when the model has no mesh header. It first looks
  for the name in the model's override list (`+0x4`, 0x18-byte entries:
  flags, surface, then four words) with `0x4ee610`, which walks the list
  from its end, skips generated (10000) and unset (-1) surfaces, and
  compares each entry's surface name, found through the mesh's offset table
  at `+0xa4`, with `_stricmp` (`0x539336`). A listed surface has OFF (2)
  and NODESCENDANTS (0x100) cleared and the request's two bits ORed in; its
  other bits stay. The answer is 1.
- An unlisted name goes to `0x4ee5b0`, which walks the hierarchy records
  in file order from `ofsSurfHierarchy` (`+0x9c` of the mesh header), 0x90
  bytes plus four per child, comparing each name with `_stricmp`. The
  first match gives its index and default flags (`+0x40`); none answers 0.
  The new flags are the default with OFF and NODESCENDANTS replaced by the
  request's; only when they differ from the default is an entry appended
  (`0x4eef90`). The answer is 1 either way.
- Every other bit of the flags word is ignored (`and esi, 0x102`).
- A null name with a model present reaches `_stricmp` and faults.
- The same list holds the skin's visibility: applying a render skin
  (`0x4ee7e0`) empties it (`0x4ee80d..0x4ee829`) and refills it from the
  skin, changing only OFF and NODESCENDANTS.

### Observed, `cgamex86.dll` 1.01

The wrapper for import 321 is `0x3005f610`: it pushes its three arguments
and 321 and calls the engine. Its eleven callers, in four routines:

| Sites | Routine | Name | Flags |
| --- | --- | --- | --- |
| `0x3003240f`, `0x3003242e`, `0x30032440` | `0x300317e0` | three stack buffers | 0, 0x100, 0 |
| `0x30039c8e` | `0x30037460` | `head_eyes_mouth` | 2 |
| `0x30046266`, `0x300462b5` | `0x30045f30` | each token of two parsed lists | 2, 0 |
| `0x30046479`, `0x3004648b`, `0x3004649d` | `0x30045f30` | `torso_rjet`, `torso_cjet`, `torso_ljet` | 0x100 each |
| `0x30055c0b`, `0x30055c33` | `0x30055b50` | entry *n* of the table at `0x30095040` | 0x100, 0 |

- `0x30039c8e` runs only for an entity of type 15 (`cent + 4`) whose
  Ghoul2 word is set and holds a model (import 293, `0x3005f190`).
- The jet calls run when the model name is not `boba_fett`
  (`0x30046453..0x3004646a`).
- The table at `0x30095040` names `l_arm_key`, `torso_canister1` to `3`,
  `torso_tube1` to `6`, `r_arm`, `l_arm`, `torso_shield`, `r_wing1`,
  `l_gear`, `nose` and others; `0x3007f070` beside it holds a small number
  for some entries.

### Inferred, from OpenJK

- Entity type 15 is `ET_BODY`, the corpse the server copies into its body
  queue, and `0x30037460` is the player draw (`CG_Player`), which closes a
  corpse's eyes.
- `0x30045f30` loads an NPC's model (`CG_G2AnimEntModelLoad`): the parsed
  lists are its `surfOff` and `surfOn`, and the jets are turned off on
  every model but Boba Fett's.
- `0x30055b50` updates an NPC's toggleable surfaces (`bgToggleableSurfaces`
  and its debris table) when the server's surface bits change.
- 0x100 is the client game's `TURN_OFF`, 0 its `TURN_ON`.

## Which site runs here

A throwaway diagnostic build printed each call's return address, name and
flags (`surfoff-diag2-korriban-bot`). The client game loaded at
`0x3d3d0000` (its `vmMain`, RVA `0x425a0`, at `0x3d4125a0`). One call ran,
returning to `0x3d409c93`, which is `0x30039c93`: the corpse site
`0x30039c8e`, with `head_eyes_mouth` and flags 2, straight after the bot's
second `rcg 1`. The corpse is the bot's: its model,
`models/players/reborn_new/model.glm`, has no `head_eyes_mouth` surface (its
head surfaces are `head`, `head_face`, `head_hat` and capping surfaces), so
the import answered 0 and changed nothing, as retail's lookup would.

## fnTech3

### Before

The client's Academy Ghoul2 store (`client_jka_cgame_ghoul2.hpp`) kept
surface visibility only from skins, per instance and model, in the skin
binding the renderer reads. Import 321 was refused by number.

### Now

- Import 321 is served in the client game's Ghoul2 dispatcher. It requires
  the x86 legacy native client game, as the other model imports do.
- A null word answers 0 and creates no store. Any other word must name an
  instance this store owns, or it is refused as stale or foreign.
- The switch works on model 0 only. An empty place 0 answers 0 before the
  name is read; with a model there, a null name is refused, where retail
  would fault.
- The name is read up to 64 bytes. A longer name matches no surface and
  answers 0. Otherwise the model's kept mesh is walked in file order, as
  `0x4ee5b0` does, matching names without regard to ASCII case; a header
  that no longer matches the parsed model, a record past the file or an
  unterminated record name is refused.
- A found surface answers 1. Its flags become the current flags (the
  binding's override, or the default) with OFF and NODESCENDANTS replaced
  by the request's. The binding keeps an override only where the result
  differs from the default; this is the same drawing state as retail's
  list, which may also keep an entry equal to the default.
- The overrides live in the same per-model binding as the skin's: a render
  skin replaces them, as retail's skin application empties its list, and a
  skin change with no render skin keeps them. A binding made only by this
  import has render skin 0, which the renderer now accepts; a binding left
  empty with render skin 0 is dropped. Duplicating an instance copies the
  binding and cleaning it drops it, as before.
- Retail's mesh-frame reset has nothing to do here: the renderer reads the
  binding every frame and registers new geometry when the visible set
  changes.

### Presentation

The switch changes which surfaces of a model Fn's skeletal renderer draws.
The model keeps Fn's materials, lighting and shadows (invariant 11); nothing
retail-specific is added.

## Evidence

### Unit

`tests/jka_cgame_surface_on_off_tests.py` compiles the production Ghoul2 and
render headers into the client-game render fixture of
`jka_cgame_render_tests.py` and drives import 321 through the production
dispatcher, then draws through Fn's renderer hooks:

- Before any model loads, a null word answers 0 and an unknown one is
  refused; neither makes a store.
- A non-native client game is refused. Unknown, partial, empty and
  over-long names answer 0 and record nothing; a surface already as asked
  answers 1 and records nothing; a null name is refused.
- Names fold case. A child off by default is turned on, which a skin cannot
  do; going back to the default drops the override and then the binding. A
  default's other bit (1) survives every switch.
- Drawn: OFF hides one surface, NODESCENDANTS hides it and its child.
- A render skin starts the list again; a skin change without one keeps it.
- Only model 0 is switched; an empty place 0 answers 0, even with a null
  name. A duplicate carries the overrides; a clean drops them.
- Foreign and stale words, a changed header and an unterminated record name
  are refused. Clearing the client game drops the store and the bindings.
- 22 mutations each fail at run time, among them: no native gate, a null
  word refused, the store made first, the last model switched instead of
  model 0, an empty place refused, a null name answered, a long name
  refused, an absent surface answering 1, case-exact names, children not
  stepped over, flags read from the shader field, only OFF switched, other
  bits dropped, the listed override ignored, defaults kept as overrides,
  empty bindings kept, state made by an unchanged switch, and render skin 0
  read as stale. A second test pins the retail addresses in the comments.

### Live

Each probe ran the client in an 800x600 window on a private desktop with
`net_enabled 0`, sound off, no input and engine screenshots only, through
`.tmp/round-2026-10-15/jka-local-sets/probe_live.py`, on
`mp/siege_korriban` (`g_gametype 7`) with `team red` and
`addbot Tavion 1 blue`. Runtimes were built from a `git archive` export of
`cf68caf1`, without (before) and with (after) this slice, Windows x86
debugoptimized with strict warnings. The diagnostic runtime is the after
build with two prints. The retail installation and the binaries were
unchanged after every run.

| Run | Runtime | Outcome |
| --- | --- | --- |
| `surfoff-before-korriban-bot` | before | Tavion dies twice; `ircg 1 32 0 0`, `rcg 1`, `ircg 1 33 0 0`, `rcg 1`, then "Unsupported jka-mp cgame import 321 (CG_G2_SETSURFACEONOFF)" |
| `surfoff-after-korriban-bot` | after | the same four commands, then "Academy cgame unsupported Raven renderer entity flags" |
| `surfoff-diag2-korriban-bot` | diagnostic | the same four commands; import 321 from `0x30039c8e` with `head_eyes_mouth`, flags 2, answered 0; capture 07 written after it; then renderfx `0x40800` refused |
| `surfoff-before2-korriban-shots` | before | captures every 400 frames; import 321 stops the client straight after the second death's `ircg 1 33 0 0` (seven captures) |
| `surfoff-after2-korriban-shots` | after | the same captures; the second `ircg 1 33 0 0` is followed by three more captures and `rcg 1`, then the renderfx refusal (six captures) |

Bot play differs from run to run. In `surfoff-diag-korriban-bot` the bot
fought before it died, and the client stopped at client-game import 299
(`CG_G2_COLLISIONDETECT`) without reaching import 321.

The captures show the siege team menu that the server's `scl` opens on
joining: the probe gives no input to close it. They prove only that the
client kept drawing frames after the import.

### Beyond it

- **A corpse's disintegration.** Some frames after import 321 the corpse
  is drawn with renderfx `0x40800`: the tint (0x800, served) and Raven's
  `RF_DISINTEGRATE2` (0x40000). The renderer adapter refuses that flag:
  "Academy cgame unsupported Raven renderer entity flags". That is the next
  refusal on this path.
- **Saber collisions.** When the bot fights, client-game import 299
  (`CG_G2_COLLISIONDETECT`) stops the client.
- The other ten call sites were not reached. The NPC and vehicle sites need
  NPCs that the probe did not spawn.

## Validation

- The `cf68caf1` export with this slice built every target of the client
  configuration with strict warnings and no warning. Its native suite
  passed 274 of 276 tests and skipped two (OpenAL loopback and the
  optional Quake Live factory probe). 72 Python test files, one file to a
  process, passed 250 tests and 1,309 subtests with one skipped: every
  `jka_*` file, and the changelog, generated-docs, release-packaging,
  release-workflow, version and player-manual files.
- HEAD then gained the Outcast client-game slice (`f2780604`) and the
  skinless-model slice (`0d1b7187`), which adds import 312 to the same
  header. The export was advanced to `0d1b7187` path for path, with this
  slice's hunks applied to that commit's files. It rebuilt every target
  with no warning; the native suite again passed 274 of 276 with the same
  two skipped, and 75 Python files (the same set with the new `jka_*`
  file, `jk2_cgame_ghoul2_tests.py` and `hud_aspect_tests.py`) passed 258
  tests and 1,351 subtests with one skipped. This commit sits on
  `0d1b7187`. The live runs used the `cf68caf1` runtimes; neither later
  slice touches this path's files other than the shared header's import
  312 case.

## Remaining and unrelated findings

- Raven's `RF_DISINTEGRATE2` (0x40000) on a corpse's Ghoul2 entity is
  refused by the render adapter.
- Client-game import 299 (`CG_G2_COLLISIONDETECT`) stops the client as soon
  as a saber meets a model.
- The Ghoul2-entity refusal in `client_jka_cgame_render.hpp` names no flag
  value; its non-Ghoul2 sibling in `client_jka_cgame_host.hpp` does.
- Client-game imports 313 and 314 (skin gore) are still not served.
