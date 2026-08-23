#pragma once

#include <algorithm>
#include <cmath>
#include <unordered_map>

#include <DirectXMath.h>

#include <ECSTypes.h>

class CharacterExternalVelocitySystem
{
public:
    // Вызывается после того, как CharacterController забрал силы прошлого кадра.
    static void BeginAtmosphereFrame()
    {
        s_velocities.clear();
    }

    static void AddVelocity(
        Entity entity,
        const DirectX::XMFLOAT3& velocity,
        float maxSpeed)
    {
        auto& total = s_velocities[entity];
        total.x += velocity.x;
        total.y += velocity.y;
        total.z += velocity.z;

        const float lengthSquared =
            total.x * total.x + total.y * total.y + total.z * total.z;
        const float limit = (std::max)(0.0f, maxSpeed);
        if (limit > 0.0f && lengthSquared > limit * limit)
        {
            const float scale = limit / std::sqrt(lengthSquared);
            total.x *= scale;
            total.y *= scale;
            total.z *= scale;
        }
    }

    static DirectX::XMFLOAT3 ConsumeVelocity(Entity entity)
    {
        const auto it = s_velocities.find(entity);
        if (it == s_velocities.end())
            return { 0.0f, 0.0f, 0.0f };

        const DirectX::XMFLOAT3 velocity = it->second;
        s_velocities.erase(it);
        return velocity;
    }

private:
    inline static std::unordered_map<Entity, DirectX::XMFLOAT3> s_velocities;
};
