#pragma once
#include <d3d11.h>
#include <DirectXMath.h>
#include <functional>
#include "LightShaderClass.h"
#include "PBRShaderClass.h"
#include "GameSettings.h"      // PhysicsDebugSettings, FogSettings
class SkyShaderClass;


// Forward declarations
class SceneManager;
class D3DClass;
class CameraClass;
class ShadowMap;
class ShadowShaderClass;
class MaterialClass;
class TextureShaderClass;
class ColorShaderClass;
class PhysicsDebugRenderer;
class CompartmentDebugRenderer;
class RenderTextureClass;
struct PhysicsDebugSettings;

using namespace DirectX;

class Renderer
{
public:
    Renderer(D3DClass* d3d, CameraClass* camera,
        SceneManager* sceneManager, ShadowMap* shadowMap);

    // ── Setters — вызывай после конструктора, до первого кадра ──────────────
    void SetShadowShader(ShadowShaderClass* s) { m_shadowShader = s; }
    void SetLightShader(LightShaderClass* s) { m_lightShader = s; }
    void SetSkyShader(SkyShaderClass* s) { m_skyShader = s; }
    void SetPBRShader(PBRShaderClass* s) { m_pbrShader = s; }
    void SetDefaultMaterial(MaterialClass* m) { m_defaultMaterial = m; }
    void SetTextureShader(TextureShaderClass* s) { m_textureShader = s; }
    void SetPhysicsDebugRenderer(PhysicsDebugRenderer* r) { m_physicsDebugRenderer = r; }
    void SetCompartmentRenderer(CompartmentDebugRenderer* r) { m_compartmentRenderer = r; }
    void SetColorShader(ColorShaderClass* s) { m_colorShader = s; }
    void SetWireframeRS(ID3D11RasterizerState* rs) { m_wireframeRS = rs; }

    void SetRenderTextures(RenderTextureClass* reflection,
        RenderTextureClass* refraction,
        RenderTextureClass* preview)
    {
        m_reflectionTexture = reflection;
        m_refractionTexture = refraction;
        m_renderTexture = preview;
    }

    /// Колбэк: Renderer вызовет его с preview-SRV после прохода preview.
    /// ApplicationClass передаёт SRV в EditorUI этим способом.
    void SetPreviewCallback(std::function<void(ID3D11ShaderResourceView*)> cb)
    {
        m_previewCallback = std::move(cb);
    }

    // ── Единственный публичный вызов на кадр ────────────────────────────────
    void RenderFrame(
        ID3D11DeviceContext* context,
        const XMMATRIX& view,
        const XMMATRIX& projection,
        Entity                       selectedEntity,
        const PhysicsDebugSettings& debugSettings);

    // ── Отдельные пассы (если нужны снаружи) ────────────────────────────────
    void Render3DObjects(
        ID3D11DeviceContext* context,
        const XMMATRIX& view,
        const XMMATRIX& projection,
        LightShaderClass* lightShader = nullptr,
        PBRShaderClass* pbrShader = nullptr,
        MaterialClass* defaultMaterial = nullptr,
        Entity             selectedEntity = INVALID_ENTITY);

    void RenderExcluding(
        ID3D11DeviceContext* context,
        const XMMATRIX& view,
        const XMMATRIX& projection,
        Entity excludeEntity = INVALID_ENTITY);

    void RenderToShadowMap(
        ID3D11DeviceContext* context,
        const XMMATRIX& lightViewProj);

    void RenderSprites(
        ID3D11DeviceContext* context,
        const XMMATRIX& view,
        const XMMATRIX& projection);

private:
    // ── Ядро ─────────────────────────────────────────────────────────────────
    D3DClass* m_direct3D;
    CameraClass* m_camera;
    ShadowMap* m_shadowMap;     // не владеем: lifetime у ApplicationClass
    SceneManager* m_sceneManager;

    // ── Ресурсы пайплайна ─────────────────────────────────────────────────────
    ShadowShaderClass* m_shadowShader = nullptr;
    LightShaderClass* m_lightShader = nullptr;
    PBRShaderClass* m_pbrShader = nullptr;
    MaterialClass* m_defaultMaterial = nullptr;
    TextureShaderClass* m_textureShader = nullptr;
    ColorShaderClass* m_colorShader = nullptr;
    PhysicsDebugRenderer* m_physicsDebugRenderer = nullptr;
    CompartmentDebugRenderer* m_compartmentRenderer = nullptr;
    ID3D11RasterizerState* m_wireframeRS = nullptr;
    RenderTextureClass* m_reflectionTexture = nullptr;
    RenderTextureClass* m_refractionTexture = nullptr;
    RenderTextureClass* m_renderTexture = nullptr;

    SkyShaderClass* m_skyShader = nullptr;
    XMFLOAT3 m_skyTop = XMFLOAT3(0.15f, 0.4f, 0.8f);      // голубой верх
    XMFLOAT3 m_skyHorizon = XMFLOAT3(0.6f, 0.75f, 0.9f);  // светлый горизонт
    ID3D11DepthStencilState* m_skyDepthState = nullptr;   // depth read-only

    std::function<void(ID3D11ShaderResourceView*)> m_previewCallback;

    // ── Приватные хелперы ─────────────────────────────────────────────────────
    XMMATRIX BuildReflectionMatrix(float reflectY = 0.0f) const;
    void     UpdateWaterMaterials(ID3D11DeviceContext* context,
        const XMMATRIX& reflectionMatrix);
    void     UpdateDirectionalLight(ID3D11DeviceContext* context);
    static void ClearSRVSlots(ID3D11DeviceContext* context, int count = 8);
};
