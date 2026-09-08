# Make the guarded native ABI probe repeatable

The opt-in cgame runner required a loose retail `cgamex86.dll`, launched on the
user's desktop and terminated the engine after observing success markers. It
now accepts a packed `baseq3/bin.pk3` installation, forces QL/windowed settings,
disables input capture and optional services, and runs on a newly created
private desktop that is never switched to the user. The synthetic module is
copied only into a temporary writable home; retail inputs remain read-only.

The engine's activeAction executes the probe command and quits after active
frames. The test requires all twelve import/callback markers and a normal zero
exit. Timeouts and failures terminate only the owned child. Desktop and stream
handles close after it stops, including failed-launch and partial-init paths.
The process helper checks the wait state before reading the exit code, so a
completed child returning 259 is not mistaken for a running process.

Windows integration tests check the child's own desktop name, normal exit,
exit 259, timeout/termination, failed launch and idempotent cleanup. Both pass;
Linux imports the helper without attempting Windows APIs and skips those two
Windows-only tests. The enabled native probe passes against the shared Windows
x86 engine and packed retail QL installation, with all twelve markers, normal
shutdown and no forced termination. The full Windows suite with this probe
enabled passes 110 configured checks, zero failures and one unavailable OpenAL
fixture skip; its aggregate passes 1,269 tests with five skips.

Run from the repository using PowerShell:

```powershell
$env:FNQL_CGAME_RUNTIME_EXE = "$PWD/.tmp/meson-msvc-x86-release/fntech3.exe"
$env:FNQL_RETAIL_QL_PATH = 'C:/Program Files (x86)/Steam/steamapps/common/Quake Live'
python tests/native_cgame_abi_probe_runtime_tests.py
```

Optionally set `FNQL_CGAME_PROBE_OUTPUT` to a new output directory to retain
console/stdout logs and a manifest with the command, binary/fixture hashes,
commit, desktop name and exit state. Output directories must be unique per run;
no retail module or asset is copied into the evidence directory. The variables
remain opt-in so environments without legitimate assets do not claim a runtime
pass. The full suite can enable this probe with the same executable variable.

The [evidence record](2026-09-08-native-runner.json) records this run. Pure is
disabled for the synthetic module; no peer or retail gameplay interoperability
is claimed. No input injection or screen capture is performed. This resolves
the runner limitation reported in the guarded-import checkpoint; broader ABI,
unknown QL snapshot-tail and release gates remain open. No new unrelated engine
issue was observed.
