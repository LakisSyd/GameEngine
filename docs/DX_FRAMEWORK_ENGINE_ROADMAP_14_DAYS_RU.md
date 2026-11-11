# DX Framework: 14 дней развития именно движка

Издание 1.0  
Срез: 24 августа 2026 года  
Фокус: ядро, рендер, физика, ECS, ресурсы, build, tests и tooling — **без добавления игрового контента**

> Правило программы: каждый день заканчивается проверяемым engine-артефактом. Новая игровая механика не считается результатом, если она не улучшает повторно используемую подсистему движка.

---

## Как проходить roadmap

На день закладывается 3–6 часов. Перед началом создать отдельную ветку. В конце дня:

1. собрать Debug и Release x64;
2. запустить engine regression scene;
3. записать измерение «до/после»;
4. сделать один тематический commit;
5. обновить known limitations.

Если критерий приёмки не пройден, следующий день не начинает зависимую тему.

## День 1. Воспроизводимая сборка и runtime layout

### Цель

Сделать так, чтобы другой компьютер или вы через год могли собрать и запустить движок по одной инструкции.

### Теория

Build configuration — часть архитектуры. Debug и Release должны различаться оптимизацией и диагностикой, а не случайным набором include/lib paths.

### Задания

1. Вынести общие x64 include directories, C++20 и compile flags в общий property sheet.
2. Настроить отдельные Debug/Release имена Bullet/Assimp/Recast libraries.
3. Добавить проверку обязательных NuGet/vcpkg dependencies с понятной ошибкой.
4. Определять runtime root от пути EXE, а не случайного current working directory.
5. Копировать нужные DLL и минимальный набор assets в output автоматически.
6. Написать `BUILDING.md` с одной командой для каждой конфигурации.

### Артефакт

Чистая папка `x64/Debug` и `x64/Release`, каждая запускается вне Visual Studio.

### Готово, если

- Debug x64 и Release x64 собираются с нуля;
- EXE запускается из своей output-папки;
- отсутствующая зависимость сообщает имя и ожидаемый путь;
- никакой абсолютный путь пользователя не обязателен.

### Проверка

Переименовать локальную build-папку, клонировать/скопировать проект в новый путь с пробелами, собрать обе конфигурации.

### Stretch

Добавить preset для AddressSanitizer или `/fsanitize=address`, если используемый MSVC/toolchain поддерживает нужный сценарий.

## День 2. Test target и безопасный lifecycle

### Цель

Получить автоматический сигнал о поломке системных контрактов и доказать, что частичная инициализация завершается безопасно.

### Теория

Модульный тест проверяет чистую функцию; integration test — связь подсистем; smoke test — что приложение вообще живо. Failpoint искусственно останавливает `Initialize` после выбранного этапа; идемпотентный `Shutdown` безопасен даже после частичного старта и повторного вызова.

### Задания

1. Добавить отдельный console test project в solution.
2. Выбрать лёгкий test framework или минимальный внутренний harness без GPU.
3. Добавить pure-engine тесты vectors/matrices, ECS lifecycle/remap, `FileSystem`, resource states и Bullet-примитивов.
4. Создать процедурную regression fixture: примитивы, технические камеры, свет, opaque/transparent материалы и физические тела — без игровой сцены.
5. Добавить failpoints после этапов Application, D3D, JobSystem, Bullet и scene initialization.
6. Проверить `Shutdown` после каждого failpoint и два последовательных вызова `Shutdown`.
7. Добавить headless-friendly exit code для smoke run и запуск одной командой после build.

### Артефакт

`DXFramework.Tests.exe`, failpoint harness и процедурная regression fixture.

### Готово, если

- минимум 20 быстрых тестов проходят менее чем за 2 секунды;
- намеренно сломанный знак матрицы или lifecycle invariant ломает тест;
- каждый failpoint завершается без crash, double free и live registration;
- двойной `Shutdown` безопасен;
- smoke fixture запускается и завершается предсказуемо.

### Проверка

Сделать одну контролируемую ошибку, увидеть красный тест, вернуть код, увидеть зелёный.

### Stretch

Сохранить JUnit/XML-отчёт для будущего CI.

## День 3. Единый контракт математики Transform

### Цель

Устранить расхождение render/Bullet/editor в углах и порядке вращений.

### Теория

Euler rotations не коммутируют. Quaternion должен быть нормализован; editor degrees не должны незаметно попадать в runtime radians.

### Задания

1. Записать официальный basis: LH, +Y up, +Z forward, row-vector.
2. Выбрать один runtime orientation contract.
3. Централизовать Euler ↔ quaternion ↔ matrix.
4. Убрать дубли world matrix из legacy `GameObject` или направить его в общий helper.
5. Добавить normal matrix inverse-transpose.
6. Добавить тесты составных pitch/yaw/roll и non-uniform scale.

### Артефакт

`TransformMath`/`MathConventions` module и тестовая сцена осей.

### Готово, если

- render mesh и Bullet collider совпадают при трёх ненулевых углах;
- camera/editor явно конвертируют degrees;
- normal lighting корректен при scale `(2,1,0.5)`;
- ни одна подсистема не собирает собственный порядок Euler.

### Проверка

Наложить wireframe collider на render mesh и вращать по всем осям.

### Stretch

Ввести strong types `Radians` и `Degrees`.

## День 4. ECS lifecycle и безопасные структурные изменения

### Цель

Сделать Entity устойчивой к удалению и повторному использованию.

### Теория

Entity index без generation не защищает от stale handle. Изменение `unordered_map` во время iteration может инвалидировать iterator.

### Задания

1. Ввести index + generation или отдельную generation table.
2. Добавить `IsAlive` и явный entity count.
3. Разделить `FindPool` и `GetOrCreatePool`, чтобы чтение не создавало pool.
4. Добавить deferred create/add/remove/destroy command buffer.
5. Запретить structural mutation внутри прямого `ForEach` или выполнять snapshot iteration.
6. Проверить все сохранённые Entity references.

### Артефакт

ECS v2 с тестами stale handles и deferred destroy.

### Готово, если

- удалённый handle никогда не видит новую сущность;
- удаление текущей entity в system callback безопасно;
- `HasComponent<NeverUsed>` не меняет registry;
- save/load remap проходит тесты.

### Проверка

Создать/удалить миллион entities, повторно использовать индексы и проверить поколения.

### Stretch

Добавить system query cache/signature bitsets.

## День 5. Scheduler, clocks и fixed simulation

### Цель

Сделать порядок систем и время явными данными движка.

### Теория

Нужны unscaled real time, scaled game time и fixed physics time. Continuous forces применяются на каждом fixed tick.

### Задания

1. Объединить обязанности `TimeManager` и `GameTimer` в один clock service.
2. Выделить phases: Input, PrePhysics, FixedPhysics, PostPhysics, RenderPrep, Render, UI.
3. Ограничить variable dt.
4. Перенести buoyancy/drag/continuous forces внутрь fixed loop.
5. Интерполировать обычные rigid bodies через alpha.
6. Добавить deterministic tick/world timestamp.

### Артефакт

Frame scheduler с логом фаз и единым time snapshot.

### Готово, если

- 60 секунд симуляции при 30/60/144 FPS дают результат с отклонением ≤1%;
- pause останавливает game time, но UI продолжает работать;
- step выполняет ровно один fixed tick;
- порядок систем виден в одном месте.

### Проверка

Запустить одинаковую seed-сцену на трёх frame caps и сравнить telemetry.

### Stretch

Сделать replay входов по tick.

## День 6. Bullet ownership и collision correctness

### Цель

Закрыть dangling objects, offset bounds и неработающие shape modes.

### Теория

Bullet world не владеет автоматически всеми переданными объектами. Constraint/action нужно удалить из world до `delete`.

### Задания

1. Ввести RAII wrappers для body, ghost/action и constraint registration.
2. Сделать owning components move-only без пропущенных полей.
3. Учитывать bounds center для box/sphere/compound children.
4. Ограничить capsule cylinder height снизу.
5. Применять `friction`, damping, angular factor и CCD из компонента.
6. Починить или удалить заявленный VHACD mode.
7. Зарегистрировать interior collider в world с явными group/mask.

### Артефакт

Physics ownership layer и collision regression scene.

### Готово, если

- 100 циклов load/unload не оставляют Bullet objects;
- pivot-offset mesh имеет совпадающий collider;
- каждый editor parameter реально меняет Bullet body;
- несуществующий VHACD не показывается как рабочая опция.

### Проверка

Показывать counters world objects/actions/constraints до и после reload.

### Stretch

Добавить collision shape asset cache.

## День 7. Render graph-lite и состояние проходов

### Цель

Превратить ручную последовательность render calls в проверяемые pass contracts.

### Теория

Каждый pass должен объявлять render target, depth/blend/raster state, inputs, camera и cleanup.

### Задания

1. Описать Shadow, Reflection, Refraction, Main, Transparent, Particle, Debug, FirstPerson как pass descriptors.
2. Добавить scoped state restoration.
3. Валидировать обязательные camera/light/animation inputs.
4. Удалить неинициализированные массивы refraction.
5. Передавать reflected camera position в reflection.
6. Добавить pass timings и draw counters.

### Артефакт

Render graph-lite/debug panel со списком проходов.

### Готово, если

- каждый pass показывает свои targets/states;
- D3D debug layer не сообщает hazard/warning;
- отключение pass не оставляет состояние следующему;
- refraction/reflection PBR использует правильную камеру и свет.

### Проверка

Автоматически сохранить baseline screenshots каждого прохода.

### Stretch

Добавить transient render texture pool.

## День 8. Прозрачность и material contract

### Цель

Сделать прозрачные и alpha-tested материалы предсказуемыми.

### Теория

Классическая alpha transparency требует back-to-front, depth test on, depth write off. Alpha cutout остаётся в opaque/depth/shadow passes.

### Задания

1. Ввести material blend mode: Opaque, Masked, Blend.
2. Создать depth-read/no-write state.
3. Сортировать transparent draws back-to-front.
4. Разделять embedded model по mesh parts, а не по флагу всего объекта.
5. Передавать animation/bone buffers в transparent/refraction/shadow.
6. Добавить alpha cutoff в main и shadow shaders.

### Артефакт

Material pipeline и сцена пересекающихся стеклянных/skinned объектов.

### Готово, если

- непрозрачные части mixed-модели остаются в opaque pass;
- прозрачный объект не закрывает дальний depth своей невидимой частью;
- skinned glass совпадает с основной моделью;
- foliage mask отбрасывает корректную тень.

### Проверка

Вращать камеру вокруг трёх пересекающихся прозрачных поверхностей.

### Stretch

Исследовать weighted blended OIT как отдельный эксперимент.

## День 9. HDR/PBR/IBL correctness

### Цель

Собрать единый linear HDR pipeline.

### Теория

Lighting выполняется в linear space; tone mapping и gamma — один раз после всей HDR-сцены. Split-sum IBL требует irradiance, prefiltered specular и BRDF LUT.

### Задания

1. Рендерить main scene в HDR target.
2. Убрать per-material tone mapping/gamma из PBR draw.
3. Добавить общий exposure/tone-map postprocess.
4. Синхронизировать orientation HDR sky и reflections.
5. Создать white albedo fallback и определить sRGB formats.
6. Подготовить irradiance/prefilter/BRDF LUT pipeline.

### Артефакт

HDR postprocess и calibrated material test scene.

### Готово, если

- tone mapping вызывается ровно один раз;
- sky и отражение смотрят в одну сторону;
- roughness реально размывает environment specular;
- нетекстурный PBR материал не чёрный;
- reference spheres выглядят стабильно при разном exposure.

### Проверка

Сравнить dielectric/metal spheres по сетке roughness × metallic.

### Stretch

Добавить bloom после threshold в HDR.

## День 10. Shadow system

### Цель

Сделать тени стабильными возле камеры и корректными для анимации.

### Теория

Directional shadow volume должен следовать camera frustum; texel snapping уменьшает shimmering; caster culling строится по light frustum.

### Задания

1. Центрировать shadow volume по camera frustum, а не по world origin.
2. Делать caster culling из light frustum.
3. Добавить texel snapping и raster depth bias.
4. Передавать skinning в shadow vertex shader.
5. Поддержать alpha-masked casters.
6. Убрать жёстко прошитый размер shadow map из PCF.

### Артефакт

Shadow module с debug frustum и bias controls.

### Готово, если

- тени не исчезают далеко от `(0,0,0)`;
- объект за камерой может отбрасывать видимую тень;
- animated shadow совпадает с позой;
- медленное движение камеры не создаёт заметного shimmering.

### Проверка

Пройти 500 метров от origin рядом с движущимся skinned character.

### Stretch

Реализовать 3–4 cascade CSM.

## День 11. Asset pipeline, async safety и безопасный FileSystem

### Цель

Отделить CPU asset data, GPU upload и lifetime/cancellation.

### Теория

D3D immediate context и ECS mutation принадлежат main thread. Worker возвращает immutable CPU result с generation token. Канонический идентификатор ресурса и проверка containment не позволяют разным написаниям пути создать дубли или выйти за asset root.

### Задания

1. Ввести AssetHandle/state machine: Queued, LoadingCPU, WaitingGPU, Ready, Failed, Cancelled.
2. Добавить cancellation и scene generation token.
3. Перехватывать исключения jobs и гарантировать decrement counters.
4. Выбирать worker count от hardware concurrency.
5. Убрать захват живых raw `this/Entity` без lifetime guard.
6. Добавить asset error object вместо только debug string.
7. Объединить конкурирующие cache/ownership paths `SceneManager`, `ModelManager` и `AssetsLoadSystem` вокруг одного resource registry.
8. Нормализовать case/slashes и отклонять absolute paths, `..` и выход через junction/reparse point в destructive `FileSystem` API.

### Артефакт

Resource registry, асинхронная загрузка с progress/cancel/error panel и набор path-security tests.

### Готово, если

- load scene во время 250-МБ import не добавляет объекты в новую сцену;
- исключение job не завершает process и не зависает `Wait`;
- GPU resources создаются только на main thread;
- shutdown во время загрузки чистый;
- варианты одного пути дают один ресурс;
- 20 load/unload циклов возвращают CPU/GPU counters к baseline;
- destructive operation не может затронуть файл вне asset root.

### Проверка

100 раз начать import и отменить на случайном проценте; затем прогнать absolute/`..`/junction/reparse traversal cases.

### Stretch

Добавить content hash и disk cache CPU preprocessing.

## День 12. Serialization v2 и транзакционный scene/runtime state

### Цель

Сделать загрузку безопасной и версионируемой.

### Теория

Scene authoring data и runtime save имеют разные требования. Запись должна быть атомарной; загрузка — validate-before-destroy.

### Задания

1. Разделить SceneAsset и RuntimeSave schemas.
2. Ввести обязательную version и migration table.
3. Писать temp → flush → atomic replace.
4. Сначала parse/validate, только затем очищать текущую сцену.
5. Централизовать Entity remap/validation.
6. Сериализовать WorldSettings, WorldArea, material modes и все owning references.
7. Добавить round-trip tests.

### Артефакт

Schema v2, migration v1→v2 и corruption tests.

### Готово, если

- повреждённый JSON не уничтожает открытую сцену;
- duplicate entity не появляется после round trip;
- ссылки либо remap, либо становятся `INVALID_ENTITY` с диагностикой;
- старый v1 файл загружается;
- последняя рабочая запись переживает сбой во время save.

### Проверка

Обрезать файл на случайном байте и попытаться загрузить.

### Stretch

Добавить human-readable JSON schema validation report.

## День 13. Profiling, diagnostics и resource leak gates

### Цель

Сделать слабые места измеримыми.

### Теория

Без budgets оптимизация превращается в угадывание. Нужны CPU phase time, draw calls, candidates/culled, GPU time и resource counters.

### Задания

1. Добавить scoped CPU profiler по system/render phase.
2. Добавить GPU timestamp queries для основных passes.
3. Показывать ECS counts, Bullet objects/actions/constraints, jobs, assets, D3D resources.
4. Ввести frame budget warnings.
5. Сделать 30-минутный soak сценарий.
6. Зафиксировать baseline для трёх сцен.

### Артефакт

Engine diagnostics dashboard и performance baseline JSON.

### Готово, если

- известен top-10 CPU/GPU cost;
- reload/load-unload не даёт монотонного роста counters;
- warning объясняет превышенный budget;
- regression сравнивается с baseline численно.

### Проверка

30 минут автоматического camera loop + spawn/despawn + scene reload.

### Stretch

Экспортировать Chrome tracing JSON.

## День 14. Engine release gate и freeze

### Цель

Выпустить не игру, а проверяемую версию движка.

### Теория

Engine release — это стабильный набор контрактов, samples, tests и known limitations.

### Задания

1. Заморозить API на один release candidate.
2. Сделать обязательный CI gate: Debug/Release x64, tests, offline HLSL validation и package artifact.
3. Добавить единый entry point `verify_engine` для unit, integration, smoke и soak checks.
4. Подготовить три чистых samples: `RenderLab`, `PhysicsLab`, `ECSStress`.
5. Проверить запуск из clean output.
6. Убрать мёртвые опции и устаревшие shader/code copies.
7. Составить changelog и known limitations.
8. Поставить локальный tag `engine-v0.2` только после зелёных проверок.

### Артефакт

DX Framework Engine v0.2: binaries, samples, tests, diagnostics и release notes.

### Готово, если

- обе x64-конфигурации зелёные;
- минимум 20 критических тестов и offline HLSL validation зелёные;
- 30-минутный soak чистый;
- samples не зависят от игровой сцены;
- новый разработчик понимает запуск менее чем за 15 минут;
- каждое оставшееся ограничение записано явно;
- CI воспроизводит весь release gate на чистом worker.

### Проверка

Скопировать release package в новый каталог и выполнить полный acceptance checklist без Visual Studio IDE.

### Stretch

Добавить portable symbols и автоматический crash-dump package.

## Итоговая Definition of Done

Через 14 дней движок должен быть не «богаче фичами», а надёжнее:

- воспроизводимая Debug/Release сборка;
- единые math/time contracts;
- безопасные ECS и Bullet lifetimes;
- явные render passes;
- корректный HDR/material/shadow pipeline;
- отменяемая async загрузка;
- versioned atomic serialization;
- тесты, profiler и soak gates;
- отдельные engine samples.

Если какой-то пункт не завершён, его нельзя скрывать новой игровой механикой: он переносится в known limitations с воспроизводимым тестом.
