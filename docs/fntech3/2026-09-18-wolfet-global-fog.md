# Enemy Territory's global fog

## Scope and authority

This work follows `e1fc035`. It closes the cgame import that retunes ET's global
fog, and the reason that import was open: the fog volume it names could not be
loaded at all.

Behaviour facts come from the pinned original
[Enemy Territory](https://github.com/id-Software/Enemy-Territory/tree/40342a9e3690cb5b627a433d4d5cbf30e3c57698),
chiefly its `renderer/tr_bsp.c`, `renderer/tr_cmds.c` and `renderer/tr_main.c`.
The reference is GPL-3.0-or-later with additional terms. No implementation text
is incorporated into fnTech3.

## A fog volume with no brush

A fog volume normally names the brush whose six axial sides give it its bounds.
ET's compiler writes one volume per map with a brush number of -1 instead: it
has no brush and covers the whole world, and it is the one a module may retune.

The Quake lineage's fog loader reads that number as a brush index, finds it out
of range, and drops the client with `fog brushNumber out of range`. So the
loader here refused the map before any import could be asked about its fog.

**This is not a hypothetical.** Of the six maps Enemy Territory ships, four
carry a fog volume, and *every fog volume Enemy Territory ships is a brushless
one*: `battery`, `oasis`, `radar` and `railgun`. Every one of them would have
refused to load. The other two ship no fog at all.

A brushless volume now takes the whole world for its bounds and skips the brush
lookup, as ET's loader does. The refusal is kept for a brush number that is
genuinely out of range rather than simply absent, so a corrupt map is still
refused. This is a property of the compiler's own convention rather than of one
profile, so it is not gated on ET; a map from any profile that carries such a
volume would previously have been refused outright.

## Retuning it

A module asks for a colour and a depth for opaque, either at once or faded over
a duration, or asks for the map's own fog back. The arithmetic is kept apart
from the renderer so it can be checked on its own: what a request becomes, and
what a running fade shows at a given moment.

Three details are ET's:

- A depth for opaque never goes below one. The texture coordinate scale is its
  reciprocal, so a zero would have no meaning.
- A restore ignores the colour it is given and heads back to what the map
  authored, which is captured when the volume is loaded.
- A fade begins at whatever the fog is showing, not at the map's value, so a
  request that arrives during another fade continues from where that one had
  reached rather than jumping.

A request is refused, rather than approximated, when the map has no such volume,
when a value is not finite, or when the duration would put the fade's end past
the counter it is timed against.

The retuned values are written into the fog volume, not into its shader. ET
writes the shader's, which it can do because its volume reads its parameters
back from the shader every frame; here a shader may be shared between volumes,
and the request names one volume.

## Verification

Clean Windows x86 RelWithDebInfo build across all three renderers; no errors or
warnings. The complete automated suite passes.

A new probe, `scripts/probe_wolfet_global_fog.py`, reads every BSP in an
installation and reports its fog volumes and how many have no brush. The
installation is only read from and is checked unchanged afterwards. On the
retail installation: six maps, four fog volumes, four of them brushless, in four
different maps.

A new executable regression drives the arithmetic: a map with no such volume
refusing both a request and a fade; an immediate request showing at once; a
depth below one being lifted to one; a restore returning the authored values; a
fade showing no jump on the frame it starts, reaching the halfway value halfway
through, landing on the target exactly and then stopping; a time already past
the end landing on the target; a fade back to the authored values starting from
wherever the fog was; a null or non-finite colour, a non-finite depth, a
non-finite current value and a duration that would overrun the counter each
refused, with no fade left behind.

Source regressions check that all three renderers load a brushless volume as
world-covering while keeping the out-of-range refusal, that each carries the
state, moves a running fade on each frame and publishes the entry, that the
request is refused outside its own profile, that the arithmetic names no profile
and reaches for no renderer state, and that the import is answered.

ET cgame import coverage moves from 143 to **144 of 147**.

## Remaining gates

- **No image has been produced.** Nothing has been fogged on screen, because the
  ET client is still gated; no visual claim is made.
- **Fog density is not claimed to match retail.** ET scales fog texture
  coordinates by the reciprocal of the depth for opaque where the Quake lineage
  this engine follows divides that by a further eight, and ET's own three sites
  do not agree with each other. The values written here are consistent with the
  fog the map itself loaded with, which is what keeps a retuned fog and an
  authored one shaded alike; absolute parity with retail needs a client.
- No installed map has been loaded through this renderer, so the brushless
  volume is asset evidence and source evidence, not a render.
- Everything else in the [ET matrix](COMPATIBILITY.md#wolfet-gate-matrix)
  remains as recorded.

## Touched outside ET

The fog loader's brushless case is shared by all profiles, as described above;
no profile that previously loaded a map loads it differently. The refresh export
gains one entry. Nothing else outside ET's own files is altered.

## Unrelated observations

ET sets the fog texture coordinate scale at three places and they disagree: its
shader parser and its immediate global-fog path use the reciprocal of the depth
for opaque, while its global-fog fade divides that by a further eight. Whichever
was intended, one of the two cannot be.
