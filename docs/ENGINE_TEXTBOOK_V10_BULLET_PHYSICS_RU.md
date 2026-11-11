# Bullet Physics в DX Framework

Эта часть объясняет Bullet не как набор загадочных классов, а как отдельную симуляцию, которую движок связывает с ECS. В проекте Bullet отвечает за столкновения, rigid body, character controller, raycast, convex sweep, импульсы взрывов и constraints подлодки. ECS хранит игровые данные, DirectX рисует результат, а Bullet вычисляет физическое состояние.

## 1 Ментальная модель Bullet

У физического объекта есть несколько разных представлений.

| Представление | Пример | Ответственность |
|---|---|---|
| Игровая сущность | `Entity 42` | связывает компоненты и правила игры |
| Положение игры | `TransformComponent` | позиция для сцены редактора и рендера |
| Геометрия столкновения | `btCollisionShape` | отвечает где объект физически занимает место |
| Физическое состояние | `btRigidBody` | масса скорость силы трение сон |
| Регистрация в мире | `btDynamicsWorld` | делает объект участником запросов и симуляции |
| Визуальный меш | `MeshRendererComponent` | рисует форму но сам не сталкивается |

Главный инвариант: render mesh, collision shape и `TransformComponent` могут описывать один предмет, но это не один объект памяти. Изменение масштаба модели не перестраивает коллайдер автоматически. Перенос `TransformComponent` не обязан переносить dynamic body, если система синхронизации не подтверждает внешний teleport.

## 2 Слои вычисления столкновения

Bullet не сравнивает каждый треугольник со всеми остальными треугольниками.

1. Broadphase хранит грубые AABB и находит потенциальные пары.
2. Narrowphase проверяет реальные формы потенциальной пары.
3. Dispatcher выбирает алгоритм столкновения.
4. Solver вычисляет импульсы, которые исправляют скорость и проникновение.
5. Dynamics world интегрирует движение и обслуживает constraints.

В текущем мире используется `btDbvtBroadphase`. DBVT означает динамическое дерево ограничивающих объёмов. Когда объект переносится вручную, его AABB должен быть обновлён, иначе broadphase некоторое время знает старое место.

## 3 Создание физического мира построчно

Файл `src/Core/PhysicsWorld.h` создаёт Bullet зависимости во временных `unique_ptr` и только после успеха передаёт их полям singleton.

```cpp
auto collisionConfig = std::make_unique<btDefaultCollisionConfiguration>();
auto dispatcher = std::make_unique<btCollisionDispatcher>(collisionConfig.get());
auto broadphase = std::make_unique<btDbvtBroadphase>();
auto solver = std::make_unique<btSequentialImpulseConstraintSolver>();
auto ghostCallback = std::make_unique<btGhostPairCallback>();
auto dynamicsWorld = std::make_unique<btDiscreteDynamicsWorld>(
    dispatcher.get(), broadphase.get(), solver.get(), collisionConfig.get());
```

| Строка | Что создаётся | Почему порядок важен |
|---|---|---|
| `collisionConfig` | настройки и фабрики narrowphase | dispatcher хранит на неё не владеющий указатель |
| `dispatcher` | выбор алгоритма для пары форм | мир вызывает его при обработке потенциальных пар |
| `broadphase` | DBVT для грубого отсева | без него мир не знает какие пары проверять |
| `solver` | последовательный импульсный решатель | исправляет скорости контактов и constraints |
| `ghostCallback` | поддержка overlap пар ghost object | нужен character controller |
| `dynamicsWorld` | объект который соединяет подсистемы | не владеет всеми переданными зависимостями |

Временные `unique_ptr` защищают от частично созданного мира. Если конструктор одного следующего объекта бросит исключение, уже созданные зависимости будут уничтожены автоматически. После успешной настройки они перемещаются в поля. Адрес самих Bullet объектов при перемещении `unique_ptr` не меняется.

## 4 Гравитация ghost callback и GImpact

```cpp
dynamicsWorld->setGravity(btVector3(0, -9.81f, 0));
dynamicsWorld->getPairCache()->setInternalGhostPairCallback(ghostCallback.get());
btGImpactCollisionAlgorithm::registerAlgorithm(dispatcher.get());
```

Первая строка задаёт ускорение мира в метрах на секунду в квадрате. Вторая разрешает pair cache отслеживать пересечения `btPairCachingGhostObject`. Третья регистрирует narrowphase алгоритм для GImpact. Создать `btGImpactMeshShape` недостаточно: dispatcher должен знать, каким алгоритмом обрабатывать такую пару.

## 5 Настройка solver

В текущем проекте число итераций увеличено до 50, включён split impulse и настроен ERP.

| Параметр | Что делает | Симптом слишком малого или большого значения |
|---|---|---|
| `m_numIterations` | сколько раз solver уточняет ограничения | мало даёт мягкие дрожащие joints много повышает CPU стоимость |
| `m_splitImpulse` | отделяет коррекцию проникновения от обычной скорости | без него глубокое проникновение может подбросить тело |
| `m_splitImpulsePenetrationThreshold` | глубина для split correction | неверный порог меняет характер исправления контакта |
| `m_erp` | доля позиционной ошибки исправляемая за шаг | слишком высоко даёт жёсткие рывки слишком низко оставляет ошибку |

Эти параметры не заменяют правильную начальную расстановку тел. Если два тяжёлых отсека создаются глубоко друг в друге, solver всё равно вынужден бороться с большой ошибкой.

## 6 Fixed timestep и аккумулятор

`PhysicsWorld::Update` не передаёт произвольный кадровый `dt` напрямую в физику.

```cpp
constexpr float timestep = 1.0f / 60.0f;
m_accumulator += (std::max)(0.0f, deltaTime);
if (m_accumulator > 0.25f)
    m_accumulator = 0.25f;
while (m_accumulator >= timestep)
{
    m_dynamicsWorld->stepSimulation(timestep, 1, timestep);
    m_accumulator -= timestep;
    ++m_stepIndex;
}
m_interpolationAlpha = std::clamp(m_accumulator / timestep, 0.0f, 1.0f);
```

| Строка | Смысл |
|---|---|
| `1 / 60` | один физический шаг всегда длится одну шестидесятую секунды |
| прибавление `deltaTime` | кадр отдаёт накопителю прошедшее реальное время |
| clamp до `0.25` | предотвращает spiral of death после долгого зависания |
| `while` | выполняет столько полных шагов сколько накоплено |
| `stepIndex` | сообщает синхронизации что появился новый физический результат |
| `interpolationAlpha` | показывает долю пути от прошлого fixed state к текущему |

Если рендер работает на 144 Hz, а Bullet на 60 Hz, несколько кадров могут видеть одинаковый физический state. Интерполяция делает визуальное движение плавным, но не должна возвращаться в Bullet как новая физическая позиция.

## 7 Static dynamic и kinematic

| Тип | Масса | Кто задаёт Transform | Реагирует на силы | Типичный объект |
|---|---|---|---|---|
| Static | `0` | создаётся один раз | нет | пол стена |
| Dynamic | больше `0` | Bullet после шага | да | ящик обломок отсек |
| Kinematic | обычно `0` и флаг | ECS перед шагом | не как обычное dynamic body | управляемая дверь платформа |

Нельзя определять тип только по массе. Kinematic body имеет специальный collision flag. В `PhysicsSystem::Update` проект сначала проверяет этот флаг, затем массу.

## 8 Владелец Transform и направление синхронизации

Текущий `PhysicsSystem::Update` разделяет синхронизацию на две стороны.

```text
до fixed step
    kinematic        ECS -> Bullet каждый кадр
    dynamic teleport ECS -> Bullet только при внешнем изменении
    static           обычно не трогаем

после fixed step
    dynamic          Bullet -> ECS
    character        ghost object -> визуальный Transform с интерполяцией
```

Это один из главных контрактов физики. Если каждый кадр писать Transform dynamic body обратно в Bullet, физика не владеет движением: скорость и solver результат постоянно перезаписываются. Если никогда не отправлять внешний teleport, перемещение в редакторе останется только визуальным.

## 9 Проверка внешнего изменения

`TransformChangedExternally` сравнивает текущий Transform с `lastSyncedPosition`, `lastSyncedRotation` и `lastSyncedScale`. Равенство допустимо здесь потому, что обе стороны сравнения были записаны самим синхронизатором. Это не общий рецепт сравнения любых вычисленных float.

При внешнем изменении dynamic body проект:

1. строит новый `btTransform`;
2. записывает world transform тела и motion state;
3. обновляет interpolation transform;
4. обнуляет скорости и силы;
5. вызывает `updateSingleAabb`;
6. активирует тело;
7. сохраняет последние синхронизированные значения.

Обнуление скорости превращает операцию в teleport, а не в продолжение прежнего полёта. Для игровой механики броска с переносом понадобится отдельный контракт, который сохраняет или назначает скорость явно.

## 10 Деактивация ECS сущности

Неактивный объект не удаляется из Bullet, но временно переводится в состояние без контактной реакции и симуляции.

```cpp
rb.body->setCollisionFlags(
    rb.collisionFlagsBeforeSuspend |
    btCollisionObject::CF_NO_CONTACT_RESPONSE);
rb.body->setLinearVelocity(btVector3(0, 0, 0));
rb.body->setAngularVelocity(btVector3(0, 0, 0));
rb.body->clearForces();
rb.body->forceActivationState(DISABLE_SIMULATION);
```

Перед этим сохраняются прежние flags и activation state. При возвращении `ActiveComponent` они восстанавливаются. Иначе временная деактивация навсегда превращала бы body в trigger или спящий объект.

## 11 Collision shape и масштаб

Bullet формы получают размеры при создании. Текущий проект поддерживает box, sphere, cylinder, capsule, convex hull, triangle mesh, GImpact и compound shape.

| Shape | Использование | Ограничение |
|---|---|---|
| `btBoxShape` | стены ящики простые предметы | передаются полуразмеры |
| `btSphereShape` | снаряд область простая динамика | не зависит от вращения |
| `btCylinderShape` | цилиндрические объекты | ось формы должна совпасть с моделью |
| `btCapsuleShape` | персонаж | height относится к цилиндрической части Bullet API |
| `btConvexHullShape` | динамический сложный объект | должен оставаться выпуклым |
| `btBvhTriangleMeshShape` | точная статическая геометрия | не предназначен для обычного dynamic body |
| `btGImpactMeshShape` | динамический вогнутый меш | дороже простых и convex форм |
| `btCompoundShape` | несколько простых частей | нужно отдельно уничтожать child shapes |

Нулевой и отрицательный scale недопустимы. В проекте они заменяются на единицу. Когда scale меняется после создания, `RefreshRigidBody` полностью перестраивает shape, потому что одного изменения визуального Transform недостаточно.

## 12 Почему box принимает полуразмеры

Для полного размера `(2, 4, 6)` в Bullet передаётся `(1, 2, 3)`. Эти числа означают расстояние от центра формы до каждой грани. Ошибка в два раза создаёт коллайдер, который выглядит смещённым или чрезмерно большим, хотя его центр математически правильный.

## 13 Triangle mesh и владение памятью

Текущий `CreateTriangleMeshShape` создаёт два связанных объекта: `btTriangleMesh` хранит треугольники, а `btBvhTriangleMeshShape` использует эту геометрию для collision query. Поэтому `RigidBodyComponent` хранит указатель `triangleMesh` отдельно. Удалить backing mesh раньше shape нельзя.

Путь каждой тройки индексов:

```cpp
const auto& p0 = vertices[ia].position;
const auto& p1 = vertices[ib].position;
const auto& p2 = vertices[ic].position;
triangleMesh->addTriangle(
    btVector3(p0.x * scale.x, p0.y * scale.y, p0.z * scale.z),
    btVector3(p1.x * scale.x, p1.y * scale.y, p1.z * scale.z),
    btVector3(p2.x * scale.x, p2.y * scale.y, p2.z * scale.z),
    true);
```

Индексы проверяются до обращения к vertices. Масштаб применяется к каждой координате. Флаг `true` разрешает удаление дублирующихся вершин внутри `btTriangleMesh` по его правилам.

## 14 GImpact и цена точности

GImpact позволяет dynamic concave mesh, но его нельзя делать формой по умолчанию для каждого предмета. Простые формы и convex hull дешевле, устойчивее и дают solver более понятные контакты. GImpact оправдан, когда игровая геометрия действительно вогнута и должна двигаться как единое тело.

После создания текущий код вызывает `updateBound`. Без обновлённой границы broadphase не имеет корректного AABB формы.

## 15 Compound shape

Compound shape хранит дочерние формы и локальный transform каждой части. Это позволяет собрать коллайдер корабля из нескольких box и capsule без одного тяжёлого triangle mesh.

```text
world transform body
    * local transform child 0 -> box
    * local transform child 1 -> cylinder
    * local transform child 2 -> convex hull
```

При удалении compound текущий код проходит child shapes, удаляет их из compound и освобождает память. Одного `delete compound` недостаточно, если проект сам владеет созданными через `new` детьми.

## 16 Создание rigid body

Минимальная последовательность Bullet выглядит так.

```cpp
btVector3 inertia(0, 0, 0);
if (mass > 0.0f)
    shape->calculateLocalInertia(mass, inertia);
auto* motionState = new btDefaultMotionState(startTransform);
btRigidBody::btRigidBodyConstructionInfo info(
    mass, motionState, shape, inertia);
auto* body = new btRigidBody(info);
world->addRigidBody(body, group, mask);
```

| Строка | Контракт |
|---|---|
| нулевая inertia | static body не нуждается в динамическом моменте инерции |
| `calculateLocalInertia` | shape и mass определяют сопротивление вращению |
| motion state | канал между body и внешним представлением transform |
| construction info | собирает массу форму motion state и inertia |
| `addRigidBody` | только здесь body начинает участвовать в мире |

Созданный объект до `addRigidBody` существует в памяти, но raycast мира его не увидит.

## 17 Collision groups и masks

Group отвечает на вопрос кто я. Mask отвечает на вопрос кого я хочу проверять. Пара рассматривается, когда оба битовых условия разрешают друг друга.

```text
(groupA & maskB) != 0
and
(groupB & maskA) != 0
```

В `PhysicsSystem.h` группы оформлены как биты: default, dynamic, submarine exterior, submarine interior, player, projectile, trigger. Такое разделение позволяет игроку не сталкиваться со служебной внутренней формой подлодки, а внешнему корпусу не получать контакты с персонажами внутри.

При добавлении новой группы выпиши таблицу взаимодействий до кода. Ошибка маски проявляется как объект, который виден, но не сталкивается, либо как trigger, который никогда не получает overlap.

## 18 Entity внутри Bullet объекта

Проект связывает collision object с ECS через `setUserIndex` и читает его через `getUserIndex`. Это удобно для raycast, но имеет контракт: `INVALID_ENTITY` и диапазон типа должны обрабатываться одинаково во всех местах. После уничтожения Entity зарегистрированный Bullet object не должен остаться в мире с устаревшим index.

## 19 Raycast как запрос мира

Луч задаётся двумя точками, а не только направлением.

```text
start = origin
end = origin + normalize(direction) * distance
```

`PhysicsRaycast::CastRay` сначала очищает `outHit`, проверяет мир и дистанцию, нормализует направление, отвергает нечисловой результат и только после этого вызывает `world->rayTest`.

```cpp
IgnoreEntityRayCallback callback(
    outHit.rayStart, outHit.rayEnd,
    ignoredObject, ignoredEntity);
world->rayTest(outHit.rayStart, outHit.rayEnd, callback);
```

Raycast не двигает тела и не создаёт урон. Он только отвечает на геометрический вопрос о первом разрешённом контакте.

## 20 Пользовательский callback луча

`IgnoreEntityRayCallback` наследует `ClosestRayResultCallback` и переопределяет `addSingleResult`.

```cpp
if (hitObject == m_ignoredObject ||
    static_cast<Entity>(hitObject->getUserIndex()) == m_ignoredEntity ||
    hitObject->getActivationState() == DISABLE_SIMULATION)
{
    return 1.0f;
}
return ClosestRayResultCallback::addSingleResult(
    rayResult, normalInWorldSpace);
```

Возврат `1.0f` сообщает callback не принимать этот кандидат как ближайшее попадание. Сравнение делается и по указателю объекта, и по Entity, потому что у одной сущности могут быть дополнительные collision objects. Неактивный объект также пропускается.

## 21 Closest и all hits

| Callback | Результат | Пример |
|---|---|---|
| `ClosestRayResultCallback` | ближайший разрешённый hit | оружие взгляд взаимодействие |
| `AllHitsRayResultCallback` | массив всех hit | прострел нескольких слоёв анализ материалов |
| custom closest | ближайший после собственных фильтров | игнорирование стрелка и disabled objects |

Для обычного оружия нужен closest hit. Иначе можно случайно нанести урон выбранной AI цели за стеной, проигнорировав реальный первый контакт.

## 22 От raycast к выстрелу

Текущее оружие является hitscan, а не летящим физическим снарядом.

```text
нажатие или решение NPC
    -> WeaponSystem TryFire
    -> проверка магазина кулдауна и луча
    -> разброс и recoil
    -> PhysicsRaycast ближайший контакт
    -> DamageEvent
    -> DamageSystem в своей стадии кадра
```

Bullet здесь отвечает только за геометрическое подтверждение. Патроны, перезарядка, урон и фракции являются gameplay правилами.

## 23 Convex sweep

Raycast проверяет бесконечно тонкий отрезок. Персонаж имеет объём, поэтому для движения и step-up нужен sweep выпуклой формы.

```cpp
world.convexSweepTest(
    &shape,
    fromTransform,
    toTransform,
    callback);
```

Sweep отвечает: как далеко форма может пройти по траектории до контакта. `m_closestHitFraction` находится в диапазоне от нуля до единицы. Позиция контакта вдоль перемещения вычисляется как `from + delta * fraction`.

## 24 Нормаль поверхности и допустимый склон

Character callbacks фильтруют поверхность по скалярному произведению нормали с мировой осью вверх.

```text
upDot = normal dot (0, 1, 0)
```

Горизонтальный пол имеет normal примерно `(0, 1, 0)` и `upDot` около единицы. Вертикальная стена имеет значение около нуля. Порог можно получить как `cos(maxSlopeAngle)`. Чем больше допустимый угол склона, тем меньше порог.

## 25 Ghost object и character controller

`CharacterSpawnSystem` создаёт capsule shape, `btPairCachingGhostObject` и `btKinematicCharacterController`. Ghost object хранит форму и overlap пары, но сам не решает динамический контакт как rigid body. Character controller выполняет специальную логику ходьбы, step up, step down и penetration recovery.

Порядок регистрации:

1. создать capsule;
2. создать ghost object;
3. назначить transform и collision shape;
4. добавить `CF_CHARACTER_OBJECT`;
5. создать controller;
6. настроить gravity, slope и step height;
7. добавить collision object с group и mask;
8. добавить controller как action мира.

При уничтожении порядок обратный: удалить action, удалить collision object, затем освободить controller, ghost object и shape.

## 26 Почему центр капсулы выше ног

`TransformComponent` персонажа хранит игровую позицию у основания, а Bullet transform ghost object относится к центру формы. Поэтому используется `capsuleCenterOffsetY`.

```text
bulletCenterY = feetY + capsuleCenterOffsetY
feetY = bulletCenterY - capsuleCenterOffsetY
```

Если вычесть offset дважды, модель провалится. Если не вычесть после чтения Bullet, она будет висеть над поверхностью.

## 27 Ground probe и step up

Ground detection не должен полагаться только на один флаг controller. Текущий код использует ray и convex sweep с фильтром поверхности. Для step up обычно проверяются три этапа.

1. Горизонтальное движение встречает препятствие.
2. Поднятая капсула имеет свободное место.
3. Из поднятой и сдвинутой позиции можно опуститься на допустимую поверхность.

Каждый этап требует отдельного наблюдаемого debug луча или sweep. Иначе баг выглядит как общее застревание, хотя причина может быть в потолке, слишком высокой ступени или неверной нормали посадки.

## 28 Контакты и manifolds

После шага dispatcher хранит contact manifolds. Каждый manifold относится к паре collision objects и содержит contact points. У точки доступны глубина, normal, позиции на A и B, а также применённый импульс.

Схема чтения:

```cpp
const int count = dispatcher->getNumManifolds();
for (int i = 0; i < count; ++i)
{
    btPersistentManifold* manifold = dispatcher->getManifoldByIndexInternal(i);
    for (int j = 0; j < manifold->getNumContacts(); ++j)
    {
        const btManifoldPoint& point = manifold->getContactPoint(j);
        if (point.getDistance() <= 0.0f)
        {
            // подтверждённый контакт
        }
    }
}
```

Не создавай gameplay событие каждый fixed step без памяти пары. У устойчивого контакта manifold живёт много шагов, поэтому звук удара или damage on enter требует отслеживания перехода пары из отсутствующей в активную.

## 29 Trigger и no contact response

Trigger использует collision object с `CF_NO_CONTACT_RESPONSE`. Broadphase и overlap продолжают работать, но solver не выталкивает тела. Это отличается от mask, которая полностью запрещает образование пары.

| Требование | Механизм |
|---|---|
| видеть вход без физического толчка | trigger и overlap pairs |
| полностью игнорировать категорию | group и mask |
| временно выключить объект | удалить из мира или согласованно отключить simulation |

## 30 Force impulse velocity и teleport

| Операция | Единица и смысл | Когда использовать |
|---|---|---|
| `applyCentralForce` | ньютон действует в течение шага | двигатель плавучесть постоянная тяга |
| `applyCentralImpulse` | изменение импульса мгновенно | взрыв удар прыжок dynamic body |
| `setLinearVelocity` | прямая замена скорости | специальный контролируемый режим |
| `setWorldTransform` | мгновенная смена положения | teleport редактор восстановление save |

Сила должна учитывать время через интегратор Bullet. Импульс уже описывает мгновенное изменение количества движения и не умножается вручную на `deltaTime`.

## 31 Импульс взрыва построчно

`ExplosionSystem::Emit` перебирает rigid bodies, исключает static и kinematic, считает направление от центра взрыва и применяет квадратичный falloff.

```cpp
const float normalizedDistance =
    (std::min)(distance / radius, 1.0f);
const float falloff = 1.0f - normalizedDistance;
const float finalImpulse = impulseStrength * falloff * falloff;
body->activate(true);
body->applyCentralImpulse(direction * finalImpulse);
```

| Строка | Результат |
|---|---|
| `distance / radius` | перевод расстояния в диапазон области взрыва |
| `1 - normalized` | максимум в центре и ноль на границе |
| квадрат | быстро ослабляет дальнюю часть импульса |
| `activate` | будит спящее тело до применения |
| central impulse | меняет линейную скорость без точки приложения |

Дополнительный torque impulse создаёт вращение. Для физически более точной модели можно применять обычный impulse в точке `applyImpulse`, тогда плечо само создаст момент.

## 32 Масса и обратная масса

Bullet часто хранит inverse mass. Для dynamic body `invMass = 1 / mass`. Для static и kinematic inverse mass равна нулю. Поэтому проверка `getInvMass() <= 0` быстро исключает тела, которые нельзя разогнать обычным импульсом.

Одинаковый impulse сильнее ускоряет лёгкое тело, потому что изменение скорости пропорционально inverse mass.

## 33 Трение restitution и damping

| Свойство | Что регулирует | Ошибка настройки |
|---|---|---|
| friction | сопротивление скольжению в контакте | предмет едет как по льду или цепляется |
| restitution | доля упругого отскока | дребезг при слишком большом значении |
| linear damping | потеря линейной скорости без контакта | объект зависает или движется бесконечно |
| angular damping | потеря вращения | обломок никогда не перестаёт крутиться |

Damping не заменяет сопротивление среды, когда нужна зависимость от скорости, глубины и направления. В физике подлодки проект дополнительно прикладывает drag force.

## 34 Плавучесть текущего проекта

Плавучесть следует идее Архимеда.

```text
F buoyancy = fluidDensity * gravity * displacedVolume * submersionRatio
```

Вода внутри не уменьшает внешний объём корпуса. Она добавляет вес. Поэтому текущий код оставляет buoyancy volume прежним и вычитает дополнительный `waterWeight` из направленной вверх силы. Затем ограничивает экстремальное значение, добавляет drag и angular damping.

Этот раздел относится к gameplay approximation. Для точной плавучести нужны несколько sample points или погружённый объём сложной формы, центр приложения силы и torque от несовпадения центра плавучести с центром масс.

## 35 Constraints и joints

`btFixedConstraint` связывает два rigid body через два локальных frame. Frame A задан в локальном пространстве тела A, frame B в локальном пространстве тела B. Они описывают точки и ориентации, которые solver старается совместить.

В `SubmarineJointSystem::CreateJoint` сначала проверяются оба body, создаются локальные frames, затем constraint помещается в ECS компонент и только после этого регистрируется в Bullet.

```cpp
jointComp.joint = new btFixedConstraint(
    *rbA->body, *rbB->body, frameA, frameB);
jointComp.joint->setBreakingImpulseThreshold(breakForce);
jointComp.joint->setOverrideNumSolverIterations(100);
```

Порог относится к solver impulse, а не непосредственно к силе в ньютонах. Название `breakForce` удобно для gameplay, но при балансировке важно помнить фактическую величину Bullet.

## 36 Владение constraint

Проект сначала передаёт constraint в ECS компонент, потом вызывает `world->addConstraint`. Это exception safe порядок: если добавление компонента не удалось, локальный владелец может освободить ресурс, и в мире не останется не владеющий dangling pointer.

При удалении constraint сначала отсоединяется от мира, затем уничтожается. `PhysicsWorld::Shutdown` также проходит все constraints и collision objects и снимает регистрации до разрушения зависимостей мира.

## 37 Сон и активация

Bullet усыпляет спокойные dynamic bodies для экономии CPU. Сила, impulse, значимое изменение скорости или `activate(true)` будит тело. `DISABLE_DEACTIVATION` запрещает обычный сон и нужен только для объектов, которые действительно должны постоянно обновляться, например некоторых управляемых механизмов.

Слишком широкое использование `DISABLE_DEACTIVATION` повышает стоимость мира и может скрыть ошибку: объект постоянно получает микросилу или дрожит в контакте.

## 38 CCD и быстрые тела

Discrete collision detection проверяет состояния по шагам. Очень быстрый маленький объект может оказаться по разные стороны тонкой стены на соседних шагах. Это tunneling.

Для dynamic projectile можно настроить CCD motion threshold и swept sphere radius. Но текущее стрелковое оружие hitscan и использует raycast, поэтому CCD ему не нужен. CCD понадобится реальному летящему снаряду, обломку высокой скорости или торпеде с маленьким коллайдером.

## 39 Настоящий физический снаряд

Чтобы заменить hitscan летящим projectile, нужны отдельные данные и жизненный цикл.

```text
WeaponSystem
    -> создаёт projectile Entity
    -> Transform и ProjectileComponent
    -> sphere или capsule rigid body
    -> initial velocity

ProjectileSystem
    -> lifetime
    -> contact enter
    -> DamageEvent
    -> удаление body из Bullet
    -> DestroyEntity
```

Такой снаряд не должен оставлять body в мире после уничтожения Entity. Также нужен запрет попадания в стрелка на первых шагах через group mask, ignored owner или отдельный collision rule.

## 40 Потокобезопасность

Текущий Bullet world следует считать ресурсом главного потока. Фоновая задача может готовить CPU геометрию, но не должна одновременно добавлять body, менять shape или выполнять запрос мира, пока основной поток делает `stepSimulation`.

Безопасная схема асинхронной подготовки:

1. worker вычисляет чистые массивы вершин или decomposition;
2. результат помещается в main thread queue;
3. главный поток создаёт Bullet shape и регистрирует body;
4. ECS получает готовый runtime resource.

## 41 Отладочная визуализация

Для физического бага полезно одновременно видеть:

- collision shapes;
- AABB;
- contact points и normals;
- velocity vectors;
- raycast и sweep paths;
- center of mass;
- group и mask выбранного объекта;
- active state и collision flags;
- источник последней синхронизации Transform.

Один цвет должен иметь одно значение. Например зелёный query дошёл без hit, красный hit, жёлтый отфильтрованный объект, синий нормаль поверхности.

## 42 Диагностика по симптомам

| Симптом | Первая проверка | Вероятная причина |
|---|---|---|
| видимый объект не блокирует луч | зарегистрирован ли body и mask луча | нет shape body не в мире неверная группа |
| dynamic body замер | mass invMass activation state | тело static или спит |
| объект дрожит | кто пишет Transform до и после шага | два владельца или проникновение |
| коллайдер остался на старом месте | AABB и rebuild shape | scale или teleport без updateSingleAabb |
| персонаж застрял на ступени | три sweep этапа и slope normal | нет места сверху плохой offset высокий step |
| raycast попадает в стрелка | ignored object и userIndex | не тот collision object или Entity |
| constraint рвётся сразу | начальные frames и penetration | большая стартовая ошибка |
| объект проходит через стену | скорость размер fixed step CCD | tunneling или неправильный mask |
| после load физика отличается | порядок создания и runtime поля | сохранён указатель или не пересобран body |
| утечка после смены сцены | remove из world до delete | нарушен жизненный цикл |

## 43 Чек лист создания физического объекта

1. Определи источник истины Transform.
2. Выбери самый простой подходящий shape.
3. Запиши единицы размера и массы.
4. Реши static dynamic или kinematic.
5. Рассчитай inertia только для dynamic body.
6. Назначь group и mask по таблице взаимодействий.
7. Запиши Entity в userIndex.
8. Зарегистрируй body в мире только после готовности владельца.
9. Добавь debug draw формы и AABB.
10. Определи обратный порядок удаления.
11. Проверь save и повторную загрузку.
12. Проверь объект при 30, 60 и 144 кадрах рендера.

## 44 Практические задания по Bullet

### Задание 1 Луч

Нарисуй start, end, нормаль и Entity первого hit. Добавь объект между оружием и мишенью и предскажи результат до запуска.

### Задание 2 Фильтры

Составь таблицу для player, world, trigger и projectile. Для каждой пары вычисли оба битовых условия вручную.

### Задание 3 Тип тела

Создай одинаковую коробку как static, dynamic и kinematic. Запиши кто меняет её Transform и как она реагирует на impulse.

### Задание 4 Sweep

Покажи debug линии трёх этапов step up. Сделай препятствие немного ниже и немного выше порога.

### Задание 5 Constraint

Соедини два простых dynamic box, задай небольшой threshold и выведи applied impulse. Затем измени только начальное проникновение и сравни результат.

### Задание 6 Жизненный цикл

Десять раз создай и удали тестовый body. После каждого цикла проверь число collision objects мира. Оно должно возвращаться к исходному значению.

## 45 Контрольные вопросы

1. Почему render mesh не является collision shape.
2. Кто владеет Transform dynamic body после fixed step.
3. Зачем нужен broadphase и когда обновлять AABB.
4. Чем raycast отличается от convex sweep.
5. Почему trigger использует no contact response, а не пустую mask.
6. Зачем ghost pair callback character controller.
7. Чем force отличается от impulse.
8. Почему triangle mesh backing storage живёт не меньше shape.
9. Почему hitscan оружию не нужен CCD.
10. В каком порядке удалять constraint, body, motion state и shape.

Если на каждый вопрос можешь ответить через конкретный путь текущего кода, Bullet перестал быть внешней чёрной коробкой и стал понятным слоем движка.

