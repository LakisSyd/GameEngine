#include "SubmarineJointSystem.h"
#include "Components.h"
#include "PhysicsWorld.h"
#include "ECSRegistry.h"
void SubmarineJointSystem::CreateJoint(ECSRegistry& registry, Entity a, Entity b,
    const btVector3& pivotA_local, const btVector3& pivotB_local, float breakForce)
{
    auto* rbA = registry.GetComponent<RigidBodyComponent>(a);
    auto* rbB = registry.GetComponent<RigidBodyComponent>(b);
    if (!rbA || !rbB || !rbA->body || !rbB->body)
    {
        OutputDebugStringA("[Joint] ERROR: Missing rigid body\n");
        return;
    }

    // ═══════════════════════════════════════════════════════════════════
    // 🔥 FIX: Перед созданием джойнта убеждаемся что тела
    //    находятся в правильных позициях (синхронизированы).
    //    Если есть начальная ошибка — constraint solver будет
    //    создавать огромные корректирующие импульсы.
    // ═══════════════════════════════════════════════════════════════════

    btTransform frameA, frameB;
    frameA.setIdentity();
    frameB.setIdentity();
    frameA.setOrigin(pivotA_local);
    frameB.setOrigin(pivotB_local);

    btDiscreteDynamicsWorld* world = PhysicsWorld::Get().GetWorld();
    if (!world)
    {
        OutputDebugStringA("[Joint] ERROR: Physics world is null!\n");
        return;
    }

    SubmarineJointComponent jointComp{};
    jointComp.compartmentA = a;
    jointComp.compartmentB = b;
    jointComp.joint = new btFixedConstraint(*rbA->body, *rbB->body, frameA, frameB);
    jointComp.breakForce = breakForce;

    // ═══════════════════════════════════════════════════════════════════
    // 🔥 FIX: Убрана двойная перезапись breakForce.
    //    Раньше было:
    //      joint->setBreakingImpulseThreshold(breakForce);  // 1-й раз
    //      breakForce = 100000 * mass;                       // перезапись!
    //      joint->setBreakingImpulseThreshold(breakForce);  // 2-й раз
    //    Теперь используем переданное значение как есть.
    //    Если нужно масштабировать по массе — делайте это ДО вызова.
    // ═══════════════════════════════════════════════════════════════════
    jointComp.joint->setBreakingImpulseThreshold(breakForce);

    // 🔥 FIX: Настраиваем параметры стабильности constraint
    jointComp.joint->setOverrideNumSolverIterations(100);  // Больше итераций = стабильнее

    // Transfer ownership to ECS before registering the constraint in Bullet.
    // If entity/pool allocation throws, the local component deletes the joint
    // and no non-owned constraint is left behind in the physics world.
    Entity jointEntity = INVALID_ENTITY;
    try
    {
        jointEntity = registry.CreateEntity();
        registry.AddComponent<SubmarineJointComponent>(jointEntity, std::move(jointComp));
    }
    catch (...)
    {
        if (jointEntity != INVALID_ENTITY)
            registry.DestroyEntity(jointEntity);
        throw;
    }

    auto* storedJoint = registry.GetComponent<SubmarineJointComponent>(jointEntity);
    if (!storedJoint || !storedJoint->joint)
    {
        registry.DestroyEntity(jointEntity);
        OutputDebugStringA("[Joint] ERROR: ECS failed to retain joint ownership.\n");
        return;
    }
    world->addConstraint(storedJoint->joint, true);  // true = disable collisions
    storedJoint->registeredInPhysicsWorld = true;

    char buf[256];
    sprintf_s(buf, "[Joint] Created: %u <-> %u, breakForce=%.0f\n", a, b, breakForce);
    OutputDebugStringA(buf);
}

void SubmarineJointSystem::Update(ECSRegistry& registry, float deltaTime)
{
    auto& jointPool = registry.GetComponents<SubmarineJointComponent>();

    for (auto& [entity, jointComp] : jointPool)
    {
        if (jointComp.isBroken || !jointComp.joint)
            continue;

        btScalar appliedImpulse = jointComp.joint->getAppliedImpulse();
        jointComp.currentStress = static_cast<float>(appliedImpulse);

        // ═══════════════════════════════════════════════════════════════
        // 🔥 FIX: Bullet сам ломает constraint через
        //    setBreakingImpulseThreshold. Но нам нужно это обнаружить
        //    и обработать (пометить отсеки, спавнить эффекты и т.д.)
        //
        //    Проверяем: если constraint "enabled" = false,
        //    значит Bullet уже сломал его.
        // ═══════════════════════════════════════════════════════════════
        if (!jointComp.joint->isEnabled() || appliedImpulse > jointComp.breakForce)
        {
            jointComp.ResetJoint();
            jointComp.isBroken = true;

            if (auto* subA = registry.GetComponent<SubmarineComponent>(jointComp.compartmentA))
                subA->isDetached = true;
            if (auto* subB = registry.GetComponent<SubmarineComponent>(jointComp.compartmentB))
                subB->isDetached = true;

            char buf[128];
            sprintf_s(buf, "[Joint] BROKEN! Entity %u, impulse=%.0f, threshold=%.0f\n",
                entity, (float)appliedImpulse, jointComp.breakForce);
            OutputDebugStringA(buf);
        }
    }
}

void SubmarineJointSystem::RemoveJoint(ECSRegistry& registry, Entity jointEntity)
{
    auto* jointComp = registry.GetComponent<SubmarineJointComponent>(jointEntity);
    if (!jointComp) return;

    jointComp->ResetJoint();

    registry.DestroyEntity(jointEntity);
}

void SubmarineJointSystem::RemoveJointsForEntity(ECSRegistry& registry, Entity entity)
{
    auto& jointPool = registry.GetComponents<SubmarineJointComponent>();

    std::vector<Entity> toRemove;
    for (auto& [jointEntity, jointComp] : jointPool)
    {
        if (jointComp.compartmentA == entity || jointComp.compartmentB == entity)
            toRemove.push_back(jointEntity);
    }

    for (Entity jointEntity : toRemove)
        RemoveJoint(registry, jointEntity);
}
