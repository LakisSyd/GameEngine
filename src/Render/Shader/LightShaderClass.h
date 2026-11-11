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

     bool SetShadowMap(
         ID3D11DeviceContext* context,
         ID3D11ShaderResourceView* shadowMapSRV,
         const XMMATRIX& lightViewProj);

    bool Render(  // ← Твоя старая сигнатура!
        ID3D11DeviceContext* deviceContext,
        int indexCount,
        XMMATRIX worldMatrix,
        XMMATRIX viewMatrix,
        XMMATRIX projectionMatrix,
        ID3D11ShaderResourceView* texture,
        XMFLOAT4 lightColors[],
        XMFLOAT4 lightPositions[],
        const XMFLOAT4* lightDirectionsAndOuterCos = nullptr,
        const XMFLOAT4* lightInnerCosAndFlags = nullptr);

    bool SetSelectedFactor(ID3D11DeviceContext* deviceContext, float factor);

    bool SetDirectionalLight(ID3D11DeviceContext* deviceContext, const XMFLOAT3& direction, const XMFLOAT4& color, const XMFLOAT4& ambient, float intensity);
    bool PrepareBeforeRender(
        ID3D11DeviceContext* context,
        float selectedFactor)
    {
        return SetSelectedFactor(context, selectedFactor);
    }

private:
    // === Твои уникальные буферы ===
    ID3D11Buffer* m_matrixBuffer;
    ID3D11Buffer* m_lightBuffer;
    ID3D11Buffer* m_lightPositionBuffer;
    ID3D11Buffer* m_lightColorBuffer;
    ID3D11Buffer* m_lightSpotBuffer;
    ID3D11Buffer* m_selectedBuffer;
    ID3D11SamplerState* m_shadowSampler;
    ID3D11ShaderResourceView* m_shadowMapSRV = nullptr; // external, owned by ShadowMap

	// 🔥 НОВОЕ: Структура для одного направленного света (слот b3 в пиксельном шейдере)

    struct DirectionalLightBufferType
    {
        XMFLOAT4 direction;  // xyz - направление, w - интенсивность
        XMFLOAT4 color;      // rgb - цвет, a - ambient strength
        XMFLOAT4 ambient;    // Ambient color
        float padding;
    };

    struct ShadowBufferType
    {
        XMMATRIX lightViewProj;
    };

    ID3D11Buffer* m_shadowBuffer;

    // Добавьте к существующим членам класса:
    ID3D11Buffer* m_directionalLightBuffer; // ← НОВОЕ


    // === Внутренние методы (твои текущие) ===
    bool InitializeShader(ID3D11Device* device, HWND hwnd, const WCHAR* vsFilename, const WCHAR* psFilename);
    void ShutdownShader();
    void OutputShaderErrorMessage(ID3D10Blob* errorMessage, HWND hwnd, WCHAR* shaderFilename);
    bool SetShaderParameters(
        ID3D11DeviceContext* deviceContext,
        XMMATRIX worldMatrix, XMMATRIX viewMatrix, XMMATRIX projectionMatrix,
        ID3D11ShaderResourceView* texture,
        XMFLOAT4 lightColors[],
        XMFLOAT4 lightPositions[],
        const XMFLOAT4* lightDirectionsAndOuterCos,
        const XMFLOAT4* lightInnerCosAndFlags);
    void RenderShader(ID3D11DeviceContext* deviceContext, int indexCount);

    // === Структуры (твои текущие) ===
    struct MatrixBufferType { XMMATRIX world, view, projection; };
    struct LightBufferType { /* твои поля */ };
    struct LightPositionBufferType { XMFLOAT4 lightPosition[NUM_LIGHTS]; };
    struct LightColorBufferType { XMFLOAT4 diffuseColor[NUM_LIGHTS]; };
    struct LightSpotBufferType
    {
        XMFLOAT4 directionAndOuterCos[NUM_LIGHTS];
        XMFLOAT4 innerCosAndFlags[NUM_LIGHTS];
    };
    struct SelectedBufferType { float selectedFactor; float padding[3]; };
};
