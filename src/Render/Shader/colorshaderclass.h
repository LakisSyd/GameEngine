#pragma once
#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <fstream>
#include "BaseShader.h"

using namespace DirectX;
using namespace std;

class ColorShaderClass : public ShaderBaseClass {
private:
    struct MatrixBufferType
    {
        XMMATRIX world;
        XMMATRIX view;
        XMMATRIX projection;
    };

    // 🔥 ИСПРАВЛЕНИЕ: выравнивание до 32 байт (кратное 16!)
    struct ColorBufferType
    {
        XMFLOAT4 color;       // 16 байт
        XMFLOAT3 padding;     // 12 байт
        float padding2;       // 4 байта = 32 байта ✓
    };

public:
    ShaderType GetType() const override { return ShaderType::Color; }
    ColorShaderClass();
    ColorShaderClass(const ColorShaderClass&);
    ~ColorShaderClass();

    bool Initialize(ID3D11Device* device, HWND hwnd);
    void ShutDown();

    bool Render(ID3D11DeviceContext*, int, XMMATRIX, XMMATRIX, XMMATRIX);
    bool Render(ID3D11DeviceContext*, int, XMMATRIX, XMMATRIX, XMMATRIX, const XMFLOAT4& color);

private:
    bool InitializeShader(ID3D11Device*, HWND, WCHAR*, WCHAR*);
    void ShutdownShader();
    void OutputShaderErrorMessage(ID3D10Blob*, HWND, WCHAR*);
    bool SetShaderParameters(ID3D11DeviceContext*, XMMATRIX, XMMATRIX, XMMATRIX);
    bool SetShaderParameters(ID3D11DeviceContext*, XMMATRIX, XMMATRIX, XMMATRIX, const XMFLOAT4& color);
    void RenderShader(ID3D11DeviceContext*, int);

private:
    ID3D11VertexShader* m_vertexShader = nullptr;
    ID3D11PixelShader* m_pixelShader = nullptr;
    ID3D11InputLayout* m_layout = nullptr;
    ID3D11Buffer* m_matrixBuffer = nullptr;
    ID3D11Buffer* m_colorBuffer = nullptr;
};