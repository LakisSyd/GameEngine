#pragma once
#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <string>
#include "../../Config/GameSettings.h"

using namespace DirectX;

enum class ShaderType : uint8_t
{
    Unknown = 0,
    Color = 1,
    Texture = 2,
    Light = 3,
    LightMap = 4,
    MultiTexture = 5,
    NormalMap = 6,
    Fog = 7,
    ClipPlane = 8,
    Reflection = 9,
    Water = 10,
    Glass = 11,
    Projection = 12,
    PBR = 13
};


// Простой базовый класс-помощник (НЕ абстрактный!)
class ShaderBaseClass
{
public:
    ShaderBaseClass();
    virtual ~ShaderBaseClass();
    virtual bool Initialize(ID3D11Device* device, HWND hwnd) { return true; }
    virtual void Shutdown();  // Не pure virtual!
    virtual ShaderType GetType() const { return ShaderType::Unknown; }
    // Помощник для привязки шейдера перед Draw
    void BindShader(ID3D11DeviceContext* context);

    // ✅ Обновляет FogBuffer — одинаково для всех шейдеров (слот b2)
    void UpdateFog(ID3D11DeviceContext* context, const Settings::FogSettings& fog);

protected:
    // === Общие ресурсы (доступны наследникам) ===
    ID3D11VertexShader* m_vertexShader;
    ID3D11PixelShader* m_pixelShader;
    ID3D11InputLayout* m_layout;
    ID3D11SamplerState* m_sampleState;
    ID3D11Buffer* m_fogBuffer;   // ← слот b2 во всех шейдерах

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

    // ✅ Создаёт FogBuffer — вызвать в конце InitializeShader() каждого шейдера
    bool CreateFogBuffer(ID3D11Device* device);

    // Хелпер для обновления буфера (Map → Write → Unmap → Bind)
    template<typename T>
    bool UpdateConstantBuffer(ID3D11DeviceContext* context, ID3D11Buffer* buffer, const T& data,
        UINT slot, bool vertexShader = true)
    {
        if (!buffer) return false;

        D3D11_MAPPED_SUBRESOURCE mapped;
        HRESULT hr = context->Map(buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
        if (FAILED(hr)) return false;

        memcpy(mapped.pData, &data, sizeof(T));
        context->Unmap(buffer, 0);

        if (vertexShader)
            context->VSSetConstantBuffers(slot, 1, &buffer);
        else
            context->PSSetConstantBuffers(slot, 1, &buffer);

        return true;
    }

private:
    // Структура тумана — одинакова для всех шейдеров
    struct FogBufferType
    {
        XMFLOAT4 fogColor;    // rgb = цвет, a = не используется
        float    fogStart;
        float    fogEnd;
        float    fogEnabled;  // 1.0 = включён, 0.0 = выключен
        float    padding;
    };
};