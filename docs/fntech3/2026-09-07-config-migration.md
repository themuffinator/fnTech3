# Legacy config migration and shared-home persistence — 2026-09-07

At `6a552c5`, alternating profiles in an isolated writable home exposed a
startup failure. Q3 saved `vm_ui`, `vm_cgame` and `vm_game` as `2` (compiled
QVM). On its first launch, QL imported that legacy `q3config.cfg`, then tried
to load retail modules as QVMs. UI loading failed and entered fatal shutdown;
the probe terminated its owned process after its deadline.

QL's legacy migration now resets these module selectors to native immediately
after executing the imported config. Ordinary user settings still migrate.
Existing `qzconfig.cfg`, Q3 startup, and safe-mode paths are unchanged. Later
QL config/autoexec execution and command-line replay retain override precedence.
This correction applies only when QL takes the legacy fallback, including
dedicated startup's `vm_game` selector; it does not remove QVM support globally.

Validation at `6a552c5` plus this patch:

- Three executable config tests pass on Windows and Linux, including client
  and dedicated compilation. They verify that module resets occur only after
  legacy QL config execution, never in Q3, safe mode or an existing QL config
  path, and retain config/override ordering.
- Windows MSVC x86 client and dedicated rebuilds pass.
- Four sequential client launches share one temporary home: Q3 writes
  sensitivity `3.25`; QL imports `3.25` and writes `4.75`; Q3 restores `3.25`;
  QL restores `4.75`. All four exit normally. QL leaves the Q3 config hash
  unchanged, and the returning Q3 launch leaves both QL config hashes unchanged.

The [runtime record](2026-09-07-config-migration.json) contains
binary identity, commands, test configs, config hashes and console observations.
The probes use stock Q3 assets and the legitimate QL installation, windowed
GLx, disabled mouse/joystick capture and isolated writes. Sensitivity is only
a persistence sentinel; no input was controlled. Proprietary inputs and saved
user configs are not committed. The temporary home from the successful probe
was removed after evidence collection.

This closes the observed migration failure and adds bounded mixed-home
persistence evidence. Broader mod/config/download isolation and extended
migration coverage remain open. Existing font/resource diagnostics
remain unrelated and unresolved.

Follow-up at `57f201a`: the same Windows x86 dedicated executable loaded maps
under both profiles with an isolated legacy config containing `vm_game=2`.
Q3 retained `2`, compiled the retail qagame QVM and loaded q3dm6. QL selected
`0`, loaded retail native qagame API 10 and loaded campgrounds/ffa. Both reached
the post-map script marker and shut down normally. These short startup probes
do not establish a live client connection or sustained dedicated operation.
The [dedicated and suite record](2026-09-07-dedicated-config-migration.json)
contains executable identity, full arguments/configs and log excerpts.

The complete local Meson suite at that revision passed 110 tests with zero
failures and one unavailable OpenAL-loopback skip. Its aggregate Python suite
ran 1,248 tests with seven skips. This used the existing MSVC x86 build with
the latest client/dedicated rebuild and `--no-rebuild`; it is not a clean-build
claim. The earlier demo-header revision `04314c9` also now has a successful
[clean Linux CI run](https://github.com/themuffinator/fnTech3/actions/runs/34166837006).
That CI result does not cover the subsequent config-migration correction.
