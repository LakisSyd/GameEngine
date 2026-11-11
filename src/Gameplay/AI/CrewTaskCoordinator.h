#pragma once

#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_set>
#include <vector>

#include <ECSRegistry.h>
#include <Components.h>

#include "../../../NPCComponents.h"
#include "Gameplay/Power/GeneratorComponent.h"
#include "Gameplay/Power/OxygenGeneratorComponent.h"
#include "Gameplay/Repair/RepairComponent.h"

// Экипаж существует постоянно, а не только когда игрок смотрит на него.
// Координатор распределяет реальные аварии между свободными мирными NPC.
enum class CrewRole
{
    Generalist, // Универсал: нейтральный коэффициент любой работы.
    Engineer,   // Лучший ремонтник: максимальный бонус к repair score.
    Operations, // Особенно охотно чинит генераторы и кислородное оборудование.
    Security,   // Может чинить, но предпочитается после техников.
    Medic       // Пока лечить некого; ремонт получает самый маленький бонус.
};

// Английское имя роли для статуса и инспектора.
inline const char* CrewRoleName(CrewRole role)
{
    switch (role)
    {
    case CrewRole::Engineer: return "Engineer";
    case CrewRole::Operations: return "Operations";
    case CrewRole::Security: return "Security";
    case CrewRole::Medic: return "Medic";
    default: return "Generalist";
    }
}

enum class CrewTaskType
{
    None,            // Назначения нет.
    RepairBreach,    // Заделать HullBreachComponent.
    RepairEquipment  // Восстановить объект с Damageable + Repairable.
};

// Человекочитаемое имя вида работы.
inline const char* CrewTaskTypeName(CrewTaskType type)
{
    switch (type)
    {
    case CrewTaskType::RepairBreach: return "Seal hull breach";
    case CrewTaskType::RepairEquipment: return "Repair equipment";
    default: return "No assignment";
    }
}

struct CrewMemberComponent
{
    // Профессия влияет на вес задач, но не запрещает их полностью.
    CrewRole role = CrewRole::Generalist;
    // false исключает NPC из автоматического распределения работ.
    bool onDuty = true;
    // Множитель скорости и привлекательности ремонта: безопасный диапазон 0.25..2.
    float repairSkill = 1.0f;

    // Runtime-состояние назначения. В сцену сохраняются role/onDuty/skill, а
    // активные заявки строятся заново из текущих аварий после запуска Play.
    Entity assignedTask = INVALID_ENTITY;
    // Мировое время выдачи текущей работы — полезно для диагностики очереди.
    double assignedAtSeconds = 0.0;
    // Сколько работ NPC успешно завершил за текущий запуск.
    unsigned int completedTasks = 0;
};

// Метка хранится НА САМОЙ цели ремонта. Поэтому любой AI/debug-инструмент видит,
// кто занял работу, а скрытая глобальная таблица назначений не нужна.
struct CrewTaskClaimComponent
{
    // NPC-владелец заявки.
    Entity assignee = INVALID_ENTITY;
    // Что именно он должен сделать.
    CrewTaskType type = CrewTaskType::None;
    // Score на момент назначения: больше = задача была важнее.
    float priority = 0.0f;
    // Когда заявка была создана, в секундах мира.
    double claimedAtSeconds = 0.0;
};

// Короткая телеметрия последнего Update для HUD/отладочной панели.
struct CrewTaskCoordinatorSnapshot
{
    // Мирные активные дежурные NPC без ручной команды.
    int availableCrew = 0;
    // Из них имеют сохранённую или только что выданную работу.
    int busyCrew = 0;
    // Количество незавершённых Repairable-целей.
    int openTasks = 0;
    // Сколько новых назначений произошло именно на этом кадре.
    int assignmentsThisFrame = 0;
};

class CrewTaskCoordinator
{
public:
    // Главный проход распределения задач. Его нужно вызывать до Utility/State,
    // чтобы brain.repairTarget был готов к исполнению в том же кадре.
    // worldTime нужен лишь для временной метки назначения, не для движения.
    static void Update(ECSRegistry& registry, double worldTime)
    {
        s_snapshot = {};

        CleanupInvalidClaims(registry);

        std::vector<Entity> candidates;
        CollectRepairCandidates(registry, candidates);
        s_snapshot.openTasks = static_cast<int>(candidates.size());

        std::unordered_set<Entity> claimedTargets;
        for (auto& [target, claim] : registry.GetComponents<CrewTaskClaimComponent>())
        {
            if (IsRepairTargetValid(registry, target) &&
                claim.assignee != INVALID_ENTITY)
            {
                claimedTargets.insert(target);
            }
        }

        std::vector<Entity> crew;
        registry.ForEach<NPCComponent, NpcBrainComponent>(
            [&](Entity entity, NPCComponent&, NpcBrainComponent&)
            {
                if (registry.HasComponent<ActiveComponent>(entity))
                    crew.push_back(entity);
            });
        std::sort(crew.begin(), crew.end());

        for (Entity npc : crew)
        {
            auto* npcData = registry.GetComponent<NPCComponent>(npc);
            auto* brain = registry.GetComponent<NpcBrainComponent>(npc);
            if (!npcData || !brain)
                continue;

            auto* member = registry.GetComponent<CrewMemberComponent>(npc);
            if (!member)
            {
                registry.AddComponent<CrewMemberComponent>(npc, CrewMemberComponent{});
                member = registry.GetComponent<CrewMemberComponent>(npc);
            }
            if (!member)
                continue;

            if (npcData->isHostile || !member->onDuty || brain->command.IsActive())
            {
                const bool manualCommandActive = brain->command.IsActive();
                ReleaseAssignment(registry, npc, *member);
                if (!manualCommandActive)
                    ClearAutomaticRepairState(*member, *brain);
                continue;
            }

            ++s_snapshot.availableCrew;

            if (KeepExistingAssignment(registry, npc, *member, *brain))
            {
                ++s_snapshot.busyCrew;
                continue;
            }

            Entity bestTarget = INVALID_ENTITY;
            float bestScore = brain->idleScore;

            for (Entity target : candidates)
            {
                if (claimedTargets.contains(target))
                    continue;

                const float score = CalculateAssignmentScore(
                    registry, npc, target, *member, *brain);
                if (score > bestScore ||
                    (score == bestScore && target < bestTarget))
                {
                    bestScore = score;
                    bestTarget = target;
                }
            }

            if (bestTarget == INVALID_ENTITY)
            {
                ClearAutomaticRepairState(*member, *brain);
                continue;
            }

            const CrewTaskType taskType =
                registry.HasComponent<HullBreachComponent>(bestTarget)
                ? CrewTaskType::RepairBreach
                : CrewTaskType::RepairEquipment;

            CrewTaskClaimComponent claim;
            claim.assignee = npc;
            claim.type = taskType;
            claim.priority = bestScore;
            claim.claimedAtSeconds = worldTime;
            registry.AddComponent<CrewTaskClaimComponent>(bestTarget, claim);

            member->assignedTask = bestTarget;
            member->assignedAtSeconds = worldTime;
            brain->repairTarget = bestTarget;
            brain->routeDirty = true;
            if (brain->state != NpcState::Repairing)
                brain->state = NpcState::MoveToRepair;
            brain->status = std::string("Crew assignment: ") +
                CrewTaskTypeName(taskType);

            claimedTargets.insert(bestTarget);
            ++s_snapshot.busyCrew;
            ++s_snapshot.assignmentsThisFrame;
        }
    }

    // Полная проверка, что цель существует, активна, ещё сломана и допускает
    // ремонт. Используется и при назначении, и при очистке старых заявок.
    static bool IsRepairTargetValid(ECSRegistry& registry, Entity target)
    {
        if (target == INVALID_ENTITY || !registry.IsAlive(target))
            return false;
        if (!registry.HasComponent<ActiveComponent>(target))
            return false;

        const auto* repairable = registry.GetComponent<RepairableComponent>(target);
        if (!repairable || repairable->repaired)
            return false;

        if (const auto* breach = registry.GetComponent<HullBreachComponent>(target))
            return !breach->sealed;

        if (const auto* damageable = registry.GetComponent<DamageableComponent>(target))
            return damageable->health < damageable->maxHealth;

        return false;
    }

    // true, если живой другой NPC уже владеет заявкой на эту цель.
    static bool IsClaimedByOther(
        ECSRegistry& registry,
        Entity target,
        Entity npc)
    {
        const auto* claim = registry.GetComponent<CrewTaskClaimComponent>(target);
        return claim && claim->assignee != INVALID_ENTITY &&
            claim->assignee != npc && registry.IsAlive(claim->assignee);
    }

    // Только чтение телеметрии последнего кадра.
    static const CrewTaskCoordinatorSnapshot& GetSnapshot()
    {
        return s_snapshot;
    }

private:
    // Собирает все валидные цели ремонта и сортирует Entity для стабильного,
    // воспроизводимого выбора при одинаковых score.
    static void CollectRepairCandidates(
        ECSRegistry& registry,
        std::vector<Entity>& result)
    {
        result.clear();
        for (auto& [entity, repairable] : registry.GetComponents<RepairableComponent>())
        {
            (void)repairable;
            if (IsRepairTargetValid(registry, entity))
                result.push_back(entity);
        }
        std::sort(result.begin(), result.end());
    }

    // Удаляет заявки, если цель уже починена либо исполнитель умер, выключен,
    // снят с дежурства или получил ручной приказ.
    static void CleanupInvalidClaims(ECSRegistry& registry)
    {
        std::vector<Entity> remove;
        for (auto& [target, claim] : registry.GetComponents<CrewTaskClaimComponent>())
        {
            const auto* brain = registry.GetComponent<NpcBrainComponent>(claim.assignee);
            auto* member = registry.GetComponent<CrewMemberComponent>(claim.assignee);
            const bool ownerCanContinue =
                claim.assignee != INVALID_ENTITY &&
                registry.IsAlive(claim.assignee) && brain && member &&
                registry.HasComponent<ActiveComponent>(claim.assignee) &&
                member->onDuty && !brain->command.IsActive();

            if (!IsRepairTargetValid(registry, target) || !ownerCanContinue)
            {
                if (member && member->assignedTask == target)
                {
                    if (!IsRepairTargetValid(registry, target))
                        ++member->completedTasks;
                    member->assignedTask = INVALID_ENTITY;
                }
                remove.push_back(target);
            }
        }

        for (Entity target : remove)
            registry.RemoveComponent<CrewTaskClaimComponent>(target);
    }

    // Освобождает занятую NPC цель, но не принимает новую задачу.
    static void ReleaseAssignment(
        ECSRegistry& registry,
        Entity npc,
        CrewMemberComponent& member)
    {
        if (member.assignedTask != INVALID_ENTITY)
        {
            if (auto* claim = registry.GetComponent<CrewTaskClaimComponent>(member.assignedTask);
                claim && claim->assignee == npc)
            {
                registry.RemoveComponent<CrewTaskClaimComponent>(member.assignedTask);
            }
        }
        member.assignedTask = INVALID_ENTITY;
    }

    // Очищает только автоматический ремонт и возвращает соответствующие
    // состояния в Idle. Ручная команда при этом не стирается.
    static void ClearAutomaticRepairState(
        CrewMemberComponent& member,
        NpcBrainComponent& brain)
    {
        member.assignedTask = INVALID_ENTITY;
        brain.repairTarget = INVALID_ENTITY;

        if (brain.state == NpcState::MoveToRepair ||
            brain.state == NpcState::Repairing ||
            brain.state == NpcState::WaitingForDoor)
        {
            brain.state = NpcState::Idle;
            brain.routeDirty = true;
            brain.status = "Automatic task: waiting for crew assignment";
        }
    }

    // Пытается продолжить прежнее валидное назначение. Возвращает true, если
    // новая оценка кандидатов этому NPC сейчас не нужна.
    static bool KeepExistingAssignment(
        ECSRegistry& registry,
        Entity npc,
        CrewMemberComponent& member,
        NpcBrainComponent& brain)
    {
        if (!IsRepairTargetValid(registry, member.assignedTask))
        {
            member.assignedTask = INVALID_ENTITY;
            return false;
        }

        const auto* claim =
            registry.GetComponent<CrewTaskClaimComponent>(member.assignedTask);
        if (!claim || claim->assignee != npc)
        {
            member.assignedTask = INVALID_ENTITY;
            return false;
        }

        if (brain.repairTarget != member.assignedTask)
        {
            brain.repairTarget = member.assignedTask;
            brain.routeDirty = true;
        }
        if (brain.state != NpcState::Repairing)
            brain.state = NpcState::MoveToRepair;
        brain.status = std::string("Crew assignment: ") +
            CrewTaskTypeName(claim->type);
        return true;
    }

    // Оценка пары «этот NPC -> эта задача». Учитывает серьёзность аварии,
    // расстояние, профессию и repairSkill. Ноль означает недоступную задачу.
    static float CalculateAssignmentScore(
        ECSRegistry& registry,
        Entity npc,
        Entity target,
        const CrewMemberComponent& member,
        const NpcBrainComponent& brain)
    {
        const auto* npcTransform = registry.GetComponent<TransformComponent>(npc);
        if (!npcTransform)
            return 0.0f;

        DirectX::XMFLOAT3 targetPosition{};
        float severity = 0.5f;
        if (const auto* breach = registry.GetComponent<HullBreachComponent>(target))
        {
            targetPosition = breach->worldPosition;
            severity = std::clamp(breach->leakRate / 0.15f, 0.2f, 1.0f);
        }
        else
        {
            const auto* transform = registry.GetComponent<TransformComponent>(target);
            const auto* damageable = registry.GetComponent<DamageableComponent>(target);
            if (!transform || !damageable)
                return 0.0f;
            targetPosition = transform->position;
            severity = damageable->maxHealth > 0.0f
                ? 1.0f - std::clamp(
                    damageable->health / damageable->maxHealth, 0.0f, 1.0f)
                : 1.0f;
        }

        const float dx = targetPosition.x - npcTransform->position.x;
        const float dy = targetPosition.y - npcTransform->position.y;
        const float dz = targetPosition.z - npcTransform->position.z;
        const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (brain.detectionRange > 0.0f && distance > brain.detectionRange)
            return 0.0f;

        const float distanceScore = brain.detectionRange > 0.0f
            ? 1.0f - std::clamp(distance / brain.detectionRange, 0.0f, 1.0f)
            : 1.0f;

        float roleAffinity = 0.90f;
        switch (member.role)
        {
        case CrewRole::Engineer: roleAffinity = 1.25f; break;
        case CrewRole::Operations: roleAffinity = 1.05f; break;
        case CrewRole::Security: roleAffinity = 0.72f; break;
        case CrewRole::Medic: roleAffinity = 0.62f; break;
        default: break;
        }

        if (member.role == CrewRole::Operations &&
            (registry.HasComponent<GeneratorComponent>(target) ||
                registry.HasComponent<OxygenGeneratorComponent>(target)))
        {
            roleAffinity += 0.15f;
        }

        const float skill = std::clamp(member.repairSkill, 0.25f, 2.0f);
        return std::clamp(
            (severity * 0.65f + distanceScore * 0.35f) * roleAffinity * skill,
            0.0f,
            2.0f);
    }

    // Общий снимок только последнего Update; на каждом кадре обнуляется.
    inline static CrewTaskCoordinatorSnapshot s_snapshot{};
};
