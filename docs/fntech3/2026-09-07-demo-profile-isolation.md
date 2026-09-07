# Demo profile isolation — 2026-09-07

The shared demo registry previously let either process profile select the
other game's format. A Q3 process could open `.dm_91`; QL's automatic search
could fall back to Q3 formats and `.dm3`. That assembled a wire/module hybrid
despite immutable profile selection.

Client selection now intersects the registry with the selected profile:
Q3 retains 66/67/68/71 and legacy `.dm3`; QL selects 91. Q3's historical search
order remains unchanged. Explicit recognized cross-profile formats return an
actionable message before file opening or disconnecting the current session.
Filename completion follows the same profile boundary. Arbitrary protocol cvar
values no longer extend the supported demo registry.

This does not add historical QL formats or guess the codec for a mislabeled
recording. Such compatibility requires separate format evidence and adapters;
choosing a Q3 codec inside a QL process is not a valid fallback.

Validation at `6f7669b` plus this patch:

- Production format-predicate/search executable tests pass on Windows and
  Linux with mixed-profile filenames. The tests verify actual file-open
  attempts, missing-file fallback and preserved Q3 ordering.
- Existing QL demo/source and recorder-finalizer checks pass (7 tests).
- Windows MSVC x86 client compilation passes.
- Runtime Q3 `.dm_91` and QL `.dm_68`/`.dm3` requests print the expected
  profile-mismatch messages and return to subsequent scripted commands.
  These probes test extension rejection without supplying cross-profile files.
- Q3's stock `four.dm_66` and a complete fnTech3-recorded QL `.dm_91` still
  replay to normal completion. The existing truncated external QL recording
  still reports truncation and exits cleanly, without being counted as a pass.

The [runtime record](evidence/2026-09-07-demo-profile-isolation.json) contains
binary hashes, commands/configs and outcomes. Runs used isolated writable homes,
windowed GLx, disabled mouse/joystick capture and startup console commands. No
input injection or screen capture was used; no proprietary inputs were added.
Broad demo-format, active-session preservation, malformed-input and full
interoperability gates remain open. Existing resource warnings are unchanged.

Follow-up at `a03b5d3`: the executable selector harness now also runs the
production `CL_PlayDemo_f` selection prefix, stopping at its disconnect
boundary. With both Q3 and QL files present, explicit cross-profile requests
return without any file-open attempt or reaching that boundary; a matching Q3
request does reach it. Production filename completion is tested with mixed
case, cross-profile formats, trailing junk, negative values, missing digits
and integer overflow. These checks pass on Windows and Linux. This strengthens
the control-flow evidence; it is not an end-to-end active-session runtime test.
