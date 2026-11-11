#pragma once

#include <algorithm>
#include <cmath>

#include "NPCComponents.h"

// Результат одного узкого решения: требуется ли NPC прямо сейчас искать укрытие.
// Структура специально хранит причину — любой новый тактический модуль должен
// уметь объяснить своё решение инспектору и журналу.
struct NpcCoverRequestDecision
{
    bool requested = false;
    const char* reason = "Укрытие не требуется";
};

// Модуль ВЫСОКОУРОВНЕВЫХ правил индивидуального боя.
//
// ЧТО ОН ДЕЛАЕТ:
//   * выбирает активный стиль Auto/Balanced/Assault/Defensive;
//   * решает, является ли сигнал угрозы достаточной причиной искать укрытие.
//
// ЧЕГО ОН НЕ ДЕЛАЕТ:
//   * не перебирает геометрию и точки укрытий;
//   * не строит NavMesh-маршруты;
//   * не меняет NpcState и не нажимает спусковой крючок.
//
// Благодаря этой границе будущие правила Push/Retreat/Suppress можно добавлять
// сюда, не затрагивая физический поиск позиции и исполнение движения.
class NpcTacticalDecisionSystem
{
public:
    // Обновляет activeStyle только по данным, которые NPC действительно знает.
    // threatPosition должна быть последней подтверждённой позицией, а не текущим
    // Transform скрытого противника.
    static void UpdateStyle(
        NpcTacticalComponent& tactical,
        const DirectX::XMFLOAT3& npcPosition,
        const DirectX::XMFLOAT3& threatPosition,
        float deltaTime)
    {
        const float dx = threatPosition.x - npcPosition.x;
        const float dz = threatPosition.z - npcPosition.z;
        const float threatDistance = std::sqrt(dx * dx + dz * dz);

        // Положительное значение означает, что последняя известная позиция
        // угрозы стала ближе. Это оценка между наблюдениями, а не всезнание.
        if (tactical.hasThreatDistanceSample && deltaTime > 0.0001f)
        {
            tactical.enemyClosingSpeed =
                (tactical.lastThreatDistance - threatDistance) / deltaTime;
        }
        else
        {
            tactical.enemyClosingSpeed = 0.0f;
            tactical.hasThreatDistanceSample = true;
        }
        tactical.lastThreatDistance = threatDistance;

        // Ручная настройка автора всегда имеет приоритет над Auto.
        if (tactical.selectedStyle != NpcTacticalStyle::Auto)
        {
            tactical.activeStyle = tactical.selectedStyle;
            tactical.tacticCommitTimer = 0.0f;
            tactical.tacticReason =
                "Стиль принудительно выбран в инспекторе";
            return;
        }

        // Уже принятое решение нельзя пересматривать каждый кадр. При этом
        // измерение дистанции выше продолжает обновляться для следующей оценки.
        if (tactical.tacticCommitTimer > 0.0f)
        {
            return;
        }

        const float aggression = (std::max)(
            0.0f,
            (std::min)(1.0f, tactical.tacticalAggression));
        const float preferredRange =
            (std::max)(3.0f, tactical.preferredRange);

        // Почти рукопашная дистанция опасна для любого стрелка. Обычная
        // «близкая» дистанция заставляет отступать только осторожного NPC.
        const bool enemyIsDangerouslyClose =
            threatDistance < (std::max)(2.5f, preferredRange * 0.35f);
        const bool enemyIsCloseForCautiousNpc =
            threatDistance < preferredRange * 0.75f && aggression < 0.65f;
        const bool enemyIsPushing = tactical.enemyClosingSpeed > 0.75f;
        const bool cautiousUnderFire =
            tactical.underFireTime > 0.0f && aggression < 0.65f;

        NpcTacticalStyle desiredStyle = NpcTacticalStyle::Balanced;
        const char* reason =
            "Средняя агрессивность: сбалансированный бой";

        if (enemyIsDangerouslyClose)
        {
            desiredStyle = NpcTacticalStyle::Defensive;
            reason = "Противник почти вплотную: разрываем дистанцию";
        }
        else if (enemyIsCloseForCautiousNpc)
        {
            desiredStyle = NpcTacticalStyle::Defensive;
            reason =
                "Противник близко к осторожному NPC: удерживаем дистанцию";
        }
        else if (enemyIsPushing && aggression < 0.75f)
        {
            desiredStyle = NpcTacticalStyle::Defensive;
            reason = "Противник быстро сближается: занимаем оборону";
        }
        else if (cautiousUnderFire)
        {
            desiredStyle = NpcTacticalStyle::Defensive;
            reason = "Осторожный NPC находится под огнём";
        }
        else if (aggression >= 0.65f)
        {
            desiredStyle = NpcTacticalStyle::Assault;
            reason = "Высокая агрессивность: разрешено давление";
        }
        else if (aggression <= 0.35f)
        {
            desiredStyle = NpcTacticalStyle::Defensive;
            reason =
                "Низкая агрессивность: приоритет дистанции и защиты";
        }

        tactical.activeStyle = desiredStyle;
        tactical.tacticReason = reason;
        tactical.tacticCommitTimer = (std::max)(
            1.0f,
            tactical.tacticCommitDuration);
    }

    // Решает только необходимость укрытия. Поиск самой точки выполняет
    // NpcCoverQuery, а жизненный цикл укрытия — NpcCoverSystem.
    static NpcCoverRequestDecision EvaluateCoverRequest(
        const NpcTacticalComponent& tactical,
        const CharacterVitalsComponent& vitals)
    {
        // Оборонительный NPC старается заранее иметь защищённую позицию.
        if (tactical.activeStyle == NpcTacticalStyle::Defensive)
        {
            return {
                true,
                "Оборонительный стиль заранее ищет защищённую позицию"
            };
        }

        // Штурм не должен разрушаться от near miss или первой случайной пули.
        // Серия попаданий и критическое здоровье уже являются веской причиной
        // временно прервать давление и восстановить позиционное преимущество.
        if (tactical.activeStyle == NpcTacticalStyle::Assault)
        {
            const float healthFraction = vitals.maxHealth > 0.001f
                ? vitals.health / vitals.maxHealth
                : 0.0f;
            const bool mustRecover =
                tactical.recentDamageHits >= 2 || healthFraction <= 0.40f;

            return {
                mustRecover,
                mustRecover
                    ? "Штурм прерван серией попаданий или критическим здоровьем"
                    : "Штурм продолжает давление несмотря на одиночную угрозу"
            };
        }

        // Сбалансированный стиль реагирует как на попадание, так и на near miss.
        const bool threatened = tactical.underFireTime > 0.0f;
        return {
            threatened,
            threatened
                ? "Сбалансированный NPC получил сигнал угрозы"
                : "Нет недавнего входящего огня"
        };
    }
};
