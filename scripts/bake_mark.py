#!/usr/bin/env python3
"""Draw the ProsperoTube mark of the Afterglow design into native/art/.

Two open rings in white with transparency: the browser tints them with the
theme's text colour (image-color) and draws the core dot itself in the accent.
tube-mark.png is the header size (44dp: half scale at 1080p, one to one at 4K),
tube-mark-large.png the About tile's. Needs Pillow; run again only when the
drawing changes, the pictures are committed.
"""
import math
import pathlib

from PIL import Image, ImageDraw

ROOT = pathlib.Path(__file__).resolve().parents[1]
SUPER = 8          # supersampling per axis
MARGIN = 0.02      # of the size, so that no antialiased edge meets the border


def ring(layer, size, radius, thickness, start, sweep, alpha):
    """An arc with round ends; angles in radians, clockwise from twelve o'clock."""
    draw = ImageDraw.Draw(layer)
    centre = size / 2
    box = [centre - radius, centre - radius, centre + radius, centre + radius]
    first = math.degrees(start) - 90
    draw.arc(box, first, first + math.degrees(sweep), fill=alpha, width=round(thickness))
    middle = radius - thickness / 2
    for angle in (start, start + sweep):
        x, y = centre + middle * math.sin(angle), centre - middle * math.cos(angle)
        draw.ellipse([x - thickness / 2, y - thickness / 2, x + thickness / 2, y + thickness / 2], fill=alpha)


def mark(size):
    big = size * SUPER
    unit = big * (1 - 2 * MARGIN)
    alpha = Image.new("L", (big, big), 0)
    ring(alpha, big, unit * 0.5, unit * 0.105, 0.95, 2 * math.pi - 1.15, 255)
    inner = Image.new("L", (big, big), 0)
    ring(inner, big, unit * 0.30, unit * 0.085, 0.95 + math.pi, 2 * math.pi - 2.3, round(255 * 0.72))
    alpha.paste(inner, (0, 0), inner.point(lambda value: 255 if value else 0))
    alpha = alpha.resize((size, size), Image.LANCZOS)
    picture = Image.new("RGBA", (size, size), (255, 255, 255, 0))
    picture.putalpha(alpha)
    return picture


def main():
    out = ROOT / "native/art"
    out.mkdir(exist_ok=True)
    for name, size in (("tube-mark.png", 88), ("tube-mark-large.png", 256)):
        mark(size).save(out / name, optimize=True)
        print(f"{name}: {size} x {size}, {(out / name).stat().st_size} bytes")


if __name__ == "__main__":
    main()
