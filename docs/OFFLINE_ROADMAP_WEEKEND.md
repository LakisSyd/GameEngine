# Offline roadmap на ближайшие выходные

Этот документ рассчитан на работу без интернета. Его цель не просто дать список задач, а помочь тебе каждый раз понимать:

- что именно сейчас делать;
- какую часть движка трогать;
- какую теорию перед этим прочитать;
- как проверить результат;
- какой уже существующий код использовать, чтобы не писать второй раз то же самое.

Игра сейчас выглядит как космический симулятор корабля с комнатами, дверями, герметичностью, пробоинами, игроком, редактором и DirectX/Bullet-пайплайном. Ближайшая разумная цель: сделать маленький, но цельный вертикальный кусок игры.

## Главная цель выходных

Собрать "живой корабль":

1. Игрок может войти в Play Mode.
2. Игрок ходит по кораблю.
3. Двери открываются через interact и визуально совпадают с логикой.
4. Комнаты правильно связаны через двери.
5. Кислород меняется по комнатам.
6. Повреждение корпуса создает заметный эффект: пробоина, утечка, падение кислорода или другой читаемый сигнал.
7. В редакторе можно быстро понять, что происходит: имена комнат, выбранный объект, дверь, room-link, oxygen.

Не пытайся за выходные сделать "всю игру". Сделай один кусок, который можно показать самому себе и сказать: "Да, это уже gameplay loop".

## Перед началом

Открой проект:

```text
C:\Users\Lakis\Desktop\DX_FrameWork\DX_FrameWork
```

Быстрая сборка:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\amd64\MSBuild.exe' 'C:\Users\Lakis\Desktop\DX_FrameWork\DX_FrameWork.sln' /m /p:Configuration=Debug /p:Platform=x64 /v:minimal
```

Если видишь сообщение:

```text
"pwsh.exe" не является внутренней или внешней командой
```

Это post-build шаг, сама сборка раньше уже могла пройти. Смотри на строку с `DX_FrameWork.exe` и код возврата в терминале Visual Studio/Codex.

## Как работать без интернета

Используй такой цикл:

1. Выбери одну задачу из roadmap.
2. Прочитай короткую теорию под этой задачей.
3. Найди существующий код через `rg`.
4. Внеси маленькое изменение.
5. Собери Debug x64.
6. Проверь в движке.
7. Запиши в `docs/DEV_NOTES_OFFLINE.md`, что получилось и что сломалось.

Полезные команды:

```powershell
rg -n "DoorComponent|DoorSystem|ResolveDoorRoomLinks" src
rg -n "RoomComponent|RoomEnvironmentSystem|oxygen" .
rg -n "HullPanelComponent|HullBreachComponent|CutByRay" .
rg -n "Frame\\(|UpdatePhysics|RenderFrame" src
rg -n "SpawnObjectDesc|SpawnObject\\(" src
```

## Приоритеты

P0: то, без чего нельзя получить playable loop.

- Play Mode стабильно включается.
- Игрок есть и двигается.
- Interact попадает в дверь.
- Дверь меняет `DoorComponent::isOpen`.
- Дверная анимация не ломает геометрию.
- Комнаты подключены по именам.
- Кислород течет через открытую дверь и не течет через закрытую.

P1: то, что делает игру понятной.

- Debug UI для выбранной двери: комнаты A/B, режим Vertical/Sliding, leaf entity.
- Debug UI для выбранной комнаты: oxygen, pressure, connectedRooms, objectsInRoom.
- Визуальный индикатор prompt "C: Open door".
- Несколько тестовых комнат с понятными именами.

P2: то, что делает игру глубже.

- Пробоины влияют на комнату.
- Поврежденные панели создают `HullBreachComponent`.
- Закрытые двери отделяют кислород.
- Роли/станции корабля: двигатель, реактор, щит, навигация.
- Маленькая миссия: "запусти реактор, закрой дверь, почини пробоину".

## День 1. Ориентация и стабилизация

### Задача 1. Собрать проект и записать baseline

Что сделать:

- Собери Debug x64.
- Запусти игру.
- Запиши FPS, число Meshes, Main/Shadow.
- Проверь, что корабль импортируется.
- Проверь, что Play Mode включается по F5.
- Проверь, что F6 режет mesh, если сейчас это ожидаемо.

Теория:

`ApplicationClass::Frame` это главный порядок кадра. Если что-то "не работает", сначала ищи, вызывается ли нужная система в `Frame`.

Порядок примерно такой:

```text
InputManager::Update
EventQueue::Clear
F5/F6 hotkeys
JobSystem main thread jobs
AssetLoadSystem update
Editor camera or Play interaction
DoorSystem events/update
Gameplay controller and character systems
PhysicsSystem
SceneManager::UpdateAll
Renderer::RenderFrame
EditorUI or HUD
Present
```

Файл:

```text
src/Core/ApplicationClass.cpp
```

Готово, если:

- проект собирается;
- ты знаешь, на каком экране/сцене стартуешь;
- у тебя есть записанный baseline.

### Задача 2. Проверить имена комнат и дверей в Blender/GLB

Что сделать:

- В Blender проверь имена объектов.
- Комнаты должны называться так, чтобы код увидел их как `RoomComponent`.
- Дверь должна иметь имя `SHIP_DOOR_A__B`.
- Раздвижные створки должны иметь имена `DOOR_LEAF_L_A__B` и `DOOR_LEAF_R_A__B`.

Важная теория:

Код не читает твои мысли. Он читает строки.

Если дверь называется:

```text
SHIP_DOOR_GUN_ROOM__HALL
```

то код ожидает комнаты:

```text
SHIP_ROOM_GUN_ROOM
SHIP_ROOM_HALL
```

и створки:

```text
DOOR_LEAF_L_GUN_ROOM__HALL
DOOR_LEAF_R_GUN_ROOM__HALL
```

Где это в коде:

```text
src/Scene/SceneManager.cpp
TryParseDoorRoomNames
MakeDoorLeafName
ResolveDoorRoomLinks
FindRoomByName
```

Готово, если:

- в debug output появляется `Linked sliding leaves`;
- дверь не поднимает раму;
- открытие двигает створки.

### Задача 3. Сделать маленькую тестовую сцену из двух комнат

Что сделать:

- Оставь только две комнаты и одну дверь между ними.
- Назови комнаты просто: `SHIP_ROOM_HALL`, `SHIP_ROOM_REACTOR`.
- Назови дверь: `SHIP_DOOR_HALL__REACTOR`.
- Назови створки: `DOOR_LEAF_L_HALL__REACTOR`, `DOOR_LEAF_R_HALL__REACTOR`.
- Импортируй GLB.
- Проверь Play Mode.

Зачем:

Большой корабль сложнее дебажить. Две комнаты дают минимальный тест для:

- room detection;
- door link;
- oxygen flow;
- interaction ray;
- door animation.

Готово, если:

- игрок может стоять в одной комнате;
- дверь открывается;
- обе комнаты есть в `RoomComponent`;
- у двери заполнены `roomA`, `roomB`;
- у двери заполнены `leftLeaf`, `rightLeaf`.

## День 2. Двери, interaction, комнаты

### Задача 4. Сделать debug-панель для двери

Что сделать:

В `EditorUI::RenderComponentManager` или рядом в object properties добавь отображение для `DoorComponent`:

- `isOpen`;
- `openProgress`;
- `animationMode`;
- `roomA`;
- `roomB`;
- `leftLeaf`;
- `rightLeaf`;
- `slideDistance`;
- `openSpeed`.

Не надо создавать новую систему. Используй существующие:

```text
src/Editor/EditorUI.cpp
EditorUI::RenderComponentManager
SceneManager::GetECSRegistry
```

Теория:

Компонент хранит данные. UI только показывает и редактирует данные. Система каждый кадр применяет данные.

```text
DoorComponent = состояние
DoorSystem = поведение
EditorUI = редактирование/просмотр
```

Готово, если:

- при выборе двери ты видишь `Sliding`;
- при нажатии interact меняется `isOpen`;
- `openProgress` плавно идет от 0 до 1.

### Задача 5. Привести DoorSystem к правилу "логика отдельно, визуал отдельно"

Что проверить:

- В режиме `Vertical` двигается root-дверь.
- В режиме `Sliding` root-дверь не двигается.
- В режиме `Sliding` двигаются только `leftLeaf` и `rightLeaf`.
- `openProgress` используется и для анимации, и для кислорода.

Файл:

```text
src/ECS/System/DoorSystem.h
```

Теория:

`openProgress` лучше, чем мгновенный `isOpen`, потому что:

- визуально дверь открывается плавно;
- кислород может начать течь постепенно;
- позже можно добавить звук/искры/застревание.

Мини-задание:

- Добавь в debug UI кнопку "Force open progress 0/1".
- Проверь, что кислород и визуал совпадают.

### Задача 6. Проверить InteractionSystem

Что сделать:

- В Play Mode наведи камеру на дверь.
- Проверь prompt.
- Нажми `C`.
- Убедись, что создается `InteractionEvent`.
- Убедись, что `DoorSystem::ProcessEvents` его получает.

Файлы:

```text
InteractionSystem.h
EventSystem.h
src/ECS/System/DoorSystem.h
src/Core/ApplicationClass.cpp
```

Теория:

События нужны, чтобы одна система не знала слишком много о другой.

Плохой вариант:

```text
InteractionSystem напрямую меняет DoorComponent
```

Хороший текущий вариант:

```text
InteractionSystem emits InteractionEvent
DoorSystem reads InteractionEvent
DoorSystem toggles DoorComponent
```

Готово, если:

- prompt появляется только на интерактивных объектах;
- дверь открывается только когда луч попал в объект с `InteractionComponent`;
- игрок не взаимодействует сам с собой, потому что `IgnorePlayerRayCallback` фильтрует player.

## День 3. Кислород и комнаты

### Задача 7. Сделать видимый oxygen debug

Что сделать:

В редакторе или HUD покажи для комнаты:

- room name;
- oxygen;
- pressure;
- connectedRooms count;
- objectsInRoom count.

Файлы:

```text
RoomSystem.h
RoomEnvironmentSystem.h
GameHud.h
src/Editor/EditorUI.cpp
```

Теория:

`RoomSystem::Update` отвечает за "кто находится в комнате".

`RoomEnvironmentSystem::Update` отвечает за расход кислорода и перетекание кислорода между комнатами.

Не смешивай их:

- detection объектов в комнате это геометрия;
- oxygen это симуляция среды.

Готово, если:

- стоя в комнате, ты видишь, что `objectsInRoom` изменился;
- oxygen уменьшается, если в комнате есть player;
- oxygen между комнатами выравнивается только при открытой двери.

### Задача 8. Проверить герметичность двери логически

Что сделать:

- Поставь oxygen одной комнаты в `1.0`.
- Поставь oxygen второй комнаты в `0.0`.
- Закрой дверь.
- Подожди 10 секунд.
- Oxygen не должен выравниваться.
- Открой дверь.
- Oxygen должен начать выравниваться.

Кодовая точка:

```text
RoomEnvironmentSystem::GetDoorFlowMultiplier
DoorComponent::GetEffectiveFlowRate
```

Теория:

Сейчас герметичность не проверяет mesh-дырки. Она проверяет состояние `DoorComponent`.

Это нормально. В игре визуальный mesh должен соответствовать логике, но сама симуляция кислорода должна опираться на надежные gameplay-данные:

```text
DoorComponent::openProgress
DoorComponent::roomA
DoorComponent::roomB
```

Готово, если:

- закрытая дверь дает flow 0;
- открытая дверь дает flow больше 0;
- сломанная дверь считается открытой или протекающей по твоему правилу.

### Задача 9. Добавить состояния двери

Минимальный набор:

- normal;
- locked;
- brokenOpen;
- brokenClosed.

Не обязательно делать enum сразу, можно начать с полей:

```cpp
bool requiresPower;
bool isBroken;
bool isOpen;
```

Но если начнет разрастаться, лучше добавить:

```cpp
enum class DoorState
{
    Closed,
    Opening,
    Open,
    Closing,
    Locked,
    BrokenOpen,
    BrokenClosed
};
```

Теория:

`bool` удобен, пока состояний 2. Если состояний 5, набор bool быстро начинает противоречить сам себе:

```text
isOpen = true
isBroken = true
isLocked = true
```

Это уже неочевидно. Для сложных правил лучше enum.

Готово, если:

- ты можешь заблокировать дверь;
- interact на locked door не открывает ее;
- debug UI показывает почему.

## День 4. Пробоины и повреждения

### Задача 10. Разобраться с текущей резкой mesh

Прочитай:

```text
docs/MeshCuttingGuide.md
src/ECS/System/MeshCuttingTest.h
src/ECS/System/MeshCuttingTest.cpp
src/Render/Model/assimpmodelclass.h
src/Render/Model/assimpmodelclass.cpp
```

Что проверить:

- F6 строит ray из камеры.
- `MeshCuttingTest::CutByRayAsync` ищет ближайший mesh hit.
- `AssimpModelClass::CutCircularHoleForTest` меняет CPU-копию геометрии.
- Потом пересоздаются GPU buffers.
- Если есть `RigidBodyComponent`, надо обновить физику через `PhysicsSystem::RefreshRigidBody`.

Теория:

Сейчас это не полноценный CSG. Это runtime mesh mutation test. Значит:

- можно получить дыру;
- можно получить много новых треугольников;
- можно получить грубые края;
- надо очень осторожно делать это на больших мешах;
- лучше резать панели, а не весь корабль.

Готово, если:

- ты можешь объяснить себе путь от F6 до изменения vertex/index buffer;
- ты знаешь, где создается физическая коллизия после изменения mesh.

### Задача 11. Связать пробоину с комнатой

Что сделать:

- Когда пробивается `HullPanelComponent`, найти `panel.room`.
- Создать `HullBreachComponent` или `BreachData`.
- Увеличить leak/уменьшить oxygen/pressure в комнате.

Где уже есть данные:

```text
HullPanelComponent::roomName
HullPanelComponent::room
HullBreachComponent
BreachData
FloodingSystem::CreateBreach
FloodingSystem::CreateBreachAtWorldPosition
```

Теория:

Визуальная дырка и gameplay-пробоина это разные вещи:

- визуальная дырка: mesh изменился;
- физическая дырка: collision изменился;
- gameplay-пробоина: комната теряет кислород/давление.

Хороший порядок:

```text
hit panel
damage panel health
if threshold reached:
    cut visual mesh
    refresh collision
    create breach gameplay component/data
    update room leak
```

Готово, если:

- пробитая панель знает свою комнату;
- комната начинает терять кислород;
- debug UI показывает breach/leak.

### Задача 12. Сделать ремонт

Минимальный ремонт:

- игрок смотрит на пробоину;
- нажимает `C`;
- `sealed = true`;
- flow/leak становится 0;
- визуально можно оставить дырку, но добавить decal/заглушку позже.

Где смотреть:

```text
InteractionSystem.h
FloodingSystem::SealBreach
DamageDecalComponent::SyncFromBreaches
```

Теория:

Сначала делай gameplay, потом красоту. Если пробоина уже влияет на комнату и может быть закрыта, игра стала глубже даже без идеального VFX.

Готово, если:

- пробоину можно закрыть;
- oxygen перестает падать;
- игрок понимает, что сделал.

## День 5. Вертикальный игровой сценарий

### Задача 13. Собрать один сценарий на 3 минуты

Сценарий:

1. Игрок стартует в `HALL`.
2. Дверь в `REACTOR` закрыта.
3. Игрок открывает дверь.
4. В реакторной кислород ниже.
5. Игрок заходит, запускает "реактор" через interact.
6. Появляется пробоина или падение давления.
7. Игрок закрывает дверь, чтобы изолировать комнату.
8. Игрок чинит пробоину.

Не обязательно делать все красиво. Главное, чтобы цикл был играбельный.

Что использовать:

- `InteractionComponent` для объектов, которые можно нажимать.
- `EventQueue` для событий.
- `DoorComponent` для дверей.
- `RoomComponent` для комнаты.
- `RoomEnvironmentSystem` для oxygen.
- `GameHud` или debug overlay для статуса.

Готово, если:

- ты можешь пройти сценарий без перезапуска движка;
- по HUD/debug понятно, что происходит;
- у тебя есть список багов, а не ощущение "ничего не работает".

## Бэклог после выходных

### P0. Основы корабля

- Проверить все naming conventions в Blender.
- Убрать placeholder `ROOMA/ROOMB` из финальных дверей.
- Сделать один small test ship.
- Сделать одну стабильную сцену `LEVEL_TEST_SHIP.json`.
- Сделать автопроверку: дверь с leaf-объектами должна быть `Sliding`.

### P1. Комнаты и атмосфера

- Room debug view.
- Oxygen HUD.
- Pressure HUD.
- Door flow debug.
- Leaks per room.
- Разные door flow rates для маленькой/большой двери.

### P1. Игрок

- First person camera polish.
- Interaction distance.
- Prompt style.
- Locked/broken door feedback.
- Basic item/tool: repair tool.

### P1. Повреждения

- Health у панелей.
- Damage threshold на пробоину.
- Decal трещины.
- Repair action.
- Ограничение пробоин только на внешние панели.

### P2. Корабельные системы

- Reactor power.
- Engines.
- Shields.
- Navigation.
- Life support.
- Door power dependency.
- Fire/electrical hazard.

### P2. Кооперативная фантазия в стиле космического экипажа

Даже если игра пока singleplayer, думай ролями:

- Pilot: рулит кораблем.
- Engineer: чинит реактор, двери, питание.
- Scientist/Scanner: ищет цели, аномалии.
- Gunner: управляет оружием.
- Captain: принимает решения и видит общую карту.

Для прототипа хватит одного игрока, который временно выполняет все роли через панели.

## Мини-теория по архитектуре задач

### Когда добавлять новый Component

Добавляй компонент, если нужно хранить состояние на entity.

Примеры:

```text
DoorComponent хранит состояние двери.
RoomComponent хранит кислород комнаты.
HullPanelComponent хранит здоровье панели.
```

Не добавляй компонент для одноразовой локальной переменной.

### Когда добавлять новую System

Добавляй систему, если нужно каждый кадр или по событию обрабатывать много entity одного типа.

Примеры:

```text
DoorSystem обновляет все двери.
RoomSystem ищет объекты внутри комнат.
RoomEnvironmentSystem обновляет oxygen.
PhysicsSystem синхронизирует Bullet и Transform.
```

Не добавляй систему, если задача касается одного места UI.

### Когда добавлять Event

Добавляй event, если действие одной системы должно быть услышано другой системой без жесткой зависимости.

Пример:

```text
InteractionSystem -> InteractionEvent -> DoorSystem
```

В будущем:

```text
DamageSystem -> HullBreachEvent -> RoomEnvironmentSystem
DoorSystem -> DoorOpenedEvent -> SoundSystem
```

### Когда использовать существующий метод

Частые случаи:

```text
Нужно создать объект -> SceneManager::SpawnObject
Нужно загрузить модель -> SceneManager::GetOrCreateModel
Нужно создать свет -> SceneManager::SpawnLight
Нужно включить/выключить объект -> SceneManager::SetObjectActive
Нужно найти комнату -> SceneManager::FindRoomByName
Нужно связать комнаты -> SceneManager::ConnectRooms / ConnectRoomsByName
Нужно обновить rigid body после изменения mesh -> PhysicsSystem::RefreshRigidBody
Нужно узнать попадание мышью -> PickingSystem::PickAt
Нужно отправить gameplay событие -> EventQueue::Emit
```

## Definition of Done для выходных

Считай выходные успешными, если есть:

- одна сцена с кораблем;
- минимум две комнаты;
- минимум одна рабочая раздвижная дверь;
- рабочий Play Mode;
- interaction с дверью;
- oxygen debug;
- один понятный gameplay incident: пробоина, утечка или разгерметизация;
- заметки, что делать дальше.

Если сделаешь только первые четыре пункта, это тоже хороший результат. Главное, чтобы кусок был стабильный.

