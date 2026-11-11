#pragma once

#include <algorithm>
#include <cmath>
#include <random>

#include "Animation/AnimationComponent.h"
#include "Components.h"
#include "ECSRegistry.h"
#include "Gameplay/Combat/DamageSystem.h"
#include "Gameplay/Combat/WeaponSystem.h"
#include "Gameplay/Player/GameplayInputSystem.h"
#include "Gameplay/Player/PlayerComponents.h"
#include "NPCComponents.h"
#include "Physics/PhysicsRaycast.h"

class PlayerPunchSystem
{
public:
    static void Update(ECSRegistry& registry, Entity playerEntity,
        const GameInputFrame& input, CameraClass* camera, btDynamicsWorld* world,
        DamageSystem& damageSystem, float deltaTime)
    {
        auto* player = registry.GetComponent<PlayerComponent>(playerEntity);
        auto* animation = registry.GetComponent<AnimationComponent>(playerEntity);
        auto* mesh = registry.GetComponent<MeshRendererComponent>(playerEntity);
        if (!player || !animation || !mesh || !mesh->model || !camera || !world)
            return;

        const bool hasWeapon =
            WeaponSystem::GetEquippedWeaponEntity(registry, playerEntity) != INVALID_ENTITY;
        if (hasWeapon)
        {
            // При оружии ПКМ + ЛКМ всегда означает выстрел, даже если
            // игрок подобрал винтовку посреди незавершённого удара.
            player->punchActive = false;
            player->meleeGuardActive = false;
            player->punchConsumedPrimary = false;
            return;
        }

        if (!input.Down(GameAction::PrimaryMouse))
            player->punchConsumedPrimary = false;

        player->meleeGuardActive = animation->meleeActions.guardEnabled &&
            input.Down(GameAction::Aim);

        if ((!animation->meleeActions.attackRequiresGuard ||
                player->meleeGuardActive) &&
            input.Pressed(GameAction::PrimaryMouse) && !player->punchActive)
        {
            struct Candidate { int index; int clip; float weight; };
            std::vector<Candidate> candidates;
            float totalWeight = 0.0f;
            for (int i = 0; i < static_cast<int>(animation->punchVariants.size()); ++i)
            {
                const auto& option = animation->punchVariants[i];
                if (!(option.weight > 0.0f) || !std::isfinite(option.weight)) continue;
                std::string name = option.worldClip;
                if (name.empty())
                    for (const auto& binding : animation->animations)
                        if (binding.state == AnimationState::Punch)
                        { name = binding.name; break; }
                const int index = mesh->model->FindAnimationIndexBySemanticName(name);
                if (!mesh->model->GetAnimation(index)) continue;
                candidates.push_back({ i, index, option.weight });
                totalWeight += option.weight;
            }
            if (!candidates.empty() && std::isfinite(totalWeight))
            {
                static thread_local std::mt19937 random{ std::random_device{}() };
                std::uniform_real_distribution<float> draw(0.0f, totalWeight);
                float roll = draw(random);
                const Candidate* chosen = &candidates.back();
                for (const auto& candidate : candidates)
                {
                    roll -= candidate.weight;
                    if (roll <= 0.0f) { chosen = &candidate; break; }
                }
                const auto* clip = mesh->model->GetAnimation(chosen->clip);
                player->punchVariant = chosen->index;
                const float rate = clip->ticksPerSecond > 0.0f
                    ? clip->ticksPerSecond : 24.0f;
                player->punchDuration = clip->duration /
                    (rate * animation->GetClipSpeed(clip->name));
                player->punchTime = 0.0f;
                player->punchHitApplied = false;
                player->punchLastHit = INVALID_ENTITY;
                player->punchActive = player->punchDuration > 0.01f;
                player->punchConsumedPrimary = player->punchActive;
                animation->poseVariant = chosen->index;
            }
        }

        if (!player->punchActive)
            return;

        const auto* controller = registry.GetComponent<CharacterController>(playerEntity);
        const btCollisionObject* playerBody = controller ? controller->ghostObj : nullptr;
        player->punchAimOrigin = camera->GetPosition();
        player->punchAimDirection = camera->GetForward();
        PhysicsRaycastHit cameraHit;
        PhysicsRaycast::CastRay(player->punchAimOrigin, player->punchAimDirection,
            world, 30.0f, playerEntity, playerBody, cameraHit);

        using namespace DirectX;
        const XMVECTOR origin = XMLoadFloat3(&player->punchAimOrigin);
        const XMVECTOR forward = XMLoadFloat3(&player->punchAimDirection);
        XMStoreFloat3(&player->punchAimPoint, cameraHit.collisionObject
            ? XMLoadFloat3(&cameraHit.position)
            : XMVectorAdd(origin, XMVectorScale(forward, 30.0f)));

        player->punchTime = (std::min)(player->punchTime + deltaTime,
            player->punchDuration);
        float contactPhase = player->punchContactPhase;
        if (player->punchVariant >= 0 &&
            player->punchVariant < static_cast<int>(animation->punchVariants.size()) &&
            animation->punchVariants[player->punchVariant].contactPhase >= 0.0f)
            contactPhase = animation->punchVariants[player->punchVariant].contactPhase;
        const float contactTime = player->punchDuration *
            std::clamp(contactPhase, 0.05f, 0.95f);

        if (!player->punchHitApplied && player->punchTime >= contactTime)
        {
            player->punchHitApplied = true;
            if (cameraHit.entity != INVALID_ENTITY && cameraHit.collisionObject)
            {
                const bool rightHand = player->punchVariant >= 0 &&
                    player->punchVariant < static_cast<int>(animation->punchVariants.size()) &&
                    animation->punchVariants[player->punchVariant].rightHand;
                // Луч камеры выбирает цель под прицелом. Второй луч от плеча
                // проверяет длину руки и стену между кулаком и целью.
                XMFLOAT3 shoulderPosition{};
                bool shoulderFound = false;
                if (const auto* transform = registry.GetComponent<TransformComponent>(playerEntity))
                {
                    XMMATRIX shoulderBone;
                    if (mesh->model->TryGetBoneModelTransform(*animation,
                        rightHand ? "mixamorig:RightArm" : "mixamorig:LeftArm",
                        shoulderBone))
                    {
                        XMStoreFloat3(&shoulderPosition, XMVector3TransformCoord(
                            shoulderBone.r[3], transform->GetWorldMatrix()));
                        shoulderFound = true;
                    }
                    else
                    {
                        shoulderPosition = transform->position;
                        shoulderPosition.y += 1.5f;
                        shoulderFound = true;
                    }
                }
                if (shoulderFound)
                {
                    const XMVECTOR toHit = XMVectorSubtract(
                        XMLoadFloat3(&cameraHit.position),
                        XMLoadFloat3(&shoulderPosition));
                    const float distance = XMVectorGetX(XMVector3Length(toHit));
                    const float fistToHit = XMVectorGetX(XMVector3Length(
                        XMVectorSubtract(XMLoadFloat3(&cameraHit.position),
                            XMLoadFloat3(&player->punchFistWorld))));
                    if (distance > 0.01f &&
                        distance <= (std::max)(0.1f, player->punchReach) &&
                        fistToHit <= (std::max)(0.01f, player->punchContactRadius))
                    {
                        XMFLOAT3 direction{};
                        XMStoreFloat3(&direction, XMVectorScale(toHit, 1.0f / distance));
                        PhysicsRaycastHit fistHit;
                        PhysicsRaycast::CastRay(shoulderPosition, direction,
                            world, distance + 0.05f, playerEntity, playerBody, fistHit);
                        if (fistHit.entity == cameraHit.entity)
                        {
                            DamageEvent damage;
                            damage.source = playerEntity;
                            damage.target = fistHit.entity;
                            damage.damageAmount = (std::max)(0.0f, player->punchDamage);
                            damage.damageType = DamageEvent::DamageType::Physical;
                            damage.hit_position = fistHit.position;
                            damage.hit_normal = fistHit.normal;
                            damageSystem.AddDamageEvent(damage);
                            player->punchLastHit = fistHit.entity;
                        }
                    }
                }
            }
        }

        if (player->punchTime >= player->punchDuration)
            player->punchActive = false;
    }

    static void ApplyIK(ECSRegistry& registry, CameraClass* previewCamera = nullptr,
        btDynamicsWorld* previewWorld = nullptr)
    {
        registry.ForEach<AnimationComponent, MeshRendererComponent>(
            [&](Entity entity, AnimationComponent& animation, MeshRendererComponent& mesh)
            {
                const auto* view = registry.GetComponent<FirstPersonViewComponent>(entity);
                const Entity owner = view ? view->owner : entity;
                auto* player = registry.GetComponent<PlayerComponent>(owner);
                const auto* npcCombat = view
                    ? nullptr : registry.GetComponent<NPCCombatComponent>(owner);
                auto* ownerAnimation = registry.GetComponent<AnimationComponent>(owner);
                const bool fullRuntimePreview = ownerAnimation &&
                    ownerAnimation->socketPreviewApplyPunchIK &&
                    !ownerAnimation->socketPreviewPunch;
                const bool preview = ownerAnimation &&
                    ownerAnimation->socketPreviewEnabled &&
                    (ownerAnimation->socketPreviewPunch || fullRuntimePreview) &&
                    previewCamera;
                const bool npcPunch = npcCombat &&
                    npcCombat->targetEntity != INVALID_ENTITY &&
                    registry.IsAlive(npcCombat->targetEntity) &&
                    animation.oneShotState == AnimationState::Punch &&
                    animation.oneShotTimeRemaining > 0.0f;
                const auto* transform = registry.GetComponent<TransformComponent>(entity);
                const int sampledPunchClip = animation.upperBodyAnimationIndex >= 0
                    ? animation.upperBodyAnimationIndex : animation.animationIndex;
                if ((!player && !npcPunch && !preview) ||
                    (player && !player->punchActive && !preview) ||
                    !transform || !mesh.model ||
                    !animation.isPlaying ||
                    sampledPunchClip < 0)
                    return;
                const int variant = preview
                    ? ownerAnimation->socketPreviewPunchVariant
                    : player ? player->punchVariant : animation.poseVariant;
                const PunchVariant* activeVariant = ownerAnimation &&
                    variant >= 0 &&
                    variant < static_cast<int>(ownerAnimation->punchVariants.size())
                    ? &ownerAnimation->punchVariants[variant] : nullptr;
                const bool rightHand = activeVariant && activeVariant->rightHand;
                const char* upperBone = rightHand
                    ? "mixamorig:RightArm" : "mixamorig:LeftArm";
                const char* forearmBone = rightHand
                    ? "mixamorig:RightForeArm" : "mixamorig:LeftForeArm";
                const char* handBone = rightHand
                    ? "mixamorig:RightHand" : "mixamorig:LeftHand";
                using namespace DirectX;
                XMMATRIX upper, lower, hand;
                if (!mesh.model->TryGetBoneModelTransform(animation,
                        upperBone, upper) ||
                    !mesh.model->TryGetBoneModelTransform(animation,
                        forearmBone, lower) ||
                    !mesh.model->TryGetBoneModelTransform(animation,
                        handBone, hand))
                    return;

                const XMMATRIX world = transform->GetWorldMatrix();
                const XMMATRIX inverseWorld = XMMatrixInverse(nullptr, world);
                const XMVECTOR shoulder = XMVector3TransformCoord(upper.r[3], world);
                const XMVECTOR elbow = XMVector3TransformCoord(lower.r[3], world);
                const XMVECTOR wrist = XMVector3TransformCoord(hand.r[3], world);
                const float armLength = XMVectorGetX(XMVector3Length(
                    XMVectorSubtract(elbow, shoulder))) +
                    XMVectorGetX(XMVector3Length(XMVectorSubtract(wrist, elbow)));
                if (armLength < 0.01f)
                    return;

                // Находим достижимую точку прямо на луче прицела. Это ведёт
                // кулак к прицелу, даже если исходный клип бьёт выше него.
                XMFLOAT3 aimOrigin{};
                XMFLOAT3 aimDirection{};
                XMFLOAT3 aimPoint{};
                if (preview)
                {
                    if (ownerAnimation->socketPreviewPunchUseCameraAim)
                    {
                        aimOrigin = previewCamera->GetPosition();
                        aimDirection = previewCamera->GetForward();
                        const XMVECTOR previewOrigin = XMLoadFloat3(&aimOrigin);
                        const XMVECTOR previewDirection = XMLoadFloat3(&aimDirection);
                        XMStoreFloat3(&aimPoint, XMVectorAdd(previewOrigin,
                            XMVectorScale(previewDirection, 30.0f)));
                        if (previewWorld)
                        {
                            PhysicsRaycastHit hit;
                            const auto* controller =
                                registry.GetComponent<CharacterController>(owner);
                            PhysicsRaycast::CastRay(aimOrigin, aimDirection,
                                previewWorld, 30.0f, owner,
                                controller ? controller->ghostObj : nullptr, hit);
                            if (hit.collisionObject) aimPoint = hit.position;
                        }
                    }
                    else
                    {
                        const XMVECTOR previewDirection = XMVector3Normalize(
                            XMVector3TransformNormal(
                                XMVectorSet(0.0f, 0.0f, -1.0f, 0.0f), world));
                        XMStoreFloat3(&aimOrigin, shoulder);
                        XMStoreFloat3(&aimDirection, previewDirection);
                        XMStoreFloat3(&aimPoint, XMVectorAdd(shoulder,
                            XMVectorScale(previewDirection,
                                std::clamp(
                                    ownerAnimation->socketPreviewPunchAimDistance,
                                    0.1f, 5.0f))));
                    }
                }
                else if (player)
                {
                    aimOrigin = player->punchAimOrigin;
                    aimDirection = player->punchAimDirection;
                    aimPoint = player->punchAimPoint;
                }
                else
                {
                    // У NPC роль прицела выполняет живая цель. Луч начинается
                    // в плече, поэтому поворот корневой кости не может увести
                    // удар в сторону от игрока.
                    const auto* targetTransform = registry.GetComponent<TransformComponent>(
                        npcCombat->targetEntity);
                    if (!targetTransform)
                        return;
                    XMStoreFloat3(&aimOrigin, shoulder);
                    aimPoint = targetTransform->position;
                    if (const auto* targetController =
                        registry.GetComponent<CharacterController>(npcCombat->targetEntity))
                        aimPoint.y += targetController->capsuleCenterOffsetY;
                    else
                        aimPoint.y += 1.1f;
                    const XMVECTOR npcAim = XMVectorSubtract(
                        XMLoadFloat3(&aimPoint), shoulder);
                    if (XMVectorGetX(XMVector3LengthSq(npcAim)) < 0.0001f)
                        return;
                    XMStoreFloat3(&aimDirection, XMVector3Normalize(npcAim));
                }
                if (activeVariant)
                {
                    XMVECTOR forward = XMVector3Normalize(XMLoadFloat3(&aimDirection));
                    XMVECTOR referenceUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
                    if (std::fabs(XMVectorGetX(XMVector3Dot(forward, referenceUp))) > 0.98f)
                        referenceUp = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
                    const XMVECTOR right = XMVector3Normalize(
                        XMVector3Cross(referenceUp, forward));
                    const XMVECTOR up = XMVector3Normalize(XMVector3Cross(forward, right));
                    XMVECTOR correctedAimPoint = XMLoadFloat3(&aimPoint);
                    correctedAimPoint = XMVectorAdd(correctedAimPoint,
                        XMVectorScale(right, activeVariant->aimOffset.x));
                    correctedAimPoint = XMVectorAdd(correctedAimPoint,
                        XMVectorScale(up, activeVariant->aimOffset.y));
                    correctedAimPoint = XMVectorAdd(correctedAimPoint,
                        XMVectorScale(forward, activeVariant->aimOffset.z));
                    XMStoreFloat3(&aimPoint, correctedAimPoint);
                    const XMVECTOR correctedDirection = XMVectorSubtract(
                        correctedAimPoint, XMLoadFloat3(&aimOrigin));
                    if (XMVectorGetX(XMVector3LengthSq(correctedDirection)) > 0.0001f)
                        XMStoreFloat3(&aimDirection,
                            XMVector3Normalize(correctedDirection));
                }
                const XMVECTOR rayOrigin = XMLoadFloat3(&aimOrigin);
                const XMVECTOR rayDirection = XMVector3Normalize(
                    XMLoadFloat3(&aimDirection));
                const float along = (std::max)(0.0f, XMVectorGetX(XMVector3Dot(
                    XMVectorSubtract(shoulder, rayOrigin), rayDirection)));
                const XMVECTOR closest = XMVectorAdd(rayOrigin,
                    XMVectorScale(rayDirection, along));
                const float lateralSq = XMVectorGetX(XMVector3LengthSq(
                    XMVectorSubtract(closest, shoulder)));
                const float reach = armLength * (activeVariant
                    ? std::clamp(activeVariant->armExtension, 0.5f, 0.99f)
                    : 0.97f);
                XMVECTOR contact = lateralSq < reach * reach
                    ? XMVectorAdd(closest, XMVectorScale(rayDirection,
                        std::sqrt(reach * reach - lateralSq)))
                    : XMVectorAdd(shoulder, XMVectorScale(XMVector3Normalize(
                        XMVectorSubtract(XMLoadFloat3(&aimPoint),
                            shoulder)), reach));
                const XMVECTOR aimPointVector = XMLoadFloat3(&aimPoint);
                if (XMVectorGetX(XMVector3LengthSq(XMVectorSubtract(aimPointVector, shoulder)))
                    < reach * reach)
                    contact = aimPointVector;

                XMFLOAT3 fistOffsetValue = player
                    ? player->punchFistOffset : XMFLOAT3{};
                if (activeVariant)
                {
                    fistOffsetValue.x += activeVariant->fistOffset.x;
                    fistOffsetValue.y += activeVariant->fistOffset.y;
                    fistOffsetValue.z += activeVariant->fistOffset.z;
                }
                XMMATRIX correctedHand = hand;
                if (activeVariant)
                {
                    const XMVECTOR handPosition = correctedHand.r[3];
                    correctedHand = XMMatrixRotationRollPitchYaw(
                        XMConvertToRadians(activeVariant->handRotation.x),
                        XMConvertToRadians(activeVariant->handRotation.y),
                        XMConvertToRadians(activeVariant->handRotation.z)) * correctedHand;
                    correctedHand.r[3] = handPosition;
                }
                const XMVECTOR fistOffset = XMVector3TransformNormal(
                    XMLoadFloat3(&fistOffsetValue), correctedHand * world);
                const XMVECTOR wristTarget = XMVectorSubtract(contact, fistOffset);
                const XMVECTOR modelTarget = XMVector3TransformCoord(wristTarget, inverseWorld);
                float phase = 1.0f;
                if (player)
                    phase = player->punchDuration > 0.01f
                        ? player->punchTime / player->punchDuration : 1.0f;
                else if (const auto* clip =
                    mesh.model->GetAnimation(animation.upperBodyAnimationIndex))
                {
                    const float rate = clip->ticksPerSecond > 0.0f
                        ? clip->ticksPerSecond : 24.0f;
                    const float clipSpeed = (std::max)(0.01f,
                        animation.GetClipSpeed(clip->name));
                    const float duration = clip->duration / (rate * clipSpeed);
                    phase = duration > 0.0f
                        ? (duration - animation.oneShotTimeRemaining) / duration
                        : 1.0f;
                }
                if (preview)
                {
                    const auto* ownerMesh = registry.GetComponent<MeshRendererComponent>(owner);
                    const int previewClipIndex =
                        ownerAnimation->upperBodyAnimationIndex >= 0
                        ? ownerAnimation->upperBodyAnimationIndex
                        : ownerAnimation->animationIndex;
                    const auto* clip = ownerMesh && ownerMesh->model
                        ? ownerMesh->model->GetAnimation(previewClipIndex)
                        : nullptr;
                    if (clip)
                    {
                        const float rate = clip->ticksPerSecond > 0.0f
                            ? clip->ticksPerSecond : 24.0f;
                        const float duration = clip->duration / rate;
                        const float previewTime = fullRuntimePreview &&
                            ownerAnimation->socketPreviewBlendEnabled &&
                            ownerAnimation->upperBodyAnimationIndex >= 0
                            ? ownerAnimation->socketPreviewUpperTime
                            : ownerAnimation->socketPreviewTime;
                        phase = duration > 0.0f
                            ? previewTime / duration : 0.0f;
                    }
                }
                phase = std::clamp(phase, 0.0f, 1.0f);
                float configuredContact = player ? player->punchContactPhase : 0.55f;
                if (ownerAnimation && variant >= 0 &&
                    variant < static_cast<int>(ownerAnimation->punchVariants.size()) &&
                    ownerAnimation->punchVariants[variant].contactPhase >= 0.0f)
                    configuredContact = ownerAnimation->punchVariants[variant].contactPhase;
                const float ikStartPhase = activeVariant
                    ? std::clamp(activeVariant->ikStartPhase, 0.0f, 0.95f)
                    : 0.0f;
                const float ikEndPhase = activeVariant
                    ? std::clamp(activeVariant->ikEndPhase,
                        ikStartPhase + 0.05f, 1.0f)
                    : 1.0f;
                const float contactPhase = std::clamp(configuredContact,
                    ikStartPhase + 0.001f, ikEndPhase - 0.001f);
                float envelope = 0.0f;
                if (phase > ikStartPhase && phase < ikEndPhase)
                    envelope = phase <= contactPhase
                        ? (phase - ikStartPhase) /
                            (contactPhase - ikStartPhase)
                        : (ikEndPhase - phase) /
                            (ikEndPhase - contactPhase);
                envelope = std::clamp(envelope, 0.0f, 1.0f);
                float configuredStrength = player ? player->punchIKStrength : 1.0f;
                if (activeVariant && activeVariant->ikStrength >= 0.0f)
                    configuredStrength = activeVariant->ikStrength;
                const float strength = std::clamp(
                    envelope * configuredStrength,
                    0.0f, 1.0f);
                const float viewStrength = view
                    ? std::clamp(envelope * player->punchFirstPersonIKStrength, 0.0f, 1.0f)
                    : strength;
                if (view && viewStrength <= 0.0001f)
                    return;
                XMMATRIX target = hand;
                if (activeVariant)
                {
                    const XMVECTOR handPosition = target.r[3];
                    target = XMMatrixRotationRollPitchYaw(
                        XMConvertToRadians(activeVariant->handRotation.x) * viewStrength,
                        XMConvertToRadians(activeVariant->handRotation.y) * viewStrength,
                        XMConvertToRadians(activeVariant->handRotation.z) * viewStrength) * target;
                    target.r[3] = handPosition;
                }
                target.r[3] = XMVectorLerp(hand.r[3], modelTarget, viewStrength);
                XMVECTOR poleTarget{};
                const XMVECTOR* pole = nullptr;
                if (activeVariant)
                {
                    const XMVECTOR configuredPole = XMLoadFloat3(
                        &activeVariant->elbowPoleOffset);
                    if (XMVectorGetX(XMVector3LengthSq(configuredPole)) > 0.000001f)
                    {
                        // A pole is a bend direction, not a small translation of
                        // the current elbow. The previous implementation added
                        // the setting to lower.r[3], so the authored elbow vector
                        // dominated the control and sliders appeared to do
                        // nothing. Project both directions onto the plane normal
                        // to the current reach axis, then smoothly hand ownership
                        // to the explicit pole as punch IK fades in.
                        const XMVECTOR reach = XMVectorSubtract(
                            target.r[3], upper.r[3]);
                        const float reachLengthSq = XMVectorGetX(
                            XMVector3LengthSq(reach));
                        if (reachLengthSq > 0.000001f)
                        {
                            const XMVECTOR reachAxis = XMVector3Normalize(reach);
                            const auto projectFromReach = [reachAxis](XMVECTOR value)
                            {
                                return XMVectorSubtract(value,
                                    XMVectorMultiply(reachAxis,
                                        XMVector3Dot(value, reachAxis)));
                            };
                            XMVECTOR authoredPole = projectFromReach(
                                XMVectorSubtract(lower.r[3], upper.r[3]));
                            XMVECTOR requestedPole = projectFromReach(configuredPole);
                            const float authoredLengthSq = XMVectorGetX(
                                XMVector3LengthSq(authoredPole));
                            const float requestedLengthSq = XMVectorGetX(
                                XMVector3LengthSq(requestedPole));
                            if (requestedLengthSq > 0.000001f)
                            {
                                requestedPole = XMVector3Normalize(requestedPole);
                                authoredPole = authoredLengthSq > 0.000001f
                                    ? XMVector3Normalize(authoredPole)
                                    : requestedPole;
                                XMVECTOR bendDirection = XMVectorLerp(
                                    authoredPole, requestedPole, viewStrength);
                                if (XMVectorGetX(XMVector3LengthSq(bendDirection)) <
                                    0.000001f)
                                    bendDirection = requestedPole;
                                poleTarget = XMVectorAdd(upper.r[3],
                                    XMVector3Normalize(bendDirection));
                                pole = &poleTarget;
                            }
                        }
                    }
                }
                if (!mesh.model->ApplyTwoBoneIK(animation,
                    upperBone, forearmBone, handBone, target, pole))
                    return;

                XMMATRIX updatedHand;
                if (mesh.model->TryGetBoneModelTransform(animation,
                    handBone, updatedHand))
                {
                    const XMVECTOR fist = XMVectorAdd(
                        XMVector3TransformCoord(updatedHand.r[3], world),
                        XMVector3TransformNormal(XMLoadFloat3(&fistOffsetValue),
                            updatedHand * world));
                    if (preview)
                        ownerAnimation->socketPreviewPunchAimError =
                            XMVectorGetX(XMVector3Length(
                                XMVectorSubtract(fist, contact)));
                    if (player && entity == owner)
                    {
                        XMStoreFloat3(&player->punchFistWorld, fist);
                        const XMVECTOR offsetFromRay = XMVectorSubtract(fist, rayOrigin);
                        const XMVECTOR closestOnRay = XMVectorAdd(rayOrigin,
                            XMVectorScale(rayDirection,
                                XMVectorGetX(XMVector3Dot(offsetFromRay, rayDirection))));
                        player->punchAimError = XMVectorGetX(XMVector3Length(
                            XMVectorSubtract(fist, closestOnRay)));
                    }
                }
            });
    }
};
