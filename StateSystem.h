#pragma once

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <queue>
#include <string>
#include <unordered_map>

#include <ECSRegistry.h>
#include "NPCComponents.h"
#include "NpcCommandSystem.h"
#include "CharacterSpawnSystem.h"
#include "Gameplay/Power/PowerConsumerComponent.h"
#include "Gameplay/AI/CrewTaskCoordinator.h"
#include "Gameplay/AI/NpcNavMeshSystem.h"
#include "Gameplay/AI/NpcCrowdSpacing.h"
#include "Gameplay/AI/NpcCombatDecisionRules.h"
#include "Gameplay/AI/NpcCombatModules.h"
#include "Gameplay/AI/NpcCoverLifecycle.h"
#include "Gameplay/Repair/RepairComponent.h"
#include "Gameplay/Repair/RepairSystem.h"
#include "AnimationComponent.h"
#include "JobSystem.h"
#include "Gameplay/Combat/WeaponComponent.h"
#include "Gameplay/Combat/WeaponSystem.h"
#include "../DebugHud.h"

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


// Переводит позицию NPC в локальные координаты кубической WorkZone, зажимает
// её внутри [-1..1] и возвращает ближайшую доступную мировую точку зоны.
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

// Переводит заранее выбранную локальную точку WorkZone обратно в мир.
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

// Перебирает несколько мест внутри рабочей зоны и выбирает то, которое NavMesh
// может связать с NPC. outPoint заполняется только при true.
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
                if (!registry.HasComponent<ActiveComponent>(npc))
                {
                    if (auto* movement =
                        registry.GetComponent<CharacterMovement>(npc))
                    {
                        StopMovement(*movement);
                    }
                    if (auto* controller =
                        registry.GetComponent<CharacterController>(npc);
                        controller && controller->controller)
                    {
                        controller->controller->setWalkDirection(
                            btVector3(0.0f, 0.0f, 0.0f));
                        controller->controller->setVelocityForTimeInterval(
                            btVector3(0.0f, 0.0f, 0.0f), btScalar(0.0f));
                    }
                    return;
                }

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
                // Разводим не только конечные позиции, но и движущиеся капсулы.
                // Transform не телепортируем: корректировку выполняет физика.
                if (!brain.command.IsActive())
                {
                    DirectX::XMFLOAT3 separation{};
                    registry.ForEach<NPCComponent, TransformComponent>([&](Entity other,
                        NPCComponent&, TransformComponent& otherTransform) {
                        const auto* otherVitals =
                            registry.GetComponent<CharacterVitalsComponent>(other);
                        if (other == npc || !otherVitals || otherVitals->health <= 0.0f ||
                            !registry.HasComponent<ActiveComponent>(other) ||
                            std::abs(otherTransform.position.y-transform.position.y) > 1.5f) return;
                        float x = transform.position.x-otherTransform.position.x;
                        float z = transform.position.z-otherTransform.position.z;
                        const auto* otherController = registry.GetComponent<CharacterController>(other);
                        const float spacing = controller->capsuleRadius +
                            (otherController ? otherController->capsuleRadius : 0.4f) + 0.6f;
                        // При полном совпадении выбираем противоположные стороны
                        // по ID, иначе нулевой вектор никогда не раздвинет пару.
                        const auto correction = NpcPairSeparation(npc, other, x, z, spacing);
                        separation.x += correction.x;
                        separation.z += correction.z;
                    });
                    if (std::hypot(separation.x,separation.z) > 0.01f)
                    {
                        const float x = movement->walkDirection.x + separation.x;
                        const float z = movement->walkDirection.z + separation.z;
                        const float length = std::hypot(x,z);
                        if (length > 0.001f)
                        {
                            movement->walkDirection = {x/length,0.0f,z/length};
                            movement->currentSpeed = movement->moveSpeed;
                            movement->isMoving = true;
                            movement->isRunning = movement->wantsRun = false;
                            movement->locomotionState = PlayerLocomotionState::Walk;
                        }
                    }
                }
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

    // Исполняет состояние одного NPC. Здесь ручная команда получает приоритет,
    // а затем switch делегирует ремонт, бой, поиск, укрытие или патруль.
    static void UpdateNpc(
        ECSRegistry& registry,
        Entity npc,
        NpcBrainComponent& brain,
         TransformComponent& transform,
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

        // После завершённого поиска старое состояние Attacking/Searching могло
        // остаться ещё на один кадр, хотя targetEntity и память позиции уже
        // очищены. Раньше такой NPC не попадал в ветку Wandering и визуально
        // застывал. Если контакта действительно больше нет и авторский patrol
        // не включён, сразу возвращаем боевого NPC в исходный патруль поиска.
        const auto* preContactCombat =
            registry.GetComponent<NPCCombatComponent>(npc);
        const auto* preContactFaction =
            registry.GetComponent<FactionComponent>(npc);
        const auto* authoredPatrol =
            registry.GetComponent<NpcPatrolComponent>(npc);
        const auto* preContactTactical =
            registry.GetComponent<NpcTacticalComponent>(npc);
        const bool defensiveHoldingCover =
            preContactTactical &&
            preContactTactical->activeStyle == NpcTacticalStyle::Defensive &&
            (preContactTactical->reservedCover != INVALID_ENTITY ||
                preContactTactical->dynamicCoverActive) &&
            brain.state == NpcState::InCover;
        const bool shouldResumeContactWander =
            preContactCombat &&
            preContactFaction &&
            preContactFaction->faction != FactionId::Neutral &&
            preContactCombat->wanderUntilContact &&
            preContactCombat->targetEntity == INVALID_ENTITY &&
            !preContactCombat->hasLastKnownPosition &&
            !defensiveHoldingCover &&
            (!authoredPatrol || !authoredPatrol->enabled);
        if (shouldResumeContactWander &&
            brain.state != NpcState::Idle &&
            brain.state != NpcState::Wandering)
        {
            brain.state = NpcState::Idle;
            brain.wanderTargetValid = false;
            brain.wanderPauseTimer = 0.0f;
            NpcNavMeshSystem::InvalidatePath(npc);
        }

        // Ручной команды нет: исполняем состояние, выбранное Utility AI.
        // WaitingForDoor обрабатывается тем же UpdateRepairTask, потому что после
        // открытия двери нужно продолжить ту же задачу, а не создавать отдельную.
        switch (brain.state)
        {
        case NpcState::Idle:
        case NpcState::Patrolling:
        case NpcState::Wandering:
            if (auto* patrol = registry.GetComponent<NpcPatrolComponent>(npc);
                patrol && patrol->enabled)
            {
                UpdatePatrol(registry, npc, brain, transform, movement,
                    npcMovement, *patrol, deltaTime);
            }
            else
            {
                const auto* combat =
                    registry.GetComponent<NPCCombatComponent>(npc);
                const auto* faction =
                    registry.GetComponent<FactionComponent>(npc);
                const bool canSearchForFirstContact =
                    combat &&
                    faction &&
                    faction->faction != FactionId::Neutral &&
                    combat->wanderUntilContact &&
                    combat->targetEntity == INVALID_ENTITY &&
                    !combat->hasLastKnownPosition;

                if (canSearchForFirstContact)
                {
                    // Это не поиск уже известного врага: NPC не знает,
                    // где он находится. Он просто ходит по случайным достижимым
                    // точкам. Поворот по ходу перемещает сектор зрения, и рано
                    // или поздно NpcCombatSystem сам подтвердит контакт.
                    if (brain.state != NpcState::Wandering)
                    {
                        // Новый боевой патруль всегда начинает новый цикл.
                        // Иначе оставшаяся от ручной команды Wander цель могла
                        // находиться в другой комнате или уже быть недоступной.
                        brain.wanderAnchor = transform.position;
                        brain.wanderTargetValid = false;
                        brain.wanderPauseTimer = 0.0f;
                    }
                    else if (!brain.wanderTargetValid)
                    {
                        // Боевой поиск не должен вечно кружить вокруг точки
                        // спавна. После каждого достигнутого waypoint текущая
                        // позиция становится новым центром следующего шага.
                        // Так два бойца способны в итоге встретиться даже если
                        // начали дальше одного contactWanderRadius.
                        brain.wanderAnchor = transform.position;
                    }

                    const float ordinaryWanderRadius = brain.wanderRadius;
                    brain.wanderRadius = (std::max)(
                        1.0f,
                        combat->contactWanderRadius);
                    NPCMovementComponent searchMovement = npcMovement;
                    searchMovement.allowRunning = false;
                    UpdateWander(
                        registry,
                        npc,
                        brain,
                        transform,
                        movement,
                        searchMovement,
                        deltaTime);
                    brain.wanderRadius = ordinaryWanderRadius;
                    brain.status = movement.isMoving
                        ? "Боевой патруль: ищу первый визуальный контакт"
                        : "Боевой патруль: осматриваю окружение";
                }
                else
                {
                    brain.state = NpcState::Idle;
                    StopMovement(movement);
                }
            }
            break;
        case NpcState::MoveToRepair:
        case NpcState::Repairing:
        case NpcState::WaitingForDoor:
            UpdateRepairTask(
                registry, npc, brain, transform, movement, npcMovement,
                brain.repairTarget, false, deltaTime, jobs);
            break;
        case NpcState::TakingCover:
        {
            auto* tactical = registry.GetComponent<NpcTacticalComponent>(npc);
            auto* combat = registry.GetComponent<NPCCombatComponent>(npc);
            if (!tactical || !combat || !combat->hasLastKnownPosition)
            {
                brain.state = NpcState::Chasing;
                break;
            }
            // TakingCover всегда означает движение: к краю, обратно за стену
            // или к новой позиции. Полнотелая peek-поза включится только после
            // NavigationResult::Arrived ниже.
            tactical->isPeeking = false;
            tactical->peekState = NpcTacticalComponent::PeekSide::none;
            NPCMovementComponent tacticalMovement = npcMovement;
            if (tactical->movingToPeek)
            {
                // Peek — это короткое резкое выглядывание, а не пробежка.
                // Поэтому повышаем линейную скорость, но оставляем
                // locomotion=Walk. Так будущий peek-clip не заменится Run-клипом.
                tacticalMovement.speed = (std::max)(
                    tacticalMovement.speed,
                    tactical->peekMoveSpeed);
                tacticalMovement.allowRunning = false;
            }
            else
            {
                // Путь к защищённой точке — аварийный рывок. Нолевой
                // порог включает runSpeed сразу, а не лишь за 8 метров.
                tacticalMovement.allowRunning = npcMovement.allowRunning;
                tacticalMovement.runWhenFartherThan = 0.0f;
            }
            const Entity room = FindContainingRoom(registry, tactical->moveTarget);
            const NavigationResult result = NavigateTo(registry, npc, brain,
                transform, movement, tacticalMovement, tactical->moveTarget,
                room, npcMovement.arrivalDistance, NpcState::TakingCover, deltaTime);
            if (result == NavigationResult::Arrived)
            {
                StopMovement(movement);
                if (tactical->movingToPeek)
                {
                    tactical->wantsPeek = false;
                    tactical->isPeeking = true;
                    tactical->peekState = NpcTacticalComponent::PeekSide::left;
                    tactical->exposed = true;
                    tactical->crouched = false;
                    // Окно огня включает время прицеливания и короткую очередь.
                    // exposeDuration больше не перекрывается одинаковой для
                    // всех константой: стили действительно получают разный ритм.
                    tactical->phaseTimer = (std::max)(
                        0.35f,
                        combat->rangedAimDelay + tactical->exposeDuration);
                    tactical->status = "Exposed: firing from cover edge";
                    brain.state = NpcState::Attacking;
                }
                else
                {
                    tactical->wantsPeek = false;
                    tactical->isPeeking = false;
                    tactical->peekState = NpcTacticalComponent::PeekSide::none;
                    tactical->exposed = false;
                    tactical->crouched = tactical->coverUsesCrouch;
                    // Небольшое детерминированное смещение не даёт двум NPC
                    // синхронно повторять один и тот же peek-цикл.
                    const float cadenceOffset = 0.15f * static_cast<float>(
                        (static_cast<unsigned int>(npc) +
                            static_cast<unsigned int>(tactical->completedPeekCount)) % 3u);
                    tactical->phaseTimer = (std::max)(
                        0.1f,
                        tactical->hideDuration + cadenceOffset);
                    tactical->status = "Protected: waiting for next peek";
                    brain.state = NpcState::InCover;
                }
            }
            else if (result == NavigationResult::Blocked)
            {
                NpcCoverLifecycle::Clear(*tactical);
                tactical->status = "Маршрут к укрытию заблокирован: ищу новый";
                brain.state = NpcState::Chasing;
            }
            break;
        }
        case NpcState::InCover:
        {
            StopMovement(movement);
            auto* tactical = registry.GetComponent<NpcTacticalComponent>(npc);
            auto* combat = registry.GetComponent<NPCCombatComponent>(npc);
            if (!tactical || !combat)
            {
                brain.state = NpcState::Searching;
                break;
            }
            if (!combat->hasLastKnownPosition)
            {
                // После полного забывания цели защитник не бросает хорошее
                // укрытие ради обычного wandering. Он остаётся за стеной до
                // нового визуального/звукового контакта.
                if (tactical->activeStyle == NpcTacticalStyle::Defensive &&
                    (tactical->reservedCover != INVALID_ENTITY ||
                        tactical->dynamicCoverActive))
                {
                    brain.status = "Оборона: удерживаю укрытие после потери контакта";
                    break;
                }

                brain.state = NpcState::Searching;
                break;
            }
            // Корпус следит за последней известной точкой: будущая crouch/lean
            // анимация получит уже правильное направление персонажа.
            const float dx = combat->lastKnownPosition.x - transform.position.x;
            const float dz = combat->lastKnownPosition.z - transform.position.z;
            if (dx*dx + dz*dz > 0.001f)
            {
                const float targetYaw = std::atan2(dx, dz) + DirectX::XM_PI;
                const float difference = std::atan2(std::sin(targetYaw-transform.rotation.y),
                    std::cos(targetYaw-transform.rotation.y));
                const float step = movement.rotationSpeed * deltaTime;
                transform.rotation.y += std::clamp(difference, -step, step);
            }
            break;
        }
        case NpcState::Chasing:
        {
            auto* combat =
                registry.GetComponent<NPCCombatComponent>(npc);
            const auto* combatModules =
                registry.GetComponent<NpcCombatModulesComponent>(npc);
            const bool pursuitEnabled = NpcCombatModuleOperational(
                combatModules,
                NpcCombatModule::Pursuit);
            const bool engagementEnabled = NpcCombatModuleOperational(
                combatModules,
                NpcCombatModule::Engagement);
            const bool searchEnabled = NpcCombatModuleOperational(
                combatModules,
                NpcCombatModule::Search);

            // Это постоянный входной gate, а не только очистка на момент
            // checkbox. Старые переходы больше не могут заново включить
            // Chasing после отключения модуля.
            if (!pursuitEnabled)
            {
                StopMovement(movement);
                if (combat && combat->targetVisible && engagementEnabled)
                    brain.state = NpcState::Attacking;
                else if (combat && combat->hasLastKnownPosition && searchEnabled)
                    brain.state = NpcState::Searching;
                else
                {
                    if (combat && !combat->targetVisible)
                        ForgetLostCombatTarget(*combat,
                            registry.GetComponent<NpcTacticalComponent>(npc));
                    brain.state = NpcState::Idle;
                }
                brain.status = "Pursuit module disabled";
                NpcNavMeshSystem::InvalidatePath(npc);
                break;
            }

            const auto* rangedWeapon = registry.GetComponent<WeaponComponent>(
                WeaponSystem::GetEquippedWeaponEntity(registry, npc));
            const bool hasRangedWeapon = rangedWeapon != nullptr;

            if (!combat ||
                combat->targetEntity == INVALID_ENTITY ||
                !registry.IsAlive(combat->targetEntity))
            {
                brain.state = NpcState::Idle;
                StopMovement(movement);
                break;
            }

            auto* targetTransform =
                registry.GetComponent<TransformComponent>(combat->targetEntity);

            if (!targetTransform)
            {
                brain.state = NpcState::Idle;
                StopMovement(movement);
                break;
            }

            if (!combat->hasLastKnownPosition)
            {
				StopMovement(movement);
                brain.state = NpcState::Idle;
                break;
            }


            const Entity targetRoom =
                FindContainingRoom(registry, combat->lastKnownPosition);

            // Возобновляем погоню обычным бегом: незавершённый Punch иначе останется
            // на слое верхней части тела. Сюда попадает только безоружная погоня.
            if (auto* anim = registry.GetComponent<AnimationComponent>(npc);
                anim && anim->oneShotState == AnimationState::Punch)
            {
                anim->oneShotRequested = false;
                anim->oneShotTimeRemaining = 0.0f;
                anim->poseVariant = -1;
            }
            NPCMovementComponent chaseMovement = npcMovement;
            if (!hasRangedWeapon)
            {
                chaseMovement.allowRunning = true;
                chaseMovement.runWhenFartherThan = 0.0f;
            }
            const float engagementRange = hasRangedWeapon
                ? (std::max)(0.0f, (std::min)(combat->rangedAttackRange, rangedWeapon->range))
                : combat->attackRange;
            auto* chaseTactical = registry.GetComponent<NpcTacticalComponent>(npc);

            // Защитник не копирует штурмовую реакцию «цель исчезла — бежать к
            // последней позиции». Пока CoverSystem ищет подходящую стену, он
            // удерживает дистанцию. Если контакт потерян, переходит в Searching,
            // где Defensive не получает наступательных поисковых секторов.
            // Не привязываем запрет погони к текущему указателю оружия: во
            // время перезарядки/смены слота он может кратко отсутствовать, но
            // выбранная оборонительная тактика от этого не становится штурмовой.
            if (chaseTactical &&
                chaseTactical->activeStyle == NpcTacticalStyle::Defensive)
            {
                StopMovement(movement);
                // Удаляем любой маршрут, который мог быть создан предыдущим
                // Balanced/Assault-решением до переключения на Defensive.
                chaseTactical->hasFlankTarget = false;
                chaseTactical->searchingSector = false;
                combat->investigatingPrediction = false;
                combat->searchPlanReady = true;
                combat->searchTravelTime = 0.0f;
                combat->searchTurnRemaining = 0.0f;
                NpcNavMeshSystem::InvalidatePath(npc);
                brain.status = combat->targetVisible
                    ? "Оборона: удерживаю дистанцию и жду укрытие"
                    : "Оборона: не преследую скрытую цель";
                if (!combat->targetVisible)
                {
                    if (searchEnabled)
                        brain.state = NpcState::Searching;
                    else
                    {
                        ForgetLostCombatTarget(*combat, chaseTactical);
                        brain.state = NpcState::Idle;
                    }
                }
                break;
            }

            const bool flanking = hasRangedWeapon &&
                chaseTactical && chaseTactical->hasFlankTarget;
            const auto chaseTarget = flanking ? chaseTactical->flankTarget : combat->lastKnownPosition;
            const float chaseDx = chaseTarget.x - transform.position.x;
            const float chaseDz = chaseTarget.z - transform.position.z;
            const float chaseDistance = std::sqrt(
                chaseDx * chaseDx + chaseDz * chaseDz);
            const float combatRunDistance = (std::min)(
                npcMovement.runWhenFartherThan,
                4.0f);
            const bool runDuringChase = npcMovement.allowRunning &&
                (!hasRangedWeapon ||
                    NpcCombatDecisionRules::ShouldRunDuringCombat(
                        NpcState::Chasing,
                        false,
                        chaseTactical && chaseTactical->underFireTime > 0.0f,
                        chaseDistance,
                        combatRunDistance));
            // В бою NPC бежит уже на средней дистанции, а под
            // огнём — на любом ненулевом участке. Около самой цели
            // он снова переходит на шаг, чтобы не проскочить точку.
            chaseMovement.allowRunning = runDuringChase;
            if (runDuringChase)
                chaseMovement.runWhenFartherThan = 0.0f;
            // Стрелок без подходящего обхода останавливается на дистанции и
            // начинает поиск. Вплотную к памяти цели бегут только безоружные.
            // Стрелок не обязан подходить на максимальную дальность оружия.
            // preferredRange задаёт удобную дистанцию, а небольшой запас не
            // позволяет состояниям Chasing/Attacking дрожать на одной границе.
            const float preferredFireRange = hasRangedWeapon && chaseTactical
                ? (std::max)(3.0f, (std::min)(
                    chaseTactical->preferredRange, engagementRange))
                : engagementRange;
            const float holdDistance = (std::max)(
                3.0f,
                preferredFireRange - 0.75f);
            const float arrivalRange = flanking ? npcMovement.arrivalDistance :
                hasRangedWeapon ? holdDistance :
                combat->targetVisible ? engagementRange : npcMovement.arrivalDistance;
            const NavigationResult result = NavigateTo(
				registry, npc, brain, transform, movement, chaseMovement,
                chaseTarget, flanking ? FindContainingRoom(registry, chaseTarget) : targetRoom,
                arrivalRange,
				NpcState::Chasing, deltaTime);
            if (flanking && result != NavigationResult::Moving)
            {
                // Засчитываем сектор только после прибытия, не при выдаче приказа.
                if (chaseTactical->searchingSector)
                {
                    if (result == NavigationResult::Arrived)
                    {
                        ++chaseTactical->searchSectorsAssigned;
                        chaseTactical->lastSearchDestination = chaseTactical->flankTarget;
                        chaseTactical->hasLastSearchDestination = true;
                    }
                    else { ++chaseTactical->searchRouteFailures; chaseTactical->searchRetryTime = 1.0f; }
                    chaseTactical->searchingSector = false;
                }
                chaseTactical->hasFlankTarget = false;
            }

            if (result == NavigationResult::Blocked && !combat->targetVisible)
            {
                // Если последнее место уже недоступно, не зависаем в погоне:
                // проверим доступное продолжение и затем завершим поиск.
                StopMovement(movement);
                if (searchEnabled)
                {
                    brain.state = NpcState::Searching;
                    combat->searchPlanReady = false;
                    combat->timeSinceTargetSeen = 0.0f;
                }
                else
                {
                    ForgetLostCombatTarget(*combat, chaseTactical);
                    brain.state = NpcState::Idle;
                }
            }
            else if (result == NavigationResult::Arrived)
            {
                StopMovement(movement);
                if (combat->targetVisible)
                {
                    if (engagementEnabled)
                    {
                        if (!hasRangedWeapon)
                            combat->EnterMeleeStance();
                        brain.state = NpcState::Attacking;
                    }
                    else
                    {
                        brain.status =
                            "Engagement module disabled; holding at range";
                    }
                    break;
                }
                else {
                    // Дошли до последнего наблюдения. Теперь можно проверить
                    // продолжение пути и, если игрока нет, подождать на месте.
                    if (searchEnabled)
                    {
                        brain.state = NpcState::Searching;
					    combat->timeSinceTargetSeen = 0.0f;
                        combat->searchPlanReady = false;
                    }
                    else
                    {
                        ForgetLostCombatTarget(*combat, chaseTactical);
                        brain.state = NpcState::Idle;
                    }
                    break;
                }
            }
            break;
        }
        case NpcState::Searching:
        {
            auto* combat = registry.GetComponent<NPCCombatComponent>(npc);
            const auto* combatModules =
                registry.GetComponent<NpcCombatModulesComponent>(npc);
            auto* searchTactical =
                registry.GetComponent<NpcTacticalComponent>(npc);
            StopMovement(movement);
            if (!NpcCombatModuleOperational(
                combatModules,
                NpcCombatModule::Search))
            {
                if (combat)
                    ForgetLostCombatTarget(*combat, searchTactical);
                brain.status = "Search module disabled";
                brain.state = NpcState::Idle;
                NpcNavMeshSystem::InvalidatePath(npc);
                break;
            }
            if (!combat || !combat->hasLastKnownPosition ||
                !registry.IsAlive(combat->targetEntity))
            {
                if (combat)
                {
                    combat->targetEntity = INVALID_ENTITY;
                    combat->targetVisible = false;
                    combat->hasLastKnownPosition = false;
                    combat->investigatingPrediction = false;
                    combat->searchPlanReady = false;
                }
                brain.state = NpcState::Idle;
                break;
            }
            if (combat->targetVisible)
            {
                if (NpcCombatModuleOperational(
                    combatModules,
                    NpcCombatModule::Pursuit))
                {
                    brain.state = NpcState::Chasing;
                }
                else if (NpcCombatModuleOperational(
                    combatModules,
                    NpcCombatModule::Engagement))
                {
                    brain.state = NpcState::Attacking;
                }
                else
                {
                    brain.state = NpcState::Idle;
                }
                break;
            }

            // Defensive не исследует последнюю известную точку ногами. Даже
            // если прогноз или сектор были назначены в предыдущем стиле, здесь
            // они отменяются до вызова NavigateTo. NPC остаётся на позиции и
            // поворачивается в сторону последнего подтверждённого контакта.
            if (searchTactical &&
                searchTactical->activeStyle == NpcTacticalStyle::Defensive)
            {
                const bool hadOffensiveSearchOrder =
                    combat->investigatingPrediction ||
                    searchTactical->hasFlankTarget ||
                    searchTactical->searchingSector;

                combat->investigatingPrediction = false;
                combat->searchPlanReady = true;
                combat->searchTravelTime = 0.0f;
                combat->searchTurnRemaining = 0.0f;
                searchTactical->hasFlankTarget = false;
                searchTactical->searchingSector = false;

                if (hadOffensiveSearchOrder)
                {
                    NpcNavMeshSystem::InvalidatePath(npc);
                }

                const float dangerDx =
                    combat->lastKnownPosition.x - transform.position.x;
                const float dangerDz =
                    combat->lastKnownPosition.z - transform.position.z;
                if (dangerDx * dangerDx + dangerDz * dangerDz > 0.001f)
                {
                    const float dangerYaw =
                        std::atan2(dangerDx, dangerDz) + DirectX::XM_PI;
                    const float yawDifference = std::atan2(
                        std::sin(dangerYaw - transform.rotation.y),
                        std::cos(dangerYaw - transform.rotation.y));
                    const float yawStep = (std::max)(
                        0.0f,
                        movement.rotationSpeed * deltaTime);
                    transform.rotation.y += std::clamp(
                        yawDifference,
                        -yawStep,
                        yawStep);
                }

                brain.status =
                    "Оборона: наблюдаю последнюю позицию, не преследую";
                break;
            }

            // Раньше этот таймер уменьшал отключённый групповой координатор.
            // Теперь индивидуальный поиск сам отвечает за повторные попытки.
            if (searchTactical)
            {
                searchTactical->searchRetryTime = (std::max)(
                    0.0f,
                    searchTactical->searchRetryTime - deltaTime);
            }
            if (!combat->searchPlanReady)
            {
                // Строим план один раз на одну потерю цели. Повторять прогноз
                // каждый кадр нельзя: иначе NPC бесконечно двигал бы свою цель.
                combat->searchPlanReady = true;
                combat->searchTravelTime = 0.0f;
                combat->BeginSearchTurn();
                combat->timeSinceTargetSeen = 0.0f;
                if (auto* anim = registry.GetComponent<AnimationComponent>(npc))
                {
                    anim->oneShotRequested = false;
                    anim->oneShotTimeRemaining = 0.0f;
                }
                // Защитник не выбегает из позиции вслед за первой догадкой.
                // Штурмовик и сбалансированный NPC сначала проверяют короткое
                // продолжение последнего наблюдавшегося движения цели.
                const bool mayInvestigatePrediction =
                    !searchTactical ||
                    searchTactical->activeStyle != NpcTacticalStyle::Defensive;
                if (mayInvestigatePrediction)
                {
                    const DirectX::XMFLOAT3 desired =
                        combat->EstimateSearchPosition();
                    const float aheadX =
                        desired.x - combat->lastKnownPosition.x;
                    const float aheadZ =
                        desired.z - combat->lastKnownPosition.z;
                    float bestScore = (std::numeric_limits<float>::max)();
                    // Если прямое продолжение упирается в стену, проверяем ещё
                    // два направления. Реальную скрытую позицию не читаем.
                    const float angles[] =
                    {
                        0.0f,
                        DirectX::XM_PI / 3.0f,
                        -DirectX::XM_PI / 3.0f
                    };
                    for (float angle : angles)
                    {
                        const DirectX::XMFLOAT3 candidate = {
                            combat->lastKnownPosition.x +
                                aheadX * std::cos(angle) -
                                aheadZ * std::sin(angle),
                            desired.y,
                            combat->lastKnownPosition.z +
                                aheadX * std::sin(angle) +
                                aheadZ * std::cos(angle) };
                        DirectX::XMFLOAT3 reachable{};
                        if (!NpcNavMeshSystem::FindReachablePoint(
                                registry,
                                brain.currentRoom,
                                transform.position,
                                candidate,
                                reachable))
                            continue;
                        const float x = reachable.x - transform.position.x;
                        const float z = reachable.z - transform.position.z;
                        const float progress =
                            (reachable.x - combat->lastKnownPosition.x) * aheadX +
                            (reachable.z - combat->lastKnownPosition.z) * aheadZ;
                        const float offsetX =
                            reachable.x - combat->lastKnownPosition.x;
                        const float offsetZ =
                            reachable.z - combat->lastKnownPosition.z;
                        const float maxDistance = (std::max)(
                            0.0f,
                            combat->maxPredictionDistance);
                        const float errorX = reachable.x - candidate.x;
                        const float errorZ = reachable.z - candidate.z;
                        const float score = errorX*errorX + errorZ*errorZ +
                            (angle == 0.0f ? 0.0f : 0.3f);
                        if (score < bestScore &&
                            offsetX*offsetX + offsetZ*offsetZ <=
                                maxDistance*maxDistance &&
                            progress > 0.05f &&
                            x*x + z*z > npcMovement.arrivalDistance *
                                npcMovement.arrivalDistance)
                        {
                            bestScore = score;
                            combat->searchDestination = reachable;
                            combat->investigatingPrediction = true;
                        }
                    }
                }
            }
            // Групповой координатор временно отключён, поэтому каждый NPC сам
            // исследует рассчитанную точку поиска. Раньше этот участок отменял
            // индивидуальный маршрут стрелка в ожидании приказа отряда.
            if (combat->investigatingPrediction)
            {
                // Предположение проверяем обычной ходьбой через общий маршрут.
                // При неудаче или истечении лимита начинаем ожидание, а не
                // застреваем навсегда в попытке дойти до неверного предположения.
                combat->searchTravelTime += deltaTime;
                NPCMovementComponent searchMovement = npcMovement;
                searchMovement.allowRunning = false;
                const Entity room = FindContainingRoom(registry, combat->searchDestination);
                const NavigationResult result = NavigateTo(registry, npc, brain,
                    transform, movement, searchMovement, combat->searchDestination,
                    room, npcMovement.arrivalDistance, NpcState::Searching, deltaTime);
                if (result == NavigationResult::Arrived || result == NavigationResult::Blocked ||
                    combat->searchTravelTime >= (std::max)(0.0f, combat->predictionTravelTimeout))
                {
                    // Если это был дополнительный сектор, учитываем реальный
                    // результат пути. Сектор считается проверенным только после
                    // прибытия; заблокированный маршрут получает отдельный счётчик.
                    if (searchTactical && searchTactical->searchingSector)
                    {
                        if (result == NavigationResult::Arrived)
                        {
                            ++searchTactical->searchSectorsAssigned;
                            searchTactical->lastSearchDestination =
                                combat->searchDestination;
                            searchTactical->hasLastSearchDestination = true;
                        }
                        else
                        {
                            ++searchTactical->searchRouteFailures;
                            searchTactical->searchRetryTime = 0.35f;
                        }
                        searchTactical->searchingSector = false;
                    }
                    combat->investigatingPrediction = false;
                    combat->timeSinceTargetSeen = 0.0f;
                    brain.state = NpcState::Searching;
                    StopMovement(movement);
                }
            }
            if (!combat->investigatingPrediction)
            {
                // После прогнозируемой точки индивидуальный AI сам назначает
                // до трёх секторов вокруг последнего контакта. Раньше эти точки
                // должен был выдавать отключённый NpcSquadCombatSystem, поэтому
                // NPC бесконечно стоял со статусом Searching и счётчиком 0/3.
                bool assignedSector = false;
                const int searchSectorLimit = !searchTactical
                    ? 0
                    : searchTactical->activeStyle == NpcTacticalStyle::Assault
                        ? 3
                        : searchTactical->activeStyle == NpcTacticalStyle::Balanced
                            ? 1
                            : 0;
                if (searchTactical &&
                    searchTactical->searchSectorsAssigned < searchSectorLimit &&
                    searchTactical->searchRouteFailures < 10 &&
                    searchTactical->searchRetryTime <= 0.0f)
                {
                    const int sectorIndex =
                        searchTactical->searchSectorsAssigned;
                    const float baseAngle = std::atan2(
                        combat->lastSeenVelocity.x,
                        combat->lastSeenVelocity.z);
                    const float sectorOffsets[3] =
                    {
                        0.0f,
                        DirectX::XM_PIDIV2,
                        -DirectX::XM_PIDIV2
                    };
                    const float sectorRadius = (std::min)(
                        8.0f,
                        (std::max)(4.0f,
                            combat->contactWanderRadius * 0.40f));

                    // Несколько радиусов дают шанс найти путь, если дальняя
                    // точка оказалась за стеной или вне связного острова NavMesh.
                    for (const float radiusScale : { 1.0f, 0.70f, 0.45f })
                    {
                        const float angle = baseAngle +
                            sectorOffsets[sectorIndex] +
                            static_cast<float>(npc % 5u) * 0.08f;
                        const DirectX::XMFLOAT3 desiredSector{
                            combat->lastKnownPosition.x +
                                std::sin(angle) * sectorRadius * radiusScale,
                            combat->lastKnownPosition.y,
                            combat->lastKnownPosition.z +
                                std::cos(angle) * sectorRadius * radiusScale
                        };
                        DirectX::XMFLOAT3 reachableSector{};
                        if (!NpcNavMeshSystem::FindReachablePoint(
                                registry,
                                brain.currentRoom,
                                transform.position,
                                desiredSector,
                                reachableSector))
                        {
                            continue;
                        }

                        const float moveDx =
                            reachableSector.x - transform.position.x;
                        const float moveDz =
                            reachableSector.z - transform.position.z;
                        if (moveDx * moveDx + moveDz * moveDz <=
                            npcMovement.arrivalDistance *
                            npcMovement.arrivalDistance)
                        {
                            continue;
                        }

                        if (searchTactical->hasLastSearchDestination)
                        {
                            const float oldDx = reachableSector.x -
                                searchTactical->lastSearchDestination.x;
                            const float oldDz = reachableSector.z -
                                searchTactical->lastSearchDestination.z;
                            if (oldDx * oldDx + oldDz * oldDz < 1.0f)
                                continue;
                        }

                        combat->searchDestination = reachableSector;
                        combat->searchTravelTime = 0.0f;
                        combat->investigatingPrediction = true;
                        searchTactical->searchingSector = true;
                        NpcNavMeshSystem::InvalidatePath(npc);
                        assignedSector = true;
                        break;
                    }

                    if (!assignedSector)
                    {
                        ++searchTactical->searchRouteFailures;
                        searchTactical->searchRetryTime = 0.35f;
                    }
                }

                if (!assignedSector)
                {
                    // Между попытками NPC не смотрит в случайную сторону, а
                    // держит последний опасный сектор перед собой.
                    const float dx = combat->lastKnownPosition.x-transform.position.x;
                    const float dz = combat->lastKnownPosition.z-transform.position.z;
                    if (dx*dx+dz*dz > 0.001f)
                    {
                        const float yaw = std::atan2(dx,dz) + DirectX::XM_PI;
                        const float difference = std::atan2(
                            std::sin(yaw-transform.rotation.y),
                            std::cos(yaw-transform.rotation.y));
                        const float step = (std::max)(
                            0.0f,
                            movement.rotationSpeed*deltaTime);
                        transform.rotation.y += std::clamp(
                            difference,
                            -step,
                            step);
                    }
                    combat->searchTurnRemaining = 0.0f;
                }
            }
            brain.status = combat->investigatingPrediction
                ? (searchTactical && searchTactical->searchingSector
                    ? "Поиск: проверяю следующий сектор"
                    : "Поиск: проверяю предполагаемый путь противника")
                : "Поиск: наблюдаю последнюю известную позицию";
            break;
        }
        case NpcState::Attacking:
        {
            auto* combat =
                registry.GetComponent<NPCCombatComponent>(npc);
            const auto* combatModules =
                registry.GetComponent<NpcCombatModulesComponent>(npc);
            const bool engagementEnabled = NpcCombatModuleOperational(
                combatModules,
                NpcCombatModule::Engagement);
            const bool pursuitEnabled = NpcCombatModuleOperational(
                combatModules,
                NpcCombatModule::Pursuit);
            const bool searchEnabled = NpcCombatModuleOperational(
                combatModules,
                NpcCombatModule::Search);

            // Постоянный gate не позволяет старой машине состояний вернуть
            // запрещённую атаку через кадр после выключения checkbox.
            if (!engagementEnabled)
            {
                StopMovement(movement);
                if (combat && combat->targetVisible && pursuitEnabled)
                    brain.state = NpcState::Chasing;
                else if (combat && combat->hasLastKnownPosition && searchEnabled)
                    brain.state = NpcState::Searching;
                else
                {
                    if (combat && !combat->targetVisible)
                        ForgetLostCombatTarget(*combat,
                            registry.GetComponent<NpcTacticalComponent>(npc));
                    brain.state = NpcState::Idle;
                }
                if (combat)
                    combat->rangedAimTime = 0.0f;
                brain.status = "Engagement module disabled";
                break;
            }

            // Без живой цели атаковать некого.
            if (!combat ||
                combat->targetEntity == INVALID_ENTITY ||
                !registry.IsAlive(combat->targetEntity))
            {
                brain.state = NpcState::Idle;
                StopMovement(movement);
                break;
            }

            if (!combat->targetVisible)
            {
				StopMovement(movement);
				if (pursuitEnabled)
				    brain.state = NpcState::Chasing;
				else if (searchEnabled)
				    brain.state = NpcState::Searching;
				else
				{
				    ForgetLostCombatTarget(*combat,
				        registry.GetComponent<NpcTacticalComponent>(npc));
				    brain.state = NpcState::Idle;
				}
				break;
            }

            auto* targetTransform =
                registry.GetComponent<TransformComponent>(combat->targetEntity);

            if (!targetTransform)
            {
                combat->targetEntity = INVALID_ENTITY;
                brain.state = NpcState::Idle;
                StopMovement(movement);
                break;
            }

            const float dx =
                targetTransform->position.x - transform.position.x;
            const float dy =
                targetTransform->position.y - transform.position.y;
            const float dz =
                targetTransform->position.z - transform.position.z;

            const float distanceSquared =
                dx * dx + dy * dy + dz * dz;

            const float attackRangeSquared =
                combat->attackRange * combat->attackRange;

            // Игрок отошёл — снова начинаем преследование.
            const auto* rangedWeapon = registry.GetComponent<WeaponComponent>(
                WeaponSystem::GetEquippedWeaponEntity(registry, npc));
            const bool unarmed = rangedWeapon == nullptr;
            const float shootingRange = rangedWeapon ? (std::max)(0.0f,
                (std::min)(combat->rangedAttackRange, rangedWeapon->range)) : 0.0f;
            if (unarmed ? !combat->ShouldHoldMeleeStance(distanceSquared, true)
                        : distanceSquared > shootingRange * shootingRange)
            {
                StopMovement(movement);
                if (pursuitEnabled)
                {
                    brain.state = NpcState::Chasing;
                }
                else
                {
                    combat->rangedAimTime = 0.0f;
                    brain.status =
                        "Target outside range; pursuit module disabled";
                }
                break;
            }

            // Stop before facing/attack checks so animation sees an idle lower
            // body immediately on the frame the NPC enters striking distance.
            StopMovement(movement);

            const float targetYaw =
                std::atan2(dx, dz) + DirectX::XM_PI;

            // Находим кратчайший угол поворота.
            float yawDifference =
                targetYaw - transform.rotation.y;

            yawDifference = std::atan2(
                std::sin(yawDifference),
                std::cos(yawDifference));

            // Плавно поворачиваем NPC к цели.
            const float maxRotation =
                movement.rotationSpeed * deltaTime;

            transform.rotation.y += std::clamp(
                yawDifference,
                -maxRotation,
                maxRotation);

            // yawDifference выше вычислен до поворота. Для допуска нужен уже
            // оставшийся угол, иначе на медленном кадре удар может начаться боком.
            const float remainingYaw = std::atan2(
                std::sin(targetYaw - transform.rotation.y),
                std::cos(targetYaw - transform.rotation.y));

            // В отличие от игрока, у NPC нет прицела, который сам направляет
            // удар. Поэтому не начинаем Punch, пока корпус не смотрит на цель.
            // Иначе одноразовый клип фиксирует удар в бок в момент разворота.
            constexpr float attackFacingTolerance = DirectX::XMConvertToRadians(8.0f);
            if (std::fabs(remainingYaw) > attackFacingTolerance)
            {
                combat->rangedAimTime = 0.0f;
                StopMovement(movement);
                break;
            }

            if (!unarmed)
            {
                // С оружием не запрашиваем Punch. Здесь только стоим, смотрим
                // на цель и набираем время прицеливания. Выстрел выполнится
                // после обновления костей, когда известно положение дула.
                if (auto* animation = registry.GetComponent<AnimationComponent>(npc);
                    animation && animation->oneShotState == AnimationState::Punch)
                {
                    animation->oneShotRequested = false;
                    animation->oneShotTimeRemaining = 0.0f;
                }
                if (NpcCombatModuleOperational(
                    combatModules,
                    NpcCombatModule::StationaryFire))
                {
                    combat->rangedAimTime += deltaTime;
                }
                else
                {
                    combat->rangedAimTime = 0.0f;
                    combat->rangedStatus =
                        "Stationary fire module disabled";
                }
                break;
            }

            // Атака пока состоит только из анимации. Таймер тикает даже у NPC
            // без модели, но запрос клипа отправляется только при наличии
            // AnimationComponent — так не будет обращения к нулевому указателю.
            auto* anim = registry.GetComponent<AnimationComponent>(npc);
            combat->attackTimer -= deltaTime;

            if (anim && anim->oneShotState == AnimationState::Punch &&
                (anim->oneShotRequested || anim->oneShotTimeRemaining > 0.0f))
            {
                // Finish the current animation, then leave a recovery gap even
                // when the configured attack cooldown is shorter than the clip.
                combat->attackTimer = (std::max)(combat->attackTimer, 0.15f);
                break;
            }
            if (unarmed && (combat->stanceTimeRemaining > 0.0f ||
                distanceSquared > attackRangeSquared))
                break;

            if (combat->attackTimer <= 0.0f)
            {
                if (anim)
                {
                    // NPC использует тот же набор взвешенных вариантов и pose
                    // keys, что и игрок. Выбор идёт через собственный seed NPC,
                    // поэтому несколько персонажей не обязаны бить синхронно.
                    float totalWeight = 0.0f;
                    for (const auto& variant : anim->punchVariants)
                        if (variant.weight > 0.0f &&
                            std::isfinite(variant.weight))
                            totalWeight += variant.weight;
                    anim->poseVariant = -1;
                    if (totalWeight > 0.0f && std::isfinite(totalWeight))
                    {
                        float roll = Random01(brain) * totalWeight;
                        for (int i = 0;
                            i < static_cast<int>(anim->punchVariants.size()); ++i)
                        {
                            const float weight = anim->punchVariants[i].weight;
                            if (!(weight > 0.0f) || !std::isfinite(weight))
                                continue;
                            anim->poseVariant = i;
                            roll -= weight;
                            if (roll <= 0.0f)
                                break;
                        }
                    }
                    anim->oneShotState = AnimationState::Punch;
                    anim->oneShotRequested = true;
                }
                else
                {
                    DebugHud::Get().Show(
                        "NPC " + std::to_string(npc) +
                            " has no AnimationComponent",
                        DebugHudMessageKind::Info);
                }

                combat->attackTimer = (std::max)(0.15f, combat->attackCooldown);
            }

            StopMovement(movement);
            break;
        }

        default:
            brain.state = NpcState::Idle;
            StopMovement(movement);
            break;
        }
    }

    // Исполняет активный NpcCommand. Одноразовые команды сами завершаются,
    // Stop/Wander/Follow остаются до явной отмены.
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


    // Находит WorkZoneComponent, принадлежащую ремонтируемому оборудованию.
    // Возвращает INVALID_ENTITY, если автор не создал подходящую зону.
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



    // Общий исполнитель ремонта пробоины и оборудования: проверяет цель,
    // находит точку работы, доводит NPC и передаёт прогресс в RepairSystem.
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


        // Настройки члена экипажа влияют не только на выбор, но и на выполнение
        // задачи: инженер с repairSkill=1.5 завершит тот же ремонт на 50% быстрее,
        // а обычный NPC сохранит нейтральный множитель 1.0.
        float repairSkill = 1.0f;
        if (const auto* crew =
            registry.GetComponent<CrewMemberComponent>(npc))
        {
            repairSkill = std::clamp(crew->repairSkill, 0.25f, 2.0f);
        }

        const float progressDelta =
            repairable->requiredProgress *
            deltaTime *
            repairSkill /
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
    

    // Поддерживает бесконечное брожение: выбирает достижимую случайную точку,
    // идёт к ней, ждёт и повторяет цикл.
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
            brain.status = "Брожение: осматриваю окружение";
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
            brain.status = "Брожение: точка достигнута";
            StopMovement(movement);
        }
        else if (result == NavigationResult::Blocked)
        {
            // Случайная точка может оказаться на другой стороне
            // баррикады. Не пытаемся вечно идти в неё: забываем
            // waypoint и на следующем шаге берём другой сектор.
            brain.wanderTargetValid = false;
            brain.wanderPauseTimer = 0.15f;
            brain.status = "Брожение: заблокированная точка отброшена";
            StopMovement(movement);
        }
    }

    // Идёт по NpcPatrolPoint с нужным routeId, соблюдая order, waitTime,
    // loop/pingPong и индивидуальное разрешение бега.
    static void UpdatePatrol(
        ECSRegistry& registry,
        Entity npc,
        NpcBrainComponent& brain,
        const TransformComponent& transform,
        CharacterMovement& movement,
        const NPCMovementComponent& npcMovement,
        NpcPatrolComponent& patrol,
        float deltaTime)
    {
        struct Point
        {
            Entity entity = INVALID_ENTITY;
            int order = 0;
            float waitTime = 0.0f;
            DirectX::XMFLOAT3 position{};
        };

        std::vector<Point> points;
        registry.ForEach<NpcPatrolPointComponent, TransformComponent>(
            [&](Entity entity, NpcPatrolPointComponent& point,
                TransformComponent& pointTransform)
            {
                if (point.enabled && point.routeId == patrol.routeId)
                    points.push_back({ entity, point.order, point.waitTime,
                        pointTransform.position });
            });
        std::sort(points.begin(), points.end(), [](const Point& a, const Point& b)
        {
            return a.order != b.order ? a.order < b.order : a.entity < b.entity;
        });

        if (points.empty())
        {
            patrol.status = "No enabled Patrol Points for route " +
                std::to_string(patrol.routeId);
            brain.status = patrol.status;
            brain.state = NpcState::Idle;
            StopMovement(movement);
            return;
        }

        patrol.currentIndex = std::clamp(
            patrol.currentIndex, 0, static_cast<int>(points.size()) - 1);
        if (patrol.waitTimer > 0.0f)
        {
            patrol.waitTimer = (std::max)(0.0f, patrol.waitTimer - deltaTime);
            patrol.status = "Waiting at patrol point";
            brain.status = patrol.status;
            brain.state = NpcState::Patrolling;
            StopMovement(movement);
            return;
        }

        const Point& target = points[static_cast<size_t>(patrol.currentIndex)];
        NPCMovementComponent patrolMovement = npcMovement;
        patrolMovement.allowRunning = patrol.allowRunning;
        const Entity room = FindContainingRoom(registry, target.position);
        brain.state = NpcState::Patrolling;
        patrol.status = "Moving to patrol point " + std::to_string(target.order);
        brain.status = patrol.status;
        const NavigationResult result = NavigateTo(
            registry, npc, brain, transform, movement, patrolMovement,
            target.position, room, (std::max)(0.05f, patrol.arrivalDistance),
            NpcState::Patrolling, deltaTime);
        if (result != NavigationResult::Arrived)
            return;

        patrol.waitTimer = (std::max)(0.0f,
            target.waitTime >= 0.0f ? target.waitTime : patrol.defaultWaitTime);
        const int last = static_cast<int>(points.size()) - 1;
        if (patrol.pingPong && points.size() > 1)
        {
            if (patrol.currentIndex >= last) patrol.direction = -1;
            else if (patrol.currentIndex <= 0) patrol.direction = 1;
            patrol.currentIndex += patrol.direction;
        }
        else if (patrol.currentIndex < last)
            ++patrol.currentIndex;
        else if (patrol.loop)
            patrol.currentIndex = 0;
        else
        {
            patrol.enabled = false;
            patrol.status = "Patrol completed";
            brain.status = patrol.status;
            brain.state = NpcState::Idle;
        }
        StopMovement(movement);
    }

    // Главная двухуровневая навигация. Сначала проводит через граф комнат и
    // дверей, затем внутри комнаты запрашивает NavMesh waypoint или идёт прямо.
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
        // Объём комнаты назначения может начинаться прямо внутри дверного проёма.
        // Поэтому активный переход сохраняем даже после смены currentRoom: иначе
        // маршрут сбросится, пока капсула NPC ещё стоит на пороге. Сам дверной шаг
        // ниже решит, когда NPC достаточно далеко вышел в соседнюю комнату.
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

                // Сначала центрируем капсулу со стороны исходной комнаты. Иначе NPC,
                // пришедший к двери по диагонали, пытается пройти сквозь косяк или
                // выступ порога, хотя при прямом подходе проход свободен.
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

                // Выравниваемся, только пока капсула остаётся со стороны исходной
                // комнаты относительно плоскости подхода. Старая проверка
                // progressBeyondDoor < 0.05 оставалась истинной почти до центра
                // двери: NPC уже проходил точку подхода, затем получал приказ
                // вернуться к ней, а следующим кадром снова шёл вперёд.
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
                // Точка выхода специально лежит в соседней комнате, поэтому
                // NavMesh одной комнаты не может провести этот короткий отрезок.
                // Пересекаем проём напрямую вдоль оси двери, а затем снова отдаём
                // движение Detour уже внутри комнаты назначения.
                NpcNavMeshSystem::InvalidatePath(npc);
                // Коллайдер рамы/створки открытой двери может совпасть со швом полов
                // разной высоты. Тогда Bullet зажимает округлую капсулу на пороге,
                // хотя визуально проход свободен. Только на время прямого перехода
                // игнорируем именно эту открытую дверь; следующий обычный кадр
                // восстановит обе пары проверки столкновений.
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

    // BFS по RoomComponent/DoorComponent. Заполняет параллельные массивы
    // doorRoute и doorDestinationRooms от текущей комнаты до цели.
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

    // Находит RoomComponent, чьи мировые bounds содержат переданную позицию.
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

    // Вычисляет мировой центр локального объёма комнаты.
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

    // Возвращает мировую позицию корня двери для подхода к порталу.
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

    // Строит безопасную точку уже ЗА дверью в направлении комнаты назначения.
    // Это не даёт capsule поворачивать в косяке до полного выхода.
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

        // Направление к центру комнаты нельзя считать нормалью портала: дверь у
        // угла даст диагональный вектор, и капсула упрётся в косяк. Вместо этого
        // находим ближайшую боковую грань комнаты назначения. Её внутренняя нормаль
        // задаёт устойчивую ось прохода даже у повёрнутых комнат.
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

    // Выбирает псевдослучайную точку внутри комнаты/радиуса и по возможности
    // проецирует её на достижимый остров NavMesh.
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

        // На открытой локации выбираем цель на том же связном острове NavMesh.
        // Если слой выключен или поверхность ещё не настроена, сохраняем
        // прежнее поведение со случайной точкой вокруг anchor.
        DirectX::XMFLOAT3 fallbackTarget = currentPosition;
        for (int attempt = 0; attempt < 12; ++attempt)
        {
            const float angle = Random01(brain) * DirectX::XM_2PI;
            const float radius = 1.0f + Random01(brain) * (std::max)(1.0f, brain.wanderRadius);
            fallbackTarget = {
                brain.wanderAnchor.x + std::sin(angle) * radius,
                currentPosition.y,
                brain.wanderAnchor.z + std::cos(angle) * radius
            };
            DirectX::XMFLOAT3 reachableTarget{};
            if (NpcNavMeshSystem::FindReachablePoint(
                    registry, INVALID_ENTITY, currentPosition,
                    fallbackTarget, reachableTarget) &&
                HorizontalDistance(currentPosition, reachableTarget) >= 1.0f)
                return reachableTarget;
        }
        return fallbackTarget;
    }

    // Дешёвый детерминированный генератор 0..1 с отдельным seed каждого NPC.
    static float Random01(NpcBrainComponent& brain)
    {
        // Простой LCG. Для gameplay-рандома его достаточно; для криптографии — нет.
        // Возвращаем равномерное число в полуинтервале [0, 1).
        brain.randomState = brain.randomState * 1664525u + 1013904223u;
        return static_cast<float>((brain.randomState >> 8) & 0x00FFFFFFu) /
            static_cast<float>(0x01000000u);
    }

    // Проверяет питание двери. Старые двери без PowerConsumer считаются
    // запитанными для обратной совместимости сцен.
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

    // Временно отключает столкновение capsule только с активной дверью и её
    // створками при проходе, затем гарантированно восстанавливает фильтры.
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

            // Bullet не удаляет дубликаты из списка игнорирования. Поэтому helper
            // вызывается только при смене отслеживаемой двери, а не каждый кадр
            // нахождения NPC внутри проёма.
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

    // Запрашивает локальный NavMesh waypoint и при успехе передаёт его в
    // MoveTowards; при неудаче использует прямую конечную цель.
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

    // Преобразует мировую цель в нормализованное направление CharacterMovement
    // и выбирает Walk/Run. Сам Transform здесь не изменяется.
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

    // Полностью обнуляет намерение движения и ставит Idle-анимацию.
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

    static void ForgetLostCombatTarget(
        NPCCombatComponent& combat,
        NpcTacticalComponent* tactical)
    {
        combat.ForgetTarget();
        if (!tactical)
            return;

        tactical->searchingSector = false;
        tactical->hasFlankTarget = false;
        tactical->searchSectorsAssigned = 0;
        tactical->searchRouteFailures = 0;
        tactical->searchRetryTime = 0.0f;
        tactical->hasLastSearchDestination = false;
    }

    // Успешно завершает одноразовую ручную команду и возвращает Utility AI.
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

    // Останавливает NPC, переводит в Blocked и сохраняет понятную причину.
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

    // Сравнивает фактическую позицию с предыдущей. Если NPC долго хочет идти,
    // но не сдвигается, инвалидирует маршрут и просит перепланирование.
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

    // Расстояние в плоскости XZ: перепад высоты не мешает достижению точки пола.
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
