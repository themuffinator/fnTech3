"""Export the raster banners and losslessly arrange the preferred silhouettes.

Run from any directory: python scripts/export_brand_artwork.py
Development-only dependency: Pillow. See assets/logos/PROMPTS.md for provenance.
"""
from pathlib import Path

from PIL import Image, ImageOps

ROOT = Path(__file__).resolve().parents[1] / "assets" / "logos"

SOURCE_ORDER = ("quake3", "quakelive", "rtcw-mp", "wolfet", "stvef-sp",
                "jka-sp", "jk2-sp", "alice", "fakk2", "mohaa")
RELEASE_ORDER = ("quake3", "fakk2", "stvef-sp", "alice", "rtcw-mp",
                 "mohaa", "jk2-sp", "wolfet", "jka-sp", "quakelive")
# Narrow corridors between the ten emblems in the user's 2172 x 724 reference.
# Alice's hair and the FAKK2 circle overlap in X, but have a transparent gap
# between them in each row. A seam through that gap preserves both outlines.
CUT_WINDOWS = ((230, 249, 239), (445, 458, 451), (693, 714, 703),
               (950, 978, 964), (1164, 1203, 1182), (1350, 1402, 1378),
               (1514, 1548, 1531), (1715, 1778, 1747), (1937, 1960, 1948))


def transparent_seam(alpha, start, stop, preferred):
    """Find a continuous background-only cut between neighboring silhouettes."""
    pixels = alpha.load()
    width = stop - start

    def cost(x, y):
        return pixels[start + x, y] * 1_000_000 + abs(start + x - preferred)

    previous = [cost(x, 0) for x in range(width)]
    back = [[0] * width]
    for y in range(1, alpha.height):
        row, parents = [], []
        for x in range(width):
            candidates = range(max(0, x - 1), min(width, x + 2))
            parent = min(candidates, key=lambda p: previous[p] + 2 * abs(p - x))
            parents.append(parent)
            row.append(cost(x, y) + previous[parent] + 2 * abs(parent - x))
        previous = row
        back.append(parents)

    seam = [0] * alpha.height
    seam[-1] = min(range(width), key=previous.__getitem__)
    for y in range(alpha.height - 1, 0, -1):
        seam[y - 1] = back[y][seam[y]]
    seam = [x + start for x in seam]
    if max(pixels[x, y] for y, x in enumerate(seam)) >= 16:
        raise ValueError("Silhouette separator crosses artwork; check the reference layout")
    return seam


def arrange_preferred_silhouettes():
    """Translate the preferred RGBA pixels; never redraw or resample an icon."""
    from collections import Counter

    reference = ROOT / "sources" / "fntech3-game-silhouettes-preferred.png"
    with Image.open(reference) as source:
        if source.mode != "RGBA" or source.size != (2172, 724):
            raise ValueError("Expected the user's original 2172 x 724 RGBA silhouette reference")
        alpha = source.getchannel("A")
        seams = [[0] * source.height]
        seams.extend(transparent_seam(alpha, *window) for window in CUT_WINDOWS)
        seams.append([source.width] * source.height)
        fragments = {}
        for index, key in enumerate(SOURCE_ORDER):
            left, right = seams[index:index + 2]
            x0, x1 = min(left), max(right)
            fragment = Image.new("RGBA", (x1 - x0, source.height))
            for y in range(source.height):
                # Paste without an alpha mask: using a mask would multiply
                # transparency a second time and damage the original edges.
                fragment.paste(source.crop((left[y], y, right[y], y + 1)),
                               (left[y] - x0, y))
            fragments[key] = fragment

        arranged = Image.new("RGBA", (sum(f.width for f in fragments.values()), source.height))
        offset = 0
        for key in RELEASE_ORDER:
            fragment = fragments[key]
            arranged.paste(fragment, (offset, 0))
            recovered = arranged.crop((offset, 0, offset + fragment.width, source.height))
            if recovered.tobytes() != fragment.tobytes():
                raise ValueError(f"Pixel preservation failed for {key}")
            offset += fragment.width

        # Every nontransparent pixel, including antialiasing, appears once
        # with its exact original RGBA value. No threshold is applied to art.
        original_pixels = Counter(p for p in source.getdata() if p[3])
        arranged_pixels = Counter(p for p in arranged.getdata() if p[3])
        if original_pixels != arranged_pixels:
            raise ValueError("Rearrangement lost, duplicated or changed source pixels")
        arranged.save(ROOT / "sources" / "fntech3-game-silhouettes.png", optimize=True)
        print("Silhouettes: all ten fragments and every nontransparent source pixel preserved")
        return arranged


def main() -> None:
    with Image.open(ROOT / "sources" / "fntech3-social-banner.png") as master:
        if abs(master.width / master.height - 16 / 9) > 0.02:
            raise ValueError("Social banner master must have a 16:9 aspect ratio")
        banner = master.convert("RGB").resize((1600, 900), Image.Resampling.LANCZOS)
        banner.save(ROOT / "fntech3-social-banner.jpg", quality=92, optimize=True)

    with arrange_preferred_silhouettes() as master:
        if master.mode != "RGBA" or master.getextrema()[3][0] != 0:
            raise ValueError("Game silhouette master must have a transparent background")
        alpha = master.getchannel("A")
        bounds = alpha.point(lambda value: 255 if value >= 16 else 0).getbbox()
        if bounds is None:
            raise ValueError("Game silhouette master is empty")
        # Ignore barely visible alpha dust when locating the outside padding.
        # The crop retains the original alpha and all interior artwork pixels.
        strip = master.crop(bounds)
        # Keep the selected raster pixels at native resolution. The README
        # controls display width; exporting must not resample the artwork.
        strip = ImageOps.expand(strip, border=16, fill=(0, 0, 0, 0))
        strip.save(ROOT / "fntech3-game-silhouettes.png", optimize=True)

    for name in ("fntech3-social-banner.jpg", "fntech3-game-silhouettes.png"):
        path = ROOT / name
        with Image.open(path) as image:
            print(f"{name}: {image.width} x {image.height}, {path.stat().st_size} bytes")


if __name__ == "__main__":
    main()
