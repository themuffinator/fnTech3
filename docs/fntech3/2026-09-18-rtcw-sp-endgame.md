# RTCW SP campaign end: the endgame import

An audit of every RTCW import ordinal against the three dispatchers found one
that the retail SP game module reaches in ordinary play and fnTech3 refused:
`G_ENDGAME`, the call that ends the campaign. This record implements it as
original SP does and verifies it by playing the campaign's final cutscene to its
end. The [machine-readable record](2026-09-18-rtcw-sp-endgame.json) holds the
audit, identities and both runs.

## The import audit

The original SP and MP import tables in
[`code/compat/rtcw_module_calls.def`](../../code/compat/rtcw_module_calls.def)
hold 435 import ordinals across the UI, cgame and game modules. Searching the
client, server, common, compat and botlib sources for each one, as `Call::NAME`
or through the dispatchers' shared-call macros, found all but these sixteen;
each of the sixteen falls through to a named unsupported-import error:

| Import | SP / MP | Original engine | fnTech3 |
| --- | --- | --- | --- |
| `G_ENDGAME` | 2 / — | handled (`ERR_ENDGAME`) | **refused; fixed here** |
| `G_FS_COPY_FILE` | 583 / — | handled (`FS_CopyFileOS`) | refused by name; the SP game calls it only in a Mac build |
| `UI_OPENURL` | — / 98 | opens the URL | refused by name, the same deliberate policy as ET |
| `UI_CM_LOADMODEL`, four old `UI_LAN_GET{LOCAL,GLOBAL}SERVER*` | both | not handled ("Bad UI system trap") | refused by name, as original |
| `BOTLIB_AAS_ENTITY_VISIBLE`, `_IN_FIELD_OF_VISION`, `_VISIBLE_CLIENTS` (300–302) | both | not handled | refused by name, as original |
| `BOTLIB_AI_SET_AVOID_GOAL_TIME`, `_ADD_AVOID_SPOT`, `BOTLIB_AAS_ALTERNATIVE_ROUTE_GOAL`, `_PREDICT_ROUTE`, `_POINT_REACHABILITY_AREA_INDEX` (574–578) | both | not handled | refused by name, as original |

Observed in [RTCW SP](https://github.com/id-Software/RTCW-SP) and
[RTCW MP](https://github.com/id-Software/RTCW-MP) `src/server/sv_game.c`,
`src/client/cl_ui.c`, `src/game/g_save.c` and `src/game/g_main.c`, read only.
After this correction 420 of the 435 are dispatched. The fifteen still refused
are either unhandled by the original engines too, deliberately refused, or
reachable only from a Mac build of the SP game.

## How SP ends

The final map, `cutscene19`, ends its script with `trigger player endmap`,
whose AI script runs `changelevel gamefinished 250`. `gamefinished` is the
keyword for "exit to credits": the game sets `g_reloading` to
`RELOAD_ENDGAME`, and when the delay runs out calls `trap_Endgame`. Original SP
answers with `Com_Error( ERR_ENDGAME, "endgame" )`, which never returns: it
prints nothing, shuts the server down, disconnects, and opens the UI's endgame
menu on the next client frame. fnTech3 had no such import and dropped the
session with an unsupported-import error at the moment the campaign finished.

## Correction

fnTech3 now has `ERR_ENDGAME`. The SP game import raises it; `Com_Error` unloads
the running game module safely, shuts the server down with reason `endgame`,
disconnects, flushes client memory and asks the client to open the endgame menu
next frame, then unwinds the frame. Like original SP it prints no error, sets
no `com_errorMessage`, and a build script does not turn it into a fatal error.
MP has no such import, and only the SP profile sets the menu request. The SP
UI's endgame menu is number 4, translated from the engine's menu as before.

## Runtime evidence

Both runs load the installed `cutscene19` with `+spdevmap`, windowed on an
unswitched private desktop with an isolated home, and play the final cutscene
through, with no movement or combat input. The only input is one click on the
briefing's Ready button, the standing AGENTS.md exception for intro and
briefing screens, delivered by the existing private probe hook
([`tests/rtcw_campaign_probe.hpp`](../../tests/rtcw_campaign_probe.hpp)). To keep
that instrumented build away from the shared working tree, both private clients
were built in a separate git worktree at `dc8416e`: first unchanged, then with
this correction applied. Each ran
[`scripts/probe_rtcw_campaign.py`](../../scripts/probe_rtcw_campaign.py)
`--maps cutscene19 --dwell 180`.

| Client | End of the final cutscene | Frame at 180 s | Exit |
| --- | --- | --- | --- |
| before (`dc8416e`) | `ERROR: Unsupported rtcw-sp game import 2 (G_ENDGAME)`; `Server Shutdown (Server crashed: ...)` | none; the probe stopped the client | forced |
| after | `Server Shutdown (endgame)`, no error | the SP endgame menu: the Gray Matter credits, over the menu background | 0, `Client Shutdown (Client quit)` |

The engine screenshot was inspected. Sound was disabled, so the credits music is
not observed.

## Tests

[`tests/rtcw_endgame_tests.py`](../../tests/rtcw_endgame_tests.py) pins the
SP-only ordinal and menu number, the dispatcher case ahead of the
unsupported-import default, the error path's order and silence, and the
client's next-frame menu request ahead of the ordinary main-menu fallback.

## Remaining gates

- One campaign end, reached from the final map loaded directly, not from a
  save carried through the campaign.
- The endgame menu's own content (credits, statistics) is only observed in the
  captured frame, not compared with retail.
