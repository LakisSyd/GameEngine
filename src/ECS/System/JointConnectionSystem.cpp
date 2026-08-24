#include "JointConnectionSystem.h"
#include "Components.h"
#include "PhysicsWorld.h"

void JointConnectionSystem::Update(ECSRegistry& registry)
{
    auto& joints = registry.GetComponents<JointConnectionComponent>();

    for (auto& [entity, jointComp] : joints)
    {
        // ═══════════════════════════════════════════════════════════════
        // Если джойнт ещё не создан, цель выбрана и не сломан — создаём
        // ═══════════════════════════════════════════════════════════════
        if (!jointComp.joint && jointComp.targetEntity != INVALID_ENTITY && !jointComp.isBroken)
        {
            CreateJoint(registry, entity, jointComp);
        }

        // ═══════════════════════════════════════════════════════════════
        // Если цель сброшена, но джойнт существует — удаляем
        // ═══════════════════════════════════════════════════════════════
        if (jointComp.targetEntity == INVALID_ENTITY && jointComp.joint)
        {
            RemoveJoint(registry, jointComp);
        }

        // ═══════════════════════════════════════════════════════════════
        // 🔥 FIX: Проверяем, не сломался ли джойнт (Bullet мог отключить)
        // ═══════════════════════════════════════════════════════════════
        if (jointComp.joint && !jointComp.isBroken)
        {
            if (!jointComp.joint->isEnabled())
            {
                jointComp.isBroken = true;
                RemoveJoint(registry, jointComp);
                OutputDebugStringA("[JointConnectionSystem] Joint broken by physics!\n");
            }
        }
    }
}

void JointConnectionSystem::CreateJoint(ECSRegistry& registry, Entity entity, JointConnectionComponent& jointComp)
{
    auto* rbA = registry.GetComponent<RigidBodyComponent>(entity);
    auto* rbB = registry.GetComponent<RigidBodyComponent>(jointComp.targetEntity);

    if (!rbA || !rbB || !rbA->body || !rbB->body)
    {
        OutputDebugStringA("[JointConnectionSystem] ERROR: Missing rigid bodies!\n");
        return;
    }

    btTransform frameA, frameB;
    frameA.setIdentity();
    frameB.setIdentity();
    frameA.setOrigin(jointComp.pivotA);
    frameB.setOrigin(jointComp.pivotB);

    btFixedConstraint* joint = new btFixedConstraint(*rbA->body, *rbB->body, frameA, frameB);
    joint->setBreakingImpulseThreshold(jointComp.breakForce);

    // 🔥 FIX: Повышаем стабильность constraint
    joint->setOverrideNumSolverIterations(100);

    btDiscreteDynamicsWorld* world = PhysicsWorld::Get().GetWorld();
    if (world)
    {
        world->addConstraint(joint, jointComp.disableCollisions);
        jointComp.joint = joint;
        jointComp.registeredInPhysicsWorld = true;

        char buf[256];
        sprintf_s(buf, "[JointConnectionSystem] Joint created: %u <-> %u, breakForce=%.0f\n",
            entity, jointComp.targetEntity, jointComp.breakForce);
        OutputDebugStringA(buf);
    }
    else
    {
        OutputDebugStringA("[JointConnectionSystem] ERROR: Physics world is null!\n");
        delete joint;
    }
}

void JointConnectionSystem::RemoveJoint(ECSRegistry& registry, JointConnectionComponent& jointComp)
{
    jointComp.ResetJoint();
}

void JointConnectionSystem::RemoveJointsForEntity(ECSRegistry& registry, Entity entity)
{
    auto& joints = registry.GetComponents<JointConnectionComponent>();

    for (auto& [e, jointComp] : joints)
    {
        if (e == entity || jointComp.targetEntity == entity)
        {
            RemoveJoint(registry, jointComp);
            if (jointComp.targetEntity == entity)
                jointComp.targetEntity = INVALID_ENTITY;
        }
    }
}
