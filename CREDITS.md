# Credits

## Project lineage

fnTech3 combines two GPL id Tech 3 engine lines into one profile-driven
codebase:

- [FnQuake3](https://github.com/themuffinator/FnQ3) supplies the Q3-oriented
  modernization lineage. The merger reference is pinned at
  [`7d8c9629cdc7979f09697266470359101ffd9eda`](https://github.com/themuffinator/FnQ3/commit/7d8c9629cdc7979f09697266470359101ffd9eda).
- [FnQL](https://github.com/themuffinator/FnQL) supplies the QL-oriented engine
  compatibility baseline. The imported reference is pinned at
  [`fce44fe714564621e6ceb79e3da7c53089646af2`](https://github.com/themuffinator/FnQL/commit/fce44fe714564621e6ceb79e3da7c53089646af2).

The FnQuake3 worktree had uncommitted renderer, documentation, build, and test
changes when the merger began. fnTech3 records only the pinned committed
baseline lineage; that dirty work was not blindly imported. Individual ideas
or changes may be ported later after review, attribution, and profile-specific
regression coverage.

The lineage above continues through:

- id Software's [Quake III Arena source
  release](https://github.com/id-Software/Quake-III-Arena), the original GPL
  engine foundation.
- [ioquake3](https://github.com/ioquake/ioq3), the community-maintained Quake
  III continuation.
- [Quake3e](https://github.com/ec-/Quake3e), the modern engine foundation used
  by FnQuake3.

These engine sources use GPL-2.0-or-later headers and are compatible with the
repository's GPL license. Their original file headers and provenance comments
remain authoritative.

## Compatibility references and adapted work

- The WebUI request notifications, bounded bulk request transport, avatar retry
  queue, and associated tests/probe are adapted from the GPL-2.0-or-later
  [FnQL WebUI work](https://github.com/themuffinator/FnQL). These were reviewed
  uncommitted updates on 2026-09-06, after the pinned merger baseline, and are
  incorporated implementation rather than behavioral reference only. The
  [port record](docs/fntech3/2026-09-06-webui-request-port.md) pins the source
  snapshot, preserves the Q3/QL profile boundary, and records fnTech3 validation.
- The Q3 viewport-FOV policy is independently implemented from the pinned
  [FnQuake3 renderer behavior](https://github.com/themuffinator/FnQ3/blob/7d8c9629cdc7979f09697266470359101ffd9eda/code/renderer/tr_main.c).
  It applies only to the Q3 profile; QL retains its module-authored projection.
- The Q3 cgame extension ordinals `112..116` and discovery names were
  cross-checked against the pinned GPL-2.0-or-later FnQuake3
  [`cg_public.h`](https://github.com/themuffinator/FnQ3/blob/7d8c9629cdc7979f09697266470359101ffd9eda/code/cgame/cg_public.h)
  and [`cl_cgame.cpp`](https://github.com/themuffinator/FnQ3/blob/7d8c9629cdc7979f09697266470359101ffd9eda/code/client/cl_cgame.cpp).
  fnTech3's profile remapping is independently written; the header records the
  source of these boundary constants.
- [Quake Live Source Reconstruction Project
  (QLSRP)](https://github.com/themuffinator/QL-SRP) provides behavioral, ABI,
  protocol, and format evidence for independently written QL engine work; its
  implementation text is not treated as an import.
- [CPMADevs CNQ3](https://bitbucket.org/CPMADevs/cnq3) is credited for the
  rainbow text-color escape lineage.
- [Spearmint](https://github.com/zturtleman/spearmint) is credited for the
  `bsp_q3ihv.c`-based Quake 3 IHV / `IBSP v43` map-loading reference.
- [WolfcamQL](https://github.com/brugal/wolfcamql) is used to cross-check legacy
  demos and QL-adjacent compatibility behavior.
- Luigi Auriemma's [QuakeLive beta files decoder
  (`quakelivedec`/qldec)](https://aluigi.altervista.org/papers.htm#qldec) is the
  GPL-2.0-or-later source of the published beta `.pk3` XOR table retained in
  `code/qcommon/quakelive_pk3_xor.h`. See the [third-party
  notice](THIRD_PARTY_NOTICES.md).
- [DarkMatter-Q2](https://github.com/Paril/DarkMatter-Q2) is used to cross-check
  Quake II `.pak` and `.wal` support.
- Mikko Mononen's [FontStash](https://github.com/memononen/fontstash) and Sean
  Barrett's `stb_truetype` are credited for the pinned historical rasterizer
  used by the host-text path; their source headers carry the applicable terms.

## RTX rendering references

The RTX renderer's independently written scene-transport work uses
[NVIDIA Q2RTX](https://github.com/NVIDIA/Q2RTX/tree/f2526e9a165949f66e91e82f0d63aa7bb2567b4d)
as a feature and behavior reference. No Q2RTX implementation text or assets
were incorporated in this round. Standard scattering and sampling equations
were checked against *Physically Based Rendering*'s
[reflection sampling](https://www.pbr-book.org/3ed-2018/Light_Transport_I_Surface_Reflection/Sampling_Reflection_Functions)
and [dielectric reflection/transmission](https://www.pbr-book.org/4ed/Reflection_Models/Specular_Reflection_and_Transmission)
chapters; the GLSL implementation and synthetic tests are independently written.

## Retail-content boundary

fnTech3 does not redistribute Quake III Arena or Quake Live data, retail game
modules, Steam SDK/runtime binaries, Awesomium, credentials, captured private
traffic, or other proprietary content. Runtime validation uses the user's
legitimate local installations and records only non-proprietary evidence.

## Project thanks

Thanks to the Quake, Quake III, and Quake Live communities: players, server
administrators, mod authors, mappers, tool authors, testers, reverse engineers,
and maintainers who keep these games understandable and playable.

## Trademark note

fnTech3 is independent and is not affiliated with, endorsed by, or sponsored by
id Software, Bethesda, ZeniMax Media, or Valve. Quake, Quake III Arena, and
Quake Live are trademarks of their respective owners.
