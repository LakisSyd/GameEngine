"""Собирает единое учебное издание 10.0 по текущему DX Framework."""

from __future__ import annotations

import re
from pathlib import Path

from docx import Document
from docx.enum.table import WD_TABLE_ALIGNMENT
from docx.enum.text import WD_ALIGN_PARAGRAPH
from docx.shared import Inches, Pt, RGBColor

import build_engine_textbook_v9 as base


DOCS = Path(__file__).resolve().parent
OUTPUT = DOCS / "DX_FRAMEWORK_ENGINE_TEXTBOOK_RU_10_0_COMPLETE_EDITION.docx"

CHAPTERS = (
    "ENGINE_TEXTBOOK_V10_PREFACE_RU.md",
    "ENGINE_TEXTBOOK_V9_MATH_FROM_ZERO_RU.md",
    "ENGINE_TEXTBOOK_V7_FOUNDATIONS_RU.md",
    "ENGINE_TEXTBOOK_V7_ENGINE_SYSTEMS_RU.md",
    "ENGINE_TEXTBOOK_V8_MORE_SYSTEMS_RU.md",
    "ENGINE_TEXTBOOK_V8_SHADERS_FROM_ZERO_RU.md",
    "ENGINE_TEXTBOOK_V10_BULLET_PHYSICS_RU.md",
    "ENGINE_TEXTBOOK_V7_NPC_RU.md",
    "ENGINE_TEXTBOOK_V9_CODE_WALKTHROUGHS_RU.md",
    "ENGINE_TEXTBOOK_V10_SYSTEM_ATLAS_RU.md",
    "ENGINE_TEXTBOOK_V9_ALGORITHMS_RU.md",
    "ENGINE_TEXTBOOK_V8_MATH_ALGORITHMS_RU.md",
    "ENGINE_TEXTBOOK_V10_ENGINEERING_PRACTICES_RU.md",
    "ENGINE_TEXTBOOK_V7_SCENE_WORKBOOK_RU.md",
    "CODE_METHOD_MAP.md",
    "ENGINE_TEXTBOOK_V9_ROADMAP_RU.md",
)


def add_cover(doc: Document) -> None:
    paragraph = doc.add_paragraph(style="Title")
    paragraph.alignment = WD_ALIGN_PARAGRAPH.CENTER
    paragraph.paragraph_format.space_before = Inches(1.25)
    paragraph.add_run("DX Framework единая книга по движку и игровому AI")

    paragraph = doc.add_paragraph()
    paragraph.alignment = WD_ALIGN_PARAGRAPH.CENTER
    paragraph.paragraph_format.space_after = Pt(18)
    run = paragraph.add_run("Полное учебное издание 10.0")
    run.font.name = "Aptos Display"
    run.font.size = Pt(17)
    run.font.bold = True
    run.font.color.rgb = base.BLACK

    for value in (
        "Математика C++ ECS DirectX 11 Bullet Physics и игровые системы",
        "Построчное чтение ключевых путей текущего исходного кода",
        "NPC от восприятия и укрытий до оружия урона и расширяемых тактик",
        "Алгоритмы инженерные практики задания и маршрут самостоятельной работы",
    ):
        paragraph = doc.add_paragraph(value)
        paragraph.alignment = WD_ALIGN_PARAGRAPH.CENTER
        paragraph.paragraph_format.space_after = Pt(7)

    paragraph = doc.add_paragraph()
    paragraph.alignment = WD_ALIGN_PARAGRAPH.CENTER
    paragraph.paragraph_format.space_before = Inches(1.8)
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
    heading.add_run("Маршрут по единой книге")

    paragraph = doc.add_paragraph(style="Normal")
    paragraph.add_run(
        "Книга устроена слоями. Сначала изучи язык данных и математику. Затем "
        "пройди один кадр движка, физику и игровые системы. После этого переходи "
        "к NPC, алгоритмам и собственным модулям. Атлас в конце помогает быстро "
        "вернуться к нужному классу после перерыва."
    )

    rows = [
        ("1", "Основа", "C++ ECS векторы матрицы лучи время"),
        ("2", "Движок", "кадр сцена ресурсы ввод DirectX и шейдеры"),
        ("3", "Bullet", "мир формы тела фильтры запросы контроллеры constraints"),
        ("4", "Gameplay", "энергия атмосфера двери инвентарь ремонт эффекты"),
        ("5", "NPC", "решение навигация укрытие оружие урон и тактики"),
        ("6", "Практика", "алгоритмы качество тесты диагностика и roadmap"),
        ("7", "Атлас", "карта файлов методов владельцев и точек расширения"),
    ]
    table = doc.add_table(rows=1, cols=3)
    table.alignment = WD_TABLE_ALIGNMENT.CENTER
    base.set_table_borders(table)
    headers = ("Этап", "Раздел", "Результат")
    for cell, value in zip(table.rows[0].cells, headers):
        cell.text = value
        base.set_cell_shading(cell, base.NAVY)
        base.set_cell_margins(cell)
        for run in cell.paragraphs[0].runs:
            run.font.color.rgb = RGBColor(255, 255, 255)
            run.font.bold = True
            run.font.size = Pt(8.8)
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
                run.font.size = Pt(8.7)

    heading = doc.add_paragraph(style="Heading 2")
    heading.add_run("Как проверять понимание")
    paragraph = doc.add_paragraph(style="Normal")
    paragraph.add_run(
        "Перед запуском напиши прогноз. После запуска сохрани наблюдаемый факт. "
        "Если они различаются, ищи первую строку цепочки, где значение изменилось "
        "не так, как ожидалось. Именно это превращает чтение в инженерный навык."
    )


def set_properties(doc: Document) -> None:
    doc.core_properties.title = "DX Framework единая книга по движку и игровому AI"
    doc.core_properties.subject = (
        "C++ ECS DirectX 11 Bullet Physics игровые системы NPC алгоритмы и практика"
    )
    doc.core_properties.author = "Codex"
    doc.core_properties.comments = (
        "Полное учебное издание 10.0 по снимку DX Framework от 1 ноября 2026 года"
    )


def main() -> None:
    for filename in CHAPTERS:
        if not (DOCS / filename).exists():
            raise FileNotFoundError(DOCS / filename)

    doc = Document()
    base.set_document_styles(doc)
    section = doc.sections[0]
    section.header.paragraphs[0].text = "DX Framework   Единая книга по текущему коду"
    section.footer.paragraphs[0].clear()
    section.footer.paragraphs[0].alignment = WD_ALIGN_PARAGRAPH.CENTER
    section.footer.paragraphs[0].add_run("Издание 10.0   ")
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

    word_count = 0
    for filename in CHAPTERS:
        word_count += len(re.findall(r"\b[\w-]+\b", (DOCS / filename).read_text(encoding="utf-8")))
    print(OUTPUT)
    print(f"paragraphs={len(doc.paragraphs)} tables={len(doc.tables)} words_approx={word_count}")


if __name__ == "__main__":
    main()

