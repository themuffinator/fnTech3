# Native Q3 game-to-client snapshot round trip

The shared Windows x86 client now has runtime evidence for native Q3 qagame,
cgame and UI DLLs running together in an in-process loopback session. The
independently authored fixture pair verifies selected module-owned player-state
and entity fields after they cross the server adapters, snapshot path and client
native boundary. Engine source and binaries are unchanged for this checkpoint.

The qagame fixture uses its guarded 516-byte entity and 468-byte player-state
prefixes inside larger module strides, as described in the
[qagame checkpoint](2026-09-08-q3-native-qagame.md). At client begin it writes a
known state and links entity 17 with SVF_BROADCAST to exclude PVS visibility
from this ABI test. Thirty server run-frame callbacks later it changes two
fields. The cgame fixture validates both states in guarded 0xd20c-byte Q3 native
snapshots using the pinned Q3 field offsets and 208-byte entity stride.

| Contract fields checked | Initial value | Later value |
| --- | --- | --- |
| Player clientNum / weapon | 0 / 7 | Unchanged |
| Player origin | 123.25, -64.5, 32 | Unchanged |
| Player stats[0] / stats[15] | 113 / 321 | 127 / 321 |
| Player persistant[15] / ammo[7] | 1234 / 47 | Unchanged |
| Entity number / eType / weapon / generic1 | 17 / 2 / 7 / 91 | Unchanged |
| Entity position trajectory base and origin | 123.25, -64.5, 32 | Unchanged |
| Entity time | 4242 | 5678 |

The client must observe the initial state before the later state. Both checks
validate snapshot bounds, timestamp agreement, entity membership and guard
preservation. The later player commandTime must be nonzero. Separately, the
qagame fixture receives a guarded 24-byte usercmd with nonzero serverTime and
weapon 7, then copies its command time into the module-owned player state.
The weapon comes from the cgame's normal SetUserCmdValue import. No keyboard,
mouse, joystick or other user input is controlled or injected.

The log contains successful initial-state, initial-snapshot, live-usercmd,
updated-state and updated-snapshot markers, plus the earlier cgame/UI checks
and clean shutdown callbacks. The `live delta` marker names denote changed
state, not independent proof of a particular wire delta-compression encoding.
The process exits normally with code zero and no forced stop. This does not
establish remote peer interoperability, nonzero movement/button handling,
every player-state field or renderer output correctness.

The existing `FNTECH3_Q3_CGAME_RUNTIME_EXE` opt-in now runs two cases: the
earlier native cgame/UI case with retail qagame QVM, and the all-native pair.
To run only the new case:

```powershell
python tests/q3_native_cgame_runtime_tests.py Q3NativeCgameRuntimeTests.test_native_game_snapshot_roundtrip
```

Set `FNTECH3_Q3_NATIVE_PAIR_OUTPUT` to a new directory for logs and manifests.
The cgame fixture is compiled with `FNTECH3_NATIVE_PAIR` only for this case;
the qagame live behavior is selected by `fntech3_probe_live=1`. Exact executable,
DLL/source and fixture identities, launch arguments, markers and log hashes are
in the [evidence record](2026-09-08-q3-native-snapshot-roundtrip.json).

The recorded run uses isolated stock pak0-pak8 assets, a temporary writable home,
sv_pure=0 for synthetic native modules, windowed GLx, disabled audio/input/Steam
and a private unswitched desktop. No OS or engine screenshot is needed for these
data-transfer checks. Generated DLLs and proprietary assets remain outside Git.

The Windows Meson suite passes 110 tests with one OpenAL skip; its Python
aggregate runs 1,274 tests with five skips. Q3 retail-QVM/native-client, all-native
loopback, dedicated invalid-binding recovery and QL native probes were enabled.
The Linux runner import check reports two explicit Windows-only skips.

The remaining ABI, retail-interoperability and release gates remain open. The
known optional QL-font warnings occur under Q3, followed by the existing Windows
font fallback; no new unrelated engine defect was found.
