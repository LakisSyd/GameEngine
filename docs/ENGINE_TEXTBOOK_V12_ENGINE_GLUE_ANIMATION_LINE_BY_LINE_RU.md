# Application ECS input scene и animation runtime

Эта глава закрывает связующий код движка. Методы Editor UI, gizmo, drag and drop и editor-only spawning намеренно не разбираются. В `ApplicationClass` объясняется только runtime-путь кадра и общие ресурсы. Импорт моделей и Studio уже подробно разобраны в предыдущих главах книги; здесь добавлен порядок исполнения конкретных методов.

## 1 ApplicationClass почему он сейчас слишком большой

`ApplicationClass` одновременно инициализирует сервисы, управляет режимами, вызывает gameplay systems и строит render frame. Это рабочая composition root, но уже не удобное место для новых механик.

Правило чтения файла:

1. Поля `unique_ptr` обычно означают владение.
2. Raw pointers на device resources требуют явного `Shutdown` или `Release`.
3. ECSRegistry принадлежит SceneManager.
4. Frame не должен становиться владельцем состояния NPC, оружия или комнаты; он только задаёт порядок.

## 2 ApplicationClass Initialize

### 2.1 InitializationGuard

1. Локальный guard получает `owner` и `committed = false`.
2. Если любой следующий шаг вернёт false, destructor guard вызовет `Shutdown`.
3. Только в самом конце успешной инициализации `committed` становится true.

Это транзакция ресурсов: частично созданное приложение не оставляет D3D и Bullet объекты.

### 2.2 Графический фундамент

1. Сохраняются width, height и HWND.
2. Создаётся `D3DClass` и Direct3D device.
3. Создаются camera и camera controller.
4. Подготавливаются shader wrappers, shadow map и render textures.
5. Default material и default texture обеспечивают fallback для неполного asset.
6. Physics debug и particle presentation получают device, но не владеют им.

### 2.3 Physics и ECS services

1. `PhysicsSystem::InitializeWorld()` создаёт Bullet singleton.
2. SceneManager получает device, context, default material и asset services.
3. GameplayController связывается с registry и input.
4. JobSystem запускает ограниченное число workers.
5. AssetLoadSystem получает main-thread upload context.

### 2.4 Scene и gameplay content

1. Выбирается стартовая scene или combat arena.
2. `LoadScene` создаёт ECS entities и запускает asset paths.
3. Player spawn выполняется только для режима, где игрок нужен. NPC-only симуляция не обязана иметь player.
4. Weapon visuals создаются после inventory и model availability.
5. Skybox загружается через отдельный SRV path.

## 3 ReloadSkyBox

1. Новый HDR или cube asset сначала загружается во временный SRV.
2. Если загрузка не удалась, старый sky остаётся рабочим.
3. Только успешный new SRV заменяет `m_skySRV`.
4. Старый SRV освобождается после замены.
5. Sky shader и PBR shader получают новую environment texture.

Это правильный commit pattern: пользователь не остаётся без неба из-за ошибочного нового файла.

## 4 ApplicationClass Frame runtime-порядок

Большой `Frame` лучше читать фазами.

### 4.1 Снимок режима

1. В начале читаются editor/play mode, pause, Studio и UI capture flags.
2. Edge flags F1, F2, F5, F6 сравниваются с прошлым кадром.
3. Переход режима выполняется один раз на нажатие.
4. ImGui capture влияет на input routing, но не должен останавливать сам render.

### 4.2 NPC Studio и свободная камера

1. Logic Studio может работать в живой сцене.
2. При входе сохраняется предыдущий camera/control state.
3. Если курсор над UI, ввод идёт интерфейсу.
4. Вне UI правой кнопкой или настроенным режимом камера получает look input.
5. Выход восстанавливает управление без телепорта gameplay entity.

### 4.3 Simulation gate

1. `simulationRunning` объединяет play mode, pause и step.
2. Системы решений, physics, timers и vitals получают `dt` только при разрешённой симуляции.
3. Render, UI и debug drawing работают даже на паузе.
4. Step пропускает ровно один фиксированный simulation slice, затем снова ставит паузу.

### 4.4 Gameplay и AI

1. Input snapshot создаётся до систем персонажа.
2. Weapon timers обновляются до попыток выстрела.
3. Damage queue прошлого producer-прохода обрабатывается в определённой фазе.
4. NPC command, utility, tactical, cover, navigation и ranged execution вызываются в зафиксированном порядке.
5. Group tactics могут быть выключены отдельным feature flag, не меняя одиночный AI.
6. Animation state читает уже принятое gameplay-решение.

### 4.5 Physics и transform

1. Character intent записывается до physics.
2. Bullet делает fixed substeps.
3. PhysicsSystem синхронизирует dynamic bodies в ECS transforms.
4. Character controller отдельно синхронизирует ghost transforms.
5. Animation attachments обновляются после итогового transform.

### 4.6 Render preparation

1. Room visibility и frustum candidates вычисляются по текущей camera.
2. Shadow pass использует light view-projection.
3. Opaque world pass рисует обычные meshes.
4. Transparent и effects идут позднее.
5. First-person pass использует отдельные правила depth/view model.
6. Debug primitives и UI завершают кадр.

### 4.7 Очистка одноразовых данных

1. Event queue очищается после всех consumers.
2. Input mouse delta сбрасывается после использования.
3. Per-frame debug buffers очищаются после render.
4. Step state возвращается в pause.

## 5 ApplicationClass Shutdown

1. `m_isShutdown` делает метод идемпотентным.
2. JobSystem останавливается до уничтожения объектов, которые могли использовать jobs.
3. Runtime systems отсоединяют Bullet constraints и bodies.
4. SceneManager очищает ECS-owned presentation resources.
5. GPU objects освобождаются до D3D device.
6. Device context очищает привязки.
7. Raw COM resources вызывают `Release` и зануляются.
8. D3DClass выключается последним среди графических объектов.

Если child resource освобождается после device, его Shutdown уже может обратиться к мёртвому context.

## 6 Weapon visual helpers

### 6.1 SetupTestRifleVisuals

1. Ищется local player и equipped weapon.
2. Старые view model entities удаляются, если owner или weapon изменился.
3. Grip profile загружается один раз и кешируется.
4. Animation bindings игрока получают нужные clips.
5. World visual создаётся для third-person/NPC view.
6. First-person visual создаётся отдельно и получает attachment component.
7. Arms и weapon visibility зависят от camera mode.

### 6.2 UpdateTestRifleVisuals

1. Каждый кадр проверяется актуальный equipped weapon.
2. Attachment bone и local grip transform обновляются.
3. Невалидный visual пропускается, а не разыменовывается.

### 6.3 UpdateNpcWeaponVisuals

1. Собираются живые NPC owners.
2. Inventory определяет реально экипированное оружие.
3. Профиль хвата загружается только при первом появлении weapon entity.
4. Visual создаётся или переиспользуется.
5. Bone attachment связывает его с рукой после animation update.

## 7 InputClass низкий уровень

### 7.1 Initialize

Массив клавиш, mouse position, delta и capture flags приводятся к нулевому состоянию. Иначе первый кадр может увидеть случайные нажатия.

### 7.2 Window events

1. `KeyDown` проверяет код меньше 256 и ставит true.
2. `KeyUp` ставит false.
3. `SetMousePosition` хранит координаты сообщения окна.
4. `AddMouseDelta` суммирует raw deltas нескольких сообщений.
5. `GetMouseDelta` читает накопление.
6. `ResetMouseDelta` очищает его один раз в конце кадра.

### 7.3 Captured mouse

1. `SetMouseCapture(true)` скрывает курсор и фиксирует режим.
2. `UpdateCapturedMouseDelta` сравнивает cursor с центром client area.
3. Разница добавляется в delta.
4. Cursor возвращается в центр, поэтому он не достигает края экрана.
5. Release возвращает нормальное положение и видимость.

## 8 InputManager высокий уровень

1. `Update` читает физическое состояние из InputClass.
2. Current и previous left button дают pressed/released edges.
3. Press запоминает стартовую позицию потенциального drag.
4. Пока расстояние меньше threshold, действие остаётся click candidate.
5. Превышение threshold включает dragging.
6. Drag callback получает текущую позицию каждый кадр.
7. Release вызывает drag end или обычный click.
8. При mouse capture UI click/drag path отключается.
9. `Reset` очищает переходные состояния при смене режима.

## 9 CameraController

### 9.1 Mode toggle

1. Требуются input manager и input.
2. Toggle edge вычисляется по текущему и прошлому состоянию.
3. UI block запрещает смену режима.
4. Free/follow mode переключается один раз.

### 9.2 Free look

1. Правая кнопка захватывает mouse.
2. Release снимает capture.
3. Delta умножается на sensitivity.
4. Yaw свободно накапливается.
5. Pitch ограничивается вверх и вниз.
6. Camera получает новые rotation values.

### 9.3 Free move

1. Из yaw вычисляются forward и right на плоскости XZ.
2. W/S двигают вдоль forward.
3. A/D вдоль right.
4. R/F меняют высоту.
5. Shift выбирает быстрый speed.
6. Все смещения умножаются на `dt`.

### 9.4 Follow и first person

1. Target transform задаёт anchor.
2. Third person camera отступает назад на distance и добавляет target offset.
3. First person использует eye height, right offset и forward offset.
4. Follow yaw/pitch меняются look input.
5. Socket Studio может временно заменить обычный follow orbit.

## 10 ECSRegistry

### 10.1 CreateEntity

1. `nextEntity` проверяется на invalid sentinel.
2. Новый id добавляется в live set.
3. Counter увеличивается.
4. Возвращается стабильный integer entity id.

Текущая схема не переиспользует id и не имеет generation. Для длительных сессий полезен entity+generation handle.

### 10.2 DestroyEntity

1. Entity удаляется из live set.
2. Каждый type-erased component pool получает `RemoveEntity`.
3. Компоненты всех типов исчезают независимо от того, знает ли registry их compile-time тип здесь.

Внешние Bullet и GPU ресурсы нельзя бездумно оставлять destructor компоненту, если они требуют world/context. Специализированная system cleanup должна выполняться раньше.

### 10.3 Add Get Has Remove

1. `AddComponent` получает или создаёт pool по `type_index`.
2. Lvalue копируется, rvalue перемещается.
3. `GetComponent` возвращает pointer или null.
4. `HasComponent` только проверяет наличие.
5. `RemoveComponent` ничего не делает при отсутствии pool.

### 10.4 ForEach одного типа

1. Получается pool T.
2. Отсутствующий pool завершает метод.
3. Итерация отдаёт entity и ссылку на component.

### 10.5 ForEach нескольких типов

1. Первый pool используется как источник entities.
2. Для каждой entity проверяются остальные components.
3. Callback вызывается только при полном наборе.

Оптимизация на будущее: выбирать самым маленьким не первый, а наименьший pool.

## 11 ComponentManager

### 11.1 Обычный компонент

1. `unordered_map<Entity,T>` хранит данные.
2. Has ищет ключ.
3. Get возвращает адрес value или null.
4. Add вставляет или заменяет component.
5. Remove стирает key.
6. begin/end позволяют systems итерироваться напрямую.

### 11.2 ActiveComponent специализация

1. Пустой tag не требует отдельного объекта на entity.
2. `unordered_set<Entity>` хранит только наличие.
3. Get возвращает адрес общего dummy, потому что данных у tag нет.
4. Add вставляет entity в set.

## 12 TransformSystem

1. Перебираются ECS transforms.
2. Entity связывается с legacy `GameObject` representation, если она существует.
3. Position, rotation и scale копируются в presentation object.
4. Система не должна копировать transform обратно в ECS в том же кадре, иначе появятся два владельца.

## 13 RenderSystem

1. Перебираются mesh renderer components.
2. Проверяются ActiveComponent, visibility, transform, model и material.
3. World matrix строится из transform.
4. Model привязывает buffers.
5. Material выбирает shader path.
6. Draw отправляется через shader manager/renderer.

Современный `Renderer` и `SceneManager` уже выполняют более сложные passes; старый RenderSystem следует считать простым reference path, а не вторым равноправным renderer.

## 14 SceneManager runtime ownership

### 14.1 SpawnObject

1. Создаётся entity.
2. Добавляется transform из descriptor.
3. Name и Active tags добавляются по настройкам.
4. MeshRenderer получает model и material.
5. Interaction component добавляется только при запросе.
6. RigidBody создаётся через PhysicsSystem.
7. Animated model получает AnimationComponent.
8. Entity возвращается caller.

### 14.2 RemoveObject

1. Проверяется, принадлежит ли entity scene.
2. Custom material освобождается, default material не удаляется.
3. Character controller и rigid body отсоединяются от Bullet.
4. Связанные presentation caches очищаются.
5. Registry уничтожает entity последним.

### 14.3 Clear

1. Сцена останавливает async work или отделяет его от старого generation.
2. Materials и models освобождаются один раз.
3. Physics objects удаляются до очистки registry.
4. Model cache очищается после entities.
5. Sprites и lights сбрасываются.

### 14.4 Render passes

1. `ShouldRenderInWorldPass` исключает first-person-only entities.
2. Opaque pass использует room visibility cache.
3. Async mesh path и обычный model path взаимоисключаются.
4. Transparent objects собираются отдельно.
5. First-person pass рисует только view models нужного owner.
6. Reflection pass может пропустить саму отражающую поверхность.

### 14.5 Save и Load

1. Save проходит по живым scene entities.
2. Runtime-only visuals, например NPC weapon visual, не сериализуются как самостоятельный gameplay object.
3. Компоненты записываются в JSON по версии schema.
4. Load сначала создаёт entity mapping old id -> new id.
5. Затем создаёт простые components.
6. Ссылки между entities разрешаются вторым проходом.
7. GPU/Bullet runtime handles пересоздаются, а не читаются из файла.

## 15 Асинхронный импорт SceneManager

1. `RequestShipImportAsync` отбрасывает пустой path, повторный импорт и отсутствие AssetLoadSystem.
2. Worker читает Assimp scene и собирает CPU descriptions.
3. Phase становится CpuReady только после полного snapshot.
4. Main thread загружает узлы небольшими порциями.
5. Panel batches создаются отдельной фазой.
6. Finalizing связывает rooms, doors, panels и membership.
7. Snapshot метода отдаёт progress UI без прямого доступа к мутирующему состоянию.

## 16 GameObject legacy wrapper

1. Constructor задаёт position/rotation нулём и scale единицей.
2. Initialize сохраняет non-owning model pointer и стартовую позицию.
3. `GetWorldMatrix` строит scale, затем rotation, затем translation в принятом соглашении.
4. Setters меняют локальные поля.

В ECS-сцене TransformComponent должен быть главным источником истины; GameObject полезен как legacy presentation wrapper.

## 17 PickingSystem

1. Camera создаёт world ray из mouse coordinates.
2. Для mesh ray переводится inverse world matrix в local space.
3. Сначала AABB быстро отбрасывает далёкий object.
4. Затем triangles дают точное попадание.
5. Local distance переводится обратно в сопоставимую world distance.
6. Lights проверяются сферами.
7. Sprites используют радиус из размера.
8. Ближайший hit выигрывает независимо от порядка перебора.
9. Equal-distance tie-breaker делает выбор стабильным.

Editor selection path в этой книге не рассматривается; сама математика picking остаётся runtime-полезной для interaction и shooting tools.

## 18 AnimationController

### 18.1 FindBinding

1. Перебирается список animation bindings.
2. Первое совпадение state возвращается pointer.
3. Отсутствие binding даёт null и позволяет fallback.

### 18.2 GetClipDurationSeconds

1. Duration ticks делится на ticks per second.
2. Нулевой ticks-per-second заменяется безопасным значением.
3. Нулевая duration не должна порождать деление или бесконечный loop.

### 18.3 Update state

1. Gameplay flags переводятся в желаемое animation state.
2. Приоритет одноразового action выше locomotion.
3. Смена state находит новый binding.
4. Clip index, time и loop policy обновляются.
5. Окончание non-loop clip возвращает locomotion/idle.
6. Controller не рассчитывает bone matrices; это делает AnimationSystem.

## 19 AnimationSystem

1. Перебираются entities с AnimationComponent и mesh model.
2. Невалидный model или отсутствие clips пропускается.
3. Playback time увеличивается на `deltaTime * speed`.
4. Loop clip использует modulo duration.
5. Non-loop clip clamp-ится к концу.
6. Model evaluates node hierarchy на текущем времени.
7. Для каждой bone вычисляется final matrix.
8. Matrices записываются в component palette.
9. Второй проход обновляет attachments после готовности всех owner palettes.

Если attachment выполнить в первом проходе до pose owner, визуал будет отставать на кадр.

## 20 TwoBoneIK

1. Root, joint и end positions задают длины двух сегментов.
2. Target distance clamp-ится между достижимым минимумом и максимумом.
3. Закон косинусов вычисляет угол локтя/колена.
4. Pole vector задаёт сторону сгиба.
5. Почти нулевой pole заменяется устойчивым перпендикуляром.
6. `RotationBetween` нормализует from и to.
7. Почти противоположные векторы требуют отдельной оси, потому что cross product почти нулевой.
8. Root и joint rotations комбинируются с исходной pose.

## 21 WeaponIKSystem

1. `GripMatrix` строит local transform из position и rotation профиля.
2. Attachment matrix оружия умножается на bone world matrix правой руки.
3. Позиция левой рукояти превращается в world target.
4. TwoBoneIK двигает левую руку к target.
5. Pole target удерживает локоть с правильной стороны.
6. Weight смешивает исходную animation pose и IK correction.
7. Отсутствующий bone или grip profile безопасно отключает IK.

## 22 MeleeActionJson и профили

1. JSON loader проверяет наличие полей до чтения.
2. Неизвестные поля игнорируются для forward compatibility.
3. Contact time clamp-ится внутри duration.
4. Animation name разрешается в clip позднее, когда model загружен.
5. Ошибка файла возвращает понятную диагностику и не стирает рабочий профиль.

## 23 Что разделить следующим рефакторингом

1. `ApplicationClass::Frame` разложить на `UpdateSession`, `UpdateSimulation`, `PrepareRender`, `RenderFrame` без перестановки вызовов.
2. Runtime render ownership вынести из SceneManager в Renderer, оставив SceneManager владельцем entities и serialization.
3. Legacy GameObject path пометить и постепенно удалить после миграции.
4. ECS ForEach выбирать минимальный component pool.
5. Async jobs привязать к scene generation token.
6. Animation evaluation и attachment pass закрепить разными именованными фазами.

