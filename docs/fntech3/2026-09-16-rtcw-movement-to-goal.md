# RTCW movement to goal

The shared engine admits RTCW game import 549 through an explicit movement
result adapter. The original SP/MP result is frozen at 52 bytes; the goal
remains 56 bytes. RTCW's no-slow-approach goal bit, future-view result bit and
invisible-grapple result type map to distinct engine meanings rather than
Quake's dropped-item, avoid-spot and bobbing-wait values.

Native and QVM calls validate complete, potentially unaligned spans and own
their input/output records before dispatch, including overlapping buffers.
Only consumed goal fields are projected. The six always-initialized result
integers are checked; untouched optional caller bytes are preserved, including
NaNs. Changed vectors must be finite, but a movement direction can legitimately
exceed the world-coordinate bound before normalization. Invalid backend
results are rejected before publication.

The inherited movement orchestration now selects original RTCW policies at
the relevant branches. It retains raw travel types, omits Quake's BFG travel,
and does not restore the travel-type field after a travel helper replaces the
result. Same-area movement honors RTCW's no-slow-approach bit. Ordinary travel
reselects its route after a short stalled interval; grapple and mover routes
retain their distinct lifetime rules. Only bobbing platforms extend RTCW's
mover timeout, as observed in both original releases. Quake retains its
existing obstacle, solid-area, avoid-list and movement policies.

Airborne jump-pad recovery keeps route flags separate from the immediate
jump-pad movement mask and uses the first fallback jump-pad link. Ladder
detection tries the original five nearby points before falling back to the
previous area; face, plane, edge and vertex accesses are checked. Look-ahead
uses a single next route endpoint in MP. SP permits equal-cost route steps
and searches to the goal or beyond a 120-cost horizon, with a 4,096-step work
bound for cycles. Route caches may name stored reachability record zero;
movement iteration still uses zero as its stop sentinel.

Before movement, the RTCW path checks initialized navigation/action storage,
callbacks, state indices, entity/client bounds and arithmetic inputs. Clock
validation accommodates week-long servers. Missing mover entities or invalid
model bounds fail safely instead of reading an undefined bobbing origin.

## Evidence

This is an independent extension of inherited GPL-2.0-or-later code, informed
by the original [SP movement source](https://github.com/id-Software/RTCW-SP/blob/master/src/botlib/be_ai_move.c)
and [MP movement source](https://github.com/id-Software/RTCW-MP/blob/master/src/botlib/be_ai_move.c).
Original and [iortcw](https://github.com/iortcw/iortcw) GPLv3 bodies and notices
remain confined to disposable comparison programs.

`tests/rtcw_move_goal_tests.py` exercises ordinary/stalled routes, movers,
jump-pad fallback, partial initialization, settings and caller bytes,
native/QVM aliases and failures, ladder geometry, malformed graph storage,
equal-cost cycles and actual route caches in both navigation worlds.
`scripts/probe_rtcw_move_goal.py` compares 18,432 cases per reference using
engine geometry, physics and action services with controlled route/BSP
responses. Its reference-local clear wrapper seeds undefined optional words;
the caller's optional words are initialized separately and remain compared.
This establishes orchestration behavior, not original collision or live AI.

The frozen layouts pass 2,207 SP and 2,197 MP static assertions against each
original/iortcw reference on Windows x86 and x64. Eighteen focused tests pass
on Windows x86 (32.620 seconds), Windows x64 (31.499 seconds), and Linux x86
with address/undefined-behavior sanitizers and leak detection (215.033 seconds).

Original SP/MP comparisons agree exactly under strict Windows x86/x64 and
sanitized Linux x86 SSE2 arithmetic. Besides six initialized integers in each
result, the corpus compares 106,506/106,041 optional words for original SP/MP
and explicitly excludes 22,518/22,983 undefined helper-local words. iortcw's
whole-result initialization permits comparison of all 129,024 optional words.

Strict iortcw SP/MP comparisons contain 1,272/1,206 differing records and
141/69 decision differences. Fast-math Windows x86 has 273/1,065 differing
records against original SP/MP, with no compared decision changes, and
5,475/5,349 differing records against iortcw, retaining its 141/69 decision
differences. Decisions include result integers, action flags/weapons, route
selection calls/masks, state area/link/flags/jump fields and commands. These
diagnostics do not supersede the installed prediction corpus's previously
recorded decision difference.

Both Win32 client/dedicated builds with GLX, VK and RTX pass, along with full
Linux x86 movement, routing and server translation-unit checks. The default
executable SHA-256 is
`2902fac26d43413c77a598e8cfd8d36c70044e8a49fb0108c2d42a779f3eeb03`;
the experimental executable SHA-256 is
`683951f190b161b18c0aa24c1dbc58153040459a8ee7eec6552e3da58bdb8345`.
These include recorded uncommitted work after `a80f240`.

All eight engine screenshots were inspected: readable SP briefing/ready
arrow, MP deployment UI/movie, Q3 world/weapon/HUD on three renderers, and QL
join UI/world on three renderers. SP remains paused before gameplay; QL has
not joined the match. Runtime probes use windowed private desktops, disabled
input and isolated writable homes; installations remain unchanged.

The complete suite passes 111 Meson tests with two skips, including 1,446
Python tests with 11 skips in 408.546 seconds. Evidence and the checkpoint
manifest are under `.tmp/rtcw-manager/move-goal-*`. Initial fixture failures
are retained. The
first complete-suite invocation lacked the MSVC environment and failed to
start the compiler; the retry uses the project's environment helper.
SP initializes 29 soldier clients and emits `rockandroll` while paused; MP
retains pure admission. Temporary homes were removed after the probes.

## Remaining work

Active cast movement, installed dynamic collisions, missing-cache generation,
campaign/save-load transitions, demos and peer interoperability remain open.
The inherited jump run-start helper is shared by these synthetic comparisons;
its full original-source parity has not been established. Default RTCW
startup remains gated. No full compatibility claim is made.

The unrelated Q3/QL routing-cache reader bounds audit remains open. No new
unrelated production issue was found in this checkpoint.
