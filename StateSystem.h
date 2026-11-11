#pragma once

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <queue>
#include <string>
#include <unordered_map>

#include <ECSRegistry.h>
#include "NPCComponents.h"
#include "NpcCommandSystem.h"
#include "CharacterSpawnSystem.h"
#include "Gameplay/Power/PowerConsumerComponent.h"
#include "Gameplay/AI/NpcNavMeshSystem.h"
#include "Gameplay/Repair/RepairComponent.h"
#include "Gameplay/Repair/RepairSystem.h"
#include "AnimationComponent.h"
#include "JobSystem.h"

// =============================================================================
// StateSystem — исполнитель NPC AI ("как выполнить выбранную задачу?").
// =============================================================================
//
// Этот файл большой не потому, что NPC "думает" здесь обо всём, а потому что
// здесь сходятся несколько исполнительных подсистем:
//
//   NpcCommand / NpcUtilitySystem
//                |
//                v
//         NpcState и цель
//                |
//                v
//     маршрут комнат через двери (BFS)
//                |
//                v
//   CharacterMovement.walkDirection/currentSpeed
//          |                         |
//          v                         v
// CharacterControllerSystem    AnimationController
//   двигает Bullet capsule       включает Idle/Walk/Run
//
// ТЕКУЩАЯ НАВИГАЦИЯ ДВУХУРОВНЕВАЯ:
//
//   1. Между комнатами строится корректный маршрут по графу дверей.
//   2. Внутри комнаты NPC идёт по прямой к двери или конечной точке.
//
// Поэтому NavMesh-библиотека сейчас НЕ обязательна. Она понадобится, когда
// внутри комнат появятся сложные неподвижные препятствия, вокруг которых нельзя
// пройти по прямой. Место подключения локального NavMesh — NavigateTo(): вместо
// одного MoveTowards можно запросить следующую точку локального пути. Utility AI,
// команды, граф дверей и ремонт при этом останутся без изменений.


static DirectX::XMFLOAT3 GetClosestPointInWorkZone(
    const DirectX::XMFLOAT3& worldPosition,
    const TransformComponent& zoneTransform)
{
    const DirectX::XMMATRIX worldToLocal =
        DirectX::XMMatrixInverse(
            nullptr,
            zoneTransform.GetWorldMatrix());

    const DirectX::XMVECTOR localVector =
        DirectX::XMVector3TransformCoord(
            DirectX::XMLoadFloat3(
                &worldPosition),
            worldToLocal);

    DirectX::XMFLOAT3 local{};
    DirectX::XMStoreFloat3(
        &local,
        localVector);

    // Empty Cube трактуем как [-1..1].
    local.x =
        std::clamp(
            local.x,
            -1.0f,
            1.0f);

    local.y =
        std::clamp(
            local.y,
            -1.0f,
            1.0f);

    local.z =
        std::clamp(
            local.z,
            -1.0f,
            1.0f);

    const DirectX::XMVECTOR worldVector =
        DirectX::XMVector3TransformCoord(
            DirectX::XMLoadFloat3(
                &local),
            zoneTransform.GetWorldMatrix());

    DirectX::XMFLOAT3 result{};

    DirectX::XMStoreFloat3(
        &result,
        worldVector);

    return result;
}

static DirectX::XMFLOAT3 TransformWorkZoneLocalPoint(
    const TransformComponent& zoneTransform,
    const DirectX::XMFLOAT3& localPoint)
{
    const DirectX::XMVECTOR worldVector =
        DirectX::XMVector3TransformCoord(
            DirectX::XMLoadFloat3(&localPoint),
            zoneTransform.GetWorldMatrix());

    DirectX::XMFLOAT3 result{};
    DirectX::XMStoreFloat3(&result, worldVector);
    return result;
}

static bool TryPickReachableWorkZonePoint(
    ECSRegistry& registry,
    Entity repairRoom,
    const DirectX::XMFLOAT3& npcPosition,
    const TransformComponent& zoneTransform,
    DirectX::XMFLOAT3& outPoint)
{
    std::vector<DirectX::XMFLOAT3> candidates;
    candidates.reserve(10);

    candidates.push_back(GetClosestPointInWorkZone(npcPosition, zoneTransform));
    candidates.push_back(zoneTransform.position);

    constexpr float kLocalY = 0.0f;
    constexpr DirectX::XMFLOAT3 kLocalSamples[] =
    {
        { -1.0f, kLocalY, -1.0f },
        {  1.0f, kLocalY, -1.0f },
        { -1.0f, kLocalY,  1.0f },
        {  1.0f, kLocalY,  1.0f },
        {  0.0f, kLocalY, -1.0f },
        {  0.0f, kLocalY,  1.0f },
        { -1.0f, kLocalY,  0.0f },
        {  1.0f, kLocalY,  0.0f },
    };

    for (const DirectX::XMFLOAT3& localPoint : kLocalSamples)
        candidates.push_back(TransformWorkZoneLocalPoint(zoneTransform, localPoint));

    bool found = false;
    float bestDistance = (std::numeric_limits<float>::max)();
    DirectX::XMFLOAT3 bestPoint{};

    for (const DirectX::XMFLOAT3& candidate : candidates)
    {
        DirectX::XMFLOAT3 reachablePoint{};
        const bool projected =
            repairRoom != INVALID_ENTITY &&
            NpcNavMeshSystem::FindReachablePoint(
                registry,
                repairRoom,
                npcPosition,
                candidate,
                reachablePoint);

        const DirectX::XMFLOAT3 point = projected ? reachablePoint : candidate;

        if (!projected && repairRoom != INVALID_ENTITY)
            continue;

        const float distance =
            std::sqrt(
                (point.x - npcPosition.x) * (point.x - npcPosition.x) +
                (point.z - npcPosition.z) * (point.z - npcPosition.z));

        if (!found || distance < bestDistance)
        {
            found = true;
            bestDistance = distance;
            bestPoint = point;
        }
    }

    if (!found)
        return false;

    outPoint = bestPoint;
    return true;
}



class StateSystem
{
public:
    // Главный update исполнителя. Вызывается один раз в Play-кадре после
    // NpcUtilitySystem и перед CharacterControllerSystem/AnimationController.
    static void Update(ECSRegistry& registry, float deltaTime, JobSystem& jobs)
    {
        // Нулевая/отрицательная deltaTime бывает на паузе. Не двигаем таймеры и
        // не добавляем прогресс ремонта в такой кадр.
        if (deltaTime <= 0.0f)
            return;

        // Для исполнения нужны мозг и мировая позиция. NPCComponent здесь не
        // запрашивается специально: StateSystem работает с любой Entity-марионеткой,
        // которой дали NpcBrainComponent и TransformComponent.
        registry.ForEach<NpcBrainComponent, TransformComponent>(
            [&](Entity npc, NpcBrainComponent& brain, TransformComponent& transform)
            {
                // One Entity cannot be controlled by both a Bullet rigid body
                // and btKinematicCharacterController. Editor mesh objects often
                // already have RigidBodyComponent when NPC AI is added; leaving
                // it attached makes both physics owners overwrite one Transform
                // every frame and the NPC visibly shakes even while standing.
                // RigidBodyComponent's destructor safely unregisters its body.
                if (registry.HasComponent<RigidBodyComponent>(npc))
                {
                    registry.RemoveComponent<RigidBodyComponent>(npc);
                    OutputDebugStringA(
                        "[NPC] Removed conflicting RigidBodyComponent; "
                        "character capsule is the sole physics owner.\n");
                }

                // Эти компоненты нужны нижнему уровню движения. Editor может
                // добавить только "NPC AI Components", поэтому недостающие
                // runtime-компоненты безопасно создаются автоматически.
                auto* movement = EnsureComponent<CharacterMovement>(registry, npc);
                auto* controller = EnsureComponent<CharacterController>(registry, npc);
                auto* npcMovement = EnsureComponent<NPCMovementComponent>(registry, npc);
                if (!movement || !controller || !npcMovement)
                    return;

                // CharacterController состоит из трёх связанных Bullet-объектов.
                // "Пустой" контроллер можно заспавнить. Частично созданный нельзя
                // пересоздавать поверх старого — ждём, чтобы не получить утечку или
                // два ghost object для одной Entity.
                const bool controllerReady =
                    controller->controller && controller->ghostObj && controller->capsuleShape;
                const bool controllerEmpty =
                    !controller->controller && !controller->ghostObj && !controller->capsuleShape;

                if (controllerEmpty)
                    CharacterSpawnSystem::Spawn(registry, npc, transform.position);
                else if (!controllerReady)
                    return;

                // Переносим удобные AI-настройки в общий CharacterMovement. Дальше
                // один и тот же controller/facing/animation pipeline работает и
                // для игрока, и для NPC.
                movement->moveSpeed = (std::max)(0.0f, npcMovement->speed);
                movement->runSpeed = (std::max)(movement->moveSpeed, npcMovement->runSpeed);
                // Если модель умеет анимации, но компонент ещё не добавлен,
                // создаём его. Конкретные названия Idle/Walk/Run клипов задаются
                // в Animation Bindings инспектора.
                if (auto* mesh = registry.GetComponent<MeshRendererComponent>(npc))
                {
                    if (mesh->model && mesh->model->HasAnimations() &&
                        !registry.HasComponent<AnimationComponent>(npc))
                    {
                        registry.AddComponent<AnimationComponent>(npc, AnimationComponent{});
                    }
                }
                // Комната определяется каждый кадр по позиции. Это позволяет
                // заметить момент прохода через дверь без отдельного trigger volume.
                // Door volumes may overlap at the threshold. Prefer the room we
                // already occupied until the capsule has genuinely left it;
                // unordered ECS iteration must not flip currentRoom every frame.
                brain.currentRoom = FindContainingRoom(
                    registry, transform.position, brain.currentRoom);

                // Сначала выдаём желаемое движение/ремонт, затем независимо
                // проверяем, действительно ли физический controller продвинулся.
                brain.portalCollisionBypassRequestedThisFrame = false;
                UpdateNpc(registry, npc, brain, transform, *movement, *npcMovement, deltaTime, jobs);
                if (brain.portalCollisionIgnoredDoor != INVALID_ENTITY &&
                    !brain.portalCollisionBypassRequestedThisFrame)
                {
                    SetPortalCollisionBypass(
                        registry,
                        npc,
                        brain,
                        brain.portalCollisionIgnoredDoor,
                        false);
                }
                UpdateStuckStatus(npc, brain, transform.position, *movement, deltaTime);
            });
    }

private:
    // Результат одного шага NavigateTo. Он не хранится в компоненте, потому что
    // нужен только вызывающему состоянию в текущем кадре.
    enum class NavigationResult
    {
        Moving,
        Arrived,
        Waiting,
        Blocked
    };

    // Служебная запись BFS. Для каждой посещённой комнаты помним, из какой комнаты
    // пришли и через какую дверь. Потом раскручиваем цепочку от цели назад.
    struct RouteNode
    {
        Entity previousRoom = INVALID_ENTITY;
        Entity viaDoor = INVALID_ENTITY;
    };

    template<typename T>
    static T* EnsureComponent(ECSRegistry& registry, Entity entity)
    {
        // Универсальный ECS helper: вернуть существующий компонент или добавить
        // компонент со значениями по умолчанию. После AddComponent указатель
        // запрашивается заново, потому что storage мог перераспределиться.
        auto* component = registry.GetComponent<T>(entity);
        if (!component)
        {
            registry.AddComponent<T>(entity, T{});
            component = registry.GetComponent<T>(entity);
        }
        return component;
    }

    static void UpdateNpc(
        ECSRegistry& registry,
        Entity npc,
        NpcBrainComponent& brain,
        const TransformComponent& transform,
        CharacterMovement& movement,
        const NPCMovementComponent& npcMovement,
        float deltaTime,
        JobSystem& jobs)
    {
        // ГЛАВНОЕ ПРАВИЛО ПРИОРИТЕТА:
        // если есть ручная команда — никакое автоматическое состояние ниже даже
        // не рассматривается. return гарантирует, что ремонт не "перетянет" NPC
        // обратно к пробоине посреди debug-команды.
        if (brain.command.IsActive())
        {
            UpdateManualCommand(registry, npc, brain, transform, movement, npcMovement, deltaTime, jobs);
            return;
        }

        // Ручной команды нет: исполняем состояние, выбранное Utility AI.
        // WaitingForDoor обрабатывается тем же UpdateRepairTask, потому что после
        // открытия двери нужно продолжить ту же задачу, а не создавать отдельную.
        switch (brain.state)
        {
        case NpcState::MoveToRepair:
        case NpcState::Repairing:
        case NpcState::WaitingForDoor:
            UpdateRepairTask(
                registry, npc, brain, transform, movement, npcMovement,
                brain.repairTarget, false, deltaTime, jobs);
            break;

        default:
            brain.state = NpcState::Idle;
            StopMovement(movement);
            break;
        }
    }

    static void UpdateManualCommand(
        ECSRegistry& registry,
        Entity npc,
        NpcBrainComponent& brain,
        const TransformComponent& transform,
        CharacterMovement& movement,
        const NPCMovementComponent& npcMovement,
        float deltaTime,
        JobSystem& jobs)
    {
        // Каждая ветка делает один небольшой шаг команды за текущий кадр.
        // Функция вызывается снова на следующем кадре, пока command.type не будет
        // очищен. Это обычная конечная машина состояний, а не блокирующий цикл.
        switch (brain.command.type)
        {
        case NpcCommandType::Stop:
            // Stop — постоянная команда: command здесь намеренно НЕ очищается.
            brain.state = NpcState::Stopped;
            brain.status = "Manual command: stopped";
            StopMovement(movement);
            return;

        case NpcCommandType::Wander:
            // Wander тоже постоянный, но внутри сам меняет случайные waypoint.
            UpdateWander(registry, npc, brain, transform, movement, npcMovement, deltaTime);
            return;

        case NpcCommandType::MoveToPosition:
        {
            // Мировая точка может лежать в другой комнате. Если удаётся определить
            // её RoomComponent, NavigateTo сначала проведёт NPC через двери.
            brain.state = NpcState::MoveToPosition;
            const Entity targetRoom = FindContainingRoom(registry, brain.command.worldPosition);
            const NavigationResult result = NavigateTo(
                registry, npc, brain, transform, movement, npcMovement,
                brain.command.worldPosition, targetRoom, npcMovement.arrivalDistance,
                NpcState::MoveToPosition, deltaTime);
            if (result == NavigationResult::Arrived)
                // Одноразовая команда сама снимает ручной приоритет по прибытии.
                CompleteManualCommand(brain, movement, "Reached debug world position");
            return;
        }

        case NpcCommandType::MoveToRoom:
        {
            const Entity room = brain.command.targetRoom;
            DirectX::XMFLOAT3 roomCenter{};
            // Центр берётся из local bounds комнаты и переводится её world matrix.
            // Это работает и для повёрнутой/масштабированной комнаты.
            if (!TryGetRoomCenter(registry, room, roomCenter))
            {
                SetBlocked(brain, movement, "Selected room does not exist or has no Transform");
                return;
            }

            brain.state = NpcState::MoveToRoom;
            const NavigationResult result = NavigateTo(
                registry, npc, brain, transform, movement, npcMovement,
                roomCenter, room, (std::max)(npcMovement.arrivalDistance, 0.75f),
                NpcState::MoveToRoom, deltaTime);
            // Room bounds can overlap the physical doorway. Completing as soon
            // as currentRoom changes leaves the capsule standing on the sill.
            // Let NavigateTo finish clearing the portal and reach a safe point
            // inside the selected room before stopping the one-shot command.
            if (result == NavigationResult::Arrived)
                CompleteManualCommand(brain, movement, "Reached selected room");
            return;
        }

        case NpcCommandType::FollowEntity:
        {
            // Transform цели читается каждый кадр: follow работает для движущегося
            // игрока/NPC и автоматически замечает переход цели в другую комнату.
            const auto* targetTransform =
                registry.GetComponent<TransformComponent>(brain.command.targetEntity);
            if (!targetTransform)
            {
                SetBlocked(brain, movement, "Follow target is missing Transform");
                return;
            }

            // 1.5f — дистанция построения, чтобы NPC не пытался встать строго в
            // центр capsule цели и не толкал её физикой.
            brain.state = NpcState::Following;
            const Entity targetRoom = FindContainingRoom(registry, targetTransform->position);
            const NavigationResult result = NavigateTo(
                registry, npc, brain, transform, movement, npcMovement,
                targetTransform->position, targetRoom, 1.5f,
                NpcState::Following, deltaTime);
            if (result == NavigationResult::Arrived)
            {
                // Follow не завершается. NPC ждёт рядом; если цель уйдёт, следующий
                // кадр снова даст NavigationResult::Moving.
                StopMovement(movement);
                brain.status = "Following: holding formation";
            }
            return;
        }

        case NpcCommandType::RepairEntity:
            // manualCommand=true сообщает UpdateRepairTask, что по завершении надо
            // очистить NpcCommand, а не только автоматический repairTarget.
            UpdateRepairTask(
                registry, npc, brain, transform, movement, npcMovement,
                brain.command.targetEntity, true, deltaTime, jobs);
            return;

        default:
            // Защита от неизвестного/повреждённого типа команды: возвращаемся в
            // безопасный automatic idle вместо продолжения со старыми данными.
            NpcCommandSystem::ResumeAutomatic(brain);
            StopMovement(movement);
            return;
        }
    }


    static Entity FindRepairZoneForOwner(
        ECSRegistry& registry,
        Entity owner)
    {
        char buffer[512]{};

        const auto* requestedName =
            registry.GetComponent<NameComponent>(owner);

        sprintf_s(
            buffer,
            "\n[WORKZONE] Looking for owner=%u name=%s "
            "Generator=%d Damageable=%d Repairable=%d\n",
            static_cast<unsigned>(owner),
            requestedName
            ? requestedName->name.c_str()
            : "<NO NAME>",
            registry.HasComponent<GeneratorComponent>(owner) ? 1 : 0,
            registry.HasComponent<DamageableComponent>(owner) ? 1 : 0,
            registry.HasComponent<RepairableComponent>(owner) ? 1 : 0);

        OutputDebugStringA(buffer);


        for (auto& [zoneEntity, zone] :
            registry.GetComponents<WorkZoneComponent>())
        {
            const auto* zoneName =
                registry.GetComponent<NameComponent>(zoneEntity);

            const auto* ownerName =
                registry.GetComponent<NameComponent>(zone.owner);

            sprintf_s(
                buffer,
                "[WORKZONE] zone=%u name=%s "
                "owner=%u ownerName=%s "
                "Generator=%d Damageable=%d Repairable=%d\n",
                static_cast<unsigned>(zoneEntity),

                zoneName
                ? zoneName->name.c_str()
                : "<NO NAME>",

                static_cast<unsigned>(zone.owner),

                ownerName
                ? ownerName->name.c_str()
                : "<NO NAME>",

                zone.owner != INVALID_ENTITY &&
                registry.HasComponent<GeneratorComponent>(zone.owner)
                ? 1 : 0,

                zone.owner != INVALID_ENTITY &&
                registry.HasComponent<DamageableComponent>(zone.owner)
                ? 1 : 0,

                zone.owner != INVALID_ENTITY &&
                registry.HasComponent<RepairableComponent>(zone.owner)
                ? 1 : 0);

            OutputDebugStringA(buffer);


            if (zone.owner == owner)
            {
                OutputDebugStringA(
                "[WORKZONE] MATCH FOUND!\n");

                return zoneEntity;
            }
        }

        Entity fallbackZone = INVALID_ENTITY;
        float bestFallbackDistanceSq = (std::numeric_limits<float>::max)();
        const auto* requestedTransform =
            registry.GetComponent<TransformComponent>(owner);

        for (auto& [zoneEntity, zone] :
            registry.GetComponents<WorkZoneComponent>())
        {
            (void)zone;

            const auto* zoneName =
                registry.GetComponent<NameComponent>(zoneEntity);

            if (!zoneName)
                continue;

            std::string upperZoneName = zoneName->name;
            std::transform(
                upperZoneName.begin(),
                upperZoneName.end(),
                upperZoneName.begin(),
                [](unsigned char c)
                {
                    return static_cast<char>(std::toupper(c));
                });

            if (upperZoneName.find("REPAIR_ZONE_") == std::string::npos)
                continue;

            const bool ownerIsOxygen =
                registry.HasComponent<OxygenGeneratorComponent>(owner);
            const bool ownerIsGenerator =
                registry.HasComponent<GeneratorComponent>(owner);

            const bool zoneMentionsOxygen =
                upperZoneName.find("OXYGEN") != std::string::npos;
            const bool zoneMentionsGenerator =
                upperZoneName.find("GENERATOR") != std::string::npos;

            const bool compatible =
                (ownerIsOxygen && (zoneMentionsOxygen || zoneMentionsGenerator)) ||
                (ownerIsGenerator && zoneMentionsGenerator && !zoneMentionsOxygen);

            if (!compatible)
                continue;

            float distanceSq = 0.0f;
            if (requestedTransform)
            {
                if (const auto* zoneTransform =
                    registry.GetComponent<TransformComponent>(zoneEntity))
                {
                    const float dx =
                        zoneTransform->position.x -
                        requestedTransform->position.x;
                    const float dy =
                        zoneTransform->position.y -
                        requestedTransform->position.y;
                    const float dz =
                        zoneTransform->position.z -
                        requestedTransform->position.z;
                    distanceSq = dx * dx + dy * dy + dz * dz;
                }
            }

            if (fallbackZone == INVALID_ENTITY ||
                distanceSq < bestFallbackDistanceSq)
            {
                fallbackZone = zoneEntity;
                bestFallbackDistanceSq = distanceSq;
            }
        }

        if (fallbackZone != INVALID_ENTITY)
        {
            OutputDebugStringA(
                "[WORKZONE] FALLBACK MATCH FOUND!\n");

            return fallbackZone;
        }

        OutputDebugStringA(
            "[WORKZONE] NO MATCH!\n");

        return INVALID_ENTITY;
    }



    static void UpdateRepairTask(
        ECSRegistry& registry,
        Entity npc,
        NpcBrainComponent& brain,
        const TransformComponent& transform,
        CharacterMovement& movement,
        const NPCMovementComponent& npcMovement,
        Entity target,
        bool manualCommand,
        float deltaTime,
        JobSystem& jobs)
    {
        // -------------------------------------------------------------------------
        // Общий компонент ремонта.
        //
        // И пробоина, и оборудование должны иметь RepairableComponent.
        // -------------------------------------------------------------------------

        auto* repairable =
            registry.GetComponent<RepairableComponent>(target);

        if (!repairable || repairable->repaired)
        {
            if (manualCommand)
            {
                CompleteManualCommand(
                    brain,
                    movement,
                    "Repair target is already complete or missing");
            }
            else
            {
                brain.repairTarget = INVALID_ENTITY;
                brain.state = NpcState::Idle;
                brain.routeDirty = true;

                StopMovement(movement);
            }

            return;
        }


        // -------------------------------------------------------------------------
        // Универсальные данные цели.
        //
        // После этого остальной код уже не обязан знать,
        // ремонтируем мы пробоину или оборудование.
        // -------------------------------------------------------------------------

        DirectX::XMFLOAT3 repairPosition{};
        Entity repairRoom = INVALID_ENTITY;

        bool validTarget = false;

        enum class RepairTargetType
        {
            Unknown,
            HullBreach,
            Equipment
        };

        RepairTargetType targetType =
            RepairTargetType::Unknown;


        // =========================================================================
        // 1. ПРОБОИНА
        // =========================================================================

        if (auto* breach =
            registry.GetComponent<HullBreachComponent>(target))
        {
            if (!breach->sealed)
            {
                repairPosition =
                    breach->worldPosition;

                repairRoom =
                    breach->room;

                validTarget =
                    true;

                targetType =
                    RepairTargetType::HullBreach;
            }
        }


        // =========================================================================
        // 2. ОБОРУДОВАНИЕ
        // =========================================================================
        else if (auto* damageable =
            registry.GetComponent<DamageableComponent>(target))
        {
            if (damageable->health < damageable->maxHealth)
            {
                if (const auto* targetTransform =
                    registry.GetComponent<TransformComponent>(target))
                {
                    // Fallback.
                    repairPosition =
                        targetTransform->position;


                    // Сначала узнаём комнату оборудования.
                    if (const auto* membership =
                        registry.GetComponent<RoomMembershipComponent>(target))
                    {
                        repairRoom =
                            membership->room;
                    }


                    // Ищем рабочую зону.
                    const Entity zoneEntity =
                        FindRepairZoneForOwner(
                            registry,
                            target);


                    if (zoneEntity != INVALID_ENTITY)
                    {
                        if (const auto* zoneTransform =
                            registry.GetComponent<TransformComponent>(zoneEntity))
                        {
                            DirectX::XMFLOAT3 zonePoint{};
                            if (TryPickReachableWorkZonePoint(
                                registry,
                                repairRoom,
                                transform.position,
                                *zoneTransform,
                                zonePoint))
                            {
                                repairPosition = zonePoint;
                            }
                        }
                    }


                    validTarget = true;

                    targetType =
                        RepairTargetType::Equipment;
                }
            }
        }


        // -------------------------------------------------------------------------
        // Цель больше не существует или больше не требует ремонта.
        // -------------------------------------------------------------------------

        if (!validTarget)
        {
            if (manualCommand)
            {
                CompleteManualCommand(
                    brain,
                    movement,
                    "Repair target is already complete or invalid");
            }
            else
            {
                brain.repairTarget =
                    INVALID_ENTITY;

                brain.state =
                    NpcState::Idle;

                brain.routeDirty =
                    true;

                brain.status =
                    "Automatic repair target invalid";

                StopMovement(movement);
            }

            return;
        }


        // =========================================================================
        // ДВИЖЕНИЕ К ЦЕЛИ
        // =========================================================================

        // Персонаж ходит по палубе, поэтому Y для рабочей дистанции
        // ремонта не учитываем.
        const float distance =
            HorizontalDistance(
                transform.position,
                repairPosition);


        if (distance > brain.repairDistance)
        {
            brain.state =
                NpcState::MoveToRepair;


            const NavigationResult result =
                NavigateTo(
                    registry,
                    npc,
                    brain,
                    transform,
                    movement,
                    npcMovement,
                    repairPosition,
                    repairRoom,
                    brain.repairDistance,
                    NpcState::MoveToRepair,
                    deltaTime);


            if (result == NavigationResult::Blocked)
            {
                if (targetType ==
                    RepairTargetType::HullBreach)
                {
                    brain.status =
                        "Blocked while moving to hull breach";
                }
                else
                {
                    brain.status =
                        "Blocked while moving to equipment";
                }
            }

            return;
        }


        // =========================================================================
        // НАЧАЛО / ПРОДОЛЖЕНИЕ РЕМОНТА
        // =========================================================================

        StopMovement(movement);

        brain.state =
            NpcState::Repairing;


        // -------------------------------------------------------------------------
        // RepairDuration — время полного ремонта с нуля.
        // -------------------------------------------------------------------------

        const float duration =
            (std::max)(
                brain.repairDuration,
                0.05f);


        const float progressDelta =
            repairable->requiredProgress *
            deltaTime /
            duration;


        // -------------------------------------------------------------------------
        // Используем тот же RepairSystem,
        // что игрок и будущие инструменты.
        //
        // Никакого отдельного NPC-ремонта здесь нет.
        // -------------------------------------------------------------------------

        const bool completed =
            RepairSystem::ApplyRepairProgress(
                registry,
                target,
                progressDelta,
                &jobs);


        // =========================================================================
        // РЕМОНТ ЕЩЁ НЕ ЗАКОНЧЕН
        // =========================================================================

        if (!completed)
        {
            // ВАЖНО:
            //
            // CompleteRepair() может удалить Entity пробоины при успешном
            // завершении, но если completed == false, target пока ещё существует.
            //
            // Всё равно лучше заново получить RepairableComponent,
            // потому что ECS storage потенциально мог измениться.
            repairable =
                registry.GetComponent<RepairableComponent>(
                    target);


            if (repairable)
            {
                brain.repairProgress =
                    std::clamp(
                        repairable->Progress01(),
                        0.0f,
                        1.0f);


                if (repairable->progress >=
                    repairable->requiredProgress)
                {
                    brain.status =
                        "Repair: waiting for runtime operation";
                }
                else
                {
                    if (targetType ==
                        RepairTargetType::HullBreach)
                    {
                        brain.status =
                            "Repairing hull breach";
                    }
                    else
                    {
                        brain.status =
                            "Repairing equipment";
                    }
                }
            }

            return;
        }


        // =========================================================================
        // РЕМОНТ ЗАВЕРШЁН
        // =========================================================================

        brain.repairProgress =
            1.0f;

        brain.repairTarget =
            INVALID_ENTITY;

        brain.routeDirty =
            true;


        // Старый маршрут больше не нужен.
        brain.doorRoute.clear();

        brain.doorDestinationRooms.clear();

        brain.doorRouteIndex =
            0;

        brain.activeDoor =
            INVALID_ENTITY;


        if (manualCommand)
        {
            CompleteManualCommand(
                brain,
                movement,
                "Manual repair completed");
        }
        else
        {
            brain.state =
                NpcState::Idle;


            if (targetType ==
                RepairTargetType::HullBreach)
            {
                brain.status =
                    "Automatic hull repair completed";
            }
            else
            {
                brain.status =
                    "Automatic equipment repair completed";
            }


            StopMovement(movement);
        }
    }
    

    static void UpdateWander(
        ECSRegistry& registry,
        Entity npc,
        NpcBrainComponent& brain,
        const TransformComponent& transform,
        CharacterMovement& movement,
        const NPCMovementComponent& npcMovement,
        float deltaTime)
    {
        brain.state = NpcState::Wandering;

        // Пауза между точками нужна не алгоритму, а внешнему виду поведения.
        if (brain.wanderPauseTimer > 0.0f)
        {
            brain.wanderPauseTimer -= deltaTime;
            brain.status = "Wander: observing surroundings";
            StopMovement(movement);
            return;
        }

        if (!brain.wanderTargetValid)
        {
            // Anchor запоминается один раз как запасной центр брожения для NPC,
            // который находится вне корректной RoomComponent.
            if (!brain.progressPositionInitialized)
            {
                brain.wanderAnchor = transform.position;
                brain.randomState ^= static_cast<std::uint32_t>(npc * 747796405u + 2891336453u);
            }
            // PickWanderTarget старается выбрать точку внутри текущей комнаты.
            brain.wanderTarget = PickWanderTarget(registry, brain, transform.position);
            brain.wanderTargetValid = true;
            brain.routeDirty = true;
        }

        const Entity targetRoom = FindContainingRoom(registry, brain.wanderTarget);
        const NavigationResult result = NavigateTo(
            registry, npc, brain, transform, movement, npcMovement,
            brain.wanderTarget, targetRoom, npcMovement.arrivalDistance,
            NpcState::Wandering, deltaTime);
        if (result == NavigationResult::Arrived)
        {
            // Текущая точка забывается; после случайной паузы будет создана новая.
            brain.wanderTargetValid = false;
            brain.wanderPauseTimer = 0.5f + Random01(brain) * 2.0f;
            brain.status = "Wander: waypoint reached";
            StopMovement(movement);
        }
    }

    static NavigationResult NavigateTo(
        ECSRegistry& registry,
        Entity npc,
        NpcBrainComponent& brain,
        const TransformComponent& transform,
        CharacterMovement& movement,
        const NPCMovementComponent& npcMovement,
        const DirectX::XMFLOAT3& finalTarget,
        Entity targetRoom,
        float arrivalDistance,
        NpcState movingState,
        float deltaTime)
    {
        // NavigateTo не знает, ЗАЧЕМ NPC идёт к цели. Одинаковый код обслуживает
        // ремонт, wander, follow и обе команды Go To. Параметр movingState нужен
        // только для правильного текста/состояния снаружи.

        // Таймер может уйти ниже нуля — это нормально. Нас интересует только
        // условие <= 0, когда разрешено дорогое перестроение маршрута.
        brain.repathTimer -= deltaTime;

        brain.debugDoorPhase = "No active door transition";
        brain.debugDoorDestinationRoom = INVALID_ENTITY;
        brain.debugDistanceToDoor = -1.0f;
        brain.debugDistanceToApproach = -1.0f;
        brain.debugProgressBeyondDoor = 0.0f;
        brain.debugRequiredExitProgress = 0.0f;
        brain.debugDoorOpenProgress = 0.0f;
        brain.debugHasDoorExit = false;

        // Маршрут по дверям нужен лишь когда обе комнаты известны и различаются.
        // Если позиция лежит вне bounds комнат, остаётся прямое локальное движение.
        const bool needsRoomRoute =
            brain.currentRoom != INVALID_ENTITY && targetRoom != INVALID_ENTITY &&
            brain.currentRoom != targetRoom;
        // The destination room volume may start inside the physical doorway.
        // Keep the active portal step alive even after currentRoom has already
        // switched; otherwise the route is cleared while the capsule is still
        // standing on the sill. The door step itself decides when enough exit
        // progress has been made.
        const bool continuingDoorTransition =
            brain.activeDoor != INVALID_ENTITY &&
            brain.doorRouteIndex < brain.doorRoute.size();

        // Маршрут перестраивается при новой комнате-цели либо после инвалидирования
        // (застряли, цель поменялась, дверь исчезла) с ограничением repathInterval.
        const bool targetRoomChanged = brain.navigationTargetRoom != targetRoom;
        const bool mayRebuildDirtyRoute = brain.routeDirty && brain.repathTimer <= 0.0f;
        if (needsRoomRoute && (targetRoomChanged || mayRebuildDirtyRoute))
        {
            if (!BuildDoorRoute(
                registry,
                brain.currentRoom,
                targetRoom,
                brain.doorRoute,
                brain.doorDestinationRooms))
            {
                // Blocked не очищает ручную команду. Это специально: разработчик
                // видит проблему, может исправить дверь и нажать Force repath,
                // не задавая команду повторно.
                SetBlocked(brain, movement, "No traversable door route to target room");
                brain.repathTimer = brain.repathInterval;
                return NavigationResult::Blocked;
            }

            // Новый маршрут всегда начинается с первой двери.
            brain.doorRouteIndex = 0;
            brain.navigationTargetRoom = targetRoom;
            brain.routeDirty = false;
            brain.repathTimer = brain.repathInterval;
        }
        else if (needsRoomRoute && brain.routeDirty)
        {
            // Маршрут грязный, но cooldown ещё не закончился. Стоим безопасно,
            // вместо того чтобы идти по заведомо устаревшему пути.
            StopMovement(movement);
            return NavigationResult::Blocked;
        }
        else if (!needsRoomRoute && !continuingDoorTransition)
        {
            // В одной комнате промежуточные двери не нужны. Финальная цель будет
            // обработана после цикла напрямую.
            brain.doorRoute.clear();
            brain.doorDestinationRooms.clear();
            brain.doorRouteIndex = 0;
            brain.navigationTargetRoom = targetRoom;
            brain.routeDirty = false;
        }

        // doorRouteIndex позволяет проходить двери одну за другой, не удаляя
        // элементы vector. while нужен, чтобы уже пройденную дверь пропустить и
        // в том же кадре начать движение к следующей.
        while (brain.doorRouteIndex < brain.doorRoute.size())
        {
            const Entity doorEntity = brain.doorRoute[brain.doorRouteIndex];
            auto* door = registry.GetComponent<DoorComponent>(doorEntity);
            DirectX::XMFLOAT3 doorPosition{};
            if (!door || !TryGetDoorPosition(registry, doorEntity, *door, doorPosition))
            {
                // Scene могла измениться после BFS. Не разыменовываем старую Entity;
                // помечаем маршрут для повторного поиска.
                brain.routeDirty = true;
                SetBlocked(brain, movement, "Route door is missing Transform");
                return NavigationResult::Blocked;
            }

            const Entity destinationRoom =
                brain.doorRouteIndex < brain.doorDestinationRooms.size()
                ? brain.doorDestinationRooms[brain.doorRouteIndex]
                : INVALID_ENTITY;
            DirectX::XMFLOAT3 doorExitPosition{};
            const bool hasDoorExit = TryGetDoorExitPosition(
                registry,
                doorPosition,
                destinationRoom,
                npcMovement.doorExitDistance,
                doorExitPosition);

            brain.debugDoorDestinationRoom = destinationRoom;
            brain.debugDoorPosition = doorPosition;
            brain.debugDoorExit = doorExitPosition;
            brain.debugHasDoorExit = hasDoorExit;
            brain.debugDoorOpenProgress = door->openProgress;

            // A room NavMesh ends before the eroded doorway boundary. Always
            // approach an interior point on the source side first. Asking the
            // source-room Detour query to project doorExitPosition (which is in
            // the next room) is exactly what produced path=0 in the debug panel.
            DirectX::XMFLOAT3 doorApproachPosition = doorPosition;
            float exitDirectionX = 0.0f;
            float exitDirectionZ = 0.0f;
            float exitLength = 0.0f;
            if (hasDoorExit)
            {
                exitDirectionX = doorExitPosition.x - doorPosition.x;
                exitDirectionZ = doorExitPosition.z - doorPosition.z;
                exitLength = std::sqrt(
                    exitDirectionX * exitDirectionX +
                    exitDirectionZ * exitDirectionZ);
                if (exitLength > 0.0001f)
                {
                    exitDirectionX /= exitLength;
                    exitDirectionZ /= exitLength;
                    const float approachDistance = std::clamp(
                        npcMovement.doorWaitDistance * 0.65f,
                        0.55f,
                        1.00f);
                    doorApproachPosition = {
                        doorPosition.x - exitDirectionX * approachDistance,
                        doorPosition.y,
                        doorPosition.z - exitDirectionZ * approachDistance
                    };
                }
            }
            brain.debugDoorApproach = doorApproachPosition;

            // ШАГ 1: подойти достаточно близко и попросить дверь открыться.
            // AI не двигает створки сам — он меняет намерение DoorComponent,
            // а фактическую анимацию openProgress выполняет DoorSystem::Update.
            const float distanceToDoor = HorizontalDistance(transform.position, doorPosition);
            brain.debugDistanceToDoor = distanceToDoor;
            brain.debugDistanceToApproach = HorizontalDistance(
                transform.position, doorApproachPosition);
            if (distanceToDoor <= npcMovement.doorOpenDistance)
            {
                const bool hasPower = DoorHasPower(registry, doorEntity, *door);
                if (!door->TryOpen(hasPower))
                {
                    // TryOpen учитывает broken/requiresPower. Если открыть нельзя,
                    // NPC остаётся перед дверью и показывает понятную причину.
                    brain.activeDoor = doorEntity;
                    SetBlocked(brain, movement, "Door requires power and cannot be opened");
                    return NavigationResult::Blocked;
                }
                brain.activeDoor = doorEntity;
            }

            // ШАГ 2: вблизи двери не идти в коллизию, пока проход ещё закрыт.
            // 0.65 — эмпирический порог: створки уже разошлись достаточно, хотя
            // полная анимация openProgress == 1.0 ещё не закончена.
            if (distanceToDoor <= npcMovement.doorWaitDistance && door->openProgress < 0.65f)
            {
                brain.debugDoorPhase = "WAITING_FOR_OPEN";
                brain.state = NpcState::WaitingForDoor;
                brain.status = "Waiting for door " + std::to_string(doorEntity);
                StopMovement(movement);
                return NavigationResult::Waiting;
            }

            // ШАГ 3: после открытия целимся НЕ в центр двери и НЕ сразу в течь,
            // а в безопасную точку внутри следующей комнаты. Иначе NPC начинал
            // резкий поворот прямо в проёме и цеплялся capsule за дверной косяк.
            if (door->openProgress >= 0.65f && hasDoorExit)
            {
                const float exitX = doorExitPosition.x - doorPosition.x;
                const float exitZ = doorExitPosition.z - doorPosition.z;
                const float npcX = transform.position.x - doorPosition.x;
                const float npcZ = transform.position.z - doorPosition.z;
                const float progressBeyondDoor = exitLength > 0.0001f
                    ? (npcX * exitX + npcZ * exitZ) / exitLength
                    : 0.0f;
                const float requiredExitProgress =
                    (std::max)(0.35f, npcMovement.doorExitDistance * 0.75f);
                brain.debugProgressBeyondDoor = progressBeyondDoor;
                brain.debugRequiredExitProgress = requiredExitProgress;

                if (HorizontalDistance(transform.position, doorExitPosition) <=
                        (std::max)(arrivalDistance, 0.45f) ||
                    (brain.currentRoom == destinationRoom &&
                        progressBeyondDoor >= requiredExitProgress))
                {
                    brain.debugDoorPhase = "PORTAL_COMPLETE";
                    brain.lastPassedDoor = doorEntity;
                    brain.activeDoor = INVALID_ENTITY;
                    ++brain.doorRouteIndex;
                    continue;
                }

                // First centre the capsule on the source side of the opening.
                // Otherwise an NPC coming from a diagonal waypoint aims through
                // the jamb/raised edge while the player approaches it head-on.
                const float approachDistance = HorizontalDistance(
                    transform.position, doorApproachPosition);
                const float approachOffsetX =
                    doorApproachPosition.x - doorPosition.x;
                const float approachOffsetZ =
                    doorApproachPosition.z - doorPosition.z;
                const float approachPlaneProgress = exitLength > 0.0001f
                    ? (approachOffsetX * exitX + approachOffsetZ * exitZ) /
                        exitLength
                    : 0.0f;

                // Align only while the capsule is still on the source side of
                // the approach plane. The old progressBeyondDoor < 0.05 test
                // stayed true almost up to the centre of the doorway. Once the
                // NPC had passed the approach point, it therefore ordered a
                // reversal back to that point; the following frame ordered the
                // direct crossing again, producing an endless tug-of-war.
                if (progressBeyondDoor < approachPlaneProgress &&
                    approachDistance > (std::max)(arrivalDistance, 0.30f))
                {
                    brain.debugDoorPhase = "ALIGNING_TO_APPROACH";
                    brain.state = movingState;
                    brain.status = "Aligning with doorway " +
                        std::to_string(doorEntity);
                    MoveTowardsNavigated(
                        registry, npc, brain, transform.position,
                        doorApproachPosition, movement, npcMovement, true);
                    brain.blockedReason.clear();
                    return NavigationResult::Moving;
                }

                brain.state = movingState;
                brain.debugDoorPhase = "CROSSING_DIRECT";
                brain.status = "Clearing doorway " + std::to_string(doorEntity);
                // The destination is deliberately in the adjacent room, so a
                // single-room NavMesh query cannot own this short portal span.
                // Cross it directly along the door axis, then resume Detour in
                // the destination room.
                NpcNavMeshSystem::InvalidatePath(npc);
                // The open door's frame/leaf collision proxy can share a seam
                // with floors of different heights. Bullet may then wedge the
                // rounded capsule at the threshold even though the portal is
                // visibly clear. Ignore only this open door during the short,
                // axis-aligned crossing; the next non-crossing frame restores
                // both collision-object ignore lists.
                SetPortalCollisionBypass(
                    registry, npc, brain, doorEntity, true);
                MoveTowards(
                    transform.position, doorExitPosition, movement, npcMovement);
                brain.blockedReason.clear();
                return NavigationResult::Moving;
            }

            // Fallback старой сцены без destination room: пройти можно, но точку
            // выхода вычислить нельзя. Не блокируем весь AI из-за неполных данных.
            if (door->openProgress >= 0.65f && !hasDoorExit &&
                distanceToDoor <= (std::max)(arrivalDistance, 0.65f))
            {
                brain.debugDoorPhase = "LEGACY_PORTAL_COMPLETE";
                brain.lastPassedDoor = doorEntity;
                brain.activeDoor = INVALID_ENTITY;
                ++brain.doorRouteIndex;
                continue;
            }

            // До двери ещё нужно идти. MoveTowards задаёт только желаемое
            // направление; физическое перемещение произойдёт позже в кадре.
            brain.state = movingState;
            brain.debugDoorPhase = hasDoorExit
                ? "NAVIGATING_TO_APPROACH"
                : "NAVIGATING_TO_DOOR_CENTER";
            brain.status = "Navigating via door " + std::to_string(doorEntity) +
                " (" + std::to_string(brain.doorRouteIndex + 1) + "/" +
                std::to_string(brain.doorRoute.size()) + ")";
            MoveTowardsNavigated(
                registry, npc, brain, transform.position,
                hasDoorExit ? doorApproachPosition : doorPosition,
                movement, npcMovement, true);
            brain.blockedReason.clear();
            return NavigationResult::Moving;
        }

        // Все двери маршрута обработаны — идём к настоящей цели задачи.
        brain.activeDoor = INVALID_ENTITY;
        if (HorizontalDistance(transform.position, finalTarget) <= arrivalDistance)
        {
            StopMovement(movement);
            return NavigationResult::Arrived;
        }

        brain.state = movingState;
        brain.status = "Moving to final target";
        MoveTowardsNavigated(
            registry, npc, brain, transform.position,
            finalTarget, movement, npcMovement, false);
        brain.blockedReason.clear();
        return NavigationResult::Moving;
    }

    static bool BuildDoorRoute(
        ECSRegistry& registry,
        Entity startRoom,
        Entity targetRoom,
        std::vector<Entity>& outDoors,
        std::vector<Entity>& outDestinationRooms)
    {
        // Верхнеуровневый pathfinding по графу:
        //   вершина графа = Room Entity
        //   ребро графа    = DoorComponent с roomA и roomB
        //
        // Используется BFS (поиск в ширину), потому что каждое ребро имеет
        // одинаковую стоимость. BFS гарантирует маршрут с минимальным количеством
        // дверей. Если позже двери получат стоимость (опасность, расстояние,
        // доступ по роли), здесь стоит заменить BFS на Dijkstra/A*.
        outDoors.clear();
        outDestinationRooms.clear();
        if (startRoom == targetRoom)
            return true;

        // frontier хранит комнаты, соседей которых ещё нужно посмотреть.
        // visited одновременно запрещает циклы и хранит данные восстановления пути.
        std::queue<Entity> frontier;
        std::unordered_map<Entity, RouteNode> visited;
        frontier.push(startRoom);
        visited.emplace(startRoom, RouteNode{});

        while (!frontier.empty())
        {
            const Entity room = frontier.front();
            frontier.pop();

            for (auto& [doorEntity, door] : registry.GetComponents<DoorComponent>())
            {
                // Дверь является соседом только если текущая комната совпала с
                // одной её стороной. Тогда следующая комната — другая сторона.
                Entity nextRoom = INVALID_ENTITY;
                if (door.roomA == room) nextRoom = door.roomB;
                else if (door.roomB == room) nextRoom = door.roomA;
                else continue;

                // Уже посещённую комнату второй раз не ставим в очередь. Иначе
                // циклический корабль мог бы гонять BFS бесконечно.
                if (nextRoom == INVALID_ENTITY || visited.find(nextRoom) != visited.end())
                    continue;

                // Неподвижная дверь без питания исключается из графа. Сломанная
                // дверь, наоборот, проходима: DoorSystem держит её открытой.
                if (!door.isBroken && door.requiresPower && !door.isOpen &&
                    !DoorHasPower(registry, doorEntity, door))
                {
                    continue;
                }

                visited.emplace(nextRoom, RouteNode{ room, doorEntity });
                if (nextRoom == targetRoom)
                {
                    // Цель найдена. visited хранит ссылки назад, поэтому сначала
                    // получаем двери в обратном порядке target -> start, а затем
                    // reverse превращает их в порядок реального прохождения.
                    Entity cursor = targetRoom;
                    while (cursor != startRoom)
                    {
                        const RouteNode& node = visited.at(cursor);
                        outDoors.push_back(node.viaDoor);
                        // cursor — комната, в которую мы попали через viaDoor.
                        // Сохраняем её рядом с дверью для построения exit waypoint.
                        outDestinationRooms.push_back(cursor);
                        cursor = node.previousRoom;
                    }
                    std::reverse(outDoors.begin(), outDoors.end());
                    std::reverse(outDestinationRooms.begin(), outDestinationRooms.end());
                    return true;
                }
                frontier.push(nextRoom);
            }
        }
        // Очередь закончилась, цель не посещена: связного проходимого пути нет.
        return false;
    }

    static Entity FindContainingRoom(
        ECSRegistry& registry,
        const DirectX::XMFLOAT3& worldPosition,
        Entity preferredRoom = INVALID_ENTITY)
    {
        // RoomComponent хранит AABB в ЛОКАЛЬНЫХ координатах комнаты. Поэтому
        // нельзя просто сравнить worldPosition с localMin/localMax: сначала точку
        // переводим обратной world matrix комнаты в её локальное пространство.
        const auto containsPoint = [&](Entity roomEntity, const RoomComponent& room)
        {
            const auto* transform = registry.GetComponent<TransformComponent>(roomEntity);
            if (!transform || !room.isActive)
                return false;

            const DirectX::XMMATRIX worldToLocal =
                DirectX::XMMatrixInverse(nullptr, transform->GetWorldMatrix());
            const DirectX::XMVECTOR localVector = DirectX::XMVector3TransformCoord(
                DirectX::XMLoadFloat3(&worldPosition), worldToLocal);
            DirectX::XMFLOAT3 local{};
            DirectX::XMStoreFloat3(&local, localVector);

            return local.x >= room.localMin.x && local.x <= room.localMax.x &&
                // SHIP_PANEL может иметь углубление ниже старой нижней границы
                // логического SHIP_ROOM. Оставляем вертикальный запас, чтобы NPC
                // не терял комнату на физически корректном полу.
                local.y >= room.localMin.y - 2.0f && local.y <= room.localMax.y + 0.15f &&
                local.z >= room.localMin.z && local.z <= room.localMax.z;
        };

        if (preferredRoom != INVALID_ENTITY)
        {
            if (const auto* preferred =
                registry.GetComponent<RoomComponent>(preferredRoom))
            {
                if (containsPoint(preferredRoom, *preferred))
                    return preferredRoom;
            }
        }

        for (auto& [roomEntity, room] : registry.GetComponents<RoomComponent>())
        {
            if (containsPoint(roomEntity, room))
                return roomEntity;
        }
        // INVALID_ENTITY допустим, например, в дверном проёме или при неверных
        // bounds. NavigateTo в этом случае использует движение к цели по прямой.
        return INVALID_ENTITY;
    }

    static bool TryGetRoomCenter(
        ECSRegistry& registry,
        Entity roomEntity,
        DirectX::XMFLOAT3& outCenter)
    {
        // Возвращаем bool, а не фиктивный {0,0,0}: мировой ноль может быть вполне
        // настоящим центром комнаты и не должен означать ошибку.
        const auto* room = registry.GetComponent<RoomComponent>(roomEntity);
        const auto* transform = registry.GetComponent<TransformComponent>(roomEntity);
        if (!room || !transform)
            return false;

        // Центр локального AABB = (min + max) / 2 по каждой оси.
        const DirectX::XMFLOAT3 localCenter = {
            (room->localMin.x + room->localMax.x) * 0.5f,
            (room->localMin.y + room->localMax.y) * 0.5f,
            (room->localMin.z + room->localMax.z) * 0.5f
        };
        // После world transform центр корректно следует за parent rotation/scale.
        const DirectX::XMVECTOR world = DirectX::XMVector3TransformCoord(
            DirectX::XMLoadFloat3(&localCenter), transform->GetWorldMatrix());
        DirectX::XMStoreFloat3(&outCenter, world);
        return true;
    }

    static bool TryGetDoorPosition(
        ECSRegistry& registry,
        Entity doorEntity,
        const DoorComponent& door,
        DirectX::XMFLOAT3& outPosition)
    {
        // Предпочтительный вариант: Transform корневой Entity двери должен стоять
        // в центре прохода и не обязан совпадать с одной из движущихся створок.
        if (const auto* transform = registry.GetComponent<TransformComponent>(doorEntity))
        {
            outPosition = transform->position;
            return true;
        }

        // Fallback для старых сцен: если у корня Transform нет, берём середину
        // между левой и правой створкой. Для одностворчатой двери fallback невозможен.
        const auto* left = registry.GetComponent<TransformComponent>(door.leftLeaf);
        const auto* right = registry.GetComponent<TransformComponent>(door.rightLeaf);
        if (!left || !right)
            return false;

        outPosition = {
            (left->position.x + right->position.x) * 0.5f,
            (left->position.y + right->position.y) * 0.5f,
            (left->position.z + right->position.z) * 0.5f
        };
        return true;
    }

    static bool TryGetDoorExitPosition(
        ECSRegistry& registry,
        const DirectX::XMFLOAT3& doorPosition,
        Entity destinationRoom,
        float exitDistance,
        DirectX::XMFLOAT3& outPosition)
    {
        const auto* room = registry.GetComponent<RoomComponent>(destinationRoom);
        const auto* roomTransform =
            registry.GetComponent<TransformComponent>(destinationRoom);
        if (!room || !roomTransform)
            return false;

        // The room centre is not a valid portal normal: a door near a corner
        // produces a diagonal vector and makes the capsule hit the jamb. Find
        // the nearest horizontal face of the destination room instead. Its
        // inward normal is the stable doorway axis even for rotated rooms.
        const DirectX::XMMATRIX roomWorld = roomTransform->GetWorldMatrix();
        const DirectX::XMMATRIX worldToRoom =
            DirectX::XMMatrixInverse(nullptr, roomWorld);
        const DirectX::XMVECTOR localDoorVector =
            DirectX::XMVector3TransformCoord(
                DirectX::XMLoadFloat3(&doorPosition), worldToRoom);
        DirectX::XMFLOAT3 localDoor{};
        DirectX::XMStoreFloat3(&localDoor, localDoorVector);

        const float distances[4] =
        {
            std::abs(localDoor.x - room->localMin.x),
            std::abs(room->localMax.x - localDoor.x),
            std::abs(localDoor.z - room->localMin.z),
            std::abs(room->localMax.z - localDoor.z)
        };
        int nearestFace = 0;
        for (int face = 1; face < 4; ++face)
        {
            if (distances[face] < distances[nearestFace])
                nearestFace = face;
        }

        DirectX::XMFLOAT3 localInward{};
        switch (nearestFace)
        {
        case 0: localInward.x = 1.0f; break;  // min X face
        case 1: localInward.x = -1.0f; break; // max X face
        case 2: localInward.z = 1.0f; break;  // min Z face
        default: localInward.z = -1.0f; break; // max Z face
        }

        DirectX::XMVECTOR worldInwardVector = DirectX::XMVector3TransformNormal(
            DirectX::XMLoadFloat3(&localInward), roomWorld);
        worldInwardVector = DirectX::XMVectorSetY(worldInwardVector, 0.0f);
        const float directionLength =
            DirectX::XMVectorGetX(DirectX::XMVector3Length(worldInwardVector));
        if (directionLength <= 0.0001f)
            return false;

        worldInwardVector = DirectX::XMVectorScale(
            worldInwardVector, 1.0f / directionLength);
        DirectX::XMFLOAT3 worldInward{};
        DirectX::XMStoreFloat3(&worldInward, worldInwardVector);

        const float safeDistance = (std::max)(0.5f, exitDistance);
        outPosition = {
            doorPosition.x + worldInward.x * safeDistance,
            doorPosition.y,
            doorPosition.z + worldInward.z * safeDistance
        };
        return true;
    }

    static DirectX::XMFLOAT3 PickWanderTarget(
        ECSRegistry& registry,
        NpcBrainComponent& brain,
        const DirectX::XMFLOAT3& currentPosition)
    {
        // Основной вариант: выбрать точку внутри bounds текущей комнаты. Это не
        // полноценный NavMesh — мебель и другие внутренние препятствия не учтены.
        const auto* room = registry.GetComponent<RoomComponent>(brain.currentRoom);
        const auto* roomTransform = registry.GetComponent<TransformComponent>(brain.currentRoom);
        if (room && roomTransform)
        {
            // Текущую world position переводим в local space комнаты, чтобы
            // сохранить высоту палубы и корректно работать с поворотом комнаты.
            const DirectX::XMMATRIX worldToLocal =
                DirectX::XMMatrixInverse(nullptr, roomTransform->GetWorldMatrix());
            const DirectX::XMVECTOR localCurrentVector = DirectX::XMVector3TransformCoord(
                DirectX::XMLoadFloat3(&currentPosition), worldToLocal);
            DirectX::XMFLOAT3 localCurrent{};
            DirectX::XMStoreFloat3(&localCurrent, localCurrentVector);

            // Padding не позволяет выбирать waypoint прямо в стене/границе AABB.
            constexpr float padding = 0.65f;
            const float minX = room->localMin.x + padding;
            const float maxX = room->localMax.x - padding;
            const float minZ = room->localMin.z + padding;
            const float maxZ = room->localMax.z - padding;
            // Если комната уже, чем двойной padding, по этой оси остаёмся на
            // текущей координате вместо создания некорректного диапазона.
            // Bounds комнаты только создают кандидатов. Окончательно цель
            // принимает NavMesh: она должна принадлежать тому же связному
            // острову, где стоит NPC. Поэтому после проекции Detour персонаж не
            // продолжит идти в исходную точку внутри мебели.
            DirectX::XMFLOAT3 fallbackTarget = currentPosition;
            constexpr int maxCandidateAttempts = 12;
            constexpr float minimumWanderDistance = 1.0f;
            for (int attempt = 0; attempt < maxCandidateAttempts; ++attempt)
            {
                DirectX::XMFLOAT3 localTarget = {
                    maxX > minX ? minX + Random01(brain) * (maxX - minX) : localCurrent.x,
                    localCurrent.y,
                    maxZ > minZ ? minZ + Random01(brain) * (maxZ - minZ) : localCurrent.z
                };
                // Возвращаем выбранную local-точку обратно в мировые координаты.
                const DirectX::XMVECTOR worldTarget = DirectX::XMVector3TransformCoord(
                    DirectX::XMLoadFloat3(&localTarget), roomTransform->GetWorldMatrix());
                DirectX::XMFLOAT3 reachableTarget{};
                DirectX::XMStoreFloat3(&fallbackTarget, worldTarget);
                if (NpcNavMeshSystem::FindReachablePoint(
                        registry,
                        brain.currentRoom,
                        currentPosition,
                        fallbackTarget,
                        reachableTarget) &&
                    HorizontalDistance(currentPosition, reachableTarget) >=
                        minimumWanderDistance)
                {
                    return reachableTarget;
                }
            }

            // При NPC_NAVMESH_ENABLED=0 функция выше является inline-заглушкой,
            // поэтому отключаемый слой сохраняет старое прямое движение.
            return fallbackTarget;
        }

        // Fallback вне комнаты: случайная точка в кольце вокруг wanderAnchor.
        // Минимальный радиус 1.0 не даёт NPC выбирать почти ту же самую позицию.
        const float angle = Random01(brain) * DirectX::XM_2PI;
        const float radius = 1.0f + Random01(brain) * (std::max)(1.0f, brain.wanderRadius);
        return {
            brain.wanderAnchor.x + std::sin(angle) * radius,
            currentPosition.y,
            brain.wanderAnchor.z + std::cos(angle) * radius
        };
    }

    static float Random01(NpcBrainComponent& brain)
    {
        // Простой LCG. Для gameplay-рандома его достаточно; для криптографии — нет.
        // Возвращаем равномерное число в полуинтервале [0, 1).
        brain.randomState = brain.randomState * 1664525u + 1013904223u;
        return static_cast<float>((brain.randomState >> 8) & 0x00FFFFFFu) /
            static_cast<float>(0x01000000u);
    }

    static bool DoorHasPower(ECSRegistry& registry, Entity doorEntity, const DoorComponent& door)
    {
        if (!door.requiresPower)
            return true;
        const auto* power = registry.GetComponent<PowerConsumerComponent>(doorEntity);
        // Старые сцены созданы до PowerConsumerComponent. Отсутствующий компонент
        // считаем "питание есть", чтобы после обновления все старые двери внезапно
        // не заблокировали корабль. Явно добавленный component уже имеет приоритет.
        return !power || power->hasPower;
    }

    static void SetPortalCollisionBypass(
        ECSRegistry& registry,
        Entity npc,
        NpcBrainComponent& brain,
        Entity doorEntity,
        bool ignore)
    {
        auto* controller = registry.GetComponent<CharacterController>(npc);
        if (!controller || !controller->ghostObj)
            return;

        const auto setPairIgnored = [&](Entity colliderEntity, bool pairIgnored)
        {
            if (colliderEntity == INVALID_ENTITY)
                return;
            auto* rigidBody = registry.GetComponent<RigidBodyComponent>(colliderEntity);
            if (!rigidBody || !rigidBody->body)
                return;

            // Bullet does not deduplicate its ignore list. This helper is called
            // only when the tracked door changes, never once per crossing frame.
            controller->ghostObj->setIgnoreCollisionCheck(
                rigidBody->body, pairIgnored);
            rigidBody->body->setIgnoreCollisionCheck(
                controller->ghostObj, pairIgnored);
        };

        if (ignore)
        {
            brain.portalCollisionBypassRequestedThisFrame = true;
            if (brain.portalCollisionIgnoredDoor == doorEntity)
                return;

            if (brain.portalCollisionIgnoredDoor != INVALID_ENTITY)
            {
                SetPortalCollisionBypass(
                    registry,
                    npc,
                    brain,
                    brain.portalCollisionIgnoredDoor,
                    false);
            }

            const auto* door = registry.GetComponent<DoorComponent>(doorEntity);
            setPairIgnored(doorEntity, true);
            if (door)
            {
                setPairIgnored(door->leftLeaf, true);
                if (door->rightLeaf != door->leftLeaf)
                    setPairIgnored(door->rightLeaf, true);
            }
            brain.portalCollisionIgnoredDoor = doorEntity;
            return;
        }

        const Entity trackedDoor = brain.portalCollisionIgnoredDoor;
        if (trackedDoor == INVALID_ENTITY)
            return;
        const auto* door = registry.GetComponent<DoorComponent>(trackedDoor);
        setPairIgnored(trackedDoor, false);
        if (door)
        {
            setPairIgnored(door->leftLeaf, false);
            if (door->rightLeaf != door->leftLeaf)
                setPairIgnored(door->rightLeaf, false);
        }
        brain.portalCollisionIgnoredDoor = INVALID_ENTITY;
    }

    static void MoveTowardsNavigated(
        ECSRegistry& registry,
        Entity npc,
        const NpcBrainComponent& brain,
        const DirectX::XMFLOAT3& from,
        const DirectX::XMFLOAT3& target,
        CharacterMovement& movement,
        const NPCMovementComponent& settings,
        bool requireForwardProgress)
    {
        // Изолированный NavMesh-слой возвращает только следующую точку. Если он
        // выключен, ещё не построен или Detour не нашёл путь, false мгновенно
        // включает старое прямое движение. Поэтому Recast не является жёсткой
        // зависимостью поведения NPC.
        DirectX::XMFLOAT3 waypoint{};
        DirectX::XMFLOAT3 movementTarget = target;
        if (NpcNavMeshSystem::FindNextWaypoint(
                registry,
                npc,
                brain.currentRoom,
                from,
                target,
                brain.command.revision,
                waypoint))
        {
            movementTarget = waypoint;
        }
        const DirectX::XMFLOAT3 navMovementTarget = movementTarget;

        // Recast отвечает за постоянную геометрию, а этот короткий steering-шаг
        // объезжает движущиеся Bullet-объекты поверх уже рассчитанного пути.
        DirectX::XMFLOAT3 avoidanceWaypoint{};
        if (NpcNavMeshSystem::FindDynamicAvoidanceWaypoint(
                registry,
                npc,
                brain.currentRoom,
                from,
                movementTarget,
                avoidanceWaypoint))
        {
            movementTarget = avoidanceWaypoint;
        }

        if (requireForwardProgress)
        {
            const float desiredX = target.x - from.x;
            const float desiredZ = target.z - from.z;
            const auto advancesTowardDoor = [&](const DirectX::XMFLOAT3& candidate)
            {
                const float candidateX = candidate.x - from.x;
                const float candidateZ = candidate.z - from.z;
                return candidateX * desiredX + candidateZ * desiredZ > 0.0f;
            };

            // Dynamic avoidance may project a side candidate onto a polygon
            // behind the NPC. That is acceptable for general navigation around
            // a large obstacle, but fatal while lining up with a portal: the
            // controller walks away from the only valid door and eventually
            // freezes against the opposite wall. Prefer the Detour waypoint if
            // it still advances toward the approach point; otherwise use the
            // geometric approach itself for this short, clear corridor segment.
            if (!advancesTowardDoor(movementTarget))
            {
                movementTarget = advancesTowardDoor(navMovementTarget)
                    ? navMovementTarget
                    : target;
            }
        }

        MoveTowards(from, movementTarget, movement, settings);
    }

    static void MoveTowards(
        const DirectX::XMFLOAT3& from,
        const DirectX::XMFLOAT3& to,
        CharacterMovement& movement,
        const NPCMovementComponent& settings)
    {
        // StateSystem НЕ меняет Transform напрямую. Он пишет намерение движения в
        // CharacterMovement, а Bullet controller позже применяет его с коллизиями.
        // Y обнуляется: обычный NPC ходит по горизонтальной палубе.
        DirectX::XMFLOAT3 direction = { to.x - from.x, 0.0f, to.z - from.z };
        const float distance = std::sqrt(direction.x * direction.x + direction.z * direction.z);
        if (distance <= 0.0001f)
        {
            StopMovement(movement);
            return;
        }

        // Нормализация нужна, чтобы скорость не зависела от расстояния до цели.
        direction.x /= distance;
        direction.z /= distance;
        // Решение walk/run принимает AI, но проигрывание конкретного clip делает
        // AnimationController по locomotionState. Если Run clip отсутствует,
        // существующий animation fallback использует доступную анимацию.
        const bool run = settings.allowRunning && distance >= settings.runWhenFartherThan;
        movement.walkDirection = direction;
        movement.currentSpeed = run ? movement.runSpeed : movement.moveSpeed;
        movement.wantsRun = run;
        movement.isRunning = run;
        movement.isMoving = true;
        movement.locomotionState = run
            ? PlayerLocomotionState::Run
            : PlayerLocomotionState::Walk;
    }

    static void StopMovement(CharacterMovement& movement)
    {
        // Сбрасываем ВСЕ locomotion-флаги вместе. Если забыть, например, wantsRun,
        // физика может стоять, а AnimationController продолжит Run-анимацию.
        movement.walkDirection = { 0.0f, 0.0f, 0.0f };
        movement.currentSpeed = 0.0f;
        movement.wantsRun = false;
        movement.isRunning = false;
        movement.isMoving = false;
        movement.locomotionState = PlayerLocomotionState::Idle;
    }

    static void CompleteManualCommand(
        NpcBrainComponent& brain,
        CharacterMovement& movement,
        const char* status)
    {
        // Единая точка окончания одноразовой команды. После Clear() Utility AI
        // сможет выбрать работу уже на следующем кадре.
        brain.command.Clear();
        brain.state = NpcState::Idle;
        brain.routeDirty = true;
        brain.doorRoute.clear();
        brain.doorDestinationRooms.clear();
        brain.doorRouteIndex = 0;
        brain.activeDoor = INVALID_ENTITY;
        brain.status = status;
        brain.blockedReason.clear();
        StopMovement(movement);
    }

    static void SetBlocked(
        NpcBrainComponent& brain,
        CharacterMovement& movement,
        const char* reason)
    {
        // Не очищаем command и цель: Blocked — диагностируемое состояние, а не
        // успешное завершение приказа. После исправления сцены можно Force repath
        // или выдать другую команду.
        brain.state = NpcState::Blocked;
        brain.blockedReason = reason;
        brain.status = std::string("Blocked: ") + reason;
        StopMovement(movement);
    }

    static void UpdateStuckStatus(
        Entity npc,
        NpcBrainComponent& brain,
        const DirectX::XMFLOAT3& position,
        const CharacterMovement& movement,
        float deltaTime)
    {
        // Проверяем не "хочет ли AI идти", а реальное изменение Transform после
        // предыдущих кадров физики. Так обнаруживается упор в стену/препятствие.
        if (!movement.isMoving)
        {
            brain.stuckTimer = 0.0f;
            brain.lastProgressPosition = position;
            brain.progressPositionInitialized = true;
            return;
        }

        if (!brain.progressPositionInitialized)
        {
            brain.lastProgressPosition = position;
            brain.progressPositionInitialized = true;
            return;
        }

        // Сдвиг хотя бы на 3 см считаем прогрессом и начинаем таймер заново.
        if (HorizontalDistance(brain.lastProgressPosition, position) >= 0.08f)
        {
            brain.lastProgressPosition = position;
            brain.stuckTimer = 0.0f;
            return;
        }

        brain.stuckTimer += deltaTime;
        if (brain.stuckTimer >= brain.stuckTimeout)
        {
            if (!brain.debugDoorSnapshotCaptured &&
                brain.activeDoor != INVALID_ENTITY)
            {
                char snapshot[2048]{};
                std::snprintf(
                    snapshot,
                    sizeof(snapshot),
                    "AUTO-FROZEN: no physical progress for %.2f sec\n"
                    "Phase: %s\n"
                    "NPC XYZ: %.3f  %.3f  %.3f\n"
                    "Current/target/destination rooms: %u / %u / %u\n"
                    "Active door: %u   route: %u / %u   dirty=%s\n"
                    "Door XYZ: %.3f  %.3f  %.3f\n"
                    "Approach XYZ: %.3f  %.3f  %.3f\n"
                    "Exit XYZ: %.3f  %.3f  %.3f   valid=%s\n"
                    "Distance door/approach: %.3f / %.3f\n"
                    "Beyond/required: %.3f / %.3f   open=%.3f\n"
                    "Movement: %s speed=%.3f dir=(%.3f, %.3f)",
                    brain.stuckTimeout,
                    brain.debugDoorPhase.c_str(),
                    position.x, position.y, position.z,
                    static_cast<unsigned>(brain.currentRoom),
                    static_cast<unsigned>(brain.navigationTargetRoom),
                    static_cast<unsigned>(brain.debugDoorDestinationRoom),
                    static_cast<unsigned>(brain.activeDoor),
                    static_cast<unsigned>(brain.doorRouteIndex),
                    static_cast<unsigned>(brain.doorRoute.size()),
                    brain.routeDirty ? "yes" : "no",
                    brain.debugDoorPosition.x,
                    brain.debugDoorPosition.y,
                    brain.debugDoorPosition.z,
                    brain.debugDoorApproach.x,
                    brain.debugDoorApproach.y,
                    brain.debugDoorApproach.z,
                    brain.debugDoorExit.x,
                    brain.debugDoorExit.y,
                    brain.debugDoorExit.z,
                    brain.debugHasDoorExit ? "yes" : "no",
                    brain.debugDistanceToDoor,
                    brain.debugDistanceToApproach,
                    brain.debugProgressBeyondDoor,
                    brain.debugRequiredExitProgress,
                    brain.debugDoorOpenProgress,
                    movement.isMoving ? "yes" : "no",
                    movement.currentSpeed,
                    movement.walkDirection.x,
                    movement.walkDirection.z);
                brain.debugDoorSnapshot = snapshot;
                brain.debugDoorSnapshotCaptured = true;
            }

            // Перестройка комнатного маршрута может помочь, если изменилась дверь.
            // Если причина — мебель внутри комнаты, прямой MoveTowards снова упрётся
            // в неё: это как раз сигнал, что пора подключать NavMesh/waypoints.
            brain.routeDirty = true;
            NpcNavMeshSystem::InvalidatePath(npc);
            if (brain.state == NpcState::Wandering)
            {
                // Перестроение пути к той же неудачной случайной точке создаёт
                // вечный цикл. Wander имеет право забыть её и выбрать новую.
                brain.wanderTargetValid = false;
                brain.wanderPauseTimer = 0.15f;
                brain.blockedReason = "Wander target made no movement progress";
                brain.status = "Wander: stuck target discarded; choosing another";
            }
            else
            {
                brain.blockedReason =
                    "No movement progress; automatic NavMesh repath requested";
                brain.status = "Stuck: automatic repath requested";
            }
            brain.stuckTimer = 0.0f;
        }
    }

    static float HorizontalDistance(
        const DirectX::XMFLOAT3& a,
        const DirectX::XMFLOAT3& b)
    {
        // Дистанция на палубе. Высота Y намеренно игнорируется для движения,
        // дверей и рабочей дистанции ремонта наземного персонажа.
        const float dx = b.x - a.x;
        const float dz = b.z - a.z;
        return std::sqrt(dx * dx + dz * dz);
    }
};
