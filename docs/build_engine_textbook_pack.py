from __future__ import annotations

import math
import re
import tempfile
import textwrap
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont
from docx import Document
from docx.enum.style import WD_STYLE_TYPE
from docx.enum.table import WD_CELL_VERTICAL_ALIGNMENT, WD_TABLE_ALIGNMENT
from docx.enum.text import WD_ALIGN_PARAGRAPH
from docx.oxml import OxmlElement
from docx.oxml.ns import qn
from docx.shared import Inches, Pt, RGBColor

import build_offline_pack as base


ROOT = Path(__file__).resolve().parents[1]
DOCS = ROOT / "docs"
PAGE_WIDTH_DXA = 9360

BLUE = "2E74B5"
DARK_BLUE = "1F4D78"
NAVY = "203748"
INK = "24313C"
MUTED = "5A6069"
PAPER = "FAFBFD"
PALE_BLUE = "E8F1FA"
PALE_GREEN = "E9F5EE"
PALE_ORANGE = "FFF1DF"
PALE_PURPLE = "F0EBFA"


def _font(size: int, bold: bool = False):
    candidates = [
        Path("C:/Windows/Fonts/calibrib.ttf" if bold else "C:/Windows/Fonts/calibri.ttf"),
        Path("C:/Windows/Fonts/arialbd.ttf" if bold else "C:/Windows/Fonts/arial.ttf"),
    ]
    for candidate in candidates:
        if candidate.exists():
            return ImageFont.truetype(str(candidate), size=size)
    return ImageFont.load_default()


def _text_size(draw, text, font):
    box = draw.textbbox((0, 0), text, font=font)
    return box[2] - box[0], box[3] - box[1]


def _wrap(draw, text, font, max_width):
    words = text.split()
    lines = []
    current = ""
    for word in words:
        candidate = f"{current} {word}".strip()
        if not current or _text_size(draw, candidate, font)[0] <= max_width:
            current = candidate
        else:
            lines.append(current)
            current = word
    if current:
        lines.append(current)
    return lines


def _centered_lines(draw, box, text, font, fill, gap=4):
    x0, y0, x1, y1 = box
    lines = _wrap(draw, text, font, x1 - x0 - 30)
    heights = [_text_size(draw, line, font)[1] for line in lines]
    total = sum(heights) + gap * max(0, len(lines) - 1)
    y = y0 + (y1 - y0 - total) / 2
    for line, height in zip(lines, heights):
        width = _text_size(draw, line, font)[0]
        draw.text((x0 + (x1 - x0 - width) / 2, y), line, font=font, fill=f"#{fill}")
        y += height + gap


def _box(draw, box, title, subtitle="", fill=PALE_BLUE, outline=BLUE):
    draw.rounded_rectangle(box, radius=18, fill=f"#{fill}", outline=f"#{outline}", width=3)
    x0, y0, x1, y1 = box
    if subtitle:
        _centered_lines(draw, (x0 + 8, y0 + 8, x1 - 8, y0 + (y1 - y0) * 0.53), title, _font(29, True), NAVY)
        _centered_lines(draw, (x0 + 8, y0 + (y1 - y0) * 0.45, x1 - 8, y1 - 6), subtitle, _font(22), MUTED)
    else:
        _centered_lines(draw, box, title, _font(27, True), NAVY)


def _arrow(draw, start, end, color=BLUE, width=5):
    draw.line([start, end], fill=f"#{color}", width=width)
    angle = math.atan2(end[1] - start[1], end[0] - start[0])
    length = 17
    spread = 0.55
    p1 = (end[0] - length * math.cos(angle - spread), end[1] - length * math.sin(angle - spread))
    p2 = (end[0] - length * math.cos(angle + spread), end[1] - length * math.sin(angle + spread))
    draw.polygon([end, p1, p2], fill=f"#{color}")


def _canvas(title, subtitle=""):
    image = Image.new("RGB", (1600, 900), f"#{PAPER}")
    draw = ImageDraw.Draw(image)
    draw.text((70, 42), title, font=_font(42, True), fill=f"#{NAVY}")
    if subtitle:
        draw.text((72, 100), subtitle, font=_font(24), fill=f"#{MUTED}")
    draw.line([(70, 143), (1530, 143)], fill=f"#{BLUE}", width=3)
    return image, draw


def make_architecture_diagram(path: Path):
    image, draw = _canvas("Карта владения и зависимостей", "Стрелка означает вызов или передачу non-owning контекста")
    _box(draw, (560, 175, 1040, 285), "WinMain → SystemClass", "окно · messages · общий shutdown", PALE_PURPLE)
    _box(draw, (500, 335, 1100, 465), "ApplicationClass", "composition root · Frame · порядок подсистем", PALE_BLUE)
    _arrow(draw, (800, 285), (800, 335))

    boxes = [
        ((65, 555, 350, 705), "SceneManager + ECS", "entities · components · save/load", PALE_GREEN),
        ((365, 555, 650, 705), "Renderer + D3D11", "passes · shaders · GPU state", PALE_BLUE),
        ((665, 555, 950, 705), "PhysicsWorld", "Bullet · fixed step · ownership", PALE_ORANGE),
        ((965, 555, 1250, 705), "Jobs + Assets", "CPU workers · main-thread upload", PALE_PURPLE),
        ((1265, 555, 1535, 705), "Editor + Input", "commands · picking · UI", "F7F1E6"),
    ]
    for box, title, subtitle, fill in boxes:
        _box(draw, box, title, subtitle, fill)
        cx = (box[0] + box[2]) // 2
        _arrow(draw, (800, 465), (cx, box[1]))

    _box(draw, (250, 775, 1350, 850), "assets · scenes · models · textures · HLSL · configuration", fill="F2F4F7", outline="AEB8C2")
    for box, *_ in boxes:
        cx = (box[0] + box[2]) // 2
        _arrow(draw, (cx, box[3]), (cx, 775), color="7A8793", width=3)
    image.save(path)


def make_frame_diagram(path: Path):
    image, draw = _canvas("Один кадр DX Framework", "Порядок — часть корректности: producer обязан отработать раньше consumer")
    stages = [
        ("Time + Win32", "dt · messages", PALE_PURPLE),
        ("Input + режим", "Editor / Play", PALE_GREEN),
        ("Main callbacks", "jobs · imports", PALE_PURPLE),
        ("Simulation", "intent · AI · animation", PALE_GREEN),
        ("Fixed physics", "flood → joints → Bullet", PALE_ORANGE),
        ("Scene update", "transforms · lights · matrices", PALE_GREEN),
        ("Render snapshot", "culling · immutable candidates", PALE_BLUE),
        ("Render passes", "shadow → scene → transparent", PALE_BLUE),
        ("UI + Present", "editor · HUD · swap chain", "F7F1E6"),
    ]
    positions = []
    left = 65
    top = 185
    width = 430
    height = 150
    gap_x = 90
    gap_y = 85
    for idx, (title, subtitle, fill) in enumerate(stages):
        row = idx // 3
        col = idx % 3
        if row % 2 == 1:
            col = 2 - col
        x0 = left + col * (width + gap_x)
        y0 = top + row * (height + gap_y)
        box = (x0, y0, x0 + width, y0 + height)
        _box(draw, box, title, subtitle, fill)
        positions.append(box)
    for a, b in zip(positions, positions[1:]):
        ax = (a[0] + a[2]) // 2
        ay = (a[1] + a[3]) // 2
        bx = (b[0] + b[2]) // 2
        by = (b[1] + b[3]) // 2
        if abs(ay - by) < 10:
            start = (a[2], ay) if bx > ax else (a[0], ay)
            end = (b[0], by) if bx > ax else (b[2], by)
        else:
            start = (ax, a[3])
            end = (bx, b[1])
        _arrow(draw, start, end)
    image.save(path)


def make_spaces_diagram(path: Path):
    image, draw = _canvas("Путь вершины по пространствам", "Left-handed · +Y вверх · +Z вперёд · row-vector convention")
    stages = [
        ("MODEL", "локальные координаты", PALE_GREEN),
        ("WORLD", "pʷ = pᵐ · W", PALE_BLUE),
        ("VIEW", "pᵛ = pʷ · V", PALE_PURPLE),
        ("CLIP", "pᶜ = pᵛ · P", PALE_ORANGE),
        ("NDC / SCREEN", "divide by w · viewport", "F7F1E6"),
    ]
    x = 45
    y0 = 245
    w = 260
    h = 170
    gap = 58
    boxes = []
    for title, subtitle, fill in stages:
        box = (x, y0, x + w, y0 + h)
        _box(draw, box, title, subtitle, fill)
        boxes.append(box)
        x += w + gap
    for a, b in zip(boxes, boxes[1:]):
        _arrow(draw, (a[2], (a[1] + a[3]) // 2), (b[0], (b[1] + b[3]) // 2))

    _box(draw, (150, 545, 1450, 690), "p_clip = p_model · W · V · P", "После perspective divide: x,y ∈ [−1,1], DirectX depth z ∈ [0,1]", "F2F4F7", "7A8793")
    draw.text((180, 760), "Нормаль при non-uniform scale:  n' = normalize(n · (W⁻¹)ᵀ)", font=_font(30, True), fill=f"#{DARK_BLUE}")
    image.save(path)


def make_render_diagram(path: Path):
    image, draw = _canvas("Фактический forward render pipeline", "Условные off-screen passes готовят ресурсы для основного кадра")
    _box(draw, (60, 200, 380, 330), "Candidate snapshot", "ECS → immutable draw list", PALE_GREEN)
    _box(draw, (475, 170, 760, 285), "Shadow", "depth from light", PALE_PURPLE)
    _box(draw, (475, 315, 760, 430), "Reflection", "условно: вода", PALE_BLUE)
    _box(draw, (475, 460, 760, 575), "Refraction", "условно: вода", PALE_BLUE)
    for y in (225, 370, 515):
        _arrow(draw, (380, 265), (475, y))

    _box(draw, (855, 220, 1165, 360), "Main opaque", "PBR · terrain · sky", PALE_BLUE)
    for y in (225, 370, 515):
        _arrow(draw, (760, y), (855, 290))
    _box(draw, (1245, 200, 1540, 330), "Transparent", "back-to-front target", PALE_ORANGE)
    _arrow(draw, (1165, 290), (1245, 265))

    _box(draw, (855, 500, 1165, 625), "Particles + Debug", "world overlays", PALE_PURPLE)
    _box(draw, (1245, 480, 1540, 645), "First-person + UI", "depth clear · HUD · editor", "F7F1E6")
    _arrow(draw, (1010, 360), (1010, 500))
    _arrow(draw, (1165, 562), (1245, 562))
    _box(draw, (610, 745, 990, 835), "Present", "swap chain", "F2F4F7", "7A8793")
    _arrow(draw, (1392, 645), (890, 745))
    image.save(path)


def make_diagrams(folder: Path):
    paths = {
        "architecture": folder / "architecture.png",
        "frame": folder / "frame.png",
        "spaces": folder / "spaces.png",
        "render": folder / "render.png",
    }
    make_architecture_diagram(paths["architecture"])
    make_frame_diagram(paths["frame"])
    make_spaces_diagram(paths["spaces"])
    make_render_diagram(paths["render"])
    return paths


def add_caption_style(doc):
    if "FigureCaptionOffline" in doc.styles:
        return
    style = doc.styles.add_style("FigureCaptionOffline", WD_STYLE_TYPE.PARAGRAPH)
    style.font.name = "Calibri"
    style._element.rPr.rFonts.set(qn("w:ascii"), "Calibri")
    style._element.rPr.rFonts.set(qn("w:hAnsi"), "Calibri")
    style._element.rPr.rFonts.set(qn("w:eastAsia"), "Calibri")
    style.font.size = Pt(9)
    style.font.italic = True
    style.font.color.rgb = RGBColor(90, 96, 105)
    style.paragraph_format.space_before = Pt(3)
    style.paragraph_format.space_after = Pt(9)
    style.paragraph_format.keep_with_next = False


def add_figure(doc, path: Path, caption: str, alt_text: str):
    p = doc.add_paragraph()
    p.alignment = WD_ALIGN_PARAGRAPH.CENTER
    p.paragraph_format.space_before = Pt(5)
    p.paragraph_format.space_after = Pt(0)
    run = p.add_run()
    inline = run.add_picture(str(path), width=Inches(6.45))
    doc_pr = inline._inline.docPr
    doc_pr.set("descr", alt_text)
    doc_pr.set("title", caption)
    cap = doc.add_paragraph(style="FigureCaptionOffline")
    cap.alignment = WD_ALIGN_PARAGRAPH.CENTER
    cap.add_run(caption)


def _column_widths(rows, cols):
    maxima = []
    for col in range(cols):
        maximum = max((len(re.sub(r"[*`]", "", row[col])) if col < len(row) else 0) for row in rows)
        maxima.append(max(10, min(maximum, 55)))
    minimum = 1100 if cols >= 4 else 1350
    remaining = PAGE_WIDTH_DXA - minimum * cols
    if remaining <= 0:
        widths = [PAGE_WIDTH_DXA // cols] * cols
    else:
        total = sum(maxima)
        widths = [minimum + round(remaining * value / total) for value in maxima]
    widths[-1] += PAGE_WIDTH_DXA - sum(widths)
    return widths


def set_cell_width(cell, width):
    tc_pr = cell._tc.get_or_add_tcPr()
    tc_w = tc_pr.find(qn("w:tcW"))
    if tc_w is None:
        tc_w = OxmlElement("w:tcW")
        tc_pr.append(tc_w)
    tc_w.set(qn("w:w"), str(width))
    tc_w.set(qn("w:type"), "dxa")


def add_fixed_table(doc, rows):
    if not rows:
        return
    cols = max(len(row) for row in rows)
    widths = _column_widths(rows, cols)
    table = doc.add_table(rows=len(rows), cols=cols)
    table.alignment = WD_TABLE_ALIGNMENT.LEFT
    table.autofit = False
    table.allow_autofit = False
    table.style = "Table Grid"
    base.set_table_borders(table)

    tbl_pr = table._tbl.tblPr
    layout = tbl_pr.find(qn("w:tblLayout"))
    if layout is None:
        layout = OxmlElement("w:tblLayout")
        tbl_pr.append(layout)
    layout.set(qn("w:type"), "fixed")

    tbl_w = tbl_pr.find(qn("w:tblW"))
    if tbl_w is None:
        tbl_w = OxmlElement("w:tblW")
        tbl_pr.insert(0, tbl_w)
    tbl_w.set(qn("w:w"), str(PAGE_WIDTH_DXA))
    tbl_w.set(qn("w:type"), "dxa")

    tbl_ind = tbl_pr.find(qn("w:tblInd"))
    if tbl_ind is None:
        tbl_ind = OxmlElement("w:tblInd")
        tbl_pr.append(tbl_ind)
    tbl_ind.set(qn("w:w"), "120")
    tbl_ind.set(qn("w:type"), "dxa")

    grid = table._tbl.tblGrid
    for child in list(grid):
        grid.remove(child)
    for width in widths:
        col = OxmlElement("w:gridCol")
        col.set(qn("w:w"), str(width))
        grid.append(col)

    for row_index, row in enumerate(rows):
        tr_pr = table.rows[row_index]._tr.get_or_add_trPr()
        cant_split = OxmlElement("w:cantSplit")
        tr_pr.append(cant_split)
        if row_index == 0:
            repeat = OxmlElement("w:tblHeader")
            repeat.set(qn("w:val"), "true")
            tr_pr.append(repeat)
        for col_index in range(cols):
            cell = table.cell(row_index, col_index)
            set_cell_width(cell, widths[col_index])
            cell.vertical_alignment = WD_CELL_VERTICAL_ALIGNMENT.CENTER
            base.set_cell_margins(cell, top=80, start=120, bottom=80, end=120)
            if row_index == 0:
                base.set_cell_shading(cell, base.TABLE_HEADER)
            paragraph = cell.paragraphs[0]
            paragraph.paragraph_format.space_after = Pt(0)
            paragraph.paragraph_format.line_spacing = 1.05
            value = row[col_index] if col_index < len(row) else ""
            base.add_inline(paragraph, value, 8.2 if cols >= 4 else 8.8)
            if row_index == 0:
                for run in paragraph.runs:
                    run.bold = True
    spacer = doc.add_paragraph()
    spacer.paragraph_format.space_after = Pt(0)


FIGURES = {
    "5.1. Верхний уровень": ("architecture", "Рисунок 1. Владение и главные зависимости движка."),
    "7. Один кадр: фактический timeline": ("frame", "Рисунок 2. Фактическая последовательность фаз кадра."),
    "14.2. Пространства": ("spaces", "Рисунок 3. Преобразование вершины из model space в экран."),
    "19. Фактический render pipeline": ("render", "Рисунок 4. Основные проходы forward-рендера."),
}

ROADMAP_CONTINUATION_DAYS = ("День 3.", "День 12.")


def parse_markdown(doc, markdown, diagrams, roadmap=False):
    lines = markdown.splitlines()
    index = 0
    paragraph_lines = []

    def flush_paragraph():
        nonlocal paragraph_lines
        if paragraph_lines:
            text = " ".join(line.strip() for line in paragraph_lines).strip()
            if text:
                paragraph = doc.add_paragraph()
                base.add_inline(paragraph, text)
            paragraph_lines = []

    first_h1_skipped = False
    skipping_cover_metadata = False
    body_heading_seen = False
    while index < len(lines):
        line = lines[index]
        stripped = line.strip()

        if skipping_cover_metadata:
            if stripped == "---":
                skipping_cover_metadata = False
            index += 1
            continue

        if stripped.startswith("```"):
            flush_paragraph()
            index += 1
            code_lines = []
            while index < len(lines) and not lines[index].strip().startswith("```"):
                code_lines.append(lines[index])
                index += 1
            base.add_code(doc, "\n".join(code_lines))
            index += 1
            continue

        heading = re.match(r"^(#{1,4})\s+(.+)$", stripped)
        if heading:
            flush_paragraph()
            raw_level = len(heading.group(1))
            title = heading.group(2).strip()
            if raw_level == 1 and not first_h1_skipped:
                first_h1_skipped = True
                skipping_cover_metadata = True
                index += 1
                continue
            level = max(1, min(3, raw_level - 1))
            paragraph = doc.add_heading(title, level=level)
            roadmap_break = roadmap and title.startswith("День ") and not title.startswith(ROADMAP_CONTINUATION_DAYS)
            if body_heading_seen and level == 1 and roadmap_break:
                paragraph.paragraph_format.page_break_before = True
            body_heading_seen = True
            figure = FIGURES.get(title)
            if figure and not roadmap:
                key, caption = figure
                add_figure(doc, diagrams[key], caption, caption)
            index += 1
            continue

        if stripped.startswith(">"):
            flush_paragraph()
            quote_lines = []
            while index < len(lines) and lines[index].strip().startswith(">"):
                quote_lines.append(lines[index].strip()[1:].strip())
                index += 1
            base.add_callout(doc, " ".join(quote_lines))
            continue

        if stripped.startswith("|") and index + 1 < len(lines) and base.is_table_separator(lines[index + 1].strip()):
            flush_paragraph()
            rows = [base.split_table_row(stripped)]
            index += 2
            while index < len(lines) and lines[index].strip().startswith("|"):
                rows.append(base.split_table_row(lines[index]))
                index += 1
            add_fixed_table(doc, rows)
            continue

        numbered = re.match(r"^\d+\.\s+(.+)$", stripped)
        bullet = re.match(r"^[-*]\s+(.+)$", stripped)
        if numbered:
            flush_paragraph()
            num_id = base.create_numbered_list_id(doc)
            paragraphs = []
            while index < len(lines):
                current = re.match(r"^\d+\.\s+(.+)$", lines[index].strip())
                if not current:
                    break
                paragraph = doc.add_paragraph(style="List Number")
                base.apply_numbering(paragraph, num_id)
                base.add_inline(paragraph, current.group(1))
                paragraphs.append(paragraph)
                index += 1
            for paragraph in paragraphs[:-1]:
                paragraph.paragraph_format.keep_with_next = True
            continue
        if bullet:
            flush_paragraph()
            paragraphs = []
            while index < len(lines):
                current = re.match(r"^[-*]\s+(.+)$", lines[index].strip())
                if not current:
                    break
                paragraph = doc.add_paragraph(style="List Bullet")
                base.add_inline(paragraph, current.group(1))
                paragraphs.append(paragraph)
                index += 1
            for paragraph in paragraphs[:-1]:
                paragraph.paragraph_format.keep_with_next = True
            continue
        if stripped == "---":
            flush_paragraph()
            index += 1
            continue
        if not stripped:
            flush_paragraph()
            index += 1
            continue
        if not first_h1_skipped:
            index += 1
            continue
        paragraph_lines.append(line)
        index += 1
    flush_paragraph()


def add_static_contents(doc, markdown, roadmap=False):
    doc.add_heading("Содержание", level=1)
    for line in markdown.splitlines():
        match = re.match(r"^##\s+(.+)$", line.strip())
        if not match:
            continue
        title = match.group(1)
        paragraph = doc.add_paragraph(style="List Bullet")
        paragraph.paragraph_format.space_after = Pt(2)
        base.add_inline(paragraph, title, 9.5 if not roadmap else 10)
    spacer = doc.add_paragraph()
    spacer.paragraph_format.page_break_before = True
    spacer.paragraph_format.space_after = Pt(0)


def build_one(source_name, output_name, eyebrow, title, subtitle, version, header, diagrams, roadmap=False):
    source = DOCS / source_name
    output = DOCS / output_name
    markdown = source.read_text(encoding="utf-8")
    doc = Document()
    base.configure_document(doc, header)
    add_caption_style(doc)
    doc.core_properties.title = title
    doc.core_properties.subject = subtitle
    doc.core_properties.author = "DX Framework / Codex"
    doc.core_properties.keywords = "DX Framework, C++20, DirectX 11, ECS, Bullet, engine"
    base.add_cover(doc, eyebrow, title, subtitle, version)
    add_static_contents(doc, markdown, roadmap=roadmap)
    parse_markdown(doc, markdown, diagrams, roadmap=roadmap)
    doc.save(output)
    return output


def main():
    with tempfile.TemporaryDirectory(prefix="dx_engine_docs_") as temporary:
        diagrams = make_diagrams(Path(temporary))
        outputs = [
            build_one(
                "DX_FRAMEWORK_ENGINE_TEXTBOOK_RU.md",
                "DX_FRAMEWORK_ENGINE_TEXTBOOK_RU.docx",
                "Архитектурный учебник и справочник",
                "DX Framework",
                "Как устроен движок: код, математика, рендер, физика, ECS и инженерные решения",
                "Издание 3.1 · ввод обновлён 5 октября 2026 года",
                "DX Framework  |  Учебник по устройству движка",
                diagrams,
            ),
            build_one(
                "DX_FRAMEWORK_ENGINE_ROADMAP_14_DAYS_RU.md",
                "DX_FRAMEWORK_ENGINE_ROADMAP_14_DAYS_RU.docx",
                "Engine-only план развития",
                "14 дней для движка",
                "Сборка, lifecycle, ECS, математика, Bullet, рендер, ресурсы и release gate",
                "Издание 1.0 · 24 августа 2026 года",
                "DX Framework  |  Engine-only roadmap",
                diagrams,
                roadmap=True,
            ),
        ]
    for output in outputs:
        print(output)


if __name__ == "__main__":
    main()
