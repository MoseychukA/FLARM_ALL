from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


ROOT = Path(__file__).resolve().parent.parent
FONT_DIR = Path(r"C:\Windows\Fonts")
WIDTH = 800
HEIGHT = 480


def rgb565(red, green, blue):
    return ((red & 0xF8) << 8) | ((green & 0xFC) << 3) | (blue >> 3)


def make_mask(text, font_path, font_size):
    font = ImageFont.truetype(str(font_path), font_size)
    probe = Image.new("L", (1, 1), 0)
    draw = ImageDraw.Draw(probe)
    bbox = draw.textbbox((0, 0), text, font=font)
    padding = 4
    width = bbox[2] - bbox[0] + padding * 2
    height = bbox[3] - bbox[1] + padding * 2
    mask = Image.new("L", (width, height), 0)
    draw = ImageDraw.Draw(mask)
    draw.text((padding - bbox[0], padding - bbox[1]), text, font=font, fill=255)
    return mask


def encode_rle(mask):
    pixels = list(mask.getdata())
    encoded = []
    index = 0
    while index < len(pixels):
        value = pixels[index]
        count = 1
        while index + count < len(pixels) and pixels[index + count] == value and count < 255:
            count += 1
        encoded.extend((count, value))
        index += count
    return encoded


def background_image():
    image = Image.new("RGB", (WIDTH, HEIGHT))
    pixels = image.load()
    for y in range(HEIGHT):
        for x in range(WIDTH):
            left_light = ((WIDTH - 1 - x) * 22) // (WIDTH - 1)
            center_glow = ((250 - y) * 8) // 250 if y < 250 else 0
            pixels[x, y] = (
                2 + left_light // 7,
                18 + left_light // 2 + center_glow,
                42 + left_light + center_glow * 2,
            )
    return image


def main():
    white = (235, 245, 248)
    yellow = (255, 190, 40)
    labels = [
        ("title", "FlyRF Display", FONT_DIR / "arial.ttf", 64, "center", 74, white),
        ("brand", "DECIMA", FONT_DIR / "arial.ttf", 56, "center", 150, white),
        ("turning_on", "ВКЛЮЧАЕТСЯ", FONT_DIR / "ariali.ttf", 52, "center", 242, yellow),
        ("wait", "ОЖИДАЙТЕ", FONT_DIR / "ariali.ttf", 52, "center", 310, yellow),
        ("copyright", "(C) 2026", FONT_DIR / "arial.ttf", 25, 22, 407, white),
        ("website", "www.decima.ru", FONT_DIR / "arial.ttf", 24, 22, 438, white),
        ("version", "FlyRf_Disp_26_07_17_26", FONT_DIR / "arial.ttf", 22, "right", 441, white),
    ]

    generated = []
    preview = background_image()
    for name, text, font_path, font_size, x_mode, y, color in labels:
        mask = make_mask(text, font_path, font_size)
        if x_mode == "center":
            x = (WIDTH - mask.width) // 2
        elif x_mode == "right":
            x = WIDTH - mask.width - 22
        else:
            x = x_mode
        rle = encode_rle(mask)
        generated.append((name, x, y, mask.width, mask.height, rgb565(*color), rle))
        color_layer = Image.new("RGB", mask.size, color)
        preview.paste(color_layer, (x, y), mask)

    lines = [
        "#pragma once",
        "",
        "#include <stdint.h>",
        "",
        "struct SmoothTextMask {",
        "  int16_t x;",
        "  int16_t y;",
        "  uint16_t width;",
        "  uint16_t height;",
        "  uint16_t color;",
        "  const uint8_t *rle;",
        "  uint32_t rleSize;",
        "};",
        "",
    ]
    for name, _x, _y, _width, _height, _color, rle in generated:
        lines.append(f"static const uint8_t SMOOTH_{name.upper()}_RLE[] = {{")
        for offset in range(0, len(rle), 24):
            values = ", ".join(str(value) for value in rle[offset:offset + 24])
            lines.append(f"  {values},")
        lines.append("};")
        lines.append("")

    lines.append("static const SmoothTextMask SMOOTH_STARTUP_TEXTS[] = {")
    for name, x, y, width, height, color, _rle in generated:
        upper = name.upper()
        lines.append(
            f"  {{{x}, {y}, {width}, {height}, 0x{color:04X}, "
            f"SMOOTH_{upper}_RLE, sizeof(SMOOTH_{upper}_RLE)}},"
        )
    lines.extend([
        "};",
        "",
        "static const uint8_t SMOOTH_STARTUP_TEXT_COUNT =",
        "  sizeof(SMOOTH_STARTUP_TEXTS) / sizeof(SMOOTH_STARTUP_TEXTS[0]);",
        "",
    ])
    (ROOT / "SmoothStartupFonts.h").write_text("\n".join(lines), encoding="ascii")
    preview.save(ROOT / "tools" / "startup_preview.png")


if __name__ == "__main__":
    main()
