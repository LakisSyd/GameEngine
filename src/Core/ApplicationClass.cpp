#include "applicationclass.h"
#include <algorithm>
#include "FileSystem.h"

// ============================================================================
// Конструктор — все поля инициализируются в заголовке (= nullptr / = 0)
// ============================================================================
ApplicationClass::ApplicationClass() = default;

// ============================================================================
// Initialize
// ============================================================================
bool ApplicationClass::Initialize(int screenWidth, int screenHeight, HWND hwnd)
{
    m_screenWidth = screenWidth;
    m_screenHeight = screenHeight;

    // ── Direct3D ─────────────────────────────────────────────────────────────
    m_direct3D = std::make_unique<D3DClass>();
    if (!m_direct3D) return false;

    if (!m_direct3D->Initialize(screenWidth, screenHeight, VSYNC_ENABLED, hwnd,
        FULL_SCREEN, SCREEN_DEPTH, SCREEN_NEAR))
    {
        MessageBox(hwnd, L"Failed to initialize Direct3D", L"Error", MB_OK);
        return false;
    }

    // ── Файловая система ─────────────────────────────────────────────────────
    m_gameFs = std::make_unique<FileSystem>(fs::current_path() / "assets");
    m_gameFs->ScanFileSystem();

    // ── EditorUI ─────────────────────────────────────────────────────────────
    if (!m_editorUI.Initialize(hwnd, m_direct3D->GetDevice(), m_direct3D->GetDeviceContext(), m_gameFs.get()))
        return false;

    // ── Камера ───────────────────────────────────────────────────────────────
    m_camera = std::make_unique<CameraClass>();
    if (!m_camera) return false;
    m_camera->SetPosition(0.0f, 2.0f, -10.0f);
    m_camera->SetRotation(0.0f, 0.0f, 0.0f);

    // ── Fallback модель (для SpawnObject без указания модели) ─────────────────
    m_mainModel = new AssimpModelClass;
    if (!m_mainModel->Initialize(m_direct3D->GetDevice(), m_direct3D->GetDeviceContext(),
        "assets/model/sphere.obj", "assets/texture/stone.tga"))
    {
        MessageBox(hwnd, L"Failed to load main model", L"Error", MB_OK);
        return false;
    }
    m_sceneManager.SetFallbackModel(m_mainModel);

    // ── Модель для сфер-индикаторов света ────────────────────────────────────
    m_lightSphereModel = new AssimpModelClass;
    if (!m_lightSphereModel->Initialize(m_direct3D->GetDevice(), m_direct3D->GetDeviceContext(),
        "assets/model/sphere.obj", "assets/texture/stone.tga"))
    {
        MessageBox(hwnd, L"Failed to load light sphere model", L"Error", MB_OK);
        return false;
    }

    // ── Загрузка всех шейдеров ───────────────────────────────────────────────
    auto& sm = ShaderManager::Get();
    m_textureShader = sm.Load<TextureShaderClass>("texture", m_direct3D->GetDevice(), hwnd);

    m_multiTextureShader = sm.Load<MultiTextureShaderClass>("multitex", m_direct3D->GetDevice(), hwnd);
    m_lightMapShader = sm.Load<LightMapShaderClass>("lightmap", m_direct3D->GetDevice(), hwnd);
    m_normalMapShader = sm.Load<NormalMapShaderClass>("normalmap", m_direct3D->GetDevice(), hwnd);
    m_fogShader = sm.Load<FogShaderClass>("fog", m_direct3D->GetDevice(), hwnd);
    m_ClipPlaneShader = sm.Load<ClipPlaneShaderClass>("clip", m_direct3D->GetDevice(), hwnd);
    m_ColorShader = sm.Load<ColorShaderClass>("color", m_direct3D->GetDevice(), hwnd);
    m_WaterShader = sm.Load<WaterShaderClass>("water", m_direct3D->GetDevice(), hwnd);
    m_reflectionShader = sm.Load<ReflectionShaderClass>("reflection", m_direct3D->GetDevice(), hwnd);
    m_glassShader = sm.Load<GlassShaderClass>("glass", m_direct3D->GetDevice(), hwnd);
    m_projectionShader = sm.Load<projectionShaderClass>("projection", m_direct3D->GetDevice(), hwnd);
    m_lightShader = sm.Load<LightShaderClass>("light", m_direct3D->GetDevice(), hwnd);
    m_pbrShader = sm.Load<PBRShaderClass>("pbr", m_direct3D->GetDevice(), hwnd);




    if (!m_textureShader || !m_lightShader || !m_pbrShader ||
        !m_multiTextureShader || !m_lightMapShader || !m_normalMapShader)
    {
        OutputDebugStringA("One or more critical shaders failed to load. Initialize() abort.\n");
        return false;
    }
    // ── Render-to-texture для отражения и преломления ─────────────────────────
    m_reflectionTexture = new RenderTextureClass;
    if (!m_reflectionTexture->Initialize(m_direct3D->GetDevice(), 512, 512, SCREEN_DEPTH, SCREEN_NEAR, 1))
    {
        MessageBox(hwnd, L"Failed to initialize ReflectionTexture", L"Error", MB_OK);
        return false;
    }

    m_refractionTexture = new RenderTextureClass;
    if (!m_refractionTexture->Initialize(m_direct3D->GetDevice(), 512, 512, SCREEN_DEPTH, SCREEN_NEAR, 1))
    {
        MessageBox(hwnd, L"Failed to initialize RefractionTexture", L"Error", MB_OK);
        return false;
    }

    m_sceneManager.SetRefractionSRV(m_refractionTexture->GetShaderResourceView());








    m_editorUI.RefreshShaderList();

    // ── Сферы-индикаторы для источников света ────────────────────────────────
    m_sceneManager.SetupLightSpheres(m_lightSpheres, m_lightSphereModel);

    // ── Один источник света по умолчанию ─────────────────────────────────────
    m_sceneManager.SetLight(0, XMFLOAT3(-1.0f, 2.0f, -1.0f), XMFLOAT4(1.0f, 1.0f, 0.9f, 1.0f), true);


    // ── Default material для всех объектов без ручного назначения ─────────
    // Раньше было: создавали Material, но не инициализировали/не назначали.
    // Из-за этого у объектов shader оставался nullptr, и модель "не видна",
    // пока не сменишь shader в UI (SetObjectShader создаёт material).
    m_defaultMaterial = new MaterialClass();
    if (!m_defaultMaterial->Initialize(m_direct3D->GetDevice(), L"DefaultMaterial"))
    {
        OutputDebugStringA("Failed to initialize m_defaultMaterial\n");
        return false;
    }

    // Т.к. для .glb текстуры embedded, ShaderType::Texture тоже будет брать fallback
    // из model->GetMeshParts() (см. RenderMesh.h).
    m_defaultMaterial->SetShader(m_textureShader);

    // Базовая текстура-заглушка (если у модели нет embedded albedo).
    {
        m_defaultTexture = new TextureClass();
        if (m_defaultTexture->Initialize(
            m_direct3D->GetDevice(),
            m_direct3D->GetDeviceContext(),
            "assets/texture/stone.tga"))
        {
            m_defaultMaterial->SetMainTexture(m_defaultTexture);
        }
        else
        {
            delete m_defaultTexture;
            m_defaultTexture = nullptr;
        }
    }

    m_sceneManager.SetDefaultMaterial(m_defaultMaterial);




    // ── Render-to-texture для Scene Preview ──────────────────────────────────
    m_renderTexture = new RenderTextureClass;
    if (!m_renderTexture->Initialize(m_direct3D->GetDevice(), 512, 512, SCREEN_DEPTH, SCREEN_NEAR, 1))
    {
        MessageBox(hwnd, L"Failed to initialize RenderTexture", L"Error", MB_OK);
        return false;
    }

    m_renderQuad = new BitmapClass;
    if (!m_renderQuad->Initialize(m_direct3D->GetDevice(), m_direct3D->GetDeviceContext(),
        screenHeight, screenWidth, 256, 256))
    {
        MessageBox(hwnd, L"Failed to initialize RenderQuad", L"Error", MB_OK);
        return false;
    }
    m_renderQuad->SetRenderLocation(10, 10);

  

    return true;
}

// ============================================================================
// Shutdown
// ============================================================================
void ApplicationClass::Shutdown()
{
    if (m_lightSphereModel)
    {
        m_lightSphereModel->Shutdown();
        delete m_lightSphereModel;
        m_lightSphereModel = nullptr;
    }

    ShaderManager::Get().Shutdown();

    if (m_defaultMaterial)
    {
        m_defaultMaterial->Shutdown();
        delete m_defaultMaterial;
        m_defaultMaterial = nullptr;
    }

    if (m_defaultTexture)
    {
        m_defaultTexture->Shutdown();
        delete m_defaultTexture;
        m_defaultTexture = nullptr;
    }

    if (m_reflectionTexture)
    {
        m_reflectionTexture->Shutdown();
        delete m_reflectionTexture;
        m_reflectionTexture = nullptr;
    }

    if (m_refractionTexture)
    {
        m_refractionTexture->Shutdown();
        delete m_refractionTexture;
        m_refractionTexture = nullptr;
    }

    if (m_renderTexture)
    {
        m_renderTexture->Shutdown();
        delete m_renderTexture;
        m_renderTexture = nullptr;
    }

    if (m_renderQuad)
    {
        m_renderQuad->Shutdown();
        delete m_renderQuad;
        m_renderQuad = nullptr;
    }

    if (m_mainModel)
    {
        m_mainModel->Shutdown();
        delete m_mainModel;
        m_mainModel = nullptr;
    }

    if (m_direct3D)
        m_direct3D->Shutdown();

    m_editorUI.Shutdown();
}

// ============================================================================
// Frame
// ============================================================================
bool ApplicationClass::Frame(float dt)
{
    m_inputManager.Update(m_input);
    HandleCameraMovement(dt);
    m_sceneManager.UpdateAll(dt);

    // ── Анимация воды: обновляем UV-смещение для всех Water-объектов ──────────
    {
        auto& meshPool = m_sceneManager.GetECSRegistry().GetComponents<MeshRendererComponent>();
        for (auto& [entity, mesh] : meshPool)
        {
            if (!m_sceneManager.IsObjectActive(entity) || !mesh.material) continue;
            auto* shader = mesh.material->GetShader();
            if (!shader || shader->GetType() != ShaderType::Water) continue;

            float t = mesh.material->GetWaterTranslation();
            t += dt * 0.001f;
            if (t > 1.0f) t -= 1.0f;
            mesh.material->SetWaterTranslation(t);
        }
    }

    m_sceneManager.UpdateLights();

    m_camera->Render();
    m_camera->GetViewMatrix(m_viewMatrix);
    m_direct3D->GetProjectionMatrix(m_projMatrix);

    ID3D11DeviceContext* context = m_direct3D->GetDeviceContext();

    // ── Обновляем reflection/refraction данные для Water и Reflection объектов ─
    XMMATRIX reflectionMatrix = BuildReflectionMatrix(m_camera.get(), 0.0f);
    {
        auto& meshPool = m_sceneManager.GetECSRegistry().GetComponents<MeshRendererComponent>();
        for (auto& [entity, mesh] : meshPool)
        {
            if (!m_sceneManager.IsObjectActive(entity) || !mesh.material) continue;
            auto* shader = mesh.material->GetShader();
            if (!shader) continue;

            if (shader->GetType() == ShaderType::Water)
            {
                mesh.material->SetReflectionMatrix(reflectionMatrix);
                mesh.material->SetReflectionSRV(m_reflectionTexture->GetShaderResourceView());
                mesh.material->SetRefractionSRV(m_refractionTexture->GetShaderResourceView());
            }
            else if (shader->GetType() == ShaderType::Reflection)
            {
                mesh.material->SetReflectionMatrix(reflectionMatrix);
                mesh.material->SetReflectionSRV(m_reflectionTexture->GetShaderResourceView());
                mesh.material->SetReflectivity(0.5f);
            }
        }
    }

    // ── Pass 0: рендер ОТРАЖЕНИЯ → m_reflectionTexture ───────────────────────
    m_reflectionTexture->SetRenderTarget(context);
    m_reflectionTexture->ClearRenderTarget(context, 0.0f, 0.0f, 0.0f, 1.0f);
    Render3DObjectsExcluding(context, reflectionMatrix, m_projMatrix, -1);
    m_direct3D->SetBackBufferRenderTarget();
    m_direct3D->ResetViewport();
    {
        ID3D11ShaderResourceView* nullSRVs[8] = {};
        context->PSSetShaderResources(0, 8, nullSRVs);
    }

    // ── Pass 1: рендер ПРЕЛОМЛЕНИЯ → m_refractionTexture ─────────────────────
    m_refractionTexture->SetRenderTarget(context);
    m_refractionTexture->ClearRenderTarget(context, 0.0f, 0.0f, 0.0f, 1.0f);
    {
        auto& meshPool = m_sceneManager.GetECSRegistry().GetComponents<MeshRendererComponent>();
        XMFLOAT4 lightColors[NUM_LIGHTS];
        XMFLOAT4 lightPositions[NUM_LIGHTS];
        SetupLights(lightColors, lightPositions);

        for (auto& [entity, mesh] : meshPool)
        {
            if (!m_sceneManager.IsObjectActive(entity)) continue;

            auto* transform = m_sceneManager.GetObjectTransform(entity);
            if (!transform || !mesh.model) continue;

            auto* mat = mesh.material ? mesh.material : m_defaultMaterial;
            if (!mat) continue;

            // Пропускаем Glass/Water — они используют refraction-текстуру
            auto* shader = mat->GetShader();
            if (shader)
            {
                ShaderType st = shader->GetType();
                if (st == ShaderType::Glass || st == ShaderType::Water)
                    continue;
            }

            XMMATRIX world = transform->GetWorldMatrix();
            RenderMesh(context, mesh.model, world, m_viewMatrix, m_projMatrix,
                mat, lightColors, lightPositions,
                m_sceneManager.GetFogSettings(), false);
        }
    }
    m_direct3D->SetBackBufferRenderTarget();
    m_direct3D->ResetViewport();
    {
        ID3D11ShaderResourceView* nullSRVs[8] = {};
        context->PSSetShaderResources(0, 8, nullSRVs);
    }

    // ── Pass 2: preview в RenderTexture (для EditorUI) ────────────────────────
    m_renderTexture->SetRenderTarget(context);
    m_renderTexture->ClearRenderTarget(context,
        m_sceneManager.GetFogSettings().color.x,
        m_sceneManager.GetFogSettings().color.y,
        m_sceneManager.GetFogSettings().color.z, 1.0f);
    XMMATRIX rtProj;
    m_renderTexture->GetProjectionMatrix(rtProj);
    Render3DObjects(context, m_viewMatrix, rtProj);
    m_editorUI.SetScenePreviewTexture(m_renderTexture->GetShaderResourceView());
    {
        ID3D11ShaderResourceView* nullSRVs[8] = {};
        context->PSSetShaderResources(0, 8, nullSRVs);
    }

    // ── Pass 3: финальный рендер на экран ─────────────────────────────────────
    m_direct3D->SetBackBufferRenderTarget();
    m_direct3D->ResetViewport();

    if (m_sceneManager.GetFogSettings().enabled)
        m_direct3D->BeginScene(
            m_sceneManager.GetFogSettings().color.x,
            m_sceneManager.GetFogSettings().color.y,
            m_sceneManager.GetFogSettings().color.z, 1.0f);
    else
        m_direct3D->BeginScene(0.0f, 0.0f, 0.0f, 1.0f);

    Render3DObjects(context, m_viewMatrix, m_projMatrix);
    RenderLightSources(context, m_viewMatrix, m_projMatrix);
    RenderWorldSprites(context, m_viewMatrix, m_projMatrix);

    bool imguiWantsMouse = m_editorUI.WantCaptureMouse()
        || ImGuizmo::IsOver()
        || ImGuizmo::IsUsing();
    if (!imguiWantsMouse)
        HandleDragDrop();

    m_editorUI.Update(m_sceneManager);

    // ── Обработка запросов от EditorUI ───────────────────────────────────────
    {
        auto spawnReq = m_editorUI.ConsumeSpawnRequest();
        if (spawnReq.pending) SpawnObjectFromRequest(spawnReq);
    }
    {
        auto saveReq = m_editorUI.ConsumeSaveRequest();
        if (saveReq.pending)
        {
            if (m_sceneManager.SaveScene(saveReq.path))
                OutputDebugStringA(("Scene saved to " + saveReq.path + "\n").c_str());
            else
                OutputDebugStringA(("Failed to save scene to " + saveReq.path + "\n").c_str());
        }
    }
    {
        auto loadReq = m_editorUI.ConsumeLoadRequest();
        if (loadReq.pending)
        {
            m_sceneManager.LoadScene(loadReq.path,
                m_direct3D->GetDevice(), m_direct3D->GetDeviceContext());
            OutputDebugStringA(("Scene loaded from: " + loadReq.path + "\n").c_str());
        }
    }

    m_editorUI.Render();
    m_direct3D->EndScene();
    return true;
}

// ============================================================================
// SpawnObjectFromRequest
// ============================================================================
// ============================================================================
// SpawnObjectFromRequest
// ============================================================================
void ApplicationClass::SpawnObjectFromRequest(const EditorUI::SpawnRequest& req)
{
    OutputDebugStringA(("Spawning: " + req.modelPath + "\n").c_str());
    ID3D11Device* device = m_direct3D->GetDevice();
    ID3D11DeviceContext* context = m_direct3D->GetDeviceContext();

    AssimpModelClass* model = m_sceneManager.GetOrCreateModel(
        req.modelPath, device, context, req.texturePath);

    if (!model)
    {
        OutputDebugStringA("SpawnObjectFromRequest: model FAILED\n");
        return;
    }

    auto* mat = new MaterialClass();
    mat->Initialize(device, L"SpawnedMaterial ");

    // --- ПРАВИЛЬНАЯ ЛОГИКА ОПРЕДЕЛЕНИЯ ТИПА МОДЕЛИ ---
    std::string modelPathLower = req.modelPath;
    std::transform(modelPathLower.begin(), modelPathLower.end(), modelPathLower.begin(), ::tolower);

    ShaderBaseClass* initShader = nullptr;

    // Для GLB/GLTF — всегда PBR
    if (modelPathLower.ends_with(".glb") || modelPathLower.ends_with(".gltf"))
    {
        initShader = m_pbrShader;
        OutputDebugStringA(("Auto-assigning PBR shader for GLB/GLTF: " + req.modelPath + "\n").c_str());
    }
    else
    {
        // Для .obj и других — LightShader (для освещения)
        initShader = m_lightShader;
        OutputDebugStringA(("Auto-assigning Light shader for: " + req.modelPath + "\n").c_str());
    }

    if (!initShader)
    {
        OutputDebugStringA("SpawnObjectFromRequest: initial shader is NULL!\n");
        delete mat;
        return;
    }

    mat->SetShader(initShader);

    // --- ЗАГРУЗКА ВНЕШНЕЙ ТЕКСТУРЫ (если указана) ---
    if (!req.texturePath.empty())
    {
        TextureClass* tex = new TextureClass();
        if (tex->Initialize(device, context, req.texturePath.c_str()))
        {
            mat->SetMainTexture(tex);
            OutputDebugStringA(("Assigned external texture: " + req.texturePath + "\n").c_str());
        }
        else
        {
            delete tex;
            // Если загрузка не удалась — используем дефолтную текстуру (stone.tga)
            if (m_defaultTexture)
            {
                mat->SetMainTexture(m_defaultTexture);
                OutputDebugStringA("Assigned default texture (stone.tga)\n");
            }
        }
    }
    // Если req.texturePath.empty() — мы НЕ назначаем MainTexture.
    // Но это нормально: для LightShader в RenderMesh.h будет использоваться material->GetMainTexture(),
    // а у m_defaultMaterial установлена на stone.tga (см. Initialize в ApplicationClass.cpp).

    Entity newEntity = m_sceneManager.SpawnObject(model, req.posX, req.posY, req.posZ);
    m_sceneManager.SetObjectMaterial(newEntity, mat);

    m_editorUI.SetSelection(EditorUI::SelectedType::Object, newEntity);

    char buf[256];
    sprintf_s(buf, "Spawned entity=%u at (%.1f, %.1f, %.1f)\n",
        newEntity, req.posX, req.posY, req.posZ);
    OutputDebugStringA(buf);
}

// (остальной код файла остается без изменений)
// ============================================================================
// Render3DObjects
// ============================================================================
void ApplicationClass::Render3DObjects(
    ID3D11DeviceContext* context,
    const XMMATRIX& view,
    const XMMATRIX& projection)
{
    m_direct3D->TurnZBufferOn();
    m_direct3D->TurnAlphaBlendingOff();

    ShaderManager::Get().ForEach([&](ShaderBaseClass* shader) {
        shader->UpdateFog(context, m_sceneManager.GetFogSettings());
        });

    XMFLOAT4 lightColors[NUM_LIGHTS];
    XMFLOAT4 lightPositions[NUM_LIGHTS];
    SetupLights(lightColors, lightPositions);

    Entity selectedEntity = (m_editorUI.GetSelectedType() == EditorUI::SelectedType::Object)
        ? m_editorUI.GetSelectedEntity() : INVALID_ENTITY;

    XMFLOAT3 camPos = m_camera->GetPosition(); // ← берём реальную позицию

    m_sceneManager.RenderAllECS(
        context, view, projection,
        m_defaultMaterial,
        lightColors, lightPositions,
        m_sceneManager.GetFogSettings(),
        selectedEntity,
        camPos);   // ← передаём


    // Пример: в конце Render3DObjects перед обычным сценовым рендером

}
// ============================================================================
// Render3DObjectsExcluding
// ============================================================================
void ApplicationClass::Render3DObjectsExcluding(
    ID3D11DeviceContext* context,
    const XMMATRIX& view,
    const XMMATRIX& projection,
    int excludeIndex)
{
    m_direct3D->TurnZBufferOn();
    m_direct3D->TurnAlphaBlendingOff();

    ShaderManager::Get().ForEach([&](ShaderBaseClass* shader) {
        shader->UpdateFog(context, m_sceneManager.GetFogSettings());
        });

    XMFLOAT4 lightColors[NUM_LIGHTS];
    XMFLOAT4 lightPositions[NUM_LIGHTS];
    SetupLights(lightColors, lightPositions);

    Entity excludeEntity = (excludeIndex >= 0)
        ? static_cast<Entity>(excludeIndex) : INVALID_ENTITY;

    m_sceneManager.RenderAllECSExcluding(
        context, view, projection,
        m_defaultMaterial,
        lightColors, lightPositions,
        m_sceneManager.GetFogSettings(),
        excludeEntity);
}

// ============================================================================
// RenderLightSources
// ============================================================================
void ApplicationClass::RenderLightSources(
    ID3D11DeviceContext* context,
    const XMMATRIX& view,
    const XMMATRIX& projection)
{
    XMFLOAT4 lightColors[NUM_LIGHTS];
    XMFLOAT4 lightPositions[NUM_LIGHTS];
    SetupLights(lightColors, lightPositions);

    for (size_t i = 0; i < m_lightSpheres.size() && i < NUM_LIGHTS; ++i)
    {
        if (lightPositions[i].w <= 0.0f ||
            (lightColors[i].x == 0 && lightColors[i].y == 0 && lightColors[i].z == 0))
            continue;

        m_lightSpheres[i].SetPosition(
            lightPositions[i].x, lightPositions[i].y, lightPositions[i].z);
        m_lightSpheres[i].RenderSimple(context, view, projection, m_textureShader);
    }
}

// ============================================================================
// RenderWorldSprites
// ============================================================================
void ApplicationClass::RenderWorldSprites(
    ID3D11DeviceContext* context,
    const XMMATRIX& view,
    const XMMATRIX& projection)
{
    m_direct3D->TurnAlphaBlendingOn();
    m_sceneManager.RenderSprites(context, view, projection, m_textureShader);
    m_direct3D->TurnAlphaBlendingOff();
}

// ============================================================================
// SetupLights
// ============================================================================
void ApplicationClass::SetupLights(XMFLOAT4 colors[], XMFLOAT4 positions[])
{
    for (int i = 0; i < NUM_LIGHTS; ++i)
    {
        colors[i] = m_sceneManager.GetLightColor(i);
        positions[i] = m_sceneManager.GetLightPosition(i);
    }
}

// ============================================================================
// RayIntersectsSphere
// ============================================================================
bool RayIntersectsSphere(const CameraClass::Ray& ray,
    const XMFLOAT3& sphereCenter, float sphereRadius)
{
    XMVECTOR L = XMLoadFloat3(&sphereCenter) - XMLoadFloat3(&ray.origin);
    float t = XMVectorGetX(XMVector3Dot(L, XMLoadFloat3(&ray.direction)));
    if (t < 0.0f) return false;

    XMVECTOR closest = XMLoadFloat3(&ray.origin) +
        XMVectorScale(XMLoadFloat3(&ray.direction), t);
    float dist = XMVectorGetX(XMVector3Length(closest - XMLoadFloat3(&sphereCenter)));
    return dist <= sphereRadius;
}

// ============================================================================
// HandleCameraMovement
// ============================================================================
void ApplicationClass::HandleCameraMovement(float dt)
{
    dt = min(dt, 0.05f);

    bool rightDown = m_inputManager.IsCameraMode();

    if (rightDown && !m_inputManager.IsMouseCaptured(m_input) && !m_editorUI.WantCaptureMouse())
        m_inputManager.SetMouseCapture(m_input, true);
    else if (!rightDown && m_inputManager.IsMouseCaptured(m_input))
        m_inputManager.SetMouseCapture(m_input, false);

    if (m_inputManager.IsMouseCaptured(m_input))
    {
        int deltaX, deltaY;
        m_input->GetMouseDelta(deltaX, deltaY);
        m_inputManager.ResetMouseDelta(m_input);

        float rotX = m_camera->GetRotation().x + deltaY * Settings::CAMERA_SENSITIVITY;
        float rotY = m_camera->GetRotation().y + deltaX * Settings::CAMERA_SENSITIVITY;
        rotX = clamp(rotX, -89.0f, 89.0f);
        m_camera->SetRotation(rotX, rotY, m_camera->GetRotation().z);
    }

    {
        float posX = m_camera->GetPosition().x;
        float posY = m_camera->GetPosition().y;
        float posZ = m_camera->GetPosition().z;

        float speed = Settings::INPUT_MOVE_SPEED;
        if (m_inputManager.IsKeyDown(m_input, VK_SHIFT))   speed *= 2.0f;
        if (m_inputManager.IsKeyDown(m_input, VK_CONTROL)) speed *= 0.5f;

        float yawRad = XMConvertToRadians(m_camera->GetRotation().y);
        float sinYaw = sinf(yawRad), cosYaw = cosf(yawRad);

        if (m_inputManager.IsKeyDown(m_input, 'W')) { posX += sinYaw * speed * dt; posZ += cosYaw * speed * dt; }
        if (m_inputManager.IsKeyDown(m_input, 'S')) { posX -= sinYaw * speed * dt; posZ -= cosYaw * speed * dt; }
        if (m_inputManager.IsKeyDown(m_input, 'A')) { posX -= cosYaw * speed * dt; posZ += sinYaw * speed * dt; }
        if (m_inputManager.IsKeyDown(m_input, 'D')) { posX += cosYaw * speed * dt; posZ -= sinYaw * speed * dt; }
        if (m_inputManager.IsKeyDown(m_input, 'R')) posY += speed * dt;
        if (m_inputManager.IsKeyDown(m_input, 'F')) posY -= speed * dt;

        m_camera->SetPosition(posX, posY, posZ);
    }
}

// ============================================================================
// HandleDragDrop
// ============================================================================
void ApplicationClass::HandleDragDrop()
{
    if (m_inputManager.IsDragging() && m_inputManager.IsLeftDown())
    {
        CameraClass::Ray ray = m_camera->CreatePickingRay(
            m_inputManager.GetMouseX(), m_inputManager.GetMouseY(),
            m_screenWidth, m_screenHeight, m_viewMatrix, m_projMatrix);

        if (!m_isDragging && m_pendingPickResult.IsValid())
        {
            switch (m_pendingPickResult.type)
            {
            case PickingSystem::PickedType::Light:
                StartDraggingLight(m_pendingPickResult.index, ray);
                break;
            case PickingSystem::PickedType::Object:
                if (auto* t = m_sceneManager.GetObjectTransform(
                    static_cast<Entity>(m_pendingPickResult.index)))
                    StartDraggingObject(m_pendingPickResult.index, ray, t->position);
                break;
            case PickingSystem::PickedType::Sprite:
                if (auto* spr = m_sceneManager.GetSprite(m_pendingPickResult.index))
                    StartDraggingSprite(m_pendingPickResult.index, ray, spr->GetPosition());
                break;
            }
        }

        if (m_isDragging)
            UpdateDraggedPosition(ray);

        return;
    }

    if (!m_inputManager.IsDragging() && !m_inputManager.IsLeftDown() && m_isDragging)
    {
        StopDragging();
        m_pendingPickResult = {};
        return;
    }

    if (m_inputManager.IsCameraMode())
        return;

    if (m_inputManager.IsLeftDown() && !m_isDragging)
    {
        m_pendingPickResult = m_pickingSystem.PickAt(
            m_inputManager.GetMouseX(), m_inputManager.GetMouseY(),
            *m_camera, m_viewMatrix, m_projMatrix,
            m_sceneManager, m_lightSpheres,
            m_screenWidth, m_screenHeight);

        if (m_pendingPickResult.IsValid())
        {
            switch (m_pendingPickResult.type)
            {
            case PickingSystem::PickedType::Object:
                m_editorUI.SetSelection(
                    EditorUI::SelectedType::Object,
                    static_cast<Entity>(m_pendingPickResult.index));
                break;
            case PickingSystem::PickedType::Light:
                m_editorUI.SetSelection(EditorUI::SelectedType::Light,
                    m_pendingPickResult.index);
                break;
            case PickingSystem::PickedType::Sprite:
                m_editorUI.SetSelection(EditorUI::SelectedType::Sprite,
                    m_pendingPickResult.index);
                break;
            }
        }
        else
        {
            m_pendingPickResult = {};
        }
    }
}

// ============================================================================
// Drag helpers
// ============================================================================
void ApplicationClass::StartDraggingLight(int index, const CameraClass::Ray& pickRay)
{
    m_isDragging = true;
    m_draggedLightIndex = index;
    m_draggedObjectIndex = -1;
    m_draggedSpriteIndex = -1;
    m_dragOffset = { 0, 0, 0 };

    m_sceneManager.DetachLight(index);

    XMFLOAT4 lightPos4 = m_sceneManager.GetLightPosition(index);
    XMFLOAT3 lightPos = { lightPos4.x, lightPos4.y, lightPos4.z };

    XMVECTOR toLight = XMLoadFloat3(&lightPos) - XMLoadFloat3(&pickRay.origin);
    m_dragDistance = max(1.0f, XMVectorGetX(XMVector3Length(toLight)));
}

void ApplicationClass::StartDraggingSprite(int index,
    const CameraClass::Ray& pickRay, const XMFLOAT3& spritePos)
{
    m_isDragging = true;
    m_draggedSpriteIndex = index;
    m_draggedLightIndex = -1;
    m_draggedObjectIndex = -1;

    XMVECTOR toSprite = XMLoadFloat3(&spritePos) - XMLoadFloat3(&pickRay.origin);
    m_dragDistance = max(1.0f, XMVectorGetX(XMVector3Length(toSprite)));
    m_dragOffset = { 0, 0, 0 };
}

void ApplicationClass::StartDraggingObject(int index,
    const CameraClass::Ray& pickRay, const XMFLOAT3& objPos)
{
    m_isDragging = true;
    m_draggedObjectIndex = index;
    m_draggedLightIndex = -1;
    m_draggedSpriteIndex = -1;

    XMVECTOR toObj = XMLoadFloat3(&objPos) - XMLoadFloat3(&pickRay.origin);
    m_dragDistance = max(1.0f, XMVectorGetX(XMVector3Length(toObj)));
    m_dragOffset = { 0, 0, 0 };
}

void ApplicationClass::UpdateDraggedPosition(const CameraClass::Ray& ray)
{
    XMFLOAT3 newPos = {
        ray.origin.x + ray.direction.x * m_dragDistance + m_dragOffset.x,
        ray.origin.y + ray.direction.y * m_dragDistance + m_dragOffset.y,
        ray.origin.z + ray.direction.z * m_dragDistance + m_dragOffset.z
    };

    if (m_draggedLightIndex >= 0)
    {
        m_sceneManager.SetLight(m_draggedLightIndex, newPos, XMFLOAT4(1, 1, 0.9f, 1), true);
    }
    else if (m_draggedSpriteIndex >= 0)
    {
        if (auto* spr = m_sceneManager.GetSprite(m_draggedSpriteIndex))
            spr->SetPosition(newPos.x, newPos.y, newPos.z);
    }
    else if (m_draggedObjectIndex >= 0)
    {
        if (auto* t = m_sceneManager.GetObjectTransform(
            static_cast<Entity>(m_draggedObjectIndex)))
        {
            t->position = newPos;
        }
    }
}

void ApplicationClass::StopDragging()
{
    m_isDragging = false;
    m_draggedObjectIndex = -1;
    m_draggedLightIndex = -1;
    m_draggedSpriteIndex = -1;
    m_dragDistance = 0.0f;
}

// ============================================================================
// BuildReflectionMatrix
// ============================================================================
XMMATRIX ApplicationClass::BuildReflectionMatrix(CameraClass* camera, float reflectY)
{
    XMFLOAT3 pos = camera->GetPosition();
    XMFLOAT3 rot = camera->GetRotation();

    float reflectedPosY = 2.0f * reflectY - pos.y;
    float pitch = rot.x * 0.0174532925f;
    float yaw = rot.y * 0.0174532925f;

    XMMATRIX rotMat = XMMatrixRotationRollPitchYaw(pitch, yaw, 0.0f);
    XMVECTOR forward = XMVector3TransformCoord(XMVectorSet(0, 0, 1, 0), rotMat);
    XMVECTOR up = XMVector3TransformCoord(XMVectorSet(0, 1, 0, 0), rotMat);

    XMVECTOR reflectPlane = XMVectorSet(0.0f, 1.0f, 0.0f, -reflectY);
    XMMATRIX reflectMat = XMMatrixReflect(reflectPlane);

    XMVECTOR reflPos = XMVectorSet(pos.x, reflectedPosY, pos.z, 1.0f);
    XMVECTOR reflForward = XMVector3TransformNormal(forward, reflectMat);
    XMVECTOR reflUp = XMVector3TransformNormal(up, reflectMat);

    return XMMatrixLookAtLH(reflPos, XMVectorAdd(reflPos, reflForward), reflUp);
}