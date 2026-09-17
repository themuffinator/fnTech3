# Official RTCW audio zones

The engine package now contains 47 generated `.azb` sidecars: 32 for RTCW SP
under `games/rtcw-sp/main/maps/`, and 15 for RTCW MP under
`games/rtcw-mp/main/maps/`. SP coverage includes every installed official BSP,
including six cutscenes and the ending. MP coverage includes the eight base
maps and all seven official `mp_pakmaps0` through `mp_pakmaps6` maps.

The [machine-readable record](2026-09-17-rtcw-audio-zones.json) pins the
installation, source commit/dirty-tree status, compiler/source hashes, selected
pak identities, per-map input/output hashes, audit settings, and results.
Only generated acoustic metadata is shipped; no BSPs, sound samples, or retail
modules are incorporated.

## Generation and material interpretation

`scripts/stock_rtcw_maps.py` supplies the same canonical inventory to the
generator, required-package validator, and Meson dependencies. The generator
reads official packages only, rejects incomplete or changed inventories and
ambiguous/unsafe BSP names, and applies later official patches. `crypt1`,
`dam`, and `end` use their `sp_pak4.pk3` versions instead of `pak0.pk3`.
Custom packages do not override the selected inputs.

The existing IBSP 47 record reader matches the RTCW layouts frozen in
`code/compat/rtcw_bsp_layout.def`. However, BSP version 47 alone cannot select
the material interpretation because Quake Live uses the same version.
The compiler now accepts an explicit `--profile rtcw-sp` or `rtcw-mp`:

- RTCW ceramic (`0x40`) contributes a stone-material vote. Q3/QL retain their
  flesh/soft interpretation of that bit.
- RTCW glass, roof, and rubble contribute stone-material votes; wood, grass,
  gravel, snow, and carpet contribute soft-material votes. Metal, sky, and
  liquid detection continue to use the shared meanings.
- Without an explicit profile, the compiler retains its previous Q3/QL
  behavior. Explicit profile/version contradictions fail before writing output.

The flag values were observed in original RTCW SP/MP `surfaceflags.h` and
cross-checked with iortcw. These are file-contract facts; the acoustic grouping
is an independently authored heuristic. Original RTCW headers carry GPLv3
terms; their implementation text was not copied into this GPLv2 project.
Attribution is recorded in [CREDITS](../../CREDITS.md).

Reproduce using a legitimate RTCW installation and a compiler built from this
change:

```powershell
python scripts/generate_stock_rtcw_audio_zones.py "E:/SteamLibrary/steamapps/common/Return to Castle Wolfenstein" --tool .tmp/fnz-layout/build/fnql-audiozonesc.exe
python scripts/build_root_archive.py --output .tmp/rtcw-audio-zones/fntech3-pkg.fnz
python scripts/verify_release_layout.py .tmp/rtcw-audio-zones/fntech3-pkg.fnz
```

The default is a maximum of 512 zones and 32,768 audit samples per map.
Every map is generated and audited before publication; an audit or generation
failure preserves the existing published set. Successful publication replaces
individual files atomically and retains custom sidecars. Temporary extracted
BSPs and the staging directory are removed, including on failure. The retail
installation is read-only throughout.

## Results and limits

- Windows x86 compiler build passed.
- All 47 files pass the shared runtime binary parser and non-strict lookup
  audit: 15,451 zones, 3,439,241 bytes, and 1,540,096 sampled lookups.
- Two independent generations produced byte-identical sidecars for all maps.
- The complete archive validates with 439 assets, including 231 audio zones,
  203 fog presets, and the existing Q3/QL shader/preset files.
- Focused tests cover profile-specific surface meanings, previous Q3/QL
  behavior, version mismatch, official patch precedence, unsafe and missing
  inputs, failure preservation, custom sidecar retention, and temporary-input
  cleanup. Packaging tests also enforce both RTCW namespaces and the complete
  source/output hash inventory.
- The focused Python suites pass: 74 tests, with one Windows symlink-creation
  check skipped because the process lacks the required privilege. The three
  Meson checks `fnql_audio_zones`, `fnql_audio_zone_authoring_audit`, and
  `fnql_audio_zone_material_map` pass. Meson reconfiguration accepts all 47
  new package inputs. This is targeted validation, not a complete compatibility
  suite or full client rebuild.

There are **28 heuristic warnings across 24 maps**: 23 maps have mostly
map-scale sampled lookups, four have large volumes relative to sidecar bounds,
and one has low sampled coverage. These categories overlap. The audit samples
the sidecar bounding volume rather than a verified playable route, so these
warnings require map-specific review, not automatic suppression. Exact map
warnings and confidence scores are retained in the JSON record. `--strict`
would reject this set because of those warnings.

No listening session or live OpenAL parity test was performed. These files are
generated estimates, not hand-tuned acoustics, and do not close RTCW audio or
gameplay compatibility gates. No unrelated engine issue was discovered in this
task.

Local generation/audit and test logs are retained under
`.tmp/rtcw-audio-zones/`.
