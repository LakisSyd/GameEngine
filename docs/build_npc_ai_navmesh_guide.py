from pathlib import Path

from docx import Document
from docx.enum.section import WD_SECTION
from docx.enum.style import WD_STYLE_TYPE
from docx.enum.text import WD_ALIGN_PARAGRAPH
from docx.oxml import OxmlElement
from docx.oxml.ns import qn
from docx.shared import Inches, Pt, RGBColor


ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "docs" / "NPC_AI_NAVMESH_CODE_GUIDE_RU.docx"

BLUE = RGBColor(46, 116, 181)
DARK_BLUE = RGBColor(31, 77, 120)
NAVY = RGBColor(32, 55, 72)
GRAY = RGBColor(90, 96, 105)
LIGHT_GRAY = "F4F6F9"
CODE_GRAY = "F6F8FA"


def set_font(run, name, size=None, bold=None, italic=None, color=None):
    run.font.name = name
    run._element.get_or_add_rPr().rFonts.set(qn("w:ascii"), name)
    run._element.get_or_add_rPr().rFonts.set(qn("w:hAnsi"), name)
    run._element.get_or_add_rPr().rFonts.set(qn("w:eastAsia"), name)
    if size is not None:
        run.font.size = Pt(size)
    if bold is not None:
        run.bold = bold
    if italic is not None:
        run.italic = italic
    if color is not None:
        run.font.color.rgb = color


def set_cell_shading(cell, fill):
    tc_pr = cell._tc.get_or_add_tcPr()
    shd = tc_pr.find(qn("w:shd"))
    if shd is None:
        shd = OxmlElement("w:shd")
        tc_pr.append(shd)
    shd.set(qn("w:fill"), fill)


def set_paragraph_shading(paragraph, fill):
    p_pr = paragraph._p.get_or_add_pPr()
    shd = p_pr.find(qn("w:shd"))
    if shd is None:
        shd = OxmlElement("w:shd")
        p_pr.append(shd)
    shd.set(qn("w:fill"), fill)


def set_paragraph_border_left(paragraph, color="2E74B5", size="18", space="8"):
    p_pr = paragraph._p.get_or_add_pPr()
    p_bdr = p_pr.find(qn("w:pBdr"))
    if p_bdr is None:
        p_bdr = OxmlElement("w:pBdr")
        p_pr.append(p_bdr)
    left = OxmlElement("w:left")
    left.set(qn("w:val"), "single")
    left.set(qn("w:sz"), size)
    left.set(qn("w:space"), space)
    left.set(qn("w:color"), color)
    p_bdr.append(left)


def add_page_number(paragraph):
    paragraph.alignment = WD_ALIGN_PARAGRAPH.RIGHT
    run = paragraph.add_run()
    begin = OxmlElement("w:fldChar")
    begin.set(qn("w:fldCharType"), "begin")
    instruction = OxmlElement("w:instrText")
    instruction.set(qn("xml:space"), "preserve")
    instruction.text = " PAGE "
    separate = OxmlElement("w:fldChar")
    separate.set(qn("w:fldCharType"), "separate")
    text = OxmlElement("w:t")
    text.text = "1"
    end = OxmlElement("w:fldChar")
    end.set(qn("w:fldCharType"), "end")
    run._r.extend([begin, instruction, separate, text, end])
    set_font(run, "Calibri", 9, color=GRAY)


def configure_document(doc):
    section = doc.sections[0]
    section.page_width = Inches(8.5)
    section.page_height = Inches(11)
    section.top_margin = Inches(1.0)
    section.bottom_margin = Inches(1.0)
    section.left_margin = Inches(1.0)
    section.right_margin = Inches(1.0)
    section.header_distance = Inches(0.492)
    section.footer_distance = Inches(0.492)

    normal = doc.styles["Normal"]
    normal.font.name = "Calibri"
    normal._element.rPr.rFonts.set(qn("w:ascii"), "Calibri")
    normal._element.rPr.rFonts.set(qn("w:hAnsi"), "Calibri")
    normal.font.size = Pt(11)
    normal.paragraph_format.space_before = Pt(0)
    normal.paragraph_format.space_after = Pt(6)
    normal.paragraph_format.line_spacing = 1.25

    for name, size, color, before, after in [
        ("Heading 1", 16, BLUE, 18, 10),
        ("Heading 2", 13, BLUE, 14, 7),
        ("Heading 3", 12, DARK_BLUE, 10, 5),
    ]:
        style = doc.styles[name]
        style.font.name = "Calibri"
        style._element.rPr.rFonts.set(qn("w:ascii"), "Calibri")
        style._element.rPr.rFonts.set(qn("w:hAnsi"), "Calibri")
        style.font.size = Pt(size)
        style.font.bold = True
        style.font.color.rgb = color
        style.paragraph_format.space_before = Pt(before)
        style.paragraph_format.space_after = Pt(after)
        style.paragraph_format.keep_with_next = True

    code_style = doc.styles.add_style("CodeBlock", WD_STYLE_TYPE.PARAGRAPH)
    code_style.font.name = "Consolas"
    code_style._element.rPr.rFonts.set(qn("w:ascii"), "Consolas")
    code_style._element.rPr.rFonts.set(qn("w:hAnsi"), "Consolas")
    code_style._element.rPr.rFonts.set(qn("w:eastAsia"), "Consolas")
    code_style.font.size = Pt(7.5)
    code_style.paragraph_format.left_indent = Inches(0.12)
    code_style.paragraph_format.right_indent = Inches(0.08)
    code_style.paragraph_format.space_before = Pt(3)
    code_style.paragraph_format.space_after = Pt(5)
    code_style.paragraph_format.line_spacing = 0.95
    code_style.paragraph_format.keep_together = False
    code_style.paragraph_format.widow_control = False

    caption = doc.styles.add_style("CodeCaption", WD_STYLE_TYPE.PARAGRAPH)
    caption.font.name = "Calibri"
    caption.font.size = Pt(9)
    caption.font.bold = True
    caption.font.color.rgb = GRAY
    caption.paragraph_format.space_before = Pt(7)
    caption.paragraph_format.space_after = Pt(2)
    caption.paragraph_format.keep_with_next = True

    header = section.header.paragraphs[0]
    header.alignment = WD_ALIGN_PARAGRAPH.LEFT
    header.paragraph_format.space_after = Pt(0)
    run = header.add_run("DX Framework  |  NPC AI + Recast/Detour")
    set_font(run, "Calibri", 9, bold=True, color=GRAY)
    add_page_number(section.footer.paragraphs[0])


def add_cover(doc):
    p = doc.add_paragraph()
    p.paragraph_format.space_before = Pt(72)
    p.paragraph_format.space_after = Pt(14)
    p.alignment = WD_ALIGN_PARAGRAPH.CENTER
    r = p.add_run("УЧЕБНОЕ РУКОВОДСТВО")
    set_font(r, "Calibri", 11, bold=True, color=BLUE)

    p = doc.add_paragraph()
    p.alignment = WD_ALIGN_PARAGRAPH.CENTER
    p.paragraph_format.space_after = Pt(8)
    r = p.add_run("NPC AI и NavMesh")
    set_font(r, "Calibri", 30, bold=True, color=NAVY)

    p = doc.add_paragraph()
    p.alignment = WD_ALIGN_PARAGRAPH.CENTER
    p.paragraph_format.space_after = Pt(18)
    r = p.add_run("Команды, Utility AI, комнаты, двери, ремонт, Recast и Detour")
    set_font(r, "Calibri", 14, color=DARK_BLUE)

    p = doc.add_paragraph()
    p.alignment = WD_ALIGN_PARAGRAPH.CENTER
    p.paragraph_format.space_after = Pt(60)
    r = p.add_run("С актуальным кодом проекта и подробными комментариями")
    set_font(r, "Calibri", 10.5, italic=True, color=GRAY)

    p = doc.add_paragraph()
    p.alignment = WD_ALIGN_PARAGRAPH.CENTER
    p.paragraph_format.space_after = Pt(4)
    r = p.add_run("DX_FrameWork")
    set_font(r, "Calibri", 12, bold=True, color=NAVY)

    p = doc.add_paragraph()
    p.alignment = WD_ALIGN_PARAGRAPH.CENTER
    r = p.add_run("Версия документа: 13 августа 2026")
    set_font(r, "Calibri", 9.5, color=GRAY)
    doc.add_page_break()


def add_callout(doc, label, text):
    p = doc.add_paragraph()
    p.paragraph_format.left_indent = Inches(0.18)
    p.paragraph_format.right_indent = Inches(0.10)
    p.paragraph_format.space_before = Pt(5)
    p.paragraph_format.space_after = Pt(9)
    set_paragraph_shading(p, LIGHT_GRAY)
    set_paragraph_border_left(p)
    r = p.add_run(label + " ")
    set_font(r, "Calibri", 10.5, bold=True, color=DARK_BLUE)
    r = p.add_run(text)
    set_font(r, "Calibri", 10.5)


def add_lead(doc, label, text):
    p = doc.add_paragraph()
    r = p.add_run(label + " ")
    set_font(r, "Calibri", 11, bold=True, color=DARK_BLUE)
    r = p.add_run(text)
    set_font(r, "Calibri", 11)


def add_code_text(doc, title, text, start_line=1):
    p = doc.add_paragraph(style="CodeCaption")
    p.add_run(title)
    lines = text.splitlines()
    chunk_size = 90
    for chunk_start in range(0, len(lines), chunk_size):
        chunk = lines[chunk_start:chunk_start + chunk_size]
        width = max(4, len(str(start_line + chunk_start + len(chunk))))
        numbered = "\n".join(
            f"{start_line + chunk_start + i:>{width}}  {line}"
            for i, line in enumerate(chunk)
        )
        p = doc.add_paragraph(style="CodeBlock")
        set_paragraph_shading(p, CODE_GRAY)
        r = p.add_run(numbered)
        set_font(r, "Consolas", 7.5)


def add_full_file(doc, path, explanation):
    full_path = ROOT / path
    doc.add_heading(path.replace("\\", "/"), level=2)
    doc.add_paragraph(explanation)
    text = full_path.read_text(encoding="utf-8", errors="replace")
    add_code_text(doc, f"Полный листинг: {path}", text, 1)


def add_excerpt(doc, path, first, last, explanation):
    full_path = ROOT / path
    lines = full_path.read_text(encoding="utf-8", errors="replace").splitlines()
    doc.add_heading(f"{path.replace(chr(92), '/')} — строки {first}-{last}", level=3)
    doc.add_paragraph(explanation)
    add_code_text(doc, "Фрагмент актуального кода", "\n".join(lines[first - 1:last]), first)


def build():
    doc = Document()
    configure_document(doc)
    add_cover(doc)

    doc.add_heading("Как пользоваться этим документом", level=1)
    doc.add_paragraph(
        "Документ рассчитан на чтение без доступа к домашнему компьютеру. Сначала идёт "
        "объяснение архитектуры человеческим языком, затем полный актуальный код основных "
        "AI-файлов с теми же подробными комментариями, которые находятся в проекте."
    )
    add_callout(
        doc,
        "Главная идея.",
        "Utility AI решает, что делать; StateSystem решает, как выполнить задачу; "
        "NpcNavMeshSystem является необязательным локальным поиском пути и не владеет поведением NPC."
    )

    doc.add_heading("Карта файлов", level=2)
    add_lead(doc, "NPCComponents.h —", "данные персонажа, состояние, команда, маршрут и настройки.")
    add_lead(doc, "NpcCommandSystem.h —", "приоритетные ручные и debug-команды.")
    add_lead(doc, "NpcUtilitySystem.h —", "оценка пробоин и автоматический выбор работы.")
    add_lead(doc, "StateSystem.h —", "исполнение состояний, двери, ремонт и движение.")
    add_lead(doc, "NpcNavMeshSystem.h/.cpp —", "изолированный адаптер Recast/Detour.")
    add_lead(doc, "DoorSystem.h —", "фактическое открытие и анимация створок.")
    add_lead(doc, "RepairSystem.h —", "единая логика ремонта игрока и NPC.")

    doc.add_heading("Архитектура одного кадра", level=1)
    add_lead(doc, "Шаг A — решение.", "NpcUtilitySystem вычисляет score задач, но не перебивает активную ручную команду.")
    add_lead(doc, "Шаг B — исполнение.", "StateSystem выбирает ручную либо автоматическую ветку и определяет текущую локальную цель.")
    add_lead(doc, "Шаг C — маршрут.", "BFS выбирает двери между комнатами, а Detour строит путь вокруг препятствий внутри текущей комнаты.")
    add_lead(doc, "Шаг D — физика.", "AI записывает walkDirection; CharacterControllerSystem применяет движение к Bullet capsule.")
    add_lead(doc, "Шаг E — анимация.", "AnimationController читает Idle/Walk/Run состояние движения.")

    doc.add_heading("Почему NPC застревал в дверном косяке", level=1)
    doc.add_paragraph(
        "Старый маршрут считал дверь пройденной, когда NPC оказывался около её центра. "
        "Следующей целью немедленно становилась пробоина. Если пробоина находилась сбоку от "
        "проёма, capsule начинала поворот на месте и задевала косяк."
    )
    add_callout(
        doc,
        "Исправление.",
        "Для каждой двери BFS теперь сохраняет комнату назначения. После открытия вычисляется "
        "doorExitPosition по направлению от двери к центру этой комнаты. NPC проходит "
        "doorExitDistance (по умолчанию 1,5 м) и только затем получает право повернуть к пробоине."
    )

    doc.add_heading("Как работает локальный NavMesh", level=1)
    add_lead(doc, "Запекание комнаты.", "SHIP_ROOM задаёт только логическую область. Реальная проходимая поверхность извлекается из треугольников физических SHIP_PANEL, поэтому углубления и перепады пола совпадают с Bullet-коллизией.")
    add_lead(doc, "Размер NPC.", "Проходимая область сжимается на радиус 0,42 м, поэтому путь держится дальше от мебели и стен.")
    add_lead(doc, "Поиск.", "Detour проецирует начало и цель на полигоны, строит corridor и превращает его в straight waypoints.")
    add_lead(doc, "Кэш.", "NavMesh запекается отдельно для каждой комнаты; путь хранится отдельно для каждого NPC.")
    add_lead(doc, "Fallback.", "Любая ошибка слоя возвращает false, после чего StateSystem использует прямой MoveTowards.")
    add_lead(doc, "Перестроение.", "Раз в 0,75 секунды система сравнивает дешёвую подпись статических коллайдеров комнаты. Если объект появился, исчез или переместился, автоматически перестраивается только эта комната и удаляются связанные пути. Debug-кнопка остаётся для принудительной проверки.")

    doc.add_heading("Лестницы без отдельного слоя Blender", level=1)
    doc.add_paragraph(
        "Раньше любая лестница попадала в сборщик как один большой красный AABB. "
        "Такой параллелепипед закрывал и ступени, и воздух над ними, поэтому голубая "
        "сетка обрывалась у нижней и верхней площадок. Теперь объекты, имя которых "
        "содержит STAIR, BRIDGE или RAMP, считаются навигационной поверхностью."
    )
    add_lead(doc, "Импортированная лестница.", "Для concave TriangleMesh берутся настоящие физические треугольники ступеней. Верхние грани с нормалью в пределах 45 градусов получают walkable area; вертикальные подступенки остаются твёрдыми, но непроходимыми.")
    add_lead(doc, "Редакторский StairRamp.", "Convex-форма сначала триангулируется через btShapeHull. Recast видит гладкий подъём, совпадающий с тем StairRamp, по которому Bullet ведёт CharacterController.")
    add_lead(doc, "Связность.", "Максимальный подъём одного voxel-перехода равен 0,45 м, высота NPC — 1,8 м, радиус — 0,42 м. Если ступень выше или проход слишком узкий, голубая сетка честно останется разорванной.")
    add_callout(doc, "Debug-проверка лестницы.", "После Rebuild статус содержит stairs=N и walkableStairs=M. Оба числа должны быть больше нуля, а голубые рёбра должны непрерывно соединять нижнюю и верхнюю площадки. Большой красный AABB вокруг всей лестницы больше не должен появляться.")

    doc.add_heading("Динамические объекты: локальный объезд без rebuild", level=1)
    doc.add_paragraph(
        "Катящийся шар, ящик или другое тело с ненулевой Bullet-массой нельзя запекать "
        "в постоянный NavMesh: его положение меняется каждый физический кадр. Вместо "
        "этого FindDynamicAvoidanceWaypoint выполняет короткий steering поверх жёлтого "
        "Detour-пути."
    )
    add_lead(doc, "Прогноз.", "Берётся AABB тела и его linearVelocity; центр препятствия прогнозируется на 0,35 секунды вперёд.")
    add_lead(doc, "Опасный коридор.", "Проверяются динамические тела впереди NPC на расстоянии до 3 м. Радиус AABB расширяется на 0,62 м — запас для капсулы персонажа.")
    add_lead(doc, "Сторона объезда.", "Сначала выбирается сторона, противоположная положению препятствия. Выбор запоминается на 0,9 секунды, чтобы NPC не дёргался влево-вправо перед движущимся шаром.")
    add_lead(doc, "Безопасная точка.", "Боковой кандидат проецируется на тот же связный остров NavMesh. Если первая сторона недоступна, проверяется противоположная.")
    add_lead(doc, "Debug.", "Красный AABB показывает обнаруженное динамическое тело, оранжевая линия — временную точку объезда, жёлтая линия — основной Detour-путь. В статусе появляется avoiding dynamic entity N.")
    add_callout(doc, "Граница решения.", "Это локальное уклонение, а не полноценный crowd simulator. Оно рассчитано на отдельные ящики и шары. Для десятков NPC и плотной толпы следующим расширением должен быть DetourCrowd/RVO.")

    doc.add_heading("Почему Wander больше не шагает на месте", level=1)
    add_lead(doc, "Проверка цели.", "Случайная точка из bounds комнаты принимается только после проекции на связанный доступный NavMesh. Выполняется до 12 попыток, минимальная дистанция до цели — 1 м.")
    add_lead(doc, "Детектор прогресса.", "Физическая дрожь меньше 8 см не считается перемещением. Если stuckTimeout истёк, Wander забывает плохую цель, делает короткую паузу и выбирает другую вместо бесконечного repath к той же координате.")

    doc.add_heading("Как полностью отключить Recast/Detour", level=1)
    doc.add_paragraph(
        "В DX_FrameWork.vcxproj установить NpcNavMeshEnabled=0. После этого заголовок "
        "NpcNavMeshSystem.h предоставляет inline-заглушки, Recast/Detour не линкуются, а "
        "NpcNavMeshSystem.cpp можно удалить из ClCompile. Остальные AI-файлы менять не нужно."
    )
    add_callout(
        doc,
        "Проверено сборкой.",
        "Проект успешно собирается как с NPC_NAVMESH_ENABLED=1, так и с "
        "NPC_NAVMESH_ENABLED=0. Текущий Debug-бинарник снова собран с включённым NavMesh."
    )

    doc.add_heading("Ограничения текущей версии", level=1)
    add_lead(doc, "Статические препятствия.", "Статические/кинематические коллайдеры запекаются в Recast; лестницы добавляют реальные поверхности, а динамические тела обрабатываются локальным прогнозируемым объездом.")
    add_lead(doc, "Консервативные AABB.", "Поворачиваемая мебель вырезается по мировому AABB. Это безопасно, но иногда оставляет больше пустого места, чем требуется.")
    add_lead(doc, "Комнатный уровень.", "NavMesh строится локально на комнату; межкомнатный выбор остаётся прозрачным BFS по DoorComponent.roomA/roomB.")
    add_lead(doc, "Runtime-редактирование.", "Изменение статической геометрии обнаруживается автоматически примерно за 0,75 секунды. Кнопка Rebuild all room NavMeshes нужна только для немедленного принудительного сброса или диагностики.")

    doc.add_heading("Диагностика в редакторе", level=1)
    doc.add_paragraph(
        "NPC Debug Control и инспектор показывают, включён ли слой, текст последнего "
        "результата, число полигонов комнаты, длину пути и индекс следующего waypoint. "
        "Force repath удаляет путь выбранного NPC; Rebuild all room NavMeshes удаляет весь кэш."
    )

    doc.add_page_break()
    doc.add_heading("Полный код основных AI-файлов", level=1)
    doc.add_paragraph(
        "Ниже приведён код непосредственно из рабочей копии проекта на момент создания "
        "документа. Нумерация слева добавлена только документом и не является частью файлов."
    )

    add_full_file(doc, "NPCComponents.h", "Словарь данных AI: состояния, команды, параметры движения, маршрут и debug-память.")
    add_full_file(doc, "NpcCommandSystem.h", "Единая точка выдачи приоритетных команд и сброса навигационной памяти.")
    add_full_file(doc, "NpcUtilitySystem.h", "Прозрачный выбор автоматической задачи через Utility score.")
    add_full_file(doc, "StateSystem.h", "Главный исполнитель задач, дверной BFS, безопасный выход и связь с NavMesh/Bullet.")
    add_full_file(doc, "src/Gameplay/AI/NpcNavMeshSystem.h", "Отключаемый интерфейс без типов Recast/Detour.")
    add_full_file(doc, "src/Gameplay/AI/NpcNavMeshSystem.cpp", "Полная реализация Recast build pipeline, Detour query и кэша путей.")
    add_full_file(doc, "src/ECS/System/DoorSystem.h", "Общий исполнитель состояния и анимации дверей.")
    add_full_file(doc, "src/Gameplay/Repair/RepairSystem.h", "Общий ремонт пробоин и устройств для игрока и NPC.")

    doc.add_heading("Интеграционные фрагменты", level=1)
    add_excerpt(doc, "src/Core/ApplicationClass.cpp", 884, 925, "Порядок систем AI, физики и анимации в игровом кадре.")
    add_excerpt(doc, "src/Editor/EditorUI.cpp", 466, 575, "Компактная debug-панель, включая NavMesh-статус и команды.")
    add_excerpt(doc, "src/Editor/EditorUI.cpp", 1008, 1175, "Полная секция NPC инспектора и runtime tuning.")
    add_excerpt(doc, "src/Scene/SceneManager.cpp", 2183, 2220, "Сериализация NPC и параметра безопасного выхода из двери.")
    add_excerpt(doc, "src/Scene/SceneManager.cpp", 3000, 3040, "Загрузка NPC-настроек из сцены.")

    doc.add_heading("Практический чек-лист проверки", level=1)
    add_lead(doc, "Комната.", "У NPC и цели корректно определяется currentRoom/room.")
    add_lead(doc, "Дверь.", "У DoorComponent заполнены roomA/roomB, а Transform находится в центре прохода.")
    add_lead(doc, "NavMesh.", "В панели написано Room NavMesh ready и polygonCount больше нуля.")
    add_lead(doc, "Лестница.", "В статусе stairs и walkableStairs больше нуля; голубая сетка соединяет обе площадки.")
    add_lead(doc, "Динамический объект.", "При пересечении пути движущимся телом появляется оранжевая точка объезда и статус avoiding dynamic entity.")
    add_lead(doc, "Путь.", "waypointCount больше нуля, next waypoint меняется во время движения.")
    add_lead(doc, "Проём.", "После открытия статус меняется на Clearing doorway; поворот начинается уже внутри комнаты.")
    add_lead(doc, "Ремонт.", "После подхода состояние становится Repairing, progress растёт до завершения.")
    add_lead(doc, "Fallback.", "При NavMesh=0 NPC всё ещё выполняет команды прямым движением.")

    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    doc.save(OUTPUT)
    print(OUTPUT)


if __name__ == "__main__":
    build()
