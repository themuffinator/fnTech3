# Engine asset package

`fntech3-pkg.fnz` is one deterministic ZIP archive beside the shared engine
executables. Its source tree is `pkg/`; paths inside the archive mirror that
tree exactly. Use `games/<profile>/<gamedir>/<qpath>`:

```text
fntech3-pkg.fnz
└── games/
    ├── quake3/
    │   ├── baseq3/{maps,sound}/...
    │   ├── baseq3/fnq3-hud.json
    │   └── missionpack/{maps,sound}/...
    ├── quakelive/
    │   └── baseq3/
    │       ├── maps/*.azb
    │       ├── maps/*.fog
    │       ├── scripts/fnql.shader
    │       └── sound/fnql-weapon-sounds.sndshd
    ├── rtcw-sp/main/{maps/*.azb,fntech3-hud.json}
    └── rtcw-mp/main/{maps/*.azb,fntech3-hud.json}
```

`<profile>` is the canonical immutable game profile key: `quake3`,
`quakelive`, `rtcw-sp`, or `rtcw-mp`. Startup aliases such as `q3` and `ql`
are not package directory names. RTCW SP and MP have separate namespaces
even though both installations use `main`. They ship audio zones for all
32 official SP maps (including cinematics and the ending) and 15 official MP maps.

The profile is the compatibility boundary; `<gamedir>` retains the retail
base/mod distinction. For example, CPMA tuning belongs in
`games/quake3/cpma/sound/fnql-weapon-sounds.sndshd`. Grouping only by
`baseq3` would mix Q3 and QL, and grouping only by title would mix RTCW modes.
One archive keeps packaging and installation simple for the shared binaries.
Retail installations, mod pak files, and writable home paths keep their
existing layout; this namespace applies only to engine-owned package assets.

## Lookup contract

1. Select the game profile before filesystem startup. Without an active
   profile, the package contributes no files.
2. Search `fntech3-pkg.fnz` beside the executable, then the existing platform
   working/base/app path fallbacks. Within each archive, follow the active
   filesystem's mod/base search order inside `games/<profile>/`.
3. Read only engine-owned sidecars: `maps/*.azb`, `maps/*.fog`,
   `scripts/fnql.shader`, and `sound/fnql-weapon-sounds.sndshd`. Q3/RTCW additionally
   permit the exact `fntech3-hud.json` and legacy `fnq3-hud.json` filenames;
   Quake Live excludes these HUD presets. The shader is
   also exposed to the renderer's exact `scripts/*.shader` listing.
4. A missing or unreadable entry returns to ordinary filesystem/pak lookup.
   There is no search through other profiles or unscoped archive entries.

The package is not mounted as a general pak. It cannot supply modules,
configs, BSPs, retail samples, or pure/download identities. Internal virtual
filenames containing `fnql` remain stable for their existing consumers.
ZIP payload CRC checks, bounded reads, and temporary-memory ownership are
unchanged. Candidate names reject traversal, separators in profile/mod
components, control characters, drive/stream syntax, and truncation.

## Authoring and validation

Place owned assets under `pkg/games/<profile>/<gamedir>/`. The builder includes
the whole tree and rejects unscoped paths, unknown profile keys, unsafe paths,
case-insensitive duplicates, escaping sources, and overlong member names.
It uses fixed ZIP timestamps and permissions for reproducible output. Required
shipped assets are validated before replacing an existing package.

The 149 tracked QL `.azb`/`.fog` pairs and console shader belong exclusively
to `quakelive`. The complete reviewed FnQuake3 package contributes 92 Q3
assets: 35 audio-zone files, 54 fog presets (34 base Q3, 20 Team Arena), two
sound shaders, and the legacy HUD preset. All are under `games/quake3/`,
with Team Arena in `missionpack/`. Sound shader filenames map to the existing
`fnql-weapon-sounds.sndshd` consumer path; authored contents and provenance
comments are retained, with LF text normalization for reproducible packaging.
The HUD preset is consumed by Q3's restored `cl_hudAspect` rules system.
The preferred custom filename is `fntech3-hud.json`, with `fnq3-hud.json` as
the fallback. Both remain isolated to the selected profile; see
[aspect correction](../ASPECT_CORRECTION.md).
The [reference inventory](2026-09-17-fnq3-package-assets.json) records source
identity, destination paths, and every asset hash. There is no implicit
shared namespace: sharing requires an intentional entry for each profile.

The 47 RTCW audio sidecars and two separate SP/MP HUD presets bring the
required package inventory to 441 assets. The HUD presets use the preferred
`fntech3-hud.json` filename and stock module layouts; they are independently
authored from retail draw metadata and layout observations.
Their [generation record](2026-09-17-rtcw-audio-zones.md) records retail input
hashes, patched-map precedence, audit warnings, and generated output hashes.
These are generated acoustic estimates; audible tuning remains a separate check.

When adding shipped assets, update `scripts/root_archive.py`'s profile
inventories and the Meson package input list as appropriate. The Q3 inventory
in `scripts/stock_q3_assets.py` drives both required assets and Meson inputs.
`scripts/stock_rtcw_maps.py` does the same for both RTCW modes.
The Meson target
rebuilds the complete package, including additional locally generated assets.
A new profile also needs its canonical key in the packaging validator; this
does not enable or claim that game's runtime compatibility.

```powershell
python scripts/build_root_archive.py --output .tmp/fntech3-pkg.fnz
python scripts/verify_release_layout.py .tmp/fntech3-pkg.fnz
```

`generate_stock_ql_sidecars.py` writes to `pkg/games/quakelive/baseq3/`;
`generate_standard_audio_zones.py` writes to `pkg/games/quake3/baseq3/`.
`generate_stock_rtcw_audio_zones.py` writes to `pkg/games/rtcw-sp/main/` and
`pkg/games/rtcw-mp/main/` after generating and auditing the entire official set.
All use user-supplied legitimate installations and package only derived
sidecars, never retail BSPs or proprietary modules.

## Upgrade

Upgrade the engine and `fntech3-pkg.fnz` together. Legacy `FnQL-pkg.fnz` and
`FnQuake3-pkg.fnz` files and their `baseq3/...`/unscoped layouts are not read. Renaming an
old archive is insufficient: rebuild from the migrated `pkg/` tree. For custom
entries, establish which game/mode they were authored for and place them in
that profile's namespace before rebuilding. The old file can be removed after
upgrading. Dated compatibility records retain the archive name and hashes
actually tested at the time.
