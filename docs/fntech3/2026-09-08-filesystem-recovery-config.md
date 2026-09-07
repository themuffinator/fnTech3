# Filesystem recovery config isolation — 2026-09-08

`FS_Restart` can recover from a configuration that cannot read `default.cfg`
by restoring the last valid base/game paths. Its recursive recovery sets
`execConfig`, which queues a config reload when the restored game directory
differs from the cleared history. That queue unconditionally used QL's
profile-root hardware/replicated files, even in a Q3 process.

The recovery queue now selects the immutable profile. Q3 queues only its
mod-local `exec q3config.cfg`, unless safe mode suppresses it. QL retains its
hardware-file/legacy fallback and client replicated-config behavior. This is
the recovery scheduling path, not every ordinary game-directory transition.
No search-path or pak/pure validation rule changes.

Validation at `99f89e8` plus this patch:

- An executable harness runs the production scheduling block, including
  clearing `execConfig`, across both profiles, client/dedicated compilation,
  safe mode, QL hardware-file presence, changed/unchanged directories and
  requested/unrequested config recovery. Windows and Linux pass. Q3 never
  queries profile-root file existence or queues QL config files.
- All three existing startup/persistence config tests and the QL filesystem
  source suite pass on Windows.
- Windows MSVC x86 client and dedicated rebuilds pass.

No runtime recovery claim is made: a full invalid-filesystem rollback with
live modules still needs separate evidence. The complete filesystem/config
gate remains open. No external implementation was incorporated and no new
unrelated issue was found.

Follow-up at `6dd1b37`: a second executable harness now runs the entire
production `FS_Restart` function, including its static saved-path history and
recursive rollback. Filesystem startup/read outcomes are controlled stubs;
the actual recovery control flow and config queue execute unchanged. Each
fresh process establishes its own history. The 96 combinations per platform
cover client/dedicated builds, both profiles, safe mode, QL hardware config
presence, base/mod game directories, initial failure, successful rollback and
failure of the restored filesystem itself. Windows and Linux pass.

Assertions verify shutdown/startup counts, pure-list clearing before restored
cvar writes, restored base/game paths, checksum feed, factory refresh, exact
config commands and the expected fatal/drop outcome. After successful
rollback, another restart queues no stale commands. A failed restored path
stops with a fatal error rather than recursing indefinitely. This strengthens
the recovery evidence but still does not substitute for live filesystem and
module teardown/reload coverage.
