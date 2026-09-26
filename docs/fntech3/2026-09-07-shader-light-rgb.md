# Shader light RGB syntax — 2026-09-07

The Q3 installation's `zz-q3netpack.pk3` supplies bare `q3map_lightRGB`
triples. All three renderer parsers previously passed these to `ParseVector`,
which required parentheses and emitted missing-parenthesis warnings. This
prevented the compiler-light color metadata from being retained correctly.

GLx, Vulkan and RTX now accept both bare triples and parenthesized triples.
The directive's existing positive-color normalization and clamping remain.
Parsing rejects non-numeric, non-finite, overflowing or incomplete input and
commits its text cursor and color only after a complete valid triple. A failed
parse leaves the caller's cursor and existing color unchanged. This is a
dedicated light-metadata parser change; other vector directives are unaffected.
The implementation was independently authored from local asset syntax
observations; no external implementation or proprietary asset text was ported.

Validation at `c43f673` plus this patch:

- An executable harness runs each production renderer function with the
  production engine tokenizer on Windows and Linux. It covers both syntaxes,
  normalization, negative clamping, zero colors, incomplete triples, stage
  boundaries, missing parentheses, trailing numeric junk, NaN and overflow.
- All three Windows MSVC x86 renderer DLLs rebuild successfully.
- Each renderer completes Q3's shipped `four.dm_66` (1,260 frames) using the
  installation containing the add-on, and the complete generated QL `.dm_91`
  fixture (79 frames). All six exit normally with no truncation, invalid RGB
  or missing-parenthesis diagnostics. Timing is not a performance comparison.

The [runtime record](2026-09-07-shader-light-rgb.json) contains
artifact and observed add-on hashes, commands/configs and frame summaries.
Runs use isolated writable homes, windowed rendering, disabled input capture,
audio, Steam and WebUI, and no input injection. No proprietary inputs or
screenshots were committed. This proves parsing and bounded playback; it does
not prove lighting parity or native ray-traced emission.

Existing missing compiler-light images, font fallback and mixed-image-flag
warnings remain unrelated and unresolved. Broader asset and renderer gates
remain open.
