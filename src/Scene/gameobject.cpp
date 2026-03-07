#include "gameobject.h"
#include "Textureshaderclass.h"
#include "Multitextureshaderclass.h"
#include "LightMapShaderClass.h"
#include "NormalMapShaderClass.h"
#include "FogShaderClass.h" // ← ДОБАВИТЬ

GameObject::GameObject()
    : m_model(nullptr),
    m_position(0.0f, 0.0f, 0.0f),
    m_rotation(0.0f, 0.0f, 0.0f),
    m_scale(1.0f, 1.0f, 1.0f)
{
}

GameObject::~GameObject()
{
    m_model = nullptr;
}

void GameObject::Initialize(AssimpModelClass* model, float x, float y, float z)
{
    m_model = model;
    m_position = XMFLOAT3(x, y, z);
    m_rotation = XMFLOAT3(0.0f, 0.0f, 0.0f);
    m_scale = XMFLOAT3(1.0f, 1.0f, 1.0f);
}

void GameObject::Update(float deltaTime)
{
}

void GameObject::SetScale(float x, float y, float z)
{
    m_scale = XMFLOAT3(x, y, z);
}

XMMATRIX GameObject::GetWorldMatrix()
{
    XMMATRIX scaleMatrix = XMMatrixScaling(m_scale.x, m_scale.y, m_scale.z);
    XMMATRIX rotX = XMMatrixRotationX(m_rotation.x);
    XMMATRIX rotY = XMMatrixRotationY(m_rotation.y);
    XMMATRIX rotZ = XMMatrixRotationZ(m_rotation.z);
    XMMATRIX rotationMatrix = rotX * rotY * rotZ;
    XMMATRIX translationMatrix = XMMatrixTranslation(m_position.x, m_position.y, m_position.z);
    return scaleMatrix * rotationMatrix * translationMatrix;
}

void GameObject::Render(
    ID3D11DeviceContext* deviceContext,
    ID3D11Device* device,
    XMMATRIX viewMatrix,
    XMMATRIX projectionMatrix,
    MaterialClass* material,
    XMFLOAT4 lightColors[],
    XMFLOAT4 lightPositions[],
    const Settings::FogSettings& fog,
    bool selected)
{
    if (!m_model || !material) return;

    auto* shader = material->GetShader();
    if (!shader) {
        OutputDebugStringA("DEBUG: GameObject::Render - material->GetShader() == nullptr\n");
        return;
    }

    XMMATRIX worldMatrix = GetWorldMatrix();

    // ── LightShaderClass ─────────────────────────────────────────────────────
    if (auto* lightShader = dynamic_cast<LightShaderClass*>(shader)) {
        auto* texture = material->GetMainTexture();
        ID3D11ShaderResourceView* textureSRV = texture ? texture->GetTexture() : nullptr;

        float selectedFactor = selected ? 2.0f : 1.0f;
        lightShader->SetSelectedFactor(deviceContext, selectedFactor);
        m_model->Render(deviceContext);
        lightShader->Render(deviceContext,
            m_model->GetIndexCount(),
            worldMatrix, viewMatrix, projectionMatrix,
            textureSRV, lightColors, lightPositions);
        return;
    }

    // ── LightMapShaderClass ──────────────────────────────────────────────────
    if (auto* lightMapShader = dynamic_cast<LightMapShaderClass*>(shader)) {
        ID3D11ShaderResourceView* srv0 = nullptr;
        ID3D11ShaderResourceView* srv1 = nullptr;
        if (auto* t0 = material->GetTexture(0)) srv0 = t0->GetTexture();
        if (auto* t1 = material->GetTexture(1)) srv1 = t1->GetTexture();

        m_model->Render(deviceContext);
        if (!lightMapShader->Render(deviceContext,
            m_model->GetIndexCount(),
            worldMatrix, viewMatrix, projectionMatrix,
            srv0, srv1))
        {
            OutputDebugStringA("❌ DEBUG: LightMapShader::Render FAILED\n");
        }
        ID3D11ShaderResourceView* nullSRVs[2] = { nullptr, nullptr };
        deviceContext->PSSetShaderResources(0, 2, nullSRVs);
        return;
    }

    // ── MultiTextureShaderClass ──────────────────────────────────────────────
    if (auto* multiShader = dynamic_cast<MultiTextureShaderClass*>(shader)) {
        ID3D11ShaderResourceView* srv0 = nullptr;
        ID3D11ShaderResourceView* srv1 = nullptr;
        if (auto* t0 = material->GetTexture(0)) srv0 = t0->GetTexture();
        if (auto* t1 = material->GetTexture(1)) srv1 = t1->GetTexture();

        m_model->Render(deviceContext);
        if (!multiShader->Render(deviceContext,
            m_model->GetIndexCount(),
            worldMatrix, viewMatrix, projectionMatrix,
            srv0, srv1))
        {
            OutputDebugStringA("❌ DEBUG: MultiTextureShader::Render FAILED\n");
        }
        ID3D11ShaderResourceView* nullSRVs[2] = { nullptr, nullptr };
        deviceContext->PSSetShaderResources(0, 2, nullSRVs);
        return;
    }

    // ── NormalMapShaderClass ─────────────────────────────────────────────────
    if (auto* normalMapShader = dynamic_cast<NormalMapShaderClass*>(shader)) {
        ID3D11ShaderResourceView* srv0 = nullptr;
        ID3D11ShaderResourceView* srv1 = nullptr;
        if (auto* t0 = material->GetTexture(0)) srv0 = t0->GetTexture();
        if (auto* t1 = material->GetTexture(1)) srv1 = t1->GetTexture();

        XMFLOAT3 lightDir(0.707f, 0.707f, 0.0f);
        XMFLOAT4 diffuseColor(1.0f, 1.0f, 1.0f, 1.0f);

        m_model->Render(deviceContext);
        if (!normalMapShader->Render(deviceContext,
            m_model->GetIndexCount(),
            worldMatrix, viewMatrix, projectionMatrix,
            srv0, srv1,
            lightDir, diffuseColor))
        {
            OutputDebugStringA("❌ DEBUG: NormalMapShader::Render FAILED\n");
        }
        ID3D11ShaderResourceView* nullSRVs[2] = { nullptr, nullptr };
        deviceContext->PSSetShaderResources(0, 2, nullSRVs);
        return;
    }

    // ── FogShaderClass ───────────────────────────────────────────────────────
    if (auto* fogShader = dynamic_cast<FogShaderClass*>(shader)) {
        auto* texture = material->GetMainTexture();
        ID3D11ShaderResourceView* textureSRV = texture ? texture->GetTexture() : nullptr;

        // Если туман выключен — рендерим объект без тумана через textureShader невозможно,
        // просто пропускаем либо рендерим с нулевым туманом (объект виден полностью)
        const float fogStart = fog.enabled ? fog.FOG_START : 10000.0f;
        const float fogEnd = fog.enabled ? fog.FOG_END : 10001.0f;

        m_model->Render(deviceContext);
        if (!fogShader->Render(deviceContext,
            m_model->GetIndexCount(),
            worldMatrix, viewMatrix, projectionMatrix,
            textureSRV, fogStart, fogEnd))
        {
            OutputDebugStringA("❌ DEBUG: FogShader::Render FAILED\n");
        }
        ID3D11ShaderResourceView* nullSRVs[1] = { nullptr };
        deviceContext->PSSetShaderResources(0, 1, nullSRVs);
        return;
    }
}

void GameObject::SetPosition(float x, float y, float z)
{
    m_position = XMFLOAT3(x, y, z);
}

void GameObject::Rotate(float x, float y, float z)
{
    m_rotation.x += x;
    m_rotation.y += y;
    m_rotation.z += z;
}

void GameObject::SetRotation(float x, float y, float z)
{
    m_rotation = XMFLOAT3(x, y, z);
}

void GameObject::RenderSimple(ID3D11DeviceContext* context,
    XMMATRIX view,
    XMMATRIX proj,
    TextureShaderClass* shader)
{
    if (!m_model || !shader) return;
    XMMATRIX world = this->GetWorldMatrix();
    m_model->Render(context);
    shader->Render(context, m_model->GetIndexCount(), world, view, proj, m_model->GetTexture());
}