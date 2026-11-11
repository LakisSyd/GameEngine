from pathlib import Path

import pypdfium2 as pdfium
from PIL import Image, ImageDraw


root = Path(__file__).resolve().parent
pdf = pdfium.PdfDocument(str(root / "DX_FRAMEWORK_ENGINE_TEXTBOOK_RU_6_0_DEEP_COURSE.pdf"))
out = root / "qa_textbook_v6"
out.mkdir(exist_ok=True)
tiles = []
for index in range(len(pdf)):
    image = pdf[index].render(scale=1.2).to_pil().convert("RGB")
    image.save(out / f"page-{index + 1:03}.png", optimize=True)
    image.thumbnail((360, 480))
    tiles.append(image)

for start in range(0, len(tiles), 16):
    sheet = Image.new("RGB", (1520, 2040), "#eeeeee")
    draw = ImageDraw.Draw(sheet)
    for offset, image in enumerate(tiles[start : start + 16]):
        x = offset % 4 * 380 + 10
        y = offset // 4 * 510 + 20
        sheet.paste(image, (x, y))
        draw.text((x, y - 16), str(start + offset + 1), fill="#111111")
    sheet.save(out / f"contact-{start // 16 + 1:02}.png", optimize=True)
print(len(pdf), "pages", (len(tiles) + 15) // 16, "contact sheets")
