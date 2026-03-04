#pragma once
#include <DirectXMath.h>
#include "Textureclass.h"
#include "Textureshaderclass.h"
using namespace DirectX;

class WorldSpriteClass {
    struct VertexType
    {
        XMFLOAT3 position;
        XMFLOAT2 texture;
    };

public:
    WorldSpriteClass();
    ~WorldSpriteClass();

    bool Initialize(ID3D11Device* device, ID3D11DeviceContext* deviceContext,
        const char* textureFilename, float width, float height);
    void Shutdown();

    void SetPosition(float x, float y, float z);
    void SetColor(XMFLOAT4 color) { m_color = color; }

    bool Render(ID3D11DeviceContext* context,  XMMATRIX view,
        XMMATRIX projection, TextureShaderClass* texture, XMMATRIX* customWorldMatrix = nullptr);

    XMFLOAT3 GetPosition() const { return m_position; }
    XMFLOAT2 GetScale() const { return { m_width, m_height }; }
    float GetRotation() const { return m_rotation; }

    // === Сеттеры с автоматической пометкой "dirty" ===
    
  

    void SetScale(float width, float height);
    void SetRotation(float radians);
    void AddRotation(float radians); // Удобно для анимации: поворот относительно текущего

    // === Комбинированный метод для быстрой настройки ===
    void SetTransform(float x, float y, float z, float width, float height, float rotationRad = 0.0f);

    XMMATRIX BuildWorldMatrix() const;

private:
    bool InitializeBuffers(ID3D11Device* device, float width, float height);
    void Move(float dx, float dy, float dz);
    void ShutdownBuffers();
    void RenderBuffers(ID3D11DeviceContext* context);

private:
    XMFLOAT3 m_position;
    XMFLOAT4 m_color;
    float m_rotation;
    TextureClass* m_Texture;
    ID3D11Buffer* m_vertexBuffer, * m_indexBuffer;
    int m_vertexCount, m_indexCount;
    float m_width, m_height;  // Размер спрайта в мировых единицах
    mutable XMMATRIX m_cachedWorldMatrix;
    mutable bool m_isMatrixDirty = true;


    void MarkMatrixDirty() { m_isMatrixDirty = true; }
};