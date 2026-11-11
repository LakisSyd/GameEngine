from __future__ import annotations

import re
from pathlib import Path

from docx import Document
from pypdf import PdfReader


DOCS = Path(__file__).resolve().parent
DOCX_PATH = DOCS / "DX_FRAMEWORK_ENGINE_TEXTBOOK_RU_12_0_RUNTIME_SOURCE_REFERENCE.docx"
PDF_PATH = (
    DOCS
    / "_qa_v12_word_release"
    / "DX_FRAMEWORK_ENGINE_TEXTBOOK_RU_12_0_RUNTIME_SOURCE_REFERENCE.pdf"
)


def document_text(document: Document) -> tuple[list[str], str]:
    paragraphs = [paragraph.text for paragraph in document.paragraphs]
    table_cells = [
        cell.text
        for table in document.tables
        for row in table.rows
        for cell in row.cells
    ]
    return paragraphs, "\n".join(paragraphs + table_cells)


def main() -> None:
    if not DOCX_PATH.exists():
        raise FileNotFoundError(DOCX_PATH)
    if not PDF_PATH.exists():
        raise FileNotFoundError(PDF_PATH)

    document = Document(DOCX_PATH)
    paragraphs, all_text = document_text(document)
    reader = PdfReader(PDF_PATH)

    empty_pdf_pages = []
    for page_number, page in enumerate(reader.pages, start=1):
        extracted = (page.extract_text() or "").strip()
        if len(extracted) < 5:
            empty_pdf_pages.append(page_number)

    raw_fences = [text for text in paragraphs if "```" in text]
    raw_markdown_headings = [
        text
        for text in paragraphs
        if re.match(r"^#{2,6}\s+", text)
    ]
    replacement_characters = all_text.count("\ufffd")

    # Эти маркеры означали бы, что в книгу случайно попал именно подробный
    # разбор исключённых реализаций, а не короткое пояснение границ книги.
    flooding_implementation = [
        text for text in paragraphs if "FloodingSystem::" in text
    ]
    editor_implementation = [
        text for text in paragraphs if "EditorUI::" in text
    ]

    word_count = len(re.findall(r"\b[\w-]+\b", all_text, flags=re.UNICODE))

    failures = {
        "empty_pdf_pages": empty_pdf_pages,
        "raw_fences": raw_fences,
        "raw_markdown_headings": raw_markdown_headings,
        "replacement_characters": replacement_characters,
        "flooding_implementation": flooding_implementation,
        "editor_implementation": editor_implementation,
    }

    print(f"docx={DOCX_PATH}")
    print(f"pdf={PDF_PATH}")
    print(f"pages={len(reader.pages)}")
    print(f"paragraphs={len(document.paragraphs)}")
    print(f"tables={len(document.tables)}")
    print(f"words={word_count}")
    for name, value in failures.items():
        print(f"{name}={value!r}")

    if any(bool(value) for value in failures.values()):
        raise SystemExit("V12 release QA failed")

    print("status=PASS")


if __name__ == "__main__":
    main()
