#pragma once
#include <d3d11.h>
#include <iterator>
#include "WorldAreaComponent.h"
#include "colorshaderclass.h"

struct Vertex
{
    XMFLOAT3 position;
    XMFLOAT4 color;
};

class AreaDebugRenderer
{
public:
    bool Initialize(ID3D11Device* device)
    {
        Vertex vertices[] =
        {
            {{-0.5f, -0.5f, -0.5f}, {0, 1, 0, 1}},
            {{ 0.5f, -0.5f, -0.5f}, {0, 1, 0, 1}},
            {{ 0.5f, -0.5f,  0.5f}, {0, 1, 0, 1}},
            {{-0.5f, -0.5f,  0.5f}, {0, 1, 0, 1}},
            {{-0.5f,  0.5f, -0.5f}, {0, 1, 0, 1}},
            {{ 0.5f,  0.5f, -0.5f}, {0, 1, 0, 1}},
            {{ 0.5f,  0.5f,  0.5f}, {0, 1, 0, 1}},
            {{-0.5f,  0.5f,  0.5f}, {0, 1, 0, 1}}
        };


        uint32_t indices[] =
        {
            0, 1,  1, 2,  2, 3,  3, 0,
            4, 5,  5, 6,  6, 7,  7, 4,
            0, 4,  1, 5,  2, 6,  3, 7
        };

        m_indexCount = static_cast<int>(std::size(indices));


        D3D11_BUFFER_DESC vertexDesc{};
        vertexDesc.Usage = D3D11_USAGE_DEFAULT;
        vertexDesc.ByteWidth = sizeof(vertices);
        vertexDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

        D3D11_SUBRESOURCE_DATA vertexData{};
        vertexData.pSysMem = vertices;

        HRESULT hr = device->CreateBuffer(
            &vertexDesc,
            &vertexData,
            &m_vertexBuffer);

        if (FAILED(hr))
            return false;


        D3D11_BUFFER_DESC indexDesc{};
        indexDesc.Usage = D3D11_USAGE_DEFAULT;
        indexDesc.ByteWidth = sizeof(indices);
        indexDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

        D3D11_SUBRESOURCE_DATA indexData{};
        indexData.pSysMem = indices;

        hr = device->CreateBuffer(
            &indexDesc,
            &indexData,
            &m_indexBuffer);

        return SUCCEEDED(hr);

    }


    void Shutdown()
    {
        if (m_indexBuffer)
        {
            m_indexBuffer->Release();
            m_indexBuffer = nullptr;
        }

        if (m_vertexBuffer)
        {
            m_vertexBuffer->Release();
            m_vertexBuffer = nullptr;
        }
    }

    void Render(
        ID3D11DeviceContext* context,
        const WorldAreaComponent& area,
        const XMMATRIX& view,
        const XMMATRIX& projection,
        ColorShaderClass* shader)
    {
        XMMATRIX World = XMMatrixScaling(area.size.x, area.size.y, area.size.z) *
			XMMatrixTranslation(area.center.x, area.center.y, area.center.z);

        UINT stride = sizeof(Vertex);
        UINT offset = 0;

        context->IASetVertexBuffers(0, 1, &m_vertexBuffer, &stride, &offset);
        context->IASetIndexBuffer(m_indexBuffer, DXGI_FORMAT_R32_UINT, 0);
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

        shader->Render(
            context,
            m_indexCount,
            World,
            view,
            projection,
            XMFLOAT4(0.1f, 1.0f, 0.2f, 1.0f));
    }

private:
    ID3D11Buffer* m_vertexBuffer = nullptr;
    ID3D11Buffer* m_indexBuffer = nullptr;

    int m_indexCount = 0;
};
