#pragma once
#include <DirectXMath.h>
#include "assimpmodelclass.h"
#include "materialclass.h"  // ← БЫЛО: #include "lightshaderclass.h" и "textureshaderclass.h"
#include "GameSettings.h"
class TextureShaderClass;
using namespace DirectX;

#ifndef NUM_LIGHTS
#define NUM_LIGHTS 4
#endif

class GameObject
{
private:
    AssimpModelClass* m_model;
    MaterialClass* m_material;  // ← НОВОЕ: вместо прямого указателя на шейдер
    XMFLOAT3 m_position;
    XMFLOAT3 m_rotation;
    XMFLOAT3 m_scale;
    float m_boundingRadius;

public:
    GameObject();
    ~GameObject();

    void Initialize(AssimpModelClass* model, float x, float y, float z);
    void Update(float deltaTime);

    void Render(
        ID3D11DeviceContext* deviceContext,
        ID3D11Device* device,
        XMMATRIX viewMatrix,
        XMMATRIX projectionMatrix,
        MaterialClass* material,
        XMFLOAT4 lightColors[],
        XMFLOAT4 lightPositions[],
        const Settings::FogSettings& fog,
        bool selected = false
		
    );

    // 🔥 RenderSimple: оставляем старую сигнатуру для спрайтов!
    void RenderSimple(ID3D11DeviceContext* context,
        XMMATRIX view,
        XMMATRIX proj,
        TextureShaderClass* shader);  // ← Оставляем TextureShaderClass*

    void SetScale(float x, float y, float z);
    float GetBoundingRadius() const { return m_boundingRadius; }
    void SetBoundingRadius(float radius) { m_boundingRadius = radius; }

    XMMATRIX GetWorldMatrix();

    // 🔥 Render: теперь принимает MaterialClass* и массивы света
  
    // 🔥 Геттеры/сеттеры для материала
    void SetMaterial(MaterialClass* material) { m_material = material; }
    MaterialClass* GetMaterial() const { return m_material; }

    void SetPosition(float x, float y, float z);
    void Rotate(float x, float y, float z);
    void SetRotation(float x, float y, float z);
    XMFLOAT3 GetPosition() const { return m_position; }
};