#pragma once

#include "BaseShader.h"

class ShadowShaderClass : public ShaderBaseClass
{
public:
    ShadowShaderClass();
    ~ShadowShaderClass();

    bool Initialize(ID3D11Device* device, HWND hwnd);
    void Shutdown();

    bool Render(
        ID3D11DeviceContext* context,
        int indexCount,
        const XMMATRIX& world,
        const XMMATRIX& lightViewProj);

private:
    struct MatrixBufferType
    {
        XMMATRIX world;
        XMMATRIX lightViewProj;
    };

    bool InitializeShader(ID3D11Device* device, HWND hwnd,
        const WCHAR* vsFilename, const WCHAR* psFilename);
    void ShutdownShader();

    bool SetShaderParameters(ID3D11DeviceContext* context,
        const XMMATRIX& world,
        const XMMATRIX& lightViewProj);
    void RenderShader(ID3D11DeviceContext* context, int indexCount);

private:
    ID3D11Buffer* m_matrixBuffer;
};
