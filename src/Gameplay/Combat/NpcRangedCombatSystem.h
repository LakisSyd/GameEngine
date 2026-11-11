#pragma once

// WeaponSystem находит экипированное оружие и выполняет сам выстрел.
#include "WeaponSystem.h"

// Здесь объявлены компоненты NPC, его мозга, тактики и боевого состояния.
#include "NPCComponents.h"

// Общие правила определяют, разрешена ли NPC стрельба во время движения.
#include "Gameplay/AI/NpcCombatDecisionRules.h"
#include "Gameplay/AI/NpcCombatModules.h"

// WeaponIKSystem нужен для вычисления положения оружия в руке NPC.
#include "Animation/WeaponIKSystem.h"

// Эта система НЕ ищет противника и НЕ переводит NPC в состояние атаки.
// Ранее это уже сделали NpcCombatSystem и StateSystem.
//
// Система запускается после обновления позы и физики. Поэтому здесь уже можно
// получить актуальное положение кисти, оружия и его дула.
//
// Её задача — проверить все условия выстрела. Как только одно условие не
// выполнено, обработка текущего NPC немедленно заканчивается через return.
class NpcRangedCombatSystem
{
public:
    // Один проход стрельбы всех NPC.
    // world нужен для проверки линии огня; damageSystem получает штатное
    // событие попадания; device/jobs нужны WeaponSystem для визуальных/фоновых
    // операций; debugRenderer/settings только показывают лучи диагностики.
    static void Update(
        ECSRegistry& registry,
        btDynamicsWorld* world,
        DamageSystem& damageSystem,
        ID3D11Device* device,
        JobSystem& jobs,
        PhysicsDebugRenderer* debugRenderer,
        const PhysicsDebugSettings& debugSettings)
    {
        // Перебираем сущности, которые одновременно являются NPC
        // и имеют данные дальнего боя.
        registry.ForEach<NPCComponent, NPCCombatComponent>(
            [&](Entity npc, NPCComponent&, NPCCombatComponent& combat)
            {
                // npc    — текущий стрелок.
                // combat — его цель, видимость, таймер прицеливания и статус.

                // Статус по умолчанию на начало кадра. Каждая причина отказа
                // ниже может заменить его более точным сообщением.
                combat.rangedStatus = "Not shooting";

                // Получаем мозг текущего NPC, чтобы узнать его состояние
                // и проверить наличие внешней команды.
                const auto* brainPointer =
                    registry.GetComponent<NpcBrainComponent>(npc);

                // Без мозга система не знает, разрешено ли NPC стрелять.
                if (!brainPointer)
                {
                    return;
                }

                // Короткая ссылка делает следующие обращения читаемее.
                const auto& brain = *brainPointer;

                // Без физического мира невозможно проверить дуло и линию огня.
                if (!world)
                {
                    return;
                }

                // Выключенная сущность не должна участвовать в бою.
                if (!registry.HasComponent<ActiveComponent>(npc))
                {
                    return;
                }

                // Этот проход предназначен для NPC, но не для сущности игрока.
                if (registry.HasComponent<PlayerGameTag>(npc))
                {
                    return;
                }

                // Стрелок обязан иметь фракцию. Старый флаг NPCComponent::isHostile
                // здесь больше не используется: отношение определяется для пары
                // «стрелок — текущая цель», а не один раз для самого NPC.
                const auto* observerFaction =
                    registry.GetComponent<FactionComponent>(npc);

                if (!observerFaction)
                {
                    combat.rangedStatus = "Shooter has no faction";
                    return;
                }

                // Мёртвый NPC не может стрелять.
                const auto* vitals =
                    registry.GetComponent<CharacterVitalsComponent>(npc);
                if (!vitals || vitals->health <= 0.0f)
                {
                    return;
                }

                // Активная внешняя команда имеет приоритет над стрельбой.
                if (brain.command.IsActive())
                {
                    return;
                }

                // Сначала находим сущность экипированного оружия, затем берём
                // находящийся на ней WeaponComponent.
                auto* weapon = registry.GetComponent<WeaponComponent>(
                    WeaponSystem::GetEquippedWeaponEntity(registry, npc));

                // NPC без экипированного оружия не может продолжить проверку.
                if (!weapon)
                {
                    combat.rangedStatus = "Unarmed";
                    return;
                }

                // Reloading is weapon state, not an NPC tactical decision.
                // The cover layer will deliberately react to it later.
                if (weapon->isReloading)
                {
                    combat.rangedStatus = "Reloading";
                    return;
                }

                if (weapon->ammoInMagazine <= 0)
                {
                    const bool started =
                        WeaponSystem::RequestReload(registry, npc);
                    combat.rangedStatus = started
                        ? "Reloading"
                        : "Out of ammunition";
                    return;
                }

                // Тактический компонент хранит сведения об обходе противника
                // и таймер прицеливания во время движения. Он необязателен.
                auto* tactical =
                    registry.GetComponent<NpcTacticalComponent>(npc);

                const auto* modules =
                    registry.GetComponent<NpcCombatModulesComponent>(npc);

                // Стрельба на ходу использует отдельный таймер прицеливания,
                // поэтому без NpcTacticalComponent этот режим недоступен.
                // Разрешены только боевые состояния Chasing/TakingCover и
                // только лично видимая в текущем кадре цель.
                const bool engagementEnabled =
                    NpcCombatModuleOperational(
                        modules,
                        NpcCombatModule::Engagement);
                const bool movingFireEnabled =
                    NpcCombatModuleOperational(
                        modules,
                        NpcCombatModule::MovingFire);
                const bool stationaryFireEnabled =
                    NpcCombatModuleOperational(
                        modules,
                        NpcCombatModule::StationaryFire);
                const bool movingFire = engagementEnabled &&
                    movingFireEnabled && tactical &&
                    NpcCombatDecisionRules::CanUseMovingFire(
                        brain.state,
                        combat.targetVisible);

                if (!engagementEnabled)
                {
                    combat.rangedStatus = "Engagement module disabled";
                    return;
                }

                if (!movingFire && !stationaryFireEnabled)
                {
                    combat.rangedStatus = "Stationary fire module disabled";
                    return;
                }

                // На ходу оружие работает короткими очередями. Это не
                // групповое подавление: каждый NPC сам ограничивает
                // свою очередь и не знает о ролях союзников.
                if (movingFire && tactical->movingBurstPauseTimer > 0.0f)
                {
                    combat.rangedStatus = "Moving: pause between bursts";
                    return;
                }

                if (movingFire && tactical->movingBurstShotsRemaining <= 0)
                {
                    tactical->movingBurstShotsRemaining = (std::max)(
                        1,
                        tactical->movingBurstSize);
                }

                if (!movingFire && tactical)
                {
                    // Стационарная стрельба живёт по обычному fireRate.
                    // Старый остаток moving-очереди ей не нужен.
                    tactical->movingBurstShotsRemaining = 0;
                    tactical->movingBurstPauseTimer = 0.0f;
                }

                // Без movingFire NPC обязан быть в состоянии Attacking.
                // В обоих случаях он обязан лично видеть цель в этом кадре.
                if ((!movingFire && brain.state != NpcState::Attacking) || !combat.targetVisible)
                {
                    combat.rangedStatus = "No visible target in firing stance";
                    return;
                }

                // Для стрельбы на ходу берётся movingAimTime, для обычной атаки
                // — rangedAimTime. До окончания задержки NPC только целится.
                if ((movingFire ? tactical->movingAimTime : combat.rangedAimTime) < combat.rangedAimDelay)
                {
                    combat.rangedStatus = "Aiming";
                    return;
                }

                // targetEntity ранее выбрала система восприятия. Здесь лишь
                // проверяем, существует ли цель всё ещё в Registry.
                if (!registry.IsAlive(combat.targetEntity))
                {
                    return;
                }

                // Существующая, но выключенная цель также не обрабатывается.
                if (!registry.HasComponent<ActiveComponent>(combat.targetEntity))
                {
                    return;
                }

                const bool targetIsCharacter =
                    registry.HasComponent<PlayerComponent>(combat.targetEntity) ||
                    registry.HasComponent<NPCComponent>(combat.targetEntity);
                const auto* targetVitals =
                    registry.GetComponent<CharacterVitalsComponent>(combat.targetEntity);
                const bool targetIsDead = targetIsCharacter &&
                    (!targetVitals || targetVitals->health <= 0.0f);

                if (targetIsDead)
                {
                    combat.rangedStatus = "Target is dead";
                    return;
                }

                // Цель также обязана участвовать в системе фракций.
                const auto* targetFaction =
                    registry.GetComponent<FactionComponent>(combat.targetEntity);

                if (!targetFaction)
                {
                    combat.rangedStatus = "Target has no faction";
                    return;
                }

                // Повторно подтверждаем отношение непосредственно перед
                // выстрелом. Так устаревший targetEntity не приведёт к огню
                // по союзнику после изменения его фракции.
                const bool targetIsEnemy = AreEnemies(
                    observerFaction->faction,
                    targetFaction->faction);

                if (!targetIsEnemy)
                {
                    combat.rangedStatus = "Target is not an enemy";
                    return;
                }

                // Положение цели понадобится для проверки поворота стрелка.
                const auto* target =
                    registry.GetComponent<TransformComponent>(combat.targetEntity);

                // Положение самого NPC понадобится для мировой позиции оружия.
                const auto* transform =
                    registry.GetComponent<TransformComponent>(npc);

                // Компонент движения обязателен по текущему контракту системы.
                const auto* movement =
                    registry.GetComponent<CharacterMovement>(npc);

                // Модель содержит скелет и кость руки, к которой привязано оружие.
                const auto* mesh =
                    registry.GetComponent<MeshRendererComponent>(npc);

                // Анимация содержит актуальную позу этой кости в текущем кадре.
                const auto* animation =
                    registry.GetComponent<AnimationComponent>(npc);

                // Без любого из этих данных нельзя надёжно вычислить положение
                // оружия и направление выстрела.
                if (!target ||
                    !transform ||
                    !movement ||
                    !mesh ||
                    !mesh->model ||
                    !animation)
                {
                    return;
                }

                // Ниже используются матрицы, векторы и функции DirectXMath.
                using namespace DirectX;

                // Здесь будет храниться актуальная матрица кости правой руки.
                XMMATRIX hand;

                // Получаем из текущей анимационной позы кость, указанную оружием.
                if (!mesh->model->TryGetBoneModelTransform(
                        *animation,
                        weapon->rightHandBone,
                        hand))
                {
                    combat.rangedStatus = "Missing right-hand bone";
                    return;
                }

                // origin — мировая позиция дула и начало будущего выстрела.
                XMFLOAT3 origin{};

                // Последовательно переводим локальную точку дула:
                // muzzlePosition -> хват оружия -> кость руки -> мир уровня.
                // Луч камеры игрока NPC здесь не используется.
                XMStoreFloat3(
                    &origin,
                    XMVector3TransformCoord(
                        XMLoadFloat3(&weapon->muzzlePosition),
                        WeaponIKSystem::GripMatrix(weapon->worldGrip) *
                            hand *
                            transform->GetWorldMatrix()));

                // Радиоконтакт не даёт права стрелять: выше проверена личная
                // видимость, а точка выбрана лучом зрения в этом кадре.
                XMFLOAT3 aimPoint = combat.visibleAimPoint;

                // Строим направление от дула к видимой точке на цели.
                XMFLOAT3 direction{
                    aimPoint.x - origin.x,
                    aimPoint.y - origin.y,
                    aimPoint.z - origin.z
                };

                // Отбрасываем нулевой или математически некорректный луч.
                if (!WeaponFiringRules::ValidRay(origin, direction))
                {
                    return;
                }

                // Длина direction равна расстоянию от дула до точки прицеливания.
                const float distance =
                    XMVectorGetX(XMVector3Length(XMLoadFloat3(&direction)));

                // Видимая цель всё равно может быть дальше дальности оружия.
                if (distance > weapon->range)
                {
                    combat.rangedStatus = "Beyond weapon range";
                    return;
                }

                // Вычисляем горизонтальный угол, под которым цель находится
                // относительно позиции самого NPC.
                const float targetYaw =
                    std::atan2(
                        target->position.x - transform->position.x,
                        target->position.z - transform->position.z) +
                    XM_PI;

                // Получаем кратчайшую разницу между текущим и нужным углом.
                const float yawError =
                    std::atan2(
                        std::sin(targetYaw - transform->rotation.y),
                        std::cos(targetYaw - transform->rotation.y));

                // Стрелять можно, только когда корпус почти повёрнут к цели.
                if (std::fabs(yawError) > XMConvertToRadians(8.0f))
                {
                    // При стрельбе на ходу сильная ошибка поворота сбрасывает
                    // накопленное время прицеливания.
                    if (movingFire)
                    {
                        tactical->movingAimTime = 0.0f;
                    }

                    combat.rangedStatus = "Turning toward target";
                    return;
                }

                // Длинный ствол может оказаться за стеной, даже если тело NPC
                // находится перед ней. Поэтому проверяем путь от груди до дула.
                const auto* controller =
                    registry.GetComponent<CharacterController>(npc);

                // Собственное физическое тело исключается из raycast, чтобы NPC
                // не обнаружил самого себя как препятствие.
                const btCollisionObject* ownBody =
                    controller ? controller->ghostObj : nullptr;

                // Создаём приблизительную точку груди из позиции NPC.
                XMFLOAT3 chest = transform->position;

                // Поднимаем её от основания персонажа на высоту груди.
                chest.y += 1.45f;

                // Направление проверочного луча от груди к дулу.
                const XMFLOAT3 toMuzzle{
                    origin.x - chest.x,
                    origin.y - chest.y,
                    origin.z - chest.z
                };

                // Длина участка между грудью и дулом.
                const float muzzleDistance =
                    XMVectorGetX(XMVector3Length(XMLoadFloat3(&toMuzzle)));

                // Почти нулевой луч не проверяем, чтобы избежать погрешностей.
                if (muzzleDistance > 0.01f)
                {
                    // В эту структуру raycast запишет найденное препятствие.
                    PhysicsRaycastHit obstruction{};

                    // Выпускаем луч от груди до дула, игнорируя самого стрелка.
                    PhysicsRaycast::CastRay(
                        chest,
                        toMuzzle,
                        world,
                        muzzleDistance,
                        npc,
                        ownBody,
                        obstruction);

                    // Любое физическое препятствие на этом коротком участке
                    // означает, что дуло оказалось за укрытием.
                    if (obstruction.collisionObject)
                    {
                        combat.rangedStatus = "Muzzle blocked by cover";
                        return;
                    }
                }

                // После проверки дула проверяем всю линию будущего выстрела.
                // Напарник мог пересечь её уже после проверки видимости цели.
                PhysicsRaycastHit fireLane{};

                // Луч идёт от дула до видимой точки прицеливания.
                PhysicsRaycast::CastRay(
                    origin,
                    direction,
                    world,
                    distance,
                    npc,
                    ownBody,
                    fireLane);

                // Если первым пересечён живой NPC дружественной фракции,
                // удерживаем огонь. Вражеская цель эту проверку проходит.
                const auto* blockingNpc =
                    registry.GetComponent<NPCComponent>(fireLane.entity);

                const auto* blockingVitals =
                    registry.GetComponent<CharacterVitalsComponent>(fireLane.entity);

                const auto* allyFaction =
                    registry.GetComponent<FactionComponent>(fireLane.entity);

                const bool friendlyInFireLane =
                    blockingNpc &&
                    blockingVitals &&
                    blockingVitals->health > 0.0f &&
                    allyFaction &&
                    GetFactionRelation(
                        observerFaction->faction,
                        allyFaction->faction) == FactionRelation::Friendly;

                if (friendlyInFireLane)
                {
                    combat.rangedStatus = "Holding fire: ally in firing lane";
                    return;
                }

                // Все проверки разрешили выстрел: удерживаем спуск оружия.
                weapon->triggerHeld = true;

                // WeaponSystem выполняет собственные оружейные проверки,
                // создаёт выстрел и передаёт попадание в DamageSystem.
                const bool fired = WeaponSystem::TryFire(
                    registry,
                    npc,
                    origin,
                    direction,
                    world,
                    damageSystem,
                    debugRenderer,
                    debugSettings,
                    device,
                    jobs);

                // Сохраняем результат для просмотра в отладчике NPC.
                combat.rangedStatus = fired
                    ? "Fired"
                    : weapon->isReloading
                        ? "Reloading"
                    : weapon->currentCooldown > 0.0f
                        ? "Weapon cooling down"
                        : "Weapon unavailable";

                // Если NPC обладает тактическим компонентом, запоминаем успешный
                // выстрел на маршруте. Он засчитывается только для movingFire и
                // только при свободном луче либо попадании луча в текущую цель.
                if (tactical)
                {
                    tactical->firedOnRoute =
                        fired &&
                        movingFire &&
                        (!fireLane.collisionObject ||
                            fireLane.entity == combat.targetEntity);

                    if (fired && movingFire)
                    {
                        --tactical->movingBurstShotsRemaining;
                        if (tactical->movingBurstShotsRemaining <= 0)
                        {
                            // Следующая очередь начнётся лишь после паузы и
                            // нового накопления movingAimTime.
                            tactical->movingBurstPauseTimer = (std::max)(
                                0.0f,
                                tactical->movingBurstPauseDuration);
                            tactical->movingAimTime = 0.0f;
                        }
                    }
                }
            });
    }
};
