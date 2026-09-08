# Bot trace sentinel and corrections to the retail runtime evidence

Botlib's `BotInitLevelItems` calls AAS_Trace with passent=-1 to mean no excluded
entity. BotImport_Trace forwarded that value to the server unchanged, where
SV_ClipMoveToEntities attempted a canonical entity lookup and rejected -1.
The callback now translates exactly -1 to ENTITYNUM_NONE. Other indices and
the trace-result transfer remain unchanged. This applies to both profiles;
the retail QL trace-import wrapper is unchanged.

The failure was observed while the legitimate QL game loaded `cursed` after a
map transition. A focused single-map `cursed` run with the corrected callback
loads AAS, admits Sarge/Grunt/Visor/Major and exits normally. The executable
callback regression checks -1, the canonical sentinel, valid indices and -2,
along with every result field the callback transfers. It passes on Windows
and WSL Ubuntu. Windows x86 and the existing Linux i686 dedicated configuration
rebuild; the latter remains an incremental dirty-scratch build, not a clean
release claim.

Investigation also corrected two interpretations from the
[retail bot checkpoint](2026-09-08-retail-bot-runtime.md):

- The probe placed long wait/status/quit sequences in the command queue.
  Game-requested map_restart/next-map commands append to that queue and remained
  behind the planned quit. Consequently, the observed COUNT_DOWN state was not
  valid evidence of a stalled engine timer. A revised probe leaves the startup
  queue clear and uses authenticated loopback RCON only for status and shutdown,
  with a fresh temporary password omitted from the evidence record. This is an
  engine command channel and does not control keyboard/mouse input.
- An isolated x86 process loaded the exact unmodified retail qagame DLL and
  called its observed logging routine with a literal message and a formatted
  integer message. Its file-write import was replaced only by a capture callback.
  Both callbacks received just `  2:03` (six bytes), before any fnTech3 formatting
  or filesystem code ran. Inspection shows the retail routine passes a zero
  formatting capacity after generating its timestamp. This reproduces the
  timestamp-only anomaly inside this retail module version, not in fnTech3's
  write adapter. No retail code was patched or reconstructed.

The isolated logging observation uses version-specific data offsets and a local
routine address. Its module hash, complete independently authored observer,
compiler command and output are retained in the evidence record. Repeating it
requires the same legitimate module hash; its addresses must not be applied to
another module version. It is a diagnostic observation, not a shipping shim or
a public API guarantee.

The revised free-running probe revealed another real failure after the trace
correction: `SV_AreaEntities_r: invalid world entity` during a later map
transition. The later [world-link restart checkpoint](2026-09-08-world-link-restart.md) fixes that reproducer and records free-running match progression. The single-map success here does
not close map-restart, match-progression or retail-interoperability gates.

The [evidence record](2026-09-08-bot-trace-and-probe-corrections.json) preserves
before/after outcomes, redacted launch commands, status samples, executable
hashes and the isolated logger observation. Probes used private unswitched
desktops, isolated writable homes, legitimate read-only assets and no user
input control or screen capture. The failing processes exited with status 1;
the single-map success exited with status 0. No forced stop was required.
Proprietary files and raw logs remain outside Git.

The full Windows suite passes 110 tests with one OpenAL skip; its Python
aggregate runs 1,276 tests with five skips, including the enabled Q3 and QL
native runtime probes.

The known optional config/font and unavailable-service warnings remain. No
additional unrelated issue was discovered.
