# Elite Force SP startup frame order and capture timing

Elite Force SP now calls its four map-settling frames at retail's times:
1000, 1100, 1200 and 1300 ms for a new level. The previous shared path
advanced the clock first and called 1100, 1200, 1300 and 1400. Both paths
ended with the engine clock at 1400, so the final clock alone did not expose
the difference to the game module.

## Native observation and correction

The user's `stvoy.exe` 1.20 starts a map at 1000 ms (`0x425ac9`). Its first
three settling frames at `0x425b43..0x425b6e` call `RunFrame` at the current
clock and then add 100. After reconnecting the client, the last settling
frame at `0x425be2..0x425bfb` does the same. The engine clock is visible to
game imports during those calls; adjusting just the argument would not
reproduce that contract.

A local probe maps the retail executable without starting it and executes
these two instruction blocks. An owned callback records the frame argument
and engine clock. The probe returns before baseline construction and
configstring publication; the second return also removes the two pending
cdecl stack arguments. Seven starting clocks cover zero, adjacent values,
the normal 1000 ms start, restored clocks and a value near the signed limit.
The [fixture](../../tests/fixtures/stvef_sp_startup_retail.json) records all
28 native frame boundaries and the executable/probe identities. No retail
implementation text or executable bytes are incorporated.

`SV_RunStartupFrame` preserves the existing command-wait and bot-frame calls.
Only `stvef-sp` advances the clock after the callbacks. The other six
profiles retain their previous ordering, the map still runs three frames
before reconnecting clients and one afterward, and both `sv.time` and
`svs.time` retain their final values. This correction concerns full map
startup; it does not claim the separate `map_restart` path has been matched
to retail SP.

`tests/stvef_sp_startup_tests.py` compiles the actual startup helper and both
call-site blocks from `SV_SpawnServer`. It compares seven SP sequences with
the native observations and checks 42 sequences for unchanged behavior in
the other six profiles: 196 game-frame boundaries per platform, plus the
callback clock, wait/bot order and final clocks. The previous production
code fails the new regression. Windows x86 passes; all 65 focused SP tests
pass on Linux i686, with AddressSanitizer and UndefinedBehaviorSanitizer on
the new probe. Windows and Linux client/dedicated builds complete. The full
Windows suite passes 120 targets with two skips and no failures; its Python
aggregate runs 1,854 tests with 72 skips in 804.985 seconds. Individual
results, commands, artifacts and hashes are recorded in the
[manifest](evidence/2026-09-21-stvef-sp-startup.json).

## Runtime coverage

The current supplied paks contain 66 non-Holomatch BSPs, each with an
`info_player_start`. Every one starts the retail game module in an isolated
Windows x86 dedicated process, runs ten further command-wait frames at a
fixed 50 ms step, prints its game-memory report and exits normally. This
fresh inventory extends the earlier 63-map record; these short server
probes establish initialization, not full campaign or client rendering.

Twelve windowed scene runs cover retail and all three fnTech3 renderers on
`tutorial` and `voy1`, plus retail/GLx pairs at 25 ms and 100 ms steps.
All complete and produce 36 engine screenshots. `voy1` shows the opening
ship scene, not the entire bridge sequence. A Linux i686 GLx session also
starts local Holomatch on `hm_borg1`, renders an engine screenshot through
private Mesa/Xvfb, and exits normally.

Three save sessions use normal elapsed-time scheduling. A new tutorial save
at `TIME=4450` reloads and re-saves at 4750. Transition to `borg2` starts a
new clock at 2500; fallback to an absent tutorial hub save starts at 2500;
return to the saved Borg hub restores and advances its clock to 2850.
Retail loads the 4450 save and writes 9650; fnTech3 loads that result and
writes 9950. All generated saves parse through their final framed chunk.
This is command-driven save/transition coverage, not manual menu acceptance.

The prepared manual engine under `E:/Games/fnTech3-tests/stvef-manual` is
refreshed to source commit `97006ad`. Eight existing settings/save/launcher
files retain their hashes. Fresh input-disabled SP save-load and Holomatch
bot-match preflights pass with that staged engine; user launchers are not run.

## Capture phase and filter controls

The startup-frame correction does not remove the earlier screenshot-time
offset. Native SP's `screenshot` command (`0x440050`) calls the capture
routine at `0x43fd10` directly; that routine calls `glReadPixels` at
`0x43fd84`. fnTech3 queues the capture for the end of the frame. The engines
also decrement their command-wait counters at different frequencies.

Taking the first fnTech3 screenshot one frame earlier, while retaining the
subsequent intervals, aligns the displayed snapshot times in five additional
runs: all three renderers at 50 ms, and GLx at 25 and 100 ms. For samples
20, 80 and 160, both engines display:

| Fixed step | Displayed snapshot times |
| --- | --- |
| 25 ms | 1950, 3450, 5450 |
| 50 ms | 2550, 5550, 9550 |
| 100 ms | 3700, 9700, 17700 |

fnTech3's packet sequence numbers are one below retail's at these matched
times. Those are internal message identifiers, not an assertion that all
animation/effect states are synchronized. The fast transition's full-frame
mean RGB difference falls from 58.826 to 5.810 out of 255; the slow middle
sample falls from 17.900 to 2.179. Other full-frame differences can increase
because the diagnostic overlay itself changes. Every sample is retained.

`tests/stvef_sp_capture.py` writes this capture script without launching a
game. Its generated retail and fnTech3 scripts match the two executed
settings-query runs byte-for-byte. The helper logs effective filtering,
picmip, gamma and overbright values, because requested settings alone do not
establish the render contract.

Those queries show that retail used `GL_LINEAR_MIPMAP_NEAREST` despite the
prepared configuration requesting `GL_LINEAR_MIPMAP_LINEAR`; fnTech3 used
the latter. Four further runs use the observed retail filter in every
engine, disable the snapshot overlay, and retain the adjusted capture
phase. All logs confirm the effective filter. Full-frame RGB results are:

| Sample | GLx mean / maximum | Vulkan mean / maximum | RTX mean / maximum |
| --- | --- | --- | --- |
| 20 | 2.254898 / 95 | 2.254976 / 95 | 2.254976 / 95 |
| 80 | 1.148274 / 114 | 1.147784 / 114 | 1.147784 / 114 |
| 160 | 1.148274 / 114 | 1.147784 / 114 | 1.147784 / 114 |

These are bounded SP renderer comparisons with verified settings and a
measured capture phase. They establish neither pixel identity nor broader
campaign/renderer parity, and do not change production screenshot or
filtering behavior. RTX uses SDR raster fallback in these Windows runs.

The later [compression investigation](2026-09-21-stvef-texture-compression.md)
finds another setting mismatch: these runs disabled Q3's differently named
compression cvar, leaving retail SP's texture compression enabled. Matching
that setting reduces the mean difference to 0.074–0.145. The measurements
above remain the historical observations; they do not isolate renderer error.
The capture helper now additionally queries `r_ext_compress_textures`, so
its current output includes one more diagnostic than the scripts recorded here.

All tests use independent writable homes under `E:/Games/fnTech3-tests`.
Client probes are windowed with mouse, joystick and feedback disabled, on an
inactive Windows desktop or private Linux display. Images come only from
the registered engine screenshot command. Useful local logs, scripts and
captures remain in the designated test areas; proprietary inputs and game
images are not committed.

Existing missing/corrupt asset diagnostics, the wider Linux-suite failures
and the Windows x86 RT limitation remain open. The Linux rebuild also emits
an existing ET/RTCW enum-comparison warning in `SV_SetConfigstring`. Physical
input, menu use, listening, feedback hardware and campaign-wide acceptance
still require evidence. Both Elite Force profiles remain development-gated.
