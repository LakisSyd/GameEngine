#include "TerrainBlendShaderClass.h"

namespace
{
    ID3D11ShaderResourceView* Create1x1SRV(ID3D11Device* device,
        unsigned char r, unsigned char g, unsigned char b, unsigned char a)
    {
        if (!device) return nullptr;

        unsigned char px[4] = { r, g, b, a };
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = 1;
        desc.Height = 1;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        D3D11_SUBRESOURCE_DATA data{};
        data.pSysMem = px;
        data.SysMemPitch = 4;

        ID3D11Texture2D* texture = nullptr;
        if (FAILED(device->CreateTexture2D(&desc, &data, &texture)))
            return nullptr;

        ID3D11ShaderResourceView* srv = nullptr;
        HRESULT hr = device->CreateShaderResourceView(texture, nullptr, &srv);
        texture->Release();
        return SUCCEEDED(hr) ? srv : nullptr;
    }
}

bool TerrainBlendShaderClass::Initialize(ID3D11Device* device, HWND hwnd)
{
    ID3D10Blob* vsBlob = nullptr;
    ID3D10Blob* psBlob = nullptr;

    if (!CompileShader(L"src/Render/Shader/Vs/TerrainBlend.vs", "TerrainBlendVertexShader", "vs_5_0", &vsBlob))
        return false;
    if (!CompileShader(L"src/Render/Shader/Ps/TerrainBlend.ps", "TerrainBlendPixelShader", "ps_5_0", &psBlob))
    {
        if (vsBlob) vsBlob->Release();
        return false;
    }

    HRESULT hr = device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &m_vertexShader);
    if (FAILED(hr)) { vsBlob->Release(); psBlob->Release(); return false; }

    hr = device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &m_pixelShader);
    if (FAILED(hr)) { vsBlob->Release(); psBlob->Release(); return false; }

    D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 20, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TANGENT",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "BINORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 44, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    if (!CreateInputLayout(device, layout, _countof(layout), vsBlob))
    {
        vsBlob->Release();
        psBlob->Release();
        return false;
    }

    vsBlob->Release();
    psBlob->Release();

    if (!CreateConstantBuffer(device, sizeof(MatrixBuffer), &m_matrixBuffer)) return false;
    if (!CreateConstantBuffer(device, sizeof(TerrainBuffer), &m_terrainBuffer)) return false;
    if (!CreateConstantBuffer(device, sizeof(ShadowBuffer), &m_shadowBuffer)) return false;
    if (!CreateConstantBuffer(device, sizeof(DirectionalLightBuffer), &m_directionalLightBuffer)) return false;
    if (!CreateDefaultSampler(device)) return false;

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
    shadowSamplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
    if (FAILED(device->CreateSamplerState(&shadowSamplerDesc, &m_shadowSampler))) return false;

    m_defaultGrassSRV = Create1x1SRV(device, 80, 120, 45, 255);
    m_defaultDirtSRV = Create1x1SRV(device, 120, 80, 45, 255);
    m_defaultMaskSRV = Create1x1SRV(device, 0, 0, 0, 255);
    m_defaultNormalSRV = Create1x1SRV(device, 128, 128, 255, 255);

    return m_defaultGrassSRV && m_defaultDirtSRV && m_defaultMaskSRV && m_defaultNormalSRV;
}

void TerrainBlendShaderClass::Shutdown()
{
    auto release = [](auto*& ptr) { if (ptr) { ptr->Release(); ptr = nullptr; } };
    release(m_matrixBuffer);
    release(m_terrainBuffer);
    release(m_shadowBuffer);
    release(m_directionalLightBuffer);
    release(m_defaultGrassSRV);
    release(m_defaultDirtSRV);
    release(m_defaultMaskSRV);
    release(m_defaultNormalSRV);
    release(m_shadowSampler);
    m_shadowMapSRV = nullptr;
    ShaderBaseClass::Shutdown();
}

bool TerrainBlendShaderClass::SetDirectionalLight(
    ID3D11DeviceContext* context,
    const XMFLOAT3& direction,
    const XMFLOAT4& color,
    const XMFLOAT4& ambient,
    float intensity)
{
    if (!context || !m_directionalLightBuffer) return false;

    XMFLOAT3 safeDir = direction;
    float lenSq = safeDir.x * safeDir.x + safeDir.y * safeDir.y + safeDir.z * safeDir.z;
    if (lenSq < 0.0001f)
        safeDir = { 0.3f, -1.0f, 0.5f };

    DirectionalLightBuffer data{};
    XMVECTOR dir = XMVector3Normalize(XMLoadFloat3(&safeDir));
    XMStoreFloat3(reinterpret_cast<XMFLOAT3*>(&data.direction), dir);
    data.direction.w = intensity;
    data.color = color;
    data.ambient = ambient;

    UpdateConstantBuffer(context, m_directionalLightBuffer, data, 4, false);
    return true;
}

bool TerrainBlendShaderClass::SetShadowMap(
    ID3D11DeviceContext* context,
    ID3D11ShaderResourceView* shadowMapSRV,
    const XMMATRIX& lightViewProj)
{
    if (!context || !shadowMapSRV || !m_shadowBuffer) return false;

    m_shadowMapSRV = shadowMapSRV;
    ShadowBuffer shadow{};
    shadow.lightViewProj = XMMatrixTranspose(lightViewProj);
    UpdateConstantBuffer(context, m_shadowBuffer, shadow, 5, false);

    context->PSSetShaderResources(7, 1, &m_shadowMapSRV);
    if (m_shadowSampler)
        context->PSSetSamplers(7, 1, &m_shadowSampler);

    return true;
}

bool TerrainBlendShaderClass::Render(
    ID3D11DeviceContext* context,
    int indexCount,
    XMMATRIX world,
    XMMATRIX view,
    XMMATRIX projection,
    ID3D11ShaderResourceView* grassTexture,
    ID3D11ShaderResourceView* dirtTexture,
    ID3D11ShaderResourceView* maskTexture,
    ID3D11ShaderResourceView* grassNormal,
    ID3D11ShaderResourceView* dirtNormal,
    ID3D11ShaderResourceView* grassVariationTexture,
    ID3D11ShaderResourceView* grassVariationNormal,
    float tileScale,
    float normalStrength,
    float variationScale,
    float variationStrength,
    float brightness,
    float ambientBoost,
    XMFLOAT2 terrainMinXZ,
    XMFLOAT2 terrainSizeXZ)
{
    if (!context) return false;

    MatrixBuffer matrices{};
    matrices.world = XMMatrixTranspose(world);
    matrices.view = XMMatrixTranspose(view);
    matrices.projection = XMMatrixTranspose(projection);
    UpdateConstantBuffer(context, m_matrixBuffer, matrices, 0, true);

    TerrainBuffer terrain{};
    terrain.terrainMinXZ = terrainMinXZ;
    terrain.terrainSizeXZ = {
        terrainSizeXZ.x > 0.0001f ? terrainSizeXZ.x : 0.0001f,
        terrainSizeXZ.y > 0.0001f ? terrainSizeXZ.y : 0.0001f
    };
    terrain.tileScale = tileScale > 0.0001f ? tileScale : 0.0001f;
    terrain.normalStrength = normalStrength > 0.0f ? normalStrength : 0.0f;
    terrain.variationScale = variationScale > 0.0001f ? variationScale : 0.0001f;
    terrain.variationStrength = variationStrength > 0.0f ? variationStrength : 0.0f;
    terrain.brightness = brightness > 0.0f ? brightness : 0.0f;
    terrain.ambientBoost = ambientBoost > 0.0f ? ambientBoost : 0.0f;
    terrain.padding = { 0.0f, 0.0f };
    UpdateConstantBuffer(context, m_terrainBuffer, terrain, 0, false);

    BindShader(context);
    if (m_sampleState)
        context->PSSetSamplers(0, 1, &m_sampleState);
    if (m_shadowBuffer)
        context->PSSetConstantBuffers(5, 1, &m_shadowBuffer);
    if (m_directionalLightBuffer)
        context->PSSetConstantBuffers(4, 1, &m_directionalLightBuffer);
    if (m_shadowMapSRV)
        context->PSSetShaderResources(7, 1, &m_shadowMapSRV);
    if (m_shadowSampler)
        context->PSSetSamplers(7, 1, &m_shadowSampler);

    ID3D11ShaderResourceView* srvs[7] =
    {
        grassTexture ? grassTexture : m_defaultGrassSRV,
        dirtTexture ? dirtTexture : m_defaultDirtSRV,
        maskTexture ? maskTexture : m_defaultMaskSRV,
        grassNormal ? grassNormal : m_defaultNormalSRV,
        dirtNormal ? dirtNormal : m_defaultNormalSRV,
        grassVariationTexture ? grassVariationTexture : (grassTexture ? grassTexture : m_defaultGrassSRV),
        grassVariationNormal ? grassVariationNormal : (grassNormal ? grassNormal : m_defaultNormalSRV)
    };
    context->PSSetShaderResources(0, 7, srvs);

    context->DrawIndexed(indexCount, 0, 0);

    ID3D11ShaderResourceView* nullSRVs[8] = {};
    context->PSSetShaderResources(0, 7, nullSRVs);
    context->PSSetShaderResources(7, 1, &nullSRVs[7]);
    return true;
}
