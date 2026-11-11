from pathlib import Path

import pypdfium2 as pdfium
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parent
PDF = ROOT / "DX_FRAMEWORK_ENGINE_TEXTBOOK_RU_4_0_preview.pdf"
OUT = ROOT / "qa_textbook_v4_release"
OUT.mkdir(exist_ok=True)
pdf = pdfium.PdfDocument(str(PDF))
tiles = []
for i in range(len(pdf)):
    image = pdf[i].render(scale=1.35).to_pil().convert("RGB")
    image.save(OUT / f"page-{i + 1:03}.png", optimize=True)
    tile = image.copy()
    tile.thumbnail((360, 480))
    tiles.append(tile)

for group_start in range(0, len(tiles), 16):
    batch = tiles[group_start:group_start + 16]
    sheet = Image.new("RGB", (4 * 380, 4 * 510), "#eeeeee")
    draw = ImageDraw.Draw(sheet)
    for j, tile in enumerate(batch):
        x = (j % 4) * 380 + 10
        y = (j // 4) * 510 + 20
        sheet.paste(tile, (x, y))
        draw.text((x, y - 16), str(group_start + j + 1), fill="#111111")
    sheet.save(OUT / f"contact-{group_start // 16 + 1:02}.png", optimize=True)
print(len(pdf), "pages", (len(tiles) + 15) // 16, "contact sheets")
