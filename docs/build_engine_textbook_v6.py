from __future__ import annotations

from pathlib import Path

from docx import Document
from docx.shared import Pt

import build_engine_textbook_v4 as v4
import build_engine_textbook_v5 as v5


DOCS = Path(__file__).resolve().parent
SOURCE = DOCS / "DX_FRAMEWORK_ENGINE_TEXTBOOK_RU_5_0_FULL_COURSE.docx"
OUTPUT = DOCS / "DX_FRAMEWORK_ENGINE_TEXTBOOK_RU_6_0_DEEP_COURSE.docx"


def insert_after_part(doc: Document, part: str, filename: str) -> None:
    heading = v4.find(doc, part)
    paragraphs = doc.paragraphs
    index = next(i for i, paragraph in enumerate(paragraphs) if paragraph._element is heading._element)
    if index + 1 >= len(paragraphs):
        raise ValueError(f"Нет содержимого после {part}")
    v5.insert_file(doc, paragraphs[index + 1], DOCS / filename, skip_first_heading=True)


def main() -> None:
    doc = Document(SOURCE)
    doc.paragraphs[0].text = "DX Framework: полный учебник разработки космического симулятора"
    doc.paragraphs[1].text = "От основ C++ и математики до NPC, Bullet, шейдеров и готового игрового эпизода"
    doc.paragraphs[2].text = "Подробные объяснения, расчёты, кодовые маршруты, упражнения и проверка результата"
    doc.paragraphs[4].text = "Издание 6.0 · 28 октября 2026 года"

    insert_after_part(doc, "Часть I.", "ENGINE_TEXTBOOK_V6_PART_I_RU.md")
    insert_after_part(doc, "Часть II.", "ENGINE_TEXTBOOK_V6_PART_II_RU.md")

    math_end = v4.find(doc, "Часть IV.")
    v4.remove_range(v4.find(doc, "Часть III."), math_end)
    for name in (
        "ENGINE_TEXTBOOK_V6_MATH_A_RU.md",
        "ENGINE_TEXTBOOK_V6_MATH_B_RU.md",
        "ENGINE_TEXTBOOK_V6_MATH_C_RU.md",
    ):
        v5.insert_file(doc, math_end, DOCS / name)

    insert_after_part(doc, "Часть IV.", "ENGINE_TEXTBOOK_V6_PART_IV_RU.md")
    insert_after_part(doc, "Часть V.", "ENGINE_TEXTBOOK_V6_SHIP_RU.md")
    insert_after_part(doc, "Часть V.", "ENGINE_TEXTBOOK_V6_PART_V_RU.md")
    insert_after_part(doc, "Часть VI.", "ENGINE_TEXTBOOK_V6_NPC_FOUNDATION_RU.md")
    insert_after_part(doc, "Часть VII.", "ENGINE_TEXTBOOK_V6_QUALITY_RU.md")
    insert_after_part(doc, "Часть VIII.", "ENGINE_TEXTBOOK_V6_PART_VIII_RU.md")
    insert_after_part(doc, "Часть IX.", "ENGINE_TEXTBOOK_V6_PART_IX_RU.md")
    insert_after_part(doc, "Часть X.", "ENGINE_TEXTBOOK_V6_ATLAS_RU.md")
    insert_after_part(doc, "Часть XI.", "ENGINE_TEXTBOOK_V6_ENGINEERING_RU.md")

    intro = v4.find(doc, "Как пользоваться учебником")
    paragraphs = doc.paragraphs
    intro_index = next(i for i, paragraph in enumerate(paragraphs) if paragraph._element is intro._element)
    paragraphs[intro_index + 1].text = (
        "Книга состоит из последовательных уроков, подробных разборов частей I–XI, "
        "математического задачника, лабораторных работ и справочных карт кода. "
        "Сначала пройди уроки 1–20 на одной учебной сцене. Затем изучай соответствующий "
        "разбор системы, решай задачи вручную и сравнивай числа с debug выводом движка. "
        "Короткие карты в конце каждой части служат для поиска классов после подробного объяснения."
    )
    paragraphs[intro_index + 1].paragraph_format.space_after = Pt(8)

    source_head = doc.add_paragraph(style="Heading 1")
    source_head.add_run("Приложение E. Первоисточники и границы применимости")
    source_head.paragraph_format.page_break_before = True
    for body in (
        "Главный источник описания DX Framework — файлы этого репозитория на 28 октября 2026 года. "
        "В книге действующее поведение и предложенные расширения названы отдельно. "
        "Внешние документы ниже поясняют библиотеки и соглашения; они не доказывают, "
        "что конкретная функция уже подключена в данном проекте.",
        "Microsoft DirectXMath: row-major, row vectors и порядок умножения. "
        "https://learn.microsoft.com/en-us/windows/win32/dxmath/pg-xnamath-getting-started",
        "Microsoft DirectXMath: транспонирование для HLSL constant buffers. "
        "https://learn.microsoft.com/en-us/windows/win32/dxmath/pg-xnamath-optimizing",
        "Microsoft Direct3D 11: vertex/pixel stages и pipeline. "
        "https://learn.microsoft.com/en-us/windows/uwp/gaming/tutorial--assembling-the-rendering-pipeline",
        "Bullet Physics: официальный User Manual и исходный код. "
        "https://github.com/bulletphysics/bullet3/blob/master/docs/Bullet_User_Manual.pdf",
        "Recast Navigation: официальный обзор NavMesh, Recast и Detour. "
        "https://github.com/recastnavigation/recastnavigation/blob/main/Docs/_1_Introduction.md",
    ):
        paragraph = doc.add_paragraph(body)
        paragraph.paragraph_format.space_after = Pt(7)

    v5.update_contents(doc)
    doc.core_properties.title = "DX Framework: полный учебник разработки космического симулятора"
    doc.core_properties.subject = "Подробный курс по архитектуре, математике, рендеру, физике и NPC"
    doc.save(OUTPUT)
    print(OUTPUT)
    print("paragraphs", len(doc.paragraphs), "tables", len(doc.tables))


if __name__ == "__main__":
    main()
