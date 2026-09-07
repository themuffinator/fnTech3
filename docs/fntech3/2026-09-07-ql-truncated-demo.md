# Existing QL recording: truncated-input probe — 2026-09-07

An existing user-supplied `.dm_91` recording was replayed with the unified QL
profile at clean checkout `d8753b9`, using the current Windows x86 client. The
recording engine is unknown, so this is not asserted to be a retail-authored
golden fixture. Its identity and exact startup command are in the
[evidence record](evidence/2026-09-07-ql-truncated-demo.json); original user
paths and gameplay logs are not committed.

The client loaded the native modules/world and processed 115 timedemo frames.
It then reported `Demo file was truncated.`, ran the completion callback and
exited normally. The callback alone is therefore not evidence of complete
playback. This run is classified as truncated-input recovery, not a normal
demo pass.

Independent envelope inspection confirms an incomplete source file: its
73,728 bytes contain 227 complete messages, followed by sequence 260 declaring
a 292-byte payload at offset 73,483. Only 245 bytes remain. There is no normal
terminal trailer. No repair, relabeling or fabricated completion data was
applied to the input.

The recording was copied into an isolated temporary home outside the
repository. The original installation/recording was unchanged. Playback used
windowed GLx, disabled mouse/joystick capture, disabled audio/Steam/WebUI and
startup console commands. No input injection or screen capture was used.
Full local probe output is `.tmp/demo-ql-external-external-overkill-dm_91/`.

This establishes one real truncated-payload recovery case. It does not prove
handling of every malformed header/body, complete QL external-demo playback,
seeking, visual parity or retail interoperability. Existing resource warnings
remain outside this result. Separately, clean Linux verification at `49fc484`
passed ([run 34164825025](https://github.com/themuffinator/fnTech3/actions/runs/34164825025)),
covering the snapshot and entity-history changes through that commit.
