#pragma once

#include <cstdint>
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "Components.h"
#include "CharacterVitalsComponent.h"
#include "Gameplay/AI/NpcCombatModules.h"




// =============================================================================
// КАК УСТРОЕН NPC AI (короткая карта перед чтением кода)
// =============================================================================
//
// AI специально разделён на несколько простых слоёв. Не пытайся искать всё
// поведение в одном огромном switch:
//
//   1. NPCComponents.h
//      Только ДАННЫЕ: текущее состояние, настройки скорости, цель ремонта,
//      маршрут через двери и отладочные значения. Здесь почти нет поведения.
//
//   2. NpcCommandSystem.h
//      Принимает РУЧНЫЕ команды из debug UI или будущих скриптов. Пока команда
//      активна, она имеет безусловный приоритет над автоматическим ремонтом.
//
//   3. NpcUtilitySystem.h
//      "Думает" за NPC в автоматическом режиме: оценивает доступные задачи и
//      выбирает самую полезную. Сейчас задача одна — ремонт пробоины.
//
//   4. StateSystem.h
//      ИСПОЛНЯЕТ выбранную задачу: идёт к цели, строит маршрут по комнатам,
//      открывает двери, ждёт их открытия и вызывает RepairSystem.
//
//   5. CharacterControllerSystem + AnimationController
//      Фактически двигают Bullet-контроллер и выбирают Idle/Walk/Run-анимацию
//      по значениям CharacterMovement, которые выставил StateSystem.
//
// Главная мысль: "что делать" и "как это сделать" разделены. Поэтому новую
// задачу (например, тушить пожар) можно добавить, не переписывая двери, физику
// и всю систему ручных debug-команд.
// =============================================================================

// NpcState — то, что NPC ФАКТИЧЕСКИ делает в текущем кадре.
// Это не то же самое, что NpcCommandType. Например, команда RepairEntity во
// время выполнения последовательно даст состояния MoveToRepair ->
// WaitingForDoor -> MoveToRepair -> Repairing.

enum class FactionId : std::uint8_t
{
    Iridian, // Одна из воюющих сторон.
    Sirius,  // Вторая воюющая сторона.
    Trade,   // Торговая сторона; сейчас враждует с другими ненейтральными.
    Neutral  // Нейтральные никого не считают врагом и сами не атакуются.
};

struct FactionComponent
{
    // Команда/сторона сущности. Бой сравнивает фракции через AreEnemies().
    FactionId faction = FactionId::Neutral;
};
enum class FactionRelation
{
    Friendly, // Одинаковая фракция: атаковать нельзя.
    Neutral,  // Хотя бы одна сторона Neutral: атаковать нельзя.
    Hostile   // Разные ненейтральные стороны: цель допустима для боя.
};

// Возвращает отношение одной фракции к другой. Функция симметрична:
// GetFactionRelation(a, b) даёт тот же результат, что и (b, a).
inline FactionRelation GetFactionRelation(FactionId a, FactionId b)
{
    if (a == b)
        return FactionRelation::Friendly;

    if (a == FactionId::Neutral || b == FactionId::Neutral)
        return FactionRelation::Neutral;

    // Пока простое правило:
    return FactionRelation::Hostile;
}

// Короткая проверка для систем зрения/боя: true означает «можно атаковать».
inline bool AreEnemies(FactionId a, FactionId b)
{
    return GetFactionRelation(a, b) == FactionRelation::Hostile;
}


enum class NpcState
{
    Idle,              // Ничего не делает, CharacterMovement остановлен.
    Wandering,         // Идёт к случайной точке и иногда делает паузу.
    MoveToPosition,    // Выполняет ручную команду движения в мировую точку.
    MoveToRoom,        // Выполняет ручную команду движения в выбранную комнату.
    Following,         // Постоянно пересчитывает цель по позиции другой Entity.
    MoveToRepair,      // Идёт к пробоине, выбранной Utility AI или вручную.
    WaitingForDoor,    // Стоит перед дверью, пока створки ещё закрывают проход.
    Repairing,         // Уже рядом с пробоиной и накапливает прогресс ремонта.
    Stopped,           // Ручная команда Stop; автоматика не перехватит управление.
    Blocked,           // Продолжить нельзя: нет маршрута, питания двери и т. п.
    Chasing,           // NPC преследует обнаруженную цель.
	Attacking,          // NPC атакует цель (например, игрока) в ближнем бою.
	Searching,          // NPC ищет цель, которую потерял из виду (например, игрока).
    Patrolling,         // Идёт по авторскому маршруту Patrol Point.
    TakingCover,       // Бежит к назначенной позиции укрытия или точке выглядывания.
    InCover            // Спрятан и ждёт своего окна для ответного огня.

};

// NpcCommandType — приказ с высоким приоритетом.
// None означает не "ничего не делать", а "разрешить автоматическому Utility AI
// самому выбрать работу". Любое другое значение временно блокирует арбитраж
// Utility AI. Это и обеспечивает требуемый приоритет debug-команд над ремонтом.
enum class NpcCommandType
{
    None,               // Ручной команды нет: работает автоматический AI.
    Stop,               // Стоять бесконечно, пока команду не заменят/не снимут.
    Wander,             // Бродить бесконечно внутри комнаты/около точки старта.
    MoveToPosition,     // Одноразово прийти в worldPosition.
    MoveToRoom,         // Одноразово прийти в targetRoom.
    FollowEntity,       // Следовать за targetEntity до отмены команды.
    RepairEntity        // Одноразово прийти и починить targetEntity-пробоину.
};

inline const char* NpcStateName(NpcState state)
{
    switch (state)
    {
    case NpcState::Idle: return "Idle";
    case NpcState::Wandering: return "Wandering";
    case NpcState::MoveToPosition: return "Move to position";
    case NpcState::MoveToRoom: return "Move to room";
    case NpcState::Following: return "Following";
    case NpcState::MoveToRepair: return "Move to repair";
    case NpcState::WaitingForDoor: return "Waiting for door";
    case NpcState::Repairing: return "Repairing";
    case NpcState::Stopped: return "Stopped";
    case NpcState::Blocked: return "Blocked";
    case NpcState::Attacking: return "Attacking";
    case NpcState::Chasing: return "Chasing";
	case NpcState::Searching: return "Searching";
    case NpcState::Patrolling: return "Patrolling";
    case NpcState::TakingCover: return "Taking cover";
    case NpcState::InCover: return "In cover";
    default: return "Unknown";
    }
}

inline const char* NpcCommandName(NpcCommandType type)
{
    switch (type)
    {
    case NpcCommandType::None: return "Automatic utility AI";
    case NpcCommandType::Stop: return "Stop";
    case NpcCommandType::Wander: return "Wander";
    case NpcCommandType::MoveToPosition: return "Move to position";
    case NpcCommandType::MoveToRoom: return "Move to room";
    case NpcCommandType::FollowEntity: return "Follow entity";
    case NpcCommandType::RepairEntity: return "Repair entity";
    default: return "Unknown";
    }
}

struct NpcCommand
{
    // Какой приказ сейчас активен. type == None передаёт управление Utility AI.
    NpcCommandType type = NpcCommandType::None;

    // Используется только командами FollowEntity и RepairEntity.
    Entity targetEntity = INVALID_ENTITY;

    // Используется только MoveToRoom. Комната обязана иметь RoomComponent и
    // TransformComponent, иначе StateSystem покажет причину в blockedReason.
    Entity targetRoom = INVALID_ENTITY;

    // Используется только MoveToPosition. Это МИРОВЫЕ, а не локальные координаты.
    DirectX::XMFLOAT3 worldPosition = { 0.0f, 0.0f, 0.0f };

    // Счётчик версии команды. Он увеличивается при каждом новом приказе и при
    // Clear(). Сейчас полезен для диагностики; в будущем по нему можно понять,
    // что асинхронный pathfinding получил уже устаревший результат.
    std::uint32_t revision = 0;

    // Единственная официальная проверка ручного приоритета. Не проверяй отдельно
    // debugControlEnabled: этот флаг лишь показывает/разрешает UI, а команда
    // может прийти и из gameplay-скрипта.
    bool IsActive() const { return type != NpcCommandType::None; }

    // Снять ручной приказ. Обрати внимание: целевые поля чистятся, а worldPosition
    // можно не обнулять — без MoveToPosition оно всё равно не читается.
    void Clear()
    {
        type = NpcCommandType::None;
        targetEntity = INVALID_ENTITY;
        targetRoom = INVALID_ENTITY;
        ++revision;
    }
};

// Маркер позиции за препятствием. TransformComponent задаёт место, где NPC
// прячется, а peekOffset — локальное смещение к краю для короткого выхода и огня.
// Один маркер занимает только один NPC: это предотвращает слипание отряда.
struct NpcCoverPointComponent
{
    // Смещение от центра укрытия к месту выглядывания, в локальных координатах.
    DirectX::XMFLOAT3 peekOffset = { 1.15f, 0.0f, 0.0f };
    // true разрешает также проверить -peekOffset. Это нужно для
    // длинной стены, из-за которой можно безопасно выйти к любому краю.
    bool allowOppositePeek = false;
    // Высота луча проверки защиты над Transform.position, в метрах мира.
    float protectionHeight = 1.35f;
    // Подсказывает AnimationController, что за этим укрытием нужна поза сидя.
    bool crouched = true;
    // false временно исключает точку из выбора, не удаляя её из сцены.
    bool enabled = true;
};

// Точка маршрута, создаваемая прямо в редакторе. routeId объединяет точки в
// маршрут, order задаёт последовательность, waitTime — паузу после прибытия.
struct NpcPatrolPointComponent
{
    // Номер маршрута. NPC использует только точки с таким же routeId.
    int routeId = 0;
    // Порядок обхода внутри маршрута: меньшее значение посещается раньше.
    int order = 0;
    // Сколько секунд стоять после прихода именно к этой точке.
    float waitTime = 1.0f;
    // false оставляет точку в сцене, но исключает её из маршрута.
    bool enabled = true;
};

// Настройка патруля есть у каждого NPC независимо от hostile/friendly.
// Runtime-поля не сохраняются: после загрузки маршрут безопасно начинается заново.
struct NpcPatrolComponent
{
    // Какой набор Patrol Point принадлежит этому NPC.
    int routeId = 0;
    // Главный выключатель патруля. При false NPC остаётся в обычном Idle/Utility AI.
    bool enabled = false;
    // После последней точки перейти к первой.
    bool loop = true;
    // После последней точки развернуть порядок: 0,1,2,1,0.
    bool pingPong = false;
    // Разрешить StateSystem выбрать runSpeed на длинном участке маршрута.
    bool allowRunning = false;
    // Дистанция до точки, на которой она считается достигнутой, в метрах.
    float arrivalDistance = 0.45f;
    // Запасная пауза, если у точки не задано более подходящее значение.
    float defaultWaitTime = 1.0f;

    // Индекс текущей точки в уже отсортированном runtime-списке.
    int currentIndex = 0;
    // +1 идём вперёд, -1 назад; используется режимом pingPong.
    int direction = 1;
    // Оставшееся время ожидания на достигнутой точке, в секундах.
    float waitTimer = 0.0f;
    // Текстовое объяснение текущей стадии для инспектора.
    std::string status = "Patrol disabled";
};

enum class NpcSquadRole
{
    None,       // Роль ещё не назначена или NPC не входит в боевую группу.
    Suppressor, // Удерживает внимание короткими очередями.
    Flanker     // Выбирает укрытие сбоку от направления на игрока.
};

// Индивидуальный стиль боя. Это НЕ роль в отряде: два врага могут выбирать
// разные стили, ничего не сообщая друг другу.
enum class NpcTacticalStyle
{
    Auto,       // Runtime-система выбирает стиль по ситуации и темпераменту.
    Balanced,   // Ограниченный поиск и обычный приоритет укрытий.
    Assault,    // Давит потерянную цель и глубже проверяет сектора поиска.
    Defensive   // Держит дистанцию и сначала ищет защитную позицию.
};

inline const char* NpcTacticalStyleName(NpcTacticalStyle style)
{
    switch (style)
    {
    case NpcTacticalStyle::Auto: return "Auto";
    case NpcTacticalStyle::Balanced: return "Balanced";
    case NpcTacticalStyle::Assault: return "Assault";
    case NpcTacticalStyle::Defensive: return "Defensive";
    default: return "Unknown";
    }
}

inline const char* NpcTacticalStyleNameRu(NpcTacticalStyle style)
{
    switch (style)
    {
    case NpcTacticalStyle::Auto: return "Авто";
    case NpcTacticalStyle::Balanced: return "Сбалансированная";
    case NpcTacticalStyle::Assault: return "Штурм";
    case NpcTacticalStyle::Defensive: return "Оборона";
    default: return "Неизвестно";
    }
}

// Тактическая память отдельного бойца. Все поля авторских настроек находятся
// здесь, поэтому новые роли/анимации можно добавить без переписывания боя.
struct NpcTacticalComponent
{
    enum PeekSide
    {
		none,       // NPC не использует укрытие и не выглядывает.
		left,       // NPC стоит за укрытием и выглядывает влево.
		right,      // NPC стоит за укрытием и выглядывает вправо.
    };

    PeekSide peekState = PeekSide::none;

	bool isPeeking = false; //Выглядывает ли NPC в данный момент из-за укрытия

	bool wantsPeek = false; // NPC хочет выглянуть из-за укрытия, но может быть заблокирован

	float peekTimer = 0.0f; // таймер для выглядывания

    // Настройка автора. Любое значение кроме Auto полностью фиксирует стиль и
    // удобно для теста «Штурм против Обороны».
    NpcTacticalStyle selectedStyle = NpcTacticalStyle::Auto;
    // Реальный стиль текущего кадра. При selectedStyle=Auto его выбирает AI.
    NpcTacticalStyle activeStyle = NpcTacticalStyle::Balanced;
    // 0 = осторожный характер, 1 = склонность давить противника. Это не
    // мгновенное решение, а устойчивое различие между двумя NPC.
    float tacticalAggression = 0.5f;
    // После смены автоматического стиля некоторое время запрещена новая смена,
    // иначе два бойца будут дёргаться между штурмом и обороной каждый кадр.
    float tacticCommitDuration = 8.0f;
    float tacticCommitTimer = 0.0f;
    // Диагностика автоматического выбора для Studio и журнала.
    std::string tacticReason = "Начальная сбалансированная тактика";
    float lastThreatDistance = 0.0f;
    bool hasThreatDistanceSample = false;
    float enemyClosingSpeed = 0.0f;
    // Номер группы. Бойцы с одинаковым squadId обмениваются последней
    // подтверждённой позицией цели и распределяют роли.
    int squadId = 1;
    // Текущая роль в группе; назначается координатором во время игры.
    NpcSquadRole role = NpcSquadRole::None;
    // Entity авторской Cover Point, закреплённой за этим NPC. INVALID_ENTITY
    // означает, что точка не занята или используется временный фланг.
    Entity reservedCover = INVALID_ENTITY;
    // true означает, что укрытие найдено анализом NavMesh/геометрии, а не
    // заранее расставленной NpcCoverPoint Entity. Для такого укрытия
    // reservedCover закономерно остаётся INVALID_ENTITY.
    bool dynamicCoverActive = false;
    // Runtime-владелец текущего назначения. Combat Lab помечает найденное им
    // укрытие, чтобы Cancel удалял только тестовый приказ, а не решение AI.
    // Поле не сохраняется в сцену.
    bool debugForcedCover = false;
    // Отдельно отмечает ручной Find/Peek/Return/Reposition. Он может работать
    // и с обычным AI-укрытием, поэтому не всегда означает владение точкой.
    bool debugForcedCoverAction = false;
    // Мировая позиция за препятствием, куда NPC прячется.
    DirectX::XMFLOAT3 coverPosition{};
    // Мировая позиция у края препятствия, откуда разрешено выглянуть и стрелять.
    DirectX::XMFLOAT3 peekPosition{};
    // Фактическая ближайшая тактическая цель движения для StateSystem.
    DirectX::XMFLOAT3 moveTarget{};
    // Как часто разрешено заново выбирать укрытие/фазу, в секундах.
    float decisionInterval = 0.45f;
    // Сколько выходов к peekPosition закончились получением урона.
    int failedPeekCount = 0;
    // Общее число законченных выходов из текущего укрытия. Даже полностью
    // безопасная позиция не должна превращаться в бесконечный тир из одного
    // угла: после нескольких циклов NPC меняет линию огня.
    int completedPeekCount = 0;
    // После возвращения за стену нужно выбрать другую Cover Point.
    bool coverChangeRequested = false;

    // Обратный таймер до следующего тактического решения.
    float decisionTimer = 0.0f;
    // Сколько сидеть полностью за укрытием перед выглядыванием.
    float hideDuration = 2.75f;
    // Сколько оставаться открытым у peekPosition перед возвратом.
    float exposeDuration = 0.75f;
    // Выход к краю стены должен быть резким. Это скорость
    // короткого шага coverPosition -> peekPosition, а не беговая
    // анимация. Позже его можно напрямую связать с lean/peek clip.
    float peekMoveSpeed = 4.0f;
    // На ходу NPC ведёт не бесконечный автоогонь, а короткие очереди.
    int movingBurstSize = 3;
    // Пауза между очередями на ходу даёт оружию восстановить отдачу.
    float movingBurstPauseDuration = 0.65f;
    // После стольких попаданий во время peek точка считается раскрытой.
    int failedPeeksBeforeRelocate = 4;
    // Даже под ответным огнём NPC обязан некоторое время разыгрывать уже
    // выбранное укрытие. Иначе два быстрых попадания превращают бой в
    // бесконечный марафон между стенами.
    float minimumCoverHoldDuration = 8.0f;
    // Runtime-таймер текущего обязательства удерживать позицию.
    float coverHoldTimer = 0.0f;
    // Однокадровый неудачный raycast не должен мгновенно выбрасывать NPC из
    // укрытия. Таймер накапливает только непрерывную потерю защиты.
    float coverValidationFailureTime = 0.0f;
    float coverValidationGraceDuration = 0.75f;
    // Оставшееся время текущей фазы «спрятан/выглянул».
    float phaseTimer = 0.0f;
    // Контроль прогресса приказа: нельзя бесконечно идти в недостижимый край.
    float moveElapsed = 0.0f;
    // Сколько секунд позиция почти не менялась при активной цели движения.
    float noProgressTime = 0.0f;
    // Позиция предыдущей проверки прогресса движения.
    DirectX::XMFLOAT3 lastProgressPosition{};
    // Последняя забракованная точка, чтобы не выбрать её снова на следующем кадре.
    Entity rejectedCover = INVALID_ENTITY;
    // Сколько ещё секунд действует временный запрет rejectedCover.
    float rejectedCoverTimer = 0.0f;
    // Насколько сильно при оценке предпочитается боковой угол к противнику.
    float flankWeight = 2.5f;
    // Желаемая дистанция боя: слишком близкие/далёкие позиции получают штраф.
    float preferredRange = 8.0f;
    // Разрешает тестовый автоматический поиск укрытия, если ни одна авторская
    // точка не подошла. Авторские точки всё равно проверяются первыми.
    bool allowDynamicCover = true;
    // Радиус и плотность выборки вокруг NPC. Это именно тестовые параметры:
    // позже перебор по окружностям можно заменить выборкой рёбер NavMesh.
    float dynamicCoverSearchRadius = 10.0f;
    int dynamicCoverSamples = 32;
    // Временная позиция обхода без авторского CoverPoint, фиксируется на время пути.
    bool hasFlankTarget = false;
    // Мировая временная позиция фланга, найденная на NavMesh без Cover Point.
    DirectX::XMFLOAT3 flankTarget{};
    // Оставшееся время, в течение которого NPC не меняет уже выбранный фланг.
    float flankCommitTime = 0.0f;
    // Сколько разных секторов уже было выдано этому NPC при поиске цели.
    int searchSectorsAssigned = 0; // Ограниченный поиск после потери общего контакта.
    // true, пока NPC идёт проверять назначенный сектор.
    bool searchingSector = false;
    int searchRouteFailures = 0; // Отказы пути не являются проверенными секторами.
    // Задержка перед следующей попыткой подобрать сектор после неудачного пути.
    float searchRetryTime = 0.0f;
    // Счётчики причин отбраковки нужны только для понятной диагностики поиска.
    int searchRejectedNavigation = 0;
    int searchRejectedSpacing = 0;
    int searchRejectedNear = 0;
    // Последняя выданная точка поиска — защита от повторения одного места.
    DirectX::XMFLOAT3 lastSearchDestination{};
    bool hasLastSearchDestination = false;
    float repositionCooldown = 3.0f; // Пауза перед сменой угла, не сбрасывается каждым лучом зрения.
    // Сколько времени весь отряд не имеет подтверждённого визуального контакта.
    float contactLostTime = 0.0f;
    // Диагностический флаг: все проверенные Cover Point оказались плохими.
    bool noSuitableCover = false;
    // Почему точки укрытия не прошли фильтр на последнем выборе.
    int coverRejectedProtection = 0;
    int coverRejectedSight = 0;
    int coverRejectedPath = 0;
    // Линии существуют только для отладки текущего последнего поиска.
    // Каждая пара соседних точек — один отрезок. Красные кандидаты не прошли
    // защиту/путь, голубые проверяют peek, зелёный показывает победителя.
    std::vector<DirectX::XMFLOAT3> dynamicCoverRejectedRays;
    std::vector<DirectX::XMFLOAT3> dynamicCoverPeekRays;
    std::vector<DirectX::XMFLOAT3> dynamicCoverAcceptedRays;
    // Накопленное время прицеливания в разрешённом режиме стрельбы на ходу.
    float movingAimTime = 0.0f;
    // Runtime-счётчики очереди. Они не сохраняются в сцену.
    int movingBurstShotsRemaining = 0;
    float movingBurstPauseTimer = 0.0f;
    // NPC уже сделал выстрел с текущего маршрута; помогает завершить фланг.
    bool firedOnRoute = false;
    // Короткая остановка в удачной позиции, чтобы NPC не пробежал её насквозь.
    float firingPositionHold = 0.0f;
    // Последняя смена тактической фазы человеческим языком для инспектора.
    std::string lastTacticalTransition;
    float underFireTime = 0.0f; // Память угрозы/попадания без точной позиции стрелявшего.
    bool damageReactionPending = false;
    // damageReactionPending сообщает о любом сигнале угрозы, включая near miss.
    // Этот флаг отдельно подтверждает, что здоровье действительно было атаковано.
    bool damageReactionWasHit = false;
    // Штурмовик не ломает атаку от одной пули, но помнит несколько попаданий,
    // полученных за короткое окно времени.
    int recentDamageHits = 0;
    float recentDamageWindow = 0.0f;
    // Попадание даёт лишь примерное направление на близкого
    // стрелка. Точная позиция и targetEntity из урона не выдаются.
    DirectX::XMFLOAT3 incomingFireDirection{};
    bool hasIncomingFireDirection = false;
    float incomingFireTurnTimer = 0.0f;
    // Дальний попавший стрелок не раскрывает своё направление.
    float incomingFireAwarenessRange = 25.0f;
    // Защита от нового полного перерасчёта на каждую пулю очереди.
    float damageReactionCooldown = 0.0f;
    // true: текущая moveTarget ведёт из укрытия к peekPosition.
    bool movingToPeek = false;
    // true: NPC сейчас находится на открытой позиции и может вести огонь.
    bool exposed = false;
    // Копия свойства выбранной Cover Point для выбора анимационной позы.
    bool coverUsesCrouch = true;
    bool crouched = false; // Уже доступно AnimationController для будущего клипа.
    std::string status = "No tactical order";
};


struct NPCCombatComponent
{
	// Entity выбранного противника. INVALID_ENTITY = сейчас цели нет.
	Entity targetEntity = INVALID_ENTITY;
	float attackRange = 1.5f; // Дистанция, на которой NPC может атаковать цель.
	float seeRange = 10.0f; // Дистанция, на которой NPC может видеть цель.
    // На дистанции почти соприкосновения противник замечается независимо от
    // направления взгляда. Физический raycast всё равно не позволяет видеть
    // через стену, поэтому это не превращается во всезнание.
    float closeAwarenessRange = 2.0f;
    // До первого контакта вооружённый NPC может ходить по
    // достижимым точкам и поворачивать сектор зрения вместе с корпусом.
    bool wanderUntilContact = true;
    float contactWanderRadius = 14.0f;
	float attackCooldown = 1.0f; // Время между атаками в секундах.
    float rangedAttackRange = 12.0f; // Желаемая дальность стрельбы, не больше дальности оружия.
    float rangedAimDelay = 0.35f; // Сколько целиться после остановки и разворота.
    float rangedAimTime = 0.0f; // Временный таймер, сбрасывается при потере цели.
    std::string rangedStatus; // Причина ожидания/отказа выстрела в дебаг-панели.
    bool targetVisible = false; // Результат проверки зрения в текущем кадре.
    DirectX::XMFLOAT3 visibleAimPoint{}; // Только лично проверенная открытая часть тела.

    // Память наблюдения: сюда попадает только то, что NPC действительно видел.
    DirectX::XMFLOAT3 lastKnownPosition = { 0.0f, 0.0f, 0.0f };
    bool hasLastKnownPosition = false;
    DirectX::XMFLOAT3 lastSeenVelocity = {};
    // Авторские настройки прогноза, сохраняемые вместе со сценой.
    // Смотрим немного вперёд по последнему направлению, но не дальше лимита.
    float predictionSeconds = 0.8f;
    float maxPredictionDistance = 3.0f;
    float predictionTravelTimeout = 6.0f;
    // Временный план поиска. Это предположение, а не новая увиденная позиция.
    // План рассчитывается один раз; скрытый игрок не двигает эту точку за собой.
    DirectX::XMFLOAT3 searchDestination = {};
    bool searchPlanReady = false;
    bool investigatingPrediction = false;
    float searchTravelTime = 0.0f;
    // Остаток осмотра на месте. Это не движение: walkDirection остаётся нулевым.
    float searchTurnRemaining = 0.0f;

    // Запускает круговой осмотр на месте после прихода к последней известной
    // позиции. Значение 2*PI означает полный оборот на 360 градусов.
    void BeginSearchTurn()
    {
        searchTurnRemaining = DirectX::XM_2PI;
    }

    // Поворачивает yaw на один небольшой шаг поиска и возвращает новый угол.
    // rotationSpeed измеряется в радианах/сек, dt — длительность кадра.
    float AdvanceSearchTurn(float yaw, float rotationSpeed, float dt)
    {
        if (!std::isfinite(dt) || dt <= 0.0f || searchTurnRemaining <= 0.0f)
            return yaw;
        // Нулевая скорость в старой сцене не должна навечно заблокировать поиск.
        const float speed = std::isfinite(rotationSpeed) && rotationSpeed > 0.0f
            ? rotationSpeed : DirectX::XM_PIDIV2;
        const float step = (std::min)(searchTurnRemaining, speed * dt);
        searchTurnRemaining = (std::max)(0.0f, searchTurnRemaining - step);
        return std::atan2(std::sin(yaw + step), std::cos(yaw + step));
    }

    // Вызывается только при видимости. Скорость получаем из двух соседних
    // наблюдений. После повторного обнаружения начинаем измерение заново:
    // перемещение игрока за стеной не должно превращаться в известную скорость.
    void ObserveVisibleTarget(const DirectX::XMFLOAT3& position, float dt,
        bool continuouslyVisible)
    {
        lastSeenVelocity = {};
        if (continuouslyVisible && hasLastKnownPosition && dt > 0.00001f)
        {
            lastSeenVelocity.x = (position.x - lastKnownPosition.x) / dt;
            lastSeenVelocity.z = (position.z - lastKnownPosition.z) / dt;
        }
        lastKnownPosition = position;
        hasLastKnownPosition = true;
        timeSinceTargetSeen = 0.0f;
        searchPlanReady = false;
        investigatingPrediction = false;
        searchTravelTime = 0.0f;
        searchTurnRemaining = 0.0f;
    }

    // Единый сброс личной памяти цели. Он нужен модульному бою: когда Search
    // отключён, старая lastKnownPosition не должна бессрочно удерживать NPC в
    // скрытом полу-боевом состоянии.
    void ForgetTarget()
    {
        targetEntity = INVALID_ENTITY;
        targetVisible = false;
        visibleAimPoint = {};
        hasLastKnownPosition = false;
        lastSeenVelocity = {};
        timeSinceTargetSeen = 0.0f;
        searchDestination = {};
        searchPlanReady = false;
        investigatingPrediction = false;
        searchTravelTime = 0.0f;
        searchTurnRemaining = 0.0f;
        rangedAimTime = 0.0f;
        stanceTimeRemaining = 0.0f;
    }

    // Возвращает предполагаемую точку поиска: последняя увиденная позиция плюс
    // ограниченное упреждение по наблюдавшейся скорости цели.
    DirectX::XMFLOAT3 EstimateSearchPosition() const
    {
        // Прогнозируем только по горизонтали: не угадываем прыжок или другой
        // этаж. Даже при резком скачке наблюдаемой скорости ограничиваем путь.
        float x = lastSeenVelocity.x * (std::max)(0.0f, predictionSeconds);
        float z = lastSeenVelocity.z * (std::max)(0.0f, predictionSeconds);
        const float length = std::sqrt(x * x + z * z);
        const float limit = (std::max)(0.0f, maxPredictionDistance);
        if (!std::isfinite(length)) return lastKnownPosition;
        if (length > limit && length > 0.00001f)
        {
            x *= limit / length;
            z *= limit / length;
        }
        return { lastKnownPosition.x + x, lastKnownPosition.y, lastKnownPosition.z + z };
    }

    float loseTargetDelay = 3.0f;      // Через сколько секунд забыть потерянную цель.
    float timeSinceTargetSeen = 0.0f;  // Ожидание после проверки точек, не время погони.
    // Обратный таймер до следующего разрешённого удара/выстрела.
    float attackTimer = 0.0f;
    // Полный горизонтальный угол обзора NPC в градусах.
    float fieldOfViewDegrees = 120.0f;
    float chaseResumeMargin = 0.35f; // Запас дистанции перед выходом из стойки в погоню.
    float stanceEnterDelay = 0.30f; // Минимальная пауза в стойке перед первым ударом.
    float stanceTimeRemaining = 0.0f; // Остаток паузы, не сохраняется в сцену.

    // Вход и выход из стойки имеют разные пороги, чтобы NPC не дёргался
    // между бегом и атакой при небольшом движении игрока на границе дистанции.
    bool ShouldHoldMeleeStance(float distanceSquared, bool alreadyInStance) const
    {
        const float range = (std::max)(0.0f, attackRange) +
            (alreadyInStance ? (std::max)(0.0f, chaseResumeMargin) : 0.0f);
        return distanceSquared <= range * range ||
            (alreadyInStance && stanceTimeRemaining > 0.0f);
    }

    // Вход в стойку не сбрасывает уже действующий более длинный кулдаун.
    void EnterMeleeStance()
    {
        stanceTimeRemaining = (std::max)(0.0f, stanceEnterDelay);
        attackTimer = (std::max)(attackTimer, stanceTimeRemaining);
    }

};

struct NPCComponent
{
    // Общие gameplay-данные персонажа. Они намеренно отделены от NpcBrainComponent:
    // здоровье/имя принадлежат самому NPC, а маршрут и scores — его "мозгу".
    std::string name = "Crew NPC"; // Отображаемое имя в инспекторе/отладке.
    std::string description;       // Свободное авторское описание персонажа.
    // Старый удобный признак типа NPC. Выбор конкретной цели боя всё равно
    // выполняется через FactionComponent и AreEnemies().
    bool isHostile = false;


};

struct NPCMovementComponent
{
    // Обычная скорость ходьбы, единиц мира в секунду.
    float speed = 2.0f;
    // Скорость бега. StateSystem не позволит ей стать меньше speed.
    float runSpeed = 4.5f;
    // Если прямая текущая цель дальше этого расстояния, разрешается бег.
    float runWhenFartherThan = 8.0f;
    // Насколько близко нужно подойти, чтобы считать обычную цель достигнутой.
    // Нулевое значение нежелательно: физический контроллер почти никогда не
    // попадает ровно в одну математическую точку.
    float arrivalDistance = 0.45f;
    // На этой дистанции NPC вызывает DoorComponent::TryOpen().
    float doorOpenDistance = 2.25f;
    // На этой дистанции NPC уже ждёт openProgress двери, а не давит в створки.
    float doorWaitDistance = 1.25f;
    // После открытия NPC обязан пройти на это расстояние ВНУТРЬ следующей
    // комнаты и только потом поворачивать к настоящей цели. Без такой точки он
    // начинал поворот прямо в проёме и упирался capsule в дверной косяк.
    float doorExitDistance = 1.50f;
    bool allowRunning = true;

    // Зарезервировано для будущего летающего NPC. Текущий MoveTowards двигается
    // только в плоскости XZ и это поле пока сознательно не используется.
    bool canFly = false;
};

// NpcBrainComponent — вся изменяемая "память" одного NPC.
// Система не хранит скрытое глобальное состояние: поэтому brain удобно смотреть
// в инспекторе, сериализовать и копировать для нескольких NPC.
struct NpcBrainComponent
{
    // Результат исполнения на текущем кадре и активный ручной приказ.
    NpcState state = NpcState::Idle;
    NpcCommand command;

    // -------------------------------------------------------------------------
    // ЦЕЛИ И МАРШРУТ ПО КОМНАТАМ
    // -------------------------------------------------------------------------

    // Цель автоматического Utility AI. Ручная команда RepairEntity хранит свою
    // цель в command.targetEntity и не затирает repairTarget. После Resume auto
    // автоматический AI сможет продолжить/переоценить свою работу.
    Entity repairTarget = INVALID_ENTITY;

    // Комната, внутри bounds которой сейчас находится позиция NPC.
    Entity currentRoom = INVALID_ENTITY;

    // Комната, для которой в последний раз строился doorRoute. Нужна, чтобы не
    // запускать BFS заново каждый кадр, если цель не изменилась.
    Entity navigationTargetRoom = INVALID_ENTITY;

    // Дверь, к которой NPC прямо сейчас идёт или открытия которой ожидает.
    Entity activeDoor = INVALID_ENTITY;

    // При проходе полностью открытой двери Bullet иногда зажимает capsule между
    // швом пола и нижним collider косяка. Только корень активной двери и створки
    // временно игнорируются; это поле запоминает дверь для точного восстановления.
    Entity portalCollisionIgnoredDoor = INVALID_ENTITY;
    // В текущем кадре NavigateTo всё ещё просит держать временный bypass.
    bool portalCollisionBypassRequestedThisFrame = false;

    // Последняя успешно пройденная дверь. Сейчас отображается для диагностики;
    // позже может использоваться, например, чтобы закрывать дверь за собой.
    Entity lastPassedDoor = INVALID_ENTITY;

    // Последовательность дверей от currentRoom до navigationTargetRoom.
    // Это маршрут ВЕРХНЕГО уровня: внутри комнаты NPC пока идёт напрямую.
    std::vector<Entity> doorRoute;
    // Комната назначения для каждой двери с тем же индексом. Пара векторов
    // означает: doorRoute[i] ведёт В doorDestinationRooms[i]. Это позволяет
    // вычислить безопасную точку выхода по направлению к центру нужной комнаты.
    std::vector<Entity> doorDestinationRooms;
    // Индекс следующей двери в doorRoute.
    std::size_t doorRouteIndex = 0;
    // true = существующий маршрут больше нельзя считать надёжным. NavigateTo
    // пересоберёт его, когда закончится repathTimer.
    bool routeDirty = true;

    // -------------------------------------------------------------------------
    // UTILITY AI И РЕМОНТ
    // -------------------------------------------------------------------------

    // Пробоины дальше этой дистанции получают score 0 и "не замечаются".
    float detectionRange = 30.0f;
    // На этой горизонтальной дистанции движение прекращается и начинается ремонт.
    float repairDistance = 1.0f;
    // Кэш для debug progress bar. Источник истины — RepairableComponent::progress.
    float repairProgress = 0.0f;
    // За сколько секунд NPC должен заполнить requiredProgress от 0 до 100%.
    float repairDuration = 3.0f;
    // Базовая полезность бездействия. Ремонт выбирается только если repairScore
    // строго больше этого значения.
    float idleScore = 0.10f;
    // Score лучшей найденной пробоины. Оставляем видимым даже при ручной команде.
    float repairScore = 0.0f;

    // -------------------------------------------------------------------------
    // ПЕРЕСТРОЕНИЕ МАРШРУТА И ДЕТЕКТОР "ЗАСТРЯЛ"
    // -------------------------------------------------------------------------

    // Минимальный интервал между перестроениями маршрута. Защищает от BFS каждый
    // кадр, если дверь/цель временно недоступна.
    float repathInterval = 0.75f;
    float repathTimer = 0.0f;
    // Сколько секунд NPC может хотеть двигаться, но фактически не менять позицию.
    float stuckTimeout = 2.0f;
    float stuckTimer = 0.0f;
    // Контрольная позиция для проверки реального прогресса движения.
    DirectX::XMFLOAT3 lastProgressPosition = { 0.0f, 0.0f, 0.0f };
    bool progressPositionInitialized = false;

    // -------------------------------------------------------------------------
    // БРОЖЕНИЕ (WANDER)
    // -------------------------------------------------------------------------

    // Центр запасного круга wander, если NPC находится вне RoomComponent.
    DirectX::XMFLOAT3 wanderAnchor = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 wanderTarget = { 0.0f, 0.0f, 0.0f };
    bool wanderTargetValid = false;
    // После достижения точки NPC немного стоит, чтобы движение выглядело живее.
    float wanderPauseTimer = 0.0f;
    float wanderRadius = 4.0f;
    // Собственный seed простого генератора случайных чисел. Благодаря этому NPC
    // не обязаны выбирать одинаковые точки и система не зависит от global rand().
    std::uint32_t randomState = 0x9E3779B9u;

    // -------------------------------------------------------------------------
    // DEBUG UI
    // -------------------------------------------------------------------------

    // Разрешает кнопкам панели создавать NpcCommand. Сам по себе НЕ является
    // командой и НЕ останавливает автоматический AI.
    bool debugControlEnabled = false;
    // Значения, отредактированные пользователем перед нажатием кнопки команды.
    Entity debugSelectedRoom = INVALID_ENTITY;
    Entity debugTargetEntity = INVALID_ENTITY;
    DirectX::XMFLOAT3 debugWorldTarget = { 0.0f, 0.0f, 0.0f };

    // Runtime-снимок прохода двери. Эти значения не участвуют в логике: панель
    // Play показывает по ним, почему capsule остановилась у портала.
    std::string debugDoorPhase = "No active door transition";
    // Комната по ту сторону текущей двери.
    Entity debugDoorDestinationRoom = INVALID_ENTITY;
    // Позиции корня двери, точки подхода и безопасного выхода в мировых координатах.
    DirectX::XMFLOAT3 debugDoorPosition = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 debugDoorApproach = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 debugDoorExit = { 0.0f, 0.0f, 0.0f };
    // Горизонтальные расстояния до двери/точки подхода; -1 = ещё не измерено.
    float debugDistanceToDoor = -1.0f;
    float debugDistanceToApproach = -1.0f;
    // Насколько NPC уже прошёл за плоскость двери и сколько требуется пройти.
    float debugProgressBeyondDoor = 0.0f;
    float debugRequiredExitProgress = 0.0f;
    // Копия DoorComponent::openProgress для одной строки диагностики.
    float debugDoorOpenProgress = 0.0f;
    // Удалось ли вычислить корректную точку за дверью.
    bool debugHasDoorExit = false;
    // true фиксирует первый проблемный снимок, чтобы он не затирался каждый кадр.
    bool debugDoorSnapshotCaptured = false;
    // Готовое подробное описание зафиксированного прохода.
    std::string debugDoorSnapshot;

    // Человекочитаемая строка "что происходит сейчас" и подробная причина
    // невозможности продолжить. Они нужны только для диагностики, не для логики.
    std::string status = "Automatic utility AI";
    std::string blockedReason;
};
