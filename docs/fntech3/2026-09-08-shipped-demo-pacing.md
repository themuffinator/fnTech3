# Shipped Q3 demo pacing and fixture identity

The Windows x86 client at clean source commit 8f4c31b plays the shipped
pak6.pk3 member demos/four.dm_66 at normal speed, with timedemo disabled.
It reaches its explicit completion marker and exits zero without forced
termination after 66.438 seconds, with no truncation warning or demo-parser error.
An engine screenshot taken during playback shows the q3dm6 world, first-person
weapon, opponent and readable HUD. This is a bounded visible playback observation,
not a reference-image comparison or frame-pacing benchmark.

The [evidence record](2026-09-08-shipped-demo-pacing.json) pins the executable,
source commit, nine stock pak hashes, member hash, exact command/config, logs and
screenshot hash. The probe uses the isolated pak0-through-pak8 asset fixture,
a fresh writable home, windowed GLx and a private unswitched desktop. Input,
audio and optional services are disabled. The screenshot comes from the engine
render target. No keyboard/mouse control or operating-system capture occurs.
Retail bytes and the screenshot remain ignored; installations are unchanged.

The earlier [shipped-demo checkpoint](2026-09-07-q3-shipped-demos.md) covered
four filename paths through timedemo. Direct comparison now establishes that
pak6.pk3:demos/four.dm_66 and pak8.pk3:demos/four.dm_68 contain exactly the same
152,158 bytes, SHA-256 bf761f64415cae84e14271c0a865e1f05c843b0bd58fdd0706da7f6f5ad4766e.
Their successful playback verifies two accepted extension paths, not two distinct
wire samples. The archive inventory contains no stock dm_67 fixture. Missing
fixtures must not be replaced by a renamed file and described as independent
protocol evidence.

No engine change or external implementation was required, and the full suite
was not repeated for this evidence-only update. Distinct protocol-67/71 fixtures,
all claimed formats, seeking, visual/collision parity and broader QL demo
interoperability remain open. Existing font fallback diagnostics are outside
this result. No full compatibility gate is promoted.
