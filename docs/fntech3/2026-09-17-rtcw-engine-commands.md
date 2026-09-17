# RTCW engine command audit

This audit starts at `0b6af9ba7a17d73bf27c5c016c00772a1a93899a` with the
existing modified worktree, including the earlier activation/pickup correction.
It inventories engine console commands from the original SP and MP source
releases, including input, client, server, filesystem, sound, renderer and
platform registrations. Original SP and MP evidence stays separate.

## Coverage and scope

The [reviewed inventory](rtcw-engine-commands.json) accounts for **183 distinct
names**: 171 occur in SP and 176 in MP. Every entry has original file/line/handler
provenance, a disposition and an explanation. The original files are pinned by
SHA-256. The [audit script](../../scripts/audit_rtcw_engine_commands.py) checks
that each implemented command still has its engine registration and can
optionally recheck the pinned original source trees.

| Disposition | Names | Meaning |
| --- | ---: | --- |
| Engine handler | 168 | Includes existing shared commands and the runtime corrections below; individual build/platform/backend guards still apply. |
| Optional tooling | 10 | Legacy cache gathering, image cropping, tag diagnostics, loading-progress estimates and translation authoring. |
| Legacy integrations | 4 | `setenv`, `shellExecute`, `buyNow`, `singlePlayLink`; their old environment/URL/purchase interfaces are outside local gameplay. |
| Unavailable service | 1 | MP `gameCompleteStatus` explicitly reports that no match report was sent. |

This is registration coverage, not a claim that 168 commands have complete
retail behavioral parity. The audit includes conditional registrations rather
than pretending every platform and renderer exposes the same command set.
`banUser`/`banClient` remain conditional legacy authorization commands; their
presence does not establish an external service. The inherited `loadgame`
routine in original MP is an SP save loader, not evidence of an MP campaign
contract; fnTech3 keeps it SP-only.

Weapon selection, `weapalt`, `weaponbank`, `+scores`, `+inventory`, `savegame`,
cheats and game-specific gameplay commands remain owned by the supplied game
or cgame modules. Engine commands must not replace that dispatch. Existing
`map`, `devmap`, `spmap`, `spdevmap`, `map_restart` and SP `loadgame` use the
already implemented profile-aware map/campaign paths. RTCW menu mode switching
uses the existing shared-executable relaunch controller.

## Runtime corrections

All entries in this table include both the press and release command.

| Commands | Contract |
| --- | --- |
| `activate`, `useitem`, `salute`, `sprint` | Regular RTCW button bits 64, 4, 8 and 32. Activation was already present. |
| `attack2`, `zoom`, `quickgren`, `reload` | Separate weapon-button bits 1, 2, 4 and 8. |
| `leanleft`, `leanright` | Separate weapon-button bits 16 and 32. Activation plus keyboard strafe continues to generate lean. |
| SP `wbutton6`, MP `dropweapon` | Mode-specific names for weapon-button bit 64. |
| `wbutton7` | Weapon-button bit 128. |
| `kick` | Separate `wolfkick` byte, 0 or 1. |

The new states use the existing two-source ownership, binding generations,
source removal and focus/reset machinery. Short taps survive one generated
command; held actions persist until released. Kick also gets this short-tap
policy, improving on the original integer truncation of fractional held time.
Q3/QL retain their own regular button field and do not acquire these RTCW-only
engine commands.

SP `notebook` clears the mail indicator and opens the supplied UI's notebook.
MP `help` opens its help menu. Both require an initialized UI and active,
non-demo play. MP `ui_restart` restarts an initialized UI; `LoadTranslations`
uses the existing bounded catalog reload. Both modes have `updatescreen` and
`setRecommended`. Recommended settings execute the appropriate retail
high/low-video configuration and honor the optional restart argument. Supported
modern platforms use the high-CPU tier; this does not reproduce RTCW's obsolete
CPU-speed classification.

`startLimboMode`/`stopLimboMode` now select `say_limbo` for ordinary chat while
preserving private-message and team-chat precedence. Limbo state resets with
the connection/map state. Chat notifications **and chat-field geometry queries**
are QL-only exports, so RTCW and Q3 native cgames now use the normal engine chat
layout without receiving unsupported QL callbacks.

SP `cld` deliberately consumes console input without action, as the original
engine does. Actual cgame damage still uses its existing usercmd field or an
explicit client-command path. MP match completion consumes `gameCompleteStatus`
with an explicit unavailable message; it does not manufacture reporting success.

`music` now reaches the existing SP music state handler instead of bypassing it.
RTCW's one-file form supplies the same intro/loop filename. SP `queuemusic`
uses that handler with the original -1/-2 queue policy and bounded integer
arguments. Existing music/backend tests cover the underlying queue behavior.

## Verification

- Windows x86 development client, dedicated server and all three renderer
  modules build using the project preset with `rtcw-development=true`.
- The final focused Windows x86 run passes **112 tests and 34 subtests**.
  This includes command execution, module exports, command ABI, input recovery,
  music and entity-stream regressions.
- The five new command/audit tests pass on Linux x86. Their executable fixtures
  call real engine handler bodies and marshal action values through both frozen
  RTCW usercmd adapters; they do not operate input devices.
- Named-action and chat-export regressions fail at behavioral assertions when
  compiled against the pre-task files, then pass with the corrections.
- The complete Windows x86 Meson attempt reports **110 passing targets, two
  failures and two skips**. Its Python aggregate runs 1,512 tests with five
  failures and 62 skips. The failures are in separate, concurrently edited
  HUD/FOV work: the HUD fixture's `Cvar_Get` linkage, an old loading-screen
  draw assertion, a missing HUD stub in the render-record fixture, RTCW
  refdef-flag expectations, and the RTX FOV source assertion. The latter also
  fails its standalone Meson target. These are recorded separately from the
  passing command checks; this is not an all-green full-suite claim.
  A subsequent recheck of all five affected files reports 38 passed, 11
  subtests passed and three failures: the loading-screen source assertion,
  missing HUD fixture stub, and RTX FOV source assertion. The other two
  failures clear during the separate work; see `unrelated-recheck.log`.
- Two installed-runtime startup probes pass with the same Windows x86 client
  against the legitimate `E:/SteamLibrary/steamapps/common/Return to Castle
  Wolfenstein` installation. They check command lists, SP/MP exclusions, safe
  inactive menu guards, screen update, MP UI restart and translation reload,
  SP queue usage validation, and the MP reporting diagnostic. Both exit normally
  with no engine errors and leave installation metadata unchanged.

Runtime probes use windowed mode, disabled mouse/joystick input, muted sound,
loopback networking and isolated writable homes on an unswitched private
desktop. They do not inject keyboard/mouse events, take operating-system
screenshots or claim visual/gameplay validation. Writable homes are removed.
The manifests pin commit/worktree context, command lines, executable and
renderer hashes, profile, installation and outcome.

Build, test, audit and runtime evidence is retained under
`.tmp/rtcw-engine-commands/`. The complete Meson suite result is recorded in
`full-suite.log`; final focused verification is in `final-focused-x86.log`
and `linux-final-x86.log`. Final binary/installation identities and runtime
outcomes are in `final-runtime/manifest.json`.

The broader campaign, actual player interactions, renderer/audio parity and
bidirectional multiplayer gates remain open. The optional authoring tools and
legacy integrations above remain explicitly excluded; master match reporting
remains unavailable.

## Repeating the audit

```powershell
python scripts/audit_rtcw_engine_commands.py
python scripts/audit_rtcw_engine_commands.py --reference-sp E:/_SOURCE/_CODE/RTCW-SP-master/src --reference-mp E:/_SOURCE/_CODE/RTCW-MP-master/src
python -m unittest discover -s tests -p rtcw_engine_command_tests.py
```

## Provenance

The original [RTCW SP engine](https://github.com/id-Software/RTCW-SP/tree/master/src)
and [RTCW MP engine](https://github.com/id-Software/RTCW-MP/tree/master/src)
provide command names, button values, menu ownership and runtime behavior facts.
Their GPLv3 sources were inspected as references; no external implementation
text, proprietary modules or assets are incorporated. The corrections and
tests are independently authored within fnTech3's existing GPLv2-or-later code.
