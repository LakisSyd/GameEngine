#pragma once
#include "BaseShader.h"

// ============================================================================
// ReflectionShaderClass Ч плоское зеркальное отражение (Planar Reflection)
//
// ѕринимает две текстуры:
//   t0 Ч текстура поверхности (пол, вода и т.д.)
//   t1 Ч RenderTexture с отражЄнной сценой
//
// —лоты константных буферов:
//   b0 (VS) Ч матрицы мира/вида/проекции
//   b1 (VS) Ч матрица отражени€ (reflectionMatrix)
//   b0 (PS) Ч reflectivity (float, степень зеркальности)
//   b2 (VS+PS) Ч FogBuffer (наследуетс€ от ShaderBaseClass)
// ============================================================================
class ReflectionShaderClass : public ShaderBaseClass
{
private:
    struct MatrixBufferType
    {
        XMMATRIX world;
        XMMATRIX view;
        XMMATRIX projection;
    };

    struct ReflectionBufferType
    {
        XMMATRIX reflectionMatrix;
    };

    struct ReflectColorBufferType
    {
        float    reflectivity;
        XMFLOAT3 padding;
    };

public:
    ReflectionShaderClass() = default;
    ~ReflectionShaderClass() = default;

    bool Initialize(ID3D11Device* device, HWND hwnd) override;
    void Shutdown() override;
    ShaderType GetType() const override { return ShaderType::Reflection; }
    // ...
    // world/view/proj    Ч стандартные матрицы кадра
    // reflectionMatrix   Ч матрица вида отражЄнной камеры
    // texture            Ч текстура поверхности (t0)
    // reflectionTexture  Ч RenderTexture с отражЄнной сценой (t1)
    // reflectivity       Ч степень зеркальности [0..1]
    bool Render(ID3D11DeviceContext* context, int indexCount,
        XMMATRIX world, XMMATRIX view, XMMATRIX projection,
        XMMATRIX reflectionMatrix,
        ID3D11ShaderResourceView* texture,
        ID3D11ShaderResourceView* reflectionTexture,
        float reflectivity);

private:
    bool InitializeShader(ID3D11Device* device, HWND hwnd,
        const wchar_t* vsFile, const wchar_t* psFile);
    bool SetShaderParameters(ID3D11DeviceContext* context,
        XMMATRIX world, XMMATRIX view, XMMATRIX projection,
        XMMATRIX reflectionMatrix,
        ID3D11ShaderResourceView* texture,
        ID3D11ShaderResourceView* reflectionTexture,
        float reflectivity);
    void RenderShader(ID3D11DeviceContext* context, int indexCount);

    ID3D11Buffer* m_matrixBuffer = nullptr; // b0 VS
    ID3D11Buffer* m_reflectBuffer = nullptr; // b1 VS
    ID3D11Buffer* m_colorBuffer = nullptr; // b0 PS
};