# Enemy Territory's demo-recording cvars

## Scope

Retail Enemy Territory draws its demo-recording notice in the cgame, not the
engine. The cgame reads three read-only cvars that the engine owns:
`cl_demorecording`, `cl_demofilename` and `cl_demooffset`. The `wolfet`
profile registered none of them. The retail cgame then created its own at
their defaults, and ET showed no recording notice at all.
`docs/fntech3/2026-09-23-profile-large-text.md` recorded this as open when it
gave `wolfet` no engine notice, as retail has none.

## Observed

`et.exe` ET 2.60b, SHA-256
`4d1326c5936b61c5f878fc017e4aa0abddf13e008139e31fce2f13d57b26243e`, was read
with capstone. Addresses are virtual addresses in the image. `Cvar_Get` is
`0x42de60` and `Cvar_Set` is `0x42e4b0`. `clc.demorecording` is `0x15f51f0`,
`clc.demofile` is `0x15f5200` and `clc.demoName` is `0x15f51b0`.

| Site | What `et.exe` does |
| --- | --- |
| `CL_Init`, `0x40d7c7`-`0x40d804` | Registers `cl_demorecording` as `"0"`, `cl_demofilename` as `""` and `cl_demooffset` as `"0"`, each with flags `0x40`, `CVAR_ROM` |
| `CL_Record`, `0x40f2d0` | Opens the file and returns on failure. Then, from `0x40f310`, sets `clc.demorecording` and `cl_demorecording` `"1"`, copies the static name `0x895730` into `clc.demoName` with a 64-byte `Q_strncpyz` (`0x41faf0`), sets `cl_demofilename` to it and `cl_demooffset` to `"0"` |
| `CL_StopRecord_f`, `0x410ee0` | Prints `Not recording a demo.` and returns if nothing records. Otherwise closes the file, then from `0x410f33` sets `"0"`, `""` and `"0"` and prints `Stopped demo.` |
| `SCR_DrawDemoRecording`, `0x413d40` | Returns if nothing records. Otherwise sets `cl_demooffset` to `va( "%d", FS_FTell( clc.demofile ) )` (`0x41fdd0`, `0x430930`). It does nothing else |
| `SCR_DrawScreenField`, `CA_ACTIVE` | Calls `CL_CGameRendering` (`0x4011f0`) at `0x413f49`, then `SCR_DrawDemoRecording` at `0x413f51` |

`et.exe` has no `RECORDING %s: %ik` string and no `cl_drawRecording`.
`CL_Record` has a third caller, `0x4130c3`: ET's `cl_autorecord`, which builds
its own `demos/%s_%i_%i.dm_%d` name and leaves the static name alone. That
path publishes the last name given to `record`, or an empty one.

The retail cgame, `cgame_mp_x86.dll` in `etmain/mp_bin.pk3`, contains all
three names, ` demo %s: %ik `, `RECORDING%s%s` and `cg_recording_statusline`.
It was read in memory from the pak.

The Enemy Territory GPL source, commit
`40342a9e3690cb5b627a433d4d5cbf30e3c57698`, agrees with `et.exe` at every site
(`client/cl_main.c`, `client/cl_scrn.c`). Its cgame registers the three
`CVAR_ROM` with the same defaults (`cgame/cg_main.c`). `CG_DrawDemoRecording`,
at the end of `CG_Draw2D`, draws `RECORDING demo <name>: <k>k` at `5`,
`cg_recording_statusline` (default `9`). It does so when `cl_demorecording` or
`cl_waverecording` is set and the status line is not `0`. It formats the demo
part into a 128-byte buffer. `CG_Draw2D` is not called while the limbo panel
is up; `CG_LimboPanel_Draw` runs instead.

These are inferences from the above:

- The cgame draws the previous frame's size, since the engine sets the offset
  after the cgame has drawn.
- The 64-byte `clc.demoName` keeps a name within 63 characters. The cgame's
  128-byte buffer was sized for that.

## Change

- `CL_RegisterDemoRecordingCvars`, called from `CL_Init`, registers the three
  cvars under `wolfet` alone, with `et.exe`'s defaults and `CVAR_ROM`. Every
  other profile registers none and leaves the pointers null.
- `CL_SetDemoRecordingCvars` publishes a start and a stop. `CL_Record_f` calls
  it once the file is open, with the demo's name without `demos/`, cut to 63
  characters. `CL_StopRecord_f` calls it when a recording was running. The
  values are those `et.exe` sets.
- `SCR_DrawDemoRecording` sets `cl_demooffset` to the file's position every
  active frame while recording. It does this before any check that hides this
  engine's own notice, as `et.exe` checks nothing else. `wolfet` still gets no
  engine notice. The call site already follows the cgame, as in `et.exe`.
- Writes go through the three pointers and never by name. The engine's
  `Cvar_Set` creates a cvar that does not exist, so a write by name would give
  the cvar to other profiles as well.

## Evidence

- `tests/demo_recording_cvar_profile_tests.py` compiles the client's own
  `CL_RegisterDemoRecordingCvars`, `CL_SetDemoRecordingCvars`, `CL_Record_f`,
  `CL_StopRecord_f`, `SCR_DemoRecordingNotice` and `SCR_DrawDemoRecording`.
  The cvar system, file system and drawing are stubbed. A write to a missing
  cvar creates it, as the engine's does. The test runs every profile id, and
  no profile selected. Under `wolfet` it checks:
  - the registration, flags and defaults;
  - the values at start, per frame and at stop;
  - that the offset still updates with the engine notice turned off and during
    a single-player demo;
  - a refused second `record`, and a frame and a `stoprecord` with nothing
    recording;
  - an automatic name, a 100-character name, and a file that fails to open.

  Every other profile, and no profile, must create and write no cvar at all.
  Two source checks follow. `CL_Init` registers once, and no client source
  spells the three names outside the registration. The retail call order is
  pinned too. Against the previous source, all 14 failing checks are
  `wolfet`'s, and the registration check fails. The call-order check passes.
- `tests/large_text_profile_tests.py` and `tests/demo_record_profile_tests.py`
  compile the same functions. They gain stubs for the new names and pass
  unchanged otherwise.
- The commit's tree, `8849e8d` with this change alone, was exported and built
  for Windows x86 with MSVC, `debugoptimized` and strict warnings (`/WX`). The
  RTCW, Elite Force and Enemy Territory development options were on, as in
  `meson/build/win32-dev`. It built without a warning. `fntech3.exe` had
  SHA-256 `8f6da8b75b8c0a0aec1678b49b2e49d57104fe078314ed2b41fc15d2c494eec1`
  and `fnql_glx_x86.dll`
  `4f68ec83b7279fea8f4ef77699c75b6aeb3d7022b550ff1335302fc96ae70b91`.
- Runtime, same build. The ET client runs in developer builds only.
  `scripts/probe_wolfet_client.py` ran it against the Steam ET 2.60b
  installation:
  - on a private desktop, windowed at 640x480;
  - with an isolated home and no input;
  - on the GL renderer, with a 256 MB hunk.

  After `+devmap oasis` and 180 frames, the active action did four things.
  It ran `record probe`, closed the limbo panel with the retail cgame's own
  `openlimbomenu`, and waited 120 frames. It then printed the three cvars
  before the engine's `screenshotPNG`. Both runs exited 0, drew windowed and
  logged no engine error. Both left the installation's files unchanged.
  - Recording. The log shows `cl_demorecording` `1`, `cl_demofilename`
    `probe` and `cl_demooffset` `9223`. The retail `cgame_mp_x86.dll` drew
    `RECORDING demo probe: 9k` at about (5, 9) of the capture. That capture
    had SHA-256
    `1dadaa65726e59e6b8ea6f93c8ad72d43cb4eecb3341c43584618936faf71bf2`.
  - Stopped. The same run with `stoprecord; wait 30` before the print shows
    `0`, an empty name and `0`. Its capture has no notice. That capture is
    otherwise the same spectator view, with the warmup text and HUD drawn,
    and had SHA-256
    `77bfc0840e9fa671f24b190b8cbd683cf9e5ffbeb4b0fdd0bda4457fcc23fdc4`.
  - Each probe manifest has `activeObserved` false. It looks for `CA_ACTIVE`
    or `entered the game`, and this log prints neither. `record` refuses to
    run before the client is active, and here it recorded.

  The captures derive from retail assets, so they stay under `.tmp`.

## Open

- `et.exe` also registers `cl_waverecording`, `cl_wavefilename` and
  `cl_waveoffset` for its `wav_record` and `wav_stoprecord` commands. This
  engine has neither command. The cgame registers the three itself at their
  defaults, so its demo notice does not depend on them.
- Names differ from `et.exe` in three ways. The published name comes from the
  name this client records under.
  - Automatic demos are named `demo-YYYYMMDD-HHMMSS`, where `et.exe` counts
    `demoNNNN`.
  - An explicit `.dm3` extension is stripped, where `et.exe` keeps any
    extension.
  - Only the published name is cut to 63 characters, not the file's.
- `et.exe`'s `cl_autorecord` quirk is not reproduced. This client's automatic
  recording runs `record`, which publishes its own name.
- Release builds still refuse the ET client. The runtime check covered one
  map, one renderer and one installation, and the notice was compared by eye.
  The recorded demo was not played back; ET demo playback is not implemented.
