"""Собирает самостоятельное учебное издание 7.0 из проверяемых глав Markdown.

Главы прошлых версий здесь служат проектным курсом и практикумом. Подробные
разборы текущего кода и математика для начинающего идут отдельными главами.
"""

from __future__ import annotations

import re
from pathlib import Path

from docx import Document
from docx.enum.style import WD_STYLE_TYPE
from docx.enum.table import WD_TABLE_ALIGNMENT
from docx.enum.text import WD_ALIGN_PARAGRAPH
from docx.oxml import OxmlElement
from docx.oxml.ns import qn
from docx.shared import Cm, Pt, RGBColor


DOCS = Path(__file__).resolve().parent
OUTPUT = DOCS / "DX_FRAMEWORK_ENGINE_TEXTBOOK_RU_7_0_SOURCE_COURSE.docx"

# Единственный календарный план — последний файл. Остальные главы не содержат
# отдельного расписания разработки.
CHAPTERS = (
    ("ENGINE_TEXTBOOK_V7_PREFACE_RU.md", None),
    ("ENGINE_TEXTBOOK_V7_FOUNDATIONS_RU.md", None),
    ("ENGINE_TEXTBOOK_V7_MATH_RU.md", None),
    ("ENGINE_TEXTBOOK_V5_COURSE_1_RU.md", "Практический курс: от первого кадра до своей сцены"),
    ("ENGINE_TEXTBOOK_V5_COURSE_2_RU.md", None),
    ("ENGINE_TEXTBOOK_V5_COURSE_3_RU.md", None),
    ("ENGINE_TEXTBOOK_V5_COURSE_4_RU.md", None),
    ("ENGINE_TEXTBOOK_V7_ENGINE_SYSTEMS_RU.md", None),
    ("ENGINE_TEXTBOOK_V6_RENDER_PHYSICS_RU.md", "Рендер и физика: объяснение от данных до кадра"),
    ("ENGINE_TEXTBOOK_V6_SHIP_RU.md", "Корабельные сети и среда: углублённый разбор"),
    ("ENGINE_TEXTBOOK_V7_NPC_RU.md", None),
    ("ENGINE_TEXTBOOK_V7_SCENE_WORKBOOK_RU.md", None),
    ("ENGINE_TEXTBOOK_V6_ENGINEERING_RU.md", "Новая механика и дорога к выпуску"),
    ("ENGINE_TEXTBOOK_V5_PRODUCTION_RU.md", None),
    ("ENGINE_TEXTBOOK_V5_MATH_WORKBOOK_RU.md", None),
    ("ENGINE_TEXTBOOK_V5_WORKBOOK_RU.md", None),
    ("ENGINE_TEXTBOOK_V5_ROADMAP_RU.md", "Единственный план развития: 14 дней"),
)


def shade(paragraph, fill: str) -> None:
    p_pr = paragraph._p.get_or_add_pPr()
    shd = OxmlElement("w:shd")
    shd.set(qn("w:fill"), fill)
    p_pr.append(shd)


def add_field(paragraph, instruction: str, placeholder: str = "") -> None:
    run = paragraph.add_run()
    start = OxmlElement("w:fldChar")
    start.set(qn("w:fldCharType"), "begin")
    run._r.append(start)
    run = paragraph.add_run()
    instr = OxmlElement("w:instrText")
    instr.set(qn("xml:space"), "preserve")
    instr.text = instruction
    run._r.append(instr)
    run = paragraph.add_run()
    sep = OxmlElement("w:fldChar")
    sep.set(qn("w:fldCharType"), "separate")
    run._r.append(sep)
    if placeholder:
        paragraph.add_run(placeholder)
    run = paragraph.add_run()
    end = OxmlElement("w:fldChar")
    end.set(qn("w:fldCharType"), "end")
    run._r.append(end)


def set_styles(doc: Document) -> None:
    section = doc.sections[0]
    section.page_width = Cm(21)
    section.page_height = Cm(29.7)
    section.top_margin = Cm(2.2)
    section.bottom_margin = Cm(2.0)
    section.left_margin = Cm(2.3)
    section.right_margin = Cm(2.0)
    section.header_distance = Cm(1.0)
    section.footer_distance = Cm(1.0)

    normal = doc.styles["Normal"]
    normal.font.name = "Georgia"
    normal.font.size = Pt(10.3)
    normal.font.color.rgb = RGBColor(34, 43, 55)
    normal.paragraph_format.space_after = Pt(6)
    normal.paragraph_format.line_spacing = 1.18
    normal.paragraph_format.widow_control = True

    for name, size, before, after in (
        ("Title", 26, 0, 18),
        ("Heading 1", 18, 0, 13),
        ("Heading 2", 13.2, 18, 7),
        ("Heading 3", 10.8, 12, 4),
    ):
        style = doc.styles[name]
        style.font.name = "Arial"
        style.font.size = Pt(size)
        style.font.bold = True
        style.font.color.rgb = RGBColor(17, 55, 84)
        style.paragraph_format.space_before = Pt(before)
        style.paragraph_format.space_after = Pt(after)
        style.paragraph_format.keep_with_next = True
        style.paragraph_format.widow_control = True

    if "TextbookCode" not in doc.styles:
        code = doc.styles.add_style("TextbookCode", WD_STYLE_TYPE.PARAGRAPH)
        code.base_style = normal
        code.font.name = "Consolas"
        code.font.size = Pt(8.0)
        code.font.color.rgb = RGBColor(26, 41, 58)
        code.paragraph_format.left_indent = Cm(0.35)
        code.paragraph_format.right_indent = Cm(0.15)
        code.paragraph_format.space_before = Pt(0)
        code.paragraph_format.space_after = Pt(0)
        code.paragraph_format.line_spacing = 1.0
    if "TextbookNote" not in doc.styles:
        note = doc.styles.add_style("TextbookNote", WD_STYLE_TYPE.PARAGRAPH)
        note.base_style = normal
        note.font.italic = True
        note.font.size = Pt(9.4)
        note.paragraph_format.left_indent = Cm(0.5)
        note.paragraph_format.right_indent = Cm(0.3)
        note.paragraph_format.space_after = Pt(7)

    header = section.header.paragraphs[0]
    header.text = "DX FRAMEWORK  ·  УЧЕБНИК ПО ИСХОДНОМУ КОДУ"
    header.alignment = WD_ALIGN_PARAGRAPH.RIGHT
    header.runs[0].font.name = "Arial"
    header.runs[0].font.size = Pt(8)
    header.runs[0].font.color.rgb = RGBColor(105, 118, 130)
    footer = section.footer.paragraphs[0]
    footer.alignment = WD_ALIGN_PARAGRAPH.CENTER
    footer.add_run("Издание 7.0   ·   ")
    add_field(footer, "PAGE", "1")
    for run in footer.runs:
        run.font.name = "Arial"
        run.font.size = Pt(8)
        run.font.color.rgb = RGBColor(105, 118, 130)


INLINE = re.compile(r"(`[^`]+`|\*\*[^*]+\*\*|\[[^]]+\]\([^)]+\))")


def inline(paragraph, value: str) -> None:
    for piece in INLINE.split(value):
        if not piece:
            continue
        if piece.startswith("`") and piece.endswith("`"):
            run = paragraph.add_run(piece[1:-1])
            run.font.name = "Consolas"
            run.font.size = Pt(8.7)
            run.font.color.rgb = RGBColor(22, 87, 120)
        elif piece.startswith("**") and piece.endswith("**"):
            paragraph.add_run(piece[2:-2]).bold = True
        elif piece.startswith("[") and "](" in piece:
            label, url = piece[1:-1].split("](", 1)
            run = paragraph.add_run(f"{label} ({url})")
            run.font.color.rgb = RGBColor(22, 87, 120)
        else:
            paragraph.add_run(piece)


def add_table(doc: Document, rows: list[str]) -> None:
    parsed = [[cell.strip() for cell in row.strip().strip("|").split("|")] for row in rows]
    if len(parsed) > 1 and all(re.fullmatch(r":?-{3,}:?", cell) for cell in parsed[1]):
        parsed.pop(1)
    if not parsed:
        return
    width = max(len(row) for row in parsed)
    table = doc.add_table(rows=0, cols=width)
    table.style = "Light Shading Accent 1"
    table.alignment = WD_TABLE_ALIGNMENT.CENTER
    for cells in parsed:
        row = table.add_row()
        for i, value in enumerate(cells):
            inline(row.cells[i].paragraphs[0], value)
    doc.add_paragraph()


def add_markdown(doc: Document, filename: str, override_title: str | None = None) -> None:
    path = DOCS / filename
    if not path.exists():
        raise FileNotFoundError(path)
    lines = path.read_text(encoding="utf-8").splitlines()
    in_code = False
    table_rows: list[str] = []
    first_heading = True
    if override_title and not any(line.startswith("# ") for line in lines):
        p = doc.add_paragraph(style="Heading 1")
        p.add_run(override_title)
        p.paragraph_format.page_break_before = True
        first_heading = False
    for raw in lines:
        line = raw.rstrip()
        if line.startswith("```"):
            if table_rows:
                add_table(doc, table_rows)
                table_rows.clear()
            in_code = not in_code
            continue
        if in_code:
            p = doc.add_paragraph(style="TextbookCode")
            p.add_run(line if line else " ")
            shade(p, "F1F5F8")
            continue
        if line.startswith("|") and line.endswith("|"):
            table_rows.append(line)
            continue
        if table_rows:
            add_table(doc, table_rows)
            table_rows.clear()
        if not line.strip():
            continue
        if line.startswith("# "):
            title = override_title if first_heading and override_title else line[2:]
            if filename == "ENGINE_TEXTBOOK_V6_RENDER_PHYSICS_RU.md" and line.startswith("# Часть V."):
                title = "Bullet Physics и симуляция мира: углублённый разбор"
            p = doc.add_paragraph(style="Heading 1")
            p.add_run(title)
            p.paragraph_format.page_break_before = True
            first_heading = False
        elif line.startswith("## "):
            if first_heading and override_title:
                p = doc.add_paragraph(style="Heading 1")
                p.add_run(override_title)
                p.paragraph_format.page_break_before = True
                first_heading = False
            p = doc.add_paragraph(style="Heading 2")
            p.add_run(line[3:])
            if filename == "ENGINE_TEXTBOOK_V5_ROADMAP_RU.md":
                p.paragraph_format.space_before = Pt(12)
        elif line.startswith("### "):
            p = doc.add_paragraph(style="Heading 3")
            p.add_run(line[4:])
        elif line.startswith("> "):
            p = doc.add_paragraph(style="TextbookNote")
            inline(p, line[2:])
        elif re.match(r"^\s*- ", line):
            p = doc.add_paragraph(style="List Bullet")
            inline(p, line.lstrip()[2:])
            p.paragraph_format.space_after = Pt(3)
        elif re.match(r"^\s*\d+\. ", line):
            p = doc.add_paragraph(style="List Number")
            inline(p, re.sub(r"^\s*\d+\. ", "", line))
            p.paragraph_format.space_after = Pt(3)
        else:
            p = doc.add_paragraph(style="Normal")
            inline(p, line)
    if table_rows:
        add_table(doc, table_rows)
    if in_code:
        raise ValueError(f"Unclosed code block: {path}")


def add_cover(doc: Document) -> None:
    p = doc.add_paragraph(style="Title")
    p.add_run("DX Framework")
    p.alignment = WD_ALIGN_PARAGRAPH.CENTER
    p.paragraph_format.space_before = Cm(4.5)
    p = doc.add_paragraph()
    p.alignment = WD_ALIGN_PARAGRAPH.CENTER
    run = p.add_run("Большой учебник по своему движку и космическому симулятору")
    run.font.name = "Arial"
    run.font.size = Pt(19)
    run.font.bold = True
    run.font.color.rgb = RGBColor(17, 55, 84)
    p.paragraph_format.space_after = Cm(1.2)
    for value in (
        "Математика с нуля · исходный C++ код · ECS · NPC и ремонт · стрельба и пули",
        "Bullet Physics · DirectX 11 · вершинные и пиксельные шейдеры · системы корабля",
        "Практические работы · выпускная сцена · один план развития на 14 дней",
    ):
        p = doc.add_paragraph(value)
        p.alignment = WD_ALIGN_PARAGRAPH.CENTER
        p.paragraph_format.space_after = Pt(9)
    p = doc.add_paragraph()
    p.alignment = WD_ALIGN_PARAGRAPH.CENTER
    p.paragraph_format.space_before = Cm(5)
    p.add_run("Издание 7.0 · 29 октября 2026 года")
    p = doc.add_paragraph()
    p.alignment = WD_ALIGN_PARAGRAPH.CENTER
    p.add_run("Подготовлено по исходникам текущего репозитория DX Framework")
    doc.add_page_break()


def add_toc(doc: Document) -> None:
    p = doc.add_paragraph()
    run = p.add_run("Содержание")
    run.font.name = "Arial"
    run.font.size = Pt(18)
    run.font.bold = True
    run.font.color.rgb = RGBColor(17, 55, 84)
    p = doc.add_paragraph()
    add_field(p, 'TOC \\o "1-2" \\h \\z \\u', "Обновите поле оглавления в Word")
    p.paragraph_format.space_after = Pt(12)
    p = doc.add_paragraph(style="TextbookNote")
    p.add_run("В PDF оглавление обновляется при экспорте. В Word его можно обновить клавишей F9.")
    doc.add_page_break()


def main() -> None:
    doc = Document()
    set_styles(doc)
    add_cover(doc)
    add_toc(doc)
    for filename, override in CHAPTERS:
        add_markdown(doc, filename, override)
    doc.core_properties.title = "DX Framework: большой учебник по исходному коду и разработке космического симулятора"
    doc.core_properties.subject = "Математика, ECS, DirectX 11, Bullet Physics, NPC, ремонт, бой и выпуск игры"
    doc.core_properties.author = "Codex"
    doc.save(OUTPUT)
    word_count = sum(len(re.findall(r"\b[\w-]+\b", (DOCS / file).read_text(encoding="utf-8"))) for file, _ in CHAPTERS)
    print(OUTPUT)
    print("paragraphs", len(doc.paragraphs), "tables", len(doc.tables), "words_approx", word_count)


if __name__ == "__main__":
    main()
