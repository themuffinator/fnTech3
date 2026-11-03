# FnQL upstream review through c919ae7

fnTech3 took FnQL at `fce44fe714564621e6ceb79e3da7c53089646af2`. FnQL has
since gained 26 commits, up to `c919ae7` (2026-11-03). This record lists each
one, says whether it applies to fnTech3, and how a ported change is scoped to
the profiles it belongs to. Everything ported is a reference port of FnQL's
own GPL-2.0-or-later text into this GPL-2.0-or-later tree; the retail
addresses quoted come from FnQL's records and were not re-measured here.

## Ported

| FnQL commit(s) | Change | fnTech3 scope |
| --- | --- | --- |
| `40a17d7`, `040c90e` | Vulkan swapchain recreation: composite alpha chosen from the surface's supported flags, a zero or "any" current extent replaced by the last drawable size and clamped to the surface's limits, image count kept at or above its minimum, transfer usage masked by `supportedUsageFlags`, and a partially initialised device shut down (`REF_KEEP_CONTEXT`) before `vk_initialize` zeroes `vk`, so a renderer restart after a driver fault never creates a second swapchain on the same window | Both Vulkan renderers (`renderervk`, `rendererrtx`), every profile |
| `a6ae2e4` | Renderer profiling reads the real clock. `ri.Milliseconds` is `CL_ScaledMilliseconds`, the per-frame `cls.realtime`, so every start/end pair inside a frame measured 0: `com_speeds` reported `rf` and `bk` as 0 and charged all renderer time to `cl`. `R_ProfileMilliseconds()` (`renderercommon/tr_profile_clock.h`, over `ri.Microseconds`) now times the frontend and backend totals, the CSM and dlight shadow atlases, cinematic uploads, `r_showImages` and the Vulkan pipeline warm-up. Rendered time, motion blur, `r_dlightTest` and the path tracer's adaptation timers stay on the frame clock | GL, Vulkan and RTX renderers, every profile; diagnostics only |
| `35c2ad2` | Quake Live key activity is `0x1000`. Retail `CL_CmdButtons` ORs `0x1000` (quakelive_steam.exe `0x004B5C67`); `0x0800` is a separate stop-follow input that retail `SpectatorThink` tests (qagamex86.dll `0x10033F82`), so every physical key press, a `+scores` key among them, left follow mode (FnQL issue #5). fnTech3 already chose the key-activity bit per profile; the `ql` branch now sends `FNTECH3_QL_BUTTON_ANY` (`0x1000`) and `ql_game_abi.h` names `0x0800` `FNTECH3_QL_BUTTON_STOP_FOLLOW`. `q_shared.h`'s `BUTTON_ANY` stays Quake III's `2048` | `ql` only; Q3, IHV and Q3Test keep `2048`, the others keep their own values |
| `cec8b9d` | Under Wine/Proton the registry and Program Files name the prefix's stub Steam client, so `Sys_SteamPath` found no install and the Steam provider was refused. After every existing lookup fails, the executable's directory is accepted when it holds the profile's retail assets | Windows `Sys_SteamPath`, every Steam-sold profile, through the existing per-profile install validation |
| `9aacae5`, `0c3dcc6`, `68d9d43`, `5d689e3` (+ test fixes `f6fa75c`, `1582041`) | WebUI bind prompt: mouse buttons and wheel notches answer a pending capture instead of clicking or scrolling the page, Escape stays with the prompt instead of toggling the menu, and keyboard and character events are withheld from the page while capture is armed (retail's window procedure skips its injection then, quakelive_steam.exe `0x4f1cf7`). The wheel injects vertical first, 30 per call, on both halves of a notch (retail `0x4f2870`, `0x4f3420`) | `ql` only (the WebUI runs only under Quake Live) |
| `fada298` | `CL_InitCGame` hides the WebUI after entering `CA_LOADING` and before `CG_INIT`, as retail does (`0x004B04C0` calls `0x004F24D0`), so a console `demo` no longer plays under the main menu (FnQL issue #7) | `ql` only: `CL_WebHost_HideForGameTransition` returns at once under every other profile |
| `dbb6104` | Steam avatars are fetched by one platform worker thread instead of two synchronous provider calls from the cgame's draw path (FnQL measured ~70 ms stalls). A lookup queues a transfer and answers 0, retail's answer for an avatar not yet delivered; `CL_Steam_AvatarFrame` uploads at most one finished avatar per frame. `FNQL_Steam_Shutdown` joins the worker before the provider unloads | Steam profiles: the frame hook runs beside `CL_SteamP2PFrame` under `usesSteam` |
| `6f504de` | Hex cvar integers. Retail quakelive_steam.exe computes `cvar_t::integer` with `sscanf( s, "0x%08x" )` for hex strings (helper `0x4cccc0`), and the retail cgame reads packed colours such as `cg_enemyUpperColor "0x2a8000FF"` through `vmCvar_t::integer`; `atoi` gave 0, so forced enemy models drew white (FnQL issue #11). `Cvar_ParseQLInteger` (`qcommon/cvar_integer.h`) takes the hex path for strings that start with `0x` | `ql` only. FnQL changed every cvar; here `Cvar_IntegerValue` uses it only while Quake Live is the selected profile, because Quake III's and the other profiles' engines compute the integer with `atoi` alone (inference from the Quake III source and the reference ports, not re-measured in each retail binary) |
| `5a48e5d` (part) | Two retail qagame import adapters. Slot 173 (`BotPredictVisiblePosition`) forwards all five arguments: retail qagame pushes five, and botlib wrote the prediction through a NULL target. Slot 200 (`STEAMID_QUERY`) returns the 64-bit SteamID in EDX:EAX, which retail `ClientConnect` stores as both halves (qagamex86 `0x1003adc4`); the 32-bit return left the upper half undefined | `ql` native qagame imports only |

## Not ported

- `5a48e5d` (rest): `vm_gameProvider`, `FnQL_GetModuleInfo` and the
  `fnql_qagamex86.dll` provider path host the clean-room
  [FnQL-game](https://github.com/themuffinator/FnQL-game) companion module.
  That module is at bring-up (four commits, no retail-wire claim), and its
  ABI mirror check compares against FnQL's tree, not this one. It is a new
  feature for the `ql` profile rather than a compatibility correction; it can
  be ported, gated to `ql`, once fnTech3 means to host that module.
- `e218408` and `c919ae7`: `sv_lockstep` exists to diff the retail qagame against
  FnQL-game (FnQL-game's gate G3). Its recorder knows only Quake Live's
  player and entity layouts; a fnTech3 version would need a design for every
  profile, and fnTech3's own retail comparisons use synthetic trace modules.
- `318b5e3`: already in fnTech3 (WebUI native-request and resource
  scheduling, imported with the FnQL feature-parity work).
- `14bdc9b` (VS Code release configuration) and `ff25f23` (FnQL changelog
  reset) have no fnTech3 counterpart.

## Validation

Observed in this tree on 2026-11-03, Windows x86:

- Source and unit tests for each slice: `renderer_profile_clock_tests.py`,
  `csm_shadow_cache_source_tests.py`, `steam_provider_source_tests.py`,
  `ql_services_source_tests.py`, `cvar_integer_profile_tests.py`, the WebUI
  wiring, backend, join-menu and launch-transition source tests, and
  `tests/cvar_integer_tests.c` (hex, limit and decimal cases).
- `cvar_range_check_tests.py` drives the real cvar system: a packed colour
  reads 0 under a non-QL profile, its hex value on create and on write under
  Quake Live, `1920x1080` stays 1920, and writes take `atoi` again once the
  profile is not Quake Live.
- `rtcw_player_interaction_tests.py` and `wolfet_input_tests.py` compile the
  production `CL_CmdButtons` and assert `0x1000`, never `0x0800`, for an
  unbound key under `ql`, with every other profile's value unchanged.
- Fixtures that compile the edited functions were given the new names
  (`R_ProfileMilliseconds`, `Com_GameIsQuakeLive`) and pass.
- The shared `meson/build/win32-dev` configuration (MSVC x86, strict
  warnings, every profile enabled) built all 420 outstanding steps, and its
  message-codec, protocol-layout, game-ABI-layout, cvar-integer, Steam and
  WebUI native tests passed (14 of 14).
- Every Python test file that names an edited source, 265 files run one
  process per file, passed (918 s). The working tree also held another
  session's uncommitted Medal of Honor audio work at the time.

Not observed: no runtime run of a retail Quake Live client covers the
spectator follow fix, the bind prompt, the wheel, the demo-start hide or the
avatar worker; no Proton run was made; no driver fault was provoked to
exercise the Vulkan restart path; and no retail module was loaded to see a
tinted forced enemy model. FnQL's own runtime observations for those are in
its commits and are not fnTech3 evidence.
