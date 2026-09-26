# fnTech3 logos

Executable and desktop icon exports are in [assets/icons](../icons/README.md).
The flame emblem's generation prompts are in
[sources/fntech3-flame-logo-prompts.md](sources/fntech3-flame-logo-prompts.md),
beside the PNG they produced.

The approved mark is the first generated version, with a black numeral **3** on the orange flame body. Its original PNG is preserved unchanged.

| Asset | Size | Use |
| --- | --- | --- |
| [fntech3-logo.png](fntech3-logo.png) | 980 × 1605, transparent PNG | Original vertical emblem |
| [fntech3-readme-banner.png](fntech3-readme-banner.png) | 2172 × 724, transparent PNG | Horizontal README banner with subtitle on the expanded orange flame base |
| [fntech3-social-banner.jpg](fntech3-social-banner.jpg) | 1600 × 900, opaque JPEG | 16:9 social preview with a soft-focused fiery backdrop, subtitle and centered game icons |
| [fntech3-game-silhouettes.png](fntech3-game-silhouettes.png) | 2229 × 292, transparent PNG | Ten grey game silhouettes in a single row below the README banner |

Both banners include the subtitle **Unified source port for id Tech 3**. The original vertical emblem has no subtitle.

The social banner has a newly generated row of ten icons in their original
colours. The README places a separate grey silhouette strip beneath its
horizontal flame banner. Both rows show one icon per title, ordered by original
PC release: **Quake III Arena, Heavy Metal: F.A.K.K.2, Star Trek Voyager:
Elite Force, American McGee's Alice, Return to Castle Wolfenstein, Medal of
Honor: Allied Assault, Jedi Outcast, Wolfenstein: Enemy Territory, Jedi Academy,
Quake Live**. The social banner uses icons at half the previous width and
height in a compact centered row; the README strip remains 720 pixels wide.

These rows use the [imagegen application-icon remakes](../icons/games/README.md)
as visual references. The RTCW title uses its completed multiplayer emblem.
MOHAA is included as standalone artwork; it has no implemented fnTech3 profile.
The row identifies games, not their current compatibility status. Profile
evidence remains in the [compatibility ledger](../../docs/fntech3/COMPATIBILITY.md).

Both game rows are raster artwork created with the built-in image generation
tool. The silhouette strip restores the user's preferred earlier artwork,
rearranged by copying its exact pixels into release order. It is not redrawn,
recoloured or resampled. No SVG or procedurally drawn icon supplies either row.
The silhouette strip
has transparent space between its marks and through their interior cutouts,
so the same PNG can sit on light or dark README backgrounds. The source game
icons retain their original colours and backgrounds in their own exports.

The root README embeds the horizontal banner and the silhouette strip at
720 pixels wide. Their placement is maintained in
[the README template](../../docs/templates/README.md.in); run
`python scripts/generate_docs.py` after editing that template.

The social banner is below GitHub's 1 MB limit. Upload it in the repository's **Settings → General → Social preview → Edit → Upload an image**. Adding an image file to the repository alone does not select it as the social preview. See [GitHub's social preview instructions](https://docs.github.com/en/repositories/managing-your-repositorys-settings-and-features/customizing-your-repository/customizing-your-repositorys-social-media-preview).

The unmodified social-banner output and user-supplied preferred silhouette
reference are retained under `sources/`, alongside the assembled silhouette
master, with hashes and provenance in
[the source manifest](sources/manifest.json).
`python scripts/export_brand_artwork.py` exports the social JPEG at quality 92
and rearranges the preferred silhouettes using transparent seams between
icons. It verifies that every nontransparent reference pixel occurs exactly
once with its original RGBA value. The strip export trims spare outer canvas
and adds a transparent margin without resizing the artwork. The original
vertical logo and horizontal flame banner remain
unchanged. [Generation prompts](PROMPTS.md) record the current revisions and
the earlier banner designs.
