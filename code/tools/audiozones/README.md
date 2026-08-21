# Audio Zone Compiler

`fnql-audiozonesc` compiles optional `maps/<map>.azb` sidecars for the OpenAL
environment system. The game ignores missing or invalid sidecars and keeps the
generic trace-based OpenAL environment heuristics.

Sidecars can be written by hand as `maps/<map>.audiozones`, or generated from an
existing Quake III `maps/<map>.bsp`. Generated zones are intended as a solid
first pass: they are derived from BSP leaves, clusters, areas, surfaces, brushes,
shader contents, and surface flags, then can be merged with small manual
overrides for places that need art-directed tuning.

Example:

```text
audiozones 1

zone "atrium" {
  bounds -512 -512 -64 512 512 384
  environment hall
  material stone
  flag outdoor
  reverbGain 1.10
  occlusionMultiplier 0.85
  lpfBias 0.95
  hpfBias 1.00
  transitionMs 900
  priority 10

  portal "hallway" {
    bounds 512 -128 -64 512 128 192
    openness 0.80
    blendDistance 128
    minBlend 0.03
    maxBlend 0.35
    curve ease-out
  }
}

zone "hallway" {
  bounds 512 -128 -64 896 128 192
  environment hallway
  material metal

  portal "atrium" {
    bounds 512 -128 -64 512 128 192
    openness 0.80
    blendDistance 128
    minBlend 0.03
    maxBlend 0.35
    curve ease-out
  }
}
```

Build and run:

```powershell
cmake --build .tmp/cmake-check --target fnql-audiozonesc --config Release
.tmp/cmake-check/fnql-audiozonesc.exe -o baseq3/maps/q3dm17.azb baseq3/maps/q3dm17.audiozones
.tmp/cmake-check/fnql-audiozonesc.exe --from-bsp -o baseq3/maps/q3dm17.azb baseq3/maps/q3dm17.bsp
.tmp/cmake-check/fnql-audiozonesc.exe --from-bsp --merge baseq3/maps/q3dm17.audiozones -o baseq3/maps/q3dm17.azb baseq3/maps/q3dm17.bsp
.tmp/cmake-check/fnql-audiozonesc.exe --from-bsp --material-map docs/audio-materials.txt -o baseq3/maps/q3dm17.azb baseq3/maps/q3dm17.bsp
.tmp/cmake-check/fnql-audiozonesc.exe --dump baseq3/maps/q3dm17.azb
.tmp/cmake-check/fnql-audiozonesc.exe --audit --samples 32768 baseq3/maps/q3dm17.azb
```

Supported environment names are `small-room`, `room`, `stone-room`, `hallway`,
`hall`, `outdoors`, and `underwater`. Supported material names are `unknown`,
`neutral`, `stone`, `metal`, `liquid`, `sky`, and `soft`. Bounds are
axis-aligned boxes in Quake world units. Higher `priority` wins when zones
overlap; equal priorities prefer the smaller box.

Version 2 metadata can be authored directly:

- `material <name>` stores the acoustic material class.
- `flag outdoor` and `flag underwater` set runtime environment flags. `outdoor
  true` and `underwater true` are equivalent.
- `portal "<target zone>" { bounds ... openness 0.0..1.0 }` adds an explicit
  cross-zone transition hint. `targetZone <index>` is also accepted for generated
  tooling, but named targets are preferred for hand-authored files. Reciprocal
  portals are recommended so `--audit --strict` can be used cleanly.
- Portal tuning is optional: `blendDistance` sets the listener distance in Quake
  units, `minBlend` sets the threshold below which the portal is ignored,
  `maxBlend` caps the crossfade, and `curve` accepts `smooth`, `linear`,
  `ease-in`, or `ease-out`.

Generated BSP zones use negative priorities, so normal hand-authored zones with
the default priority `0` override them naturally. Merged hand-authored zones keep
their material, outdoor/underwater flags, and explicit portals; only the internal
generated flag is stripped from overrides. The compiler writes version 3 sidecars
with material classes, portal hints, and per-portal blend tuning between adjacent
generated volumes. The runtime still accepts version 1 and version 2 sidecars;
with version 2+ files it uses the generated outdoor/underwater flags and applies
a bounded crossfade toward adjacent zone environments when the listener is near a
portal hint. Version 2 portals inherit the default 192-unit smooth blend,
0.02 minimum threshold, and 0.45 maximum crossfade.

For generated BSP zones, `--material-map <path>` lets maintainers override weak
shader-name heuristics without editing the map. Each non-comment line is:

```text
shader/pattern material [preset name] [flag outdoor] [weight N]
```

Patterns are case-insensitive path substrings unless they contain `*` or `?`, in
which case they are matched as simple wildcards. Materials use the same names as
hand-authored zones; optional `preset`, `flag`, and `weight` fields make a rule
more authoritative. For example:

```text
textures/custom/pipe_* metal preset hallway weight 8
textures/custom/canopy sky preset outdoors flag outdoor
textures/custom/slosh liquid preset underwater flag underwater weight 12
```

## How BSP generation works

Generation starts from one candidate zone per non-opaque BSP leaf, clipped to the
world model bounds, and then merges leaves back into room-scale volumes.

Merging is driven by a region adjacency graph rather than by comparing bounding
boxes. Two leaves are connected when their boxes actually touch, and the graph
records the accumulated open contact area between them. This is what separates
"one room a BSP plane happened to cut in half" from "two rooms sharing a wall
with a door in it": the two cases look identical as bounding boxes, but the BSP
cut shares a full cross-section while the door shares a fraction of it. A merge
also has to keep the combined box a tight fit, both against the two boxes going
into it and against the leaf volume it actually contains — the second test is
what stops a long chain of individually cheap merges from growing one zone to the
size of the map.

A quality schedule of progressively looser passes always runs, because BSP leaves
are fragments of rooms and the environment preset is only meaningful once a zone
has room-scale bounds. A coarsen schedule runs after it, and only while the zone
count is still above `--max-zones`.

Classification then follows from the merged volume:

- The environment preset comes from size and shape. The acoustic material only
  chooses between `room` and `stone-room` for a room-sized volume; it does not
  override the size test.
- Material votes are weighted by shader reference type — visible draw surfaces
  outrank brush bodies, and brush sides count only as weak supporting evidence,
  since a single leaf routinely references several times more brush sides than
  draw surfaces.
- `outdoor` comes from the share of a zone's draw-surface evidence that is sky.
  Very large volumes need proportionally less of it, so the void of a space map
  reads as open air rather than as a reverberant hall.
- `underwater` requires the volume to be inside a liquid brush, tested against
  the brush planes. Leaf brush lists also name brushes that merely touch the
  leaf, so a containment test is what keeps a dry room beside a pool out of the
  underwater preset.
- Generated priorities are assigned by ascending zone volume, so the tightest
  zone containing the listener always wins and no two generated zones ever tie.

Portal hints come from the same adjacency graph, so a hint exists only where two
zones are genuinely connected, its quad covers the real opening, and its
`openness` is the share of the smaller zone's face that is actually open. Blend
tuning is derived from that geometry: `blendDistance` scales with the size of the
opening, `maxBlend` scales with openness, and the curve is `ease-out` for wide
openings, `ease-in` for narrow ones, and `smooth` in between.

Use `--audit` on generated sidecars before listening passes. It runs the same
runtime parser used by the client, prints preset/material/flag/portal coverage,
reports suspicious overlap or portal patterns, summarizes portal tuning, and
performs a deterministic zone lookup/portal-blend profile across the sidecar
bounds. The audit also emits material, portal, lookup, overlap, overall
confidence, an anomaly score, and a grade so generated maps can be triaged before
listening. `--samples N` controls the profile grid size; `--strict` returns a
non-zero exit code when warnings are emitted, which is useful for CI experiments
or large-map sweeps.

The `scale` line reports zone size relative to the sidecar bounds and counts
zones that cover more than half of the map. Lookup confidence is the share of
resolved samples that land in a zone of sane scale rather than in a map-sized
box, because raw coverage rewards the wrong thing — a single zone spanning the
whole level scores 100% coverage. A map whose largest zone spans most of the
bounds is worth inspecting, though it is legitimate for space maps and other
single-volume arenas, where the open void really is one enormous space.

## Bulk migration sweeps

Large map sets can be migrated with `scripts/audio_zone_sweep.py`. The script
discovers `.bsp` files, generates matching `.azb` sidecars, merges any
corresponding `.audiozones` overrides, audits every result, and writes both JSON
and CSV reports with warning, confidence, and anomaly fields for CI artifacts or
listening-triage notes.

```powershell
python scripts/audio_zone_sweep.py `
  --tool .tmp/cmake-check/Release/fnql-audiozonesc.exe `
  --relative-root baseq3 `
  --override-root baseq3 `
  --material-map docs/audio-materials.txt `
  --output-root .tmp/audio-zone-sweeps/baseq3 `
  --strict `
  baseq3/maps
```

Use `--dry-run` to review the planned compiler and audit commands without
touching generated sidecars. `--samples N` controls the per-map audit grid, and
`--max-zones N` is forwarded to BSP generation for conservative fallback passes
on unusually fragmented legacy maps. The default reports are
`audio-zone-sweep.json` and `audio-zone-sweep.csv` under the output root.

## Standard Q3A sidecars

FnQL ships generated `.azb` sidecars for the standard Quake III Arena
`baseq3` arena maps. The tracked package source sidecars live under
`pkg/baseq3/maps/`, and release/install builds pack them into
`FnQL-pkg.fnz` under `baseq3/maps/` archive paths. The same package source
tree also carries other data-only OpenAL tuning files, such as the standard
weapon sound shader under `pkg/baseq3/sound/`.
Regenerate the sidecars from a local retail `baseq3` install with:

```powershell
python scripts/generate_standard_audio_zones.py `
  --tool meson/build/fnql-audiozonesc.exe `
  "C:/Program Files (x86)/Steam/steamapps/common/Quake 3 Arena/baseq3"
```

The helper reads only official `pak0.pk3` through `pak8.pk3` style archives,
derives the map set from shipped arena metadata, extracts BSPs into `.tmp/`,
and writes the compiled sidecars back to `pkg/baseq3/maps/`. Its default
`--max-zones 512` keeps generated lookup cost bounded while preserving detailed
coverage for maps that stay below the cap.
