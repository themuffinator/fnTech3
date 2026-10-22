# Jedi Academy's Ghoul2 surfaces, server skins and model removal, 2026-10-22

The [Ghoul2 collision slice](2026-10-22-jka-g2-collision.md) left the
server's models without a surface override list: game import 609 (switch a
surface on or off) and 612 (a surface's render status) stopped the server,
a server skin named surfaces but hid none, and the collision walk read every
surface's file flags. Model removal stopped both sides: game imports 603 and
604, and client-game import 311, which ended siege rounds on
`mp/siege_hoth` and `mp/siege_desert` in the
[skinless-model slice](2026-10-16-jka-skinless-model.md). This slice serves
all five for Jedi Academy multiplayer (`jka-mp`), applies a server skin's
visibility as retail does, and lets an init fill the place a removal empties.
The [JSON companion](2026-10-22-jka-g2-surfaces.json) records the binaries,
retail identities, commands and per-case outcomes. No compatibility gate
closes.

Implementation is independently authored. Retail `jamp.exe` 1.01 (SHA256
`e8062d39...`) and the 1.01 `jampgamex86.dll` and `cgamex86.dll` in
`assets3.pk3` were read with capstone and pefile. Pinned OpenJK `1a6a643`
(`codemp/rd-vanilla/G2_API.cpp`, `G2_surfaces.cpp`, `tr_skin.cpp`) only
named the routines (reference observation; no text incorporated).

## Observed, retail `jamp.exe` 1.01

### Handlers

| Import | Handler | Core |
| --- | --- | --- |
| game 609, `G_G2_SETSURFACEONOFF` | `0x45ced2` | `0x4deb70`, then `0x4ee700` |
| game 612, `G_G2_GETSURFACERENDERSTATUS` | `0x45cf26` | `0x4116e0`, `0x4dec00`, then `0x4eee70` |
| game 603, `G_G2_REMOVEGHOUL2MODEL` | `0x45cd9a` | `0x4decc0` |
| game 604, `G_G2_REMOVEGHOUL2MODELS` | `0x45cdb5` | `0x4deea0` |
| client game 311, `CG_G2_REMOVEGHOUL2MODEL` | `0x413c69` | `0x4decc0` |
| game 591, `G_G2_SETSKIN` | `0x45cbb2` | `0x4116e0`, `0x4deb50`, then `0x4ee7e0` |

609 hands the instance word, the mapped name and the flags to the core the
client game's 321 reaches ([surface-switch record](2026-10-16-jka-surface-on-off.md)):
model 0 only, 0 for a null word, an instance with no places or an empty
place 0. 612 picks the model's record without a bound (`0x4116e0`, which
reads through the instance word). 603 and 604 map the address of the
module's field for the instance and read the instance through it. The
client game's 311 (dispatcher `0x412170`, byte table `0x414638` index 187,
jump table `0x4142bc`) reaches the same removal core as 603.

### The override list

Each model record holds a list (`+0x4`) of 0x18-byte entries: flags, surface
number, four words. `0x4ee700` finds a listed surface by name from the
list's end (`0x4ee610`, `_stricmp` against the hierarchy name of the
level-0 surface the entry names), or an unlisted one in the file in order
(`0x4ee5b0`), takes only OFF (2) and NODESCENDANTS (0x100) from the flags
it is given (`0x4ee774`) and keeps the surface's other bits. An unlisted
surface is listed only when the result differs from its own flags. Either
way the answer is 1; a name the model lacks answers 0.

### A surface's render status

`0x4eee70` answers -1 for a name the model lacks. Otherwise it walks the
ancestors from the parent up (`+0x88`); each is looked up by its own name,
in the file (`0x4ee5b0`) and then the list (`0x4ee610`), and the first whose
flags carry NODESCENDANTS ends the walk with the surface's own file flags
plus OFF (`0x4eef36`). With none, a surface whose own file flags are not
zero answers them whatever its list entry says (`0x4eef47`); only a surface
whose file flags are zero answers its listed flags. `0x4dec00` answers -1
for an empty place, whose setup (`0x4e0b10`) fails.

### The server's mesh names

A dedicated server reads meshes with a loader of its own (`0x4a8be0`,
reached from `0x4a8d60`), which keeps every hierarchy name as the file
writes it. The client's loader (`0x49acd0`) lowercases them and drops a
trailing `_off` (`0x49adf0..0x49ae4f`). A model's setup (`0x4e0b2b`) takes
the server's loader when `dedicated` is set; in another process a further
test (`0x4de580`, not followed) chooses. 1,308 of the 5,854 surface names in
the 127 installed meshes end in `_off`; they are off in their files (flags
2). Measured below, the dedicated server knows Kyle's surface 50 as
`head_cap_torso_off` and not `head_cap_torso`.

### Server skins

Import 582 reaches `0x4a0650`, which on a dedicated server registers through
RE_RegisterSkin (`0x4a0370`) with server shaders:

- No name, or one of 64 bytes or more, answers 0. A name registered before,
  compared without ASCII case (`0x4446c0`), answers its handle while the
  skin has a surface and 0 otherwise. Otherwise the skin takes the next
  place (1023 at most) before any file is read, so a missing file still
  uses a handle.
- `path/|head|torso|lower` reads three files, `path/head.skin` and so on
  (`0x49ff90`); a later file is read only while the earlier ones left a
  surface. A name with fewer bars is one file.
- Each file (`0x4a0160`) is cut into tokens by `0x4a0540`: line and block
  remarks skipped, quoted tokens, a word ending at a byte at or below space
  (compared signed) or a comma, a word of 1024 bytes or more empty. Each
  surface name is cut to 63 bytes and lowercased, and a comma after it is
  passed over. A name beginning `tag_` is skipped before its shader is read.
  A name ending `_off` is skipped when its shader is `*off` and otherwise
  loses the `_off`. At 128 surfaces the file stops.
- 591's core (`0x4deb50`) keeps the skin word and, for a nonzero render
  skin, empties the model's list and applies the skin (`0x4ee7e0`): each
  surface whose shader is named `*off` is switched off; any other the model
  has is switched on unless its own flags are off (`0x4ee87d`). A render
  skin outside the table is skin 0 (`0x4ee7e3..0x4ee809`).

Of the 305 installed skins, 3,326 lines name an `_off` surface with a real
shader, which the server's untrimmed meshes never match, and 3,381 name one
with `*off`, which are skipped. Twelve (`jedi_*` default and siege skins)
run to more than 128 lines, but once those lines are skipped none keeps more
than 101 surfaces, so no installed skin reaches the cap.

### Model removal and the init

`0x4decc0` answers 0 for an index at or past the instance's places
(`0x4ded20`) or a place already empty (`+0x30` is -1, `0x4ded4e`).
Otherwise it frees the model's gore set and bone cache, empties its bone,
bolt and override lists and marks the place empty (`0x4ded6a..0x4dedf7`),
drops empty places at the end, and when none is left deletes the instance
and empties the module's field (`0x4dee5a..0x4dee87`); it answers 1. It
reads through a null field and does not check a negative index. `0x4deea0`
empties every place the same way, so it always deletes the instance. The
init (`0x4de5c0`) fills the first empty place (`0x4de690`) before it
appends one.

### Who calls them

The retail game module calls 612 from 22 sites (limbs, heads and vehicle
parts, by name), 609 from four (`head_hatchcover`, a toggleable-surface
update and a limb's surface and cap), 603 from two (a turret's model 0
before its replacement model) and 604 from one; 591 hands the same server
skin as skin and render skin (`0x200777ed`). The client game calls 311 from
14 sites, each after import 310 says the place holds a model (weapon and
saber models among them).

## Served

- `code/ghoul2/ghoul2_server.hpp`: each model keeps its file's surface flags
  and parents and a surface override list. `SetSurfaceOnOff` and
  `SurfaceRenderStatus` follow `0x4ee700` and `0x4eee70`, quirks included;
  the list keeps no entry equal to a surface's own flags, which no reader
  tells apart from retail's. The server skin registry is rewritten to read
  skins as `0x4a0370`/`0x4a0160` do, each surface kept with whether it is
  off, and `ApplySkinVisibility` follows `0x4ee7e0`. `Store::RemoveModel`
  and `RemoveModels` follow `0x4decc0` and `0x4deea0` into a field of either
  width, and a store may fill an instance's first empty place on init.
- `code/server/server_jka_game.hpp` serves 609, 612, 603 and 604, and 591
  applies a nonzero render skin. Academy's server store fills empty places
  (`server_raven_assets.hpp`); Outcast's does not change.
- `code/ghoul2/ghoul2_instance_collision.hpp`'s `StoredOverrides` hands a
  model's list to the collision walk; the server trace (import 28) and the
  collision imports 606 and 607 use it.
- `code/client/client_jka_cgame_ghoul2.hpp` serves 311 from the client
  game's store, which now fills empty places on init too; a removed model's
  visibility binding goes with it, and a deleted instance's bindings with
  that.
- Refused by name, where retail reads through or faults: a null or foreign
  instance or field, a negative model index (603, 311), a model index
  outside the instance (612), a null surface name, and a render skin the
  server never registered, which faulted retail's dedicated server
  (below).

### Presentation

Nothing here is drawn on the server. On the client, 311 changes which
models of an instance Fn's skeletal renderer draws; its render cache drops a
removed model's geometry, as it did for a cleaned instance.

## Verification

### Retail and fnTech3 side by side

A synthetic game module (`.tmp/jka-g2surface/g2surface-probe.cpp`, no
gameplay, SHA256 `37e4018f...`) ran on `mp/ffa1` in retail `jamp.exe`
(dedicated, private desktop) and in the dedicated server built from an
export of this slice's commit (`816aa758`). It
registers four skins first, then switches surfaces of Kyle's model 0 through
fifteen cases (case, flag bits, a parent's NODESCENDANTS, surfaces off in
their files, unknown, empty and null names, a null instance), applies Kyle's
menu, default and blue skins, a three-part skin, one with a missing part and
the 165- and 149-line `jedi_hf` and `jedi_hm` default skins, removes models
from instances of one to three places in every order, re-inits into the
places left, and copies (600) and duplicates (601) a model with overrides.
After each step it asks 612 for every surface by the name 629 gives it, and
606 casts thirteen rays at the instance.

| Compared | Retail and fnTech3 |
| --- | --- |
| 612 answers | 3,464, all equal |
| 609 answers | 24, all equal |
| 591 answers | 14, all equal |
| 603 and 604 answers | 13, all equal |
| 629 surface names | 494, all equal |
| 582 handles | equal: 1, 2, 1, 3 for blue, menu, blue again, red; 0 for missing files |
| fields, places and inits after removal | all equal but one, below |
| 606 calls | 156, hit counts equal; 163 records, integers equal, distances within 1.6e-5 units, normals within 2.2e-6 |

The one line that differs is the probe's report that after 604 deleted an
instance, the next init into the emptied field made an instance at the same
address: retail's heap reused it; fnTech3 never reuses an identity, by
design (`OpaqueHandleSequence`).

Among the answers: `TORSO` matches `torso`; a listed NODESCENDANTS on the
torso reports every descendant as its own flags plus 2; `*head_eyes`
(flags 1) answers 1 after being switched off, and `head_cap_torso_off`
answers 2 after being switched on, though the collision walk then hits it;
`head_cap_torso` is no surface on the dedicated server; the menu skin hides
the hips, legs, arms and hands and the walk stops hitting them; the
three-part skin applies its parts in turn; the 214- and 198-surface
`jedi_hf` and `jedi_hm` bodies answer alike for every surface with their
skins; a removed place answers -1 and is the one the next init fills; and
the overrides go with a copy and a duplicate, each its own from then on.

A further run with 591 handed render skin 999 ended retail's dedicated
server with an access violation (exit code `0xC0000005`) straight after
the call. The same step in fnTech3's server stops it with "Jedi Academy
G_G2_SETSKIN was handed render skin 999, which the server never
registered". The same comparison on the working tree's build before the
commit gave the same answers.

### Live, siege with a bot, no input

The [skinless-model slice](2026-10-16-jka-skinless-model.md)'s siege run
(its `.tmp/round-2026-10-15/jka-local-sets/probe_live.py`, copied to
`.tmp/jka-g2surface/live/` to record a runtime outside the repository;
windowed 800x600 on a private desktop, no network sockets, sound off, no
input) ran again with the client built from the same export: `g_gametype
7`, then `set
cl_menuBlur 0`, `team red` and `addbot Tavion 1 blue` from the probe's
config, three engine captures.

| Map | Before (skinless slice) | Now |
| --- | --- | --- |
| `mp/siege_hoth` | both players enter, then "Unsupported jka-mp cgame import 311 (CG_G2_REMOVEGHOUL2MODEL)" | both players enter; the client runs to the probe's end and quits normally |
| `mp/siege_desert` | the same stop | the same as `mp/siege_hoth` |

The captures show the siege objectives and team menu, which the probe does
not close; they prove only that the client went on drawing. No count of
import 311's calls was printed. The installation and the binaries were
unchanged after every run.

### Unit

- `tests/jka_g2_surface_tests.cpp` (Meson `fntech3_jka_g2_surface`): a mesh
  with Kyle's surface shape and two names that fold alike; the switch's
  bits, case and first match; every render-status rule; the skin loader's
  tokens, remarks, quotes, high bytes, long words, `tag_`, `_off`, 63-byte
  names, 128 surfaces, 1023 skins and three-part names; the skin's
  visibility; removal, trimming, deletion through pointer and four-byte
  fields, reuse and a store that does not reuse; copies; and the collision
  walk reading a model's list through `StoredOverrides`.
- `tests/raven_ghoul2_server_tests.cpp`: the skin registry's test reads as
  retail reads.
- `tests/jka_cgame_remove_model_tests.py` drives 311 through the production
  dispatcher in the client-game fixture; eight mutations each fail at run
  time (unserved, no native gate, an empty slot or a negative index
  answered, 1 for nothing removed, a binding or a deleted instance's
  bindings kept, an init that appends).
- `tests/jka_server_g2_surface_tests.py` pins the server cases to their
  retail handlers and refusals, the stores' options and the trace's lists.
- `tests/jka_module_refusal_tests.py` counts 167 served game imports.

## Validation

- **Build.** A `git archive` export of `816aa758`, configured as the Windows
  x86 developer build (debugoptimized, strict warnings, GL, Vulkan and RTX
  renderers), built all 1,548 targets with no warning.
- **Native tests.** 277 native tests (all but the entry that runs the whole
  Python suite): 267 passed and 2 were skipped as usual (OpenAL loopback,
  the optional Quake Live factory probe). Five timed out while the Python
  files below ran beside them and passed alone (`fntech3_jka_server_attachment`,
  `fntech3_jka_render_host`, `fntech3_jka_shader_files`,
  `fntech3_raven_bsp_renderer_dispatch`, `fnql_platform_port_safety_source`).
  Three, `fntech3_probe_jka_client`, `_openjk_peer` and `_openjk_demo`,
  failed only because their scripts ask git for the commit and an export
  is no repository; their 33 tests pass in the checkout.
- **Python tests.** One pytest process per file, in the export: the 84
  files that read the changed headers or the documents, every `jka_*` and
  `jk2_*` test, and the changelog, generated-docs, player-manual, release
  and version files, passed 233 tests and 1,451 subtests.
- The record, ledger, changelog and games page then gained the export
  runs' results; the changelog, generated-docs and player-manual files pass
  on that tree.

## Not served, and open

- Raven's `RF_DISINTEGRATE2` (renderfx 0x40000), which a fading corpse sets,
  is still refused by the render adapter: it needs a distance cut-away in
  Fn's skeletal renderer, and capture evidence for it.
- Client-game import 324 (`CG_G2_GETSURFACERENDERSTATUS`, three call sites)
  is not served. The client's surface names are the file's. Inferred, not
  measured: the client game's models load through retail's client loader,
  which lowercases them and drops `_off`, and 321, 343 and a client skin's
  visibility would then see those names.
- Ragdoll and IK (game 615, 622, 623; client game 328 to 338) and the menus'
  Ghoul2 imports remain refused.
- Which loader a listen server's models take in retail was not followed
  (`0x4de580`); fnTech3's server keeps its own store and answers as the
  dedicated server does.
