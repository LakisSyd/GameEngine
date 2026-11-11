#pragma once

#include <algorithm>
#include <cmath>

#include <DirectXMath.h>

// Чистая математика боя. Здесь нет Registry, компонентов, raycast и смены
// состояний, поэтому граничные случаи можно проверять обычными unit-тестами.
namespace NpcCombatMath
{
    inline float DistanceSquared(
        const DirectX::XMFLOAT3& from,
        const DirectX::XMFLOAT3& to)
    {
        const float dx = to.x - from.x;
        const float dy = to.y - from.y;
        const float dz = to.z - from.z;
        return dx * dx + dy * dy + dz * dz;
    }

    inline float HorizontalDistanceSquared(
        const DirectX::XMFLOAT3& from,
        const DirectX::XMFLOAT3& to)
    {
        const float dx = to.x - from.x;
        const float dz = to.z - from.z;
        return dx * dx + dz * dz;
    }

    inline bool IsWithinDistanceSquared(float distanceSquared, float range)
    {
        if (!std::isfinite(distanceSquared) || distanceSquared < 0.0f ||
            !std::isfinite(range))
            return false;
        const float safeRange = (std::max)(0.0f, range);
        return distanceSquared <= safeRange * safeRange;
    }

    inline float NormalizeAngle(float angle)
    {
        if (!std::isfinite(angle))
            return 0.0f;
        return std::atan2(std::sin(angle), std::cos(angle));
    }

    inline bool TryTargetYaw(float deltaX, float deltaZ, float& result)
    {
        if (!std::isfinite(deltaX) || !std::isfinite(deltaZ) ||
            deltaX * deltaX + deltaZ * deltaZ <= 0.000001f)
        {
            return false;
        }
        result = NormalizeAngle(
            std::atan2(deltaX, deltaZ) + DirectX::XM_PI);
        return true;
    }

    inline float TargetYaw(float deltaX, float deltaZ)
    {
        float result = 0.0f;
        TryTargetYaw(deltaX, deltaZ, result);
        return result;
    }

    inline float ShortestYawDelta(float currentYaw, float targetYaw)
    {
        return NormalizeAngle(targetYaw - currentYaw);
    }

    inline float StepYawToward(
        float currentYaw,
        float targetYaw,
        float maxStep)
    {
        if (!std::isfinite(maxStep) || maxStep <= 0.0f)
            return NormalizeAngle(currentYaw);
        const float delta = ShortestYawDelta(currentYaw, targetYaw);
        return NormalizeAngle(currentYaw + std::clamp(delta, -maxStep, maxStep));
    }

    // Проверяет только горизонтальный сектор зрения. Блокирующую геометрию
    // по-прежнему обязан подтвердить PhysicsRaycast.
    inline bool IsInsideViewCone(
        float observerYaw,
        float deltaX,
        float deltaZ,
        float fieldOfViewDegrees)
    {
        const float lengthSquared = deltaX * deltaX + deltaZ * deltaZ;
        if (!std::isfinite(observerYaw) ||
            !std::isfinite(fieldOfViewDegrees) ||
            !std::isfinite(lengthSquared))
        {
            return false;
        }
        if (lengthSquared <= 0.000001f)
            return true;

        const float length = std::sqrt(lengthSquared);
        const float directionX = deltaX / length;
        const float directionZ = deltaZ / length;
        const float forwardX = -std::sin(observerYaw);
        const float forwardZ = -std::cos(observerYaw);
        const float dot = std::clamp(
            forwardX * directionX + forwardZ * directionZ,
            -1.0f,
            1.0f);
        const float safeFov = std::clamp(fieldOfViewDegrees, 0.0f, 360.0f);
        const float minimumDot = std::cos(DirectX::XMConvertToRadians(
            safeFov * 0.5f));
        return dot >= minimumDot;
    }

    inline DirectX::XMFLOAT3 HorizontalLeft(
        const DirectX::XMFLOAT3& from,
        const DirectX::XMFLOAT3& toward)
    {
        const float dx = toward.x - from.x;
        const float dz = toward.z - from.z;
        const float lengthSquared = dx * dx + dz * dz;
        if (!std::isfinite(lengthSquared))
            return { 0.0f, 0.0f, 0.0f };
        if (lengthSquared <= 0.000001f)
            return { -1.0f, 0.0f, 0.0f };
        const float inverseLength = 1.0f / std::sqrt(lengthSquared);
        return { dz * inverseLength, 0.0f, -dx * inverseLength };
    }
}
