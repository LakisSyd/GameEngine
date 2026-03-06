#include "gameobject.h"
#include "Textureshaderclass.h"
#include "Multitextureshaderclass.h" // ← добавлено
#include "LightMapShaderClass.h"
#include "../Render/Shader/NormalMapShaderClass.h" // ← ДОБАВИТЬ

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
    // Логика движения (пока пустая)
}

void GameObject::SetScale(float x, float y, float z)
{
    m_scale = XMFLOAT3(x, y, z);
}

// 🔥 РЕАЛИЗАЦИЯ НОВОГО МЕТОДА
XMMATRIX GameObject::GetWorldMatrix()
{
    XMMATRIX scaleMatrix = XMMatrixScaling(m_scale.x, m_scale.y, m_scale.z);
    XMMATRIX rotX = XMMatrixRotationX(m_rotation.x);
    XMMATRIX rotY = XMMatrixRotationY(m_rotation.y);
    XMMATRIX rotZ = XMMatrixRotationZ(m_rotation.z);
    XMMATRIX rotationMatrix = rotX * rotY * rotZ;
    XMMATRIX translationMatrix = XMMatrixTranslation(m_position.x, m_position.y, m_position.z);

    // Порядок: Масштаб -> Вращение -> Перемещение
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
    bool selected)
{
    if (!m_model || !material) return;

    // DEBUG: покажем указатель на материал и на шейдер
    {
        char buf[256];
        sprintf_s(buf, "DEBUG: GameObject::Render - material=%p shader=%p\n", (void*)material, (void*)material->GetShader());
        OutputDebugStringA(buf);
    }

    // Получаем шейдер из материала
    auto* shader = material->GetShader();
    if (!shader) {
        OutputDebugStringA("DEBUG: GameObject::Render - material->GetShader() == nullptr\n");
        return;
    }

    if (material) {
        ID3D11ShaderResourceView* debugSrv0 = nullptr;
        ID3D11ShaderResourceView* debugSrv1 = nullptr;
        auto* t0 = material->GetTexture(0);
        auto* t1 = material->GetTexture(1);
        if (t0) debugSrv0 = t0->GetTexture();
        if (t1) debugSrv1 = t1->GetTexture();
        char bufDbg[256];
        sprintf_s(bufDbg, "DEBUG: material=%p tex0=%p tex1=%p\n", (void*)material, (void*)debugSrv0, (void*)debugSrv1);
        OutputDebugStringA(bufDbg);
    }

    XMMATRIX worldMatrix = GetWorldMatrix();

    // Попытка привести к LightShaderClass
    if (auto* lightShader = dynamic_cast<LightShaderClass*>(shader)) {
        OutputDebugStringA("DEBUG: GameObject::Render - using LightShaderClass\n");

        // Привязываем текстуру из материала (если есть)
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

    if (auto* lightMapShader = dynamic_cast<LightMapShaderClass*>(shader)) {
        OutputDebugStringA("DEBUG: GameObject::Render - using LightMapShaderClass\n");

        ID3D11ShaderResourceView* srv0 = nullptr;
        ID3D11ShaderResourceView* srv1 = nullptr;

        auto* t0 = material->GetTexture(0);
        if (t0) srv0 = t0->GetTexture();

        auto* t1 = material->GetTexture(1);
        if (t1) srv1 = t1->GetTexture();

        char buf[256];
        sprintf_s(buf, "DEBUG: LightMapShader - srv0=%p srv1=%p\n", (void*)srv0, (void*)srv1);
        OutputDebugStringA(buf);

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

    // Попытка привести к MultiTextureShaderClass
    if (auto* multiShader = dynamic_cast<MultiTextureShaderClass*>(shader)) {
        OutputDebugStringA("DEBUG: GameObject::Render - using MultiTextureShaderClass\n");

        ID3D11ShaderResourceView* srv0 = nullptr;
        ID3D11ShaderResourceView* srv1 = nullptr;

        auto* t0 = material->GetTexture(0);
        if (t0) srv0 = t0->GetTexture();

        auto* t1 = material->GetTexture(1);
        if (t1) srv1 = t1->GetTexture();

        {
            char buf[256];
            sprintf_s(buf, "DEBUG: GameObject::Render - srv0=%p srv1=%p\n", (void*)srv0, (void*)srv1);
            OutputDebugStringA(buf);
        }

        m_model->Render(deviceContext);

        if (!multiShader->Render(deviceContext,
            m_model->GetIndexCount(),
            worldMatrix, viewMatrix, projectionMatrix,
            srv0, srv1))
        {
            OutputDebugStringA("❌ DEBUG: MultiTextureShader::Render FAILED\n");
        }

        if (material) {
        ID3D11ShaderResourceView* debugSrv0 = nullptr;
        ID3D11ShaderResourceView* debugSrv1 = nullptr;
        auto* t0 = material->GetTexture(0);
        auto* t1 = material->GetTexture(1);
        if (t0) debugSrv0 = t0->GetTexture();
        if (t1) debugSrv1 = t1->GetTexture();
        char bufDbg[256];
        sprintf_s(bufDbg, "DEBUG: material=%p tex0=%p tex1=%p\n", (void*)material, (void*)debugSrv0, (void*)debugSrv1);
        OutputDebugStringA(bufDbg);
    }

        // Очистка слотов
        ID3D11ShaderResourceView* nullSRVs[2] = { nullptr, nullptr };
        deviceContext->PSSetShaderResources(0, 2, nullSRVs);
        return;
    }

    // Попытка привести к NormalMapShaderClass
    if (auto* normalMapShader = dynamic_cast<NormalMapShaderClass*>(shader)) {
        OutputDebugStringA("DEBUG: GameObject::Render - using NormalMapShaderClass\n");

        ID3D11ShaderResourceView* srv0 = nullptr;
        ID3D11ShaderResourceView* srv1 = nullptr;

        auto* t0 = material->GetTexture(0);
        if (t0) srv0 = t0->GetTexture();

        auto* t1 = material->GetTexture(1);
        if (t1) srv1 = t1->GetTexture();

        {
            char buf[256];
            sprintf_s(buf, "DEBUG: GameObject::Render - srv0=%p srv1=%p\n", (void*)srv0, (void*)srv1);
            OutputDebugStringA(buf);
        }

        m_model->Render(deviceContext);

        // 🔥 ИСПРАВЛЕНО: добавлены параметры освещения
        XMFLOAT3 lightDir(0.707f, 0.707f, 0.0f);  // Направление света (нормализовано)
        XMFLOAT4 diffuseColor(1.0f, 1.0f, 1.0f, 1.0f);  // Белый свет

        if (!normalMapShader->Render(deviceContext,
            m_model->GetIndexCount(),
            worldMatrix, viewMatrix, projectionMatrix,
            srv0, srv1,
            lightDir, diffuseColor))
        {
            OutputDebugStringA("❌ DEBUG: NormalMapShader::Render FAILED\n");
        }

        if (material) {
            ID3D11ShaderResourceView* debugSrv0 = nullptr;
            ID3D11ShaderResourceView* debugSrv1 = nullptr;
            auto* t0 = material->GetTexture(0);
            auto* t1 = material->GetTexture(1);
            if (t0) debugSrv0 = t0->GetTexture();
            if (t1) debugSrv1 = t1->GetTexture();
            char bufDbg[256];
            sprintf_s(bufDbg, "DEBUG: material=%p tex0=%p tex1=%p\n", (void*)material, (void*)debugSrv0, (void*)debugSrv1);
            OutputDebugStringA(bufDbg);
        }

        // Очистка слотов
        ID3D11ShaderResourceView* nullSRVs[2] = { nullptr, nullptr };
        deviceContext->PSSetShaderResources(0, 2, nullSRVs);
        return;
    }

    OutputDebugStringA("DEBUG: GameObject::Render - unsupported shader type\n");
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