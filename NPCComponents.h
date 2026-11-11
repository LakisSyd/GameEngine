#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "Components.h"

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
    Blocked            // Продолжить нельзя: нет маршрута, питания двери и т. п.
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

struct NPCComponent
{
    // Общие gameplay-данные персонажа. Они намеренно отделены от NpcBrainComponent:
    // здоровье/имя принадлежат самому NPC, а маршрут и scores — его "мозгу".
    std::string name = "Crew NPC";
    std::string description;
    float health = 100.0f;
    float maxHealth = 100.0f;
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

    // While crossing a fully open portal, Bullet may wedge the capsule between
    // a floor seam and the lower door-frame collider. Only the active door root
    // and its leaves are temporarily ignored; this Entity tracks the pair so it
    // can be restored deterministically on the first non-crossing frame.
    Entity portalCollisionIgnoredDoor = INVALID_ENTITY;
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

    // Runtime snapshot of the current door transition. These values are only
    // diagnostic and let the play-mode panel explain exactly why a capsule is
    // stopping at a portal without relying on transient debugger breakpoints.
    std::string debugDoorPhase = "No active door transition";
    Entity debugDoorDestinationRoom = INVALID_ENTITY;
    DirectX::XMFLOAT3 debugDoorPosition = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 debugDoorApproach = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 debugDoorExit = { 0.0f, 0.0f, 0.0f };
    float debugDistanceToDoor = -1.0f;
    float debugDistanceToApproach = -1.0f;
    float debugProgressBeyondDoor = 0.0f;
    float debugRequiredExitProgress = 0.0f;
    float debugDoorOpenProgress = 0.0f;
    bool debugHasDoorExit = false;
    bool debugDoorSnapshotCaptured = false;
    std::string debugDoorSnapshot;

    // Человекочитаемая строка "что происходит сейчас" и подробная причина
    // невозможности продолжить. Они нужны только для диагностики, не для логики.
    std::string status = "Automatic utility AI";
    std::string blockedReason;
};
