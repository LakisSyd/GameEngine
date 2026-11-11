"""Build small QA contact sheets for the rendered V12 textbook pages."""

from pathlib import Path

from PIL import Image, ImageDraw


ROOT = Path(__file__).resolve().parent / "_qa_v12_word_release"
PAGES = ROOT / "pages"
OUTPUT = ROOT / "sheets"
OUTPUT.mkdir(parents=True, exist_ok=True)

images = sorted(
    PAGES.glob("page-*.jpg"),
    key=lambda path: int(path.stem.rsplit("-", 1)[1]),
)

columns = 4
rows = 4
thumb_width = 300
label_height = 24
margin = 12

for sheet_index, start in enumerate(range(0, len(images), columns * rows), start=1):
    batch = images[start : start + columns * rows]
    opened: list[tuple[Path, Image.Image]] = []
    for path in batch:
        image = Image.open(path).convert("RGB")
        ratio = thumb_width / image.width
        image = image.resize((thumb_width, int(image.height * ratio)))
        opened.append((path, image))

    thumb_height = max(image.height for _, image in opened)
    canvas = Image.new(
        "RGB",
        (
            margin + columns * (thumb_width + margin),
            margin + rows * (thumb_height + label_height + margin),
        ),
        "#2c2f36",
    )
    draw = ImageDraw.Draw(canvas)

    for index, (path, image) in enumerate(opened):
        x = margin + (index % columns) * (thumb_width + margin)
        y = margin + (index // columns) * (thumb_height + label_height + margin)
        page_number = int(path.stem.rsplit("-", 1)[1])
        draw.text((x, y), f"Page {page_number}", fill="white")
        canvas.paste(image, (x, y + label_height))
        draw.rectangle(
            (x, y + label_height, x + image.width - 1, y + label_height + image.height - 1),
            outline="#8a8f98",
            width=1,
        )

    canvas.save(OUTPUT / f"sheet-{sheet_index:02d}.jpg", quality=88)

print(f"pages={len(images)} sheets={len(list(OUTPUT.glob('sheet-*.jpg')))}")
