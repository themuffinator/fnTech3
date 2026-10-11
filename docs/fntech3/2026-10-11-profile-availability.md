# Game availability and release status

Every one of fnTech3's sixteen profiles is selectable in every build, including
release, dedicated-server and diagnostic builds. The former startup restriction
confused unfinished compatibility evidence with availability: `RuntimeBlocked`
rejected a catalogue entry carrying a warning unless a game-specific development
macro bypassed it. That restriction has been removed from detection and
activation. The profile's `compatibilityWarning` remains informational in the
launcher and startup log. The launcher no longer suggests recompiling to unlock
a game.

The old Meson `*-development` options and PowerShell helper switches remain
accepted for existing scripts. Neither true nor false can hide or disable a
profile. The legacy `q3-only` diagnostic option still selects a separate Windows
x64 output, but no longer filters the catalogue. It does not make x86 retail
libraries loadable in a 64-bit process.

Profile immutability, installation identity, ambiguous-installation rejection,
per-profile protocol/module contracts and host ABI checks remain unchanged.
Windows-only retail modules still require Windows x86. All existing incomplete
compatibility gates remain incomplete; selection is not proof of playable
campaigns, rendering, saves or network interoperability.

GitHub releases must always be ordinary releases. The workflow creates an
ordinary draft with `--prerelease=false`, validates that status before uploading,
and writes `prerelease: false` in every package manifest. The candidate packaging
channel still records missing runtime evidence as `not-established`; the stricter
GLx promotion/evidence channel retains its requirements. Release classification
does not substitute for compatibility evidence. Published version tags and assets
remain immutable.

The workflow also rejects prerelease manifests from older tagged scripts before
any release API write. An old tag cannot bypass the current publication policy.

`tests/game_profile_availability_tests.py` detects every profile explicitly and
from its own marker, activates each in a fresh process, and checks immutability
and rejection of forged catalogue entries. It runs default, legacy-flags-off,
legacy-flags-on, dedicated and legacy-diagnostic configurations. Release workflow
tests require ordinary manifests and drafts and reject prerelease drafts.

Validation used the Windows x86 Release configuration with all ten legacy
development options explicitly false. The client, dedicated server, launcher
and all three renderers built. All 159 available native Meson groups passed;
the OpenAL loopback group skipped because its backend was unavailable. The
profile matrix passed on Clang/MSVC x86, MinGW GCC 16.2 x86 and Linux x86.
The production dedicated executable's `gameList` also reports all sixteen
profiles as startup enabled with all ten development options false.

That same release client started Q3 on `q3dm1`, QL on `campgrounds`, RTCW SP
on `escape1` and RTCW MP on `mp_beach`, and wrote engine screenshots before
exiting normally. Its dedicated server answered nonce-checked info/status
queries for Q3, QL and RTCW MP on those maps. The probes were windowed on an
unswitched private desktop with isolated home paths and no input injection.
RTCW SP reached the mission briefing; this is startup evidence, not campaign
completion. Binary hashes, commands and local evidence locations are recorded
in the adjacent JSON and `.tmp/always-available-games/runtime-smoke/result.json`.

Historical records retain the options and gating policy used at the time of
their observations. This policy supersedes those availability restrictions;
it does not revise their measured outcomes.
