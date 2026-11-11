#pragma once

#include <cmath>

#include "ECSRegistry.h"
#include "Components.h"
#include "cameraclass.h"
#include "Animation/AnimationComponent.h"
#include "Gameplay/Player/PlayerComponents.h"

class FirstPersonViewSystem
{
public:
    static void UpdateCameraAnchors(
        ECSRegistry& registry,
        CameraClass* camera,
        Entity localPlayer)
    {
        if (!camera || localPlayer == INVALID_ENTITY)
            return;

        const DirectX::XMFLOAT3 liveCameraPosition = camera->GetPosition();
        const DirectX::XMFLOAT3 liveCameraRotation = camera->GetRotation();

        registry.ForEach<FirstPersonViewComponent, TransformComponent>(
            [&](Entity entity, FirstPersonViewComponent& viewModel, TransformComponent& transform)
            {
                if (viewModel.owner == INVALID_ENTITY)
                    viewModel.owner = localPlayer;
                if (viewModel.owner != localPlayer || !viewModel.followCamera)
                    return;

                if (viewModel.studioFreezeCameraAnchor &&
                    !viewModel.studioCameraAnchorCaptured)
                {
                    viewModel.studioCameraPosition = liveCameraPosition;
                    viewModel.studioCameraRotation = liveCameraRotation;
                    viewModel.studioCameraAnchorCaptured = true;
                }
                if (!viewModel.studioFreezeCameraAnchor)
                    viewModel.studioCameraAnchorCaptured = false;

                const DirectX::XMFLOAT3 cameraPosition =
                    viewModel.studioCameraAnchorCaptured
                    ? viewModel.studioCameraPosition : liveCameraPosition;
                const DirectX::XMFLOAT3 cameraRotation =
                    viewModel.studioCameraAnchorCaptured
                    ? viewModel.studioCameraRotation : liveCameraRotation;
                const float pitch = DirectX::XMConvertToRadians(cameraRotation.x);
                const float yaw = DirectX::XMConvertToRadians(cameraRotation.y);

                // Match CameraClass::Render exactly. A camera-space offset must
                // keep the same distance from the captured/live eye at every
                // pitch and yaw.
                const DirectX::XMMATRIX cameraRotationMatrix =
                    DirectX::XMMatrixRotationRollPitchYaw(
                        pitch, yaw,
                        DirectX::XMConvertToRadians(cameraRotation.z));

                const float viewPitch =
                    -pitch * viewModel.pitchScale + viewModel.pitchOffset;

                // Keep the shared camera anchor stable. Guard and each punch
                // add their own saved framing only while that clip is active.
                DirectX::XMFLOAT3 cameraOffset = viewModel.cameraOffset;
                DirectX::XMFLOAT3 rotationOffset = viewModel.rotationOffset;
                const auto* ownerAnimation =
                    registry.GetComponent<AnimationComponent>(viewModel.owner);
                const auto* armsAnimation =
                    registry.GetComponent<AnimationComponent>(entity);
                const auto* armsMesh =
                    registry.GetComponent<MeshRendererComponent>(entity);
                const auto* player =
                    registry.GetComponent<PlayerComponent>(viewModel.owner);
                std::string clipName;
                int variant = -1;
                if (ownerAnimation && armsAnimation &&
                    ownerAnimation->socketPreviewEnabled &&
                    armsAnimation->socketPreviewEnabled)
                {
                    if (ownerAnimation->socketPreviewPunch)
                    {
                        variant = ownerAnimation->socketPreviewPunchVariant;
                        if (variant >= 0 && variant < static_cast<int>(
                            ownerAnimation->punchVariants.size()))
                            clipName = ownerAnimation->punchVariants[variant].firstPersonClip;
                    }
                    else if (armsMesh && armsMesh->model)
                    {
                        const auto* clip = armsMesh->model->GetAnimation(
                            armsAnimation->socketPreviewClip);
                        if (clip) clipName = clip->name;
                    }
                }
                else if (ownerAnimation && player && player->punchActive)
                {
                    variant = player->punchVariant;
                    if (variant >= 0 && variant < static_cast<int>(
                        ownerAnimation->punchVariants.size()))
                        clipName = ownerAnimation->punchVariants[variant].firstPersonClip;
                }
                else if (ownerAnimation && player && player->meleeGuardActive)
                {
                    if (ownerAnimation->currentState == AnimationState::Walk)
                        clipName = ownerAnimation->meleeActions.guardMoveFirstPersonClip;
                    if (clipName.empty())
                        clipName = ownerAnimation->meleeActions.guardFirstPersonClip;
                }
                if (clipName.empty() && armsAnimation && armsMesh && armsMesh->model)
                {
                    const int index = armsAnimation->upperBodyAnimationIndex >= 0
                        ? armsAnimation->upperBodyAnimationIndex
                        : armsAnimation->animationIndex;
                    const auto* clip = armsMesh->model->GetAnimation(index);
                    if (clip) clipName = clip->name;
                }
                if (ownerAnimation && !clipName.empty())
                {
                    const auto findOffset = [&](const std::string& name, int clipVariant)
                        -> const FirstPersonClipOffset*
                    {
                        const FirstPersonClipOffset* shared = nullptr;
                        for (const auto& offset : ownerAnimation->firstPersonClipOffsets)
                            if (offset.clip == name)
                            {
                                if (clipVariant >= 0 && offset.variant == clipVariant)
                                    return &offset;
                                if (offset.variant == -1) shared = &offset;
                            }
                        return shared;
                    };
                    const FirstPersonClipOffset* selected = findOffset(clipName, variant);
                    // A punch without its own camera framing inherits the
                    // guard framing. Otherwise random attack variants can
                    // briefly move both hands below the first-person camera.
                    if (!selected && player && player->punchActive)
                    {
                        if (ownerAnimation->currentState == AnimationState::Walk)
                            selected = findOffset(
                                ownerAnimation->meleeActions.guardMoveFirstPersonClip, -1);
                        if (!selected)
                            selected = findOffset(
                                ownerAnimation->meleeActions.guardFirstPersonClip, -1);
                    }
                    if (selected)
                    {
                        cameraOffset.x += selected->position.x;
                        cameraOffset.y += selected->position.y;
                        cameraOffset.z += selected->position.z;
                        rotationOffset.x += DirectX::XMConvertToRadians(selected->rotation.x);
                        rotationOffset.y += DirectX::XMConvertToRadians(selected->rotation.y);
                        rotationOffset.z += DirectX::XMConvertToRadians(selected->rotation.z);
                    }
                }

                const DirectX::XMVECTOR worldOffset =
                    DirectX::XMVector3TransformNormal(
                        DirectX::XMLoadFloat3(&cameraOffset),
                        cameraRotationMatrix);
                DirectX::XMStoreFloat3(
                    &transform.position,
                    DirectX::XMVectorAdd(
                        DirectX::XMLoadFloat3(&cameraPosition), worldOffset));
                transform.rotation = {
                    viewPitch + rotationOffset.x,
                    yaw + rotationOffset.y,
                    rotationOffset.z
                };
                transform.scale = viewModel.scale;
            });
    }

    static void UpdateBoneAttachments(ECSRegistry& registry)
    {
        registry.ForEach<BoneAttachmentComponent, TransformComponent>(
            [&](Entity, BoneAttachmentComponent& attachment, TransformComponent& transform)
            {
                if (!attachment.enabled || attachment.parent == INVALID_ENTITY)
                    return;

                auto* parentTransform = registry.GetComponent<TransformComponent>(attachment.parent);
                auto* parentMesh = registry.GetComponent<MeshRendererComponent>(attachment.parent);
                auto* parentAnimation = registry.GetComponent<AnimationComponent>(attachment.parent);
                if (!parentTransform || !parentMesh || !parentMesh->model || !parentAnimation)
                    return;

                DirectX::XMMATRIX boneModel;
                if (!parentMesh->model->TryGetBoneModelTransform(
                    *parentAnimation, attachment.boneName, boneModel))
                {
                    return;
                }

                const DirectX::XMMATRIX local =
                    DirectX::XMMatrixScaling(
                        attachment.localScale.x,
                        attachment.localScale.y,
                        attachment.localScale.z) *
                    DirectX::XMMatrixRotationRollPitchYaw(
                        attachment.localRotation.x,
                        attachment.localRotation.y,
                        attachment.localRotation.z) *
                    DirectX::XMMatrixTranslation(
                        attachment.localPosition.x,
                        attachment.localPosition.y,
                        attachment.localPosition.z);

                ApplyMatrixToTransform(
                    local * boneModel * parentTransform->GetWorldMatrix(),
                    transform);
            });
    }

private:
    static void ApplyMatrixToTransform(
        const DirectX::XMMATRIX& matrix,
        TransformComponent& transform)
    {
        DirectX::XMVECTOR scale;
        DirectX::XMVECTOR rotation;
        DirectX::XMVECTOR translation;
        if (!DirectX::XMMatrixDecompose(&scale, &rotation, &translation, matrix))
            return;

        DirectX::XMFLOAT4 quaternion{};
        DirectX::XMStoreFloat3(&transform.scale, scale);
        DirectX::XMStoreFloat3(&transform.position, translation);
        DirectX::XMStoreFloat4(&quaternion, rotation);

        const float sinX = 2.0f * (quaternion.w * quaternion.x + quaternion.y * quaternion.z);
        const float cosX = 1.0f - 2.0f * (quaternion.x * quaternion.x + quaternion.y * quaternion.y);
        const float sinY = 2.0f * (quaternion.w * quaternion.y - quaternion.z * quaternion.x);
        const float sinZ = 2.0f * (quaternion.w * quaternion.z + quaternion.x * quaternion.y);
        const float cosZ = 1.0f - 2.0f * (quaternion.y * quaternion.y + quaternion.z * quaternion.z);

        transform.rotation = {
            std::atan2(sinX, cosX),
            std::fabs(sinY) >= 1.0f
                ? std::copysign(DirectX::XM_PIDIV2, sinY)
                : std::asin(sinY),
            std::atan2(sinZ, cosZ)
        };
    }
};
