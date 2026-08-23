#pragma once

#include <cmath>

#include "ECSRegistry.h"
#include "Components.h"
#include "cameraclass.h"

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

        const DirectX::XMFLOAT3 cameraPosition = camera->GetPosition();
        const DirectX::XMFLOAT3 cameraRotation = camera->GetRotation();
        const float pitch = DirectX::XMConvertToRadians(cameraRotation.x);
        const float yaw = DirectX::XMConvertToRadians(cameraRotation.y);

        const float sinYaw = std::sin(yaw);
        const float cosYaw = std::cos(yaw);
        const float sinPitch = std::sin(pitch);
        const float cosPitch = std::cos(pitch);

        const DirectX::XMFLOAT3 right = { cosYaw, 0.0f, -sinYaw };
        const DirectX::XMFLOAT3 up = {
            -sinYaw * sinPitch,
            cosPitch,
            -cosYaw * sinPitch
        };
        const DirectX::XMFLOAT3 forward = {
            sinYaw * cosPitch,
            -sinPitch,
            cosYaw * cosPitch
        };

        registry.ForEach<FirstPersonViewComponent, TransformComponent>(
            [&](Entity, FirstPersonViewComponent& viewModel, TransformComponent& transform)
            {
                if (viewModel.owner == INVALID_ENTITY)
                    viewModel.owner = localPlayer;
                if (viewModel.owner != localPlayer || !viewModel.followCamera)
                    return;

                const float viewPitch =
                    -pitch * viewModel.pitchScale + viewModel.pitchOffset;

                transform.position = {
                    cameraPosition.x + right.x * viewModel.cameraOffset.x +
                        up.x * viewModel.cameraOffset.y + forward.x * viewModel.cameraOffset.z,
                    cameraPosition.y + right.y * viewModel.cameraOffset.x +
                        up.y * viewModel.cameraOffset.y + forward.y * viewModel.cameraOffset.z,
                    cameraPosition.z + right.z * viewModel.cameraOffset.x +
                        up.z * viewModel.cameraOffset.y + forward.z * viewModel.cameraOffset.z
                };
                transform.rotation = {
                    viewPitch + viewModel.rotationOffset.x,
                    yaw + viewModel.rotationOffset.y,
                    viewModel.rotationOffset.z
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
