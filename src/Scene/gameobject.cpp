#include "gameobject.h"
#include "Textureshaderclass.h"
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
    MaterialClass* material,  // ← Теперь MaterialClass*
    XMFLOAT4 lightColors[],
    XMFLOAT4 lightPositions[],
    bool selected)
{
    if (!m_model || !material) return;

    // Получаем шейдер из материала
    auto* shader = material->GetShader();
    if (!shader) return;

    XMMATRIX worldMatrix = GetWorldMatrix();

    // 🔥 Привязываем текстуру из материала (если есть)
    auto* texture = material->GetMainTexture();
    ID3D11ShaderResourceView* textureSRV = texture ? texture->GetTexture() : nullptr;

    // 🔥 Вызываем рендер через dynamic_cast
    auto* lightShader = dynamic_cast<LightShaderClass*>(shader);
    if (!lightShader) {
        OutputDebugStringA("❌ ERROR: Material's shader is NOT LightShaderClass!\n");
        OutputDebugStringA("   Object will NOT be rendered with lighting.\n");
        return;  // Или fallback на простой рендер
    }

    // Теперь используем lightShader
    float selectedFactor = selected ? 2.0f : 1.0f;
    lightShader->SetSelectedFactor(deviceContext, selectedFactor);
    m_model->Render(deviceContext);
    lightShader->Render(deviceContext,
        m_model->GetIndexCount(),
        worldMatrix, viewMatrix, projectionMatrix,
        textureSRV, lightColors, lightPositions);
    // 🔥 Можно добавить поддержку других шейдеров в будущем:
    // else if (auto* pbrShader = dynamic_cast<PBRShaderClass*>(shader)) { ... }
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