#include "Renderer.h"
#include "D3DClass.h"
#include "SceneManager.h"
#include "ShadowMap.h"
#include "ShadowShaderClass.h"
#include "cameraclass.h"
#include "TimeManager.h"
#include "MaterialClass.h"
#include "ShaderManager.h"
#include "PhysicsWorld.h"
#include "RenderTextureClass.h"
#include "PhysicsDebugRenderer.h"
#include "CompartmentDebugRenderer.h"

// ============================================================================
Renderer::Renderer(D3DClass* d3d, CameraClass* camera,
    SceneManager* sceneManager, ShadowMap* shadowMap)
    : m_direct3D(d3d)
    , m_camera(camera)
    , m_sceneManager(sceneManager)
    , m_shadowMap(shadowMap)
{
}

// ============================================================================
// RenderFrame — единственная точка входа за кадр
// ============================================================================
void Renderer::RenderFrame(
    ID3D11DeviceContext* context,
    const XMMATRIX& view,
    const XMMATRIX& projection,
    Entity                      selectedEntity,
    const PhysicsDebugSettings& debugSettings)
{
    // 0. Обновляем направленный свет в шейдерах
    UpdateDirectionalLight(context);

    // ── Pass: Shadow Map ──────────────────────────────────────────────────────
    if (m_shadowMap)
    {
        m_shadowMap->SetRenderTarget(context);
        m_shadowMap->ClearRenderTarget(context);
        RenderToShadowMap(context, m_shadowMap->GetLightViewProjection());
        m_direct3D->SetBackBufferRenderTarget();
        m_direct3D->ResetViewport();
    }

    // ── Вычисляем матрицу отражения ──────────────────────────────────────────
    const XMMATRIX reflectionMatrix = BuildReflectionMatrix(0.0f);
    UpdateWaterMaterials(context, reflectionMatrix);

    // ── Pass: Reflection → m_reflectionTexture ────────────────────────────────
    if (m_reflectionTexture)
    {
        m_reflectionTexture->SetRenderTarget(context);
        m_reflectionTexture->ClearRenderTarget(context, 0.f, 0.f, 0.f, 1.f);
        RenderExcluding(context, reflectionMatrix, projection, INVALID_ENTITY);
        m_direct3D->SetBackBufferRenderTarget();
        m_direct3D->ResetViewport();
        ClearSRVSlots(context);
    }

    // ── Pass: Refraction → m_refractionTexture ───────────────────────────────
    if (m_refractionTexture)
    {
        m_refractionTexture->SetRenderTarget(context);
        m_refractionTexture->ClearRenderTarget(context, 0.f, 0.f, 0.f, 1.f);

        // Рендерим всё кроме Glass/Water
        m_direct3D->TurnZBufferOn();
        m_direct3D->TurnAlphaBlendingOff();

        XMFLOAT4 lightColors[NUM_LIGHTS];
        XMFLOAT4 lightPositions[NUM_LIGHTS];
        auto& meshPool = m_sceneManager->GetECSRegistry().GetComponents<MeshRendererComponent>();

        for (auto& [entity, mesh] : meshPool)
        {
            if (!m_sceneManager->IsObjectActive(entity)) continue;
            auto* transform = m_sceneManager->GetObjectTransform(entity);
            if (!transform || !mesh.model) continue;
            auto* mat = mesh.material ? mesh.material : m_defaultMaterial;
            if (!mat) continue;

            auto* shader = mat->GetShader();
            if (shader)
            {
                ShaderType st = shader->GetType();
                if (st == ShaderType::Glass || st == ShaderType::Water) continue;
            }

            XMMATRIX world = transform->GetWorldMatrix();
            RenderMesh(context, mesh.model, world, view, projection,
                mat, lightColors, lightPositions,
                m_sceneManager->GetFogSettings(), false);
        }

        m_direct3D->SetBackBufferRenderTarget();
        m_direct3D->ResetViewport();
        ClearSRVSlots(context);
    }

    // ── Pass: Scene Preview → m_renderTexture (для EditorUI) ─────────────────
    if (m_renderTexture)
    {
        const auto& fog = m_sceneManager->GetFogSettings();
        m_renderTexture->SetRenderTarget(context);
        m_renderTexture->ClearRenderTarget(context,
            fog.color.x, fog.color.y, fog.color.z, 1.f);

        XMMATRIX rtProj;
        m_renderTexture->GetProjectionMatrix(rtProj);
        Render3DObjects(context, view, rtProj,
            m_lightShader, m_pbrShader, m_defaultMaterial, INVALID_ENTITY);

        if (m_previewCallback)
            m_previewCallback(m_renderTexture->GetShaderResourceView());

        ClearSRVSlots(context);
    }

    // ── Pass: Main scene → backbuffer ─────────────────────────────────────────
    m_direct3D->SetBackBufferRenderTarget();
    m_direct3D->ResetViewport();

    const auto& fog = m_sceneManager->GetFogSettings();
    if (fog.enabled)
        m_direct3D->BeginScene(fog.color.x, fog.color.y, fog.color.z, 1.f);
    else
        m_direct3D->BeginScene(0.f, 0.f, 0.f, 1.f);

    Render3DObjects(context, view, projection,
        m_lightShader, m_pbrShader, m_defaultMaterial, selectedEntity);

    // ── Sprites ───────────────────────────────────────────────────────────────
    RenderSprites(context, view, projection);

    // ── Compartments (solid) ──────────────────────────────────────────────────
    if (m_compartmentRenderer && m_colorShader)
    {
        ID3D11RasterizerState* oldRS = nullptr;
        context->RSGetState(&oldRS);
        context->RSSetState(nullptr);   // solid

        m_compartmentRenderer->RenderCompartments(
            m_sceneManager->GetECSRegistry(),
            context, view, projection, m_colorShader);

        context->RSSetState(oldRS);
        if (oldRS) oldRS->Release();
    }

    // ── Physics debug (wireframe) ─────────────────────────────────────────────
    if (m_physicsDebugRenderer && m_colorShader &&
        (debugSettings.showColliders || debugSettings.showAABB || debugSettings.showContacts))
    {
        ID3D11RasterizerState* oldRS = nullptr;
        context->RSGetState(&oldRS);
        context->RSSetState(m_wireframeRS);

        m_physicsDebugRenderer->Render(
            m_sceneManager->GetECSRegistry(),
            PhysicsWorld::Get().GetWorld(),
            context, view, projection, m_colorShader, debugSettings);

        context->RSSetState(oldRS);
        if (oldRS) oldRS->Release();
    }
}

// ============================================================================
// Render3DObjects
// ============================================================================
void Renderer::Render3DObjects(
    ID3D11DeviceContext* context,
    const XMMATRIX& view,
    const XMMATRIX& projection,
    LightShaderClass* lightShader,
    PBRShaderClass* pbrShader,
    MaterialClass* defaultMaterial,
    Entity             selectedEntity)
{
    m_direct3D->TurnZBufferOn();
    m_direct3D->TurnAlphaBlendingOff();

    ShaderManager::Get().ForEach([&](ShaderBaseClass* s) {
        s->UpdateFog(context, m_sceneManager->GetFogSettings());
        });

    XMFLOAT3 camPos = m_camera->GetPosition();

    if (lightShader && m_shadowMap)
    {
        XMMATRIX lvp = m_shadowMap->GetLightViewProjection();
        lightShader->SetShadowMap(context, m_shadowMap->GetShaderResourceView(), lvp);
    }
    if (pbrShader && m_shadowMap)
    {
        XMMATRIX lvp = m_shadowMap->GetLightViewProjection();
        pbrShader->SetShadowMap(context, m_shadowMap->GetShaderResourceView(), lvp);
    }

    // Передаём данные о трещинах в PBR шейдер
    if (pbrShader)
    {
        auto& registry = m_sceneManager->GetECSRegistry();
        auto& meshPool = registry.GetComponents<MeshRendererComponent>();

        for (auto& [entity, mesh] : meshPool)
        {
            if (!mesh.material || !mesh.material->GetShader()) continue;
            if (mesh.material->GetShader()->GetType() != ShaderType::PBR) continue;

            auto* transform = m_sceneManager->GetObjectTransform(entity);
            if (!transform) continue;

            auto* damage = registry.GetComponent<DamageDecalComponent>(entity);
            pbrShader->SetDamageDecals(context, damage, transform->GetWorldMatrix());
        }
    }

    XMFLOAT4 lightColors[NUM_LIGHTS];
    XMFLOAT4 lightPositions[NUM_LIGHTS];

    m_sceneManager->RenderAllECS(
        context, view, projection,
        defaultMaterial ? defaultMaterial : m_defaultMaterial,
        lightColors, lightPositions,
        m_sceneManager->GetFogSettings(),
        selectedEntity, camPos,
        TimeManager::UnscaledDeltaTime());

    // Очищаем слот shadow map
    ID3D11ShaderResourceView* nullSRV = nullptr;
    context->PSSetShaderResources(7, 1, &nullSRV);
}

// ============================================================================
// RenderExcluding  (бывший ApplicationClass::Render3DObjectsExcluding)
// ============================================================================
void Renderer::RenderExcluding(
    ID3D11DeviceContext* context,
    const XMMATRIX& view,
    const XMMATRIX& projection,
    Entity excludeEntity)
{
    m_direct3D->TurnZBufferOn();
    m_direct3D->TurnAlphaBlendingOff();

    ShaderManager::Get().ForEach([&](ShaderBaseClass* s) {
        s->UpdateFog(context, m_sceneManager->GetFogSettings());
        });

    XMFLOAT4 lightColors[NUM_LIGHTS];
    XMFLOAT4 lightPositions[NUM_LIGHTS];

    m_sceneManager->RenderAllECSExcluding(
        context, view, projection,
        m_defaultMaterial,
        lightColors, lightPositions,
        m_sceneManager->GetFogSettings(),
        excludeEntity,
        TimeManager::UnscaledDeltaTime());
}

// ============================================================================
// RenderToShadowMap  (бывший ApplicationClass::Render3DObjectsToShadowMap)
// ============================================================================
void Renderer::RenderToShadowMap(
    ID3D11DeviceContext* context,
    const XMMATRIX& lightViewProj)
{
    if (!m_shadowShader) return;

    auto& meshPool = m_sceneManager->GetECSRegistry()
        .GetComponents<MeshRendererComponent>();

    for (auto& [entity, mesh] : meshPool)
    {
        if (!m_sceneManager->IsObjectActive(entity)) continue;
        auto* transform = m_sceneManager->GetObjectTransform(entity);
        if (!transform || !mesh.model) continue;

        auto* mat = mesh.material;
        if (!mat || !mat->GetShader()) continue;

        ShaderType st = mat->GetShader()->GetType();
        if (st != ShaderType::Light && st != ShaderType::PBR) continue;

        XMMATRIX world = transform->GetWorldMatrix();
        mesh.model->Render(context);
        m_shadowShader->Render(context, mesh.model->GetIndexCount(), world, lightViewProj);
    }

    // Очистка
    ID3D11Buffer* nullCB[6] = {};
    ID3D11ShaderResourceView* nullSRV[8] = {};
    context->VSSetConstantBuffers(0, 6, nullCB);
    context->PSSetConstantBuffers(0, 6, nullCB);
    context->PSSetShaderResources(0, 8, nullSRV);
}

// ============================================================================
// RenderSprites  (бывший ApplicationClass::RenderWorldSprites)
// ============================================================================
void Renderer::RenderSprites(
    ID3D11DeviceContext* context,
    const XMMATRIX& view,
    const XMMATRIX& projection)
{
    if (!m_textureShader) return;
    m_direct3D->TurnAlphaBlendingOn();
    m_sceneManager->RenderSprites(context, view, projection, m_textureShader);
    m_direct3D->TurnAlphaBlendingOff();
}

// ============================================================================
// Приватные хелперы
// ============================================================================
void Renderer::UpdateDirectionalLight(ID3D11DeviceContext* context)
{
    const XMFLOAT3 sunDir = { 0.3f, -1.0f, 0.5f };
    const XMFLOAT4 sunColor = { 1.0f, 0.95f, 0.8f, 1.0f };
    const XMFLOAT4 ambientPBR = { 0.15f, 0.2f, 0.3f, 0.25f };
    const XMFLOAT4 ambientLight = { 0.4f,  0.4f, 0.45f, 0.6f };

    if (m_lightShader)
        m_lightShader->SetDirectionalLight(context, sunDir, sunColor, ambientLight, 2.5f);

    if (m_pbrShader)
        m_pbrShader->SetDirectionalLight(context, sunDir, sunColor, ambientPBR, 1.0f);
}

XMMATRIX Renderer::BuildReflectionMatrix(float reflectY) const
{
    XMFLOAT3 pos = m_camera->GetPosition();
    XMFLOAT3 rot = m_camera->GetRotation();

    float reflectedPosY = 2.f * reflectY - pos.y;
    XMMATRIX rotMat = XMMatrixRotationRollPitchYaw(
        rot.x * 0.0174532925f, rot.y * 0.0174532925f, 0.f);

    XMVECTOR forward = XMVector3TransformCoord(XMVectorSet(0, 0, 1, 0), rotMat);
    XMVECTOR up = XMVector3TransformCoord(XMVectorSet(0, 1, 0, 0), rotMat);

    XMVECTOR reflectPlane = XMVectorSet(0.f, 1.f, 0.f, -reflectY);
    XMMATRIX reflectMat = XMMatrixReflect(reflectPlane);

    XMVECTOR reflPos = XMVectorSet(pos.x, reflectedPosY, pos.z, 1.f);
    XMVECTOR reflForward = XMVector3TransformNormal(forward, reflectMat);
    XMVECTOR reflUp = XMVector3TransformNormal(up, reflectMat);

    return XMMatrixLookAtLH(reflPos, XMVectorAdd(reflPos, reflForward), reflUp);
}

void Renderer::UpdateWaterMaterials(ID3D11DeviceContext* /*context*/,
    const XMMATRIX& reflectionMatrix)
{
    auto& meshPool = m_sceneManager->GetECSRegistry().GetComponents<MeshRendererComponent>();
    for (auto& [entity, mesh] : meshPool)
    {
        if (!m_sceneManager->IsObjectActive(entity) || !mesh.material) continue;
        auto* shader = mesh.material->GetShader();
        if (!shader) continue;

        if (shader->GetType() == ShaderType::Water)
        {
            mesh.material->SetReflectionMatrix(reflectionMatrix);
            if (m_reflectionTexture)
                mesh.material->SetReflectionSRV(m_reflectionTexture->GetShaderResourceView());
            if (m_refractionTexture)
                mesh.material->SetRefractionSRV(m_refractionTexture->GetShaderResourceView());
        }
        else if (shader->GetType() == ShaderType::Reflection)
        {
            mesh.material->SetReflectionMatrix(reflectionMatrix);
            if (m_reflectionTexture)
                mesh.material->SetReflectionSRV(m_reflectionTexture->GetShaderResourceView());
            mesh.material->SetReflectivity(0.5f);
        }
    }
}

void Renderer::ClearSRVSlots(ID3D11DeviceContext* context, int count)
{
    std::vector<ID3D11ShaderResourceView*> nulls(count, nullptr);
    context->PSSetShaderResources(0, count, nulls.data());
}
