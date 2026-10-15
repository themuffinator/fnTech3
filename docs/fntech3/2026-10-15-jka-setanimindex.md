# Jedi Academy frame-indexed shaders: `RF_SETANIMINDEX`, 2026-10-15

The [one-shot animation slice](2026-10-15-jka-oneshot-animmap.md) left one
render flag refused. Academy's client game sets `RF_SETANIMINDEX`
(renderfx `0x80000` in multiplayer, `0x20000` in single player) on
`EF_SHADER_ANIM` movers, with the frame to show in `skinNum`. The render
adapter refused the flag, and a refused entity drops the session. The
first locked door in view therefore stopped the game. The flag now
crosses the adapter as an engine-owned field, and all three renderers take
an animMap stage's frame from `skinNum` for the two Academy profiles. On
`mp/siege_hoth`, locked doors show their red light and unlocked doors their
green one, in GLx, Vulkan and RTX. The
[JSON companion](2026-10-15-jka-setanimindex.json) records the binaries,
retail identities, commands and outcomes. No compatibility gate closes.

Implementation is independently authored. Retail `jamp.exe` 1.01 and
`jasp.exe` were read with capstone. Pinned OpenJK was used only to find
which game entities set the flag (reference observation; no text
incorporated).

## Observed, retail

**R_BindAnimatedImage** (`jamp.exe` `0x4ac3d0`, `jasp.exe` `0x472fd0`),
after the video, `r_fullbright` lightmap and one-frame cases:

- `jamp.exe` `0x4ac42f` tests the current entity's renderfx with `0x80000`.
  If set, the index is the entity's dword at `+0x84` (`skinNum`)
  (`0x4ac438`), and the time path is skipped.
- `jasp.exe` `0x473026` tests `0x20000` and takes `+0x68` (`0x47302f`).
- Only the time path clamps a negative index to 0 (`0x4ac465..0x4ac469`).
  The `skinNum` path jumps past that clamp to `0x4ac46b`.
- A one-shot bundle compares the index with its count, signed (`cmp`/`jl`,
  `0x4ac476`). At or past the count it binds the last frame. Below it, it
  binds `frames[index]` with no modulo (`0x4ac490`).
- Any other bundle binds `frames[index % count]` with a signed `idiv`
  (`0x4ac48b`).
- So a negative `skinNum` makes retail read before its frame array, on
  either kind of bundle. `jasp.exe` has the same sequence
  (`0x473058..0x47307c`).

**Which shaders and entities use it.** A scan of every multiplayer map's
entities and brush-model surfaces in the installed paks
(`.tmp/jka-setanimindex/scan_movers.py`) looked for the three movers the
game marks `EF_SHADER_ANIM`:

- a `func_door` with spawnflag 16 (locked);
- a `func_static` with spawnflag 4;
- a `func_usable` with an `endframe`.

Only `mp/siege_hoth` has such movers whose shaders animate: 33 locked doors
on `textures/hoth/door_02_onoff` and `textures/hoth/h_door2_onoff`
(`assets1.pk3` `shaders/hoth.shader`). Each shader's last stage is a
two-frame `oneshotanimMap 1` of a red picture and then a green one, added
with `GL_ONE GL_ONE` and `glow`. `mp/siege_desert` has 26 locked doors,
none on an animated shader. A locked door starts at frame 0, and using it
through its target unlocks it at frame 1 (OpenJK's game code, reference
observation; the live runs below show both).

## Change

- **`tr_types.h`.** `jkaRenderEntity_t` gains `setAnimIndex`.
- **`jka_render_adapter.hpp`.** Renderfx `0x80000` is now served. It sets
  `jka.setAnimIndex`, has no Quake III flag alias and keeps `skinNum` as
  sent. Single player's `0x20000` stays refused here: it is not a
  multiplayer flag, and `jka-sp` has no client-module boundary yet. Every
  other refused bit still refuses.
- **`tr_shade.c` in all three renderers.** For `jka-sp` and `jka-mp`, when
  the current entity has `setAnimIndex`, the frame index is its `skinNum`.
  The functions are R_BindAnimatedImage (GLx, RTX) and
  R_AnimatedImageDescriptor (Vulkan). The existing negative clamp, one-shot
  hold and wrap still follow. No other profile reads the field.

**Fn presentation (invariant 11).** The flag decides which picture a stage
binds, and nothing else. Fn's lighting, glow, bloom, fog, sorting and
post-processing draw the door's stages as they draw any stage. Nothing is
left out, and there is no retail-exact option, because there is no
presentation to choose between.

**Engine choice that differs from retail:** a negative `skinNum` shows
frame 0. Retail reads memory before its frame array. In OpenJK's game
code (reference observation) a locked door's `s.frame` is 0 or 1 and a
`func_usable`'s counts up from 0, and the live runs below show only frames
0 and 1. No module is known to send a negative one.

## Unit (Source)

`tests/jka_oneshot_animmap_tests.py` compiles each renderer's own frame
selection, sliced from `tr_shade.c`. With the flag set and an eight-frame
bundle, for both Academy profiles and at three shader times:

| `skinNum` | 0 | 3 | 7 | 8 | 9 | 17 | -1 | -9 | `INT_MAX` | `INT_MIN` |
|---|---|---|---|---|---|---|---|---|---|---|
| looped bundle | 0 | 3 | 7 | 0 | 1 | 1 | 0 | 0 | 7 | 0 |
| one-shot bundle | 0 | 3 | 7 | 7 | 7 | 7 | 0 | 0 | 7 | 0 |

- Without the flag, `skinNum` changes nothing.
- No other profile reads the field.
- A one-frame bundle still binds its only frame.
- A view with no current entity keeps the timed frame.
- Every assertion from the one-shot slice still holds.

`tests/jka_render_tests.cpp`:

- `0x80000`, alone and with `0x400` or `0x2000`, crosses as
  `jka.setAnimIndex`. `skinNum` is kept, a negative one included, and no
  Quake III flag is added.
- With `0x1000` beside it, the entity is still refused.
- `0x20000` is refused.

`tests/jka_entity_override_tests.py` no longer lists `0x80000` among the
refused flags. The other test files that compile the adapter or the frame
selection all pass, in the Visual Studio x86 shell:

- the cgame host, render, spatial-sound and tag tests;
- the entity-refraction, sky-portal, surface and UI Ghoul2 tests;
- the RTX and Vulkan safety tests;
- the shader-animation, Alice shader, model shader, particle and
  render-host tests.

## Live (Live subset)

There are two runtimes, from a `git archive` of `aa0e7049` built with
`win32-dev`'s options and strict warnings:

- `.tmp/runtime/jka-setanimindex-before`, `aa0e7049` itself;
- `.tmp/runtime/jka-setanimindex`, the same export with this slice.

The harness is `.tmp/jka-setanimindex/probe.py`. Each run is windowed, with
`net_enabled 0` and no input, on a private desktop, with an isolated home.
It hosts `mp/siege_hoth` in a free-for-all as a spectator and takes four
views with `setviewpos`, one engine `screenshotPNG` each:

- the locked pair `*14`/`*15`, from both sides;
- the `walldoor_unlock` pair `*3`/`*4`, from both sides.

The entity override `.tmp/jka-setanimindex/siege_hoth-doors.ent` is the
level's own entities with these changes:

- One added `trigger_always` targets `walldoor_unlock`, so that pair
  unlocks as the level starts.
- The level's `misc_skyportal` is left out. The adapter at `aa0e7049`
  refuses its view, which [the siege slice](2026-10-15-jka-siege.md) has
  served since.
- NPCs, vehicles, turrets, emplaced guns, ammo and shield units and the
  siege item are left out. With them, the client game asks for the unserved
  import 312 (`CG_G2_SKINLESSMODEL`).
- The ion-cannon `fx_runner` is left out. At `aa0e7049` it needs the
  unserved camera-shake primitive, which
  [the camera-shake slice](2026-10-15-jka-camera-shake.md) has served
  since.

Results:

- **Before**, in GLx, Vulkan and RTX, the session stops as the first locked
  door is drawn: `unsupported Raven renderer entity flags (type=0,
  flags=0x80040)`. Nothing is captured.
- **After**, every run reaches its scripted end with exit code 0. The
  `shaderlist` lists both `_onoff` shaders. The captures count saturated
  red and green pixels (`.tmp/jka-setanimindex/colours.py`; sheet
  `runs/doors-sheet.png`):

| run | locked, front | locked, back | unlocked, front | unlocked, back |
|---|---|---|---|---|
| GLx after | 1228 red, 0 green | 1111 red, 0 green | 0 red, 12158 green | 0 red, 7396 green |
| Vulkan after | 1040 red, 0 green | 1109 red, 0 green | 0 red, 11866 green | 0 red, 7366 green |
| RTX after | 1029 red, 0 green | 1100 red, 0 green | 0 red, 11272 green | 0 red, 6777 green |
| GLx control | 0 red, 480 green | 0 red, 647 green | 0 red, 12160 green | 0 red, 7404 green |

A third runtime, `.tmp/runtime/jka-setanimindex-head`, is the same slice
built on `77f0ab93`, which serves the sky portal and camera shake. With an
override that keeps the level's `misc_skyportal` and ion-cannon runner
(`siege_hoth-doors-portal.ent`), GLx shows the same result: 1158 and 1117
red pixels on the locked pair, 12182 and 7407 green on the unlocked pair.
With only the `trigger_always` added (`siege_hoth-unlock.ent`), the session
still stops, on import 312.

The control is a throwaway GLx renderer (`.tmp/runtime/jka-setanimindex-diag`)
that accepts the flag but leaves the frame to the stage's clock. There the
locked doors also turn green. The red therefore comes from `skinNum`, not
from the clock: at speed 1 a one-shot two-frame stage holds green after one
second of level time. The binaries' and the retail installation's hashes
are unchanged after every run. Retail `E:\Games\Jedi Academy` was read
only.

## Open

- `jka-sp` shares the renderer path but has no client-module boundary, so
  its `0x20000` has no adapter to cross yet.
- `mp/siege_hoth` as shipped still stops on the client game's Ghoul2
  import 312. Its sky portal and the ion cannon's camera shake stopped it
  at `aa0e7049`, and both are served now.
- No `func_static` or `func_usable` with an animated shader was found in
  the installed multiplayer maps, so the `endframe` count-up path has not
  been run live.
- No capture compares the doors with retail's renderer.
