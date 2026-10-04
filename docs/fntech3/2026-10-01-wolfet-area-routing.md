# ET area-cache costs and contents flags

The [Holomatch routing record](2026-09-29-stvef-route-seed.md) identified an
open ET mismatch: fnTech3 started an area cache with Quake III's zero costs,
while installed `et.exe` starts with the goal area's first cost row. This
follow-up repairs the ET cache update and its contents flags. It adds
original-executable numeric comparisons, without starting a game. The
[companion JSON](2026-10-01-wolfet-area-routing.json) records identities,
commands, observations, regressions and the remaining limits.

## Observed contract

The read-only retail executable has SHA-256
`4d1326c5936b61c5f878fc017e4aa0abddf13e008139e31fce2f13d57b26243e`.
Disassembly and bounded x86 emulation agree on these rules:

- `AAS_UpdateAreaRoutingCache`, `0x00481290`, starts with
  `areatraveltimes[goal][0]`. Each reversed link into that goal pays the time
  in its own entry of the row, as well as its link and the cache's start time.
- A disabled destination is skipped. ET's area flag `0x10` adds 1,000 to the
  route cost; it is not Quake III's bridge flag. Otherwise, `0x800` adds 200
  when the route allows Axis travel (`0x04000000`), and `0x1000` adds 200 for
  Allied travel (`0x08000000`). General avoidance takes precedence, and the
  two team penalties do not accumulate. Costs and comparisons use sixteen
  bits, including wraparound.
- Eligible links increment the frame-update counter, even when the source
  area's missing cost table then prevents publishing that route. Rejected
  travel types, disabled destinations, rejected contents, foreign clusters
  and areas beyond the cluster's reachability range do not increment it.
- `AAS_AreaContentsTravelFlags`, `0x0047cf00`, returns a liquid's own flag
  first. Otherwise it returns air with the `256` do-not-enter and `1024`
  large do-not-enter restrictions. It does not interpret Quake III's team
  contents or bridge area flag. These numeric rules match RTCW's existing
  contents helper, which ET now selects explicitly.

The original dedicated executable, SHA-256
`7eb606913a56d6901e358a598fb0079120b2730e113a10fc963f90b777580cdf`,
has the corresponding functions at `0x0044bbf0` and `0x00447860`. Repeating
the same generated cases there gives identical costs, first links, counters
and contents flags.

The official [ET routing source](https://github.com/id-Software/Enemy-Territory/blob/40342a9e3690cb5b627a433d4d5cbf30e3c57698/src/botlib/be_aas_route.c)
is a GPLv3 reference, used for observations only. No implementation text is
incorporated. The numeric comparisons use the installed executable as their
authority; the new penalty helper and test-world construction are independently
authored. No executable, navigation or proprietary asset bytes are committed.

## Implementation and comparisons

The production routing update selects ET's goal row, applies ET's avoidance
penalties and counts eligible links. It skips a missing source cost table
before changing the cache. Additional bounds checks reject negative cluster
indices, invalid outgoing row indices and missing individual rows; the
frame counter saturates rather than overflowing a signed integer.

`scripts/probe_wolfet_area_routing.py` runs only the original cache-update and
contents functions in Unicorn 2.1.4. It maps the supplied image read-only from
disk into emulator memory, substitutes generated navigation records, and
limits instruction count, time and allowed executable addresses. It executes
no entry point, operating-system imports, renderer or input code. The generated
two-area cases exclude their return link by travel type, avoiding cycles.
The four-area cases give the goal three different incoming costs and offer
two first links, so the comparison also checks route selection.

Before the correction, production returned 11 for the ordinary generated
route where retail returned 28. Afterward, all 42 observed caches match their
costs, chosen outgoing links and update counts. The 128 observed contents
combinations also match under six area-flag combinations, giving 768 compiled
checks. Fixtures cover general/team avoidance, precedence, rejection, missing
tables, cluster limits, zero/wrapped costs and alternative first links. Separate
production tests cover absent goal rows, malformed indices, partial cleanup,
shutdown and a saturated counter.

Quake III, Quake Live, RTCW, Elite Force and Jedi profile controls retain their
existing valid routing behavior. Holomatch's mutation regressions and the
goal-link regressions still exercise its distinct goal row and disabled-area
rules, including goals entered by more than 128 links. The Holomatch measurement
harness has been updated for the expanded profile predicate.

## Runtime regressions

The rebuilt stable x86 binaries load `q3dm1` (Quake III), `campgrounds`
(Quake Live) and `hm_borg1` (Holomatch) on isolated dedicated servers. Each
loads its installed navigation, initializes routing, lists the requested bot
and exits normally. The installation inventories and tested binary hashes
remain unchanged. An additional windowed ET client joins both teams on
retail `railgun` with pure checking and visibility filtering enabled, then
exits normally with the installation unchanged. These tests use console
commands on private desktops; no mouse or keyboard input is injected.

The first three dedicated probes placed their completion commands in the
client-only `activeAction` hook and were deliberately stopped at their
timeouts. They are retained as invalid setup evidence. The corrected probes
queue their dedicated-server commands directly and pass. This setup error
was not an engine failure.

## Evidence limits and further gaps

The supplied ET installation has four archives and no `.aas` or `.rcd` entry,
nor a loose navigation file. The executable comparison supplies generated cost
matrices directly: it does not establish loaded ET navigation, its geometry-to-
cost calculation, complete bot routing or gameplay. Stock-map startup and
admission regressions cannot exercise that missing content.

This checkpoint also found ET's loader selecting Quake III's 28-byte area
settings instead of ET's 32-byte records. The
[AAS8 loading follow-up](2026-10-01-wolfet-aas8-loading.md) repairs that disk
boundary with generated load, failure and cleanup checks. ET's geometry-to-
cost calculation and routing-cache query/budget rules still require their
own comparisons. Neither correction closes loaded retail ET navigation,
complete bot routing or release gates.

This change introduces no presentation path or retail-exact option. Existing
fnTech3 audio, rendering and input features retain their scope. Alice remains
deferred by the user; RTCW's interactive `escape1` listening check is still open.

Examples of earlier records lacking the JSON companions required by `AGENTS.md` are
`2026-09-24-engine-input-capture-fixes.md`, `2026-09-17-wolfet-pure.md` and
`2026-09-25-wolfet-demo-playback.md`. This is existing documentation debt;
their historical evidence has not been reconstructed or relabeled here.
