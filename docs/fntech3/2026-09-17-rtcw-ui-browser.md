# RTCW UI browser queries — 2026-09-17

Opening the installed RTCW MP **Join Server** menu previously stopped at
unsupported UI import 75 (`UI_LAN_GETSERVERCOUNT`). The menu now opens and
renders its empty local-server list. This is menu runtime evidence, not a
claim of complete browser or network compatibility.

## Implementation and sources

`code/client/cl_ui.cpp` routes 14 RTCW LAN query imports to the existing shared
handlers: ping queue/count/clear/info, server count/address/info/ping,
visibility, visible-ping updates, reset, status and sorting. RTCW list IDs are
translated before dispatch, without modifying the module's argument frame or
reading unused arguments. Ping queue indices and address arguments are left
unchanged. Output-buffer lengths and the separate ping-result span are checked;
the null-buffer, zero-length server-status query/reset form remains valid.

The original source contracts provide these observed IDs:

| List | SP | MP | Shared destination |
| --- | --- | --- | --- |
| Local | 0 | 0 | `AS_LOCAL` |
| Global | 2 | 1 | `AS_GLOBAL` |
| Favorites | 3 | 2 | `AS_FAVORITES` |
| MPlayer | 1 | 3 | Unavailable/empty |

Invalid IDs also select an empty list. The retired MPlayer service is never
silently replaced with global results. The adapter only applies to RTCW;
existing Q3 and QL dispatch is preserved.

Sources are the user's original RTCW SP/MP references under
`E:/_SOURCE/_CODE/`, corresponding to the original
[SP shared contract](https://github.com/id-Software/RTCW-SP/blob/master/src/game/q_shared.h),
[MP shared contract](https://github.com/id-Software/RTCW-MP/blob/master/src/game/q_shared.h)
and their `src/ui/ui_public.h` / `src/client/cl_ui.c` declarations and dispatch.
Those GPLv3-or-later references supply contract observations only; this adapter
is independently written GPLv2-or-later code, with no reference implementation
text incorporated. Attribution is recorded in `CREDITS.md`.

## Validation

- `python -m unittest discover -s tests -p rtcw_module_cvar_tests.py -v`:
  one compiled execution test passes. Both profiles cover source translation,
  invalid IDs, argument-frame preservation, unused arguments, ping/address
  non-translation, supported imports and malformed output buffers.
- `python -m unittest discover -s tests -p 'ui_*tests.py' -v`: 32 shared UI tests
  pass, including Q3/QL ABI and native-boundary checks.
- The Windows x86 development client builds successfully with Meson's
  `fntech3` target. The ordinary client is rebuilt after removing temporary
  capture controls. This slice does not rerun or supersede the prior camera
  checkpoint's complete-suite evidence.

Runtime evidence is under `.tmp/screenies/`, on Windows x86 with user-supplied
installed RTCW content and working tree based on
`a80f24099284604326ec41dc41b4f73d7442ecd0`:

- `runs/rtcw-mp-server-browser-no-csm-capture-2/manifest.json` records the exact
  command, installation path, modules and outcome. The single approved menu
  click opens Join Server; the engine exits normally without an engine error.
- `rtcw-mp/server-browser-no-csm-capture-2.png` is the inspected, untouched
  1600x1200 engine `screenshotPNG` output. CSM is disabled and `r_picmip` is 0.
- The run uses an isolated writable home, forced windowed mode and an
  unswitched private desktop. There is no OS input or screen capture. The
  temporary home is removed and installation inventory is unchanged.

Artifact SHA-256 identities:

| Artifact | SHA-256 |
| --- | --- |
| Ordinary rebuilt client | `c50e364c6b4d6f9fe97361c1403021dc81bb25950a75f163b22048494cb0076e` |
| Private capture client v2 | `39a443b0f1632b4ef5fb3bfe341f0684ac4388f87e711d85ff7c342000b83fcb` |
| GLX renderer | `1d5a93ca976183cbee982ad0fab0c2a970b5da603d25f66e70804ec731a64235` |
| UI source | `05234c33f389892c92f72befda69dfd4df19237226900195f6d56fb848d07775` |
| Restored ordinary `cl_main.cpp` | `1af70946515c465024f044255667eea759f98a5c3914d6c98c010a12a2259505` |

## Limits and screenshot observations

Remote server discovery/join, favorites add/remove, legacy browser imports,
retail interoperability and sustained gameplay remain open. The screenshot
batch also reaches an RTCW MP first-person Allied soldier on local `mp_beach`
and SP's knife/health-HUD view after the opening cinematic, without movement
or combat input. These observations do not close gameplay gates.

Earlier capture scripts left menu-appended join commands behind their own
queued quit, and delayed a warmup restart behind waits. Those provisional
images are excluded from the gallery and retained with their observations.
The final local-world scripts issue team commands before capture waits and
capture MP before the warmup transition; the final MP frame has no connection
warning. This is a capture-sequencing correction, not evidence of a production
networking fix.

Known visual issues remain labelled in the gallery: missing RTCW SP character
bodies/skin assembly in cinematic frames, and triangular gaps in the Q3 menu
logo. They persist in captures with CSM off and `r_picmip 0`.
