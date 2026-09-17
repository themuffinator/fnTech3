# RTCW corona rendering, 2026-09-16

Approved retest 4 reached `CG_R_ADDCORONATOSCENE` (SP import 58) after the
decal correction. The late engine screenshot records that error. Retest 5,
with the corona adapter and renderer paths present, instead stopped at
`CG_LOADCAMERA` (SP import 98). Its late image remains an error dialog, not a
playable mission or halo-rendering comparison. All five specifically approved
startup clicks are consumed. Each test was windowed, used an isolated writable
home and an unswitched private desktop, sent exactly one native UI ready click,
and left the installation unchanged. No movement or combat input was sent.

Retest 5's instrumented executable has SHA-256
`2196461f23255f6b22de2d764c187c7220942d2d9f2d5c45f21d813303f94469`.
The matching current/autosave files contain 218,225 bytes with SHA-256
`46d2342c8251b6752ff08a981633e08e3146fe71c55171626e95d088fcb8e780`.
The late engine image has SHA-256
`3169d99f27805e4eed0d5c2869af4b469c6ea5c86a3519fe73300e6002563863`.
Logs, manifests, source-instrumentation identities and captures remain under
`.tmp/rtcw-manager/startup-retest-4/` and `startup-retest-5/`.

Renderer API 25 admits a separate, owned corona record. The common cgame/UI
adapter validates vectors, scalar values and machine-word integer arguments.
Scene accumulation preserves the original 32-corona frame capacity and copies
each scene's records into its render command. No module or mutable frontend
pointer is retained by the backend.

All three renderer backends share the independently authored 128-entry fade
policy, projection checks, per-ID/scene/portal identity and original draw order.
SP interprets visibility and spotlight bits; MP interprets the argument as a
boolean. They use the game's reported visibility, original size equation,
identity-light RGB and fading vertex alpha. SP selects the installed spotlight
material when requested. Existing orthographic quad submission performs the
actual draw; the new path does not substitute Q3's depth test, inverse-square
attenuation or optional lens layers. Byte conversion and geometry bounds are
defined for malformed numeric inputs.

The profile defaults are `r_flares 1` and `r_flareFade 5`; modes 1, 2 and 3
retain RTCW's corona/dynamic-light meanings. The original RTCW renderer's
disabled BSP-flare surfaces remain disabled for these profiles. Q3/QL retain
their existing defaults and flare paths. The original renderer and material
files were used as observations; no RTCW implementation text was copied.

The combined seven-check mark/corona/voice/client-state group passes Windows
x86 (4.744 seconds), Windows x64 (5.090 seconds), and Linux x86 with address/
undefined-behavior sanitizers and leak detection (30.602 seconds).
Default and development Windows x86 client/server
builds and all three renderers pass. Complete changed GL, Vulkan and RTX C
translation units also pass Linux x86 syntax checks. Six fresh Q3/QL engine
captures, one per renderer/profile pair, were personally inspected and audited
in `.tmp/rtcw-manager/startup-render-live/`; all load the expected installed
modules and remove their isolated homes.

The full suite passes 111 Meson tests with two skips. The Python suite passes
1,465 tests with eleven skips in 529.459 seconds. Source, test, executable and
evidence identities are recorded in
`.tmp/rtcw-manager/startup-render-source-identities.json`. Actual RTCW
halo/fog image parity, campaign play, cinematic cameras, active SP voice
amplitude and save restoration remain open. Vulkan/RTX also retain the earlier
explicit active scene-fog guard. Successful builds and accepted imports do not
close these runtime gates.
