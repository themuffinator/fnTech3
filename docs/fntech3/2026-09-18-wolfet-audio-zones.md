# Official Enemy Territory audio zones

The engine package now contains six generated `.azb` sidecars, one for each
official Enemy Territory map -- battery, fueldump, goldrush, oasis, radar and
railgun -- under `games/wolfet/etmain/maps/`. Together they hold 2,155 zones
in 490,816 bytes.

The [machine-readable record](2026-09-18-wolfet-audio-zones.json) pins the
installation, the source commit and dirty-tree status, compiler and source
hashes, the official pak's identity, per-map input and output hashes, audit
settings and results. Only generated acoustic metadata is shipped; no BSPs,
sound samples or retail modules are incorporated.

## Generation and material interpretation

`scripts/stock_wolfet_maps.py` supplies the canonical inventory to the
generator, the root archive's required assets and Meson's dependencies.
`scripts/generate_stock_wolfet_audio_zones.py` reads ET's official base paks
only, rejects an incomplete or changed inventory and ambiguous or unsafe BSP
names, extracts each map into a fresh working directory under `.tmp`, compiles
and audits it, and publishes only after every map has passed. Custom packages
never override the selected inputs, and the retail installation is only read.

ET's maps are IBSP 47, as Quake Live's and RTCW's are, so the version cannot
select how surface flags are read. The zone compiler now accepts `--profile
wolfet`:

- ET keeps RTCW's material bits: glass, roof and rubble vote stone; wood,
  grass, gravel, snow and carpet vote soft. Metal, sky and liquid detection
  keep their shared meanings.
- ET gives `0x40` to a water splash, where RTCW has ceramic and Quake 3 and
  Quake Live flesh. It votes liquid, which a dry zone never takes as its own
  material, so it is not counted as stone.
- ET's `0x80000000` marks ground a landmine can be planted in, which says
  nothing acoustic and is not counted.

The flag values come from ET's own `surfaceflags.h`; the acoustic grouping is
this engine's own heuristic.

Reproduce with a legitimate installation and a compiler built from this
change:

```powershell
python scripts/generate_stock_wolfet_audio_zones.py "E:/SteamLibrary/steamapps/common/Wolfenstein Enemy Territory" --tool meson/build/win32-dev/fnql-audiozonesc.exe
python scripts/build_root_archive.py --output .tmp/wolfet-audio-zones/fntech3-pkg.fnz
python scripts/verify_release_layout.py .tmp/wolfet-audio-zones/fntech3-pkg.fnz
```

The defaults are at most 512 zones and 32,768 audit samples per map. As for
RTCW, the audit runs without `--strict`: its heuristic warnings are recorded
and published rather than treated as failures.

## Results and limits

All six maps generated and audited with grade "good". Goldrush and oasis reach
the 512-zone ceiling. Every map carries the heuristic warning that most lookups
resolve to map-scale zones; battery and railgun also carry the warning that
zones cover more than half of the sidecar bounds. A second full run produced
six byte-identical sidecars.

At runtime, every map's zones load in the developer client, reported by
`s_info` with the zone count, the active zone and its material; see the [map
load record](2026-09-18-wolfet-map-load.md). No listening test has been
performed, and no claim is made that these zones match any authored intent.
