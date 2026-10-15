# Raven's `sun` shader keyword, 2026-10-15

The [siege and sky-portal slice](2026-10-15-jka-siege.md) found
`mp/duel9`'s sky drawn a flat grey. Fn's shader parser did not know the
general `sun` line of Raven's sky shaders, so it rejected each such shader
and drew the default image in its place. This slice measures what every
retail parser does with `sun`, counts the installed shaders that use it,
and has the four Jedi profiles read it as their engines do, into fnTech3's
own sky sun. Two more words share those shaders and are served with it:
Raven's `lightColor`, in both Jedi games, and `noTC` under Jedi Outcast,
which the renderers had read for Jedi Academy alone. The
[JSON companion](2026-10-15-raven-sun-keyword.json) records the binaries,
identities, addresses, counts, commands and outcomes. No compatibility gate
closes.

Implementation is independently authored. The retail executables were read
with capstone and pefile. Pinned OpenJK was used only to find the branch
(reference observation; no text incorporated). OpenAlice's Ghidra corpus was
read for `alice.exe`, and OpenMoHAA's renderer for Medal of Honor (reference
observation; no text incorporated).

## What the retail parsers do

### Observed: the four Jedi engines

All four send `sun` and `q3map_sun` to one branch of `ParseShader`. It is
Quake III's `q3map_sun` branch with a second name.

| Engine | Word compares | Branch | `tr.sunLight` | `tr.sunDirection` | Back to the loop |
| --- | --- | --- | --- | --- | --- |
| `jamp.exe` 1.01 | `sun` `0x4b4dc4`, `q3map_sun` `0x4b4ddd` | `0x4b51a1` | `0xfe3bb8` | `0xfe3bc4` | `0x4b52e5` |
| `jasp.exe` 1.01 | `q3map_sun` `0x47aef6`, `sun` `0x47af0c` | `0x47b312` | `0xc98ddc` | `0xc98de8` | `0x47b453` |
| `jk2mp.exe` 1.02a | `sun` `0x48c63f`, `q3map_sun` `0x48c655` | `0x48ccbc` | `0x1a50640` | `0x1a5064c` | `0x48c5a0` |
| `jk2sp.exe` 1.02a | `q3map_sun` `0x47c651`, `sun` `0x47c667` | `0x47cab4` | `0x12e402c` | `0x12e4038` | `0x47c604` |

- **Six words.** Each branch reads six words, one
  `COM_ParseExt( text, qfalse )` and `atof` each: red, green, blue,
  intensity, degrees and elevation.
  - A word missing from the end of the line reads as an empty token, so
    as 0.
  - Once a read has crossed the line break, the next read takes the
    following line's first word.
- **The light.** The colour is normalized to unit length and scaled by the
  intensity into `tr.sunLight`.
  - `jamp.exe` and `jk2mp.exe` call `VectorNormalize` (`0x443d40`,
    `0x433dd0`).
  - `jasp.exe` and `jk2sp.exe` normalize inline and leave a colour of no
    length as it is.
- **The direction.** Degrees and elevation become radians, and
  `tr.sunDirection` is `(cos a cos b, sin a cos b, sin b)`.
  - `jamp.exe` divides by 180 and multiplies by pi.
  - `jasp.exe` and `jk2sp.exe` multiply by 1/180 and pi.
  - `jk2mp.exe` multiplies by `0.0174533`.
- **The rest of the line.** No branch skips it. Each jumps straight back to
  the loop's `COM_ParseExt( text, qtrue )`, so a seventh word on the line is
  read as the next general keyword.
  - An unknown keyword ends the shader with "unknown general shader
    parameter", and the shader is defaulted (`0x4b5344`, `0x47b2ef`,
    `0x48cdc6`, `0x47cc16`).
- **Globals.** `tr.sunLight` and `tr.sunDirection` are globals. The last
  shader parsed that says `sun` sets the renderer's sun, whether or not it
  is a sky.
- **The collision model.** `jamp.exe` and `jasp.exe` also read shaders for
  the collision model (`jamp.exe` `0x42e9d0`; `jasp.exe`'s compares at
  `0x4131a2`, `0x4131b8`). There `sun` and `q3map_sun` consume the same six
  words and keep nothing (`0x42eb3d`, `0x413244`). fnTech3 has no such
  parser.

### Observed: `lightColor` and Outcast's `noTC`

- **`lightColor`.** `jamp.exe`, `jasp.exe` and `jk2sp.exe` skip the rest of
  the line (`0x4b4e28`, `0x47afcc`, `0x47c701`).
  - `jk2mp.exe` reads `( r g b )` with `ParseVector` (`0x48c697`,
    `0x48c0e0`) into the shader being parsed. A missing bracket or element
    fails the whole shader.
  - Its `FinishShader` (`0x48f610`) copies a sky's colour into a global
    (`0x1a5065c`, OpenJK's `tr.sunAmbient`), with its `surfacelight`. The
    word it tests is the one the sky parameters set (`0x48f386`).
- **`noTC`.** Both Outcast engines know it, as Academy's do.
  - `jk2mp.exe` (`0x48c80b`) and `jk2sp.exe` (`0x47c7a9`) set a word of the
    shader being parsed (`0x1a88e5c`, `0x131a02c`).
  - `jk2mp.exe`'s `clampmap` and `animMap` image requests pass its inverse as
    the compression-permitted argument (`0x48cf33`, `0x48d9ff`).

### Observed: the other engines

- **No `sun` keyword.** No parser of Elite Force 1.20 (`stvoy.exe`,
  `stvoyHM.exe`), Quake III 1.32, Quake Live, RTCW single player and
  multiplayer, Enemy Territory 2.60b, Q3Test 1.02 to 1.08 or the IHV Test
  compares a token with `sun`.
  - Where the string `sun` appears, it is the name of the sun shader the
    renderer looks up after its flare shader (`stvoy.exe` `0x45594a`,
    `stvoyHM.exe` `0x468b88`, `quake3.exe` 1.32 `0x467a61`,
    `quakelive_steam.exe` `0x4590d7`, Q3Test 1.05 to 1.08).
  - RTCW's and Enemy Territory's executables have no `sun` string of their
    own; it appears only as the end of `q3map_sun`, and no code points
    there.
  - A `sun` line therefore reaches each parser's unknown-keyword branch and
    defaults the shader.
- **`q3map_sun`.** All of them but the IHV Test read `q3map_sun`.
  - Elite Force reads it at `0x452942` and `0x465828`.
  - Quake III 1.32 reads it (`0x464523`) as the Jedi engines do: six words,
    then straight back to its loop (`0x4644b8`).
- **The encrypted engines.** `alice.exe`, `fakk2.exe` and `MOHAA.exe` are
  encrypted on disk.
  - OpenAlice's decompile of `alice.exe` reads `q3map_sun` and no `sun`, and
    so does OpenMoHAA's renderer.
  - None of those three installations has a general `sun` line, so no data
    depends on it.

## The installed shaders

Every shader script in each installation was read: the paks' `scripts/` and
`shaders/` files, the IHV Test's loose scripts and each Q3Test release's own
pak.

| Title | Scripts | General `sun` lines | Shaders |
| --- | --- | --- | --- |
| Jedi Academy 1.01 | 68 | 59 | 58 |
| Jedi Outcast 1.02a | 29 | 11 | 11 |
| Elite Force 1.20 | 39 | 0 | 0 |
| Every other title | | 0 | 0 |

- **Academy.** All 59 lines are in `assets1.pk3`.
  - `textures/mp/desert_03` is defined twice, so 58 shaders. Every one but
    `textures/mp/s_blacksky` is a sky.
  - Two say intensity 0: `textures/mp/hoth_ctf_sky_sides` and
    `textures/skies/yavin_nodraw`.
  - 14 also say `lightColor`: the twelve `rmg_sky_*`,
    `textures/skies/bluelight` and `textures/skies/orangelight`. Four more
    shaders say `lightColor` without `sun`.
  - Two more Academy shaders say `q3map_sun`, which all four Jedi engines
    read in the same branch.
- **Outcast.** All 11 are skies in `assets0.pk3`'s `shaders/skies.shader`.
  Two also say `lightColor` and two `noTC`
  (`textures/skies/kejim_light`, `textures/skies/new_test`).
- **Maps.** 14 Academy maps name such a shader in their BSP:

  | Map | Shader | `sun` line |
  | --- | --- | --- |
  | `mp/ctf2` | `textures/mp/hoth_ctf_sky_sides` | `255 255 255 0 345 85` |
  | `mp/ctf3` | `textures/mp/s_fatalsky` | `0.874291 1 0.819403 170 25 80` |
  | `mp/duel1` | `textures/mp/s_bespin` | `1 0.728909 0.624151 60 50 60` |
  | `mp/duel4` | `textures/mp/s_space` | `0.538903 0.6962 1 60 0 35` |
  | `mp/duel9` | `textures/mp/s_hothsky` | `0.8 0.8 0.95 160 140 80` |
  | `mp/ffa2` | `textures/mp/s_kor_sky` | `0.766119 0.79123 1 175 75 70` |
  | `mp/ffa3` | `textures/mp/h_tatsky` | `0.874291 1.000000 0.819403 175 25 70` |
  | `mp/ffa4` | `textures/mp/s_blacksky` | `1 1 1 32 90 90` |
  | `mp/siege_hoth` | `textures/siege/hoth_sky` | `1 0.96 0.9 90 90 90` |
  | `hoth2` | `textures/skies/hoth` | `255 255 255 355 345 85` |
  | `t1_rail` | `textures/skies/rail_sun` | `1 1 1 355 345 85` |
  | `t3_hevil` | `textures/skies/hevil_2` | `1 1 1 400 300 50` |
  | `t3_rift` | `textures/skies/artus_light` | `0.75 0.79 1 250 0 65` |
  | `yavin2` | `textures/skies/yavin_nodraw` | `0.933333 1 0.8 0 0 0` |

  Two Outcast maps do: `duel_jedi` (`textures/skies/artus_light`) and
  `artus_detention` (`textures/skies/kejim_light`).

## What fnTech3 does now

- **`sun` and `q3map_sun`.** The four Jedi profiles read both as their
  engines do (`R_JediSunKeyword` and `R_JediSunParms`, in
  [`tr_jka_shader.h`](../../code/renderercommon/tr_jka_shader.h)): six
  words, a missing one as 0, and the rest of the line left to the keyword
  loop.
  - If the text runs out during the six words, the shader fails for want of
    its closing brace, as retail's does. Fn's tokenizer would otherwise have
    been handed a null text.
- **The sky sun.** The six values go through the conversion the `q3map_sun`
  branch already used. Each renderer's `ParseSkySunParms` is split into the
  word reader and `SkySunFromParms`.
  - The colour is scaled so its largest component is 1, and the intensity
    and direction are kept.
  - A sky with a sun then hands it to the world sun, as a `q3map_sun` sky
    does.
- **`lightColor`.** The line is skipped, except under `jk2-mp`. There
  `ParseVector` reads `( r g b )` and fails the shader on a malformed
  vector, as `jk2mp.exe` does. Nothing keeps the colour.
- **`noTC`.** Both Outcast profiles now read it as Academy's do, into the
  same image flag.
- **The other profiles.** Nothing changes: `sun`, `lightColor` and `noTC`
  still end a shader there, as their engines would, and their `q3map_sun`
  keeps its branch.

### FnQ3/FnQL features (invariant 11)

- **What the sun feeds.** The sky now draws through Fn's own sky path. Its
  sun feeds Fn's world sun, the source of three features:
  - cascaded shadows (`r_csmShadows`, GLx and Vulkan);
  - the sun sprite (`r_drawSun`);
  - entity lighting on a map without a light grid.
- **RTX.** The path tracer lights the map with the sun and with the sky's
  own surface light.
- **Left out, with reasons.**
  - **A sun on a shader that is not a sky.** Retail's global takes any
    shader's, but Fn's world sun comes from the map's sky, so
    `textures/mp/s_blacksky`'s sun is read and not used.
  - **A sun of intensity 0.** Retail keeps its direction with a black light;
    Fn's sky sun has no such state, so those two skies have no sun.
  - **`jk2mp.exe`'s sky `lightColor`.** No Fn feature reads an ambient sky
    colour.
- **No retail-exact option.** The places retail shows these values, its sun
  sprite and its entity lighting without a light grid, read the same
  direction in Fn (`RB_DrawSun`, `R_SetupEntityLighting`), so no option is
  added.

## Evidence

### Tests

- **`raven_sun_keyword_tests.py`, sliced branches.**
  [`tests/raven_sun_keyword_tests.py`](../../tests/raven_sun_keyword_tests.py)
  compiles each renderer's own Jedi branches and `q3map_sun` branch with
  `SkySunFromParms`, `ParseSkySunParms`, `ParseVector`, the engine's
  tokenizer and `tr_jka_shader.h`. It runs them under all sixteen profiles.
  - It covers recognition in any case, `mp/duel9`'s line, `q3map_sun`
    through the same branch, a 0..255 colour and intensity 0.
  - It covers the rest of a line (a `noTC` after the sixth word is read, and
    an unknown seventh word fails the shader) and short lines (missing
    words as 0, and the next line's word after a crossed line break).
  - It covers a brace read as a word, a line that runs off the text, each
    profile's `lightColor`, and RTX's extended-sun fields.
- **`raven_sun_keyword_tests.py`, whole parser.** A second test runs the
  whole production parser, through `alice_shader_parse_tests`' host, in
  each renderer and profile.
  - A duel sky's definition loads with its sun, direction and `noTC`, and a
    sky with `lightColor` before its sun loads.
  - `jk2-mp` fails a bracketless `lightColor`.
  - Every other profile still refuses `sun` and `lightcolor`.
- **Mutations.** Four mutations each fail the tests: Outcast dropped from
  the profile test, the rest of the line skipped, `jk2-mp`'s `lightColor`
  skipped, and the guard for a text that runs out removed.
- **`jka_shader_notc_tests.py`.**
  [`tests/jka_shader_notc_tests.py`](../../tests/jka_shader_notc_tests.py)
  now expects both Outcast profiles to read `noTC`.

### Every installed Jedi definition through the production parser

The same host parsed every `name { ... }` definition of both installations'
scripts under each profile, once with HEAD's parser and once with this
change. The counts are the same in GLx, Vulkan and RTX.

| Text, profile | Definitions | Refused before | Refused after |
| --- | --- | --- | --- |
| Academy, `jka-sp` and `jka-mp` | 3,391 | 142 | 79 |
| Outcast, `jk2-sp` and `jk2-mp` | 1,187 | 90 | 69 |
| Academy, `q3` (control) | 3,391 | 871 | 871 |
| Outcast, `q3` (control) | 1,187 | 90 | 90 |

- **Newly loading.** 63 Academy definitions load: the 59 with `sun` and the
  4 with `lightColor` alone. 21 Outcast definitions load: the 11 with `sun`
  and 10 with `noTC`.
- **Unchanged otherwise.** No definition fails anew, and none fails for a
  different reason.
- **Still refused.** 40 Academy and 25 Outcast definitions have no stage
  (compiler-only shaders, which retail rejects too). 39 Academy and 22
  Outcast definitions use the stage keyword `surfaceSprites`. 21 Outcast
  definitions use `oneshotanimMap` and one `clampanimMap` (see below).

### Live

- **Runtimes.** Two runtimes were built from an export of `80d15494`,
  without and with this change (`.tmp/runtime/raven-sun-before` and
  `-after`). They differ only in the three renderer libraries.
- **Probe.** Each run was a windowed client on an unswitched private desktop,
  with `net_enabled 0`, its own home, a spectator and no input. Captures are
  the engine's `screenshotPNG`.
  - The installation and the binaries were unchanged after every run.

`mp/duel9`, free-for-all, spectator's view. The sky patch is pixels 40..300
by 5..60 of 800x600.

| Renderer | Before | After |
| --- | --- | --- |
| GLx | `(DEFAULTED)`; patch `(82, 82, 82)`, one colour | `E sky`; mean `(127.8, 138.9, 134.1)`, 163 colours |
| Vulkan | `(DEFAULTED)`; patch `(83, 83, 83)`, one colour | `E sky`; mean `(127.5, 138.3, 133.5)`, 441 colours |
| RTX | `(DEFAULTED)`; patch `(0, 0, 0)`, one colour | `E sky`; mean `(127.4, 138.2, 133.5)`, 437 colours |

- **The parser's warning.** Before, each log prints "unknown general shader
  parameter 'sun' in 'textures/mp/s_hothsky'"; after, none does.
- **RTX.** Before, the path tracer found no sun ("the first sky surface's
  shader textures/hoth/snowbiga has no q3map_sun"), and the level drew
  nearly black.
  - After, it prints "q3map sun 86.6 86.6 102.9 toward -0.133 0.112 0.985".
    That is `s_hothsky`'s line: its unit-length colour times 160, yaw 140,
    elevation 80.
  - It also lights the level with 24 lights instead of 4, the sky's surface
    light among them.
- **GLx's cascaded shadows.** With `r_csmShadows 1`, `r_speeds 4` and
  `r_csmDebug 1`, every frame before printed "csm plan cascades:0 skip
  no-sky-sun".
  - After, every frame prints "csm shadows sky:textures/mp/s_hothsky
    cascades:4 ... to-sun:-0.13 0.11 0.98".

The other Academy multiplayer maps that name such a shader, in GLx:

| Map | Before | After |
| --- | --- | --- |
| `mp/ctf3` (CTF) | `s_fatalsky` `(DEFAULTED)` | `E sky` |
| `mp/duel1` | `s_bespin` `(DEFAULTED)` | `E sky` |
| `mp/duel4` | `s_space` `(DEFAULTED)` | `E sky` |
| `mp/ffa2` | `s_kor_sky` `(DEFAULTED)` | `E sky` |
| `mp/ffa3` | `h_tatsky` `(DEFAULTED)` | `E sky` |
| `mp/ffa4` | `s_blacksky` `(DEFAULTED)` | `E gen` (not a sky) |
| `mp/siege_hoth` | warns on `sun`; stops at client-game import 312 | no warning; stops at import 312 |
| `mp/ctf2` (CTF) | stops before the world loads | the same |

- **The stops.** `mp/siege_hoth`'s stop (`CG_G2_SKINLESSMODEL`) and
  `mp/ctf2`'s ("active world-effect commands are not served yet") are the
  same in both runtimes.
- **Not drawn live.** No Outcast sky and no single-player sky was drawn
  live. Outcast's multiplayer client stops at client-game import 300 before
  a map is drawn, and both Jedi single-player profiles stop at their module
  boundary. For them, the production-parser counts above are the evidence.

## Remaining and unrelated findings

- **`surfaceSprites`.** The stage keyword defaults 39 Academy and 22 Outcast
  definitions, the grass and other sprites Raven scatters over surfaces.
- **Outcast's `oneshotanimMap` and `clampanimMap`.** `jk2mp.exe` knows both
  (its `animMap` branch compares them at `0x48d97b` and `0x48d98c`). Fn
  reads them only under the Academy profiles
  ([`tr_jka_shader.h`](../../code/renderercommon/tr_jka_shader.h)), so 22
  Outcast definitions are refused.
- **RTX misreads Raven's surface flags.** RTX's path tracer marks a surface
  as the compiler's sky when its BSP flags have Quake III's `SURF_SKY`
  (`0x4`, [`tr_bsp.c`](../../code/rendererrtx/tr_bsp.c)).
  - In Raven's maps the low five bits are a material number, and the sky
    bit is `0x2000`. `mp/duel9`'s `textures/hoth/snowbiga` carries `0xe`,
    snow, and was the "first sky surface" before this change.
  - Raven's `s_hothsky` carries `0x00182000`, which that test never sees.
  - A misread surface placed before the real sky would withhold the sun.
- **Fn's parser can leave a null text.** A general keyword's
  `COM_ParseExt` that runs off the end of the shader text leaves the text
  null, and `COM_ParseComplex` or `SkipRestOfLine` then reads through it.
  - The Jedi sun branch now guards its own reads. Others, such as `light`
    and the shared `q3map_sun` branch, do not.
  - Retail's `COM_ParseExt` accepts a null text.
- **Fn's shared `q3map_sun` branch differs from retail.** It skips the rest
  of the line and, after a short line, the following line too. Quake III
  1.32 reads six words and returns to its loop, as the Jedi engines do.
  Only malformed lines differ.
- **`s_hothsky`'s light image.** Its `q3map_lightimage` names
  `textures/skies/hoth_up.tga`, which no pak holds. Fn's developer warning
  about it is the data's.
