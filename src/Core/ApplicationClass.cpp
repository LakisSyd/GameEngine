
#include "applicationclass.h"
#include <algorithm>
#include <array>
#include <cmath>
#include "FileSystem.h"
#include "PhysicsSystem.h"
#include "helper.h"
#include "TimeManager.h"
#include "PressureSystem.h"
#include "SubmarineJointSystem.h"
#include "FloodingSystem.h"

#include "JointConnectionSystem.h"
#include "../../SubmarineSystem.h"
#include "PhysicsWorld.h"
#include "../../Renderer.h"
#include "../../SkyShaderClass.h"
#include <DirectXTex.h>  
#include "directxtk/DDSTextureLoader.h"
#include <stb_image.h>
#include "../../CharacterSpawnSystem.h"
#include "../../CharacterMovementSystem.h"
// ============================================================================
// Конструктор — все поля инициализируются в заголовке (= nullptr / = 0)
// ============================================================================


ID3D11ShaderResourceView* LoadEquirectangularHDR(
    ID3D11Device* device,
    ID3D11DeviceContext* context,
    const std::string& path)
{
    int width = 0, height = 0, channels = 0;

    // stb_image загружает HDR в float формате (32-bit per channel)
    float* hdrData = stbi_loadf(path.c_str(), &width, &height, &channels, 3);
    if (!hdrData)
    {
        OutputDebugStringA(("Failed to load HDR: " + path + "\n").c_str());
        return nullptr;
    }

    // Создаём 2D текстуру с float форматом
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R32G32B32_FLOAT;  // 🔥 HDR формат
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = 0;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = hdrData;
    initData.SysMemPitch = width * 3 * sizeof(float);  // 3 канала * 4 байта

    ID3D11Texture2D* tex2D = nullptr;
    if (FAILED(device->CreateTexture2D(&desc, &initData, &tex2D)))
    {
        stbi_image_free(hdrData);
        return nullptr;
    }

    // Создаём SRV
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = desc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = 1;

    ID3D11ShaderResourceView* srv = nullptr;
    if (FAILED(device->CreateShaderResourceView(tex2D, &srvDesc, &srv)))
    {
        tex2D->Release();
        stbi_image_free(hdrData);
        return nullptr;
    }

    tex2D->Release();
    stbi_image_free(hdrData);

    OutputDebugStringA(("✅ HDR loaded: " + path + "\n").c_str());
    return srv;
}

// В ApplicationClass.cpp


ID3D11ShaderResourceView* LoadSkyboxCubeMap(
    ID3D11Device* device,
    ID3D11DeviceContext* context,
    const std::string& path)
{
    ID3D11Resource* resource = nullptr;
    ID3D11ShaderResourceView* srv = nullptr;

    HRESULT hr = CreateDDSTextureFromFile(
        device,
        context,
        std::wstring(path.begin(), path.end()).c_str(),
        &resource,
        &srv);

    if (FAILED(hr) || !srv)
    {
        OutputDebugStringA(("❌ Failed to load cube map: " + path + "\n").c_str());
        if (resource) resource->Release();
        return nullptr;
    }

    // 🔥 ПРАВИЛЬНАЯ ПРОВЕРКА: смотрим на ViewDimension в SRV
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srv->GetDesc(&srvDesc);

    if (srvDesc.ViewDimension != D3D11_SRV_DIMENSION_TEXTURECUBE)
    {
        OutputDebugStringA("❌ Loaded SRV is not a TEXTURECUBE!\n");

        // 🔍 Отладочная информация
        char msg[256];
        sprintf_s(msg, "   ViewDimension: %d (expected %d = TEXTURECUBE)\n",
            srvDesc.ViewDimension, D3D11_SRV_DIMENSION_TEXTURECUBE);
        OutputDebugStringA(msg);

        if (srv) srv->Release();
        if (resource) resource->Release();
        return nullptr;
    }

    // Опционально: проверить формат
    if (srvDesc.Format != DXGI_FORMAT_R16G16B16A16_FLOAT &&
        srvDesc.Format != DXGI_FORMAT_R32G32B32A32_FLOAT)
    {
        OutputDebugStringA("⚠️ Warning: Cube map format may not be HDR float\n");
    }

    if (resource) resource->Release();  // SRV держит ссылку
    OutputDebugStringA(("✅ Cube map loaded successfully: " + path + "\n").c_str());
    return srv;
}

ID3D11ShaderResourceView* LoadSkyboxCubemap(
    ID3D11Device* device,
    ID3D11DeviceContext* context,
    const std::string& basePath);

struct SkyboxPreset
{
    const char* name;
    const char* ddsPath;
    const char* faceBasePath;
};

ID3D11ShaderResourceView* LoadFirstAvailableSkybox(
    ID3D11Device* device,
    ID3D11DeviceContext* context)
{
    // Preferred layout:
    //   assets/texture/cubemap/day.dds
    //   assets/texture/cubemap/evening.dds
    //   assets/texture/cubemap/night.dds
    // Alternative 6-face layout uses suffixes: _px, _nx, _py, _ny, _pz, _nz.
    // Example: assets/texture/cubemap/day/sky_px.jpg ... sky_nz.jpg
    constexpr std::array<SkyboxPreset, 4> presets = {{
        { "day",     "assets/texture/cubemap/day.dds",     "assets/texture/cubemap/day/sky" },
        { "evening", "assets/texture/cubemap/evening.dds", "assets/texture/cubemap/evening/sky" },
        { "night",   "assets/texture/cubemap/night.dds",   "assets/texture/cubemap/night/sky" },
        { "legacy",  "assets/texture/cubemap/sky_cube1.dds", nullptr }
    }};

    for (const auto& preset : presets)
    {
        if (preset.ddsPath)
        {
            if (auto* srv = LoadSkyboxCubeMap(device, context, preset.ddsPath))
            {
                OutputDebugStringA((std::string("✅ Skybox preset loaded from DDS: ") +
                    preset.name + "\n").c_str());
                return srv;
            }
        }

        if (preset.faceBasePath)
        {
            if (auto* srv = LoadSkyboxCubemap(device, context, preset.faceBasePath))
            {
                OutputDebugStringA((std::string("✅ Skybox preset loaded from 6 faces: ") +
                    preset.name + "\n").c_str());
                return srv;
            }
        }
    }

    OutputDebugStringA("⚠️ No skybox preset found. See docs/SKYBOX_ASSETS.md\n");
    return nullptr;
}

ApplicationClass::ApplicationClass() = default;

// ============================================================================
// Initialize
// ============================================================================
bool ApplicationClass::Initialize(int screenWidth, int screenHeight, HWND hwnd)
{
    m_hwnd = hwnd;
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


    {
        D3D11_RASTERIZER_DESC rsDesc = {};
        rsDesc.FillMode = D3D11_FILL_WIREFRAME;  // ← КЛЮЧЕВОЙ ПАРАМЕТР
        rsDesc.CullMode = D3D11_CULL_NONE;        // Не отсекать грани (видно изнутри)
        rsDesc.FrontCounterClockwise = FALSE;
        rsDesc.DepthBias = 0;
        rsDesc.SlopeScaledDepthBias = 0.0f;
        rsDesc.DepthClipEnable = TRUE;
        rsDesc.ScissorEnable = FALSE;
        rsDesc.MultisampleEnable = FALSE;
        rsDesc.AntialiasedLineEnable = TRUE;      // Сглаживание линий (опционально)

        DX_CHECK(m_direct3D->GetDevice()->CreateRasterizerState(&rsDesc, &m_wireframeRS));
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
    m_sceneManager.SetLightModel(m_lightSphereModel);   // ✅ НОВОЕ
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
    m_SkyShader = sm.Load<SkyShaderClass>("sky",m_direct3D->GetDevice(), hwnd);
    

    m_physicsDebugRenderer = new PhysicsDebugRenderer();
    if (!m_physicsDebugRenderer->Initialize(m_direct3D->GetDevice()))
    {
        MessageBox(hwnd, L"Failed to initialize PhysicsDebugRenderer", L"Error", MB_OK);
        return false;
    }

    m_compartmentRenderer = new CompartmentDebugRenderer();
    if (!m_compartmentRenderer->Initialize(m_direct3D->GetDevice()))
    {
        MessageBox(hwnd, L"Failed to initialize CompartmentDebugRenderer", L"Error", MB_OK);
        return false;
    }

    m_colorShader = new ColorShaderClass();

    m_colorShader->Initialize(m_direct3D->GetDevice(), hwnd);
    // Инициализируем физику
    if (!PhysicsSystem::InitializeWorld())
    {
        MessageBox(hwnd, L"Failed to initialize Bullet Physics", L"Error", MB_OK);
        return false;
    }


    m_shadowMap = new ShadowMap();
    if (!m_shadowMap->Initialize(m_direct3D->GetDevice(), 2048, 2048))
    {
        MessageBox(hwnd, L"Failed to init ShadowMap", L"Error", MB_OK);
        return false;
    }

    // ══════════════════════════════════════════════════════════
    // SHADOW SHADER
    // ══════════════════════════════════════════════════════════
    m_shadowShader = new ShadowShaderClass();
    if (!m_shadowShader->Initialize(m_direct3D->GetDevice(), hwnd))
    {
        MessageBox(hwnd, L"Failed to init ShadowShader", L"Error", MB_OK);
        return false;
    }

    XMVECTOR lightPos = XMVectorSet(10, 20, -10, 1);
    XMVECTOR lightTarget = XMVectorSet(0, 0, 0, 1);
    XMVECTOR up = XMVectorSet(0, 1, 0, 0);

    XMMATRIX lightView = XMMatrixLookAtLH(lightPos, lightTarget, up);

    // 🔥 FIX: Расширяем ортогональную проекцию (была 30x30, теперь 50x50)
    // Это позволит охватить все объекты
    XMMATRIX lightProj = XMMatrixOrthographicLH(100, 100, 0.5f, 200.0f);

    m_shadowMap->SetLightViewProjection(lightView, lightProj);

    m_shadowMap->SetLightViewProjection(lightView, lightProj);
    // Создаём тестовую физическую сцену
   // CreatePhysicsTestScene();



    // Создаём визуализатор отладочных контактов
    m_debugVisualizer = new DebugPhysicsVisualizer();


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





   

	TestEntity = m_sceneManager.SpawnObject(m_mainModel, 0, 0, 0);
    if (TestEntity != INVALID_ENTITY)
    {
        auto& registry = m_sceneManager.GetECSRegistry();
        registry.AddComponent<PlayerGameTag>(TestEntity, PlayerGameTag{});
        registry.AddComponent<CharacterMovement>(TestEntity, CharacterMovement{});
        if (auto* name = registry.GetComponent<NameComponent>(TestEntity))
            name->name = "Player";
    }

    m_editorUI.RefreshShaderList();

    // ── Сферы-индикаторы для источников света ────────────────────────────────
    m_sceneManager.SetupLightSpheres(m_lightSpheres, m_lightSphereModel);

    // ── Один источник света по умолчанию ─────────────────────────────────────


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


    m_boxModel = new AssimpModelClass;
    if (!m_boxModel->Initialize(m_direct3D->GetDevice(), m_direct3D->GetDeviceContext(),
        "assets/model/sphere.obj", "assets/texture/stone.tga")) // или используй stone.tga
    {
        // Если куба нет — можно временно использовать сферу, но будет неточно
        OutputDebugStringA("Warning: Failed to load cube.obj for debug visualizer\n");
        delete m_boxModel;
        m_boxModel = m_mainModel; // fallback на сферу
    }

    // ── Модель для капсул (опционально) ─────────────────────────────
    m_capsuleModel = new AssimpModelClass;
    if (!m_capsuleModel->Initialize(m_direct3D->GetDevice(), m_direct3D->GetDeviceContext(),
        "assets/model/capsule.obj", "assets/texture/wireframe.tga"))
    {
        OutputDebugStringA("Warning: Failed to load capsule.obj, capsule debug disabled\n");
        delete m_capsuleModel;
        m_capsuleModel = nullptr;
    }

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

    auto* mat = new MaterialClass();
    mat->Initialize(m_direct3D->GetDevice(), L"ProjectorMat");
    mat->SetShader(m_projectionShader);

    // Устанавливаем основную текстуру (слот 0)
    TextureClass* colorTex = new TextureClass();
    colorTex->Initialize(m_direct3D->GetDevice(), m_direct3D->GetDeviceContext(), "assets/texture/stone.tga");
    mat->SetTexture(0, colorTex);

   

    m_renderer = new Renderer(
        m_direct3D.get(),           // D3DClass*
        m_camera.get(),             // CameraClass*
        &m_sceneManager,            // SceneManager*
        m_shadowMap                 // ShadowMap*
    );

    // Теперь можно безопасно настраивать шейдеры:
    m_renderer->SetLightShader(m_lightShader);
    m_renderer->SetPBRShader(m_pbrShader);
    m_renderer->SetSkyShader(m_SkyShader);
    m_renderer->SetDefaultMaterial(m_defaultMaterial);
    m_renderer->SetTextureShader(m_textureShader);
    m_renderer->SetColorShader(m_colorShader);
    m_renderer->SetShadowShader(m_shadowShader);

    // Настройка рендер-текстур:
    m_renderer->SetRenderTextures(
        m_reflectionTexture,
        m_refractionTexture,
        m_renderTexture);

    // Отладочные рендереры:
    m_renderer->SetPhysicsDebugRenderer(m_physicsDebugRenderer);
    m_renderer->SetCompartmentRenderer(m_compartmentRenderer);
    m_renderer->SetWireframeRS(m_wireframeRS);

   
    m_dragDropSystem = std::make_unique<DragDropSystem>(
        m_camera.get(), &m_inputManager, m_input, &m_editorUI, &m_sceneManager);

    m_skySRV = LoadFirstAvailableSkybox(
        m_direct3D->GetDevice(),
        m_direct3D->GetDeviceContext());

    if (!m_skySRV)
    {
        OutputDebugStringA("⚠️ Warning: Failed to load skybox, using fallback\n");
        // Можно создать заглушку или продолжить без неба
    }
    else
    {
        OutputDebugStringA("✅ Skybox loaded successfully\n");
    }

    // Передаём в рендерер
    if (m_renderer && m_SkyShader)
    {
        m_renderer->SetSkyShader(m_SkyShader);
        m_SkyShader->SetSkyTexture(m_skySRV);  // ← Новый метод в шейдере
    }

    

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

    if (m_compartmentRenderer)
    {
        m_compartmentRenderer->Shutdown();
        delete m_compartmentRenderer;
        m_compartmentRenderer = nullptr;
    }

    if (m_physicsDebugRenderer)
    {
        m_physicsDebugRenderer->Shutdown();
        delete m_physicsDebugRenderer;
        m_physicsDebugRenderer = nullptr;
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

    if (m_debugVisualizer)
    {
        delete m_debugVisualizer;
        m_debugVisualizer = nullptr;
    }

    if (m_shadowShader)
    {
        m_shadowShader->Shutdown();
        delete m_shadowShader;
        m_shadowShader = nullptr;
    }

    if (m_shadowMap)
    {
        m_shadowMap->Shutdown();
        delete m_shadowMap;
        m_shadowMap = nullptr;
    }

    if (m_renderer)  // Проверка перед удалением
    {
      
        delete m_renderer;
        m_renderer = nullptr;  // Обязательно обнуляем!
    }

    if (m_direct3D)
        m_direct3D->Shutdown();
    PhysicsWorld::Get().Shutdown();
    m_editorUI.Shutdown();
}

bool ApplicationClass::Frame(float dt)
{
    // 1. Получаем настройки отладки и обновляем input в начале кадра.
    // Camera movement must use the current right-button state; updating input
    // after camera handling made capture/movement one frame late and jittery.
    m_physicsDebugSettings = m_editorUI.GetPhysicsDebugSettings();
    m_inputManager.Update(m_input);
    UpdateCameraControlMode();

    bool imguiWantsMouse = m_editorUI.WantCaptureMouse() || ImGuizmo::IsOver();
    if (imguiWantsMouse)
    {
        ReleaseMouseCapture();
    }
    else if (m_cameraControlMode == CameraControlMode::Free)
    {
        HandleCameraMovement(dt);
    }
    // 2. Синхронизируем позиции отсеков (связь с подлодкой)
    SyncCompartmentPositions();
    // 3. Физика
    UpdatePhysics(dt);

    // 4. Анимация воды
    UpdateWaterAnimation(dt);

  
    if (!imguiWantsMouse && m_cameraControlMode == CameraControlMode::FollowTarget)
        CharacterMovementSystem::Update(m_sceneManager.GetECSRegistry(), dt, TestEntity, m_input);

    if (m_cameraControlMode == CameraControlMode::FollowTarget)
        UpdateFollowCamera(dt);

    m_sceneManager.UpdateAll(dt);
    m_sceneManager.UpdateLights();
   
    m_camera->Render();
    m_camera->GetViewMatrix(m_viewMatrix);
    m_direct3D->GetProjectionMatrix(m_projMatrix);

    // 6. Debug visualizer update
    if (m_debugVisualizer)
        m_debugVisualizer->UpdateFromWorld(PhysicsWorld::Get().GetWorld());

    // 7. Drag-drop система
    imguiWantsMouse = m_editorUI.WantCaptureMouse()
        || ImGuizmo::IsOver()
        || ImGuizmo::IsUsing();
    if (!imguiWantsMouse)
    {
        m_dragDropSystem->Update(m_viewMatrix, m_projMatrix,
            m_screenWidth, m_screenHeight, m_lightSpheres,
            [this](const auto& req) { SpawnObjectFromRequest(req); });
    }

    // ════════════════════════════════════════════════════════════════════════════
    // ★ ЕДИНСТВЕННЫЙ ВЫЗОВ РЕНДЕРА — всё остальное внутри Renderer::RenderFrame()
    // ════════════════════════════════════════════════════════════════════════════
    Entity selectedEntity = (m_editorUI.GetSelectedType() == EditorUI::SelectedType::Object)
        ? m_editorUI.GetSelectedEntity() : INVALID_ENTITY;

    m_renderer->RenderFrame(m_direct3D->GetDeviceContext(), m_viewMatrix, m_projMatrix, selectedEntity, m_physicsDebugSettings);
    
    // 8. EditorUI
    m_editorUI.Update(m_sceneManager);
    RenderCompartmentCornerHandles();
    RenderCompoundCornerHandles();
    HandleCompartmentGizmo();
  
    // 9. Editor requests (save/load/spawn)
    ProcessEditorRequests();

    // 10. ImGui render
    m_editorUI.Render();
    m_direct3D->EndScene();

    return true;
}

// ============================================================================
// SyncCompartmentPositions — синхронизация позиций отсеков с подлодкой
// ============================================================================
void ApplicationClass::SyncCompartmentPositions()
{
    auto& registry = m_sceneManager.GetECSRegistry();
    auto& compartmentPool = registry.GetComponents<CompartmentComponent>();
    auto& transformPool = registry.GetComponents<TransformComponent>();
    auto& subPool = registry.GetComponents<SubmarineComponent>();

    for (auto& [entity, comp] : compartmentPool)
    {
        auto* subLink = subPool.Get(entity);
        if (!subLink || subLink->submarineRoot == INVALID_ENTITY) continue;
        if (entity == subLink->submarineRoot) continue;  // Пропускаем корень

        auto* rootTransform = transformPool.Get(subLink->submarineRoot);
        auto* compartmentTransform = transformPool.Get(entity);
        if (!rootTransform || !compartmentTransform) continue;

        // Поворачиваем локальное смещение на угол подлодки
        XMMATRIX rootRotation = XMMatrixRotationRollPitchYaw(
            rootTransform->rotation.x,
            rootTransform->rotation.y,
            rootTransform->rotation.z);

        XMVECTOR localOffset = XMLoadFloat3(&comp.localCenter);
        XMVECTOR rotatedOffset = XMVector3TransformCoord(localOffset, rootRotation);

        // Мировая позиция = позиция подлодки + повёрнутое смещение
        XMVECTOR rootPos = XMLoadFloat3(&rootTransform->position);
        XMVECTOR worldPos = XMVectorAdd(rootPos, rotatedOffset);

        // Обновляем трансформ отсека
        XMFLOAT3 newPos;
        XMStoreFloat3(&newPos, worldPos);
        compartmentTransform->position = newPos;
        compartmentTransform->rotation = rootTransform->rotation;
    }
}

// ============================================================================
// UpdatePhysics — все системы физики в правильном порядке
// ============================================================================
void ApplicationClass::UpdatePhysics(float dt)
{   
    if (dt <= 0.f) return;

    auto& registry = m_sceneManager.GetECSRegistry();
  
    // 1. Затопление отсеков
    FloodingSystem::Update(registry, dt, m_sceneManager.GetWaterLevel());

    // 2. Создание джойнтов из редактора (ДО физики!)
    JointConnectionSystem::Update(registry);

    // 3. Обновление подлодок
    auto& subPool = registry.GetComponents<SubmarineComponent>();
    for (auto& [entity, subComp] : subPool)
    {
        if (subComp.submarineRoot == entity)  // Только корневые
            SubmarineSystem::UpdateSubmarine(registry, entity);
    }

    // 4. Физика
    PhysicsSystem::Update(registry, dt, m_sceneManager.GetWaterLevel());

    // 5. Обнаружение сломанных джойнтов (ПОСЛЕ физики!)
    SubmarineJointSystem::Update(registry, dt);

    // 6. Давление и ремонт
    PressureSystem::Update(registry, dt, m_sceneManager.GetWaterLevel());
}

// ============================================================================
// UpdateWaterAnimation — анимация воды (UV смещение)
// ============================================================================
void ApplicationClass::UpdateWaterAnimation(float dt)
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

// ============================================================================
// HandleCompartmentGizmo — ImGuizmo для редактирования отсеков
// ============================================================================
void ApplicationClass::HandleCompartmentGizmo()
{
    if (!m_editorUI.IsEditingCompartment() || !m_compartmentRenderer)
        return;

    auto& registry = m_sceneManager.GetECSRegistry();
    auto* comp = registry.GetComponent<CompartmentComponent>(
        m_editorUI.GetEditingCompartmentEntity());

    if (!comp || !m_camera) return;

    // ── Находим мировую матрицу корневой подлодки ─────────────────────────────
    XMMATRIX worldMatrix = XMMatrixIdentity();
    {
        auto& subPool = registry.GetComponents<SubmarineComponent>();
        auto& transformPool = registry.GetComponents<TransformComponent>();

        auto* subLink = subPool.Get(m_editorUI.GetEditingCompartmentEntity());
        if (subLink && subLink->submarineRoot != INVALID_ENTITY)
        {
            auto* rt = transformPool.Get(subLink->submarineRoot);
            if (rt) worldMatrix = rt->GetWorldMatrix();
        }
        else
        {
            auto* st = transformPool.Get(m_editorUI.GetEditingCompartmentEntity());
            if (st) worldMatrix = st->GetWorldMatrix();
        }
    }

    ImGuizmo::SetDrawlist(ImGui::GetBackgroundDrawList());
    ImGuizmo::SetRect(0, 0, ImGui::GetIO().DisplaySize.x, ImGui::GetIO().DisplaySize.y);

    XMMATRIX view, proj;
    m_camera->GetViewMatrix(view);
    m_direct3D->GetProjectionMatrix(proj);

    // Текущая трансформация отсека (центр + размер как масштаб)
    XMMATRIX compartmentTransform =
        XMMatrixScaling(m_editorUI.GetCompartmentSize().x,
            m_editorUI.GetCompartmentSize().y,
            m_editorUI.GetCompartmentSize().z) *
        XMMatrixTranslation(m_editorUI.GetCompartmentCenter().x,
            m_editorUI.GetCompartmentCenter().y,
            m_editorUI.GetCompartmentCenter().z);

    float matrix[16];
    XMStoreFloat4x4(reinterpret_cast<XMFLOAT4X4*>(matrix), compartmentTransform);

    ImGuizmo::OPERATION op = (m_editorUI.GetCompartmentEditMode() == EditorUI::CompartmentEditMode::Move)
        ? ImGuizmo::TRANSLATE
        : ImGuizmo::SCALE;

    // Рисуем гизмо
    if (ImGuizmo::Manipulate(
        reinterpret_cast<const float*>(&view),
        reinterpret_cast<const float*>(&proj),
        op,
        ImGuizmo::LOCAL,
        matrix))
    {
        // Пересчитываем из матрицы обратно в позицию/размер
        XMFLOAT4X4 floatMat;
        memcpy(&floatMat, matrix, sizeof(XMFLOAT4X4));
        XMMATRIX newTransform = XMLoadFloat4x4(&floatMat);

        XMFLOAT3 newCenter, newScale;
        XMStoreFloat3(&newCenter, newTransform.r[3]);
        XMStoreFloat3(&newScale, newTransform.r[0]);

        m_editorUI.SetCompartmentCenter(newCenter);
        m_editorUI.SetCompartmentSize(newScale);
        m_editorUI.ApplyCompartmentCacheToComponent(comp);
    }
}

// ============================================================================
// ProcessEditorRequests — обработка save/load/spawn запросов от EditorUI
// ============================================================================
void ApplicationClass::ProcessEditorRequests()
{
    // Spawn request
    {
        auto req = m_editorUI.ConsumeSpawnRequest();
        if (req.pending)
            SpawnObjectFromRequest(req);
    }

    // Save request
    {
        auto req = m_editorUI.ConsumeSaveRequest();
        if (req.pending)
        {
            if (m_sceneManager.SaveScene(req.path))
                OutputDebugStringA(("Scene saved to " + req.path + "\n").c_str());
            else
                OutputDebugStringA(("Failed to save scene to " + req.path + "\n").c_str());
        }
    }

    // Load request
    {
        auto req = m_editorUI.ConsumeLoadRequest();
        if (req.pending)
        {
            m_sceneManager.LoadScene(req.path,
                m_direct3D->GetDevice(), m_direct3D->GetDeviceContext());
            OutputDebugStringA(("Scene loaded from: " + req.path + "\n").c_str());
        }
    }
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
    mat->Initialize(device, L"SpawnedMaterial");

    std::string modelPathLower = req.modelPath;
    std::transform(modelPathLower.begin(), modelPathLower.end(),
        modelPathLower.begin(), ::tolower);

    ShaderBaseClass* initShader = nullptr;

    if (modelPathLower.ends_with(".glb") || modelPathLower.ends_with(".gltf"))
    {
        initShader = m_pbrShader;
        OutputDebugStringA(("Auto-assigning PBR shader for GLB/GLTF: " + req.modelPath + "\n").c_str());
    }
    else
    {
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
            if (m_defaultTexture)
            {
                mat->SetMainTexture(m_defaultTexture);
                OutputDebugStringA("Assigned default texture (stone.tga)\n");
            }
        }
    }

    Entity newEntity = m_sceneManager.SpawnObject(model, req.posX, req.posY, req.posZ);

    // 🔥 DEBUG 1: Проверяем MeshRendererComponent ДО SetObjectMaterial
    {
        auto* meshComp = m_sceneManager.GetECSRegistry()
            .GetComponent<MeshRendererComponent>(newEntity);

        if (meshComp)
        {
            OutputDebugStringA("✅ Entity BEFORE SetObjectMaterial: has MeshRendererComponent\n");
            if (meshComp->material)
                OutputDebugStringA("   → Material already set\n");
            else
                OutputDebugStringA("   → Material is NULL\n");
        }
        else
        {
            OutputDebugStringA("❌ Entity BEFORE SetObjectMaterial: NO MeshRendererComponent!\n");
        }
    }

    m_sceneManager.SetObjectMaterial(newEntity, mat);

    // 🔥 DEBUG 2: Проверяем MeshRendererComponent ПОСЛЕ SetObjectMaterial
    {
        auto* meshComp = m_sceneManager.GetECSRegistry()
            .GetComponent<MeshRendererComponent>(newEntity);

        if (meshComp)
        {
            OutputDebugStringA("✅ Entity AFTER SetObjectMaterial: has MeshRendererComponent\n");
            if (meshComp->material)
                OutputDebugStringA("   → Material is set\n");
            else
                OutputDebugStringA("   → Material is NULL!!!\n");
        }
        else
        {
            OutputDebugStringA("❌ Entity AFTER SetObjectMaterial: LOST MeshRendererComponent!!!\n");
        }
    }

    // 🔥 DEBUG 3: Проверяем ВСЕ объекты в сцене
    {
        auto& meshPool = m_sceneManager.GetECSRegistry()
            .GetComponents<MeshRendererComponent>();

        char buf[512];
        sprintf_s(buf, "\n📊 TOTAL ENTITIES WITH MeshRenderer: %zu\n", meshPool.GetCount());
        OutputDebugStringA(buf);

        int idx = 0;
        for (auto& [ent, mesh] : meshPool)
        {
            sprintf_s(buf, "   [%d] entity=%u, shader=%p, model=%p\n",
                idx++, ent,
                mesh.material ? mesh.material->GetShader() : nullptr,
                mesh.model);
            OutputDebugStringA(buf);
        }
        OutputDebugStringA("\n");
    }

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

    Entity selectedEntity = (m_editorUI.GetSelectedType() == EditorUI::SelectedType::Object)
        ? m_editorUI.GetSelectedEntity() : INVALID_ENTITY;

    XMFLOAT3 camPos = m_camera->GetPosition();

    // Передаём shadow map в LightShader ДО рендера
    if (m_lightShader && m_shadowMap)
    {
        XMMATRIX lightVP = m_shadowMap->GetLightViewProjection();
        m_lightShader->SetShadowMap(context, m_shadowMap->GetShaderResourceView(), lightVP);
    }

    // Также передаём shadow map в PBRShader (если он есть)
    if (m_pbrShader && m_shadowMap)
    {
        XMMATRIX lightVP = m_shadowMap->GetLightViewProjection();
        m_pbrShader->SetShadowMap(context, m_shadowMap->GetShaderResourceView(), lightVP);
    }
    // ── Передаём данные о трещинах в PBR шейдер для каждого объекта ──────────
    if (m_pbrShader)
    {
        auto& registry = m_sceneManager.GetECSRegistry();
        auto& meshPool = registry.GetComponents<MeshRendererComponent>();

        for (auto& [entity, mesh] : meshPool)
        {
            if (!mesh.material || !mesh.material->GetShader()) continue;
            if (mesh.material->GetShader()->GetType() != ShaderType::PBR) continue;

            auto* transform = m_sceneManager.GetObjectTransform(entity);
            if (!transform) continue;

            auto* damage = registry.GetComponent<DamageDecalComponent>(entity);
            // damage == nullptr → SetDamageDecals сбросит crackCount=0 → нет эффекта
            m_pbrShader->SetDamageDecals(
                context,
                damage,
                transform->GetWorldMatrix());
        }
    }
    m_sceneManager.RenderAllECS(
        context, view, projection,
        m_defaultMaterial,
        lightColors, lightPositions,
        m_sceneManager.GetFogSettings(),
        selectedEntity,
        camPos, TimeManager::UnscaledDeltaTime());

    // После рендера желательно очистить слот 7, чтобы другие проходы не видели shadow map случайно
    {
        ID3D11ShaderResourceView* nullSRV = nullptr;
        context->PSSetShaderResources(7, 1, &nullSRV);
    }
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


    Entity excludeEntity = (excludeIndex >= 0)
        ? static_cast<Entity>(excludeIndex) : INVALID_ENTITY;

    m_sceneManager.RenderAllECSExcluding(
        context, view, projection,
        m_defaultMaterial,
        lightColors, lightPositions,
        m_sceneManager.GetFogSettings(),
        excludeEntity, TimeManager::UnscaledDeltaTime());
}

// ============================================================================
// RenderLightSources
// ============================================================================
// ============================================================================
// RenderLightSources — рендер визуальных индикаторов света (Gizmos)
// ============================================================================


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
// RayIntersectsSphere
// ============================================================================
bool RayIntersectsSphere(const CameraClass::Ray& ray,
    const XMFLOAT3& sphereCenter, float sphereRadius)
{
    XMVECTOR L = XMVectorSubtract(XMLoadFloat3(&sphereCenter), XMLoadFloat3(&ray.origin));
    float t = XMVectorGetX(XMVector3Dot(L, XMLoadFloat3(&ray.direction)));
    if (t < 0.0f) return false;

    XMVECTOR directionScaled = XMVectorScale(XMLoadFloat3(&ray.direction), t);
    XMVECTOR closest = XMVectorAdd(XMLoadFloat3(&ray.origin), directionScaled);
    XMVECTOR sphereCenterVec = XMLoadFloat3(&sphereCenter);
    float dist = XMVectorGetX(XMVector3Length(XMVectorSubtract(closest, sphereCenterVec)));
    return dist <= sphereRadius;
}

// ============================================================================
// UpdateCameraControlMode — переключение между свободной камерой и привязкой
// ============================================================================
void ApplicationClass::UpdateCameraControlMode()
{
    const bool toggleDown = m_inputManager.IsKeyDown(m_input, 'C');
    if (toggleDown && !m_cameraModeToggleWasDown && !m_editorUI.WantCaptureMouse())
    {
        m_cameraControlMode = (m_cameraControlMode == CameraControlMode::Free)
            ? CameraControlMode::FollowTarget
            : CameraControlMode::Free;

        ReleaseMouseCapture();

        OutputDebugStringA(m_cameraControlMode == CameraControlMode::Free
            ? "[Camera] Mode: Free (RMB + WASD/R/F)\n"
            : "[Camera] Mode: FollowTarget (WASD controls player)\n");
    }

    m_cameraModeToggleWasDown = toggleDown;
}

// ============================================================================
// ReleaseMouseCapture — единая точка освобождения мыши
// ============================================================================
void ApplicationClass::ReleaseMouseCapture()
{
    if (!m_inputManager.IsMouseCaptured(m_input)) return;
    m_inputManager.SetMouseCapture(m_input, false);
    m_inputManager.ResetMouseDelta(m_input);
}

// ============================================================================
// UpdateFollowCamera — камера следует за тестовым игроком
// ============================================================================
void ApplicationClass::UpdateFollowCamera(float dt)
{
    if (!m_camera || TestEntity == INVALID_ENTITY) return;

    auto* transform = m_sceneManager.GetECSRegistry().GetComponent<TransformComponent>(TestEntity);
    if (!transform) return;

    dt = std::min(dt, 0.05f);

    XMFLOAT3 current = m_camera->GetPosition();
    XMFLOAT3 desired = {
        transform->position.x + m_followCameraOffset.x,
        transform->position.y + m_followCameraOffset.y,
        transform->position.z + m_followCameraOffset.z
    };

    const float followSpeed = 12.0f;
    const float t = std::clamp(followSpeed * dt, 0.0f, 1.0f);

    XMFLOAT3 next = {
        current.x + (desired.x - current.x) * t,
        current.y + (desired.y - current.y) * t,
        current.z + (desired.z - current.z) * t
    };
    m_camera->SetPosition(next.x, next.y, next.z);

    XMVECTOR cameraPos = XMLoadFloat3(&next);
    XMVECTOR targetPos = XMLoadFloat3(&transform->position);
    XMVECTOR direction = XMVector3Normalize(XMVectorSubtract(targetPos, cameraPos));

    XMFLOAT3 dir;
    XMStoreFloat3(&dir, direction);

    const float yaw = XMConvertToDegrees(std::atan2(dir.x, dir.z));
    const float pitch = XMConvertToDegrees(std::asin(std::clamp(-dir.y, -1.0f, 1.0f)));
    m_camera->SetRotation(pitch, yaw, 0.0f);
}

// ============================================================================
// HandleCameraMovement
// ============================================================================
void ApplicationClass::HandleCameraMovement(float dt)
{

    if (m_editorUI.WantCaptureMouse()) return; // Пропускать ввод, если UI активен

    dt = min(dt, 0.05f);

    bool rightDown = m_inputManager.IsCameraMode();

    if (rightDown && !m_inputManager.IsMouseCaptured(m_input) && !m_editorUI.WantCaptureMouse())
        m_inputManager.SetMouseCapture(m_input, true);
    else if (!rightDown && m_inputManager.IsMouseCaptured(m_input))
        m_inputManager.SetMouseCapture(m_input, false);

    if (m_inputManager.IsMouseCaptured(m_input))
    {
        m_input->UpdateCapturedMouseDelta(m_hwnd);

        int deltaX, deltaY;
        m_input->GetMouseDelta(deltaX, deltaY);
        m_inputManager.ResetMouseDelta(m_input);

        // Mouse delta is already accumulated per frame, so it must not be
        // multiplied by dt. Scaling by dt made rotation depend on frame time
        // twice: longer frames collect more mouse pixels and then amplified
        // those pixels by a larger dt, causing visible stutter while moving.
        deltaX = std::clamp(deltaX, -200, 200);
        deltaY = std::clamp(deltaY, -200, 200);

        const float mouseSensitivity = Settings::CAMERA_SENSITIVITY;
        XMFLOAT3 rotation = m_camera->GetRotation();
        float rotX = rotation.x + deltaY * mouseSensitivity;
        float rotY = rotation.y + deltaX * mouseSensitivity;
        rotX = std::clamp(rotX, -89.0f, 89.0f);
        m_camera->SetRotation(rotX, rotY, rotation.z);
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
    if (m_editorUI.IsCornerEditActive() || m_editorUI.IsEditingCompartment())
        return;

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


void ApplicationClass::StartDraggingSprite(int index,
    const CameraClass::Ray& pickRay, const XMFLOAT3& spritePos)
{
    m_isDragging = true;
    m_draggedSpriteIndex = index;
    m_draggedLightIndex = -1;
    m_draggedObjectIndex = -1;

    XMVECTOR toSprite = XMVectorSubtract(XMLoadFloat3(&spritePos), XMLoadFloat3(&pickRay.origin));
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

    XMVECTOR toObj = XMVectorSubtract(XMLoadFloat3(&objPos), XMLoadFloat3(&pickRay.origin));
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
        // m_sceneManager.SetLight(m_draggedLightIndex, newPos, XMFLOAT4(1, 1, 0.9f, 1), true);
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




// В ApplicationClass::CreateTestSubmarine()

void ApplicationClass::CreateTestSubmarine()
{
    auto& registry = m_sceneManager.GetECSRegistry();
    ID3D11Device* device = m_direct3D->GetDevice();
    ID3D11DeviceContext* context = m_direct3D->GetDeviceContext();

    AssimpModelClass* subModel = m_sceneManager.GetOrCreateModel(
        "assets/model/submarine.glb", device, context, "");
    if (!subModel) { OutputDebugStringA("[ERROR] Sub model failed!\n"); return; }

    Entity subRoot = registry.CreateEntity();
    registry.AddComponent<TransformComponent>(subRoot,
        TransformComponent{ {0, 10.0f, 0}, {0, 0, 0}, {1, 1, 1} });

    auto* subMat = new MaterialClass();
    subMat->Initialize(device, L"SubmarineMaterial");
    if (m_pbrShader) subMat->SetShader(m_pbrShader);
    else if (m_lightShader) subMat->SetShader(m_lightShader);
    else { delete subMat; return; }
    if (m_defaultTexture) subMat->SetMainTexture(m_defaultTexture);

    registry.AddComponent<MeshRendererComponent>(subRoot,
        MeshRendererComponent{ subModel, subMat });
    registry.AddComponent<SubmarineComponent>(subRoot, SubmarineComponent{ subRoot });
    registry.AddComponent<ActiveComponent>(subRoot, ActiveComponent{});

    // ══════════════════════════════════════════════════════════════════
    // КОЛЛИЗИЯ ПОДЛОДКИ: Compound из 3 частей (нос + тело + корма)
    // Каждая часть — capsule/cylinder, вместе дают форму "пули"
    // ══════════════════════════════════════════════════════════════════
    RigidBodyComponent subRB{};
    subRB.mass = 5000.0f;
    subRB.isCompound = true;

    // Часть 1: Центральный цилиндр (основной корпус)
    {
        RigidBodyComponent::CompoundElement body;
        body.type = RigidBodyComponent::CompoundElement::Cylinder;
        body.radius = 1.8f;
        body.height = 8.0f;
        body.localPosition = { 0.0f, 0.0f, 0.0f };
        // Поворот цилиндра вдоль оси Z (Bullet-цилиндр по умолчанию вдоль Y)
        // Поворачиваем на 90° вокруг Z чтобы он лёг горизонтально
        XMVECTOR qRot = XMQuaternionRotationAxis(XMVectorSet(0, 0, 1, 0),
            XM_PIDIV2);
        XMStoreFloat4(&body.localRotation, qRot);
        subRB.compoundElements.push_back(body);
    }

    // Часть 2: Носовая капсула (сужение спереди)
    {
        RigidBodyComponent::CompoundElement nose;
        nose.type = RigidBodyComponent::CompoundElement::Capsule;
        nose.radius = 1.4f;
        nose.height = 2.0f;
        nose.localPosition = { 5.0f, 0.0f, 0.0f }; // сдвиг вперёд по X
        XMVECTOR qRot = XMQuaternionRotationAxis(XMVectorSet(0, 0, 1, 0),
            XM_PIDIV2);
        XMStoreFloat4(&nose.localRotation, qRot);
        subRB.compoundElements.push_back(nose);
    }

    // Часть 3: Кормовая часть (срез сзади — чуть шире)
    {
        RigidBodyComponent::CompoundElement stern;
        stern.type = RigidBodyComponent::CompoundElement::Capsule;
        stern.radius = 1.6f;
        stern.height = 2.0f;
        stern.localPosition = { -5.0f, 0.0f, 0.0f }; // сдвиг назад по X
        XMVECTOR qRot = XMQuaternionRotationAxis(XMVectorSet(0, 0, 1, 0),
            XM_PIDIV2);
        XMStoreFloat4(&stern.localRotation, qRot);
        subRB.compoundElements.push_back(stern);
    }

    subRB.linearDamping = 0.5f;
    subRB.angularDamping = 0.8f;
    subRB.friction = 0.1f;

    registry.AddComponent<RigidBodyComponent>(subRoot, std::move(subRB));
    registry.AddComponent<BuoyancyComponent>(subRoot, BuoyancyComponent{ 1.0f, 0.5f });
    PhysicsSystem::CreateRigidBody(registry, subRoot);

    // ══════════════════════════════════════════════════════════════════
    // ОТСЕКИ: только логика + СТЕНЫ для персонажей
    // ══════════════════════════════════════════════════════════════════
    struct CompartmentDesc {
        const char* name;
        XMFLOAT3 localMin;
        XMFLOAT3 localMax;
        XMFLOAT3 localCenter;
    };

    std::vector<CompartmentDesc> compartments = {
        { "Bow",    {  2.0f, -1.5f, -1.8f}, { 5.0f, 1.5f, 1.8f}, { 3.5f, 0, 0} },
        { "Center", { -2.0f, -1.5f, -1.8f}, { 2.0f, 1.5f, 1.8f}, { 0.0f, 0, 0} },
        { "Stern",  { -5.0f, -1.5f, -1.8f}, {-2.0f, 1.5f, 1.8f}, {-3.5f, 0, 0} }
    };

    auto* subComp = registry.GetComponent<SubmarineComponent>(subRoot);
    auto* rootTransform = registry.GetComponent<TransformComponent>(subRoot);

    for (const auto& desc : compartments)
    {
        Entity compEntity = registry.CreateEntity();

        XMFLOAT3 worldPos;
        XMStoreFloat3(&worldPos,
            XMVectorAdd(
                XMLoadFloat3(&rootTransform->position),
                XMLoadFloat3(&desc.localCenter)));

        registry.AddComponent<TransformComponent>(compEntity,
            TransformComponent{ worldPos, {0,0,0}, {1,1,1} });

        CompartmentComponent compartment{};
        compartment.localMin = desc.localMin;
        compartment.localMax = desc.localMax;
        compartment.localCenter = desc.localCenter;
        compartment.RecalculateVolume();
        compartment.isSealed = true;
        registry.AddComponent<CompartmentComponent>(compEntity, compartment);

        SubmarineComponent link{};
        link.submarineRoot = subRoot;
        registry.AddComponent<SubmarineComponent>(compEntity, link);
        registry.AddComponent<ActiveComponent>(compEntity, ActiveComponent{});

        if (subComp) subComp->allCompartments.push_back(compEntity);
    }

    OutputDebugStringA("✅ Submarine created with bullet-shape collision\n");
}

void ApplicationClass::Render3DObjectsToShadowMap(
    ID3D11DeviceContext* context,
    const XMMATRIX& lightViewProj)
{
    auto& meshPool = m_sceneManager.GetECSRegistry()
        .GetComponents<MeshRendererComponent>();

    int renderedCount = 0;

    for (auto& [entity, mesh] : meshPool)
    {
        if (!m_sceneManager.IsObjectActive(entity)) continue;
        auto* transform = m_sceneManager.GetObjectTransform(entity);
        if (!transform || !mesh.model) continue;

        auto* mat = mesh.material;
        if (!mat || !mat->GetShader()) continue;

        ShaderType st = mat->GetShader()->GetType();

        if (st != ShaderType::Light && st != ShaderType::PBR)
            continue;

        XMMATRIX world = transform->GetWorldMatrix();

        // ВАЖНО: Устанавливаем vertex/index buffer для текущей модели!
        mesh.model->Render(context);

        m_shadowShader->Render(context,
            mesh.model->GetIndexCount(), world, lightViewProj);

        // DEBUG: Логируем каждый объект


        renderedCount++;
    }

    // ВАЖНЫЙ ЛОГ

    // Очищаем состояние
    {
        ID3D11Buffer* nullBuffers[6] = {};
        context->VSSetConstantBuffers(0, 6, nullBuffers);
        context->PSSetConstantBuffers(0, 6, nullBuffers);

        ID3D11ShaderResourceView* nullSRVs[8] = {};
        context->PSSetShaderResources(0, 8, nullSRVs);
    }
}


// ============================================================================
// RenderCompartmentCornerHandles — 8 угловых ручек для AABB отсека
// Вызывать ПОСЛЕ m_editorUI.Update() (ImGui уже начат), но ДО Render().
// ============================================================================
void ApplicationClass::RenderCompartmentCornerHandles()
{
    if (!m_editorUI.IsEditingCompartment()) return;

    auto& registry = m_sceneManager.GetECSRegistry();
    Entity compEnt = m_editorUI.GetEditingCompartmentEntity();
    auto* comp = registry.GetComponent<CompartmentComponent>(compEnt);
    if (!comp) return;

    // ── Находим мировую матрицу корневой подлодки ─────────────────────────────
    XMMATRIX rootWorld = XMMatrixIdentity();
    {
        auto* subLink = registry.GetComponent<SubmarineComponent>(compEnt);
        if (subLink && subLink->submarineRoot != INVALID_ENTITY)
        {
            auto* rt = registry.GetComponent<TransformComponent>(subLink->submarineRoot);
            if (rt) rootWorld = rt->GetWorldMatrix();
        }
        else
        {
            auto* selfT = registry.GetComponent<TransformComponent>(compEnt);
            if (selfT) rootWorld = selfT->GetWorldMatrix();
        }
    }

    XMMATRIX view, proj;
    m_camera->GetViewMatrix(view);
    m_direct3D->GetProjectionMatrix(proj);
    XMMATRIX vp = view * proj;

    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    ImGuiIO& io = ImGui::GetIO();
    const float W = io.DisplaySize.x;
    const float H = io.DisplaySize.y;

    // ── 8 углов AABB ──────────────────────────────────────────────────────────
    // Биты индекса: бит0=X(0→min,1→max), бит1=Y, бит2=Z
    auto getCornerLocal = [&](int idx) -> XMFLOAT3 {
        return {
            (idx & 1) ? comp->localMax.x : comp->localMin.x,
            (idx & 2) ? comp->localMax.y : comp->localMin.y,
            (idx & 4) ? comp->localMax.z : comp->localMin.z
        };
        };

    // Проецирует локальную точку отсека → пиксели экрана
    auto projectPoint = [&](const XMFLOAT3& localPt) -> ImVec2 {
        XMVECTOR v = XMLoadFloat3(&localPt);
        XMVECTOR w = XMVector3TransformCoord(v, rootWorld);
        XMVECTOR clip = XMVector3TransformCoord(w, vp);
        XMFLOAT3 ndc;
        XMStoreFloat3(&ndc, clip);
        return ImVec2((ndc.x * 0.5f + 0.5f) * W,
            (1.0f - (ndc.y * 0.5f + 0.5f)) * H);
        };

    // ── Рисуем рёбра AABB (12 линий) ─────────────────────────────────────────
    static const int edges[12][2] = {
        {0,1},{2,3},{4,5},{6,7},   // по X
        {0,2},{1,3},{4,6},{5,7},   // по Y
        {0,4},{1,5},{2,6},{3,7}    // по Z
    };
    for (auto& [a, b] : edges)
    {
        ImVec2 pa = projectPoint(getCornerLocal(a));
        ImVec2 pb = projectPoint(getCornerLocal(b));
        dl->AddLine(pa, pb, IM_COL32(80, 200, 120, 160), 1.2f);
    }

    // ── Ручки углов ───────────────────────────────────────────────────────────
    const float HANDLE_R = 7.0f;   // радиус ручки (px)
    const float HANDLE_HIT = 10.0f;  // радиус hit-zone
    ImVec2 mouse = io.MousePos;

    // Завершаем drag по отпусканию ЛКМ
    if (m_editorUI.m_cornerDragActive && !ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        m_editorUI.m_cornerDragActive = false;
        m_editorUI.m_dragCornerIndex = -1;
    }

    // Обновляем drag
    if (m_editorUI.m_cornerDragActive && m_editorUI.m_dragCornerIndex >= 0)
    {
        int idx = m_editorUI.m_dragCornerIndex;

        // Нужно перевести перемещение мыши обратно в локальное пространство.
        // Используем простой подход: находим ближайшую точку луча к исходному углу.
        CameraClass::Ray ray = m_camera->CreatePickingRay(
            (int)mouse.x, (int)mouse.y,
            (int)W, (int)H, view, proj);

        // Исходный угол в мировых координатах
        XMFLOAT3 origLocal = getCornerLocal(idx);
        XMVECTOR origWorld = XMVector3TransformCoord(XMLoadFloat3(&origLocal), rootWorld);
        XMFLOAT3 origWorldF;
        XMStoreFloat3(&origWorldF, origWorld);

        // Проецируем луч на плоскость, параллельную экрану, проходящую через угол
        XMVECTOR rayO = XMLoadFloat3(&ray.origin);
        XMVECTOR rayD = XMLoadFloat3(&ray.direction);

        // Нормаль плоскости = направление к камере (плоскость "экранная")
        XMFLOAT3 camPos = m_camera->GetPosition();
        XMVECTOR camFwd = XMVector3Normalize(
            XMVectorSubtract(origWorld, XMLoadFloat3(&camPos)));

        float denom = XMVectorGetX(XMVector3Dot(camFwd, rayD));
        XMFLOAT3 newWorldF = origWorldF;
        if (fabsf(denom) > 1e-5f)
        {
            float t = XMVectorGetX(XMVector3Dot(
                XMVectorSubtract(origWorld, rayO), camFwd)) / denom;
            XMVECTOR hitWorld = XMVectorAdd(rayO, XMVectorScale(rayD, t));

            // Переводим в локальное пространство отсека
            XMMATRIX rootWorldInv = XMMatrixInverse(nullptr, rootWorld);
            XMVECTOR hitLocal = XMVector3TransformCoord(hitWorld, rootWorldInv);
            XMFLOAT3 hitLocalF;
            XMStoreFloat3(&hitLocalF, hitLocal);

            // Обновляем только соответствующую грань (по битам индекса)
            XMFLOAT3 newMin = m_editorUI.m_dragCornerStartMin;
            XMFLOAT3 newMax = m_editorUI.m_dragCornerStartMax;

            auto clampFace = [](float val, float other, bool isMax) -> float {
                // Не даём граням пересекаться (мин. размер 0.2)
                if (isMax) return fmaxf(val, other + 0.2f);
                else       return fminf(val, other - 0.2f);
                };

            if (idx & 1) newMax.x = clampFace(hitLocalF.x, newMin.x, true);
            else         newMin.x = clampFace(hitLocalF.x, newMax.x, false);

            if (idx & 2) newMax.y = clampFace(hitLocalF.y, newMin.y, true);
            else         newMin.y = clampFace(hitLocalF.y, newMax.y, false);

            if (idx & 4) newMax.z = clampFace(hitLocalF.z, newMin.z, true);
            else         newMin.z = clampFace(hitLocalF.z, newMax.z, false);

            comp->localMin = newMin;
            comp->localMax = newMax;
            comp->RecalculateVolume();

            // Синхронизируем кэш EditorUI
            m_editorUI.UpdateCacheFromCompartment_Public(comp);
        }
    }

    // Рисуем 8 ручек + начинаем drag
    for (int i = 0; i < 8; ++i)
    {
        ImVec2 screenPt = projectPoint(getCornerLocal(i));

        float dx = mouse.x - screenPt.x;
        float dy = mouse.y - screenPt.y;
        float dist = sqrtf(dx * dx + dy * dy);

        bool hovered = (dist < HANDLE_HIT) && !m_editorUI.m_cornerDragActive;
        bool active = (m_editorUI.m_dragCornerIndex == i && m_editorUI.m_cornerDragActive);

        // Начало drag
        if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            m_editorUI.m_cornerDragActive = true;
            m_editorUI.m_dragCornerIndex = i;
            m_editorUI.m_dragCornerStartMin = comp->localMin;
            m_editorUI.m_dragCornerStartMax = comp->localMax;
        }

        // Цвет: белый=обычный, жёлтый=hover, оранжевый=active
        ImU32 col = active ? IM_COL32(255, 140, 0, 255) :
            hovered ? IM_COL32(255, 240, 80, 255) :
            IM_COL32(220, 220, 220, 210);

        dl->AddCircleFilled(screenPt, HANDLE_R, col);
        dl->AddCircle(screenPt, HANDLE_R + 1.0f, IM_COL32(0, 0, 0, 180), 0, 1.5f);

        // Подсказка при наведении
        if (hovered)
        {
            const char* axisLabels[8] = {
                "-X-Y-Z", "+X-Y-Z", "-X+Y-Z", "+X+Y-Z",
                "-X-Y+Z", "+X-Y+Z", "-X+Y+Z", "+X+Y+Z"
            };
            dl->AddRectFilled(
                ImVec2(screenPt.x + 12, screenPt.y - 10),
                ImVec2(screenPt.x + 12 + 60, screenPt.y + 8),
                IM_COL32(30, 30, 30, 200), 3.0f);
            dl->AddText(ImVec2(screenPt.x + 14, screenPt.y - 8),
                IM_COL32(255, 255, 255, 255), axisLabels[i]);
        }
    }
}



void ApplicationClass::RenderCompoundCornerHandles()
{
    if (!m_editorUI.IsCornerEditActive()) return;

    auto& registry = m_sceneManager.GetECSRegistry();
    Entity entity = m_editorUI.GetCornerEditEntity();
    int    elemIdx = m_editorUI.GetCornerEditElemIndex();

    auto* rb = registry.GetComponent<RigidBodyComponent>(entity);
    if (!rb || elemIdx < 0 || elemIdx >= (int)rb->compoundElements.size()) return;

    auto& elem = rb->compoundElements[elemIdx];
    if (elem.type != RigidBodyComponent::CompoundElement::Box) return;

    auto* transform = registry.GetComponent<TransformComponent>(entity);
    if (!transform) return;

    XMMATRIX worldMat = transform->GetWorldMatrix();
    XMMATRIX view, proj;
    m_camera->GetViewMatrix(view);
    m_direct3D->GetProjectionMatrix(proj);
    XMMATRIX vp = view * proj;

    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    ImGuiIO& io = ImGui::GetIO();
    const float W = io.DisplaySize.x;
    const float H = io.DisplaySize.y;
    ImVec2      mouse = io.MousePos;

    // ── Текущие границы бокса ─────────────────────────────────────────────────
    XMFLOAT3 bMin = {
        elem.localPosition.x + elem.boxOffsetMin.x,
        elem.localPosition.y + elem.boxOffsetMin.y,
        elem.localPosition.z + elem.boxOffsetMin.z
    };
    XMFLOAT3 bMax = {
        elem.localPosition.x + elem.boxOffsetMax.x,
        elem.localPosition.y + elem.boxOffsetMax.y,
        elem.localPosition.z + elem.boxOffsetMax.z
    };
    XMFLOAT3 bCen = {
        (bMin.x + bMax.x) * 0.5f,
        (bMin.y + bMax.y) * 0.5f,
        (bMin.z + bMax.z) * 0.5f
    };

    auto projectPt = [&](const XMFLOAT3& local) -> ImVec2 {
        XMVECTOR w = XMVector3TransformCoord(XMLoadFloat3(&local), worldMat);
        XMVECTOR clip = XMVector3TransformCoord(w, vp);
        XMFLOAT3 ndc;
        XMStoreFloat3(&ndc, clip);
        return ImVec2((ndc.x * 0.5f + 0.5f) * W,
            (ndc.y * -0.5f + 0.5f) * H);
        };

    // ── 8 угловых точек ───────────────────────────────────────────────────────
    auto getCorner = [&](int idx) -> XMFLOAT3 {
        return {
            (idx & 1) ? bMax.x : bMin.x,
            (idx & 2) ? bMax.y : bMin.y,
            (idx & 4) ? bMax.z : bMin.z
        };
        };

    // ── 6 граневых точек (центр каждой грани) — двигают ТОЛЬКО одну ось ───────
    // Индексы 8..13: 8=-X, 9=+X, 10=-Y, 11=+Y, 12=-Z, 13=+Z
    auto getFacePoint = [&](int faceIdx) -> XMFLOAT3 {
        switch (faceIdx) {
        case  8: return { bMin.x, bCen.y, bCen.z }; // -X грань
        case  9: return { bMax.x, bCen.y, bCen.z }; // +X грань
        case 10: return { bCen.x, bMin.y, bCen.z }; // -Y грань
        case 11: return { bCen.x, bMax.y, bCen.z }; // +Y грань
        case 12: return { bCen.x, bCen.y, bMin.z }; // -Z грань
        default: return { bCen.x, bCen.y, bMax.z }; // +Z грань
        }
        };

    // Для граневой точки: какую ось и направление менять
    // returns {axisIndex (0=X,1=Y,2=Z), isMax}
    struct FaceInfo { int axis; bool isMax; };
    auto getFaceInfo = [](int faceIdx) -> FaceInfo {
        switch (faceIdx) {
        case  8: return { 0, false };
        case  9: return { 0, true };
        case 10: return { 1, false };
        case 11: return { 1, true };
        case 12: return { 2, false };
        default: return { 2, true };
        }
        };

    // ── Рёбра AABB ────────────────────────────────────────────────────────────
    static constexpr int edges[12][2] = {
        {0,1},{2,3},{4,5},{6,7},
        {0,2},{1,3},{4,6},{5,7},
        {0,4},{1,5},{2,6},{3,7}
    };
    for (auto& [a, b] : edges)
        dl->AddLine(projectPt(getCorner(a)), projectPt(getCorner(b)),
            IM_COL32(255, 160, 30, 180), 1.5f);

    // ── Завершаем drag по отпусканию ЛКМ ─────────────────────────────────────
    if (m_editorUI.m_cornerDragActive && !ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        m_editorUI.m_cornerDragActive = false;
        m_editorUI.m_cornerDragIndex = -1;
        RefreshRigidBodyExternal(registry, entity, rb);
    }

    // ── Обновляем drag ────────────────────────────────────────────────────────
    if (m_editorUI.m_cornerDragActive && m_editorUI.m_cornerDragIndex >= 0)
    {
        int idx = m_editorUI.m_cornerDragIndex;

        CameraClass::Ray ray = m_camera->CreatePickingRay(
            (int)mouse.x, (int)mouse.y, (int)W, (int)H, view, proj);

        // Берём точку привязки: угол или центр грани
        XMFLOAT3 anchorLocal = (idx < 8) ? getCorner(idx) : getFacePoint(idx);
        XMVECTOR anchorWorld = XMVector3TransformCoord(XMLoadFloat3(&anchorLocal), worldMat);

        XMFLOAT3 camPos = m_camera->GetPosition();
        XMVECTOR camFwd = XMVector3Normalize(
            XMVectorSubtract(anchorWorld, XMLoadFloat3(&camPos)));

        XMVECTOR rayO = XMLoadFloat3(&ray.origin);
        XMVECTOR rayD = XMLoadFloat3(&ray.direction);

        float denom = XMVectorGetX(XMVector3Dot(camFwd, rayD));
        if (fabsf(denom) > 1e-5f)
        {
            float t = XMVectorGetX(XMVector3Dot(
                XMVectorSubtract(anchorWorld, rayO), camFwd)) / denom;
            XMVECTOR hitWorld = XMVectorAdd(rayO, XMVectorScale(rayD, t));

            XMMATRIX worldInv = XMMatrixInverse(nullptr, worldMat);
            XMFLOAT3 hitLocal;
            XMStoreFloat3(&hitLocal, XMVector3TransformCoord(hitWorld, worldInv));

            XMFLOAT3 hitOffset = {
                hitLocal.x - elem.localPosition.x,
                hitLocal.y - elem.localPosition.y,
                hitLocal.z - elem.localPosition.z
            };

            XMFLOAT3 newMin = m_editorUI.m_cornerDragStartMin;
            XMFLOAT3 newMax = m_editorUI.m_cornerDragStartMax;
            constexpr float MIN_SIZE = 0.1f;

            if (idx < 8)
            {
                // ── Угловая точка: меняем все 3 оси одновременно ─────────────
                if (idx & 1) newMax.x = fmaxf(hitOffset.x, newMin.x + MIN_SIZE);
                else         newMin.x = fminf(hitOffset.x, newMax.x - MIN_SIZE);
                if (idx & 2) newMax.y = fmaxf(hitOffset.y, newMin.y + MIN_SIZE);
                else         newMin.y = fminf(hitOffset.y, newMax.y - MIN_SIZE);
                if (idx & 4) newMax.z = fmaxf(hitOffset.z, newMin.z + MIN_SIZE);
                else         newMin.z = fminf(hitOffset.z, newMax.z - MIN_SIZE);
            }
            else
            {
                // ── Граневая точка: меняем ТОЛЬКО одну ось ───────────────────
                auto [axis, isMax] = getFaceInfo(idx);
                float* pMin = (axis == 0) ? &newMin.x : (axis == 1) ? &newMin.y : &newMin.z;
                float* pMax = (axis == 0) ? &newMax.x : (axis == 1) ? &newMax.y : &newMax.z;
                float  val = (axis == 0) ? hitOffset.x : (axis == 1) ? hitOffset.y : hitOffset.z;

                if (isMax) *pMax = fmaxf(val, *pMin + MIN_SIZE);
                else       *pMin = fminf(val, *pMax - MIN_SIZE);
            }

            elem.boxOffsetMin = newMin;
            elem.boxOffsetMax = newMax;
        }
    }

    // ── Рисуем ручки ─────────────────────────────────────────────────────────
    static constexpr const char* cornerLabels[8] = {
        "-X-Y-Z","+X-Y-Z","-X+Y-Z","+X+Y-Z",
        "-X-Y+Z","+X-Y+Z","-X+Y+Z","+X+Y+Z"
    };
    static constexpr const char* faceLabels[6] = {
        "-X", "+X", "-Y", "+Y", "-Z", "+Z"
    };
    // Цвета граневых ручек по осям
    static constexpr ImU32 axisColors[3] = {
        IM_COL32(220, 60,  60,  220),   // X — красный
        IM_COL32(60,  220, 60,  220),   // Y — зелёный
        IM_COL32(60,  100, 220, 220),   // Z — синий
    };

    const float CORNER_R = 6.0f;
    const float FACE_R = 8.0f;
    const float HIT_R = 11.0f;

    // 8 угловых (белые/жёлтые/оранжевые)
    for (int i = 0; i < 8; ++i)
    {
        ImVec2 sp = projectPt(getCorner(i));
        float  dx = mouse.x - sp.x;
        float  dy = mouse.y - sp.y;
        bool hovered = (sqrtf(dx * dx + dy * dy) < HIT_R) && !m_editorUI.m_cornerDragActive;
        bool active = (m_editorUI.m_cornerDragIndex == i && m_editorUI.m_cornerDragActive);

        if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            m_editorUI.m_cornerDragActive = true;
            m_editorUI.m_cornerDragIndex = i;
            m_editorUI.m_cornerDragStartMin = elem.boxOffsetMin;
            m_editorUI.m_cornerDragStartMax = elem.boxOffsetMax;
        }

        ImU32 col = active ? IM_COL32(255, 120, 0, 255) :
            hovered ? IM_COL32(255, 240, 80, 255) :
            IM_COL32(220, 220, 220, 200);

        dl->AddCircleFilled(sp, CORNER_R, col);
        dl->AddCircle(sp, CORNER_R + 1.5f, IM_COL32(0, 0, 0, 160), 0, 1.5f);

        if (hovered) {
            ImVec2 tp = ImVec2(sp.x + 12.f, sp.y - 9.f);
            dl->AddRectFilled(tp, ImVec2(tp.x + 62.f, tp.y + 17.f), IM_COL32(20, 20, 20, 210), 3.f);
            dl->AddText(ImVec2(tp.x + 3.f, tp.y + 2.f), IM_COL32(255, 255, 255, 255), cornerLabels[i]);
        }
    }

    // 6 граневых (цветные по осям — двигают ТОЛЬКО одну грань)
    for (int i = 8; i < 14; ++i)
    {
        ImVec2 sp = projectPt(getFacePoint(i));
        float  dx = mouse.x - sp.x;
        float  dy = mouse.y - sp.y;
        bool hovered = (sqrtf(dx * dx + dy * dy) < HIT_R) && !m_editorUI.m_cornerDragActive;
        bool active = (m_editorUI.m_cornerDragIndex == i && m_editorUI.m_cornerDragActive);

        if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            m_editorUI.m_cornerDragActive = true;
            m_editorUI.m_cornerDragIndex = i;
            m_editorUI.m_cornerDragStartMin = elem.boxOffsetMin;
            m_editorUI.m_cornerDragStartMax = elem.boxOffsetMax;
        }

        auto [axis, isMax] = getFaceInfo(i);
        ImU32 baseCol = axisColors[axis];
        ImU32 col = active ? IM_COL32(255, 240, 80, 255) :
            hovered ? IM_COL32(255, 255, 150, 255) :
            baseCol;

        // Граневые точки — ромб, чтобы отличались от угловых
        dl->AddCircleFilled(sp, FACE_R, col);
        dl->AddCircle(sp, FACE_R + 1.5f, IM_COL32(0, 0, 0, 180), 0, 2.0f);

        if (hovered) {
            ImVec2 tp = ImVec2(sp.x + 12.f, sp.y - 9.f);
            dl->AddRectFilled(tp, ImVec2(tp.x + 32.f, tp.y + 17.f), IM_COL32(20, 20, 20, 210), 3.f);
            dl->AddText(ImVec2(tp.x + 3.f, tp.y + 2.f), IM_COL32(255, 255, 255, 255), faceLabels[i - 8]);
        }
    }
}

void ApplicationClass::RefreshRigidBodyExternal(ECSRegistry& registry, Entity entity, RigidBodyComponent* rb)
{
    if (!rb || !rb->body) return;

    // Удаляем старое тело из мира
    auto world = PhysicsWorld::Get().GetWorld();
    world->removeRigidBody(rb->body);

    // Удаляем shape и motion state
    delete rb->body->getMotionState();
    btCollisionShape* oldShape = rb->body->getCollisionShape();
    delete rb->body;
    rb->body = nullptr;
    if (oldShape) delete oldShape;

    // Пересоздаём
    PhysicsSystem::CreateRigidBody(registry, entity);
}

ID3D11ShaderResourceView* LoadSkyboxCubemap(
    ID3D11Device* device,
    ID3D11DeviceContext* context,
    const std::string& basePath)
{
    // 🔥 Порядок граней для DX11 кубмапа (важно!)
    const char* faces[6] = {
        "_px.jpg", "_nx.jpg",  // +X, -X (Right, Left)
        "_py.jpg", "_ny.jpg",  // +Y, -Y (Up, Down)
        "_pz.jpg", "_nz.jpg"   // +Z, -Z (Front, Back)
    };

    std::vector<unsigned char*> faceData(6, nullptr);
    int width = 0, height = 0;

    // 1. Загружаем все 6 граней через stb_image
    for (int i = 0; i < 6; ++i)
    {
        std::string path = basePath + faces[i];
        int channels = 0;

        faceData[i] = stbi_load(path.c_str(), &width, &height, &channels, 4);
        if (!faceData[i])
        {
            // Очистка при ошибке
            for (int j = 0; j < i; ++j)
                if (faceData[j]) stbi_image_free(faceData[j]);
            return nullptr;
        }
    }

    // 2. Создаём текстуру-куб
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;  // Можно 0 для авто-генерации, но тогда нужна отдельная логика
    desc.ArraySize = 6;  // 🔥 Ключевое: 6 граней
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;  // 🔥 Флаг кубмапа!

    ID3D11Texture2D* cubeTex = nullptr;
    if (FAILED(device->CreateTexture2D(&desc, nullptr, &cubeTex)))
    {
        for (auto* data : faceData) if (data) stbi_image_free(data);
        return nullptr;
    }

    // 3. Копируем данные каждой грани
    for (int i = 0; i < 6; ++i)
    {
        context->UpdateSubresource(
            cubeTex,
            D3D11CalcSubresource(0, i, 1),  // Mip 0, ArraySlice i
            0,
            faceData[i],
            width * 4,  // Row pitch: ширина * 4 байта на пиксель
            0
        );
        stbi_image_free(faceData[i]);
    }

    // 4. Создаём SRV для кубмапа
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = desc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
    srvDesc.TextureCube.MostDetailedMip = 0;
    srvDesc.TextureCube.MipLevels = 1;

    ID3D11ShaderResourceView* skySRV = nullptr;
    if (FAILED(device->CreateShaderResourceView(cubeTex, &srvDesc, &skySRV)))
    {
        cubeTex->Release();
        return nullptr;
    }

    cubeTex->Release();  // SRV держит ссылку, текстуру можно освободить
    return skySRV;
}

void ApplicationClass::SpawnTestBot(ECSRegistry& registry, const XMFLOAT3& pos)
{
    // ── 1. Создаём сущность ─────────────────────────────────────────────────
    Entity bot = registry.CreateEntity();

    // ── 2. Добавляем обязательные компоненты ────────────────────────────────
    registry.AddComponent<TransformComponent>(bot,
        TransformComponent{ pos, {0,0,0}, {1,1,1} });

    // 🔥 КРИТИЧНО: Добавляем ActiveComponent, иначе объект не рендерится!
    registry.AddComponent<ActiveComponent>(bot, ActiveComponent{});

    // ── 3. Меш для визуализации ────────────────────────────────────────────
    if (m_mainModel)
    {
        auto* mat = new MaterialClass();
        if (!mat->Initialize(m_direct3D->GetDevice(), L"TestBotMat"))
        {
            delete mat;
            OutputDebugStringA("[SpawnTestBot] Failed to init material\n");
            return;
        }

        // Выбираем шейдер
        mat->SetShader(m_lightShader);

        // Текстура
        if (m_defaultTexture) mat->SetMainTexture(m_defaultTexture);

        registry.AddComponent<MeshRendererComponent>(bot,
            MeshRendererComponent{ m_mainModel, mat });
    }

    // ── 4. Компоненты персонажа ────────────────────────────────────────────
    registry.AddComponent<CharacterController>(bot, CharacterController{});
    registry.AddComponent<CharacterMovement>(bot, CharacterMovement{});
    registry.AddComponent<SubmarineComponent>(bot, SubmarineComponent{});
    // ── 5. 🔥 РЕГИСТРАЦИЯ В SCENEMANAGER (критично!) ───────────────────────
    // Это добавляет объект в списки рендера, редактора, обновления
 

    // ── 6. Вызываем систему спавна (Bullet) ────────────────────────────────
    CharacterSpawnSystem::Spawn(registry, bot, pos);

    // ── 7. Отладочный вывод ────────────────────────────────────────────────
    OutputDebugStringA(("[Test] Bot spawned: entity=" + std::to_string(bot) +
        " at (" + std::to_string(pos.x) + ", " +
        std::to_string(pos.y) + ", " +
        std::to_string(pos.z) + ")\n").c_str());

    // 🔥 Проверка: видим ли мы объект в менеджере?
    if (m_sceneManager.IsObjectActive(bot))
        OutputDebugStringA("[Test] ✅ Object is ACTIVE in SceneManager\n");
    else
        OutputDebugStringA("[Test] ❌ Object is NOT ACTIVE — check RegisterEntity!\n");

    auto* move = registry.GetComponent<CharacterMovement>(bot);
    if (move) {
        move->walkDirection = XMFLOAT3(0, 0, 1);  // Вперёд по Z
        move->moveSpeed = 3.5f;
      OutputDebugStringA("[Test] Forced walk direction: forward\n");
    }



}
