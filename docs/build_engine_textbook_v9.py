"""Собирает учебное издание 9.0 по текущему коду DX Framework.

Версия 9.0 намеренно не склеивает старые главы. Она заново собирает четыре
учебных маршрута: математика, построчное чтение систем, алгоритмы и практика.
Формулы вставляются как нативные объекты Word, а не как сырой LaTeX.
"""

from __future__ import annotations

import re
from pathlib import Path

from docx import Document
from docx.enum.section import WD_SECTION
from docx.enum.style import WD_STYLE_TYPE
from docx.enum.table import WD_CELL_VERTICAL_ALIGNMENT, WD_TABLE_ALIGNMENT
from docx.enum.text import WD_ALIGN_PARAGRAPH, WD_BREAK
from docx.oxml import OxmlElement
from docx.oxml.ns import qn
from docx.shared import Inches, Pt, RGBColor


DOCS = Path(__file__).resolve().parent
OUTPUT = DOCS / "DX_FRAMEWORK_ENGINE_TEXTBOOK_RU_9_0_LEARNING_EDITION.docx"

CHAPTERS = (
    "ENGINE_TEXTBOOK_V9_MATH_FROM_ZERO_RU.md",
    "ENGINE_TEXTBOOK_V9_CODE_WALKTHROUGHS_RU.md",
    "ENGINE_TEXTBOOK_V9_ALGORITHMS_RU.md",
    "ENGINE_TEXTBOOK_V9_ROADMAP_RU.md",
)

BLACK = RGBColor(0, 0, 0)
BODY = RGBColor(32, 35, 38)
MUTED = RGBColor(91, 98, 104)
NAVY = "24364B"
LIGHT_BLUE = "EAF0F5"
LIGHT_GRAY = "F5F6F7"
BORDER = "C9CED3"


def set_cell_shading(cell, fill: str) -> None:
    tc_pr = cell._tc.get_or_add_tcPr()
    shd = tc_pr.find(qn("w:shd"))
    if shd is None:
        shd = OxmlElement("w:shd")
        tc_pr.append(shd)
    shd.set(qn("w:fill"), fill)


def set_cell_margins(cell, top: int = 90, start: int = 110,
                     bottom: int = 90, end: int = 110) -> None:
    tc = cell._tc
    tc_pr = tc.get_or_add_tcPr()
    tc_mar = tc_pr.first_child_found_in("w:tcMar")
    if tc_mar is None:
        tc_mar = OxmlElement("w:tcMar")
        tc_pr.append(tc_mar)
    for name, value in (("top", top), ("start", start),
                        ("bottom", bottom), ("end", end)):
        node = tc_mar.find(qn(f"w:{name}"))
        if node is None:
            node = OxmlElement(f"w:{name}")
            tc_mar.append(node)
        node.set(qn("w:w"), str(value))
        node.set(qn("w:type"), "dxa")


def set_repeat_table_header(row) -> None:
    tr_pr = row._tr.get_or_add_trPr()
    tbl_header = OxmlElement("w:tblHeader")
    tbl_header.set(qn("w:val"), "true")
    tr_pr.append(tbl_header)


def keep_table_row_together(row) -> None:
    """Запрещает Word разрывать одну логическую строку между страницами."""
    tr_pr = row._tr.get_or_add_trPr()
    cant_split = OxmlElement("w:cantSplit")
    cant_split.set(qn("w:val"), "true")
    tr_pr.append(cant_split)


def set_table_borders(table) -> None:
    tbl_pr = table._tbl.tblPr
    borders = tbl_pr.first_child_found_in("w:tblBorders")
    if borders is None:
        borders = OxmlElement("w:tblBorders")
        tbl_pr.append(borders)
    for edge in ("top", "left", "bottom", "right", "insideH", "insideV"):
        tag = borders.find(qn(f"w:{edge}"))
        if tag is None:
            tag = OxmlElement(f"w:{edge}")
            borders.append(tag)
        tag.set(qn("w:val"), "single")
        tag.set(qn("w:sz"), "4")
        tag.set(qn("w:space"), "0")
        tag.set(qn("w:color"), BORDER)


def add_page_field(paragraph) -> None:
    run = paragraph.add_run()
    begin = OxmlElement("w:fldChar")
    begin.set(qn("w:fldCharType"), "begin")
    run._r.append(begin)

    run = paragraph.add_run()
    instruction = OxmlElement("w:instrText")
    instruction.set(qn("xml:space"), "preserve")
    instruction.text = "PAGE"
    run._r.append(instruction)

    run = paragraph.add_run()
    separate = OxmlElement("w:fldChar")
    separate.set(qn("w:fldCharType"), "separate")
    run._r.append(separate)
    paragraph.add_run("1")

    run = paragraph.add_run()
    end = OxmlElement("w:fldChar")
    end.set(qn("w:fldCharType"), "end")
    run._r.append(end)


def set_document_styles(doc: Document) -> None:
    section = doc.sections[0]
    section.page_width = Inches(8.5)
    section.page_height = Inches(11)
    section.top_margin = Inches(0.78)
    section.bottom_margin = Inches(0.72)
    section.left_margin = Inches(0.86)
    section.right_margin = Inches(0.76)
    section.header_distance = Inches(0.32)
    section.footer_distance = Inches(0.35)
    section.different_first_page_header_footer = True

    normal = doc.styles["Normal"]
    normal.font.name = "Aptos"
    normal.font.size = Pt(10.5)
    normal.font.color.rgb = BODY
    normal.paragraph_format.space_after = Pt(5.5)
    normal.paragraph_format.line_spacing = 1.13
    normal.paragraph_format.widow_control = True

    for name, size, before, after in (
        ("Title", 25, 0, 18),
        ("Heading 1", 18, 0, 12),
        ("Heading 2", 13.5, 15, 6),
        ("Heading 3", 11.2, 11, 4),
        ("Heading 4", 10.3, 8, 3),
    ):
        style = doc.styles[name]
        style.font.name = "Aptos Display"
        style.font.size = Pt(size)
        style.font.bold = True
        style.font.color.rgb = BLACK
        style.paragraph_format.space_before = Pt(before)
        style.paragraph_format.space_after = Pt(after)
        style.paragraph_format.keep_with_next = True
        # Word не должен переносить половину длинного заголовка на следующую
        # страницу. Особенно это заметно на заголовках дней учебного плана.
        style.paragraph_format.keep_together = True
        style.paragraph_format.widow_control = True

    # Встроенный стиль Title в Word иногда содержит нижнюю синюю границу.
    # Для обложки книги она не нужна: заголовок остаётся простым и чёрным.
    title_p_pr = doc.styles["Title"].element.get_or_add_pPr()
    title_border = title_p_pr.find(qn("w:pBdr"))
    if title_border is not None:
        title_p_pr.remove(title_border)

    if "TextbookCode" not in doc.styles:
        code = doc.styles.add_style("TextbookCode", WD_STYLE_TYPE.PARAGRAPH)
        code.base_style = normal
        code.font.name = "Consolas"
        code.font.size = Pt(8.1)
        code.font.color.rgb = RGBColor(20, 27, 34)
        code.paragraph_format.left_indent = Inches(0.18)
        code.paragraph_format.right_indent = Inches(0.08)
        code.paragraph_format.space_before = Pt(0)
        code.paragraph_format.space_after = Pt(0)
        code.paragraph_format.line_spacing = 1.0
        code.paragraph_format.widow_control = False

    if "TextbookEquation" not in doc.styles:
        equation = doc.styles.add_style("TextbookEquation", WD_STYLE_TYPE.PARAGRAPH)
        equation.base_style = normal
        equation.font.name = "Cambria Math"
        equation.font.size = Pt(10.5)
        equation.font.color.rgb = BLACK
        equation.paragraph_format.alignment = WD_ALIGN_PARAGRAPH.CENTER
        equation.paragraph_format.space_before = Pt(4)
        equation.paragraph_format.space_after = Pt(7)
        equation.paragraph_format.keep_together = True

    if "TextbookSmall" not in doc.styles:
        small = doc.styles.add_style("TextbookSmall", WD_STYLE_TYPE.PARAGRAPH)
        small.base_style = normal
        small.font.name = "Aptos"
        small.font.size = Pt(8.8)
        small.font.color.rgb = MUTED
        small.paragraph_format.space_after = Pt(4)

    for list_name in ("List Bullet", "List Number"):
        style = doc.styles[list_name]
        style.font.name = "Aptos"
        style.font.size = Pt(10.3)
        style.font.color.rgb = BODY
        style.paragraph_format.left_indent = Inches(0.28)
        style.paragraph_format.first_line_indent = Inches(-0.17)
        style.paragraph_format.space_after = Pt(3)

    header = section.header.paragraphs[0]
    header.text = "DX Framework   Учебное издание по текущему коду"
    header.alignment = WD_ALIGN_PARAGRAPH.RIGHT
    for run in header.runs:
        run.font.name = "Aptos"
        run.font.size = Pt(8)
        run.font.color.rgb = MUTED

    footer = section.footer.paragraphs[0]
    footer.alignment = WD_ALIGN_PARAGRAPH.CENTER
    footer.add_run("Издание 9.0   ")
    add_page_field(footer)
    for run in footer.runs:
        run.font.name = "Aptos"
        run.font.size = Pt(8)
        run.font.color.rgb = MUTED

    section.first_page_header.paragraphs[0].text = ""
    section.first_page_footer.paragraphs[0].text = ""


INLINE = re.compile(r"(`[^`]+`|\*\*[^*]+\*\*|\[[^]]+\]\([^)]+\))")


def add_inline(paragraph, value: str, color: RGBColor | None = None) -> None:
    for piece in INLINE.split(value):
        if not piece:
            continue
        if piece.startswith("`") and piece.endswith("`"):
            run = paragraph.add_run(piece[1:-1])
            run.font.name = "Consolas"
            run.font.size = Pt(8.6)
            run.font.color.rgb = color or RGBColor(32, 68, 92)
        elif piece.startswith("**") and piece.endswith("**"):
            run = paragraph.add_run(piece[2:-2])
            run.bold = True
            if color:
                run.font.color.rgb = color
        elif piece.startswith("[") and "](" in piece:
            label, url = piece[1:-1].split("](", 1)
            run = paragraph.add_run(f"{label} ({url})")
            run.font.color.rgb = color or RGBColor(32, 68, 92)
        else:
            run = paragraph.add_run(piece)
            if color:
                run.font.color.rgb = color


def clean_latex(value: str) -> str:
    """Переводит использованный в главах LaTeX в читаемую линейную запись.

    Word хранит результат в OMML. Это не общий LaTeX-парсер: функция специально
    покрывает небольшой учебный словарь этой книги и не оставляет команды с '\\'.
    """
    text = " ".join(line.strip() for line in value.strip().splitlines())
    replacements = {
        r"\begin{bmatrix}": "[",
        r"\end{bmatrix}": "]",
        r"\\": "; ",
        r"\cdot": "·",
        r"\times": "×",
        r"\approx": "≈",
        r"\ge": "≥",
        r"\le": "≤",
        r"\ne": "≠",
        r"\quad": "  ",
        r"\theta": "θ",
        r"\pi": "π",
        r"\hat": "unit ",
        r"\circ": "°",
        r"\sum": "Σ",
        r"\cos": "cos",
        r"\sin": "sin",
        r"\atan": "atan",
        r"\left": "",
        r"\right": "",
        r"\,": " ",
        r"\ ": " ",
    }
    for source, target in replacements.items():
        text = text.replace(source, target)

    # Несколько проходов покрывают вложенные простые дроби и корни.
    fraction = re.compile(r"\\frac\{([^{}]+)\}\{([^{}]+)\}")
    square_root = re.compile(r"\\sqrt\{([^{}]+)\}")
    for _ in range(4):
        text = fraction.sub(r"(\1)/(\2)", text)
        text = square_root.sub(r"√(\1)", text)

    text = text.replace("&", ", ")
    text = re.sub(r"\^\{([^{}]+)\}", r"^(\1)", text)
    text = re.sub(r"_\{([^{}]+)\}", r"_(\1)", text)
    text = text.replace("^2", "²")
    text = text.replace("^°", "°")
    text = text.replace("{", "(").replace("}", ")")
    text = re.sub(r"\s+", " ", text).strip()
    if "\\" in text:
        raise ValueError(f"Необработанная команда формулы: {text}")
    return text


def add_native_equation(doc: Document, latex: str) -> None:
    linear = clean_latex(latex)
    paragraph = doc.add_paragraph(style="TextbookEquation")

    # Длинные матрицы Word не переносит внутри одного OMML-объекта. Для них
    # используем математический шрифт и явный перенос строк матрицы. Так вся
    # формула остаётся видимой и не выходит за правое поле страницы.
    if len(linear) > 92 or linear.count(";") >= 3:
        parts = linear.split(";")
        for index, part in enumerate(parts):
            run = paragraph.add_run(part.strip())
            run.font.name = "Cambria Math"
            run.font.size = Pt(10.2)
            if index < len(parts) - 1:
                run.add_text(";")
                run.add_break()
        return

    math_para = OxmlElement("m:oMathPara")
    math_para_pr = OxmlElement("m:oMathParaPr")
    justification = OxmlElement("m:jc")
    justification.set(qn("m:val"), "center")
    math_para_pr.append(justification)
    math_para.append(math_para_pr)

    math = OxmlElement("m:oMath")
    run = OxmlElement("m:r")
    run_pr = OxmlElement("m:rPr")
    style = OxmlElement("m:sty")
    style.set(qn("m:val"), "p")
    run_pr.append(style)
    run.append(run_pr)
    text = OxmlElement("m:t")
    text.set(qn("xml:space"), "preserve")
    text.text = linear
    run.append(text)
    math.append(run)
    math_para.append(math)
    paragraph._p.append(math_para)


def add_markdown_table(doc: Document, source_rows: list[str]) -> None:
    def split_row(row: str) -> list[str]:
        # Вертикальная черта внутри `a || b` является частью C++, а не
        # разделителем Markdown-таблицы. Поэтому учитываем границы inline-code.
        value = row.strip()
        if value.startswith("|"):
            value = value[1:]
        if value.endswith("|"):
            value = value[:-1]
        cells: list[str] = []
        buffer: list[str] = []
        inside_code = False
        for character in value:
            if character == "`":
                inside_code = not inside_code
                buffer.append(character)
            elif character == "|" and not inside_code:
                cells.append("".join(buffer).strip())
                buffer.clear()
            else:
                buffer.append(character)
        cells.append("".join(buffer).strip())
        return cells

    rows = [split_row(row) for row in source_rows]
    if len(rows) > 1 and all(
        re.fullmatch(r":?-{3,}:?", cell) for cell in rows[1]
    ):
        rows.pop(1)
    if not rows:
        return

    column_count = max(len(row) for row in rows)
    table = doc.add_table(rows=0, cols=column_count)
    table.alignment = WD_TABLE_ALIGNMENT.CENTER
    table.autofit = True
    set_table_borders(table)

    for row_index, values in enumerate(rows):
        row = table.add_row()
        keep_table_row_together(row)
        if row_index == 0:
            set_repeat_table_header(row)
        for column_index in range(column_count):
            cell = row.cells[column_index]
            cell.vertical_alignment = WD_CELL_VERTICAL_ALIGNMENT.CENTER
            set_cell_margins(cell)
            paragraph = cell.paragraphs[0]
            paragraph.paragraph_format.space_after = Pt(0)
            paragraph.paragraph_format.line_spacing = 1.0
            value = values[column_index] if column_index < len(values) else ""
            if row_index == 0:
                set_cell_shading(cell, NAVY)
                add_inline(paragraph, value, RGBColor(255, 255, 255))
                for run in paragraph.runs:
                    run.bold = True
                    run.font.size = Pt(8.4)
            else:
                set_cell_shading(cell, "FFFFFF" if row_index % 2 else LIGHT_GRAY)
                add_inline(paragraph, value)
                for run in paragraph.runs:
                    run.font.size = Pt(8.2)

    spacer = doc.add_paragraph()
    spacer.paragraph_format.space_after = Pt(1)


def shade_code_paragraph(paragraph) -> None:
    p_pr = paragraph._p.get_or_add_pPr()
    shd = OxmlElement("w:shd")
    shd.set(qn("w:fill"), LIGHT_GRAY)
    p_pr.append(shd)


def add_markdown(doc: Document, filename: str) -> None:
    path = DOCS / filename
    lines = path.read_text(encoding="utf-8").splitlines()
    in_code = False
    in_equation = False
    equation_lines: list[str] = []
    table_rows: list[str] = []
    code_paragraphs = []
    previous_paragraph = None
    previous_kind = None

    def flush_table() -> None:
        if table_rows:
            add_markdown_table(doc, table_rows)
            table_rows.clear()

    for raw in lines:
        line = raw.rstrip()

        if line.strip() == "$$":
            flush_table()
            if in_equation:
                add_native_equation(doc, "\n".join(equation_lines))
                equation_lines.clear()
                in_equation = False
            else:
                in_equation = True
            continue

        if in_equation:
            equation_lines.append(line)
            continue

        if line.startswith("```"):
            flush_table()
            if in_code:
                # Не оставляем одну строку блока кода внизу страницы.
                for paragraph in code_paragraphs[:-1]:
                    paragraph.paragraph_format.keep_with_next = True
                    paragraph.paragraph_format.keep_together = True
                code_paragraphs.clear()
                in_code = False
                previous_kind = "code"
            else:
                # Вводная строка вроде «Навигация и прибытие» не должна
                # оставаться одна внизу страницы без следующего блока кода.
                if previous_paragraph is not None:
                    previous_paragraph.paragraph_format.keep_with_next = True
                code_paragraphs.clear()
                in_code = True
            continue

        if in_code:
            paragraph = doc.add_paragraph(style="TextbookCode")
            paragraph.add_run(line if line else " ")
            shade_code_paragraph(paragraph)
            code_paragraphs.append(paragraph)
            previous_paragraph = paragraph
            continue

        if line.startswith("|") and line.endswith("|"):
            # Сохраняем вместе с таблицей короткую строку, которая объясняет,
            # что именно читатель сейчас увидит в её колонках.
            if not table_rows and previous_paragraph is not None:
                previous_paragraph.paragraph_format.keep_with_next = True
            table_rows.append(line)
            continue

        flush_table()

        if not line.strip():
            continue
        if line.startswith("# "):
            paragraph = doc.add_paragraph(style="Heading 1")
            paragraph.paragraph_format.page_break_before = True
            paragraph.add_run(line[2:])
        elif line.startswith("## "):
            paragraph = doc.add_paragraph(style="Heading 2")
            paragraph.add_run(line[3:])
        elif line.startswith("### "):
            paragraph = doc.add_paragraph(style="Heading 3")
            paragraph.add_run(line[4:])
        elif line.startswith("#### "):
            # Карта методов использует четвёртый уровень для имён функций.
            # Не оставляем Markdown-маркеры в готовой книге: Word получает
            # настоящий заголовок, который сохраняет навигацию и интервалы.
            paragraph = doc.add_paragraph(style="Heading 4")
            paragraph.add_run(line[5:])
        elif re.match(r"^\s*- ", line):
            # Первый пункт остаётся с вводной строкой, а каждый следующий — с
            # предыдущим. Последний пункт не приклеивается к следующему разделу.
            if previous_paragraph is not None:
                previous_paragraph.paragraph_format.keep_with_next = True
            paragraph = doc.add_paragraph(style="List Bullet")
            add_inline(paragraph, line.lstrip()[2:])
            paragraph.paragraph_format.keep_together = True
            previous_kind = "list"
        elif re.match(r"^\s*\d+\. ", line):
            # Номер берём из Markdown явно. Встроенная нумерация Word иначе
            # продолжает предыдущий список на другой странице и превращает
            # новый алгоритм 1..5 в 8..12.
            match = re.match(r"^\s*(\d+)\.\s+(.*)", line)
            assert match is not None
            paragraph = doc.add_paragraph(style="Normal")
            paragraph.paragraph_format.left_indent = Inches(0.28)
            paragraph.paragraph_format.first_line_indent = Inches(-0.17)
            paragraph.paragraph_format.space_after = Pt(3)
            paragraph.add_run(f"{match.group(1)}. ")
            add_inline(paragraph, match.group(2))
            if previous_paragraph is not None:
                previous_paragraph.paragraph_format.keep_with_next = True
            paragraph.paragraph_format.keep_together = True
            previous_kind = "list"
        else:
            paragraph = doc.add_paragraph(style="Normal")
            add_inline(paragraph, line)
            previous_kind = "normal"

        previous_paragraph = paragraph
        if paragraph.style.name.startswith("Heading"):
            previous_kind = "heading"

    flush_table()
    if in_code:
        raise ValueError(f"Незакрытый блок кода в {path}")
    if in_equation:
        raise ValueError(f"Незакрытая формула в {path}")


def add_cover(doc: Document) -> None:
    paragraph = doc.add_paragraph(style="Title")
    paragraph.alignment = WD_ALIGN_PARAGRAPH.CENTER
    paragraph.paragraph_format.space_before = Inches(1.5)
    paragraph.add_run("DX Framework учебник по математике и исходному коду")

    paragraph = doc.add_paragraph()
    paragraph.alignment = WD_ALIGN_PARAGRAPH.CENTER
    paragraph.paragraph_format.space_after = Pt(16)
    run = paragraph.add_run("Учебное издание 9.0")
    run.font.name = "Aptos Display"
    run.font.size = Pt(17)
    run.font.bold = True
    run.font.color.rgb = BLACK

    for value in (
        "Математика от ручного расчёта до DirectXMath",
        "Построчное чтение текущих ECS боевых и тактических систем",
        "Справочник алгоритмов и учебный маршрут на четыре недели",
    ):
        paragraph = doc.add_paragraph(value)
        paragraph.alignment = WD_ALIGN_PARAGRAPH.CENTER
        paragraph.paragraph_format.space_after = Pt(7)

    paragraph = doc.add_paragraph()
    paragraph.alignment = WD_ALIGN_PARAGRAPH.CENTER
    paragraph.paragraph_format.space_before = Inches(2.25)
    run = paragraph.add_run("Собрано по текущему снимку репозитория")
    run.font.size = Pt(9.5)
    run.font.color.rgb = MUTED

    paragraph = doc.add_paragraph()
    paragraph.alignment = WD_ALIGN_PARAGRAPH.CENTER
    run = paragraph.add_run("1 ноября 2026 года")
    run.font.size = Pt(9.5)
    run.font.color.rgb = MUTED
    doc.add_page_break()


def add_book_map(doc: Document) -> None:
    paragraph = doc.add_paragraph(style="Heading 1")
    paragraph.add_run("Как пользоваться книгой")

    for text in (
        "Эта редакция не повторяет старый справочник. Она построена вокруг четырёх действий: посчитать вручную, найти то же действие в текущем коде, изменить маленький участок самостоятельно и доказать результат отладкой.",
        "Не читай книгу подряд как роман. После каждой темы выполняй указанное мини задание. Если не можешь предсказать результат до запуска, вернись к ручному примеру.",
        "Построчные таблицы используют реальный код проекта. Колонка с поломкой нужна не для страха, а для понимания контракта строки. Именно контракт подсказывает, куда безопасно добавлять новую механику.",
    ):
        paragraph = doc.add_paragraph(style="Normal")
        paragraph.add_run(text)

    paragraph = doc.add_paragraph(style="Heading 2")
    paragraph.add_run("Карта книги")

    data = [
        ("Математика движка с нуля", "Векторы матрицы лучи пересечения время и упражнения"),
        ("Построчный разбор систем DX Framework", "ECS Transform тактика укрытия состояние стрельба и урон"),
        ("Справочник полезных алгоритмов", "Готовые схемы выбора движения навигации и отладки"),
        ("Учебный маршрут по DX Framework и NPC", "Интенсив на день и практика на четыре недели"),
    ]
    table = doc.add_table(rows=1, cols=2)
    table.alignment = WD_TABLE_ALIGNMENT.CENTER
    set_table_borders(table)
    table.rows[0].cells[0].text = "Раздел"
    table.rows[0].cells[1].text = "Что ты научишься делать"
    set_repeat_table_header(table.rows[0])
    keep_table_row_together(table.rows[0])
    for cell in table.rows[0].cells:
        set_cell_shading(cell, NAVY)
        set_cell_margins(cell)
        for run in cell.paragraphs[0].runs:
            run.font.color.rgb = RGBColor(255, 255, 255)
            run.font.bold = True
            run.font.size = Pt(8.8)
    for index, values in enumerate(data, start=1):
        cells = table.add_row().cells
        keep_table_row_together(table.rows[-1])
        for cell, value in zip(cells, values):
            set_cell_margins(cell)
            set_cell_shading(cell, "FFFFFF" if index % 2 else LIGHT_GRAY)
            cell.text = value
            for run in cell.paragraphs[0].runs:
                run.font.size = Pt(8.8)

    paragraph = doc.add_paragraph(style="Heading 2")
    paragraph.add_run("Условие честной проверки")
    paragraph = doc.add_paragraph(style="Normal")
    paragraph.add_run(
        "Сначала записывай ожидаемое поведение, затем запускай сцену. "
        "Если результат отличается, ищи первую строку цепочки данных, где "
        "фактическое значение перестало совпадать с ожидаемым."
    )


def set_core_properties(doc: Document) -> None:
    doc.core_properties.title = (
        "DX Framework учебник по математике и исходному коду"
    )
    doc.core_properties.subject = (
        "Математика матрицы лучи ECS NPC бой укрытия алгоритмы и практика"
    )
    doc.core_properties.author = "Codex"
    doc.core_properties.comments = (
        "Учебное издание 9.0 по текущему снимку DX Framework"
    )


def main() -> None:
    for filename in CHAPTERS:
        if not (DOCS / filename).exists():
            raise FileNotFoundError(DOCS / filename)

    doc = Document()
    set_document_styles(doc)
    add_cover(doc)
    add_book_map(doc)
    for filename in CHAPTERS:
        add_markdown(doc, filename)
    set_core_properties(doc)
    doc.save(OUTPUT)

    total_words = 0
    for filename in CHAPTERS:
        text = (DOCS / filename).read_text(encoding="utf-8")
        total_words += len(re.findall(r"\b[\w-]+\b", text))
    print(OUTPUT)
    print(
        f"paragraphs={len(doc.paragraphs)} "
        f"tables={len(doc.tables)} words_approx={total_words}"
    )


if __name__ == "__main__":
    main()
