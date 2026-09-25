# The light grid's ambient scale

## Scope

Every renderer lights each model its game does not light some other way from
the map's light grid, and scales the grid's ambient by `r_ambientScale` before
it adds the minimum light. fnTech3 registered Quake III's `0.6`, a cheat cvar,
for every profile. This record reads what each retail engine registers and how its
grid path uses the value. It moves American McGee's Alice (`alice`) to its
engine's `0.5`, and Heavy Metal: F.A.K.K.2 (`fakk2`) to the same once that
profile is committed (see Open). It moves Quake Live (`ql`) to its engine's
`10`, a player option bounded to 1 through 100. Quake III keeps `0.6`, the
value `quake3.exe` 1.32 registers. The other profiles' engines were surveyed
and left as they were. The evidence JSON is
[2026-09-25-ambient-scale.json](2026-09-25-ambient-scale.json).

In the two Ritual games the change reaches props, items and whatever else the
game module leaves unflagged. Characters and weapons are lit from the maps'
compiled lights instead
([model light record](2026-09-25-ritual-model-lights.md)).

## Observed: the Ritual engines

Both executables were read with capstone; addresses are virtual addresses in
the image. `alice.exe` (2,209,835 bytes, SHA-256
`8aa22917b7707f0e1a7c4dbe9a3c38332b1873ae2059174ac5329de26921fc18`) is
SafeDisc-wrapped, but its code section reads. `fakk2.exe` 1.02 (1,318,912
bytes, SHA-256
`681bc51c3343d01d772c5ae4aae7523ae4547350b243c875156e138043335785`) is the
patch's plain executable.

| What | `alice.exe` | `fakk2.exe` 1.02 |
| --- | --- | --- |
| `r_ambientScale` | `0x004754ed..0x00475501`: `"0.5"`, flags `0x200` (cheat); the pointer is kept at `0x00f02f10` | `0x00461123..0x00461137`: `"0.5"`, flags `0x200`; the pointer is kept at `0x00d3e3f0` |
| `r_directedScale` | `0x00475507..0x0047551b`: `"1"`, flags `0x200`; kept at `0x00f02ff4` | `0x0046113d..0x00461151`: `"1"`, flags `0x200`; kept at `0x00d3e4c8` |
| Read by | `R_SetupEntityLighting` (`0x004ac7e0`) alone, at `0x004acb70..0x004acbea` | `R_SetupEntityLighting` (`0x0048ed10`) alone, at `0x0048f0b5..0x0048f131` |
| Order | renormalise when the eight samples' weight is above 0 and below 0.99 (`0x004acb02`), ambient times `r_ambientScale`, directed times `r_directedScale`, direction normalised, then 32 times `identityLight` for a model flagged minlight (`0x004acc66`) | the same (`0x0048f045`, `0x0048f1a5`) |

That is Quake III's grid path, in Quake III's order. fnTech3's
`R_SetupEntityLightingGrid` follows it for every profile, so only the
registered value differed. The flat light a model takes without the grid,
150 times `identityLight` (`0x004acc28`, `0x0048f143`), is not scaled.

## Observed: Quake III and Quake Live

`quake3.exe` 1.32 (909,312 bytes, SHA-256
`80b0add0e014b863cf47cab6c997e0a54939236219a56910b7a3eb77dc2d44d4`) registers
`r_ambientScale` as `"0.6"` and `r_directedScale` as `"1"`, both with flags
`0x200` (`0x0045152a..0x0045155b`). Its grid function reads the first at
`0x0047120e`. fnTech3's `q3` profile already matched.

`quakelive_steam.exe` (2,107,904 bytes, SHA-256
`c926fe9f6c851e00b3b9332e88903ad01f28fdd60454873891c0158f5ded1299`, linked
2016-06-03) differs:

| What | Where | Reading |
| --- | --- | --- |
| `r_ambientScale` | `0x00449013..0x00449031` | registered through the renderer's second cvar import with `"10"`, `"1"`, `"100"` and flags `0x81800`; the pointer is kept at `0x01740f88` |
| That import | filled with `0x004cdd30` by the client (`0x004b9ca2`); `GetRefAPI` (`0x00449f70`) copies the table to `0x01740d20` | a bounded `Cvar_Get`: it keeps the minimum and maximum as floats (`+0x34`, `+0x38`), moves a value already set outside them to the nearer one, and sets bit `0x1000` on every cvar it makes |
| `Cvar_Set` | `0x004cd0a4..0x004cd0cc` | a value set outside a cvar with bit `0x1000` becomes the nearer bound. Nothing rounds it, so a value such as 2.5 stands; bit `0x2000` snaps to one end instead |
| `r_directedScale` | `0x0044935b..0x0044936f` | `"1"`, flags `0x200`, through the plain `Cvar_Get` |
| The grid | `0x0044a810`, read at `0x0044ab56..0x0044abb3` | Quake III's function: the same renormalisation (0.99, `0x0052f790`), then ambient times `r_ambientScale` and directed times `r_directedScale`. Its caller (`0x0044acb0`) adds 32 (`0x0052f7b0`) and keeps the ambient to `identityLightByte`, as Quake III does |

Flags `0x81800` are `0x800`, protected, which writes the value to the
replicated `repconfig.cfg`; `0x1000`, bounded, as above; and `0x80000`, cloud.
Retail's web interface (`web.pak`) lists `r_ambientscale` on its Video page,
between `r_dynamiclight` and `cg_shadows`, as "Ambient Light Scale": a range
from 1 to 100 in steps of 1. The replicated configs the retail client wrote
into this installation hold `seta r_ambientScale "10"`.

So retail Quake Live gives a model ten times its grid ambient, 16.7 times what
fnTech3 gave it, before it keeps the ambient to `identityLightByte`.

Two other sources disagree with the executable, and it outranks both:

- QLSRP's reconstruction registers the cvar as an integral 1..100 and checks
  it with `AssertCvarRange( ..., qtrue )`. Retail calls no range check on it,
  and its bounded `Cvar_Get` never rounds.
- FnQL, the Quake Live baseline, registered Quake III's `0.6` cheat, and its
  web interface hid the retail row as a cheat control. A hardware config FnQL
  wrote into this installation holds `seta r_ambientScale "0.6"`; the `ql`
  profile executes the replicated config after it, so retail's value wins
  where both exist.

## Survey of the other engines

The same scanner read each profile's own executable. Only the registrations
were read; how each engine's grid path uses the value was not traced.

| Profile | Executable (SHA-256, first bytes) | `r_ambientScale` | `r_directedScale` | fnTech3 |
| --- | --- | --- | --- | --- |
| `rtcw-sp` | `WolfSP.exe` (`07d206f6`) | `"0.5"`, `0x200` (`0x0044712e`) | `"1"`, `0x200` | `0.6` |
| `rtcw-mp` | `WolfMP.exe` (`7cf99a93`) | `"0.5"`, `0x200` (`0x0045ae63`) | `"1"`, `0x200` | `0.6` |
| `wolfet` | `et.exe` 2.60b (`4d1326c5`) | `"0.5"`, `0x200` (`0x0049d9c6`) | `"1"`, `0x200` | `0.6` |
| `stvef-sp` | `stvoy.exe` (`8f7ccadb`) | `"0.5"`, `0x200` (`0x00440a36`) | `"1"`, `0x200` | `0.6` |
| `stvef-mp` | `stvoyHM.exe` (`5b5b6e82`) | `"0.5"`, `0x200` (`0x00453c5f`) | `"1"`, `0x200` | `0.6` |
| `jka-sp` | `jasp.exe` (`9351a8de`) | `"0.5"`, `0x200` (`0x00469d99`) | `"1"`, `0x200` | `0.6` |
| `jka-mp` | `jamp.exe` (`e8062d39`) | `"0.6"`, `0x200` (`0x004a25a2`) | `"1"`, `0x200` | `0.6` |
| `jk2-sp` | `jk2sp.exe` (`0d3c635e`) | `"0.5"`, `0x200` (`0x0046ea55`) | `"1"`, `0x200` | `0.6` |
| `jk2-mp` | `jk2mp.exe` (`2f08dcf9`) | `"0.6"`, `0x200` (`0x0047f200`) | `"1"`, `0x200` | `0.6` |
| `q3-ihv` | `quake3.exe` (`e9fcaa0a`) | not in the file | not in the file | `0.6`, unused by its own model light |
| `q3test` | 1.02, 1.03, 1.05 | not in the file | not in the file | `0.6` |
| `q3test` | 1.06 (`a18c714f`) | `"0.5"`, flags `0` (`0x004263c0`) | `"1"`, flags `0` | `0.6` |
| `q3test` | 1.07 (`a507b42a`), 1.08 (`89a7dfb4`) | `"0.5"`, `0x200` | `"1"`, `0x200` | `0.6` |
| `mohaa` | `MOHAA.exe` (`7abe43b4`) | not readable: SafeDisc, no plain name | -- | `0.6` |

OpenMoHAA's renderers register ioquake3's `0.6`, which says nothing about
retail's.

## Measured

Two builds ran from `meson/build/win32-alice-client`, which is configured for
both Ritual profiles. Before the change the folder held `fntech3.exe` from
14:31 and renderers from 14:38, built from the shared tree on base commit
`075abdd`. After it, the executable, the three renderers and `fnql-web.pak`
were rebuilt in place from the same tree with this change at 17:47. The
package of map sidecars was left as it was. The evidence JSON gives every
binary's hash, and each batch hashed its binaries before and after its runs.
After the runs the registration was respelled with `RITUAL_ML_PROFILE`, which
in that tree expands to the same test, and the renderers were rebuilt.

Every run was windowed at 800 x 600 on a private desktop through
`scripts/probe_ritual_client.py`, with an isolated home holding an empty
`base/config.cfg` (`fakk/config.cfg` for F.A.K.K.2), `ui_subtitles 0` for Alice
and `net_enabled 1` on loopback. Each took `devmap <map>`, waited 12 s, ran
`skipcinematic`, waited 4 s, printed `r_ambientScale`, and took nine engine
`screenshotPNG` captures 1.7 s apart. The first capture was at the build's
default; the others set `r_ambientScale` to 0, 4, 0, 4, 0.6, 0.5, 0.6 and 0.5.
No input was sent, and each installation was unchanged after every run. The
Windows session was unlocked when checked before the first run, at the
rebuild and after the last run. The client is Windows x86 on Windows 11,
drawing on an NVIDIA GeForce RTX 4060 Laptop GPU.

A pixel counts as grid-lit when its luma rises by more than 24 from 0 to 4 in
both pairs, its 0.6 capture lies between them, and it repeats within 4 across
each pair of equal settings; Alice's HUD gauges, the outer 40 columns, are
left out. The world, the HUD and the models lit from compiled lights never
read the cvar, so what remains is the grid-lit props. Of the thirteen Alice
and twelve F.A.K.K.2 opening views tried, six of each held one large enough to
measure.

**Before the change**, on GL, the props' mean luma at 0.6 and at 0.5, and the
ratio between the two repeated pairs:

| Map | Grid-lit model | Pixels | 0.6 | 0.5 | 0.5 / 0.6 | Repeat |
| --- | --- | --- | --- | --- | --- | --- |
| `garden1` | plant at the left edge | 732 | 31.57 | 30.90 | 0.979 | 0.981 |
| `utemple` | small plant ahead | 64 | 30.08 | 27.85 | 0.926 | 0.931 |
| `centipede1` | hanging roots, plants | 1,563 | 28.13 | 27.56 | 0.980 | 0.987 |
| `hatter2` | object before the clock | 382 | 33.64 | 30.78 | 0.915 | 0.921 |
| `pandemonium` | object under the lantern | 602 | 14.81 | 12.95 | 0.874 | 0.886 |
| `hedge1` | urn | 4,344 | 29.14 | 27.59 | 0.947 | 0.948 |
| `cemetery` | two statues | 30,374 | 52.82 | 49.12 | 0.930 | 0.930 |
| `homes1` | water tower, building | 6,742 | 40.69 | 37.75 | 0.928 | 0.928 |
| `over` | two wall lamps | 2,361 | 60.71 | 57.69 | 0.950 | 0.950 |
| `water` | fittings along the pool | 1,083 | 61.81 | 56.54 | 0.915 | 0.915 |
| `fakkhouse` | small distant object | 83 | 68.56 | 64.97 | 0.948 | 0.946 |
| `swamp1` | pots on the ground | 232 | 43.08 | 42.12 | 0.978 | 0.966 |

So at the same light, 0.6 had drawn these props 2% to 14% brighter than
retail's 0.5. How much depends on the ambient's share of a prop's light; a
linear reading of the 0 and 4 captures puts it highest on `pandemonium` and
`hatter2`, about 65% and 43% at 0.6.

- **After the change** the Ritual builds print `"r_ambientScale" is:"0.5"
  default:"0.5"`. Both builds run the same steps from the same spawn, and
  their frame sequences match: from one capture to the next, they change by
  the same amounts. So the first captures, each at its build's default, were
  compared on the props directly. The new default draws them at 0.876
  (`pandemonium`) to 0.983 (`garden1`) of the old on Alice's views, and at
  0.915 (`water`) to 0.972 (`swamp1`) on F.A.K.K.2's, as the table predicts.
  Outside the props, 95.4% to 99.5% of the pixels are identical on Alice's
  views, and 78% to 97% on F.A.K.K.2's. Within one F.A.K.K.2 run, where the
  view holds still, the default capture differs from the explicit 0.5 by 0.00
  to 0.25 in mean luma on the props, and from 0.6 by 1.6 to 5.4.
- **Retail F.A.K.K.2 agrees.** An earlier sweep captured retail `fakk2.exe`
  1.02's own view of each map as it joined an fnTech3 server
  (`.tmp/fakk2/sweep/<map>/frames/retail`), from the same spawn. On
  `cemetery`'s statues retail averages 47.56 against 52.82 at 0.6 and 49.12
  at 0.5; on `homes1`'s buildings 36.51 against 40.69 and 37.75. So the props
  went from 11% brighter than retail to within 3.3%. Turning fnTech3's static
  lights off leaves both at 0.968 and 0.967, so they do not account for the
  rest. The frames' gradients correlate best at zero offset, at 0.52 and
  0.63. `over`'s lamps glow and `water`'s view differs from retail's (0.45 and
  0.15, `water`'s best offset 2 by 8 pixels), so those two are not compared.
- **Vulkan and RTX** turn bloom on by default, and at `r_ambientScale 4` a
  large prop blooms over the whole frame, so they were measured on the GL
  mask of the same view (gradient correlation 0.97 to 0.99). They give the
  same ratios as GL, 0.947 on `hedge1`, 0.981 to 0.985 on `centipede1`, 0.930
  on `cemetery`, and 0.960 on `over`, whose lamps bloom. On F.A.K.K.2's views
  their new default draws the props exactly as an explicit 0.5 does (49.12
  and 97.65 against 49.12 and 97.64).
- **Quake Live**, on `campgrounds` after `team f`, in third person, before
  the change: the default was `"0.6"`, `150` and `0.5` were both accepted, and
  on a plain `map` setting it was refused (`r_ambientScale is cheat
  protected.`). Neither config held it. The player model's grid-lit pixels
  averaged 33.06 at 0.6 and 80.97 at 10, retail's default: 2.45 times as
  bright. After the change the default is `"10"`, and the model draws at
  81.16, as an explicit 10 (81.24). `0.6` and `0.5` are set to the minimum,
  1, and `150` to the maximum, 100, each with a warning. On a plain `map` a
  player's `25` is taken, `cvarlist` shows no cheat flag, and the value is
  written to `repconfig.cfg` (`seta r_ambientScale "25"`), not
  `qzconfig.cfg`.
- **The web interface.** With `cl_webuiEnable 1`, `web_changeHash
  #/settings/video` opened retail's Video page, and the host's own
  `web_dumpSurface` wrote the page surface. Before the change the overlay hid
  15 retail rows there; after it, 14, and Ambient Light Scale is drawn between
  Dynamic Light and Player Shadows, as retail lists it. In these probes every
  retail row showed zero or off, the Brightness row 0.0 with `r_gamma` at 1,
  in the old build as in the new, so they do not show what value the row
  starts at.
- **The change alone.** A `git archive` of `075abdd` with only this change
  applied, configured as `win32-dev` plus `alice-development`, built all 1,351
  targets with strict warnings and no warning. Of its 169 Meson tests outside
  the Python suite, 167 passed and 2 were skipped. Its Python suite passed
  2,007 tests and skipped 76 that need tools or installations this machine
  lacks.

## The change

- `r_ambientScale` is registered per profile in all three renderers
  (`code/renderer/tr_init.c`, `code/renderervk/tr_init.c`,
  `code/rendererrtx/tr_init.c`). The Ritual profiles take `"0.5"`, still a
  cheat. The `ql` profile takes `"10"`, `CVAR_PROTECTED | CVAR_CLOUD`, bounded
  1 to 100 by `Cvar_CheckRange` as a float: a player option kept in
  `repconfig.cfg`, as retail keeps it. Retail's third flag, `0x1000`, is its
  bounded marker. In this engine that bit is `CVAR_VM_CREATED`, which marks a
  cvar a module made, and a game-directory change unsets every such cvar
  (`Com_GameRestart`), so it is left out, as it is for `com_maxfps`. Every
  other profile keeps Quake III's `"0.6"` cheat.
- The Quake Live web interface shows retail's Ambient Light Scale row again.
  The overlay no longer hides it (`code/client/webui/fnql-settings.js`), and
  the configuration snapshot that retail's rows take their values from
  publishes `r_ambientScale` (`code/client/cl_webui.cpp`;
  [web interface notes](../fnql/WEBUI_BACKEND.md)).
- `tests/ambient_scale_defaults_tests.py` compiles each renderer's two
  registrations against a recording `ri` and runs them for every profile:
  Quake Live takes `"10"`, its two flags and its bounds; each Ritual profile
  `"0.5"`; every other profile `"0.6"`; and every profile `r_directedScale`
  `"1"`. `tests/webui_settings_overlay_tests.py` expects the row and the
  published cvar.
- The [model light record](2026-09-25-ritual-model-lights.md) said the grid's
  ambient is its stored ambient times 0.6; that was fnTech3's value, and the
  record now says so.

## Contract and presentation

Retail is authoritative here for how each game's grid data is read (AGENTS.md
invariant 11): the share of the grid's ambient a model receives is what the
map's light means to that game. That covers each engine's registered value,
and for Quake Live the range and persistence that make it a player option.
The presentation around it is fnTech3's own and keeps its defaults: dynamic
lights with their shadows, the maps' static lights, the compiled model lights
of the Ritual games, and the Vulkan and RTX renderers' HDR and bloom. The cvar
stays adjustable in every profile, as in retail, so no retail-exact option is
added.

## Open

- **The other profiles.** RTCW single player and multiplayer, Enemy
  Territory, Elite Force single player and Holomatch, Jedi Academy and Jedi
  Outcast single player, and Q3Test 1.06 to 1.08 register `0.5` (survey
  above), and fnTech3 still gives them Quake III's `0.6`. Their grid paths
  were not traced, and RTCW's world shader scales the grid as well
  (`FnRTCWWorldGridScale`), so each needs its own reading before it moves.
  Medal of Honor's executable is encrypted, and its value is unknown.
- **F.A.K.K.2's profile is not at this commit.** The registration names the
  Ritual profiles with the model-light header's `RITUAL_ML_PROFILE`, which is
  Alice alone at the base commit and both once `COM_GAME_ID_IS_RITUAL` names
  F.A.K.K.2, as the shared tree's uncommitted profile does; the F.A.K.K.2
  runs above were made there. `tests/ambient_scale_defaults_tests.py`
  requires `0.5` of every profile `COM_GAME_ID_IS_RITUAL` names.
- **No retail Alice or Quake Live frame.** `alice.exe` cannot run here, and no
  retail Quake Live frame was set beside fnTech3's. Both rest on the
  executables' code, whose grid path is Quake III's.
- **The web row's starting value was not seen.** Every retail Video row read
  zero or off in these probes, in both builds; the row's value comes from the
  configuration snapshot, which the source test covers.
- **Bit `0x1000`.** fnTech3's Quake Live module flag table calls it
  `CVAR_VM_CREATED` (`code/compat/ql_game_abi.h`), while retail's own cvar
  code uses it as the bounded marker (`0x004cdd30`, `0x004cd0a4`). What the
  retail modules mean by it was not examined here.
