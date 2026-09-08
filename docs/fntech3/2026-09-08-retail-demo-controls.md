# Retail QL pause limits and scripted frame advance

The retail comparison on 2026-09-08 resolves the interpretation of the earlier
QL timescale-zero observation: both retail and fnTech3 report timescale 0.1
after a script requests zero during native QL demo playback. Retail explicitly
reports a minimum of 0.1 and maximum of 10. Continued slow playback therefore
matches the observed retail limit, rather than establishing a broken pause.
Both retail and fnTech3 hold QL frames pixel-identically with cl_freezeDemo.

The [machine-readable evidence](2026-09-08-retail-demo-controls.json) records
source baseline `605fe2c`, binaries, input hashes, configs, logs, queried cvars,
process outcomes and image comparisons. It supplements the
[command-history correction](2026-09-08-paused-command-history.md); no engine
source changed in this follow-up.

## Direct comparison

The same identified retail-written QL demo was loaded by both executables in
fresh writable homes. The script sets timescale zero, waits, queries the actual
cvar at each screenshot checkpoint, restores normal speed and allows completion.
The queried value is 0.1 in both engines. Retail's two screenshots differ by
30,266 pixels as playback continues. That number is not a cross-renderer
comparison: the retail and fnTech3 viewport sizes differ.

A separate retail run sets cl_freezeDemo to 1, captures two paused frames,
unfreezes and completes. The paused images differ by zero RGB pixels. The
retail native UI shows Play/Advance controls over the frozen first-person scene.
The previous fnTech3 run likewise had zero differing QL paused pixels. The
precise code path responsible for restoring the timescale minimum was not
isolated; the runtime cvar observations establish the behavior being compared.

## Advance and hold

The existing engine demo-key handler in cl_keys.cpp queues:

```text
timescale 1; cl_freezeDemo 0; wait; wait; cl_freezeDemo 1
```

The probe invokes this console sequence directly while paused, waits for its
rendered result, captures an advanced image, waits another 60 frames, captures
a held image, then resumes to normal demo completion. It does not inject a key
or claim to test keyboard routing, focus, native mouse controls or exact
single-snapshot increments.

| Profile | Before versus advanced | Advanced versus held | Held versus resumed |
| --- | --- | --- | --- |
| Q3 | 64,457 differing pixels | 217, confined to lagometer bounds x=1185..1275/y=896..905 | 146,712 |
| QL | 5,949 differing pixels | 0 | 14,798 |

Inspection confirms first-person rendering advances and holds. QL's advanced
and held images both display demo time 0:00.907. Q3's connection warning remains
absent; its lagometer continues updating as recorded previously. All five runs
in this checkpoint exit 0 normally with completion markers and requested engine
screenshots. Seeking, reverse stepping, repeated/long cycles and native input
interaction remain unverified.

## Validation and unrelated diagnostics

Clients were windowed with input/audio disabled on private unswitched Windows
desktops. Commands came from startup scripts, and screenshots came from engine
render targets. No OS capture or keyboard/mouse control was used. Retail TGA
conversion was only for inspection. Legitimate installations remained read-only
inputs; raw demos, screenshots, logs and proprietary binaries remain untracked.

The full suite was not rerun for this documentation-only update. The preceding
[correction checkpoint](2026-09-08-paused-command-history.md) records the passing
Windows suite and Windows/Linux executable regression. This update passed
release-document staging and link validation.

Unrelated diagnostics persist: retail website-loading error -2, muted-audio
messages, and Q3's updating lagometer. The QL timescale-zero observation is now
classified as matching retail's observed lower limit, not an outstanding pause
failure. No general website, audio or complete demo-control compatibility is
claimed.

## Paused restart follow-up

At source baseline `70134ad`, the same client restarts each profile's demo while
cl_freezeDemo is 1. A separate restart config installs a new activeAction before
issuing demo again, avoiding a recursive restart loop. The new activeAction
queries cl_freezeDemo and captures a screenshot after five frames, waits another
90 frames, queries/captures again, and finally unpauses. The retail Windows QL
client repeats the same sequence with the same identified QL input.

All three runs retain cl_freezeDemo=1 at both checkpoints. All paused A/B image
pairs are pixel-identical. Inspection confirms a first-person world and HUD,
with both QL clients showing demo time 0:00.000 and paused controls. Both fnTech3
profiles and retail QL then reach the exact completion marker and exit 0 normally
after the explicit unpause. Commands, identities, hashes and outcomes are appended
to the machine-readable record under pausedRestartFollowup.

Retaining pause across this explicit same-demo restart matches the observed
retail QL behavior; it is not treated as unwanted state leakage or changed in
engine code. This does not establish cross-profile process switching (profiles
remain immutable), live-session transition behavior, repeated restart stress,
seeking to arbitrary times or native input routing. The same isolated,
windowed, input-disabled engine-screenshot procedure was used. Documentation
staging and links passed; no engine source changed or full suite rerun was needed.
Existing retail website/audio diagnostics remain separate limitations.
