from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


ROOT = Path(__file__).resolve().parent.parent
FONT_DIR = Path(r"C:\Windows\Fonts")
CHARACTERS = "".join(chr(code) for code in range(32, 127)) + chr(0x0401) + \
             "".join(chr(code) for code in range(0x0410, 0x0430))
CHARACTERS = "".join(chr(code) for code in range(32, 127)) + "ЁАБВГДЕЖЗИЙКЛМНОПРСТУФХЦЧШЩЪЫЬЭЮЯ"

# Итоговый набор задается кодовыми точками и не зависит от кодировки файла.
CHARACTERS = "".join(chr(code) for code in range(32, 127)) + chr(0x0401) + \
             "".join(chr(code) for code in range(0x0410, 0x0430))


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


def make_glyph(font, character):
    advance = max(1, round(font.getlength(character)))
    if character == " ":
        return Image.new("L", (1, 1), 0), 0, 0, advance

    bbox = font.getbbox(character, anchor="ls")
    padding = 2
    width = max(1, bbox[2] - bbox[0] + padding * 2)
    height = max(1, bbox[3] - bbox[1] + padding * 2)
    mask = Image.new("L", (width, height), 0)
    draw = ImageDraw.Draw(mask)
    draw.text((padding - bbox[0], padding - bbox[1]), character,
              font=font, fill=255, anchor="ls")
    return mask, bbox[0] - padding, bbox[1] - padding, advance


def emit_font(lines, name, font_path, size):
    font = ImageFont.truetype(str(font_path), size)
    glyphs = []
    for character in CHARACTERS:
        mask, x_offset, y_offset, advance = make_glyph(font, character)
        rle = encode_rle(mask)
        identifier = f"FONT_{name}_{ord(character):04X}"
        lines.append(f"static const uint8_t {identifier}_RLE[] = {{")
        for offset in range(0, len(rle), 24):
            values = ", ".join(str(value) for value in rle[offset:offset + 24])
            lines.append(f"  {values},")
        lines.append("};")
        lines.append("")
        glyphs.append((ord(character), mask.width, mask.height, x_offset, y_offset, advance, identifier))

    lines.append(f"static const SmoothGlyph FONT_{name}_GLYPHS[] = {{")
    for codepoint, width, height, x_offset, y_offset, advance, identifier in glyphs:
        lines.append(
            f"  {{{codepoint}, {width}, {height}, {x_offset}, {y_offset}, {advance}, "
            f"{identifier}_RLE, sizeof({identifier}_RLE)}},"
        )
    lines.append("};")
    lines.append("")
    lines.append(
        f"static const SmoothFont FONT_{name} = "
        f"{{FONT_{name}_GLYPHS, sizeof(FONT_{name}_GLYPHS) / sizeof(FONT_{name}_GLYPHS[0]), {size + 5}}};"
    )
    lines.append("")


def main():
    lines = [
        "#pragma once",
        "",
        "#include <stdint.h>",
        "",
        "struct SmoothGlyph {",
        "  uint16_t codepoint;",
        "  uint8_t width;",
        "  uint8_t height;",
        "  int8_t xOffset;",
        "  int8_t yOffset;",
        "  uint8_t xAdvance;",
        "  const uint8_t *rle;",
        "  uint16_t rleSize;",
        "};",
        "",
        "struct SmoothFont {",
        "  const SmoothGlyph *glyphs;",
        "  uint16_t glyphCount;",
        "  uint8_t lineHeight;",
        "};",
        "",
    ]
    emit_font(lines, "TINY", FONT_DIR / "arial.ttf", 16)
    emit_font(lines, "SMALL", FONT_DIR / "arial.ttf", 18)
    emit_font(lines, "MEDIUM", FONT_DIR / "arial.ttf", 28)
    emit_font(lines, "LARGE", FONT_DIR / "arial.ttf", 36)
    (ROOT / "SmoothRuntimeFonts.h").write_text("\n".join(lines), encoding="ascii")


if __name__ == "__main__":
    main()
