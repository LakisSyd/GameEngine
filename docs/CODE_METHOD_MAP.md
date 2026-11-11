# Карта кода и методов DX_FrameWork

Этот документ нужен, чтобы не переписывать уже существующие методы. Если ты хочешь добавить новую фичу, сначала найди похожий пункт здесь.

## Главная архитектура

Проект сейчас держится на пяти слоях:

```text
ApplicationClass
    главный кадр, режим Editor/Play, вызов систем

SceneManager
    ECS-сцена, импорт корабля, spawn, save/load, lights, sprites, render helpers

ECSRegistry + Components
    данные entity: Transform, MeshRenderer, Door, Room, RigidBody, HullPanel

Systems
    поведение: PhysicsSystem, DoorSystem, RoomSystem, InteractionSystem, FloodingSystem

Renderer / Shaders / AssimpModelClass
    DirectX, материалы, модели, буферы, render passes
```

## Быстрый выбор места для новой фичи

| Хочу сделать | Сначала смотреть | Скорее всего использовать |
| --- | --- | --- |
| Создать объект в сцене | `SceneManager` | `SpawnObject(SpawnObjectDesc&&)` |
| Загрузить модель | `SceneManager` | `GetOrCreateModel` |
| Импортировать корабль из GLB | `SceneManager` | `ImportShipFromGlb`, `ImportShipNodeRecursive` |
| Добавить комнату | `Components.h`, `SceneManager` | `RoomComponent`, `FindRoomByName` |
| Связать комнаты | `SceneManager` | `ConnectRooms`, `ConnectRoomsByName`, `ResolveDoorRoomLinks` |
| Добавить дверь | `Components.h`, `DoorSystem.h` | `DoorComponent`, `InteractionComponent` |
| Сделать дверь раздвижной | `DoorSystem.h`, naming в Blender | `DoorAnimationMode::Sliding`, `leftLeaf`, `rightLeaf` |
| Добавить interact | `InteractionSystem.h`, `EventSystem.h` | `InteractionComponent`, `InteractionEvent` |
| Добавить кислород/атмосферу | `RoomEnvironmentSystem.h` | `RoomComponent::oxygen`, `DoorComponent::GetEffectiveFlowRate` |
| Добавить физику объекту | `PhysicsSystem` | `RigidBodyComponent`, `PhysicsSystem::CreateRigidBody` |
| Пересоздать физику после изменения mesh | `PhysicsSystem` | `RefreshRigidBody` |
| Резать mesh | `MeshCuttingTest`, `AssimpModelClass` | `CutByRayAsync`, `CutCircularHoleForTest` |
| Выбрать объект мышью в редакторе | `PickingSystem` | `PickAt` |
| Отрисовать объект | `RenderMesh.h`, `SceneManager` | `RenderMesh`, `RenderAllECS` |
| Настроить материал | `MaterialClass` | `SetShader`, `SetTexture`, `SetGlass` |
| Добавить новый шейдер | `ShaderManager`, shader class | `ShaderManager::Load<T>` |
| Показать UI | `EditorUI.cpp` | `RenderComponentManager`, `RenderDetailsPanel` |
| Добавить HUD | `GameHud.h` | `GameHud::Render` |

## ApplicationClass

Файлы:

```text
src/Core/ApplicationClass.h
src/Core/ApplicationClass.cpp
```

`ApplicationClass` это координатор всего приложения. Он владеет D3D, камерой, renderer, scene manager, editor UI, gameplay controller, input, events.

### `Initialize`

Инициализирует окно/Direct3D/шейдеры/модели/рендер-текстуры/сцену/editor UI. Если нужно добавить глобальную подсистему, чаще всего она создается или связывается здесь.

Используй для:

- создания renderer dependency;
- загрузки базовых моделей;
- загрузки shader через `ShaderManager`;
- настройки callbacks между renderer и editor UI.

Не используй для:

- gameplay-логики кадра;
- временных тестовых изменений, которые должны происходить каждый кадр.

### `Frame(float dt)`

Главный кадр. Здесь решается порядок всех систем.

Текущий смысл блоков:

```text
0. Input
1. F5/F1/F6 hotkeys
2. Editor input / camera
3. Play interaction and doors
4. Gameplay movement / animation
5. Physics and water
6. Scene systems and lights
7. Renderer
8. Editor UI or debug overlay
9. HUD
10. Present
```

Если новая система должна обновляться каждый кадр, реши, где она должна стоять:

- до физики;
- после физики;
- только в Play;
- только в Editor;
- до рендера;
- после рендера.

Примеры:

```text
DoorSystem стоит в Play до gameplay/physics.
RoomSystem и RoomEnvironmentSystem вызываются внутри SceneManager::UpdateAll.
MeshCuttingTest запускается по F6.
```

### `SyncCompartmentPositions`

Синхронизирует позиции отсеков подлодки/корабля с root entity. Если root двигается, отсеки получают позицию через локальные смещения.

Используй как пример, когда нужно обновлять дочерние части от root transform.

### `UpdatePhysics`

Вызывает физику, давление, затопление и синхронизацию Bullet/Transform. Если объект физический, не двигай только `TransformComponent`, подумай, не надо ли синхронизировать Bullet body.

### `ProcessEditorRequests`

Обрабатывает запросы из UI: spawn, import, save/load, file actions. Если editor UI должен попросить движок что-то сделать, лучше добавить request и обработать его здесь, чем напрямую делать тяжелую работу внутри ImGui отрисовки.

### `SpawnObjectFromRequest`

Создает объект из `EditorUI::SpawnRequest`. Используй как пример для spawn-логики из UI.

### `UpdateCameraControlMode`

Отвечает за режим камеры. Не дублируй управление камерой в gameplay-коде, сначала смотри `CameraController`.

## SceneManager

Файлы:

```text
src/Scene/SceneManager.h
src/Scene/SceneManager.cpp
```

`SceneManager` это владелец ECS-сцены. Почти все игровые объекты проходят через него.

### Импорт корабля

#### `ImportShipFromGlb`

Главный вход для импорта корабля из GLB.

Что делает:

1. Читает GLB через Assimp.
2. Создает `ShipRoot`.
3. Рекурсивно импортирует node через `ImportShipNodeRecursive`.
4. Если включен batch-путь панелей, собирает `ShipPanelVisualBatch`.
5. Вызывает `ResolveDoorRoomLinks`.
6. Привязывает `HullPanelComponent::room`.

Используй, если нужно импортировать корабль целиком.

Не используй для обычного spawn одного объекта.

#### `ImportShipNodeRecursive`

Проходит по Assimp node-дереву.

Распознает имена:

```text
SHIP_ROOM_*      -> RoomComponent, invisible logic room
SHIP_PANEL_*     -> HullPanelComponent, collision, visual batch
SHIP_DOOR_*      -> DoorComponent + InteractionComponent
COL_* / PHYS_*   -> collision-only
STAIR / BRIDGE   -> static physics helper
```

Ключевая идея:

- `aiNode` хранит transform;
- `aiMesh` хранит геометрию;
- entity получает transform из node world matrix.

Если новая часть корабля должна импортироваться особым образом, добавляй распознавание имени здесь.

#### `ResolveDoorRoomLinks`

Автоматически связывает дверь с комнатами и створками.

Ожидаемый naming:

```text
SHIP_DOOR_HALL__REACTOR
DOOR_LEAF_L_HALL__REACTOR
DOOR_LEAF_R_HALL__REACTOR
SHIP_ROOM_HALL
SHIP_ROOM_REACTOR
```

Что делает:

- парсит имена комнат из имени двери;
- ищет leaf-объекты;
- если leaf найдены, ставит `DoorAnimationMode::Sliding`;
- ищет комнаты;
- если комнаты найдены, записывает `door.roomA`, `door.roomB`;
- вызывает `ConnectRooms`.

Важно:

Створки теперь линкуются до проверки комнат. Это позволяет раздвижной анимации работать даже если имя комнаты временно placeholder, но для oxygen правильные room names все равно нужны.

#### `FindRoomByName`

Ищет `RoomComponent` по имени. Если передать `HALL`, метод сам приведет к `SHIP_ROOM_HALL`.

Используй всегда вместо ручного перебора комнат, если ищешь room по имени.

#### `ConnectRooms`

Связывает две комнаты по entity. Добавляет каждую комнату в `connectedRooms` другой комнаты, без дублей.

#### `ConnectRoomsByName`

То же самое, но принимает имена. Внутри использует `FindRoomByName` и `ConnectRooms`.

### Spawn и удаление

#### `SpawnObject(SpawnObjectDesc&& desc)`

Главный метод создания ECS entity.

Добавляет:

- `TransformComponent`;
- `MeshRendererComponent`, если `addMeshRenderer`;
- `NameComponent`;
- `ActiveComponent`, если `active`;
- `InteractionComponent`, если `addInteraction`;
- `RigidBodyComponent`, если `addRigidBody`;
- `AnimationComponent`, если модель содержит анимации.

Если хочешь создать объект правильно, используй `SpawnObjectDesc`, а не вручную `CreateEntity + AddComponent` в пяти местах.

#### `SpawnObject(AssimpModelClass* model, float x, float y, float z)`

Короткий overload для простого spawn модели в позиции.

#### `RemoveObject`

Деактивирует и удаляет entity через ECS. Чистит material, если он не default. Используй для удаления объектов из сцены.

#### `Clear`

Очищает сцену, sprites, model cache, imported ship models. Используй при загрузке новой сцены.

### Обновление и рендер

#### `UpdateAll`

Делает ECS update, который принадлежит сцене:

- переносит ship parts при движении `ShipRoot`;
- обновляет animation;
- обновляет `RoomSystem`;
- обновляет `RoomEnvironmentSystem`;
- обновляет `PlayerVitalsSystem`.

Если новая логика относится ко всей сцене и не должна жить прямо в `ApplicationClass::Frame`, можно добавить ее сюда.

#### `RenderAllECS`

Рендерит активные `MeshRendererComponent`:

- собирает lights;
- делает frustum culling;
- сначала рисует непрозрачные объекты;
- потом прозрачные/стекло;
- передает damage decals в PBR;
- поддерживает async mesh path.

Не добавляй новый общий рендер-цикл, пока этот метод можно расширить.

#### `RenderAllECSExcluding`

Рендерит сцену без одного entity. Используется для reflection pass, чтобы объект не отражал сам себя.

### Save / Load

#### `SaveScene`

Сохраняет world settings, objects, components, materials, lights, sprites.

#### `SerializeObject`

Собирает json для одного entity. Если добавляешь новый компонент и он должен сохраняться, добавляй запись здесь.

#### `LoadScene`

Читает json, очищает сцену, восстанавливает объекты, потом remap-ит entity references.

#### `DeserializeObject`

Создает entity из json. Если добавляешь новый сохраняемый компонент, добавь чтение здесь.

#### `RemapEntityReferences`

После load старые entity id становятся новыми. Этот метод чинит ссылки внутри компонентов:

- attached lights;
- joints;
- doors left/right leaf;
- rooms;
- panels;
- etc.

Если компонент хранит `Entity`, его нужно remap-ить здесь.

### Lights

#### `SpawnLight`

Создает light entity, transform, light component, optional gizmo.

#### `RemoveLight`

Удаляет light entity.

#### `SetLightColor`, `SetLightPosition`, `SetLightEnabled`

Изменяют отдельные свойства света.

#### `AttachLightToObject`, `DetachLight`

Привязывают свет к entity через offset.

#### `UpdateLights`

Обновляет attached lights. Если target двигается, свет следует за ним.

#### `GatherLightsForShader`

Заполняет массивы `XMFLOAT4 outColors[]` и `outPositions[]` для shader uniforms.

## ECS

Файлы:

```text
src/ECS/ECSRegistry.h
src/ECS/Components/ComponentManager.h
src/ECS/Components/Components.h
src/ECS/ECSTypes.h
```

### `ECSRegistry::CreateEntity`

Выдает новый `Entity`. Это просто id.

### `DestroyEntity`

Удаляет все компоненты entity из всех component pools.

### `AddComponent<T>`

Добавляет компонент entity. Есть copy-версия и move-версия. Move-версия важна для `RigidBodyComponent`, потому что он хранит Bullet pointers и запрещает copy.

### `GetComponent<T>`

Возвращает указатель на компонент или `nullptr`.

Паттерн:

```cpp
auto* transform = registry.GetComponent<TransformComponent>(entity);
if (!transform)
    return;
```

### `HasComponent<T>`

Проверяет наличие компонента.

### `RemoveComponent<T>`

Удаляет компонент.

### `GetComponents<T>`

Возвращает pool всех компонентов типа `T`. Используется для ручной итерации.

### `ForEach<T>`

Итерирует все компоненты типа `T`:

```cpp
registry.ForEach<DoorComponent>(
    [&](Entity entity, DoorComponent& door)
    {
        // update door
    });
```

## Основные компоненты

Файл:

```text
src/ECS/Components/Components.h
```

### `TransformComponent`

Позиция, поворот, масштаб. `GetWorldMatrix` строит world matrix:

```text
scale * rotation * translation
```

Почти все visual/physics/entity logic начинается отсюда.

### `MeshRendererComponent`

Хранит:

- `model`;
- `material`;
- `castsShadow`;
- async model path.

Если entity должен рисоваться, у него должен быть этот компонент.

### `RigidBodyComponent`

Физика Bullet:

- `body`;
- `shape`;
- mass/dynamic/kinematic;
- shape type;
- collision model;
- triangle mesh;
- convex hulls;
- compound elements.

Компонент move-only. Не копируй его.

Если меняешь mesh/collision, вызывай `PhysicsSystem::RefreshRigidBody`.

### `DoorComponent`

Хранит состояние двери:

- `isOpen`;
- `isBroken`;
- `openProgress`;
- `openSpeed`;
- `animationMode`;
- `openHeight`;
- `leftLeaf`;
- `rightLeaf`;
- `roomA`;
- `roomB`;
- `flowRate`.

Методы:

- `TryOpen`;
- `TryClose`;
- `GetEffectiveFlowRate`.

`GetEffectiveFlowRate` важен для кислорода: закрытая дверь дает 0, открытая дает flow по `openProgress`.

### `RoomComponent`

Комната:

- `roomName`;
- `objectsInRoom`;
- `connectedRooms`;
- bounds `localMin/localMax`;
- `oxygen`;
- `pressure`;
- `temperature`;
- `leakRate`;
- `isActive`.

### `HullPanelComponent`

Панель корпуса:

- имя панели;
- имя комнаты;
- ссылка на room;
- health;
- breach count;
- max breaches;
- external/destroyed flags.

Методы:

- `CanCreateBreach`;
- `Health01`.

### `HullBreachComponent`

Gameplay-пробоина:

- panel;
- room;
- local/world position;
- local normal;
- radius;
- leakRate;
- sealed.

### `CompartmentComponent`

Старый/параллельный компонент отсека:

- bounds;
- volume;
- water volume/level;
- breaches;
- connected compartments;
- pump rate.

Методы:

- `RecalculateVolume`;
- `HasSizeChanged`;
- `CacheSize`;
- `ContainsPoint`.

### `DamageDecalComponent`

Данные для трещин/дефектов в PBR shader. `SyncFromBreaches` строит список cracks по `CompartmentComponent::breaches`.

## Systems

### DoorSystem

Файл:

```text
src/ECS/System/DoorSystem.h
```

#### `ProcessEvents`

Читает `InteractionEvent` из `EventQueue`. Если target имеет `DoorComponent`, переключает `isOpen`. Если дверь открылась, создает `DoorOpenedEvent`.

#### `Update`

Обновляет `openProgress`.

Если `animationMode == Sliding` и есть `leftLeaf/rightLeaf`:

- сохраняет закрытые позиции створок;
- вычисляет slide axis из правой створки минус левая;
- двигает створки в разные стороны;
- root/рама остается на месте.

Иначе:

- использует старый Vertical mode;
- двигает transform root вверх по Y.

### InteractionSystem

Файл:

```text
InteractionSystem.h
```

#### `Update`

Строит ray из камеры вперед, raycast-ит Bullet world, игнорирует player, проверяет `InteractionComponent`, сохраняет hovered entity/prompt, по нажатию `C` создает `InteractionEvent`.

Используй это как основу для:

- двери;
- repair tool;
- панели управления;
- предметы;
- станции корабля.

### EventQueue

Файл:

```text
EventSystem.h
```

#### `Emit`

Добавляет событие в `std::vector<std::variant<...>>`.

#### `GetEvents`

Дает const reference на события текущего кадра.

#### `Clear`

Очищает очередь. Вызов стоит в начале `ApplicationClass::Frame`.

### RoomSystem

Файл:

```text
RoomSystem.h
```

#### `Update`

Очищает `objectsInRoom` у всех комнат, потом проверяет transform всех trackable entity и добавляет их в комнату, если позиция внутри room bounds.

Trackable entity:

- player/character controller;
- dynamic rigid body.

### RoomEnvironmentSystem

Файл:

```text
RoomEnvironmentSystem.h
```

#### `Update`

Делает две вещи:

1. Уменьшает oxygen в комнате, если в ней есть breathing entity.
2. Выравнивает oxygen между connectedRooms.

#### `GetDoorFlowMultiplier`

Если между двумя комнатами есть дверь, flow зависит от `DoorComponent::GetEffectiveFlowRate`. Если двери нет, flow = 1.

Это место отвечает за логическую герметичность.

### PhysicsSystem

Файлы:

```text
src/ECS/System/PhysicsSystem.h
src/ECS/System/PhysicsSystem.cpp
```

Ключевые методы:

- `InitializeWorld`: создает Bullet world.
- `Update`: step simulation, sync transforms.
- `CreateRigidBody`: создает Bullet body из `RigidBodyComponent`.
- `RefreshRigidBody`: пересоздает body/shape после изменения параметров или mesh.
- `CreateBoxShape`, `CreateSphereShape`, `CreateCylinderShape`, `CreateCapsuleShape`: простые формы.
- `CreateConvexHullShape`: convex hull из модели.
- `CreateTriangleMeshShape`: статичный triangle mesh.
- `CreateGImpactMeshShape`: concave mesh для dynamic случаев.
- `CreateCompoundElementShape`: shape для compound child.
- `UpdateCharacterPos`: переносит позицию Bullet character controller обратно в Transform.

Правило:

Если объект имеет Bullet body, не меняй только transform и не жди, что физика сама поймет. Либо двигай Bullet body, либо вызывай refresh/sync.

### FloodingSystem

Файлы:

```text
src/ECS/System/FloodingSystem.h
src/ECS/System/FloodingSystem.cpp
```

Методы:

- `Update`: обновляет затопление отсеков.
- `CreateBreach`: добавляет пробоину в compartment local space.
- `CreateBreachAtWorldPosition`: переводит world position в local и создает пробоину.
- `SealBreach`: закрывает пробоину.
- `FindCompartmentForObject`: ищет отсек по world position.
- `CalculateFlowRate`: считает поток по пробоине.

Можно использовать как основу для космической разгерметизации, даже если вода потом будет заменена на oxygen/pressure leak.

### MeshCuttingTest

Файлы:

```text
src/ECS/System/MeshCuttingTest.h
src/ECS/System/MeshCuttingTest.cpp
```

Методы:

- `CutRandomHoleInReactor`: тестовая пробоина.
- `CutByRay`: синхронная резка по ray.
- `CutByRayAsync`: тяжелую часть отправляет в `JobSystem`, а DirectX/Bullet обновляет на main thread.

Не воспринимай как финальную систему разрушения. Это prototype.

### JointConnectionSystem

Файлы:

```text
src/ECS/System/JointConnectionSystem.h
src/ECS/System/JointConnectionSystem.cpp
```

Методы:

- `Update`: создает/удаляет constraints по компонентам.
- `CreateJoint`: создает физический joint.
- `RemoveJoint`: удаляет constraint.
- `RemoveJointsForEntity`: чистит все joints для entity.

### CameraController

Файлы:

```text
src/Core/CameraController.h
src/Core/CameraController.cpp
```

Методы:

- `SetInput`: связывает input manager, raw input и HWND.
- `UpdateModeToggle`: переключает camera mode.
- `UpdateFree`: editor/free camera.
- `UpdateFollow`: play camera следует за player.
- `ApplyFreeLook`: вращение мышью.
- `ApplyFreeMove`: WASD/RF перемещение.
- `EnterPlayMode`, `ExitPlayMode`: переключают настройки камеры под Play/Editor.

Если камера ведет себя странно, начинай отсюда.

### InputManager / InputClass

Файлы:

```text
src/Input/InputManager.h
src/Input/InputManager.cpp
src/Input/InputClass.h
src/Input/InputClass.cpp
```

`InputClass` хранит низкоуровневое состояние клавиш/мыши.

`InputManager` делает поверх этого:

- key pressed edge;
- mouse click;
- drag start/update/end;
- mouse capture helpers.

Используй:

```cpp
inputManager.IsKeyDown(input, 'W')
inputManager.IsKeyPressed(input, 'C')
```

`IsKeyPressed` true только в кадр нажатия.

## Персонажи и NPC

Этот раздел описывает актуальную цепочку персонажа. Главное правило: AI создаёт намерение, общий controller исполняет его через Bullet, animation читает результат. Ни utility, ни hostile decision не должны напрямую телепортировать `TransformComponent` во время обычной ходьбы.

### `CharacterSpawnSystem`

Файл:

```text
src/Gameplay/Player/CharacterSpawnSystem.h
src/Gameplay/Player/CharacterSpawnSystem.cpp
```

#### `Spawn`

Создаёт runtime Bullet capsule для entity, у которой уже есть `CharacterController` и `TransformComponent`. Повторный вызов сначала уничтожает старый runtime controller. Регистрирует ghost object и character action в `PhysicsWorld`.

Не путать с созданием ECS entity: метод не добавляет модель, NPC brain или movement-компоненты.

#### `Destroy`

Снимает action и collision object с Bullet world, затем освобождает controller, ghost и capsule. Вызывать перед удалением персонажа/очисткой сцены.

### `CharacterControllerSystem`

Файл:

```text
src/Gameplay/Player/CharacterControllerSystem.h
```

#### `Update`

Обрабатывает все `CharacterController`: active-state suspension, ground support, automatic step, external velocity, jump, walk direction и стабильный physics idle. Читает `CharacterMovement`, но не решает игровую цель персонажа.

#### `IsAutomaticStepActive`

Диагностирует, выполняется ли сейчас автоматическое преодоление порога.

#### `HasGroundSupport`

Возвращает результат геометрической проверки опоры, используемый как источник grounded.

#### `IsIdlePhysicsLocked`

Показывает, переведён ли стоящий персонаж в стабильный no-motion режим Bullet.

#### `ForceReconfigure`

Сбрасывает runtime traversal cache после изменения physics tuning в редакторе. Не пересоздаёт сущность и не заменяет `CharacterSpawnSystem::Spawn`.

### `CharacterFacingSystem`

Файл:

```text
src/Gameplay/Player/CharacterFacingSystem.h
```

#### `Update`

Поворачивает визуальный Transform по горизонтальному `walkDirection`. Вызывать после AI/gameplay intent и до анимации/рендера. Не использовать для выбора цели.

### `AnimationController`

Файл:

```text
src/Animation/AnimationController.h
```

#### `Update`

Выбирает locomotion-анимацию по `CharacterMovement` для игрока и NPC. AI должен выставлять Idle/Walk/Run intent, а не запускать клип напрямую.

### Базовые NPC-данные

Файл:

```text
NPCComponents.h
```

#### `NPCComponent`

Хранит name, description, health/maxHealth и `isHostile`. `isHostile` уже является источником различия экипажа и врага, редактируется в инспекторе и сохраняется как `npc_hostile`.

#### `NPCMovementComponent`

Хранит walk/run speed, дистанции прибытия и дверей, возможность бега. Это authoring-настройки навигации, а не фактическое движение текущего кадра.

#### `NpcBrainComponent`

Хранит state, manual command, repair target, комнатный/дверной маршрут, utility score, repath/stuck timers, wander runtime и debug snapshot.

#### `NpcCommand::IsActive`

Единственная официальная проверка ручного приоритета: любая команда кроме `None` блокирует автоматическое назначение.

#### `NpcCommand::Clear`

Возвращает command в `None`, очищает entity targets и увеличивает revision.

### `NpcCommandSystem`

Файл:

```text
NpcCommandSystem.h
```

Методы записывают приоритетный приказ в brain; фактическое выполнение остаётся в `StateSystem`.

| Метод | Назначение |
|---|---|
| `Stop` | постоянная остановка с ручным приоритетом |
| `Wander` | постоянное блуждание |
| `MoveToPosition` | одноразовое движение в world point |
| `MoveToRoom` | одноразовое движение внутрь комнаты |
| `FollowEntity` | постоянное следование за Entity |
| `RepairEntity` | одноразовый ручной ремонт |
| `ResumeAutomatic` | очистить manual command и вернуть utility/crew authority |
| `ForceRepath` | инвалидировать текущий маршрут, сохранив задачу |

### `NpcUtilitySystem`

Файл:

```text
NpcUtilitySystem.h
```

#### `Update`

Обходит подходящих NPC и вызывает индивидуальную оценку задач. Должен считать scores для debug даже при manual command, но не менять решение при активном command.

#### `CalculateRepairScore`

Оценивает пробоину по тяжести и расстоянию, учитывая detection range и reservation.

#### `CalculateEquipmentRepairScore`

Оценивает повреждённое оборудование. Невалидное, полностью исправное или чужое назначение получает нулевую полезность.

#### `IsRepairTargetStillValid`

Проверяет общий `RepairableComponent`, состояние breach либо health оборудования.

### `CrewTaskCoordinator`

Файл:

```text
src/Gameplay/AI/CrewTaskCoordinator.h
```

#### `Update`

Очищает claims, собирает ремонтные задачи и активный экипаж, сохраняет валидные назначения и закрепляет свободные цели. Hostile, off-duty и NPC с manual command исключаются.

#### `IsRepairTargetValid`

Публичный контракт валидности repair target: Entity жива, активна, имеет незавершённый `RepairableComponent` и всё ещё повреждена.

#### `IsClaimedByOther`

Проверяет, закреплена ли цель за другим живым NPC.

#### `GetSnapshot`

Возвращает telemetry последнего update: available crew, busy crew, open tasks и новые назначения кадра.

#### `CollectRepairCandidates`

Собирает и сортирует валидные цели. Сортировка нужна для воспроизводимого выбора.

#### `CleanupInvalidClaims`

Удаляет claims завершённых/удалённых целей или NPC, которые больше не могут продолжать. Синхронизирует `CrewMemberComponent::assignedTask`.

#### `ReleaseAssignment`

Освобождает claim только если он принадлежит указанному NPC.

#### `KeepExistingAssignment`

Сохраняет валидную текущую работу и восстанавливает согласованность brain/crew state.

#### `CalculateAssignmentScore`

Объединяет severity, distance, role affinity и repair skill. Engineer имеет преимущество ремонта, Operations — дополнительное преимущество для энергетического оборудования.

### `StateSystem`

Файл:

```text
StateSystem.h
```

#### `Update`

Исполняет manual command или automatic state для каждого NPC с обязательными компонентами. Это consumer решения utility/coordinator и producer `CharacterMovement`.

#### `HandleManualCommand`

Маршрутизирует Stop, Wander, MoveToPosition, MoveToRoom, FollowEntity и RepairEntity. Постоянные команды не очищаются автоматически; одноразовые завершаются через общую точку.

#### `UpdateRepairTask`

Унифицирует ремонт пробоины и оборудования через `RepairableComponent`, определяет точку/комнату, ведёт NPC к цели и передаёт прогресс в ремонтный путь. Учитывает `CrewMemberComponent::repairSkill`.

#### `FindRepairZoneForOwner`

Ищет `WorkZoneComponent` с owner. Содержит fallback по имени/типу оборудования для старых сцен; новый контент должен задавать owner явно.

#### `NavigateTo`

Общий исполнитель навигации: комнаты, двери, portal crossing, локальный NavMesh waypoint, движение и blocked status. Не копировать эту логику в hostile AI.

#### `TryBuildDoorRoute`

Строит верхнеуровневый маршрут по графу связанных комнат.

#### `MoveTowards`

Записывает нормализованное направление, walk/run speed и locomotion state в `CharacterMovement`.

#### `StopMovement`

Сбрасывает все locomotion-поля вместе. Использовать вместо частичного `isMoving = false`, иначе может остаться Run-анимация или скорость.

#### `CompleteManualCommand`

Очищает одноразовую команду, маршрут, дверь, status и движение; после этого automatic AI снова получает authority.

#### `SetBlocked`

Останавливает движение и записывает диагностическую причину, не уничтожая команду/цель.

#### `UpdateStuckStatus`

Сравнивает реальное изменение Transform с намерением движения. При отсутствии прогресса инвалидирует NavMesh path и записывает snapshot дверного перехода.

### `NpcNavMeshSystem`

Файлы:

```text
src/Gameplay/AI/NpcNavMeshSystem.h
src/Gameplay/AI/NpcNavMeshSystem.cpp
```

#### `FindNextWaypoint`

Строит/читает путь Detour внутри комнаты и возвращает следующую точку. `false` означает использовать предусмотренный fallback, а не аварийно завершать приложение.

#### `FindReachablePoint`

Проецирует желаемую точку на связный остров NavMesh текущего старта; используется для безопасного wander/work-zone target.

#### `FindDynamicAvoidanceWaypoint`

Строит короткий локальный объезд ближайшего динамического Bullet AABB. Это не полноценный crowd/RVO.

#### `InvalidatePath`

Сбрасывает путь одного NPC после смены target или обнаружения застревания.

#### `InvalidateAll`

Сбрасывает room meshes и все paths после изменения геометрии/коллайдеров.

#### `GetDebugInfo`

Возвращает построенность mesh, fallback status, polygon/waypoint counts, path и debug edges.

#### `GetDebugDrawSettings`

Возвращает настройки one-frame world visualization NavMesh/path/obstacles.

### Дружественный и враждебный NPC

Текущий источник истины — `NPCComponent::isHostile`.

```text
isHostile == false → допускается CrewTaskCoordinator и ремонт экипажа
isHostile == true  → исключается из crew assignment; combat AI добавляется отдельно
```

Боевой компонент должен хранить perception/combat runtime (target, ranges, cooldown), но не дублировать флаг hostile. Hostile decision вызывается до facing/controller/animation и переиспользует `NavigateTo`/общий navigation executor.

## Renderer

Файлы:

```text
Renderer.h
Renderer.cpp
src/ECS/System/RenderMesh.h
src/ECS/System/RenderSystem.h
```

### `Renderer::RenderFrame`

Главная точка рендера кадра.

Пассы:

```text
shadow map
reflection texture
refraction texture
back buffer main scene
sprites/debug/preview
```

Статистика записывается в `RenderFrameStats`.

### `RenderMesh`

Inline helper, который:

1. Берет shader из material.
2. Вызывает `model->Render`.
3. По `ShaderType` выбирает нужный shader class.
4. Передает texture/material/light/fog/matrices.

Если объект белый/невидимый, проверь:

- есть ли material;
- есть ли shader;
- есть ли texture/SRV;
- подходит ли ShaderType.

### `RenderSystem::Render`

Простой ECS render loop. Сейчас основная логика богаче в `SceneManager::RenderAllECS`, но этот класс показывает базовый паттерн.

## AssimpModelClass

Файлы:

```text
src/Render/Model/assimpmodelclass.h
src/Render/Model/assimpmodelclass.cpp
```

Это модель, которая владеет DirectX vertex/index buffers, mesh parts, embedded textures, CPU-копией геометрии и animation data.

### Загрузка

#### `Initialize`

Загружает модель по пути.

#### `InitializeFromMeshes`

Создает модель из нескольких `aiMesh` одного node. Используется при импорте корабля.

#### `InitializeFromMesh`

Создает модель из одного `aiMesh`.

#### `InitializeFromGeometry`

Создает модель из уже готовых CPU vertices/indices. Используется для `ShipPanelVisualBatch`.

### Геометрия и bounds

#### `GetBoundingBox`

Возвращает local AABB модели.

#### `GetBoundingSphere`

Возвращает center/radius.

#### `GetVertices`, `GetRawVertices`, `GetRawTriangles`

Дают CPU-копию геометрии. Используются picking/cutting/physics.

### Runtime mesh mutation

#### `ApplyCpuMeshForTest`

Заменяет CPU mesh и пересоздает DirectX buffers.

#### `CutSphereHoleForTest`

Грубый тест: удаляет треугольники внутри sphere.

#### `CutCircularHoleForTest`

Более аккуратный prototype: режет круг на поверхности через clipping в tangent space.

### Animation

#### `LoadBones`

Читает bone weights/index из Assimp.

#### `LoadAnimations`

Читает clips/channels/keyframes.

#### `UpdateAnimation`

Обновляет `AnimationComponent` по deltaTime.

#### `SetBoneMatricesFromComponent`

Передает matrices костей в shader buffer.

#### `ComputeBoneMatrices`

Считает matrices для текущего animation time.

## MaterialClass

Файлы:

```text
src/Render/Material/MaterialClass.h
src/Render/Material/MaterialClass.cpp
```

Материал хранит shader, textures и параметры для разных shader types.

Частые методы:

- `SetShader`, `GetShader`;
- `SetTexture`, `AdoptTexture`, `SetMainTexture`;
- `SetTexturePath`, `GetTexturePath`;
- `SetGlass`, `IsGlass`, `GetGlassOpacity`;
- `SetClipPlane`, `SetClipEnabled`;
- `SetReflectionMatrix`, `SetReflectionSRV`;
- `SetWaterTranslation`, `SetRefractionSRV`;
- `SetTerrain...`;
- `Bind`, `Unbind`.

Не создавай новый material class под каждую фичу. Расширяй этот, если параметр реально относится к материалу.

## ShaderManager / ShaderBaseClass

Файлы:

```text
src/Render/Shader/ShaderManager.h
src/Render/Shader/BaseShader.h
```

### `ShaderManager::Load<T>`

Создает и инициализирует shader, сохраняет по имени. Если shader уже есть, возвращает существующий.

### `Get`, `GetAs`, `GetShaderType`

Дают доступ к загруженным shader.

### `ForEach`

Позволяет обновить общий параметр во всех shader, например fog.

### `ShaderBaseClass`

Базовый помощник для shader:

- compile;
- input layout;
- sampler;
- constant buffer;
- fog buffer;
- `UpdateConstantBuffer`.

## EditorUI

Файлы:

```text
src/Editor/EditorUI.h
src/Editor/EditorUI.cpp
```

Главные методы:

- `Initialize`, `Shutdown`, `NewFrame`, `Render`;
- `Update`: основная отрисовка UI;
- `RenderSceneOutliner`: список entity;
- `RenderDetailsPanel`: правая панель деталей;
- `RenderObjectProperties`: transform/name/material;
- `RenderComponentManager`: компоненты entity;
- `RenderSpawnPanel`: spawn/import;
- `RenderFogPanel`: настройки мира;
- `RenderPhysicsSettings`: debug physics;
- `RenderCompartmentEditor`: редактор отсеков;
- `RenderContentBrowser`: файловый браузер;
- `RenderKnifeTool`: UI для clipping/cutting;
- `RenderShaderSelector`: выбор shader.

Правило:

UI должен менять данные/создавать request. Тяжелую работу лучше делать в `ApplicationClass::ProcessEditorRequests`.

## PickingSystem

Файлы:

```text
src/Config/PickingSystem.h
src/Config/PickingSystem.cpp
```

### `PickAt`

Порядок выбора:

1. light spheres;
2. world sprites;
3. ECS mesh objects.

Для mesh сначала проверяет AABB, потом реальные triangles через `RayIntersectsModelTriangles`.

Если объект выбирается неправильно, проверь:

- ActiveComponent;
- MeshRendererComponent;
- TransformComponent;
- bounds модели;
- `EditorSelectionComponent` redirect.

## Naming conventions корабля

### Комнаты

```text
SHIP_ROOM_HALL
SHIP_ROOM_REACTOR
SHIP_ROOM_GUN_ROOM
```

Комната обычно невидимая логическая коробка. Ее bounds идут из mesh bounding box.

### Панели корпуса

```text
SHIP_PANEL_REACTOR_WALL_B_07
SHIP_PANEL_HALL_FLOOR_01
SHIP_PANEL_GUN_ROOM_CEIL_02
```

`TryParsePanelRoomName` пытается достать room name из имени панели.

### Двери

```text
SHIP_DOOR_HALL__REACTOR
```

Разделитель между комнатами обязательно двойной:

```text
__
```

### Створки раздвижной двери

```text
DOOR_LEAF_L_HALL__REACTOR
DOOR_LEAF_R_HALL__REACTOR
```

Если leaf не найдены, дверь останется `Vertical`.

### Коллизия

```text
COL_*
SHIP_COL_*
PHYS_*
COLLISION_*
```

Такие node не рисуются, но могут получить rigid body.

## Частые ошибки

### "Рама двери поднимается вверх"

Причина:

- `DoorComponent::animationMode` остался `Vertical`;
- leaf-объекты не найдены;
- имена leaf не совпадают с именем двери;
- дверь названа placeholder rooms, а комнаты не совпадают.

Проверка:

```text
DoorComponent.animationMode
DoorComponent.leftLeaf
DoorComponent.rightLeaf
```

### "Кислород проходит через закрытую дверь"

Проверить:

- связаны ли комнаты напрямую без двери;
- есть ли между roomA/roomB дверь;
- `DoorComponent::GetEffectiveFlowRate`;
- `openProgress`;
- `RoomEnvironmentSystem::GetDoorFlowMultiplier`.

### "Объект виден, но по нему нельзя ходить"

Проверить:

- есть ли `RigidBodyComponent`;
- создан ли Bullet `body`;
- правильный `shapeType`;
- `mass = 0` для static;
- collision group/mask;
- не collision-only ли это node.

### "Объект выбирается как весь корабль"

Это может быть специально:

```text
EditorSelectionComponent.redirectTo = ShipRoot
```

Детали корабля перенаправляют selection на root, чтобы случайно не двигать одну стенку отдельно.

### "Mesh изменился, а физика старая"

После изменения геометрии нужен:

```text
PhysicsSystem::RefreshRigidBody
```

### "Стекло рисуется неправильно"

Проверить:

- `MaterialClass::IsGlass`;
- `AssimpModelClass::MeshPart::isTransparent`;
- второй transparent pass в `SceneManager::RenderAllECS`;
- blend state в PBR path.

## Правило перед новым кодом

Перед тем как писать новый метод, задай себе вопросы:

1. Это данные entity? Тогда компонент.
2. Это каждый кадр обрабатывает много entity? Тогда система.
3. Это реакция на действие? Тогда event.
4. Это UI? Тогда `EditorUI`, а тяжелую работу через request.
5. Это spawn/save/load/render/ship import? Тогда сначала `SceneManager`.
6. Это DirectX resource? Тогда ищи похожее в `D3DClass`, `AssimpModelClass`, shader class, `MaterialClass`.
