"""Собирает единое мастер издание 11.0 по текущему DX Framework."""

from __future__ import annotations

import re
from pathlib import Path

from docx import Document
from docx.enum.table import WD_TABLE_ALIGNMENT
from docx.enum.text import WD_ALIGN_PARAGRAPH
from docx.shared import Inches, Pt, RGBColor

import build_engine_textbook_v10 as previous


base = previous.base
DOCS = Path(__file__).resolve().parent
OUTPUT = DOCS / "DX_FRAMEWORK_ENGINE_TEXTBOOK_RU_11_0_MASTER_COURSE.docx"

CHAPTERS = (
    "ENGINE_TEXTBOOK_V11_PREFACE_RU.md",
    "ENGINE_TEXTBOOK_V9_MATH_FROM_ZERO_RU.md",
    "ENGINE_TEXTBOOK_V11_CPP_MASTER_RU.md",
    "ENGINE_TEXTBOOK_V7_FOUNDATIONS_RU.md",
    "ENGINE_TEXTBOOK_V7_ENGINE_SYSTEMS_RU.md",
    "ENGINE_TEXTBOOK_V8_MORE_SYSTEMS_RU.md",
    "ENGINE_TEXTBOOK_V8_SHADERS_FROM_ZERO_RU.md",
    "ENGINE_TEXTBOOK_V11_SHADERS_PIPELINE_RU.md",
    "ENGINE_TEXTBOOK_V11_ASSETS_ANIMATION_STUDIOS_RU.md",
    "ENGINE_TEXTBOOK_V10_BULLET_PHYSICS_RU.md",
    "ENGINE_TEXTBOOK_V7_NPC_RU.md",
    "ENGINE_TEXTBOOK_V11_AI_ARCHITECTURE_RU.md",
    "ENGINE_TEXTBOOK_V9_CODE_WALKTHROUGHS_RU.md",
    "ENGINE_TEXTBOOK_V10_SYSTEM_ATLAS_RU.md",
    "ENGINE_TEXTBOOK_V9_ALGORITHMS_RU.md",
    "ENGINE_TEXTBOOK_V8_MATH_ALGORITHMS_RU.md",
    "ENGINE_TEXTBOOK_V10_ENGINEERING_PRACTICES_RU.md",
    "ENGINE_TEXTBOOK_V11_PRODUCTION_GROWTH_RU.md",
    "ENGINE_TEXTBOOK_V7_SCENE_WORKBOOK_RU.md",
    "CODE_METHOD_MAP.md",
    "ENGINE_TEXTBOOK_V9_ROADMAP_RU.md",
)


def add_cover(doc: Document) -> None:
    paragraph = doc.add_paragraph(style="Title")
    paragraph.alignment = WD_ALIGN_PARAGRAPH.CENTER
    paragraph.paragraph_format.space_before = Inches(1.05)
    paragraph.add_run("DX Framework полная книга по движку C++ графике и игровому AI")

    paragraph = doc.add_paragraph()
    paragraph.alignment = WD_ALIGN_PARAGRAPH.CENTER
    paragraph.paragraph_format.space_after = Pt(18)
    run = paragraph.add_run("Мастер курс издание 11.0")
    run.font.name = "Aptos Display"
    run.font.size = Pt(17)
    run.font.bold = True
    run.font.color.rgb = base.BLACK

    for value in (
        "От языка и математики до DirectX 11 HLSL Bullet и ECS",
        "Импорт GLB скелетная анимация IK Animation Studio и NPC Studio",
        "Боевой AI без всезнания и конфликтов систем",
        "Бегство поиск оружия сдача пленение тактика и развитие движка",
    ):
        paragraph = doc.add_paragraph(value)
        paragraph.alignment = WD_ALIGN_PARAGRAPH.CENTER
        paragraph.paragraph_format.space_after = Pt(7)

    paragraph = doc.add_paragraph()
    paragraph.alignment = WD_ALIGN_PARAGRAPH.CENTER
    paragraph.paragraph_format.space_before = Inches(1.55)
    run = paragraph.add_run("Собрано по текущему снимку репозитория")
    run.font.size = Pt(9.5)
    run.font.color.rgb = base.MUTED

    paragraph = doc.add_paragraph()
    paragraph.alignment = WD_ALIGN_PARAGRAPH.CENTER
    run = paragraph.add_run("1 ноября 2026 года")
    run.font.size = Pt(9.5)
    run.font.color.rgb = base.MUTED
    doc.add_page_break()


def add_book_map(doc: Document) -> None:
    heading = doc.add_paragraph(style="Heading 1")
    heading.add_run("Как пройти мастер курс")

    paragraph = doc.add_paragraph(style="Normal")
    paragraph.add_run(
        "Это одна книга, но читать её можно разными маршрутами. Полный путь идёт "
        "от математики и C++ через кадр движка, графику, физику и assets к NPC. "
        "Если сейчас хочется только спокойно читать, выбирай один маршрут и после "
        "каждого раздела отвечай: кто владеет данными, кто их меняет и как увидеть результат."
    )

    rows = [
        ("Основа", "Математика и C++", "типы память матрицы лучи время и ECS"),
        ("Движок", "Кадр и gameplay", "сцена ввод ресурсы физика оружие урон"),
        ("Графика", "DirectX и HLSL", "pipeline PBR buffers skinning тени и shader contract"),
        ("Assets", "GLB и анимация", "Assimp кости клипы IK первая персона и профили"),
        ("Инструменты", "Animation и NPC Studio", "authoring snapshots события pause step и export"),
        ("AI", "Решение и исполнение", "память goals actions reservations мораль и пленение"),
        ("Рост", "Практика и roadmap", "тесты profiling assets migrations и новые системы"),
        ("Справочник", "Атлас текущего кода", "файлы методы владельцы и точки расширения"),
    ]
    table = doc.add_table(rows=1, cols=3)
    table.alignment = WD_TABLE_ALIGNMENT.CENTER
    base.set_table_borders(table)
    headers = ("Маршрут", "Раздел", "Что станет понятно")
    for cell, value in zip(table.rows[0].cells, headers):
        cell.text = value
        base.set_cell_shading(cell, base.NAVY)
        base.set_cell_margins(cell)
        for run in cell.paragraphs[0].runs:
            run.font.color.rgb = RGBColor(255, 255, 255)
            run.font.bold = True
            run.font.size = Pt(8.7)
    base.set_repeat_table_header(table.rows[0])
    base.keep_table_row_together(table.rows[0])
    for index, values in enumerate(rows, start=1):
        cells = table.add_row().cells
        base.keep_table_row_together(table.rows[-1])
        for cell, value in zip(cells, values):
            base.set_cell_margins(cell)
            base.set_cell_shading(cell, "FFFFFF" if index % 2 else base.LIGHT_GRAY)
            cell.text = value
            for run in cell.paragraphs[0].runs:
                run.font.size = Pt(8.5)

    heading = doc.add_paragraph(style="Heading 2")
    heading.add_run("Маршрут только по новым главам")
    paragraph = doc.add_paragraph(style="Normal")
    paragraph.add_run(
        "Сначала прочитай C++ для разработчика движка. Затем пройди графический "
        "конвейер, импорт и Studio. После этого переходи к архитектуре AI и главе "
        "о росте проекта. Старые главы остаются подробным фундаментом и справочником."
    )

    heading = doc.add_paragraph(style="Heading 2")
    heading.add_run("Правило практики")
    paragraph = doc.add_paragraph(style="Normal")
    paragraph.add_run(
        "Сначала напиши прогноз, затем наблюдай факт, после этого ищи первую стадию, "
        "где значение изменилось не так. Не меняй одновременно решение, исполнение и "
        "анимацию: иначе невозможно понять, какая гипотеза была верной."
    )


def set_properties(doc: Document) -> None:
    doc.core_properties.title = (
        "DX Framework полная книга по движку C++ графике и игровому AI"
    )
    doc.core_properties.subject = (
        "C++ ECS DirectX 11 HLSL GLB Assimp анимация Bullet NPC Studio и архитектура AI"
    )
    doc.core_properties.author = "Codex"
    doc.core_properties.comments = (
        "Мастер курс издание 11.0 по снимку DX Framework от 1 ноября 2026 года"
    )


def main() -> None:
    for filename in CHAPTERS:
        if not (DOCS / filename).exists():
            raise FileNotFoundError(DOCS / filename)

    doc = Document()
    base.set_document_styles(doc)
    section = doc.sections[0]
    section.header.paragraphs[0].text = "DX Framework   Мастер курс по текущему коду"
    section.footer.paragraphs[0].clear()
    section.footer.paragraphs[0].alignment = WD_ALIGN_PARAGRAPH.CENTER
    section.footer.paragraphs[0].add_run("Издание 11.0   ")
    base.add_page_field(section.footer.paragraphs[0])
    for run in section.footer.paragraphs[0].runs:
        run.font.name = "Aptos"
        run.font.size = Pt(8)
        run.font.color.rgb = base.MUTED

    add_cover(doc)
    add_book_map(doc)
    for filename in CHAPTERS:
        base.add_markdown(doc, filename)
    set_properties(doc)
    doc.save(OUTPUT)

    word_count = sum(
        len(re.findall(r"\b[\w-]+\b", (DOCS / filename).read_text(encoding="utf-8")))
        for filename in CHAPTERS
    )
    print(OUTPUT)
    print(
        f"paragraphs={len(doc.paragraphs)} tables={len(doc.tables)} "
        f"words_approx={word_count}"
    )


if __name__ == "__main__":
    main()

