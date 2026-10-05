"""Render deterministic, unframed Emery screenshots and a store banner."""
from pathlib import Path
import struct

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
SIZE = (200, 228)


def load_masks(system):
    data = (ROOT / f"resources/numerals/{system}.bin").read_bytes()
    offsets = struct.unpack_from("<161I", data)
    masks = []
    for start, end in zip(offsets, offsets[1:]):
        width, height = data[start:start + 2]
        image = Image.new("1", (width, height))
        draw = ImageDraw.Draw(image)
        position, row = start + 2, 0
        while position < end:
            repeat, count = data[position:position + 2]
            position += 2
            for _ in range(count):
                x, length = data[position:position + 2]
                position += 2
                draw.rectangle((x, row, x + length - 1, row + repeat - 1), fill=1)
            row += repeat
        masks.append(image)
    return masks


def frames(minute):
    width, height = SIZE
    margin_x = margin_y = 2
    usable_w, usable_h = width - 2 * margin_x, height - 2 * margin_y
    state = minute * 7 % 16
    left = (usable_w - 3) * (22 + state * 56 // 15) // 100
    result = [None] * 4
    for column, column_w in enumerate((left, usable_w - 3 - left)):
        split = (usable_h - 3) * (28 + ((minute * 5 + column * 7) % 16) * 44 // 15) // 100
        x = margin_x + (left + 3 if column else 0)
        result[column] = (x, margin_y, column_w, split)
        result[column + 2] = (x, margin_y + split + 3, column_w, usable_h - 3 - split)
    return result


def render(system, hour, minute, second, upper, lower):
    masks = load_masks(system)
    image = Image.new("RGB", SIZE, "black")
    field = Image.new("RGB", SIZE, upper)
    boundary = SIZE[1] - SIZE[1] * second // 60
    ImageDraw.Draw(field).rectangle((0, boundary, SIZE[0], SIZE[1]), fill=lower)
    rounded = Image.new("1", SIZE)
    ImageDraw.Draw(rounded).rounded_rectangle((0, 0, SIZE[0] - 1, SIZE[1] - 1), radius=12, fill=1)
    image.paste(field, mask=rounded)

    digits = (hour // 10, hour % 10, minute // 10, minute % 10)
    for digit, (x, y, width, height) in zip(digits, frames(hour * 60 + minute)):
        normalized = width * 80 // height
        state = max(0, min(15, (normalized - 18) * 15 // 90))
        mask = masks[digit * 16 + state].resize((width, height), Image.Resampling.NEAREST)
        mask_canvas = Image.new("1", SIZE)
        mask_canvas.paste(mask, (x, y))
        mask_canvas = Image.composite(mask_canvas, Image.new("1", SIZE), rounded)
        image.paste("white", mask=mask_canvas)
    return image


def main():
    resources = ROOT / "resources/images"
    screenshots = ROOT / "store/screenshots/emery"
    marketing = ROOT / "store/marketing"
    resources.mkdir(parents=True, exist_ok=True)
    screenshots.mkdir(parents=True, exist_ok=True)
    marketing.mkdir(parents=True, exist_ok=True)
    scenes = [
        ("emery_01_mint_violet.png", "western", 10, 9, 18, "#55ffaa", "#aa55ff"),
        ("emery_02_sky_navy.png", "western", 8, 42, 43, "#55aaff", "#000055"),
        ("emery_03_devanagari.png", "devanagari", 3, 57, 55, "#00aaff", "#0000aa"),
    ]
    rendered = []
    for filename, system, hour, minute, second, upper, lower in scenes:
        image = render(system, hour, minute, second, upper, lower)
        image.save(screenshots / filename, optimize=True)
        rendered.append(image)

    # Embedded PBW launcher art used by Pebble/Core app lists after sideloading.
    icon = Image.new("1", (25, 25), 0)
    icon_draw = ImageDraw.Draw(icon)
    icon_draw.rounded_rectangle((1, 1, 23, 23), radius=4, outline=1, width=2)
    icon_draw.line((7, 5, 7, 19), fill=1, width=2)
    icon_draw.line((13, 5, 13, 19), fill=1, width=2)
    icon_draw.line((19, 5, 19, 19), fill=1, width=2)
    icon_draw.line((4, 11, 21, 11), fill=1, width=2)
    icon.save(resources / "menu_icon.png", optimize=True)

    banner = Image.new("RGB", (1440, 720), "#05070d")
    draw = ImageDraw.Draw(banner)
    font_path = ROOT / "tools/fonts/RobotoFlex.ttf"
    title = ImageFont.truetype(font_path, 108)
    subtitle = ImageFont.truetype(font_path, 38)
    draw.text((90, 94), "hugenumbers", font=title, fill="white")
    draw.text((96, 226), "Time rises. Numerals transform.", font=subtitle, fill="#aaffff")
    draw.text((96, 292), "Flux-inspired typography for Pebble", font=subtitle, fill="#aaaaff")
    for index, screenshot in enumerate(rendered):
        enlarged = screenshot.resize((300, 342), Image.Resampling.NEAREST)
        x = 430 + index * 320
        banner.paste(enlarged, (x, 338), enlarged.convert("RGBA"))
    banner.save(marketing / "hugenumbers-banner-1440x720.png", optimize=True)
    print("Rendered menu icon, 3 Emery screenshots, and 1 marketing banner.")


if __name__ == "__main__":
    main()
