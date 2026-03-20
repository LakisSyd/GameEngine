////////////////////////////////////////////////////////////////////////////////
// Filename: clipplaneshaderclass.h
////////////////////////////////////////////////////////////////////////////////

#pragma once

//////////////
// INCLUDES //
//////////////
#include <d3d11.h>
#include <d3dcompiler.h>
#include <directxmath.h>
#include <fstream>
#include "BaseShader.h"
using namespace DirectX;
using namespace std;

////////////////////////////////////////////////////////////////////////////////
// Class name: ClipPlaneShaderClass
////////////////////////////////////////////////////////////////////////////////
class ClipPlaneShaderClass : public ShaderBaseClass
{
private:
    struct MatrixBufferType
    {
        XMMATRIX world;
        XMMATRIX view;
        XMMATRIX projection;
    };

    // ” нас по€вилась нова€ структура дл€ соответстви€ посто€нному буферу плоскости отсечени€ в вершинном шейдере.
    struct ClipPlaneBufferType
    {
        XMFLOAT4 clipPlane;
    };

public:
    ClipPlaneShaderClass();
    ClipPlaneShaderClass(const ClipPlaneShaderClass&);
    ~ClipPlaneShaderClass();

    bool Initialize(ID3D11Device*, HWND);
    void Shutdown();
    bool Render(ID3D11DeviceContext*, int, XMMATRIX, XMMATRIX, XMMATRIX, ID3D11ShaderResourceView*, XMFLOAT4);

    ShaderType GetType() const override { return ShaderType::ClipPlane; }
   

    // Store clip plane for later use (e.g., from EditorUI)
    void SetClipPlane(const XMFLOAT4& plane) { m_storedClipPlane = plane; }
    XMFLOAT4 GetClipPlane() const { return m_storedClipPlane; }

private:
    bool InitializeShader(ID3D11Device*, HWND, WCHAR*, WCHAR*);
    void ShutdownShader();
    void OutputShaderErrorMessage(ID3D10Blob*, HWND, WCHAR*);
    bool SetShaderParameters(ID3D11DeviceContext*, XMMATRIX, XMMATRIX, XMMATRIX, ID3D11ShaderResourceView*, XMFLOAT4);
    void RenderShader(ID3D11DeviceContext*, int);

private:
    ID3D11VertexShader* m_vertexShader;
    ID3D11PixelShader* m_pixelShader;
    ID3D11InputLayout* m_layout;
    ID3D11Buffer* m_matrixBuffer;
    ID3D11SamplerState* m_sampleState;

    // ” нас по€вилс€ новый посто€нный буфер дл€ данных плоскости отсечени€.
    ID3D11Buffer* m_clipPlaneBuffer;

    // Stored clip plane value (set by editor, used during render)
    XMFLOAT4 m_storedClipPlane = { 0.0f, 1.0f, 0.0f, 0.0f };
};

