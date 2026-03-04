#pragma once
#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <string>

using namespace DirectX;

// Простой базовый класс-помощник (НЕ абстрактный!)
class ShaderBaseClass
{
public:
    ShaderBaseClass();
    virtual ~ShaderBaseClass();
    virtual void Shutdown();  // Не pure virtual!

    // Помощник для привязки шейдера перед Draw
    void BindShader(ID3D11DeviceContext* context);

protected:
    // === Общие ресурсы (доступны наследникам) ===
    ID3D11VertexShader* m_vertexShader;
    ID3D11PixelShader* m_pixelShader;
    ID3D11InputLayout* m_layout;
    ID3D11SamplerState* m_sampleState;

    // === Утилиты для наследников (НЕ виртуальные!) ===
    bool CompileShader(
        const std::wstring& filename,
        const char* entryPoint,
        const char* profile,
        ID3D10Blob** outBlob);

    bool CreateInputLayout(
        ID3D11Device* device,
        const D3D11_INPUT_ELEMENT_DESC* layoutDesc,
        int numElements,
        ID3D10Blob* vertexShaderBlob);

    bool CreateDefaultSampler(ID3D11Device* device);

    void OutputShaderErrorMessage(
        ID3D10Blob* errorMessage,
        HWND hwnd,
        const WCHAR* filename);

    // Хелпер для создания динамического констант-буфера
    bool CreateConstantBuffer(ID3D11Device* device, UINT byteWidth, ID3D11Buffer** outBuffer);

    // Хелпер для обновления буфера (Map → Write → Unmap → Bind)
    template<typename T>
    bool UpdateConstantBuffer(ID3D11DeviceContext* context, ID3D11Buffer* buffer, const T& data,
        UINT slot, bool vertexShader = true);

  
};