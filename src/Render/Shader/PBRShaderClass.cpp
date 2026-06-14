#include "PBRShaderClass.h"

namespace
{
    static ID3D11ShaderResourceView* Create1x1SRV(ID3D11Device* device,
        unsigned char r, unsigned char g, unsigned char b, unsigned char a)
    {
        if (!device) return nullptr;

        unsigned char px[4] = { r, g, b, a };
        D3D11_TEXTURE2D_DESC td{};
        td.Width = 1; td.Height = 1; td.MipLevels = 1; td.ArraySize = 1;
        td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        td.SampleDesc.Count = 1;
        td.Usage = D3D11_USAGE_DEFAULT;
        td.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        D3D11_SUBRESOURCE_DATA sd{};
        sd.pSysMem = px; sd.SysMemPitch = 4;

        ID3D11Texture2D* tex = nullptr;
        if (FAILED(device->CreateTexture2D(&td, &sd, &tex)) || !tex) return nullptr;

        ID3D11ShaderResourceView* srv = nullptr;
        const HRESULT hr = device->CreateShaderResourceView(tex, nullptr, &srv);
        tex->Release();
        return SUCCEEDED(hr) ? srv : nullptr;
    }
}

// ============================================================================
bool PBRShaderClass::Initialize(ID3D11Device* device, HWND hwnd)
{
    ID3D10Blob* vsBlob = nullptr;
    ID3D10Blob* psBlob = nullptr;

    if (!CompileShader(L"src/Render/Shader/Vs/pbr.vs", "PBRVertexShader", "vs_5_0", &vsBlob))
        return false;
    if (!CompileShader(L"src/Render/Shader/Ps/pbr.ps", "PBRPixelShader", "ps_5_0", &psBlob))
    {
        vsBlob->Release(); return false;
    }

    HRESULT hr;
    hr = device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &m_vertexShader);
    if (FAILED(hr)) { vsBlob->Release(); psBlob->Release(); return false; }

    hr = device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &m_pixelShader);
    if (FAILED(hr)) { vsBlob->Release(); psBlob->Release(); return false; }

    D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }, // offset 12
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 20, D3D11_INPUT_PER_VERTEX_DATA, 0 }, // offset 20
        { "TANGENT",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0 }, // offset 32
        { "BINORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 44, D3D11_INPUT_PER_VERTEX_DATA, 0 }, // offset 44

        // НОВОЕ: Веса костей - ПРОВЕРЬТЕ OFFSET!
        { "TEXCOORD", 1, DXGI_FORMAT_R32G32B32A32_SINT,   0, 56, D3D11_INPUT_PER_VERTEX_DATA, 0 }, // boneIDs - offset 56
        { "TEXCOORD", 2, DXGI_FORMAT_R32G32B32A32_FLOAT,  0, 72, D3D11_INPUT_PER_VERTEX_DATA, 0 }, // boneWeights - offset 72
    };
    if (!CreateInputLayout(device, layout, _countof(layout), vsBlob)) { vsBlob->Release(); psBlob->Release(); return false; }
    vsBlob->Release(); psBlob->Release();

    // -- Constant buffers ------------------------------------------------------
    if (!CreateConstantBuffer(device, sizeof(MatrixBuffer), &m_matrixBuffer))           return false;
    if (!CreateConstantBuffer(device, sizeof(LightPositionBuffer), &m_lightPosBuffer))         return false;
    if (!CreateConstantBuffer(device, sizeof(LightColorBuffer), &m_lightColorBuffer))       return false;
    if (!CreateConstantBuffer(device, sizeof(PBRParamsBuffer), &m_pbrParamsBuffer))        return false;
    if (!CreateConstantBuffer(device, sizeof(SelectedBuffer), &m_selectedBuffer))         return false;
    if (!CreateConstantBuffer(device, sizeof(DirectionalLightBuffer), &m_directionalLightBuffer)) return false;
    if (!CreateConstantBuffer(device, sizeof(ShadowBufferType), &m_shadowBuffer))           return false;
    if (!CreateConstantBuffer(device, sizeof(DamageBufferType), &m_damageBuffer))           return false;
    if (!CreateFogBuffer(device))        return false;
    if (!CreateDefaultSampler(device))   return false;

    // -- Shadow sampler (s7) ---------------------------------------------------
    D3D11_SAMPLER_DESC shadowSamplerDesc = {};
    shadowSamplerDesc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
    shadowSamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
    shadowSamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
    shadowSamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
    shadowSamplerDesc.BorderColor[0] = shadowSamplerDesc.BorderColor[1] =
        shadowSamplerDesc.BorderColor[2] = shadowSamplerDesc.BorderColor[3] = 1.0f;
    shadowSamplerDesc.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;
    shadowSamplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
    if (FAILED(device->CreateSamplerState(&shadowSamplerDesc, &m_shadowSampler))) return false;

    // -- Fallback текстуры -----------------------------------------------------
    m_defaultNormalSRV = Create1x1SRV(device, 128, 128, 255, 255); // нейтральный нормал
    m_defaultMetalnessSRV = Create1x1SRV(device, 0, 0, 0, 255); // диэлектрик
    m_defaultRoughnessSRV = Create1x1SRV(device, 220, 220, 220, 255); // умеренный блеск для PBR без roughness map
    m_defaultCrackSRV = Create1x1SRV(device, 0, 0, 0, 255); // нет трещин (чёрный)

    return true;
}

// ============================================================================
void PBRShaderClass::Shutdown()
{
    auto release = [](auto*& ptr) { if (ptr) { ptr->Release(); ptr = nullptr; } };

    release(m_matrixBuffer);
    release(m_lightPosBuffer);
    release(m_lightColorBuffer);
    release(m_pbrParamsBuffer);
    release(m_selectedBuffer);
    release(m_directionalLightBuffer);
    release(m_shadowBuffer);
    release(m_shadowSampler);
    release(m_damageBuffer);         // < буфер трещин
    release(m_defaultNormalSRV);
    release(m_defaultMetalnessSRV);
    release(m_defaultRoughnessSRV);
    release(m_defaultCrackSRV);      // < fallback трещин

    // m_crackTextureSRV и m_environmentMapSRV НЕ освобождаем — владеет ApplicationClass
    m_environmentMapSRV = nullptr;
    m_shadowMapSRV = nullptr;
    ShaderBaseClass::Shutdown();
}

// ============================================================================
void PBRShaderClass::SetMatrixBuffers(
    ID3D11DeviceContext* context,
    XMMATRIX world,
    XMMATRIX view,
    XMMATRIX projection)
{
    if (!context) return;

    MatrixBuffer mb;
    mb.world = XMMatrixTranspose(world);
    mb.view = XMMatrixTranspose(view);
    mb.projection = XMMatrixTranspose(projection);
    UpdateConstantBuffer(context, m_matrixBuffer, mb, 0, true);
}

// ============================================================================
bool PBRShaderClass::Render(
    ID3D11DeviceContext* context,
    int                  indexCount,
    XMMATRIX             world,
    XMMATRIX             view,
    XMMATRIX             projection,
    ID3D11ShaderResourceView* albedo,
    ID3D11ShaderResourceView* normalMap,
    ID3D11ShaderResourceView* metalness,
    ID3D11ShaderResourceView* roughness,
    XMFLOAT4  lightColors[NUM_LIGHTS],
    XMFLOAT4  lightPositions[NUM_LIGHTS],
    XMFLOAT3  cameraPos,
    float     selectedFactor,
    float     glassOpacity)
{
    if (!context) return false;

    // -- Fallback если текстура не назначена -----------------------------------
    if (!normalMap)  normalMap = m_defaultNormalSRV;
    if (!metalness)  metalness = m_defaultMetalnessSRV;
    if (!roughness)  roughness = m_defaultRoughnessSRV;

    // -- Constant buffers ------------------------------------------------------
    MatrixBuffer mb;
    mb.world = XMMatrixTranspose(world);
    mb.view = XMMatrixTranspose(view);
    mb.projection = XMMatrixTranspose(projection);
    UpdateConstantBuffer(context, m_matrixBuffer, mb, 0, true);

    LightPositionBuffer lpb;
    for (int i = 0; i < NUM_LIGHTS; i++) lpb.lightPosition[i] = lightPositions[i];
    UpdateConstantBuffer(context, m_lightPosBuffer, lpb, 1, true);
    UpdateConstantBuffer(context, m_lightPosBuffer, lpb, 1, false);

    LightColorBuffer lcb;
    for (int i = 0; i < NUM_LIGHTS; i++) lcb.diffuseColor[i] = lightColors[i];
    UpdateConstantBuffer(context, m_lightColorBuffer, lcb, 0, false);

    PBRParamsBuffer ppb;
    ppb.cameraPos = cameraPos;
    ppb.glassOpacity = glassOpacity;
    UpdateConstantBuffer(context, m_pbrParamsBuffer, ppb, 6, false);

    SelectedBuffer sb;
    sb.selectedFactor = selectedFactor;
    sb.padding = { 0, 0, 0 };
    UpdateConstantBuffer(context, m_selectedBuffer, sb, 3, false);

    // -- Биндим шейдеры и семплер ----------------------------------------------
    BindShader(context);
    if (m_sampleState)
        context->PSSetSamplers(0, 1, &m_sampleState);
    if (m_directionalLightBuffer)
        context->PSSetConstantBuffers(4, 1, &m_directionalLightBuffer);
    if (m_shadowBuffer)
        context->PSSetConstantBuffers(5, 1, &m_shadowBuffer);
    if (m_shadowMapSRV)
        context->PSSetShaderResources(7, 1, &m_shadowMapSRV);
    if (m_shadowSampler)
        context->PSSetSamplers(7, 1, &m_shadowSampler);

    // -- SRV слоты t0-t3 (PBR текстуры) ---------------------------------------
    ID3D11ShaderResourceView* srvs[4] = { albedo, normalMap, metalness, roughness };
    context->PSSetShaderResources(0, 4, srvs);

    // -- SRV слот t6 (crack маска) — ПЕРЕД DrawIndexed -------------------------
    // Используем внешнюю текстуру если загружена, иначе чёрный fallback (нет трещин)
    ID3D11ShaderResourceView* crackSRV = m_crackTextureSRV ? m_crackTextureSRV : m_defaultCrackSRV;
    context->PSSetShaderResources(6, 1, &crackSRV);

    DamageBufferType emptyDamage{};
    UpdateConstantBuffer(context, m_damageBuffer, emptyDamage, 7, false);

    // -- SRV слот t8 (environment cubemap) --------------------------------------
    // Это не полноценный IBL/prefiltered BRDF, а лёгкий skybox reflection,
    // чтобы PBR-материалы не выглядели плоско при отсутствии HDRI.
    ID3D11ShaderResourceView* environmentSRV = m_environmentMapSRV;
    context->PSSetShaderResources(8, 1, &environmentSRV);

    // -- Draw ------------------------------------------------------------------
    context->DrawIndexed(indexCount, 0, 0);

    // -- Очистка слотов --------------------------------------------------------
    ID3D11ShaderResourceView* nullSRVs[9] = {};
    context->PSSetShaderResources(0, 9, nullSRVs);

    if (!albedo)
    {
        OutputDebugStringA("WARNING: Albedo texture is NULL!\n");
        // Создайте белую текстуру-заглушку
    }

    return true;
}

// ============================================================================
bool PBRShaderClass::SetDirectionalLight(
    ID3D11DeviceContext* context,
    const XMFLOAT3& direction,
    const XMFLOAT4& color,
    const XMFLOAT4& ambient,
    float intensity)
{
    D3D11_MAPPED_SUBRESOURCE mapped;
    if (FAILED(context->Map(m_directionalLightBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
        return false;

    DirectionalLightBuffer* data = (DirectionalLightBuffer*)mapped.pData;
    XMVECTOR dir = XMVector3Normalize(XMLoadFloat3(&direction));
    XMStoreFloat3((XMFLOAT3*)&data->direction, dir);
    data->direction.w = intensity;
    data->color = color;
    data->ambient = ambient;

    context->Unmap(m_directionalLightBuffer, 0);
    context->PSSetConstantBuffers(4, 1, &m_directionalLightBuffer);
    return true;
}

// ============================================================================
bool PBRShaderClass::SetShadowMap(
    ID3D11DeviceContext* context,
    ID3D11ShaderResourceView* shadowMapSRV,
    const XMMATRIX& lightViewProj)
{
    if (!context || !shadowMapSRV) return false;

    m_shadowMapSRV = shadowMapSRV;
    context->PSSetShaderResources(7, 1, &shadowMapSRV);
    if (m_shadowSampler) context->PSSetSamplers(7, 1, &m_shadowSampler);

    D3D11_MAPPED_SUBRESOURCE mapped;
    if (FAILED(context->Map(m_shadowBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
        return false;

    ((ShadowBufferType*)mapped.pData)->lightViewProj = XMMatrixTranspose(lightViewProj);
    context->Unmap(m_shadowBuffer, 0);
    context->PSSetConstantBuffers(5, 1, &m_shadowBuffer);
    return true;
}

// ============================================================================
bool PBRShaderClass::SetDamageDecals(
    ID3D11DeviceContext* context,
    const DamageDecalComponent* damage,
    const XMMATRIX& worldMatrix)
{
    if (!context || !m_damageBuffer) return false;

    DamageBufferType data{};

    if (damage)
    {
        int count = std::min((int)damage->cracks.size(), DamageDecalComponent::MAX_CRACKS);
        data.crackCount = count;

        for (int i = 0; i < count; ++i)
        {
            const auto& crack = damage->cracks[i];

            // Локальная позиция > мировая
            XMVECTOR worldPos = XMVector3TransformCoord(
                XMLoadFloat3(&crack.localPosition), worldMatrix);
            XMFLOAT3 wp;
            XMStoreFloat3(&wp, worldPos);

            data.crackPositions[i] = { wp.x, wp.y, wp.z, crack.radius };
            data.crackParams[i] = { crack.intensity, crack.rotation, 0.f, 0.f };
        }
    }
    // damage == nullptr > crackCount = 0 > шейдер пропускает цикл

    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(context->Map(m_damageBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
        return false;

    memcpy(mapped.pData, &data, sizeof(DamageBufferType));
    context->Unmap(m_damageBuffer, 0);
    context->PSSetConstantBuffers(7, 1, &m_damageBuffer);
    return true;
}
