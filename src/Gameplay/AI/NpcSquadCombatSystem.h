#pragma once

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_set>
#include <vector>

#include "ECSRegistry.h"
#include "Components.h"
#include "NPCComponents.h"
#include "Physics/PhysicsRaycast.h"
#include "Gameplay/Combat/WeaponSystem.h"
#include "Gameplay/AI/NpcNavMeshSystem.h"
#include "Gameplay/AI/NpcCombatDecisionRules.h"

// Координатор не двигает capsule сам. Он только распределяет роли и записывает
// тактическую цель; StateSystem остаётся единственным владельцем движения.
//
// ВАЖНО ДЛЯ УКРЫТИЙ:
//   reservedCover — настоящая авторская NpcCoverPointComponent, которую нельзя
//   одновременно отдавать двум бойцам;
//   coverPosition — защищённая сторона точки;
//   peekPosition — край, откуда видна цель;
//   flankTarget — временная NavMesh-точка обхода БЕЗ гарантии физической стены.
//
// За один Update координатор собирает боеспособных NPC, делится подтверждённым
// контактом внутри squadId, назначает первого подавляющим, остальных —
// флангующими, обновляет таймеры и выбирает/проверяет их тактические позиции.
// Он не имеет права ставить targetVisible союзнику: видимость всегда личная и
// подтверждается физическим raycast в NpcCombatSystem.
class NpcSquadCombatSystem
{
public:
    // dt измеряется в секундах; world нужен для проверки защиты и видимости.
    // При некорректных аргументах система безопасно пропускает кадр.
    static void Update(ECSRegistry& registry, float dt, btDynamicsWorld* world)
    {
        if (!world || !std::isfinite(dt) || dt <= 0.0f) return;
        Entity player = INVALID_ENTITY;
        registry.ForEach<PlayerGameTag>([&](Entity e, PlayerGameTag&) { player = e; });
        const auto* playerTransform = registry.GetComponent<TransformComponent>(player);
        if (!playerTransform) return;

        struct Member
        {
            // Entity бойца и прямые указатели на компоненты, уже проверенные при
            // сборе списка. Указатели живут только до конца текущего Update.
            Entity entity;
            NPCCombatComponent* combat;      // Цель, видимость и память контакта.
            NpcBrainComponent* brain;        // Исполняемое состояние и маршрут.
            NpcTacticalComponent* tactical;  // Роль, укрытие и фазовые таймеры.
            TransformComponent* transform;   // Текущая мировая позиция бойца.
        };
        std::vector<Member> members;
        registry.ForEach<NpcTacticalComponent>(
            [&](Entity npc, NpcTacticalComponent& tactical)
            {
                auto* data = registry.GetComponent<NPCComponent>(npc);
                auto* combat = registry.GetComponent<NPCCombatComponent>(npc);
                auto* brain = registry.GetComponent<NpcBrainComponent>(npc);
                auto* transform = registry.GetComponent<TransformComponent>(npc);
                const auto* vitals =
                    registry.GetComponent<CharacterVitalsComponent>(npc);
                if (registry.HasComponent<ActiveComponent>(npc) &&
                    data && data->isHostile && vitals && vitals->health > 0.0f &&
                    combat && brain && transform &&
                    !brain->command.IsActive() &&
                    WeaponSystem::GetEquippedWeaponEntity(registry, npc) != INVALID_ENTITY)
                {
                    members.push_back({ npc, combat, brain, &tactical, transform });
                }
            });
        if (members.empty()) return;

        // Радиосвязь отряда: увиденная одним бойцом позиция становится общей
        // последней известной точкой, но не превращается в видимость сквозь стену.
        for (Member& observer : members)
        {
            if (!observer.combat->targetVisible) continue;
            for (Member& ally : members)
            {
                if (ally.tactical->squadId != observer.tactical->squadId) continue;
                ally.combat->targetEntity = observer.combat->targetEntity;
                ally.combat->lastKnownPosition = observer.combat->lastKnownPosition;
                ally.combat->hasLastKnownPosition = true;
                ally.combat->timeSinceTargetSeen = 0.0f;
                ally.tactical->searchSectorsAssigned = 0;
                ally.tactical->searchRouteFailures = 0;
                ally.tactical->hasLastSearchDestination = false;
                ally.tactical->searchingSector = false;
                if (!ally.combat->targetVisible)
                    ally.combat->lastSeenVelocity = observer.combat->lastSeenVelocity;
                // Сообщение будит напарника, но не подделывает личную видимость.
                if (!ally.combat->targetVisible &&
                    (ally.brain->state == NpcState::Idle || ally.brain->state == NpcState::Patrolling ||
                        ally.brain->state == NpcState::Searching))
                {
                    ally.combat->searchPlanReady = false;
                    ally.brain->state = NpcState::Chasing;
                }
            }
        }

        // Самый ранний живой участник подавляет, остальные расходятся по флангам.
        std::sort(members.begin(), members.end(),
            [](const Member& a, const Member& b)
            {
                if (a.tactical->squadId != b.tactical->squadId)
                    return a.tactical->squadId < b.tactical->squadId;
                return a.entity < b.entity;
            });
        int previousSquad = (std::numeric_limits<int>::min)();
        for (Member& member : members)
        {
            const auto role = member.tactical->squadId == previousSquad
                ? NpcSquadRole::Flanker : NpcSquadRole::Suppressor;
            if (role != member.tactical->role)
            {
                // После гибели напарника прежний фланговый приказ теряет смысл.
                member.tactical->reservedCover = INVALID_ENTITY;
                member.tactical->exposed = false;
                member.tactical->hasFlankTarget = false;
                member.tactical->repositionCooldown = 0.5f;
                member.brain->state = NpcState::Chasing;
                member.tactical->decisionTimer = 0.0f;
            }
            member.tactical->role = role;
            previousSquad = member.tactical->squadId;
        }

        std::unordered_set<Entity> occupied;
        for (const Member& member : members)
            if (member.tactical->reservedCover != INVALID_ENTITY)
                occupied.insert(member.tactical->reservedCover);

        for (Member& member : members)
        {
            auto& tactical = *member.tactical;
            tactical.searchRetryTime = (std::max)(0.0f, tactical.searchRetryTime - dt);
            tactical.underFireTime = (std::max)(0.0f, tactical.underFireTime - dt);
            tactical.damageReactionCooldown = (std::max)(0.0f, tactical.damageReactionCooldown - dt);
            if (tactical.damageReactionPending)
            {
                tactical.damageReactionPending = false;
                tactical.damageReactionCooldown = 2.0f;
                tactical.decisionTimer = 0.0f;
                tactical.repositionCooldown = 0.0f;
                // Из открытого края немедленно возвращаемся за своё укрытие.
                // На уже выполняемом пути не меняем цель от каждой пули.
                if (tactical.reservedCover != INVALID_ENTITY && tactical.exposed)
                {
                    tactical.exposed = tactical.movingToPeek = false;
                    tactical.moveTarget = tactical.coverPosition;
                    tactical.moveElapsed = tactical.noProgressTime = 0.0f;
                    tactical.status = "Hit: breaking exposure";
                    member.brain->state = NpcState::TakingCover;
                }
            }
            tactical.repositionCooldown = (std::max)(0.0f, tactical.repositionCooldown - dt);
            tactical.decisionTimer = (std::max)(0.0f, tactical.decisionTimer - dt);
            tactical.phaseTimer = (std::max)(0.0f, tactical.phaseTimer - dt);
            tactical.rejectedCoverTimer = (std::max)(0.0f, tactical.rejectedCoverTimer - dt);
            tactical.flankCommitTime = (std::max)(0.0f, tactical.flankCommitTime - dt);
            tactical.firingPositionHold = (std::max)(0.0f, tactical.firingPositionHold - dt);
            bool separatedForFire = true;
            for (const Member& ally : members)
                if (ally.entity != member.entity && ally.tactical->squadId == tactical.squadId &&
                    std::hypot(member.transform->position.x - ally.transform->position.x,
                        member.transform->position.z - ally.transform->position.z) < 2.0f)
                    separatedForFire = false;
            // ---------------------------------------------------------------------
            // ПРИОРИТЕТ НАСТОЯЩИХ ТОЧЕК УКРЫТИЯ
            // ---------------------------------------------------------------------
            // Здесь рассматриваются только сущности с NpcCoverPointComponent.
            // Это важно: flankTarget ниже по файлу — всего лишь удобная открытая
            // позиция для обхода, она не обещает физической защиты от пуль.
            //
            // До фиксации хорошей огневой позиции NPC пытается занять настоящее
            // укрытие. Стрелок делает это, если находится под огнём или тяжело
            // ранен; фланкер ищет свободное укрытие даже без полученного попадания.
            const auto* coverVitals =
                registry.GetComponent<CharacterVitalsComponent>(member.entity);
            const bool coverWounded = coverVitals &&
                coverVitals->health < coverVitals->maxHealth * 0.35f;
            const bool wantCover = tactical.underFireTime > 0.0f ||
                coverWounded || tactical.role == NpcSquadRole::Flanker;

            if (wantCover && member.combat->targetVisible &&
                member.combat->hasLastKnownPosition &&
                tactical.reservedCover == INVALID_ENTITY &&
                tactical.decisionTimer <= 0.0f &&
                member.brain->state != NpcState::TakingCover &&
                member.brain->state != NpcState::InCover)
            {
                TransformComponent knownTargetForCover = *playerTransform;
                knownTargetForCover.position = member.combat->lastKnownPosition;

                // Результат перебора. chosenCover — ID сущности-точки укрытия,
                // chosenPosition — её защищённая позиция из Transform,
                // chosenPeek — край для стрельбы, вычисленный из peekOffset.
                Entity chosenCover = INVALID_ENTITY;
                DirectX::XMFLOAT3 chosenPosition{};
                DirectX::XMFLOAT3 chosenPeek{};
                bool chosenCrouch = false; // Нужно ли присесть за этим укрытием.
                float chosenScore = (std::numeric_limits<float>::max)();

                registry.ForEach<NpcCoverPointComponent, TransformComponent>(
                    [&](Entity point, NpcCoverPointComponent& cover,
                        TransformComponent& coverTransform)
                    {
                        // Отключённую, занятую напарником или недавно проваленную
                        // точку даже не проверяем. rejectedCoverTimer не даёт NPC
                        // выбирать одно и то же недоступное укрытие каждый кадр.
                        if (!cover.enabled || occupied.contains(point) ||
                            (point == tactical.rejectedCover &&
                                tactical.rejectedCoverTimer > 0.0f))
                            return;

                        const auto position = coverTransform.position;
                        const auto peek = LocalOffset(coverTransform, cover.peekOffset);
                        // Хорошая точка обязана выполнять сразу два условия:
                        // 1) из основной позиции геометрия закрывает NPC от цели;
                        // 2) из peek-позиции NPC действительно видит цель.
                        if (!ProtectedFromPlayer(registry, player, knownTargetForCover,
                            position, cover.protectionHeight, world) ||
                            !CanSeePlayer(registry, player, peek, knownTargetForCover, world))
                            return;

                        // Проверяем обе половины манёвра по NavMesh: NPC должен
                        // добраться и за стену, и к краю для последующего выстрела.
                        // Допуск 0.6 м отсеивает случаи, когда NavMesh нашёл лишь
                        // ближайшую проходимую точку далеко от нужного места.
                        DirectX::XMFLOAT3 reachable{};
                        if (!NpcNavMeshSystem::FindReachablePoint(registry,
                            member.brain->currentRoom, member.transform->position,
                            position, reachable) ||
                            std::hypot(reachable.x - position.x,
                                reachable.z - position.z) > 0.6f)
                            return;
                        if (!NpcNavMeshSystem::FindReachablePoint(registry,
                            member.brain->currentRoom, member.transform->position,
                            peek, reachable) ||
                            std::hypot(reachable.x - peek.x,
                                reachable.z - peek.z) > 0.6f)
                            return;

                        // Не используем укрытие прямо у ног противника.
                        if (std::hypot(position.x - knownTargetForCover.position.x,
                            position.z - knownTargetForCover.position.z) < 3.0f)
                            return;

                        // Два бойца не должны стрелять почти из одной точки:
                        // это мешает друг другу и уничтожает смысл фланга.
                        bool isolated = true;
                        for (const Member& ally : members)
                        {
                            if (ally.entity == member.entity ||
                                ally.tactical->squadId != tactical.squadId)
                                continue;
                            const auto anchor = ally.tactical->reservedCover != INVALID_ENTITY
                                ? ally.tactical->peekPosition : ally.transform->position;
                            if (std::hypot(peek.x - anchor.x, peek.z - anchor.z) < 3.0f)
                            {
                                isolated = false;
                                break;
                            }
                        }
                        if (!isolated) return;

                        // Побеждает близкое укрытие, которое сохраняет примерно
                        // preferredRange до последней известной позиции цели.
                        const float distance = std::hypot(
                            position.x - member.transform->position.x,
                            position.z - member.transform->position.z);
                        const float rangeError = std::hypot(
                            position.x - knownTargetForCover.position.x,
                            position.z - knownTargetForCover.position.z) - tactical.preferredRange;
                        const float score = distance + 0.15f * std::abs(rangeError);
                        if (score < chosenScore)
                        {
                            chosenScore = score;
                            chosenCover = point;
                            chosenPosition = position;
                            chosenPeek = peek;
                            chosenCrouch = cover.crouched;
                        }
                    });

                // Не прерываем уже запущенное движение, пока не найдено
                // гарантированно доступное укрытие. После выбора сразу резервируем
                // его в occupied, чтобы следующий NPC в этом же кадре не взял его.
                if (chosenCover != INVALID_ENTITY)
                {
                    tactical.reservedCover = chosenCover;
                    occupied.insert(chosenCover);
                    tactical.coverPosition = chosenPosition;
                    tactical.peekPosition = chosenPeek;
                    tactical.coverUsesCrouch = chosenCrouch;
                    tactical.moveTarget = chosenPosition;
                    tactical.movingToPeek = false;
                    tactical.exposed = false;
                    tactical.crouched = false;
                    tactical.noSuitableCover = false;
                    tactical.hasFlankTarget = false;
                    tactical.searchingSector = false;
                    tactical.flankCommitTime = 0.0f;
                    tactical.firingPositionHold = 0.0f;
                    tactical.moveElapsed = 0.0f;
                    tactical.noProgressTime = 0.0f;
                    tactical.lastProgressPosition = member.transform->position;
                    tactical.decisionTimer = (std::max)(0.2f, tactical.decisionInterval);
                    tactical.status = "[TEST] Moving to validated cover";
                    tactical.lastTacticalTransition = "[TEST] Cover preferred over open flank";
                    member.brain->state = NpcState::TakingCover;
                    NpcNavMeshSystem::InvalidatePath(member.entity);
                    continue;
                }
                // Слишком частый поиск укрытий может быть дорогим.
                tactical.decisionTimer = (std::max)(0.3f, tactical.decisionInterval);
            }
            // Конец раннего поиска приоритетного авторского укрытия.

            if (tactical.hasFlankTarget && NpcCombatDecisionRules::CanFinishFlank(
                member.combat->targetVisible, tactical.firedOnRoute,
                tactical.underFireTime > 0.0f, separatedForFire))
            {
                // Цель обхода — получить огневую позицию, а не обязательно
                // дойти до абстрактной точки кольца. Реальный выстрел подтверждает
                // разворот, доступность ствола и личную видимость.
                tactical.hasFlankTarget = tactical.searchingSector = false;
                tactical.flankCommitTime = 0.0f;
                tactical.firingPositionHold = 4.0f;
                tactical.lastTacticalTransition = "Flank completed early: fired from a separated position";
                NpcNavMeshSystem::InvalidatePath(member.entity);
            }
            tactical.firedOnRoute = false;
            if (tactical.firingPositionHold > 0.0f && member.combat->targetVisible &&
                tactical.underFireTime <= 0.0f && separatedForFire)
            {
                member.brain->state = NpcState::Attacking;
                tactical.status = "Holding acquired firing position";
                continue;
            }
            // ---------------------------------------------------------------------
            // УДЕРЖАНИЕ УЖЕ НАЙДЕННОЙ ОГНЕВОЙ ПОЗИЦИИ
            // ---------------------------------------------------------------------
            // Фланкер прекращает обход, если уже
            // видит цель, не находится под огнём и достаточно удалён от союзника.
            // ВНИМАНИЕ: личная видимость ещё не гарантирует свободную линию
            // выстрела от дула. Для финальной версии нужна дополнительная проверка.
            const bool goodFiringPosition =
                tactical.role == NpcSquadRole::Flanker &&
                member.combat->targetVisible &&
                separatedForFire &&
                tactical.underFireTime <= 0.0f;

            if (goodFiringPosition && tactical.hasFlankTarget)
            {
                tactical.hasFlankTarget = false;
                tactical.searchingSector = false;
                tactical.flankCommitTime = 0.0f;
                tactical.firingPositionHold = 5.0f;

                member.brain->state = NpcState::Attacking;
                tactical.status = "Holding good firing angle";

                NpcNavMeshSystem::InvalidatePath(member.entity);
                continue;
            }
            // Конец проверки на удержание хорошего угла стрельбы.

            // Контакт не отменяет обход: иначе фланкер каждый кадр снова
            // превращается в неподвижного стрелка рядом с напарником.
            if (tactical.hasFlankTarget && tactical.flankCommitTime > 0.0f)
            {
                // Проверяем фактическое продвижение, а не только срок приказа.
                // Иначе недоступная точка удерживала NPC на месте до 10 секунд.
                const float progress = std::hypot(member.transform->position.x - tactical.lastProgressPosition.x,
                    member.transform->position.z - tactical.lastProgressPosition.z);
                if (progress > 0.15f)
                {
                    tactical.lastProgressPosition = member.transform->position;
                    tactical.noProgressTime = 0.0f;
                }
                else tactical.noProgressTime += dt;
                if (tactical.noProgressTime < 1.5f)
                {
                    member.brain->state = NpcState::Chasing;
                    continue;
                }
                tactical.flankCommitTime = 0.0f;
                tactical.noProgressTime = 0.0f;
                tactical.status = "Route stalled: selecting another sector";
            }
            // Истёк срок конкретного маршрута — разрешаем поиск другого сектора.
            if (tactical.hasFlankTarget)
            {
                if (tactical.searchingSector)
                {
                    ++tactical.searchRouteFailures;
                    tactical.searchRetryTime = 1.0f;
                    tactical.searchingSector = false;
                }
                tactical.hasFlankTarget = false;
                NpcNavMeshSystem::InvalidatePath(member.entity);
            }
            if (!member.combat->hasLastKnownPosition) continue;

            bool squadHasContact = false;
            int squadSize = 0;
            for (const Member& ally : members)
                if (ally.tactical->squadId == tactical.squadId)
                {
                    ++squadSize;
                    if (ally.combat->targetVisible) squadHasContact = true;
                }
            tactical.contactLostTime = squadHasContact ? 0.0f : tactical.contactLostTime + dt;
            // Прикрывающий на короткое время удерживает выход, фланкер уже ищет
            // другой угол. Это ограниченное ожидание, а не вечная засада.
            if (!squadHasContact && squadSize > 1 && tactical.role == NpcSquadRole::Suppressor &&
                tactical.contactLostTime < 2.0f && tactical.searchSectorsAssigned == 0)
            {
                member.brain->state = NpcState::Searching;
                tactical.status = "Holding exit while partner searches";
                continue;
            }
            // Даже рабочее укрытие не должно навечно запирать фланкера.
            // При наличии общего контакта периодически выбираем новый угол
            // минимум в 60 градусах от текущего; одиночка тоже меняет позицию.
            const auto* vitals =
                registry.GetComponent<CharacterVitalsComponent>(member.entity);
            const bool wounded = vitals && vitals->health < vitals->maxHealth * 0.35f;
            if (squadHasContact && tactical.repositionCooldown <= 0.0f && !wounded &&
                tactical.noSuitableCover && tactical.reservedCover == INVALID_ENTITY &&
                NpcCombatDecisionRules::NeedsNewAngle(member.combat->targetVisible, tactical.underFireTime > 0.0f) &&
                member.brain->state != NpcState::TakingCover &&
                (tactical.role == NpcSquadRole::Flanker || squadSize == 1))
            {
                const auto center = member.combat->lastKnownPosition;
                const float sx = member.transform->position.x - center.x;
                const float sz = member.transform->position.z - center.z;
                const float radius = (std::max)(4.0f, tactical.preferredRange);
                float bestScore = (std::numeric_limits<float>::max)();
                DirectX::XMFLOAT3 bestPoint{};
                for (int i = 0; i < 12; ++i)
                {
                    const float angle = DirectX::XM_2PI * i / 12.0f;
                    const float x = std::cos(angle), z = std::sin(angle);
                    if ((sx * x + sz * z) > std::hypot(sx, sz) * 0.5f) continue;
                    DirectX::XMFLOAT3 desired{ center.x + x * radius, member.transform->position.y, center.z + z * radius };
                    DirectX::XMFLOAT3 reachable{};
                    if (!NpcNavMeshSystem::FindReachablePoint(registry, member.brain->currentRoom,
                        member.transform->position, desired, reachable) ||
                        std::hypot(reachable.x - desired.x, reachable.z - desired.z) > 0.6f) continue;
                    float score = std::hypot(reachable.x - member.transform->position.x,
                        reachable.z - member.transform->position.z);
                    bool separate = true;
                    for (const Member& ally : members)
                    {
                        if (ally.entity == member.entity || ally.tactical->squadId != tactical.squadId) continue;
                        const auto anchor = ally.tactical->hasFlankTarget ? ally.tactical->flankTarget : ally.transform->position;
                        const float separation = std::hypot(reachable.x - anchor.x, reachable.z - anchor.z);
                        if (separation < 5.0f) separate = false;
                        score -= (std::min)(separation, radius * 2.0f);
                    }
                    if (separate && score < bestScore) { bestScore = score; bestPoint = reachable; }
                }
                tactical.repositionCooldown = 8.0f;
                if (bestScore < (std::numeric_limits<float>::max)())
                {
                    occupied.erase(tactical.reservedCover);
                    tactical.reservedCover = INVALID_ENTITY;
                    tactical.exposed = tactical.crouched = false;
                    tactical.flankTarget = bestPoint;
                    tactical.hasFlankTarget = true;
                    tactical.flankCommitTime = 8.0f;
                    tactical.lastProgressPosition = member.transform->position;
                    tactical.noProgressTime = 0.0f;
                    member.brain->state = NpcState::Chasing;
                    tactical.status = squadSize == 1 ? "Solo reposition" : "Wide flank: changing firing angle";
                    NpcNavMeshSystem::InvalidatePath(member.entity);
                    continue;
                }
            }
            if (!squadHasContact)
            {
                // Без контакта нельзя вечно выглядывать из старого укрытия.
                // Проверяем несколько разных секторов вокруг ПРОГНОЗА, даже
                // если из них пока нет луча до последней позиции игрока.
                occupied.erase(tactical.reservedCover);
                tactical.reservedCover = INVALID_ENTITY;
                tactical.exposed = false;
                tactical.crouched = false;
                if (tactical.searchSectorsAssigned < 3 && tactical.searchRouteFailures < 10)
                {
                    if (tactical.searchRetryTime > 0.0f)
                    {
                        member.brain->state = NpcState::Searching;
                        tactical.status = "Search route unavailable: waiting to retry";
                        continue;
                    }
                    const auto predicted = member.combat->EstimateSearchPosition();
                    const float baseAngle = std::atan2(member.combat->lastSeenVelocity.z,
                        member.combat->lastSeenVelocity.x);
                    const float side = tactical.role == NpcSquadRole::Flanker ? 1.0f : -1.0f;
                    bool assigned = false;
                    tactical.searchRejectedNavigation = tactical.searchRejectedSpacing = tactical.searchRejectedNear = 0;
                    float searchBestScore = (std::numeric_limits<float>::max)();
                    // Оцениваем весь набор, а не берём первую случайно доступную
                    // точку. Это уменьшает повторный выбор уже проверенного угла.
                    for (int attempt = 0; attempt < 12; ++attempt)
                    {
                        const float angle = baseAngle + side * (0.65f +
                            tactical.searchSectorsAssigned * 0.8f + attempt * 0.4f + tactical.searchRouteFailures * 0.55f);
                        // Проверяем и ближние углы: одно большое кольцо часто
                        // целиком оказывалось снаружи узкого прохода или комнаты.
                        const float radius = (attempt % 2 == 0 ? 4.0f : 2.0f) + tactical.searchSectorsAssigned;
                        DirectX::XMFLOAT3 desired{ predicted.x + std::cos(angle) * radius,
                            member.transform->position.y, predicted.z + std::sin(angle) * radius };
                        DirectX::XMFLOAT3 reachable{};
                        if (!NpcNavMeshSystem::FindReachablePoint(registry, member.brain->currentRoom,
                            member.transform->position, desired, reachable))
                        {
                            ++tactical.searchRejectedNavigation; continue;
                        }
                        if (std::hypot(reachable.x - member.transform->position.x,
                            reachable.z - member.transform->position.z) < 1.0f)
                        {
                            ++tactical.searchRejectedNear; continue;
                        }
                        if (tactical.hasLastSearchDestination && std::hypot(
                            reachable.x - tactical.lastSearchDestination.x, reachable.z - tactical.lastSearchDestination.z) < 1.5f)
                        {
                            ++tactical.searchRejectedNear; continue;
                        }
                        bool separate = true;
                        float allyPenalty = 0.0f;
                        for (const Member& ally : members)
                        {
                            if (ally.entity == member.entity || ally.tactical->squadId != tactical.squadId) continue;
                            const auto& anchor = ally.tactical->hasFlankTarget ? ally.tactical->flankTarget : ally.transform->position;
                            const float gap = std::hypot(reachable.x - anchor.x, reachable.z - anchor.z);
                            // Жёсткий запрет нужен для капсул, а не для условных
                            // трёх метров: иначе узкий коридор запрещал весь поиск.
                            if (gap < 1.4f) separate = false;
                            allyPenalty += (std::max)(0.0f, 4.0f - gap) * 3.0f;
                        }
                        if (!separate) { ++tactical.searchRejectedSpacing; continue; }
                        const float score = std::hypot(reachable.x - member.transform->position.x,
                            reachable.z - member.transform->position.z) + allyPenalty +
                            std::hypot(reachable.x - desired.x, reachable.z - desired.z) * 2.0f;
                        if (score >= searchBestScore) continue;
                        searchBestScore = score;
                        tactical.flankTarget = reachable;
                        tactical.hasFlankTarget = true;
                        tactical.searchingSector = true;
                        tactical.flankCommitTime = 10.0f;
                        tactical.lastProgressPosition = member.transform->position;
                        tactical.noProgressTime = 0.0f;
                        tactical.status = "Searching assigned sector";
                        member.brain->state = NpcState::Chasing;
                        member.combat->searchPlanReady = false;
                        member.combat->timeSinceTargetSeen = 0.0f;
                        assigned = true;
                    }
                    if (assigned) continue;
                    ++tactical.searchRouteFailures;
                    tactical.searchRetryTime = 1.0f;
                    member.brain->state = NpcState::Searching;
                    tactical.status = "No reachable separated search point (NavMesh / spacing)";
                    continue;
                }
                member.brain->state = NpcState::Searching;
                tactical.status = tactical.searchSectorsAssigned >= 3 ? "Scanning after completed sector search" :
                    "Search blocked: navigation retries exhausted";
                continue;
            }

            // Тактика пользуется общей последней увиденной позицией, а не
            // координатами скрытого за стеной игрока.
            TransformComponent knownTarget = *playerTransform;
            knownTarget.position = member.combat->lastKnownPosition;
            if (tactical.reservedCover != INVALID_ENTITY)
            {
                const float dx = member.transform->position.x - tactical.lastProgressPosition.x;
                const float dz = member.transform->position.z - tactical.lastProgressPosition.z;
                if (dx * dx + dz * dz > 0.04f)
                {
                    tactical.lastProgressPosition = member.transform->position;
                    tactical.noProgressTime = 0.0f;
                }
                else if (member.brain->state == NpcState::TakingCover)
                    tactical.noProgressTime += dt;
                tactical.moveElapsed += dt;
                const auto* cover = registry.GetComponent<NpcCoverPointComponent>(tactical.reservedCover);
                const bool invalid = (tactical.exposed &&
                    member.combat->rangedStatus == "Muzzle blocked by cover") ||
                    !cover || !cover->enabled ||
                    !ProtectedFromPlayer(registry, player, knownTarget,
                        tactical.coverPosition, cover->protectionHeight, world) ||
                    !CanSeePlayer(registry, player, tactical.peekPosition, knownTarget, world);
                if (invalid || tactical.noProgressTime > 1.5f ||
                    (member.brain->state == NpcState::TakingCover && tactical.moveElapsed > 8.0f))
                {
                    occupied.erase(tactical.reservedCover);
                    tactical.rejectedCover = tactical.reservedCover;
                    tactical.rejectedCoverTimer = 4.0f;
                    tactical.reservedCover = INVALID_ENTITY;
                    tactical.exposed = false;
                    tactical.movingToPeek = false;
                    tactical.crouched = false;
                    tactical.decisionTimer = 0.0f;
                    member.brain->state = NpcState::Chasing;
                    NpcNavMeshSystem::InvalidatePath(member.entity);
                }
            }

            // В укрытии чередуем безопасную фазу и короткий выход к peek-точке.
            if (member.brain->state == NpcState::InCover && tactical.phaseTimer <= 0.0f)
            {
                // Раненый не повторяет одинаковый выход под очередь. Пауза
                // конечна: закончилась память об огне — снова проверяем край.
                if (tactical.underFireTime > 0.0f)
                {
                    tactical.phaseTimer = 0.3f;
                    tactical.status = "Recovering behind cover under fire";
                    continue;
                }
                tactical.movingToPeek = true;
                tactical.exposed = false;
                tactical.crouched = false;
                tactical.moveTarget = tactical.peekPosition;
                tactical.moveElapsed = 0.0f;
                tactical.noProgressTime = 0.0f;
                tactical.phaseTimer = tactical.exposeDuration;
                tactical.status = "Leaving cover to fire";
                member.brain->state = NpcState::TakingCover;
                continue;
            }
            if (tactical.exposed)
            {
                bool allyMoving = false;
                for (const Member& ally : members)
                    if (ally.entity != member.entity && ally.tactical->squadId == tactical.squadId &&
                        (ally.brain->state == NpcState::TakingCover || ally.tactical->hasFlankTarget))
                        allyMoving = true;
                // Прикрывающий продолжает огонь, пока напарник меняет угол.
                // Оба не должны одновременно уходить за стену по таймеру.
                if (allyMoving && member.combat->targetVisible)
                    tactical.phaseTimer = (std::max)(tactical.phaseTimer, 0.4f);
                // Даём зрению и развороту обновиться после прибытия. Раньше
                // отсутствие видимости в первый кадр мгновенно загоняло NPC назад.
                if (tactical.phaseTimer <= 0.0f)
                {
                    tactical.exposed = false;
                    tactical.movingToPeek = false;
                    tactical.crouched = tactical.coverUsesCrouch;
                    tactical.moveElapsed = 0.0f;
                    tactical.moveTarget = tactical.coverPosition;
                    tactical.status = "Returning behind cover";
                    member.brain->state = NpcState::TakingCover;
                }
                else if (member.combat->targetVisible)
                    member.brain->state = NpcState::Attacking;
                continue;
            }
            if (member.brain->state == NpcState::TakingCover ||
                member.brain->state == NpcState::InCover) continue;
            if (tactical.decisionTimer > 0.0f) continue;
            tactical.decisionTimer = (std::max)(0.05f, tactical.decisionInterval);

            Entity best = INVALID_ENTITY;
            float bestScore = (std::numeric_limits<float>::max)();
            DirectX::XMFLOAT3 bestCover{}, bestPeek{};
            bool bestCrouch = true;
            tactical.coverRejectedProtection = tactical.coverRejectedSight = tactical.coverRejectedPath = 0;
            registry.ForEach<NpcCoverPointComponent, TransformComponent>(
                [&](Entity point, NpcCoverPointComponent& cover, TransformComponent& transform) {
                    if (!cover.enabled || (occupied.contains(point) && point != tactical.reservedCover)) return;
                    if (point == tactical.rejectedCover && tactical.rejectedCoverTimer > 0.0f) return;
                    const DirectX::XMFLOAT3 coverPosition = transform.position;
                    const DirectX::XMFLOAT3 peek = LocalOffset(transform, cover.peekOffset);
                    if (!ProtectedFromPlayer(registry, player, knownTarget, coverPosition,
                        cover.protectionHeight, world)) {
                        ++tactical.coverRejectedProtection; return;
                    }
                    // Из peek-точки должна быть реальная линия огня до игрока.
                    if (!CanSeePlayer(registry, player, peek, knownTarget, world))
                    {
                        ++tactical.coverRejectedSight; return;
                    }
                    DirectX::XMFLOAT3 reachable{};
                    if (!NpcNavMeshSystem::FindReachablePoint(registry, member.brain->currentRoom,
                        member.transform->position, peek, reachable))
                    {
                        ++tactical.coverRejectedPath; return;
                    }
                    if (std::hypot(reachable.x - peek.x, reachable.z - peek.z) > 0.6f)
                    {
                        ++tactical.coverRejectedPath; return;
                    }
                    // Доступность края не доказывает доступность самого укрытия.
                    if (!NpcNavMeshSystem::FindReachablePoint(registry, member.brain->currentRoom,
                        member.transform->position, coverPosition, reachable) ||
                        std::hypot(reachable.x - coverPosition.x, reachable.z - coverPosition.z) > 0.6f)
                    {
                        ++tactical.coverRejectedPath; return;
                    }
                    const float nx = coverPosition.x - member.transform->position.x;
                    const float nz = coverPosition.z - member.transform->position.z;
                    const float px = coverPosition.x - knownTarget.position.x;
                    const float pz = coverPosition.z - knownTarget.position.z;
                    // Даже хорошее укрытие не оправдывает сближение стрелка
                    // вплотную с последней известной позицией противника.
                    if (px * px + pz * pz < 9.0f) return;
                    float score = nx * nx + nz * nz;
                    // Под огнём важнее быстро закрыться, чем бежать к далёкому
                    // идеальному углу. Раненые также предпочитают ближнюю защиту.
                    if (wounded || tactical.underFireTime > 0.0f) score *= 3.0f;
                    const float rangeError = std::sqrt(px * px + pz * pz) - tactical.preferredRange;
                    score += rangeError * rangeError;
                    for (const Member& ally : members)
                    {
                        if (ally.entity == member.entity || ally.tactical->squadId != tactical.squadId) continue;
                        const auto& anchor = ally.tactical->reservedCover != INVALID_ENTITY
                            ? ally.tactical->peekPosition : ally.transform->position;
                        const float separation = std::hypot(peek.x - anchor.x, peek.z - anchor.z);
                        if (separation < 3.0f) return;
                        const float ax = anchor.x - knownTarget.position.x;
                        const float az = anchor.z - knownTarget.position.z;
                        const float denom = (std::max)(0.1f, std::hypot(ax, az) * std::hypot(px, pz));
                        // Независимые углы стрельбы ценнее двух соседних укрытий.
                        score += (ax * px + az * pz) / denom * tactical.flankWeight * 30.0f;
                    }
                    if (tactical.role == NpcSquadRole::Flanker)
                    {
                        const float tx = member.transform->position.x - knownTarget.position.x;
                        const float tz = member.transform->position.z - knownTarget.position.z;
                        const float cross = std::abs(tx * pz - tz * px);
                        score -= cross * tactical.flankWeight;
                    }
                    if (score < bestScore) {
                        bestScore = score; best = point; bestCover = coverPosition;
                        bestPeek = peek; bestCrouch = cover.crouched;
                    }
                });
            if (best == INVALID_ENTITY)
            {
                tactical.noSuitableCover = true;
                // Нет авторского укрытия — ищем доступный угол на дистанции,
                // а не отправляем стрелка в центр последнего контакта.
                if (!member.combat->targetVisible &&
                    (!tactical.hasFlankTarget || tactical.flankCommitTime <= 0.0f))
                {
                    tactical.hasFlankTarget = false;
                    float flankScore = (std::numeric_limits<float>::max)();
                    const float radius = (std::max)(3.0f, tactical.preferredRange);
                    for (int i = 0; i < 12; ++i)
                    {
                        const float angle = DirectX::XM_2PI * i / 12.0f;
                        DirectX::XMFLOAT3 candidate{ knownTarget.position.x + std::cos(angle) * radius,
                            member.transform->position.y, knownTarget.position.z + std::sin(angle) * radius };
                        DirectX::XMFLOAT3 reachable{};
                        if (!NpcNavMeshSystem::FindReachablePoint(registry, member.brain->currentRoom,
                            member.transform->position, candidate, reachable)) continue;
                        if (std::hypot(reachable.x - candidate.x, reachable.z - candidate.z) > 0.6f ||
                            !CanSeePlayer(registry, player, reachable, knownTarget, world)) continue;
                        float score = std::hypot(reachable.x - member.transform->position.x,
                            reachable.z - member.transform->position.z);
                        if (score < 1.0f) continue;
                        bool separated = true;
                        for (const Member& ally : members)
                        {
                            if (ally.entity == member.entity || ally.tactical->squadId != tactical.squadId) continue;
                            const auto& anchor = ally.tactical->hasFlankTarget ? ally.tactical->flankTarget :
                                ally.tactical->reservedCover != INVALID_ENTITY ? ally.tactical->peekPosition : ally.transform->position;
                            const float separation = std::hypot(reachable.x - anchor.x, reachable.z - anchor.z);
                            if (separation < 5.0f) separated = false;
                            score -= (std::min)(separation, radius) * tactical.flankWeight;
                        }
                        if (separated && score < flankScore)
                        {
                            flankScore = score;
                            tactical.flankTarget = reachable;
                            tactical.hasFlankTarget = true;
                        }
                    }
                    tactical.flankCommitTime = 6.0f;
                    tactical.lastProgressPosition = member.transform->position;
                    tactical.noProgressTime = 0.0f;
                }
                if (tactical.hasFlankTarget)
                    member.brain->state = NpcState::Chasing;
                // При отсутствии подходящего края видимый противник остаётся
                // целью стрельбы: не ломаем рабочую атаку ради пустого приказа.
                tactical.status = tactical.hasFlankTarget ? "No cover: moving to firing angle" :
                    member.combat->targetVisible ? "Holding firing angle: no reason to reposition" :
                    "No cover or reachable firing angle";
                continue;
            }
            if (tactical.reservedCover != INVALID_ENTITY) occupied.erase(tactical.reservedCover);
            tactical.reservedCover = best;
            tactical.noSuitableCover = false;
            tactical.hasFlankTarget = false;
            occupied.insert(best);
            tactical.coverPosition = bestCover;
            tactical.peekPosition = bestPeek;
            tactical.coverUsesCrouch = bestCrouch;
            tactical.moveTarget = bestCover;
            tactical.moveElapsed = 0.0f;
            tactical.noProgressTime = 0.0f;
            tactical.lastProgressPosition = member.transform->position;
            tactical.movingToPeek = false;
            tactical.crouched = false;
            tactical.status = tactical.role == NpcSquadRole::Flanker
                ? "Flanking through reserved cover" : "Moving to suppressing cover";
            member.brain->state = NpcState::TakingCover;
        }
    }

private:
    // Переводит локальный peekOffset точки укрытия в мировые координаты с
    // учётом позиции, поворота и масштаба её Transform.
    static DirectX::XMFLOAT3 LocalOffset(const TransformComponent& transform,
        const DirectX::XMFLOAT3& offset)
    {
        DirectX::XMFLOAT3 result{};
        DirectX::XMStoreFloat3(&result, DirectX::XMVector3TransformCoord(
            DirectX::XMLoadFloat3(&offset), transform.GetWorldMatrix()));
        return result;
    }

    // Луч от игрока к защищённой позиции. true возвращается только если первым
    // препятствием стала статическая/кинематическая геометрия, то есть стена.
    static bool ProtectedFromPlayer(ECSRegistry& registry, Entity player,
        const TransformComponent& playerTransform, DirectX::XMFLOAT3 point,
        float height, btDynamicsWorld* world)
    {
        DirectX::XMFLOAT3 origin = playerTransform.position; origin.y += 1.0f;
        point.y += (std::max)(0.2f, height);
        const DirectX::XMFLOAT3 direction{ point.x - origin.x, point.y - origin.y, point.z - origin.z };
        const float distance = DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMLoadFloat3(&direction)));
        const auto* controller = registry.GetComponent<CharacterController>(player);
        PhysicsRaycastHit hit{};
        return distance > 0.01f && PhysicsRaycast::CastRay(origin, direction, world, distance,
            player, controller ? controller->ghostObj : nullptr, hit) && hit.collisionObject &&
            hit.collisionObject->isStaticOrKinematicObject();
    }

    // Проверяет, виден ли игрок из потенциальной точки выглядывания. Несколько
    // высот не дают низкому ящику ошибочно скрыть всё тело целиком.
    static bool CanSeePlayer(ECSRegistry& registry, Entity player,
        DirectX::XMFLOAT3 origin, const TransformComponent& playerTransform,
        btDynamicsWorld* world)
    {
        origin.y += 1.45f;
        const auto* targetCamera = registry.GetComponent<PlayCameraComponent>(player);
        const float headHeight = targetCamera ? targetCamera->firstPersonEyeOffset.y : 1.6f;
        for (float height : { 1.0f, headHeight, 0.55f })
        {
            DirectX::XMFLOAT3 target = playerTransform.position; target.y += height;
            const DirectX::XMFLOAT3 direction{ target.x - origin.x,target.y - origin.y,target.z - origin.z };
            const float distance = DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMLoadFloat3(&direction)));
            PhysicsRaycastHit hit{};
            PhysicsRaycast::CastRay(origin, direction, world, distance, INVALID_ENTITY, nullptr, hit);
            if (!hit.collisionObject || hit.entity == player) return true;
        }
        return false;
    }
};
