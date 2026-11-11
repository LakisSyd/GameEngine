#pragma once
#include <algorithm>
#include <cmath>
#include "WeaponComponent.h"

// Общие правила выстрела не зависят от ввода игрока и решений NPC.
namespace WeaponFiringRules
{
    inline void AdvanceTimers(WeaponComponent& weapon, float dt)
    {
        if (!std::isfinite(dt) || dt <= 0.0f) return;
        weapon.currentCooldown = (std::max)(0.0f, weapon.currentCooldown - dt);
        weapon.shotFlashTime = (std::max)(0.0f, weapon.shotFlashTime - dt);
        weapon.currentRecoilDegrees = (std::max)(0.0f,
            weapon.currentRecoilDegrees - (std::max)(0.0f, weapon.recoilRecoveryDegrees) * dt);

        if (!weapon.isReloading)
            return;

        weapon.reloadTimer = (std::max)(0.0f, weapon.reloadTimer - dt);
        if (weapon.reloadTimer > 0.0f)
            return;

        // Transfer only the rounds that are actually missing. A partial
        // magazine reload therefore preserves the rounds already loaded.
        const int capacity = (std::max)(0, weapon.magazineSize);
        const int loaded = std::clamp(weapon.ammoInMagazine, 0, capacity);
        const int reserve = (std::max)(0, weapon.reserveAmmo);
        const int transferred = (std::min)(capacity - loaded, reserve);
        weapon.ammoInMagazine = loaded + transferred;
        weapon.reserveAmmo = reserve - transferred;
        weapon.isReloading = false;
    }

    inline bool CanStartReload(const WeaponComponent& weapon)
    {
        return !weapon.isReloading &&
            weapon.magazineSize > 0 &&
            weapon.ammoInMagazine < weapon.magazineSize &&
            weapon.reserveAmmo > 0 &&
            std::isfinite(weapon.reloadDuration) &&
            weapon.reloadDuration > 0.0f;
    }

    inline bool StartReload(WeaponComponent& weapon)
    {
        if (!CanStartReload(weapon))
            return false;

        weapon.ammoInMagazine = std::clamp(
            weapon.ammoInMagazine, 0, weapon.magazineSize);
        weapon.reserveAmmo = (std::max)(0, weapon.reserveAmmo);
        weapon.reloadTimer = weapon.reloadDuration;
        weapon.isReloading = true;
        weapon.triggerHeld = false;
        return true;
    }

    inline bool CanFire(const WeaponComponent& weapon)
    {
        return std::isfinite(weapon.fireRate) && weapon.fireRate > 0.0f &&
            std::isfinite(weapon.range) && weapon.range > 0.0f &&
            std::isfinite(weapon.currentCooldown) && weapon.currentCooldown <= 0.0f &&
            !weapon.isReloading && weapon.magazineSize > 0 &&
            weapon.ammoInMagazine > 0;
    }

    inline bool ValidRay(const DirectX::XMFLOAT3& origin, const DirectX::XMFLOAT3& direction)
    {
        const float lengthSquared = direction.x*direction.x + direction.y*direction.y + direction.z*direction.z;
        return std::isfinite(origin.x) && std::isfinite(origin.y) && std::isfinite(origin.z) &&
            std::isfinite(lengthSquared) && lengthSquared > 0.000001f;
    }

    inline float MotionMultiplier(const WeaponComponent& weapon, bool moving)
    {
        return moving ? (std::max)(1.0f, weapon.movingSpreadMultiplier) : 1.0f;
    }

    inline void CommitShot(WeaponComponent& weapon, float motionScale = 1.0f)
    {
        // Регистрируем и промахи: иначе они не тратят время между выстрелами.
        weapon.currentCooldown = 1.0f / weapon.fireRate;
        weapon.ammoInMagazine = (std::max)(0, weapon.ammoInMagazine - 1);
        ++weapon.shotsFired;
        weapon.shotFlashTime = 0.08f;
        weapon.currentRecoilDegrees = (std::clamp)(weapon.currentRecoilDegrees +
            (std::max)(0.0f, weapon.recoilPerShotDegrees) * motionScale,
            0.0f, (std::max)(0.0f, weapon.maxRecoilDegrees));
    }

    // Равномерный конус по телесному углу, без перекоса по осям и зависимости от FPS.
    inline DirectX::XMFLOAT3 ConeDirection(const DirectX::XMFLOAT3& direction,
        float degrees, float radialSample, float angularSample)
    {
        using namespace DirectX;
        const XMVECTOR forward = XMVector3Normalize(XMLoadFloat3(&direction));
        const float angle = XMConvertToRadians(std::isfinite(degrees)
            ? (std::clamp)(degrees, 0.0f, 45.0f) : 0.0f);
        const float c = 1.0f - (std::clamp)(radialSample, 0.0f, 1.0f) * (1.0f - std::cos(angle));
        const float s = std::sqrt((std::max)(0.0f, 1.0f - c*c));
        const float phi = angularSample * XM_2PI;
        const XMVECTOR reference = std::abs(XMVectorGetY(forward)) < 0.99f
            ? XMVectorSet(0, 1, 0, 0) : XMVectorSet(1, 0, 0, 0);
        const XMVECTOR right = XMVector3Normalize(XMVector3Cross(reference, forward));
        const XMVECTOR up = XMVector3Cross(forward, right);
        XMFLOAT3 result;
        XMStoreFloat3(&result, XMVector3Normalize(XMVectorAdd(XMVectorScale(forward, c),
            XMVectorAdd(XMVectorScale(right, s * std::cos(phi)), XMVectorScale(up, s * std::sin(phi))))));
        return result;
    }
}
