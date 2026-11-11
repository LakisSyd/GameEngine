#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include <DirectXMath.h>

#include <ECSTypes.h>
#include <assimpmodelclass.h>

// Один вырез, который можно отменить независимо от остальных пробоин.
// Координаты локальные для конкретного render/collision меша.
struct RuntimeMeshCutOperation
{
    std::uint64_t id = 0;
    Entity ownerPanel = INVALID_ENTITY;
    DirectX::XMFLOAT3 localCenter = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 localNormal = { 0.0f, 1.0f, 0.0f };
    float radius = 0.0f;
    float depth = 0.0f;
    bool standalonePanelMesh = false;
};

// Immutable original geometry shared with repair worker jobs. A shared snapshot
// avoids copying the complete undamaged ship-panel mesh on the main thread when
// repair reaches 100%, and remains valid even if the component's track vector
// reallocates while a worker is running.
struct RuntimeMeshGeometrySnapshot
{
    std::vector<AssimpModelClass::VertexType> vertices;
    std::vector<unsigned long> indices;
};

// Ленивая копия исходной геометрии и все активные пробоины одного меша.
struct RuntimeMeshDamageTrack
{
    AssimpModelClass* model = nullptr;
    ID3D11Device* device = nullptr; // Устройство принадлежит рендереру и живёт дольше сцены.
    std::shared_ptr<const RuntimeMeshGeometrySnapshot> baseline;
    std::vector<RuntimeMeshCutOperation> cuts;
    std::uint64_t revision = 0;
};

// У entity могут быть разные render- и collision-меши, поэтому храним набор
// независимых дорожек восстановления, а не только одну копию геометрии.
struct RuntimeMeshDamageComponent
{
    std::vector<RuntimeMeshDamageTrack> tracks;
};

// Связывает игровую пробоину с вырезами в видимом и физическом мешах.
struct HullBreachRepairLinkComponent
{
    std::uint64_t cutOperationId = 0;
};
