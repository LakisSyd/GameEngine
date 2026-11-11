# Построчный разбор систем DX Framework

Эта часть разбирает текущий снимок проекта, а не абстрактный учебный движок. Номера строк относятся к версии кода на момент сборки книги. После правок номера могут сместиться, поэтому главным ориентиром остаются имя файла, имя метода и сама строка.

## 1 Как читать систему до изменения кода

Читай каждый `Update` в пять проходов:

1. Какие сущности система перебирает.
2. Какие компоненты только читает, а какие изменяет.
3. Какие guard clauses досрочно прекращают обработку.
4. Где принимается решение и где оно исполняется.
5. Какие значения надо показать в инспекторе, чтобы увидеть причину решения.

Не пытайся сразу удержать весь файл в голове. Сначала проследи один конкретный путь: например, «NPC увидел цель, дошёл до края укрытия и сделал один выстрел».

## 2 ECSRegistry и перебор двух компонентов

Файл `src/ECS/ECSRegistry.h`, метод `ForEach<T1, T2>`.

```cpp
template<typename T1, typename T2, typename Fn>
void ForEach(Fn&& fn)
{
    auto* pool1 = FindPool<T1>();
    auto* pool2 = FindPool<T2>();
    if (!pool1 || !pool2) return;
    for (auto& [entity, comp1] : *pool1)
    {
        if (pool2->Has(entity))
        {
            auto& comp2 = *pool2->Get(entity);
            fn(entity, comp1, comp2);
        }
    }
}
```

| Строка | Что происходит | Зачем это здесь | Что сломается при ошибке |
|---|---|---|---|
| `template<typename T1, typename T2, typename Fn>` | Метод получает два типа компонентов и тип функции обратного вызова | Один код работает для любых пар компонентов | Жёстко заданные типы породят отдельный цикл на каждую систему |
| `void ForEach(Fn&& fn)` | Принимается callable без лишнего копирования | Лямбда может захватывать данные системы | Передача тяжёлой функции по значению создаст копии |
| `pool1 = FindPool<T1>()` | Ищется существующее хранилище первого типа | Перебирать можно только созданный pool | Создание pool при чтении скроет отсутствие компонентов |
| `pool2 = FindPool<T2>()` | Ищется хранилище второго типа | Оно будет проверяться для каждой сущности | Без него нельзя получить `comp2` |
| `if (!pool1 || !pool2) return` | Вся операция прекращается, если одного типа вообще нет в мире | Нет ни одного совпадения | Разыменование `nullptr` приведёт к падению |
| `for (... : *pool1)` | Перебирается только первый pool | Стоимость зависит от размера первого pool | Если первым выбран очень массовый компонент, цикл будет дороже |
| `pool2->Has(entity)` | Проверяется наличие второго компонента у той же сущности | Формируется пересечение двух множеств | Без проверки `Get` может вернуть `nullptr` |
| `comp2 = *pool2->Get(entity)` | Получается ссылка на второй компонент | Лямбда может читать и изменять его | Копия не сохранит изменения в ECS |
| `fn(entity, comp1, comp2)` | Управление передаётся системе | Registry не знает игровой логики | Неверный порядок аргументов даст путаницу типов |

### Важная деталь производительности

Этот `ForEach` всегда перебирает `pool1`. Поэтому полезно первым указывать более редкий компонент. Если `TransformComponent` есть у 10 000 сущностей, а `NPCCombatComponent` у 40, вызов `ForEach<NPCCombatComponent, TransformComponent>` выгоднее обратного порядка.

### Мини задание

Найди вызов `ForEach<FactionComponent, TransformComponent>` в выборе цели. Ответь: сколько сущностей попадёт во внешний цикл и сколько проверок будет сделано, если Faction есть у 20 сущностей, а Transform у 5000.

## 3 TransformComponent и мировая матрица

Файл `src/ECS/Components/Components.h`, метод `GetWorldMatrix`.

```cpp
XMMATRIX GetWorldMatrix() const
{
    XMMATRIX scaleMatrix = XMMatrixScaling(scale.x, scale.y, scale.z);
    XMMATRIX rotX = XMMatrixRotationX(rotation.x);
    XMMATRIX rotY = XMMatrixRotationY(rotation.y);
    XMMATRIX rotZ = XMMatrixRotationZ(rotation.z);
    XMMATRIX rotationMatrix = rotX * rotY * rotZ;
    XMMATRIX translationMatrix = XMMatrixTranslation(position.x, position.y, position.z);
    return scaleMatrix * rotationMatrix * translationMatrix;
}
```

| Строка | Что происходит | Откуда берутся данные | Проверка |
|---|---|---|---|
| `GetWorldMatrix() const` | Строится матрица из текущего Transform | `position`, `rotation`, `scale` принадлежат компоненту | Метод не должен менять компонент |
| `XMMatrixScaling` | Создаётся масштаб по трём осям | `scale` по умолчанию `(1,1,1)` | Нулевой масштаб мешает обратной матрице |
| `XMMatrixRotationX` | Поворот вокруг X | `rotation.x` должен быть в радианах | Градусы дадут неверный огромный угол |
| `XMMatrixRotationY` | Поворот вокруг вертикальной оси | Для NPC это обычно yaw | Проверь знак и направление модели |
| `XMMatrixRotationZ` | Поворот вокруг Z | Roll или наклон | Неверная ось наклонит не туда |
| `rotX * rotY * rotZ` | Три поворота соединяются | Порядок является соглашением проекта | Перестановка изменит ориентацию |
| `XMMatrixTranslation` | Создаётся перенос в мировую позицию | `position` должна быть мировой | Локальная позиция родителя потребует иерархии |
| `scale * rotation * translation` | Возвращается полный локально-мировой конвейер | DirectXMath использует выбранную проектом конвенцию | Другой порядок заставит объект вращаться вокруг иной точки |

### Как проверить без веры в формулу

Поставь объект в `(5, 0, 0)`, масштаб `(2, 1, 1)`, поворот Y на 90 градусов. Возьми локальную точку `(1, 0, 0)` и выведи её мировую позицию после `XMVector3TransformCoord`. Затем по одному отключай масштаб, поворот и перенос. Так видно вклад каждой матрицы.

## 4 NpcTacticalDecisionSystem и выбор стиля

Файл `NpcTacticalDecisionSystem.h`, метод `UpdateStyle`.

```cpp
const float dx = threatPosition.x - npcPosition.x;
const float dz = threatPosition.z - npcPosition.z;
const float threatDistance = std::sqrt(dx * dx + dz * dz);

if (tactical.hasThreatDistanceSample && deltaTime > 0.0001f)
{
    tactical.enemyClosingSpeed =
        (tactical.lastThreatDistance - threatDistance) / deltaTime;
}
else
{
    tactical.enemyClosingSpeed = 0.0f;
    tactical.hasThreatDistanceSample = true;
}
tactical.lastThreatDistance = threatDistance;
```

| Строка | Состояние до строки | Результат строки | Почему важно |
|---|---|---|---|
| `dx = threat.x - npc.x` | Есть две мировые позиции | Горизонтальная разница по X | Знак показывает сторону, квадрат убирает знак для длины |
| `dz = threat.z - npc.z` | То же по Z | Горизонтальная разница по Z | Y намеренно исключён из тактики дистанции |
| `sqrt(dx*dx + dz*dz)` | Есть катеты | Получено расстояние на плоскости | Для вывода скорости нужна настоящая длина, не квадрат |
| `hasThreatDistanceSample` | Первый кадр ещё не имеет истории | Определяется, можно ли сравнивать измерения | Без истории скорость сближения будет мусором |
| `deltaTime > 0.0001f` | Кадр мог быть паузой или нулевым шагом | Защита деления | Деление на ноль создаст бесконечность |
| `last - current` | Известны две дистанции | Положительный результат означает сближение | Обратный порядок поменяет смысл знака |
| `/ deltaTime` | Есть изменение за кадр | Получаются метры в секунду | Без деления значение зависит от FPS |
| `enemyClosingSpeed = 0` | Нет достоверной пары измерений | Безопасное начальное значение | NPC не принимает случайное решение в первый кадр |
| `hasThreatDistanceSample = true` | Первый образец принят | Следующий кадр сможет вычислить скорость | Флаг должен ставиться только после реального измерения |
| `lastThreatDistance = threatDistance` | Старое значение больше не нужно | Сохранена база для следующего кадра | Если записать раньше расчёта, скорость всегда будет нулём |

Далее система уважает ручной выбор:

```cpp
if (tactical.selectedStyle != NpcTacticalStyle::Auto)
{
    tactical.activeStyle = tactical.selectedStyle;
    tactical.tacticCommitTimer = 0.0f;
    tactical.tacticReason = "Стиль принудительно выбран в инспекторе";
    return;
}
```

| Строка | Смысл |
|---|---|
| `selectedStyle != Auto` | Дизайнер хочет детерминированный тест, а не авто выбор |
| `activeStyle = selectedStyle` | Исполняемый стиль синхронизируется с настройкой |
| `commitTimer = 0` | Для ручного режима не нужна фиксация авто решения |
| `tacticReason = ...` | Инспектор объясняет источник решения |
| `return` | Ни одно авто правило ниже не имеет права перезаписать стиль |

Затем вычисляются понятные булевы факты, а уже из них выбирается стиль. Это лучше одного гигантского `if`: каждый факт можно показать в отладчике и протестировать отдельно.

```cpp
const bool enemyIsDangerouslyClose =
    threatDistance < (std::max)(2.5f, preferredRange * 0.35f);
const bool enemyIsCloseForCautiousNpc =
    threatDistance < preferredRange * 0.75f && aggression < 0.65f;
const bool enemyIsPushing = tactical.enemyClosingSpeed > 0.75f;
const bool cautiousUnderFire =
    tactical.underFireTime > 0.0f && aggression < 0.65f;
```

Порядок последующих `if` задаёт приоритет. Опасная близость должна проверяться раньше высокой агрессии, иначе агрессивный NPC продолжит пушить даже в неудобной почти рукопашной дистанции.

## 5 NpcCoverQuery и доказательство защиты

Файл `NpcCoverQuery.h`, метод `IsProtectedFromTarget`.

```cpp
targetPosition.y += 1.0f;
coverPosition.y += (std::max)(0.2f, protectionHeight);

const XMFLOAT3 direction{
    coverPosition.x - targetPosition.x,
    coverPosition.y - targetPosition.y,
    coverPosition.z - targetPosition.z
};
const float distance = XMVectorGetX(
    XMVector3Length(XMLoadFloat3(&direction)));

if (distance <= 0.01f)
    return false;
```

| Строка | Геометрический смысл | Почему не иначе |
|---|---|---|
| `target.y += 1.0f` | Источник поднимается примерно на уровень оружия | Луч от пола ошибочно признает низкий бордюр полной защитой |
| `cover.y += protectionHeight` | Проверяется конкретная высота защищаемого тела | Одна высота не доказывает защиту головы и груди одновременно |
| `direction = cover - target` | Луч направлен от угрозы к кандидату | Обратный вектор полетит от укрытия прочь от врага |
| `Length(direction)` | Получается конечная дальность луча | Бесконечный ray может подобрать геометрию за кандидатом |
| `distance <= 0.01f` | Отклоняется вырожденная проверка | Нормализация и raycast нулевого отрезка ненадёжны |

Продолжение:

```cpp
const auto* targetController =
    registry.GetComponent<CharacterController>(targetEntity);
const btCollisionObject* targetBody =
    targetController ? targetController->ghostObj : nullptr;

PhysicsRaycastHit hit{};
PhysicsRaycast::CastRay(
    targetPosition,
    direction,
    world,
    distance,
    targetEntity,
    targetBody,
    hit);

return hit.collisionObject &&
    hit.collisionObject->isStaticOrKinematicObject();
```

| Строка | Что происходит | Возможная ошибка |
|---|---|---|
| `GetComponent<CharacterController>` | Ищется физическое представление цели | Не все сущности обязаны иметь controller |
| тернарный оператор | Безопасно выбирается `ghostObj` или `nullptr` | Разыменование отсутствующего controller уронит игру |
| `PhysicsRaycastHit hit{}` | Результат обнуляется перед вызовом | Старое попадание не должно пережить новый луч |
| `CastRay` | Проверяется только отрезок до позиции укрытия | Большая дальность даст ложную стену за точкой |
| `targetEntity`, `targetBody` | Цель исключается из собственного луча | Луч иначе столкнётся со стрелком в самом начале |
| `hit.collisionObject` | Должно существовать препятствие | Свободная линия означает отсутствие защиты |
| `isStaticOrKinematicObject()` | Защитой признаётся стабильная геометрия | Живой NPC не должен считаться стеной |

### Ограничение одной высоты

Этот метод отвечает только на вопрос про одну выбранную высоту. Для стойки нужны минимум лучи к груди и голове. Для приседа можно проверять более низкие точки. Поэтому будущая оценка укрытия должна возвращать не просто `bool`, а профиль: `standingProtected`, `crouchProtected`, `leftPeekValid`, `rightPeekValid`.

## 6 StateSystem и исполнение движения в укрытие

Файл `StateSystem.h`, ветка `NpcState::TakingCover`.

```cpp
auto* tactical = registry.GetComponent<NpcTacticalComponent>(npc);
auto* combat = registry.GetComponent<NPCCombatComponent>(npc);
if (!tactical || !combat || !combat->hasLastKnownPosition)
{
    brain.state = NpcState::Chasing;
    break;
}
```

| Строка | Кто владеет данными | Решение |
|---|---|---|
| получение `tactical` | Тактический компонент хранит цель движения и фазу укрытия | Без него состояние не может исполниться |
| получение `combat` | Боевой компонент хранит контакт | Укрытие оценивается относительно угрозы |
| guard clause | Проверяет минимальный контракт состояния | Нельзя продолжать с неполными данными |
| `state = Chasing` | Выбирается восстановительное состояние | Сейчас это политика; защитнику позже может подойти `Searching` или удержание позиции |
| `break` | Завершается только текущая ветка `switch` | `return` мог бы прекратить обработку всей сущности или системы в зависимости от контекста |

Выбор режима перемещения:

```cpp
NPCMovementComponent tacticalMovement = npcMovement;
if (tactical->movingToPeek)
{
    tacticalMovement.speed = (std::max)(
        tacticalMovement.speed,
        tactical->peekMoveSpeed);
    tacticalMovement.allowRunning = false;
}
else
{
    tacticalMovement.allowRunning = npcMovement.allowRunning;
    tacticalMovement.runWhenFartherThan = 0.0f;
}
```

| Строка | Смысл | Почему создана копия |
|---|---|---|
| `tacticalMovement = npcMovement` | Берутся обычные настройки NPC | Исходный компонент не меняется навсегда |
| `movingToPeek` | NPC идёт к точке выглядывания, а не к безопасной точке | Эти движения должны выглядеть по-разному |
| `max(speed, peekMoveSpeed)` | Выглядывание не станет медленнее обычной ходьбы | Резкая анимация требует короткого перемещения |
| `allowRunning = false` | Выглядывание не переключает locomotion в Run | Позже его заменит отдельный lean или peek clip |
| ветка `else` | NPC реально бежит к укрытию | Под огнём медленная прогулка неверна |
| `runWhenFartherThan = 0` | Бег разрешён на любой ненулевой дистанции | Обычный порог дальности здесь слишком поздний |

Навигация и прибытие:

```cpp
const NavigationResult result = NavigateTo(
    registry, npc, brain, transform, movement,
    tacticalMovement, tactical->moveTarget,
    room, npcMovement.arrivalDistance,
    NpcState::TakingCover, deltaTime);

if (result == NavigationResult::Arrived)
{
    StopMovement(movement);
    if (tactical->movingToPeek)
    {
        tactical->exposed = true;
        tactical->crouched = false;
        tactical->phaseTimer = (std::max)(
            combat->rangedAimDelay + 1.2f,
            tactical->exposeDuration);
        brain.state = NpcState::Attacking;
    }
    else
    {
        tactical->exposed = false;
        tactical->crouched = tactical->coverUsesCrouch;
        tactical->phaseTimer = (std::max)(0.1f, tactical->hideDuration);
        brain.state = NpcState::InCover;
    }
}
```

| Строка | Изменение состояния | Инвариант после строки |
|---|---|---|
| `NavigateTo` | Исполнитель пробует приблизиться к `moveTarget` | Решение о точке уже принято другой системой |
| `Arrived` | Путь закончен | Нельзя оставлять старую скорость |
| `StopMovement` | Скорость и флаг движения сбрасываются | Анимация не продолжает бег на месте |
| `movingToPeek` | Различаются край укрытия и защищённая позиция | Одна координата не должна означать обе фазы |
| `exposed = true` | Тактика знает, что NPC доступен для огня | Получение урона в этой фазе может увеличить `failedPeekCount` |
| `crouched = false` | Для текущего peek используется стойка | Будущий профиль укрытия сможет выбрать crouch-peek |
| `phaseTimer = max(...)` | Окно включает прицеливание и короткую очередь | Слишком короткий таймер вернёт NPC до выстрела |
| `state = Attacking` | Управление передаётся боевой ветке | Сам `TakingCover` не стреляет |
| защищённая ветка | NPC скрыт и ждёт | `phaseTimer` становится временем ожидания до peek |
| `state = InCover` | Управление передаётся фазе укрытия | Следующий переход должен быть явным |

Если `NavigateTo` возвращает `Blocked`, резервирование сбрасывается, таймер решения обнуляется и поиск должен получить шанс выбрать другую точку. Это пример восстановления после ошибки исполнения, а не нового тактического решения.

## 7 NpcRangedCombatSystem и точка дула

Файл `src/Gameplay/Combat/NpcRangedCombatSystem.h`.

```cpp
XMMATRIX hand;
if (!mesh->model->TryGetBoneModelTransform(
        *animation,
        weapon->rightHandBone,
        hand))
{
    combat.rangedStatus = "Missing right-hand bone";
    return;
}
```

| Строка | Что проверяется | Почему выстрел запрещается |
|---|---|---|
| `XMMATRIX hand` | Подготавливается место для матрицы кости | Дуло привязано к текущей анимационной позе |
| `TryGetBoneModelTransform` | Модель ищет кость, заданную оружием | Имена костей отличаются между моделями |
| `!result` | Кость не найдена | Вычислять мировое дуло не из чего |
| `rangedStatus` | Причина сохраняется для инспектора | Иначе NPC молча перестанет стрелять |
| `return` | Текущая попытка выстрела прекращается | Подстановка нулевой матрицы даст луч из неверного места |

Мировая точка дула:

```cpp
XMStoreFloat3(
    &origin,
    XMVector3TransformCoord(
        XMLoadFloat3(&weapon->muzzlePosition),
        WeaponIKSystem::GripMatrix(weapon->worldGrip) *
            hand *
            transform->GetWorldMatrix()));
```

Читаем изнутри наружу:

1. `muzzlePosition` — точка в локальном пространстве оружия.
2. `GripMatrix` учитывает хват и ориентацию оружия.
3. `hand` переносит оружие в текущую позу кости.
4. `GetWorldMatrix` переносит персонажа в мир уровня.
5. `TransformCoord` применяет полный конвейер к точке.
6. `XMStoreFloat3` сохраняет SIMD-вектор в обычный `XMFLOAT3`.

Направление строится только к точке, подтверждённой личным зрением:

```cpp
XMFLOAT3 aimPoint = combat.visibleAimPoint;
XMFLOAT3 direction{
    aimPoint.x - origin.x,
    aimPoint.y - origin.y,
    aimPoint.z - origin.z
};
```

`targetEntity` может оставаться активным после ухода игрока за стену. Поэтому он не является разрешением стрелять. `visibleAimPoint` должен обновляться только при текущем подтверждённом контакте.

## 8 Проверка chest to muzzle

```cpp
XMFLOAT3 chest = transform->position;
chest.y += 1.45f;

const XMFLOAT3 toMuzzle{
    origin.x - chest.x,
    origin.y - chest.y,
    origin.z - chest.z
};

const float muzzleDistance = XMVectorGetX(
    XMVector3Length(XMLoadFloat3(&toMuzzle)));
```

| Строка | Геометрия | Ошибка, которую ловит |
|---|---|---|
| `chest = position` | Берётся мировая база персонажа | Нужна стабильная точка внутри тела |
| `chest.y += 1.45` | Точка поднимается к груди | Луч от ног пересёк бы пол или низ стены |
| `origin - chest` | Направление идёт к дулу | Проверяем, не высунулся ли ствол сквозь геометрию |
| `Length` | Получается длина короткого отрезка | Raycast не должен идти дальше дула |

После raycast любое препятствие на этом отрезке ставит статус `Muzzle blocked by cover` и запрещает выстрел. Эта проверка не заменяет линию огня: она отвечает только за участок внутри позы NPC и укрытия.

## 9 Проверка линии огня и союзника

```cpp
PhysicsRaycastHit fireLane{};
PhysicsRaycast::CastRay(
    origin,
    direction,
    world,
    distance,
    npc,
    ownBody,
    fireLane);
```

| Аргумент | Точное назначение |
|---|---|
| `origin` | мировое дуло |
| `direction` | ненормализованный вектор к видимой точке; обёртка raycast знает длину отдельно |
| `world` | физический мир |
| `distance` | конец проверки находится на цели, а не за ней |
| `npc` | сущность стрелка исключается |
| `ownBody` | физический ghost стрелка исключается |
| `fireLane` | первое попадание возвращается сюда |

```cpp
const auto* blockingNpc =
    registry.GetComponent<NPCComponent>(fireLane.entity);
const auto* allyFaction =
    registry.GetComponent<FactionComponent>(fireLane.entity);

const bool friendlyInFireLane =
    blockingNpc &&
    blockingNpc->health > 0.0f &&
    allyFaction &&
    GetFactionRelation(
        observerFaction->faction,
        allyFaction->faction) == FactionRelation::Friendly;
```

Булево выражение читается слева направо благодаря short circuit:

1. Первый объект луча должен быть NPC.
2. Он должен быть жив.
3. У него должна быть фракция.
4. Отношение фракций должно быть Friendly.

Только после успешной проверки указателя читаются его поля. Переставлять `blockingNpc->health` перед `blockingNpc` нельзя.

## 10 WeaponSystem и одна попытка выстрела

Файл `src/Gameplay/Combat/WeaponSystem.h`, метод `TryFire`.

```cpp
if (!world || !registry.IsAlive(shooterEntity) ||
    !registry.HasComponent<ActiveComponent>(shooterEntity) ||
    !WeaponFiringRules::ValidRay(origin, direction))
    return false;
```

| Проверка | Что гарантирует следующему коду |
|---|---|
| `world` | физический raycast можно вызвать |
| `IsAlive` | entity не уничтожен |
| `ActiveComponent` | выключенная сущность не стреляет |
| `ValidRay` | координаты конечны и направление ненулевое |

Поиск оружия и перезарядка:

```cpp
const Entity weaponEntity =
    GetEquippedWeaponEntity(registry, shooterEntity);
auto* weapon = registry.GetComponent<WeaponComponent>(weaponEntity);
if (!weapon)
    return false;

if (weapon->ammoInMagazine <= 0)
{
    WeaponFiringRules::StartReload(*weapon);
    return false;
}

if (!WeaponFiringRules::CanFire(*weapon))
    return false;
```

| Строка | Состояние оружия | Результат |
|---|---|---|
| `GetEquippedWeaponEntity` | Стрелок может иметь инвентарь и несколько предметов | Находится именно экипированное оружие |
| `GetComponent<WeaponComponent>` | Entity мог быть неверным или предмет не оружие | Получается механическое состояние |
| `ammo <= 0` | Магазин пуст | Запускается таймер перезарядки |
| `return false` | В этот кадр пуля не создаётся | Перезарядка не считается выстрелом |
| `CanFire` | Проверяет кулдаун, reload и другие правила | Все стрелки используют один контракт |

Разброс и фиксация выстрела:

```cpp
XMStoreFloat3(&normalized,
    XMVector3Normalize(XMLoadFloat3(&direction)));

const auto* movement =
    registry.GetComponent<CharacterMovement>(shooterEntity);
const float motionScale = WeaponFiringRules::MotionMultiplier(
    *weapon,
    movement && movement->isMoving);

const float radial = sample(random);
const float angular = sample(random);
const XMFLOAT3 muzzleDirection = normalized;
normalized = WeaponFiringRules::ConeDirection(
    normalized,
    weapon->spreadDegrees * motionScale +
        weapon->currentRecoilDegrees,
    radial,
    angular);

WeaponFiringRules::CommitShot(*weapon, motionScale);
```

| Строка | Почему расположена именно здесь |
|---|---|
| нормализация | Дальность и конус ожидают единичное направление |
| чтение движения | Разброс зависит от состояния стрелка |
| `movement && ...` | Отсутствующий компонент безопасно трактуется как неподвижность |
| две случайные выборки | Одна задаёт радиус, другая угол внутри конуса |
| сохранение `muzzleDirection` | Muzzle flash должен смотреть по стволу, а не по случайному отклонению пули |
| `ConeDirection` | Получается фактический луч этого выстрела |
| `CommitShot` | Списывается патрон, ставится кулдаун и отдача | 

Сначала фиксируется выстрел, затем делается raycast. Поэтому промах всё равно тратит патрон и создаёт кулдаун — это правильная механика.

Попадание превращается в событие:

```cpp
PhysicsRaycastHit rayHit{};
const bool hit = PhysicsRaycast::CastRay(
    origin,
    normalized,
    world,
    weapon->range,
    shooterEntity,
    shooterObject,
    rayHit);

if (!hit)
    return true;

DamageEvent damageEvent{};
damageEvent.source = weaponEntity;
damageEvent.target = rayHit.entity;
damageEvent.damageAmount = weapon->damage;
damageSystem.AddDamageEvent(damageEvent);
return true;
```

`true` означает «выстрел состоялся», а не «цель получила урон». Это различие нужно для анимации, звука, расхода патронов и тактического поведения.

## 11 Один кадр боя от восприятия до урона

Полезно читать не отдельные классы, а движение данных:

1. Система восприятия подтверждает личную видимость и обновляет `visibleAimPoint`.
2. Боевая система хранит `targetEntity`, но не считает его вечной видимостью.
3. Тактическое решение выбирает стиль и запрос укрытия.
4. Cover query оценивает кандидатов лучами относительно подтверждённой угрозы.
5. `StateSystem` исполняет `moveTarget` через навигацию.
6. В состоянии атаки `NpcRangedCombatSystem` вычисляет мировое дуло.
7. Проверяются поворот, chest-to-muzzle и дружественная линия огня.
8. `WeaponSystem::TryFire` проверяет оружие, создаёт разброс и физический луч.
9. Первое попадание превращается в `DamageEvent`.
10. `DamageSystem` применяет урон и записывает факт угрозы для следующего тактического решения.

Если NPC не стреляет, проверяй цепочку сверху вниз. Не начинай сразу с `TryFire`: до него мог не дойти вызов.

## 12 Метод локализации бага

Для каждого перехода храни три строки отладки:

- `decision`: чего NPC хотел;
- `reason`: почему он этого захотел;
- `execution`: что исполнитель реально сделал.

Пример:

```text
decision: перейти к укрытию 41
reason: два попадания во время выглядывания
execution: путь не найден, cover 41 отклонено на 4 секунды
```

Это различает ошибку решения и ошибку исполнения. Если решение правильное, но NPC стоит, смотри NavMesh и состояние движения. Если путь есть, но выбрана плохая сторона стены, смотри геометрию cover query и score.

## 13 Практика по чтению кода

### Задание 1 ECS

Нарисуй два множества сущностей: с `NPCCombatComponent` и с `TransformComponent`. Руками отметь пересечение, которое получит лямбда.

### Задание 2 матрица дула

В отладке покажи четыре точки разными цветами: локальное дуло после grip, после hand, после world и окончательный `origin`. Если одна стадия прыгает, ты сразу узнаешь виноватую матрицу.

### Задание 3 укрытие

Для одного кандидата нарисуй лучи к ногам, груди и голове. В журнале напиши, какой луч встретил статическую геометрию первым.

### Задание 4 выстрел

Поставь союзника между двумя врагами. Запиши по кадрам `fireLane.entity`, отношение фракций и `rangedStatus`. Затем убери союзника и сравни цепочку.

### Задание 5 объяснение

Не глядя в книгу объясни вслух, почему активный `targetEntity` не даёт права стрелять. Правильный ответ должен упомянуть текущую личную видимость, укрытие и первое физическое попадание луча.

## 14 Контрольный список

После разбора ты должен уметь:

- отличить выбор сущностей ECS от игровой логики лямбды;
- назвать пространство каждой точки в расчёте дула;
- объяснить, почему `lastKnownPosition` не является всезнанием;
- разделить решение об укрытии, поиск геометрии и исполнение движения;
- прочитать guard clause как контракт следующего участка;
- отличить «выстрел состоялся» от «луч попал»;
- найти, какая система владеет изменяемым полем;
- построить цепочку данных одного кадра;
- добавить отладку причины, а не только результата.
