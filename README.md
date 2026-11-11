# DX Framework

Учебный игровой движок и редактор на DirectX 11 с ECS, Bullet Physics,
редактированием сцен и игровыми системами для симуляции космического корабля.

## Документация

Если открываете проект впервые или вернулись после перерыва, читайте в таком порядке:

1. [Большой учебник по исходному коду DX Framework (издание 8.0)](docs/DX_FRAMEWORK_ENGINE_TEXTBOOK_RU_8_0_EXPANDED_COURSE.docx) ([PDF](docs/DX_FRAMEWORK_ENGINE_TEXTBOOK_RU_8_0_EXPANDED_COURSE.pdf)) — математика и алгоритмы, шейдеры с нуля, подробный разбор систем движка, Bullet Physics, корабельные системы, ремонт и бой NPC, практикумы и единый план на 14 дней.
2. [Карта кода и методов](docs/CODE_METHOD_MAP.md) — куда идти для конкретной задачи и за что отвечают основные методы.
3. [Полный справочник](docs/DX_FRAMEWORK_FULL_DOCUMENTATION_RU.md) — gameplay, корабельные системы, диагностика и развитие живого мира.
4. [Editor & Gameplay Guide](docs/DX_FRAMEWORK_EDITOR_GAMEPLAY_GUIDE_RU.md) — работа с редактором и готовыми игровыми компонентами.

Главное практическое руководство по редактору и gameplay-системам:

- [DX Framework: Editor & Gameplay Guide (RU)](docs/DX_FRAMEWORK_EDITOR_GAMEPLAY_GUIDE_RU.md)

В нём описаны интерфейс редактора, Quick Create, компоненты, no-code
Trigger Zone/Trigger Action, NPC и автономный экипаж, HUD, инвентарь,
GameTimer, формат сцен JSON v2, практические рецепты и диагностика.

## Основные возможности

- Editor/Play Mode, Outliner, Inspector, Transform gizmo и snapping;
- data-driven триггерные зоны с действиями без написания C++;
- взаимодействие с устройствами и NPC;
- автономное распределение аварийных задач между членами экипажа;
- sci-fi HUD игрока, состояние атмосферы и корабля;
- инвентарь на три оружейных слота, подбор и выброс винтовок;
- пауза и масштаб времени симуляции;
- сохранение gameplay-компонентов и неактивных сущностей в JSON v2.

## Проверка проекта

Из PowerShell в корне проекта:

```powershell
.\scripts\verify_engine.ps1 -Configuration Debug
```

Скрипт проверяет shader entry points, собирает движок и запускает core
contract tests. Для Debug и Release используйте `-Configuration All`.
