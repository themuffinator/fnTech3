# Profile-owned console shader — 2026-09-07

The merged client unconditionally registered `fnql/console`. That alias uses
QL retail images absent from Q3, producing missing-image warnings in Q3 client
startup and renderer restart. The immutable profile now selects Q3's stock
`console` shader or QL's existing `fnql/console` alias. No shader implementation,
asset, module contract or renderer API was copied or changed.

Read-only inspection of the legitimate local Steam archives found
`textures/sfx2/screen01.jpg` and `textures/effects2/console01.png` in QL
`pak00.pk3`, with neither image stem in the Q3 archives. The alias names `.tga`
paths; the existing renderer resolves supported alternative image extensions.
QL retains its distinct alias because its pak contains duplicate stock
console definitions. The prior Q3 missing-image observation is in the
[reference-peer checkpoint](2026-09-07-q3-reference-interop.md).

Windows x86 MSVC compilation passed. Both profiles then passed a local GLx
pure session and reached `CS_ACTIVE` with the console open. Engine-generated
screenshots were inspected: Q3 shows its console over q3dm6, and QL shows its
console over the native campgrounds join UI/world. Neither run reports the
missing QL console texture. Both client/server pairs exited normally.

The [evidence record](2026-09-07-console-profiles.json) contains exact
commands, generated configs, hashes and log excerpts, based on `cc6537a` plus
the console patch. Client SHA-256 is
`cc98ce03a69b32f2575f9aa13c571a123f797322160a00230f316ee2ab0613a2`;
dedicated SHA-256 is
`0f5cd82ce3efdb44754dd7b42c9785060909158abec465341f5f0eede76b7cea`.
Probes used isolated writable homes and an unswitched private desktop,
`r_fullscreen=0`, disabled input capture/audio/Steam/WebUI, and engine
`screenshotPNG`; no user input or OS screenshot capture was used.

The first QL attempt used an overly long client frame wait: the server quit
before capture, and the client required bounded cleanup. It is not counted as
a pass. Shortening the probe's client wait from 600 to 120 frames yielded the
successful QL screenshot and exits. Engine timing was unchanged. Four profile
config/probe tests also passed, with four subtests; these are supplemental
checks rather than a substitute for the runtime evidence.

This closes the observed console-texture regression only. Other missing model
and compile-time shader-image warnings remain, as do broader renderer and
compatibility gates. The engine-only packaging CI Linux regression lane failed
before tests during dependency download timeouts/HTTP 502 responses; this is
not a test failure or evidence that packaging validation passed on that runner.
