#pragma once
#include <d3d11.h>
#include <d3dcompiler.h>
#include <directxmath.h>
#include <fstream>
#include "BaseShader.h"  // ← исправлен путь
using namespace DirectX;
using namespace std;

////////////////////////////////////////////////////////////////////////////////
// Имя класса: FogShaderClass
////////////////////////////////////////////////////////////////////////////////
class FogShaderClass : public ShaderBaseClass
{
private:
    // Буфер для матриц трансформации
    struct MatrixBufferType
    {
        XMMATRIX world;
        XMMATRIX view;
        XMMATRIX projection;
    };

    // 👇 Буфер для параметров тумана
    // Важно: 8 байт паддинга для выравнивания до 16 байт (требование DirectX)
    struct FogBufferType
    {
        float fogStart;
        float fogEnd;
        float padding1, padding2;  // Выравнивание!
    };

public:
    FogShaderClass();
    FogShaderClass(const FogShaderClass&) = delete;
    FogShaderClass& operator=(const FogShaderClass&) = delete;
    ~FogShaderClass();

    bool Initialize(ID3D11Device*, HWND);
    void Shutdown();

    // 👇 Render теперь принимает fogStart и fogEnd
    bool Render(ID3D11DeviceContext*, int, XMMATRIX, XMMATRIX, XMMATRIX,
        ID3D11ShaderResourceView*, float fogStart, float fogEnd);

private:
    bool InitializeShader(ID3D11Device*, HWND, WCHAR*, WCHAR*);
    void ShutdownShader();
    void OutputShaderErrorMessage(ID3D10Blob*, HWND, WCHAR*);

    bool SetShaderParameters(ID3D11DeviceContext*, XMMATRIX, XMMATRIX, XMMATRIX,
        ID3D11ShaderResourceView*, float fogStart, float fogEnd);
    void RenderShader(ID3D11DeviceContext*, int);

private:

    ID3D11Buffer* m_matrixBuffer;
    ID3D11Buffer* m_fogBuffer;
};
