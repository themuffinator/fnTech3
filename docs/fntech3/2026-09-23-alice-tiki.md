# Alice's model definitions at run time: TIKI, poses and tags

## Scope

This record covers what American McGee's Alice does with its model
definitions once they are loaded: the compiled `.cik` form the retail engine
actually reads, the `.tik` text rules it falls back to, the queries the game
module and the client game make of a definition, and the pose, skinning and
tag arithmetic behind them. It builds on the
[model record](2026-09-22-alice-models.md), which established the four file
formats, and on the [client-game record](2026-09-23-alice-cgame.md), which
named the import slots that reach this code.

Every fact is measured against the retail November 2000 `alice.exe`, image
base `0x00400000`, and the installation's own files. Observed facts are
marked by the address or file they come from; inferences are called
inferences. Nothing from Ritual's F.A.K.K.2 SDK or from any reconstruction is
incorporated as text.

## Correction: `.cik` is a compiled definition

The model record describes the 797 `.cik` files as a cinematic layer. They
are not. Each is version 1 followed by an image of the engine's own in-memory
definition record, and the retail loader prefers it to the text: with
`developer` 0, `0x0045c840` looks for the `.cik` beside a requested `.tik`
and loads it through `0x0045c600`, relinking its internal offsets at
`0x0045c550`, before it ever opens the text. After loading an image it
registers the skeleton and every animation again by the names stored inside
it and clears the alias list; the shader handles the image stores are stale
and the renderer registers them afresh.

A research compiler written against the text rules below reproduces all 797
images from their `.tik` sources byte for byte, which is what establishes
those rules; that compiler is investigation tooling and is not part of this
tree. Four of the 801 definitions ship no image. Alice's own image matches
`pak0`'s 206-animation `models/alice.tik`, not the older 167-animation copy
in `pak1_small`.

The image layout, offsets relative to the byte after the version:

| Offset | Field |
| --- | --- |
| `0x00` | name, 128 bytes |
| `0x80`, `0x84`, `0x88` | surface, tag and animation counts |
| `0x8c` | alias list |
| `0x90`/`0x94`, `0x98`/`0x9c` | client and server init command count and offset |
| `0xa0` | surfaces offset |
| `0xa4`, `0xa8`, `0xac` | scale, LOD scale, LOD bias |
| `0xb0`, `0xbc` | light offset, origin |
| `0xc8`, `0xd4` | bounds |
| `0xe0` | radius |
| `0xe4`, `0xe8` | skeleton handle, skeleton name (128 bytes) |
| `0x168` | animation offset table |

A surface record is `0x15c` bytes: name (64), four shader names (64 each),
four shader handles at `+0x140`, skin count at `+0x150`, flags at `+0x154`,
damage multiplier at `+0x158`. An animation record is `0xd0` bytes: alias
(48), full path (128) at `+0x30`, weight at `+0xb0`, crossblend time at
`+0xb4`, animation handle at `+0xb8`, flags at `+0xbc`, and the client and
server frame-command counts and offsets from `+0xc0` to `+0xcc`. A command is
`0x88` bytes: frame, argument count and 32 argument offsets. Every offset is
relative to the record that holds it.

## The text rules

The tokenizer at `0x00465xxx` differs from Quake III's in ways the corpus
depends on:

- An unquoted token ends only at whitespace or a comment; braces are not
  delimiters, so `{`` ` is one token. Quoted strings take `\n`, `\'`, `\"`
  and `\\` and cannot cross a line. Tokens are at most 256 bytes. `;`, `#`
  and `//` start line comments.
- **A block comment ends only at `*` followed by a backslash**
  (`0x00465444`-`0x0046544c`), not at `*/`. `c_boojum_emitters.tik` therefore
  swallows its animations and fails to load, and ships no image.
- `$define name value` declares a macro for the file and every file that
  includes it, up to 48; `$include` passes the defines down. A token's
  `pre$name$post` expands once, case-insensitively; `$$` is a dollar and an
  unknown name is left as written.
- String comparison (`0x0044be40`, `0x0044bda0`) folds `a`-`z` to upper case
  over signed characters, which sets the collation animations sort by.

A file (`0x0045bcd0`) must start with `TIKI`. `setup` (`0x0045b3d0`) takes
`scale` (non-positive means 1), `lod_scale` (stored times 0.2), `lod_bias`,
`skelmodel` (a failure fails the load), `path` (a slash appended unless one
ends it), `origin`, `lightoffset` (stored only), `radius` (explicit, not
scaled) and `surface NAME` followed by `flags` (`skin1` 1, `skin2` 2, `skin3`
3, `nodraw` 4, `crossfade` 0x40, `nodamage` 0x80, `nomipmaps` 0x100,
`nopicmip` 0x200), `damage` and `shader` (up to four skins; a name containing
a dot is a path under `path`). Binding (`0x00459d40`, `0x00459ed0`) walks the
model's surfaces in file order: a plain name binds the first surface of that
name, a name ending in `*` binds every surface it prefixes, and `all` binds
every surface. A surface that never receives a shader keeps an empty name --
215 do in the corpus -- and cannot be found by name.

`init` holds `client` and `server` command lists, up to 160 a side with 13
arguments each; the engine interprets none of them. `animations`
(`0x0045afb0` onwards) takes `alias file [flags]` with an optional command
block. A file that fails to register skips its entry and block (49 do). At
most 511 animations; flags are `weight`, `deltadriven` 0x01,
`default_angles` 0x08, `notimecheck` 0x10, Alice's own `torso_adjust` 0x20,
and `crossblend` in milliseconds (default 200). A command block line is
`FRAME command args`, where `start`/`first` is 0, `end`/`last` the last
frame, `every` -1, `exit` -2 and `entry`/`enter` -3; `(` and `)` reuse the
previous frame and are pushed back as commands of their own. Frames at or
below -4 or past the end are refused. **For a version 2 skeletal animation
the frame count used for that check is read where version 3 keeps it, which
is the bone count** (`0x0045ad44`); retail's validation is wrong there and
the runtime reproduces it.

Animation indices are the sorted order under that collation, sorted by
Visual C++ 6's `qsort` (`0x004e8ae7`): selection of the maximum for eight
elements or fewer, a middle pivot otherwise, the smaller side first. The
order only matters where names tie, which happens once
(`c_insanechild-runner` has two `walk`), and there it matters.

## Queries

A handle out of range prints a message and returns zero, null, -1 or 0.0.
Per-animation values are read out of the animation file at query time: frame
count at `+76` (SKAN 3) or `+72` (SKAN 2 and TAN), total time at `+84`/`+80`,
frame time at `+88`/`+84` or per frame at `+0x40` for TAN, total delta at
`+92`/`+88`. A frame is `40 + 16 x bones` bytes at SKAN 3, `40 + 28 x bones`
at SKAN 2 and 68 at TAN; bounds are at `+0`/`+0xc`, the radius at `+0x18`
(SKAN) or `+0x3c` (TAN), the delta at `+0x1c` or `+0x30`.

- Model bounds are the union of every frame's bounds plus the setup origin,
  unscaled, replaced by +-4 when any extent exceeds 100,000. The radius is
  the setup radius when non-zero, else the longest bounds corner times the
  scale. `CalculateBounds` scales the bounds by the model scale and its own
  argument, origin included; `Frame_Bounds` does the same without the origin
  and leaves its outputs untouched on error.
- The total delta is read, not summed: 509 skeletal and 326 vertex
  animations store a total that is not the sum of their frames.
  `Anim_AbsoluteDelta` sums the absolute frame deltas.
- `Anim_Random` (`0x0045d180`) chooses among animations whose names start
  with the prefix and continue with anything but `_`, stopping at the first
  sorted name that compares greater, at most 16 candidates weighted by their
  weights, rolling `rand() x total / 32767`.
- `Frame_Commands` matches a command whose frame equals the request, or is
  `every` for a non-negative request, and gives up past 192 matches. The
  command list handed back is `{ int count; { int argc; char *argv[13]; }
  [192]; }`, 10,756 bytes, with argument pointers into the definition's own
  storage.
- The game's `NumSkins` slot always answers 0, and its `Alias_Clear` slot
  dumps the list rather than clearing it. The client game's `Surface_Flags`
  slot checks the surface index and then always answers the first surface's
  flags; the damage-multiplier query at `0x0045df80` reaches neither module.

Aliases (`0x00422850` to `0x00423b12`, implemented in
`code/qcommon/alice_alias.cpp`) are per model and shared between server and
client, plus one engine list named `Global`. Observed:

- **Layout.** A list is `0x30` bytes: name[32] (strncpy'd, so unterminated
  at 32), dirty `+0x20`, count `+0x24`, sorted array `+0x28`, chain head
  `+0x2c`. A record is `0xc4`: name[32] lower case at most 31, real name[128]
  at most 127 at `+0x20`, weight `+0xa0` (1), global byte `+0xa4`, stop byte
  `+0xa5`, timeout `+0xa8`, maxuse `+0xac` (-1), times played `+0xb0`, played
  byte `+0xb4`, last played `+0xb8`, actor chain `+0xbc`, next `+0xc0`. An
  actor record is `0x14`: entity number, times played, played byte, last
  played, next. Game import 85 (`0x00454090` -> `0x0045cdb0`) returns the
  model's list pointer itself, and the game's savegame code
  (`fgamex86.dll` `0x100a3cb0`) walks all 512 model indices' chains reading
  those fields; on load it restores them through imports 86 (`0x00454020`,
  record) and 87 (`0x00454050`, actor).
- **Parameters** (`0x00422a60`, the engine tokenizer `0x0044b640`, keywords
  by `_stricmp`): only `global`, `stop`, `timeout F`, `maxuse N` and
  `weight F`; values must exceed 0 (`Com_Printf` warning otherwise); any other
  word prints `Unknown parameter for %s alias` and parsing continues with the
  next word; a missing value ends the string. No shipped `.tik` or script
  passes parameters (a read-only scan of the paks found none).
- **Duplicates.** Before the list is next sorted, a stored name that begins
  the new name (case-sensitive against the name as given) refuses it
  silently. After a sort, a line the new name prefixes refuses it. The sort
  (`0x00422e30`, selection over the chain, strncmp over the candidate's own
  length) drops a line whose name the previously kept line's name begins,
  keeping the newest of equal names; a longer name taken before its prefix
  survives. Duplicate reports are `Com_DPrintf`.
- **Lookup** (`0x00423770`): binary search for one line the name prefixes,
  then down and up over the lines it prefixes, accepting those whose next
  character is not `_` until 64; the run's ends start at the found line and
  every caller walks the whole run. `FindRandom` (`0x004236c0`) draws
  `(rand() & 0x7fff) / 32768 x total` and walks running weights.
- **Dialog** (`0x004232f0`): history is the record's for actor -1 or a
  `global` line, else per actor (counting creates the actor records). With
  nothing unplayed, lines reset unless `stop`, unless `timeout` seconds of
  `Sys_Milliseconds` have not been exceeded, or unless played `maxuse` times.
  In order, the first unplayed line answers; at random the pick is
  `(int)(fraction x available + 0.5)`, null when it equals `available`.
- `Dump` (`0x00423990`) prints only under `developer`, ending with
  `%d total aliases`; the client registers it as `aliasdump` (`0x0042cca5`).

## Poses

A skeleton bone is a parent, a flags word (1 marks a leg bone) and a 64-byte
name; a vertex carries a normal, texture coordinates and weights of bone,
weight and a bone-local offset. There is no bind pose.

- **Decoding** (`0x0045f5bb`, `0x004a3a10`): at version 3 a quaternion
  component is a short over 32767 and an offset component a short over 63,
  with one pad short; version 2 stores floats.
- **Matrices** (`0x0044a5b0`, `0x0045e2a0`) are row-major with the rows as
  axes; a local vector maps to model space as `v0 x row0 + v1 x row1 + v2 x
  row2` (`0x0044a130`).
- **The hierarchy** is walked in array order, parents first:
  `Q = Q_local (x) Q_parent` (Hamilton product over x, y, z, w), and
  `origin = origin_parent + M_parent^T x offset`.
- **Controllers** are five 36-byte records: bone, Euler angles, quaternion at
  `+0x10`, scale at `+0x20`. A controller on a bone post-multiplies its
  rotation, turning the bone and its subtree about the bone's own origin, and
  its scale multiplies down the chain. The renderer applies one controller a
  bone, the last, and only when the entity asks for controllers; the tag code
  applies every matching controller regardless.
- **Interpolation**: the old frame is weighted by the back-lerp. A crossblend
  slerps its frame against the lerped pose by its weight, offsets mixed
  linearly; a negative weight means none. The slerp (`0x0044a690`) takes the
  short path, goes linear when nearly equal and does not renormalise.
- **Legs and torso**: with both enabled the legs animation drives the
  flag-1 bones and the torso animation the rest; with one enabled it drives
  every bone; with neither the renderer refuses the entity.
- **Skinning** (`0x0049ffe0`): a vertex is the weighted sum of
  `M_bone^T x offset + origin_bone`, times the entity scale and the model
  scale. The normal takes the first weight's bone only and is not
  renormalised.

This arithmetic reproduces the bounds the animation files store: over 2,517
poses on 66 skeletons the median error is 0.067 units at version 3 and
0.0001 at version 2, where parent-first composition or an untransposed matrix
misses by 15 and 31 units. 3,253,939 of the corpus's 3,254,001 stored
rotations are unit quaternions; the exceptions sit in fifteen clockwork
animations.

**Tags.** `Tag_Orientation` (`0x0045f440`) poses only the chain from the tag
to the root and, on a skeletal model, ignores its scale argument; the origin
is scaled by the controller scale and the model scale, and the model origin
is added at the root, which the renderer does not do (it uses the origin for
culling). `Tag_OrientationEx` (`0x0045f1b0` into `0x0045e3c0`) takes sixteen
arguments including crossblend and legs/torso and does use its scale.
`Tag_LerpedOrientation` reads the entity record directly: animation `+0x54`,
old animation `+0x58`, frames `+0x5c`/`+0x5e`, scale `+0x60`, torso fields
`+0x68`-`+0x72` and `+0x88`-`+0x94`, crossblend `+0x74`/`+0x7c`/`+0x80`, the
two use flags `+0x84` and `+0x98`, back-lerps `+0xa8`/`+0xac`, controllers
`+0xf4`. An error returns an all-zero orientation from the first two and
identity axes from the third.

**Vertex models** (TAN): a frame is 68 bytes -- bounds, scale at `0x18`,
offset at `0x24`, delta at `0x30`, radius at `0x3c`, frame time at `0x40` --
and a vertex is three unsigned shorts and a packed normal. A position
(`0x0049f950`) is the shorts times the frame scale, blended between the
current and old frame (which may come from different animations), plus the
blended offsets and the model origin, all times the entity and model scale.
The shorts times the frame scale plus its offset land inside the frame's
stored bounds with a median error of 0.0013 units.

## Names the paks do not hold

Three model names the game asks for are not in any pak, and a developer
client on `gvillage` (and `garden1`, for the sprites) reported each of them.
All three are content, not engine, faults; what differed was how fnTech3
answered them. A fourth report, about an image two shaders share, is taken
last.

- **`models/smoke.tik`.** `gvillage` and `pandemonium` place `smoke1`
  entities with `"model" "smoke.tik"`, and no pak has a `smoke.tik` or
  `smoke.cik` in any directory (only `smokefire1` and `smoketrail1`;
  `w_demondice.tik`'s `cache smoke.tik` is commented out). The retail game
  module says so itself, "Classname smoke1 used, but TIKI was not valid,
  using Object." Retail's engine answers the missing definition with a
  developer's note from its definition loader, `Tiki:LoadFile Couldn't load
  %s` (`TIKI_LoadTIKIfile` 0x0045bcd0 calls the script loader with its quiet
  flag clear at 0x0045bcf7; the note is `Com_DPrintf` at 0x0046637d, and only
  for a file that is not there, `length < 0`), and its renderer, whose model
  slot then stays empty, says nothing (`R_RegisterModel` 0x0048982e ..
  0x00489ee7). fnTech3's loader was silent and its renderers warned
  `R_LoadAliceTiki: %s did not load`; now the loader makes retail's note and
  the renderers' line is developer-only.
- **`targeted_outofrange.spr`** (client game) and **`crosshair.spr`** (server
  game) name shaders no shipped script defines and images no pak holds.
  `targeted_outofrange` survives only in editor leftovers the shader loader
  never reads (`scripts/sprites.shader~`, `scripts/#sprites.shader#`,
  `scripts/old_shader_bits.txt`, all mapping a `sprites/targeted1.tga` that is
  not shipped either); `crosshair` is nowhere. Of the 109 sprite names the two
  modules and the definitions' uncommented lines use, 98 have a script, none
  has an image alone, and 11 have neither (the other nine are definitions'
  blood, fire and mist sprites). Retail's sprite loader (0x004afb30) strips the
  extension and asks `R_FindShader(name, -1, 0, 0, 0)` -- no mipmaps, no
  picmip, clamped (0x004afb4d..0x004afb57). Its `R_FindShader`, finding
  neither script nor image (the image lookup tries `.ftx` before `.tga`,
  0x00483f40..0x0048400b), notes "Couldn't find image for shader %s" at
  `PRINT_WARNING` (0x0048b2e4), which retail's `CL_RefPrintf` sends to
  `Com_DPrintf` (0x0042c31b), and then hands back the default shader itself
  (`FinishShader` 0x0048e3ed..0x0048e410). So the sprite takes the 16x16
  default image's size and registers. Its own "Could not find image for
  sprite in shader %s" is `ri.Error(ERR_DROP)` (0x004afbc1; the import at
  0x00e9d6c4 is `Com_Error`, filled at 0x0042c4e9) and is never reached this
  way. fnTech3's `R_FindShader` returned a stageless default shader instead,
  so its sprite loader printed that message as a warning and registered no
  model. Now it takes the default shader when it gets a default one, as
  retail's does, and asks without mipmaps as retail does; no shipped sprite
  is an image without a script, so the second change draws nothing
  differently today.
- **`reused image ... with mixed flags`.** `garden1`'s
  `models/fx/wfall/wfall2.tga (3 vs 1)` and `gvillage`'s
  `textures/testliq/top3.tga (3 vs 1)` come from fnTech3's `r_picmipFilter`
  (default 1, only `textures/*` may be reduced by `r_picmip`): the image is
  first loaded by a `textures/...` shader and then by a shader with a bare
  name (`rogue_fall`, which `fx_waterfall_g1.tik` binds, and
  `torchgnome_ball`), which the filter marks `nopicmip`. Retail derives an image's picmip only
  from a stage's `nopicmip`/`nomipmaps` (`R_FindImageFile` arguments at
  0x0048b65e..0x0048b687), so it would load both the same way and say
  nothing. A probe of both maps with `r_picmipFilter 0` makes neither `3 vs
  1` report. At the default `r_picmip 0` the image is identical either way;
  the filter is a deliberate option shared by every profile, and the message
  is developer-only, so it is left. `gvillage`'s
  `textures/effects/swirlie_02.tga (0 vs 1)` is the content's own:
  `textures/effects/starshootertube` loads it with `nomipmaps` and
  `nopicmip`, `torchgnome_ball` with mipmaps (`0 vs 3` with the filter off),
  and retail would report that pair too, as "mixed mipmap parm" and "mixed
  allowPicmip parm", both at
  `PRINT_DEVELOPER` (0x00484665..0x0048468f). Like id's, both engines keep
  the image as first loaded.

Probe, windowed, isolated home, `developer 1`, `+devmap gvillage`: before,
`R_LoadAliceTiki: models/smoke.tik did not load` and two `Could not find image
for sprite in shader ...` warnings; after, none of the three, with five
developer notes `Tiki:LoadFile Couldn't load models/smoke.tik` (one per
registration, as retail makes them) and the existing developer lines `Shader
image unavailable for: targeted_outofrange` and `... crosshair`.
`tests/alice_missing_content_tests.py` pins both answers in all three
renderers and the loader.

## The renderer's checks of an entity's animation state (2026-09-24)

### Observed

`R_AddEntitySurfaces` switches on the model record's type (`+0x40`, jump
table at `0x00478e81`) and calls `R_AddTikiSurfaces` (`0x004a6320`) from
`0x00478eb3`, its only caller. When the definition has a skeleton handle
(`+0xe4` not negative) it hands the entity at `0x004a6390` to
`R_AddSkelSurfaces` (`0x004a3a10`), which passes a version 2 skeleton on to
`0x004a2420` at `0x004a3ac5`. Both check every animation and frame number the
entity names before anything reads them, and write their corrections into the
renderer's own copy of the entity.

For a vertex-animated model, in `R_AddTikiSurfaces`, all before its culling
calls at `0x004a6565` and `0x004a658b` (the entity's animation at `+0x54`, old
animation `+0x58`, old frame `+0x5c`, frame `+0x5e`, the last two shorts):

| Address | Behaviour |
| --- | --- |
| `0x004a63a0..0x004a63c8` | Animation not in `[0, count)`, the count at definition `+0x88`: `R_AddTikiSurfaces: no such anim %d for '%s'\n` (`0x00528f68`); animation and frame set to 0 |
| `0x004a63df..0x004a640d` | No file for the animation (`0x00458e90`): `R_AddTikiSurfaces: couldn't get anim %d in '%s'\n` (`0x00528f34`), and the entity is not drawn |
| `0x004a640e..0x004a6437` | Frame not in `[0, frames)`, the count at the TAN header's `+0x48`: `R_AddTikiSurfaces: no such frame %d in anim %d for '%s'\n` (`0x00528ef8`); frame set to 0 (`mov word ptr [esi+0x5e], 0` at `0x004a6437`) |
| `0x004a6455..0x004a647e` | Old animation out of range: `R_AddTikiSurfaces: no such oldanim %d for '%s'\n` (`0x00528ec8`), printed with the **current** animation's number (`[esi+0x54]` at `0x004a6464`); old animation and old frame set to 0 |
| `0x004a6499..0x004a64b2` | No file for the old animation: the couldn't-get message with its number, and the entity is not drawn |
| `0x004a64b4..0x004a64dd` | Old frame out of its animation's range: `R_AddTikiSurfaces: no such oldframe %d in anim %d for '%s'\n` (`0x00528e8c`); old frame set to 0 |

For a skeletal model each pair goes through `0x004a1f60` (`0x004a2110` for a
version 2 skeleton), which does the same with `R_AddSkelSurfaces: no such
anim %d for '%s'\n` (`0x00528da8`, animation and frame set to 0),
`R_AddSkelSurfaces: couldn't get anim %d in '%s'\n` (`0x00528d74`, null
returned) and `R_AddSkelSurfaces: no such frame %d in anim %d for '%s'\n`
(`0x00528d38`, frame set to 0). The frame count is read at `+0x4c` in the
version 3 path and `+0x48` in the version 2 path, where each version of
`SKAN` keeps it; the choice follows the skeleton's version, not the
animation's. The pairs, in `0x004a3a10`'s order (the version 2 path calls the
same pairs at `0x004a2559`, `0x004a2579`, `0x004a25d1`, `0x004a25f8`,
`0x004a261a` and `0x004a288b`):

- with the legs in use (`+0x84`): the animation and frame at `0x004a3b74`,
  where a null ends the draw, and the old ones at `0x004a3b94`; then the
  entity is culled on the legs' frames (`0x004a60e0` at `0x004a3ba1`);
- with the torso in use (`+0x98`): its animation and frame at `0x004a3be7`
  (a null ends the draw), its old ones at `0x004a3c07`, and its cross-blend
  pair (`+0x90`, `+0x94`) at `0x004a3c20` whatever its weight;
- after the level of detail, with the legs in use and a cross-blend weight
  (`+0x74`) not below 0.0 (`0x004a3e82` against `0x004ff3a8`): the legs'
  cross-blend pair (`+0x7c`, `+0x80`) at `0x004a3ea6`.

Every message goes through the renderer's print import at level 1. The slot
at `0x00e9d6c0` is the first of the 39 words `GetRefAPI` (`0x00476280`)
copies from the client's table, where `CL_InitRef` stores `0x0042c2d0`
(`0x0042c4e1`). That function hands level 1 to `0x00438a40`, which prints only
while the cvar at `0x00bd1c58` is non-zero; that cvar is `developer`,
registered at `0x004398cd`. The corrections are made whatever `developer` is.
The `'%s'` is the model record `0x004893f0` returns (records `0x70` bytes
apart, the type at `+0x40`); that its name comes first, as in Quake III's
`model_t`, is an inference.

A scan of the 797 images finds all 2,211 skeletal animations they name in the
same version as their skeleton, so for the shipped models reading an
animation's own count is reading the one retail reads.

### What fnTech3 does

`RA_TikiCheckAnimation` in `code/renderercommon/tr_alice_tiki.cpp` makes these
checks, corrections and messages, at `PRINT_DEVELOPER` in retail's words, and
`R_AddAliceTikiSurfaces` calls it first in all three renderers. The level of
detail, the lighting point, the pose and the vertex-animated tessellator read
frame 0 for a frame out of range, as the corrected entity would; they used to
clamp to the last frame. `tests/alice_tiki_render_tests.cpp` pins each message,
each correction and those reads. With `developer 1` on `gvillage`, GL, Vulkan
and RTX printed no correction: the retail client game named animations and
frames in range there.

Two differences remain. This renderer culls on the model's bounds rather than
the legs' frames, so it makes every check first, and only which messages a
culled skeletal entity prints can differ from retail. An image can name an
animation whose file the installation lacks (the text loader drops such an
animation); retail's renderer then draws nothing, while this one checks no
further and keeps its drawing as before. That case is open.

## Skeletal surfaces are indexed, not walked (2026-09-24)

A skeleton's surfaces form a chain, each giving the offset of the next. The
renderer found surface `n` by walking the chain from the first on every call:
once per surface when a model registers, and once per surface every time a
skeletal surface is tessellated. The engine's full reader, `ParseSkb`, already
locates every surface when the file is read and refuses a skeleton whose
chain does not tile the file exactly, so the engine now keeps its records
(`File::skbSurfaces`, `code/qcommon/alice_tiki.cpp` `LoadFile`) and the
renderer indexes them.

For the 79 skeletons in `pak0`, `pak1_large`, `pak1_small`, `Pak2` and `Pak3`
(278 surfaces, none refused by the reader), the records answer every index
from -1 to one past the last exactly as the walk did: counts, triangle,
vertex and collapse-map offsets, and the lowest level of detail
(`tests/alice_tiki_render_tests.cpp`, which reads the installation when
present and copies nothing). At run time, with `fixedtime 16`, `gvillage`'s
capture 14 seconds in, Alice's skeletal model in view, was pixel-identical
in three GL runs of the build before the change and three after it, and in
the RTX pair; a fourth GL run before, and the Vulkan pair, differed only around
Alice's model (a whole-frame mean of 0.18 per channel). Alice's runs are not
repeatable even so: the capture at 12 seconds varied between runs of one
build by a mean of 0.2 to 2.9 per channel.

## Surface mipmaps and picmip (2026-09-24)

Retail's model loader (`0x0048989a..0x004898be`) turns a surface's flags
(`+0x154`) into two separate arguments to its `R_FindShader` (`0x0048afe0`):
mipmaps unless `nomipmaps` (0x100), picmip unless `nopicmip` (0x200), and a
fifth argument of 1. Only the path for a shader with no script reads the
three: at `0x0048b2ad..0x0048b2d8` it passes them to the image lookup
(`0x00484550`), the last as wrapping `0x2900 + ( argument != 0 )`, that is
`GL_CLAMP` or `GL_REPEAT`. A model surface's image therefore always repeats,
and its two flags act apart; the sprite loader asks for none of the three
(`0x004afb4d..0x004afb57`).

fnTech3 passed only `nomipmaps`, through Quake III's single `mipRawImage`: it
ignored `nopicmip`, and `nomipmaps` also turned picmip off and clamped the
image. `R_FindAliceSurfaceShader` in each renderer's `tr_shader.c` now takes
the two flags apart and leaves the image repeating; a shader with a script is
unaffected, as in retail. No shipped model uses either flag: all 1,259
surfaces in the 797 images have flags 0, and no `.tik` in the installation
has a `surface ... flags` line.

## What fnTech3 implements

One runtime per process, as retail keeps one cache for server, client and
renderer: `code/qcommon/alice_tiki.cpp` loads the image when there is one and
compiles the text by the rules above when there is not, holds retail's cache
limits, and serves every query the two modules call, the quirks above
included. The pose arithmetic is `code/compat/alice_tiki_pose.hpp`, shared by
the tag queries and by the OpenGL renderer's skinning in
`code/renderercommon/tr_alice_tiki.cpp` and `code/renderer/tr_alice.c`. The
TAN frame size of 68 bytes corrects the 64 the vertex reader carried.

Not implemented: LOD (the fraction retail computes from `r_lodscale`, the
model's LOD scale and bias and the projected radius, with a collapse map),
crossfade surfaces, custom skins, `r_showskel`, and the Vulkan and RTX
renderers' drawing of either model kind. No developer-mode reload of changed
`.tik` files is offered. Because the image is preferred, `pak0`'s
`models/alice.cik` (139,701 bytes) decides Alice's own definition although
`pak1_small` carries a later-sorting 30,216-byte `models/alice.tik`; only
`pak0` carries the image.

**Correction (2026-09-24).** The two paragraphs above predate the renderers'
side. All three renderers now draw both model kinds through the shared
`code/renderercommon/tr_alice_tiki.cpp`, with retail's level of detail
(`code/compat/alice_tiki_lod.hpp`), crossfading skins and custom skins, and
`tests/alice_renderer_parity_tests.py` holds the Vulkan and RTX front ends to
GL's text. GL, Vulkan and RTX captures of `garden1` and `gvillage` show
Alice's model in each. `r_showskel` and a developer-mode reload are still not
implemented.
