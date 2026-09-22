# Enemy Territory HUD aspect correction

Enemy Territory now takes the engine's HUD aspect correction, so its 2D drawing
keeps its proportions on a non-4:3 display instead of stretching with the frame.
Its audio zones were already complete and are unchanged. This follows baseline
`f08e52e`.

## What was missing, and what was not

Audio zones were **not** missing. All six official maps — the complete inventory
of `etmain/pak0.pk3`, confirmed against the retail installation — already had
sidecars from the 2026-09-18 slice. Every one was regenerated from the
installation with the current compiler and compared byte for byte: all six
match, which also confirms that adding Elite Force's profiles to that compiler
left Enemy Territory's output untouched.

The HUD correction was missing, and not as a data file. The system is
whitelisted by title in two places — the client enablement in `CL_HudInit` and
the root-archive allowlist in `FS_RootArchiveAllowsFile`, which decides what the
engine's own package may serve. Enemy Territory reached neither. Its own compat
layer already routes its 2D draws through `CL_HudDrawStretchPic` and
`CL_HudAdjustPic`, so nothing else needed to move: both predicates now include
ET and the correction applies.

## What the correction does here

The client was run on `oasis` at 1280×720 with the correction off and on. With
`cl_hudAspect 0` the limbo panel stretches to fill the frame. With `1` it
renders centred in 4:3 with pillarbox bars and the whole composition intact.
Both frames show the same mission time, so this pair isolates the correction —
unlike the Elite Force gameplay pair recorded on 2026-09-21, which compared two
differently-timed frames and was withdrawn as evidence for that reason.

Captures stay under `.tmp`, never in the repository, because they are derived
from retail assets.

## Why there are no rules

`pkg/games/wolfet/etmain/fntech3-hud.json` ships with an empty rule set, and
says so in the file.

Enemy Territory's client reaches the limbo panel and no further, which is what
the [client run record](2026-09-18-wolfet-client-run.md) already stated. A
`cl_hudDump` capture there records 161 draw groups across 67 shaders, and all of
them are the limbo panel, the console backdrop or the menu cursor. The limbo
panel is one full-screen composition: pinning its pieces to different edges
would pull it apart, exactly as it would Elite Force's loading emblem, so the
correct rule set for what can be observed is the empty one.

Reaching the in-game HUD was attempted rather than assumed. The `team` command
was accepted and the server logged the player entering the game; shortening
`g_redlimbotime` and restarting the map did not change the recorded set, because
closing the limbo panel needs a key press and the harness injects no input. ET's
health, ammunition, compass and stamina draws have therefore never been recorded
here, and anchoring rules for them are absent rather than guessed.

`scripts/probe_wolfet_client.py` now preserves the engine's HUD dump alongside
the console log and captures it already kept, so the capture can be repeated
once the client can reach ordinary play.

## Validation

`tests/wolfet_presentation_assets_tests.py` pins both enablement predicates,
that ET's compat layer routes its draws through the HUD path, that the empty
rule set carries its own explanation rather than being indistinguishable from a
forgotten one, that every official map has a parsable sidecar, and that Enemy
Territory keeps its own material interpretation — its `0x40` is a water splash
where RTCW's is ceramic — now that the compiler has more profiles.

Four existing tests needed updating and are corrected in the same commit: the
compiled HUD probe needed a `Com_GameIsWolfET` stub to link, the
launch-and-transition source check and the Elite Force wiring check both pinned
the enablement line, which is now shared by four titles and is asserted by
membership instead, and the release packaging counts rose by one required
destination for ET's rule file.

## Remaining gates

No per-element HUD anchoring is claimed for Enemy Territory. Only the centred
uniform correction is enabled and observed, and only on the limbo panel — no
gameplay HUD, and no gameplay, is claimed here.

The audio zones remain what the 2026-09-18 slice recorded: a first pass from BSP
geometry and shader evidence, with no listening check made against any of them.

Enemy Territory remains development-gated; release builds still refuse the
profile.
