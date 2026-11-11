#pragma once
#include "../../../NPCComponents.h"
#include "NpcCombatMath.h"

// Чистые правила, общие для исполнения и регрессионных тестов.
namespace NpcCombatDecisionRules
{
    // Вплотную персонаж замечает противника боковым зрением, по звуку и контакту
    // капсул. Это только обход FOV: препятствия по-прежнему проверяет raycast.
    inline bool HasCloseProximityAwareness(
        float horizontalDistanceSquared,
        float awarenessRange)
    {
        return NpcCombatMath::IsWithinDistanceSquared(
            horizontalDistanceSquared,
            awarenessRange);
    }

    // Длинная перебежка должна смотреть по маршруту. На цель корпус направлен
    // стоя, во время атаки и при коротком боковом шаге к peek-точке.
    inline bool ShouldFaceCombatAttention(
        NpcState state,
        bool isMoving,
        bool movingToPeek)
    {
        return !isMoving ||
            state == NpcState::Attacking ||
            movingToPeek;
    }

    // Полнотелая peek-поза допустима только после прибытия к краю. Один старый
    // runtime-флаг не должен перекрывать Walk/Run во время следующего маршрута.
    inline bool CanUsePeekPose(
        bool isPeeking,
        bool exposed,
        bool isMoving,
        NpcState state)
    {
        return isPeeking && exposed && !isMoving &&
            state == NpcState::Attacking;
    }

    // Фланг считается законченным только когда NPC реально увидел цель, успел
    // выстрелить из этой позиции, сейчас не подавлен огнём и не слипся с союзником.
    inline bool CanFinishFlank(bool visible, bool firedFromHere, bool underFire, bool separated)
    {
        return visible && firedFromHere && !underFire && separated;
    }
    // Стрельба на ходу разрешена только внутри боевого перемещения. Chasing
    // означает сближение/смену угла, TakingCover — путь к cover/peek position.
    // Патруль, ремонт и ручная команда сюда не входят. Одного targetEntity тоже
    // недостаточно: NPC обязан лично видеть цель именно в текущем кадре.
    inline bool CanUseMovingFire(NpcState state, bool visible)
    {
        const bool combatMovement =
            state == NpcState::Chasing ||
            state == NpcState::TakingCover;
        return visible && combatMovement;
    }
    // Возврат за стену — аварийный рывок. Выход к peekPosition,
    // наоборот, не помечаем бегом: для него будет отдельная lean-анимация.
    // При погоне бежим, если путь длинный или NPC недавно получил огонь.
    inline bool ShouldRunDuringCombat(
        NpcState state,
        bool movingToPeek,
        bool underFire,
        float distance,
        float runDistance)
    {
        if (state == NpcState::TakingCover)
            return !movingToPeek;
        if (state == NpcState::Chasing)
            return underFire || distance >= (std::max)(0.0f, runDistance);
        return false;
    }

    // Пустой магазин не даёт команду выглянуть. Если есть резерв,
    // NPC ждёт конца перезарядки; если нет — остаётся за защитой.
    inline bool ShouldHoldCoverForWeapon(
        bool reloading,
        int ammoInMagazine,
        int reserveAmmo)
    {
        // Нуль в магазине уже блокирует выход, ещё до того как
        // NpcRangedCombatSystem в конце кадра успеет запустить reloadTimer.
        (void)reserveAmmo;
        return reloading || ammoInMagazine <= 0;
    }

    // Неудачный peek — это попадание, а не просто истёкший таймер.
    inline bool ShouldRelocateAfterFailedPeeks(int failedPeeks, int limit)
    {
        return failedPeeks >= (std::max)(1, limit);
    }

    // Количество безопасных выходов, после которого один и тот же угол уже
    // считается прочитанным противником. Штурмовик чаще меняет направление,
    // оборонительный NPC дольше использует надёжную позицию.
    inline int PeekCyclesBeforeReposition(NpcTacticalStyle style)
    {
        switch (style)
        {
        case NpcTacticalStyle::Assault: return 1;
        case NpcTacticalStyle::Balanced: return 2;
        case NpcTacticalStyle::Defensive: return 3;
        default: return 2;
        }
    }

    inline bool ShouldRepositionAfterPeekCycle(
        NpcTacticalStyle style,
        int completedPeeks,
        int failedPeeks,
        int failedPeekLimit)
    {
        return ShouldRelocateAfterFailedPeeks(failedPeeks, failedPeekLimit) ||
            completedPeeks >= PeekCyclesBeforeReposition(style);
    }

    // Тихая атака не выдаёт направление. Обычное попадание
    // даёт его лишь в пределах локальной слышимости/ощущения выстрела.
    inline bool CanReactToIncomingFireDirection(
        bool silentKill,
        float distanceSquared,
        float awarenessRange)
    {
        const float safeRange = (std::max)(0.0f, awarenessRange);
        return !silentKill &&
            std::isfinite(distanceSquared) &&
            distanceSquared > 0.0001f &&
            distanceSquared <= safeRange * safeRange;
    }
    // Рабочая позиция не меняется просто из-за таймера: новый угол нужен, если
    // враг больше не виден либо текущую позицию накрыли ответным огнём.
    inline bool NeedsNewAngle(bool visible, bool underFire)
    {
        // Сам по себе истёкший таймер не причина покидать рабочую позицию.
        return !visible || underFire;
    }
}
