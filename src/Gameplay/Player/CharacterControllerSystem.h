#pragma once
#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <unordered_set>
#include <ECSRegistry.h>
#include "CharacterExternalVelocitySystem.h"
#include "CharacterPhysicsTuningComponent.h"
#include "PhysicsWorld.h"

class CharacterControllerSystem
{
public:
    static void Update(ECSRegistry& registry, float dt)
    {
        std::unordered_set<Entity> aliveCharacters;
        registry.ForEach<CharacterController>(
            [&](Entity entity, CharacterController& controller)
            {
                aliveCharacters.insert(entity);
                auto* movement = registry.GetComponent<CharacterMovement>(entity);

                if (!registry.HasComponent<ActiveComponent>(entity))
                {
                    if (movement)
                    {
                        movement->walkDirection = { 0.0f, 0.0f, 0.0f };
                        movement->currentSpeed = 0.0f;
                        movement->isMoving = false;
                        movement->wantsRun = false;
                        movement->isRunning = false;
                        movement->wantJump = false;
                        movement->locomotionState = PlayerLocomotionState::Idle;
                    }
                    if (controller.controller)
                    {
                        controller.controller->setWalkDirection(
                            btVector3(0.0f, 0.0f, 0.0f));
                        controller.controller->setVelocityForTimeInterval(
                            btVector3(0.0f, 0.0f, 0.0f), btScalar(0.0f));
                    }
                    if (!controller.suspendedByActiveState)
                    {
                        if (auto* world = PhysicsWorld::Get().GetWorld())
                        {
                            if (controller.controller)
                                world->removeAction(controller.controller);
                            if (controller.ghostObj &&
                                controller.ghostObj->getBroadphaseHandle())
                            {
                                world->removeCollisionObject(controller.ghostObj);
                            }
                        }
                        controller.suspendedByActiveState = true;
                    }
                    return;
                }

                if (controller.suspendedByActiveState)
                {
                    if (auto* world = PhysicsWorld::Get().GetWorld())
                    {
                        if (controller.ghostObj &&
                            !controller.ghostObj->getBroadphaseHandle())
                        {
                            world->addCollisionObject(
                                controller.ghostObj,
                                btBroadphaseProxy::CharacterFilter,
                                btBroadphaseProxy::DefaultFilter |
                                btBroadphaseProxy::StaticFilter |
                                btBroadphaseProxy::KinematicFilter);
                        }
                        if (controller.controller)
                            world->addAction(controller.controller);
                    }
                    controller.suspendedByActiveState = false;
                }

                if (!movement || !controller.controller)
                    return;

                auto* transform = registry.GetComponent<TransformComponent>(entity);
                auto* tuning = registry.GetComponent<CharacterPhysicsTuningComponent>(entity);
                if (!tuning)
                {
                    // Old scenes do not contain the new tuning component. Add
                    // safe defaults lazily so Player and NPC remain compatible.
                    registry.AddComponent<CharacterPhysicsTuningComponent>(
                        entity, CharacterPhysicsTuningComponent{});
                    tuning = registry.GetComponent<CharacterPhysicsTuningComponent>(entity);
                }
                if (!tuning)
                    return;

                TraversalState& traversal = s_traversal[entity];
                ConfigureRuntimeController(controller, transform, *tuning, traversal);
                const bool stabilizeNavigationIntent =
                    !registry.HasComponent<PlayerGameTag>(entity);

                // Bullet's onGround() is derived mostly from the controller's
                // vertical velocity. At rest it can remain true after a floor
                // disappears, and on a touching/penetrating triangle it can
                // flicker false. Use our geometric support query as the single
                // source of truth for both Player and NPC.
                float groundSurfaceY = transform ? transform->position.y : 0.0f;
                const bool hasGroundSupport =
                    HasWalkableGroundSupport(controller, *tuning, &groundSurfaceY);
                traversal.hasGroundSupport = hasGroundSupport;
                traversal.groundSurfaceY = groundSurfaceY;
                controller.isGrounded = hasGroundSupport;
                movement->isGrounded = controller.isGrounded;

                const bool physicalStepSolved = UpdateAutomaticStep(
                    controller, *movement, transform, *tuning,
                    traversal, (std::max)(0.0f, dt),
                    stabilizeNavigationIntent);

                if (!physicalStepSolved && hasGroundSupport)
                {
                    PushDynamicBodyInMovementDirection(
                        controller, *movement, *tuning, dt);
                }

                constexpr float fixedPhysicsStep = 1.0f / 60.0f;
                const DirectX::XMFLOAT3 externalVelocity =
                    CharacterExternalVelocitySystem::ConsumeVelocity(entity);
                btVector3 walk(
                    (movement->walkDirection.x * movement->currentSpeed + externalVelocity.x) * fixedPhysicsStep,
                    (movement->walkDirection.y * movement->currentSpeed + externalVelocity.y) * fixedPhysicsStep,
                    (movement->walkDirection.z * movement->currentSpeed + externalVelocity.z) * fixedPhysicsStep
                );

                // The solver has already consumed part of this movement slice.
                // Apply only the remaining distance, but always retain a small
                // continuation impulse so the character never visibly stops at
                // the threshold. External forces are intentionally kept.
                if (physicalStepSolved)
                {
                    const float directionLength = std::sqrt(
                        movement->walkDirection.x * movement->walkDirection.x +
                        movement->walkDirection.z * movement->walkDirection.z);
                    const float desiredAdvance =
                        movement->currentSpeed * fixedPhysicsStep;
                    if (directionLength > 0.0001f && desiredAdvance > 0.0f)
                    {
                        const float minimumContinuation = desiredAdvance * 0.25f;
                        const float remainingAdvance = std::clamp(
                            desiredAdvance - traversal.solvedHorizontalAdvance,
                            minimumContinuation,
                            desiredAdvance);
                        const float inverseDirectionLength = 1.0f / directionLength;
                        walk.setX(
                            movement->walkDirection.x * inverseDirectionLength * remainingAdvance +
                            externalVelocity.x * fixedPhysicsStep);
                        walk.setZ(
                            movement->walkDirection.z * inverseDirectionLength * remainingAdvance +
                            externalVelocity.z * fixedPhysicsStep);
                    }
                }

                if (movement->wantJump && controller.controller->canJump())
                {
                    // Auto-step никогда не должен складываться с настоящим
                    // прыжком: перед jump возвращаем обычную высоту ступени.
                    CancelAutomaticStep(controller, *tuning, traversal, false);
                    controller.controller->jump();
                }

                const float externalSpeedSquared =
                    externalVelocity.x * externalVelocity.x +
                    externalVelocity.y * externalVelocity.y +
                    externalVelocity.z * externalVelocity.z;
                const bool hasMovementIntent = movement->isMoving &&
                    movement->currentSpeed > 0.01f &&
                    (movement->walkDirection.x * movement->walkDirection.x +
                        movement->walkDirection.z * movement->walkDirection.z) > 0.0001f;
                const bool shouldPhysicallySleepCharacter =
                    !hasMovementIntent &&
                    !movement->wantJump &&
                    externalSpeedSquared <= 0.000001f &&
                    hasGroundSupport;
                traversal.physicallySleeping = shouldPhysicallySleepCharacter;

                if (shouldPhysicallySleepCharacter)
                {
                    // setWalkDirection(0) does NOT idle Bullet: it leaves
                    // m_useWalkDirection=true, so playerStep still executes
                    // gravity, stepUp, stepDown and penetration recovery every
                    // fixed tick. This affects both the player and NPCs. Zero
                    // velocity mode takes Bullet's explicit no-motion early
                    // return. The independent support sweep above resumes
                    // normal gravity if the floor disappears.
                    // Keep the physical shell out of penetration while keeping
                    // the visual foot point on the surface. Merely increasing
                    // capsuleCenterOffsetY moves the shell relative to the mesh,
                    // but gravity immediately drops it back into contact. Snap
                    // the center to floor + half capsule + the SAME clearance
                    // that PhysicsSystem subtracts when producing Transform.
                    // Thus the capsule floats by the configured skin while the
                    // model feet remain exactly on the detected surface.
                    if (controller.ghostObj && controller.capsuleShape)
                    {
                        const float halfCapsule =
                            controller.capsuleShape->getHalfHeight() +
                            controller.capsuleShape->getRadius();
                        const float contactGap = std::clamp(
                            tuning->capsuleFootClearance, 0.002f, 0.05f);
                        btTransform stable =
                            controller.ghostObj->getWorldTransform();
                        btVector3 stableCenter = stable.getOrigin();
                        stableCenter.setY(
                            groundSurfaceY + halfCapsule + contactGap);
                        stable.setOrigin(stableCenter);
                        controller.controller->warp(stableCenter);
                        controller.ghostObj->setInterpolationWorldTransform(stable);
                    }

                    controller.controller->setVelocityForTimeInterval(
                        btVector3(0.0f, 0.0f, 0.0f), btScalar(0.0f));
                }
                else
                {
                    controller.controller->setWalkDirection(walk);
                }

                movement->wantJump = false;
            });

        // Entity может быть удалена или потерять CharacterController в Editor.
        // Удаляем её маленькое runtime-состояние, чтобы карта не росла вечно.
        for (auto it = s_traversal.begin(); it != s_traversal.end();)
        {
            if (aliveCharacters.find(it->first) == aliveCharacters.end())
                it = s_traversal.erase(it);
            else
                ++it;
        }
    }

    static bool IsAutomaticStepActive(Entity entity)
    {
        const auto it = s_traversal.find(entity);
        return it != s_traversal.end() && it->second.active;
    }

    static bool HasGroundSupport(Entity entity)
    {
        const auto it = s_traversal.find(entity);
        return it != s_traversal.end() && it->second.hasGroundSupport;
    }

    static bool IsIdlePhysicsLocked(Entity entity)
    {
        const auto it = s_traversal.find(entity);
        return it != s_traversal.end() && it->second.physicallySleeping;
    }

    // The inspector calls this after editing settings. The next Update treats
    // the existing Bullet object as freshly configured without destroying it.
    static void ForceReconfigure(Entity entity)
    {
        s_traversal.erase(entity);
    }

private:
    struct DynamicBodySweepCallback final
        : btCollisionWorld::ClosestConvexResultCallback
    {
        DynamicBodySweepCallback(
            const btVector3& from,
            const btVector3& to,
            const btCollisionObject* ignoredObject)
            : btCollisionWorld::ClosestConvexResultCallback(from, to)
            , ignored(ignoredObject)
        {
            m_collisionFilterGroup = btBroadphaseProxy::CharacterFilter;
            m_collisionFilterMask = btBroadphaseProxy::DefaultFilter;
        }

        bool needsCollision(btBroadphaseProxy* proxy) const override
        {
            if (!btCollisionWorld::ClosestConvexResultCallback::needsCollision(proxy))
                return false;

            const auto* object =
                static_cast<const btCollisionObject*>(proxy->m_clientObject);
            if (!object || object == ignored || !object->hasContactResponse())
                return false;

            const btRigidBody* body = btRigidBody::upcast(object);
            return body && !body->isStaticOrKinematicObject() &&
                body->getInvMass() > btScalar(0.0f);
        }

        const btCollisionObject* ignored = nullptr;
    };

    static void PushDynamicBodyInMovementDirection(
        CharacterController& component,
        const CharacterMovement& movement,
        const CharacterPhysicsTuningComponent& tuning,
        float dt)
    {
        if (!tuning.pushDynamicBodies || !component.ghostObj ||
            !component.capsuleShape || !movement.isMoving ||
            movement.currentSpeed <= 0.01f || dt <= 0.0f)
        {
            return;
        }

        auto* world = PhysicsWorld::Get().GetWorld();
        if (!world)
            return;

        btVector3 direction(
            movement.walkDirection.x,
            0.0f,
            movement.walkDirection.z);
        if (direction.length2() <= btScalar(0.0001f))
            return;
        direction.normalize();

        // Проверяем небольшое расстояние перед капсулой. Благодаря этому тело
        // получает импульс до заметного взаимного проникновения коллизий.
        const float frameTime = std::clamp(dt, 0.0f, 1.0f / 30.0f);
        const float probeDistance = std::clamp(
            movement.currentSpeed * frameTime +
                (std::max)(0.0f, tuning.dynamicBodyPushProbe),
            0.02f,
            0.20f);
        const btTransform from = component.ghostObj->getWorldTransform();
        btTransform to = from;
        to.setOrigin(from.getOrigin() + direction * probeDistance);

        DynamicBodySweepCallback callback(
            from.getOrigin(), to.getOrigin(), component.ghostObj);
        if (component.ghostObj->getBroadphaseHandle())
        {
            callback.m_collisionFilterGroup = component.ghostObj
                ->getBroadphaseHandle()->m_collisionFilterGroup;
            callback.m_collisionFilterMask = component.ghostObj
                ->getBroadphaseHandle()->m_collisionFilterMask;
        }

        world->convexSweepTest(
            component.capsuleShape,
            from,
            to,
            callback,
            btScalar(0.0f));
        if (!callback.hasHit() || !callback.m_hitCollisionObject)
            return;

        btRigidBody* body = btRigidBody::upcast(
            const_cast<btCollisionObject*>(callback.m_hitCollisionObject));
        if (!body || body->getInvMass() <= btScalar(0.0f))
            return;

        const float inverseBodyMass = static_cast<float>(body->getInvMass());
        const float bodyMass = 1.0f / inverseBodyMass;
        const float maxPushableMass =
            (std::max)(0.0f, tuning.dynamicBodyMaxPushableMass);
        if (!std::isfinite(bodyMass) || maxPushableMass <= 0.0f ||
            bodyMass > maxPushableMass)
        {
            return;
        }

        btVector3 surfaceNormal = callback.m_hitNormalWorld;
        surfaceNormal.setY(0.0f);
        if (surfaceNormal.length2() <= btScalar(0.0001f))
            return;
        surfaceNormal.normalize();

        // Толкаем только поверхность перед персонажем. Пол, наклонная верхняя
        // грань и предмет, от которого персонаж уже отходит, сюда не попадут.
        const float approach = std::clamp(
            static_cast<float>(-direction.dot(surfaceNormal)),
            0.0f,
            1.0f);
        if (approach <= 0.05f)
            return;

        const float bodySpeed = static_cast<float>(
            body->getLinearVelocity().dot(direction));
        // A constant force is integrated as force * dt. Reapplying it every
        // frame is correct, but without a target-speed cap its impulses would
        // accumulate forever. Stop adding impulse as soon as the body reaches
        // the configured push speed.
        const float targetPushSpeed = (std::min)(
            movement.currentSpeed,
            (std::max)(0.0f, tuning.dynamicBodyMaxPushSpeed));
        if (targetPushSpeed <= 0.0f)
            return;
        const float remainingSpeed = targetPushSpeed - bodySpeed;
        if (remainingSpeed <= 0.0f)
            return;

        const float speedFactor = std::clamp(
            remainingSpeed / (std::max)(targetPushSpeed, 0.01f),
            0.0f,
            1.0f);
        float impulseMagnitude =
            (std::max)(0.0f, tuning.dynamicBodyPushForce) *
            frameTime * approach * speedFactor;

        // Ограничение не позволяет лёгкому предмету сразу разогнаться быстрее
        // персонажа. Массу здесь не умножаем: Bullet сам применит inverse mass.
        const float impulseForDesiredSpeed = remainingSpeed / inverseBodyMass;
        impulseMagnitude = (std::min)(
            impulseMagnitude,
            impulseForDesiredSpeed);
        if (impulseMagnitude <= 0.0f)
            return;

        const btVector3 relativeHitPoint =
            callback.m_hitPointWorld - body->getCenterOfMassPosition();
        body->activate(true);
        body->applyImpulse(
            direction * impulseMagnitude,
            relativeHitPoint);
    }

    struct TraversalState
    {
        btKinematicCharacterController* runtimeController = nullptr;
        DirectX::XMFLOAT3 lastProgressPosition{};
        DirectX::XMFLOAT3 lastIntentDirection{};
        DirectX::XMFLOAT3 stableProbeDirection{};
        bool progressInitialized = false;
        bool stableProbeInitialized = false;
        bool active = false;
        bool hasGroundSupport = false;
        bool physicallySleeping = false;
        float groundSurfaceY = 0.0f;
        float blockedTimer = 0.0f;
        float activeTimer = 0.0f;
        float cooldown = 0.0f;
        float solvedHorizontalAdvance = 0.0f;
    };

    inline static std::unordered_map<Entity, TraversalState> s_traversal;

    static void ConfigureRuntimeController(
        CharacterController& component,
        const TransformComponent* transform,
        const CharacterPhysicsTuningComponent& tuning,
        TraversalState& traversal)
    {
        if (!component.controller)
            return;

        if (traversal.runtimeController != component.controller)
        {
            traversal = TraversalState{};
            traversal.runtimeController = component.controller;

            // Never depend on the order in which setGravity and the Bullet
            // constructor happen to select the up axis.
            if (tuning.forceYAxis)
                component.controller->setUp(btVector3(0.0f, 1.0f, 0.0f));

            if (tuning.migrateLegacyStepHeight &&
                std::abs(component.stepHeight - tuning.legacyStepHeight) <= 0.001f)
            {
                component.stepHeight = (std::max)(
                    0.0f, tuning.legacyReplacementStepHeight);
            }
        }

        // These assignments are intentionally repeated. Inspector edits become
        // visible on the next frame and do not require restarting Play mode.
        component.controller->setGravity(btVector3(0.0f, -(std::max)(0.0f, component.gravity), 0.0f));
        component.controller->setJumpSpeed((std::max)(0.0f, component.jumpSpeed));
        component.controller->setMaxSlope(
            std::clamp(component.maxSlope, 0.0f, 89.9f) * 0.01745329252f);
        component.controller->setUseGhostSweepTest(tuning.useGhostSweepTest);
        component.controller->setMaxPenetrationDepth(
            std::clamp(tuning.maxPenetrationDepth, 0.001f, 0.50f));
        component.controller->setFallSpeed((std::max)(0.1f, tuning.fallSpeed));

        if (tuning.clampNormalStepHeight)
        {
            const float minimum = (std::max)(0.0f,
                (std::min)(tuning.minimumStepHeight, tuning.maximumStepHeight));
            const float maximum = (std::max)(minimum,
                (std::max)(tuning.minimumStepHeight, tuning.maximumStepHeight));
            component.stepHeight = std::clamp(component.stepHeight, minimum, maximum);
        }
        else
        {
            component.stepHeight = (std::max)(0.0f, component.stepHeight);
        }

        // Transform.position is the foot point. CharacterSpawnSystem creates
        // the capsule with its old automatic offset, then this common layer may
        // move only the Bullet center to the user's manual/clearance value.
        // Read the currently built Bullet shape. Editing size in the inspector
        // must not move the old shape as though it had already been rebuilt.
        const float halfCapsule = component.capsuleShape
            ? component.capsuleShape->getHalfHeight() + component.capsuleShape->getRadius()
            : component.capsuleHeight * 0.5f + component.capsuleRadius;
        const float desiredOffset = tuning.autoCalculateCapsuleCenterOffset
            ? halfCapsule + tuning.capsuleFootClearance
            : tuning.manualCapsuleCenterOffsetY;
        if (transform && component.ghostObj && std::isfinite(desiredOffset) &&
            std::abs(component.capsuleCenterOffsetY - desiredOffset) > 0.0001f)
        {
            component.capsuleCenterOffsetY = desiredOffset;
            const btVector3 center(
                transform->position.x,
                transform->position.y + desiredOffset,
                transform->position.z);
            component.controller->warp(center);
            component.ghostObj->setInterpolationWorldTransform(
                component.ghostObj->getWorldTransform());
            component.lastSyncedPosition = transform->position;
        }

        component.controller->setStepHeight(component.stepHeight);
    }

    static bool UpdateAutomaticStep(
        CharacterController& component,
        const CharacterMovement& movement,
        const TransformComponent* transform,
        const CharacterPhysicsTuningComponent& tuning,
        TraversalState& traversal,
        float dt,
        bool stabilizeNavigationIntent)
    {
        if (!component.controller || !transform)
            return false;

        traversal.solvedHorizontalAdvance = 0.0f;

        traversal.cooldown = (std::max)(0.0f, traversal.cooldown - dt);
        if (traversal.active)
        {
            traversal.activeTimer -= dt;
            if (traversal.activeTimer <= 0.0f)
                CancelAutomaticStep(
                    component, tuning, traversal, !tuning.automaticStepPredictive);
        }

        const float inputLengthSquared =
            movement.walkDirection.x * movement.walkDirection.x +
            movement.walkDirection.z * movement.walkDirection.z;
        const bool wantsHorizontalMovement =
            movement.isMoving && movement.currentSpeed > 0.01f && inputLengthSquared > 0.01f;

        // onGround() may flicker for one physics slice on triangle seams or
        // while the capsule is pressed into the corner of a low ledge. A short
        // walkable-floor sweep is a conservative fallback: it is far too short
        // to catch a genuinely airborne character, but keeps the step solver
        // available in exactly the frame in which Bullet can otherwise strand
        // the character against a curb.
        const bool hasGroundSupport = component.controller->onGround() ||
            HasWalkableGroundSupport(component, tuning);

        // В воздухе и во время прыжка assist запрещён. Это главное отличие от
        // постоянного stepHeight=1.25: система не может подтолкнуть капсулу
        // вверх вдоль стены или помочь выбраться на крышу корпуса.
        if (!wantsHorizontalMovement || movement.wantJump ||
            (!hasGroundSupport && !traversal.active))
        {
            CancelAutomaticStep(component, tuning, traversal, false);
            traversal.blockedTimer = 0.0f;
            traversal.lastProgressPosition = transform->position;
            traversal.progressInitialized = true;
            traversal.stableProbeInitialized = false;
            return false;
        }

        if (!tuning.automaticStepEnabled)
        {
            CancelAutomaticStep(component, tuning, traversal, false);
            traversal.blockedTimer = 0.0f;
            return false;
        }

        // Normal gameplay uses predictive stepping. A cheap forward sweep runs
        // before Bullet reaches the vertical face; only when that sweep finds a
        // close obstacle do the up/forward/down validation sweeps run. There is
        // therefore no deliberate stop at the ledge and player/NPC timing no
        // longer depends on walking speed.
        if (tuning.automaticStepPredictive)
        {
            const float inverseInputLength = 1.0f / std::sqrt(inputLengthSquared);
            const DirectX::XMFLOAT3 intentDirection{
                movement.walkDirection.x * inverseInputLength,
                0.0f,
                movement.walkDirection.z * inverseInputLength };
            CharacterMovement probeMovement = movement;

            if (stabilizeNavigationIntent)
            {
                if (!traversal.stableProbeInitialized)
                {
                    traversal.stableProbeDirection = intentDirection;
                    traversal.stableProbeInitialized = true;
                }
                else
                {
                    const float probeDot =
                        traversal.stableProbeDirection.x * intentDirection.x +
                        traversal.stableProbeDirection.z * intentDirection.z;

                    // A large deliberate turn starts a new probe immediately.
                    // Smaller NavMesh waypoint corrections are blended so the
                    // capsule keeps testing one coherent direction at the curb
                    // instead of alternating around a triangle/portal corner.
                    if (probeDot < 0.35f)
                    {
                        traversal.stableProbeDirection = intentDirection;
                    }
                    else
                    {
                        const float blend = std::clamp(dt * 10.0f, 0.12f, 1.0f);
                        float blendedX = traversal.stableProbeDirection.x +
                            (intentDirection.x - traversal.stableProbeDirection.x) * blend;
                        float blendedZ = traversal.stableProbeDirection.z +
                            (intentDirection.z - traversal.stableProbeDirection.z) * blend;
                        const float blendedLength = std::sqrt(
                            blendedX * blendedX + blendedZ * blendedZ);
                        if (blendedLength > 0.0001f)
                        {
                            blendedX /= blendedLength;
                            blendedZ /= blendedLength;
                            traversal.stableProbeDirection = {
                                blendedX, 0.0f, blendedZ };
                        }
                    }
                }

                probeMovement.walkDirection = traversal.stableProbeDirection;
            }
            else
            {
                traversal.stableProbeDirection = intentDirection;
                traversal.stableProbeInitialized = true;
            }

            if (!traversal.progressInitialized)
            {
                traversal.lastProgressPosition = transform->position;
                traversal.lastIntentDirection = intentDirection;
                traversal.progressInitialized = true;
                traversal.blockedTimer = 0.0f;
            }
            else
            {
                const float intentDot =
                    traversal.lastIntentDirection.x * intentDirection.x +
                    traversal.lastIntentDirection.z * intentDirection.z;
                const float dx = transform->position.x - traversal.lastProgressPosition.x;
                const float dz = transform->position.z - traversal.lastProgressPosition.z;
                const float progressAlongIntent =
                    dx * intentDirection.x + dz * intentDirection.z;
                const float progressDistance = (std::max)(
                    0.001f, tuning.automaticStepProgressDistance);

                // Changing direction invalidates the old progress anchor. Only
                // movement along the current intent counts as progress: sliding
                // sideways along a curb must not hide the fact that the desired
                // route is still blocked.
                const bool intentWasReplaced = stabilizeNavigationIntent
                    ? intentDot < -0.10f
                    : intentDot < 0.80f;
                if (intentWasReplaced || progressAlongIntent >= progressDistance)
                {
                    traversal.lastProgressPosition = transform->position;
                    traversal.lastIntentDirection = intentDirection;
                    traversal.blockedTimer = 0.0f;
                }
                else
                {
                    traversal.blockedTimer += dt;
                }
            }

            if (traversal.cooldown <= 0.0f &&
                TrySolvePhysicalStep(
                    component, probeMovement, tuning, true, hasGroundSupport,
                    dt, traversal.solvedHorizontalAdvance))
            {
                traversal.active = true;
                traversal.activeTimer = (std::max)(0.01f, tuning.automaticStepDuration);
                // One short guard is enough to prevent the same edge from being
                // solved twice, while remaining invisible on a staircase.
                traversal.cooldown = (std::min)(0.02f,
                    (std::max)(0.0f, tuning.automaticStepCooldown));
                traversal.blockedTimer = 0.0f;
                traversal.lastProgressPosition = transform->position;
                traversal.lastIntentDirection = intentDirection;
                traversal.progressInitialized = true;
                return true;
            }

            // Predictive casts intentionally activate only immediately before
            // contact. If a mesh seam, a very thin curb or an initial overlap
            // makes that one cast inconclusive, retry without the activation
            // distance once measured progress proves that we are really stuck.
            const float progressDistance = (std::max)(
                0.001f, tuning.automaticStepProgressDistance);
            const float expectedProgressTime = progressDistance /
                (std::max)(0.01f, movement.currentSpeed);
            const float fallbackDelay = (std::max)(
                (std::max)(0.03f, tuning.automaticStepDelay),
                expectedProgressTime * 1.5f + (1.0f / 60.0f));
            if (traversal.blockedTimer >= fallbackDelay &&
                traversal.cooldown <= 0.0f && hasGroundSupport)
            {
                if (TrySolvePhysicalStep(
                    component, probeMovement, tuning, false, hasGroundSupport,
                    dt, traversal.solvedHorizontalAdvance))
                {
                    traversal.active = true;
                    traversal.activeTimer = (std::max)(
                        0.01f, tuning.automaticStepDuration);
                    traversal.cooldown = (std::min)(0.02f,
                        (std::max)(0.0f, tuning.automaticStepCooldown));
                    traversal.blockedTimer = 0.0f;
                    traversal.lastProgressPosition = transform->position;
                    traversal.lastIntentDirection = intentDirection;
                    return true;
                }

                // A real wall is retried at a modest rate. Keep the accumulated
                // blocked state so a single failed seam cast cannot restart the
                // whole delay and leave the character permanently wedged.
                traversal.cooldown = (std::max)(
                    0.03f, (std::min)(0.06f, tuning.automaticStepCooldown));
            }
            return false;
        }

        if (!traversal.progressInitialized)
        {
            traversal.lastProgressPosition = transform->position;
            traversal.progressInitialized = true;
            return false;
        }

        const float dx = transform->position.x - traversal.lastProgressPosition.x;
        const float dz = transform->position.z - traversal.lastProgressPosition.z;
        const float progressDistance = (std::max)(
            0.001f, tuning.automaticStepProgressDistance);
        if (dx * dx + dz * dz >= progressDistance * progressDistance)
        {
            traversal.lastProgressPosition = transform->position;
            traversal.blockedTimer = 0.0f;
            if (traversal.active)
                CancelAutomaticStep(component, tuning, traversal, true);
            return false;
        }

        traversal.blockedTimer += dt;
        if (traversal.blockedTimer < (std::max)(0.0f, tuning.automaticStepDelay) ||
            traversal.active || traversal.cooldown > 0.0f ||
            !component.controller->onGround())
        {
            return false;
        }

        if (TrySolvePhysicalStep(
            component, movement, tuning, false, hasGroundSupport,
            dt, traversal.solvedHorizontalAdvance))
        {
            traversal.active = true;
            traversal.activeTimer = (std::max)(0.01f, tuning.automaticStepDuration);
            traversal.blockedTimer = 0.0f;
            return true;
        }
        else
        {
            // Failed probes are cheap, but there is no reason to repeat all
            // three sweeps every frame while the character faces a real wall.
            traversal.cooldown = (std::max)(0.03f, tuning.automaticStepCooldown);
            traversal.blockedTimer = 0.0f;
        }
        return false;
    }

    // ClosestConvexResultCallback normally returns the first hit of any kind.
    // For stepping that is not enough: a horizontal sweep must ignore the floor,
    // while the final downward sweep must ignore wall edges and accept only a
    // walkable upward normal. This callback filters by the world-space Y normal
    // and always ignores the character's own ghost object.
    struct CharacterSweepCallback final : btCollisionWorld::ClosestConvexResultCallback
    {
        CharacterSweepCallback(
            const btVector3& from,
            const btVector3& to,
            const btCollisionObject* ignoredObject,
            float minimumUpDot,
            float maximumUpDot)
            : btCollisionWorld::ClosestConvexResultCallback(from, to)
            , ignored(ignoredObject)
            , minUpDot(minimumUpDot)
            , maxUpDot(maximumUpDot)
        {
            m_collisionFilterGroup = btBroadphaseProxy::CharacterFilter;
            m_collisionFilterMask =
                btBroadphaseProxy::DefaultFilter |
                btBroadphaseProxy::StaticFilter |
                btBroadphaseProxy::KinematicFilter;
        }

        bool needsCollision(btBroadphaseProxy* proxy) const override
        {
            if (!btCollisionWorld::ClosestConvexResultCallback::needsCollision(proxy))
                return false;

            const auto* object = static_cast<const btCollisionObject*>(proxy->m_clientObject);
            return object && object != ignored && object->hasContactResponse();
        }

        btScalar addSingleResult(
            btCollisionWorld::LocalConvexResult& result,
            bool normalInWorldSpace) override
        {
            btVector3 normal = result.m_hitNormalLocal;
            if (!normalInWorldSpace && result.m_hitCollisionObject)
            {
                normal = result.m_hitCollisionObject->getWorldTransform().getBasis() * normal;
            }
            normal.normalize();
            const float upDot = normal.dot(btVector3(0.0f, 1.0f, 0.0f));
            if (upDot < minUpDot || upDot > maxUpDot)
                return btScalar(1.0f);

            return btCollisionWorld::ClosestConvexResultCallback::addSingleResult(
                result, normalInWorldSpace);
        }

        const btCollisionObject* ignored = nullptr;
        float minUpDot = -1.0f;
        float maxUpDot = 1.0f;
    };

    // A downward capsule sweep can miss when the capsule already touches or
    // slightly overlaps a triangle: the cast starts in contact and Bullet has
    // no positive hit fraction to report. A ray from the capsule center to just
    // below its foot does not have that ambiguity. It is used only as a support
    // fallback and accepts upward, walkable surfaces.
    struct CharacterGroundRayCallback final : btCollisionWorld::ClosestRayResultCallback
    {
        CharacterGroundRayCallback(
            const btVector3& from,
            const btVector3& to,
            const btCollisionObject* ignoredObject,
            float minimumUpDot)
            : btCollisionWorld::ClosestRayResultCallback(from, to)
            , ignored(ignoredObject)
            , minUpDot(minimumUpDot)
        {
            m_collisionFilterGroup = btBroadphaseProxy::CharacterFilter;
            m_collisionFilterMask =
                btBroadphaseProxy::DefaultFilter |
                btBroadphaseProxy::StaticFilter |
                btBroadphaseProxy::KinematicFilter;
        }

        bool needsCollision(btBroadphaseProxy* proxy) const override
        {
            if (!btCollisionWorld::ClosestRayResultCallback::needsCollision(proxy))
                return false;

            const auto* object =
                static_cast<const btCollisionObject*>(proxy->m_clientObject);
            return object && object != ignored && object->hasContactResponse();
        }

        btScalar addSingleResult(
            btCollisionWorld::LocalRayResult& result,
            bool normalInWorldSpace) override
        {
            btVector3 normal = result.m_hitNormalLocal;
            if (!normalInWorldSpace && result.m_collisionObject)
            {
                normal = result.m_collisionObject->getWorldTransform().getBasis() * normal;
            }
            normal.normalize();
            if (normal.dot(btVector3(0.0f, 1.0f, 0.0f)) < minUpDot)
                return btScalar(1.0f);

            return btCollisionWorld::ClosestRayResultCallback::addSingleResult(
                result, normalInWorldSpace);
        }

        const btCollisionObject* ignored = nullptr;
        float minUpDot = 0.0f;
    };

    static bool SweepCapsule(
        btCollisionWorld& world,
        btConvexShape& shape,
        const btTransform& from,
        const btTransform& to,
        const btCollisionObject* ignored,
        float minimumUpDot,
        float maximumUpDot,
        float allowedPenetration,
        btScalar& hitFraction,
        btVector3& hitNormal)
    {
        CharacterSweepCallback callback(
            from.getOrigin(), to.getOrigin(), ignored,
            minimumUpDot, maximumUpDot);
        if (ignored && ignored->getBroadphaseHandle())
        {
            // Match the actual character ghost's filters. The callback defaults
            // to Default/All; that made NPC auto-step sweeps see the followed
            // player's CharacterFilter ghost as a curb or landing surface even
            // though normal character movement excludes character collisions.
            callback.m_collisionFilterGroup =
                ignored->getBroadphaseHandle()->m_collisionFilterGroup;
            callback.m_collisionFilterMask =
                ignored->getBroadphaseHandle()->m_collisionFilterMask;
        }
        world.convexSweepTest(
            &shape,
            from,
            to,
            callback,
            (std::max)(0.0f, allowedPenetration));

        if (!callback.hasHit())
            return false;

        hitFraction = callback.m_closestHitFraction;
        hitNormal = callback.m_hitNormalWorld;
        return true;
    }

    static bool HasWalkableGroundSupport(
        CharacterController& component,
        const CharacterPhysicsTuningComponent& tuning,
        float* outGroundSurfaceY = nullptr)
    {
        auto* world = PhysicsWorld::Get().GetWorld();
        if (!world || !component.ghostObj || !component.capsuleShape)
            return false;

        const float maxSlopeRadians =
            std::clamp(component.maxSlope, 0.0f, 89.0f) * 0.01745329252f;
        const float minimumWalkableUpDot = std::cos(maxSlopeRadians);
        const float allowedPenetration = std::clamp(
            tuning.maxPenetrationDepth, 0.001f, 0.10f);
        const float probeDistance = std::clamp(
            (std::max)(tuning.capsuleFootClearance +
                tuning.automaticStepSkin * 2.0f,
                tuning.maxPenetrationDepth),
            0.02f, 0.06f);

        const btTransform current = component.ghostObj->getWorldTransform();
        const float capsuleBottomDistance =
            component.capsuleShape->getHalfHeight() +
            component.capsuleShape->getRadius();

        // Prefer the center-to-foot ray. Unlike a convex sweep it does not
        // begin with the capsule surface already touching/overlapping the deck.
        // The ray is vertical and only extends a few centimetres past the foot,
        // so any responding object it reaches is valid geometric support even
        // when imported triangle winding gives the contact a flipped normal.
        const btVector3 rayFrom = current.getOrigin();
        const btVector3 rayTo = rayFrom + btVector3(
            0.0f, -(capsuleBottomDistance + probeDistance), 0.0f);
        CharacterGroundRayCallback ray(
            rayFrom, rayTo, component.ghostObj, -1.0f);
        if (component.ghostObj->getBroadphaseHandle())
        {
            ray.m_collisionFilterGroup = component.ghostObj
                ->getBroadphaseHandle()->m_collisionFilterGroup;
            ray.m_collisionFilterMask = component.ghostObj
                ->getBroadphaseHandle()->m_collisionFilterMask;
        }
        world->rayTest(rayFrom, rayTo, ray);
        if (ray.hasHit())
        {
            if (outGroundSurfaceY)
                *outGroundSurfaceY = static_cast<float>(ray.m_hitPointWorld.y());
            return true;
        }

        btTransform below = current;
        below.setOrigin(current.getOrigin() + btVector3(0.0f, -probeDistance, 0.0f));

        btScalar fraction = btScalar(1.0f);
        btVector3 normal(0.0f, 1.0f, 0.0f);
        if (SweepCapsule(
            *world, *component.capsuleShape, current, below,
            component.ghostObj, minimumWalkableUpDot, 1.0f,
            allowedPenetration, fraction, normal))
        {
            if (outGroundSurfaceY)
            {
                const float centerAtHitY = static_cast<float>(
                    current.getOrigin().y() +
                    (below.getOrigin().y() - current.getOrigin().y()) * fraction);
                *outGroundSurfaceY = centerAtHitY - capsuleBottomDistance;
            }
            return true;
        }
        return false;
    }

    // Reliable low-ledge traversal shared by Player and NPC.
    //
    // 1) A horizontal capsule sweep proves that we are actually facing an
    //    obstacle. This prevents a spontaneous lift on ordinary floors.
    // 2) An upward sweep looks only for a ceiling, proving headroom.
    // 3) A forward sweep at the raised position proves the capsule clears the
    //    obstacle instead of being teleported through a wall.
    // 4) A downward sweep accepts only a walkable floor and calculates the exact
    //    landing height. The warp is rejected if the rise is outside the small
    //    configured step range.
    static bool TrySolvePhysicalStep(
        CharacterController& component,
        const CharacterMovement& movement,
        const CharacterPhysicsTuningComponent& tuning,
        bool predictive,
        bool hasGroundSupport,
        float frameDt,
        float& solvedHorizontalAdvance)
    {
        auto* world = PhysicsWorld::Get().GetWorld();
        if (!world || !component.ghostObj || !component.capsuleShape ||
            !component.controller || !hasGroundSupport)
        {
            return false;
        }

        btVector3 forward(movement.walkDirection.x, 0.0f, movement.walkDirection.z);
        if (forward.length2() <= btScalar(0.0001f))
            return false;
        forward.normalize();

        // The project only promises comfortable traversal of 10-15 cm ledges.
        // Capping at 35 cm prevents this helper from becoming wall climbing even
        // if an experimental scene still contains the old 0.50 setting.
        const float maxStepHeight = std::clamp(
            tuning.automaticStepHeight, 0.05f, 0.35f);
        const float forwardProbe = std::clamp(
            tuning.automaticStepForwardProbe, 0.04f, 0.30f);
        const float downProbe = std::clamp(
            tuning.automaticStepDownProbe, 0.02f, 0.25f);
        const float skin = std::clamp(tuning.automaticStepSkin, 0.005f, 0.05f);
        const float allowedPenetration = std::clamp(
            tuning.maxPenetrationDepth, 0.001f, 0.10f);

        // setWalkDirection in this project is expressed as displacement for one
        // 60 Hz physics slice. The animation speed may change how soon we reach
        // the ledge, but it must not decide whether the capsule can clear it.
        // Keep a small geometry-derived minimum look-ahead so walking and running
        // use the same traversal envelope.
        constexpr float fixedPhysicsStep = 1.0f / 60.0f;
        const float plannedAdvance = std::clamp(
            movement.currentSpeed * fixedPhysicsStep,
            skin * 2.0f,
            forwardProbe);
        const float capsuleRadius = component.capsuleShape->getRadius();
        const float minimumLookAhead = std::clamp(
            capsuleRadius * 0.15f,
            skin * 2.0f,
            forwardProbe);
        const float predictionHorizon = std::clamp(
            frameDt, fixedPhysicsStep, fixedPhysicsStep * 3.0f);
        const float activationDistance = (std::min)(
            forwardProbe,
            (std::max)(movement.currentSpeed * predictionHorizon,
                minimumLookAhead) + skin * 2.0f);

        btTransform current = component.ghostObj->getWorldTransform();
        const btVector3 currentCenter = current.getOrigin();
        const btVector3 up(0.0f, 1.0f, 0.0f);

        btScalar fraction = btScalar(1.0f);
        btVector3 normal(0.0f, 1.0f, 0.0f);

        // Ignore floor-like normals: this query is specifically looking for the
        // vertical face/edge that stopped horizontal movement.
        btTransform horizontalEnd = current;
        horizontalEnd.setOrigin(currentCenter + forward * forwardProbe);
        if (!SweepCapsule(
            *world, *component.capsuleShape, current, horizontalEnd,
            component.ghostObj, -1.0f, 0.50f,
            allowedPenetration, fraction, normal))
        {
            return false;
        }

        const float obstacleDistance = forwardProbe * static_cast<float>(fraction);
        if (predictive && obstacleDistance > activationDistance)
            return false;

        // NavMesh and analogue input often approach a threshold diagonally. The
        // hit normal tells us which part of the desired motion actually crosses
        // the ledge; the remainder is harmless motion along its edge.
        btVector3 obstacleNormal(normal.x(), 0.0f, normal.z());
        if (obstacleNormal.length2() <= btScalar(0.0001f))
            return false;
        obstacleNormal.normalize();
        const btVector3 intoObstacle = -obstacleNormal;
        const float approachDot = static_cast<float>(forward.dot(intoObstacle));
        if (approachDot <= 0.05f)
            return false;
        const btVector3 tangent = forward - intoObstacle * approachDot;

        // Headroom check. Only downward-facing normals are ceilings; touching a
        // wall beside the capsule must not make an upward sweep fail at t=0.
        btTransform raised = current;
        raised.setOrigin(currentCenter + up * (maxStepHeight + skin));
        if (SweepCapsule(
            *world, *component.capsuleShape, current, raised,
            component.ghostObj, -1.0f, -0.20f,
            allowedPenetration, fraction, normal))
        {
            return false;
        }

        const float maxSlopeRadians =
            std::clamp(component.maxSlope, 0.0f, 89.0f) * 0.01745329252f;
        const float minimumWalkableUpDot = std::cos(maxSlopeRadians);

        // A single fixed landing point is unreliable: it can overshoot a thin
        // curb, miss the top at a diagonal corner, or land exactly on a triangle
        // seam. Probe several monotonically increasing offsets and accept the
        // nearest valid one. The contact part follows the requested direction;
        // only the small post-contact part is biased through the hit normal.
        const btVector3 contactOffset = forward * obstacleDistance;

        // Clearing an edge is a geometric requirement, not a speed requirement.
        // Previously the furthest landing probe was based on plannedAdvance:
        // an NPC walking at 2 m/s only tested about 5.8 cm past the edge, while
        // running tested about 12.2 cm. That made Follow Player cross the exact
        // same threshold only after switching to run. Try close points first for
        // thin curbs, then always include capsule-sized and full-probe clearances.
        const float geometricClearance = std::clamp(
            capsuleRadius * 0.25f,
            skin * 3.0f,
            forwardProbe);
        const float crossingDistances[] = {
            skin * 1.25f,
            (std::max)(skin * 2.0f, plannedAdvance * 0.75f),
            (std::max)(skin * 3.0f, geometricClearance * 0.50f),
            geometricClearance,
            forwardProbe
        };

        const float tangentScales[] = { 1.0f, 0.0f, -0.50f };
        float previousCrossing = -1.0f;
        for (const float crossing : crossingDistances)
        {
            if (crossing <= previousCrossing + 0.001f)
                continue;
            previousCrossing = crossing;

            for (const float tangentScale : tangentScales)
            {
                const float tangentAdvance = (std::min)(
                    plannedAdvance * 0.50f, crossing) * tangentScale;
                const btVector3 landingOffset = contactOffset +
                    intoObstacle * crossing + tangent * tangentAdvance;

                btTransform raisedForward = raised;
                raisedForward.setOrigin(raised.getOrigin() + landingOffset);
                if (SweepCapsule(
                    *world, *component.capsuleShape, raised, raisedForward,
                    component.ghostObj, -1.0f, 0.50f,
                    allowedPenetration, fraction, normal))
                {
                    continue;
                }

                btTransform downEnd = raisedForward;
                downEnd.setOrigin(
                    currentCenter + landingOffset - up * downProbe);
                if (!SweepCapsule(
                    *world, *component.capsuleShape, raisedForward, downEnd,
                    component.ghostObj, minimumWalkableUpDot, 1.0f,
                    allowedPenetration, fraction, normal))
                {
                    continue;
                }

                const btVector3 downDelta =
                    downEnd.getOrigin() - raisedForward.getOrigin();
                const btVector3 landingCenter =
                    raisedForward.getOrigin() + downDelta * fraction + up * skin;
                const float rise = landingCenter.y() - currentCenter.y();
                if (rise < 0.015f || rise > maxStepHeight + skin * 2.0f)
                    continue;

                component.controller->warp(landingCenter);
                component.ghostObj->setInterpolationWorldTransform(
                    component.ghostObj->getWorldTransform());
                world->updateSingleAabb(component.ghostObj);
                solvedHorizontalAdvance = (std::max)(0.0f,
                    static_cast<float>(landingOffset.dot(forward)));
                return true;
            }
        }

        return false;
    }

    static void CancelAutomaticStep(
        CharacterController& component,
        const CharacterPhysicsTuningComponent& tuning,
        TraversalState& traversal,
        bool startCooldown)
    {
        traversal.active = false;
        traversal.activeTimer = 0.0f;
        if (startCooldown)
            traversal.cooldown = (std::max)(0.0f, tuning.automaticStepCooldown);
    }
};
