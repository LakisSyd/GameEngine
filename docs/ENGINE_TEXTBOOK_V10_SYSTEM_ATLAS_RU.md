# Построчный атлас систем DX Framework

Эта часть дополняет подробные главы короткими рабочими маршрутами остальных подсистем. Для каждого маршрута показано, что является входом, где принимается решение, куда записывается результат и каким наблюдением его проверять. Номера строк относятся к снимку репозитория от 1 ноября 2026 года и нужны как ориентир, а не как вечный API.

## 1 ApplicationClass и порядок кадра

`ApplicationClass::Frame` является оркестратором. Он не должен содержать внутреннюю математику каждой механики, но обязан задавать порядок стадий.

```cpp
m_inputManager.Update(m_input);
m_events.Clear();
m_jobSystem.ExecuteMainThreadJobs();
m_assetLoadSystem.Update(m_direct3D->GetDevice());
m_sceneManager.UpdateAsyncShipImport(
    m_direct3D->GetDevice(),
    m_direct3D->GetDeviceContext(),
    4.0f);
```

| Строка | Что происходит | Почему здесь |
|---|---|---|
| обновление input | сырые состояния превращаются во фронты кадра | все потребители должны видеть один snapshot |
| очистка events | удаляются события прошлого кадра | одноразовое событие не должно повторяться |
| main thread jobs | применяются подготовленные worker результаты | D3D и мир меняются на разрешённом потоке |
| asset update | state machine загрузки движется дальше | GPU upload использует device |
| async ship import | импорт получает ограниченный бюджет | большой ассет не должен заморозить кадр |

При добавлении системы сначала выпиши её зависимости. Если она читает результат физики, она не может стоять до fixed step. Если она создаёт движение, оно должно появиться до физического исполнения.

## 2 Режим Editor и Play

Переключение F5 является фронтом нажатия: `f5Down && !m_wasF5Down`. Без проверки прошлого кадра переход выполнялся бы каждый кадр удержания клавиши.

При выходе из Play код обнуляет `CharacterMovement` и команды Bullet controller. Это не косметика: controller хранит последнее направление и продолжил бы применять его в Editor.

Инвариант режима: Editor может редактировать authoring Transform, Play исполняет runtime логику, а переход явно очищает одноразовые намерения.

## 3 Время симуляции и время интерфейса

`DebugHud` получает обычный кадровый `dt`, поэтому сообщения исчезают даже на паузе. Игровые системы получают `simulationDeltaTime`, который учитывает pause, step и time scale NPC Studio.

| Время | Для чего | Нельзя использовать для |
|---|---|---|
| real frame dt | UI загрузка камеры редактора | баланс симуляции на паузе |
| simulation dt | gameplay AI timers animation | сообщение которое должно жить при pause |
| fixed physics dt | Bullet integration | UI transition |
| total world time | журнал последовательность событий | непосредственное движение без delta |

## 4 ECSRegistry построчно

```cpp
Entity CreateEntity()
{
    if (nextEntity == INVALID_ENTITY)
        throw std::overflow_error("ECS entity id space exhausted");
    const Entity entity = nextEntity++;
    m_liveEntities.insert(entity);
    return entity;
}
```

| Строка | Смысл |
|---|---|
| проверка sentinel | запрещает выдать ID зарезервированный как invalid |
| post increment | возвращает текущий ID и готовит следующий |
| live set | делает `IsAlive` независимым от наличия компонентов |
| return | Entity остаётся числовым ключом без поведения |

`DestroyEntity` удаляет ID из live set и просит каждый component pool удалить запись. Поэтому внешние системы не должны хранить голый Entity бесконечно без `IsAlive`.

## 5 ForEach двух компонентов

```cpp
auto* pool1 = FindPool<T1>();
auto* pool2 = FindPool<T2>();
if (!pool1 || !pool2) return;
for (auto& [entity, comp1] : *pool1)
{
    if (pool2->Has(entity))
        fn(entity, comp1, *pool2->Get(entity));
}
```

Перебор идёт по первому pool. Для производительности первым полезно ставить более редкий компонент. Удалять текущий компонент из того же pool внутри callback опасно, если контейнер инвалидирует iterator. Безопасный шаблон: собрать Entity в `toRemove`, удалить после прохода.

## 6 ComponentManager и владение

Каждый тип компонента имеет отдельный pool. `std::type_index` выбирает pool во время выполнения, а шаблон возвращает правильный статический тип. Registry владеет pools через `unique_ptr<IComponentPool>`. Система получает ссылку или указатель на компонент, но не становится его владельцем.

Проверка: после `DestroyEntity` `GetComponent<T>` должен вернуть `nullptr` для каждого существовавшего T.

## 7 TransformSystem

Transform связывает ECS с объектами сцены. Ключевой вопрос каждой строки: локальное это значение или мировое. Позиция, rotation и scale нельзя смешивать с уже собранной world matrix без явного пространства.

Рабочий порядок матрицы проекта нужно проверять по DirectX соглашению и текущему shader contract. Не запоминай `S * R * T` отдельно от того, используются row vectors или column vectors. Проверка проще: возьми точку `(1,0,0)`, применяй операции вручную и сравни с результатом кода.

## 8 SceneManager создание объекта

Путь spawn состоит из нескольких независимых обязательств.

```text
SpawnObjectDesc
    -> CreateEntity
    -> TransformComponent
    -> MeshRendererComponent
    -> optional gameplay components
    -> optional RigidBodyComponent
    -> PhysicsSystem CreateRigidBody
    -> editor name and hierarchy
```

Если видимый объект не имеет физики, проверяй не Renderer, а факт создания `RigidBodyComponent` и вызова `CreateRigidBody`. Если объект существует в Bullet, но не виден, проверяй mesh, material, ActiveComponent и render candidates.

## 9 Сохранение сцены

`SaveScene` должен сериализовать authoring state, а не адреса runtime ресурсов. Указатели на `btRigidBody`, D3D buffer, model instance и controller не имеют смысла в следующем запуске.

| Сохранять | Пересоздавать после load |
|---|---|
| position rotation scale | world matrix cache |
| mass shape type размеры | `btCollisionShape` и `btRigidBody` |
| weapon параметры и ammo если это save game | ray callback и debug traces |
| faction тактический профиль | текущий pointer на target component |
| ссылки как Entity с remap | старые числовые ID без remap |

## 10 Загрузка и Entity remap

Старый Entity ID из JSON нельзя считать новым ID registry. Загрузка сначала создаёт новые сущности и таблицу `old -> new`, затем восстанавливает компоненты и только после этого remap ссылок между сущностями.

Пример: дверь хранит две комнаты. Пока комнаты ещё не созданы, нельзя окончательно записать runtime ссылку. Поэтому восстановление ссылок является отдельной фазой.

## 11 JobSystem

Фоновая задача должна возвращать данные, которые безопасно передать на главный поток.

```text
worker queue
    CPU parse mesh computation path preparation
        -> result object
main thread queue
    D3D resource creation ECS mutation Bullet registration
```

Захват ссылки на временный объект в worker lambda создаёт use after free. Захват `this` требует гарантировать жизнь владельца до завершения job или использовать отмену и weak ownership.

## 12 AssetLoadSystem

Загрузка ассета является state machine, а не одним bool.

1. Запрос создан.
2. Файл читается и разбирается на worker.
3. CPU результат готов.
4. GPU upload выполняется на main thread.
5. Готовый resource публикуется потребителям.
6. Ошибка сохраняется как отдельное конечное состояние.

Потребитель не должен использовать model только потому, что файл уже прочитан. Готовность означает завершение всех стадий, необходимых этому потребителю.

## 13 InputClass и InputManager

InputClass читает состояние устройства. InputManager сравнивает текущий и прошлый кадр.

```text
down     = current
pressed  = current and not previous
released = not current and previous
```

Стрельба automatic weapon может использовать `down`. Переключение режима, перезарядка по кнопке и открытие окна используют `pressed`. Если перепутать, команда повторяется каждый кадр удержания.

## 14 GameplayInputSystem

Этот слой превращает клавиши в именованные `GameAction`. В результате gameplay не обязан знать VK коды. Контекст `isPlayMode`, UI capture и observer camera решает, разрешено ли действие до того, как оно попадёт в движение или оружие.

Инвариант: ImGui, камера и gameplay не должны одновременно считать мышь своей в одном кадре.

## 15 CharacterMovementSystem

Система движения читает intent, направление камеры и настройки скорости, затем пишет `CharacterMovement`. Она не должна напрямую наносить урон или выбирать NPC target.

Проверяемые величины:

- ненормализованное input направление;
- нормализованное горизонтальное направление;
- walk или run speed;
- итоговая скорость в единицах мира в секунду;
- displacement или walk direction для controller;
- флаг grounded и запрос jump.

## 16 CharacterControllerSystem

Controller исполняет движение объёмной капсулой. Его ключевые этапы: reconfigure при изменении размеров, ground probe, step logic, push dynamic body, установка walk direction и запись диагностических флагов.

Если персонаж не движется, сначала проверь intent в `CharacterMovement`, затем controller pointer, затем collision sweep. Если intent уже нулевой, Bullet не является причиной.

## 17 CharacterFacingSystem

Поворот visual model отделён от вращения Y up capsule. Капсула симметрична вокруг вертикальной оси, поэтому бессмысленно warp физический controller каждый кадр только из-за направления взгляда. Это уменьшает дрожание и сохраняет одного владельца физической позиции.

## 18 FirstPersonViewSystem

Первое лицо связывает камеру, visual arms и оружие. Оно является presentation слоем: не должно иметь отдельный magazine или собственное решение о попадании. Entity оружия и `WeaponComponent` остаются общим gameplay источником истины.

## 19 AnimationSystem

Упрощённый рабочий путь анимации:

```text
CharacterMovement and action state
    -> animation controller selects clip and pose variant
    -> AnimationSystem advances time
    -> model computes bone matrices
    -> WeaponIK corrects upper body or hands
    -> renderer uploads matrices
```

`deltaTime` продвигает время клипа. Скорость клипа и скорость locomotion должны связываться явным коэффициентом, иначе ноги скользят. IK применяется после базовой позы, потому что он корректирует уже вычисленную цепь костей.

## 20 TwoBoneIK

Двухкостная IK решает треугольник из корня, сустава и effector. Ей нужны длины двух костей, target и pole direction. Target за пределами суммы длин должен быть ограничен, иначе acos получает значение вне диапазона из-за геометрии или float погрешности.

Проверка: рисуй root, joint, target, pole и окружности достижимости. Без визуализации неверное пространство target выглядит как плохая анимация.

## 21 RenderCandidateSystem

Система кандидатов отделяет вопрос можно ли рисовать объект от самого draw call. Она фильтрует ActiveComponent, mesh, visibility, frustum и другие условия, а Renderer потребляет подготовленный список.

Если объект не виден, проверяй по порядку: Entity active, mesh/model ready, transform finite, candidate создан, frustum test, material/shader, draw call.

## 22 Renderer

`Renderer::RenderFrame` организует passes. Обычный путь объекта:

```text
Transform
    -> world matrix
camera -> view matrix
D3D -> projection matrix
material textures lights
    -> constant buffers and shader resources
    -> draw indexed
```

Renderer не должен менять здоровье, комнату NPC или ammo. Его вход является снимком состояния, а результатом изображение и presentation statistics.

## 23 BaseShader и constant buffers

CPU структура и HLSL `cbuffer` обязаны совпадать по порядку полей, размеру и выравниванию 16 байт. `float3` не гарантирует, что следующее поле можно бездумно уложить в оставшиеся четыре байта во всех структурах C++.

При изменении shader contract меняй четыре места: HLSL объявление, C++ buffer struct, создание buffer size и заполнение перед draw. Debug layer DirectX помогает найти несогласованное bind state, но не понимает смысл перепутанных полей.

## 24 Camera и picking ray

Клик мыши проходит путь screen -> normalized device coordinates -> view space -> world space. Для направления используется inverse view без переноса. После преобразования direction нормализуется.

Проверка: центральный пиксель должен давать направление близкое `camera.GetForward`. Луч из левого края должен отклоняться влево в соответствии с FOV и aspect ratio.

## 25 Material и texture

Material собирает параметры поверхности, texture является GPU ресурсом. Один material может ссылаться на несколько maps: base color, normal, metallic roughness и другие. Gameplay не должен проверять имя texture, чтобы решить физический материал; для этого нужен отдельный gameplay surface tag.

## 26 Frustum culling

Frustum состоит из плоскостей камеры. Bounding sphere даёт быстрый грубый тест, AABB точнее для некоторых форм. Culling не удаляет Entity и не отключает симуляцию мира. Он только исключает draw candidate текущей камеры.

## 27 InteractionSystem

Interaction использует camera или weapon independent ray, находит ближайший допустимый объект, проверяет дистанцию и компонент взаимодействия, затем публикует действие или меняет UI состояния.

Разделяй три факта: объект под прицелом, объект доступен по правилам и кнопка нажата в этом кадре. Только их сочетание выполняет взаимодействие.

## 28 TriggerZoneSystem

Trigger zone хранит множество сущностей прошлого кадра и сравнивает его с текущим пересечением.

```text
current minus previous -> Enter
current intersect previous -> Stay
previous minus current -> Exit
```

Без памяти previous любое пребывание выглядело бы как новый Enter каждый кадр. После удаления Entity устаревший ID надо очищать из множества.

## 29 EventQueue

Producer вызывает `Emit`, consumers читают события в определённой стадии, а начало следующего кадра очищает очередь. Событие описывает уже случившийся факт. Команда описывает желаемое действие. Смешение этих понятий создаёт циклы, когда система повторно считает собственный запрос фактом.

## 30 InventorySystem

Inventory хранит Entity предметов, а предмет хранит собственные компоненты. Экипировка не должна копировать `WeaponComponent` в player. Иначе ammo расходуется в одной копии, а UI показывает другую.

Инварианты:

- один предмет не находится одновременно в двух inventories;
- выбранный slot либо пуст, либо ссылается на живую Entity;
- удаление предмета очищает ссылку слота;
- view model не является вторым gameplay оружием.

## 31 WeaponPickupSystem

Pickup проверяет дистанцию или ray hit, свободный slot, валидность weapon Entity и фронт действия. После успеха меняется ownership и presentation, а не создаётся дубликат оружия без причины.

## 32 WeaponSystem

`UpdateTimers` один раз за кадр продвигает кулдаун и reload всех оружий. `TryFire` выполняет одну механическую попытку. Адаптер player или NPC решает, когда вызывать попытку.

Это разделение позволяет стрелять в движении, менять тактику во время reload и тестировать оружие без камеры.

## 33 DamageSystem

DamageEvent является границей между подтверждённым попаданием и изменением gameplay состояния. DamageSystem проверяет target, тип получателя, armour или hull правила, меняет health, создаёт последствия и отладочную запись.

Очередь защищает от изменения component pools прямо внутри raycast обхода и даёт одну точку для god mode, бессмертия, hit reaction и статистики.

## 34 RepairSystem

Ремонт соединяет input или NPC command, дистанцию, runtime mesh damage, progress и фоновую перестройку геометрии. Сама длительная CPU операция готовится worker, а mutation model, ECS и D3D выполняется на главном потоке.

Нельзя считать ремонт завершённым только по таймеру. Завершение означает, что gameplay damage закрыт, визуальная геометрия обновлена или запланирована, физический shape согласован и reservation освобождён.

## 35 ParticleSystem

Particle проходит состояния alive time, position, velocity, size, color и lifetime. `Update` уменьшает lifetime и интегрирует движение, renderer собирает только живые particles.

Object pool снижает число allocations, но требует полного reset переиспользуемого particle. Забытое поле из прошлой жизни создаёт редкие визуальные артефакты.

## 36 ExplosionSystem

Explosion сначала создаёт visual effect, затем перебирает dynamic bodies, считает falloff и применяет impulse. Визуальная частица не должна быть единственным источником урона: gameplay radius и физический impulse должны быть явными параметрами события взрыва.

## 37 PowerSystem построчно

```cpp
float availableMw = 0.0f;
registry.ForEach<GeneratorComponent>([&](Entity entity, GeneratorComponent& generator)
{
    if (!registry.HasComponent<ActiveComponent>(entity) ||
        !generator.isOnline)
        return;
    availableMw += (std::max)(0.0f, generator.currentOutputMw);
});
```

| Строка | Смысл |
|---|---|
| `availableMw` | бюджет мощности текущего кадра |
| обход generators | источники независимы от игрока |
| active и online | объект существует но может не участвовать |
| clamp снизу | отрицательная генерация не превращается в скрытого consumer |
| сумма | общий бюджет перед распределением |

Затем consumers обнуляются, группируются по priority и получают справедливые доли внутри одного приоритета. `powerRatio` вычисляется только после распределения.

## 38 SupplyEvenly

Алгоритм water filling делит остаток поровну, насыщенные consumers удаляет и повторяет деление между оставшимися. Это важно: простой один проход оставил бы лишнюю мощность у маленького consumer и не перераспределил её большому.

## 39 GeneratorSystem и ShipPowerGridSystem

Generator определяет доступную выработку и расход ресурса. PowerSystem распределяет мгновенную мощность. Grid и battery добавляют хранение энергии. Не смешивай MW и MWh: мощность умноженная на часы даёт энергию.

## 40 RoomSystem

RoomSystem определяет membership объектов в комнатах. Атмосфера, HUD и decompression используют этот результат. Поэтому membership должен обновиться до систем, которые читают `objectsInRoom`.

Для объекта на границе комнаты нужна стабильная политика: приоритет текущей комнаты, epsilon или явный portal transition. Иначе он прыгает между двумя средами каждый кадр.

## 41 RoomEnvironmentSystem построчно

```cpp
if (deltaTime <= 0.0f)
    return;
deltaTime = (std::min)(deltaTime, 0.10f);
auto& rooms = registry.GetComponents<RoomComponent>();
CharacterExternalVelocitySystem::BeginAtmosphereFrame();
```

Guard clause останавливает симуляцию на pause. Clamp не даёт одному длинному кадру мгновенно опустошить комнату. Pool rooms является рабочим набором. `BeginAtmosphereFrame` очищает накопленные внешние скорости до новых источников suction.

Сначала комнаты расходуют кислород, затем `ApplyBreaches` меняет давление и температуру, после этого `EqualizeConnectedRooms` переносит величины между комнатами.

## 42 Двухфазный перенос между комнатами

Проект сначала вычисляет массив `RoomTransfer`, потом применяет его. Если менять комнаты прямо во время первого обхода, результат зависит от порядка Entity: третья комната увидит уже изменённое значение второй.

```text
phase 1 snapshot differences -> transfers
phase 2 transfers -> all room values
```

Это общий алгоритм для тепла, жидкости, газа, заражения и электрических сетей.

## 43 DoorSystem

DoorSystem читает command или interaction state, power, lock и progress. Она изменяет openness и collision representation, затем публикует событие изменения. Атмосфера читает door flow multiplier, а NavMesh решает проход.

Одна дверь поэтому имеет несколько представлений: visual leaves, gameplay state, physics blocker, room connection и navigation portal. Их нужно обновлять из одного состояния, а не хранить пять независимых bool.

## 44 FloodingSystem и PressureSystem

Flooding обновляет объём воды и уровень в compartment. Pressure вычисляет воздействие глубины и целостности корпуса. Физическая сила плавучести использует результат позже.

Проверяй размерности: объём в кубических метрах, уровень в метрах, плотность в килограммах на кубический метр, давление в паскалях или нормализованной игровой величине. Нормализованное поле должно называться так, чтобы его не приняли за SI значение.

## 45 ShipAlertSystem

Alert system агрегирует факты: пожар, низкий кислород, пробоина, недостаток энергии. Она выбирает наиболее серьёзный уровень, но не должна сама создавать пробоину или отключать генератор. Это projection состояния мира для UI и поведения экипажа.

## 46 RoomCullingSystem

Room culling использует текущую комнату камеры и связи portal. Это presentation optimization. Невидимая комната продолжает симулировать атмосферу, питание и NPC, если отдельная simulation LOD политика не говорит иначе.

## 47 EditorUI

EditorUI читает selected Entity, показывает компоненты и пишет authoring поля. Добавление компонента требует три независимых действия: разрешить пункт для подходящей Entity, вызвать `AddComponent` один раз, затем рисовать controls вне `Selectable` блока каждый кадр.

Combo нельзя размещать только внутри события выбора пункта меню: popup закроется, а control не будет существовать на следующих кадрах.

## 48 PickingSystem

Picking строит camera ray, проверяет scene или Bullet hit и возвращает Entity. Он не должен выбирать NPC target для AI: это editor input query с другими фильтрами и временем жизни.

## 49 Debug UI как датчик

Хороший inspector показывает не только итоговый state, но и причину последнего перехода, значения timer, owner Entity, источник команды и rejected candidates. Журнал должен быть ring buffer, иначе длинная симуляция бесконечно растит память.

## 50 Полный вертикальный срез новой системы

Новая механика считается встроенной, когда существует весь путь.

1. Component с authoring и runtime полями.
2. System с явным входом и выходом.
3. Место вызова в кадре.
4. Inspector для настройки.
5. Save и load с version handling.
6. Debug representation и причина отказа.
7. Test scene или автоматический тест.
8. Cleanup runtime ресурсов.
9. Документированный владелец данных.

Если пропустить один пункт, код может компилироваться, но механика останется частично подключённой.

## 51 Чек лист чтения любой системы

- Найди public entrypoint.
- Выпиши guard clauses.
- Перечисли компоненты чтения.
- Перечисли компоненты записи.
- Отметь внешние ресурсы и их владельцев.
- Определи используемое время.
- Найди повторный вызов и one shot события.
- Найди порядок относительно соседних систем.
- Запиши один инвариант.
- Добавь наблюдаемый proof в debug UI.

Этот чек лист применим и к бою, и к двери, и к shader upload. Различаются данные, но инженерский способ чтения остаётся одним.

