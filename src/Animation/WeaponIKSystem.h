#pragma once
#include "ECSRegistry.h"
#include "Components.h"
#include "Gameplay/Combat/WeaponComponent.h"
#include "Gameplay/Player/PlayerComponents.h"

class WeaponIKSystem
{
public:
    static DirectX::XMMATRIX GripMatrix(const WeaponGripTransform& grip)
    {
        using namespace DirectX;
        return XMMatrixTranslation(-grip.pivot.x, -grip.pivot.y, -grip.pivot.z) *
            XMMatrixScaling(grip.scale.x, grip.scale.y, grip.scale.z) *
            XMMatrixRotationRollPitchYaw(grip.rotation.x, grip.rotation.y, grip.rotation.z) *
            XMMatrixTranslation(grip.pivot.x + grip.position.x,
                grip.pivot.y + grip.position.y, grip.pivot.z + grip.position.z);
    }
    static DirectX::XMFLOAT3 AttachmentPosition(const WeaponGripTransform& grip)
    {
        DirectX::XMFLOAT3 position{};
        DirectX::XMStoreFloat3(&position, GripMatrix(grip).r[3]);
        return position;
    }
    static void Update(ECSRegistry& registry)
    {
        registry.ForEach<AnimationComponent, MeshRendererComponent>(
            [&](Entity entity, AnimationComponent& animation, MeshRendererComponent& mesh) {
                animation.leftHandIKApplied = false;
                animation.leftHandIKError = 0.0f;
                if (!mesh.model || !animation.isPlaying) return;
                const auto* view = registry.GetComponent<FirstPersonViewComponent>(entity);
                if (view && !view->followCamera) return;
                const bool firstPerson = view != nullptr;
                const Entity owner = firstPerson ? view->owner : entity;
                if (const auto* ownerAnimation = registry.GetComponent<AnimationComponent>(owner);
                    ownerAnimation && ownerAnimation->socketPreviewEnabled &&
                    !ownerAnimation->socketPreviewApplyIK)
                    return;
                if (const auto* player = registry.GetComponent<PlayerComponent>(owner);
                    player && player->punchActive)
                    return;
                const auto* equipped = registry.GetComponent<EquippedWeaponComponent>(owner);
                const auto* weapon = registry.GetComponent<WeaponComponent>(equipped ? equipped->weapon : owner);
                if (!weapon || !(firstPerson ? weapon->enableFirstPersonLeftHandIK
                    : weapon->enableLeftHandIK)) return;
                using namespace DirectX;
                XMMATRIX rightHand;
                if (!mesh.model->TryGetBoneModelTransform(animation, weapon->rightHandBone, rightHand)) return;
                const XMMATRIX weaponModel = GripMatrix(firstPerson
                    ? weapon->firstPersonGrip : weapon->worldGrip) * rightHand;
                const auto& r = firstPerson ? weapon->firstPersonLeftHandRotation
                    : weapon->leftHandRotation;
                const auto& p = firstPerson ? weapon->firstPersonLeftHandGrip
                    : weapon->leftHandGrip;
                const XMMATRIX target = XMMatrixRotationRollPitchYaw(r.x, r.y, r.z) *
                    XMMatrixTranslation(p.x, p.y, p.z) * weaponModel;
                animation.leftHandIKApplied = mesh.model->ApplyTwoBoneIK(animation,
                    weapon->leftUpperArmBone, weapon->leftForearmBone, weapon->leftHandBone, target);
            });
    }
};
