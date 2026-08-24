#pragma once
#include <d3d11.h>
#include <directxmath.h>
#include "Textureclass.h"

using namespace DirectX;

////////////////////////////////////////////////////////////////////////////////
// Class name: ModelClass
////////////////////////////////////////////////////////////////////////////////
class ModelClass
{
private:
    // Определение типа вершины. Должно совпадать с layout в ColorShaderClass и структурой в шейдере.
    struct VertexType
    {
        XMFLOAT3 position;
        XMFLOAT2 texture;
        XMFLOAT3 normal;
    };

    struct ModelType
    {
        float x, y, z;
        float tu, tv;
        float nx, ny, nz;
    };

public:
    ModelClass();
    ModelClass(const ModelClass&) = delete;
    ModelClass& operator=(const ModelClass&) = delete;
    ~ModelClass();

    bool Initialize(ID3D11Device*, ID3D11DeviceContext*, const char* tex1, const char* tex2);
    void Shutdown();
    void Render(ID3D11DeviceContext*);
    int GetIndexCount();

    ID3D11ShaderResourceView* GetTexture(int index);

private:
    bool InitializeBuffers(ID3D11Device*);
    void ShutdownBuffers();
    void RenderBuffers(ID3D11DeviceContext*);

    bool LoadTextures(ID3D11Device*, ID3D11DeviceContext*, const char* tex1, const char* tex2);
    void ReleaseTextures();

private:
    ID3D11Buffer* m_vertexBuffer, * m_indexBuffer;
    int m_vertexCount, m_indexCount;
    TextureClass* m_Textures[2];  // 🔥 Массив из 2 текстур
};
