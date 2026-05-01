#pragma once
#include <d3d11.h>
#include <directxmath.h>
#include "Textureclass.h"

using namespace DirectX;

class SkyboxClass
{
private:
    struct VertexType
    {
        XMFLOAT3 position;
        XMFLOAT2 texture;
    };

public:
    SkyboxClass();
    SkyboxClass(const SkyboxClass&);
    ~SkyboxClass();

    bool Initialize(ID3D11Device*, ID3D11DeviceContext*, 
                    const char* right, const char* left, 
                    const char* top, const char* bottom,
                    const char* front, const char* back);
    void Shutdown();
    void Render(ID3D11DeviceContext* deviceContext);

    ID3D11ShaderResourceView* GetTexture(int index);
    int GetIndexCount() const { return m_indexCount; }  // Для использования в шейдере

private:
    bool InitializeBuffers(ID3D11Device*);
    void ShutdownBuffers();
    void RenderBuffers(ID3D11DeviceContext*);

    bool LoadTextures(ID3D11Device*, ID3D11DeviceContext*,
                      const char* right, const char* left,
                      const char* top, const char* bottom,
                      const char* front, const char* back);
    void ReleaseTextures();

private:
    ID3D11Buffer* m_vertexBuffer;
    ID3D11Buffer* m_indexBuffer;
    int m_vertexCount;
    int m_indexCount;
    TextureClass* m_Textures[6];  // 6 граней куба: right, left, top, bottom, front, back
};
