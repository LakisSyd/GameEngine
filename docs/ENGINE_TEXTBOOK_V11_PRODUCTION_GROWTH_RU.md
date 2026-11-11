# Как превратить растущий проект в устойчивый движок

Новые возможности становятся полезными только тогда, когда их можно повторяемо загрузить, проверить, отладить и изменить без цепной поломки. Эта глава собирает инженерные улучшения, которые особенно пригодятся DX Framework на следующем этапе.

## 1 Вертикальный срез вместо большого слоя

Вертикальный срез проходит через все необходимые уровни одной маленькой способности.

Например, «NPC ищет оружие»:

- компонент знания о pickup;
- чистая оценка кандидата;
- reservation;
- action и навигация;
- штатный `InventorySystem::PickupWeapon`;
- animation request;
- Inspector и журнал;
- сохранение только долгоживущих данных;
- один scenario test.

Горизонтальный подход сначала пишет огромный универсальный planner, потом универсальные actions, а увидеть результат можно через месяц. Вертикальный срез раньше выявляет плохие границы.

## 2 Архитектурная запись решения

Для заметного изменения полезен короткий ADR-файл:

```text
Название: Кто владеет NpcIntent
Контекст: cover и combat пишут state независимо
Решение: только GoalArbiter меняет goal, executors меняют phase
Альтернативы: последний writer wins; единая огромная система
Последствия: нужен feedback и migration старых states
```

ADR не должен быть бюрократией. Пять абзацев сохраняют причину через месяц, когда код уже не объясняет, почему выбран именно такой путь.

## 3 Data driven настройки

Числа тактики не стоит размазывать по системам. Профиль может хранить:

- preferred range;
- aggression и morale baseline;
- cover weights;
- burst policy;
- допустимые цели;
- animation semantics;
- rules of engagement.

Runtime component хранит выбранный profile id и изменяемое состояние. JSON schema имеет version. При загрузке значения clamp и validate.

Data driven не означает «вынести каждый bool в JSON». Инварианты и алгоритмы остаются в C++. В данные уходят параметры и выбор модулей.

## 4 Semantic names

Система становится хрупкой, если gameplay знает конкретный clip `Kevin_Melee_Jab_L`. Лучше semantic action:

```text
Locomotion.Run.Rifle
Combat.Peek.Left.Stand
Interaction.Pickup.Weapon
Compliance.Surrender
Compliance.Restrain
```

Профиль модели связывает semantic с clip. NPC action публикует semantic. Animation controller выбирает доступный clip и fallback. Animation Studio редактирует связь.

Тот же подход полезен для shader/material features и audio events.

## 5 События и команды

Событие сообщает, что произошло. Команда просит что-то сделать.

- `DamageApplied` — событие прошлого факта.
- `RequestReload` — команда.
- `WeaponReloaded` — событие результата.
- `TargetSpotted` — событие perception.
- `MoveIntent` — команда movement executor.

Смешивание приводит к циклам. Если `DamageApplied` сразу телепортирует NPC в cover, оно перестаёт быть событием и скрывает решение.

Event queue должна иметь понятный момент доставки. Событие этого кадра либо обрабатывается позже в том же кадре по документированному порядку, либо на следующем тике. Иначе появляются ошибки «иногда на один кадр».

## 6 Версии компонентов и сцен

Сцена живёт дольше структуры C++. Нужны:

- номер версии scene format;
- версия больших component payload;
- значения по умолчанию для отсутствующих полей;
- явная migration для изменённого смысла;
- warning для неизвестных полей или enum;
- round-trip test load → save → load.

Нельзя полагаться только на то, что JSON библиотека проигнорирует отсутствующее поле. Если раньше `isHostile` определял врага, а теперь это делает `FactionComponent`, migration должна назначить faction осмысленно или сообщить о ручной проверке.

## 7 Asset database

По мере роста файлов полезен реестр assets:

```text
AssetId
source path
type
content hash
dependencies
import settings
runtime resource state
last error
```

Scene сохраняет стабильный AssetId, а не случайный абсолютный путь. Importer отслеживает texture dependencies. Изменение файла инвалидирует только зависящие assets.

Asset database также позволяет найти, какие сцены используют модель перед несовместимым изменением skeleton.

## 8 Асинхронная загрузка

Полезное разделение:

- IO и декодирование на worker;
- создание Direct3D resources на render/main thread;
- atomic публикация готового handle;
- fallback asset до готовности;
- cancel, если requester уничтожен.

Состояния:

```text
Unloaded → Queued → LoadingCPU → WaitingGPU → Ready
                                  ↘ Failed
```

Нельзя отдавать наружу наполовину созданный mesh. Handle возвращает Ready resource или fallback.

## 9 Handles вместо сырых указателей

Модель или texture может hot reload. Сырой pointer в сотне компонентов легко становится висячим. Handle содержит id и generation. Resource manager проверяет generation и возвращает актуальный объект.

ECS Entity уже использует похожую идею идентичности. Для assets она особенно важна при асинхронности.

## 10 Renderer graph и проходы

Когда renderer растёт, список вызовов превращается в неявную зависимость ресурсов. Render graph описывает passes:

- shadow;
- opaque PBR;
- transparent;
- particles;
- debug;
- postprocess;
- UI.

Каждый pass объявляет inputs/outputs. Graph вычисляет порядок и время жизни временных targets. Для небольшого DirectX 11 renderer не нужен универсальный фреймворк сразу. Начать можно с явной структуры `RenderFramePlan` и списка ресурсов.

## 11 Material feature set

Один гигантский shader со всеми bool ветками становится дорогим и трудным. Тысячи полностью отдельных файлов тоже неудобны.

Компромисс:

- небольшой набор shader families;
- compile-time defines для крупных features;
- runtime parameters для дешёвых изменений;
- cache варианта по feature mask;
- fallback variant при ошибке.

Feature mask может включать skinning, normal map, alpha test, damage overlay и shadow receiving. Вариант компилируется и отражается один раз.

## 12 Профилирование

Измерять надо время и количество работы.

CPU markers:

- total frame;
- AI sensing;
- goal arbitration;
- NavMesh queries;
- physics step;
- animation sampling;
- render submission.

Счётчики:

- NPC evaluated;
- visibility rays;
- cover candidates;
- path rebuilds;
- draw calls;
- vertices/triangles;
- shader switches;
- constant buffer updates;
- loaded texture memory.

Среднее скрывает spike. Нужны median, 95/99 percentile и worst recent frame.

## 13 Budget мышление

Вместо «оптимизировать AI» задаётся бюджет:

```text
60 FPS frame: 16.67 ms
AI total target: 2.0 ms
visibility rays per frame: 40
new path queries per frame: 4
cover full searches per frame: 2
```

Requests ставятся в очередь по срочности. Emergency target visibility выше фонового wander sensing. Если budget закончился, неэкстренное решение использует свежий предыдущий результат с возрастом.

## 14 Fixed timestep и replay

Physics и важная симуляция стабильнее на фиксированном шаге. Rendering интерполирует между состояниями.

Для replay записываются:

- initial scene hash;
- fixed timestep;
- random seeds;
- команды игрока и Studio;
- внешние события;
- важные version numbers.

Полный Transform каждого кадра записывать необязательно. При детерминированной симуляции достаточно входов. Bullet может иметь платформенные различия, поэтому для тестов полезны tolerance и high-level invariants.

## 15 Debug draw как API

Сейчас tactical component хранит векторы debug rays. При росте лучше общий временный DebugDrawQueue:

```cpp
DebugLine{from, to, color, lifetime, category, entity}
DebugSphere{center, radius, color, lifetime, category, entity}
DebugText{position, text, category, entity}
```

Категории: perception, cover, navigation, weapon, animation IK, physics. UI включает фильтр. Очередь не сохраняется в scene и имеет предел памяти.

## 16 Ошибки кодировки и документации

В текущих shader/model исходниках встречаются повреждённые комментарии. Это сигнал не только о тексте, но и о toolchain.

Рекомендуется:

- сохранить исходники UTF-8;
- включить `/utf-8` в MSVC;
- добавить `.editorconfig`;
- не смешивать OEM/Windows-1251;
- проверять JSON и logs на UTF-8;
- постепенно исправлять комментарии рядом с изменяемым кодом, не выполняя огромную механическую замену без diff review.

## 17 Автоматические проверки

Минимальная лестница:

1. форматирование изменённых файлов;
2. compile headers/targets;
3. unit tests чистых rules;
4. asset validation;
5. headless scenario ticks;
6. render smoke scene;
7. ручная проверка Studio.

Проверка должна соответствовать риску. Изменение текста книги не требует сборки игры. Изменение HLSL требует compile всех variants и эталонного кадра.

## 18 Golden scenes

Небольшие сцены, каждая проверяет один контракт:

- PBR spheres с сеткой roughness/metalness;
- skinned skeleton и extreme pose;
- shadows и transparent glass;
- NPC line of sight behind cover;
- two cover sides and low cover;
- weapon pickup competition;
- surrender/capture;
- doors/NavMesh traversal.

Сцена должна быстро загружаться и иметь фиксированную камеру. Она является лабораторным прибором, а не красивым уровнем.

## 19 Работа с большим компонентом

`NpcTacticalComponent` уже хранит настройки, runtime timers, debug rays, group fields и status. Он полезен, но будет расти.

Безопасное разбиение по владельцам:

- `NpcTacticalProfileComponent` — авторские параметры;
- `NpcTacticalRuntimeComponent` — фазы, timers, current cover;
- `NpcPerceptionMemoryComponent` — контакты;
- `NpcIntentComponent` — goal/action/reason;
- `NpcReservationComponent` — handles;
- `NpcDebugComponent` — optional debug state.

Разбивать надо не по числу строк, а когда поля имеют разные жизненные циклы и писателей. Миграция выполняется вертикально, по одной группе.

## 20 Код и комментарии

Комментарий должен объяснять причину, единицы, владельца или неочевидный контракт.

Плохой:

```cpp
// Уменьшаем таймер.
timer -= dt;
```

Хороший:

```cpp
// Commitment использует simulation time. Насыщение до нуля позволяет
// всем правилам проверять один порог даже после длинного кадра.
timer = std::max(0.0f, timer - std::max(0.0f, dt));
```

Построчные комментарии полезны в учебной копии, но production-код не должен повторять синтаксис каждой строки. Имена и маленькие функции несут основное объяснение.

## 21 API редакторских инструментов

EditorUI сейчас большой и содержит много логики Studio. Следующий шаг — отделить модели инструментов:

- `AnimationStudioSession` хранит selection, playhead, edit layer, undo stack;
- `AnimationProfileSerializer` читает/пишет versioned JSON;
- `AnimationStudioPanel` только рисует UI и отправляет команды session;
- `NpcStudioSession` управляет capture/filter/export;
- `NpcStudioPanel` отображает snapshot;
- viewport gizmo adapter преобразует координаты.

Так UI можно перестраивать без изменения runtime animation.

## 22 Undo redo

Редакторская операция оформляется командой с `Do` и `Undo`:

```text
MovePoseKey
AddPoseKey
DeletePoseKey
ChangeClipBinding
ChangeWeaponGrip
```

При drag gizmo не надо создавать сотни команд. На mouse down запоминается before, во время drag идёт preview, на mouse up фиксируется одна команда after.

## 23 Идеи новых учебных систем

### 23.1 Sound propagation

Звук имеет источник, громкость, категорию и затухание через комнаты/двери. NPC получает неопределённый район, не точную позицию. Это отличная практика графов, perception memory и отладки.

### 23.2 Suspicion

Подозрение растёт от частичных наблюдений и падает со временем. Порог меняет idle → investigate → confirmed combat. Оно связывает stealth и существующий search без всезнания.

### 23.3 Smart objects

Дверь, турель, аптечка и терминал публикуют actions с preconditions, duration и slots. Planner использует общий интерфейс взаимодействия.

### 23.4 Tactical influence map

Сетка хранит видимость, опасность, союзную поддержку и звук. Это не истина NPC автоматически: карта строится из его памяти или является командным ресурсом с задержкой.

### 23.5 Damage anatomy

Отдельные injuries влияют на скорость, aim и stamina, не превращая здоровье в единственный показатель. Начинать стоит с двух-трёх gameplay-эффектов, а не полной медицинской симуляции.

## 24 Долгий roadmap

### Фаза A Наблюдаемость

- единая таблица владельцев AI полей;
- intent и action в NPC Studio;
- structured failure;
- общий debug draw;
- фиксированные scenario scenes.

### Фаза B Ресурсы и действия

- reservation service;
- acquire weapon vertical slice;
- action channels;
- semantic animation requests;
- stamina.

### Фаза C Социальное поведение

- morale;
- surrender;
- rules of engagement;
- capture and escort;
- group overwatch proposal.

### Фаза D Контент и renderer

- asset validator;
- shader reflection;
- material variants;
- transactional hot reload;
- GLB dependency database.

### Фаза E Масштабирование

- profiling budgets;
- spatial target queries;
- scheduled sensing;
- replay;
- selective multithreading.

## 25 Как самому изучать любую новую область

Используй цикл:

1. Нарисовать поток данных.
2. Найти владельца каждого состояния.
3. Выписать инварианты и единицы.
4. Пройти один конкретный пример вручную.
5. Добавить наблюдаемость.
6. Сделать минимальный вертикальный срез.
7. Проверить крайние случаи.
8. Только затем обобщать.

Этот метод одинаково работает для шейдера, импорта, физики, AI и editor tool.

## 26 Финальный чек лист новой системы

- Есть одно предложение о назначении системы.
- Определены входы и выходы.
- Известен единственный владелец изменяемого состояния.
- Отсутствующие данные имеют явный результат.
- Используются правильные часы и единицы.
- Действие можно прервать по документированной политике.
- Ошибка возвращается как данные.
- Состояние диагностируется в Studio или логе.
- Есть маленькая тестовая сцена.
- Сохранение и migration продуманы.
- Нет скрытого чтения знания, которого персонаж не получал.
- UI не подменяет runtime pipeline.
- Оптимизация основана на измерении.

Когда эти пункты выполняются, система обычно остаётся понятной даже после добавления следующих пяти возможностей.

