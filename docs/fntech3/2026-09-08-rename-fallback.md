# Preserve the source when rename fallback fails

`FS_SV_Rename` (including download finalization) and `FS_Rename` both tried a
copy after a failed native replacement, then removed the source unconditionally.
`FS_CopyFile` could return early when it could not open the source/destination
or create the destination path. Consequently, an unwritable destination could
cause the completed temporary download or another source file to be deleted.

The copy helper now returns success explicitly, including checking the final
destination close result. Both callers remove the source only after success;
a returned failure raises `ERR_DROP` with a source-retained diagnostic. Empty
files use a nonzero allocation size so allocator-specific `malloc(0)` behavior
cannot turn a valid empty copy into an allocation error. Existing fatal
allocation/read/write error paths remain unchanged. This independently
authored correction incorporates no external implementation text.

## Verification and limits

`tests/rename_fallback_tests.py` compiles the production copy and both rename
functions. Real temporary files and a nonempty destination directory reproduce
the original source deletion. Windows and Linux pass after the correction:
the source contents and destination sentinel survive the failed fallback,
successful fallback copies remove the source, empty copies succeed and native
replacement still works. A controlled destination-close failure also verifies
source retention. Native replacement failure and close errors are controlled
in this harness; it does not induce physical storage faults.

The change preserves the source, not atomic destination replacement or
durability. A failed copy may leave partial destination content, and the
existing `FS_Rename` destination-removal behavior is unchanged. Broader
replacement/rollback semantics and power-loss handling remain open.

The [validation record](evidence/2026-09-08-rename-fallback.json) records the
shared Windows x86 build, full suite and both profiles' HTTP download/reconnect
non-regressions. Those windowed probes use isolated homes, disabled input
capture and engine screenshots on an unswitched private desktop. Retail
fixture copies stay temporary and are not committed or published.

The rebuilt shared binaries complete HTTP download/reconnect and pure
admission for both profiles with matching archive hashes, normal process
exits and world/HUD or native join UI screenshots. The complete Meson suite
passes 110 tests, zero failures and one unavailable OpenAL-loopback skip;
the aggregate Python suite runs 1,257 tests with seven skips.

Existing image/font warnings and hosted CI's account billing/spending blocker
remain unresolved.
