# RTCW autosave write results

An explicitly approved, single native-UI click on the SP mission briefing's
ready arrow reached `g_playerStart=1` and `G_SaveGame 'autosave\escape1'`.
The installed game immediately reported insufficient disk space. The drive
had 801,432,182,784 bytes available. The engine had written the save header,
but discarded the filesystem's byte-count result and returned zero through
`G_FS_WRITE`. The retail game interprets zero as failure.

The [original SP game dispatcher](https://github.com/id-Software/RTCW-SP/blob/master/src/server/sv_game.c)
and [MP dispatcher](https://github.com/id-Software/RTCW-MP/blob/master/src/server/sv_game.c)
return the actual write result. Both cgame dispatchers do the same, while
their UI write traps return zero. fnTech3 now retains that profile-specific
contract. Existing Q3/QL write traps still return zero. The owned-handle
filesystem wrapper forwards actual success or failure; it never substitutes
the requested length for a failed write.

The game rename import needed to complete a save is also admitted. Both
filenames are copied from checked native/QVM buffers. They are restricted to
the active writable game directory and existing path/extension restrictions.
Missing destination directories are created, then the existing platform file
replacement operation commits the temporary save. A failed replacement retains
the temporary file and previous save. Unlike the inherited generic rename
fallback, this path never truncates the previous save through a copy fallback.
Equivalent filename spellings are harmless no-ops.

Two executable regression cases exercise the real writer, owned-handle wrapper,
profile dispatch cases and rename function. They cover partial writes, actual
file contents, failure results, owner isolation, invalid handles and pointers,
QVM span/terminator bounds, zero lengths, filename aliases, path traversal,
blocked extensions, nested autosave directories and failed replacement.
Nine focused checks pass on Windows x86 (5.075 seconds), Windows x64 (3.986
seconds), and Linux x86 with address/undefined-behavior sanitizers and leak
detection (33.773 seconds). Both Windows client/dedicated builds and all three
renderer modules build; complete Linux cgame/server translation units pass.

The uninstrumented default executable SHA-256 is
`0fe65e9db5a676e34e15994d17bee3bf923f5a54154e49c1a41d6af03dc18205`;
the experimental executable is
`3f9a1a55cad53b530dd0813466dbf540c3da0fbe2ca5003ee74f8a935fb80a88`.
The first full suite found an isolated cvar fixture that needed a declaration
for the new rename dispatcher. That fixture is corrected. The final suite
passes 111 Meson tests with two skips; its Python suite passes 1,457 tests
with eleven skips. Evidence is frozen in
`.tmp/rtcw-manager/file-write-source-identities.json`.

The failing runtime used temporary one-use input instrumentation, retained in
`.tmp/rtcw-manager/ready-click-instrumentation.json` and its patch. The original
client source was restored exactly after compilation. The private desktop
was never switched, no OS pointer was moved, and captures came from the engine.
The test installation stayed unchanged and the writable home was removed.
Its log and manifest are under `.tmp/rtcw-manager/approved-ready-click-sp/`.
The click permission was consumed by that run.

The user then approved up to five fresh startup retests. The first retest
completed the initial mission autosave and current save: both files contained
218,188 bytes with SHA-256
`4dd54f98de554c0928e868f7c2226b12d7480ca5ee823069560f590cb5788194`.
It then stopped on the renderer's explicit missing scene-fog guard. The late
engine PNG was inspected and shows the real RTCW error dialog. This establishes
save creation, not save restoration or playable SP. The instrumented executable
SHA-256 was `a759f0c1a0b42abb2058aaa53d406d1109c22bcac9c4a4c14cf6bb4f5f3cbe7c`;
the source was restored byte-for-byte before the run. The manifest, log,
instrumentation and engine PNGs are in `.tmp/rtcw-manager/startup-retest-1/`.
The run exited normally, consumed exactly one ready click, left the installation
unchanged and removed its isolated writable home.
Rebuilding the restored client produces SHA-256
`f408da0d7c771bb3f9ea70bf649bdf4a591caa7d0c67484680d399de2432f79a`;
the pre-instrumentation executable identity above belongs to the earlier build.

Six default-build Q3/QL GLx, Vulkan and RTX-raster smoke captures were inspected.
Q3 shows the world, weapon and HUD; QL shows its native join UI and world.
All runs were windowed on unswitched private desktops with no input supplied.
The audited manifest and captures are in `.tmp/rtcw-manager/file-write-live/`.

Independent source review also found that the current absolute engine menu
pointer path is forwarded as RTCW's relative 640x480 mouse deltas. The approved
probe addressed the native UI directly using its documented delta contract;
it does not establish ordinary mouse-input compatibility. This mismatch,
mouth amplitudes, further gameplay imports, save/load restoration and sustained
campaign execution remain open.
