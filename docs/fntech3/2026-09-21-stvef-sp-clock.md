# Elite Force SP startup clock and camera tags

New single-player levels now start at 1,000 ms, matching retail. Starting at
zero prevented delayed reference tags from registering. In `voy1`, the native
game then used invalid coordinates for several scripted camera moves, producing
empty or misplaced views. This was a server initialization defect shared by
all three renderers.

## Evidence and change

Retail `stvoy.exe` 1.20 writes `0x3e8` to its level clock at `0x425ac9`, before
`SV_InitGameProgs` at `0x425b3e`. fnTech3's shared spawn path previously left the
new server clock at zero. The retail game DLL's `ref_tag` spawn at
`0x20076b10` reads level time, adds the `0.1` double at `0x200be280`,
truncates it through `0x200b24e4`, and stores the next-think integer at
entity offset `0x1f8`. Zero is the native game's no-think sentinel, so
these targeted tags never reached the registry.

The native ICARUS diagnostics reported missing `sevenshake`, `la_kim`,
`ha_paris`, `cu_railing`, `side_seven` and `cu_janeway`. A private diagnostic
build recorded camera coordinates far outside the map after those failures;
literal-coordinate moves still worked. With the corrected clock the tags
resolve and the camera stays within the bridge. For example, a resolved move
passes near `(115.83, 182.13, 598.05)`, consistent with the installed map's
`sevenshake` origin `(116, 182, 598)`.

`SV_SpawnServer` sets the clock only for `stvef-sp`, before game initialization.
Other profiles retain their existing clocks and RTCW save-time restoration.
Elite Force full saves still restore their saved `TIME` after spawning the map;
the new initial value does not replace the saved clock.

This also corrects the [original host record](2026-09-19-stvef-sp-host.md): its
retail column identified 1,000 ms, but the statement that fnTech3 already used
the same spawn sequence overlooked the unset clock. Earlier map startup and
process-completion observations did not establish that every scripted camera
worked. The initial wider scene sweep exposed the error; its affected images
are retained as failures, not renderer parity evidence. Enabling audio alone
did not repair it.

The retail executable, installed map entities and native-module diagnostics
are the authoritative evidence. `game/g_ref.cpp` in the local Elite-Reinforce
reference helped explain the fractional delay. This is observation of the
contract; no external implementation text was incorporated.

## Validation

- The new compiled regression fails before the fix and passes afterward on
  Windows x86 and Linux i686. It executes the actual pre-`Init` spawn phase,
  checks zero and nonzero previous clocks across all seven profiles, exercises
  the delayed-tag condition, and preserves RTCW's restored clock. All five
  SP host tests pass on both platforms. Windows client/server and Linux
  dedicated builds complete.
- The production client completes 24 scene runs: `_brig`, `stasis1`, `forge3`,
  `tour/deck01`, `voy1`, `tutorial`, `dn1` and `scav1`, each on verified GLx,
  Vulkan and RTX renderers. All exit normally and write an engine screenshot.
  The `voy1` logs contain none of the missing-tag failures.
- Three additional production `voy1` runs keep audio initialized through the
  private OpenAL Wave Writer, with muted output. Each renderer completes the
  early, middle and late captures without tag errors. These show bridge
  characters, interiors and scene progression. They are not synchronized
  pixel comparisons or listening tests. RTX uses SDR raster output here.
- A full tutorial save records `TIME=5100`; reloading and saving again records
  `5400`. Transitioning to `borg2` starts a fresh clock (`2850` at the next
  save). A missing tutorial hub save falls back to a new level (`3000`), and
  returning to the saved Borg hub restores its clock (`3150`). Every generated
  save parses to its last framed chunk.
- fnTech3 loads the existing retail tutorial fixture at `TIME=9000` and writes
  a new full save at `11600`. Retail loads the new fnTech3 save and displays
  the tutorial with 100 health. Retail's subsequent console save is refused
  as "while dead" in this unattended harness; the same refusal occurs after
  a fresh retail-only `devmap tutorial`. This is a harness/control limitation,
  not evidence of a successful retail re-save or proof of a format defect.
- Both staged manual installations pass separate automated preflight with
  input and sound disabled: the SP retail save loads and Holomatch starts the
  local map with a Munro bot. Hands-on input, menu save/load and listening are
  still pending.

The [later runtime follow-up](2026-09-21-stvef-runtime-followup.md) resolves
the unattended retail re-save limitation for this tutorial fixture: after a
longer wait, retail saves successfully and fnTech3 reloads and re-saves its
output. The original early refusals remain recorded separately.

The initial complete Windows run has 119 passing Meson targets, two skips and
one failing Python aggregate. Its 1,844 Python tests have one error and 72
skips: an unrelated GLx dry-run fixture exceeds the Windows path limit under
the long temporary directory. All 201 tests in that fixture group pass after
shortening `TEMP` and `TMP` to `.tmp/w` inside this worktree. The complete
Python aggregate then succeeds: 1,844 tests, 72 skips, no failures, in
688.260 seconds.
Together with the initial run, all 120 applicable Meson targets pass and
two skip. The [manifest](2026-09-21-stvef-sp-clock.json) preserves
the initial failure, focused retry and full aggregate retry separately.
The previously recorded broader Linux failures remain open; the Linux host checks are not a full-suite pass.

## Reproduction and limits

The manifest records source hashes over base commit `365cad7`, production and
diagnostic executable identities, all commands/settings, save times, log and
capture hashes. It distinguishes private diagnostic binaries from production
reruns. Tests use independent writable homes under `E:/Games/fnTech3-tests`,
read the copied legitimate assets, force windowed mode, disable mouse and
joystick input, and run on a private desktop that is never activated. Every
image is written by the engine's `screenshot` command. The retail installation
is unchanged and no proprietary inputs or images are committed.

The prepared manual launchers and instructions are under
`E:/Games/fnTech3-tests/stvef-manual`. Their normal input and audio are enabled
only when the user launches them. Force-feedback effects were unimplemented
at this checkpoint; the [later bridge](2026-09-21-stvef-force-feedback.md)
adds the device boundary and native effect rules, with physical tests pending.
The [extended runtime record](2026-09-21-stvef-runtime-coverage.md) covers full
Holomatch demos and the observed x86 ray-tracing capability limit. Broader
campaign play, synchronized retail image parity and physical-device acceptance
remain required; neither profile is promoted out of its development gate.
