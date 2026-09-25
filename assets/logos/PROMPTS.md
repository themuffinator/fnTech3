# fnTech3 banner generation prompts

## Preferred silhouette restoration (2026-09-24)

The user selected the earlier silhouette artwork and explicitly authorized
lossless raster cropping and rearrangement. The supplied PNG is preserved
unchanged as `sources/fntech3-game-silhouettes-preferred.png` (2172 × 724).
This restoration supersedes the silhouette cleanup and generated reordering
passes recorded below; the chronological social banner remains unchanged.

`scripts/export_brand_artwork.py` copies the ten original RGBA fragments into
release order: Quake III Arena, F.A.K.K.2, Elite Force, Alice, RTCW, MOHAA,
Jedi Outcast, Enemy Territory, Jedi Academy, Quake Live. Transparent seams
separate neighboring contours without cutting visible artwork. There is no
redrawing, recolouring or resampling. The assembled 2217 × 724 master retains
every nontransparent reference pixel exactly once; the exporter checks this
and verifies each copied fragment byte for byte.

The 2229 × 292 README strip crops spare outer canvas using the alpha ≥ 16
bounds, retains all pixels inside that crop, and adds a 16 px transparent
margin. The README displays it at 720 px width. Hashes and reference provenance
are in `sources/manifest.json`.

## Chronological rows and smaller social icons (2026-09-24)

Both ten-icon rows now follow original PC release order: Quake III Arena,
F.A.K.K.2, Elite Force, Alice, RTCW, MOHAA, Jedi Outcast, Enemy Territory,
Jedi Academy, Quake Live. Release references are recorded in
`sources/manifest.json`; later ports and storefront rereleases do not affect
the order. Quake Live remains last whether its public beta or full launch is used.

The social icon edit targets half the previous width and height, keeping the
large logo and subtitle. The README silhouettes retain their full size and
720 px display width. This revision supersedes the initial arrangements below.
At this stage the selected images were unmodified imagegen outputs, exported
as a 1600 × 900 JPEG and 2097 × 290 transparent strip. The preferred silhouette
restoration above subsequently replaced that strip.

### Social-banner reordering and initial size edit

```text
Use case: precise-object-edit. Image 1 is the current fnTech3 social banner. Make ONLY these two changes to its bottom row of ten game icons: reorder them chronologically and make each icon HALF its present size.
The exact NEW left-to-right order is: Quake III Arena; Heavy Metal F.A.K.K.2; Star Trek Voyager Elite Force; American McGee's Alice; Return to Castle Wolfenstein; Medal of Honor Allied Assault; Jedi Outcast; Wolfenstein Enemy Territory; Jedi Academy; Quake Live.
To remove ambiguity, move the CURRENT slots into this new order: 1, 9, 5, 8, 3, 10, 7, 4, 6, 2. There must still be exactly ten icons, each once. Q3 is the red three-pronged emblem; FAKK2 is the round gas-masked skull; Elite Force is the gold/silver delta; Alice is the dark-haired pale face; RTCW is the red-white winged emblem WITHOUT the ET plate; MOHAA is the silver star in a gold ring; Jedi Outcast is the blue blade through a ring WITHOUT lettering; Enemy Territory is the red-white winged emblem WITH the ET plate; Jedi Academy is the blue beam and burst above red JEDI lettering; Quake Live is the round red disc with its white Q.
SIZE: scale EVERY game icon to exactly 50% of its current WIDTH and 50% of its current HEIGHT, preserving its own proportions and relative optical sizes. This means one-quarter the present area, not 50% area. On a 1600 x 900 output the icons should now be roughly 50–70 pixels high rather than the current roughly 100–140 pixels. Keep all ten on ONE centered compact horizontal row with balanced gaps. The entire icon group should occupy roughly the middle 800 pixels of the banner width, centered around x=800, with its vertical center around y=790. Leave clear breathing room below the subtitle and above the bottom edge. These smaller icons are secondary to the logo.
Preserve the EXACT existing colourful icon designs, shapes, materials and palette while rearranging and scaling them. No silhouettes, new logos, cards, backplates, labels or game names. Retain the original ET and JEDI letterforms within their icons. Completely remove the old large icons from their former positions and restore the fiery background cleanly behind them; no duplicates or ghosts.
Everything else is unchanged: preserve the big central orange/black fnTech3 flame mark, its letterforms, BLACK numeral 3, its size and position, exact orange subtitle "Unified source port for id Tech 3", typography and position, all flames, ember bokeh, smoke, colours, framing and 16:9 aspect ratio. Only the bottom icon row is to change. Deliver a finished opaque 1600 x 900 raster banner.
```

### Social-banner final size edit

```text
Use case: precise-object-edit. Image 1 is the new chronological social-banner draft. Image 2 is the original banner before this size change.
The chronological order in image 1 is correct and must remain EXACTLY as it is: Q3, FAKK2, Elite Force, Alice, RTCW, MOHAA, Jedi Outcast, Enemy Territory, Jedi Academy, Quake Live.
Make one final sizing correction: the icons in image 1 are still a little too large. Shrink its ENTIRE ten-icon footer group to 80% of its current width and height around the same group center, without changing the dimensions of the banner. This reduces each icon by another 20% to reach approximately HALF the original icon width and height in image 2.
The icon silhouettes should be only about 55–65 pixels high on the final 1600 x 900 banner, with the tallest light-beam or pointed icon allowed to reach 70 pixels including its glow. The Quake Live disc should be about 58 pixels in diameter. RTCW and ET winged emblems should be about 75 pixels wide. They must visibly be miniature footer marks, substantially smaller than the subtitle lettering. Preserve the icons' exact designs and colours. Keep all ten, each once, on a compact centered row around y=790, evenly spaced.
Do not scale the main logo, change the subtitle, move or redraw anything above the footer, or change the flame background, typography, palette, dimensions or composition. Restore the fiery backdrop cleanly in any newly vacated pixels. No duplicates, labels or other additions.
Deliver the same complete opaque 16:9 banner, target 1600 x 900.
```

### README silhouette reordering

```text
Use case: precise-object-edit. Image 1 is the approved transparent grey silhouette strip containing ten game emblems. REORDER ONLY; preserve the exact ten existing silhouettes, their proportions, grey colour, transparent negative spaces, detail cutouts and relative sizes. Do not halve or shrink these README icons.
Arrange a SINGLE horizontal row in this exact new left-to-right chronological order: Quake III Arena; Heavy Metal F.A.K.K.2; Star Trek Voyager Elite Force; American McGee's Alice; Return to Castle Wolfenstein; Medal of Honor Allied Assault; Jedi Outcast; Wolfenstein Enemy Territory; Jedi Academy; Quake Live.
The mapping from current positions to the new row is precisely: 1, 9, 5, 8, 3, 10, 7, 4, 6, 2. Check ten icons total, each once, no duplicates or omissions.
Shape identification: Q3 is the three-pronged trident; FAKK2 is the gas-mask skull and circle; Elite Force is the narrow delta and side tabs; Alice is the front-facing long-haired portrait; RTCW is the striped winged emblem without a plate; MOHAA is the five-pointed star and circle; Jedi Outcast is the vertical blade through a circle with no lettering; Enemy Territory is the winged emblem over its ET plate; Jedi Academy is the vertical beam and burst over JEDI; Quake Live is the circular Q badge.
Keep evenly spaced clear gaps, one optical center line, complete unclipped tips and circles, modest transparent safety margins. Same monochrome grey silhouette finish. No new colours, gradients, effects, labels, dates, numbers, headings or background. ET and JEDI remain as intrinsic parts of their emblems. Maintain genuine alpha transparency outside and inside the knockout spaces, with no black/white rectangle or checkerboard.
Output a wide shallow RGBA PNG strip, approximately 2130 x 300 pixels. If the output canvas has extra space, keep all ten marks in one compact horizontal row with transparent empty space around it.
```

## README grey silhouette strip (2026-09-24)

Tool: built-in `image_gen.imagegen`. The initial export trimmed unused
transparent padding (ignoring alpha values below 16 only when measuring the
crop), added a 16 px transparent safety margin and retained the generated
artwork and alpha. That strip was 2129 × 300 pixels, embedded at 720 px width.
The current master and export are described in the restoration section above.

Reference: the same ten-icon sheet used for the social banner, in the same
order. A second imagegen pass cleaned the initial silhouettes. No SVG trace,
procedural shape drawing or manual colour conversion was used.

### Generation

```text
Use case: logo-brand / style-transfer. Asset type: a horizontal transparent PNG strip of ten grey game-icon silhouettes for a GitHub README, shown directly below a brand banner.
The supplied image is a LABELLED TWO-ROW REFERENCE SHEET of ten approved game icons. It is reference only. Create completely new high-quality RASTER silhouette artwork for these ten icons, preserving their distinctive designs. Do NOT copy its two-row layout, colours, dark background, labels or squares.
LAYOUT: ONE SINGLE HORIZONTAL ROW of exactly TEN icons, each appearing once, evenly spaced and optically balanced, no wrapping. Ultra-wide short canvas, target 2400 x 300 pixels (8:1 landscape). Each icon fits within an approximately 180 x 200 pixel area. All share a visual center line and equivalent optical size; the portrait and circular icons must not overpower the thinner emblems. Generous even horizontal gaps; safe margin all around. The strip will be displayed 720 px wide. Do not crop any tip or outer circle.
Exact left-to-right order, reading the reference sheet's top row then bottom row:
1 Quake III Arena — the three-pronged Quake III trident with its shallow upturned crossbar, NOT a circular Quake emblem.
2 Quake Live — circular badge/rim with its open-top Quake Q and centered lower spike, keeping clear knockout gaps.
3 Return to Castle Wolfenstein — its exact stepped, striped, split-center geometric winged game emblem and three diagonal lower strokes.
4 Enemy Territory — the winged game emblem over its square plate with the distinct ET monogram at the bottom; preserve the ET lettering as part of the mark.
5 Elite Force — the narrow asymmetric delta with its small horizontal side tabs, with clean internal negative spaces.
6 Jedi Academy — a vertical beam into a cross-shaped light burst above the readable intrinsic JEDI capitals; simplify the beam and burst to sharp solid shapes.
7 Jedi Outcast — the single vertical pointed blade through an open circular ring; preserve ring cutouts.
8 American McGee's Alice — the front-facing head, center-parted long hair and shoulder silhouette; use a few clean transparent facial cutouts for her eyes and face so this remains an identifiable portrait, not a featureless oval.
9 F.A.K.K.2 — circular ring, skull and gas-mask silhouette; preserve the paired round eye openings and round respirator/filter shapes as bold negative-space details.
10 Medal of Honor Allied Assault — the upright five-pointed star in front of its complete circular ring; retain the five open spaces between points and rim and a few clear facet cutouts.
COLOUR: every icon in the SAME flat neutral medium grey, approximately #858B92, with smooth antialiased edges. No orange, red, blue, gold, white or black fill. No gradients, lighting, texture, bevel shading, drop shadows, photorealistic surfaces or glows. This is a precise silhouette treatment with selected transparent interior cutouts, not a greyscale photograph.
BACKGROUND: genuine transparent alpha=0 everywhere outside and between the ten shapes and inside the required knockout details. NO rectangular background, NO opaque strip, NO checkerboard pixels, NO cards or square tiles. Do not add a horizontal rule joining the icons. No title, labels, game names or numbers; only the intrinsic ET and JEDI letterforms belonging to their emblems.
Deliver one finished ultra-wide RGBA PNG containing all ten grey silhouettes in one horizontal row.
```

### Cleanup edit

```text
Use case: precise-object-edit. Image 1 is a draft horizontal strip of ten grey game silhouettes. Image 2 is the approved colour-icon reference sheet.
Polish image 1 into a clean production-quality silhouette asset. Keep exactly the same ten icons, their left-to-right order and the one-row layout. Fix the rough speckled contours, coloured fringes, ragged tiny holes, uneven fills and lost details.
Use a SINGLE perfectly flat neutral grey #858585 for all ten emblems, fully opaque inside each intended grey shape. Clean smooth antialiased outer contours. Every negative-space gap must be genuinely transparent, with crisp deliberate shapes. No coloured pixels, blue or red specks, white flecks, gradients, shading, reflections, texture, noise or semitransparent grey interiors.
Redraw the essential interior knockouts cleanly: preserve the Quake Live Q inside its circle, Wolfenstein's parallel wing gaps, the ET monogram cut out clearly from its plate, Jedi Academy's readable intrinsic JEDI lettering, the Jedi Outcast ring opening, Alice's simple face/eye features framed by her center-parted long hair, FAKK2's two round goggle lenses and round central respirator filter inside the surrounding ring, and MOHAA's five-pointed star with open gaps to its circle. The other emblems follow the reference precisely. These are grey silhouettes with a few deliberate transparent detail cutouts, not photographic greyscale images.
Improve the spacing: all ten icons must be complete and clearly separate, equal optical height, with even clear transparent gaps. No neighboring silhouettes touch. Make the first and last icons comfortably inset from the left and right canvas edges. Keep all tips safely inside the canvas.
Output an ultra-wide shallow transparent PNG strip, target 3000 x 400, suitable for a compact horizontal README row. Minimize unused transparent vertical padding; put the row near the top and bottom with a modest safety margin. Actual alpha transparency everywhere outside the shapes, not black or white background or baked checkerboard. No new symbols, words, labels, outlines around the strip, boxes, or horizontal connecting line.
```

## Ten-game social banner (2026-09-24)

Tool: built-in `image_gen.imagegen`. The unmodified generated master is
`sources/fntech3-social-banner.png`. The 1600 × 900 JPEG is a mechanical export
at quality 92. The previous four-icon row is replaced completely.

Reference 1 was the previous social banner. Reference 2 was a labelled contact
sheet of the ten [imagegen icon masters](../icons/games/sources/manifest.json),
in the order below; it only arranged and resized those references. No SVG or
procedurally drawn icon supplied this revision. The RTCW title uses its
completed multiplayer emblem. MOHAA is standalone artwork, not an implemented
engine profile. No extracted retail reference is included in these exports.

```text
Use case: precise-object-edit / compositing. Asset type: fnTech3 GitHub social preview banner, 16:9 landscape.
Image 1 is the existing social banner to update. Image 2 is a two-row labelled reference sheet of the ten newly approved game application icons. Its labels are identification aids, NOT artwork to include. Use the reference sheet in reading order, left-to-right across its top row and then its bottom row, as one horizontal row on the banner. The EXACT required left-to-right order is: Quake III Arena; Quake Live; Return to Castle Wolfenstein; Wolfenstein: Enemy Territory; Star Trek Voyager Elite Force; Jedi Academy; Jedi Outcast; American McGee's Alice; Heavy Metal F.A.K.K.2; Medal of Honor Allied Assault.
Replace the ENTIRE existing footer row of four small orange emblems with a completely newly rendered row of these TEN game emblems. Remove all four old emblems cleanly. Use the new references as the authoritative shapes and materials. Render the ten new icons in their ORIGINAL COLOURS and high-quality dimensional finish, not orange vector outlines. Keep recognizable exact silhouettes, proportions, spacing within each symbol, and fine readable details. All ten must be present ONCE and only once, evenly spaced on ONE horizontal row, centered below the subtitle. Use a generous row width of roughly 90% of the canvas. Each icon has equivalent optical weight and maximum height about 8.5% of the banner, with safe gaps and bottom clearance. They must be readable as separate icons and must not touch the subtitle.
Remove black square application backgrounds from the Elite Force and both Jedi references and let their emblems float on the dark banner background. Preserve the Jedi Academy's blue burst and exact red "JEDI" lettering, the Jedi Outcast single blue blade and circular energy ring, Alice's isolated pale face and dark hair, the complete FAKK2 mask medallion, and MOHAA's silver five-pointed star and brass ring. Keep the RTCW emblem separate from the ET plate and ET monogram. Quake Live stays a red disc with its white Quake symbol. Do not include multiplayer duplicates or the Quake IHV logo.
Preserve EVERYTHING above the footer faithfully: the large orange and black fnTech3 flame logo centered at its current position, all logo letter shapes including the BLACK numeral 3, the exact one-line orange subtitle "Unified source port for id Tech 3", its clear heavy type, and the atmospheric softly focused orange flames, dark smoke and ember background. Keep the main logo and subtitle unchanged in wording and design. Preserve the dramatic high contrast and 16:9 framing. The game icons are secondary to the large logo.
No labels under the icons, no extra text, no cards, panels, borders, new insignia or decorations. Deliver a premium finished opaque 1600 x 900 landscape banner. This must be raster artwork, not an SVG-style geometric redraw.
```

## Enemy Territory social-banner revision (2026-09-17)

Tool: built-in image generation. Final export: `fntech3-social-banner.jpg`, 1600 × 900, JPEG quality 92. Reference 2 was decoded from the pinned original ET application icon for local reference only.

Use case: precise-object-edit / compositing.
Asset type: existing fnTech3 GitHub social preview banner.
Input image 1 is the edit target: the approved 1600 x 900 social banner. Input image 2 is a small original Wolfenstein: Enemy Territory application-icon reference, solely for the fourth emblem shape.
Add the Wolfenstein: Enemy Territory emblem as the FOURTH icon in the small hot-orange game-icon row under the subtitle. Preserve the original fnTech3 flame logo, logo position and size, black fn and 3, orange TECH inset, exact subtitle "Unified source port for id Tech 3", fiery soft-focused background, colors and 16:9 composition closely.
Recenter the footer row as a compact group of FOUR distinct emblems ordered Quake III Arena, Quake Live, Return to Castle Wolfenstein, Enemy Territory. Retain the exact recognizable three existing emblems. Give the new ET icon the same optical height and monochrome orange treatment as its neighbors. From reference 2, preserve the characteristic winged Wolfenstein upper symbol and the intrinsic angular ET monogram beneath it, including dark knockout gaps. Omit the Windows-icon white outline and red/white color treatment. The ET monogram distinguishes this fourth emblem from the third RTCW emblem. Do not substitute the ET: Legacy letter-E logo. No additional game-title text or labels.
The row should remain secondary and below the subtitle, around 90 percent of image height, evenly spaced and centered with safe bottom clearance. Add width to the row to accommodate the fourth icon without shrinking the main logo or subtitle. Keep every other aspect as close to image 1 as possible.
Output an opaque finished 1600 x 900 landscape image suitable for JPEG export.

## Original banner generation

Tool: built-in image generation. Both banner prompts use the approved first logo, `fntech3-logo.png`, as their visual reference.

## Social banner

Use case: compositing / logo-brand.
Create a finished GitHub social banner for fnTech3 using image 1 as the exact approved logo. Image 1 is the FIRST logo version chosen by the user.
CANVAS: exactly 16:9 landscape, 1536 x 864 pixels, edge-to-edge opaque background.
Place the complete orange-and-black flame emblem from image 1 centrally, sharp, undistorted, and legible. Keep the exact custom lowercase black "fn", the orange "TECH" on its black band, and the BLACK numeral "3" on the orange lower body as shown in this reference. Preserve the emblem's shape, color, lettering, and proportions. Do not redesign the logo or invert the 3.
Scale the single upright emblem to about 75 percent of the canvas height and center it, leaving generous room at both sides and at least 12 percent breathing room above and below.
Backdrop: fiery, atmospheric, softly out-of-focus orange flames and warm ember bokeh across a deep charcoal-black background. Fire is diffuse and soft-focused, like a cinematic shallow-depth-of-field photograph; the logo remains crisp. Use muted crimson and glowing orange flame shapes mainly at the sides and lower edges, with dark smoke and a subdued dark central zone behind the emblem for strong contrast. No sharp busy flames overlapping the logo. Elegant, bold game-engine branding with the original logo as the focal point.
The only text anywhere is the existing "fn", "TECH", "3" within the emblem. No additional title, tagline, badges, border, UI, watermark, or mockup.
Deliver a clean 16:9 full-bleed landscape banner ready for GitHub social preview.

## Horizontal README banner

Use case: logo-brand / style-transfer.
Create the HORIZONTAL README banner variation of the exact approved fnTech3 logo in image 1.
Output a single flat orange-and-black logo on a genuinely transparent PNG background, designed for a wide short header. Canvas approximately 3:1, 1800 x 600 pixels, with the logo spanning most of the width and clean transparent margins.

Recompose the stacked vertical logo into one coherent wide horizontal flame emblem. This is a real horizontal variation, not a vertical icon next to unrelated lettering and not a stretched/distorted copy.
Arrange the exact lettering in a single readable left-to-right line: large lowercase "fn" on the left, uppercase "TECH" in the middle, and a large numeral "3" on the right.
The lowercase "fn" must closely match the reference's distinctive very bold, tightly fitted, curved serif/slab lettering and rounded f terminal. The numeral "3" must match the reference's heavy rounded numeral. Both "fn" and "3" stay BLACK on the HOT ORANGE flame body.
The middle "TECH" stays hot orange, in the reference's heavy uppercase block style, on a black inset rectangular band, integrated between the fn and 3. The three elements must feel like one cohesive logo, with close spacing and balanced optical sizes; do not add a second fnTech3 title.
Enclose the wordmark in a broad low hot-orange flame silhouette with a few clean upward curling flame tips along the upper edge and gently tapering outer flame tips at the ends. Keep the flame motif visibly related to the approved logo, but low and broad for a README header. Preserve the exact graphic language: opaque flat hot orange approximately #FF5A00, opaque black typography and inset, crisp edges, heavy custom lettering.
No white, no gradients, no texture, no shadows, no 3D, no extra text, no realistic fire, no tagline, no badge, no mockup.
The outside background and the outside spaces between flame tips must be actual fully transparent alpha=0, with no background rectangle and no checkerboard baked in. The intentional black parts within the logo stay opaque. Clean smooth antialiasing. One horizontal mark only.

## README subtitle revision

The existing horizontal banner is the edit target.

Use case: precise-object-edit / text-localization.
Image 1 is the existing approved transparent horizontal fnTech3 README banner. Edit this exact image, preserving the current logo styling.
Add one subtitle, with this exact spelling and capitalization:
"Unified source port for id Tech 3"

Extend the BOTTOM of the existing orange flame body downward into a broader, deeper, gently curved continuous orange base, creating enough space beneath the main "fn TECH 3" wordmark. The subtitle must sit directly ON THE ORANGE FILL, in opaque BLACK lettering, centered BELOW the large fnTech3 lettering. Keep it inside the silhouette, with comfortable orange padding above, below, and at the ends. This is an integrated extension of the same flame, not a separate rectangle, capsule, ribbon or floating caption.
Set the subtitle on one line in bold, clean, slightly condensed sans-serif type that complements the heavy TECH lettering. Make it readily readable when the whole banner is displayed at 720 pixels wide; subtitle lettering should occupy roughly 70–80 percent of the main logo's width, with sufficient letter height and comfortable spacing. Preserve the exact sentence case, including lowercase "id" and capital T in "Tech". No all-caps substitution.

Preserve the current main wordmark unchanged: very heavy curved black lowercase "fn" on the left, hot-orange uppercase "TECH" inside the existing black band, and the large black "3" on orange at the right. Preserve their relative sizes, letter shapes and colors, as well as the upper and side flame tongues. Keep the low, wide horizontal character; let the canvas become slightly taller only as needed for the new bottom base and subtitle. Do not squeeze or stretch the lettering.
Use only flat hot orange approximately #FF5A00 and black, with smooth antialiased edges. No gradients, shadows, glow, bevel, texture, extra symbols or extra wording.
Keep the entire exterior background and the spaces outside the flame tongues genuinely transparent (PNG alpha=0). Keep internal black lettering, TECH panel and subtitle opaque. The subtitle must not sit on transparent space. One complete horizontal emblem, with clean transparent margins and no cropped tips.

## Social subtitle revision

The existing social banner is the edit target.

Use case: precise-object-edit / text-localization.
Image 1 is the approved fnTech3 GitHub social banner. Add a subtitle to this exact design while retaining the 16:9 landscape composition and fiery soft-focused background.
Exact subtitle text, including capitalization:
"Unified source port for id Tech 3"

Keep the same upright hot-orange flame emblem, distinctive black "fn", orange "TECH" on its black strip, and BLACK numeral "3" on orange. Preserve the logo design and all letter shapes. Keep the cinematic softly blurred orange flames, ember bokeh, deep charcoal smoke and warm colors of the existing backdrop.
To make room for the subtitle, scale the COMPLETE emblem down uniformly to about 66–70 percent of the image height and position it horizontally centered in the upper-middle of the frame, leaving a clear lower text area. Do not crop or distort any part of the logo.
Place the subtitle on one centered line BELOW the emblem, within the lower part of the banner, with clear separation from the emblem and a generous bottom safe margin. Use bold, clean, slightly condensed sans-serif lettering in bright warm orange that harmonizes with the logo. Match the sentence case exactly: capital U in "Unified", lowercase "source port for id", capital T in "Tech", numeral "3".
The subtitle must be large enough to remain readily legible in a social preview thumbnail: about 42–48 pixels high on a 1600-pixel-wide canvas, spanning roughly 65–75 percent of the banner's width. Keep the backdrop directly behind the text darker and subtly defocused so the orange text reads cleanly; preserve the fiery atmosphere at the sides. No box or separate caption panel.
No additional words, badges, borders, watermarks or new objects.
Output a clean finished opaque full-bleed 16:9 landscape banner, target 1600 x 900 pixels.

## Social banner game icons

Only the social banner was updated with game icons. References are listed in the asset README.

Use case: precise-object-edit / compositing.
Edit image 1, the approved fnTech3 SOCIAL BANNER with subtitle. Add a tiny centered game-icon row below the subtitle. This edit is ONLY for the social banner.
Images 2–4 are icon references in order: Q3A three-spike/curved-ring emblem; QL circular Q badge; RTCW angular winged/eagle emblem (use only the eagle below the game title).

PRESERVE the existing large flame logo, its size and position, the exact subtitle, and the existing fiery soft-focus backdrop closely. Do not shrink the logo or subtitle to feature the icons. Do not alter the existing main logo or subtitle text:
"Unified source port for id Tech 3"

Place a small row of exactly THREE monochrome HOT-ORANGE game emblems beneath that subtitle, horizontally centered as a group, in Q3A / QL / RTCW order. Accurately preserve each icon's recognizable shape. No labels or extra wording.
IMPORTANT SCALE: the entire three-icon group must occupy ONLY 16 percent of the full canvas width, about 250 pixels total on a 1600-pixel-wide banner. Individual icons should be only about 45–50 pixels high; the RTCW eagle can be slightly wider because of its wings. This is a small secondary line, much less prominent than the subtitle. Do NOT make a large row of prominent emblems.
Use consistent visual heights and evenly spaced icon centers. Position the row in the small footer space around 88–91 percent of the canvas height, leaving roughly 20–25 pixels of clear separation below the subtitle and a safe bottom margin. Keep the small area immediately behind the icons dark for contrast. All three icons fit within a compact centered band.
Q3A: clean three-spike silhouette with its curved ring, no splatter or texture.
QL: recognizable circular badge with the open-top Q shape and short central spike, using orange and dark knockout shapes.
RTCW: the specific angular winged emblem from reference 4, not a generic eagle, W letter or game title.
Keep the original black "fn", orange "TECH" on black, black numeral 3 on orange, warm orange subtitle, charcoal smoke, flames and ember bokeh. No additional text, badges, borders, watermarks or background panels.
Output a finished opaque 16:9 landscape social banner, target 1600 x 900 pixels.
