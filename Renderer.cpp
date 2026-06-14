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
    #include "SkyShaderClass.h"
    #include "AssetsLoadSystem.h"
    #include "src/Render/Grass/GrassRenderer.h"
#include "src/Render/Shader/TerrainBlendShaderClass.h"

    // ============================================================================
Renderer::Renderer(D3DClass* d3d, CameraClass* camera,
    SceneManager* sceneManager, ShadowMap* shadowMap)
    : m_direct3D(d3d)
    , m_camera(camera)
    , m_sceneManager(sceneManager)
    , m_shadowMap(shadowMap)
    // ? Инициализируй ВСЕ указатели в списке инициализации:
    , m_skyDepthState(nullptr)
    , m_lightShader(nullptr)
    , m_pbrShader(nullptr)
    , m_skyShader(nullptr)
    , m_textureShader(nullptr)
    , m_colorShader(nullptr)
    , m_wireframeRS(nullptr)
    , m_reflectionTexture(nullptr)
    , m_refractionTexture(nullptr)
    , m_renderTexture(nullptr)
    , m_physicsDebugRenderer(nullptr)
    , m_compartmentRenderer(nullptr)
    , m_defaultMaterial(nullptr)
    , m_previewCallback(nullptr)
    {
        D3D11_DEPTH_STENCIL_DESC dsDesc = {};  // ? Обнуляем всё
        dsDesc.DepthEnable = TRUE;
        dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;  // ?? КЛЮЧЕВОЕ
        dsDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
        dsDesc.StencilEnable = FALSE;  // на всякий случай


         
        D3D11_RASTERIZER_DESC rsDesc = {};
        rsDesc.FillMode = D3D11_FILL_SOLID;
        rsDesc.CullMode = D3D11_CULL_NONE;
        rsDesc.FrontCounterClockwise = FALSE;
        rsDesc.DepthClipEnable = FALSE;

        HRESULT hr = m_direct3D->GetDevice()->CreateRasterizerState(&rsDesc, &m_skyRS);
        if (FAILED(hr))
        {
            OutputDebugStringA("Failed to create sky rasterizer state\n");
        }

       

        hr = m_direct3D->GetDevice()->CreateDepthStencilState(&dsDesc, &m_skyDepthState);
        if (FAILED(hr))
        {
            OutputDebugStringA("Failed to create sky depth stencil state\n");
        }
    }

    // ============================================================================
    // RenderFrame — единственная точка входа за кадр
    // ============================================================================
    void Renderer::RenderFrame(
        ID3D11DeviceContext* context,
        const XMMATRIX& view,
        const XMMATRIX& projection,
        Entity                       selectedEntity,
        const PhysicsDebugSettings& debugSettings)
    {



        // Проверка критических указателей
        if (m_direct3D == nullptr) OutputDebugStringA("? m_direct3D is null\n");
        if (m_camera == nullptr) OutputDebugStringA("? m_camera is null\n");
        if (m_sceneManager == nullptr) OutputDebugStringA("? m_sceneManager is null\n");

        // Проверка шейдеров
        auto checkPtr = [](void* p, const char* name) {
            if (p == nullptr) return;
            __try {
                volatile uintptr_t test = reinterpret_cast<uintptr_t>(p);
                (void)test;
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {
                char buf[256];
                sprintf_s(buf, "? %s is CORRUPTED (0x%p)\n", name, p);
                OutputDebugStringA(buf);
            }
            };

        checkPtr(m_lightShader, "m_lightShader");
        checkPtr(m_pbrShader, "m_pbrShader");
        checkPtr(m_skyShader, "m_skyShader");


        // 0. Обновляем направленный свет и environment map в шейдерах
        UpdateDirectionalLight(context);
        if (m_pbrShader)
        {
            m_pbrShader->SetEnvironmentMap(
                m_skyShader ? m_skyShader->GetSkyTexture() : nullptr);
        }

        // -- Pass: Shadow Map ------------------------------------------------------
        if (m_shadowMap)
        {
            m_shadowMap->SetRenderTarget(context);
            m_shadowMap->ClearRenderTarget(context);
            RenderToShadowMap(context, m_shadowMap->GetLightViewProjection());
            m_direct3D->SetBackBufferRenderTarget();
            m_direct3D->ResetViewport();
        }

        // -- Вычисляем матрицу отражения ------------------------------------------
        const XMMATRIX reflectionMatrix = BuildReflectionMatrix(0.0f);
        UpdateWaterMaterials(context, reflectionMatrix);

        // -- Pass: Reflection > m_reflectionTexture --------------------------------
        if (m_reflectionTexture)
        {
            m_reflectionTexture->SetRenderTarget(context);
            m_reflectionTexture->ClearRenderTarget(context, 0.f, 0.f, 0.f, 1.f);
            RenderExcluding(context, reflectionMatrix, projection, INVALID_ENTITY);
            m_direct3D->SetBackBufferRenderTarget();
            m_direct3D->ResetViewport();
            ClearSRVSlots(context);
        }

        // -- Pass: Refraction > m_refractionTexture -------------------------------
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
                const auto fog = m_sceneManager->BuildFogSettingsFromWorld();
                RenderMesh(context, mesh.model, world, view, projection,
                    mat, lightColors, lightPositions,
                    fog, false);
            }

            m_direct3D->SetBackBufferRenderTarget();
            m_direct3D->ResetViewport();
            ClearSRVSlots(context);
        }

        // -- Pass: Scene Preview > m_renderTexture (для EditorUI) -----------------
        if (m_renderTexture)
        {
            auto fog = m_sceneManager->BuildFogSettingsFromWorld();
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

        // -- Pass: Main scene > backbuffer -----------------------------------------
        m_direct3D->SetBackBufferRenderTarget();
        m_direct3D->ResetViewport();

        auto fog = m_sceneManager->BuildFogSettingsFromWorld();
        if (fog.enabled)
            m_direct3D->BeginScene(fog.color.x, fog.color.y, fog.color.z, 1.f);
        else
            m_direct3D->BeginScene(0.f, 0.f, 0.f, 1.f);

        if (m_skyShader && m_skyShader->GetSkyTexture())
        {
            // Skybox follows camera rotation, but ignores camera translation.
            // This keeps the cube visually infinitely far away while allowing
            // the player to look around inside the cubemap naturally.
            XMMATRIX skyView = view;
            skyView.r[3] = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
            XMMATRIX invSkyViewProjection = XMMatrixInverse(nullptr, skyView * projection);

            // 3. Сохраняем старые состояния
            ID3D11DepthStencilState* oldDS = nullptr;
            ID3D11RasterizerState* oldRS = nullptr;
            context->OMGetDepthStencilState(&oldDS, nullptr);
            context->RSGetState(&oldRS);

            

           
            
            context->RSSetState(m_skyRS);
            context->OMSetDepthStencilState(m_skyDepthState, 0);

            m_skyShader->UpdateFog(context, fog);
            m_skyShader->Render(context, invSkyViewProjection);

            context->RSSetState(oldRS);
            context->OMSetDepthStencilState(oldDS, 0);

            if (oldDS) oldDS->Release();
            if (oldRS) oldRS->Release();
        }

        Render3DObjects(context, view, projection,
            m_lightShader, m_pbrShader, m_defaultMaterial, selectedEntity);

        // -- Sprites ---------------------------------------------------------------
        RenderSprites(context, view, projection);

        // -- Compartments (solid) --------------------------------------------------
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

        // -- Physics debug (wireframe) ---------------------------------------------
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

        const auto fog = m_sceneManager->BuildFogSettingsFromWorld();

        ShaderManager::Get().ForEach([&](ShaderBaseClass* s) {
            s->UpdateFog(context, fog);
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
        if (m_shadowMap)
        {
            auto* terrainShader = ShaderManager::Get().GetAs<TerrainBlendShaderClass>("terrain_blend");
            if (terrainShader)
            {
                XMMATRIX lvp = m_shadowMap->GetLightViewProjection();
                terrainShader->SetShadowMap(context, m_shadowMap->GetShaderResourceView(), lvp);
            }
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
            fog,
            selectedEntity, camPos,
            TimeManager::UnscaledDeltaTime());

        if (m_grassRenderer)
        {
            const auto& world = m_sceneManager->GetWorldSettings();
            m_grassRenderer->Render(
                context,
                view,
                projection,
                camPos,
                TimeManager::UnscaledDeltaTime(),
                world.sunDirection,
                world.ambientStrength);
        }

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

        const auto fog = m_sceneManager->BuildFogSettingsFromWorld();

        ShaderManager::Get().ForEach([&](ShaderBaseClass* s) {
            s->UpdateFog(context, fog);
            });

        XMFLOAT4 lightColors[NUM_LIGHTS];
        XMFLOAT4 lightPositions[NUM_LIGHTS];

        m_sceneManager->RenderAllECSExcluding(
            context, view, projection,
            m_defaultMaterial,
            lightColors, lightPositions,
            fog,
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
            if (st != ShaderType::Light && st != ShaderType::PBR && st != ShaderType::TerrainBlend) continue;

            XMMATRIX world = transform->GetWorldMatrix();

            if (mesh.useAsyncMesh && m_sceneManager->GetAssetLoadSystem() && !mesh.asyncModelPath.empty())
            {
                uint32_t indexCount = 0;
                if (m_sceneManager->GetAssetLoadSystem()->BindMeshAsset(context, mesh.asyncModelPath, indexCount))
                {
                    m_shadowShader->Render(context, static_cast<int>(indexCount), world, lightViewProj);
                    continue;
                }
            }

            mesh.model->Render(context);
            m_shadowShader->Render(context, mesh.model->GetIndexCount(), world, lightViewProj);
        }

        // Shadow pass uses only VS slot b0. Keep the pixel-shader light buffers
        // bound: UpdateDirectionalLight prepared them for the following passes.
        ID3D11Buffer* nullCB = nullptr;
        ID3D11ShaderResourceView* nullSRV[8] = {};
        context->VSSetConstantBuffers(0, 1, &nullCB);
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
        const auto& world = m_sceneManager->GetWorldSettings();

        XMFLOAT3 sunDir = world.sunDirection;
        const float lenSq =
            sunDir.x * sunDir.x +
            sunDir.y * sunDir.y +
            sunDir.z * sunDir.z;
        if (lenSq < 0.0001f)
        {
            sunDir = { 0.3f, -1.0f, 0.5f };
        }

        const XMFLOAT4 sunColor = { 1.0f, 0.95f, 0.8f, 1.0f };
        const XMFLOAT4 ambientLight = { 0.8f, 0.9f, 1.0f, world.ambientStrength };

        // Проверка: просто на nullptr
        if (m_lightShader != nullptr)
        {
            m_lightShader->SetDirectionalLight(
                context,
                sunDir,
                sunColor,
                ambientLight,
                world.sunIntensity);
        }

        if (m_pbrShader != nullptr)
        {
            m_pbrShader->SetDirectionalLight(
                context,
                sunDir,
                sunColor,
                ambientLight,
                world.sunIntensity);
        }

        if (auto* terrainShader = ShaderManager::Get().GetAs<TerrainBlendShaderClass>("terrain_blend"))
        {
            terrainShader->SetDirectionalLight(
                context,
                sunDir,
                sunColor,
                ambientLight,
                world.sunIntensity);
        }
        UpdateShadowLightViewProjection();
    }

    void Renderer::UpdateShadowLightViewProjection()
    {
        if (!m_shadowMap || !m_sceneManager)
            return;

        const auto& world = m_sceneManager->GetWorldSettings();

        XMFLOAT3 sunDir = world.sunDirection;
        float lenSq = sunDir.x * sunDir.x + sunDir.y * sunDir.y + sunDir.z * sunDir.z;
        if (lenSq < 0.0001f)
        {
            sunDir = { 0.3f, -1.0f, 0.5f };
        }

        XMVECTOR lightDir = XMVector3Normalize(XMLoadFloat3(&sunDir));
        XMVECTOR sceneCenter = XMVectorZero();

        float shadowDistance = world.shadowDistance;
        if (shadowDistance < 10.0f)
            shadowDistance = 100.0f;

        XMVECTOR lightPos = XMVectorSubtract(sceneCenter, XMVectorScale(lightDir, shadowDistance));
        XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

        float dotUp = XMVectorGetX(XMVector3Dot(lightDir, up));
        if (dotUp > 0.95f || dotUp < -0.95f)
            up = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);

        XMMATRIX lightView = XMMatrixLookAtLH(lightPos, sceneCenter, up);
        XMMATRIX lightProj = XMMatrixOrthographicLH(
            shadowDistance,
            shadowDistance,
            0.5f,
            shadowDistance * 2.5f);

        m_shadowMap->SetLightViewProjection(lightView, lightProj);
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
