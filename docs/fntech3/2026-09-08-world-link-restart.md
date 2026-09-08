# Clear stale world links when restarting game modules

Game-module restart retained world-sector entries from the previous entity
binding. A replacement module could expose fewer entities or query collision
before relinking them, causing `SV_AreaEntities_r: invalid world entity`.
The server now clears its world after GAME_SHUTDOWN and before invalidating
the old binding and initializing the replacement module. World reset clears
both sector lists and entity link pointers while retaining snapshot baselines.

An independently authored Q3 native fixture leaves entity 63 linked, restarts
with only two registered entities, then queries collision. It reproduces the
error before the fix and passes afterward, including ordinary link, area,
trace, point-content and unlink checks. Invalid-stride rejection and recovery
still pass. An executable unit test checks both link directions, repeated reset,
world-tree reconstruction and preservation of baseline values on Windows and
WSL Ubuntu.

The same Windows x86 dedicated binary ran legitimate retail QL API-10 qagame
with four bots. With the startup command queue left clear, the game transitioned
from campgrounds to retribution and restarted its native game module. Authenticated
loopback RCON observed IN_PROGRESS at 5.328 and 28.375 seconds; status observed
Sarge, Grunt, Visor and Major, with Sarge's score increasing from zero to one.
The process shut down normally at 30.313 seconds with exit code zero and no
forced stop. These are observed restart and scored-gameplay facts, not proof of
remote retail-client interoperability or long-session transition robustness.

The [evidence record](2026-09-08-world-link-restart.json) contains before/after
manifests, source and executable hashes, legitimate input hashes, launch/config
details, lifecycle lines and status responses. Runs used isolated writable homes,
private unswitched desktops and read-only installations. Temporary RCON credentials
are redacted. There was no keyboard/mouse control or screen capture; proprietary
inputs and raw logs remain outside Git. The recorded base commit precedes the
dirty source changes included in this checkpoint; source hashes identify them.

Validation: Windows x86 rebuild passed; the full configured suite passed 110
tests with one OpenAL skip. Its Python aggregate ran 1,277 tests with five skips,
including enabled Q3 and QL native runtime probes. The existing Linux i686 dedicated
scratch build passed all four incremental steps; it is not a clean release build.

The prior retail module timestamp-only logging anomaly and optional config/font
and unavailable-service diagnostics remain. No new unrelated defect was found.
Broader transition stress, pure admission with retail peers, and other open
compatibility gates remain required.
