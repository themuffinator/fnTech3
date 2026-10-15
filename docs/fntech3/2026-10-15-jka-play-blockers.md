# Jedi Academy local play: ended clips and distortion copies, 2026-10-15

After the [spawn-effect slice](2026-10-15-jka-spawn-effect.md), a local
`mp/ffa1` join reached play but two separate refusals stopped it. This slice
resolves both. A windowed jka-mp client hosting the map with retail default
settings now plays a scripted 35-second session to its end in GLx, Vulkan
and RTX. The [JSON companion](2026-10-15-jka-play-blockers.json) records the
binaries, commands and outcomes. No compatibility gate closes.

Retail `jamp.exe` 1.01 and the 1.01 `cgamex86.dll` were read with capstone.
Implementation is independently authored.

## Ended animation clips

**Refusal.** About 12.5 s into play, a bolt query on Kyle stopped with "bolt
pose is outside its served hierarchy/animation boundary". The clip on bone 52
ran frames 4..6 at speed 0.05 with flags `0x08`: an override with neither
loop nor freeze. At clock 13,517 its frame had passed 5, the last one. Fn's
timing called that end "unmeasured" and refused every query of it.

**Observed, retail.** In the timing routine (`0x497290`), a forward clip
whose frame is past `end - 1.0f` (binary32, `0x497340..0x497353`) and has no
loop bit takes this path:

- With the freeze bits `0x48`, it holds the last frame
  (`0x497565..0x497590`).
- Otherwise it clears the bone record's animation bits (`0x4975a6`:
  `flags &= 0xffffff27`, i.e. `~0xd8`) and returns without writing a frame.

The bone transform (`0x497860`, call `0x497960`) then leaves that bone on the
frames it inherited, and every later query finds no clip (`0x4e368d` tests
`0x18`). The clip query (`0x4e3640`) calls the same timing into locals it
never initialised. Called before any build has cleared the clip, it would
return that stack as the frame.

**Change.** `code/ghoul2/ghoul2_server.hpp` adds `ClipEnded`, the retail
end test in binary32 arithmetic. A build that reaches an ended clip leaves
the bone on its inherited frames and clears the bone's flags word, exactly
as retail's record is cleared; the clip setter then has nothing to blend
from. The clip query and the client game's bone-frame import (305) answer an
ended clip as an inactive bone once a build has cleared its word. Before
that, they keep a named refusal, because retail would publish uninitialised
memory. Jedi Outcast's own timing is unchanged.

## Distortion copies

**Refusal.** With `cg_renderToTextureFX` at its retail default of 1, the
retail client game adds a second copy of the spawn shield with renderfx
`0x2400`: distortion plus forced alpha (`cgamex86.dll` 1.01, sites
`0x3004a98a` and `0x3004db31`). Fn's render adapter refused the distortion
bit.

**Observed, retail.** The renderer builds an `internal_distortion` shader
with an ARB vertex program (`0x4b6e63`). Before drawing a `0x2000` entity,
the backend (`0x48af26`) copies the screen region behind it into a texture.
The copy only bends the background seen through the model; it adds no colour
of its own.

**Change.** The client game's entity import validates a distortion record
as an ordinary entity (type, shape fields and remaining flags still refuse
by name) and then does not draw it. A Ghoul2 distortion copy is validated
the same way.

**Fn presentation (invariant 11).** Fn has no entity refraction pass. Its
liquid refraction snapshot serves only liquid surfaces, and its `screenMap`
stage is first-person environment mapping, so neither applies. Omitting the
copy removes only the bending; the shield itself is the first, ordinary
entity and is drawn. A refraction pass for such entities is open work, and
`cg_renderToTextureFX 0` remains the retail way to turn the copy off.

## Unit (Source)

- `tests/raven_ghoul2_server_tests.cpp` (`CheckAnEndedClipIsCleared`)
  covers the end test at the live refusal's own numbers. It checks loop,
  freeze, zero and reverse speeds, and a future start. It also checks that a
  bolt query past the end is served and clears the word, that the clip query
  refuses before a build and answers inactive after one, that a frozen clip
  holds, and that a blend set over a cleared clip has nothing to blend from.
- `tests/jka_cgame_bolt_tests.py`: import 305 refuses an ended clip until a
  bolt query's build clears it, then answers an inactive bone.
- `tests/jka_render_host_tests.py`: `0x2400` records, plain and Ghoul2, are
  consumed without a draw. A distortion record with another unsupported
  flag, an unsupported type or an invalid field still refuses.

## Live (subset)

Runtime `.tmp/runtime/jka-spawn-effect`, rebuilt from the shared working
tree. That tree also held other sessions' uncommitted Alice renderer work.
Each run used the spawn-effect slice's harness: windowed, private desktop,
loopback only, no input. It joined the battle with `cmd team free`, closed
the menu, and captured once a second, 30 times, at normal speed. The only
setting changed was `cl_menuBlur 0`, so the scene is not softened behind
the closed menu's focus. `cg_renderToTextureFX` stayed at its retail default.

GLx, Vulkan and RTX each ran the session to its end and quit normally, with
no refusal and 30 engine captures in play. Each run passed the 12.5-second
point where the ended clip had stopped play, and the spawn shield's
distortion copy was submitted and consumed.

## Validation

The commit's tree alone was exported and built: HEAD `27273055` plus this
slice's files, with no other session's work. All 1,541 Windows x86 targets
built with strict warnings. Its Meson suite passed 265 tests, with 2 skipped
and none failed, including the wrapper that runs the Python suite. Release
packaging passed 48, with one symlink case skipped. The 32 Python files that
touch Ghoul2, the Academy render host or the client game passed 111 tests
and 212 subtests in the export.

## Remaining

- Fn draws no refraction for distortion copies.
- No capture yet isolates a spawn Line's or bolt's appearance.
- Longer play, other maps, other characters and combat have not been
  exercised here.
