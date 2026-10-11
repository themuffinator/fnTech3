# Medal of Honor's hidden carriers and visible effects, 2026-10-11

The two gas barrels near the start of `m1l1` broke without their fire, smoke
or flying fragments. Their game events and animation commands reached the
retail client game, which submitted the effects, but fnTech3 rejected their
draw calls. The same rejection affected other effects made by hidden
carriers. Both scene submission adapters now accept those calls.

The [JSON companion](2026-10-11-mohaa-effects.json) records the build,
installation, checks and local evidence paths. This is Source, Unit and
Live subset evidence; no compatibility gate closes. The original
SafeDisc-encrypted `MOHAA.exe` cannot run here, so these are observations of
the retail modules running in fnTech3, not comparisons with the retail
engine.

## Cause and contract

Read from the legitimate 1.00 installation and observed at runtime:

- `m1l1` has two `func_barrel` entities named `barrel`, of type `gas`, at
  `(-3750, -488, 372)` and `(-3833, -467, 369)`. Their destruction selects
  `models/fx/barrel_gas_destroyed.tik`. The TIKI's client animation commands
  produce sprites and TIKI debris.
- A diagnostic build observed the retail client game's animation-entry
  request (`Frame_Commands`, frame `-3`) succeed. It then submitted
  `explosed2.spr`, `explosed.spr`, `air_explosion.spr`, `vsssource.spr` and
  `models/fx/bp_debris3.tik`, all with `renderfx 0x90`. The old adapters
  rejected these at imports 88 and 89 because bit `0x80` was set.
- The retail client game already decides whether to draw the hidden
  network model: `cgamex86.dll` at `0x3002c1dc..0x3002c1ea` tests bit
  `0x80` and jumps past import 88's call at `0x3002c277`. An effect's
  inherited flags do not constitute another
  request for the engine to hide the submitted sprite or fragment.

[OpenMoHAA at the pinned revision](https://github.com/openmoh/openmohaa/tree/ab43a0def6c4feccedae36fdb58c2e007d7e4c35)
corroborates this contract **[O]**: `BarrelObject::BarrelKilled` creates the
hidden effect carrier; `cg_modelanim.c` skips drawing a network model with
`RF_DONTDRAW` but processes its effects; `cg_commands.cpp` inherits that
flag into spawned effects; `RE_AddRefEntityToScene` and
`RE_AddRefSpriteToScene` in `renderergl1/tr_scene.c` do not filter it.
These are reference observations; no external implementation text was
incorporated by this change.

An asset inventory found 1,378 TIKIs, including 160 under `models/fx/` and
122 containing `rendereffects ... dontdraw`. That establishes the breadth
of the authoring pattern, not 122 successful runtime tests. The correction
is at the common submission boundary, with no barrel-name or map exception.

## Effects and events audit

| Path | Result |
| --- | --- |
| Network client-game messages | Protocol 6's messages reach the retail `ParseCGMessage` export through its message-reader imports. There is no engine whitelist of explosion or impact event types. The grenade and bazooka fixture exercises this route. Existing framing tests cover widths, capacity, malformed writes and the terminating continuation bit. |
| TIKI init and animation commands | Model registration and `R_SpawnEffectModel` invoke the retail client's init-command export; `Frame_Commands` and `Frame_CommandsTime` expose the authored commands. The barrel's entry command was observed before the failing draw submissions. No replacement game-event interpreter is needed. |
| Continuous emitters | The retail client owns their spawn rates, particle lifetimes, motion and emission commands. Steam, water, fire/smoke and electrical arcs use the same corrected sprite path. |
| Sprite and model submissions | Imports 88 and 89 now preserve the module's submissions regardless of inherited `RF_DONTDRAW`. Shader sprites, beams, rail cores/rings, lightning and portals retain their existing type mapping. TIKI debris retains its pose, scale and surface state. |
| Lights, polygons, marks and trails | Existing adapters serve dynamic lights, polygons (including the client's beams), terrain/brush marks, smoke lighting and swipe strips. The corresponding light, decal, terrain, flare and swipe regression tests still pass. This change does not establish retail visual parity for every one of these effects. |
| Unserved imports | All 19 unserved client-game slots are marked unreferenced in the measured 1.00 import table. None was called by these probes. Unknown imports still fail by name rather than silently succeeding. |

Only `code/client/cl_mohaa_cgame.cpp` changes runtime behavior. Original
MOHAA flags continue to travel in the MOHAA renderer record; they are not
cast into Quake III's flags, where `0x80` means something different. Other
profiles' submission paths, protocol tables and game modules are untouched.

Presentation follows invariant 11: particles and debris use fnTech3's
shared sprite, shader, TIKI and scene pipelines in GL, Vulkan and RTX, with
their existing lighting and sidecars. Effect lights continue into its
dynamic-light pipeline and sounds into its spatial OpenAL/alias pipeline.
No rendering or audio feature is switched off as a compatibility fallback,
and no retail-exact mode is introduced. The probes mute audio, so they do
not establish audible fidelity; RTX coverage here is raster mode.

## Validation

- Rebuilt the Windows x86 `win32-dev` client in place with the existing
  `mohaa-development=true` preset. The source tree includes pre-existing
  unrelated changes; the JSON identifies the tested binary and dirty base.
- `tests/mohaa_effect_submission_tests.py` compiles the production scene
  adapters against a recording renderer. It checks all supported scene
  types, flagged and unflagged sprites and debris, retained geometry,
  color, timing, pose and flags, invalid inputs and scene reset. Restoring
  either old rejection independently makes the fixture fail. It passed
  with Windows x64 test headers, with the actual Windows x86 MOHAA headers,
  and on Linux x86 under WSL.
- All 72 `mohaa*_tests.py` tests passed, as did six shared renderer-parity
  checks and the three native MOHAA module-ABI, savegame and load-record
  tests. The complete multi-profile test suite was not rerun for this
  MOHAA-only submission correction.
- The 16 player-manual tests passed with the updated guide links.
- `scripts/probe_mohaa_effects.py` supplies an original effects-only
  mission script in an isolated home. It destroys both original gas
  barrels, exercises empty/oil/water barrel debris, grenade and bazooka
  network explosions, electric arcs, steam, a waterfall, fire/smoke, a
  mortar and a tank explosion. It takes early, late and settled engine
  screenshots, plus a before frame. Screenshot existence and one network
  event per explosion are automatic checks; visible effects are inspected
  separately. It does not exercise campaign progression.

All thirteen cases completed on each renderer, with their visible effects
inspected in engine captures. Each run wrote 40 screenshots and carried
one grenade and one bazooka client-game message. Local reports and captures:

| Renderer | Evidence directory | Result |
| --- | --- | --- |
| GL | `.tmp/mohaa-effects/glx/` | 13 cases; 40 captures; both network events; installation unchanged |
| Vulkan | `.tmp/mohaa-effects/vk/` | 13 cases; 40 captures; both network events; installation unchanged |
| RTX, `rtx_rt_mode 0` | `.tmp/mohaa-effects/rtx-raster/` | 13 cases; 40 captures; both network events; installation unchanged |

The probe runs windowed on a private desktop, with mouse/joystick input
disabled and no injected input. It uses the engine's `screenshot` command
only. Retail assets and DLLs are read from the installation; all writable
files remain under `.tmp/mohaa-effects/`. Installation size/mtime
inventories are unchanged after each run. Reproduce with an empty output
directory, changing `glx` to `vk` or `rtx`:

```powershell
python scripts/probe_mohaa_effects.py --renderer glx --output .tmp/mohaa-effects/glx
```

The fixture intentionally puts effects from other missions on `m1l1`.
Its mortar and tank sound aliases (`arty_exp_sand`, `explode_tank`) are not
in that map's active alias set and the game reports them missing. This
limits the fixture's sound coverage. An existing startup `Unknown command
"fov"` diagnostic also remains. Neither prevented the visual probe.

Unrelated environment issue: drive C: had no free space, causing an initial
MSVC link to fail. Directing that build's `TEMP` and `TMP` to the project's
temporary directory allowed the rebuild to complete. No installation or
system-wide settings were changed. The initial RTX probe also encountered
the already-recorded lack of ray-query acceleration structures in this
32-bit driver; the completed RTX run explicitly selected raster mode.

Cleanup of the verified disposable project-local probe files was rejected
by automatic approval review ("blocked by policy"), including a second
attempt naming each inspected absolute directory explicitly. The temporary
homes, diagnostic output and caches therefore remain under
`.tmp/mohaa-effects/`; none is part of the source change.
