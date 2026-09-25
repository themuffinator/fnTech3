# Game icon imagegen record

Generated on 2026-09-24 using the built-in `image_gen.imagegen` tool.
No CLI/API fallback was used. Each icon was generated separately from
its original executable icon. The source PNGs are unmodified tool outputs
(1254 x 1254 pixels, despite the requested 1024 px in the prompts).

Thirteen replacements completed. RTCW single-player was rejected by the
imagegen output safety filter three times, including a user-requested retry.
It retains its previous 1024 px vector-rendered frame, recovered from its
existing ICNS. That fallback is not an imagegen remake.

Reference identities and output hashes are in [the manifest](sources/manifest.json).
Extracted retail reference images are not distributed. Alice also used its
original menu portrait for character details; F.A.K.K.2 used its installer
splash for the skull, respirator and ring. Neither reference was composited
into the exported icons.

MOHAA was added afterward, referenced to the user's `MOHAA.exe`, and became a
profile icon (resource 115) once the `mohaa` engine profile was added.

The following are the per-icon generation prompts and final edit prompts.
Background edits restored the originals' opaque square fields; the initial
requests for an exterior alpha margin had removed parts of those fields.

## quake3

### Generation

```text
Use case: logo-brand. Asset type: a production game application icon, square 1024 x 1024 RGBA PNG.
Input image 1: original Quake III Arena executable icon, authoritative design reference. Create a new beautifully finished high-resolution remake faithful to this original, NOT a redesigned emblem and NOT an enlarged pixel image.
Preserve exactly its narrow upright central red spear, the two shorter parallel red downward prongs, the wide shallow curved crossbar with upturned hooked tips, its proportions, negative space and front view. This is the Quake III icon from the reference, not the circular Quake 1 logo. Rich blood-red lacquered metal, subtle beveled edges, dark red recesses and restrained highlights that reproduce the original depth with modern quality. Crisp continuous contours. No new elements, text, disc, badge, tile, border, ornament, lens flare or extra spikes.
Single emblem centered, occupying about 90% of canvas height, complete tips safely inside the canvas. Genuinely transparent background including the spaces between prongs; no black or white rectangle and no checkerboard pixels. Preserve the original composition and icon readability at 32 px. Deliver only the icon.
```

## quakelive

### Generation

```text
Use case: logo-brand. Asset type: a production game application icon, square 1024 x 1024 RGBA PNG.
Input image 1: the original Quake Live executable icon, authoritative visual design reference. Remake this exact design at high quality, with smooth edges and beautifully resolved materials. Preserve the circular glossy deep red disc, thin bright white/silver outer rim, and thick clean white open-topped Quake ring whose bottom joins one centered vertical spear pointing up from the disc's lower edge. Match the reference ring geometry, gap width, curved terminals, red shading and composition. Keep white symbol flat-bright, not gold, and red disc richly shaded with fine subtle texture, no added motifs or text.
This is a faithful restoration of the original icon, not a redesign. One front-facing circular icon centered and filling 92% of the square, with small even safe margin. Genuinely transparent outside the disc; the red interior remains opaque. No square tile, backdrop, drop shadow outside the circle, mockup, lettering or extra graphic. Deliver only the icon.
```

### Final edit

```text
Use case: precise-object-edit. Image 1 is the generated Quake Live application icon. Keep the exact white Quake symbol, red disc, silver-white rim, shading, proportions and design unchanged. Clean the circular outer contour: remove every stray white speck, fragment, irregular bump and pixel outside the perfect smooth circular rim. Make the single complete disc exactly centered and slightly smaller, occupying 88% of the square canvas with a clear even safe margin on all four sides. All area outside the precise circular boundary must have true zero alpha; the red disc and white artwork remain fully opaque. One clean crisp cutout, no shadow, white halo, checkerboard, border or backdrop. Preserve the smooth red material and white mark. High-resolution square PNG.
```

## rtcw-sp

Status: blocked by imagegen; previous artwork retained.

### Generation

```text
Use case: logo-brand. Create a high-resolution faithful remake of the attached original Return to Castle Wolfenstein SINGLE PLAYER application icon for neutral identification of the video game in an engine launcher. This is preservation of a fictional game software emblem, with no political message or endorsement.
Use the attached original as the exact reference for the abstract angular winged emblem: BLACK interior, fine WHITE/silver outlines and thin horizontal parallel wing stripes, split geometric head, and three tapering lower strokes. Match the proportions, negative spaces and original scale. Delicately finished black enamel and restrained polished silver edges, sharp continuous contours. Preserve the original solid dark oxblood maroon SQUARE background with square corners, filling the complete square canvas. Fully opaque image. Do not remove the maroon field. Do not add wording, new symbols, decorations, a rounded tile or a mockup. Premium high quality restoration, 1024 by 1024 PNG.
```

## rtcw-mp

### Generation

```text
Use case: logo-brand. Asset type: production game application icon, square 1024 x 1024 RGBA PNG. Input image 1 is the original retail executable icon, the authoritative design reference. Create a high-quality high-resolution remake, not a new logo and not an enlarged pixel image. Reconstruct crisp smooth geometry and clean high quality edge shading while preserving the exact original silhouette, proportions, negative spaces, colours, composition and emblem. The Wolfenstein emblem is an abstract geometric bird with a split neck/head, horizontally spread stepped three-stripe wings, and a narrow asymmetric W-like three-stripe tail. Follow the supplied reference precisely; do not invent a naturalistic bird or add historical insignia, circles, stars, weapons, text or ornaments. This is the multiplayer icon from WolfMP.exe: a BRIGHT RED eagle with a bold clean WHITE edge and white lines separating its wing stripes and tail strokes, floating without a background. Preserve the original wide wings, split head, red parts, white double-outline rhythm and downward tail. Polished red enamel with subtle restrained bevel highlights, bright ivory-white/silver edges, not deep 3D extrusions. Center the complete emblem, width 92% of canvas, keep the original broad low silhouette rather than stretching vertically. Genuinely transparent background and negative spaces. No square tile or background, maroon rectangle, gray plate, ET letters, shadow outside the mark or extra elements. Deliver only the icon.
```

## wolfet

### Generation

```text
Use case: logo-brand. Asset type: production game application icon, square 1024 x 1024 RGBA PNG. Input image 1 is the original retail executable icon, the authoritative design reference. Create a high-quality high-resolution remake, not a new logo and not an enlarged pixel image. Reconstruct crisp smooth geometry and clean high quality edge shading while preserving the exact original silhouette, proportions, negative spaces, colours, composition and emblem. The Wolfenstein emblem is an abstract geometric bird with a split neck/head, horizontally spread stepped three-stripe wings, and a narrow asymmetric W-like three-stripe tail. Follow the supplied reference precisely; do not invent a naturalistic bird or add historical insignia, circles, stars, weapons, text or ornaments. This is the Enemy Territory icon from et.exe. Keep the red abstract Wolfenstein eagle with bright white and black edging across the TOP of a light steel-grey square plate. Its wings extend wider than the plate. Keep the eagle's long red three-stripe tail descending over the plate's center. At the bottom foreground preserve the large angular interlocking black "ET" monogram edged in WHITE and black, slightly slanted exactly like the reference. Text (verbatim): "ET", no other words. Recreate polished red enamel, restrained cool steel plate shading and sharp machined bevels without introducing decorative textures. The grey plate has square corners. Exact compact stacked composition, not a generic eagle badge. Center the complete silhouette inside 92% of canvas. Genuinely transparent outside the emblem and plate, including the outer corners. No background rectangle, round tile, mockup, new lettering, or extra symbols. Deliver only the icon.
```

### Final edit

```text
Use case: precise-object-edit. Image 1 is the generated Wolfenstein: Enemy Territory game icon to correct. Image 2 is the original et.exe application icon, the authoritative geometry reference. Image 3 is the generated Return to Castle Wolfenstein multiplayer game icon, showing the correct family of geometric game emblem. These are neutral game identification assets.
Preserve image 1's grey plate, red enamel/white border/black edge materials, ET letters, size and composition. Correct ONLY its winged emblem to match image 2: the top-center must be the original SPLIT symmetrical angular double-prong form, like image 3, not a natural bird head turned left. The three long lower stripes must end in the original diagonally cut staggered W-like tail, not a rectangular middle stripe with symmetric side flaps. Preserve three horizontal bands per wing and their geometry from image 2. No new symbols or wording. Clean smooth silhouette, transparent surroundings, square high-resolution PNG. Keep "ET" exactly.
```

## stvef-sp

### Generation

```text
Use case: logo-brand. Asset type: production game application icon, square 1024 x 1024 RGBA PNG. Input image 1: original Star Trek Voyager Elite Force executable icon, authoritative visual reference. Rebuild this exact icon as beautifully finished high-resolution game emblem art, not a redesign or a blown-up pixel image. Preserve the original tall slender Starfleet delta with a rich aged gold outline and softly shaded silver face; its lower opening is an inverted V, and the elongated top point is above center. Behind the delta, preserve the red horizontal segmented bridge/bar with golden trim running left and right across the middle; follow the irregular red-and-gold silhouette in the reference rather than introducing new wings or lettering. Exact front view and original proportions. Polished metal, restrained bevels, crisp silhouette, smoothly resolved original shading. Do not add a star inside the delta, text, characters, spacecraft or decoration. Single-player design from stvoy.exe: original plain BLACK square background. The emblem occupies about 85% of the black square height with small safe margins. Keep the black square straight and square-cornered, centered filling 92% of the canvas, with a thin genuinely transparent margin outside the square. No rounded tile or drop shadow, no blue background, no nebula or starfield. Deliver only the single icon.
```

### Final edit

```text
Use case: precise-object-edit. Image 1 is the generated stvef-sp application icon to finish. Keep the icon's emblem, silhouette, geometry, colours, materials, composition unchanged. Make exactly one correction: restore its original solid pure BLACK square background. Fill ALL background and empty regions throughout the entire square canvas with fully opaque black, including the negative spaces within the emblem. The image must be opaque right up to all four straight square edges and all four corners. Preserve luminous edges and shading, cleanly blended onto the black field. Do not add rounded corners, borders, shadow, labels, objects or any other changes. Produce a fully opaque RGB PNG, 1024 x 1024, the finished square icon.
```

## stvef-mp

### Generation

```text
Use case: logo-brand. Asset type: production game application icon, square 1024 x 1024 RGBA PNG. Input image 1: original Star Trek Voyager Elite Force executable icon, authoritative visual reference. Rebuild this exact icon as beautifully finished high-resolution game emblem art, not a redesign or a blown-up pixel image. Preserve the original tall slender Starfleet delta with a rich aged gold outline and softly shaded silver face; its lower opening is an inverted V, and the elongated top point is above center. Behind the delta, preserve the red horizontal segmented bridge/bar with golden trim running left and right across the middle; follow the irregular red-and-gold silhouette in the reference rather than introducing new wings or lettering. Exact front view and original proportions. Polished metal, restrained bevels, crisp silhouette, smoothly resolved original shading. Do not add a star inside the delta, text, characters, spacecraft or decoration. Holomatch design from stvoyHM.exe: preserve its distinctive electric royal BLUE holodeck background with DARK BLUE fine tiled grid/checker pattern. Keep the SAME silver-and-gold delta and red horizontal bridge as the single-player original. Follow the reference's blue field exactly, no futuristic scenery or starfield. Background is a square with straight square corners, centered filling 92% of the canvas, with a thin genuinely transparent margin outside the square. Keep the emblem fully visible within it. No rounded tile, cast shadow, labels, lens flare or extra symbols. Deliver only the single icon.
```

## q3-ihv

### Generation

```text
Use case: logo-brand. Asset type: a production game application icon, square 1024 x 1024 RGBA PNG.
Input image 1: original February 1999 Quake III IHV Test executable icon, authoritative design reference. Make a high-resolution faithful remake with immaculate continuous edges. Preserve the BLACK Quake crescent ring on a WHITE square, open at the top with tapering tips, with precisely THREE narrow parallel black vertical strokes running through the bottom center, exactly as the reference. This is the IHV prototype icon, not the red retail trident. Keep the reference's proportions and stark monochrome appearance. Slightly beveled charcoal edge shading only if consistent with the original; no extra ornament, colour, typography, textures or modernized logo. The original has three plain bars rather than decorative serif letters.
One square white icon tile with straight square corners, centered occupying 92% of the canvas, and a thin genuinely transparent exterior safety margin. Do not round corners. No shadow, text, mockup, extra borders, or background outside the tile. Deliver only the icon.
```

### Final edit

```text
Use case: precise-object-edit. Image 1 is the generated q3-ihv application icon to finish. Keep the icon's emblem, silhouette, geometry, colours, materials, composition unchanged. Make exactly one correction: restore its original solid pure WHITE square background. Fill ALL background and empty regions throughout the entire square canvas with fully opaque white, including the negative spaces within the emblem. The image must be opaque right up to all four straight square edges and all four corners. The emblem itself remains BLACK, with exactly three parallel vertical bars, clearly visible against the white field. Do not add rounded corners, borders, shadow, labels, objects or any other changes. Produce a fully opaque RGB PNG, 1024 x 1024, the finished square icon.
```

### Nail correction

The user corrected the initial interpretation: the three strokes are nails,
with wider heads and pointed tips. This edit supersedes the bars requested above.

```text
Use case: precise-object-edit. Asset type: Quake III IHV Test application icon.
Image 1 is the current generated IHV icon to edit. The user correction is: "for q3 ihv, the 3 lines should be nails".
Change only the THREE plain vertical black bars into THREE recognizable long black iron NAILS. Each nail has a short clearly wider flat nail head at the TOP, a narrow straight shaft, and a clean sharply tapered pointed tip at the BOTTOM. All three nails point DOWN. Keep exactly three, parallel, upright, evenly spaced, equal length, centered in their current positions, passing through the lower center of the Quake crescent. The nail heads must be visibly wider than their shafts; the lower ends must be pointed rather than blunt rectangles. The narrow white gaps between the nails must remain clear. This should read as three nails integrated into the original Quake III IHV logo, not the letters III, screws, bolts, swords or red retail Quake III prongs.
Preserve the existing outer crescent ring EXACTLY: same black colour, shape, opening, tapered upper tips, width, placement and size. Preserve the pure opaque WHITE square background, square corners, current framing and margins. Use the same restrained monochrome logo finish with crisp clean high-resolution edges. Do not add text, perspective, shadows, colours, decoration, frames or new shapes outside the three nails.
Deliver a fully opaque RGB square PNG at the same resolution as the input.
```

## jka-sp

### Generation

```text
Use case: logo-brand. Asset type: production game application icon, square 1024 x 1024 RGBA PNG. Input image 1 is the original retail executable icon, the authoritative design reference. Create a beautifully finished high-resolution restoration of that exact icon: preserve composition, colours, geometry and the distinction between single player and multiplayer. Smooth edges, premium game key-art finish, clear at 32 px. Not an enlarged pixel image or a new emblem. Keep the original black square background with straight square corners, centered occupying 92% of the canvas with a thin truly transparent exterior margin. Do not round the tile or add a frame, drop shadow, characters, weapons with handles, titles, stars or unrelated decoration. Jedi Academy SINGLE PLAYER (jasp.exe). One narrow vertical electric-blue beam descends from the top center into a radiant blue-white horizontal starburst above the lettering. The burst has a brilliant compact white center with a soft concentrated cobalt aura, symmetrical thin horizontal rays. The LOWER THIRD contains exactly "JEDI" in large tightly spaced RED serif capitals, with subtle beveled crimson-metal shading. Text (verbatim): "JEDI". Match the original tall letterforms and baseline; keep text prominent and clearly readable. No other text, no gold ring, no second beam, no hilt. Preserve the original silhouette and black negative space. Deliver only the icon.
```

### Final edit

```text
Use case: precise-object-edit. Image 1 is the generated jka-sp application icon to finish. Keep the icon's emblem, silhouette, geometry, colours, materials, composition and exact red "JEDI" lettering unchanged. Make exactly one correction: restore its original solid pure BLACK square background. Fill ALL background and empty regions throughout the entire square canvas with fully opaque black, including the negative spaces within the emblem. The image must be opaque right up to all four straight square edges and all four corners. Preserve luminous edges and shading, cleanly blended onto the black field. Do not add rounded corners, borders, shadow, labels, objects or any other changes. Produce a fully opaque RGB PNG, 1024 x 1024, the finished square icon.
```

## jka-mp

### Generation

```text
Use case: logo-brand. Asset type: production game application icon, square 1024 x 1024 RGBA PNG. Input image 1 is the original retail executable icon, the authoritative design reference. Create a beautifully finished high-resolution restoration of that exact icon: preserve composition, colours, geometry and the distinction between single player and multiplayer. Smooth edges, premium game key-art finish, clear at 32 px. Not an enlarged pixel image or a new emblem. Keep the original black square background with straight square corners, centered occupying 92% of the canvas with a thin truly transparent exterior margin. Do not round the tile or add a frame, drop shadow, characters, weapons with handles, titles, stars or unrelated decoration. Jedi Academy MULTIPLAYER (jamp.exe). TWO clearly distinct parallel vertical electric-blue light beams descend from the upper edge, one left of center, one right of center, into TWO blue-white horizontal light bursts above the text. Match the reference's stagger: the left burst is slightly higher, the right burst slightly lower; their concentrated cobalt-blue luminous envelopes overlap across the center. The LOWER THIRD contains exactly "JEDI" in large tightly spaced RED serif capitals with subtly beveled crimson-metal shading. Text (verbatim): "JEDI". Same type style as original, readable red letters, no other wording. Do not merge the two beams into one. Preserve black negative space and original composition. Deliver only the icon.
```

### Final edit

```text
Use case: precise-object-edit. Image 1 is the generated jka-mp application icon to finish. Keep the icon's emblem, silhouette, geometry, colours, materials, composition and exact red "JEDI" lettering unchanged. Make exactly one correction: restore its original solid pure BLACK square background. Fill ALL background and empty regions throughout the entire square canvas with fully opaque black, including the negative spaces within the emblem. The image must be opaque right up to all four straight square edges and all four corners. Preserve luminous edges and shading, cleanly blended onto the black field. Do not add rounded corners, borders, shadow, labels, objects or any other changes. Produce a fully opaque RGB PNG, 1024 x 1024, the finished square icon.
```

## jk2-sp

### Generation

```text
Use case: logo-brand. Asset type: production game application icon, square 1024 x 1024 RGBA PNG. Input image 1 is the original retail executable icon, the authoritative design reference. Create a beautifully finished high-resolution restoration of that exact icon: preserve composition, colours, geometry and the distinction between single player and multiplayer. Smooth edges, premium game key-art finish, clear at 32 px. Not an enlarged pixel image or a new emblem. Keep the original black square background with straight square corners, centered occupying 92% of the canvas with a thin truly transparent exterior margin. Do not round the tile or add a frame, drop shadow, characters, weapons with handles, titles, stars or unrelated decoration. Jedi Outcast SINGLE PLAYER (jk2sp.exe). No lettering at all. A single upright WHITE blade of light descends from near the top center into and through ONE rounded circular/oval silver-white energy ring in the lower center, then terminates in a pointed white tip near the bottom. Follow the reference's ring silhouette and internal black/grey negative-space channels carefully. A narrow intense CYAN glow hugs the blade and ring, grading into deep cobalt blue then black. This is a stylized luminous blade-and-orbit emblem, NOT a realistic lightsaber with a handle, not a generic lens flare. Keep the original bold white/silver central core, lightly shaded ring, cyan edge aura and broad black corners. Avoid excessive rays or smoke; the icon's ring must remain legible. Deliver only the icon.
```

### Final edit

```text
Use case: precise-object-edit. Image 1 is the generated jk2-sp application icon to finish. Keep the icon's emblem, silhouette, geometry, colours, materials, composition unchanged. Make exactly one correction: restore its original solid pure BLACK square background. Fill ALL background and empty regions throughout the entire square canvas with fully opaque black, including the negative spaces within the emblem. The image must be opaque right up to all four straight square edges and all four corners. Preserve luminous edges and shading, cleanly blended onto the black field. Do not add rounded corners, borders, shadow, labels, objects or any other changes. Produce a fully opaque RGB PNG, 1024 x 1024, the finished square icon.
```

## jk2-mp

### Generation

```text
Use case: logo-brand. Asset type: production game application icon, square 1024 x 1024 RGBA PNG. Input image 1 is the original retail executable icon, the authoritative design reference. Create a beautifully finished high-resolution restoration of that exact icon: preserve composition, colours, geometry and the distinction between single player and multiplayer. Smooth edges, premium game key-art finish, clear at 32 px. Not an enlarged pixel image or a new emblem. Keep the original black square background with straight square corners, centered occupying 92% of the canvas with a thin truly transparent exterior margin. Do not round the tile or add a frame, drop shadow, characters, weapons with handles, titles, stars or unrelated decoration. Jedi Outcast MULTIPLAYER (jk2mp.exe). No lettering at all. TWO parallel WHITE blades of light with silver-white circular/oval energy rings around their LOWER portions: left blade taller and left ring slightly higher, right blade shorter and right ring lower. Preserve the clearly double, tightly overlapping ring arrangement seen in the reference. White/silver blades have a narrow concentrated CYAN glow grading to cobalt blue then black. Exact compact paired silhouette and original proportions, softly shaded ring surfaces with black negative-space interiors. Not a single merged blade, not a double-ended saber, no handles and no crossing blades. Avoid excessive rays, flare or smoke, which would obscure the paired rings. Deliver only the icon.
```

### Final edit

```text
Use case: precise-object-edit. Image 1 is the generated jk2-mp application icon to finish. Keep the icon's emblem, silhouette, geometry, colours, materials, composition unchanged. Make exactly one correction: restore its original solid pure BLACK square background. Fill ALL background and empty regions throughout the entire square canvas with fully opaque black, including the negative spaces within the emblem. The image must be opaque right up to all four straight square edges and all four corners. Preserve luminous edges and shading, cleanly blended onto the black field. Do not add rounded corners, borders, shadow, labels, objects or any other changes. Produce a fully opaque RGB PNG, 1024 x 1024, the finished square icon.
```

## alice

### Generation

```text
Use case: stylized-concept. Asset type: production game application icon, square 1024 x 1024 RGBA PNG.
Input image 1: original American McGee's Alice (2000) executable icon, PRIMARY authoritative composition reference. Input image 2: Alice's original in-game portrait, supporting reference ONLY for her face, pale complexion, large vivid green eyes, black hair, costume and distinctive gothic art direction. DO NOT copy the side-facing pose from image 2.
Create a beautifully rendered high-resolution remake of the ORIGINAL FRONT-FACING HEAD PORTRAIT icon. Alice looks straight at the viewer with the same stern, wary expression, narrow pale face, strong arched dark brows, green eyes, dark eye circles, small unsmiling lips, and straight center-parted long black hair with silver highlights at the crown. Keep her recognizable American McGee's Alice character design, not a generic woman, modern fashion model, Disney Alice, anime character, or Alice Madness Returns redesign. Use a richly resolved painterly/pre-rendered gothic game-art style, with sculpted face anatomy, subtle pale skin shading, crisp fine hair detail, slightly unsettling beauty, and convincing depth. No plastic vector face.
Match the icon's tight straight-on crop: full hair crown near the top, pale face dominant in the center, hair descending to both lower sides, just a tiny hint of the original white collar and blue garment at the bottom. Genuinely transparent around the hair and shoulder silhouette; NO square background, rectangular portrait panel, scenery, lettering, embellishments or objects. Complete silhouette centered and inside the canvas with small safe margins. Deliver only the icon.
```

## fakk2

### Generation

```text
Use case: stylized-concept. Asset type: production game application icon, square 1024 x 1024 RGBA PNG.
Input image 1: original Heavy Metal F.A.K.K.2 executable icon, PRIMARY authoritative composition and proportions reference. Input image 2: original installer art, supporting high-detail reference for the SAME skull, respirator, horns, segmented ring and materials.
Create a premium high-resolution remake of the original gas-masked skull medallion icon. Preserve the bone-white human skull with small side horns, dark circular glass goggle lenses inside aged metal rims, nasal cavity and skeletal cheeks, and worn industrial respirator with a large central round filter and two angled canisters extending down-left/down-right. Behind it: the original dark green radial plate in a circular rusty crimson-red segmented metal ring divided by dark radial spokes. The skull is centered, front view. Exact original subject, shape, compact composition and proportions. Give the skull convincingly aged bone with fine cracks, realistic machined respirator parts, smoky reflective goggle lenses and weathered red metal, lit clearly enough to read as an application icon. Retain the original muted bone/red/green/black palette, not gold.
Use image 2 only to resolve the subject, not to add its splash-screen lettering: NO text anywhere on the forehead or filter, no HEAVY METAL text, no FAKK2 title and no website address. No extra weapons, flames, ornaments, symbols, rectangular backdrop or scene.
One fully visible circular medallion, centered occupying 90% of the square canvas with comfortable even margins. Genuinely transparent outside the circle; dark-green interior stays opaque. Deliver only the icon.
```

## mohaa

The `mohaa` profile's icon (resource 115), with its generated launcher.

### Generation

```text
Use case: logo-brand. Asset type: a production application icon for Medal of Honor: Allied Assault, high-resolution square PNG.
The attached image is an enlarged nearest-neighbor preview of the ORIGINAL 32px icon extracted from the user's MOHAA.exe. It is the authoritative design reference. Reconstruct that exact emblem with beautifully resolved high-quality materials and clean continuous contours, not pixel art and not an enlarged pixel image.
Preserve the classic single FIVE-POINTED star, upright with one point at twelve o'clock, centered over a slim complete circular ring. The star is metallic SILVER with champagne-gold/beige edge reflections, faceted bevels and dark graphite recessed facets. The surrounding circle is warm antique GOLD/BRASS with restrained copper edge reflections. The ring passes BEHIND the five star points and remains visible as five curved arcs in the spaces between the star's points. Reproduce the original scale relationship, silhouette and front-facing composition. Keep the original pale top star point, horizontal left/right shoulders and two lower diagonal points, with the circular rim close around the star. No extra ribbons, medal suspension, wings, wreaths, letters, flags, military devices or decorations.
Modern premium icon craftsmanship: sharp machined bevels, realistic brushed metal, subtle rich warm highlights, restrained dark shadows contained within the metal. No distressed dirt, scratches or added textures that alter the design. Symmetric, flat frontal view, no tilt or perspective. The complete star and rim centered occupying 90% of the canvas, with safe even margins.
GENUINELY transparent alpha around the outside and THROUGH the five spaces between star and circle. Those dark areas in the enlarged reference are empty negative space, not a black disc or square backdrop. No white or black rectangle, no baked checkerboard, no text, no drop shadow outside the emblem, no border or mockup. Deliver just the beautifully remade star-and-ring icon as a 1536 x 1536 transparent PNG.
```
