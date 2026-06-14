#include "GrassRenderer.h"

#include <algorithm>
#include <cstddef>
#include <d3dcompiler.h>
#include <random>
#include <windows.h>

#include "ModelScaleUtils.h"
#include "PhysicsWorld.h"

using namespace DirectX;

namespace
{
    template<typename T>
    void ReleaseResource(T*& resource)
    {
        if (resource)
        {
            resource->Release();
            resource = nullptr;
        }
    }

    bool CompileShader(
        const wchar_t* path,
        const char* entryPoint,
        const char* profile,
        ID3DBlob** blob)
    {
        ID3DBlob* errors = nullptr;
        DWORD flags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
        flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

        HRESULT hr = D3DCompileFromFile(
            path,
            nullptr,
            D3D_COMPILE_STANDARD_FILE_INCLUDE,
            entryPoint,
            profile,
            flags,
            0,
            blob,
            &errors);

        if (errors)
        {
            OutputDebugStringA(static_cast<const char*>(errors->GetBufferPointer()));
            errors->Release();
        }

        return SUCCEEDED(hr);
    }

    template<typename T>
    bool CreateDynamicConstantBuffer(ID3D11Device* device, ID3D11Buffer** buffer)
    {
        D3D11_BUFFER_DESC desc = {};
        desc.Usage = D3D11_USAGE_DYNAMIC;
        desc.ByteWidth = (sizeof(T) + 15u) & ~15u;
        desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        return SUCCEEDED(device->CreateBuffer(&desc, nullptr, buffer));
    }
}

GrassRenderer::~GrassRenderer()
{
    Shutdown();
}

bool GrassRenderer::Initialize(ID3D11Device* device, ID3D11DeviceContext* context)
{
    if (!device || !context)
        return false;

    Shutdown();

    if (!InitializeShaders(device))
    {
        Shutdown();
        return false;
    }

    if (!InitializeBatch(device, context, m_batches[0], "assets/model/grass1.glb") ||
        !InitializeBatch(device, context, m_batches[1], "assets/model/grass2.glb"))
    {
        Shutdown();
        return false;
    }

    return true;
}

void GrassRenderer::Shutdown()
{
    for (auto& batch : m_batches)
    {
        ReleaseResource(batch.instanceBuffer);
        batch.instances.clear();
        batch.model.Shutdown();
    }

    ReleaseResource(m_rasterizerState);
    ReleaseResource(m_sampler);
    ReleaseResource(m_batchBuffer);
    ReleaseResource(m_grassSettingsBuffer);
    ReleaseResource(m_lightBuffer);
    ReleaseResource(m_matrixBuffer);
    ReleaseResource(m_inputLayout);
    ReleaseResource(m_pixelShader);
    ReleaseResource(m_vertexShader);
}

bool GrassRenderer::InitializeShaders(ID3D11Device* device)
{
    ID3DBlob* vsBlob = nullptr;
    ID3DBlob* psBlob = nullptr;

    if (!CompileShader(
            L"src/Render/Grass/grass_instanced.vs",
            "GrassVertexShader",
            "vs_5_0",
            &vsBlob))
    {
        return false;
    }

    if (!CompileShader(
            L"src/Render/Grass/grass_instanced.ps",
            "GrassPixelShader",
            "ps_5_0",
            &psBlob))
    {
        vsBlob->Release();
        return false;
    }

    HRESULT hr = device->CreateVertexShader(
        vsBlob->GetBufferPointer(),
        vsBlob->GetBufferSize(),
        nullptr,
        &m_vertexShader);
    if (FAILED(hr))
    {
        vsBlob->Release();
        psBlob->Release();
        return false;
    }

    hr = device->CreatePixelShader(
        psBlob->GetBufferPointer(),
        psBlob->GetBufferSize(),
        nullptr,
        &m_pixelShader);
    if (FAILED(hr))
    {
        vsBlob->Release();
        psBlob->Release();
        return false;
    }

    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(AssimpModelClass::VertexType, position), D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, offsetof(AssimpModelClass::VertexType, texture),  D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(AssimpModelClass::VertexType, normal),   D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "INSTANCE", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, offsetof(GrassInstanceGpu, row0), D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "INSTANCE", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, offsetof(GrassInstanceGpu, row1), D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "INSTANCE", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, offsetof(GrassInstanceGpu, row2), D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "INSTANCE", 3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, offsetof(GrassInstanceGpu, row3), D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "INSTANCE", 4, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, offsetof(GrassInstanceGpu, params), D3D11_INPUT_PER_INSTANCE_DATA, 1 }
    };

    hr = device->CreateInputLayout(
        layout,
        static_cast<UINT>(std::size(layout)),
        vsBlob->GetBufferPointer(),
        vsBlob->GetBufferSize(),
        &m_inputLayout);

    vsBlob->Release();
    psBlob->Release();

    if (FAILED(hr) ||
        !CreateDynamicConstantBuffer<MatrixBufferType>(device, &m_matrixBuffer) ||
        !CreateDynamicConstantBuffer<LightBufferType>(device, &m_lightBuffer) ||
        !CreateDynamicConstantBuffer<GrassSettingsBufferType>(device, &m_grassSettingsBuffer) ||
        !CreateDynamicConstantBuffer<BatchBufferType>(device, &m_batchBuffer))
    {
        return false;
    }

    D3D11_SAMPLER_DESC samplerDesc = {};
    samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.MinLOD = 0.0f;
    samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
    if (FAILED(device->CreateSamplerState(&samplerDesc, &m_sampler)))
        return false;

    D3D11_RASTERIZER_DESC rasterizerDesc = {};
    rasterizerDesc.FillMode = D3D11_FILL_SOLID;
    rasterizerDesc.CullMode = D3D11_CULL_NONE;
    rasterizerDesc.DepthClipEnable = TRUE;
    return SUCCEEDED(device->CreateRasterizerState(&rasterizerDesc, &m_rasterizerState));
}

bool GrassRenderer::InitializeBatch(
    ID3D11Device* device,
    ID3D11DeviceContext* context,
    GrassBatch& batch,
    const char* modelPath)
{
    if (!batch.model.Initialize(device, context, modelPath, nullptr))
    {
        OutputDebugStringA(("GrassRenderer: failed to load " + std::string(modelPath) + "\n").c_str());
        return false;
    }

    XMFLOAT3 minBounds;
    XMFLOAT3 maxBounds;
    batch.model.GetBoundingBox(&minBounds, &maxBounds);
    batch.importedMinY = minBounds.y;
    batch.importedHeight = (std::max)(0.0001f, maxBounds.y - minBounds.y);
    batch.normalizationScale = ModelScaleUtils::UniformScaleToFit(
        batch.model,
        m_targetGrassFootprint,
        m_targetGrassHeight,
        m_targetGrassFootprint);

    return CreateInstanceBuffer(device, batch);
}

bool GrassRenderer::CreateInstanceBuffer(ID3D11Device* device, GrassBatch& batch)
{

    ReleaseResource(batch.instanceBuffer);

    if (batch.instances.empty())
        return true;




    D3D11_BUFFER_DESC desc = {};
    desc.Usage = D3D11_USAGE_DYNAMIC;
    desc.ByteWidth = static_cast<UINT>(sizeof(GrassInstanceGpu) * batch.instances.size());
    desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    return SUCCEEDED(device->CreateBuffer(&desc, nullptr, &batch.instanceBuffer));
}

bool GrassRenderer::UpdateVisibleInstances(
    ID3D11DeviceContext* context,
    GrassBatch& batch,
    const XMFLOAT3& cameraPosition)
{
    if (!batch.instanceBuffer || batch.instances.empty())
    {
        batch.visibleCount = 0;
        return false;
    }

    D3D11_MAPPED_SUBRESOURCE mapped = {};
    if (FAILED(context->Map(batch.instanceBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
        return false;

    auto* output = static_cast<GrassInstanceGpu*>(mapped.pData);
    UINT visibleCount = 0;
    const float cullDistanceSq = m_cullDistance * m_cullDistance;

    for (const auto& instance : batch.instances)
    {
        const float dx = instance.position.x - cameraPosition.x;
        const float dz = instance.position.z - cameraPosition.z;
        if (dx * dx + dz * dz > cullDistanceSq)
            continue;

        const float finalScale = instance.scale * batch.normalizationScale;
        const XMMATRIX world =
            XMMatrixTranslation(0.0f, -batch.importedMinY, 0.0f) *
            XMMatrixScaling(finalScale, finalScale, finalScale) *
            XMMatrixRotationY(XMConvertToRadians(instance.rotationY)) *
            XMMatrixTranslation(
                instance.position.x,
                instance.position.y,
                instance.position.z);

        XMFLOAT4X4 stored;
        XMStoreFloat4x4(&stored, world);
        output[visibleCount++] = {
            { stored._11, stored._12, stored._13, stored._14 },
            { stored._21, stored._22, stored._23, stored._24 },
            { stored._31, stored._32, stored._33, stored._34 },
            { stored._41, stored._42, stored._43, stored._44 },
            { instance.rotationY * 0.031f, 0.0f, 0.0f, 0.0f }
        };
    }

    context->Unmap(batch.instanceBuffer, 0);
    batch.visibleCount = visibleCount;
    return batch.visibleCount > 0;
}

bool GrassRenderer::UpdateFrameBuffers(
    ID3D11DeviceContext* context,
    const XMMATRIX& view,
    const XMMATRIX& projection,
    const XMFLOAT3& cameraPosition,
    const XMFLOAT3& sunDirection,
    float ambientStrength)
{
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    if (FAILED(context->Map(m_matrixBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
        return false;

    auto* matrices = static_cast<MatrixBufferType*>(mapped.pData);
    matrices->view = XMMatrixTranspose(view);
    matrices->projection = XMMatrixTranspose(projection);
    context->Unmap(m_matrixBuffer, 0);
    context->VSSetConstantBuffers(0, 1, &m_matrixBuffer);

    if (FAILED(context->Map(m_lightBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
        return false;

    auto* light = static_cast<LightBufferType*>(mapped.pData);
    light->direction = { sunDirection.x, sunDirection.y, sunDirection.z, 0.0f };
    const float ambient = (std::max)(0.12f, ambientStrength);
    light->ambient = { ambient * 0.85f, ambient, ambient * 0.72f, 1.0f };
    context->Unmap(m_lightBuffer, 0);
    context->PSSetConstantBuffers(0, 1, &m_lightBuffer);

    if (FAILED(context->Map(m_grassSettingsBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
        return false;

    auto* settings = static_cast<GrassSettingsBufferType*>(mapped.pData);
    settings->cameraPositionAndTime = {
        cameraPosition.x, cameraPosition.y, cameraPosition.z, m_elapsedTime
    };
    settings->distancesAndWind = {
        m_fadeStartDistance, m_cullDistance, m_windStrength, 0.0f
    };
    context->Unmap(m_grassSettingsBuffer, 0);
    context->VSSetConstantBuffers(1, 1, &m_grassSettingsBuffer);
    context->PSSetConstantBuffers(1, 1, &m_grassSettingsBuffer);

    return true;
}

void GrassRenderer::Render(
    ID3D11DeviceContext* context,
    const XMMATRIX& view,
    const XMMATRIX& projection,
    const XMFLOAT3& cameraPosition,
    float deltaTime,
    const XMFLOAT3& sunDirection,
    float ambientStrength)
{
    if (!context || !m_vertexShader || !m_pixelShader)
        return;

    m_elapsedTime += (std::max)(0.0f, deltaTime);
    if (!UpdateFrameBuffers(
        context, view, projection, cameraPosition, sunDirection, ambientStrength))
        return;

    ID3D11RasterizerState* previousRasterizer = nullptr;
    context->RSGetState(&previousRasterizer);

    context->RSSetState(m_rasterizerState);
    context->IASetInputLayout(m_inputLayout);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->VSSetShader(m_vertexShader, nullptr, 0);
    context->PSSetShader(m_pixelShader, nullptr, 0);
    context->PSSetSamplers(0, 1, &m_sampler);

    for (auto& batch : m_batches)
    {
        if (UpdateVisibleInstances(context, batch, cameraPosition))
            RenderBatch(context, batch);
    }

    ID3D11ShaderResourceView* nullSrv = nullptr;
    context->PSSetShaderResources(0, 1, &nullSrv);
    context->RSSetState(previousRasterizer);
    ReleaseResource(previousRasterizer);
}

void GrassRenderer::RenderBatch(ID3D11DeviceContext* context, const GrassBatch& batch)
{
    const UINT strides[] = {
        sizeof(AssimpModelClass::VertexType),
        sizeof(GrassInstanceGpu)
    };
    const UINT offsets[] = { 0, 0 };

    D3D11_MAPPED_SUBRESOURCE mapped = {};
    if (FAILED(context->Map(m_batchBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
        return;

    auto* batchSettings = static_cast<BatchBufferType*>(mapped.pData);
    batchSettings->heightData = {
        batch.importedMinY,
        1.0f / batch.importedHeight,
        0.0f,
        0.0f
    };
    context->Unmap(m_batchBuffer, 0);
    context->VSSetConstantBuffers(2, 1, &m_batchBuffer);

    for (const auto& part : batch.model.GetMeshParts())
    {
        ID3D11Buffer* buffers[] = { part.vertexBuffer, batch.instanceBuffer };
        context->IASetVertexBuffers(0, 2, buffers, strides, offsets);
        context->IASetIndexBuffer(part.indexBuffer, DXGI_FORMAT_R32_UINT, 0);
        context->PSSetShaderResources(0, 1, &part.texture);
        context->DrawIndexedInstanced(
            part.indexCount,
            batch.visibleCount,
            0,
            0,
            0);
    }
}


void GrassRenderer::GenerateAreas(
    ID3D11Device* device,
    ComponentManager<WorldAreaComponent>& areas)
{
    if (!device || m_batches.empty())
        return;

    for (auto& batch : m_batches)
        batch.instances.clear();

    for (auto& [entity, area] : areas)
    {
        if (!area.enabled)
            continue;

        if (area.type != WorldAreaType::Grass)
            continue;

        AppendArea(area);
    }

    for (auto& batch : m_batches)
        CreateInstanceBuffer(device, batch);
}

void GrassRenderer::AppendArea(const WorldAreaComponent& area)
{
    const int count = static_cast<int>(
        area.size.x * area.size.z * area.density);

    std::mt19937 gen(area.seed);

    std::uniform_real_distribution<float> offsetX(
        -area.size.x * 0.5f,
        area.size.x * 0.5f);

    std::uniform_real_distribution<float> offsetZ(
        -area.size.z * 0.5f,
        area.size.z * 0.5f);

    std::uniform_real_distribution<float> rotation(0.0f, 360.0f);
    std::uniform_real_distribution<float> scale(0.85f, 1.15f);
    std::uniform_int_distribution<size_t> batchIndex(
        0, m_batches.size() - 1);

    int accepted = 0;
    int attempts = 0;
    const int maxAttempts = (std::max)(count * 4, count);

    while (accepted < count && attempts < maxAttempts)
    {
        ++attempts;

        const float x = area.center.x + offsetX(gen);
        const float z = area.center.z + offsetZ(gen);
        float terrainY = 0.0f;
        if (!TryProjectToTerrain(x, z, area, terrainY))
            continue;

        GrassInstance instance;
        instance.position = {
            x,
            terrainY + 0.01f,
            z
        };
        instance.rotationY = rotation(gen);
        instance.scale = scale(gen);

        m_batches[batchIndex(gen)].instances.push_back(instance);
        ++accepted;
    }
}

void GrassRenderer::GenerateArea(
    ID3D11Device* device,
    const WorldAreaComponent& area)
{
    if (!device || m_batches.empty())
        return;

    for (auto& batch : m_batches)
        batch.instances.clear();

    AppendArea(area);

    for (auto& batch : m_batches)
        CreateInstanceBuffer(device, batch);
}

bool GrassRenderer::TryProjectToTerrain(
    float x,
    float z,
    const WorldAreaComponent& area,
    float& terrainY) const
{
    auto* world = PhysicsWorld::Get().GetWorld();
    if (!world)
        return false;

    const float halfHeight = (std::max)(area.size.y * 0.5f, 1.0f);
    constexpr float rayPadding = 1000.0f;

    const btVector3 rayStart(
        x,
        area.center.y + halfHeight + rayPadding,
        z);
    const btVector3 rayEnd(
        x,
        area.center.y - halfHeight - rayPadding,
        z);

    btCollisionWorld::ClosestRayResultCallback hit(rayStart, rayEnd);
    hit.m_collisionFilterGroup = btBroadphaseProxy::DefaultFilter;
    hit.m_collisionFilterMask = btBroadphaseProxy::StaticFilter;

    world->rayTest(rayStart, rayEnd, hit);
    if (!hit.hasHit())
        return false;

    terrainY = hit.m_hitPointWorld.y();
    return true;
}
