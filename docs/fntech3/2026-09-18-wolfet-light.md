# Enemy Territory dynamic light

## Scope and authority

This work follows `db03f4e`. It carries ET's dynamic light, the last of its
scene imports whose arguments had nowhere to go, taking the cgame boundary to
126 of 147 imports and the UI boundary to 111 of 121. The ET client is still
gated and nothing here has been drawn.

Layout and behaviour facts come from the pinned original
[Enemy Territory](https://github.com/id-Software/Enemy-Territory/tree/40342a9e3690cb5b627a433d4d5cbf30e3c57698).
The reference is GPL-3.0-or-later with additional terms. No implementation text
is incorporated into fnTech3.

## A light with two magnitudes

Every other profile's dynamic light names one magnitude. ET's names two: a
**radius**, which is how far the light reaches, and an **intensity**, which is
how bright it is there. It then carries a shader handle and a flag word. The
previous records refused the import rather than picking one of the two
magnitudes and discarding the other.

Both now cross. The radius becomes the light's radius and the intensity scales
its colour, so a light ET made dimmer at the same reach arrives dimmer at the
same reach. A light with no radius or no brightness is discarded before anything
else is read, as ET's own does, and a non-finite radius, brightness, colour or
position is discarded rather than published to a scene.

## The three flags

ET's flag word has exactly three bits, recorded from its own `game/q_shared.h`:

| Bit | Flag | What this engine does |
| --- | --- | --- |
| 31 | `REF_FORCE_DLIGHT` | Honoured: the light is drawn even when dynamic lights are switched off, which is what ET's own renderer does. Its cgame uses this only for a thrown charge's glow. Until the [engine scene lights record](2026-09-24-wolfet-engine-lights.md) the renderer then cleared every light at `r_dynamiclight 0`, so the glow was not drawn after all. |
| 30 | `REF_JUNIOR_DLIGHT` | The light is meant to affect only the dynamic light grid and not surfaces. This renderer has no such path, so the light is skipped and the console says once that grid-only lights are not drawn. Retail ET's own modules never set this bit. The light still takes one of the frame's 32, as ET's does ([engine scene lights record](2026-09-24-wolfet-engine-lights.md)). |
| 29 | `REF_DIRECTED_DLIGHT` | The origin is a direction, not a position: a global directional source. This renderer has no such light, so it is skipped with a one-time notice rather than drawn as an ordinary point light at a position that is really a direction. ET's cgame sets it for a map `dlight` entity with angles, such as radar's lightning; the light still takes one of the frame's 32, as ET's does ([engine scene lights record](2026-09-24-wolfet-engine-lights.md)). |

A non-zero shader handle is likewise reported once and the light drawn without
it. Retail ET's own modules pass zero every time; a per-light material is a
renderer feature this engine does not have.

Nothing is silently dropped: each unsupported case says once, on the console,
that those lights are not drawn.

## Verification

Clean Windows x86 RelWithDebInfo build across all three renderers; no errors or
warnings. The complete automated suite passes.

The source regressions that hold ET's import coverage now count this import as
handled on both boundaries, and the boundary records no longer list it as open.

## Remaining gates

- **Nothing here is drawn.** The ET client stays gated, so no ET module has
  submitted a light. No visual claim is made.
- ET's global directional and grid-only lights, and its per-light shaders, are
  skipped; the console says so.
- The renderer still has no MDM/MDX model type, and ET's decals, polygon buffer,
  global fog, view parameters, 2D polygons, dynamic shaders and render to
  texture remain refused.
- Everything else in the [ET matrix](COMPATIBILITY.md#wolfet-gate-matrix)
  remains as recorded.

## Touched outside ET

Each of the three renderers gained one entry point and its registration. The
implementation is a single shared header, and it refuses any profile that is not
ET, so no other profile's lighting path changed.

## Unrelated observations

None new in this slice.
