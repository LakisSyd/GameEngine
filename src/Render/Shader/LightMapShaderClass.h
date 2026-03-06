#pragma once
////////////////////////////////////////////////////////////////////////////////
// Filename: multitextureshaderclass.h
////////////////////////////////////////////////////////////////////////////////

#include <d3d11.h>
#include <d3dcompiler.h>
#include <directxmath.h>
#include <fstream>
#include "BaseShader.h"

using namespace DirectX;
using namespace std;


class LightMapShaderClass : public ShaderBaseClass
{
private:
    struct MatrixBufferType
    {
        XMMATRIX world;
        XMMATRIX view;
        XMMATRIX projection;
    };

public:
    LightMapShaderClass();
    LightMapShaderClass(const LightMapShaderClass&);
    ~LightMapShaderClass();


    bool Initialize(ID3D11Device* device, HWND hwnd);
    void ShutDown();
    bool Render(ID3D11DeviceContext* deviceContext, int indexCount, XMMATRIX worldMatrix,
        XMMATRIX viewMatrix, XMMATRIX projectionMatrix, ID3D11ShaderResourceView* texture1,
        ID3D11ShaderResourceView* texture2);


private:
    bool InitializeShader(ID3D11Device*, HWND, WCHAR* vs, WCHAR* ps);
    void ShutdownShader();
    void OutputShaderErrorMessage(ID3D10Blob*, HWND, WCHAR*);
    bool SetShaderParameters(ID3D11DeviceContext*, XMMATRIX, XMMATRIX, XMMATRIX,
        ID3D11ShaderResourceView*, ID3D11ShaderResourceView*);
    void RenderShader(ID3D11DeviceContext*, int);

private:
    ID3D11VertexShader* m_vertexShader;      // Указатель на вершинный шейдер
    ID3D11PixelShader* m_pixelShader;        // Указатель на пиксельный шейдер
    ID3D11InputLayout* m_layout;             // Описание входного формата вершин
    ID3D11Buffer* m_matrixBuffer;            // Буфер констант для матриц
    ID3D11SamplerState* m_sampleState;       // Состояние сэмплера текстур
};
