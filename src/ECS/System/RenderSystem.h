#pragma once
#include "ECSRegistry.h"
#include "Components.h"
#include "RenderMesh.h"
#include "GameSettings.h"

#ifndef NUM_LIGHTS
#define NUM_LIGHTS 4
#endif

class RenderSystem
{
public:
    static void Render(
        ECSRegistry& registry,
        ID3D11DeviceContext* context,
        const XMMATRIX& view,
        const XMMATRIX& projection,
        MaterialClass* defaultMaterial,
        XMFLOAT4                 lightColors[],
        XMFLOAT4                 lightPositions[],
        const Settings::FogSettings& fog,
        Entity                   selectedEntity = INVALID_ENTITY,
        XMFLOAT3                 cameraPos = { 0.0f, 0.0f, 0.0f }) // ← новый параметр
    {
        auto& meshPool = registry.GetComponents<MeshRendererComponent>();

        for (auto& [entity, mesh] : meshPool)
        {
            if (!registry.HasComponent<ActiveComponent>(entity)) continue;

            auto* transform = registry.GetComponent<TransformComponent>(entity);
            if (!transform) continue;

            if (!mesh.model) continue;

            auto* material = mesh.material ? mesh.material : defaultMaterial;
            if (!material) continue;

            XMMATRIX world = transform->GetWorldMatrix();
            bool     selected = (entity == selectedEntity);

            RenderMesh(context, mesh.model, world, view, projection,
                material, lightColors, lightPositions, fog, selected, cameraPos);
        }
    }
};