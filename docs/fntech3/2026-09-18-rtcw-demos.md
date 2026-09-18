# RTCW demo recording, playback and retail demo exchange

RTCW demos were a listed open gate in the [RTCW matrix](COMPATIBILITY.md#rtcw-gate-matrix).
This slice makes demo protocols profile-owned, records and replays RTCW MP demos,
and exchanges `.dm_60` files with retail `WolfMP.exe` in both directions. It also
settles RTCW SP: the original SP engine cannot replay SP demos at all, so the SP
profile records under original SP's own precondition and refuses replay
explicitly. Q3 and QL demo recording and playback pass unchanged. The
[machine-readable record](2026-09-18-rtcw-demos.json) holds identities, commands
and per-run results.

## Defects found

- **RTCW recordings were named as Q3 demos.** `CL_Record_f` set `dm68compat` for
  every wire profile except QL's, so stopping an RTCW MP recording renamed it
  `.dm_68`. Replaying that file under Q3 would hand RTCW message bodies to Q3's
  entity and player-state field tables.
- **RTCW could not replay its own demos.** The demo protocol table held only
  Q3's 66, 67, 68, 71 and QL's 91, and the per-profile filter admitted every
  non-QL protocol to every non-QL profile. An RTCW profile therefore searched,
  and would have parsed, Q3 demos with Q3 field tables, including the pre-1.27
  `.dm3` fallback, while refusing `.dm_49` and `.dm_60`.
- **Playback took the Q3 wire profile for any non-QL protocol**, and marked
  every protocol at or below 68 as Q3 legacy framing, including RTCW's 49/60.

## Correction

Demo protocols are now owned per profile, matching what each original engine
writes (`demos/%s.dm_%d` with its single `PROTOCOL_VERSION`, in original
[RTCW SP](https://github.com/id-Software/RTCW-SP/blob/master/src/client/cl_main.c)
and [RTCW MP](https://github.com/id-Software/RTCW-MP/blob/master/src/client/cl_main.c)
`CL_Record_f`):

| Profile | Demo protocols admitted | `.dm3` |
| --- | --- | --- |
| `q3` | 66, 67, 68, 71, in Q3's historical search order | yes |
| `ql` | 91 | no |
| `rtcw-sp` | 49 (recording; replay refused, below) | no |
| `rtcw-mp` | 60 | no |
| `wolfet` | none; ET demo playback is not implemented | no |

Protocols 49 and 60 now map to the RTCW SP/MP wire profiles for playback, and
the playback `compat` flag follows the live-connection rule (Q3 legacy framing
only). Only Q3 wire profiles keep the `dm_68` relabelling fallback, so a stale
flag cannot rename an RTCW or QL recording. External layouts, ordinals and wire
codecs are unchanged. The original sources were read only; no implementation
text is incorporated.

## RTCW SP: the original contract

Two original SP behaviours are now the SP profile's:

- **Recording requires synchronous clients.** Original SP `CL_Record_f` refuses
  with `The server must have 'g_synchronousClients 1' set for demos` and records
  nothing; MP and Q3 turned this into a warning. Retail `WolfSP.exe` refuses with
  that exact line, and records once `g_synchronousClients 1` is set.
- **SP demos cannot be replayed.** The SP cgame asks the game module for each
  player's animation data (`CG_GETMODELINFO` → `SV_GetModelInfo` →
  `VM_Call( gvm, GAME_GETMODELINFO, ... )`), and demo playback runs no game
  module. Retail `WolfSP.exe` was given its **own** recording: it loaded
  `escape2`, reached `LOADING... WolfPlayer`, shut the client down and stopped
  on a fatal-error dialog whose text, read from its console window on the
  private desktop, is `VM_Call with NULL vm`. It failed the same way on the
  fnTech3-written SP demo. fnTech3's cgame boundary returns no model info in the
  same place, which previously produced a recoverable drop after loading the
  map. The SP profile now refuses replay before disturbing the current session:
  `RTCW single-player demos cannot be replayed: its cgame needs animation data
  from a running game module.`

Making SP demos replayable would need a game module running during playback,
which no RTCW engine does; it is not attempted.

## Runtime evidence

All runs use the same Windows x86 `RelWithDebInfo` development build (client
SHA-256 `55adb7cc…d7ff5`, dedicated `4b97945f…c71c`), windowed on unswitched
private desktops with isolated homes, sound and mouse/joystick input disabled.
Console commands from startup configs drive every step; no mouse or keyboard
input was injected and no operating-system capture was taken. Every
installation inventory is unchanged afterwards. Run with
[`scripts/probe_demo_roundtrip.py`](../../scripts/probe_demo_roundtrip.py).

| Run | Demo written | Messages / envelope | Result |
| --- | --- | --- | --- |
| Q3 local `q3dm17`, record then replay | `.dm_68`, 5,389 bytes | 121, terminated, increasing | normal + timedemo (153 frames) complete, exit 0 |
| QL local `campgrounds` FFA, record then replay | `.dm_91`, 9,901 bytes | 240, terminated, increasing | normal + timedemo (153 frames) complete, exit 0 |
| RTCW MP local `mp_beach`, record then replay | `.dm_60`, 14,242 bytes | 121, terminated, increasing | normal + timedemo (152 frames) complete, exit 0 |
| Retail `WolfMP.exe` replays that fnTech3 `.dm_60` | — | same file | two retail screenshots, `nextdemo quit`, exit 0, no error |
| Retail `WolfMP.exe` records against the fnTech3 dedicated server, non-pure | `.dm_60`, 14,629 bytes | 147, terminated, increasing | server `CS_ACTIVE` + `ClientBegin`; fnTech3 normal + timedemo (148 frames) complete |
| Same, server `sv_pure 1` | `.dm_60`, 14,785 bytes | 146, terminated, increasing | pure admission, no unpure drop; fnTech3 normal + timedemo (147 frames) complete |
| RTCW SP `escape2` save, record | unsynchronized `record` refused, nothing written; synchronized `.dm_49`, 23,229 bytes | 121, terminated, increasing | replay refused before loading a map; client exits 0 |

The retail replay of the fnTech3 demo and fnTech3's replay of it show the same
moment: warmup `Match begins in: 17`, mission clock 7:54, spectator view. Every
fnTech3 capture was inspected: Q3 first-person HUD, QL's native join panel with
its demo controls, RTCW MP warmup spectator view and scoreboard, and SP
`escape2` gameplay with the recording notice. Timedemo figures are frame counts
from bounded runs, not performance claims.

## Tests

`tests/demo_profile_selection_tests.py` now reads the production protocol table
and executes the real lookup, name-completion and `demo` command code for every
profile: each RTCW mode finds only its own protocol, never opens a Q3/QL demo or
`.dm3`, refuses every cross-profile file, Q3 never opens an RTCW demo, SP refuses
its own file after finding it, and ET admits nothing.
`tests/demo_record_profile_tests.py` executes the real finalizer for RTCW SP/MP
(including a stale compatibility flag) and a new harness runs the real
`record` command, covering SP's refusal against local and remote servers, SP's
float test, and the unchanged MP/Q3 warning. Both files fail against the
previous source and pass now.

The full Python suite, run after the `rtcw_md3_normal_tests.py` fix below and
before the demo changes, gave 1,679 passed, 67 skipped and two failed; both
failures came from the WolfET session's uncommitted work. The run after the demo
changes reported 27 failing entries. Twenty were this slice's and are fixed:
`tests/ql_demo_source_tests.py` pinned QL as the table's last entry rather than
its membership, and this record's JSON link target did not yet exist, which
failed 19 release-packaging checks. Both files then pass in full. The other
seven are the WolfET session's, listed below.

## Remaining gates

- One map per profile and short, spectator-heavy recordings. Active combat,
  longer recordings, map changes during recording and every stock map remain
  open.
- No malformed or truncated RTCW demo corpus has been exercised; the shared
  reader's existing bounds were not re-verified for RTCW bodies.
- Retail SP and RTCW MP demos from other releases (1.0-era protocols) are not
  admitted; only the installed retail release's protocols were observed.
- Seeking, pausing and frame advance are unexercised for RTCW.
- ET demo playback stays open and belongs to the ET work.

## Unrelated observations

- `tests/rtcw_md3_normal_tests.py` failed whenever pytest ran outside an x86
  developer prompt: clang then targets x64, `q_platform.h` defines `_MSC_SSE2`,
  and the extracted `Q_rsqrt` body lost the `<intrin.h>` include that sits just
  outside it. The test prelude now includes it the same way; the test passes
  on x86 and x64 targets.
- Retail `WolfMP.exe` appends `activeAction` to its command buffer without a
  newline, so the cgame's next command (`openLimboMenu`) joins its last word.
  Probe actions for retail end with `;`.
- Retail `WolfSP.exe` finds savegames under `fs_basepath`, not the home path.
- The in-progress WolfET session's uncommitted changes currently break seven
  checks. `vm.c` now calls `FnTech3_WolfETKeyEventFromInternal`,
  `FnTech3_WolfETMenuFromInternal` and `FnTech3_WolfETMenuToInternal`, which the
  RTCW VM harnesses in `rtcw_animation_handoff_tests.py`, `rtcw_cast_query_tests.py`,
  `rtcw_model_info_tests.py`, `rtcw_module_export_tests.py` and
  `rtcw_tag_handoff_tests.py` do not declare; `tr_rtcw_fog_scene.h` now reads an
  `identityLight` field that `rtcw_fog_scene_tests.py` does not provide; and
  `wolfet_client_services_tests.py` expects the pre-change popup call. None is
  caused or changed by this slice.
