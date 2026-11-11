from __future__ import annotations

import re
from pathlib import Path

from docx import Document
from docx.enum.style import WD_STYLE_TYPE
from docx.oxml import OxmlElement
from docx.oxml.ns import qn
from docx.shared import Pt, RGBColor

import build_engine_textbook_v4 as previous


DOCS = Path(__file__).resolve().parent
SOURCE = DOCS / "DX_FRAMEWORK_ENGINE_TEXTBOOK_RU_4_0.docx"
OUTPUT = DOCS / "DX_FRAMEWORK_ENGINE_TEXTBOOK_RU_5_0_FULL_COURSE.docx"
COURSE_FILES = (
    "ENGINE_TEXTBOOK_V5_COURSE_1_RU.md",
    "ENGINE_TEXTBOOK_V5_COURSE_2_RU.md",
    "ENGINE_TEXTBOOK_V5_COURSE_3_RU.md",
    "ENGINE_TEXTBOOK_V5_COURSE_4_RU.md",
    "ENGINE_TEXTBOOK_V5_PRODUCTION_RU.md",
    "ENGINE_TEXTBOOK_V5_MATH_WORKBOOK_RU.md",
    "ENGINE_TEXTBOOK_V5_WORKBOOK_RU.md",
)


def code_style(doc: Document) -> None:
    if "CourseCode" in doc.styles:
        return
    style = doc.styles.add_style("CourseCode", WD_STYLE_TYPE.PARAGRAPH)
    style.base_style = doc.styles["Normal"]
    style.font.name = "Consolas"
    style.font.size = Pt(8.5)
    style.font.color.rgb = RGBColor(32, 42, 54)
    style.paragraph_format.left_indent = Pt(12)
    style.paragraph_format.space_before = Pt(0)
    style.paragraph_format.space_after = Pt(0)
    style.paragraph_format.line_spacing = 1.0


def add_inline(paragraph, text: str) -> None:
    for index, chunk in enumerate(text.split("`")):
        if not chunk:
            continue
        run = paragraph.add_run(chunk)
        run.font.size = Pt(9.2 if index % 2 else 11)
        if index % 2:
            run.font.name = "Consolas"


def insert_file(doc: Document, anchor, path: Path, skip_first_heading: bool = False) -> None:
    in_code = False
    first_heading_skipped = False
    for raw in path.read_text(encoding="utf-8").splitlines():
        line = raw.rstrip()
        if skip_first_heading and not first_heading_skipped and line.startswith("# "):
            first_heading_skipped = True
            continue
        if line.startswith("```"):
            in_code = not in_code
            continue
        if in_code:
            p = anchor.insert_paragraph_before(style="CourseCode")
            p.add_run(line or " ")
            continue
        if not line.strip():
            continue
        if line.startswith("# "):
            p = anchor.insert_paragraph_before(style="Heading 1")
            p.add_run(line[2:])
            p.paragraph_format.page_break_before = True
            p.paragraph_format.keep_with_next = True
        elif line.startswith("## "):
            p = anchor.insert_paragraph_before(style="Heading 1")
            p.add_run(line[3:])
            p.paragraph_format.keep_with_next = True
        elif line.startswith("### "):
            p = anchor.insert_paragraph_before(style="Heading 2")
            p.add_run(line[4:])
            p.paragraph_format.keep_with_next = True
        elif line.startswith("- "):
            p = anchor.insert_paragraph_before(style="List Bullet")
            add_inline(p, line[2:])
        else:
            p = anchor.insert_paragraph_before(style="Normal")
            add_inline(p, line)
            p.paragraph_format.space_after = Pt(7)
            p.paragraph_format.line_spacing = 1.15
            p.paragraph_format.widow_control = True
    if in_code:
        raise ValueError(f"Незакрытый code block: {path}")


def update_contents(doc: Document) -> None:
    start = previous.find(doc, "Содержание")
    end = previous.find(doc, "Как пользоваться учебником")
    parent = start._element.getparent()
    children = list(parent)
    a = children.index(start._element) + 1
    b = children.index(end._element)
    for child in children[a:b]:
        parent.remove(child)
    entries = []
    for p in doc.paragraphs:
        if p.style.name != "Heading 1":
            continue
        if p.text.startswith(("Учебный курс", "Производственная часть", "Математический задачник", "Математическая мастерская", "Практикум создания", "Часть ", "Приложение ", "Урок ", "Задача ", "Лабораторная ", "Разбор ")) or re.match(r"^\d+\. ", p.text):
            entries.append(p.text)
    for entry in entries:
        p = end.insert_paragraph_before(style="Normal")
        p.add_run(entry)
        p.paragraph_format.left_indent = Pt(0 if entry.startswith(("Учебный курс", "Производственная часть", "Математический задачник", "Практикум создания", "Часть ")) else 16)
        p.paragraph_format.space_after = Pt(1.5)
        p.runs[0].font.size = Pt(9)


def main() -> None:
    doc = Document(SOURCE)
    code_style(doc)
    doc.paragraphs[0].text = "DX Framework полный учебник создания космического симулятора"
    doc.paragraphs[1].text = "Пошаговый курс по архитектуре, математике, Bullet, DirectX 11, NPC и разработке игры"
    doc.paragraphs[2].text = "Сквозной проект от первой сцены до проверяемой релизной сборки"
    doc.paragraphs[4].text = "Издание 5.0 · сверено с исходным кодом 28 октября 2026 года"

    first_part = previous.find(doc, "Часть I.")
    previous.remove_range(previous.find(doc, "Как читать это издание"), first_part)
    intro = first_part.insert_paragraph_before(style="Heading 1")
    intro.add_run("Как пользоваться учебником")
    intro.paragraph_format.page_break_before = True
    for text in (
        "Сначала проходи уроки 1–20 по порядку, решай математические задачи и выполняй лабораторные работы на одной небольшой сцене. Это связанный учебный курс: каждая новая механика использует решения предыдущих уроков. Затем обращайся к частям I–XII как к подробному справочнику классов, формул, систем и текущего кода.",
        "Код в блоках курса иногда является учебным псевдокодом; рядом объясняется, что нужно связать с реальными типами проекта. Имена существующих файлов и методов указаны явно. Реализацию проверяй по состоянию репозитория: старые практикумы о групповом бое описывают возможное развитие, а вызов NpcSquadCombatSystem сейчас отключён.",
        "После учебного курса выполни выпускную работу и пройди единственный четырнадцатидневный план в конце книги. План посвящён улучшению NPC и недостающих связей космического симулятора. Для внешнего игрока оценивай не только компиляцию, но и понятность целей, сохранение, работу без редактора и запуск вне исходного каталога.",
    ):
        p = first_part.insert_paragraph_before(style="Normal")
        p.add_run(text)
        p.paragraph_format.space_after = Pt(8)

    for filename in COURSE_FILES:
        insert_file(doc, first_part, DOCS / filename)
    first_part.paragraph_format.page_break_before = True
    roadmap_end = previous.find(doc, "Приложение A.")
    previous.remove_range(previous.find(doc, "Часть XII."), roadmap_end)
    insert_file(doc, roadmap_end, DOCS / "ENGINE_TEXTBOOK_V5_ROADMAP_RU.md")
    update_contents(doc)
    doc.core_properties.title = "DX Framework полный учебник создания космического симулятора"
    doc.core_properties.subject = "Учебный курс и справочник движка DX Framework"
    doc.save(OUTPUT)
    print(OUTPUT)
    print("paragraphs", len(doc.paragraphs), "tables", len(doc.tables))


if __name__ == "__main__":
    main()
