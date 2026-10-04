# System console branding and Elite Force icon correction

Date: 2026-10-02. Evidence: Source / Unit / Build. Companion:
[machine-readable record](2026-10-02-system-console-branding.json).

Windows and SDL desktop system consoles use an engine-owned palette for the
immutable active title. Ten palettes cover all sixteen profiles; mode variants
and the Quake III test releases share their title's colors. Startup without a
selected profile uses a neutral palette. Selection updates presentation even
with a custom console caption, before other profile startup can fail.

The log and input use dark, desaturated backgrounds with pale tinted text.
Footer/header, borders and buttons derive their colors from the same palette.
Measured log-text contrast is at least 12.65:1; primary text across all its
surfaces is at least 8.62:1, and muted/status/prompt text at least 6.12:1.
Log, input, footer/button and text HSV saturation does not exceed 35%. These are arithmetic
checks of the compiled palette values, not retail branding specifications or
runtime game-renderer observations.

Both Windows executables embed all remade game icons and bind the active
profile's icon at caption and taskbar sizes. The first icon group remains
fnTech3's, preserving Explorer identity. No retail icon extraction is needed.
SDL desktop consoles use the same active-profile remake; terminal-only
dedicated servers retain their terminal's appearance.

The Elite Force masters were redone with built-in imagegen against the user's
larger reference, restoring the tall gold delta, textured silver center,
rounded lower opening, red-and-black striped trapezoidal plaque and silver
rods. A mistaken half-circle was removed. SP keeps an opaque black square.
Holomatch uses an opaque blue rounded tile with subtle grid lines; only the
outside corner cutouts are transparent. Its generated master has near-opaque
alpha (251–254); standard export normalizes that ceiling to 255 while keeping
the antialiased corner mask and unmodified master. PNG, ICO, ICNS, embedded
frames and the overview were regenerated. Exact prompts and source hashes
are in [the icon provenance](../../assets/icons/games/PROMPTS.md) and
[manifest](../../assets/icons/games/sources/manifest.json).

The FnQ3/FnQL desktop log, input, copy/clear controls, status display and error
handling remain in use. This changes their presentation; no applicable engine
feature is disabled, and no retail-exact presentation option is added. It
does not change the in-game console, renderer, audio or compatibility contracts.

Validation used the existing Windows x86 `meson/build/win32-dev` preset:

- Incremental MSVC builds of client, dedicated server and application-icon test
  passed; this was not a clean build or the complete compatibility suite.
- 41 relevant Python tests and 287 subtests passed. Existing alpha assertions
  account for Holomatch's subpixel rounded corners at the 16 px caption size.
- The native application-icon test passed, including hidden Windows/SDL icon
  assignment. Symlink cases were skipped because this host denied symlinks.
- All 17 icon groups (engine plus sixteen profiles), every frame, were compared
  byte-for-byte against the exports in both built executables.
- The SDL console translation unit compiled with Windows x86 Clang and bundled
  SDL 3.4.0 headers. This is compile evidence, not a Linux runtime claim.
- No game, user-input control or screen capture was used. Useful logs and the
  detailed palette measurements remain in `.tmp/console-branding/`.

Automatic approval review rejected the verified disposable-file cleanup with
“blocked by policy.” Probe sources, executable and object file therefore also
remain in that task directory; no deletion was performed.

Unrelated observations: other work in the shared tree briefly referenced a
missing `server_jka_actions.hpp` and shadowed `challenge` in the JK2 handshake.
Both were corrected in that work before the final successful build. Existing
Ninja recovery, CMake generator-expression and FreeType wrap-revision warnings
were also observed. No changes to those warnings were made here. Gameplay and
live compatibility gates remain open.
