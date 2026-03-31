#pragma once
#include <DirectXMath.h>
#include "assimpmodelclass.h"
#include "colorshaderclass.h"
class TextureShaderClass;
using namespace DirectX;

// ============================================================================
// GameObject — легковесная обёртка для визуальных индикаторов (light-сферы).
// НЕ является частью ECS. Вся игровая логика — в ECS (TransformComponent и т.д.)
// ============================================================================
class GameObject
{
public:
    GameObject();
    ~GameObject();

    void Initialize(AssimpModelClass* model, float x, float y, float z);

    // Упрощённый рендер с TextureShader (для сфер-индикаторов света)
    void RenderSimple(ID3D11DeviceContext* context,
        XMMATRIX view,
        XMMATRIX proj,
        TextureShaderClass* shader);

    void RenderSimpleColor(
        ID3D11DeviceContext* context,
        XMMATRIX view,
        XMMATRIX projection,
        ColorShaderClass* colorShader);

    XMMATRIX GetWorldMatrix();

    // ── Сеттеры / геттеры ────────────────────────────────────────────────────
    void SetPosition(float x, float y, float z);
    void SetPosition(XMFLOAT3 pos);
    void SetScale(float x, float y, float z);
    void SetScale(XMFLOAT3 s);
    void SetRotation(float x, float y, float z)
    {
		m_rotation = { x, y, z };
    }

    XMFLOAT3 GetPosition() const { return m_position; }
    XMFLOAT3 GetScale()    const { return m_scale; }

private:
    AssimpModelClass* m_model = nullptr;
    XMFLOAT3 m_position = { 0, 0, 0 };
    XMFLOAT3 m_rotation = { 0, 0, 0 };
    XMFLOAT3 m_scale = { 1, 1, 1 };
};