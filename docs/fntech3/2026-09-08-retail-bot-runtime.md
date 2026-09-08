# Retail-module bot runtime after the command-boundary correction

Follow-up: the [probe and trace investigation](2026-09-08-bot-trace-and-probe-corrections.md)
identified command-queue interference in the COUNT_DOWN observations and
reproduced timestamp-only logging inside the unmodified retail DLL. The original
observations below are retained, but must not be treated as proof of an engine
countdown or file-write defect.

The unchanged Windows x86 dedicated executable from the
[bot-command correction](2026-09-08-bot-usercmd-boundary.md) runs four retail Q3
bots through pickups and combat. The legitimate QL API-10 game module also
admits four bots, but the tested bot-only matches do not establish progression
to an active scored match. These outcomes must not be described as equivalent.

| Profile and configuration | Observation | Total probe duration |
| --- | --- | ---: |
| Q3 q3dm6, explicit Sarge/Grunt/Visor/Major, skill 3 | Four bots in both status samples; pickups, two opponent kills and one self-kill; Major finishes with score 1; normal shutdown | 40.843 s |
| QL campgrounds/ffa, same explicit bots | Four bots in both status samples; bot-related stats callbacks; scores remain zero; normal shutdown | 20.578 s |
| QL with allready and g_doWarmup=0 | Starts PRE_GAME, ends COUNT_DOWN; four bots and zero scores; bot chat observed; normal shutdown | 35.953 s |
| QL with allready, g_doWarmup=0, g_warmup=0 and g_warmupDelay=0 | Still ends COUNT_DOWN with four bots and zero scores; normal shutdown | 25.907 s |

The initial bot_minplayers=4 experiment added Q3 bots gradually and produced
pickups, but admitted no QL bots during its observation window. Explicit addbot
commands were then used to separate admission from population policy. Timing
uses engine wait frames, so identical frame counts have different wall-clock
durations between profiles. The QL profile's read-only sv_fps was preserved.

The Q3 game log and console contain item pickups and concrete weapon kills,
including Sarge killing Visor with a rocket and Major killing Grunt with rocket
splash. This is retail-QVM gameplay evidence beyond the synthetic command
round trip. It does not establish every bot behavior, map or skill level.

QL loads the retail game module, campgrounds AAS and bot definitions, and
reports all four named clients as bots. Stats/achievement callbacks and chat
show activity, but neither admission nor those callbacks prove an active
scored match. Even the zero-warmup follow-up ends with g_gameState=COUNT_DOWN.
These are observations from bot-only configurations; a retail baseline or
human-client comparison is needed to distinguish expected policy from a
remaining engine/module defect. No match-progression fix is claimed here.

A separate diagnostic anomaly is recorded: the QL g_log file contains timestamp
prefixes without event text, whereas Q3 emits complete events. Its cause remains
unresolved. Optional QL stats services report their unavailable state; no
authentication, achievement or service success was synthesized.

All probes used the same engine binary, separate temporary writable homes,
read-only legitimate installations, loopback networking, sv_pure=1, empty master
addresses, disabled Steam integration and input, and private unswitched desktops.
There were no human clients or user input actions. All processes exited normally
with code zero, without forced termination. No screenshot was needed. A pure=1
bot-only server does not prove remote pure negotiation.

The [evidence record](2026-09-08-retail-bot-runtime.json) contains exact launch
arguments/configs, binary and asset hashes, decisive log lines, status samples,
log hashes and outcomes, including the unsuccessful QL population attempt.
Retail files, raw logs and generated game logs remain outside Git. No engine
source or tests changed for these runtime observations; the preceding tested
build remains the baseline.

The compatibility ledger keeps QL bot match progression and logging open, along
with retail client interoperability and the broader ABI/network/release gates.
Existing optional config/font and disabled-service warnings remain; no other
unrelated issue was discovered.
