# Q3 native modules and pure selection — 2026-09-07

Native-module probes at `62f97d1` exposed a profile-boundary defect: Q3
retained QL's pinned-native client-module policy after connecting to a pure
server. Native UI/cgame loaded, but their missing QVM pak references caused
pure admission to fail. The original GPL Q3 client instead forces compiled
QVMs for UI and cgame whenever the server is pure.

The shared client now applies that Q3 rule explicitly in both loaders.
Non-pure Q3 startup mode selection remains unchanged. QL retains its pinned
native-image policy; the pure-proof format and server validation are unchanged.
This implementation was independently written from the observed policy in
`code/client/cl_ui.c:1150` and `cl_cgame.c:732` of the local
[id Software GPL source release](https://github.com/id-Software/Quake-III-Arena).
The repository already credits that source in `CREDITS.md`.

The local source archive has no verified commit identity. Its Release Win32
project source lists were built without source edits using MSVC x86 into
qagame, cgame and UI fixture DLLs. GPL compatibility was checked against its
source headers and `COPYING.txt`. Source/header hashes, compiler response
arguments and DLL hashes are recorded in the
[evidence bundle](evidence/2026-09-07-q3-native-modules.json). DLLs remain
disposable test artifacts and are not added to the engine distribution.

Validation at `62f97d1` plus the loader correction:

- Production UI/cgame selection blocks execute in a harness covering both
  profiles, pure/non-pure state, all four module modes, and present/absent
  native pins. Windows and Linux pass; Q3 pure never consults a native pin.
- The executable pure-proof test and 62 UI/cgame bridge tests pass. The
  Windows MSVC x86 client rebuild passes.
- Q3 non-pure loads all three legacy native DLLs, reaches active state,
  receives snapshots, renders world/HUD and shuts down normally.
- Q3 pure initially loads native UI/cgame, then reloads retail QVMs at pure
  admission. Its server continues using native qagame. Admission, active
  state, snapshots, world/HUD rendering and normal shutdown pass.
- QL pure still reloads pinned native UI/cgame and reaches active state,
  renders the world/native join UI, receives snapshots and exits normally.

All runtime sessions used windowed GLx, separate temporary writable homes,
unswitched private desktops, and disabled input capture, audio, WebUI and Steam.
Q3 used stock retail pak0–pak8; QL used its legitimate installation. Images
were written by the engine screenshot command and visually inspected. No
proprietary inputs, images or native DLLs were committed.

The initial build harness placed linker response arguments on separate lines,
so it produced no DLLs and the first runtime probe fell back to QVMs. That
probe is excluded from native evidence. Joining the response arguments on one
line produced all three DLLs; subsequent logs verify the actual loading mode.
The pre-correction pure rejection is preserved as a failure in the bundle.

These short local sessions do not close the entire native ABI, mod, extension
or retail-peer matrix. Existing resource/font warnings remain unresolved;
no new unrelated engine issue was identified.

## Native map restart

At `7552cd0`, an additional Q3 non-pure session with the same native fixture
modules executed server `map_restart 0` after initial client admission. The
server shut down/reloaded native qagame, reinitialized q3dm6 and admitted the
client again; the native UI/cgame reloaded and the world/HUD rendered with
continuing snapshots. This observed path includes map reinitialization, not
a claim that every restart used the fast in-place path.

A matching QL pure session restarted its retail API-10 qagame on campgrounds.
The client processed the `map_restart` server command and continued receiving
snapshots and rendering the world/native join UI. Both clients and servers
exited normally. Post-restart engine PNGs were visually inspected.

The [map-restart record](evidence/2026-09-07-native-map-restart.json) includes
artifact hashes, full arguments/configs and lifecycle excerpts. The windowed,
isolated-home, no-input-control restrictions above remain in force. Audio,
WebUI and Steam were disabled. This adds one bounded restart per profile;
extended restart stress, all mod/native ABI paths and authenticated peers
remain open. No new unrelated issue was found.
