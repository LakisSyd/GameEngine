#pragma once

#include <cmath>
#include <limits>
#include <ECSRegistry.h>

#include "NPCComponents.h"
#include "Components.h"
#include "Physics/PhysicsRaycast.h"
#include "PhysicsDebugRenderer.h"
#include "Gameplay/Combat/WeaponComponent.h"
#include "Gameplay/Combat/WeaponSystem.h"
#include "Gameplay/AI/NpcCombatDecisionRules.h"
#include "Gameplay/AI/NpcCombatMath.h"
#include "Gameplay/AI/NpcCombatModules.h"
#include "Gameplay/AI/NpcNavMeshSystem.h"
#include "Animation/AnimationComponent.h"
#include "Render/Model/assimpmodelclass.h"

// =============================================================================
// NpcCombatSystem — зрение, выбор противника и переходы боя
// =============================================================================
//
// Эта система НЕ выпускает пулю и НЕ двигает capsule. За кадр она:
//
//   1. для каждого NPCCombatComponent ищет ближайшего живого врага по фракции;
//   2. проверяет seeRange, fieldOfViewDegrees и физическую видимость лучами;
//   3. запоминает только реально увиденную позицию/скорость цели;
//   4. обновляет targetEntity, targetVisible и visibleAimPoint;
//   5. переводит brain в Chasing/Attacking/Searching;
//   6. накапливает rangedAimTime, когда стрелок устойчиво видит цель.
//
// Затем StateSystem исполняет движение, а NpcRangedCombatSystem отдельно
// проверяет оружие, дуло и безопасную линию огня перед реальным выстрелом.
// Такое разделение важно: «я вижу врага» ещё не означает «можно стрелять».
//
// НИКОГДА не подставляй Transform скрытой цели в lastKnownPosition. Иначе NPC
// начнёт честно рисовать поиск, но фактически будет видеть игрока сквозь стены.
class NpcCombatSystem
{
public:
    // registry — все компоненты мира; deltaTime — секунды текущего кадра;
    // dynamicsWorld — Bullet-мир для лучей видимости; debugRenderer/settings
    // только визуализируют проверки и не влияют на принятие решения.
    static void Update(
        ECSRegistry& registry,
        float deltaTime,
        btDynamicsWorld* dynamicsWorld,
        PhysicsDebugRenderer* debugRenderer,
        const PhysicsDebugSettings& debugSettings)
    {
        

     
		


        

        registry.ForEach<NPCCombatComponent>(
            [&](Entity npcEntity, NPCCombatComponent& combat)
            {

                const auto* modules =
                    registry.GetComponent<NpcCombatModulesComponent>(npcEntity);
                auto* brain =
                    registry.GetComponent<NpcBrainComponent>(npcEntity);
                if (!NpcCombatModuleOperational(
                    modules,
                    NpcCombatModule::Perception))
                {
                    combat.targetVisible = false;
                    combat.rangedAimTime = 0.0f;
                    combat.rangedStatus = "Perception module disabled";
                    if (auto* tactical = registry.GetComponent<
                        NpcTacticalComponent>(npcEntity))
                    {
                        tactical->movingAimTime = 0.0f;
                    }
                    if (brain)
                    {
                        UpdateLostTargetMemory(
                            registry,
                            npcEntity,
                            combat,
                            *brain,
                            deltaTime);
                    }
                    return;
                }

                // npcEntity - это наблюдатель: NPC, который прямо сейчас ищет
                // цель. Переменные выбора находятся внутри этой лямбды, поэтому
                // каждый NPC получает собственную цель, а не общую на весь кадр.
                Entity bestTargetEntity = INVALID_ENTITY;
                float bestTargetDistanceSquared =
                    (std::numeric_limits<float>::max)();

                auto* npcData =
                    registry.GetComponent<NPCComponent>(npcEntity);

                auto* npcTransform =
                    registry.GetComponent<TransformComponent>(npcEntity);

                if (!npcData || !brain || !npcTransform)
                    return;

                if (brain->command.IsActive())
                {
                    combat.rangedAimTime = 0.0f;
                    return;
                }

				// Фракция наблюдателя определяет отношения с каждым кандидатом.
				// Старый isHostile здесь больше не нужен: NPCCombatComponent разрешает
				// участие в бою, а AreEnemies решает, кого можно атаковать.
				const auto* observerFaction =
					registry.GetComponent<FactionComponent>(npcEntity);

				if (!observerFaction)
				{
					combat.targetVisible = false;
					combat.rangedAimTime = 0.0f;
					return;
				}



                registry.ForEach<FactionComponent, TransformComponent>(
                    [&](Entity candidateEntity, FactionComponent& candidateFaction, TransformComponent& candidateTransform)
                    {
                        // candidateEntity - только возможная цель. Она станет
                        // bestTargetEntity лишь после всех проверок ниже.

                        // Наблюдатель не может выбрать целью самого себя.
                        if (candidateEntity == npcEntity)
                            return;

                        // Неактивная Entity выключена из игрового мира.
                        if (!registry.HasComponent<ActiveComponent>(candidateEntity))
                            return;

                        const auto* candidateNpc =
                            registry.GetComponent<NPCComponent>(candidateEntity);

                        const auto* candidatePlayer =
                            registry.GetComponent<PlayerComponent>(candidateEntity);

                        if (!candidateNpc && !candidatePlayer)
                            return;

                        const auto* candidateVitals =
                            registry.GetComponent<CharacterVitalsComponent>(candidateEntity);
                        const bool candidateAlive = candidateVitals &&
                            candidateVitals->health > 0.0f;

                        if (!candidateAlive)
                            return;





                        // Одинаковая и нейтральная фракции не являются врагами.
                        if (!AreEnemies(observerFaction->faction,
                            candidateFaction.faction))
                        {
                            return;
                        }

                        // Для первичного теста выбираем ближайшего живого врага.
                        // Сравниваем квадрат расстояния: sqrt для сортировки не нужен.
                        const float candidateDistanceSquared =
                            NpcCombatMath::DistanceSquared(
                                npcTransform->position,
                                candidateTransform.position);

                        if (candidateDistanceSquared < bestTargetDistanceSquared)
                        {
                            bestTargetDistanceSquared = candidateDistanceSquared;
                            bestTargetEntity = candidateEntity;
                        }
                    });

                // Ни один живой активный враг не найден. Старая Entity больше
                // не должна оставаться доступной системе стрельбы.
                if (bestTargetEntity == INVALID_ENTITY)
                {
                    combat.targetEntity = INVALID_ENTITY;
                    combat.targetVisible = false;
                    combat.rangedAimTime = 0.0f;
                    return;
                }

                // candidateTransform был ссылкой только внутри внутренней
                // лямбды. Здесь заново получаем Transform окончательной цели.
                const auto* targetTransform =
                    registry.GetComponent<TransformComponent>(bestTargetEntity);

                if (!targetTransform)
                {
                    combat.targetVisible = false;
                    combat.rangedAimTime = 0.0f;
                    return;
                }

                combat.stanceTimeRemaining = (std::max)(0.0f,
                    combat.stanceTimeRemaining - (std::max)(0.0f, deltaTime));

                // Расстояние центр-центр понадобится для выбора между погоней
                // и атакой. Луч зрения ниже начинается отдельно от глаз NPC.
                const float dx =
                    targetTransform->position.x - npcTransform->position.x;

                const float dy =
                    targetTransform->position.y - npcTransform->position.y;

                const float dz =
                    targetTransform->position.z - npcTransform->position.z;

              
                XMFLOAT3 npcPosition = npcTransform->position;
                XMFLOAT3 npcEyePosition = { npcPosition.x, npcPosition.y + 1.6f, npcPosition.z };
                // Начало луча берём из реальной позы: фиксированная высота
                // могла оставлять «глаза» внутри баррикады при видимой голове.
                const auto* sightMesh = registry.GetComponent<MeshRendererComponent>(npcEntity);
                const auto* sightAnimation = registry.GetComponent<AnimationComponent>(npcEntity);
                DirectX::XMMATRIX headTransform;
                if (sightMesh && sightMesh->model && sightAnimation &&
                    sightMesh->model->TryGetBoneModelTransform(*sightAnimation, "mixamorig:Head", headTransform))
                {
                    DirectX::XMStoreFloat3(&npcEyePosition, DirectX::XMVector3TransformCoord(
                        DirectX::XMVectorSet(0.0f, 0.08f, 0.0f, 1.0f),
                        headTransform * npcTransform->GetWorldMatrix()));
                }

                // Реальная позиция цели используется только для геометрической
                // проверки. Она попадёт в память NPC лишь после успешного луча.
                XMFLOAT3 targetPosition = targetTransform->position;
                XMFLOAT3 ChestPosition = {
                    targetPosition.x, targetPosition.y + 1.0f, targetPosition.z };

                XMFLOAT3 ToEntity = { ChestPosition.x - npcEyePosition.x,
                    ChestPosition.y - npcEyePosition.y, ChestPosition.z - npcEyePosition.z };

                float distanceSquared =
                    ToEntity.x * ToEntity.x +
                    ToEntity.y * ToEntity.y +
                    ToEntity.z * ToEntity.z;


                float distance = std::sqrt(distanceSquared);

                if (distance <= 0.001f)
                    return;

                XMFLOAT3 direction = {
                    ToEntity.x / distance,
                    ToEntity.y / distance,
                    ToEntity.z / distance
                };


            

                const bool entityInRange =
                    NpcCombatMath::IsWithinDistanceSquared(
                        distanceSquared,
                        combat.seeRange);

                const float horizontalDistanceSquared =
                    ToEntity.x * ToEntity.x +
                    ToEntity.z * ToEntity.z;

                const bool closeProximityAwareness =
                    NpcCombatDecisionRules::HasCloseProximityAwareness(
                        horizontalDistanceSquared,
                        combat.closeAwarenessRange);

                const bool EntityInViewCone =
                    closeProximityAwareness ||
                    NpcCombatMath::IsInsideViewCone(
                        npcTransform->rotation.y,
                        ToEntity.x,
                        ToEntity.z,
                        combat.fieldOfViewDegrees);

                bool entityVisible = false;

                if (entityInRange && EntityInViewCone)
                {
                    // Грудь может быть закрыта: проверяем также голову и нижнюю
                    // часть корпуса. Стрелять будем в первую действительно видимую точку.
                    // У игрока высота головы может зависеть от камеры. У NPC нет
                    // PlayCameraComponent, поэтому используется 1.6 метра.
                    const auto* targetCamera =
                        registry.GetComponent<PlayCameraComponent>(bestTargetEntity);
                    const float headHeight = targetCamera ? targetCamera->firstPersonEyeOffset.y : 1.6f;
                    for (float height : { 1.0f, headHeight, 0.55f })
                    {
                    ChestPosition = { targetPosition.x, targetPosition.y + height, targetPosition.z };
                    ToEntity = { ChestPosition.x - npcEyePosition.x,
                        ChestPosition.y - npcEyePosition.y, ChestPosition.z - npcEyePosition.z };
                    distance = std::sqrt(ToEntity.x*ToEntity.x + ToEntity.y*ToEntity.y + ToEntity.z*ToEntity.z);
                    if (!dynamicsWorld || distance <= 0.001f || distance > combat.seeRange) continue;
                    direction = { ToEntity.x/distance, ToEntity.y/distance, ToEntity.z/distance };
                    PhysicsRaycastHit visionHit{};

                    PhysicsRaycast::CastRay(
                        npcEyePosition,
                        direction,
                        dynamicsWorld,
                        distance + 0.05f,
                        npcEntity,
                        nullptr,
                        visionHit
                    );

                    entityVisible =
                        visionHit.collisionObject == nullptr ||
                        visionHit.entity == bestTargetEntity;

                    if (debugRenderer && debugSettings.showNpcSightRays)
                    {
                        const XMFLOAT3 rayEnd = visionHit.collisionObject
                            ? visionHit.position
                            : ChestPosition;
                        debugRenderer->AddNpcSightRay(
                            npcEyePosition,
                            rayEnd,
                            entityVisible,
                            debugSettings.npcSightRayLifetime);
                    }
                    if (entityVisible)
                    {
                        combat.visibleAimPoint = ChestPosition;
                        break;
                    }
                    }
                }

                // Предыдущую видимость можно переиспользовать только для той же
                // цели. Иначе скорость новой цели вычислилась бы из позиции старой.
                const bool previouslyVisible =
                    combat.targetEntity == bestTargetEntity &&
                    combat.targetVisible;
                combat.targetVisible = entityVisible;
                if (auto* tactical = registry.GetComponent<NpcTacticalComponent>(npcEntity))
                {
                    // Прицеливание на ходу относится ко всем боевым маршрутам,
                    // а не только к старому squad-флангу. Таймер растёт лишь
                    // при личной видимости цели; за стеной он сразу сбрасывается.
                    const bool movingCombatState =
                        brain->state == NpcState::Chasing ||
                        brain->state == NpcState::TakingCover;
                    const bool movingFireEnabled =
                        NpcCombatModuleOperational(
                            modules,
                            NpcCombatModule::MovingFire);
                    tactical->movingAimTime = movingFireEnabled &&
                        entityVisible && movingCombatState
                        ? tactical->movingAimTime + (std::max)(0.0f, deltaTime)
                        : 0.0f;
                }

                if(entityVisible)
                {
                    combat.targetEntity = bestTargetEntity;
                    // Обновляем факт и скорость только здесь, после проверки
                    // зрения. При скрытой цели её текущая позиция в память не идёт.
                    combat.ObserveVisibleTarget(targetTransform->position,
                        deltaTime, previouslyVisible);

                    // Повторное обнаружение начинает новую поисковую историю.
                    // Без сброса NPC после первого потерянного контакта навсегда
                    // сохранял 3/3 проверенных сектора и при следующей потере
                    // снова переходил к стоянию на месте.
                    if (!previouslyVisible)
                    {
                        if (auto* searchTactical =
                            registry.GetComponent<NpcTacticalComponent>(npcEntity))
                        {
                            searchTactical->searchSectorsAssigned = 0;
                            searchTactical->searchRouteFailures = 0;
                            searchTactical->searchRetryTime = 0.0f;
                            searchTactical->searchingSector = false;
                            searchTactical->hasLastSearchDestination = false;
                        }
                    }

                    // Будущая реакция на перезарядку ВРАГА должна начинаться
                    // именно в этом блоке: здесь цель лично видна. После
                    // появления reload-анимации здесь можно записать короткий
                    // observedEnemyReloadTimer. Читать WeaponComponent скрытой
                    // цели в CoverSystem нельзя — это вернёт NPC «всезнание».


					

                    // Дистанцию атаки считаем между позициями сущностей — точно так же,
                    // как StateSystem. Смещение «глаза NPC -> грудь цели» применяется
                    // только к лучу проверки видимости и не должно менять дальность оружия.
                    const float targetDistanceSquared = dx * dx + dy * dy + dz * dz;
                    const auto* weapon = registry.GetComponent<WeaponComponent>(
                        WeaponSystem::GetEquippedWeaponEntity(registry, npcEntity));
                    const bool unarmed = weapon == nullptr;
                    const bool wasAttacking = brain->state == NpcState::Attacking;
                    const float shootingRange = weapon ? (std::max)(0.0f,
                        (std::min)(combat.rangedAttackRange, weapon->range)) : 0.0f;
                    const bool attackStance = unarmed
                        ? combat.ShouldHoldMeleeStance(targetDistanceSquared, wasAttacking)
                        : targetDistanceSquared <= shootingRange * shootingRange;
                    if (unarmed && attackStance && !wasAttacking)
                        combat.EnterMeleeStance();
                    const auto* tactical =
                        registry.GetComponent<NpcTacticalComponent>(npcEntity);
                    const bool executingCoverOrder = tactical &&
                        (tactical->reservedCover != INVALID_ENTITY ||
                            tactical->dynamicCoverActive) &&
                        !tactical->exposed &&
                        (brain->state == NpcState::TakingCover ||
                            brain->state == NpcState::InCover);
                    // Обычная реакция зрения не должна каждый кадр отменять
                    // уже назначенный путь к укрытию. На peek-фазе exposed=true,
                    // поэтому там снова разрешена стандартная атака.
                    if (!executingCoverOrder)
                    {
                        const bool engagementEnabled =
                            NpcCombatModuleOperational(
                                modules,
                                NpcCombatModule::Engagement);
                        const bool pursuitEnabled =
                            NpcCombatModuleOperational(
                                modules,
                                NpcCombatModule::Pursuit);
                        if (attackStance && engagementEnabled)
                            brain->state = NpcState::Attacking;
                        else if (pursuitEnabled)
                            brain->state = NpcState::Chasing;
                        else
                            brain->state = NpcState::Idle;
                    }
                }
                else if (combat.targetEntity != INVALID_ENTITY)
                {
                    const bool pursuitEnabled =
                        NpcCombatModuleOperational(
                            modules,
                            NpcCombatModule::Pursuit);
                    const bool searchEnabled =
                        NpcCombatModuleOperational(
                            modules,
                            NpcCombatModule::Search);

                    if (!pursuitEnabled && !searchEnabled)
                    {
                        combat.ForgetTarget();
                        if (auto* tactical = registry.GetComponent<
                            NpcTacticalComponent>(npcEntity))
                        {
                            tactical->searchingSector = false;
                            tactical->hasFlankTarget = false;
                            tactical->searchSectorsAssigned = 0;
                            tactical->searchRouteFailures = 0;
                            tactical->searchRetryTime = 0.0f;
                        }
                        brain->state = NpcState::Idle;
                        NpcNavMeshSystem::InvalidatePath(npcEntity);
                        return;
                    }

                    if (!pursuitEnabled && searchEnabled &&
                        brain->state == NpcState::Idle)
                    {
                        brain->state = NpcState::Searching;
                    }

                    UpdateLostTargetMemory(
                        registry,
                        npcEntity,
                        combat,
                        *brain,
                        deltaTime);
                    

                    
                }

                // Накопление прицеливания разрешено только стоящему стрелку
                // с видимой целью. В остальных состояниях начнём его заново.
                if (!combat.targetVisible ||
                    brain->state != NpcState::Attacking ||
                    !NpcCombatModuleOperational(
                        modules,
                        NpcCombatModule::Engagement) ||
                    !NpcCombatModuleOperational(
                        modules,
                        NpcCombatModule::StationaryFire))
                    combat.rangedAimTime = 0.0f;

                

                
            });
    }

private:
    static void UpdateLostTargetMemory(
        ECSRegistry& registry,
        Entity npcEntity,
        NPCCombatComponent& combat,
        NpcBrainComponent& brain,
        float deltaTime)
    {
        // Забывание принадлежит памяти/поиску, а не зрению. Поэтому этот код
        // выполняется и при выключенном Perception: отключённый сенсор не должен
        // превращать последнюю позицию в вечное знание.
        if (brain.state != NpcState::Searching ||
            !combat.searchPlanReady ||
            combat.investigatingPrediction)
        {
            return;
        }

        combat.timeSinceTargetSeen += (std::max)(0.0f, deltaTime);
        auto* searchTactical =
            registry.GetComponent<NpcTacticalComponent>(npcEntity);
        const int searchSectorLimit = !searchTactical
            ? 0
            : searchTactical->activeStyle == NpcTacticalStyle::Assault
                ? 3
                : searchTactical->activeStyle == NpcTacticalStyle::Balanced
                    ? 1
                    : 0;
        const bool sectorsPending = searchTactical &&
            WeaponSystem::GetEquippedWeaponEntity(registry, npcEntity) !=
                INVALID_ENTITY &&
            (searchTactical->hasFlankTarget ||
                (searchTactical->searchSectorsAssigned < searchSectorLimit &&
                    searchTactical->searchRouteFailures < 10));
        if (sectorsPending ||
            combat.timeSinceTargetSeen < combat.loseTargetDelay ||
            combat.searchTurnRemaining > 0.0f)
        {
            return;
        }

        combat.ForgetTarget();
        if (searchTactical)
        {
            searchTactical->searchingSector = false;
            searchTactical->hasFlankTarget = false;
        }
        brain.state = NpcState::Idle;
        NpcNavMeshSystem::InvalidatePath(npcEntity);
    }
};
