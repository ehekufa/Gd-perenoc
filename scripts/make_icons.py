#!/usr/bin/env python3
"""Renders the Android launcher icons from the game textures.

Usage: python3 scripts/make_icons.py
Requires Pillow (pip install pillow).
"""
import os
from PIL import Image, ImageDraw, ImageFilter

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TEX = os.path.join(ROOT, "assets", "textures")

SIZE = 1024


def tint(img, colour, strength):
    solid = Image.new("RGBA", img.size, colour + (255,))
    return Image.blend(img, solid, strength)


def bevel(img, radius, light=(255, 255, 255, 110), dark=(0, 0, 0, 150)):
    """Draws a rounded, beveled tile out of an arbitrary texture."""
    w, h = img.size
    mask = Image.new("L", (w, h), 0)
    ImageDraw.Draw(mask).rounded_rectangle([0, 0, w - 1, h - 1], radius, fill=255)
    out = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    out.paste(img, (0, 0), mask)

    d = ImageDraw.Draw(out)
    d.rounded_rectangle([6, 6, w - 7, h - 7], radius, outline=light, width=5)
    d.rounded_rectangle([0, 0, w - 1, h - 1], radius, outline=dark, width=4)
    # inner grid lines, GD tile style
    for i in range(1, 2):
        d.line([(0, h * i / 2), (w, h * i / 2)], fill=(20, 24, 56, 160), width=3)
        d.line([(w * i / 2, 0), (w * i / 2, h)], fill=(20, 24, 56, 160), width=3)
    return out


def background():
    img = Image.new("RGBA", (SIZE, SIZE), (10, 8, 26, 255))
    d = ImageDraw.Draw(img)
    for y in range(SIZE):
        t = y / SIZE
        r = int(38 + (10 - 38) * t)
        g = int(20 + (8 - 20) * t)
        b = int(74 + (32 - 74) * t)
        d.line([(0, y), (SIZE, y)], fill=(r, g, b, 255))
    glow = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    ImageDraw.Draw(glow).ellipse([120, 620, SIZE - 120, SIZE + 260],
                                 fill=(120, 90, 235, 110))
    glow = glow.filter(ImageFilter.GaussianBlur(90))
    img = Image.alpha_composite(img, glow)

    # faint checkerboard, like the level background
    checker = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    cd = ImageDraw.Draw(checker)
    cell = SIZE // 8
    for i in range(8):
        for j in range(8):
            if (i + j) % 2 == 0:
                cd.rectangle([i * cell, j * cell, (i + 1) * cell, (j + 1) * cell],
                             fill=(150, 120, 255, 14))
    img = Image.alpha_composite(img, checker)
    return img


def build_icon():
    img = background()

    blocks = Image.open(os.path.join(TEX, "blocks.png")).convert("RGBA")
    cube = Image.open(os.path.join(TEX, "cube.png")).convert("RGBA")

    # the block plate
    plate_tex = tint(blocks.resize((720, 720)), (56, 74, 150), 0.45)
    plate = bevel(plate_tex, 56)
    shadow = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    ImageDraw.Draw(shadow).rounded_rectangle(
        [172, 292, 872, 892], 56, fill=(0, 0, 0, 120))
    shadow = shadow.filter(ImageFilter.GaussianBlur(28))
    img = Image.alpha_composite(img, shadow)
    img.alpha_composite(plate, (152, 262))

    # the cube
    cube_tex = tint(cube.resize((430, 430)), (150, 100, 255), 0.35)
    cube_img = bevel(cube_tex, 26, light=(235, 245, 255, 180), dark=(20, 20, 60, 220))
    cube_img = cube_img.rotate(-14, resample=Image.BICUBIC, expand=True)
    glow = Image.new("RGBA", cube_img.size, (0, 0, 0, 0))
    glow.paste(cube_img, (0, 0), cube_img)
    glow = glow.filter(ImageFilter.GaussianBlur(26))
    glow = tint(glow, (140, 200, 255), 0.5)
    cx = (SIZE - cube_img.width) // 2
    cy = 150
    img.alpha_composite(glow, (cx, cy))
    img.alpha_composite(cube_img, (cx, cy))

    # a couple of spikes for flavour
    d = ImageDraw.Draw(img)
    for x0, w0 in ((150, 90), (790, 90), (250, 60)):
        d.polygon([(x0, 900), (x0 + w0 / 2, 900 - w0 * 1.05), (x0 + w0, 900)],
                  fill=(235, 240, 255, 235), outline=(20, 24, 60, 255))
    return img


def main():
    icon = build_icon()
    master = os.path.join(ROOT, "docs", "icon.png")
    os.makedirs(os.path.dirname(master), exist_ok=True)
    icon.save(master)

    sizes = {
        "mipmap-mdpi": 48,
        "mipmap-hdpi": 72,
        "mipmap-xhdpi": 96,
        "mipmap-xxhdpi": 144,
        "mipmap-xxxhdpi": 192,
    }
    for folder, size in sizes.items():
        dst = os.path.join(ROOT, "android", "app", "src", "main", "res", folder,
                           "ic_launcher.png")
        icon.resize((size, size), Image.LANCZOS).save(dst)
        print("wrote", dst)
    print("wrote", master)


if __name__ == "__main__":
    main()
