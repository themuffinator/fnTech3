# Runtime checks after the QL command-codec corrections

The same Windows x86 fnTech3 client and dedicated binaries pass isolated Q3
and QL loopback sessions at clean commit `5334423`. This validates normal
connection/download/pure-admission behavior after the
[movement preservation](2026-09-08-retail-usercmd-wire.md) and
[key-mask correction](2026-09-08-retail-usercmd-key-mask.md).
The [record](2026-09-08-post-codec-runtime.json) contains commands, installation
and executable identities, selected log evidence, screenshot hashes and exits.
Both executable hashes match the binaries recorded with the key-mask tests.

Each profile uses legitimate read-only game assets, separate temporary writable
homes and a private Windows desktop that is never switched to the input desktop.
Clients run windowed at 640 by 480 with mouse/joystick disabled and no input
injection. Audio, Steam integration and WebUI are disabled. Master-server
addresses are empty; game and fixture HTTP servers bind to loopback.

Each server requires pure admission and an independently generated 32,916-byte
fixture PK3. Each client makes one HTTP request; the response sends one byte,
waits half a second, then sends the remainder. Both clients download the exact
expected archive, leave no partial archive, reconnect and reach CS_ACTIVE.
The Q3 challenge advertises protocol 68; QL reports protocol 91. Both server
logs confirm the primed-to-active transition after the client pure-server
messages. Q3 completes in 36.141 seconds and QL in 35.422 seconds. All four
engine processes exit zero without forced termination.

The engine's `screenshotPNG` command produces both visual observations. The
Q3 image shows the q3dm6 world, weapon and HUD. The QL image shows the
campgrounds world and native join panel. Neither operating-system capture nor
input control is used. The QL observation does not establish joining a match
or active team gameplay. Raw images remain ignored because they contain retail
assets; only their identities and observations are recorded.

The helper/output names retain the older `fnclient-refserver` argument, but
both actual executable paths identify fnTech3. No FnQL or FnQuake3 reference
engine is launched. No packet loss, delay or duplication is configured in the
relay; the intentional HTTP response split is separate. These are live
self-pair regressions, not independent retail-peer interoperability. They do
not deliberately force the minimum-movement or extra-key-bit cases on the live
wire; those cases retain the focused codec and retail-function evidence linked
above.

This checkpoint changes documentation only. The complete Windows suite and
Linux codec/corpus results from the key-mask correction remain the automated
validation; no new full-suite run is claimed. No new unrelated engine defect
was established. Required compatibility gates remain open.
