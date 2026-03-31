#pragma once
#include <d3d11.h>
#include <DirectXMath.h>
#include "ECSRegistry .h"
#include "colorshaderclass.h"

using namespace DirectX;

class CompartmentDebugRenderer
{
public:
    CompartmentDebugRenderer();
    ~CompartmentDebugRenderer();

    bool Initialize(ID3D11Device* device);
    void Shutdown();

    // Рендерит все отсеки из ECS
    void RenderCompartments(
        ECSRegistry& registry,
        ID3D11DeviceContext* context,
        const XMMATRIX& view,
        const XMMATRIX& projection,
        ColorShaderClass* shader);

private:
    // Вершина куба с цветом
    struct ColorVertex
    {
        XMFLOAT3 position;
        XMFLOAT4 color;
    };

    void CreateCubeBuffers(ID3D11Device* device);

    ID3D11Buffer* m_vertexBuffer = nullptr;
    ID3D11Buffer* m_indexBuffer = nullptr;
    int m_indexCount = 0;
};
