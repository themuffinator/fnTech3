# Completed branch integration — 2026-10-23

The combined source at `a9ef0e8dd6f97bb188b510bfd5352024f76ffbc9` brings the
completed Jedi Academy, liquid and RTX work onto main's CSM lighting baseline.
The [JSON companion](2026-10-23-recent-work-integration.json) records source
identities, validation commands, runtime binaries, installation paths and
capture hashes. No compatibility gate closes.

## Source review and merge

The starting main was `d01a4b72e803726ecb228c1b0f7fb5959eba1200`. All four
Claude worktrees were clean when integration resumed after the RTX work
finished, and remained clean at the final branch review.

| Completed work | Source | Integration |
| --- | --- | --- |
| [JKA server surfaces, skins and model removal](2026-10-22-jka-g2-surfaces.md) | `816aa758`, `3099ae8a` on `claude/happy-taussig-aba6a3` | `912aca1e`; retained both sides of the changelog and ledger conflicts, with the gore cleanup correction below |
| [Profile liquid feeds and underwater audio](2026-10-22-liquid-feeds-all-profiles.md) | `5d7f2d8d`, `3aeccd6c` on `claude/priceless-benz-d33f29` | `039adaf9`; merged without textual conflicts |
| [RTX quality](2026-10-22-rtx-quality.md), [traversal/filter optimization](2026-10-23-rtx-optimization.md) and [forest speckles](2026-10-23-rtx-speckles.md) | The completed uncommitted work on `codex/csm-baked-lighting`, preserved in `2f932ca5` | `a9ef0e8d`; merged without textual conflicts |

`claude/unruffled-sammet-293a0c` (`2cf81276`) was already an ancestor of main.
`claude/stoic-shaw-af895f` (`3369bee7`, `abacf0f0`) was already integrated by
`863969c6` and `308cef72`: range-diff confirmed the same portal-sky code and
the intentionally adapted documentation. It was not merged a second time.
The other existing Codex branches were already ancestors of main.

The code remains fnTech3's own implementation. This integration introduces
no external implementation text or proprietary files. Existing source and
retail observations remain attributed in their original records.

## Integration correction

Main had acquired skin-gore state after the JKA branch forked. Its new
`CG_G2_REMOVEGHOUL2MODEL` path removed the model and skin binding but left that
gore state behind. Reusing the model slot could inherit stale marks, and the
removed model's shared-set reference was not released.

`gore::State::ClearModel` now releases only the removed model's reference and
erases its mapping. The import calls it only after a successful removal or
last-model deletion. Absent models and failed removals leave the state alone;
other model slots and duplicate instances retain their references. The retail
removal observation at `0x4decc0` in the surfaces record already includes this
release, so no new retail behavior is inferred.

The production import fixture in `tests/jka_cgame_remove_model_tests.py`
reproduced the missing cleanup before the fix. It now covers a shared set
through the actual duplicate import, private sets in other slots, negative
and out-of-range removals, repeated absent removal, slot reuse, final-model
deletion and final duplicate cleanup. Mutation checks reject both omitted
cleanup and clearing the entire instance.

The full suite also exposed two stale fixtures on the starting main. The
Ritual model-light fixture lacked the CSM sun state and capture helper; the
Elite Force alpha assertion expected the earlier depth-fragment condition.
All five failing renderer subtests reproduced at `d01a4b72`. Commit
`4a7d263f` updates the Ritual fixture to use the real CSM helper and verifies
the sun budget and full-bright exclusion, and keeps the alpha check strict
while accounting for the CSM caster guard. Only tests changed in that commit.

## Validation

The shared client, dedicated server, all three renderers and native tests
built successfully in the existing `meson/build/win32-dev` preset using
MSVC 2022 x86, `debugoptimized`, strict warnings and four build workers.
This was an incremental rebuild of every affected target, not a clean export
build or a Linux build.

- The focused JKA checks passed: 40 tests and 913 subtests.
- The focused liquid, RTCW, portal-sky, audio and manual checks passed:
  90 tests and 903 subtests.
- The native Meson suite reported 162 passed, one skipped and zero failed.
  `fnql_audio_loopback` skipped because `ALC_SOFT_loopback` was unavailable.
- The audio-zone material-map check passed with the built tool (19 tests).
  The retail QL factory probe initially skipped automatic discovery, then
  passed all five tests when given the legitimate installation's actual path.
- The final manual, generated-document, changelog and release-packaging checks
  passed: 73 tests and 865 subtests, with one symlink test skipped because this
  process lacks symlink-creation privileges.
- The full Python run reported 2,897 passed, 127 skipped and 69,069 subtests
  passed, with the five pre-existing fixture failures described above. The
  corrected fixture files and CSM lighting checks then passed: 13 tests and
  83 subtests. All five failures were resolved in that follow-up. The full
  suite was not repeated after these test-only edits; the engine binaries
  remain the ones used by the completed build and runtime probes.
- Python skips include unavailable symlink privileges and opt-in native ABI
  probes. Audio-zone tool discovery and QL installation discovery skips were
  covered by the explicitly configured checks above. The JSON keeps the
  complete initial summary and skip details.
- The RTX shader generator's `--check` passed for all 97 embedded variants.

## Combined runtime smoke checks

Every run used the same freshly rebuilt binaries at the stable preset path,
an isolated home under `.tmp/merge-recent-work/runs/`, windowed 800 × 600 on
an unswitched private desktop, disabled mouse and joystick, no injected input,
disabled sound and no network sockets. Captures came from the engine's
`screenshotPNG` command. Each run reached its final console step and exited
normally with code zero; installation inventories and runtime binary hashes
were unchanged.

| Profile and renderer | Probe | Observed result |
| --- | --- | --- |
| Q3, RTX | `q3dm6`, `rtx_rt_mode 2`, `rtx_rt_require 1` | Compute ray tracing dispatched and the engine capture showed the level and HUD. |
| JKA MP, GLx | `mp/siege_hoth`, siege mode, `team red`, `addbot Tavion 1 blue` | Both players were admitted and the client kept drawing through two captures. The objectives/team menu remained open; this does not demonstrate gameplay or count import 311 calls. |
| RTCW SP, GLx | `forest`, `noclip`, `setviewpos 3584 -5120 150 0`, `r_underwater 1`, `r_liquid 1`, `cg_shadows 1` | The engine capture showed the underwater view; the earlier non-finite-mark error did not recur. |
| RTCW SP, RTX | The same forest position and settings, with required ray tracing | The log confirmed compute ray tracing and the underwater view rendered without the mark error. |

The x86 process on this NVIDIA RTX 4060 Laptop GPU exposes the compute tracing
backend; these runs do not validate the hardware ray-tracing pipeline. They
also do not measure FPS, listening quality, full liquid behavior across every
profile or bidirectional interoperability. The feature-specific records keep
their original, more detailed measurements and remaining limitations.

## Documentation and unrelated observations

The player overview and generated READMEs now describe the shared optional
underwater/ripple scope. The README's advice to avoid RTX on RTCW `forest`
was stale after main's earlier validated shadow fix and has been corrected.
JKA's bounded siege evidence is linked without claiming complete compatibility.

The RTCW logs still report missing authored `q3map_lightImage` files,
including `textures/lightimage/light_m15.blend.tga`, and shader/image reuse
warnings. The JKA siege log reports absent bot-route/navigation data and
`scripts/mp/siege_hoth/npc_spawner`. These warnings did not terminate the
probes; their underlying asset/script behavior was not changed here.
The two stale renderer fixtures on the starting main were repaired as
described above; no unresolved test failure was found in the merged engine.

Useful build/test logs, engine captures, probe configs and their metadata are
retained under `.tmp/merge-recent-work/`. Git removed the clean temporary
integration worktree after verifying that its history was preserved in main
and its resolved path contained no links. Automatic approval review rejected
the bulk scratch-file cleanup with "blocked by policy" before it ran, so the
disposable homes, cached retail modules, compiler scratch and test caches
remain in the task area. They are ignored and were not committed. The existing
feature worktrees and other chats' investigation output are preserved.
