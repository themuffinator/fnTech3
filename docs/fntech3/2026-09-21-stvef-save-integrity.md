# Elite Force SP save integrity

Elite Force SP now preserves the previous save if writing or closing the
replacement fails. Failed saves no longer print `Done.`, and an I/O failure
while keeping a hub level stops the transition. The implementation is commit
`af3b719`, following baseline `eb48fc4`.

## Failure and correction

The initial extracted-function regression writes real temporary files and
forces the first `fwrite` boundary to fail. Before the correction, the engine
reports success and replaces the old save with the incomplete output:
`original preserved=0; replacements=1`. The
[evidence manifest](2026-09-21-stvef-save-integrity.json) identifies
the baseline source objects and the expected-failure log.

`SV_STVEFSPAppendChunk` already returned failure for a short write, but callers
could ignore that return, including the retail game's `WriteLevel` callback.
The whole-save writer then closed and published `current.sav`. Its ordinary
close function also discarded errors from the final buffered flush.

Each save transaction now retains its first chunk error. Later appends do
no further I/O, and the failed transaction cannot replace the destination.
Negative lengths, missing payloads and lengths beyond the signed filesystem
size boundary also fail the transaction. The opt-in `FS_FCloseFileWrite`
checks both `ferror` and `fclose`, closes even after an existing stream error,
and clears the owned loose handle. It refuses invalid and archive handles.
The existing ordinary close API and other profiles' callers are unchanged.

A failed write/close removes incomplete `current.sav`; a failed atomic
replacement preserves the old destination and retains the complete temporary
file. A subsequent independent save starts with fresh error state. Missing
or reentrant game writers are refused before truncating a temporary file.
The fileless `sv_testsave` callback behavior remains available.

The save command announces completion only on success. A hub transition
distinguishes an attempted transaction that failed from the game deliberately
refusing a save. File creation, write, close and replacement errors keep the
current level running and release its cached screenshot. Deliberate game
refusals retain their existing transition flow, which campaign scripts may
depend on.

The successful chunk framing, field order, checksums and module imports are
unchanged. This is independently implemented engine error handling; no
external code is incorporated. Checked stdio completion is not a guarantee
of durability through power loss, and this change adds no `fsync` policy.

## Executable validation

`tests/stvef_sp_save_write_tests.py` compiles the actual save transaction,
append/import, save command, hub transition, shared write and checked-close
functions. The harness uses owned game callbacks and checksum fixtures with
real temporary files. It exercises 97 primary scenarios plus a successful
retry after each of 70 failed write boundaries across full and automatic
saves. These cover engine/game headers, payloads and trailers; malformed
module chunks; create, stream, close and replacement errors; unavailable and
reentrant writers; fileless test saves; command messages; and hub failure,
success and deliberate-refusal behavior. A failed destination retains its
original bytes; valid output is parsed through the actual chunk reader.

The fixture replacement helper is explicitly a stand-in. Separate existing
platform replacement tests and the real Windows runs below cover production
replacement behavior. The initial pre-fix reproducer was expanded into this
tracked regression after establishing the failure.

Twenty-two focused tests pass on Windows x86. Linux runs the same 22 tests
with one Windows-only skip; the new save regression uses AddressSanitizer
and UndefinedBehaviorSanitizer. Windows x86 and Linux i686 client/dedicated
builds complete. The full Windows suite passes 120 targets with
2 skips; its Python aggregate runs 1,865 tests with
72 skips in 695.345 seconds.

## Production runtime observations

All runs use canonical binaries and user-supplied legitimate modules/assets.
Ordinary save/reload, new-level transition, missing-hub fallback and existing
hub restore complete. Retail `stvoy.exe` 1.20 loads the fnTech3 save, writes
another, and fnTech3 loads that result. Tutorial resumes from time 5,100 at
5,450; the retail round trip advances through 11,750 to 12,100. Each save's
chunk boundaries and final magic parse to the end of the file.

Two independent homes exercise real filesystem errors. One contains an owned
empty directory at `saves/current.sav`, preventing file creation. The other
holds its owned destination save files open without delete sharing, causing
Windows atomic replacement to fail. The legitimate installation is not used
as a fault target.

In both cases, an explicit save and the outgoing Tutorial hub save fail with
no `Done.` message. The two previous destination files remain byte-identical,
and `mapname` remains `tutorial`. The replacement-failure run also retains a
complete, parsed `current.sav`. After each obstruction is removed, a fresh
process overwrites the save successfully and transitions to `borg2`.
Historical before/after hashes were checked during each phase; recovery
intentionally changes those destination paths afterward.

| Runtime group | Sessions | Engine captures |
| --- | ---: | ---: |
| Full saves, hubs and retail round trip | 3 | 5 |
| File creation/replacement failures and recovery | 4 | 4 |
| Refreshed SP and Holomatch manual-stage preflight | 2 | 2 |

The manual-stage SP probe loads the existing MDR scene save in third person;
Holomatch retains Munro through a map restart. Both exit normally. The manual
installation at `E:/Games/fnTech3-tests/stvef-manual` contains six refreshed
engine artifacts from `af3b719`. Its seven settings/save/launcher files
retain their hashes; only the README's engine-provenance paragraph changes.
The original interactive launchers and manual homes are not run.

Every game runs windowed on a private inactive desktop, with mouse/joystick,
audio and feedback disabled. Images come from the engine's `screenshot`
command. No keyboard/mouse input is controlled, injected or captured.
Useful scripts, logs and capture review remain in the worktree's
`.tmp/stvef-gaps`; independent runtime homes remain under
`E:/Games/fnTech3-tests`.

## Remaining gates

These checks close the observed save-publication failure, including real
file obstruction and recovery. They do not establish complete campaign
traversal, physical movement/look/fire, menu rebinding/save/load, subjective
listening, feedback hardware or universal renderer parity. Both Elite Force
profiles remain development-gated. Retail SP remains a Windows x86 runtime
boundary; the Linux save harness is source-level validation, not SP runtime
support.

Existing unrelated issues remain: the installed `video/st_21.bik` has a bad
ZIP CRC, missing asset paths and excess `messhall` subwaves produce warnings,
the Windows x86 driver lacks native RT support, and the wider Linux suite
has previously recorded failures outside this slice. Existing compiler
warnings also remain. None is represented as fixed by this change.
