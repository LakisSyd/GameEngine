#include "gameobject.h"
#include "Textureshaderclass.h"
#include <colorshaderclass.h>

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

// ============================================================================
// RenderSimple — упрощённый рендер только с TextureShader
// Используется для сфер-индикаторов источников света
// ============================================================================
void GameObject::RenderSimple(ID3D11DeviceContext* context,
    XMMATRIX view,
    XMMATRIX proj,
    TextureShaderClass* shader)
{
    if (!m_model || !shader) return;
    XMMATRIX world = GetWorldMatrix();
    m_model->Render(context);
    shader->Render(context, m_model->GetIndexCount(), world, view, proj, m_model->GetTexture());
}
void GameObject::RenderSimpleColor(
    ID3D11DeviceContext* context,
    XMMATRIX view,
    XMMATRIX projection,
    ColorShaderClass* colorShader)
{
    if (!m_model || !colorShader) return;

    XMMATRIX world = GetWorldMatrix(); // или собери из position/rotation/scale

    // ColorShaderClass::Render ожидает: (context, indexCount, world, view, projection)
    colorShader->Render(
        context,
        m_model->GetIndexCount(),
        world,
        view,
        projection
    );
}


// ============================================================================
// GetWorldMatrix: Scale * Rotation * Translation
// ============================================================================
XMMATRIX GameObject::GetWorldMatrix()
{
    XMMATRIX scaleMatrix = XMMatrixScaling(m_scale.x, m_scale.y, m_scale.z);
    XMMATRIX rotationMatrix = XMMatrixRotationX(m_rotation.x)
        * XMMatrixRotationY(m_rotation.y)
        * XMMatrixRotationZ(m_rotation.z);
    XMMATRIX translationMatrix = XMMatrixTranslation(m_position.x, m_position.y, m_position.z);
    return scaleMatrix * rotationMatrix * translationMatrix;
}

// ── Сеттеры ──────────────────────────────────────────────────────────────────
void GameObject::SetPosition(float x, float y, float z) { m_position = XMFLOAT3(x, y, z); }
void GameObject::SetPosition(XMFLOAT3 pos) { m_position = pos; }
void GameObject::SetScale(float x, float y, float z) { m_scale = XMFLOAT3(x, y, z); }
void GameObject::SetScale(XMFLOAT3 s) { m_scale = s; }