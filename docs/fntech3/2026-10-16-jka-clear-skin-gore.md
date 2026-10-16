# Jedi Academy's skin-gore clear on respawn, 2026-10-16

When a Jedi Academy multiplayer (`jka-mp`) player respawns, the game module
copies the body into its body queue and sends every client `ircg` (or `rcg`
when no body is left). The retail client game answers either one by asking
the engine to clear the player's skin gore: client-game import 315,
`CG_G2_CLEARSKINGORE`. fnTech3 refused that import, so the first respawn in
any match stopped the client. The [menu slice](2026-10-15-jka-ui-menus.md)
reached it through throwaway runtimes on `mp/siege_korriban`, and the
[pass-entity slice](2026-10-16-jka-trace-pass-none.md) there with a bot. This
slice serves it as retail `jamp.exe` 1.01 does. The
[JSON companion](2026-10-16-jka-clear-skin-gore.json) records the binaries,
retail identities, commands and outcomes. No compatibility gate closes.

Implementation is independently authored. Retail `jamp.exe` 1.01 and the
1.01 `cgamex86.dll` in `assets3.pk3` were read with capstone and pefile.
Pinned OpenJK was used only to name routines and find the server's callers
(reference observation; no text incorporated).

## Retail

### Observed, `jamp.exe` 1.01

- The client game's dispatcher (`0x412170`) takes import 315 to index 191
  (`byte [0x414638 + 315]`) and its target `0x413cd5`
  (`dword [0x4142bc + 191*4]`).
- `0x413cd5` loads the client game's word, `args[1]`, into `esi`, calls
  `0x4e07f0` and answers 0. Nothing else reads the arguments.
- `0x4e07f0` reads the instance through that word: `mov ecx, [esi]` at
  `0x4e07fb` takes the instance's handle. A null word faults there; retail
  has no answer for it. It then asks the Ghoul2 info array (`0x4de4b0`)
  whether the handle is valid (`[vtable+8]`). An invalid handle clears
  nothing.
- For a valid handle it walks the instance's models, 0xc8 bytes each (the
  count is `(end - begin) / 200` by the `0x51eb851f` multiply). For each
  model whose gore-set word at `+0x4c` is set it calls `0x4e9310` on that
  word and then zeroes it. `0x4e9310` finds the set in the gore-record map
  (`0x12a5048`) and drops one reference or erases it.
- Nothing else in the instance changes: models, skins, bolts and bone
  state stay as they were.
- Imports 313 (`CG_G2_GETNUMGOREMARKS`, `0x413c9d` to `0x4e07a0`) and 314
  (`CG_G2_ADDSKINGORE`, `0x413cb6` to `0x4e08f0`) read and fill the same
  gore-record map.

### Observed, `cgamex86.dll` 1.01

- The client game's wrapper for import 315 is `0x3005f590`: it pushes its
  one argument and 315 (`0x3005f595`) and calls the engine. It has one
  caller, `0x3005d938`, which passes the entity's Ghoul2 word
  (`cent + 0x21c`) by value.
- Just before it, the same routine resets the ragdoll through import 328
  (`0x3005d929`) only when the entity is ragging (`cent + 0x630`), and the
  body-queue copy (`0x3005ce80`) runs first.
- The wrappers for 313 (`0x3005f550`) and 314 (`0x3005f570`) each have one
  caller, `0x3004c5a0` and `0x3004c6e4`.

### Inferred, from OpenJK

- The routine at `0x3005d938` is OpenJK's `CG_RestoreClientGhoul_f`,
  registered for both `ircg` and `rcg`. It returns early when the
  entity has no Ghoul2 instance, so the retail client game never passes a
  null word.
- The server's `respawn` sends `ircg` from `CopyToBodyQue` and `rcg` when no
  body is copied, so every respawn reaches import 315 on every client.

## fnTech3

### Before

fnTech3's client keeps the Academy client game's models in its own Ghoul2
store (`client_jka_cgame_ghoul2.hpp`). That store holds no skin gore at
all: no model record has a gore-set field, and imports 313 and 314 are not
served. Import 315 was refused by number.

### Now

- Import 315 is served in the client game's Ghoul2 dispatcher. It requires
  the x86 legacy native client game, as the other model imports do.
- A null word is refused with an error, where retail would fault.
- Any other word must name an instance this store owns, or it is refused as
  stale or foreign. Before any model loads there is no store, and the clear
  does not create one.
- With no gore kept, validation is the whole clear: the import answers 0
  and leaves the instance, its models and their skins as they were. When
  313 and 314 are served, the clear must also delete what 314 adds.

## Evidence

### Unit

`tests/jka_cgame_clear_skin_gore_tests.py` compiles the production header
beside the client-game Ghoul2 fixture of `jka_cgame_ghoul2_tests.py` and
runs import 315 through the production dispatcher:

- Before any model loads, a null word and an unknown word are refused and
  no store is made.
- After two models load with skins 5 and 6, a non-native client game is
  refused. The clear answers 0 twice and leaves the same instance, both
  models and both skins. `CG_G2_HAVEWEGHOULMODELS` still answers 1.
- A null word, another store's instance, an instance its own clean released
  and one after the owner is cleared are each refused.
- Imports 313 and 314 stay unserved.
- Seven mutations of the production case each fail at run time: no null
  refusal, no identity check, no native-module gate, a nonzero answer, the
  store made before the check, models released by the clear, and the case
  removed. A second test pins the retail addresses in the case's comment.

### Live

Each probe ran the client in an 800x600 window on a private desktop with
`net_enabled 0`, sound off, no input and engine screenshots only, through
`.tmp/round-2026-10-15/jka-local-sets/probe_live.py`. The retail
installation and the binaries were unchanged after every run.

The first pair was built from a `git archive` export of `d17f578a`, without
and with this slice, Windows x86 debugoptimized with strict warnings. On
`mp/ffa3` (`g_gametype 0`) a console `kill` does nothing at first: the
local player joins as a spectator behind the player-configuration menu the
server's `spc` opens. With `team free` first, `g_forcerespawn 1` and two
`kill`s, the before runtime stopped at import 315 after the first
`ircg 0 32 4 1`, and the after runtime served both respawns
(`cleargore-*-ffa3-teamkill`, `cleargore-*-ffa3-view`).

HEAD then gained the [pass-entity slice](2026-10-16-jka-trace-pass-none.md),
which lets a bot play. The export was advanced to `2e2207e0` path for path
and both runtimes rebuilt there:

| Run | Runtime | Console steps | Outcome |
| --- | --- | --- | --- |
| `cleargore2-before-ffa3-view` | before | `mp/ffa3`: `g_forcerespawn 1`, `team free`, `togglemenu`, `kill`, `kill` | `ircg 0 32 4 1`, then "Unsupported jka-mp cgame import 315" |
| `cleargore2-after-ffa3-view` | after | the same | `ircg 0 32 4 1` and `ircg 0 33 4 1` both served; the probe ends normally |
| `cleargore2-before-korriban-bot` | before | `mp/siege_korriban` (`g_gametype 7`): `team red`, `addbot Tavion 1 blue` | the round begins; Tavion dies, `ircg 1 32 0 0`, then import 315 |
| `cleargore2-after-korriban-bot` | after | the same | Tavion dies three times; `ircg 1 32 0 0`, `rcg 1`, `ircg 1 33 0 0` and `rcg 1` are all served; the client stops later at import 321 |

The `cleargore2-after-ffa3-view` captures show the respawned player in play
after two suicides: score -2, health 100, the second `ircg` on the console.

### Beyond it

- **Siege with a bot.** On `mp/siege_korriban`, some frames after the
  bot's third death and its `rcg 1`, the client stops at client-game import
  321, `CG_G2_SETSURFACEONOFF`. That is the next refusal on this path. The
  retail client game's wrapper for it (`0x3005f610`) has eleven call sites;
  which one ran was not identified.
- **A free-for-all bot.** With `addbot Tavion 4` on `mp/ffa3`
  (`cleargore2-after-ffa3-tavion`) the probe played out, but the bot never
  reached the player in its 80 seconds.
- **NPC fire.** `npc spawn stormtrooper` stops the client at client-game
  import 299, `CG_G2_COLLISIONDETECT`, before any death
  (`cleargore2-after-ffa3-npc`, and `cleargore-after-ffa3-npc` on the first
  pair).
- **Gore itself.** Imports 313 and 314 were not reached in these runs.
  Inferred from OpenJK: the client game adds gore through 314 when a hit
  lands on a player's model. Serving it will give this clear something to
  delete.

## Validation

- The `d17f578a` export with this slice built all targets with strict
  warnings and no warning (client configuration; the dedicated server is
  not built there and this slice does not touch it). Its native suite
  passed 274 of 276 tests and skipped two (OpenAL loopback and the optional
  Quake Live factory probe).
- On that export, 71 Python test files passed one file to a process, 248
  tests and 1,287 subtests with one skipped: every `jka_*` file, and the
  changelog, generated-docs, release-packaging, release-workflow, version
  and player-manual files.
- Advanced to `2e2207e0` path for path with this slice, the export rebuilt
  with no warning. Its native suite again passed 274 of 276 with the same
  two skipped, and the same 71 Python files passed with the same counts.
- Advanced again to `35e01aa1`, the light-style slice, which touches none
  of this slice's files except the shared ledger and docs index, the
  export rebuilt with no warning, and the native suite and the 71 files
  gave the same results. This commit sits on `35e01aa1`.

## Remaining and unrelated findings

- Client-game import 321 (`CG_G2_SETSURFACEONOFF`) stops a siege match with
  a bot after the bot's third death.
- Client-game import 299 (`CG_G2_COLLISIONDETECT`) stops the client as soon
  as a spawned stormtrooper engages.
- Client-game imports 313 and 314 (skin gore) are not served.
- Joining a free-for-all from the console needs `team free`: the local
  player starts as a spectator behind the player-configuration menu, and
  `kill` does nothing until then. This is the game's own behaviour, not a
  fault.
- `addbot Kyle` reports "Bot 'Kyle' not defined"; Academy's bot names are
  its own (`Tavion` works).
