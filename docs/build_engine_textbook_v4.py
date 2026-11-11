from __future__ import annotations

import re
from pathlib import Path

from docx import Document
from docx.enum.style import WD_STYLE_TYPE
from docx.oxml import OxmlElement
from docx.oxml.ns import qn
from docx.shared import Pt, RGBColor


ROOT = Path(__file__).resolve().parents[1]
DOCS = ROOT / "docs"
SOURCE = ROOT.parent / "DX_FrameWork_Book_RU_3_4.docx"
OUTPUT = DOCS / "DX_FRAMEWORK_ENGINE_TEXTBOOK_RU_4_0.docx"


def find(doc: Document, prefix: str):
    for paragraph in doc.paragraphs:
        if paragraph.style.name == "Heading 1" and paragraph.text.startswith(prefix):
            return paragraph
    raise ValueError(f"Не найден раздел: {prefix}")


def remove_range(start, end) -> None:
    parent = start._element.getparent()
    children = list(parent)
    first = children.index(start._element)
    last = children.index(end._element)
    for child in children[first:last]:
        parent.remove(child)


def add_inline(paragraph, text: str) -> None:
    for i, chunk in enumerate(text.split("`")):
        if not chunk:
            continue
        run = paragraph.add_run(chunk)
        if i % 2:
            run.font.name = "Consolas"
            run.font.size = Pt(9)


def insert_markdown(doc: Document, anchor, source: Path) -> None:
    first_part = True
    for raw in source.read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if not line:
            continue
        if line.startswith("# "):
            p = anchor.insert_paragraph_before(style="Heading 1")
            if first_part:
                p.paragraph_format.page_break_before = True
            else:
                p.paragraph_format.page_break_before = True
            p.add_run(line[2:])
            p.paragraph_format.keep_with_next = True
            first_part = False
        elif line.startswith("## "):
            p = anchor.insert_paragraph_before(style="Heading 1")
            p.add_run(line[3:])
            p.paragraph_format.keep_with_next = True
        elif line.startswith("### "):
            p = anchor.insert_paragraph_before(style="Heading 2")
            p.add_run(line[4:])
            p.paragraph_format.keep_with_next = True
        else:
            p = anchor.insert_paragraph_before(style="Normal")
            add_inline(p, line)
            p.paragraph_format.space_after = Pt(6)
            p.paragraph_format.line_spacing = 1.12


def replace_cover(doc: Document) -> None:
    title = doc.paragraphs[0]
    title.text = "Учебник по DX Framework и космическому симулятору"
    title.style = "Title"
    doc.paragraphs[1].text = "Архитектура, математика, DirectX 11, Bullet Physics, NPC и разработка новых систем"
    doc.paragraphs[2].text = "Подробное руководство по текущему коду и практический план развития на 14 дней"
    doc.paragraphs[4].text = "Издание 4.0 · обновлено по исходному коду 28 октября 2026 года"
    for props in (title._element.pPr, doc.styles["Title"]._element.pPr):
        if props is not None:
            border = props.find(qn("w:pBdr"))
            if border is not None:
                props.remove(border)


def update_styles(doc: Document) -> None:
    for name in ("Title", "Heading 1", "Heading 2", "Heading 3"):
        style = doc.styles[name]
        style.font.color.rgb = RGBColor(0, 0, 0)
    doc.styles["Title"].font.underline = False
    doc.styles["Title"].font.size = Pt(22)
    doc.styles["Normal"].font.size = Pt(10.5)


def update_contents(doc: Document) -> None:
    heading = find(doc, "Содержание")
    first_part = find(doc, "Часть I.")
    # Старое статическое оглавление содержит удалённые дорожные карты.
    parent = heading._element.getparent()
    children = list(parent)
    first = children.index(heading._element) + 1
    last = children.index(first_part._element)
    for child in children[first:last]:
        parent.remove(child)
    entries = []
    for p in doc.paragraphs:
        if p.style.name != "Heading 1":
            continue
        if p.text.startswith("Часть ") or re.match(r"^\d+\. ", p.text) or p.text.startswith("Приложение "):
            entries.append(p.text)
    for entry in entries:
        p = first_part.insert_paragraph_before(style="Normal")
        p.add_run(entry)
        p.paragraph_format.left_indent = Pt(0 if entry.startswith("Часть ") else 16)
        p.paragraph_format.space_after = Pt(1.5)
        for r in p.runs:
            r.font.size = Pt(9)
    heading.paragraph_format.page_break_before = True


def add_revision_note(doc: Document) -> None:
    first_part = find(doc, "Часть I.")
    p = first_part.insert_paragraph_before(style="Heading 1")
    p.add_run("Как читать это издание")
    p.paragraph_format.page_break_before = True
    for text in (
        "Учебник соединяет описание уже работающего кода с практическими улучшениями. Путь к файлу и имя метода обозначают текущую реализацию; главы с проектными задачами описывают то, что ещё предстоит добавить.",
        "Математическая часть переписана с нуля: каждый приём привязан к коду и численному примеру. Разделы 70–77 подробно проходят ремонт, атаку, Bullet Physics, шейдеры и способ добавления систем. Разделы 78–91 содержат единственный план на две недели.",
        "Важная поправка к прежним главам о боевом отряде: вызов NpcSquadCombatSystem в текущем ApplicationClass.cpp закомментирован. Индивидуальные укрытия и стрельба работают; командная тактика рассматривается как дальнейшая работа на день 10. При расхождении текста и исходников проверяй порядок вызовов в ApplicationClass.cpp и конкретный класс системы.",
    ):
        q = first_part.insert_paragraph_before(style="Normal")
        q.add_run(text)
        q.paragraph_format.space_after = Pt(7)


def add_combat_status_note(doc: Document) -> None:
    chapter = find(doc, "51. ")
    p = chapter.insert_paragraph_before(style="Normal")
    p.add_run("Статус реализации на 28 октября 2026 года. ").bold = True
    p.add_run(
        "В этой части групповые роли и координация описаны как учебный проект и возможное развитие. "
        "В текущем игровом кадре NpcSquadCombatSystem::Update отключён в ApplicationClass.cpp. "
        "Проверяй работающий индивидуальный цикл через NpcCombatSystem, NpcCoverSystem, StateSystem и NpcRangedCombatSystem."
    )
    p.paragraph_format.space_after = Pt(9)


def main() -> None:
    doc = Document(SOURCE)
    replace_cover(doc)

    # Переписываем математическую часть вместо повторения прежнего краткого обзора.
    math_start = find(doc, "Часть III.")
    math_end = find(doc, "Часть IV.")
    remove_range(math_start, math_end)
    insert_markdown(doc, math_end, DOCS / "ENGINE_TEXTBOOK_V4_MATH_RU.md")

    # В издании остаётся только новый четырнадцатидневный план.
    remove_range(find(doc, "59. Roadmap"), find(doc, "Часть X."))
    remove_range(find(doc, "Часть XI."), find(doc, "Приложение A."))
    insert_markdown(doc, find(doc, "Приложение A."), DOCS / "ENGINE_TEXTBOOK_V4_SYSTEMS_RU.md")

    find(doc, "Часть VII.").text = "Часть VII. Срез качества и диагностика"
    add_combat_status_note(doc)
    update_styles(doc)
    update_contents(doc)
    add_revision_note(doc)
    doc.core_properties.title = "Учебник по DX Framework и космическому симулятору"
    doc.core_properties.subject = "Архитектура движка, математика, NPC, Bullet Physics, DirectX 11"
    doc.save(OUTPUT)
    print(OUTPUT)
    print("paragraphs", len(doc.paragraphs), "tables", len(doc.tables))


if __name__ == "__main__":
    main()
