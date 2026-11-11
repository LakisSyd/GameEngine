"""Создаёт листы предварительного просмотра всех страниц PDF вне репозитория."""

from __future__ import annotations

import tempfile
from pathlib import Path

import pypdfium2 as pdfium
from PIL import Image, ImageDraw


DOCS = Path(__file__).resolve().parent
PDF = DOCS / "DX_FRAMEWORK_ENGINE_TEXTBOOK_RU_7_0_SOURCE_COURSE.pdf"
OUT = Path(tempfile.gettempdir()) / "dx_framework_textbook_v7_qa"
OUT.mkdir(parents=True, exist_ok=True)

pdf = pdfium.PdfDocument(str(PDF))
tiles = []
for index in range(len(pdf)):
    image = pdf[index].render(scale=0.9).to_pil().convert("RGB")
    image.thumbnail((350, 480))
    tiles.append(image)

for start in range(0, len(tiles), 16):
    sheet = Image.new("RGB", (1480, 2040), "#e8edf1")
    draw = ImageDraw.Draw(sheet)
    for offset, image in enumerate(tiles[start : start + 16]):
        x = 10 + (offset % 4) * 370
        y = 25 + (offset // 4) * 505
        sheet.paste(image, (x, y))
        draw.text((x, y - 18), str(start + offset + 1), fill="#18344c")
    sheet.save(OUT / f"contact-{start // 16 + 1:02}.png", optimize=True)

print(f"{len(pdf)} pages, {(len(tiles) + 15) // 16} contact sheets: {OUT}")
