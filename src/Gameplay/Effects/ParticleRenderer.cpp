#include "ParticleRenderer.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iterator>
#include <map>

#include <d3dcompiler.h>

#include "TextureClass.h"

using namespace DirectX;

namespace
{
    template<typename T>
    void SafeRelease(T*& resource)
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
        ID3DBlob** shaderBlob)
    {
        UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
        flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

        ID3DBlob* errorBlob = nullptr;
        const HRESULT result = D3DCompileFromFile(
            path,
            nullptr,
            D3D_COMPILE_STANDARD_FILE_INCLUDE,
            entryPoint,
            profile,
            flags,
            0,
            shaderBlob,
            &errorBlob);

        if (FAILED(result))
        {
            if (errorBlob)
                OutputDebugStringA(static_cast<const char*>(errorBlob->GetBufferPointer()));
            else
                OutputDebugStringW(path);
        }

        SafeRelease(errorBlob);
        return SUCCEEDED(result);
    }

    float DistanceSquared(const XMFLOAT3& left, const XMFLOAT3& right)
    {
        const float x = left.x - right.x;
        const float y = left.y - right.y;
        const float z = left.z - right.z;
        return x * x + y * y + z * z;
    }
}

ParticleRenderer::~ParticleRenderer()
{
    Shutdown();
}

bool ParticleRenderer::Initialize(ID3D11Device* device)
{
    if (!device)
        return false;

    Shutdown();
    m_cpuVertices.reserve(ParticleSystem::MAX_PARTICLES * 6);
    return CreateShaders(device) && CreatePipelineStates(device);
}

void ParticleRenderer::Shutdown()
{
    ClearTextureCache();
    SafeRelease(m_textureSampler);
    SafeRelease(m_rasterizerState);
    SafeRelease(m_depthReadState);
    SafeRelease(m_additiveBlendState);
    SafeRelease(m_alphaBlendState);
    SafeRelease(m_matrixBuffer);
    SafeRelease(m_vertexBuffer);
    SafeRelease(m_inputLayout);
    SafeRelease(m_pixelShader);
    SafeRelease(m_vertexShader);
    m_cpuVertices.clear();
    m_lastRenderedParticleCount = 0;
    m_lastDrawCallCount = 0;
}

void ParticleRenderer::ClearTextureCache()
{
    for (auto& entry : m_textureCache)
        SafeRelease(entry.second);
    m_textureCache.clear();
    m_failedTexturePaths.clear();
}

void ParticleRenderer::Render(
    ID3D11DeviceContext* context,
    const XMMATRIX& view,
    const XMMATRIX& projection,
    const XMFLOAT3& cameraPosition,
    const std::vector<ParticleRenderItem>& particles)
{
    m_lastRenderedParticleCount = 0;
    m_lastDrawCallCount = 0;

    if (!context || particles.empty() || !m_vertexShader || !m_pixelShader)
        return;

    std::vector<const ParticleRenderItem*> alphaParticles;
    std::vector<const ParticleRenderItem*> additiveParticles;
    alphaParticles.reserve(particles.size());
    additiveParticles.reserve(particles.size());

    for (const ParticleRenderItem& particle : particles)
    {
        if (particle.color.w <= 0.001f || particle.size <= 0.001f)
            continue;

        if (particle.blendMode == ParticleBlendMode::Alpha)
            alphaParticles.push_back(&particle);
        else
            additiveParticles.push_back(&particle);
    }

    // Только обычная прозрачность требует сортировки от дальних частиц к ближним.
    std::sort(
        alphaParticles.begin(),
        alphaParticles.end(),
        [&cameraPosition](const ParticleRenderItem* left, const ParticleRenderItem* right)
        {
            return DistanceSquared(left->position, cameraPosition) >
                DistanceSquared(right->position, cameraPosition);
        });

    XMMATRIX inverseView = XMMatrixInverse(nullptr, view);
    XMFLOAT3 cameraRight{};
    XMFLOAT3 cameraUp{};
    XMStoreFloat3(&cameraRight, XMVector3Normalize(inverseView.r[0]));
    XMStoreFloat3(&cameraUp, XMVector3Normalize(inverseView.r[1]));

    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(context->Map(m_matrixBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
        return;

    auto* matrices = static_cast<MatrixBuffer*>(mapped.pData);
    matrices->view = XMMatrixTranspose(view);
    matrices->projection = XMMatrixTranspose(projection);
    context->Unmap(m_matrixBuffer, 0);

    ID3D11BlendState* oldBlendState = nullptr;
    ID3D11DepthStencilState* oldDepthState = nullptr;
    ID3D11RasterizerState* oldRasterizerState = nullptr;
    FLOAT oldBlendFactor[4]{};
    UINT oldSampleMask = 0;
    UINT oldStencilReference = 0;
    context->OMGetBlendState(&oldBlendState, oldBlendFactor, &oldSampleMask);
    context->OMGetDepthStencilState(&oldDepthState, &oldStencilReference);
    context->RSGetState(&oldRasterizerState);

    const UINT stride = sizeof(Vertex);
    const UINT offset = 0;
    context->IASetInputLayout(m_inputLayout);
    context->IASetVertexBuffers(0, 1, &m_vertexBuffer, &stride, &offset);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->VSSetShader(m_vertexShader, nullptr, 0);
    context->VSSetConstantBuffers(0, 1, &m_matrixBuffer);
    context->PSSetShader(m_pixelShader, nullptr, 0);
    context->OMSetDepthStencilState(m_depthReadState, 0);
    context->RSSetState(m_rasterizerState);

    ID3D11ShaderResourceView* oldTexture = nullptr;
    ID3D11SamplerState* oldSampler = nullptr;
    context->PSGetShaderResources(0, 1, &oldTexture);
    context->PSGetSamplers(0, 1, &oldSampler);

    RenderMaterialBatches(context, alphaParticles, cameraRight, cameraUp, m_alphaBlendState);
    RenderMaterialBatches(context, additiveParticles, cameraRight, cameraUp, m_additiveBlendState);

    context->PSSetShaderResources(0, 1, &oldTexture);
    context->PSSetSamplers(0, 1, &oldSampler);

    context->OMSetBlendState(oldBlendState, oldBlendFactor, oldSampleMask);
    context->OMSetDepthStencilState(oldDepthState, oldStencilReference);
    context->RSSetState(oldRasterizerState);

    SafeRelease(oldRasterizerState);
    SafeRelease(oldDepthState);
    SafeRelease(oldBlendState);
    SafeRelease(oldSampler);
    SafeRelease(oldTexture);
}

bool ParticleRenderer::CreateShaders(ID3D11Device* device)
{
    ID3DBlob* vertexBlob = nullptr;
    ID3DBlob* pixelBlob = nullptr;

    if (!CompileShader(
        L"src/Render/Shader/Vs/particle.vs",
        "ParticleVertexShader",
        "vs_5_0",
        &vertexBlob))
    {
        return false;
    }

    if (!CompileShader(
        L"src/Render/Shader/Ps/particle.ps",
        "ParticlePixelShader",
        "ps_5_0",
        &pixelBlob))
    {
        SafeRelease(vertexBlob);
        return false;
    }

    HRESULT result = device->CreateVertexShader(
        vertexBlob->GetBufferPointer(),
        vertexBlob->GetBufferSize(),
        nullptr,
        &m_vertexShader);
    if (FAILED(result))
    {
        SafeRelease(pixelBlob);
        SafeRelease(vertexBlob);
        return false;
    }

    result = device->CreatePixelShader(
        pixelBlob->GetBufferPointer(),
        pixelBlob->GetBufferSize(),
        nullptr,
        &m_pixelShader);
    if (FAILED(result))
    {
        SafeRelease(pixelBlob);
        SafeRelease(vertexBlob);
        return false;
    }

    const D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
            D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12,
            D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 20,
            D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT, 0, 36,
            D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };

    result = device->CreateInputLayout(
        layout,
        static_cast<UINT>(std::size(layout)),
        vertexBlob->GetBufferPointer(),
        vertexBlob->GetBufferSize(),
        &m_inputLayout);

    SafeRelease(pixelBlob);
    SafeRelease(vertexBlob);
    return SUCCEEDED(result);
}

bool ParticleRenderer::CreatePipelineStates(ID3D11Device* device)
{
    D3D11_BUFFER_DESC vertexBufferDesc{};
    vertexBufferDesc.ByteWidth = static_cast<UINT>(
        ParticleSystem::MAX_PARTICLES * 6 * sizeof(Vertex));
    vertexBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vertexBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    if (FAILED(device->CreateBuffer(&vertexBufferDesc, nullptr, &m_vertexBuffer)))
        return false;

    D3D11_BUFFER_DESC matrixBufferDesc{};
    matrixBufferDesc.ByteWidth = sizeof(MatrixBuffer);
    matrixBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    matrixBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    matrixBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    if (FAILED(device->CreateBuffer(&matrixBufferDesc, nullptr, &m_matrixBuffer)))
        return false;

    D3D11_BLEND_DESC alphaBlend{};
    alphaBlend.RenderTarget[0].BlendEnable = TRUE;
    alphaBlend.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    alphaBlend.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    alphaBlend.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    alphaBlend.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    alphaBlend.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    alphaBlend.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    alphaBlend.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    if (FAILED(device->CreateBlendState(&alphaBlend, &m_alphaBlendState)))
        return false;

    D3D11_BLEND_DESC additiveBlend = alphaBlend;
    additiveBlend.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
    additiveBlend.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
    if (FAILED(device->CreateBlendState(&additiveBlend, &m_additiveBlendState)))
        return false;

    D3D11_DEPTH_STENCIL_DESC depthDesc{};
    depthDesc.DepthEnable = TRUE;
    depthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    depthDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
    if (FAILED(device->CreateDepthStencilState(&depthDesc, &m_depthReadState)))
        return false;

    D3D11_RASTERIZER_DESC rasterizerDesc{};
    rasterizerDesc.FillMode = D3D11_FILL_SOLID;
    rasterizerDesc.CullMode = D3D11_CULL_NONE;
    rasterizerDesc.DepthClipEnable = TRUE;
    if (FAILED(device->CreateRasterizerState(&rasterizerDesc, &m_rasterizerState)))
        return false;

    D3D11_SAMPLER_DESC samplerDesc{};
    samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    samplerDesc.MinLOD = 0.0f;
    samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
    return SUCCEEDED(device->CreateSamplerState(&samplerDesc, &m_textureSampler));
}

void ParticleRenderer::RenderMaterialBatches(
    ID3D11DeviceContext* context,
    const std::vector<const ParticleRenderItem*>& particles,
    const XMFLOAT3& cameraRight,
    const XMFLOAT3& cameraUp,
    ID3D11BlendState* blendState)
{
    // One draw call per texture. Within every alpha texture the original
    // far-to-near order is preserved.
    std::map<std::string, std::vector<const ParticleRenderItem*>> batches;
    for (const ParticleRenderItem* particle : particles)
        batches[particle->texturePath].push_back(particle);

    for (const auto& batch : batches)
        RenderBatch(context, batch.second, cameraRight, cameraUp, blendState, batch.first);
}

bool ParticleRenderer::RenderBatch(
    ID3D11DeviceContext* context,
    const std::vector<const ParticleRenderItem*>& particles,
    const XMFLOAT3& cameraRight,
    const XMFLOAT3& cameraUp,
    ID3D11BlendState* blendState,
    const std::string& texturePath)
{
    if (particles.empty())
        return true;

    ID3D11ShaderResourceView* texture = nullptr;
    if (!texturePath.empty())
    {
        texture = ResolveTexture(context, texturePath);
        if (!texture)
            return false;
    }

    context->PSSetShaderResources(0, 1, &texture);
    context->PSSetSamplers(0, 1, &m_textureSampler);

    m_cpuVertices.clear();
    for (const ParticleRenderItem* particle : particles)
        AppendBillboard(*particle, cameraRight, cameraUp);

    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(context->Map(m_vertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
        return false;

    std::memcpy(
        mapped.pData,
        m_cpuVertices.data(),
        m_cpuVertices.size() * sizeof(Vertex));
    context->Unmap(m_vertexBuffer, 0);

    const FLOAT blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    context->OMSetBlendState(blendState, blendFactor, 0xFFFFFFFF);
    context->Draw(static_cast<UINT>(m_cpuVertices.size()), 0);
    m_lastRenderedParticleCount += particles.size();
    ++m_lastDrawCallCount;
    return true;
}

ID3D11ShaderResourceView* ParticleRenderer::ResolveTexture(
    ID3D11DeviceContext* context,
    const std::string& texturePath)
{
    const auto cached = m_textureCache.find(texturePath);
    if (cached != m_textureCache.end())
        return cached->second;
    if (m_failedTexturePaths.find(texturePath) != m_failedTexturePaths.end())
        return nullptr;

    ID3D11Device* device = nullptr;
    context->GetDevice(&device);
    if (!device)
        return nullptr;

    TextureClass texture;
    const bool loaded = texture.Initialize(device, context, texturePath.c_str(), true);
    SafeRelease(device);
    if (!loaded || !texture.GetTexture())
    {
        m_failedTexturePaths.insert(texturePath);
        const std::string message = "[Particles] Cannot load texture: " + texturePath + "\n";
        OutputDebugStringA(message.c_str());
        return nullptr;
    }

    ID3D11ShaderResourceView* view = texture.GetTexture();
    view->AddRef();
    m_textureCache.emplace(texturePath, view);
    return view;
}

void ParticleRenderer::AppendBillboard(
    const ParticleRenderItem& particle,
    const XMFLOAT3& cameraRight,
    const XMFLOAT3& cameraUp)
{
    const float cosine = std::cos(particle.rotation);
    const float sine = std::sin(particle.rotation);

    XMFLOAT3 right = {
        cameraRight.x * cosine + cameraUp.x * sine,
        cameraRight.y * cosine + cameraUp.y * sine,
        cameraRight.z * cosine + cameraUp.z * sine
    };
    XMFLOAT3 up = {
        cameraUp.x * cosine - cameraRight.x * sine,
        cameraUp.y * cosine - cameraRight.y * sine,
        cameraUp.z * cosine - cameraRight.z * sine
    };

    const float halfWidth = particle.size * particle.aspectRatio * 0.5f;
    const float halfHeight = particle.size * 0.5f;
    right = { right.x * halfWidth, right.y * halfWidth, right.z * halfWidth };
    up = { up.x * halfHeight, up.y * halfHeight, up.z * halfHeight };

    const auto corner = [&](float rightSign, float upSign)
    {
        return XMFLOAT3{
            particle.position.x + right.x * rightSign + up.x * upSign,
            particle.position.y + right.y * rightSign + up.y * upSign,
            particle.position.z + right.z * rightSign + up.z * upSign
        };
    };

    const XMFLOAT2 parameters = {
        particle.normalizedAge,
        static_cast<float>(particle.shape)
    };
    const XMFLOAT3 topLeft = corner(-1.0f, 1.0f);
    const XMFLOAT3 topRight = corner(1.0f, 1.0f);
    const XMFLOAT3 bottomLeft = corner(-1.0f, -1.0f);
    const XMFLOAT3 bottomRight = corner(1.0f, -1.0f);

    m_cpuVertices.push_back({ topLeft, particle.uvMin, particle.color, parameters });
    m_cpuVertices.push_back({ topRight, { particle.uvMax.x, particle.uvMin.y }, particle.color, parameters });
    m_cpuVertices.push_back({ bottomLeft, { particle.uvMin.x, particle.uvMax.y }, particle.color, parameters });
    m_cpuVertices.push_back({ bottomLeft, { particle.uvMin.x, particle.uvMax.y }, particle.color, parameters });
    m_cpuVertices.push_back({ topRight, { particle.uvMax.x, particle.uvMin.y }, particle.color, parameters });
    m_cpuVertices.push_back({ bottomRight, particle.uvMax, particle.color, parameters });
}
