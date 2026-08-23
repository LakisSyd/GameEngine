#include "ExplosionSystem.h"

#include <algorithm>
#include <cmath>

#include <Components.h>
#include "ParticleSystem.h"

using namespace DirectX;

int ExplosionSystem::Emit(
    ECSRegistry& registry,
    ParticleSystem& particleSystem,
    const XMFLOAT3& position,
    const ExplosionSettings& settings)
{
    const float radius = (std::max)(0.05f, settings.radius);
    const float impulseStrength = (std::max)(0.0f, settings.impulse);
    int affectedBodies = 0;

    particleSystem.EmitExplosion(position, settings.particleScale);

    registry.ForEach<RigidBodyComponent>(
        [&](Entity, RigidBodyComponent& rigidBody)
        {
            btRigidBody* body = rigidBody.body;
            if (!body || body->isStaticOrKinematicObject() || body->getInvMass() <= 0.0f)
                return;

            const btVector3 center = body->getCenterOfMassPosition();
            btVector3 direction(
                center.x() - position.x,
                center.y() - position.y,
                center.z() - position.z);

            const float distance = static_cast<float>(direction.length());
            if (distance > radius)
                return;

            if (distance > 0.001f)
                direction /= distance;
            else
                direction = btVector3(0.0f, 1.0f, 0.0f);

            direction.setY(direction.y() + settings.upwardBias);
            direction.normalize();

            const float normalizedDistance = (std::min)(distance / radius, 1.0f);
            const float falloff = 1.0f - normalizedDistance;
            const float finalImpulse = impulseStrength * falloff * falloff;
            if (finalImpulse <= 0.001f)
                return;

            body->activate(true);
            body->applyCentralImpulse(direction * finalImpulse);

            // Небольшое вращение делает разлёт предметов менее механическим.
            const btVector3 torqueAxis = direction.cross(btVector3(0.31f, 0.87f, 0.19f));
            body->applyTorqueImpulse(torqueAxis * finalImpulse * 0.18f);
            ++affectedBodies;
        });

    return affectedBodies;
}
