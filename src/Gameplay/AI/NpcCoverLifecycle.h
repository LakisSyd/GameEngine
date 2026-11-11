#pragma once

#include <algorithm>

#include "../../../NPCComponents.h"

// Единая инициализация runtime-цикла укрытия. Автоматика и Combat Lab должны
// запускать одну и ту же последовательность полей; иначе новый приказ может
// унаследовать таймеры и ошибки от предыдущей точки.
class NpcCoverLifecycle
{
public:
    static void Assign(
        NpcTacticalComponent& tactical,
        Entity coverEntity,
        bool dynamic,
        const DirectX::XMFLOAT3& coverPosition,
        const DirectX::XMFLOAT3& peekPosition,
        bool usesCrouch,
        const DirectX::XMFLOAT3& currentPosition,
        bool assignedByCombatLab)
    {
        tactical.reservedCover = coverEntity;
        tactical.dynamicCoverActive = dynamic;
        tactical.debugForcedCover = assignedByCombatLab;
        tactical.debugForcedCoverAction = assignedByCombatLab;
        tactical.coverPosition = coverPosition;
        tactical.peekPosition = peekPosition;
        tactical.moveTarget = coverPosition;
        tactical.coverUsesCrouch = usesCrouch;

        tactical.movingToPeek = false;
        tactical.exposed = false;
        tactical.isPeeking = false;
        tactical.wantsPeek = false;
        tactical.peekState = NpcTacticalComponent::PeekSide::none;
        tactical.crouched = false;
        tactical.noSuitableCover = false;
        tactical.coverChangeRequested = false;

        tactical.moveElapsed = 0.0f;
        tactical.noProgressTime = 0.0f;
        tactical.lastProgressPosition = currentPosition;
        tactical.coverValidationFailureTime = 0.0f;
        tactical.phaseTimer = 0.0f;
        tactical.coverHoldTimer = (std::max)(
            0.0f,
            tactical.minimumCoverHoldDuration);
        tactical.failedPeekCount = 0;
        tactical.completedPeekCount = 0;
        tactical.decisionTimer = (std::max)(
            0.2f,
            tactical.decisionInterval);
        tactical.firedOnRoute = false;
        tactical.firingPositionHold = 0.0f;
        tactical.movingAimTime = 0.0f;
        tactical.movingBurstShotsRemaining = 0;
        tactical.movingBurstPauseTimer = 0.0f;
    }

    static void Clear(NpcTacticalComponent& tactical)
    {
        tactical.reservedCover = INVALID_ENTITY;
        tactical.dynamicCoverActive = false;
        tactical.debugForcedCover = false;
        tactical.debugForcedCoverAction = false;
        tactical.movingToPeek = false;
        tactical.exposed = false;
        tactical.isPeeking = false;
        tactical.wantsPeek = false;
        tactical.peekState = NpcTacticalComponent::PeekSide::none;
        tactical.crouched = false;
        tactical.noSuitableCover = false;
        tactical.coverChangeRequested = false;
        tactical.failedPeekCount = 0;
        tactical.completedPeekCount = 0;
        tactical.coverValidationFailureTime = 0.0f;
        tactical.phaseTimer = 0.0f;
        tactical.coverHoldTimer = 0.0f;
        tactical.moveElapsed = 0.0f;
        tactical.noProgressTime = 0.0f;
        tactical.decisionTimer = 0.0f;
        tactical.firedOnRoute = false;
        tactical.firingPositionHold = 0.0f;
    }
};
