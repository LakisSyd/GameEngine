"""Собирает единое издание 12.0 с полным справочником runtime исходников."""

from __future__ import annotations

import re
from pathlib import Path

from docx import Document
from docx.enum.table import WD_TABLE_ALIGNMENT
from docx.enum.text import WD_ALIGN_PARAGRAPH
from docx.shared import Inches, Pt, RGBColor

import build_engine_textbook_v11 as previous


base = previous.base
DOCS = Path(__file__).resolve().parent
OUTPUT = DOCS / "DX_FRAMEWORK_ENGINE_TEXTBOOK_RU_12_0_RUNTIME_SOURCE_REFERENCE.docx"

CHAPTERS = (
    "ENGINE_TEXTBOOK_V12_PREFACE_RU.md",
    "ENGINE_TEXTBOOK_V9_MATH_FROM_ZERO_RU.md",
    "ENGINE_TEXTBOOK_V11_CPP_MASTER_RU.md",
    "ENGINE_TEXTBOOK_V7_FOUNDATIONS_RU.md",
    "ENGINE_TEXTBOOK_V12_CORE_RUNTIME_LINE_BY_LINE_RU.md",
    "ENGINE_TEXTBOOK_V8_MORE_SYSTEMS_RU.md",
    "ENGINE_TEXTBOOK_V12_ENGINE_GLUE_ANIMATION_LINE_BY_LINE_RU.md",
    "ENGINE_TEXTBOOK_V12_GAMEPLAY_RUNTIME_LINE_BY_LINE_RU.md",
    "ENGINE_TEXTBOOK_V12_WORLD_EFFECTS_RUNTIME_LINE_BY_LINE_RU.md",
    "ENGINE_TEXTBOOK_V8_SHADERS_FROM_ZERO_RU.md",
    "ENGINE_TEXTBOOK_V11_SHADERS_PIPELINE_RU.md",
    "ENGINE_TEXTBOOK_V12_RENDER_RUNTIME_LINE_BY_LINE_RU.md",
    "ENGINE_TEXTBOOK_V11_ASSETS_ANIMATION_STUDIOS_RU.md",
    "ENGINE_TEXTBOOK_V10_BULLET_PHYSICS_RU.md",
    "ENGINE_TEXTBOOK_V12_PHYSICS_GEOMETRY_AI_LINE_BY_LINE_RU.md",
    "ENGINE_TEXTBOOK_V7_NPC_RU.md",
    "ENGINE_TEXTBOOK_V11_AI_ARCHITECTURE_RU.md",
    "ENGINE_TEXTBOOK_V9_ALGORITHMS_RU.md",
    "ENGINE_TEXTBOOK_V8_MATH_ALGORITHMS_RU.md",
    "ENGINE_TEXTBOOK_V10_ENGINEERING_PRACTICES_RU.md",
    "ENGINE_TEXTBOOK_V11_PRODUCTION_GROWTH_RU.md",
    "ENGINE_TEXTBOOK_V12_COMPLETE_RUNTIME_INDEX_RU.md",
    "ENGINE_TEXTBOOK_V9_ROADMAP_RU.md",
)


def add_cover(doc: Document) -> None:
    paragraph = doc.add_paragraph(style="Title")
    paragraph.alignment = WD_ALIGN_PARAGRAPH.CENTER
    paragraph.paragraph_format.space_before = Inches(0.9)
    paragraph.add_run(
        "DX Framework полная книга по движку и построчному разбору runtime"
    )

    paragraph = doc.add_paragraph()
    paragraph.alignment = WD_ALIGN_PARAGRAPH.CENTER
    paragraph.paragraph_format.space_after = Pt(18)
    run = paragraph.add_run("Полное справочное издание 12.0")
    run.font.name = "Aptos Display"
    run.font.size = Pt(17)
    run.font.bold = True
    run.font.color.rgb = base.BLACK

    for value in (
        "C++ математика ECS DirectX 11 HLSL Bullet assets и анимация",
        "Построчный разбор собственного runtime кода и порядка кадра",
        "Оружие физика геометрия комнаты энергия ремонт и эффекты",
        "NPC одиночная тактика cover navigation crew и безопасная архитектура AI",
        "Editor и устаревшая система затопления осознанно исключены",
    ):
        paragraph = doc.add_paragraph(value)
        paragraph.alignment = WD_ALIGN_PARAGRAPH.CENTER
        paragraph.paragraph_format.space_after = Pt(7)

    paragraph = doc.add_paragraph()
    paragraph.alignment = WD_ALIGN_PARAGRAPH.CENTER
    paragraph.paragraph_format.space_before = Inches(1.3)
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
    heading.add_run("Как пользоваться изданием 12.0")

    paragraph = doc.add_paragraph(style="Normal")
    paragraph.add_run(
        "Книга остаётся единым учебником, а новое издание добавляет проверяемый "
        "справочник исходников. Для изучения системы сначала прочитай теорию, затем "
        "открой профильную runtime главу и пройди метод сверху вниз вместе с кодом."
    )

    rows = [
        ("Основа", "Математика и C++", "память типы матрицы лучи время и ошибки"),
        ("Каркас", "Core Application ECS", "инициализация кадр ownership input scene jobs"),
        ("Gameplay", "Игрок оружие мир", "movement damage inventory rooms power repair effects"),
        ("Графика", "DirectX и HLSL", "renderer passes resources shaders PBR skinning и тени"),
        ("Assets", "Импорт и анимация", "Assimp GLB bones clips IK profiles и Studio"),
        ("Физика", "Bullet и geometry", "bodies rays controller mesh cutting и repair"),
        ("AI", "NPC runtime", "perception tactics cover navigation crew и squad layer"),
        ("Проверка", "Полный индекс", "каждый собственный runtime модуль и его глава"),
    ]
    table = doc.add_table(rows=1, cols=3)
    table.alignment = WD_TABLE_ALIGNMENT.CENTER
    base.set_table_borders(table)
    for cell, value in zip(
        table.rows[0].cells, ("Маршрут", "Раздел", "Что станет понятно")
    ):
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
    heading.add_run("Как выглядит построчное чтение")
    paragraph = doc.add_paragraph(style="Normal")
    paragraph.add_run(
        "Для каждого метода отмечай пять вещей: что пришло на вход, где ранний выход, "
        "какие данные прочитаны, кто изменил состояние и какой следующий модуль увидит "
        "результат. Скобки и одинаковые Release объединены, значимые проверки и записи "
        "объясняются в порядке выполнения."
    )

    heading = doc.add_paragraph(style="Heading 2")
    heading.add_run("Граница этого издания")
    paragraph = doc.add_paragraph(style="Normal")
    paragraph.add_run(
        "Editor не разбирается. FloodingSystem не разбирается как рудимент ранней "
        "концепции подлодки. Сохранившиеся полезные room, pressure, power и structural "
        "модули объясняются с пометкой исторических имён."
    )


def set_properties(doc: Document) -> None:
    doc.core_properties.title = (
        "DX Framework полная книга по движку и построчному разбору runtime"
    )
    doc.core_properties.subject = (
        "C++ ECS DirectX 11 HLSL Bullet gameplay animation geometry и NPC AI"
    )
    doc.core_properties.author = "Codex"
    doc.core_properties.comments = (
        "Полное справочное издание 12.0 по снимку DX Framework от 1 ноября 2026 года"
    )


def main() -> None:
    for filename in CHAPTERS:
        if not (DOCS / filename).exists():
            raise FileNotFoundError(DOCS / filename)

    doc = Document()
    base.set_document_styles(doc)
    section = doc.sections[0]
    section.header.paragraphs[0].text = (
        "DX Framework   Полный runtime справочник"
    )
    section.footer.paragraphs[0].clear()
    section.footer.paragraphs[0].alignment = WD_ALIGN_PARAGRAPH.CENTER
    section.footer.paragraphs[0].add_run("Издание 12.0   ")
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
