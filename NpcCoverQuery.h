#pragma once

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

#include "ECSRegistry.h"
#include "NPCComponents.h"
#include "Gameplay/AI/NpcNavMeshSystem.h"
#include "Physics/PhysicsRaycast.h"

// Унифицированный ответ любого поставщика укрытия.
//
// Сейчас поставщика два:
//   1. авторские NpcCoverPointComponent;
//   2. динамическая выборка NavMesh + физические лучи.
//
// В будущем сюда можно добавить третий модуль (например, заранее запечённые
// рёбра укрытий), не меняя код исполнения в NpcCoverSystem.
struct NpcCoverSearchResult
{
    bool found = false;
    bool dynamic = false;
    Entity coverEntity = INVALID_ENTITY;
    DirectX::XMFLOAT3 coverPosition{};
    DirectX::XMFLOAT3 peekPosition{};
    bool crouched = false;
    float score = (std::numeric_limits<float>::max)();
};

// Геометрический модуль поиска укрытия.
//
// ЧТО ОН ЧИТАЕТ:
//   * авторские Cover Point;
//   * достижимость NavMesh;
//   * статическую/кинематическую геометрию физического мира;
//   * последнюю подтверждённую позицию угрозы.
//
// ЧТО ОН МЕНЯЕТ:
//   * только диагностические счётчики и линии в tactical.
//
// ЧЕГО ОН НЕ ДЕЛАЕТ:
//   * не меняет NpcState;
//   * не назначает маршрут;
//   * не решает, нужно ли NPC вообще искать укрытие.
class NpcCoverQuery
{
public:
    // Полный поиск. Авторские точки проверяются первыми, потому что художник
    // может точно настроить сторону выглядывания и будущую анимационную позу.
    // Динамический поиск используется только как запасной поставщик.
    static NpcCoverSearchResult FindBest(
        ECSRegistry& registry,
        Entity targetEntity,
        Entity currentRoom,
        const DirectX::XMFLOAT3& npcPosition,
        const DirectX::XMFLOAT3& threatPosition,
        NpcTacticalComponent& tactical,
        btDynamicsWorld* world,
        bool allowDynamicProvider = true,
        const DirectX::XMFLOAT3* excludedPosition = nullptr,
        float exclusionRadius = 1.5f)
    {
        NpcCoverSearchResult result{};

        // Диагностика относится только к последнему полному поиску.
        tactical.coverRejectedProtection = 0;
        tactical.coverRejectedSight = 0;
        tactical.coverRejectedPath = 0;
        tactical.dynamicCoverRejectedRays.clear();
        tactical.dynamicCoverPeekRays.clear();
        tactical.dynamicCoverAcceptedRays.clear();

        FindBestAuthored(
            registry,
            targetEntity,
            currentRoom,
            npcPosition,
            threatPosition,
            tactical,
            world,
            excludedPosition,
            exclusionRadius,
            result);

        if (!result.found && tactical.allowDynamicCover &&
            allowDynamicProvider)
        {
            FindBestDynamic(
                registry,
                targetEntity,
                currentRoom,
                npcPosition,
                threatPosition,
                tactical,
                world,
                excludedPosition,
                exclusionRadius,
                result);
        }

        return result;
    }

    // Общая проверка уже занятой позиции. Она публична, потому что жизненный
    // цикл в NpcCoverSystem обязан периодически подтверждать старое укрытие.
    static bool IsProtectedFromTarget(
        ECSRegistry& registry,
        Entity targetEntity,
        DirectX::XMFLOAT3 targetPosition,
        DirectX::XMFLOAT3 coverPosition,
        float protectionHeight,
        btDynamicsWorld* world)
    {
        // Начало луча находится примерно на уровне оружия противника.
        targetPosition.y += 1.0f;

        // Конец луча задаёт высоту тела NPC, которую обязана закрывать стена.
        coverPosition.y += (std::max)(0.2f, protectionHeight);

        const DirectX::XMFLOAT3 direction{
            coverPosition.x - targetPosition.x,
            coverPosition.y - targetPosition.y,
            coverPosition.z - targetPosition.z
        };
        const float distance = DirectX::XMVectorGetX(
            DirectX::XMVector3Length(DirectX::XMLoadFloat3(&direction)));

        if (distance <= 0.01f)
            return false;

        // Игнорируем физическое тело цели в начале луча.
        const auto* targetController =
            registry.GetComponent<CharacterController>(targetEntity);
        const btCollisionObject* targetBody =
            targetController ? targetController->ghostObj : nullptr;

        PhysicsRaycastHit hit{};
        PhysicsRaycast::CastRay(
            targetPosition,
            direction,
            world,
            distance,
            targetEntity,
            targetBody,
            hit);

        // Живые персонажи не считаются стеной. Защищает только стабильная
        // статическая или кинематическая геометрия.
        return hit.collisionObject &&
            hit.collisionObject->isStaticOrKinematicObject();
    }

private:
    // Ищет лучший результат только среди расставленных автором точек.
    static void FindBestAuthored(
        ECSRegistry& registry,
        Entity targetEntity,
        Entity currentRoom,
        const DirectX::XMFLOAT3& npcPosition,
        const DirectX::XMFLOAT3& threatPosition,
        NpcTacticalComponent& tactical,
        btDynamicsWorld* world,
        const DirectX::XMFLOAT3* excludedPosition,
        float exclusionRadius,
        NpcCoverSearchResult& best)
    {
        registry.ForEach<NpcCoverPointComponent, TransformComponent>(
            [&](Entity coverEntity,
                NpcCoverPointComponent& cover,
                TransformComponent& coverTransform)
            {
                if (!cover.enabled)
                    return;

                // Недавно отклонённую точку временно не выбираем повторно.
                if (coverEntity == tactical.rejectedCover &&
                    tactical.rejectedCoverTimer > 0.0f)
                {
                    return;
                }

                // Одна авторская точка принадлежит только одному бойцу.
                // Без этой проверки два противника могли независимо выбрать
                // один маркер и затем толкаться в одинаковой позиции.
                bool reservedByAnotherNpc = false;
                registry.ForEach<NpcTacticalComponent>(
                    [&](Entity, NpcTacticalComponent& other)
                    {
                        if (&other != &tactical &&
                            other.reservedCover == coverEntity)
                        {
                            reservedByAnotherNpc = true;
                        }
                    });
                if (reservedByAnotherNpc)
                    return;

                const DirectX::XMFLOAT3 authoredPosition =
                    coverTransform.position;

                if (excludedPosition)
                {
                    const float excludedDx =
                        authoredPosition.x - excludedPosition->x;
                    const float excludedDz =
                        authoredPosition.z - excludedPosition->z;
                    const float safeRadius = (std::max)(
                        0.0f,
                        exclusionRadius);
                    if (excludedDx * excludedDx + excludedDz * excludedDz <=
                        safeRadius * safeRadius)
                    {
                        return;
                    }
                }
                const float threatDx =
                    authoredPosition.x - threatPosition.x;
                const float threatDz =
                    authoredPosition.z - threatPosition.z;

                // Укрытие прямо у ног врага не даёт безопасной фазы.
                if (threatDx * threatDx + threatDz * threatDz < 9.0f)
                {
                    ++tactical.coverRejectedProtection;
                    return;
                }

                // NavMesh обязан привести почти к авторской точке, а не
                // спроецировать NPC на противоположную сторону стены.
                DirectX::XMFLOAT3 reachable{};
                if (!NpcNavMeshSystem::FindReachablePoint(
                        registry,
                        currentRoom,
                        npcPosition,
                        authoredPosition,
                        reachable))
                {
                    ++tactical.coverRejectedPath;
                    return;
                }

                const float projectionDx = reachable.x - authoredPosition.x;
                const float projectionDz = reachable.z - authoredPosition.z;
                if (projectionDx * projectionDx + projectionDz * projectionDz >
                    0.36f)
                {
                    ++tactical.coverRejectedPath;
                    return;
                }

                // Проверяем именно фактическую достижимую позицию.
                if (!IsProtectedFromTarget(
                        registry,
                        targetEntity,
                        threatPosition,
                        reachable,
                        cover.protectionHeight,
                        world))
                {
                    ++tactical.coverRejectedProtection;
                    return;
                }

                // Проверяем не только основной локальный offset, но и
                // противоположный край длинной стены, если автор это разрешил.
                // Выбранная peek-точка также должна реально лежать на NavMesh.
                bool foundReachablePeek = false;
                bool foundVisiblePeek = false;
                DirectX::XMFLOAT3 peekPosition{};
                const int peekSideCount = cover.allowOppositePeek ? 2 : 1;
                for (int sideIndex = 0; sideIndex < peekSideCount; ++sideIndex)
                {
                    const float sign = sideIndex == 0 ? 1.0f : -1.0f;
                    const DirectX::XMFLOAT3 localOffset{
                        cover.peekOffset.x * sign,
                        cover.peekOffset.y,
                        cover.peekOffset.z * sign
                    };
                    const DirectX::XMFLOAT3 desiredPeek =
                        LocalOffset(coverTransform, localOffset);
                    DirectX::XMFLOAT3 reachablePeek{};
                    if (!NpcNavMeshSystem::FindReachablePoint(
                            registry,
                            currentRoom,
                            reachable,
                            desiredPeek,
                            reachablePeek))
                    {
                        continue;
                    }

                    const float peekProjectionDx =
                        reachablePeek.x - desiredPeek.x;
                    const float peekProjectionDz =
                        reachablePeek.z - desiredPeek.z;
                    if (peekProjectionDx * peekProjectionDx +
                        peekProjectionDz * peekProjectionDz > 0.64f)
                    {
                        continue;
                    }

                    foundReachablePeek = true;
                    if (!CanSeeTargetFromPeek(
                            targetEntity,
                            reachablePeek,
                            threatPosition,
                            world))
                    {
                        continue;
                    }

                    peekPosition = reachablePeek;
                    foundVisiblePeek = true;
                    break;
                }

                if (!foundVisiblePeek)
                {
                    if (foundReachablePeek)
                        ++tactical.coverRejectedSight;
                    else
                        ++tactical.coverRejectedPath;
                    return;
                }

                const float score = ScoreCandidate(
                    tactical,
                    npcPosition,
                    threatPosition,
                    reachable);
                if (score >= best.score)
                    return;

                best.found = true;
                best.dynamic = false;
                best.coverEntity = coverEntity;
                best.coverPosition = reachable;
                best.peekPosition = peekPosition;
                best.crouched = cover.crouched;
                best.score = score;
            });
    }

    // Запасной поиск без заранее расставленного Entity.
    static void FindBestDynamic(
        ECSRegistry& registry,
        Entity targetEntity,
        Entity currentRoom,
        const DirectX::XMFLOAT3& npcPosition,
        const DirectX::XMFLOAT3& threatPosition,
        NpcTacticalComponent& tactical,
        btDynamicsWorld* world,
        const DirectX::XMFLOAT3* excludedPosition,
        float exclusionRadius,
        NpcCoverSearchResult& best)
    {
        const int sampleCount =
            (std::max)(8, tactical.dynamicCoverSamples);
        const float searchRadius =
            (std::max)(3.0f, tactical.dynamicCoverSearchRadius);

        for (int sample = 0; sample < sampleCount; ++sample)
        {
            const float normalized =
                static_cast<float>(sample) / static_cast<float>(sampleCount);
            const float angle = normalized * DirectX::XM_2PI;

            // Три чередующихся кольца позволяют увидеть и ближайшие стены, и
            // более дальние позиции, не создавая отдельную сетку кандидатов.
            const int ring = sample % 3;
            const float radius = searchRadius * (0.45f + 0.275f * ring);
            const DirectX::XMFLOAT3 desired{
                npcPosition.x + std::cos(angle) * radius,
                npcPosition.y,
                npcPosition.z + std::sin(angle) * radius
            };

            DirectX::XMFLOAT3 reachable{};
            if (!NpcNavMeshSystem::FindReachablePoint(
                    registry,
                    currentRoom,
                    npcPosition,
                    desired,
                    reachable))
            {
                ++tactical.coverRejectedPath;
                continue;
            }

            const float projectionDx = reachable.x - desired.x;
            const float projectionDz = reachable.z - desired.z;
            if (projectionDx * projectionDx + projectionDz * projectionDz >
                2.25f)
            {
                ++tactical.coverRejectedPath;
                AddDebugRay(tactical.dynamicCoverRejectedRays,
                    desired, reachable);
                continue;
            }

            if (excludedPosition)
            {
                const float excludedDx = reachable.x - excludedPosition->x;
                const float excludedDz = reachable.z - excludedPosition->z;
                const float safeRadius = (std::max)(0.0f, exclusionRadius);
                if (excludedDx * excludedDx + excludedDz * excludedDz <=
                    safeRadius * safeRadius)
                {
                    continue;
                }
            }

            const float threatDx = reachable.x - threatPosition.x;
            const float threatDz = reachable.z - threatPosition.z;
            const float threatDistanceSquared =
                threatDx * threatDx + threatDz * threatDz;

            if (threatDistanceSquared < 9.0f ||
                !IsProtectedFromTarget(
                    registry,
                    targetEntity,
                    threatPosition,
                    reachable,
                    1.25f,
                    world))
            {
                ++tactical.coverRejectedProtection;
                AddDebugRay(tactical.dynamicCoverRejectedRays,
                    threatPosition, reachable);
                continue;
            }

            const float threatDistance = std::sqrt(threatDistanceSquared);
            const float sideX = -threatDz / threatDistance;
            const float sideZ = threatDx / threatDistance;
            bool peekFound = false;
            DirectX::XMFLOAT3 selectedPeek{};

            // Проверяем обе стороны препятствия. Первая реально доступная и
            // открытая сторона становится peekPosition этого кандидата.
            for (const float sideSign : { -1.0f, 1.0f })
            {
                const DirectX::XMFLOAT3 desiredPeek{
                    reachable.x + sideX * sideSign,
                    reachable.y,
                    reachable.z + sideZ * sideSign
                };
                DirectX::XMFLOAT3 reachablePeek{};

                if (!NpcNavMeshSystem::FindReachablePoint(
                        registry,
                        currentRoom,
                        npcPosition,
                        desiredPeek,
                        reachablePeek))
                {
                    continue;
                }

                const float peekDx = reachablePeek.x - desiredPeek.x;
                const float peekDz = reachablePeek.z - desiredPeek.z;
                if (peekDx * peekDx + peekDz * peekDz > 0.64f)
                    continue;

                AddDebugRay(tactical.dynamicCoverPeekRays,
                    reachablePeek, threatPosition);

                if (CanSeeTargetFromPeek(
                        targetEntity,
                        reachablePeek,
                        threatPosition,
                        world))
                {
                    selectedPeek = reachablePeek;
                    peekFound = true;
                    break;
                }
            }

            if (!peekFound)
            {
                ++tactical.coverRejectedSight;
                continue;
            }

            const float score = ScoreCandidate(
                tactical,
                npcPosition,
                threatPosition,
                reachable);
            if (score >= best.score)
                continue;

            best.found = true;
            best.dynamic = true;
            best.coverEntity = INVALID_ENTITY;
            best.coverPosition = reachable;
            best.peekPosition = selectedPeek;
            best.crouched = false;
            best.score = score;
        }

        if (best.found && best.dynamic)
        {
            AddDebugRay(tactical.dynamicCoverAcceptedRays,
                npcPosition, best.coverPosition);
        }
    }

    // Меньшая оценка означает более привлекательное укрытие.
    static float ScoreCandidate(
        const NpcTacticalComponent& tactical,
        const DirectX::XMFLOAT3& npcPosition,
        const DirectX::XMFLOAT3& threatPosition,
        const DirectX::XMFLOAT3& candidate)
    {
        const float travelDx = candidate.x - npcPosition.x;
        const float travelDz = candidate.z - npcPosition.z;
        const float travelSquared =
            travelDx * travelDx + travelDz * travelDz;

        const float candidateDx = candidate.x - threatPosition.x;
        const float candidateDz = candidate.z - threatPosition.z;
        const float candidateDistance = std::sqrt(
            candidateDx * candidateDx + candidateDz * candidateDz);

        const float currentDx = npcPosition.x - threatPosition.x;
        const float currentDz = npcPosition.z - threatPosition.z;
        const float currentDistance = std::sqrt(
            currentDx * currentDx + currentDz * currentDz);

        float desiredRange = (std::max)(3.0f, tactical.preferredRange);
        float rangeWeight = 0.45f;

        if (tactical.activeStyle == NpcTacticalStyle::Assault)
        {
            desiredRange *= 0.80f;
            rangeWeight = 0.20f;
        }
        else if (tactical.activeStyle == NpcTacticalStyle::Defensive)
        {
            desiredRange *= 1.20f;
            rangeWeight = 1.40f;
        }

        const float rangeError = candidateDistance - desiredRange;
        float score =
            travelSquared + rangeError * rangeError * rangeWeight;

        // Defensive почти всегда предпочитает точку, которая не приближает его
        // к пушащему противнику. Это штраф, а не жёсткий запрет: при отсутствии
        // других вариантов NPC всё ещё способен выбрать единственную защиту.
        if (tactical.activeStyle == NpcTacticalStyle::Defensive &&
            candidateDistance < currentDistance)
        {
            const float unwantedAdvance = currentDistance - candidateDistance;
            score += 25.0f + unwantedAdvance * unwantedAdvance * 8.0f;
        }

        // Для наступательных стилей новая точка должна менять линию огня, а
        // не быть соседним маркером на той же прямой. Максимальную награду
        // получает боковой угол; Defensive сохраняет приоритет безопасности.
        if (tactical.activeStyle != NpcTacticalStyle::Defensive &&
            currentDistance > 0.1f && candidateDistance > 0.1f)
        {
            const float alignment = std::clamp(
                (currentDx * candidateDx + currentDz * candidateDz) /
                    (currentDistance * candidateDistance),
                -1.0f,
                1.0f);
            const float lateralAngle = 1.0f - std::abs(alignment);
            const float angleWeight =
                tactical.activeStyle == NpcTacticalStyle::Assault
                    ? 18.0f
                    : 8.0f;
            score -= lateralAngle * tactical.flankWeight * angleWeight;

            // Штурмовик действительно перебегает на новый угол, а не меняет
            // один соседний маркер на другой в пределах пары шагов.
            if (tactical.activeStyle == NpcTacticalStyle::Assault &&
                travelSquared < 9.0f)
            {
                score += 18.0f;
            }
        }

        return score;
    }

    // Превращает локальное смещение авторского маркера в мировую позицию.
    static DirectX::XMFLOAT3 LocalOffset(
        const TransformComponent& transform,
        const DirectX::XMFLOAT3& offset)
    {
        DirectX::XMFLOAT3 result{};
        DirectX::XMStoreFloat3(
            &result,
            DirectX::XMVector3TransformCoord(
                DirectX::XMLoadFloat3(&offset),
                transform.GetWorldMatrix()));
        return result;
    }

    // Проверяет возможность стрельбы из peekPosition, не изменяя targetVisible.
    static bool CanSeeTargetFromPeek(
        Entity targetEntity,
        DirectX::XMFLOAT3 peekPosition,
        const DirectX::XMFLOAT3& targetPosition,
        btDynamicsWorld* world)
    {
        peekPosition.y += 1.45f;

        for (const float targetHeight : { 1.0f, 1.6f, 0.55f })
        {
            DirectX::XMFLOAT3 aimPoint = targetPosition;
            aimPoint.y += targetHeight;
            const DirectX::XMFLOAT3 direction{
                aimPoint.x - peekPosition.x,
                aimPoint.y - peekPosition.y,
                aimPoint.z - peekPosition.z
            };
            const float distance = DirectX::XMVectorGetX(
                DirectX::XMVector3Length(DirectX::XMLoadFloat3(&direction)));

            if (distance <= 0.01f)
                continue;

            PhysicsRaycastHit hit{};
            PhysicsRaycast::CastRay(
                peekPosition,
                direction,
                world,
                distance,
                INVALID_ENTITY,
                nullptr,
                hit);

            if (!hit.collisionObject || hit.entity == targetEntity)
                return true;
        }

        return false;
    }

    // Debug renderer ожидает пары точек: начало и конец каждого отрезка.
    static void AddDebugRay(
        std::vector<DirectX::XMFLOAT3>& rays,
        const DirectX::XMFLOAT3& from,
        const DirectX::XMFLOAT3& to)
    {
        rays.push_back(from);
        rays.push_back(to);
    }
};
