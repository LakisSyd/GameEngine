#pragma once

#include <DirectXMath.h>
#include <cstdint>
#include <deque>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>

#include "../../../NPCComponents.h"
#include "../Combat/DamageEvent.h"
#include "../../ECS/ECSRegistry.h"

// Категория нужна и фильтрам окна, и экспорту: журнал остаётся пригодным для
// машинного разбора, а не превращается в набор несвязанных строк.
enum class NpcDebugEventCategory
{
    Decision,   // AI сменил задачу, роль или состояние.
    Movement,   // Начало/конец движения и заметный прогресс.
    Damage,     // Получение урона или тестовый near miss.
    Perception, // Цель замечена, потеряна или снова обнаружена.
    Navigation, // Изменился маршрут, дверь или NavMesh-результат.
    Warning,    // AI заблокирован либо данные сцены некорректны.
    Debug       // Прочая диагностическая запись.
};

struct NpcDebugEvent
{
    // Монотонный номер записи: помогает восстановить точный порядок событий.
    std::uint64_t sequence = 0;
    // Время игрового мира, а не системные часы компьютера.
    double timeSeconds = 0.0;
    // NPC, к которому относится событие.
    Entity npc = INVALID_ENTITY;
    // Категория для фильтра и автоматической паузы.
    NpcDebugEventCategory category = NpcDebugEventCategory::Debug;
    // Русские заголовок и подробности.
    std::string title;
    std::string details;
    // Английские варианты для переключаемого интерфейса/экспорта.
    std::string titleEnglish;
    std::string detailsEnglish;
};

struct NpcDebugDamageRequest
{
    // Кому послать штатный DamageEvent.
    Entity target = INVALID_ENTITY;
    // Кто считается источником; INVALID_ENTITY допустим для окружения.
    Entity source = INVALID_ENTITY;
    // Величина тестового урона.
    float amount = 20.0f;
    // Тип влияет на штатную обработку урона.
    DamageEvent::DamageType type = DamageEvent::DamageType::Physical;
    // true имитирует близкий промах: угроза есть, здоровье не уменьшается.
    bool nearMiss = false;
};

// Боевые команды Studio не являются обычным NpcCommand: они проверяют один
// узкий слой и не должны навсегда блокировать автономный AI.
enum class NpcDebugCombatAction
{
    AcquireNearestTarget,
    FaceTarget,
    ChaseTarget,
    HoldPosition,
    FireStance,
    DodgeLeft,
    DodgeRight,
    FindCover,
    PeekLeft,
    ReturnBehindCover,
    Reposition,
    CancelForcedAction
};

struct NpcDebugCombatActionRequest
{
    Entity npc = INVALID_ENTITY;
    NpcDebugCombatAction action =
        NpcDebugCombatAction::AcquireNearestTarget;
};

// Только отладочное состояние. Оно не сохраняется в сцену, не участвует в
// принятии решений и наблюдает AI уже после выполнения штатных систем.
struct NpcSimulationStudio
{
    // Окно и сбор истории включены.
    inline static bool enabled = false;
    // Симуляция остановлена между шагами.
    inline static bool paused = true;
    // Разрешить ровно один штатный тик даже при paused == true.
    inline static bool step = false;
    // UI запросил телепортацию тестовой цели в targetPosition.
    inline static bool moveTarget = false;
    // Желаемая мировая позиция тестовой цели.
    inline static DirectX::XMFLOAT3 targetPosition{};
    // Множитель скорости симуляции из студии.
    inline static float speed = 1.0f;
    // Две сохранённые позиции для воспроизведения одинакового сценария.
    inline static DirectX::XMFLOAT3 targetBookmarks[2]{};
    inline static bool hasBookmark[2]{false, false};

    // NPC, подробности которого показывает вкладка Inspector.
    inline static Entity inspectedNpc = INVALID_ENTITY;
    // Одноэлементная очередь запроса тестового урона.
    inline static NpcDebugDamageRequest damageRequest{};
    inline static bool damageRequestPending = false;
    // Одноэлементная очередь принудительного действия Combat Lab.
    inline static NpcDebugCombatActionRequest combatActionRequest{};
    inline static bool combatActionPending = false;
    inline static bool lastCombatActionSucceeded = false;
    inline static std::string lastCombatActionResult =
        "No Combat Lab command has run yet";
    // Автоматически поставить паузу после события выбранной категории.
    inline static bool freezeOnDecision = false;
    inline static bool freezeOnDamage = false;
    inline static bool freezeOnWarning = true;
    // false = английские подписи студии, true = русские.
    inline static bool russianInterface = false;

    // История возвращается только для чтения; добавлять события надо PushEvent().
    static const std::deque<NpcDebugEvent>& Events() { return s_events; }

    // Полностью начинает новую диагностическую сессию.
    static void ClearHistory()
    {
        s_events.clear();
        s_snapshots.clear();
        s_nextSequence = 1;
        step = false;
        moveTarget = false;
        damageRequestPending = false;
        damageRequest = {};
        combatActionPending = false;
        combatActionRequest = {};
        lastCombatActionSucceeded = false;
        lastCombatActionResult = "No Combat Lab command has run yet";
    }

    // Ставит тестовый урон в очередь и будит один тик симуляции.
    static void QueueDamage(const NpcDebugDamageRequest& request)
    {
        damageRequest = request;
        damageRequestPending = true;
        // Даже на паузе один штатный тик обязан обработать поставленное событие.
        step = true;
    }

    // Забирает ожидающий запрос ровно один раз. true = request заполнен.
    static bool ConsumeDamageRequest(NpcDebugDamageRequest& request)
    {
        if (!damageRequestPending)
            return false;
        request = damageRequest;
        damageRequestPending = false;
        return true;
    }

    static void QueueCombatAction(const NpcDebugCombatActionRequest& request)
    {
        combatActionRequest = request;
        combatActionPending = true;
        lastCombatActionResult = "Command queued";
        // Команда исполняется настоящим игровым тиком даже на паузе.
        step = true;
    }

    static bool ConsumeCombatAction(NpcDebugCombatActionRequest& request)
    {
        if (!combatActionPending)
            return false;
        request = combatActionRequest;
        combatActionPending = false;
        return true;
    }

    static void ReportCombatAction(bool succeeded, std::string result)
    {
        lastCombatActionSucceeded = succeeded;
        lastCombatActionResult = std::move(result);
    }

    // Добавляет одну запись, подставляет английский fallback, ограничивает
    // размер истории и при необходимости останавливает симуляцию.
    static void PushEvent(double timeSeconds, Entity npc,
        NpcDebugEventCategory category, std::string title, std::string details = {},
        std::string titleEnglish = {}, std::string detailsEnglish = {})
    {
        NpcDebugEvent event;
        event.sequence = s_nextSequence++;
        event.timeSeconds = timeSeconds;
        event.npc = npc;
        event.category = category;
        event.title = std::move(title);
        event.details = std::move(details);
        event.titleEnglish = titleEnglish.empty() ? event.title : std::move(titleEnglish);
        event.detailsEnglish = detailsEnglish.empty() ? event.details : std::move(detailsEnglish);
        s_events.push_back(std::move(event));
        while (s_events.size() > MaxEvents)
            s_events.pop_front();

        // Точка останова срабатывает после записи, поэтому виновное событие
        // всегда остаётся последней видимой строкой журнала.
        if ((category == NpcDebugEventCategory::Decision && freezeOnDecision) ||
            (category == NpcDebugEventCategory::Damage && freezeOnDamage) ||
            (category == NpcDebugEventCategory::Warning && freezeOnWarning))
            paused = true;
    }

    // Сравнивает текущие снимки NPC с предыдущим кадром и превращает реальные
    // изменения состояния/цели/здоровья в события журнала.
    static void Capture(ECSRegistry& registry, double timeSeconds)
    {
        registry.ForEach<NpcBrainComponent>([&](Entity npc, NpcBrainComponent& brain)
        {
            Snapshot current;
            current.state = brain.state;
            current.commandRevision = brain.command.revision;
            current.blockedReason = brain.blockedReason;
            current.routeIndex = brain.doorRouteIndex;
            current.routeSize = brain.doorRoute.size();

            if (const auto* vitals =
                registry.GetComponent<CharacterVitalsComponent>(npc))
                current.health = vitals->health;
            if (const auto* damageable = registry.GetComponent<DamageableComponent>(npc))
                current.health = damageable->health;
            if (const auto* combat = registry.GetComponent<NPCCombatComponent>(npc))
            {
                current.target = combat->targetEntity;
                current.targetVisible = combat->targetVisible;
            }
            if (const auto* tactical = registry.GetComponent<NpcTacticalComponent>(npc))
            {
                current.hasTactical = true;
                current.goal = tactical->hasFlankTarget ? tactical->flankTarget : tactical->moveTarget;
                current.hasFlankTarget = tactical->hasFlankTarget;
                current.cover = tactical->reservedCover;
                current.dynamicCover = tactical->dynamicCoverActive;
                current.activeStyle = tactical->activeStyle;
                current.tacticReason = tactical->tacticReason;
                current.status = tactical->status;
                current.transition = tactical->lastTacticalTransition;
                current.noProgressTime = tactical->noProgressTime;
            }

            auto [it, inserted] = s_snapshots.try_emplace(npc, current);
            if (inserted)
            {
                // A newly opened Studio used to create only a silent baseline.
                // Since the simulation starts paused, the history then looked
                // completely broken until some later value happened to change.
                std::ostringstream details;
                std::ostringstream detailsEnglish;
                details << "Состояние: " << RussianStateName(current.state)
                    << " | здоровье: " << std::fixed << std::setprecision(1)
                    << current.health
                    << " | цель: " << static_cast<unsigned>(current.target)
                    << (current.targetVisible ? " видима" : " не видима");
                detailsEnglish << "State: " << NpcStateName(current.state)
                    << " | health: " << std::fixed << std::setprecision(1)
                    << current.health
                    << " | target: " << static_cast<unsigned>(current.target)
                    << (current.targetVisible ? " visible" : " not visible");
                if (current.hasTactical && !current.status.empty())
                {
                    details << " | тактика: " << TranslateRuntimeText(current.status);
                    detailsEnglish << " | tactics: " << current.status;
                }
                PushEvent(timeSeconds, npc, NpcDebugEventCategory::Debug,
                    "NPC добавлен в наблюдение", details.str(),
                    "NPC observation started", detailsEnglish.str());
                return;
            }
            const Snapshot& previous = it->second;

            if (current.hasTactical && previous.hasTactical &&
                previous.activeStyle != current.activeStyle)
            {
                const std::string russianTitle =
                    std::string("Тактика: ") +
                    NpcTacticalStyleNameRu(previous.activeStyle) + " -> " +
                    NpcTacticalStyleNameRu(current.activeStyle);
                const std::string englishTitle =
                    std::string("Tactic: ") +
                    NpcTacticalStyleName(previous.activeStyle) + " -> " +
                    NpcTacticalStyleName(current.activeStyle);
                PushEvent(timeSeconds, npc, NpcDebugEventCategory::Decision,
                    russianTitle,
                    current.tacticReason.empty()
                        ? "Причина не указана"
                        : current.tacticReason,
                    englishTitle,
                    current.tacticReason);
            }

            if (previous.state != current.state)
            {
                std::string reason = current.transition;
                if (reason.empty()) reason = current.status;
                if (reason.empty()) reason = current.blockedReason;
                PushEvent(timeSeconds, npc, NpcDebugEventCategory::Decision,
                    std::string(RussianStateName(previous.state)) + " -> " +
                        RussianStateName(current.state),
                    reason.empty() ? "Причина не сообщена системой" :
                        TranslateRuntimeText(reason),
                    std::string(NpcStateName(previous.state)) + " -> " + NpcStateName(current.state),
                    reason.empty() ? "The system did not report a reason" : reason);
            }
            else if (current.hasTactical && previous.hasTactical &&
                (previous.status != current.status || previous.transition != current.transition) &&
                (!current.status.empty() || !current.transition.empty()))
            {
                // Тактическое решение может смениться внутри того же состояния;
                // без этой строки перебежки TakingCover -> TakingCover не видны.
                std::string details = current.transition;
                if (!current.status.empty())
                {
                    if (!details.empty()) details += " | ";
                    details += current.status;
                }
                PushEvent(timeSeconds, npc, NpcDebugEventCategory::Decision,
                    "Обновлена причина тактического решения",
                    TranslateRuntimeText(details),
                    "Tactical decision reason updated", details);
            }
            if (previous.commandRevision != current.commandRevision)
            {
                PushEvent(timeSeconds, npc, NpcDebugEventCategory::Decision,
                    "Изменена команда NPC",
                    std::string("Команда: ") + RussianCommandName(brain.command.type),
                    "NPC command changed",
                    std::string("Command: ") + NpcCommandName(brain.command.type));
            }
            if (previous.targetVisible != current.targetVisible || previous.target != current.target)
            {
                std::ostringstream details;
                std::ostringstream detailsEnglish;
                details << "Цель " << static_cast<unsigned>(current.target)
                    << (current.targetVisible ? " видима" : " не видима");
                detailsEnglish << "Target " << static_cast<unsigned>(current.target)
                    << (current.targetVisible ? " is visible" : " is not visible");
                PushEvent(timeSeconds, npc, NpcDebugEventCategory::Perception,
                    "Изменился боевой контакт", details.str(),
                    "Combat contact changed", detailsEnglish.str());
            }
            if (current.health + 0.001f < previous.health)
            {
                std::ostringstream details;
                std::ostringstream detailsEnglish;
                details << std::fixed << std::setprecision(1)
                    << "Здоровье: " << previous.health << " -> " << current.health;
                detailsEnglish << std::fixed << std::setprecision(1)
                    << "Health: " << previous.health << " -> " << current.health;
                PushEvent(timeSeconds, npc, NpcDebugEventCategory::Damage,
                    "NPC получил урон", details.str(),
                    "NPC received damage", detailsEnglish.str());
            }
            if (current.hasTactical && previous.hasTactical &&
                (DistanceSquared(previous.goal, current.goal) > 0.0025f ||
                    previous.hasFlankTarget != current.hasFlankTarget))
            {
                std::ostringstream details;
                std::ostringstream detailsEnglish;
                details << std::fixed << std::setprecision(2)
                    << "Новая цель: " << current.goal.x << ", " << current.goal.y << ", " << current.goal.z
                    << " | " << (current.hasFlankTarget ? "фланг" : "обычное перемещение")
                    << " | причина: " << TranslateRuntimeText(current.status);
                detailsEnglish << std::fixed << std::setprecision(2)
                    << "New target: " << current.goal.x << ", " << current.goal.y << ", " << current.goal.z
                    << " | " << (current.hasFlankTarget ? "flank" : "regular movement")
                    << " | reason: " << current.status;
                PushEvent(timeSeconds, npc, NpcDebugEventCategory::Movement,
                    "Изменилась тактическая точка", details.str(),
                    "Tactical target changed", detailsEnglish.str());
            }
            if (previous.cover != current.cover ||
                previous.dynamicCover != current.dynamicCover)
            {
                std::ostringstream details;
                std::ostringstream detailsEnglish;
                details << "Укрытие: " << static_cast<unsigned>(previous.cover)
                    << " -> " << static_cast<unsigned>(current.cover)
                    << (current.dynamicCover ? " (динамическое)" : "")
                    << " | причина: " << TranslateRuntimeText(current.status);
                detailsEnglish << "Cover: " << static_cast<unsigned>(previous.cover)
                    << " -> " << static_cast<unsigned>(current.cover)
                    << (current.dynamicCover ? " (dynamic)" : "")
                    << " | reason: " << current.status;
                PushEvent(timeSeconds, npc, NpcDebugEventCategory::Navigation,
                    "Изменилось зарезервированное укрытие", details.str(),
                    "Reserved cover changed", detailsEnglish.str());
            }
            if (previous.routeIndex != current.routeIndex || previous.routeSize != current.routeSize)
            {
                std::ostringstream details;
                std::ostringstream detailsEnglish;
                details << "Шаг маршрута: " << current.routeIndex << " / " << current.routeSize;
                detailsEnglish << "Route step: " << current.routeIndex << " / " << current.routeSize;
                PushEvent(timeSeconds, npc, NpcDebugEventCategory::Navigation,
                    "Изменился маршрут через двери", details.str(),
                    "Door route changed", detailsEnglish.str());
            }
            if (previous.noProgressTime < 1.0f && current.noProgressTime >= 1.0f)
                PushEvent(timeSeconds, npc, NpcDebugEventCategory::Warning,
                    "Нет прогресса движения", TranslateRuntimeText(current.status),
                    "No movement progress", current.status);
            if (previous.blockedReason != current.blockedReason && !current.blockedReason.empty())
                PushEvent(timeSeconds, npc, NpcDebugEventCategory::Warning,
                    "NPC заблокирован", TranslateRuntimeText(current.blockedReason),
                    "NPC is blocked", current.blockedReason);

            it->second = std::move(current);
        });
    }

    // Записывает всю историю в обычный текст либо JSON. russian выбирает язык
    // текстового экспорта; JSON всегда содержит оба языка.
    static bool ExportHistory(const std::string& path, bool json, bool russian)
    {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        if (!output)
            return false;

        if (!json)
        {
            for (const auto& event : s_events)
            {
                const std::string& title = russian ? event.title : event.titleEnglish;
                const std::string& details = russian ? event.details : event.detailsEnglish;
                output << '[' << std::fixed << std::setprecision(3) << event.timeSeconds << "] "
                    << "NPC " << static_cast<unsigned>(event.npc) << " | "
                    << CategoryName(event.category) << " | " << title;
                if (!details.empty()) output << " | " << details;
                output << '\n';
            }
            return output.good();
        }

        output << "{\n  \"version\": 1,\n  \"events\": [\n";
        for (std::size_t index = 0; index < s_events.size(); ++index)
        {
            const auto& event = s_events[index];
            output << "    {\"sequence\":" << event.sequence
                << ",\"time\":" << std::fixed << std::setprecision(3) << event.timeSeconds
                << ",\"npc\":" << static_cast<unsigned>(event.npc)
                << ",\"category\":\"" << EscapeJson(CategoryName(event.category))
                << "\",\"title\":\"" << EscapeJson(event.title)
                << "\",\"details\":\"" << EscapeJson(event.details)
                << "\",\"title_en\":\"" << EscapeJson(event.titleEnglish)
                << "\",\"details_en\":\"" << EscapeJson(event.detailsEnglish) << "\"}"
                << (index + 1 == s_events.size() ? "\n" : ",\n");
        }
        output << "  ]\n}\n";
        return output.good();
    }

    // Стабильное машинное имя категории для фильтра и JSON.
    static const char* CategoryName(NpcDebugEventCategory category)
    {
        switch (category)
        {
        case NpcDebugEventCategory::Decision: return "decision";
        case NpcDebugEventCategory::Movement: return "movement";
        case NpcDebugEventCategory::Damage: return "damage";
        case NpcDebugEventCategory::Perception: return "perception";
        case NpcDebugEventCategory::Navigation: return "navigation";
        case NpcDebugEventCategory::Warning: return "warning";
        default: return "debug";
        }
    }

private:
    // Runtime-системы пока хранят короткие диагностические строки на
    // английском. Журнал переводит наиболее частые действия централизованно,
    // поэтому игровая логика не зависит от выбранного языка интерфейса.
    static const char* RussianStateName(NpcState state)
    {
        switch (state)
        {
        case NpcState::Idle: return "Ожидание";
        case NpcState::Wandering: return "Брожение";
        case NpcState::MoveToPosition: return "Движение к позиции";
        case NpcState::MoveToRoom: return "Движение в комнату";
        case NpcState::Following: return "Следование";
        case NpcState::MoveToRepair: return "Движение к ремонту";
        case NpcState::WaitingForDoor: return "Ожидание двери";
        case NpcState::Repairing: return "Ремонт";
        case NpcState::Stopped: return "Остановлен";
        case NpcState::Blocked: return "Заблокирован";
        case NpcState::Attacking: return "Атака";
        case NpcState::Chasing: return "Преследование";
        case NpcState::Searching: return "Поиск";
        case NpcState::Patrolling: return "Патрулирование";
        case NpcState::TakingCover: return "Движение к укрытию";
        case NpcState::InCover: return "В укрытии";
        default: return "Неизвестно";
        }
    }

    static const char* RussianCommandName(NpcCommandType command)
    {
        switch (command)
        {
        case NpcCommandType::None: return "Автоматический AI";
        case NpcCommandType::Stop: return "Остановиться";
        case NpcCommandType::Wander: return "Бродить";
        case NpcCommandType::MoveToPosition: return "Идти к позиции";
        case NpcCommandType::MoveToRoom: return "Идти в комнату";
        case NpcCommandType::FollowEntity: return "Следовать за сущностью";
        case NpcCommandType::RepairEntity: return "Ремонтировать сущность";
        default: return "Неизвестная команда";
        }
    }

    static std::string TranslateRuntimeText(const std::string& text)
    {
        if (text.empty()) return {};
        static const std::unordered_map<std::string, std::string> translations{
            { "Cover logic inactive: NPC entity is disabled", "Логика укрытий отключена: NPC неактивен" },
            { "Cover logic inactive: NPC is dead", "Логика укрытий отключена: NPC погиб" },
            { "Cover logic paused by manual command", "Логика укрытий приостановлена ручной командой" },
            { "Cover idle: no active combat target", "Укрытие не требуется: активной боевой цели нет" },
            { "Cover idle: target position is not confirmed", "Укрытие не выбирается: позиция цели не подтверждена" },
            { "Cover idle: selected target is not an enemy", "Укрытие не выбирается: цель не является врагом" },
            { "Returning to cover while reloading", "Возвращаюсь в укрытие для перезарядки" },
            { "Returning behind cover", "Возвращаюсь за укрытие" },
            { "Changing compromised cover", "Меняю раскрытое укрытие" },
            { "Holding cover: out of ammunition", "Остаюсь в укрытии: патроны закончились" },
            { "Reloading behind cover", "Перезаряжаюсь за укрытием" },
            { "Leaving cover for a firing peek", "Резко выхожу из укрытия для стрельбы" },
            { "Current cover no longer protects", "Текущее укрытие больше не защищает" },
            { "Rechecking current cover protection", "Повторно проверяю защиту текущего укрытия" },
            { "A cover ray failed briefly; waiting for confirmation", "Один луч кратко потерял защиту: жду подтверждения" },
            { "Holding firing position: no recent incoming fire", "Удерживаю огневую позицию: недавнего обстрела нет" },
            { "Waiting before next cover evaluation", "Жду следующей оценки укрытий" },
            { "No suitable individual cover", "Подходящее одиночное укрытие не найдено" },
            { "Moving to replacement cover", "Перемещаюсь к новому укрытию" },
            { "Moving to dynamically discovered cover", "Перемещаюсь к динамически найденному укрытию" },
            { "Moving to individual cover", "Перемещаюсь к личному укрытию" },
            { "Hit while exposed", "Получил попадание во время выглядывания" },
            { "Exposure timer expired", "Время открытой фазы истекло" },
            { "Magazine emptied during exposure", "Магазин опустел во время выглядывания" },
            { "Two exposed peeks ended with incoming damage", "Два выглядывания завершились попаданием" },
            { "Hide timer expired; moving to peek position", "Время за укрытием истекло: выхожу к краю" },
            { "Enemy position changed or cover geometry no longer blocks fire", "Враг сместился или геометрия больше не перекрывает огонь" },
            { "Cover decision cooldown is active", "Активна пауза перед новой оценкой укрытий" },
            { "Every cover point failed protection, peek sight, or path validation", "Все укрытия отклонены проверкой защиты, обзора или пути" },
            { "No authored point was suitable; NavMesh and ray tests found cover", "Авторские точки не подошли; укрытие найдено по NavMesh и лучам" },
            { "Recent incoming fire triggered a valid cover selection", "Недавний обстрел запустил выбор подходящего укрытия" },
            { "Repeated exposed hits triggered a safer position search", "Повторные попадания запустили поиск более безопасной позиции" },
            { "Exposed: firing from cover edge", "Открыт: веду огонь от края укрытия" },
            { "Protected: waiting for next peek", "Защищён: жду следующего выглядывания" },
            { "Cover route blocked; replanning", "Маршрут к укрытию заблокирован: перестраиваю" },
            { "Moving to final target", "Двигаюсь к конечной точке" },
            { "Combat NPC: crew utility disabled", "Боевой NPC: задачи экипажа отключены" }
        };
        if (const auto it = translations.find(text); it != translations.end())
            return it->second;

        // Некоторые события объединяют причину и статус через разделитель.
        // Переводим каждую половину, не ломая неизвестный диагностический текст.
        const std::size_t separator = text.find(" | ");
        if (separator != std::string::npos)
        {
            return TranslateRuntimeText(text.substr(0, separator)) + " | " +
                TranslateRuntimeText(text.substr(separator + 3));
        }
        return text;
    }

    // Минимальный снимок NPC между кадрами. Студия сравнивает два Snapshot и
    // пишет событие только при реальном изменении, а не каждый кадр.
    struct Snapshot
    {
        // Исполняемое состояние и версия ручной команды.
        NpcState state = NpcState::Idle;
        std::uint32_t commandRevision = 0;
        // Боевая цель и результат личной проверки видимости.
        Entity target = INVALID_ENTITY;
        bool targetVisible = false;
        // Здоровье берётся из NPCComponent либо DamageableComponent.
        float health = 0.0f;
        // Есть ли NpcTacticalComponent и какова его текущая цель движения.
        bool hasTactical = false;
        DirectX::XMFLOAT3 goal{};
        bool hasFlankTarget = false;
        // Зарезервированная авторская точка укрытия.
        Entity cover = INVALID_ENTITY;
        bool dynamicCover = false;
        NpcTacticalStyle activeStyle = NpcTacticalStyle::Balanced;
        std::string tacticReason;
        // Тексты объясняют решение и блокировку без подключения отладчика.
        std::string status;
        std::string transition;
        std::string blockedReason;
        // Положение в маршруте через двери.
        std::size_t routeIndex = 0;
        std::size_t routeSize = 0;
        // Таймер детектора застревания тактического движения.
        float noProgressTime = 0.0f;
    };

    // Квадрат расстояния без дорогого sqrt; нужен лишь порог изменения точки.
    static float DistanceSquared(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b)
    {
        const float x = a.x - b.x;
        const float y = a.y - b.y;
        const float z = a.z - b.z;
        return x * x + y * y + z * z;
    }

    // Экранирует служебные символы перед ручной записью JSON-строки.
    static std::string EscapeJson(const std::string& text)
    {
        std::string result;
        result.reserve(text.size());
        for (const char ch : text)
        {
            switch (ch)
            {
            case '\\': result += "\\\\"; break;
            case '"': result += "\\\""; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default: result += ch; break;
            }
        }
        return result;
    }

    inline static constexpr std::size_t MaxEvents = 2000; // Предел памяти журнала.
    inline static std::deque<NpcDebugEvent> s_events; // Старые записи удаляются спереди.
    inline static std::unordered_map<Entity, Snapshot> s_snapshots; // Прошлый кадр каждого NPC.
    inline static std::uint64_t s_nextSequence = 1; // Следующий уникальный номер события.
};
