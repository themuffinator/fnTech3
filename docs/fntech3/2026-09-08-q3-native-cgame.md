# Exercise the legacy Q3 cgame DLL boundary

A new test-only Q3 cgame DLL exports classic `dllEntry` and `vmMain`, and receives
the variadic syscall pointer. It is independently authored from the pinned Q3
ordinal/layout contracts credited in CREDITS. It contains no game implementation
or proprietary inputs and complements the existing structured QL API-8 fixture.

The actual shared Windows x86 engine logs `found legacy vmMain` for the fixture.
Six checks pass through that boundary: guarded glconfig, configstring/game-state,
24-byte user-command and 0xd20c-byte snapshot transfers, plus frame and console
callbacks. The probe validates dimensions, configstring offsets/termination,
weapon selection, snapshot timestamp/entity bounds and no writes on a rejected
snapshot request. The Q3 cgame initialization supplies weapon and packed-float
sensitivity using the legacy two-argument import. It does not supply QL-only
primary-weapon/FOV arguments.

The opt-in runner builds the DLL with MSVC x86, copies it only to an isolated
home, sets vm_cgame=0 and com_gameProfile=q3, and starts q3dm6 using legitimate
Q3 assets and the existing qagame/UI QVMs. It requires all markers, the legacy
loader log and normal zero exit. The desktop remains private and unswitched;
windowed GLx, disabled input/optional services and loopback binding prevent user
input capture. No input injection or OS screenshot is used. Pure is disabled
for the synthetic DLL, so this is not retail-peer or pure-admission evidence.

Run using PowerShell:

```powershell
$env:FNTECH3_Q3_CGAME_RUNTIME_EXE = "$PWD/.tmp/meson-msvc-x86-release/fntech3.exe"
$env:FNTECH3_Q3_RETAIL_PATH = 'C:/Program Files (x86)/Steam/steamapps/common/Quake 3 Arena'
python tests/q3_native_cgame_runtime_tests.py
```

An optional `FNTECH3_Q3_CGAME_PROBE_OUTPUT` names a new evidence directory.
It receives logs, command/fixture hashes, markers and exit status, not retail
files. The test skips without explicit executable opt-in, and skips on Linux;
no Linux native module evidence is claimed by that skip.

The full Windows suite with both native probes enabled passes 110 configured
checks, zero failures and one unavailable OpenAL skip. Its aggregate passes
1,270 tests with five skips. The formatted Q3 fixture was rebuilt and rerun
separately, again passing all six markers and normal exit.

The [evidence record](2026-09-08-q3-native-cgame.json) preserves this run and
full-suite results. This broadens Q3 native cgame coverage; Q3 native qagame/UI,
retail peers, complete imports, and full compatibility gates remain open. No
new unrelated engine issue was observed.
