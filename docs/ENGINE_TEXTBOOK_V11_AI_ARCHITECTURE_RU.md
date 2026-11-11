# Архитектура сложного игрового AI без конфликтов систем

Хороший AI — не тот, у которого больше условий. Хороший AI знает только доступные персонажу факты, принимает устойчивое объяснимое решение, резервирует нужные ресурсы, выполняет действие через штатные системы и умеет признать неудачу. Эта глава проектирует следующий слой поверх уже существующих компонентов DX Framework.

## 1 Почему системы начинают конфликтовать

Конфликт появляется, когда несколько систем считают себя владельцами одного результата.

Пример плохого кадра:

- perception увидел противника и записал `Attacking`;
- cover system записал `TakingCover`;
- weapon search решил идти к винтовке и записал `Chasing`;
- damage reaction повернул transform;
- animation controller увидел скорость и включил run;
- squad system в конце кадра снова выдал flank target.

Каждая локальная причина выглядит разумно, но общего решения нет. Последняя система в порядке кадра случайно побеждает.

Исправление — не «правильнее отсортировать все if», а разделить полномочия.

## 2 Шесть слоёв AI

Устойчивый pipeline:

```text
World truth
   ↓ sensing
Perception memory
   ↓ interpretation
Situation and needs
   ↓ arbitration
Chosen goal and intent
   ↓ planning
Action sequence
   ↓ execution
Movement Weapon Interaction Animation
   ↓ feedback
Success Failure Blocked Interrupted
```

### 2.1 World truth

Registry, Transform, physics world, health и inventory содержат реальность симуляции. NPC не должен автоматически читать всё это как знание.

### 2.2 Perception memory

Сюда попадают только подтверждённые наблюдения: видимая цель, услышанный выстрел, направление близкого попадания, сообщение союзника с уровнем доверия.

### 2.3 Situation

Интерпретация отвечает: угроза близко, патроны заканчиваются, NPC окружён, союзник ранен, выход заблокирован. Это производные факты, а не действие.

### 2.4 Goal и intent

Goal — желаемое состояние: выжить, получить оружие, удержать дистанцию, обезвредить противника. Intent — выбранное направление на ближайший период с причиной и обязательством.

### 2.5 Plan и action

Plan раскладывает цель на действия: найти оружие → зарезервировать → добежать → поднять → экипировать → вернуться в бой. Executor выполняет только текущее действие.

### 2.6 Feedback

Исполнитель не переписывает глобальную цель. Он сообщает `Succeeded`, `Failed`, `Blocked`, `Interrupted` и подробную причину. Planner или arbiter решает, что дальше.

## 3 Владение данными

Ниже рекомендуемая граница.

| Данные | Единственный писатель | Читатели |
|---|---|---|
| Видимые контакты | PerceptionSystem | planner, combat, Studio |
| Последняя известная позиция | ContactMemorySystem | search, tactics |
| Выбранная стратегическая цель | GoalArbiter | planner, Studio |
| Текущий action | ActionPlanner | executors, animation |
| Путь и steering | Navigation executor | movement, Studio |
| Trigger и reload request | Weapon executor | WeaponSystem |
| Фаза оружия и ammo | WeaponSystem | AI, UI, animation |
| Animation action request | Action/animation bridge | AnimationController |
| Transform персонажа | movement/physics | все остальные |
| Tactical cover reservation | ReservationSystem | cover query, planner |
| Причина решения | модуль, создавший intent | Studio |

Слово «единственный» относится к смысловому полю. Разные системы могут изменять разные части одного компонента, но по мере роста лучше разделять компоненты по владельцам.

## 4 Истина мира не равна знанию NPC

В текущем боевом коде уже заложено правильное правило: `targetEntity` может оставаться активным, но стрелять можно только при личной проверке `targetVisible`, а поиск использует `lastKnownPosition`.

Нужны три разных понятия:

- Entity identity: кого NPC считает вероятной целью.
- Confirmed observation: где цель действительно видна сейчас.
- Belief: где цель могла оказаться после потери контакта.

Belief не обновляется скрытым `Transform` игрока. Иначе NPC постоянно знает, где он.

### 4.1 Контакт от союзника

Сообщение напарника должно создавать `reported contact`, а не `targetVisible = true`. Получатель знает район и время сообщения, но обязан сам подтвердить цель перед точным огнём.

Полезные поля контакта:

```cpp
struct ContactMemory
{
    Entity suspectedEntity = INVALID_ENTITY;
    DirectX::XMFLOAT3 lastPosition{};
    float confidence = 0.0f;
    float age = 0.0f;
    bool personallyConfirmed = false;
    ContactSource source = ContactSource::Sight;
};
```

Confidence убывает со временем. Оно влияет на решение проверить сектор, но не превращает догадку в линию прицеливания.

## 5 FSM Utility Behavior Tree и GOAP

Это не конкурирующие религии. Они отвечают на разные уровни.

### 5.1 FSM

Finite State Machine удобна для фазы исполнения: `Moving`, `Aiming`, `Reloading`, `Interacting`. Она плоха как единственный мозг, когда переходы между десятками целей образуют паутину.

### 5.2 Utility AI

Utility оценивает несколько целей числами. Хорошо подходит для выбора «бежать, искать оружие, продолжать бой, сдаться». Нужны нормализация, hard constraints, hysteresis и объяснение score.

### 5.3 Behavior Tree

Дерево удобно описывает реактивную последовательность с fallback. Без дисциплины blackboard и владения оно тоже может конфликтовать с другими системами.

### 5.4 GOAP

GOAP строит цепочку действий к желаемому состоянию на основе preconditions и effects. Полезен для редких составных целей, но избыточен для каждого короткого peek.

### 5.5 Рекомендация для DX Framework

Гибрид:

- Utility Goal Arbiter выбирает высокоуровневую цель.
- Небольшой planner или явная action sequence строит план.
- FSM каждого executor выполняет действие.
- Чистые tactical rules вычисляют локальные разрешения.
- Animation controller отображает semantic action в клипы.

Так сохраняется нынешняя понятная структура и появляется место для новых целей.

## 6 Blackboard без глобальной свалки

Blackboard полезен как структурированный снимок данных для решения. Он не должен быть `unordered_map<string, any>` без владельцев.

```cpp
struct NpcDecisionContext
{
    Entity self = INVALID_ENTITY;
    float healthFraction = 1.0f;
    int ammoInMagazine = 0;
    int reserveAmmo = 0;
    bool hasWeapon = false;
    bool targetVisible = false;
    bool underFire = false;
    float targetDistance = 0.0f;
    float nearestWeaponPathCost = 0.0f;
    bool canSurrender = false;
};
```

Context строится один раз для цикла решения и передаётся как `const&`. Модули не ходят сами по Registry и не получают тайное знание. Результат становится тестируемым.

## 7 Goal Arbiter

Arbiter рассматривает кандидатов и выбирает один intent.

```cpp
enum class NpcGoal
{
    None,
    Survive,
    Fight,
    KeepDistance,
    AcquireWeapon,
    SearchContact,
    HelpAlly,
    CaptureEnemy,
    Surrender,
    Escape
};

struct GoalCandidate
{
    NpcGoal goal = NpcGoal::None;
    float score = 0.0f;
    bool allowed = false;
    const char* reason = "";
};
```

### 7.1 Сначала запреты потом score

Нельзя компенсировать запрет огромным score. Если нет пути к оружию, `AcquireWeapon` не allowed. Если фракция запрещает плен, `CaptureEnemy` отсутствует. Если NPC не видел сдачу, он не знает, что противник готов.

Порядок:

1. hard constraints;
2. emergency overrides;
3. score допустимых целей;
4. hysteresis и commitment;
5. выбор;
6. причина решения.

### 7.2 Нормализация score

Каждый фактор приводится к `[0,1]`. Иначе расстояние в метрах случайно победит health fraction.

Пример функции срочности оружия:

```text
unarmed = 1 если оружия нет, иначе 0
reachable = 1 - saturate(pathCost / maxUsefulCost)
dangerPenalty = saturate(exposureRisk)
weaponScore = unarmed * reachable * (1 - dangerPenalty)
```

Формула не обязана быть «умной». Она должна давать предсказуемые крайние случаи.

## 8 Устойчивость решения

Если два score близки, выбор может меняться каждый кадр. Используются четыре механизма.

### 8.1 Commitment

После выбора цель удерживается минимальное время, если не произошло критическое событие. Текущий `tacticCommitTimer` уже решает похожую задачу для стиля.

### 8.2 Hysteresis

Новая цель должна превысить текущую не на 0.001, а на margin:

```text
switch if newScore > currentScore + 0.15
```

### 8.3 Cooldown

После отказа конкретному ресурсу даётся cooldown. `rejectedCoverTimer` — существующий пример.

### 8.4 Interrupt policy

Каждый action сообщает, чем его можно прервать.

- Reload можно прервать критической опасностью, но не слабым изменением score.
- Vault нельзя прервать в середине физического перехода.
- Aim можно прервать потерей видимости.
- Surrender может быть необратим после отброшенного оружия.

## 9 Тактические модули как пазл

Модуль не должен сам управлять NPC. Он предлагает intent.

```cpp
struct TacticalProposal
{
    TacticalAction action;
    float score;
    DirectX::XMFLOAT3 targetPosition{};
    Entity targetEntity = INVALID_ENTITY;
    float commitment = 0.0f;
    const char* reason = "";
};

class ITacticalModule
{
public:
    virtual void Collect(
        const NpcDecisionContext& context,
        std::vector<TacticalProposal>& output) const = 0;
};
```

Примеры модулей:

- HoldCover;
- PeekAndFire;
- RetreatToRange;
- ChangeAngle;
- Flank;
- Suppress;
- CloseDistance;
- AcquireWeapon;
- Surrender;
- ArrestSurrenderedEnemy.

Arbiter выбирает одну заявку. Только executor меняет движение и trigger.

## 10 Как встроить текущие боевые системы

Не нужно выбрасывать работающий код.

- `NpcCombatSystem` остаётся perception/target-selection слоем и поддерживает confirmed visibility и память.
- `NpcTacticalDecisionSystem` остаётся чистым высокоуровневым правилом стиля и запроса укрытия.
- `NpcCoverQuery` остаётся сервисом поиска и оценки кандидатов.
- `NpcCoverSystem` постепенно становится executor жизненного цикла cover/peek.
- `NpcRangedCombatSystem` остаётся weapon executor: aim, muzzle, fire lane, ammo, reload.
- `NpcNavMeshSystem` остаётся навигационным сервисом и исполнителем пути.
- `NpcSquadCombatSystem` не должен напрямую перезаписывать индивидуальный intent; позже он публикует role/order proposal.

Первый рефакторинг — не перенос кода, а запись таблицы «кто имеет право менять поле» и устранение двойных писателей.

## 11 Бегство как цель Survive

NPC не должен убегать только по условию `health < 20`. Решение зависит от возможности и цены.

Факторы:

- health fraction;
- recent damage rate;
- число видимых угроз;
- наличие укрытия;
- дистанция до безопасной зоны;
- боезапас;
- наличие союзников;
- morale;
- приказ удерживать позицию;
- возможность сдачи.

### 11.1 Score бегства

Пример:

```text
healthNeed = 1 - healthFraction
damageNeed = saturate(recentDamagePerSecond / dangerousDps)
outnumbered = saturate((enemyCount - allyCount) / 3)
escapeChance = 1 - pathRisk

escapeScore =
    0.35 * healthNeed +
    0.30 * damageNeed +
    0.20 * outnumbered +
    0.15 * escapeChance
```

Это начало настройки, не универсальная истина.

### 11.2 Safe destination

«Бежать от врага» не равно «бежать в точку по обратному вектору». Точка должна:

- находиться на NavMesh;
- увеличивать дистанцию или разрывать line of sight;
- иметь путь с приемлемым риском;
- не вести в тупик рядом с другой угрозой;
- не быть занята союзником, если узкая;
- иметь следующую точку выхода.

Можно оценивать кандидаты по кольцам NavMesh или tactical graph. Cover является одним фактором, но не единственной целью.

### 11.3 Отступление и паника

Тактическое отступление сохраняет оружие и может вести огонь короткими очередями. Паника снижает точность, игнорирует строй и выбирает ближайший очевидный выход. Это разные action profiles, даже если оба увеличивают дистанцию.

## 12 Поиск оружия

Текущий `InventorySystem` уже предоставляет надёжную операцию pickup, владение и слоты. AI-слою не нужно копировать оружие. Ему надо выбрать доступную сущность и выполнить обычное взаимодействие.

### 12.1 Когда искать

Кандидат `AcquireWeapon` допустим, если:

- NPC способен использовать оружие;
- активного подходящего оружия нет или оно бесполезно;
- существует известный pickup;
- pickup не принадлежит другому;
- есть свободный слот или разрешена замена;
- путь достижим;
- риск не превышает полезность.

Не надо искать Transform всех weapons через стены как знание. Источник знания: увиденный pickup, известный шкаф, команда, память об уроненном оружии.

### 12.2 Оценка оружия

```text
weaponUtility =
    combatPower * ammoFactor * roleFit * condition -
    pathCost * distanceWeight -
    exposureRisk -
    reservationPenalty
```

Combat power учитывает damage, fire rate, range и тип цели. Role fit не должен быть жёстким запретом: оборонщик предпочитает дальнее оружие, но возьмёт пистолет вместо пустых рук.

### 12.3 Резервация

Два NPC не должны одновременно бежать к одной винтовке. Нужен общий сервис:

```cpp
struct Reservation
{
    Entity resource = INVALID_ENTITY;
    Entity owner = INVALID_ENTITY;
    ReservationKind kind{};
    float expiresAt = 0.0f;
    std::uint32_t revision = 0;
};
```

Резервация имеет TTL и освобождается при успехе, отказе, смерти владельца или смене цели. Она не является владением предметом: `WeaponOwnerComponent` появляется только после pickup.

### 12.4 План

```text
ReserveWeapon
→ NavigateToPickup
→ FacePickup
→ PlayPickupAction
→ InventorySystem::PickupWeapon
→ ReleaseReservation
→ Equip
```

Если оружие исчезло на третьем шаге, action возвращает `ResourceGone`. Planner не продолжает анимацию поднятия пустого места.

## 13 Боезапас и решение

Ammo принадлежит `WeaponComponent`. AI читает:

- ammo in magazine;
- reserve ammo;
- is reloading;
- reload timer;
- эффективную дальность.

Пустой магазин при наличии резерва создаёт `Reload` action. Нулевой общий запас может повысить `AcquireWeapon`, `TakeAmmo`, `Retreat` или `Surrender`. Он не должен заставлять `NpcRangedCombatSystem` притворяться, что оружия нет: executor сообщает состояние, arbiter выбирает цель.

## 14 Сдача

Сдача — изменение социального и боевого состояния, а не просто анимация рук вверх.

Условия:

- мораль ниже порога;
- противник демонстрирует превосходство;
- NPC понимает требование сдаться;
- есть право на сдачу по фракции/характеру;
- нет активной невыполнимой клятвы или scripted override;
- NPC не находится в физически непрерываемом действии.

Последствия:

- прекращается огонь;
- оружие блокируется или отбрасывается;
- faction relation для targeting меняется на `Surrendered`, а не обязательно Neutral;
- включается surrender animation action;
- появляется interaction для ареста;
- союзники получают наблюдаемое событие, но не телепатическую истину.

Нужен отдельный компонент состояния:

```cpp
enum class ComplianceState
{
    None,
    ConsideringSurrender,
    Surrendering,
    Surrendered,
    Restrained,
    EscapingCustody
};
```

Не стоит кодировать это через `health = 0` или `isHostile = false`: потеряется смысл и появятся невозможные комбинации.

## 15 Взять в плен или убить

Это решение разделено между правилами мира и локальной тактикой.

### 15.1 Rules of engagement

Фракция или миссия задаёт:

- разрешена ли летальная сила;
- надо ли предлагать сдачу;
- какие цели должны быть захвачены живыми;
- считается ли сдающийся защищённым;
- можно ли стрелять по бегущему пленнику;
- кто имеет полномочие ареста.

AI не должен выводить моральные правила только из score.

### 15.2 Tactical feasibility

Даже при приказе захватить живым NPC оценивает:

- действительно ли цель сдалась;
- есть ли прикрытие во время подхода;
- не держит ли цель скрытое оружие;
- есть ли свободный союзник для overwatch;
- достижим ли interaction range;
- не появилась ли новая смертельная угроза.

### 15.3 Capture plan

```text
ConfirmSurrender
→ ReservePrisonerInteraction
→ AssignCoveringActor optional
→ ApproachFromSafeArc
→ OrderDropWeapon
→ WaitForCompliance
→ RestrainInteraction
→ MarkRestrained
→ EscortToDestination
```

Если цель снова берёт оружие, compliance state меняется, interaction прерывается, а rules of engagement заново определяет допустимую силу.

## 16 Нелетальная сила

«Не убивать» не означает стрелять обычным оружием до 1 HP. Нужны отдельные damage effects:

- stun;
- suppression;
- pain/compliance;
- stamina loss;
- incapacitation;
- restraint.

Damage pipeline может передавать tags и magnitude. Health остаётся физическим повреждением, а consciousness/morale — отдельными ресурсами. Тогда электрошокер, удар, газ и команда сдаться не конкурируют за одно число.

## 17 Stamina

Stamina полезна, если она создаёт решения, а не просто полоску.

Расход:

- sprint;
- melee attack;
- удержание тяжёлого оружия;
- быстрый peek/lean;
- vault;
- сопротивление захвату.

Восстановление зависит от движения, posture, suppression и injuries.

AI не должен ждать полного нуля. Utility учитывает прогноз стоимости плана:

```text
canSprintToCover = stamina >= pathSprintCost + safetyReserve
```

Владельцем stamina является character resource system. Movement executor запрашивает режим, resource system списывает фактический расход, planner читает снимок.

## 18 Morale

Morale медленнее краткого fear. На него влияют:

- лидер рядом;
- численное преимущество;
- смерть союзника;
- попадания и suppression;
- отсутствие боезапаса;
- изоляция;
- успех собственных действий;
- характер и обучение.

Morale не должен мгновенно прыгать. Используются накопление, decay и thresholds с hysteresis. Он влияет на score, но не напрямую записывает state.

## 19 Укрытие и выбор стороны

Текущий `NpcCoverQuery` уже проверяет protection, peek sight и path. Для укрытия с двумя сторонами одна Entity не обязана иметь один peek. Лучше представить cover как позицию защиты плюс набор firing slots:

```cpp
struct CoverSlot
{
    DirectX::XMFLOAT3 peekPosition{};
    PeekSide side = PeekSide::Left;
    Stance stance = Stance::Stand;
    float exposure = 0.0f;
};
```

Каждый slot оценивается отдельно. Low cover может иметь crouch hide и stand peek. Высокая стена — left/right lean. Один slot резервируется конкретным NPC.

Важно проверять защиту несколькими лучами к телу: голова, грудь, таз. Один луч, упёршийся в низкую стену, не доказывает, что стоящий NPC защищён.

## 20 Навигация и tactical cost

Кратчайший путь не всегда лучший в бою. Ребро NavMesh может иметь дополнительные стоимости:

- exposure to known enemy;
- noise;
- narrow doorway;
- fire/hazard;
- crowd density;
- distance from desired combat range.

Полная динамическая стоимость каждого polygon может быть дорогой. Начать можно с оценки нескольких готовых путей по sampled points. Planner выбирает цель, NavMesh строит путь, tactical evaluator измеряет риск, затем при необходимости проверяется следующий кандидат.

## 21 Движение и стрельба

Текущие правила правильно разрешают moving fire только для боевого движения и при `targetVisible`. Executor должен дополнительно учитывать:

- animation/posture supports firing;
- muzzle не перекрыто;
- fire lane не содержит союзника;
- weapon не reloading;
- spread допустим;
- burst budget и pause;
- действие не требует обеих рук.

Стрельба является параллельным каналом только там, где контракты совместимы. Нельзя стрелять во время pickup, surrender или vault. Вместо огромной таблицы `if` action может публиковать capability mask:

```cpp
enum ActionCapability
{
    CanAim = 1 << 0,
    CanFire = 1 << 1,
    CanSprint = 1 << 2,
    CanInteract = 1 << 3
};
```

Weapon executor читает разрешение текущего action.

## 22 Параллельные каналы действий

Один NPC может одновременно идти, смотреть и говорить. Но некоторые действия конфликтуют по телу и ресурсам.

Удобные каналы:

- locomotion;
- facing/aim;
- hands/weapon;
- interaction;
- speech;
- upper-body animation.

Action объявляет, какие каналы захватывает. Reload захватывает hands/weapon, но может разрешить walk. Vault захватывает locomotion и body animation. Surrender захватывает weapon и upper body.

Арбитр каналов предотвращает случай, когда reload и pickup одновременно управляют руками.

## 23 Групповая работа как отдельный слой

Индивидуальный AI должен работать без squad. Group coordinator читает публичные возможности и предлагает:

- role assignment;
- shared contact report;
- reservation of sectors;
- suppress/flank timing;
- prisoner overwatch.

Он не должен напрямую вести каждого по точкам каждый кадр. Приказ группы становится ограничением или proposal индивидуального arbiter. Боец может отклонить его по аварийной причине и записать отказ.

## 24 Объяснимость решения

Для каждого выбранного intent надо сохранить:

- имя цели;
- winning score;
- предыдущую цель и score;
- hard constraints;
- три главных фактора;
- commitment remaining;
- interrupt cause;
- plan step;
- последний failure.

Пример журнала:

```text
Goal Fight → AcquireWeapon
score 0.82 vs 0.34
reason: unarmed 1.00, weapon reachable 0.91, exposure risk 0.18
reserved entity 42 for 4.0 s
```

Это полезнее строки «Moving to target».

## 25 Ошибки как данные

Action failure должен иметь тип:

```cpp
enum class ActionFailure
{
    None,
    TargetGone,
    NoPath,
    PathBlocked,
    ReservationLost,
    InteractionRejected,
    WeaponUnavailable,
    InterruptedByDanger,
    AnimationMissing,
    Timeout
};
```

Planner может реагировать по-разному. `NoPath` запрещает кандидата на некоторое время. `ReservationLost` запускает поиск другого предмета. `AnimationMissing` обычно не отменяет gameplay и использует fallback.

## 26 Частота обновления

Не все слои работают каждый кадр.

- sensing raycasts: распределённо или по budget;
- target visibility для текущей цели: чаще;
- emergency reaction: каждый simulation tick;
- goal arbitration: 2–5 раз в секунду или по событию;
- plan rebuild: при смене цели или failure;
- steering и weapon execution: каждый tick;
- Studio capture: после штатных систем.

Событие будит редкий decision loop, но всё равно проходит arbiter. Так критический урон реагирует быстро, не превращая полный planner в 60 запусков в секунду.

## 27 Детерминизм и случайность

Случайность нужна для вариативности, но должна воспроизводиться. У каждого NPC или сценария хранится seed. Решение записывает выбранное случайное значение или индекс варианта.

Не использовать глобальный `rand()` из разных систем: порядок вызовов изменится после любого рефакторинга. Лучше отдельный RNG stream по назначению: tactics, animation variety, audio.

## 28 Сохранение игры

Не все runtime поля стоит сохранять.

Сохраняются:

- долгосрочная цель, если она важна;
- inventory и ammo;
- health, stamina, morale;
- compliance state;
- память контактов с timestamp относительно мира;
- selected style и характер;
- активный action только если он умеет безопасно восстановиться.

Не сохраняются:

- сырые указатели;
- временный путь NavMesh, если его можно перестроить;
- UI selection Studio;
- debug rays;
- pointer на reservation без revision и validation.

После загрузки выполняется revalidation: сущность ещё существует, путь строится, ресурс свободен.

## 29 Тестирование AI

### 29.1 Чистые unit tests

Тестировать utility и rules без мира:

- нулевое здоровье;
- нет оружия и один reachable pickup;
- pickup занят;
- surrender forbidden;
- два score на границе hysteresis;
- commitment и emergency interrupt.

### 29.2 Scenario tests

Маленькая сцена с фиксированным seed и входными событиями. Проверяется не точная позиция каждого кадра, а инварианты:

- NPC не стреляет без visibility;
- два NPC не владеют одной reservation;
- surrendered NPC не нажимает trigger;
- planner не перестраивается каждый кадр;
- отсутствующий clip не отменяет arrest.

### 29.3 Soak test

Десятки минут ускоренной симуляции ищут deadlock, утечки reservations, бесконечные планы и рост журнала.

## 30 Пошаговый план внедрения без большого взрыва

### Этап 1 Карта владельцев

Записать все поля `NpcBrainComponent`, `NPCCombatComponent`, `NpcTacticalComponent` и системы-писатели. Найти поля с двумя писателями.

### Этап 2 Intent рядом с текущим state

Добавить небольшой `NpcIntentComponent` с goal, action, reason, revision и commitment. Сначала только отображать в Studio, не менять поведение.

### Этап 3 Один модуль

Перенести решение `AcquireWeapon` или `RetreatToRange` в чистый proposal. Старый combat path остаётся fallback.

### Этап 4 Executor и feedback

Навигация к оружию возвращает typed result. Только после успеха вызывается `InventorySystem::PickupWeapon`.

### Этап 5 Reservation service

Один общий сервис для weapon, cover slot и prisoner interaction.

### Этап 6 Goal Arbiter

Подключить Fight, AcquireWeapon и Escape. Ввести hysteresis и журнал score.

### Этап 7 Compliance

Добавить surrender/capture после того, как action ownership уже работает. Иначе социальные состояния умножат существующие конфликты.

### Этап 8 Squad proposals

Вернуть командную работу поверх индивидуальных возможностей.

## 31 Пример полного решения

Ситуация: Sirius NPC имеет 25% здоровья, магазин пуст, резерв пуст, видит вооружённого Iridian на 9 м и знает о винтовке за стеной в 6 м.

1. Perception подтверждает Iridian и последнюю позицию.
2. Inventory snapshot сообщает отсутствие боезапаса.
3. Weapon knowledge содержит pickup и свежесть наблюдения.
4. Goal candidates:
   - Fight allowed, score 0.18;
   - AcquireWeapon allowed, score 0.62;
   - Escape allowed, score 0.78;
   - Surrender allowed, score 0.55.
5. Escape побеждает и получает commitment 3 с.
6. Safe-position query находит путь, разрывающий line of sight.
7. Executor резервирует destination slot и запускает sprint.
8. Weapon channel запрещает fire, потому что ammo 0.
9. При достижении безопасности score пересчитываются. AcquireWeapon теперь 0.81 из-за низкого риска.
10. Резервируется винтовка, строится plan pickup.
11. NPC поднимает реальную entity через `InventorySystem`.
12. Studio показывает обе смены целей и факторы.

Ни одна система не записывает всё поведение. Каждая предоставляет часть доказательства.

## 32 Практические задания

### Задание 1 Владельцы

Для текущих `targetEntity`, `targetVisible`, `moveTarget`, `triggerHeld` и `currentState` выпиши всех писателей. Если писателей несколько, реши, кто должен стать владельцем, а кто — отправлять запрос.

### Задание 2 Acquire weapon

На бумаге создай три оружия: близкое без патронов, далёкое хорошее, среднее занятое союзником. Рассчитай простые score и объясни reservation.

### Задание 3 Escape

Придумай пять кандидатов NavMesh. Для каждого оцени расстояние, разрыв видимости, риск и тупик. Выбери не обязательно ближайший.

### Задание 4 Capture

Нарисуй state diagram `Armed → Considering → Surrendering → Surrendered → Restrained`. Добавь прерывание «снова взял оружие» и «арестующий получил новую угрозу».

### Задание 5 Конфликт каналов

Составь таблицу для Reload, Sprint, Peek, Pickup, Surrender и Arrest. Отметь locomotion, aim, hands, interaction и animation. Найди действия, которые могут выполняться параллельно.

### Задание 6 Studio

Спроектируй вкладку Decision: goal scores, current commitment, plan steps, reservations и last failure. Каждый текст должен отвечать на вопрос, а не повторять имя state.

## 33 Критерий хорошего AI

AI можно считать архитектурно здоровым, если:

- скрытая цель не передвигает память NPC;
- одна система владеет одним смысловым выходом;
- решение отделено от исполнения;
- действие возвращает typed feedback;
- ресурсы резервируются и освобождаются;
- новый модуль предлагает intent, а не переписывает пять компонентов;
- score устойчивы commitment и hysteresis;
- анимация отображает действие, но не определяет игровую истину;
- Studio объясняет выбор и не подменяет pipeline;
- любой сложный сценарий можно воспроизвести фиксированным seed и журналом.

Тогда «лучший AI» означает не всезнающего и идеально стреляющего противника. Это персонаж, который знает ограниченную информацию, делает понятные ошибки, адаптируется к последствиям и остаётся расширяемым для разработчика.

