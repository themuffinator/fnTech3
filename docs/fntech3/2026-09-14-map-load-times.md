# Quake Live map loading: 2026-09-14

`devmap mcsarges ffa` reaches an active snapshot faster with the loading
changes below. Vulkan reloads improve by 26.4% in the measured sample, and
GLx reloads by 23.3%. The first load in a fresh process improves less.

## Measured results

Times run from a console marker immediately before `devmap mcsarges ffa` to
an `activeAction` marker after the first active snapshot. They include server
map setup, client assets and the snapshot transition, but exclude application
startup before the map command.

| Renderer / load | Baseline median | Updated median | Reduction | Samples per version |
| --- | ---: | ---: | ---: | ---: |
| Vulkan, fresh process | 2.518 s | 2.184 s | 13.3% | 3 |
| Vulkan, reload | 2.190 s | 1.612 s | 26.4% | 6 |
| GLx, fresh process | 2.678 s | 2.567 s | 4.1% | 1 |
| GLx, reload | 2.062 s | 1.582 s | 23.3% | 2 |

Baseline and updated processes alternated, with three map loads per process:
24 QL loads total, all reaching active state and exiting normally. OS and
driver caches were not flushed; “fresh process” is not a cold-storage claim.
Desktop scheduling varies, and the GLx sample is small. These are combined
end-to-end results, not isolated speedups attributable to each individual edit.

The machine used Windows, an Intel Core i7-13700H and an NVIDIA GeForce RTX
4060 Laptop GPU. Both builds are MSVC 19.44 Windows x86 release builds. Tests
used the user's rendering, audio and cgame cvars, including subdivision 1 and
HDR/bloom, with sound initialized but muted and Steam/WebUI enabled. Every
process used an isolated writable home, windowed rendering, disabled mouse
and joystick input, and read-only lookup of the legitimate retail installation.

[Machine-readable evidence](2026-09-14-map-load-times.json) records every
measured command, executable/renderer/package SHA-256, map identity, individual
timing and summary. The baseline is commit
`c75573c7375e44c908256020ada2d7e12970cacf`. Concurrent game-manager work in the
shared checkout required building this change in an isolated source snapshot
of that commit plus the loading edits. This evidence does not certify those
concurrent changes. Both versions used the same project-authored root package.

## What changed

- ZIP absolute forward seeks continue the current inflate stream. They no
  longer reopen and decompress the same prefix; backward seeks retain reset
  behavior and the existing filesystem return contract.
- Complete Ogg sound loads read and validate the compressed payload once,
  then let Vorbis seek in memory. Music keeps its streaming path. Metadata,
  sample bounds and speaker ordering share the existing checks.
- The pak reader uses the existing maintained zlib dependency instead of its
  embedded 1998 decoder. PNG decoding also uses zlib and validates the zlib
  header, complete decoded length and Adler trailer. Meson, CMake and Make
  receive the dependency wiring; the inherited minizip attribution remains.
- CRC32 processes eight bytes per iteration with the same polynomial and
  portable byte assembly. Whole-file CRC admission remains enforced.
- Conservative patch-bounds rejection avoids comparing unrelated patch pairs
  during stitching and LoD synchronization in all three renderers. The exact
  vertex matching rules remain. Baseline and updated `mcsarges` logs both show
  10,048 faces, 730 meshes, 15 trisurfs and 268 stitched LoD cracks.
- libjpeg-turbo writes RGBA directly. A bounded decoded-JPEG LRU survives map
  reloads. Every request still reads through the active filesystem, validates
  the payload and compares the exact compressed bytes before reusing pixels.
  Changed files invalidate the cached result; pure admission is not bypassed.

`cl_jpegCacheMegs` defaults to **192 MiB**, accepts 0–256, and can be set to 0
to disable caching. Capacity changes apply on the next JPEG request.
`jpegcacheinfo` reports payload memory and hit/miss counts. This map retained
**135.9 MiB** of compressed bytes plus decoded pixels and reused **238 JPEG
requests per reload**. Entry/container overhead is additional and entry count
is bounded to 512. The cache trades process memory for faster repeat loads;
an undersized budget can evict this map's working set.

## Awaiting snapshot

`CA_PRIMED` now begins after renderer end-registration and the memory-touch
pass. Deferred asset work stays in the loading phase instead of being labelled
as waiting for a snapshot. `CL_InitCGame` measures that full phase, and a new
`CL_FirstSnapshot` log separates its duration from the remaining snapshot gap.

In the final QL samples that gap was **5–17 ms on reloads** and **15–140 ms on
initial joins**. The observed seconds of delay were predominantly asset and
map initialization. No snapshot protocol, user-command generation or remote
network behavior changed. These local measurements do not establish a fix for
delayed packets from an external server.

## Validation and reproduction

The same updated executable loaded retail QL native modules on `mcsarges ffa`
and retail Q3 QVMs on `q3dm1`, with pure admission and engine screenshots under
Vulkan and GLx. Six Q3 loads, including a baseline Vulkan comparison, exited
normally. Screenshots were inspected from the engine render target. Tests did
not inject keyboard or mouse input or validate player movement/combat.

Windows release client, dedicated server and GLx/Vulkan/RTX renderer builds
succeeded. The final full Meson suite reports **111 passed, 0 failed, 2 skipped**;
its Python source gate ran **1,303 tests**, with 11 skips and no failures.
An earlier unzip fixture failed because it omitted the frontend's required
first-entry selection; the corrected fixture passes in the final complete run.
Automated checks cover CRC oracle vectors and alignment, ZIP seek
work/data, memory-stream overflow and seek bounds, exact-byte JPEG cache
invalidation and eviction, conservative patch rejection, Ogg metadata/partial
decode cleanup, and actual zlib/minizip/PNG code including allocation failures
and malformed streams. The PNG and unzip tests also compile and pass with
Linux GCC `-m32` using the existing zlib 1.3.2 wrap sources. This task does not
claim a complete Linux client build, CMake/Make build, or RTX runtime pass.

The [reusable benchmark](../../scripts/benchmark_map_load.py) creates its own
home, runs repeated devmaps, captures engine screenshots and records timings
and hashes. For example, in PowerShell from the repository:

```powershell
python scripts/benchmark_map_load.py --exe .tmp/load-times/verified-bin/fntech3.exe --profile ql --basepath 'C:/Program Files (x86)/Steam/steamapps/common/Quake Live-' --map mcsarges --factory ffa --renderer vk --loads 3 --settings-from .tmp/vscode/ql --with-services --steam-provider meson/build/win32/fnql_steam.dll
```

The provider argument above names an existing local provider; it is not
included in the engine build. Supply the corresponding legitimate paths on
another machine. The default output is a new `.tmp/map-load-*` directory.
Local evidence is retained in `.tmp/load-times/evidence/`, with the verified
engine-only binaries in `.tmp/load-times/verified-bin/` and a patch against
the pinned baseline for the actual isolated build.

Automatic approval review rejected recursive cleanup with only “blocked by
policy” as its reason, including the narrower explicit build-directory attempt.
About 0.62 GiB of disposable builds and test homes therefore remains under
`.tmp/load-times/`, separate from `evidence/` and `verified-bin/`.

## Other observations and limits

Existing warnings appeared in both versions: Generic Software OpenAL lacks
OpenAL Soft features/HRTF, `Hunk_FreeTempMemory: not the final block`, and
`mcsarges` invalid collision-bevel and mixed image-flag messages. Q3 screenshots
also show excessive bloom/glare in both baseline and updated Vulkan output.
These observations remain separate issues; this change does not fix them.

The existing compatibility ledger stays partial. Broader renderer parity,
long-duration memory behavior, demos, retail bidirectional interoperability,
Steam authentication and the concurrent game-manager changes retain their
own required gates.
