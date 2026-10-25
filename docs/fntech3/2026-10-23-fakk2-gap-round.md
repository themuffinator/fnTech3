# Heavy Metal: F.A.K.K.2 gap round, 2026-10-23

## Scope

Among the titles other than Quake III Arena, Quake Live and Elite Force,
Heavy Metal: F.A.K.K.2 had the fewest open gaps in the
[ledger](COMPATIBILITY.md#fakk2-gate-matrix): retail `fakk2.exe` 1.02 had
already played fnTech3's demos, drawn the same frames and played on its
server, fnTech3's client had played on retail's, and saves had crossed both
ways, all on the first level. What was left was a rendering difference
nobody had traced (the canal water on `over` and `under`), sessions and saves
beyond the first level, damaged input, and the retail server's later levels,
which need the disc. This round settles the first and a second it found on
the way, the density of Ritual's fog volumes; it extends the others as far as
retail's client can take them without the disc, and corrects one stale ledger
line. Its machine-readable companion is
[2026-10-23-fakk2-gap-round.json](2026-10-23-fakk2-gap-round.json).

Every gate stays partial and the release gate stays closed.

## The runs

- **Build:** a `git archive` of `f4d07ef0` built in `.tmp/fakk2-round/b`
  (Windows x86, MSVC 2022, `debugoptimized`, strict warnings, the three
  renderers), frozen in `.tmp/runtime/fakk2-base`; the same executables with
  this round's renderer libraries in `.tmp/runtime/fakk2-fix` (the stage fold)
  and `.tmp/runtime/fakk2-fog` (the fold and the fog image). `fntech3.exe`
  SHA-256 `e7159320...`, `fntech3.ded.exe` `516533d3...`; the renderer
  libraries' hashes are in the companion. The 2026-10-24
  [playability round](2026-10-24-fakk2-playability-round.md) has since put
  its own build in `fakk2-fix`, so the fold-only libraries are known by
  those hashes alone.
- **Retail:** `fakk2.exe` 1.02 (SHA-256 `681bc51c...`), the official patch's
  plain executable, run from `.tmp/runtime/fakk2-retail` as a client only; its
  server needs the game CD.
- **Installation:** the layout laid down from the user's disc under
  `.tmp/fakk2-install/game`; every manifest records it unchanged.
- **Conditions:** windowed at 800x600 on a private desktop, isolated homes
  under `.tmp/fakk2-round/`, loopback only, no mouse or keyboard input, frames
  from the engines' own screenshot commands.

## Canal water and the stage fold

**Observed.** The canal water on `over` and `under` is lime green in retail
and was a bluer green in fnTech3. Its shader,
`textures/under/tunnelwater`, is a lightmap, the swamp-water texture modulated
over it, and 47 caustics frames added with `rgbGen constant 0.5 0.8 0.5`. The
earlier record measured that retail's caustics add no blue. Laid over both
engines as a test `pak9.pk3` (`.tmp/fakk2-round/tools/caustics_variants.py`),
white caustics frames under `rgbGen constant 0 0 1` add nothing to retail's
water, under `constant 1 0 0` they add red, and under `rgbGen identity` they
add white: retail loses the blue of a constant colour there, and only of a
constant. Retail's GL log (`r_logFile 1`) draws the pass as fnTech3 does, a
`GL_ONE GL_ONE` pass over the folded lightmap pass. Under cdb, the stage
`fakk2.exe` draws holds the constant `00 00 00 00` for `constant 0 0 1`. A
write breakpoint on the byte its parser stored (0x00476609) caught the writer:
the shader's finish (0x00479112).

**Retail's rule.** FinishShader folds neighbouring stages into one
multitexture pass (`fakk2.exe` 1.02 0x00478ead..0x004791c0; `alice.exe`
0x0048e798..0x0048ea83 is the same code). A pair folds when neither stage has
a texture environment yet (`nextBundle` gives one), their state but for
blending and depth writes is the same, the first is opaque or blends as the
second, the second modulates (`GL_DST_COLOR GL_ZERO`, `GL_ZERO GL_SRC_COLOR`)
or adds (`GL_ONE GL_ONE`, table 0x004f87d0), and their colour and alpha
sources and waves are the same. It then combines the two stages' constant
colours: a product for a modulate, and for an add the sum, which its clamp,
written inverted, raises to at least 1 (0x004790b0..0x004790dd). It stores the
four bytes 0x2ed apart (0x00479117), where its stages lie 0x2ec apart, so
channel k lands in the constant colour of the k-th stage from the folded one.
The next stage left after the fold takes the pair's blue, and the one after
that its alpha. Two stages with no constant of their own combine to 0 for a
modulate and to 255 for an add. The canal's lightmap and water fold, and the
caustics stage, next, takes their 0 as its blue.

**Change.** `R_RitualFoldStageColours` (`code/renderercommon/tr_alice_shader.h`)
puts the colours where retail's fold leaves them, at the start of each
renderer's FinishShader and before any of fnTech3's own folding, under the
Alice and F.A.K.K.2 profiles. A folded pair's second stage gives its constant
up to white, since retail draws the pair with the first stage's colour alone,
whether or not this renderer folds it. The header's list of retail bugs not
reproduced now names this one as reproduced: shipped scripts look as it
leaves them. Medal of Honor is left out: its executable cannot be read, and
OpenMoHAA's fold combines no colour.

**Which shaders.** Through the real parsers, every installed script
(`tests/alice_shader_parse_tests.py`): eight F.A.K.K.2 shaders draw
differently, and no Alice one. The canal water and the six caustics walls and
ceilings of `under` lose their constant's blue. `textures/otto/lt_fluorescent1`
(on `otto` and `homes2evil`), two constant stages folded as an add, draws its
first stage with red 255 and its second with no colour of its own. Every other
change writes bytes the stage never draws with.

**Live.** `over`'s first view, retail's client and fnTech3's each on
fnTech3's dedicated server, the mean colour of two boxes of water clear of
Julie:

| Run (default far fog) | Right box | Left box |
| --- | --- | --- |
| retail `fakk2.exe` 1.02 | 31.8, 61.0, 33.9 | 20.9, 43.3, 23.6 |
| fnTech3 before, GL | 35.0, 68.6, 60.2 | 21.5, 42.1, 41.5 |
| fnTech3 after, GL | 33.9, 66.4, 36.2 | 21.4, 45.3, 24.5 |
| fnTech3 before / after, Vulkan | blue 59.0 / 35.6 | blue 42.6 / 24.3 |
| fnTech3 before / after, RTX | blue 86.6 / 59.1 | blue 45.6 / 23.5 |

With white caustics frames, retail's water and fnTech3's after the change take
no blue from them, where fnTech3's had taken 116. The RTX raster path's water
is brighter than the other two, from its own liquid shading; Vulkan's white
frames bloom. (`tests/alice_shader_parse_tests.py`: the fold's channels, the
add's raised sum, the next stages' blue and alpha, the constant pair, the
pairs that do not fold, and no other profile's colours.)

## Fog volumes and the fog image

**Observed.** With the far fog off (`r_farplane_nofog 1`), both engines draw
`over`'s black fog volume over the canal, and fnTech3's left about half of
the water's light where retail's left four fifths. Four stock maps have fog
volumes and no far fog, so retail draws them by default there: `cemetery`
(nine volumes), `cliff1`, `cliff2` and `oracleway`.

**Retail's rule.** The fog's distance scale is Quake III's, 1/(8 x depth)
(`R_LoadFogs` 0x00485c7b), and so are its texture coordinates, with a point
inside a volume at t = 31/32 (0x00494f9a) and the square-root table
(0x0046fb50). Its 256 x 32 fog image is not Quake III's
(0x0046fdbb..0x0046fef1; `alice.exe` 0x00484b83..0x00484c9a, the same). Where
Quake III scales the distance by (t - 1/32) / (30/32) below t = 31/32, it
scales by (t - 1/256) x 32/254 below t = 255/256, the image's width put where
Quake III has its height. No row of 32 reaches 255/256, so every texel takes
the scale, and a point inside a volume keeps about 0.97/8 of the distance
Quake III's image gives it: the volumes are much thinner.

**Change.** `R_RitualFogFactor` (`code/renderercommon/tr_fog_math.h`) is that
texel rule. Under the two Ritual profiles each renderer builds its fog image
from it and answers its CPU vertex fog with it (`R_FogFactor`; RTX's
`R_CompatibilityFogFactor`); every other profile keeps Quake III's.
`tests/ritual_fog_volume_tests.py` checks all 8,192 texels against an
independent transcription of retail's arithmetic, the thinner fog inside a
volume, and the renderers' routing.

**Live.** `over` with `r_farplane_nofog 1`, fnTech3 alone on its listen
server: the right box of water reads 13.2, 27.3, 13.9 before and 27.2, 56.3,
28.3 after, where retail reads 27.6, 58.0, 28.9; without the caustics, 26.0,
38.7, 28.4 against retail's 26.6, 40.5, 28.7. Julie and the walls are lit as
retail's are. The left box reads brighter than retail's, under fnTech3's own
water ripples. On the four maps with fog volumes, the first views barely look
into one (at most 7,608 pixels changed by more than 8). Alice uses the same
image, and eight Alice maps have fog volumes and no far fog; `alice.exe`
cannot run here, but on fnTech3's `fortress1`, whose opening looks into a
black fog volume, the spikes hanging inside it now show through, and nothing
else in the view changes.

**Not reproduced.** Retail fades a blended stage inside a fog volume on the
CPU with a rule of its own, max(0, 1 - (t - 1/4) x s x 256) for colour, alpha
or both (0x004944f0, 0x004945c0, 0x00494650), with no upper bound, so a vertex
outside the volume's plane is brightened. fnTech3's GL path fades such stages
with the image's curve, and the Vulkan family with the image itself, on the
GPU. RTX's `r_fogMode 1`, an option off by default, computes Quake III's
analytic curve in its shaders.

## The whole campaign

**Observed.** One fnTech3 client process walked the campaign from `fakkhouse`
to `end` (`.tmp/fakk2-round/tools/campaign_walk.py`). At each level's first
snapshot it skipped the opening, took a frame and fired the level's exit as
the game does: Julie put inside the level's `trigger_changelevel`
(`script $player origin`), or the level script's own changelevel thread or
cinematic skip thread (`script thread <file>::<label>`). The module then
issued its own `map` or `gamemap`. 31 exits brought 32 level entries, through
every level the campaign visits:
`fakkhouse`, `training`, back to `fakkhouse`, `homes1`, `landersroost`, back
to `homes1`, `creeperpens`, `homes2good`, `towncenter_good`, `under`, `over`,
`shield`, `homes2evil`, `otto`, `towncenter_evil`, `cliff1`, `cliff2`,
`swamp1`, `swamp2`, `swamp3`, `gruff`, `gruff_cinema`, `cemetery`, `fog`, back
to `cemetery`, `water`, back, `blood`, back, `oracleway`, `oracle` and `end`.
Each entered at the spawn target the exit names. Before each change the
engine archived the persistent file through the module (63
`SV_ArchivePersistantFile` passes in all), and each new level archived itself
and copied that file into its autosave, 31 sets of four files; Julie's
costume changes as the scripts dress her. The log has no error. Every level's
script prints `Can't create thread. Label 'LevelComplete' not found` when its
exit fires, the retail module looking for an optional label. One
`Can't find maps/cliff2_precache.scr` is the 1.02 patch's file, which this
disc layout lacks.

**Saves.** Each of the 31 autosaves loaded in a fresh process, with no map
running, to its own level (`load_sweep.py`). Retail's list of them is under
Retail as the client below.

## Damaged demos

`scripts/probe_demo_faults.py` now takes the `fakk2` and `alice` profiles:
their `fakk` and `base` folders, `.dm3` and the 48 KiB message limit. Its tick
loop had spun without end under them. A Ritual `wait N` counts the frame's
milliseconds down before every line, so a frame longer than N -- the level
load, or the long first frame -- runs a `wait N; vstr` loop round again in the
same pass; retail's `Cbuf_Execute` does the same (0x0042c590), and `wait 1`
is always one frame there. The probe uses one-frame waits under the Ritual
profiles. fnTech3 is unchanged.

From fnTech3's recording of `fakkhouse` (188 messages), the seven fixed faults
and eight seeded body corruptions all end as expected: the three truncations
and one corruption complete; the three bad lengths (-2, 49,153 and
2,147,483,647), the flipped body and seven corruptions are rejected with an
engine error (`CL_ReadDeltaEntity: invalid or truncated entity delta`,
`Illegible server message`), the main loop still running. None crashed, hung
or stalled; the installation was unchanged.

Retail `fakk2.exe` played the same fifteen files (`retail_probes.py
faults`). Fourteen end as fnTech3's do: the three truncations and the same
corruption complete; the over-long lengths stop with `CL_ReadDemoMessage:
demoMsglen > MAX_MSGLEN`, and the flipped body and seven corruptions with
`CL_ParsePacketEntities: end of message` or `Illegible server message`. The
fifteenth, a record claiming -2 bytes, differs: retail reads it as a
truncation, prints `Demo file was truncated.` and ends the demo, where fnTech3
refuses the length with an error. Both stay running; fnTech3's refusal of a
negative length is its envelope reader's rule for every profile and is kept.

## Demos on every map

`.tmp/fakk2-round/tools/demo_exchange.py`, on the fold runtime (the fog image
changes nothing on the wire):

- **fnTech3 to retail.** fnTech3's client recorded each of the thirty maps on
  its own listen server, from the opening into play: 185 to 193 messages,
  19,144 to 130,486 bytes. Retail played every one with `timedemo 1` to its
  end marker and printed its timedemo summary (237 to 239 frames), with no
  parse error.
- **Retail to fnTech3.** Retail's client joined fnTech3's dedicated server on
  each map, reached the game and recorded with `g_syncronousClients 1`: 126
  messages on `gruff_cinema`, a cinematic, and 201 to 239 elsewhere.
  fnTech3 played every one to its end with no error (131 to 239 timedemo
  frames). Each playback prints `Not all children could find their parents`
  about once a message: retail's recorder leaves out entity 0's baseline
  (0x0042039e), so Julie and her weapon are missing on playback in both
  engines, as the contracts record found on `fakkhouse`.

## Retail as the client

All without the disc, retail's client only:

- **Save list.** With the walk's 31 autosaves laid into its runtime, retail's
  Load/Save menu lists them, newest first, with each level's title ("Starting
  End", "Starting Tomb of the We", "Starting The Bridge of Reason" and on),
  time and date, and the selected save's picture.
- **Pure server.** Retail joined fnTech3's dedicated server on `fakkhouse`
  with `sv_pure 1`, entered the game and answered `whatis 0` at the start
  point, with no complaint.
- **Under loss.** Retail's client played ten minutes on fnTech3's dedicated
  server while the server dropped a tenth of the packets it received
  (`net_dropsim 10`; retail has no such setting, so the loss falls on what
  reaches the server). All 120 of its `whatis 0` rounds, five seconds apart,
  were answered. Halfway, its `rcon script trigger $training_trigger` took
  the server from `fakkhouse` to `training`; the server resent the level's
  gamestate 20 times, as Quake III's rule resends it to a client still
  acknowledging the old one, until a whole copy came through, and the client
  entered the new level and played on. The session ended at the steps' end,
  with no drop, overflow or timeout on either side.
- **First views.** On the eight maps this round's two rules reach, retail's
  client and fnTech3's, each on fnTech3's server with both changes, shot their
  first views after the opening (`frames_cmp.py`). Their 3D views differ by
  1.3 to 6.8 levels of 255 on average: `homes2evil` 1.28, `cliff1` 2.96,
  `cemetery` 4.21, `under` 4.29, `otto` 5.03, `cliff2` 5.22 and `oracleway`
  6.83. `over` differs by 11.2: its canal is retail's lime green now, and its
  tunnel walls are lit brighter by fnTech3's static lights, as the gate
  matrix already records. `under` and `otto` were shot again with longer
  waits: retail's `under` was still loading at the first capture, and `otto`'s
  first pair caught the two cameras at different moments.

## Validation

A second `git archive` of `f4d07ef0` with only this round's code, test and
script files laid over it (`.tmp/fakk2-round/val`) built every target with
strict warnings. Its native tests passed, 281 with 2 skipped, the one that
timed out under the probes' load passing alone
(`fnql_platform_port_safety_source`, 16 s). The 55 test files that compile or
read the shader and image code, the two changed ones among them, passed
there; the export holds no laid-down F.A.K.K.2, so its script census ran in
the shared tree, where it passed in all three parsers. HEAD then moved to
`771325b7`, an Elite Force commit that touches none of this round's files; on
it, with this round's files, the changed tests, the flare parity test it added,
and the player manual and generated-docs tests passed.

## A stale ledger line

The client row said the RTX renderer's ray-traced mode had not activated for
this profile. Since 2026-10-22 it has run on `fakkhouse` with the entity sun
([shadow formats record](2026-10-22-fnq-shadow-formats.md)); the row now says
so.

## Not established

- **Retail as the server beyond the first level.** Retail's server needs the
  game CD; the disc image and the machine key were not used this round, so
  retail's server has still run only `fakkhouse`, and saves made in later
  levels have not been loaded by retail.
- **`creeperpens` with no player.** The retail module's fault there, once the
  only client has left a dedicated server, is still inferred **[I]** to happen
  on retail's own server; checking it needs the disc.
- **Listening.** Other levels' mixes, and the OpenAL path on any level, have
  not been listened to.
- **Platforms.** The retail modules are Windows x86 libraries; no other
  platform has run.
- **Fog volumes.** Retail's CPU fade of blended stages in a fog volume is not
  reproduced (above), and no frame inside one of the four maps' default fog
  volumes has been set beside retail's. Both were taken up on 2026-10-24: a
  view inside `cemetery`'s black fog matches retail's with fnTech3's static
  lights off, and the fade is recorded as an accepted difference
  ([playability round](2026-10-24-fakk2-playability-round.md#inside-the-fog-volumes)).
