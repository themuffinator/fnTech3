# Jedi Outcast's `oneshotanimMap` and `clampanimMap`, 2026-10-15

The [sun keyword slice](2026-10-15-raven-sun-keyword.md) ran every installed
Jedi shader definition through the production parser. Under `jk2-sp` and
`jk2-mp`, 22 of the definitions it still refused name Raven's
`oneshotanimMap` or `clampanimMap` stage keywords, which fnTech3 read for
Jedi Academy alone ([Academy's record](2026-10-15-jka-oneshot-animmap.md)).
This slice measures what both Outcast engines do with
the two words and serves them in exactly the profiles whose engines read
them, in all three renderers. The
[JSON companion](2026-10-15-jk2-animmap-variants.json) records the
binaries, identities, addresses, counts, commands and outcomes. No
compatibility gate closes.

Implementation is independently authored. The retail executables were read
with capstone and pefile; no reference port was consulted for this slice.

## What the retail engines do

### Observed: Jedi Outcast 1.02a

Both engines send `animMap`, `clampanimMap` and `oneshotanimMap` to one
branch of `ParseStage`, compared without regard to case.

| | `jk2mp.exe` | `jk2sp.exe` |
| --- | --- | --- |
| The three compares, in the keyword chain | `0x48cf79`, `0x48cf8f`, `0x48cfa5` | `0x47cda9`, `0x47cdbf`, `0x47cdd5` |
| The shared branch | `0x48d97b` | `0x47d7ab` |
| One-shot byte written | `0x48d9c7` (bundle `+0x51`) | `0x47d7f7` (bundle `+0x4d`) |
| Frame bound | `cmp esi, 8` at `0x48d9ed` | `cmp esi, 8` at `0x47d81d` |
| Wrap argument | `0x48d9f2` | `0x47d822` |
| Image request | `0x48da29` | `0x47d859` |
| `R_BindAnimatedImage` | `0x488300` | `0x478180` |
| One-shot hold | `0x488389` | `0x478225` |

- **The words.** The branch compares the token with `clampanimMap` and
  `oneshotanimMap` once more and keeps each answer as a flag. A plain
  `animMap` leaves both clear.
- **Clamping.** Every frame's image request passes `GL_REPEAT` (`0x2901`),
  or `GL_CLAMP` (`0x2900`) when the word was `clampanimMap`. With it go
  three of the shader's words, each inverted, the same for all three
  keywords; the third is `noTC` (`0x1a88e5c`, `0x131a02c`).
- **One shot.** The branch writes the `oneshotanimMap` flag into a byte of
  the stage's first bundle, and writes it clear for the other two words.
- **Eight frames.** Both engines keep at most 8 frames, Quake III's bound.
  Further names on the line are read and skipped without a warning.
  Academy's engines keep 32 (`jamp.exe` `0x4b42e4`, `jasp.exe`
  `0x47a497`).
- **A missing speed.** With no word after the keyword, the shader fails
  with "missing parameter". The message names `animMap` when the word was
  `clampanimMap` and `clampanimMap` otherwise, the inverse of what was
  read; only the message is affected.
- **The bind.** `R_BindAnimatedImage` takes the frame from the shader time
  and the bundle's speed, and a negative one as 0. A bundle whose one-shot
  byte is set then holds its last frame once the count is reached; any
  other bundle wraps. This is Academy's hold (`jamp.exe` `0x4ac46b`,
  `jasp.exe` `0x473058`).

### Observed: single player's frame override

`jk2sp.exe`'s bind (`0x4781ee`) takes the frame from the current entity's
`skinNum` when its `renderfx` has `0x20000` set, Raven's `RF_SETANIMINDEX`,
and does not clamp a negative one. The one-shot hold still follows it.
`jk2mp.exe`'s bind has no such test. fnTech3 serves the override for
Academy's two profiles alone (`e.jka.setAnimIndex`); Outcast single player
cannot yet run its client game, so the gap is recorded here and left open.

### Inference: what the bytes mean

The bundle offsets are read from both functions: in `jk2mp.exe` the parser
addresses the stage from 8 bytes before the bundle, and the bind addresses
the bundle itself, so the parser's `+0x59` is the bind's `+0x51`; in
`jk2sp.exe` the stage is 12 bytes before it, and `+0x59` is `+0x4d`.

## What fnTech3 now does

- `R_JediClampAnimMapKeyword` and `R_JediOneShotAnimMapKeyword`, in
  `code/renderercommon/tr_jka_shader.h`, accept the two words under the
  four Jedi profiles, as `R_JediShaderProfile` already does for `noTC`,
  `sun` and `lightColor`. Every other profile still ends the stage with
  "unknown parameter" at either word, as its engine does.
- The `animMap` branch of `ParseStage` in GLx, Vulkan and RTX is otherwise
  unchanged: `clampAnimMap` loads its frames with `IMGFLAG_CLAMPTOEDGE`,
  and `oneShotAnimMap` sets the bundle's one-shot bit, which each
  renderer's bind already holds on the last frame.
- The frame bound stays the profile's: Academy's 32
  (`MAX_IMAGE_ANIMATIONS_JKA`), Outcast's Quake III 8. fnTech3 warns about
  the frames it skips, where Outcast's engines say nothing.
- The renamed helpers are the only code change. The bundle's bit keeps its
  name, `jkaOneShotAnimMap`.

The frames enter fnTech3's own image and stage pipeline; nothing about how
they are drawn is taken from retail (invariant 11).

## The installed shaders

Every shader script of each installation's paks was read for the two
words (`frames.py`).

| Title | Stages | Definitions | Words as spelled | Most frames |
| --- | --- | --- | --- | --- |
| Jedi Outcast 1.02a | 32 | 22 | `oneshotanimMap` 11, `oneshotanimmap` 20, `clampanimMap` 1 | 8 |
| Jedi Academy 1.01 | 64 | 54 | `oneshotanimMap` 42, `oneshotanimmap` 20, `clampanimMap` 2 | 8 |

No installed stage has more than 8 frames, so neither bound drops one.
The Outcast definitions are 12 doors, panels and screens of the campaign's
maps (`textures/bespin`, `doomgiver`, `impdetention`, `kejim`, `imp_mine`,
`cairn`; `textures/cairn/door02` is the one `clampanimMap`) and 10
explosions (`gfx/exp/*`, `electricalExplosionSlow`, `surfaceExplosion`).

## Through the production parser

`corpus.py` builds `tests/alice_shader_parse_tests.py`'s host around each
renderer's whole `tr_shader.c` and parses every `name { ... }` definition
of the installed Academy and Outcast scripts under each Jedi profile, and
under Quake III as a control. Before is an export of `f8e1ee98`; after is
the same export with this change. The two commits that landed between it and
this one change no parser source.

| Definitions | Profile | Refused before | Refused after |
| --- | --- | --- | --- |
| Outcast, 1,187 | `jk2-sp`, `jk2-mp` | 69 | 47 |
| Outcast, 1,187 | `q3` (control) | 90 | 90 |
| Academy, 3,391 | `jka-sp`, `jka-mp` | 79 | 79 |
| Academy, 3,391 | `q3` (control) | 871 | 871 |

The same in GLx, Vulkan and RTX. The 22 that now load are exactly the 21
`oneshotanimMap` and 1 `clampanimMap` definitions; no definition newly
fails. Outcast's remaining 47 are 25 definitions with no stage
(compiler-only shaders, which retail rejects too) and 22 that use the stage
keyword `surfaceSprites`.

## Tests

- `tests/jka_oneshot_animmap_tests.py` slices each renderer's `animMap`
  branch and animated-image bind. It now expects both Outcast profiles to
  read the two words, the clamp flag and the one-shot bit set as in
  Academy, 8 frames kept of 40 where Academy keeps 32, and the bind to hold
  a one-shot bundle under `jk2-sp` and `jk2-mp`; the retail addresses are
  named in the header.
- `tests/shader_animation_tests.py` runs the production branch under both
  Outcast profiles, and parses `oneShotAnimMap` in all four Jedi profiles
  to the same frame bound as `animMap`.
- Against the tree before this change, both fail on the new assertions;
  giving Outcast 32 frames fails 6 subtests, and gating GLx's hold to
  Academy fails 1.
- The 27 test files that slice `tr_shader.c`, `tr_shade.c` or
  `tr_jka_shader.h`, with `jk2_shader_wave_names_tests.py` and
  `jka_shader_files_tests.py`, pass: 181 tests and 30,423 subtests.
- An export of `f8e1ee98` with this change builds the GLx, Vulkan and RTX
  modules with MSVC x86 under strict warnings, with no warning.

## Live evidence

None. No Outcast map has been drawn: the multiplayer client still stops at
client-game import 300 before a map draws, and single player stops at
client import 3. The definitions are parsed, not seen.

## Open

- Outcast single player's `RF_SETANIMINDEX` frame override (above).
- The bind's frame is the shader time times the speed, truncated, where
  all four Jedi engines and Quake III take it through a fixed-point
  product (`* 1024`, `>> 10`); fnTech3's shared bind keeps its own
  arithmetic for every profile.
- Outcast's `surfaceSprites` stages.
