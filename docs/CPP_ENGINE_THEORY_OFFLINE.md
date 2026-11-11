# C++ и теория движка: офлайн-конспект

Этот файл не заменяет учебник C++, но покрывает именно те темы, которые постоянно встречаются в твоем проекте.

## 1. Как читать C++ файл

Обычно файл устроен так:

```cpp
#pragma once

#include "SomeHeader.h"
#include <vector>

class MyClass
{
public:
    void Update(float dt);

private:
    int m_value = 0;
};
```

### `#pragma once`

Говорит компилятору: подключай этот header только один раз в один translation unit. Это защита от повторного объявления классов/структур.

В проекте почти все `.h` используют `#pragma once`.

### `#include`

Подключает объявления из другого файла.

```cpp
#include "Components.h"      // локальный файл проекта
#include <vector>            // стандартная библиотека
#include <d3d11.h>           // Windows/DirectX SDK
```

Если в header можно обойтись forward declaration, лучше не тащить тяжелый include.

Пример:

```cpp
class SceneManager;
class D3DClass;
```

Это говорит: "такой класс существует, полное определение понадобится в .cpp".

## 2. `struct` и `class`

В C++ `struct` и `class` почти одинаковые. Разница по умолчанию:

- `struct`: поля public;
- `class`: поля private.

В твоем проекте:

```cpp
struct TransformComponent
{
    XMFLOAT3 position;
    XMFLOAT3 rotation;
    XMFLOAT3 scale;
};
```

Компоненты обычно `struct`, потому что это данные.

```cpp
class SceneManager
{
public:
    Entity SpawnObject(SpawnObjectDesc&& desc);

private:
    ECSRegistry m_registry;
};
```

Системы/менеджеры обычно `class`, потому что у них есть поведение и скрытое состояние.

## 3. Public и private

```cpp
class DoorSystem
{
public:
    void Update(ECSRegistry& registry, float deltaTime);

private:
    int m_internalValue = 0;
};
```

`public` это API класса. Другой код может вызывать эти методы.

`private` это детали реализации. Другой код не должен напрямую трогать их.

Правило:

Если поле может быть изменено откуда угодно, отладка становится тяжелее. Поэтому у менеджеров состояние лучше private, а доступ через методы.

## 4. Указатели, ссылки и значения

### Значение

```cpp
TransformComponent transform;
```

Это сам объект. Он живет в текущей области видимости.

### Указатель

```cpp
TransformComponent* transform = registry.GetComponent<TransformComponent>(entity);
```

Указатель может быть `nullptr`. Поэтому почти всегда после получения компонента нужна проверка:

```cpp
if (!transform)
    return;
```

В ECS это главный паттерн.

### Ссылка

```cpp
void Update(ECSRegistry& registry, float dt)
```

`registry&` значит: передаем существующий объект без копирования. Ссылка не может быть `nullptr`.

Используй ссылку, когда объект обязан существовать.

Используй указатель, когда объекта может не быть.

### `const`

```cpp
const RoomComponent& room
const XMFLOAT3& position
```

`const` значит: метод не должен менять объект.

Это полезно для чтения:

```cpp
bool ContainsPoint(const XMFLOAT3& worldPos) const;
```

Первый `const` защищает аргумент. Второй `const` после метода говорит: метод не меняет поля своего объекта.

## 5. RAII и владение ресурсами

RAII: ресурс освобождается в деструкторе объекта.

Пример в проекте:

```cpp
struct RigidBodyComponent
{
    btRigidBody* body = nullptr;

    ~RigidBodyComponent()
    {
        DestroyResources();
    }
};
```

Зачем это нужно:

- Bullet body надо удалить;
- collision shape надо удалить;
- triangle mesh надо удалить;
- если забыть, будет leak;
- если удалить дважды, будет crash.

### `std::unique_ptr`

```cpp
std::unique_ptr<D3DClass> m_direct3D;
```

`unique_ptr` означает единственное владение. Когда объект-владелец уничтожается, `unique_ptr` сам вызывает delete.

Используй `unique_ptr` для новых объектов, если можешь. Сырые указатели оставляй там, где ресурс живет в другом месте или приходит из DirectX/Bullet API.

## 6. Copy и move

### Copy

Копирование создает второй объект с теми же данными.

Для простых структур нормально:

```cpp
TransformComponent a;
TransformComponent b = a;
```

### Почему `RigidBodyComponent` нельзя копировать

Если скопировать raw pointer на Bullet body, два компонента будут думать, что владеют одним body. Потом оба попытаются удалить его.

Поэтому:

```cpp
RigidBodyComponent(const RigidBodyComponent&) = delete;
RigidBodyComponent& operator=(const RigidBodyComponent&) = delete;
```

### Move

Move переносит владение:

```cpp
registry.AddComponent<RigidBodyComponent>(entity, std::move(rb));
```

После move старый объект должен обнулить pointers, чтобы не удалить ресурс дважды.

В `RigidBodyComponent` уже есть move constructor и move assignment.

## 7. Шаблоны

В ECS много шаблонов:

```cpp
template<typename T>
T* GetComponent(Entity e)
{
    return GetOrCreatePool<T>().Get(e);
}
```

`T` это тип компонента:

```cpp
registry.GetComponent<TransformComponent>(entity);
registry.GetComponent<DoorComponent>(entity);
registry.GetComponent<RoomComponent>(entity);
```

Компилятор создает версии метода под каждый тип.

### Почему ECS так удобен

Без шаблонов пришлось бы писать:

```cpp
GetTransformComponent(entity)
GetDoorComponent(entity)
GetRoomComponent(entity)
```

С шаблонами:

```cpp
GetComponent<T>
```

## 8. Lambda

Lambda это маленькая функция на месте.

Пример:

```cpp
registry.ForEach<DoorComponent>(
    [&](Entity entity, DoorComponent& door)
    {
        door.openProgress += deltaTime;
    });
```

`[&]` означает: захватить внешние переменные по ссылке.

В твоем коде lambda часто используется для:

- ECS iteration;
- callbacks;
- helper logic внутри метода.

Будь осторожен:

Если lambda живет дольше текущего метода, захват `[&]` может стать опасным. Для локальной итерации, как в `ForEach`, это нормально.

## 9. `std::vector`, `std::unordered_map`, `std::variant`

### `std::vector`

Динамический массив:

```cpp
std::vector<Entity> connectedRooms;
```

Используется, когда элементов может быть много и их порядок иногда важен.

### `std::unordered_map`

Hash map:

```cpp
std::unordered_map<Entity, T> m_components;
```

Используется в `ComponentManager<T>`: entity id -> component.

### `std::variant`

Хранит одно из нескольких возможных типов.

В `EventSystem`:

```cpp
using GameEvent = std::variant<
    InteractionEvent,
    DoorOpenedEvent
>;
```

Чтобы проверить тип:

```cpp
const auto* interaction = std::get_if<InteractionEvent>(&gameEvent);
if (!interaction)
    continue;
```

## 10. Entity Component System

ECS делит объект на:

- Entity: id;
- Component: данные;
- System: логика.

Пример двери:

```text
Entity 42
    TransformComponent
    NameComponent
    MeshRendererComponent
    RigidBodyComponent
    InteractionComponent
    DoorComponent
```

`DoorComponent` ничего сам не обновляет каждый кадр. Его читает `DoorSystem`.

Преимущества:

- можно легко добавить/убрать поведение entity;
- системы работают пачками по компонентам;
- редактор может показывать компоненты;
- save/load может сериализовать компоненты.

Недостатки:

- нужно аккуратно хранить ссылки `Entity`;
- после load нужен remap entity references;
- легко забыть добавить нужный компонент.

## 11. Game loop

Игра каждый кадр делает:

```text
read input
update game state
update physics
update camera
render world
render UI
present frame
```

В твоем коде это `ApplicationClass::Frame`.

Важное правило:

Не рендерь в update-системах. Не меняй game state внутри shader/render helper без причины.

Плохо:

```text
RenderMesh меняет DoorComponent
```

Хорошо:

```text
DoorSystem меняет DoorComponent
RenderMesh только рисует текущее состояние
```

## 12. Events

Событие это сообщение между системами.

Текущий путь interact:

```text
InteractionSystem::Update
    raycast
    hit entity
    if key C
        EventQueue::Emit(InteractionEvent)

DoorSystem::ProcessEvents
    reads InteractionEvent
    toggles DoorComponent::isOpen
```

Почему это хорошо:

- InteractionSystem не знает про двери;
- DoorSystem не знает про raycast;
- позже можно добавить `RepairSystem`, который тоже читает InteractionEvent.

Когда добавлять event:

- door opened;
- hull breached;
- item picked;
- system repaired;
- player entered room;
- oxygen critical.

## 13. DirectXMath: `XMFLOAT3`, `XMVECTOR`, `XMMATRIX`

### `XMFLOAT3`

Простое хранение трех float:

```cpp
XMFLOAT3 position = { 0, 0, 0 };
```

Используется в компонентах, потому что удобно хранить.

### `XMVECTOR`

SIMD vector для вычислений.

```cpp
XMVECTOR pos = XMLoadFloat3(&position);
```

### `XMMATRIX`

Матрица 4x4 для transform/render.

```cpp
XMMATRIX world = transform.GetWorldMatrix();
```

### Load/Store

Паттерн:

```cpp
XMVECTOR v = XMLoadFloat3(&someFloat3);
v = XMVector3Normalize(v);
XMStoreFloat3(&someFloat3, v);
```

Храни в `XMFLOAT3`, считай в `XMVECTOR`.

## 14. Transform и матрицы

`TransformComponent::GetWorldMatrix`:

```cpp
scale * rotation * translation
```

Порядок важен.

Если поменять порядок, объект начнет:

- вращаться вокруг другой точки;
- масштабироваться в world axis;
- съезжать при повороте.

Для ray/model operations часто нужно перевести world ray в local space:

```cpp
XMMATRIX inverseWorld = XMMatrixInverse(nullptr, world);
localOrigin = XMVector3TransformCoord(worldOrigin, inverseWorld);
localDirection = XMVector3TransformNormal(worldDirection, inverseWorld);
```

Это есть в `PickingSystem` и mesh cutting.

## 15. DirectX render pipeline в проекте

Упрощенно:

```text
AssimpModelClass
    owns vertex/index buffers

MaterialClass
    owns shader pointer and textures

RenderMesh
    chooses shader by ShaderType
    sends matrices/lights/textures
    calls draw

Renderer::RenderFrame
    organizes render passes
```

### Vertex buffer

Список вершин.

### Index buffer

Порядок треугольников.

### Shader resource view

Текстура, которую shader может читать.

### Constant buffer

Маленький блок данных для shader:

- matrices;
- lights;
- fog;
- material params.

В `ShaderBaseClass::UpdateConstantBuffer` используется:

```text
Map -> memcpy -> Unmap -> VSSetConstantBuffers/PSSetConstantBuffers
```

## 16. Materials и shaders

`MaterialClass` выбирает shader и хранит параметры.

Пример:

```cpp
material->SetShader(ShaderManager::Get().GetShaderType(ShaderType::PBR));
material->SetGlass(true, 0.15f);
```

`RenderMesh` смотрит:

```cpp
switch (shader->GetType())
```

и вызывает нужный shader class.

Если добавляешь новый shader:

1. Добавь `ShaderType`.
2. Добавь shader class.
3. Загрузить через `ShaderManager::Load`.
4. Добавь case в `RenderMesh`.
5. Добавь UI выбор, если нужно.

## 17. Bullet Physics в проекте

Bullet хранит физический мир отдельно от ECS.

ECS:

```text
TransformComponent
RigidBodyComponent
```

Bullet:

```text
btRigidBody
btCollisionShape
btDynamicsWorld
```

`PhysicsSystem` связывает эти два мира.

### Static body

```cpp
mass = 0.0f;
isDynamic = false;
```

Подходит для стен, пола, корабельных панелей.

### Dynamic body

```cpp
mass > 0.0f;
isDynamic = true;
```

Подходит для предметов, которые должны падать/сталкиваться.

### Kinematic

Двигается кодом, но участвует в collision. Подходит для moving platforms/doors, если дойдешь до физики дверей.

### TriangleMesh vs ConvexHull

TriangleMesh:

- хорошо для static;
- плохо/опасно для dynamic concave.

ConvexHull:

- проще для dynamic;
- не может быть вогнутым.

GImpact:

- dynamic concave;
- тяжелее;
- нужно осторожно.

## 18. GLB/Blender naming theory

Твой импорт сильно завязан на имена объектов.

Это не плохо. Для прототипа это мощный способ дать designer-friendly pipeline.

Код смотрит:

```text
StartsWith(name, "SHIP_ROOM_")
StartsWith(name, "SHIP_PANEL_")
StartsWith(name, "SHIP_DOOR_")
StartsWith(name, "COL_")
```

Плюсы:

- быстро;
- понятно в Blender;
- не нужен отдельный metadata формат.

Минусы:

- typo ломает gameplay;
- placeholder имена ломают room-link;
- Blender numeric suffix `.001` надо обрезать.

Правило:

Перед дебагом кода всегда сначала проверь имя объекта.

## 19. Двери и герметичность

Есть две герметичности:

### Визуальная

Mesh двери должен выглядеть закрытым. Не должно быть дырок между рамой и створками, если дверь закрыта.

Это решается в Blender/model.

### Логическая

Кислород не должен проходить, если дверь закрыта.

Это решается в коде:

```cpp
DoorComponent::GetEffectiveFlowRate()
RoomEnvironmentSystem::GetDoorFlowMultiplier()
```

Важно:

Даже если mesh выглядит герметичным, oxygen logic не должна пытаться анализировать mesh. Gameplay должен опираться на `DoorComponent`.

## 20. Runtime mesh cutting theory

Текущая резка:

```text
ray hit
local hit point + local normal
build tangent plane
build circular polygon
clip triangles
rebuild CPU mesh
rebuild GPU buffers
refresh collision
```

Это не идеальный CSG.

Главные риски:

- большое число новых triangles;
- дырка без толщины/кромки;
- mismatch visual/collision;
- expensive CPU work;
- не все mesh подходят.

Хорошая архитектура разрушений:

```text
DamageSystem
    получает попадание
    уменьшает HullPanelComponent health
    решает, нужна ли пробоина

MeshCutting
    меняет visual mesh

PhysicsSystem
    обновляет collision

RoomEnvironmentSystem
    симулирует leak/pressure/oxygen

Renderer
    рисует decal/VFX
```

## 21. Save/load и Entity remap

Entity это число в текущем запуске. После загрузки сцены числа могут стать другими.

Поэтому нельзя просто сохранить:

```json
"door_left_leaf": 42
```

и ожидать, что entity 42 всегда будет той же створкой.

После load нужен remap:

```text
old id -> new entity
```

В проекте это делает:

```text
SceneManager::RemapEntityReferences
```

Если добавляешь компонент с полем `Entity`, обязательно добавь remap.

## 22. Debugging без интернета

### Сначала проверь вызов

Если система не работает, найди:

```powershell
rg -n "MySystem::Update|MySystem" .
```

Убедись, что update реально вызывается в `ApplicationClass::Frame` или `SceneManager::UpdateAll`.

### Потом проверь компоненты

Например дверь:

```text
NameComponent
TransformComponent
MeshRendererComponent
RigidBodyComponent
InteractionComponent
DoorComponent
```

Если одного нет, система может молча пропустить entity.

### Потом проверь данные

Для двери:

```text
isOpen
openProgress
animationMode
leftLeaf/rightLeaf
roomA/roomB
```

### Потом проверь side effects

Для двери:

```text
меняется ли TransformComponent створок
не меняется ли TransformComponent рамы
есть ли физика у двери
есть ли prompt
```

### Используй OutputDebugStringA

В проекте уже много:

```cpp
OutputDebugStringA("[Doors] Linked sliding leaves\n");
```

Это нормально для локальной отладки.

## 23. Практические упражнения на C++

### Упражнение 1. Безопасное получение компонента

Напиши функцию:

```cpp
void PrintDoorDebug(ECSRegistry& registry, Entity entity)
{
    auto* door = registry.GetComponent<DoorComponent>(entity);
    if (!door)
        return;

    // OutputDebugStringA with state
}
```

Цель: привыкнуть к pointer/nullptr pattern.

### Упражнение 2. Новый event

Добавь:

```cpp
struct DoorClosedEvent
{
    Entity door = INVALID_ENTITY;
};
```

Потом добавь в `GameEvent`.

Цель: понять `std::variant`.

### Упражнение 3. Новый component

Добавь:

```cpp
struct RepairableComponent
{
    float repairProgress = 0.0f;
    float repairTime = 3.0f;
};
```

Цель: понять, как ECS хранит данные.

### Упражнение 4. Новая system

Сделай `RepairSystem`, который по event или key увеличивает repair progress.

Цель: понять system update.

### Упражнение 5. Debug UI

Покажи `RepairableComponent` в `EditorUI::RenderComponentManager`.

Цель: связать component и editor.

## 24. Практические упражнения по движку

### Упражнение A. Дверь

1. Создай две комнаты.
2. Создай дверь.
3. Создай две створки.
4. Проверь `Sliding`.
5. Проверь oxygen flow.

### Упражнение B. Комната

1. Сделай `SHIP_ROOM_TEST`.
2. Поставь player внутрь.
3. Проверь `objectsInRoom`.
4. Уменьши oxygen.
5. Покажи oxygen на HUD.

### Упражнение C. Пробоина

1. Найди panel через ray.
2. Уменьши health.
3. Когда health <= 0, создай breach.
4. Уменьшай oxygen комнаты.
5. Добавь repair.

### Упражнение D. Render

1. Создай новый material.
2. Назначь PBR shader.
3. Назначь texture.
4. Проверь, что объект рисуется.
5. Включи glass.

## 25. Чеклист перед большим изменением

Перед тем как менять код:

- Я знаю, где вызывается нужная система.
- Я знаю, какие компоненты нужны entity.
- Я знаю, где данные сохраняются.
- Я знаю, нужно ли remap после load.
- Я знаю, надо ли обновлять Bullet body.
- Я знаю, надо ли обновлять GPU buffers.
- Я знаю, какая часть работает только в Editor, а какая в Play.

Если на два пункта ответ "не знаю", сначала найди код через `rg`, потом меняй.

