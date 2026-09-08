# Repeated retail dedicated-server transitions

The same Windows x86 dedicated executable passed a 90-second transition series
for each profile after the [world-link restart fix](2026-09-08-world-link-restart.md).
Each process executed three immediate `map_restart 0` commands and two explicit
map changes. All six status checkpoints per profile contained Sarge, Grunt,
Visor and Major. Both processes exited with status zero following RCON quit;
neither required forced termination.

| Profile | Maps observed at successive checkpoint pairs | Restart evidence | Gameplay evidence |
| --- | --- | --- | --- |
| Q3 | q3dm6, q3dm17, q3dm6 | Three shutdown/reinitialization replies containing QVM VM_Restart | Visor scored one point on q3dm17 after restart; retail bot combat appears in the console log |
| QL | servitude, cursed, campgrounds | Three shutdown/reload replies containing native API 10 exports | IN_PROGRESS at all six checkpoints; four bots present, but sampled scores were all zero |

The QL startup configuration requested campgrounds; the retail game selected
servitude before the first checkpoint. Subsequent explicit map commands reached
the requested cursed and campgrounds maps, and each restart retained its current
map. The probe replaces bots after explicit map changes, but does not replace
them across map_restart. This distinguishes bot re-admission after a map change
from survival across a module restart. No stale-world-entity diagnostic occurred
in the console or captured RCON responses.

The reusable [probe](../../tests/retail_bot_transition_probe.py) requires explicit
executable, profile, legitimate asset directory and a new output directory:

```powershell
python tests/retail_bot_transition_probe.py --exe <dedicated-exe> --profile q3 --assets <q3-assets> --output .tmp/q3-transition-run
python tests/retail_bot_transition_probe.py --exe <same-dedicated-exe> --profile ql --assets <ql-installation> --output .tmp/ql-transition-run
```

It uses isolated writable homes, read-only assets, unswitched private desktops
and fresh temporary loopback RCON credentials. It neither controls input devices
nor captures the screen. The startup queue contains no long sequence delaying
game-requested transitions. Status, restart, map and shutdown commands are issued
at recorded times through the private server command channel.

The [sanitized evidence](2026-09-08-retail-transition-series.json) includes both
launch commands/configurations, all RCON replies, asset and engine hashes,
timestamps, base commit and dirty-source identity. A follow-up verifier was added
after execution to require actual restart replies and requested map destinations;
it passes against both saved transcripts. Its source hash is recorded separately
from the probe version used to launch the processes. The same checks now run
automatically in future probes. Negative checks reject a missing restart or wrong
destination. Python compilation also passes. Engine binaries are unchanged from
the preceding checkpoint, whose full suite and platform build results still apply.

This supplies bounded local lifecycle evidence with legitimate modules. It does
not establish long-session robustness, remote human-client admission, pure
negotiation, bidirectional retail interoperability or complete compatibility.

Additional observations: Q3 reports missing optional config/symbol files. QL
reports conflicting registration defaults for bot_grapple (0/1) and g_ammoPack
(1/0), alongside known unavailable-service diagnostics. These warnings did not
prevent this series; their defaults were not changed or independently validated
here. The known retail timestamp-only logging issue remains.
