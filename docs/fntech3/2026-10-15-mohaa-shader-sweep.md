# Medal of Honor's shader warnings across every map, 2026-10-15

Each of the 54 maps in the legitimate 1.00 installation was loaded under the
`mohaa` profile with `developer 1`, in the GL and the Vulkan renderer, and
every shader warning the logs printed was traced to its source. The parser
met no keyword it lacks: every warning came either from the engine looking up
Quake III's own client art, which 1.00 does not ship, or from 1.00's data
naming images and shaders its paks do not hold. The engine-side ones are
fixed; the data-side ones are listed below and left as the data has them.

The [JSON companion](2026-10-15-mohaa-shader-sweep.json) records the builds,
the installation's hashes, the command lines, each sweep's outcomes and every
distinct warning with its map count. This is Source, Unit and Live subset
evidence; no gate closes. The SafeDisc-encrypted `MOHAA.exe` cannot run here,
so nothing below is compared with the retail engine's own logs or drawing.

## Method

`scripts/probe_retail_map_loads.py` gained `--developer`, which runs each map
with `developer 1`, and `--then`, which runs a console command once the map
has settled and settles again. Each map ran in its own private-desktop
process and home: `map <name>`, six seconds, `finishloadingscreen` (so a
single-player mission leaves its Continue screen and starts), six more
seconds, `quit`. `scripts/scan_shader_warnings.py` then groups every
shader-related line of the kept console logs by kind -- unknown keyword,
missing image, default shader, malformed script, lightmap mismatch -- with the
maps that printed it.

Both runtimes were built from a `git archive` export of `27273055`, the
"after" one with only this change's hunks added, under the shared developer
options with strict warnings (`/W4 /WX`); the companion holds their hashes.

A static inventory of the shader scripts backs the runtime one: the six paks
hold 137 scripts and 4,502 shader definitions, and every general keyword,
stage keyword and keyword value they use was listed
(`.tmp/mohaa-shaders/tools/static_scan.py`, counts in the companion).

## Results

All 54 maps loaded in all four sweeps (GL and Vulkan, before and after). The
GL and Vulkan logs printed the same 35 distinct shader warnings before the
change. None was an unknown keyword or value apart from `cull front`.

| Warning | Maps | Source | Action |
| --- | --- | --- | --- |
| `console`: `gfx/2d/BattleCry.tga` not found | 54 | The client registered Quake III's "console"; 1.00's `scripts/3dk.shader` defines one naming an image no pak holds | The client no longer looks it up |
| default shader for `icons/record`, `menu/art/3_cursor2`, `ui/assets/backscreen_smoke` | 54 | Quake III client art the client's shared path registered | No longer looked up |
| default shader for `sun` | 54 | The renderer's Quake III sun; 1.00's sun is its maps' lens flares | No longer looked up |
| `cull front` invalid | 10 | Four of the seven shaders that write it, all hedgerows named `*_backculled` | Accepted as front-sided culling |
| `white`: `textures/sprites/white.tga` not found | 11 | 1.00's `scripts/sprites.shader`; the DM and objective loading menus ask for it | Data; see Open |
| other images not found | 1-2 each | `senn_fire1`/`senn_fire2`, `mg42_starflash`, `hammer`, `textures/hud/item_uniform` name images no pak holds | Data |
| default shader for `textures/menu/exit` | 54 | `cgamex86.dll` registers it; no pak holds it (`textures/menu/pause` beside it does) | Data |
| default shader for `poopy`, `mg`, `opelhubstill`, `opeltreadstill`, `material27`-`31` | 1-24 | TIKI surfaces their `.tik` files give no shader, named after the surface | Data |
| default shader for `cardboard-gib.spr` | 2 | `models/fx/crates/debris_3.tik` caches a sprite with no shader or image | Data |
| default shader for `textures/notexture`, `textures/misc_outside/nu_grass_set2a_clmp` | 6, 1 | World shaders no script defines and no image matches | Data |
| `nextBundle without an image` in `bp_debris2g`/`3g`/`3` | 14-22 | The debris shaders end their stage with `nextbundle` and `rgbGen lightingSpherical` alone | Already a developer note |
| lightmap but no lightmap stage | 1-6 | `notexture`, `nu_grass_set2a_clmp`, `switchflat_pulse`, `water_nodraw` | Quake III's own developer note |

After the change the GL and Vulkan logs again agree, and hold 26 distinct
shader warnings, all of them the data rows above: the nine the engine caused
are gone from every map, and nothing new appears. No map's outcome, first
error or world surface counts changed (`probe_retail_map_loads.py --compare`,
0 of 54 in each renderer).

## The engine's changes

**Client art.** `CL_InitRenderer` gives Medal of Honor a branch of its own,
as Alice, Q3Test and the IHV Test have: Quake III's character sheet, which
1.00 ships (`Pak0 gfx/2d/bigchars`), and the engine's own white for the fills
and the console background, which `CL_MOHAAWhite` already gave the fills. The
recording icon, console logo and console pointer are none; the console
draws its own pointer, as it does for every profile without one, and 1.00's
own `gfx/2d/mouse_cursor` stays its menus'. `SCR_Init` registers no connect
background, as for Enemy Territory and the Ritual titles: 1.00's menus draw
its loading screens. With `con_backgroundStyle 0` the console had drawn the
default shader; it now draws the engine white tinted as the style asks.

**Sun.** `CreateExternalShaders` in all three renderers looks up no `sun`
for Medal of Honor, as for Enemy Territory: 1.00 defines none, and a level's
sun is its lens flare. The lookup had found neither script nor image, so the
stageless shader it made drew nothing even under `r_drawSun 1`; only the
lookup and its warning go.

**`cull front`.** Quake III's shader manual names `cull front` as the
default; Quake III's parser and OpenMoHAA's **[O]** reject the word, warn
and keep front-sided culling. The seven `*_backculled` hedgerows in
`scripts/misc_outside.shader` and `scripts/wilderness.shader` use it, and
their names ask for exactly that. Under the `mohaa` profile alone the word is
now front-sided culling without the warning; the drawing is unchanged, and
every other profile still warns.

Invariant 11: these are presentation choices of the engine's own interface
(its console and fills), and the shader dialect is the data contract; no
retail-exact option is involved.

`tests/mohaa_shader_gaps_tests.py` pins the three changes,
`tests/scan_shader_warnings_tests.py` the scanner and
`tests/probe_retail_map_loads_tests.py` the probe's new console script.
`tests/mohaa_white_shader_tests.py` and `tests/wolfet_map_load_tests.py`
follow the moved lines.

## The data's own gaps

The paks were searched for every missing name under any extension: none
exists, so each is 1.00's own. OpenMoHAA **[O]** fails such a stage and hands
back its default shader, as fnTech3 does. The TIKI surfaces get a shader
named after themselves by the 2.0 rule fnTech3's TIKI loader incorporates
from OpenMoHAA (`code/mohaa/tiki/tiki_files.cpp`); under 1.x's renderer such a
surface draws its first, empty shader slot, the default shader **[O]**, so
the drawing is the same either way. `vehicle_german_opeltruckgreen.tik` maps
`opelhub` and `opeltread` but not the model's `opelhubstill` and
`opeltreadstill` surfaces, which `opeltruck.tik` hides with `+nodraw`.

Two spellings in the scripts are outside the parser, and no map registers
either: `textures/misc_outside/regpalm_shadow` has a bare `nodraw` line
(`scripts/misc_outside.shader:292`), which warns and is skipped, and
`seagull` has `alpahFunc GE128` (`scripts/effects.shader:1072`), which fails
the shader. OpenMoHAA **[O]** treats both alike.

Reusing an image under other wrap or mip flags (`textures/effects/fire_ring.tga`
on 45 maps) keeps the first load and warns, as OpenMoHAA's image cache
**[O]** does; it is not a shader gap. [Reused images and the picmip
filter](#reused-images-and-the-picmip-filter-2026-10-15) sorts every such
report by its cause.

## Open

- The DM and objective loading screens (`ui/loading_*.urc`) draw a
  480x368 label with shader `white` behind the map's photograph, a white mat
  under the sepia overlay. 1.00's `white` fails, so fnTech3's menus skip the
  label and OpenMoHAA's **[O]** would draw its default image there. What
  1.00 drew is unknown **[I]**; it needs a capture of the retail game.
- The sweeps register what a map loads and its first seconds use; shaders
  registered later in play (an effect a script spawns late, a weapon not yet
  fired) are covered by the static inventory alone.

## Reused images and the picmip filter, 2026-10-15

The [Alice and F.A.K.K.2 sweep](2026-10-15-ritual-shader-sweep.md) found
that a TIKI surface's own image skipped FnQ3's `r_picmipFilter`, which
caused most of those titles' "reused image ... with mixed flags" reports,
and made the filter decide that image's picmip under the two Ritual profiles.
Medal of Honor's TIKI surfaces reach the same branch
(`R_FindAliceSurfaceShader`, called from `tr_mohaa.c`), so this sweep's
reports were sorted by cause.

**Method.** The GL sweep above was repeated on a runtime built from a
`git archive` export of `aa0e7049` with the Ritual sweep's staged
`tr_shader.c` hunks, which change nothing under this profile; again with
`r_picmipFilter 0`; and with a diagnostic copy of that GL renderer, never
staged, that logged every lookup reaching the branch where the caller chose
the image's flags, with the name, the filter's verdict and the image found.
The TIKI files of the six paks and their `models/` includes were read as well.

**Results.** All 54 maps loaded in every run. The logs hold 118 reused-image
lines on 45 maps, 15 distinct, the same in GL and Vulkan:

| Lines | Distinct | Flags that differ | Source |
| --- | --- | --- | --- |
| 60 | 9 | picmip alone (`3 vs 1`) | `r_picmipFilter`'s rule by shader name; none with the filter off |
| 58 | 6 | wrap or mipmaps | 1.00's scripts treating one image two ways; the same with the filter off |
| 0 | 0 | -- | A TIKI surface's own image |

Each of the 60 is an image that a world shader under `textures/` names -- a
script, or a map surface no script defines, as `textures/models/items/v2` in
`obj_team2` and `textures/models/submodels/sub_bunk` in `m2l2b` -- and that a
model script with a pathless name, such as `car`, `gasmask`, `static_v2` or
`Painting_master5`, names too. The filter judges each shader by its own name,
so the first gets picmip and the second does not.
`textures/common/reflection1.tga`, the environment map of 56 vehicle, item
and uniform scripts, makes 42 of them. This is the rule the Ritual record
leaves open under every profile, and at `r_picmip 0`, the default, it changes
no image. The filter is FnQ3's own, and OpenMoHAA **[O]** has none, so
these 60 are fnTech3's own **[I]**.

The 58 others are the data's, and the filter leaves them alone. `fire_ring`
clamps `textures/effects/fire_ring.tga` in one bundle and wraps it in the
next (46 lines); `beam` and `opellights` (5), the hedgerow scripts (2) and
`regpalmsprite` and `regpalmshadow` (1) likewise clamp an image another
wraps; and a script with `nomipmaps` asks for an image the other scripts
mip (`miscpieces.tga` 3, `pulse.tga` 1, all on the `m2l2` maps).

None came from a TIKI surface's own image. In the diagnostic run the branch
ran 32 times on 24 maps, for four names alone -- `poopy`, `mg`,
`opelhubstill` and `opeltreadstill`, the shaders named after surfaces their
models leave unshaded (above) -- and found an image for none. The paks agree:
the 2,898 `surface ... shader` lines of 1.00's 1,378 TIKI files and their
includes name 1,331 shaders, 1,322 defined by a script and nine that no
script defines and no image matches. None has a file extension, which is how
a TIKI names an image, relative to its `path`; every `path` 1.00 sets is
under `models/`.

**The change.** Under the `mohaa` profile the branch now clears a surface
image's picmip when the filter refuses it, as for the Ritual profiles. 1.00's
data never reaches it with an image, so no retail image or report changes:
the "after" runtime, which adds this hunk alone, prints the same 118 lines in
GL and Vulkan, and `probe_retail_map_loads.py --compare` finds no map's
outcome, first error or surface counts changed (0 of 54 in each renderer). A
TIKI that does name an image file, under `models/` by its `path`, now gets
picmip as a script there would, not from whichever shader loaded it first.
Invariant 11: `r_picmipFilter` is FnQ3's image control, now applied to
Medal of Honor's model images as to every other; no retail-exact option is
involved.

`tests/mohaa_shader_gaps_tests.py` pins the condition, runs the branch
through each renderer's real `R_FindShaderInternal` under the profile, and
checks the TIKI facts above against the installation when it is present;
`tests/ritual_shader_gaps_tests.py`'s Medal of Honor case and
`tests/alice_renderer_parity_tests.py` follow the changed line. The
companion's `picmip_filter_check` holds the builds, the six sweeps and the
counts.
