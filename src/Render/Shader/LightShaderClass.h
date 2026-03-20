#pragma once
#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <fstream>
#include "BaseShader.h"  // ← Подключаем базовый класс

#define NUM_LIGHTS 4

class LightShaderClass : public ShaderBaseClass  // Просто наследуемся, без override
{
public:
    LightShaderClass();
    ~LightShaderClass();  // Не virtual, не override

    // === ТВОИ СТАРЫЕ МЕТОДЫ — сигнатуры НЕ меняем! ===
    bool Initialize(ID3D11Device* device, HWND hwnd);  // ← Твоя старая сигнатура!
    void Shutdown();


     ShaderType GetType() const override { return ShaderType::Light; }

    bool Render(  // ← Твоя старая сигнатура!
        ID3D11DeviceContext* deviceContext,
        int indexCount,
        XMMATRIX worldMatrix,
        XMMATRIX viewMatrix,
        XMMATRIX projectionMatrix,
        ID3D11ShaderResourceView* texture,
        XMFLOAT4 lightColors[],
        XMFLOAT4 lightPositions[]);

    bool SetSelectedFactor(ID3D11DeviceContext* deviceContext, float factor);

private:
    // === Твои уникальные буферы ===
    ID3D11Buffer* m_matrixBuffer;
    ID3D11Buffer* m_lightBuffer;
    ID3D11Buffer* m_lightPositionBuffer;
    ID3D11Buffer* m_lightColorBuffer;
    ID3D11Buffer* m_selectedBuffer;

    // === Внутренние методы (твои текущие) ===
    bool InitializeShader(ID3D11Device* device, HWND hwnd, const WCHAR* vsFilename, const WCHAR* psFilename);
    void ShutdownShader();
    void OutputShaderErrorMessage(ID3D10Blob* errorMessage, HWND hwnd, WCHAR* shaderFilename);
    bool SetShaderParameters(
        ID3D11DeviceContext* deviceContext,
        XMMATRIX worldMatrix, XMMATRIX viewMatrix, XMMATRIX projectionMatrix,
        ID3D11ShaderResourceView* texture,
        XMFLOAT4 lightColors[],
        XMFLOAT4 lightPositions[]);
    void RenderShader(ID3D11DeviceContext* deviceContext, int indexCount);

    // === Структуры (твои текущие) ===
    struct MatrixBufferType { XMMATRIX world, view, projection; };
    struct LightBufferType { /* твои поля */ };
    struct LightPositionBufferType { XMFLOAT4 lightPosition[NUM_LIGHTS]; };
    struct LightColorBufferType { XMFLOAT4 diffuseColor[NUM_LIGHTS]; };
    struct SelectedBufferType { float selectedFactor; float padding[3]; };
};