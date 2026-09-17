# RTCW cast-query callback boundary

This checkpoint supplies the game callbacks required by RTCW tactical
navigation. It does not implement the remaining hide/attack search imports
or establish live campaign AI. Work is subsequent to
`a80f24099284604326ec41dc41b4f73d7442ecd0` and the visibility checkpoint.

## Contract and implementation

Original [SP server callbacks](https://github.com/id-Software/RTCW-SP/blob/master/src/server/sv_bot.c)
and [MP server callbacks](https://github.com/id-Software/RTCW-MP/blob/master/src/server/sv_bot.c),
their respective `game/g_main.c` dispatchers, and
[iortcw](https://github.com/iortcw/iortcw) establish these contracts:

| Callback | SP/MP export | Arguments |
| --- | --- | --- |
| Visibility | 11 | Source position, source entity, destination position, destination entity, update-visible-position flag |
| Attack at position | 12 | Entity, enemy, candidate position, ducking flag, allow-hit-world flag |

The engine assigns distinct semantic identifiers above the Quake export range;
`VM_Call` translates them only for RTCW. Q3/QL reject these semantic calls
before reading arguments, while their own exports numbered 11/12 remain
unchanged. Both RTCW callbacks require five arguments and a legacy x86 native
game module, consistent with the existing module policy.

`server_rtcw_cast.hpp` copies each position into aligned local storage before
calling the game. Nested callbacks have independent storage. A game may write
to its arguments without modifying botlib-owned or caller-owned vectors.
Entity indices use the game ABI's 10-bit limit in both modes; flags must be
zero or one; positions must be finite and within navigation coordinates.
The later [prediction checkpoint](2026-09-16-rtcw-movement-prediction.md)
corrects the guard's former use of botlib allocation capacity. SP allocates
2,048 internal slots but its game module still permits only 1,024 entities.
That checkpoint supersedes the historical boundary evidence below.
The result must be a valid zero/one boolean, so an unsupported game export's
negative result cannot become a successful visibility/attack answer.

The callbacks are registered through an engine-private interface. Quake's
`botlib_import_t` and export POD layouts do not grow. The interface exposes
availability separately from visibility: absent game code must not be used
as evidence that a position is hidden. Registered engine function pointers
survive navigation map reloads and inspect the current game VM when invoked;
they do not retain a module pointer. Q3/QL registration clears the interface.

This code is independently authored under GPL-2.0-or-later. The original
GPLv3 bodies were inspected as contract evidence and were not incorporated
into production code. Synthetic test entry points contain no retail game
logic or assets.

## Validation

`tests/rtcw_cast_query_tests.py` executes the actual VM dispatcher, callback
registration and server boundary against a synthetic native entry point.
It checks both modes, exact ordinals and argument ordering, all flag values,
false/true results, padding, entity limits, unaligned inputs, owned vectors,
nested calls, missing modules, unsupported module types, invalid arguments
and invalid results. Windows x64 verifies explicit pointer-width rejection;
successful pointer handoff is tested on x86.

`tests/rtcw_module_export_tests.py` now covers the new semantic calls alongside
the existing export corpus for client and dedicated configurations. The
visibility tests also execute the new registration-containing botlib world
scaffolding. Five focused tests pass on Windows x86 in 7.044 seconds, Windows
x64 in 7.790 seconds and Linux x86 with AddressSanitizer, UndefinedBehaviorSanitizer
and leak detection in 54.930 seconds.

The initial Linux run rejected an unaligned source passed directly to a typed
`memcpy` argument. The copy helper now accepts a byte span through `const void *`,
then validates the aligned local floats. The same unaligned fixture passes;
sanitizer checks remain enabled. The failed initial log is retained separately.

Both Win32 release configurations build the client, dedicated server and all
three renderers. SP/MP map probes pass with isolated home paths, unchanged
installed content, windowed rendering and no supplied input. Inspected engine
captures show SP's ready-arrow briefing and MP's deployment UI and movie. SP
has 29 soldier clients and reaches `rockandroll` while still paused in the
briefing. These observations do not demonstrate live callback use or gameplay.

All six Q3/QL renderer smoke probes pass. Their engine captures were inspected:
Q3 shows its world, weapon and HUD; QL shows its world and join UI. QL's match
was not entered. Renderer/module selection, windowed mode and removal of each
temporary home path were checked.

The complete Meson gate passes 111 targets with two skips. Its Python discovery
gate passes 1,417 tests with 11 skips in 342.458 seconds. Linux x86 syntax
validation also passes for the complete botlib world translation unit.

Evidence is retained under `.tmp/rtcw-manager/cast-query-*`:
`cast-query-source-identities.json` records the source, reference-contract,
test, executable and runtime-manifest hashes. The default client SHA-256 is
`1a04c809b4d6685485ec389416b16338a4f6798eb702c2fca5c34f3d16c10c37`;
the RTCW development client is
`da52d235b603c92f99bea6fc1b80b2fc8b8b8669585b41afd3dfa0ce34ce62ad`.
Installed assets and modules remain outside tracked files.

## Remaining scope

The hide-position and attack-position algorithms still need their distinct
SP/MP policies and checked module imports. MP attack-area selection depends
on linked entity areas; SP uses fuzzy point selection. Hide budgets and ladder
handling also differ. The now-available callback interface is a prerequisite,
not proof that those searches work. Movement prediction, cache generation,
live gameplay, save/load, demos and peer interoperability remain open.

A compiled source-contract probe also establishes a hide-search flag difference:
original SP's unparenthesized default macro leaves jump pads enabled
(`20713406`), whereas original MP and both iortcw modes exclude them
(`20451262`). All four permit slime. This is recorded in
`.tmp/rtcw-manager/tactical-flag-contract.json`; the future wrapper must not
substitute Quake's default mask or claim identical policy across these sources.

The existing unrelated Q3/QL routing-cache reader bounds audit remains open.
No new unrelated issue was found in this checkpoint.
