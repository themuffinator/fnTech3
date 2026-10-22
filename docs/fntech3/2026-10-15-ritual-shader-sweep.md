# Alice's and F.A.K.K.2's shader warnings across every map, 2026-10-15

Each of the 36 maps in the legitimate retail American McGee's Alice
installation and the 30 in the Heavy Metal: F.A.K.K.2 layout laid down from
its disc was loaded under its own profile, `alice` or `fakk2`, with
`developer 1`, in the GL and the Vulkan renderer, and every shader warning
the logs printed was traced to its source. The two titles share Ritual's
shader dialect (`code/renderercommon/tr_alice_shader.h`), and its parser met
no keyword it lacks. Two warnings came from the engine: the renderer looked
up Quake III's `sun`, which neither Ritual engine does, and a model's own
image skipped the `r_picmipFilter` rule a script naming the same image
obeyed, so the image cache reported mixed flags. Both are fixed for the two
Ritual profiles alone. Every other warning is the titles' own data, listed
below and left as the data has it.

The [JSON companion](2026-10-15-ritual-shader-sweep.json) records the builds,
the installations' hashes, the command lines, each sweep's outcomes and every
distinct warning with its map count. This is Source, Unit and Live subset
evidence; no gate closes. `alice.exe` never ran: the retail binaries were
read instead, by address, for each lookup the engine side of a warning
depends on. The plain F.A.K.K.2 1.02 `fakk2.exe` cannot start a server
without the disc ([F.A.K.K.2 contracts](2026-09-24-fakk2-contracts.md)), but
its client's own developer logs of all 30 maps, from joining fnTech3's
server on 2026-09-25, are compared below.

## Method

`scripts/probe_retail_map_loads.py --developer` ran each map in its own
private-desktop process and home: `map <name>`, six seconds (Ritual's waits
count milliseconds), `quit`. `scripts/scan_shader_warnings.py` then grouped
every shader line of the kept console logs by kind. Developer-only lines it
does not classify, the image cache's "reused image ... with mixed flags"
among them, were counted from the same logs.

Both runtimes were built from a `git archive` export of `fbde92d4`, the
"after" one with only this change's hunks added, under the shared developer
options with strict warnings (`/W4 /WX`). Only the three `tr_shader.c`
objects differ, and `fntech3.exe` is byte-identical in the two; the companion
holds their hashes. `fbde92d4` predates the Medal of Honor sweep's commit
`6bf845bc`, whose condition shares the `sun` line; in that export the hunk
added the Ritual condition alone, and the committed line names both. A fifth
sweep ran
the "after" runtime with `r_picmipFilter 0` to separate the filter's warnings
from the data's.

Every definition of every `scripts/*.shader` in both titles' paks was also
run through the real GL parser (the host of
`tests/alice_shader_parse_tests.py`): Alice's 2,162 and F.A.K.K.2's 1,347.

## Results

All 66 maps loaded in all five sweeps. The GL and Vulkan logs printed the same
shader warnings before the change and again after it; only which random idle
dialogue lines played differed.

| Warning | Maps | Source | Action |
| --- | --- | --- | --- |
| default shader for `sun` | 36 Alice, once per renderer start | The renderer's Quake III sun | No longer looked up |
| `reused image ... with mixed flags` | 720 of 800 lines (F.A.K.K.2 691, Alice 29) | A TIKI surface's raw image skipped `r_picmipFilter` | The filter now decides its picmip |
| `reused image ... with mixed flags`, `3 vs 1` | 77 lines | `r_picmipFilter` judges each shader by its own name, so an image under `textures/` that a model's script also names gets two flag sets | Left: the filter's rule under every profile; see Open |
| `reused image ... with mixed flags` | 3 lines | Scripts disagreeing on their own `nopicmip`, `nomipmaps` or `clampmap` | Data; retail warns as well |
| default shader for `crosshair` | 36 Alice | `fgamex86.dll` gives an entity it creates for the crosshair the model `crosshair.spr` (0x100f9ccb); no script or image | Data |
| default shader for `targeted_outofrange` | 36 Alice | `cgamex86.dll` registers `targeted_outofrange.spr` (0x30023ec7, 0x30023ed7); only `scripts/old_shader_bits.txt` defines it | Data |
| default shader for `textures/notexture` | 2 Alice | `funhouse` and `hatter1` register it from their shader lumps | Data |
| default shader for `beamshader` | 1 Alice (`hatter1`) | `cgamex86.dll`'s default beam shader name (0x30014028) | Data |
| default shader for `textures/sky/cliffsky` | 1 F.A.K.K.2 (`zoo`) | Named in `zoo.bsp`'s shader lump | Data |
| lightmap but no lightmap stage | 1-2 each | `vol_light`, `notexture`, `common/black`, `otto/ottoscrew` | Quake III's developer note, which both retail engines print |

After the change no map prints the `sun` lookup, and the image cache's
reports fall from 800 lines to 80 in each renderer (Alice 47 to 18,
F.A.K.K.2 753 to 62). Nothing new appears. No map's outcome, first error or
world surface counts changed (`probe_retail_map_loads.py --compare`, 0 of 66
in each renderer).

The static parse failed only stageless compiler shaders (`nodraw`, `clip`,
`hint` and the like, 23 in Alice and 15 in F.A.K.K.2, which every Quake III
parser refuses alike) and two broken F.A.K.K.2 scripts no map registers:
`crank` in `scripts/beau.shader` lacks its outer brace, so its `map` reads
as a general keyword, and `textures/aldiewater` in `scripts/sprites.shader`
writes `deform`, which neither engine knows (both hold only
`deformVertexes`). Its only warnings were three Alice shaders' `cull front`
(`powerups/manabase`, `powerups/healthbase`, `textures/test/light01`), which
no map registered either. `alice.exe` accepts exactly Quake III's cull
words -- `none`, `twosided`, `disable`, `back`, `backside`, `backsided`
(0x48ccb6..0x48cd4c) -- and warns on any other, as fnTech3 does, so `front`
keeps its warning here, unlike Medal of Honor's
([its sweep](2026-10-15-mohaa-shader-sweep.md)).

## The engine's changes

**Sun.** `CreateExternalShaders` in all three renderers looks up no `sun`
for Alice or F.A.K.K.2, as for Enemy Territory and Medal of Honor. Neither
retail engine looks one up. `alice.exe`'s own external shaders end with
`projectionShadow` and `flare` (0x48fc55..0x48fc7e, stored at 0xe9d7ac and
0xe9d7b0) and return; the next field, 0xe9d7b4, is the sun shader its
`r_drawSun` draw reads (0x4b0b3b), and no code stores to it, so it keeps the
zero the renderer's state starts with. The plain 1.02 `fakk2.exe` registers
`r_drawSun` (0x461015) and never reads it. Both name "sun" in two places
only, and both are lens-flare code: `alice.exe` compares a lens-flare
section's name with it (0x4a83f5) and initialises a global string with it
(0x4a88f0), and `fakk2.exe` does the same at 0x48ba95 and 0x48bf90. No
reference reaches the `q3map_sun` string's merged tail. Alice ships no `sun`
script or image, so the lookup made a stageless default shader; F.A.K.K.2's
`sun` (`scripts/beau.shader`) is the `sun` surface of its `fx_edensky2.tik`
sky model, which still finds it when the model loads. fnTech3 draws
`tr.sunShader` only under `r_drawSun` (default 0) and skips a null one, so
nothing drawn by default changes; with `r_drawSun 1` F.A.K.K.2 no longer
draws its sky model's texture as Quake III's sun, which retail never drew.

**Model images and `r_picmipFilter`.** A TIKI surface whose shader names an
image with no script passes that image's mipmap and picmip treatment
straight to the image (`R_FindAliceSurfaceShader`, from the surface's
`nomipmaps` and `nopicmip` flags). That branch skipped FnQ3's
`r_picmipFilter`, which `InitShader` applies to every shader by its path
(default 1: picmip under `textures/` only) and which every other raw image
honours. So a skin such as `models/weapons/sword/sword1.tga` took picmip from
the `hilt` surface of `weapon_sword.tik` and none from the `swordsteel`
script, and whichever loaded first decided it. Under the two Ritual
profiles the filter's verdict now clears the model's picmip too. With
`r_picmip 0`, the default, no image changes size; under a higher
`r_picmip`, such images follow the filter as their scripts do. Medal of
Honor, the branch's other caller, had [a check of its
own](2026-10-15-mohaa-shader-sweep.md#reused-images-and-the-picmip-filter-2026-10-15),
which extends the rule to it.

`alice.exe` warns about the same reuse (0x484656..0x4846c5: a different
mipmap or picmip at level 1, a different wrap mode at level 0) and returns
the first image, as fnTech3 does. Retail has no picmip filter, so these 720
reports were the engine's own; retail F.A.K.K.2's logs below print none of
them.

Invariant 11: the sun lookup is a Quake III presentation feature the Ritual
engines never had, and `r_picmipFilter` is FnQ3's own image control, now
applied to Ritual models as to every other image; the shader dialect itself
is unchanged, and no retail-exact option is involved.

`tests/ritual_shader_gaps_tests.py` pins both changes. It runs
`R_FindShaderInternal` of each renderer through `tests/model_shader_tests.py`'s
fixture under both Ritual profiles, Medal of Honor and Quake III, and checks
the data the sun change rests on in the installations when present.
`tests/alice_renderer_parity_tests.py`, `tests/mohaa_shader_gaps_tests.py`
and `tests/wolfet_map_load_tests.py` follow the changed lines.

## Retail F.A.K.K.2's own logs

On 2026-09-25 the retail 1.02 client (`fakk2.exe` SHA-256 `681bc51c...`,
`developer 1`, windowed on a private desktop) joined an fnTech3 dedicated
server on each of the 30 maps and reached `CS_ACTIVE` on every one
(`.tmp/fakk2/sweep`, harness in
[F.A.K.K.2 contracts](2026-09-24-fakk2-contracts.md)). Its client loads each
map's shaders and models from its own paks: the laid-down game's plus the
1.02 patch's `pak2.pk3`. Across all 30 its logs hold four shader lines, each
on the map where fnTech3 prints the same gap:

| Retail 1.02 | Map | fnTech3 |
| --- | --- | --- |
| `Couldn't find image for shader textures/sky/cliffsky` | `zoo` | default shader for `textures/sky/cliffsky` |
| `reused image textures/hud/health.tga with mixed glWrapClampMode parm` | `training` | `health.tga ... (0 vs 4)` |
| `reused image textures/sprites/rope.tga with mixed allowPicmip parm` | `fog` | `rope.tga ... (3 vs 1)` |
| `shader 'textures/otto/ottoscrew' has lightmap but no lightmap stage!` | `blood` | the same line |

Retail looked up no `sun` and printed none of the 691 model-image reports
fnTech3's F.A.K.K.2 maps printed before the change. After it, with
`r_picmipFilter 0`, fnTech3's F.A.K.K.2 reports are exactly retail's two;
with the default filter, the 60 others are the filter's own (see Open).

## The data's own gaps

The paks were searched for every missing name under any extension. None of
`crosshair`, `textures/notexture`, `beamshader` or `textures/sky/cliffsky`
exists as a script or an image in either title, so each lookup ends at the
default shader in retail as it does here; the sprite lookups follow retail's
own (`R_LoadAliceSprite`, which records the addresses).
`targeted_outofrange` is defined only in `scripts/old_shader_bits.txt`, and
`alice.exe` loads `scripts/*.shader` alone (its `FS_ListFiles( "scripts",
".shader" )`, 0x48f8bb..0x48f8c9). Its images, `targeted_outofrange.tga` and
six more, lie inside `sprites/targeted.zip`, a zip file stored inside
`pak1_large.pk3`, which no engine opens.

Outside the shaders, Alice's `gvillage` and `pandemonium` name a
`models/smoke.tik` and F.A.K.K.2's `zoo` a `models/item_healthmush_glowing.tik`
that neither title's paks hold.

## Open

- `r_picmipFilter` decides picmip by each shader's own path, so an image
  under `textures/` that a model's script also names gets picmip from one and
  not the other: 77 "reused image" lines, all `3 vs 1`, which disappear
  under `r_picmipFilter 0`. The rule is the same under every profile; judging
  by the image's path instead would change every profile and is left for a
  change of its own. At `r_picmip 0` it alters no image.
- The sweeps register what a map loads and its first seconds use. Shaders
  registered later in play are covered by the static parse alone.
