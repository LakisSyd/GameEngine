#pragma once
////////////////////////////////////////////////////////////////////////////////
// Filename: normalmapshaderclass.h
// Адаптирован для MaterialClass + AssimpModelClass архитектуры
////////////////////////////////////////////////////////////////////////////////

#include <d3d11.h>
#include <d3dcompiler.h>
#include <directxmath.h>
#include <fstream>
#include "BaseShader.h"

using namespace DirectX;
using namespace std;

class NormalMapShaderClass : public ShaderBaseClass
{
private:
    // Буфер матриц трансформации
    struct MatrixBufferType
    {
        XMMATRIX world;
        XMMATRIX view;
        XMMATRIX projection;
    };

    // Буфер освещения (свет + направление)
    struct LightBufferType
    {
        XMFLOAT4 diffuseColor;   // Цвет рассеянного света (RGBA)
        XMFLOAT3 lightDirection; // Направление света (XYZ)
        float padding;           // Выравнивание до 16 байт
    };

public:
    NormalMapShaderClass();
    NormalMapShaderClass(const NormalMapShaderClass&);
    ~NormalMapShaderClass();

    // Инициализация и завершение
    bool Initialize(ID3D11Device* device, HWND hwnd);
    void ShutDown();

    // Рендеринг (сигнатура под новую архитектуру)
    bool Render(
        ID3D11DeviceContext* deviceContext,
        int indexCount,
        XMMATRIX worldMatrix,
        XMMATRIX viewMatrix,
        XMMATRIX projectionMatrix,
        ID3D11ShaderResourceView* texture,      // Базовая текстура (t0)
        ID3D11ShaderResourceView* normalMap,    // Карта нормалей (t1)
        XMFLOAT3 lightDirection,
        XMFLOAT4 diffuseColor
    );

private:
    bool InitializeShader(ID3D11Device* device, HWND hwnd, const WCHAR* vsFilename, const WCHAR* psFilename);
    void ShutdownShader();
    void OutputShaderErrorMessage(ID3D10Blob* errorMessage, HWND hwnd, const WCHAR* shaderFilename);
    bool SetShaderParameters(
        ID3D11DeviceContext* deviceContext,
        XMMATRIX worldMatrix,
        XMMATRIX viewMatrix,
        XMMATRIX projectionMatrix,
        ID3D11ShaderResourceView* texture,
        ID3D11ShaderResourceView* normalMap,
        XMFLOAT3 lightDirection,
        XMFLOAT4 diffuseColor
    );
    void RenderShader(ID3D11DeviceContext* deviceContext, int indexCount);

private:
    ID3D11VertexShader* m_vertexShader;
    ID3D11PixelShader* m_pixelShader;
    ID3D11InputLayout* m_layout;
    ID3D11Buffer* m_matrixBuffer;
    ID3D11Buffer* m_lightBuffer;
    ID3D11SamplerState* m_sampleState;
};
