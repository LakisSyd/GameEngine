#include "PBRShaderClass.h"

namespace
{
    static ID3D11ShaderResourceView* Create1x1SRV(ID3D11Device* device,
        unsigned char r, unsigned char g, unsigned char b, unsigned char a)
    {
        if (!device) return nullptr;

        unsigned char px[4] = { r, g, b, a };
        D3D11_TEXTURE2D_DESC td{};
        td.Width = 1;
        td.Height = 1;
        td.MipLevels = 1;
        td.ArraySize = 1;
        td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        td.SampleDesc.Count = 1;
        td.Usage = D3D11_USAGE_DEFAULT;
        td.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        D3D11_SUBRESOURCE_DATA sd{};
        sd.pSysMem = px;
        sd.SysMemPitch = 4;

        ID3D11Texture2D* tex = nullptr;
        if (FAILED(device->CreateTexture2D(&td, &sd, &tex)) || !tex)
            return nullptr;

        ID3D11ShaderResourceView* srv = nullptr;
        const HRESULT hr = device->CreateShaderResourceView(tex, nullptr, &srv);
        tex->Release();

        return SUCCEEDED(hr) ? srv : nullptr;
    }
}

bool PBRShaderClass::Initialize(ID3D11Device* device, HWND hwnd)
{
    // ── Компилируем шейдеры ───────────────────────────────────────────────────
    ID3D10Blob* vsBlob = nullptr;
    ID3D10Blob* psBlob = nullptr;

    if (!CompileShader(L"src/Render/Shader/Vs/pbr.vs", "PBRVertexShader", "vs_5_0", &vsBlob))
        return false;

    if (!CompileShader(L"src/Render/Shader/Ps/pbr.ps", "PBRPixelShader", "ps_5_0", &psBlob))
    {
        vsBlob->Release();
        return false;
    }

    // ── Создаём объекты шейдеров ──────────────────────────────────────────────
    HRESULT hr;
    hr = device->CreateVertexShader(
        vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &m_vertexShader);
    if (FAILED(hr)) { vsBlob->Release(); psBlob->Release(); return false; }

    hr = device->CreatePixelShader(
        psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &m_pixelShader);
    if (FAILED(hr)) { vsBlob->Release(); psBlob->Release(); return false; }

    // ── Input Layout: POSITION + TEXCOORD + NORMAL + TANGENT + BINORMAL ───────
    // Должен совпадать с VertexInputType в pbr.vs
    D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 20, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TANGENT",  0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "BINORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 44, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    if (!CreateInputLayout(device, layout, 5, vsBlob))
    {
        vsBlob->Release(); psBlob->Release();
        return false;
    }

    vsBlob->Release();
    psBlob->Release();

    // ── Создаём constant buffers ──────────────────────────────────────────────
    if (!CreateConstantBuffer(device, sizeof(MatrixBuffer), &m_matrixBuffer))     return false;
    if (!CreateConstantBuffer(device, sizeof(LightPositionBuffer), &m_lightPosBuffer))   return false;
    if (!CreateConstantBuffer(device, sizeof(LightColorBuffer), &m_lightColorBuffer)) return false;
    if (!CreateConstantBuffer(device, sizeof(PBRParamsBuffer), &m_pbrParamsBuffer))  return false;
    if (!CreateConstantBuffer(device, sizeof(SelectedBuffer), &m_selectedBuffer))   return false;
    if (!CreateFogBuffer(device))                                                         return false;
    if (!CreateDefaultSampler(device))                                                    return false;
    if (!CreateConstantBuffer(device, sizeof(DirectionalLightBuffer),
        &m_directionalLightBuffer))
        return false;

    // 🔥 FIX: Создаём ShadowBuffer (b5) — раньше отсутствовал!
    if (!CreateConstantBuffer(device, sizeof(ShadowBufferType), &m_shadowBuffer))
        return false;

    // 🔥 FIX: Создаём comparison sampler для shadow map (s7)
    D3D11_SAMPLER_DESC shadowSamplerDesc = {};
    shadowSamplerDesc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
    shadowSamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
    shadowSamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
    shadowSamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
    shadowSamplerDesc.BorderColor[0] = 1.0f;
    shadowSamplerDesc.BorderColor[1] = 1.0f;
    shadowSamplerDesc.BorderColor[2] = 1.0f;
    shadowSamplerDesc.BorderColor[3] = 1.0f;
    shadowSamplerDesc.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;
    shadowSamplerDesc.MinLOD = 0;
    shadowSamplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

    if (FAILED(device->CreateSamplerState(&shadowSamplerDesc, &m_shadowSampler)))
        return false;

    // ... fallback текстуры без изменений ...
    m_defaultNormalSRV = Create1x1SRV(device, 128, 128, 255, 255);
    m_defaultMetalnessSRV = Create1x1SRV(device, 0, 0, 0, 255);
    m_defaultRoughnessSRV = Create1x1SRV(device, 255, 255, 255, 255);

  
    // ── 1x1 fallback текстуры для PBR ─────────────────────────────────────────
    // normal: neutral (0.5,0.5,1) in tangent space
    m_defaultNormalSRV = Create1x1SRV(device, 128, 128, 255, 255);
    // metalness: 0
    m_defaultMetalnessSRV = Create1x1SRV(device, 0, 0, 0, 255);
    // roughness: 1
    m_defaultRoughnessSRV = Create1x1SRV(device, 255, 255, 255, 255);

    return true;
}


bool PBRShaderClass::SetDirectionalLight(
    ID3D11DeviceContext* context,
    const XMFLOAT3& direction,
    const XMFLOAT4& color,
    const XMFLOAT4& ambient,
    float intensity)
{
    HRESULT result;
    D3D11_MAPPED_SUBRESOURCE mapped;

    result = context->Map(m_directionalLightBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if (FAILED(result)) return false;

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


void PBRShaderClass::Shutdown()
{
    // Освобождаем буферы
    auto release = [](auto*& ptr) {
        if (ptr) { ptr->Release(); ptr = nullptr; }
        };

    release(m_matrixBuffer);
    release(m_lightPosBuffer);
    release(m_lightColorBuffer);
    release(m_pbrParamsBuffer);
    release(m_selectedBuffer);
    release(m_directionalLightBuffer);
    release(m_shadowBuffer);        // 🔥 FIX: Добавлено
    release(m_shadowSampler);       // 🔥 FIX: Добавлено

    release(m_defaultNormalSRV);
    release(m_defaultMetalnessSRV);
    release(m_defaultRoughnessSRV);

    ShaderBaseClass::Shutdown();
}


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
    float     selectedFactor)
{
    if (!context) return false;

    // ── Fallback SRV'ы чтобы шейдер не семплил "null" ─────────────────────────
    if (!normalMap)  normalMap = m_defaultNormalSRV;
    if (!metalness)  metalness = m_defaultMetalnessSRV;
    if (!roughness)  roughness = m_defaultRoughnessSRV;

    // ── Подготавливаем и обновляем все константные буферы (матрицы, свет, камера, выделение)
    // (Оставляем существующую логику — здесь предполагается, что ты уже обновляешь буферы выше)
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
    ppb.padding = 0.0f;
    UpdateConstantBuffer(context, m_pbrParamsBuffer, ppb, 6, false);

    SelectedBuffer sb;
    sb.selectedFactor = selectedFactor;
    sb.padding = { 0,0,0 };
    UpdateConstantBuffer(context, m_selectedBuffer, sb, 3, false);

    // ── Подготовка SRV массива (логируем для диагностики)
    ID3D11ShaderResourceView* srvs[4] = { albedo, normalMap, metalness, roughness };
    {
        char buf[256];
        sprintf_s(buf, "PBRShaderClass::Render SRVs before bind: t0=%p t1=%p t2=%p t3=%p\n",
            (void*)srvs[0], (void*)srvs[1], (void*)srvs[2], (void*)srvs[3]);
        OutputDebugStringA(buf);
    }

    // ── Биндим шейдеры (включая sampler) перед отрисовкой
    BindShader(context);

    // Явно ставим сэмплер (на всякий случай, BindShader уже делает это)
    if (m_sampleState)
        context->PSSetSamplers(0, 1, &m_sampleState);

    // ── Устанавливаем SRV после BindShader — это безопаснее и устраняет разные hazard'ы
    context->PSSetShaderResources(0, 4, srvs);

    // Лог — перед DrawIndexed
    {
        char buf[256];
        sprintf_s(buf, "PBRShaderClass::Render -> DrawIndexed indexCount=%d\n", indexCount);
        OutputDebugStringA(buf);
    }

    // Отрисовать
    context->DrawIndexed(indexCount, 0, 0);

    // Очистка слотов после рендера
    ID3D11ShaderResourceView* nullSRVs[4] = { nullptr, nullptr, nullptr, nullptr };
    context->PSSetShaderResources(0, 4, nullSRVs);

    return true;
}

bool PBRShaderClass::SetShadowMap(
    ID3D11DeviceContext* context,
    ID3D11ShaderResourceView* shadowMapSRV,
    const XMMATRIX& lightViewProj)
{
    if (!context || !shadowMapSRV) return false;

    // 1. Привязываем shadow map к слоту t7
    context->PSSetShaderResources(7, 1, &shadowMapSRV);

    // 2. Привязываем comparison sampler к слоту s7
    if (m_shadowSampler)
        context->PSSetSamplers(7, 1, &m_shadowSampler);

    // 3. Обновляем shadow constant buffer (b5)
    D3D11_MAPPED_SUBRESOURCE mapped;
    HRESULT result = context->Map(m_shadowBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if (FAILED(result)) return false;

    ShadowBufferType* data = (ShadowBufferType*)mapped.pData;
    data->lightViewProj = XMMatrixTranspose(lightViewProj);

    context->Unmap(m_shadowBuffer, 0);
    context->PSSetConstantBuffers(5, 1, &m_shadowBuffer);

    return true;
}
